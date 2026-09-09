#include "starfox/render/gpu_effects.hpp"
#if defined(STARFOX_GPU_EFFECTS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "effects_compute_shader.hpp"
#include <cstring>
#include <stdexcept>
#endif
namespace starfox::render {
#if defined(STARFOX_GPU_EFFECTS)
using Microsoft::WRL::ComPtr;
namespace {
void checked(HRESULT hr,const char* message) { if(FAILED(hr)) throw std::runtime_error(message); }
struct Parameters {
    std::uint32_t width,height,scale,stage,hdr,chromatic,smoothing,model_effect;
    std::uint32_t world_effect,model_intensity,world_intensity,aa;
    std::uint32_t lighting,surface_width,surface_height,reserved;
    std::int32_t surface_x,surface_y,minimum_x,minimum_y,maximum_x,maximum_y,pad0,pad1;
    std::uint32_t bloom_model,bloom_world,bloom_width,bloom_height;
    std::uint32_t filter,highlight_filter,pad2,pad3;
    std::uint32_t shadow_width,shadow_height; std::int32_t shadow_y; std::uint32_t shadow_enabled;
};
static_assert(sizeof(Parameters)==144);
static_assert(sizeof(SurfaceSample)==20 && offsetof(SurfaceSample,valid)==17);
}
struct GpuEffects::Impl {
    std::string status{"GPU effects not initialized"};
    bool failed{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11ComputeShader> shader;
    ComPtr<ID3D11Buffer> parameters,tags;
    ComPtr<ID3D11ShaderResourceView> tag_view;
    ComPtr<ID3D11Buffer> indexed,surface_data;
    ComPtr<ID3D11ShaderResourceView> indexed_view,surface_view;
    unsigned surface_bytes{};
    ComPtr<ID3D11Buffer> shadow_data;
    ComPtr<ID3D11ShaderResourceView> shadow_view;
    unsigned shadow_bytes{};
    ComPtr<ID3D11Texture2D> images[2],readback;
    ComPtr<ID3D11Texture2D> bloom_snapshots[2];
    ComPtr<ID3D11Texture2D> split_snapshots[2],split_glow;
    ComPtr<ID3D11ShaderResourceView> split_inputs[2];
    ComPtr<ID3D11UnorderedAccessView> split_output;
    ComPtr<ID3D11ShaderResourceView> inputs[2];
    ComPtr<ID3D11UnorderedAccessView> outputs[2];
    ComPtr<ID3D11Texture2D> bloom_images[4];
    ComPtr<ID3D11ShaderResourceView> bloom_inputs[4];
    ComPtr<ID3D11UnorderedAccessView> bloom_outputs[4];
    unsigned bloom_width{},bloom_height{};
    ComPtr<ID3D11Texture2D> filter_image;
    ComPtr<ID3D11ShaderResourceView> filter_input;
    ComPtr<ID3D11UnorderedAccessView> filter_output;
    unsigned filter_width{},filter_height{};
    unsigned width{},height{};
    void initialize(ID3D11Device* source) {
        device=source; device->GetImmediateContext(context.GetAddressOf());
        if(device->GetFeatureLevel()<D3D_FEATURE_LEVEL_11_0) throw std::runtime_error("Shader Model 5 unavailable");
        checked(device->CreateComputeShader(effects_compute_shader,sizeof(effects_compute_shader),nullptr,
            shader.GetAddressOf()),"GPU effect shader creation failed");
        D3D11_BUFFER_DESC desc{}; desc.ByteWidth=sizeof(Parameters); desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        checked(device->CreateBuffer(&desc,nullptr,parameters.GetAddressOf()),"GPU effect constants failed");
        status="D3D11 GPU compute effects";
    }
    void resize(unsigned w,unsigned h) {
        if(w==width && h==height) return;
        for(unsigned i=0;i<2;++i) { images[i].Reset(); inputs[i].Reset(); outputs[i].Reset(); }
        readback.Reset(); tags.Reset(); tag_view.Reset(); width=height=0;
        for(auto& snapshot:bloom_snapshots) snapshot.Reset();
        for(unsigned i=0;i<2;++i) {split_snapshots[i].Reset();split_inputs[i].Reset();}
        split_glow.Reset();split_output.Reset();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=w; desc.Height=h; desc.MipLevels=1; desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        for(unsigned i=0;i<2;++i) {
            checked(device->CreateTexture2D(&desc,nullptr,images[i].GetAddressOf()),"GPU effects texture failed");
            checked(device->CreateShaderResourceView(images[i].Get(),nullptr,inputs[i].GetAddressOf()),"GPU effects SRV failed");
            checked(device->CreateUnorderedAccessView(images[i].Get(),nullptr,outputs[i].GetAddressOf()),"GPU effects UAV failed");
        }
        desc.BindFlags=0; desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        checked(device->CreateTexture2D(&desc,nullptr,readback.GetAddressOf()),"GPU effects readback failed");
        D3D11_BUFFER_DESC buffer{};
        buffer.ByteWidth=(w*h+3)&~3U; buffer.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        buffer.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        checked(device->CreateBuffer(&buffer,nullptr,tags.GetAddressOf()),"GPU effects tags failed");
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format=DXGI_FORMAT_R32_TYPELESS; view.ViewDimension=D3D11_SRV_DIMENSION_BUFFEREX;
        view.BufferEx.NumElements=buffer.ByteWidth/4; view.BufferEx.Flags=D3D11_BUFFEREX_SRV_FLAG_RAW;
        checked(device->CreateShaderResourceView(tags.Get(),&view,tag_view.GetAddressOf()),"GPU effects tag SRV failed");
        width=w; height=h;
    }
    void raw_buffer(ComPtr<ID3D11Buffer>& buffer,ComPtr<ID3D11ShaderResourceView>& view,
        unsigned bytes,const void* data) {
        buffer.Reset(); view.Reset();
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth=std::max(4U,(bytes+3)&~3U); desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        D3D11_SUBRESOURCE_DATA contents{data,0,0};
        checked(device->CreateBuffer(&desc,data?&contents:nullptr,buffer.GetAddressOf()),"GPU raw buffer failed");
        D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format=DXGI_FORMAT_R32_TYPELESS;srv.ViewDimension=D3D11_SRV_DIMENSION_BUFFEREX;
        srv.BufferEx.NumElements=desc.ByteWidth/4;srv.BufferEx.Flags=D3D11_BUFFEREX_SRV_FLAG_RAW;
        checked(device->CreateShaderResourceView(buffer.Get(),&srv,view.GetAddressOf()),"GPU raw SRV failed");
    }
    void resize_bloom(unsigned w,unsigned h) {
        if(w==bloom_width && h==bloom_height) return;
        bloom_width=bloom_height=0;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=w;desc.Height=h;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;desc.SampleDesc.Count=1;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        for(unsigned i=0;i<4;++i) {
            bloom_images[i].Reset();bloom_inputs[i].Reset();bloom_outputs[i].Reset();
            checked(device->CreateTexture2D(&desc,nullptr,bloom_images[i].GetAddressOf()),"GPU bloom texture failed");
            checked(device->CreateShaderResourceView(bloom_images[i].Get(),nullptr,bloom_inputs[i].GetAddressOf()),"GPU bloom SRV failed");
            checked(device->CreateUnorderedAccessView(bloom_images[i].Get(),nullptr,bloom_outputs[i].GetAddressOf()),"GPU bloom UAV failed");
        }
        bloom_width=w;bloom_height=h;
    }
    void resize_filter(unsigned w,unsigned h) {
        if(w==filter_width && h==filter_height) return;
        filter_width=filter_height=0;
        filter_image.Reset();filter_input.Reset();filter_output.Reset();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=w;desc.Height=h;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        checked(device->CreateTexture2D(&desc,nullptr,filter_image.GetAddressOf()),"GPU filter source failed");
        checked(device->CreateShaderResourceView(filter_image.Get(),nullptr,filter_input.GetAddressOf()),"GPU filter SRV failed");
        checked(device->CreateUnorderedAccessView(filter_image.Get(),nullptr,filter_output.GetAddressOf()),"GPU filter UAV failed");
        filter_width=w;filter_height=h;
    }
    void apply(const Framebuffer& frame,std::vector<std::uint8_t>& rgba,const GpuEffectSettings& settings) {
        resize(frame.stored_width(),frame.stored_height());
        auto* presentation=static_cast<ID3D11Texture2D*>(settings.presentation_texture);
        auto* glow_presentation=static_cast<ID3D11Texture2D*>(settings.presentation_glow_texture);
        auto* model_presentation=static_cast<ID3D11Texture2D*>(settings.presentation_model_texture);
        if(model_presentation && (!presentation || !settings.surfaces || settings.surfaces->empty()))
            throw std::runtime_error("GPU model presentation requires surface data and a base texture");
        if(glow_presentation && (!presentation || !(settings.bloom_model || settings.bloom_world)))
            throw std::runtime_error("GPU glow presentation requires bloom and a base texture");
        for(auto* target:{presentation,glow_presentation,model_presentation}) if(target) {
            D3D11_TEXTURE2D_DESC desc{};target->GetDesc(&desc);
            if(desc.Width!=width || desc.Height!=height || desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM
                || desc.SampleDesc.Count!=1 || desc.MipLevels!=1 || desc.ArraySize!=1)
                throw std::runtime_error("Incompatible GPU presentation texture");
        }
        if((glow_presentation || model_presentation) && !split_glow) {
            D3D11_TEXTURE2D_DESC desc{};images[0]->GetDesc(&desc);
            checked(device->CreateTexture2D(&desc,nullptr,split_glow.GetAddressOf()),"GPU split output failed");
            checked(device->CreateUnorderedAccessView(split_glow.Get(),nullptr,split_output.GetAddressOf()),"GPU split UAV failed");
        }
        context->UpdateSubresource(images[0].Get(),0,nullptr,rgba.data(),width*4,0);
        // The last raw load is a full DWORD, including odd-sized buffers.
        std::vector<std::uint8_t> padded((frame.pixels().size()+3)&~std::size_t(3),0);
        std::copy(frame.layer_tags().begin(),frame.layer_tags().end(),padded.begin());
        context->UpdateSubresource(tags.Get(),0,nullptr,padded.data(),0,0);
        Parameters p{width,height,frame.draw_scale(),0,settings.hdr,settings.chromatic,settings.smoothing,
            settings.model_effect,settings.world_effect,settings.model_intensity,settings.world_intensity,settings.anti_aliasing,
            0,0,0,0,0,0,0,0,0,0,0,0,settings.bloom_model,settings.bloom_world,
            (width+2*frame.draw_scale()-1)/(2*frame.draw_scale()),(height+2*frame.draw_scale()-1)/(2*frame.draw_scale()),
            settings.filter,settings.highlight_filter,settings.overlay_filter?1U:0U,0,
            settings.shadow_width,settings.shadow_height,settings.shadow_offset_y,0};
        if(!settings.shadow_mask.empty()) {
            if(!p.shadow_width || !p.shadow_height || settings.shadow_mask.size()!=std::size_t(p.shadow_width)*p.shadow_height)
                throw std::runtime_error("Invalid GPU shadow dimensions");
            std::vector<std::uint8_t> mask((settings.shadow_mask.size()+3)&~std::size_t(3),0);
            std::copy(settings.shadow_mask.begin(),settings.shadow_mask.end(),mask.begin());
            if(!shadow_data || shadow_bytes!=mask.size()) {
                raw_buffer(shadow_data,shadow_view,unsigned(mask.size()),nullptr); shadow_bytes=unsigned(mask.size());
            }
            context->UpdateSubresource(shadow_data.Get(),0,nullptr,mask.data(),0,0);
            p.shadow_enabled=1;
        }
        if((settings.lighting || model_presentation) && settings.surfaces && !settings.surfaces->empty()) {
            const auto& surface=*settings.surfaces;
            p.lighting=settings.lighting;p.surface_width=surface.width();p.surface_height=surface.height();
            p.surface_x=settings.surface_x;p.surface_y=settings.surface_y;
            p.minimum_x=std::max(1,p.surface_x+int(surface.minimum_x()));
            p.minimum_y=std::max(1,p.surface_y+int(surface.minimum_y()));
            p.maximum_x=std::min(int(width)-1,p.surface_x+int(surface.maximum_x()));
            p.maximum_y=std::min(int(height)-1,p.surface_y+int(surface.maximum_y()));
            std::copy(frame.pixels().begin(),frame.pixels().end(),padded.begin());
            raw_buffer(indexed,indexed_view,unsigned(padded.size()),padded.data());
            const auto bytes=unsigned(surface.samples().size_bytes());
            if(!surface_data || bytes!=surface_bytes) {
                raw_buffer(surface_data,surface_view,bytes,nullptr); surface_bytes=bytes;
            }
            context->UpdateSubresource(surface_data.Get(),0,nullptr,surface.samples().data(),0,0);
        }
        context->CSSetShader(shader.Get(),nullptr,0);
        auto* cb=parameters.Get(); context->CSSetConstantBuffers(0,1,&cb);
        unsigned current=0;
        const bool enabled[]{false,p.hdr!=0,p.chromatic!=0,p.smoothing!=0,
            p.model_effect!=0 || p.world_effect!=0,p.aa!=0};
        for(int sequence=-1;sequence<=6;++sequence) {
            const auto stage=sequence<0?14:sequence==0?6:sequence==6?15:sequence;
            if(stage==5 && (p.bloom_model || p.bloom_world)) {
                if(glow_presentation) {
                    for(unsigned i=0;i<2;++i) if(!split_snapshots[i]) {
                        D3D11_TEXTURE2D_DESC desc{};images[0]->GetDesc(&desc);desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                        checked(device->CreateTexture2D(&desc,nullptr,split_snapshots[i].GetAddressOf()),"GPU split snapshot failed");
                        checked(device->CreateShaderResourceView(split_snapshots[i].Get(),nullptr,split_inputs[i].GetAddressOf()),"GPU split SRV failed");
                    }
                    context->CopyResource(split_snapshots[0].Get(),images[current].Get());
                } else if(settings.bloom_base || settings.bloom_glow) {
                    for(auto& snapshot:bloom_snapshots) if(!snapshot) {
                        D3D11_TEXTURE2D_DESC desc{};readback->GetDesc(&desc);
                        checked(device->CreateTexture2D(&desc,nullptr,snapshot.GetAddressOf()),"GPU bloom snapshot failed");
                    }
                    context->CopyResource(bloom_snapshots[0].Get(),images[current].Get());
                }
                resize_bloom(p.bloom_width,p.bloom_height);
                const unsigned source[]{0,0,1,2,1,3},target[]{0,1,2,1,3,0};
                for(unsigned pass=0;pass<6;++pass) {
                    p.stage=7+pass;context->UpdateSubresource(parameters.Get(),0,nullptr,&p,0,0);
                    ID3D11ShaderResourceView* views[]{inputs[current].Get(),tag_view.Get(),nullptr,nullptr,
                        pass==0?nullptr:bloom_inputs[source[pass]].Get(),pass==5?bloom_inputs[2].Get():nullptr};
                    context->CSSetShaderResources(0,6,views);
                    ID3D11UnorderedAccessView* out[]{pass==5?outputs[1-current].Get():nullptr,
                        pass==5?nullptr:bloom_outputs[target[pass]].Get()};
                    context->CSSetUnorderedAccessViews(0,2,out,nullptr);
                    context->Dispatch(((pass==5?width:p.bloom_width)+7)/8,((pass==5?height:p.bloom_height)+7)/8,1);
                    ID3D11UnorderedAccessView* noOut[2]{};context->CSSetUnorderedAccessViews(0,2,noOut,nullptr);
                    ID3D11ShaderResourceView* noIn[6]{};context->CSSetShaderResources(0,6,noIn);
                }
                current=1-current;
                if(glow_presentation) context->CopyResource(split_snapshots[1].Get(),images[current].Get());
                else if(settings.bloom_base || settings.bloom_glow)
                    context->CopyResource(bloom_snapshots[1].Get(),images[current].Get());
            }
            if(stage==15?!p.shadow_enabled:stage==14?!p.filter:stage==6?!p.lighting:!enabled[stage]) continue;
            if(stage==14) {
                resize_filter(width/p.scale,height/p.scale);
                p.stage=13;context->UpdateSubresource(parameters.Get(),0,nullptr,&p,0,0);
                ID3D11ShaderResourceView* views[]{inputs[current].Get(),tag_view.Get()};
                context->CSSetShaderResources(0,2,views);
                auto* output=filter_output.Get();context->CSSetUnorderedAccessViews(2,1,&output,nullptr);
                context->Dispatch((filter_width+7)/8,(filter_height+7)/8,1);
                ID3D11UnorderedAccessView* noOut{};context->CSSetUnorderedAccessViews(2,1,&noOut,nullptr);
                ID3D11ShaderResourceView* noIn[2]{};context->CSSetShaderResources(0,2,noIn);
            }
            p.stage=stage; context->UpdateSubresource(parameters.Get(),0,nullptr,&p,0,0);
            ID3D11ShaderResourceView* views[]{inputs[current].Get(),tag_view.Get(),indexed_view.Get(),surface_view.Get(),nullptr,nullptr,
                stage==14?filter_input.Get():nullptr,stage==15?shadow_view.Get():nullptr};
            context->CSSetShaderResources(0,8,views);
            auto* output=outputs[1-current].Get(); context->CSSetUnorderedAccessViews(0,1,&output,nullptr);
            context->Dispatch((width+7)/8,(height+7)/8,1);
            ID3D11UnorderedAccessView* noOutput{}; context->CSSetUnorderedAccessViews(0,1,&noOutput,nullptr);
            ID3D11ShaderResourceView* noInputs[8]{}; context->CSSetShaderResources(0,8,noInputs);
            current=1-current;
        }
        context->CSSetShader(nullptr,nullptr,0);
        context->CopyResource(readback.Get(),images[current].Get());
        if(glow_presentation) {
            p.stage=16;context->UpdateSubresource(parameters.Get(),0,nullptr,&p,0,0);
            context->CSSetShader(shader.Get(),nullptr,0);
            ID3D11ShaderResourceView* views[10]{inputs[current].Get(),nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,
                split_inputs[0].Get(),split_inputs[1].Get()};
            context->CSSetShaderResources(0,10,views);
            ID3D11UnorderedAccessView* outputsToBind[]{outputs[1-current].Get(),nullptr,nullptr,split_output.Get()};
            context->CSSetUnorderedAccessViews(0,4,outputsToBind,nullptr);
            context->Dispatch((width+7)/8,(height+7)/8,1);
            ID3D11UnorderedAccessView* noOut[4]{};context->CSSetUnorderedAccessViews(0,4,noOut,nullptr);
            ID3D11ShaderResourceView* noIn[10]{};context->CSSetShaderResources(0,10,noIn);
            context->CSSetShader(nullptr,nullptr,0);
            current=1-current;context->CopyResource(glow_presentation,split_glow.Get());
        }
        if(model_presentation) {
            p.stage=17;context->UpdateSubresource(parameters.Get(),0,nullptr,&p,0,0);
            context->CSSetShader(shader.Get(),nullptr,0);
            ID3D11ShaderResourceView* views[]{inputs[current].Get(),nullptr,indexed_view.Get(),surface_view.Get()};
            context->CSSetShaderResources(0,4,views);
            ID3D11UnorderedAccessView* out[]{outputs[1-current].Get(),nullptr,nullptr,split_output.Get()};
            context->CSSetUnorderedAccessViews(0,4,out,nullptr);
            context->Dispatch((width+7)/8,(height+7)/8,1);
            ID3D11UnorderedAccessView* noOut[4]{};context->CSSetUnorderedAccessViews(0,4,noOut,nullptr);
            ID3D11ShaderResourceView* noIn[4]{};context->CSSetShaderResources(0,4,noIn);
            context->CSSetShader(nullptr,nullptr,0);
            current=1-current;context->CopyResource(model_presentation,split_glow.Get());
        }
        if(presentation) {
            context->CopyResource(presentation,images[current].Get());
            return;
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(p.bloom_model || p.bloom_world) {
            std::vector<std::uint8_t>* destinations[]{settings.bloom_base,settings.bloom_glow};
            for(unsigned i=0;i<2;++i) if(destinations[i]) {
                destinations[i]->resize(rgba.size());
                checked(context->Map(bloom_snapshots[i].Get(),0,D3D11_MAP_READ,0,&mapped),"GPU bloom snapshot map failed");
                for(unsigned y=0;y<height;++y)
                    std::memcpy(destinations[i]->data()+std::size_t(y)*width*4,
                        static_cast<const std::uint8_t*>(mapped.pData)+std::size_t(y)*mapped.RowPitch,width*4);
                context->Unmap(bloom_snapshots[i].Get(),0);
            }
        }
        checked(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"GPU effects readback map failed");
        for(unsigned y=0;y<height;++y)
            std::memcpy(rgba.data()+std::size_t(y)*width*4,
                static_cast<const std::uint8_t*>(mapped.pData)+std::size_t(y)*mapped.RowPitch,width*4);
        context->Unmap(readback.Get(),0);
    }
};
#else
struct GpuEffects::Impl { std::string status{"GPU compute effects unavailable on this build"}; };
#endif
GpuEffects::GpuEffects():impl_(std::make_unique<Impl>()) {}
GpuEffects::~GpuEffects()=default;
void GpuEffects::release_device() noexcept { impl_.reset(); }
const std::string& GpuEffects::status() const {
    static const std::string released{"GPU effects device released"};
    return impl_?impl_->status:released;
}
bool GpuEffects::readback(std::vector<std::uint8_t>& rgba) {
#if defined(STARFOX_GPU_EFFECTS)
    if(!impl_ || !impl_->readback || rgba.size()!=std::size_t(impl_->width)*impl_->height*4) return false;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(impl_->context->Map(impl_->readback.Get(),0,D3D11_MAP_READ,0,&mapped))) return false;
    for(unsigned y=0;y<impl_->height;++y)
        std::memcpy(rgba.data()+std::size_t(y)*impl_->width*4,
            static_cast<const std::uint8_t*>(mapped.pData)+std::size_t(y)*mapped.RowPitch,impl_->width*4);
    impl_->context->Unmap(impl_->readback.Get(),0);return true;
#else
    (void)rgba;return false;
#endif
}
bool GpuEffects::apply(void* source,const Framebuffer& frame,std::vector<std::uint8_t>& rgba,
    const GpuEffectSettings& settings) {
    if(!impl_) impl_=std::make_unique<Impl>();
#if defined(STARFOX_GPU_EFFECTS)
    if(!source || !frame.layer_tags_enabled() || rgba.size()!=frame.pixels().size()*4 || rgba.empty()) return false;
#if !defined(STARFOX_ENABLE_XBRZ)
    if(settings.filter==2) return false;
#endif
    if(impl_->device.Get()!=source) impl_=std::make_unique<Impl>();
    if(impl_->failed) return false;
    try {
        if(!impl_->device) impl_->initialize(static_cast<ID3D11Device*>(source));
        impl_->apply(frame,rgba,settings); return true;
    } catch(const std::exception& e) {
        impl_->status=e.what(); impl_->failed=true; return false;
    }
#else
    (void)source; (void)frame; (void)rgba; (void)settings; return false;
#endif
}
}
