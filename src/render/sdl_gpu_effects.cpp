#include "starfox/render/sdl_gpu_effects.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <cstring>
#include <stdexcept>
#if defined(STARFOX_ENABLE_XBRZ)
#include "shaders/generated/effects_portable_1.hpp"
#else
#include "shaders/generated/effects_portable_0.hpp"
#endif
#endif
namespace starfox::render {
#if defined(STARFOX_SDL_GPU_EFFECTS)
namespace {
void require(bool ok) { if(!ok) throw std::runtime_error(SDL_GetError()); }
struct Parameters {
    Uint32 width,height,scale,stage,hdr,chromatic,smoothing,model,world,model_intensity,world_intensity,aa;
    Uint32 lighting,surface_width,surface_height,reserved;
    Sint32 surface_x,surface_y,min_x,min_y,max_x,max_y,pad0,pad1;
    Uint32 bloom_model,bloom_world,bloom_width,bloom_height,filter,highlight_filter,overlay_filter,pad3;
    Uint32 shadow_width,shadow_height;Sint32 shadow_y;Uint32 shadow_enabled;
};
static_assert(sizeof(Parameters)==144);
static_assert(sizeof(SurfaceSample)==20 && offsetof(SurfaceSample,valid)==17);
}
struct SdlGpuEffects::Impl {
    SDL_GPUDevice* device{};SDL_GPUComputePipeline* pipeline{};
    SDL_GPUCommandBuffer* command{};SDL_GPUFence* fence{};
    SDL_GPUTransferBuffer *upload{},*download{};Uint32 upload_size{},download_size{};
    SDL_GPUBuffer* buffers[4]{};Uint32 buffer_sizes[4]{};
    SDL_GPUTexture *images[2]{},*snapshots[2]{},*bloom[4]{},*native{},*side{},*dummy_read{},*dummy_write[4]{};
    Uint32 width{},height{},scale{};std::string status{"SDL GPU effects not initialized"};
    bool failed{};
    ~Impl() {
        if(!device) return;
        if(command) SDL_CancelGPUCommandBuffer(command);
        if(fence) {SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);}
        release_textures();
        for(auto* b:buffers) if(b) SDL_ReleaseGPUBuffer(device,b);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
    }
    void finish() {
        if(fence) {require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);fence=nullptr;}
    }
    void release_textures() {
        const auto release=[&](SDL_GPUTexture*& t){if(t) SDL_ReleaseGPUTexture(device,t);t=nullptr;};
        for(auto& t:images) release(t);
        for(auto& t:snapshots) release(t);
        for(auto& t:bloom) release(t);
        for(auto& t:dummy_write) release(t);
        release(native);release(side);release(dummy_read);
    }
    SDL_GPUTexture* texture(Uint32 w,Uint32 h,bool floating=false) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
        info.format=floating?SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width=w;info.height=h;info.layer_count_or_depth=1;info.num_levels=1;
        auto* t=SDL_CreateGPUTexture(device,&info);require(t);return t;
    }
    void initialize(SDL_GPUDevice* source) {
        device=source;SDL_GPUComputePipelineCreateInfo info{};
        const auto formats=SDL_GetGPUShaderFormats(device);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=portable_shader::spirv;
            info.code_size=sizeof(portable_shader::spirv);info.entrypoint="main";
        } else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
            info.format=SDL_GPU_SHADERFORMAT_MSL;info.code=reinterpret_cast<const Uint8*>(portable_shader::metal);
            info.code_size=sizeof(portable_shader::metal)-1;info.entrypoint="main0";
        } else throw std::runtime_error("SDL GPU effects require Vulkan SPIR-V or Metal");
        info.num_readonly_storage_textures=6;info.num_readonly_storage_buffers=4;
        info.num_readwrite_storage_textures=4;info.num_uniform_buffers=1;
        info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        pipeline=SDL_CreateGPUComputePipeline(device,&info);require(pipeline);
        status=std::string("SDL GPU compute: ")+SDL_GetGPUDeviceDriver(device);
    }
    void resize(const Framebuffer& frame) {
        if(width==frame.stored_width() && height==frame.stored_height() && scale==frame.draw_scale()) return;
        release_textures();width=height=scale=0;
        const auto w=frame.stored_width(),h=frame.stored_height(),s=frame.draw_scale();
        for(auto& t:images) t=texture(w,h);
        for(auto& t:snapshots) t=texture(w,h);
        for(auto& t:bloom) t=texture((w+2*s-1)/(2*s),(h+2*s-1)/(2*s),true);
        side=texture(w,h);native=texture(w/s,h/s);dummy_read=texture(1,1);
        for(unsigned i=0;i<4;++i) dummy_write[i]=texture(1,1,i==1);
        width=w;height=h;scale=s;
    }
    void transfer(SDL_GPUTransferBuffer*& b,Uint32& capacity,Uint32 size,SDL_GPUTransferBufferUsage usage) {
        if(b && capacity>=size) return;
        if(b) SDL_ReleaseGPUTransferBuffer(device,b);
        b=nullptr;SDL_GPUTransferBufferCreateInfo info{usage,size,0};
        b=SDL_CreateGPUTransferBuffer(device,&info);require(b);capacity=size;
    }
    void copy(SDL_GPUTexture* src,SDL_GPUTexture* dst) {
        auto* pass=SDL_BeginGPUCopyPass(command);require(pass);
        SDL_GPUTextureLocation a{src,0,0,0,0,0},b{dst,0,0,0,0,0};
        SDL_CopyGPUTextureToTexture(pass,&a,&b,width,height,1,false);SDL_EndGPUCopyPass(pass);
    }
    void dispatch(Parameters& p,Uint32 stage,SDL_GPUTexture* input,SDL_GPUTexture* output,
        SDL_GPUTexture* bright=nullptr,SDL_GPUTexture* core=nullptr,SDL_GPUTexture* bright_output=nullptr,
        SDL_GPUTexture* native_output=nullptr,SDL_GPUTexture* side_output=nullptr) {
        p.stage=stage;SDL_PushGPUComputeUniformData(command,0,&p,sizeof(p));
        SDL_GPUStorageTextureReadWriteBinding targets[4]{};
        SDL_GPUTexture* out[]{output,bright_output,native_output,side_output};
        for(unsigned i=0;i<4;++i) targets[i].texture=out[i]?out[i]:dummy_write[i];
        auto* pass=SDL_BeginGPUComputePass(command,targets,4,nullptr,0);require(pass);
        SDL_BindGPUComputePipeline(pass,pipeline);
        SDL_GPUTexture* inputs[]{input,bright?bright:dummy_read,core?core:dummy_read,native,snapshots[0],snapshots[1]};
        for(auto& t:inputs) for(auto* written:out) if(written && t==written) t=dummy_read;
        SDL_BindGPUComputeStorageTextures(pass,0,inputs,6);SDL_BindGPUComputeStorageBuffers(pass,0,buffers,4);
        const auto w=stage>=7 && stage<=11?p.bloom_width:stage==13?width/scale:width;
        const auto h=stage>=7 && stage<=11?p.bloom_height:stage==13?height/scale:height;
        SDL_DispatchGPUCompute(pass,(w+7)/8,(h+7)/8,1);SDL_EndGPUComputePass(pass);
    }
    void download_texture(SDL_GPUTexture* t,Uint32 offset) {
        auto* pass=SDL_BeginGPUCopyPass(command);require(pass);
        SDL_GPUTextureRegion region{t,0,0,0,0,0,width,height,1};
        SDL_GPUTextureTransferInfo target{download,offset,0,0};
        SDL_DownloadFromGPUTexture(pass,&region,&target);SDL_EndGPUCopyPass(pass);
    }
    bool read(std::vector<std::uint8_t>& rgba,Uint32 offset=0) {
        finish();auto* bytes=static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(device,download,false));
        require(bytes);rgba.resize(std::size_t(width)*height*4);
        std::memcpy(rgba.data(),bytes+offset,rgba.size());SDL_UnmapGPUTransferBuffer(device,download);return true;
    }
    void apply(const Framebuffer& frame,std::vector<std::uint8_t>& rgba,const GpuEffectSettings& s) {
        finish();resize(frame);
        const Uint32 bytes=width*height*4;
        Parameters p{width,height,scale,0,s.hdr,s.chromatic,s.smoothing,s.model_effect,s.world_effect,
            s.model_intensity,s.world_intensity,s.anti_aliasing,0,0,0,0,0,0,0,0,0,0,0,0,
            s.bloom_model,s.bloom_world,(width+2*scale-1)/(2*scale),(height+2*scale-1)/(2*scale),
            s.filter,s.highlight_filter,s.overlay_filter?1U:0U,0,s.shadow_width,s.shadow_height,s.shadow_offset_y,0};
        auto* present=static_cast<SDL_GPUTexture*>(s.presentation_texture);
        auto* glow=static_cast<SDL_GPUTexture*>(s.presentation_glow_texture);
        auto* model=static_cast<SDL_GPUTexture*>(s.presentation_model_texture);
        if((glow || model) && !present) throw std::runtime_error("GPU layer output requires base output");
        if(model && (!s.surfaces || s.surfaces->empty())) throw std::runtime_error("Missing model surfaces");
        if(glow && !(s.bloom_model || s.bloom_world)) throw std::runtime_error("Missing bloom settings");
        std::span<const Uint8> data[4]{frame.layer_tags(),frame.pixels(),{},s.shadow_mask};
        if((s.lighting || model) && s.surfaces && !s.surfaces->empty()) {
            const auto& surface=*s.surfaces;p.lighting=s.lighting;p.surface_width=surface.width();p.surface_height=surface.height();
            p.surface_x=s.surface_x;p.surface_y=s.surface_y;
            p.min_x=std::max(1,p.surface_x+int(surface.minimum_x()));p.min_y=std::max(1,p.surface_y+int(surface.minimum_y()));
            p.max_x=std::min(int(width)-1,p.surface_x+int(surface.maximum_x()));p.max_y=std::min(int(height)-1,p.surface_y+int(surface.maximum_y()));
            data[2]={reinterpret_cast<const Uint8*>(surface.samples().data()),surface.samples().size_bytes()};
        }
        if(!s.shadow_mask.empty()) {
            if(!s.shadow_width || !s.shadow_height || s.shadow_mask.size()!=std::size_t(s.shadow_width)*s.shadow_height)
                throw std::runtime_error("Invalid shadow dimensions");
            p.shadow_enabled=1;
        }
        Uint32 offsets[4]{},sizes[4]{},total=(bytes+255)&~255U;
        for(unsigned i=0;i<4;++i) {
            sizes[i]=std::max(4U,(Uint32(data[i].size())+3)&~3U);offsets[i]=total;total+=(sizes[i]+255)&~255U;
            if(!buffers[i] || buffer_sizes[i]<sizes[i]) {
                if(buffers[i]) SDL_ReleaseGPUBuffer(device,buffers[i]);
                buffers[i]=nullptr;
                SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
                buffers[i]=SDL_CreateGPUBuffer(device,&info);require(buffers[i]);buffer_sizes[i]=sizes[i];
            }
        }
        transfer(upload,upload_size,total,SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
        transfer(download,download_size,bytes*3,SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
        auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true));require(mapped);
        std::memset(mapped,0,total);std::memcpy(mapped,rgba.data(),bytes);
        for(unsigned i=0;i<4;++i) if(!data[i].empty()) std::memcpy(mapped+offsets[i],data[i].data(),data[i].size());
        SDL_UnmapGPUTransferBuffer(device,upload);
        command=SDL_AcquireGPUCommandBuffer(device);require(command);
        auto* pass=SDL_BeginGPUCopyPass(command);require(pass);
        SDL_GPUTextureTransferInfo source{upload,0,0,0};SDL_GPUTextureRegion target{images[0],0,0,0,0,0,width,height,1};
        SDL_UploadToGPUTexture(pass,&source,&target,false);
        for(unsigned i=0;i<4;++i) {
            SDL_GPUTransferBufferLocation a{upload,offsets[i]};SDL_GPUBufferRegion b{buffers[i],0,sizes[i]};
            SDL_UploadToGPUBuffer(pass,&a,&b,false);
        }
        SDL_EndGPUCopyPass(pass);unsigned current=0;
        const auto run=[&](unsigned stage){dispatch(p,stage,images[current],images[1-current]);current=1-current;};
        if(p.filter) {dispatch(p,13,images[current],nullptr,nullptr,nullptr,nullptr,native);run(14);}
        if(p.lighting) run(6);
        if(p.hdr) run(1);
        if(p.chromatic) run(2);
        if(p.smoothing) run(3);
        if(p.model || p.world) run(4);
        if(p.bloom_model || p.bloom_world) {
            copy(images[current],snapshots[0]);
            dispatch(p,7,images[current],nullptr,nullptr,nullptr,bloom[0]);
            dispatch(p,8,images[current],nullptr,bloom[0],nullptr,bloom[1]);
            dispatch(p,9,images[current],nullptr,bloom[1],nullptr,bloom[2]);
            dispatch(p,10,images[current],nullptr,bloom[2],nullptr,bloom[1]);
            dispatch(p,11,images[current],nullptr,bloom[1],nullptr,bloom[3]);
            dispatch(p,12,images[current],images[1-current],bloom[3],bloom[2]);current=1-current;
            copy(images[current],snapshots[1]);
        }
        if(p.aa) run(5);
        if(p.shadow_enabled) run(15);
        download_texture(images[current],0);
        if(glow) {
            dispatch(p,16,images[current],images[1-current],nullptr,nullptr,nullptr,nullptr,side);current=1-current;copy(side,glow);
        }
        if(model) {
            dispatch(p,17,images[current],images[1-current],nullptr,nullptr,nullptr,nullptr,side);current=1-current;copy(side,model);
        }
        if(present) copy(images[current],present);
        else if(p.bloom_model || p.bloom_world) {
            if(s.bloom_base) download_texture(snapshots[0],bytes);
            if(s.bloom_glow) download_texture(snapshots[1],bytes*2);
        }
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require(fence);
        if(!present) {
            // Keep input untouched until all requested snapshots succeed.
            if(p.bloom_model || p.bloom_world) {if(s.bloom_base) read(*s.bloom_base,bytes);if(s.bloom_glow) read(*s.bloom_glow,bytes*2);}
            read(rgba);
        }
    }
};
#else
struct SdlGpuEffects::Impl { std::string status{"SDL GPU effects unavailable on this build"}; };
#endif
SdlGpuEffects::SdlGpuEffects():impl_(std::make_unique<Impl>()) {}
SdlGpuEffects::~SdlGpuEffects()=default;
void SdlGpuEffects::release_device() noexcept {impl_.reset();}
const std::string& SdlGpuEffects::status() const {static const std::string empty{"SDL GPU device released"};return impl_?impl_->status:empty;}
bool SdlGpuEffects::readback(std::vector<std::uint8_t>& rgba) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_ || !impl_->download) return false;
    try {return impl_->read(rgba);} catch(const std::exception& e){impl_->status=e.what();return false;}
#else
    (void)rgba;return false;
#endif
}
bool SdlGpuEffects::apply(void* source,const Framebuffer& frame,std::vector<std::uint8_t>& rgba,const GpuEffectSettings& settings) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!source || !frame.layer_tags_enabled() || rgba.empty() || rgba.size()!=frame.pixels().size()*4) return false;
#if !defined(STARFOX_ENABLE_XBRZ)
    if(settings.filter==2) return false;
#endif
    if(!impl_ || impl_->device!=source) impl_=std::make_unique<Impl>();
    if(impl_->failed) return false;
    try {if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(source));impl_->apply(frame,rgba,settings);return true;}
    catch(const std::exception& e) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->status=e.what();impl_->failed=true;return false;
    }
#else
    (void)source;(void)frame;(void)rgba;(void)settings;return false;
#endif
}
}
