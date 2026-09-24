#include "starfox/render/gpu_clip.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "shaders/generated/clip_portable.hpp"
#include "shaders/generated/clip_continuous_portable.hpp"
#include "shaders/generated/spans_portable.hpp"
#include <cstring>
#include <bit>
#include <stdexcept>
#include <iostream>
#endif
namespace starfox::render {
struct GpuClip::Impl {
    std::string status{"Native GPU clipping unavailable"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};
    SDL_GPUBuffer* output{};std::uint32_t capacity{};
    SDL_GPUComputePipeline* spans_pipeline{};
    SDL_GPUBuffer* spans{};std::uint32_t spans_capacity{};
    SDL_GPUBuffer* masks{};std::uint32_t masks_capacity{};
    NativeClipSettings settings{};
    SDL_GPUComputePipeline* continuous_pipeline{};bool continuous{};
    ~Impl(){release();}
    static void require(bool ok){if(!ok) throw std::runtime_error(SDL_GetError());}
    void release() noexcept {
        if(spans) SDL_ReleaseGPUBuffer(device,spans);
        if(masks) SDL_ReleaseGPUBuffer(device,masks);
        masks=nullptr;masks_capacity=0;
        if(spans_pipeline) SDL_ReleaseGPUComputePipeline(device,spans_pipeline);
        if(continuous_pipeline) SDL_ReleaseGPUComputePipeline(device,continuous_pipeline);
        continuous_pipeline=nullptr;continuous=false;
        spans=nullptr;spans_pipeline=nullptr;spans_capacity=0;settings={};
        if(output) SDL_ReleaseGPUBuffer(device,output);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        output=nullptr;pipeline=nullptr;device=nullptr;capacity=0;
    }
    void initialize(SDL_GPUDevice* next) {
        if(device==next && pipeline && spans_pipeline && continuous_pipeline) return;
        release();device=next;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        const bool intel_dxil=dxil && SDL_GetNumberProperty(SDL_GetGPUDeviceProperties(device),
            "starfox.gpu.vendor_id",0)==0x8086;
        if(!spirv && !dxil && !(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_MSL))
            throw std::runtime_error("Native clipping requires Vulkan, Metal or D3D12");
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
        if(spirv) {info.code=clip_shader::spirv;info.code_size=sizeof(clip_shader::spirv);}
#if defined(_WIN32)
        else if(dxil) {info.code=clip_shader::dxil;info.code_size=sizeof(clip_shader::dxil);}
#endif
#if defined(__APPLE__)
        else {info.code=reinterpret_cast<const Uint8*>(clip_shader::metal);info.code_size=std::strlen(clip_shader::metal);}
#else
        else throw std::runtime_error("Native clipping shader unavailable for this platform");
#endif
        info.entrypoint=(spirv||dxil)?"main":"main0";
        info.num_readonly_storage_buffers=5;info.num_readwrite_storage_buffers=1;
        info.num_uniform_buffers=1;info.threadcount_x=32;info.threadcount_y=info.threadcount_z=1;
        const bool trace=SDL_getenv("STARFOX_TRACE_GPU_MODEL_DISPATCH")!=nullptr;
        if(trace) std::cerr<<"clip-pipeline: creating native\n";
        pipeline=SDL_CreateGPUComputePipeline(device,&info);require(pipeline);
        if(spirv) {info.code=clip_continuous_shader::spirv;info.code_size=sizeof(clip_continuous_shader::spirv);}
#if defined(_WIN32)
        else if(dxil) {
            info.code=intel_dxil?clip_continuous_shader::intel_dxil:clip_continuous_shader::dxil;
            info.code_size=intel_dxil?sizeof(clip_continuous_shader::intel_dxil):sizeof(clip_continuous_shader::dxil);
        }
#endif
#if defined(__APPLE__)
        else {info.code=reinterpret_cast<const Uint8*>(clip_continuous_shader::metal);info.code_size=std::strlen(clip_continuous_shader::metal);}
#else
        else throw std::runtime_error("Continuous clipping shader unavailable for this platform");
#endif
        info.num_readonly_storage_buffers=6;
        if(trace) std::cerr<<"clip-pipeline: creating continuous"<<(intel_dxil?" intel-compact":"")<<'\n';
        continuous_pipeline=SDL_CreateGPUComputePipeline(device,&info);require(continuous_pipeline);
        if(spirv) {info.code=spans_shader::spirv;info.code_size=sizeof(spans_shader::spirv);}
#if defined(_WIN32)
        else if(dxil) {info.code=spans_shader::dxil;info.code_size=sizeof(spans_shader::dxil);}
#endif
#if defined(__APPLE__)
        else {info.code=reinterpret_cast<const Uint8*>(spans_shader::metal);info.code_size=std::strlen(spans_shader::metal);}
#else
        else throw std::runtime_error("Span shader unavailable for this platform");
#endif
        info.num_readonly_storage_buffers=4;
        info.num_readwrite_storage_buffers=2;
        if(trace) std::cerr<<"clip-pipeline: creating spans\n";
        spans_pipeline=SDL_CreateGPUComputePipeline(device,&info);require(spans_pipeline);
        if(trace) std::cerr<<"clip-pipeline: ready\n";
    }
#endif
};
GpuClip::GpuClip():impl_(std::make_unique<Impl>()){}
std::uint32_t GpuClip::mask_buffer_bytes() const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    return impl_->masks_capacity;
#else
    return 0;
#endif
}
GpuClip::~GpuClip()=default;
const std::string& GpuClip::status()const noexcept{return impl_->status;}
void GpuClip::release_device()noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
void* GpuClip::enqueue(void* device,void* command,void* points,void* corners,
    void* polygons,void* visibility,const NativeClipSettings& settings,bool continuous,
    void* projection_params,std::uint32_t projection_count,void* point_residuals,std::uint32_t residual_count) {
    if(!device || !command || !points || !corners || !polygons || !visibility
        || !settings.polygon_count || settings.polygon_count>65536
        || settings.width<=0 || settings.width>32767 || settings.height<=0 || settings.height>32767) {
        impl_->status="Invalid native clipping input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(points==impl_->output || corners==impl_->output || polygons==impl_->output || visibility==impl_->output
            || (projection_params && projection_params==impl_->output) || (point_residuals && point_residuals==impl_->output))
            throw std::runtime_error("Native clipping input aliases output");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(impl_->capacity<settings.polygon_count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,settings.polygon_count*129*16,0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->output) SDL_ReleaseGPUBuffer(impl_->device,impl_->output);
            impl_->output=replacement;impl_->capacity=settings.polygon_count;
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const bool trace=SDL_getenv("STARFOX_TRACE_GPU_MODEL_DISPATCH")!=nullptr;
        if(trace) std::cerr<<"clip-enqueue: uniforms continuous="<<continuous
            <<" polygons="<<settings.polygon_count<<" points="<<settings.point_count
            <<" corners="<<settings.corner_count<<" residuals="<<residual_count<<'\n';
        auto uniforms=settings;uniforms.reserved[0]=projection_params?projection_count:0;
        uniforms.reserved[1]=continuous && point_residuals?residual_count:0;
        SDL_PushGPUComputeUniformData(cmd,0,&uniforms,sizeof(uniforms));
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=impl_->output;binding.cycle=true;
        if(trace) std::cerr<<"clip-enqueue: begin\n";
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);Impl::require(pass);
        if(trace) std::cerr<<"clip-enqueue: bind pipeline\n";
        SDL_BindGPUComputePipeline(pass,continuous?impl_->continuous_pipeline:impl_->pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(points),static_cast<SDL_GPUBuffer*>(corners),
            static_cast<SDL_GPUBuffer*>(polygons),static_cast<SDL_GPUBuffer*>(visibility),
            static_cast<SDL_GPUBuffer*>(projection_params?projection_params:points),
            static_cast<SDL_GPUBuffer*>(point_residuals?point_residuals:points)};
        if(trace) std::cerr<<"clip-enqueue: bind buffers\n";
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,continuous?6:5);
        if(trace) std::cerr<<"clip-enqueue: dispatch\n";
        SDL_DispatchGPUCompute(pass,(settings.polygon_count+31)/32,1,1);
        if(trace) std::cerr<<"clip-enqueue: end pass\n";
        SDL_EndGPUComputePass(pass);
        if(trace) std::cerr<<"clip-enqueue: done\n";
        impl_->settings=settings;
        impl_->continuous=continuous;
        impl_->status=continuous?"Continuous screen clipping GPU resident":"Native screen clipping GPU resident";return impl_->output;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuClip::enqueue_spans(void* command,void* materials,bool winding_independent,std::uint32_t render_scale,const GpuSpanOrder* order,std::uint32_t line_thickness,void* source_texels,std::uint32_t source_texel_bytes,void** masked_texels,std::array<std::uint32_t,2> raster_size,bool reuse_span_scratch) {
    if(masked_texels) *masked_texels=nullptr;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(render_scale<1 || render_scale>4) throw std::runtime_error("Invalid span render scale");
        if(!command || !materials || !impl_->settings.polygon_count || !impl_->output || !impl_->spans_pipeline)
            throw std::runtime_error("Solid span emission requires clipped polygons and materials");
        if(materials==impl_->spans || materials==impl_->output)
            throw std::runtime_error("Solid span materials alias scratch data");
        if(order && (!order->indices || !order->results || !order->capacity || order->capacity>65536
            || order->first>UINT32_MAX-order->capacity
            || order->indices==impl_->spans || order->results==impl_->spans))
            throw std::runtime_error("Invalid or aliased BSP span order");
        const auto& s=impl_->settings;
        const auto slots=order?order->capacity:s.polygon_count;
        const bool custom=raster_size[0] || raster_size[1];
        if(custom && (!raster_size[0] || !raster_size[1] || !impl_->continuous))
            throw std::runtime_error("Custom raster size requires continuous geometry and two positive dimensions");
        const auto width=custom?raster_size[0]:Uint32(s.width)*render_scale,height=custom?raster_size[1]:Uint32(s.height)*render_scale;
        if(width>32767 || height>32767) throw std::runtime_error("Scaled span viewport exceeds fixed-point bounds");
        const auto bytes=std::uint64_t(slots)*height*96;
        if(bytes>256U*1024*1024) throw std::runtime_error("Solid span batch exceeds 256 MiB scratch budget");
        if(impl_->spans_capacity<bytes) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,Uint32(bytes),0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->spans) SDL_ReleaseGPUBuffer(impl_->device,impl_->spans);
            impl_->spans=replacement;impl_->spans_capacity=Uint32(bytes);
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const auto mask_stride=((width+31)/32)*4;
        const auto mask_bytes=masked_texels?std::uint64_t(slots)*height*mask_stride+source_texel_bytes:4;
        if(mask_bytes>256U*1024*1024 || (source_texel_bytes&3U)
            || (source_texel_bytes && !source_texels) || (source_texels && source_texels==impl_->masks)
            || materials==impl_->masks || (order && (order->indices==impl_->masks || order->results==impl_->masks)))
            throw std::runtime_error("Invalid or oversized span mask storage");
        if(impl_->masks_capacity<mask_bytes) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,Uint32(mask_bytes),0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->masks) SDL_ReleaseGPUBuffer(impl_->device,impl_->masks);
            impl_->masks=replacement;impl_->masks_capacity=Uint32(mask_bytes);
        }
        const bool copied=masked_texels && source_texel_bytes;
        if(copied) {
            auto* copy=SDL_BeginGPUCopyPass(cmd);Impl::require(copy);
            SDL_GPUBufferLocation from{static_cast<SDL_GPUBuffer*>(source_texels),0},to{impl_->masks,0};
            SDL_CopyGPUBufferToBuffer(copy,&from,&to,source_texel_bytes,true);SDL_EndGPUCopyPass(copy);
        }
        const Uint32 settings[]{slots,width,height,winding_independent?1U:0U,
            render_scale,impl_->continuous?1U:0U,order?1U:0U,s.polygon_count,
            order?order->first:0U,order?order->tree_index:0U,line_thickness,0,
            masked_texels?1U:0U,source_texel_bytes,mask_stride,custom?1U:0U,
            std::bit_cast<Uint32>(float(width)/s.width),std::bit_cast<Uint32>(float(height)/s.height),0,0};
        SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding bindings[2]{};
        // Ordered model batches rasterize these rows before the next model
        // overwrites them. Cycling would retain a full polygon*height buffer
        // per terrain tile until submission completes (several GiB at 4x).
        bindings[0].buffer=impl_->spans;bindings[0].cycle=!reuse_span_scratch;
        bindings[1].buffer=impl_->masks;bindings[1].cycle=!copied;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,bindings,2);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->spans_pipeline);
        SDL_GPUBuffer* inputs[]{impl_->output,static_cast<SDL_GPUBuffer*>(materials),
            static_cast<SDL_GPUBuffer*>(order?order->indices:materials),
            static_cast<SDL_GPUBuffer*>(order?order->results:materials)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,4);
        SDL_DispatchGPUCompute(pass,(slots+31)/32,1,1);SDL_EndGPUComputePass(pass);
        if(masked_texels) *masked_texels=impl_->masks;
        impl_->status="Native clipping and spans GPU resident";return impl_->spans;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
}
