#include "starfox/render/gpu_scalefx.hpp"
#include <cstring>
#include <stdexcept>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include "shaders/generated/scalefx_portable.hpp"
#endif

namespace starfox::render {
struct GpuScaleFx::Impl {
    std::string status{"ScaleFX GPU not initialized"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};
    SDL_GPUComputePipeline* pipelines[6]{};
    SDL_GPUBuffer* texture_input{};
    Uint32 texture_capacity{};
    SDL_GPUBuffer* buffers[5]{};
    Uint32 capacity{};
    ~Impl() { release(); }
    void release() noexcept {
        if(device) {
            for(auto*& p:pipelines) {if(p) SDL_ReleaseGPUComputePipeline(device,p);p=nullptr;}
            for(auto*& b:buffers) {if(b) SDL_ReleaseGPUBuffer(device,b);b=nullptr;}
            if(texture_input) SDL_ReleaseGPUBuffer(device,texture_input);
        }
        texture_input=nullptr;texture_capacity=0;
        device=nullptr;capacity=0;
    }
    static void require(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}
    void initialize(SDL_GPUDevice* requested) {
        if(device && device!=requested)
            throw std::runtime_error("ScaleFX requires release before switching GPU devices");
        if(device) return;
        device=requested;
        try {
            using namespace scalefx_shader;
            const unsigned char* spv[]{spirv0,spirv1,spirv2,spirv3,spirv4,spirv5};
            const size_t sizes[]{sizeof(spirv0),sizeof(spirv1),sizeof(spirv2),sizeof(spirv3),sizeof(spirv4),sizeof(spirv5)};
#if defined(__APPLE__)
            const char* msl[]{metal0,metal1,metal2,metal3,metal4,metal5};
#endif
#if defined(_WIN32)
            const unsigned char* native[]{dxil0,dxil1,dxil2,dxil3,dxil4,dxil5};
            const size_t native_sizes[]{sizeof(dxil0),sizeof(dxil1),sizeof(dxil2),sizeof(dxil3),sizeof(dxil4),sizeof(dxil5)};
#endif
            const auto formats=SDL_GetGPUShaderFormats(device);
            if(!(formats&(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_MSL|SDL_GPU_SHADERFORMAT_DXIL)))
                throw std::runtime_error("ScaleFX requires Vulkan, Metal or D3D12");
            for(unsigned i=0;i<6;++i) {
                SDL_GPUComputePipelineCreateInfo info{};
                if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
                    info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=spv[i];info.code_size=sizes[i];info.entrypoint="main";
                }
#if defined(__APPLE__)
                else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
                    info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(msl[i]);
                    info.code_size=std::strlen(msl[i]);info.entrypoint="main0";
                }
#endif
#if defined(_WIN32)
                else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
                    info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=native[i];info.code_size=native_sizes[i];info.entrypoint="main";
                }
#endif
                else throw std::runtime_error("ScaleFX native shader format unsupported");
                info.num_readonly_storage_buffers=i==5?0:3;info.num_readwrite_storage_buffers=1;
                info.num_readonly_storage_textures=i==5?1:0;
                info.num_uniform_buffers=1;info.threadcount_x=8;info.threadcount_y=8;info.threadcount_z=1;
                pipelines[i]=SDL_CreateGPUComputePipeline(device,&info);require(pipelines[i]);
            }
        } catch(...) {release();throw;}
    }
    void allocate(Uint32 bytes) {
        if(capacity>=bytes) return;
        SDL_GPUBuffer* replacement[5]{};
        for(unsigned i=0;i<5;++i) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,i==4?bytes*9:bytes,0};
            replacement[i]=SDL_CreateGPUBuffer(device,&info);
            if(!replacement[i]) {
                for(auto* b:replacement) if(b) SDL_ReleaseGPUBuffer(device,b);
                throw std::runtime_error(SDL_GetError());
            }
        }
        for(unsigned i=0;i<5;++i) {
            if(buffers[i]) SDL_ReleaseGPUBuffer(device,buffers[i]);
            buffers[i]=replacement[i];
        }
        capacity=bytes;
    }
#endif
};

GpuScaleFx::GpuScaleFx():impl_(std::make_unique<Impl>()) {}
GpuScaleFx::~GpuScaleFx()=default;
GpuScaleFxOutput GpuScaleFx::enqueue_texture(void* device,void* command,void* texture,
    std::uint32_t width,std::uint32_t height) {
    const auto count=std::uint64_t(width)*height;
    if(!device || !command || !texture || !width || !height || count>UINT32_MAX/(16U*9U)) {
        impl_->status="Invalid ScaleFX texture or dimensions";return {};
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        const auto bytes=static_cast<Uint32>(count*16);
        if(impl_->texture_capacity<bytes) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,bytes,0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->texture_input) SDL_ReleaseGPUBuffer(impl_->device,impl_->texture_input);
            impl_->texture_input=replacement;impl_->texture_capacity=bytes;
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const Uint32 settings[]{width,height,0,0};
        SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding output{};
        output.buffer=impl_->texture_input;output.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&output,1);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->pipelines[5]);
        auto* image=static_cast<SDL_GPUTexture*>(texture);
        SDL_BindGPUComputeStorageTextures(pass,0,&image,1);
        SDL_DispatchGPUCompute(pass,(width+7)/8,(height+7)/8,1);
        SDL_EndGPUComputePass(pass);
        return enqueue(device,command,impl_->texture_input,width,height);
    } catch(const std::exception& e) {impl_->status=e.what();return {};}
#else
    impl_->status="ScaleFX GPU backend unavailable";return {};
#endif
}
const std::string& GpuScaleFx::status() const noexcept {return impl_->status;}
void GpuScaleFx::release_device() noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
GpuScaleFxOutput GpuScaleFx::enqueue(void* device,void* command,void* input,
    std::uint32_t width,std::uint32_t height) {
    const auto count=std::uint64_t(width)*height;
    if(!device || !command || !input || !width || !height || count>UINT32_MAX/(16U*9U)) {
        impl_->status="Invalid ScaleFX input or dimensions";return {};
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        for(auto* buffer:impl_->buffers)
            if(buffer==input) throw std::runtime_error("ScaleFX input aliases its scratch buffers");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        impl_->allocate(static_cast<Uint32>(count*16));
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        auto* original=static_cast<SDL_GPUBuffer*>(input);
        const Uint32 settings[]{width,height,0,0};
        SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        for(unsigned i=0;i<5;++i) {
            SDL_GPUStorageBufferReadWriteBinding output{};
            output.buffer=impl_->buffers[i];output.cycle=true;
            auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&output,1);Impl::require(pass);
            SDL_BindGPUComputePipeline(pass,impl_->pipelines[i]);
            SDL_GPUBuffer* inputs[]{i?impl_->buffers[i-1]:original,
                i?impl_->buffers[0]:original,original};
            SDL_BindGPUComputeStorageBuffers(pass,0,inputs,3);
            const unsigned scale=i==4?3:1;
            SDL_DispatchGPUCompute(pass,(width*scale+7)/8,(height*scale+7)/8,1);
            SDL_EndGPUComputePass(pass);
        }
        impl_->status="ScaleFX GPU resident";
        return {impl_->buffers[4],width*3,height*3};
    } catch(const std::exception& e) {impl_->status=e.what();return {};}
#else
    impl_->status="ScaleFX GPU backend unavailable";return {};
#endif
}
}
