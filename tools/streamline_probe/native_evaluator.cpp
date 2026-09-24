#include "starfox/render/dlss_native.h"
#include <windows.h>
#include <d3d12.h>
#include <sl.h>
#include <sl_helpers.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void check(sl::Result r, const char* message) {
    if(r!=sl::Result::eOk) throw std::runtime_error(std::string(message)+": "+sl::getResultAsStr(r));
}
template<class T> T* api(void* module,const char* name) {
    check(module!=nullptr,"Null SDK module");
    auto* fn=reinterpret_cast<T*>(GetProcAddress(static_cast<HMODULE>(module),name));
    check(fn!=nullptr,name);return fn;
}
template<class T> T* feature(void* module,const char* name) {
    void* fn{};check(api<PFun_slGetFeatureFunction>(module,"slGetFeatureFunction")(sl::kFeatureDLSS,name,fn),name);
    check(fn!=nullptr,name);return reinterpret_cast<T*>(fn);
}
template<class F> int guarded(F&& call,char* error,uint32_t capacity) noexcept {
    if(error && capacity) error[0]=0;
    try {call();return 0;} catch(const std::exception& e) {
        if(error && capacity) {auto n=std::min<size_t>(std::strlen(e.what()),capacity-1);std::memcpy(error,e.what(),n);error[n]=0;}
    } catch(...) {if(error && capacity) error[0]=0;}
    return 1;
}
template<size_t N> void finite(const float (&values)[N]) {
    for(float v:values) check(std::isfinite(v),"Nonfinite camera input");
}
sl::float4x4 matrix(const float* a) {
    sl::float4x4 m{};for(unsigned i=0;i<4;++i) m[i]={a[i*4],a[i*4+1],a[i*4+2],a[i*4+3]};return m;
}
sl::float3 vec(const float* a) {return {a[0],a[1],a[2]};}
}
int starfox_dlss_configure_v1(void* module,uint32_t viewport,uint32_t mode,uint32_t ow,uint32_t oh,
    uint32_t* w,uint32_t* h,char* error,uint32_t capacity) {
    return guarded([&] {
        check(w && h && ow && oh && ow<=16384 && oh<=16384 && mode>=1 && mode<=4,"Invalid DLSS configuration");
        const sl::DLSSMode modes[]{sl::DLSSMode::eMaxQuality,sl::DLSSMode::eBalanced,sl::DLSSMode::eMaxPerformance,sl::DLSSMode::eDLAA};
        sl::DLSSOptions options{};options.mode=modes[mode-1];options.outputWidth=ow;options.outputHeight=oh;
        options.colorBuffersHDR=sl::Boolean::eFalse;options.useAutoExposure=sl::Boolean::eFalse;
        sl::DLSSOptimalSettings optimal{};
        check(feature<PFun_slDLSSGetOptimalSettings>(module,"slDLSSGetOptimalSettings")(options,optimal),"DLSS dimensions");
        check(feature<PFun_slDLSSSetOptions>(module,"slDLSSSetOptions")(sl::ViewportHandle{viewport},options),"DLSS options");
        *w=optimal.optimalRenderWidth;*h=optimal.optimalRenderHeight;
    },error,capacity);
}
int starfox_dlss_evaluate_v1(void* module,const StarfoxDlssFrameV1* f,char* error,uint32_t capacity) {
    return guarded([&] {
        check(f && f->size==sizeof(*f),"DLSS frame ABI mismatch");
        check(f->command && f->width && f->height && f->output_width && f->output_height && f->reset<=1,"Invalid DLSS frame");
        finite(f->view_to_clip);finite(f->clip_to_view);finite(f->clip_to_previous);finite(f->previous_to_clip);
        finite(f->camera_position);finite(f->camera_up);finite(f->camera_right);finite(f->camera_forward);finite(f->jitter);finite(f->pinhole);
        check(f->near_plane>0 && f->far_plane>f->near_plane && std::isfinite(f->far_plane) &&
            f->vertical_fov>0 && f->vertical_fov<3.141593f && f->aspect>0 && std::isfinite(f->aspect),"Invalid DLSS camera");
        auto* list=static_cast<ID3D12GraphicsCommandList*>(f->command);
        Microsoft::WRL::ComPtr<ID3D12Device> device;
        check(SUCCEEDED(list->GetDevice(IID_PPV_ARGS(&device))),"Cannot identify command device");
        void* handles[]{f->color,f->depth,f->motion,f->output,f->exposure};
        const DXGI_FORMAT formats[]{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32G32_FLOAT,DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R32_FLOAT};
        sl::Resource resources[5];
        for(unsigned i=0;i<5;++i) {
            check(handles[i]!=nullptr,"Null DLSS resource");for(unsigned j=0;j<i;++j) check(handles[i]!=handles[j],"Aliased DLSS resources");
            auto* resource=static_cast<ID3D12Resource*>(handles[i]);auto d=resource->GetDesc();
            const auto w=i==4?1:i==3?f->output_width:f->width,h=i==4?1:i==3?f->output_height:f->height;
            check(d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width==w && d.Height==h && d.Format==formats[i] &&
                d.MipLevels==1 && d.DepthOrArraySize==1 && d.SampleDesc.Count==1,"DLSS resource format/extent mismatch");
            check(i!=3 || (d.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),"DLSS output lacks UAV usage");
            check(f->states[i]==uint32_t(i==3?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),"DLSS resource state mismatch");
            Microsoft::WRL::ComPtr<ID3D12Device> owner;check(SUCCEEDED(resource->GetDevice(IID_PPV_ARGS(&owner))) && owner.Get()==device.Get(),"DLSS resource device mismatch");
            resources[i]=sl::Resource{sl::ResourceType::eTex2d,resource,f->states[i]};
        }
        sl::Constants c{};c.cameraViewToClip=matrix(f->view_to_clip);c.clipToCameraView=matrix(f->clip_to_view);
        c.clipToPrevClip=matrix(f->clip_to_previous);c.prevClipToClip=matrix(f->previous_to_clip);
        const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};c.clipToLensClip=matrix(identity);
        c.cameraPos=vec(f->camera_position);c.cameraUp=vec(f->camera_up);c.cameraRight=vec(f->camera_right);c.cameraFwd=vec(f->camera_forward);
        c.cameraNear=f->near_plane;c.cameraFar=f->far_plane;c.cameraFOV=f->vertical_fov;c.cameraAspectRatio=f->aspect;
        c.jitterOffset={f->jitter[0],f->jitter[1]};c.cameraPinholeOffset={f->pinhole[0],f->pinhole[1]};
        c.mvecScale={1.f/f->width,1.f/f->height};c.reset=f->reset?sl::Boolean::eTrue:sl::Boolean::eFalse;
        c.depthInverted=sl::Boolean::eFalse;c.cameraMotionIncluded=sl::Boolean::eTrue;c.motionVectors3D=sl::Boolean::eFalse;c.motionVectorsInvalidValue=-FLT_MAX;
        c.motionVectorsJittered=sl::Boolean::eFalse;
        sl::FrameToken* token{};auto index=f->frame_index;
        check(api<PFun_slGetNewFrameToken>(module,"slGetNewFrameToken")(token,&index),"DLSS frame token");check(token!=nullptr,"Null DLSS token");
        sl::ViewportHandle viewport{f->viewport};check(api<PFun_slSetConstants>(module,"slSetConstants")(c,*token,viewport),"DLSS constants");
        const sl::Extent input{0,0,f->width,f->height},output{0,0,f->output_width,f->output_height},exposure{0,0,1,1};
        const sl::BufferType types[]{sl::kBufferTypeScalingInputColor,sl::kBufferTypeDepth,sl::kBufferTypeMotionVectors,sl::kBufferTypeScalingOutputColor,sl::kBufferTypeExposure};
        sl::ResourceTag tags[5];for(unsigned i=0;i<5;++i) tags[i]={&resources[i],types[i],sl::ResourceLifecycle::eValidUntilEvaluate,i==4?&exposure:i==3?&output:&input};
        check(api<PFun_slSetTagForFrame>(module,"slSetTagForFrame")(*token,viewport,tags,5,list),"DLSS tags");
        const sl::BaseStructure* inputs[]{&viewport};
        check(api<PFun_slEvaluateFeature>(module,"slEvaluateFeature")(sl::kFeatureDLSS,*token,inputs,1,list),"Evaluate DLSS");
    },error,capacity);
}
