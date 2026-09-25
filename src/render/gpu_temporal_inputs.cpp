#include "starfox/render/gpu_temporal_inputs.hpp"
#include <cmath>
#include <stdexcept>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "shaders/generated/temporal_inputs_portable.hpp"
#include "shaders/generated/temporal_hud_portable.hpp"
#include "shaders/generated/temporal_resample_portable.hpp"
#endif
namespace starfox::render {
struct GpuTemporalInputs::Impl {
    std::string status{"Temporal textures unavailable"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};
    SDL_GPUTexture* textures[3]{};Uint32 width{},height{};
    SDL_GPUComputePipeline* hud_pipeline{};SDL_GPUTexture* hud_texture{};Uint32 hud_width{},hud_height{};
    SDL_GPUComputePipeline* resample_pipeline{};SDL_GPUTexture* resampled[3]{};Uint32 resample_width{},resample_height{};
    ~Impl() {
        if(!device) return;
        for(auto* texture:textures) if(texture) SDL_ReleaseGPUTexture(device,texture);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(hud_pipeline) SDL_ReleaseGPUComputePipeline(device,hud_pipeline);
        if(hud_texture) SDL_ReleaseGPUTexture(device,hud_texture);
        if(resample_pipeline) SDL_ReleaseGPUComputePipeline(device,resample_pipeline);
        for(auto* texture:resampled) if(texture) SDL_ReleaseGPUTexture(device,texture);
    }
    void initialize(SDL_GPUDevice* d) {
        device=d;SDL_GPUComputePipelineCreateInfo info{};
        const auto formats=SDL_GetGPUShaderFormats(d);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=temporal_inputs_shader::spirv;info.code_size=sizeof(temporal_inputs_shader::spirv);info.entrypoint="main";}
        else if(formats&SDL_GPU_SHADERFORMAT_MSL) {info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(temporal_inputs_shader::metal);info.code_size=sizeof(temporal_inputs_shader::metal)-1;info.entrypoint="main0";}
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=temporal_inputs_shader::dxil;info.code_size=sizeof(temporal_inputs_shader::dxil);info.entrypoint="main";}
        else throw std::runtime_error("Temporal textures require a GPU shader backend");
        info.num_readonly_storage_buffers=3;info.num_readwrite_storage_textures=3;info.num_uniform_buffers=1;
        info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        pipeline=SDL_CreateGPUComputePipeline(device,&info);if(!pipeline) throw std::runtime_error(SDL_GetError());
    }
    void resize(Uint32 w,Uint32 h) {
        if(width==w && height==h && textures[0] && textures[1] && textures[2]) return;
        for(auto*& texture:textures) {if(texture) SDL_ReleaseGPUTexture(device,texture);texture=nullptr;}
        width=height=0;
        for(unsigned i=0;i<3;++i) {
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
            info.format=i==1?SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT:SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
            info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width=i==2?1:w;info.height=i==2?1:h;info.layer_count_or_depth=1;info.num_levels=1;
            textures[i]=SDL_CreateGPUTexture(device,&info);if(!textures[i]) throw std::runtime_error(SDL_GetError());
        }
        width=w;height=h;
    }
#endif
};
GpuTemporalResampled GpuTemporalInputs::resample(void* device,void* command,void* color,
    const GpuTemporalTextures& source,std::uint32_t width,std::uint32_t height) {
    if(!impl_) impl_=std::make_unique<Impl>();
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(!device || source.device!=device || !command || !color || !source.depth || !source.motion ||
            !width || !height || width>source.width || height>source.height || source.width>4096 || source.height>4096)
            throw std::runtime_error("Invalid temporal resampling inputs");
        if(impl_->device && impl_->device!=device) impl_=std::make_unique<Impl>();
        auto* d=static_cast<SDL_GPUDevice*>(device);impl_->device=d;
        for(auto* output:impl_->resampled) if(output && (output==color || output==source.depth || output==source.motion))
            throw std::runtime_error("Aliased temporal resampling inputs");
        if(!impl_->resample_pipeline) {
            SDL_GPUComputePipelineCreateInfo info{};const auto formats=SDL_GetGPUShaderFormats(d);
            if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=temporal_resample_shader::spirv;info.code_size=sizeof(temporal_resample_shader::spirv);info.entrypoint="main";}
            else if(formats&SDL_GPU_SHADERFORMAT_MSL) {info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(temporal_resample_shader::metal);info.code_size=sizeof(temporal_resample_shader::metal)-1;info.entrypoint="main0";}
            else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=temporal_resample_shader::dxil;info.code_size=sizeof(temporal_resample_shader::dxil);info.entrypoint="main";}
            else throw std::runtime_error("Temporal resampling requires GPU shaders");
            info.num_readonly_storage_textures=3;info.num_readwrite_storage_textures=3;info.num_uniform_buffers=1;
            info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
            impl_->resample_pipeline=SDL_CreateGPUComputePipeline(d,&info);if(!impl_->resample_pipeline) throw std::runtime_error(SDL_GetError());
        }
        if(impl_->resample_width!=width || impl_->resample_height!=height) {
            for(auto*& texture:impl_->resampled) {if(texture) SDL_ReleaseGPUTexture(d,texture);texture=nullptr;}
            impl_->resample_width=impl_->resample_height=0;
            const SDL_GPUTextureFormat formats[]{SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_R32_FLOAT,SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT};
            for(unsigned i=0;i<3;++i) {
                SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=formats[i];
                info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_SAMPLER;
                info.width=width;info.height=height;info.layer_count_or_depth=1;info.num_levels=1;
                impl_->resampled[i]=SDL_CreateGPUTexture(d,&info);if(!impl_->resampled[i]) throw std::runtime_error(SDL_GetError());
            }
            impl_->resample_width=width;impl_->resample_height=height;
        }
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);Uint32 constants[]{source.width,source.height,width,height};
        SDL_PushGPUComputeUniformData(cb,0,constants,sizeof(constants));
        SDL_GPUStorageTextureReadWriteBinding outputs[3]{};for(unsigned i=0;i<3;++i) outputs[i].texture=impl_->resampled[i];
        auto* pass=SDL_BeginGPUComputePass(cb,outputs,3,nullptr,0);if(!pass) throw std::runtime_error(SDL_GetError());
        SDL_BindGPUComputePipeline(pass,impl_->resample_pipeline);
        SDL_GPUTexture* inputs[]{static_cast<SDL_GPUTexture*>(color),static_cast<SDL_GPUTexture*>(source.depth),static_cast<SDL_GPUTexture*>(source.motion)};
        SDL_BindGPUComputeStorageTextures(pass,0,inputs,3);SDL_DispatchGPUCompute(pass,(width+7)/8,(height+7)/8,1);SDL_EndGPUComputePass(pass);
        return {impl_->resampled[0],{device,impl_->resampled[1],impl_->resampled[2],source.exposure,width,height}};
    } catch(const std::exception& e) {impl_->status=e.what();}
