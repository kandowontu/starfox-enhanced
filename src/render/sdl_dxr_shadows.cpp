#include "starfox/render/sdl_dxr_shadows.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(STARFOX_DXR)
#define STARFOX_NATIVE_SDL_DXR 1
#include "starfox/render/sdl_d3d12_bridge.h"
#include <SDL3/SDL.h>
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "starfox/render/sdl_vulkan_bridge.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <cstring>
#include <limits>
#include <stdexcept>
#endif
namespace starfox::render::shadows {
#if defined(STARFOX_NATIVE_SDL_DXR)
#include "sdl_vulkan_shadow_import.inc"
#endif
struct SdlDxrShadows::Impl {
    std::string status{"Native SDL DXR unavailable"};
    GpuShadowOutput output{};
    std::uint32_t bytes_per_pixel{1};
#if defined(STARFOX_NATIVE_SDL_DXR)
    SDL_GPUDevice* device{};
    SDL_GPUBuffer* buffer{};
    SDL_GPUTransferBuffer* download{};
    SDL_GPUCommandBuffer* command{};
    SDL_GPUFence* fence{};
    bool dependency_pending{};
    ID3D12Device* native{};
    const StarfoxSdlD3D12BridgeV2* bridge{};
    const StarfoxSdlD3D12GeometryBridgeV1* geometry_bridge{};
    const StarfoxSdlVulkanGeometryBridgeV1* vulkan_geometry_bridge{};
    SdlVulkanShadowImport vulkan;
    SdlVulkanShadowImport vulkan_geometry;
    std::unique_ptr<DxrShadows> producer;
    Microsoft::WRL::ComPtr<ID3D12Resource> imported;
    Microsoft::WRL::ComPtr<ID3D12Resource> imported_geometry;
    void* geometry_source{};
    std::uint64_t geometry_bytes{};
    Microsoft::WRL::ComPtr<ID3D12Fence> ready_fence;
    Microsoft::WRL::ComPtr<ID3D12Fence> geometry_completion;
    std::uint64_t geometry_serial{};
    void* imported_source{};
    std::uint64_t imported_bytes{};
    Uint32 capacity{};
    static void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
    void finish() {
        if(fence) {
            require(SDL_WaitForGPUFences(device,true,&fence,1),SDL_GetError());
            SDL_ReleaseGPUFence(device,fence);fence=nullptr;
        }
    }
    ~Impl() {
        if(!device) return;
        if(command) SDL_CancelGPUCommandBuffer(command);
        if(fence) {SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);}
        else if(dependency_pending) SDL_WaitForGPUIdle(device);
        // SDL tracks destination buffers through consumers; the imported source
        // only participates in the completed copy above. No device-wide idle.
        if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        if(download) SDL_ReleaseGPUTransferBuffer(device,download);
    }
    void initialize(SDL_GPUDevice* source) {
        require(source,"Missing SDL GPU device");
        const auto props=SDL_GetGPUDeviceProperties(source);
        native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_DEVICE,nullptr));
        bridge=static_cast<const StarfoxSdlD3D12BridgeV2*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_BRIDGE,nullptr));
        vulkan.bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_BRIDGE,nullptr));
        geometry_bridge=static_cast<const StarfoxSdlD3D12GeometryBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_GEOMETRY_BRIDGE,nullptr));
        vulkan_geometry_bridge=static_cast<const StarfoxSdlVulkanGeometryBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_GEOMETRY_BRIDGE,nullptr));
        vulkan_geometry.bridge=vulkan.bridge;
        if(vulkan.bridge) {
            const auto& vk=*vulkan.bridge;
            require(vk.version==2 && vk.wait_timeline && vk.copy_external
                && vk.get_device_proc(vk.device,"vkGetMemoryWin32HandlePropertiesKHR"),"Vulkan external-memory options unavailable");
            auto query=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(vk.get_instance_proc(vk.instance,"vkGetPhysicalDeviceProperties2"));
            require(query,"Vulkan adapter query unavailable");
            VkPhysicalDeviceIDProperties id{};id.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
            VkPhysicalDeviceProperties2 info{};info.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;info.pNext=&id;
            query(vk.physical_device,&info);require(id.deviceLUIDValid,"Vulkan adapter LUID unavailable");
            std::array<std::uint8_t,8> identity{};std::memcpy(identity.data(),id.deviceLUID,8);
            producer=std::make_unique<DxrShadows>(identity);
            require(producer->available(),producer->status().c_str());device=source;return;
        }
        require(native && bridge && bridge->version==2 && bridge->wait_fence,"SDL backend has no native DXR bridge");
        LUID luid{};
