#include "starfox/render/portable_shadows.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include "shaders/generated/shadow_portable.hpp"
#include <cstring>
#include <stdexcept>
#endif
namespace starfox::render::shadows {
#if defined(STARFOX_SDL_GPU_EFFECTS)
namespace {
void require(bool value) { if(!value) throw std::runtime_error(SDL_GetError()); }
struct Float4 { float x{},y{},z{},w{}; };
struct Parameters { Float4 camera,options,ground_point,ground_normal,lights[8]; };
static_assert(sizeof(Parameters)==192);
static_assert(sizeof(Scene::GpuNode)==48 && sizeof(Scene::GpuTriangle)==48);
}
struct PortableShadows::Impl {
    SDL_GPUDevice* device{};
    SDL_GPUComputePipeline* pipeline{};
    SDL_GPUBuffer* buffers[3]{};
    Uint32 capacities[3]{};
    SDL_GPUTransferBuffer *upload{},*download{};
    Uint32 upload_capacity{},download_capacity{};
    SDL_GPUCommandBuffer* command{};
    SDL_GPUFence* fence{};
    bool failed{};
    bool owns_device{}, valid{};
    Uint32 width{},height{};
    std::string status{"Portable GPU shadows not initialized"};
    std::vector<Scene::GpuNode> nodes;
    std::vector<Scene::GpuTriangle> triangles;
    ~Impl() {
        if(!device) return;
        if(command) SDL_CancelGPUCommandBuffer(command);
        if(fence) {SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);}
        for(auto* buffer:buffers) if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(download) SDL_ReleaseGPUTransferBuffer(device,download);
        if(pipeline) SDL_ReleaseGPUComputePipeline(device,pipeline);
        if(owns_device) SDL_DestroyGPUDevice(device);
    }
    void finish() {
        if(fence) {require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);fence=nullptr;}
    }
    void initialize(SDL_GPUDevice* borrowed=nullptr) {
        if(borrowed) device=borrowed;
        else {
#if defined(_WIN32)
        SDL_SetHintWithPriority(SDL_HINT_GPU_DRIVER,"vulkan",SDL_HINT_DEFAULT);
#endif
        const auto properties=SDL_CreateProperties();require(properties!=0);
        SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
        SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN,true);
        SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
        // A CPU Vulkan driver is useful for parity CI, but slower than our
        // native CPU fallback and must not masquerade as hardware in-game.
        SDL_SetBooleanProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_VULKAN_REQUIRE_HARDWARE_ACCELERATION_BOOLEAN,
            SDL_getenv("STARFOX_TEST_SOFTWARE_GPU")==nullptr);
        device=SDL_CreateGPUDeviceWithProperties(properties);
        SDL_DestroyProperties(properties);
        require(device);
        owns_device=true;
        }
        SDL_GPUComputePipelineCreateInfo info{};
        const auto formats=SDL_GetGPUShaderFormats(device);
        if(formats&SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format=SDL_GPU_SHADERFORMAT_SPIRV;info.code=portable_shader::spirv;
            info.code_size=sizeof(portable_shader::spirv);info.entrypoint="main";
        }
#if defined(__APPLE__)
        else if(formats&SDL_GPU_SHADERFORMAT_MSL) {
            info.format=SDL_GPU_SHADERFORMAT_MSL;
            info.code=reinterpret_cast<const Uint8*>(portable_shader::metal);
            info.code_size=sizeof(portable_shader::metal)-1;info.entrypoint="main0";
        }
#endif
#if defined(_WIN32)
        else if(formats&SDL_GPU_SHADERFORMAT_DXIL) {
            info.format=SDL_GPU_SHADERFORMAT_DXIL;info.code=portable_shader::dxil;
            info.code_size=sizeof(portable_shader::dxil);info.entrypoint="main";
        }
