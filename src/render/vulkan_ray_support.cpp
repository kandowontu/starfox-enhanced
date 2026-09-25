#include "starfox/render/vulkan_ray_support.hpp"

#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(__linux__)
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "starfox/render/sdl_vulkan_bridge.h"
#include <SDL3/SDL.h>
#include <cstring>
#endif

namespace starfox::render::shadows {

bool request_vulkan_ray_query(std::uint32_t properties) {
#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(__linux__)
    static VkPhysicalDeviceVulkan12Features core=[] {
        VkPhysicalDeviceVulkan12Features value{};
        value.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        value.timelineSemaphore=VK_TRUE;
        value.bufferDeviceAddress=VK_TRUE;
        return value;
    }();
    static VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration=[] {
        VkPhysicalDeviceAccelerationStructureFeaturesKHR value{};
        value.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        value.accelerationStructure=VK_TRUE;
        return value;
    }();
    static VkPhysicalDeviceRayQueryFeaturesKHR ray_query=[] {
        VkPhysicalDeviceRayQueryFeaturesKHR value{};
        value.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        value.rayQuery=VK_TRUE;
        return value;
    }();
    core.pNext=&acceleration;
    acceleration.pNext=&ray_query;
    static const char* extensions[]{
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    };
    static SDL_GPUVulkanOptions options{
        VK_API_VERSION_1_2,&core,nullptr,3,extensions,0,nullptr};
    return SDL_SetPointerProperty(properties,
        SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER,&options);
#else
    (void)properties;
    return false;
#endif
}

VulkanRaySupport query_vulkan_ray_query(void* raw) {
#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(__linux__)
    auto* device=static_cast<SDL_GPUDevice*>(raw);
    if(!device || !SDL_GetGPUDeviceDriver(device)
        || std::strcmp(SDL_GetGPUDeviceDriver(device),"vulkan")!=0)
        return {false,"Vulkan ray query requires the Vulkan GPU backend"};
    if(!SDL_GetBooleanProperty(SDL_GetGPUDeviceProperties(device),
            "starfox.vulkan.ray_query.enabled",false))
        return {false,"Vulkan ray-query device features were not enabled"};
    auto* bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(
        SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(device),
            STARFOX_SDL_VULKAN_BRIDGE,nullptr));
    if(!bridge || bridge->version!=2 || !bridge->device
        || !bridge->physical_device || !bridge->get_instance_proc
        || !bridge->get_device_proc)
        return {false,"SDL native Vulkan device bridge unavailable"};
    const auto get_features=reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
        bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceFeatures2"));
    if(!get_features)
        return {false,"Vulkan physical-device feature query unavailable"};
    VkPhysicalDeviceRayQueryFeaturesKHR ray_query{};
    ray_query.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{};
    acceleration.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    acceleration.pNext=&ray_query;
    VkPhysicalDeviceVulkan12Features core{};
    core.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    core.pNext=&acceleration;
    VkPhysicalDeviceFeatures2 features{};
    features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext=&core;
    get_features(bridge->physical_device,&features);
    if(!core.bufferDeviceAddress || !acceleration.accelerationStructure
        || !ray_query.rayQuery)
        return {false,"GPU lacks Vulkan buffer address, acceleration structure, or ray query"};
    for(const auto* entry:{"vkCreateAccelerationStructureKHR",
            "vkGetAccelerationStructureBuildSizesKHR",
            "vkCmdBuildAccelerationStructuresKHR"})
        if(!bridge->get_device_proc(bridge->device,entry))
            return {false,std::string{"Vulkan ray entry unavailable: "}+entry};
    return {true,"Vulkan ray-query device ready"};
#else
    (void)raw;
    return {};
#endif
}

} // namespace starfox::render::shadows
