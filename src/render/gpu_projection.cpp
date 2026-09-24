#include "starfox/render/gpu_projection.hpp"
#include "starfox/render/temporal_jitter.hpp"
#include "starfox/render/grid_projection.hpp"
#include <cmath>
#include <bit>
#include <algorithm>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "shaders/generated/projection_portable.hpp"
#include "shaders/generated/motion_portable.hpp"
#include "shaders/generated/visibility_portable.hpp"
#include "shaders/generated/transform_portable.hpp"
#include "shaders/generated/continuous_portable.hpp"
#include "shaders/generated/continuous_visibility_portable.hpp"
#include "shaders/generated/axis_portable.hpp"
#include "shaders/generated/grid_portable.hpp"
#include "shaders/generated/grid_spans_portable.hpp"
#include "shaders/generated/dust_portable.hpp"
#include "shaders/generated/particle_portable.hpp"
#include "shaders/generated/projected_text_portable.hpp"
#include <cstring>
#include <stdexcept>
#endif
namespace starfox::render {
struct GpuProjection::Impl {
    std::string status{"Native GPU projection unavailable"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};
    SDL_GPUComputePipeline* motion_pipeline{};
    SDL_GPUBuffer* motion_output{};
    std::uint32_t motion_capacity{};
    SDL_GPUComputePipeline* text_pipeline{};
    SDL_GPUBuffer *text_glyphs{},*text_work{},*text_pixels{};
    SDL_GPUTransferBuffer* text_upload{};
    std::uint32_t text_capacity{};
    SDL_GPUComputePipeline* pipeline{};
    SDL_GPUBuffer* output{};
    std::uint32_t capacity{};
    SDL_GPUComputePipeline* visibility_pipeline{};
    SDL_GPUBuffer* visibility_output{};
    std::uint32_t visibility_capacity{},point_count{};
    SDL_GPUComputePipeline* transform_pipeline{};
    SDL_GPUComputePipeline* continuous_pipeline{};
    SDL_GPUComputePipeline* continuous_visibility_pipeline{};
    SDL_GPUComputePipeline* axis_pipeline{};
    SDL_GPUComputePipeline* grid_pipeline{};
    SDL_GPUComputePipeline* dust_pipeline{};
    SDL_GPUComputePipeline* particle_pipeline{};
    SDL_GPUBuffer* particle_output{};
    SDL_GPUBuffer* particle_input{};
    SDL_GPUTransferBuffer* particle_upload{};
    void* particle_command{};
    std::uint32_t particle_count{},particle_height{};
    SDL_GPUBuffer* dust_output{};
    SDL_GPUBuffer* dust_input{};
    SDL_GPUBuffer* dust_colours{};
    SDL_GPUTransferBuffer* dust_upload{};
    void* dust_command{};
    std::uint32_t dust_height{},dust_count{};
    SDL_GPUBuffer* grid_output{};
    void* grid_command{};
    std::uint32_t grid_height{};
    SDL_GPUComputePipeline* grid_spans_pipeline{};
    SDL_GPUBuffer* grid_spans{};
    std::uint32_t grid_spans_capacity{};
    SDL_GPUBuffer* axis_output{};
    SDL_GPUBuffer* axis_residuals{};
    std::uint32_t continuous_count{};
    SDL_GPUBuffer* transformed{};
    SDL_GPUBuffer* continuous_residuals{};
    std::uint32_t residual_capacity{};
    std::uint32_t transform_capacity{};
    ~Impl(){release();}
    static void require(bool ok){if(!ok) throw std::runtime_error(SDL_GetError());}
    void release() noexcept {
        if(motion_pipeline) SDL_ReleaseGPUComputePipeline(device,motion_pipeline);
        if(motion_output) SDL_ReleaseGPUBuffer(device,motion_output);
        motion_pipeline=nullptr;motion_output=nullptr;motion_capacity=0;
        if(text_pipeline) SDL_ReleaseGPUComputePipeline(device,text_pipeline);
        if(text_glyphs) SDL_ReleaseGPUBuffer(device,text_glyphs);
        if(text_work) SDL_ReleaseGPUBuffer(device,text_work);
        if(text_pixels) SDL_ReleaseGPUBuffer(device,text_pixels);
        if(text_upload) SDL_ReleaseGPUTransferBuffer(device,text_upload);
        text_pipeline=nullptr;text_glyphs=text_work=text_pixels=nullptr;text_upload=nullptr;text_capacity=0;
        if(particle_input) SDL_ReleaseGPUBuffer(device,particle_input);
        if(particle_upload) SDL_ReleaseGPUTransferBuffer(device,particle_upload);
        particle_input=nullptr;particle_upload=nullptr;
        if(particle_output) SDL_ReleaseGPUBuffer(device,particle_output);
        if(particle_pipeline) SDL_ReleaseGPUComputePipeline(device,particle_pipeline);
        particle_output=nullptr;particle_pipeline=nullptr;
        particle_command=nullptr;particle_count=particle_height=0;
        if(dust_input) SDL_ReleaseGPUBuffer(device,dust_input);
        if(dust_colours) SDL_ReleaseGPUBuffer(device,dust_colours);
        if(dust_upload) SDL_ReleaseGPUTransferBuffer(device,dust_upload);
        dust_input=dust_colours=nullptr;dust_upload=nullptr;
        if(dust_output) SDL_ReleaseGPUBuffer(device,dust_output);
        if(dust_pipeline) SDL_ReleaseGPUComputePipeline(device,dust_pipeline);
        dust_output=nullptr;dust_pipeline=nullptr;
        dust_command=nullptr;dust_height=dust_count=0;
        if(grid_spans) SDL_ReleaseGPUBuffer(device,grid_spans);
        if(grid_spans_pipeline) SDL_ReleaseGPUComputePipeline(device,grid_spans_pipeline);
        grid_spans=nullptr;grid_spans_pipeline=nullptr;grid_spans_capacity=0;
        if(grid_output) SDL_ReleaseGPUBuffer(device,grid_output);
        if(grid_pipeline) SDL_ReleaseGPUComputePipeline(device,grid_pipeline);
        grid_output=nullptr;grid_pipeline=nullptr;
        grid_command=nullptr;grid_height=0;
        if(continuous_residuals) SDL_ReleaseGPUBuffer(device,continuous_residuals);
        continuous_residuals=nullptr;residual_capacity=0;
        if(axis_output) SDL_ReleaseGPUBuffer(device,axis_output);
        if(axis_residuals) SDL_ReleaseGPUBuffer(device,axis_residuals);
        axis_residuals=nullptr;
        if(axis_pipeline) SDL_ReleaseGPUComputePipeline(device,axis_pipeline);
        axis_output=nullptr;axis_pipeline=nullptr;
        if(transformed) SDL_ReleaseGPUBuffer(device,transformed);
        if(transform_pipeline) SDL_ReleaseGPUComputePipeline(device,transform_pipeline);
        if(continuous_pipeline) SDL_ReleaseGPUComputePipeline(device,continuous_pipeline);
        if(continuous_visibility_pipeline) SDL_ReleaseGPUComputePipeline(device,continuous_visibility_pipeline);
        continuous_visibility_pipeline=nullptr;continuous_count=0;
        continuous_pipeline=nullptr;
        transformed=nullptr;transform_pipeline=nullptr;transform_capacity=0;
        if(visibility_output) SDL_ReleaseGPUBuffer(device,visibility_output);
        if(visibility_pipeline) SDL_ReleaseGPUComputePipeline(device,visibility_pipeline);
        visibility_output=nullptr;visibility_pipeline=nullptr;visibility_capacity=point_count=0;
        if(output) SDL_ReleaseGPUBuffer(device,output);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        output=nullptr;pipeline=nullptr;device=nullptr;capacity=0;
    }
    void initialize(SDL_GPUDevice* next) {
        if(device==next && pipeline && visibility_pipeline && transform_pipeline && continuous_pipeline && continuous_visibility_pipeline) return;
        release();device=next;
        const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        if(!spirv && !dxil && !(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_MSL))
            throw std::runtime_error("Native projection requires Vulkan, Metal or D3D12");
        SDL_GPUComputePipelineCreateInfo info{};
        info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
        info.code=spirv?projection_shader::spirv:dxil?projection_shader::dxil:reinterpret_cast<const Uint8*>(projection_shader::metal);
        info.code_size=spirv?sizeof(projection_shader::spirv):dxil?sizeof(projection_shader::dxil):std::strlen(projection_shader::metal);
        info.entrypoint=(spirv||dxil)?"main":"main0";
        info.num_readonly_storage_buffers=1;info.num_readwrite_storage_buffers=1;
        info.num_uniform_buffers=1;info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
        pipeline=SDL_CreateGPUComputePipeline(device,&info);require(pipeline);
        info.code=spirv?visibility_shader::spirv:dxil?visibility_shader::dxil:reinterpret_cast<const Uint8*>(visibility_shader::metal);
        info.code_size=spirv?sizeof(visibility_shader::spirv):dxil?sizeof(visibility_shader::dxil):std::strlen(visibility_shader::metal);
        info.num_readonly_storage_buffers=2;
        visibility_pipeline=SDL_CreateGPUComputePipeline(device,&info);require(visibility_pipeline);
        info.code=spirv?transform_shader::spirv:dxil?transform_shader::dxil:reinterpret_cast<const Uint8*>(transform_shader::metal);
        info.code_size=spirv?sizeof(transform_shader::spirv):dxil?sizeof(transform_shader::dxil):std::strlen(transform_shader::metal);
        transform_pipeline=SDL_CreateGPUComputePipeline(device,&info);require(transform_pipeline);
        info.code=spirv?continuous_shader::spirv:dxil?continuous_shader::dxil:reinterpret_cast<const Uint8*>(continuous_shader::metal);
        info.code_size=spirv?sizeof(continuous_shader::spirv):dxil?sizeof(continuous_shader::dxil):std::strlen(continuous_shader::metal);
        info.num_readwrite_storage_buffers=2;
        continuous_pipeline=SDL_CreateGPUComputePipeline(device,&info);require(continuous_pipeline);
        info.num_readwrite_storage_buffers=1;
        info.code=spirv?continuous_visibility_shader::spirv:dxil?continuous_visibility_shader::dxil:reinterpret_cast<const Uint8*>(continuous_visibility_shader::metal);
        info.code_size=spirv?sizeof(continuous_visibility_shader::spirv):dxil?sizeof(continuous_visibility_shader::dxil):std::strlen(continuous_visibility_shader::metal);
        continuous_visibility_pipeline=SDL_CreateGPUComputePipeline(device,&info);require(continuous_visibility_pipeline);
    }
#endif
};
GpuProjection::GpuProjection():impl_(std::make_unique<Impl>()){}
GpuProjection::~GpuProjection()=default;
void* GpuProjection::enqueue_motion(void* device,void* command,void* current_points,
    void* previous_points,std::uint32_t count,float scale_x,float scale_y,bool reset_history) {
    return enqueue_motion_impl(device,command,current_points,previous_points,count,scale_x,scale_y,reset_history,nullptr);
}
void* GpuProjection::enqueue_motion_surface(void* device,void* command,void* camera_depth,
    const MotionSurfaceSettings& settings) {
    bool valid=settings.width && settings.height && settings.width<=4096 && settings.height<=4096
        && std::uint64_t(settings.width)*settings.height<=65535U*64U;
    for(const auto* row:{settings.current_projection,settings.previous_projection,
        settings.previous_row0,settings.previous_row1,settings.previous_row2})
        for(unsigned i=0;i<4;++i) valid=valid && std::isfinite(row[i]);
    valid=valid && settings.current_projection[0]>0 && settings.current_projection[1]>0
        && settings.previous_projection[0]>0 && settings.previous_projection[1]>0
        && std::isfinite(settings.jitter_x) && std::isfinite(settings.jitter_y)
        && std::isfinite(settings.previous_near) && settings.previous_near>=0;
    if(!valid) {impl_->status="Invalid per-pixel motion settings";return nullptr;}
    return enqueue_motion_impl(device,command,camera_depth,camera_depth,settings.width*settings.height,
        1,1,settings.reset_history!=0,&settings);
}
void* GpuProjection::enqueue_motion_impl(void* device,void* command,void* current_points,
    void* previous_points,std::uint32_t count,float scale_x,float scale_y,bool reset_history,
    const MotionSurfaceSettings* surface) {
    if(!device || !command || !current_points || !previous_points || !count
        || count>65535U*64U || !std::isfinite(scale_x) || !std::isfinite(scale_y)
        || scale_x<=0 || scale_y<=0) {
        impl_->status="Invalid GPU motion input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(current_points==impl_->motion_output || previous_points==impl_->motion_output)
            throw std::runtime_error("GPU motion input aliases its output");
        if(!impl_->motion_pipeline) {
            const auto formats=SDL_GetGPUShaderFormats(impl_->device);
            const bool spv=(formats&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
            const bool dxil=(formats&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            info.format=spv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
            info.code=spv?motion_shader::spirv:dxil?motion_shader::dxil:reinterpret_cast<const Uint8*>(motion_shader::metal);
            info.code_size=spv?sizeof(motion_shader::spirv):dxil?sizeof(motion_shader::dxil):std::strlen(motion_shader::metal);
            info.entrypoint=(spv||dxil)?"main":"main0";
            info.num_readonly_storage_buffers=2;info.num_readwrite_storage_buffers=1;
            info.num_uniform_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            impl_->motion_pipeline=SDL_CreateGPUComputePipeline(impl_->device,&info);
            Impl::require(impl_->motion_pipeline);
        }
        if(count>impl_->motion_capacity) {
            if(impl_->motion_output) SDL_ReleaseGPUBuffer(impl_->device,impl_->motion_output);
            impl_->motion_output=nullptr;impl_->motion_capacity=0;
            SDL_GPUBufferCreateInfo info{};
            info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
            info.size=count*16;
            impl_->motion_output=SDL_CreateGPUBuffer(impl_->device,&info);
            Impl::require(impl_->motion_output);impl_->motion_capacity=count;
        }
        struct Settings {std::uint32_t count,reset;float x,y;MotionSurfaceSettings surface;};
        Settings settings{count,reset_history?1U:0U,scale_x,scale_y,{}};
        static_assert(sizeof(Settings)==128);
        if(surface) settings.surface=*surface;
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        SDL_PushGPUComputeUniformData(cmd,0,&settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding out{};
        out.buffer=impl_->motion_output;out.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&out,1);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->motion_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(current_points),static_cast<SDL_GPUBuffer*>(previous_points)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,2);
        SDL_DispatchGPUCompute(pass,(count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        impl_->status=surface?"GPU-resident per-pixel motion":"GPU-resident vertex motion";
        return impl_->motion_output;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    impl_->status="GPU motion unavailable";return nullptr;
#endif
}
void* GpuProjection::enqueue_text(void* device,void* command,const ScaledTextRenderer::ProjectedFrame& frame,
    std::uint32_t width,std::uint32_t height,std::uint32_t scale,std::uint8_t tag,float eye_x,float convergence,
    std::array<std::uint32_t,2> logical_viewport,std::array<float,2> jitter) {
    if(!valid_raster_jitter(jitter)) {impl_->status="Invalid text jitter";return nullptr;}
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        const bool custom=logical_viewport[0] || logical_viewport[1];
        if(!device || !command || !width || !height || width>8192 || height>8192
            || !scale || scale>4 || (!custom && (width%scale || height%scale))
            || (custom && (!logical_viewport[0] || !logical_viewport[1]
                || logical_viewport[0]>2048 || logical_viewport[1]>2048)) || frame.glyphs.size()>256
            || frame.character_size< -1 || frame.character_size>254
            || !std::isfinite(frame.pose.x) || !std::isfinite(frame.pose.y) || !std::isfinite(frame.pose.z)
            || std::abs(frame.pose.x)>1e6 || std::abs(frame.pose.y)>1e6
            || !std::isfinite(eye_x) || std::abs(eye_x)>32767 || !std::isfinite(convergence) || convergence<1)
            throw std::runtime_error("Invalid GPU text layout");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(!impl_->text_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
            info.code=spirv?projected_text_shader::spirv:dxil?projected_text_shader::dxil:reinterpret_cast<const Uint8*>(projected_text_shader::metal);
            info.code_size=spirv?sizeof(projected_text_shader::spirv):dxil?sizeof(projected_text_shader::dxil):std::strlen(projected_text_shader::metal);
            info.entrypoint=(spirv||dxil)?"main":"main0";info.num_uniform_buffers=1;
            info.num_readonly_storage_buffers=info.num_readwrite_storage_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            impl_->text_pipeline=SDL_CreateGPUComputePipeline(impl_->device,&info);Impl::require(impl_->text_pipeline);
        }
        if(!impl_->text_glyphs) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,256*16*4,0};
            impl_->text_glyphs=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->text_glyphs);
        }
        if(!impl_->text_upload) {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,256*16*4,0};
            impl_->text_upload=SDL_CreateGPUTransferBuffer(impl_->device,&info);Impl::require(impl_->text_upload);
        }
        const auto bytes=width*height*4;
        if(bytes>impl_->text_capacity || !impl_->text_work || !impl_->text_pixels) {
            if(impl_->text_work) SDL_ReleaseGPUBuffer(impl_->device,impl_->text_work);
            if(impl_->text_pixels) SDL_ReleaseGPUBuffer(impl_->device,impl_->text_pixels);
            impl_->text_work=impl_->text_pixels=nullptr;impl_->text_capacity=0;
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,bytes+16,0};
            impl_->text_work=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->text_work);
            info.size=bytes;impl_->text_pixels=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->text_pixels);
            impl_->text_capacity=bytes;
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        if(!frame.glyphs.empty()) {
            auto* mapped=static_cast<Uint32*>(SDL_MapGPUTransferBuffer(impl_->device,impl_->text_upload,true));Impl::require(mapped);
            for(std::size_t i=0;i<frame.glyphs.size();++i) for(unsigned row=0;row<16;++row) mapped[i*16+row]=frame.glyphs[i][row];
            SDL_UnmapGPUTransferBuffer(impl_->device,impl_->text_upload);
            auto* copy=SDL_BeginGPUCopyPass(cmd);Impl::require(copy);
            SDL_GPUTransferBufferLocation from{impl_->text_upload,0};
            SDL_GPUBufferRegion to{impl_->text_glyphs,0,Uint32(frame.glyphs.size()*64)};
            SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        }
        struct Settings {
            double x,y,z;Uint32 width,height,scale,count;std::int32_t size;Uint32 colour;
            float eye,convergence;Uint32 stage,tag;
            Uint32 reference_width,reference_height;std::array<float,2> jitter;
        } settings{frame.pose.x,frame.pose.y,frame.pose.z,width,height,scale,Uint32(frame.glyphs.size()),
            frame.character_size,frame.colour,eye_x,convergence,0,tag,
            custom?logical_viewport[0]*scale:width,custom?logical_viewport[1]*scale:height,jitter};
        static_assert(sizeof(Settings)==80);
        for(unsigned stage=0;stage<2;++stage) {
            settings.stage=stage;SDL_PushGPUComputeUniformData(cmd,0,&settings,sizeof(settings));
            SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=impl_->text_work;binding.cycle=stage==0;
            auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);Impl::require(pass);
            SDL_BindGPUComputePipeline(pass,impl_->text_pipeline);
            SDL_BindGPUComputeStorageBuffers(pass,0,&impl_->text_glyphs,1);
            SDL_DispatchGPUCompute(pass,stage?(width*height+63)/64:1,1,1);SDL_EndGPUComputePass(pass);
        }
        auto* copy=SDL_BeginGPUCopyPass(cmd);Impl::require(copy);
        SDL_GPUBufferLocation from{impl_->text_work,16},to{impl_->text_pixels,0};
        SDL_CopyGPUBufferToBuffer(copy,&from,&to,bytes,true);SDL_EndGPUCopyPass(copy);
        impl_->status="Projected text GPU resident";return impl_->text_pixels;
    } catch(const std::exception& error) {impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuProjection::enqueue_particle_frame(void* device,void* command,const ParticleRenderer::OwnerFrame& frame,
    std::uint32_t width,std::uint32_t height,float eye_x,float convergence) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->particle_command=nullptr;impl_->particle_count=0;
    try {
        if(!device || !command || width<2 || height<2 || width>32767 || height>32767
            || frame.particles.size()>300 || !std::isfinite(frame.alpha)
            || !std::isfinite(frame.pose.x) || !std::isfinite(frame.pose.y) || !std::isfinite(frame.pose.z)
            || std::abs(frame.pose.x)>1e9 || std::abs(frame.pose.y)>1e9 || std::abs(frame.pose.z)>1e9
            || !std::isfinite(eye_x) || std::abs(eye_x)>32767
            || !std::isfinite(convergence) || convergence<1)
            throw std::runtime_error("Invalid particle frame layout");
        std::array<std::array<std::int32_t,8>,300> packed{};
        std::uint32_t count=0;
        for(const auto& particle:frame.particles) {
            if(!particle.life || particle.owner!=frame.owner) continue;
            packed[count++]={particle.x,particle.y,particle.z,
                std::int32_t(std::uint8_t(frame.colour_base+particle.colour))|((particle.flags&4)?256:0),
                particle.previous_x,particle.previous_y,particle.previous_z,0};
        }
        if(!count) throw std::runtime_error("Empty particle frame");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(!impl_->particle_input) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,300*32,0};
            impl_->particle_input=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->particle_input);
        }
        if(!impl_->particle_upload) {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,300*32,0};
            impl_->particle_upload=SDL_CreateGPUTransferBuffer(impl_->device,&info);Impl::require(impl_->particle_upload);
        }
        auto* mapped=SDL_MapGPUTransferBuffer(impl_->device,impl_->particle_upload,true);Impl::require(mapped);
        const auto bytes=count*32;
        std::memcpy(mapped,packed.data(),bytes);SDL_UnmapGPUTransferBuffer(impl_->device,impl_->particle_upload);
        auto* copy=SDL_BeginGPUCopyPass(static_cast<SDL_GPUCommandBuffer*>(command));Impl::require(copy);
        SDL_GPUTransferBufferLocation source{impl_->particle_upload,0};
        SDL_GPUBufferRegion destination{impl_->particle_input,0,bytes};
        SDL_UploadToGPUBuffer(copy,&source,&destination,true);SDL_EndGPUCopyPass(copy);
        ParticleSettings settings;
        settings.owner_x=frame.pose.x;settings.owner_y=frame.pose.y;settings.owner_z=frame.pose.z;
        settings.alpha=std::clamp(frame.alpha,0.,1.);settings.width=width;settings.height=height;
        settings.count=count;settings.eye_x=eye_x;settings.convergence=convergence;
        return enqueue_particles(device,command,impl_->particle_input,settings);
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuProjection::enqueue_particles(void* device,void* command,void* points,const ParticleSettings& settings) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->particle_command=nullptr;impl_->particle_count=0;
#endif
    if(!device || !command || !points || !settings.count || settings.count>300
        || settings.width<2 || settings.height<2 || settings.width>32767 || settings.height>32767
        || !std::isfinite(settings.alpha) || settings.alpha<0 || settings.alpha>1
        || !std::isfinite(settings.owner_x) || !std::isfinite(settings.owner_y) || !std::isfinite(settings.owner_z)
        || std::abs(settings.owner_x)>1000000000. || std::abs(settings.owner_y)>1000000000. || std::abs(settings.owner_z)>1000000000.
        || !std::isfinite(settings.eye_x) || std::abs(settings.eye_x)>32767
        || !std::isfinite(settings.convergence) || settings.convergence<1) {
        impl_->status="Invalid particle projection input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(points==impl_->particle_output) throw std::runtime_error("Particle input aliases output");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(!impl_->particle_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
            info.code=spirv?particle_shader::spirv:dxil?particle_shader::dxil:reinterpret_cast<const Uint8*>(particle_shader::metal);
            info.code_size=spirv?sizeof(particle_shader::spirv):dxil?sizeof(particle_shader::dxil):std::strlen(particle_shader::metal);
            info.entrypoint=(spirv||dxil)?"main":"main0";info.num_uniform_buffers=1;
            info.num_readonly_storage_buffers=1;info.num_readwrite_storage_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            impl_->particle_pipeline=SDL_CreateGPUComputePipeline(impl_->device,&info);Impl::require(impl_->particle_pipeline);
        }
        if(!impl_->particle_output) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,300*32,0};
            impl_->particle_output=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->particle_output);
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);SDL_PushGPUComputeUniformData(cmd,0,&settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=impl_->particle_output;binding.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->particle_pipeline);
        auto* input=static_cast<SDL_GPUBuffer*>(points);SDL_BindGPUComputeStorageBuffers(pass,0,&input,1);
        SDL_DispatchGPUCompute(pass,(settings.count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        impl_->particle_command=command;impl_->particle_count=settings.count;impl_->particle_height=settings.height;
        impl_->status="Particle projection GPU resident";return impl_->particle_output;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuProjection::enqueue_dust_frame(void* device,void* command,const DustRenderer::DustFrame& frame,
    std::uint32_t width,std::uint32_t height,float eye_x,float convergence) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->dust_command=nullptr;impl_->dust_count=0;
    try {
        if(!device || !command || !width || !height || width>32767 || height>32767
            || frame.points.empty()) throw std::runtime_error("Invalid dust frame layout");
        const auto packed=DustRenderer::pack_dust_points(frame);
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(!impl_->dust_input) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,511*32,0};
            impl_->dust_input=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->dust_input);
        }
        if(!impl_->dust_colours) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,256,0};
            impl_->dust_colours=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->dust_colours);
        }
        if(!impl_->dust_upload) {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,511*32+256,0};
            impl_->dust_upload=SDL_CreateGPUTransferBuffer(impl_->device,&info);Impl::require(impl_->dust_upload);
        }
        auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(impl_->device,impl_->dust_upload,true));Impl::require(mapped);
        const auto bytes=Uint32(packed.size()*32);
        std::memcpy(mapped,packed.data(),bytes);
        std::array<Uint32,64> palette{};for(unsigned i=0;i<64;++i) palette[i]=frame.colours[i];
        std::memcpy(mapped+511*32,palette.data(),256);SDL_UnmapGPUTransferBuffer(impl_->device,impl_->dust_upload);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        auto* copy=SDL_BeginGPUCopyPass(cmd);Impl::require(copy);
        SDL_GPUTransferBufferLocation source{impl_->dust_upload,0};SDL_GPUBufferRegion destination{impl_->dust_input,0,bytes};
        SDL_UploadToGPUBuffer(copy,&source,&destination,true);
        source.offset=511*32;destination={impl_->dust_colours,0,256};
        SDL_UploadToGPUBuffer(copy,&source,&destination,true);SDL_EndGPUCopyPass(copy);
        DustSettings settings;
        for(unsigned i=0;i<3;++i) {
            settings.row_x[i]=frame.matrix[i*3];settings.row_y[i]=frame.matrix[i*3+1];settings.row_z[i]=frame.matrix[i*3+2];
        }
        settings.viewport[0]=std::int32_t(width);settings.viewport[1]=std::int32_t(height);
        settings.viewport[2]=frame.offset_x;settings.viewport[3]=frame.offset_y;
        settings.count=Uint32(packed.size());settings.eye_x=eye_x;settings.convergence=convergence;
        return enqueue_dust(device,command,impl_->dust_input,impl_->dust_colours,settings);
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuProjection::enqueue_dust(void* device,void* command,void* points,void* colours,const DustSettings& settings) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->dust_command=nullptr;impl_->dust_count=0;
#endif
    if(!device || !command || !points || !colours || !settings.count || settings.count>511
        || settings.viewport[0]<=0 || settings.viewport[1]<=0
        || settings.viewport[0]>32767 || settings.viewport[1]>32767
        || std::abs(std::int64_t(settings.viewport[2]))>32767 || std::abs(std::int64_t(settings.viewport[3]))>32767
        || !std::isfinite(settings.eye_x) || std::abs(settings.eye_x)>32767
        || !std::isfinite(settings.convergence) || settings.convergence<1) {
        impl_->status="Invalid dust projection input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(points==impl_->dust_output || colours==impl_->dust_output) throw std::runtime_error("Dust input aliases output");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(!impl_->dust_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
            info.code=spirv?dust_shader::spirv:dxil?dust_shader::dxil:reinterpret_cast<const Uint8*>(dust_shader::metal);
            info.code_size=spirv?sizeof(dust_shader::spirv):dxil?sizeof(dust_shader::dxil):std::strlen(dust_shader::metal);
            info.entrypoint=(spirv||dxil)?"main":"main0";
            info.num_readonly_storage_buffers=2;info.num_readwrite_storage_buffers=1;info.num_uniform_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            impl_->dust_pipeline=SDL_CreateGPUComputePipeline(impl_->device,&info);Impl::require(impl_->dust_pipeline);
        }
        if(!impl_->dust_output) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,511*16,0};
            impl_->dust_output=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->dust_output);
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        SDL_PushGPUComputeUniformData(cmd,0,&settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=impl_->dust_output;binding.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->dust_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(points),static_cast<SDL_GPUBuffer*>(colours)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,2);
        SDL_DispatchGPUCompute(pass,(settings.count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        impl_->dust_command=command;impl_->dust_count=settings.count;impl_->dust_height=std::uint32_t(settings.viewport[1]);
        impl_->status="Dust projection GPU resident";return impl_->dust_output;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuProjection::enqueue_grid_spans(void* command,std::uint32_t height,
    std::uint32_t scale,std::uint8_t colour,std::uint8_t tag,const std::int16_t* line_start,std::array<std::uint32_t,3> raster_mapping,std::array<float,2> jitter) {
    return enqueue_point_spans(command,height,scale,colour,tag,line_start,0,raster_mapping,jitter);
}
void* GpuProjection::enqueue_dust_spans(void* command,std::uint32_t height,std::uint32_t scale,
    std::uint8_t tag,std::int16_t left,std::int16_t right,std::array<std::uint32_t,3> raster_mapping,std::array<float,2> jitter) {
    const std::int16_t exclusion[]{left,right};
    return enqueue_point_spans(command,height,scale,0,tag,exclusion,2,raster_mapping,jitter);
}
void* GpuProjection::enqueue_particle_spans(void* command,std::uint32_t height,std::uint32_t scale,
    std::uint8_t tag,std::int16_t left,std::int16_t right,std::array<std::uint32_t,3> raster_mapping,std::array<float,2> jitter) {
    const std::int16_t clip[]{left,right};
    return enqueue_point_spans(command,height,scale,0,tag,clip,3,raster_mapping,jitter);
}
void* GpuProjection::enqueue_point_spans(void* command,std::uint32_t height,
    std::uint32_t scale,std::uint8_t colour,std::uint8_t tag,const std::int16_t* line_start,unsigned kind,
    std::array<std::uint32_t,3> raster_mapping,std::array<float,2> jitter) {
    const bool custom=raster_mapping!=std::array<std::uint32_t,3>{};
    if(!valid_raster_jitter(jitter) || !command || !height || height>8192 || !scale || scale>32 || (!custom && height%scale)
        || (custom && (!raster_mapping[0] || !raster_mapping[1] || !raster_mapping[2]
            || raster_mapping[0]>32767 || raster_mapping[1]>32767 || raster_mapping[2]>8192))) {
        impl_->status="Invalid grid span input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        auto* point_buffer=kind==3?impl_->particle_output:kind==2?impl_->dust_output:impl_->grid_output;
        const auto source_command=kind==3?impl_->particle_command:kind==2?impl_->dust_command:impl_->grid_command;
        const auto source_height=kind==3?impl_->particle_height:kind==2?impl_->dust_height:impl_->grid_height;
        if(!impl_->device || !point_buffer || source_command!=command || source_height!=(custom?raster_mapping[1]:height/scale))
            throw std::runtime_error("Grid spans require matching projected points on the same command");
        if(!impl_->grid_spans_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
            info.code=spirv?grid_spans_shader::spirv:dxil?grid_spans_shader::dxil:reinterpret_cast<const Uint8*>(grid_spans_shader::metal);
            info.code_size=spirv?sizeof(grid_spans_shader::spirv):dxil?sizeof(grid_spans_shader::dxil):std::strlen(grid_spans_shader::metal);
            info.entrypoint=(spirv||dxil)?"main":"main0";
            info.num_readonly_storage_buffers=1;info.num_readwrite_storage_buffers=1;info.num_uniform_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            impl_->grid_spans_pipeline=SDL_CreateGPUComputePipeline(impl_->device,&info);Impl::require(impl_->grid_spans_pipeline);
        }
        const Uint32 count=kind==3?impl_->particle_count:kind==2?impl_->dust_count:line_start?675:225;
        const Uint32 size=count*height*96;
        if(impl_->grid_spans_capacity<size) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,size,0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->grid_spans) SDL_ReleaseGPUBuffer(impl_->device,impl_->grid_spans);
            impl_->grid_spans=replacement;impl_->grid_spans_capacity=size;
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const Uint32 settings[]{height,scale,colour,tag,kind?kind:line_start?1U:0U,
            Uint32(line_start?line_start[0]:0),Uint32(line_start?line_start[1]:0),kind?count:0U,
            raster_mapping[0],raster_mapping[1],raster_mapping[2],0,
            std::bit_cast<Uint32>(jitter[0]),std::bit_cast<Uint32>(jitter[1]),0,0};
        SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=impl_->grid_spans;binding.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->grid_spans_pipeline);
        SDL_BindGPUComputeStorageBuffers(pass,0,&point_buffer,1);
        SDL_DispatchGPUCompute(pass,(count*height+63)/64,1,1);SDL_EndGPUComputePass(pass);
        impl_->status="Grid spans GPU resident";return impl_->grid_spans;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuProjection::enqueue_grid(void* device,void* command,const GridLattice& lattice,
    std::uint32_t width,std::uint32_t height,float eye_x,float convergence) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->grid_command=nullptr;impl_->grid_height=0;
#endif
    if(!device || !command || !width || !height || width>32767 || height>32767
        || !std::isfinite(eye_x) || !std::isfinite(convergence) || convergence<=0
        || std::abs(eye_x)>32767 || convergence<1) {
        impl_->status="Invalid grid projection input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(!impl_->grid_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
            info.code=spirv?grid_shader::spirv:dxil?grid_shader::dxil:reinterpret_cast<const Uint8*>(grid_shader::metal);
            info.code_size=spirv?sizeof(grid_shader::spirv):dxil?sizeof(grid_shader::dxil):std::strlen(grid_shader::metal);
            info.entrypoint=(spirv||dxil)?"main":"main0";
            info.num_readwrite_storage_buffers=1;info.num_uniform_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            impl_->grid_pipeline=SDL_CreateGPUComputePipeline(impl_->device,&info);Impl::require(impl_->grid_pipeline);
        }
        if(!impl_->grid_output) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,225*16,0};
            impl_->grid_output=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->grid_output);
        }
        struct Settings { Sint32 origin[4]{},x_step[4]{},z_step[4]{};Uint32 width,height;float eye_x,convergence; } settings{};
        static_assert(sizeof(Settings)==64);
        for(unsigned i=0;i<3;++i) {
            settings.origin[i]=lattice.origin[i];settings.x_step[i]=lattice.x_step[i];settings.z_step[i]=lattice.z_step[i];
        }
        settings.width=width;settings.height=height;settings.eye_x=eye_x;settings.convergence=convergence;
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        SDL_PushGPUComputeUniformData(cmd,0,&settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=impl_->grid_output;binding.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->grid_pipeline);
        SDL_DispatchGPUCompute(pass,4,1,1);SDL_EndGPUComputePass(pass);
        impl_->grid_command=command;impl_->grid_height=height;
        impl_->status="Grid projection GPU resident";return impl_->grid_output;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuProjection::enqueue_axis_points(void* device,void* command,void* points,
    std::uint32_t point_count,void* indices,std::uint32_t index_count,
    const std::uint32_t ranges[4],bool fractional,float vanish_x,float vanish_y,float focal_length,void* input_residuals,void** output_residuals,bool native_words) {
    if(output_residuals) *output_residuals=nullptr;
    if(native_words && (fractional || input_residuals || focal_length!=256.f)) {
        impl_->status="Native axis requires word input and source focal length";return nullptr;
    }
    if(!device || !command || !points || !indices || !ranges || !point_count || !index_count) {
        impl_->status="Invalid axis reduction input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(points==impl_->axis_output || indices==impl_->axis_output || points==impl_->axis_residuals || indices==impl_->axis_residuals
            || (input_residuals && (input_residuals==impl_->axis_output || input_residuals==impl_->axis_residuals)))
            throw std::runtime_error("Axis input aliases output");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(!impl_->axis_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(impl_->device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
            info.code=spirv?axis_shader::spirv:dxil?axis_shader::dxil:reinterpret_cast<const Uint8*>(axis_shader::metal);
            info.code_size=spirv?sizeof(axis_shader::spirv):dxil?sizeof(axis_shader::dxil):std::strlen(axis_shader::metal);
            info.entrypoint=(spirv||dxil)?"main":"main0";
            info.num_readonly_storage_buffers=3;info.num_readwrite_storage_buffers=2;info.num_uniform_buffers=1;
            info.threadcount_x=2;info.threadcount_y=info.threadcount_z=1;
            impl_->axis_pipeline=SDL_CreateGPUComputePipeline(impl_->device,&info);Impl::require(impl_->axis_pipeline);
        }
        if(!impl_->axis_output) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,64,0};
            impl_->axis_output=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->axis_output);
        }
        if(!impl_->axis_residuals) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,64,0};
            impl_->axis_residuals=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(impl_->axis_residuals);
        }
        struct {Uint32 dimensions[4];Uint32 ranges[4];float projection[4];} settings{
            {point_count,index_count,native_words?2U:Uint32(fractional),input_residuals?1U:0U},{ranges[0],ranges[1],ranges[2],ranges[3]},
            {vanish_x,vanish_y,focal_length,0}};
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);SDL_PushGPUComputeUniformData(cmd,0,&settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding bindings[2]{};bindings[0].buffer=impl_->axis_output;bindings[0].cycle=true;
        bindings[1].buffer=impl_->axis_residuals;bindings[1].cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,bindings,2);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->axis_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(points),static_cast<SDL_GPUBuffer*>(indices),static_cast<SDL_GPUBuffer*>(input_residuals?input_residuals:points)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,3);SDL_DispatchGPUCompute(pass,1,1,1);SDL_EndGPUComputePass(pass);
        if(output_residuals) *output_residuals=impl_->axis_residuals;
        impl_->status="Axis camera means GPU resident";return impl_->axis_output;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuProjection::enqueue_continuous(void* device,void* command,void* vertices,
    std::uint32_t count,void* poses,std::uint32_t pose_count,void** residual_points,bool lossless_camera) {
    if(residual_points) *residual_points=nullptr;
    if(!device || !command || !vertices || !poses || !count || count>4'000'000 || !pose_count) {
        impl_->status="Invalid continuous transform input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(vertices==impl_->transformed || poses==impl_->transformed || vertices==impl_->continuous_residuals || poses==impl_->continuous_residuals)
            throw std::runtime_error("Continuous transform input aliases output");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        // Never let word visibility silently consume an older source frame.
        impl_->point_count=0;
        if(impl_->transform_capacity<count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,count*32,0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->transformed) SDL_ReleaseGPUBuffer(impl_->device,impl_->transformed);
            impl_->transformed=replacement;impl_->transform_capacity=count;
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const auto residual_count=residual_points?count:1U;
        if(impl_->residual_capacity<residual_count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,residual_count*32,0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->continuous_residuals) SDL_ReleaseGPUBuffer(impl_->device,impl_->continuous_residuals);
            impl_->continuous_residuals=replacement;impl_->residual_capacity=residual_count;
        }
        const Uint32 settings[]{count,pose_count,residual_points?(lossless_camera?2U:1U):0U,0};
        SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding bindings[2]{};
        bindings[0].buffer=impl_->transformed;bindings[0].cycle=true;
        bindings[1].buffer=impl_->continuous_residuals;bindings[1].cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,bindings,2);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->continuous_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(vertices),static_cast<SDL_GPUBuffer*>(poses)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,2);
        SDL_DispatchGPUCompute(pass,(count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        impl_->status="Continuous transform and projection GPU resident";
        impl_->continuous_count=count;
        if(residual_points) *residual_points=impl_->continuous_residuals;
        return impl_->transformed;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuProjection::enqueue_transformed(void* device,void* command,void* vertices,
    std::uint32_t count,void* poses,std::uint32_t pose_count,void** camera_points) {
    if(camera_points) *camera_points=nullptr;
    if(!device || !command || !vertices || !poses || !count || count>4'000'000 || !pose_count) {
        impl_->status="Invalid native transform input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(vertices==impl_->transformed || poses==impl_->transformed)
            throw std::runtime_error("Native transform input aliases output");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(impl_->transform_capacity<count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,count*32,0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->transformed) SDL_ReleaseGPUBuffer(impl_->device,impl_->transformed);
            impl_->transformed=replacement;impl_->transform_capacity=count;
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const Uint32 settings[]{count,pose_count,0,0};
        SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=impl_->transformed;binding.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->transform_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(vertices),static_cast<SDL_GPUBuffer*>(poses)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,2);
        SDL_DispatchGPUCompute(pass,(count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        auto* output=enqueue(device,command,impl_->transformed,count);
        if(output && camera_points) *camera_points=impl_->transformed;
        return output;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
const std::string& GpuProjection::status()const noexcept{return impl_->status;}
void GpuProjection::release_device()noexcept{
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
void* GpuProjection::enqueue(void* device,void* command,void* input,std::uint32_t count) {
    if(!device || !command || !input || !count || count>4'000'000) {
        impl_->status="Invalid native projection input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(input==impl_->output || input==impl_->visibility_output) throw std::runtime_error("Native projection input aliases output");
        impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        if(impl_->capacity<count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,count*16,0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->output) SDL_ReleaseGPUBuffer(impl_->device,impl_->output);
            impl_->output=replacement;impl_->capacity=count;
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const Uint32 settings[]{count,0,0,0};
        SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding binding{};
        binding.buffer=impl_->output;binding.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,impl_->pipeline);
        auto* source=static_cast<SDL_GPUBuffer*>(input);
        SDL_BindGPUComputeStorageBuffers(pass,0,&source,1);
        SDL_DispatchGPUCompute(pass,(count+63)/64,1,1);
        SDL_EndGPUComputePass(pass);
        impl_->point_count=count;
        impl_->continuous_count=0;
        impl_->status="Native word projection GPU resident";return impl_->output;
    } catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
void* GpuProjection::enqueue_visibility(void* command,void* faces,std::uint32_t count) {
    return enqueue_visibility_impl(command,faces,count,false);
}
void* GpuProjection::enqueue_continuous_visibility(void* command,void* faces,std::uint32_t count) {
    return enqueue_visibility_impl(command,faces,count,true);
}
void* GpuProjection::enqueue_visibility_impl(void* command,void* faces,std::uint32_t count,bool continuous) {
    if(!command || !faces || !count || count>4'000'000) {
        impl_->status="Invalid native visibility input";return nullptr;
    }
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        auto* points=continuous?impl_->transformed:impl_->output;
        auto* pipeline=continuous?impl_->continuous_visibility_pipeline:impl_->visibility_pipeline;
        const auto point_count=continuous?impl_->continuous_count:impl_->point_count;
        if(!point_count || !points || !pipeline)
            throw std::runtime_error("Native visibility requires projected points");
        if(faces==points || faces==impl_->visibility_output)
            throw std::runtime_error("Native visibility indices alias scratch data");
        if(impl_->visibility_capacity<count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,count*4,0};
            auto* replacement=SDL_CreateGPUBuffer(impl_->device,&info);Impl::require(replacement);
            if(impl_->visibility_output) SDL_ReleaseGPUBuffer(impl_->device,impl_->visibility_output);
            impl_->visibility_output=replacement;impl_->visibility_capacity=count;
        }
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        const Uint32 settings[]{count,point_count,0,0};
        SDL_PushGPUComputeUniformData(cmd,0,settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=impl_->visibility_output;binding.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);Impl::require(pass);
        SDL_BindGPUComputePipeline(pass,pipeline);
        SDL_GPUBuffer* inputs[]{points,static_cast<SDL_GPUBuffer*>(faces)};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,2);
        SDL_DispatchGPUCompute(pass,(count+63)/64,1,1);SDL_EndGPUComputePass(pass);
        impl_->status=continuous?"Continuous projection and visibility GPU resident":"Native projection and visibility GPU resident";
        return impl_->visibility_output;
    }catch(const std::exception& error){impl_->status=error.what();return nullptr;}
#else
    return nullptr;
#endif
}
}
