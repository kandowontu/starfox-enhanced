#include "starfox/render/gpu_fsr1.hpp"
#include "starfox/render/gpu_temporal_inputs.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "shaders/generated/fsr1_portable.hpp"
#endif

namespace starfox::render {
struct GpuFsr1::Impl {
    std::string status{"FSR1 not initialized"};
    GpuTemporalInputs hud;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};
    SDL_GPUComputePipeline* pipeline{};
    SDL_GPUSampler* sampler{};
    SDL_GPUTexture* images[2]{};
    Fsr1Extent size{};
    static void require(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}
    void release() noexcept {
        hud.release_device();
        if(device) {
            for(auto*& image:images) {if(image) SDL_ReleaseGPUTexture(device,image);image=nullptr;}
            if(sampler) SDL_ReleaseGPUSampler(device,sampler);
            if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        }
        device=nullptr;pipeline=nullptr;sampler=nullptr;size={};
    }
    ~Impl() {release();}
    void initialize(SDL_GPUDevice* requested) {
        if(device && device!=requested) throw std::runtime_error("FSR1 device changed without release");
        if(device) return;
        device=requested;
        try {
            using namespace fsr1_shader;
            const auto formats=SDL_GetGPUShaderFormats(device);
            SDL_GPUComputePipelineCreateInfo p{};
            if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
                p.format=SDL_GPU_SHADERFORMAT_SPIRV;p.code=spirv;p.code_size=sizeof(spirv);p.entrypoint="main";
            }
#if defined(__APPLE__)
            else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
                p.format=SDL_GPU_SHADERFORMAT_MSL;p.code=reinterpret_cast<const Uint8*>(metal);
                p.code_size=sizeof(metal)-1;p.entrypoint="main0";
            }
#endif
#if defined(_WIN32)
            else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
                p.format=SDL_GPU_SHADERFORMAT_DXIL;p.code=dxil;p.code_size=sizeof(dxil);p.entrypoint="main";
            }
#endif
            else throw std::runtime_error("FSR1 shader format unsupported");
            p.num_samplers=1;p.num_readwrite_storage_textures=1;p.num_uniform_buffers=1;
            p.threadcount_x=8;p.threadcount_y=8;p.threadcount_z=1;
            pipeline=SDL_CreateGPUComputePipeline(device,&p);require(pipeline);
            SDL_GPUSamplerCreateInfo s{};
            s.min_filter=s.mag_filter=SDL_GPU_FILTER_LINEAR;
            s.address_mode_u=s.address_mode_v=s.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            sampler=SDL_CreateGPUSampler(device,&s);require(sampler);
        } catch(...) {release();throw;}
    }
    void allocate(Fsr1Extent extent) {
        if(size==extent) return;
        SDL_GPUTextureCreateInfo t{};
        t.type=SDL_GPU_TEXTURETYPE_2D;t.format=SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        t.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE
            |SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
        t.width=extent.width;t.height=extent.height;t.layer_count_or_depth=1;t.num_levels=1;
        SDL_GPUTexture* replacement[2]{};
        for(unsigned i=0;i<2;++i) {
            replacement[i]=SDL_CreateGPUTexture(device,&t);
            if(!replacement[i]) {
                for(auto* image:replacement) if(image) SDL_ReleaseGPUTexture(device,image);
                throw std::runtime_error(SDL_GetError());
            }
        }
        for(unsigned i=0;i<2;++i) {if(images[i]) SDL_ReleaseGPUTexture(device,images[i]);images[i]=replacement[i];}
        size=extent;
    }
#endif
};
GpuFsr1::GpuFsr1():impl_(std::make_unique<Impl>()) {}
GpuFsr1::~GpuFsr1()=default;
GpuCompositeOutput GpuFsr1::enqueue_composite(void* command,const GpuCompositeOutput& scene,
    const GpuCompositeOutput& original,float sharpness) {
    if(!scene.device || scene.device!=original.device || !original.rgba || !original.packed) {
        impl_->status="Invalid FSR1 composition inputs";return {};
    }
    auto* upscaled=enqueue(scene.device,command,scene.rgba,{scene.width,scene.height},
        {original.width,original.height},sharpness);
    if(!upscaled) return {};
    auto* result=impl_->hud.restore_hud(scene.device,command,original.rgba,upscaled,
        original.packed,original.width,original.height,false);
    if(!result) {impl_->status=impl_->hud.status();return {};}
    auto output=original;output.rgba=result;return output;
}
const std::string& GpuFsr1::status() const noexcept {return impl_->status;}
void GpuFsr1::release_device() noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
void* GpuFsr1::enqueue(void* device,void* command,void* input,Fsr1Extent source,
    Fsr1Extent output,float sharpness) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(!device || !command || !input || !source.width || !source.height ||
            !output.width || !output.height || output.width<source.width || output.height<source.height ||
            !std::isfinite(sharpness)) throw std::runtime_error("Invalid FSR1 input");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(input==impl_->images[0] || input==impl_->images[1]) throw std::runtime_error("FSR1 input aliases output");
        impl_->allocate(output);
        struct Parameters {Uint32 sw,sh,ow,oh,stage;float sharpness;Uint32 padding[2];};
        static_assert(sizeof(Parameters)==32);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        for(unsigned stage=0;stage<2;++stage) {
            Parameters p{stage?output.width:source.width,stage?output.height:source.height,
                output.width,output.height,stage,sharpness,{}};
            SDL_PushGPUComputeUniformData(cmd,0,&p,sizeof(p));
            SDL_GPUStorageTextureReadWriteBinding target{};
            target.texture=impl_->images[stage];target.cycle=true;
            auto* pass=SDL_BeginGPUComputePass(cmd,&target,1,nullptr,0);
            Impl::require(pass);
            SDL_BindGPUComputePipeline(pass,impl_->pipeline);
            SDL_GPUTextureSamplerBinding sampled{stage?impl_->images[0]:static_cast<SDL_GPUTexture*>(input),impl_->sampler};
            SDL_BindGPUComputeSamplers(pass,0,&sampled,1);
            SDL_DispatchGPUCompute(pass,(output.width+7)/8,(output.height+7)/8,1);
            SDL_EndGPUComputePass(pass);
        }
        impl_->status="FSR1 EASU/RCAS recorded";
        return impl_->images[1];
    } catch(const std::exception& e) {impl_->status=e.what();return nullptr;}
#else
    (void)device;(void)command;(void)input;(void)source;(void)output;(void)sharpness;
    impl_->status="FSR1 requires SDL GPU";return nullptr;
#endif
}
}