#else
    (void)device;(void)command;(void)color;(void)source;(void)width;(void)height;
#endif
    return {};
}
void* GpuTemporalInputs::restore_hud(void* device,void* command,void* original,void* reconstructed,
    void* packed,std::uint32_t width,std::uint32_t height,bool preserve_artwork) {
    if(!impl_) impl_=std::make_unique<Impl>();
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(!device || !command || !original || !reconstructed || !packed || !width || !height || width>4096 || height>4096 ||
            original==impl_->hud_texture || reconstructed==impl_->hud_texture) throw std::runtime_error("Invalid temporal HUD inputs");
        if(impl_->device && impl_->device!=device) impl_=std::make_unique<Impl>();
        auto* d=static_cast<SDL_GPUDevice*>(device);impl_->device=d;
        if(!impl_->hud_pipeline) {
            SDL_GPUComputePipelineCreateInfo info{};const auto formats=SDL_GetGPUShaderFormats(d);
            if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=temporal_hud_shader::spirv;info.code_size=sizeof(temporal_hud_shader::spirv);info.entrypoint="main";}
            else if(formats&SDL_GPU_SHADERFORMAT_MSL) {info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(temporal_hud_shader::metal);info.code_size=sizeof(temporal_hud_shader::metal)-1;info.entrypoint="main0";}
            else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=temporal_hud_shader::dxil;info.code_size=sizeof(temporal_hud_shader::dxil);info.entrypoint="main";}
            else throw std::runtime_error("HUD reconstruction requires GPU shaders");
            info.num_readonly_storage_textures=2;info.num_readonly_storage_buffers=1;info.num_readwrite_storage_textures=1;info.num_uniform_buffers=1;
            info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
            impl_->hud_pipeline=SDL_CreateGPUComputePipeline(d,&info);if(!impl_->hud_pipeline) throw std::runtime_error(SDL_GetError());
        }
        if(!impl_->hud_texture || impl_->hud_width!=width || impl_->hud_height!=height) {
            if(impl_->hud_texture) SDL_ReleaseGPUTexture(d,impl_->hud_texture);
            impl_->hud_texture=nullptr;
            SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_SAMPLER;
            info.width=width;info.height=height;info.layer_count_or_depth=1;info.num_levels=1;
            impl_->hud_texture=SDL_CreateGPUTexture(d,&info);if(!impl_->hud_texture) throw std::runtime_error(SDL_GetError());
            impl_->hud_width=width;impl_->hud_height=height;
        }
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);Uint32 constants[]{width,height,preserve_artwork?1U:0U,0};
        SDL_PushGPUComputeUniformData(cb,0,constants,sizeof(constants));
        SDL_GPUStorageTextureReadWriteBinding out{};out.texture=impl_->hud_texture;
        auto* pass=SDL_BeginGPUComputePass(cb,&out,1,nullptr,0);if(!pass) throw std::runtime_error(SDL_GetError());
        SDL_BindGPUComputePipeline(pass,impl_->hud_pipeline);
        SDL_GPUTexture* inputs[]{static_cast<SDL_GPUTexture*>(original),static_cast<SDL_GPUTexture*>(reconstructed)};
        auto* buffer=static_cast<SDL_GPUBuffer*>(packed);
        SDL_BindGPUComputeStorageTextures(pass,0,inputs,2);SDL_BindGPUComputeStorageBuffers(pass,0,&buffer,1);
        SDL_DispatchGPUCompute(pass,(width+7)/8,(height+7)/8,1);SDL_EndGPUComputePass(pass);
        return impl_->hud_texture;
    } catch(const std::exception& e) {impl_->status=e.what();}