#endif
        else throw std::runtime_error("Portable shadows require a supported native shader format");
        info.num_readonly_storage_buffers=2;info.num_readwrite_storage_buffers=1;
        info.num_uniform_buffers=1;info.threadcount_x=info.threadcount_y=8;info.threadcount_z=1;
        pipeline=SDL_CreateGPUComputePipeline(device,&info);require(pipeline);
        status=std::string("GPU compute shadows: ")+SDL_GetGPUDeviceDriver(device);
        status+=" (";status+=SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),
            SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown device");status+=")";
    }
    void buffer(unsigned index, Uint32 bytes) {
        if(buffers[index] && capacities[index]>=bytes) return;
        if(buffers[index]) SDL_ReleaseGPUBuffer(device,buffers[index]);
        buffers[index]=nullptr;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
            |(index==2?SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE:0U),bytes,0};
        buffers[index]=SDL_CreateGPUBuffer(device,&info);require(buffers[index]);capacities[index]=bytes;
    }
    void transfer(SDL_GPUTransferBuffer*& target,Uint32& capacity,Uint32 bytes,SDL_GPUTransferBufferUsage usage) {
        if(target && capacity>=bytes) return;
        if(target) SDL_ReleaseGPUTransferBuffer(device,target);
        target=nullptr;
        SDL_GPUTransferBufferCreateInfo info{usage,bytes,0};
        target=SDL_CreateGPUTransferBuffer(device,&info);require(target);capacity=bytes;
    }
    void render(const Scene& scene,Camera camera,Vec3 light,std::optional<ReceiverPlane> ground,
        std::vector<std::uint8_t>* mask) {
        finish();valid=false;
        const auto pixels=std::size_t(camera.width)*camera.height;
        const auto length=std::sqrt(dot(light,light));
        if(!pixels || !scene.triangle_count()) {if(mask) mask->assign(pixels,0);return;}
        if(camera.width>16384 || camera.height>16384 || pixels>UINT32_MAX/4
            || camera.focal_length<=0 || !std::isfinite(length) || length<=1e-10
            || camera.vertical_focal_length()<=0 || !std::isfinite(camera.vertical_focal_length())
            || !scene.pack_gpu(nodes,triangles) || nodes.empty()
            || nodes.size()>UINT32_MAX/48 || triangles.size()>UINT32_MAX/48)
            throw std::runtime_error("Invalid portable shadow scene");
        const Uint32 node_bytes=Uint32(nodes.size()*48),triangle_bytes=Uint32(triangles.size()*48);
        if(std::uint64_t(node_bytes)+triangle_bytes>UINT32_MAX) throw std::runtime_error("Shadow upload too large");
        const Uint32 output_bytes=Uint32(pixels*4);
        buffer(0,node_bytes);buffer(1,triangle_bytes);buffer(2,output_bytes);
        transfer(upload,upload_capacity,node_bytes+triangle_bytes,SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
        auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,upload,true));require(mapped);
        std::memcpy(mapped,nodes.data(),node_bytes);std::memcpy(mapped+node_bytes,triangles.data(),triangle_bytes);
        SDL_UnmapGPUTransferBuffer(device,upload);
        Parameters p{};
        p.camera={float(camera.width),float(camera.height),float(camera.focal_length),float(camera.center_x)};
        p.options={float(camera.center_y),ground?1.F:0.F,float(camera.vertical_focal_length()),0};
        if(ground) {
            p.ground_point={float(ground->point.x),float(ground->point.y),float(ground->point.z),0};
            p.ground_normal={float(ground->normal.x),float(ground->normal.y),float(ground->normal.z),0};
        }
        light=light*(1/length);
        const auto reference=std::abs(light.y)<.9?Vec3{0,1,0}:Vec3{1,0,0};
        auto tangent=cross(light,reference);tangent=tangent*(1/std::sqrt(dot(tangent,tangent)));
        const auto bitangent=cross(light,tangent);
        for(unsigned i=0;i<8;++i) {
            const auto radius=.015*std::sqrt((i+.5)/8),angle=i*2.399963229728653;
            auto direction=light+tangent*(radius*std::cos(angle))+bitangent*(radius*std::sin(angle));
            direction=direction*(1/std::sqrt(dot(direction,direction)));
            p.lights[i]={float(direction.x),float(direction.y),float(direction.z),0};
        }
        command=SDL_AcquireGPUCommandBuffer(device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        for(unsigned i=0;i<2;++i) {
            SDL_GPUTransferBufferLocation source{upload,i?node_bytes:0};
            SDL_GPUBufferRegion destination{buffers[i],0,i?triangle_bytes:node_bytes};
            SDL_UploadToGPUBuffer(copy,&source,&destination,false);
        }
        SDL_EndGPUCopyPass(copy);
        SDL_PushGPUComputeUniformData(command,0,&p,sizeof(p));
        SDL_GPUStorageBufferReadWriteBinding output{};output.buffer=buffers[2];
        auto* compute=SDL_BeginGPUComputePass(command,nullptr,0,&output,1);require(compute);
        SDL_BindGPUComputePipeline(compute,pipeline);
        SDL_BindGPUComputeStorageBuffers(compute,0,buffers,2);
        SDL_DispatchGPUCompute(compute,(camera.width+7)/8,(camera.height+7)/8,1);
        SDL_EndGPUComputePass(compute);
        if(mask) enqueue_download(output_bytes);
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require(fence);
        width=camera.width;height=camera.height;valid=true;
        if(mask) read(*mask,true);
    }
    void enqueue_download(Uint32 output_bytes) {
        if(!device || !command || !buffers[2] || !output_bytes)
            throw std::runtime_error("Portable shadow readback source is missing");
        transfer(download,download_capacity,output_bytes,SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion source{buffers[2],0,output_bytes};
        SDL_GPUTransferBufferLocation destination{download,0};
        SDL_DownloadFromGPUBuffer(copy,&source,&destination);SDL_EndGPUCopyPass(copy);
    }
    void read(std::vector<std::uint8_t>& mask,bool already_downloaded=false) {
        if(!valid) throw std::runtime_error("No resident shadow frame");
        finish();
        const auto pixels=std::size_t(width)*height;
        if(!already_downloaded) {
            command=SDL_AcquireGPUCommandBuffer(device);require(command);
            enqueue_download(Uint32(pixels*4));
            fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require(fence);
            finish();
        }
        auto* values=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(device,download,false));require(values);
        mask.resize(pixels);
        for(std::size_t i=0;i<pixels;++i) mask[i]=std::uint8_t(values[i]);
        SDL_UnmapGPUTransferBuffer(device,download);
    }
};
#else
struct PortableShadows::Impl {std::string status{"Portable GPU shadows unavailable on this build"};};
#endif
PortableShadows::PortableShadows():impl_(std::make_unique<Impl>()) {}
PortableShadows::~PortableShadows()=default;
const std::string& PortableShadows::status() const {static const std::string released{"Portable shadows released"};return impl_?impl_->status:released;}
void PortableShadows::release_device() noexcept {impl_.reset();}
bool PortableShadows::render(const Scene& scene,Camera camera,Vec3 light,
    std::optional<ReceiverPlane> ground,std::vector<std::uint8_t>& mask) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_) impl_=std::make_unique<Impl>();
    if(impl_->failed) return false;
    try {
        if(!impl_->device) impl_->initialize();
        impl_->render(scene,camera,light,ground,&mask);return true;
    } catch(const std::exception& error) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->failed=true;impl_->status=error.what();return false;
    }