#if defined(__MINGW32__)
        native->GetAdapterLuid(&luid);
#else
        luid=native->GetAdapterLuid();
#endif
        std::array<std::uint8_t,8> identity{};std::memcpy(identity.data(),&luid,8);
        producer=std::make_unique<DxrShadows>(identity);
        require(producer->available(),producer->status().c_str());
        device=source;
    }
    DxrShadows::ResidentGeometry copy_geometry(const GpuScene::RayGeometryOutput& source,bool include_materials) {
        require(source.complete && source.device==device && source.buffer && source.vertex_count
            && source.vertex_count%3==0 && source.vertex_count<=UINT32_MAX/16U,"Invalid resident caster geometry");
        if(vulkan.bridge) require(vulkan_geometry_bridge && vulkan_geometry_bridge->version==1
            && vulkan_geometry_bridge->copy_to_external && vulkan_geometry_bridge->signal_timeline,"Vulkan geometry bridge unavailable");
        else require(geometry_bridge && geometry_bridge->version==1 && geometry_bridge->copy_to_external && geometry_bridge->signal_fence,
            "D3D12 geometry bridge unavailable");
        const auto bytes=include_materials && source.material_offset?std::uint64_t(source.material_offset)+std::uint64_t(source.vertex_count/3)*64:
            std::uint64_t(source.vertex_count)*16U;
        require(bytes<=UINT32_MAX && (!source.material_offset || (source.material_offset%16==0
            && source.material_offset>=std::uint64_t(source.vertex_count)*16)),"Invalid resident material range");
        // Shared allocation remains vertex-sized; pad to whole triangles while
        // retaining the actual BLAS vertex count below.
        auto target=producer->prepare_shared_geometry(Uint32((bytes+47)/48*3));
        require(target.resource,"DXR geometry allocation failed");
        target.vertex_count=source.vertex_count;
        if(target.resource!=geometry_source || bytes!=geometry_bytes) {
            if(vulkan.bridge) vulkan_geometry.open(*producer,bytes,true);
            else {
                imported_geometry.Reset();
                HANDLE handle=static_cast<HANDLE>(producer->export_geometry_handle());
                require(handle,"DXR geometry export failed");
                const auto hr=native->OpenSharedHandle(handle,IID_ID3D12Resource,reinterpret_cast<void**>(imported_geometry.GetAddressOf()));
                CloseHandle(handle);require(SUCCEEDED(hr),"SDL geometry import failed");
            }
            geometry_source=target.resource;geometry_bytes=bytes;
        }
        if(!vulkan.bridge && !geometry_completion) {
            HANDLE handle=static_cast<HANDLE>(producer->export_geometry_completion_handle());
            require(handle,"Geometry completion export failed");
            const auto hr=native->OpenSharedHandle(handle,IID_ID3D12Fence,reinterpret_cast<void**>(geometry_completion.GetAddressOf()));
            CloseHandle(handle);require(SUCCEEDED(hr),"Geometry completion import failed");
        }
        command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
        if(vulkan.bridge) {
            require(vulkan.bridge->wait_timeline(device,vulkan_geometry.ready,target.ready_value),SDL_GetError());
            dependency_pending=true;
            require(vulkan_geometry_bridge->copy_to_external(command,source.buffer,vulkan_geometry.buffer,bytes,Uint32(bytes)),SDL_GetError());
        } else require(geometry_bridge->copy_to_external(command,source.buffer,imported_geometry.Get(),Uint32(bytes)),SDL_GetError());
        const bool submitted=SDL_SubmitGPUCommandBuffer(command);command=nullptr;require(submitted,SDL_GetError());
        dependency_pending=true;
        const auto value=++geometry_serial;
        // Signal is queued only after the copy submission succeeds. DXR waits
        // on its independent incoming timeline; the CPU never waits for model
        // rasterization/caster copies before encoding the trace.
        if(vulkan.bridge) require(vulkan_geometry_bridge->signal_timeline(device,vulkan_geometry.done,value),SDL_GetError());
        else require(geometry_bridge->signal_fence(device,geometry_completion.Get(),value),SDL_GetError());
        require(producer->wait_for_geometry(value),"DXR geometry queue dependency failed");
        return target;
    }
    void render(const Scene& scene,Camera camera,Vec3 light,std::optional<ReceiverPlane> plane,
        const GpuScene::RayGeometryOutput* geometry,const DxrShadows::ReflectionInput* reflection=nullptr) {
        output={};finish();
        DxrShadows::ResidentGeometry resident_geometry;
        if(geometry) resident_geometry=copy_geometry(*geometry,reflection!=nullptr);
        auto bound_reflection=reflection?*reflection:DxrShadows::ReflectionInput{};
        if(reflection && geometry && geometry->material_offset) {
            bound_reflection.resident_materials=resident_geometry.resource;
            bound_reflection.resident_material_offset=geometry->material_offset;
        }
        if(!producer->render_resident(scene,camera,light,plane,geometry?&resident_geometry:nullptr,nullptr,true,true,reflection?&bound_reflection:nullptr))
            throw std::runtime_error("DXR producer: "+producer->status());
        if(!vulkan.bridge && !ready_fence) {
            HANDLE shared=static_cast<HANDLE>(producer->export_ready_fence_handle());
            require(shared,"Native producer fence export failed");
            const auto hr=native->OpenSharedHandle(shared,IID_ID3D12Fence,reinterpret_cast<void**>(ready_fence.GetAddressOf()));
            CloseHandle(shared);require(SUCCEEDED(hr),"Native producer fence import failed");
        }
        auto source=producer->resident_output();
        bytes_per_pixel=source.bytes_per_pixel;
        const auto bytes=std::uint64_t(source.row_bytes)*source.height;
        require(bytes && bytes<=std::numeric_limits<Uint32>::max(),"Native mask too large");
        // Opening a shared handle may create a different COM wrapper. Its
        // reference need not keep the producer wrapper's address from reuse.
        // A resize therefore invalidates the import even if pointers match.
        if(source.resource!=imported_source || bytes!=imported_bytes) {
            if(vulkan.bridge) {
                vulkan.open(*producer,bytes);
                source=producer->resident_output();
            } else {
            imported.Reset();imported_source=nullptr;
            HANDLE shared=static_cast<HANDLE>(producer->export_resident_handle());
            require(shared,"Native shadow export failed");
            const auto hr=native->OpenSharedHandle(shared,IID_ID3D12Resource,reinterpret_cast<void**>(imported.GetAddressOf()));
            CloseHandle(shared);require(SUCCEEDED(hr),"Native shadow import failed");
            }
            imported_source=source.resource;
            imported_bytes=bytes;
        }
        if(capacity!=bytes) {
            if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);
            buffer=nullptr;download=nullptr;capacity=0;
            SDL_GPUBufferCreateInfo info{};info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;info.size=Uint32(bytes);
            buffer=SDL_CreateGPUBuffer(device,&info);require(buffer,SDL_GetError());capacity=Uint32(bytes);
        }
        command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
        if(vulkan.bridge) {
            if(!vulkan.bridge->wait_timeline(device,vulkan.ready,source.ready_value))
                throw std::runtime_error(std::string("Vulkan shadow dependency: ")+SDL_GetError());
            dependency_pending=true;
            if(!vulkan.bridge->copy_external(command,vulkan.buffer,bytes,buffer,capacity))
                throw std::runtime_error(std::string("Vulkan shadow copy: ")+SDL_GetError());
        } else {
        if(!bridge->wait_fence(device,ready_fence.Get(),source.ready_value))
            throw std::runtime_error(std::string("Producer dependency: ")+SDL_GetError());
        if(!bridge->copy_buffer(command,imported.Get(),buffer,capacity))
            throw std::runtime_error(std::string("Native copy: ")+SDL_GetError());
        }
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require(fence,SDL_GetError());
        dependency_pending=false;
        // Effects are submitted on this same SDL queue after the copy. Retain
        // source/import/fence until next reuse; destination cycling protects
        // previously submitted consumers without a CPU wait here.
        output={device,buffer,source.width,source.height,source.row_bytes};
        status=geometry?"GPU-resident SDL geometry and DXR shadows":"GPU-resident SDL DXR shadows";
    }
    void read(std::vector<std::uint8_t>& pixels) {
        require(device && buffer && output.buffer==buffer && capacity,"No resident DXR mask");finish();
        if(!download) {
            SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;info.size=capacity;
            download=SDL_CreateGPUTransferBuffer(device,&info);require(download,SDL_GetError());
        }
        command=SDL_AcquireGPUCommandBuffer(device);require(command,SDL_GetError());
        auto* pass=SDL_BeginGPUCopyPass(command);require(pass,SDL_GetError());
        SDL_GPUBufferRegion source{buffer,0,capacity};SDL_GPUTransferBufferLocation target{download,0};
        SDL_DownloadFromGPUBuffer(pass,&source,&target);SDL_EndGPUCopyPass(pass);
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;require(fence,SDL_GetError());finish();
        const auto row=std::size_t(output.width)*bytes_per_pixel;
        pixels.resize(row*output.height);
        const auto* data=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,download,false));require(data,SDL_GetError());
        for(unsigned y=0;y<output.height;++y) std::memcpy(pixels.data()+std::size_t(y)*row,data+std::size_t(y)*output.packed_row_bytes,row);
        SDL_UnmapGPUTransferBuffer(device,download);
    }