#else
    (void)device;(void)command;(void)original;(void)reconstructed;(void)packed;(void)width;(void)height;(void)preserve_artwork;
#endif
    return nullptr;
}
GpuTemporalInputs::GpuTemporalInputs():impl_(std::make_unique<Impl>()) {}
GpuTemporalInputs::~GpuTemporalInputs()=default;
void GpuTemporalInputs::release_device() noexcept {impl_.reset();}
const std::string& GpuTemporalInputs::status() const {static const std::string empty{"Temporal textures released"};return impl_?impl_->status:empty;}
GpuTemporalTextures GpuTemporalInputs::enqueue(void* device,void* command,void* depth,void* motion,
    std::uint32_t width,std::uint32_t height,float near_plane,float far_plane,bool reset,const TemporalGroundInputs* ground) {
    if(!impl_) impl_=std::make_unique<Impl>();
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(!device || !command || !depth || (!reset && (!motion || depth==motion)) || !width || !height || width>4096 || height>4096 ||
            !(near_plane>0) || !(far_plane>near_plane) || !std::isfinite(far_plane)) throw std::runtime_error("Invalid temporal texture inputs");
        if(ground) {
            if(!ground->coverage || ground->coverage==depth || ground->coverage==motion)
                throw std::runtime_error("Invalid terrain coverage");
            for(const auto* values:{&ground->plane,&ground->projection,&ground->previous_projection})
                for(float v:*values) if(!std::isfinite(v)) throw std::runtime_error("Invalid terrain projection");
            for(float v:ground->current_to_previous) if(!std::isfinite(v)) throw std::runtime_error("Invalid terrain history");
            for(float v:ground->raster_jitter) if(!std::isfinite(v)) throw std::runtime_error("Invalid terrain jitter");
            const auto& matrix=ground->current_to_previous;
            if(matrix[3]!=0 || matrix[7]!=0 || matrix[11]!=0 || matrix[15]!=1)
                throw std::runtime_error("Terrain history must be affine");
            if(ground->plane[0]==0 && ground->plane[1]==0 && ground->plane[2]==0)
                throw std::runtime_error("Terrain plane has no normal");
            if(ground->projection[0]<=0 || ground->projection[1]<=0 || ground->previous_projection[0]<=0 || ground->previous_projection[1]<=0)
                throw std::runtime_error("Invalid terrain focal length");
        }
        if(impl_->device && impl_->device!=device) impl_=std::make_unique<Impl>();
        if(!impl_->pipeline) impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        impl_->resize(width,height);
        struct Constants {
            Uint32 width,height,reset,pad;float near_plane,far_plane,pad1,pad2;
            Uint32 ground_enabled,ground_previous_valid;std::array<float,2> ground_jitter;
            std::array<float,4> plane,projection,previous_projection;
            std::array<float,16> previous;
        } constants{};
        constants.width=width;constants.height=height;constants.reset=reset;
        constants.near_plane=near_plane;constants.far_plane=far_plane;
        if(ground) {
            constants.ground_enabled=ground->packed_coverage?2:1;constants.ground_previous_valid=ground->previous_valid;
            constants.ground_jitter=ground->raster_jitter;
            constants.plane=ground->plane;constants.projection=ground->projection;
            constants.previous_projection=ground->previous_projection;constants.previous=ground->current_to_previous;
        }
        auto* cb=static_cast<SDL_GPUCommandBuffer*>(command);
        SDL_PushGPUComputeUniformData(cb,0,&constants,sizeof(constants));
        SDL_GPUStorageTextureReadWriteBinding outputs[3]{};for(unsigned i=0;i<3;++i) outputs[i].texture=impl_->textures[i];
        auto* pass=SDL_BeginGPUComputePass(cb,outputs,3,nullptr,0);if(!pass) throw std::runtime_error(SDL_GetError());
        SDL_BindGPUComputePipeline(pass,impl_->pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(depth),static_cast<SDL_GPUBuffer*>(motion?motion:depth),static_cast<SDL_GPUBuffer*>(ground?ground->coverage:depth)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,3);SDL_DispatchGPUCompute(pass,(width+7)/8,(height+7)/8,1);SDL_EndGPUComputePass(pass);
        impl_->status="Temporal textures GPU resident";
        return {device,impl_->textures[0],impl_->textures[1],impl_->textures[2],width,height};
    } catch(const std::exception& e) {impl_->status=e.what();}
#else
    (void)device;(void)command;(void)depth;(void)motion;(void)width;(void)height;(void)near_plane;(void)far_plane;(void)reset;(void)ground;
#endif
    return {};
}
}