#else
    (void)scene;(void)camera;(void)light;(void)ground;(void)mask;return false;
#endif
}
bool PortableShadows::render_resident(void* device,const Scene& scene,Camera camera,Vec3 light,
    std::optional<ReceiverPlane> ground) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!device) return false;
    if(!impl_ || impl_->device!=device) impl_=std::make_unique<Impl>();
    if(impl_->failed) return false;
    try {
        if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        impl_->render(scene,camera,light,ground,nullptr);return impl_->valid;
    } catch(const std::exception& error) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->valid=false;impl_->failed=true;impl_->status=error.what();return false;
    }
#else
    (void)device;(void)scene;(void)camera;(void)light;(void)ground;return false;
#endif
}
GpuShadowOutput PortableShadows::output() const {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(impl_ && impl_->valid) return {impl_->device,impl_->buffers[2],impl_->width,impl_->height};
#endif
    return {};
}
bool PortableShadows::readback(std::vector<std::uint8_t>& mask) {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    if(!impl_ || !impl_->valid) return false;
    try {impl_->read(mask);return true;}
    catch(const std::exception& error) {
        if(impl_->command) {SDL_CancelGPUCommandBuffer(impl_->command);impl_->command=nullptr;}
        impl_->status=error.what();return false;
    }
#else
    (void)mask;return false;
#endif
}
}