#endif
};
SdlDxrShadows::SdlDxrShadows():impl_(std::make_unique<Impl>()) {}
SdlDxrShadows::~SdlDxrShadows()=default;
bool SdlDxrShadows::request_vulkan_interop(std::uint32_t properties) {
#if defined(STARFOX_NATIVE_SDL_DXR)
    static VkPhysicalDeviceVulkan12Features features=[] {
        VkPhysicalDeviceVulkan12Features value{};value.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        value.timelineSemaphore=VK_TRUE;return value;
    }();
    static const char* extensions[]={VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME};
    static SDL_GPUVulkanOptions options{VK_API_VERSION_1_2,&features,nullptr,2,extensions,0,nullptr};
    return SDL_SetPointerProperty(properties,SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER,&options);
#else
    (void)properties;return false;
#endif
}
GpuShadowOutput SdlDxrShadows::output() const {return impl_->bytes_per_pixel==1?impl_->output:GpuShadowOutput{};}
GpuReflectionOutput SdlDxrShadows::reflection_output() const {
    const auto& o=impl_->output;
    return impl_->bytes_per_pixel==4?GpuReflectionOutput{o.device,o.buffer,o.width,o.height,o.packed_row_bytes}:GpuReflectionOutput{};
}
bool SdlDxrShadows::render_reflections(void* device,Camera camera,const GpuScene::RayGeometryOutput& geometry,
    std::span<const std::uint32_t,256> palette,std::uint32_t environment,float roughness,std::uint32_t metallic,
    std::span<const std::uint32_t> environment_cube,std::uint32_t face_size,std::array<float,9> environment_rotation,
    const GpuBackgroundDraw* background,std::optional<ReceiverPlane> ground,float background_eye_x,const RayWater* water) {
    impl_->output={};
#if defined(STARFOX_NATIVE_SDL_DXR)
    if(!geometry.complete || !geometry.materials || (!geometry.material_offset
        && geometry.materials->triangles.size()*3!=geometry.vertex_count)) return false;
    try {
        if(impl_->device && impl_->device!=device) release_device();
        if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        DxrShadows::ReflectionInput reflection{geometry.materials,palette,environment,roughness,metallic,environment_cube,face_size,environment_rotation,background,ground,background_eye_x};
        reflection.water=water;
        impl_->render(Scene{},camera,{0,1,0},{},&geometry,&reflection);return true;
    }catch(const std::exception& error){const std::string message=error.what();release_device();impl_->status=message;}
#endif
    (void)device;(void)camera;(void)geometry;(void)palette;(void)environment;(void)roughness;(void)metallic;
    (void)environment_cube;(void)face_size;(void)environment_rotation;(void)background;(void)ground;(void)background_eye_x;(void)water;return false;
}
const std::string& SdlDxrShadows::status() const {return impl_->status;}
void SdlDxrShadows::release_device() noexcept {impl_.reset(new Impl);}
bool SdlDxrShadows::render_resident(void* device,const Scene& scene,Camera camera,Vec3 light,std::optional<ReceiverPlane> plane,
    const GpuScene::RayGeometryOutput* geometry) {
    impl_->output={};
#if defined(STARFOX_NATIVE_SDL_DXR)
    try {
        if(impl_->device && impl_->device!=device) release_device();
        if(!impl_->device) impl_->initialize(static_cast<SDL_GPUDevice*>(device));
        impl_->render(scene,camera,light,plane,geometry);return true;
    } catch(const std::exception& error) {
        const std::string message=error.what();release_device();impl_->status=message;
    }
#endif
    (void)device;(void)scene;(void)camera;(void)light;(void)plane;(void)geometry;
    return false;
}
bool SdlDxrShadows::readback(std::vector<std::uint8_t>& pixels) {
#if defined(STARFOX_NATIVE_SDL_DXR)
    try {impl_->read(pixels);return true;}
    catch(const std::exception& error) {impl_->status=error.what();}
#endif
    pixels.clear();return false;
}
}
