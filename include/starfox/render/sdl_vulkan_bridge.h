#pragma once
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
/* Private ABI for our pinned SDL build. Include Vulkan declarations first.
 * All handles are borrowed and expire with the SDL GPU device. This exposes
 * no queue: external submissions must use a separately synchronized bridge. */
#define STARFOX_SDL_VULKAN_BRIDGE "starfox.gpu.vulkan.bridge.v2"
typedef struct StarfoxSdlVulkanBridgeV2 {
    uint32_t version;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    PFN_vkGetInstanceProcAddr get_instance_proc;
    PFN_vkGetDeviceProcAddr get_device_proc;
    uint32_t queue_family;
    /* Semaphore/source belong to this device and must outlive the submitted
     * copy. Source is externally owned; copy acquires and releases ownership.
     * Call outside active SDL passes, with a transfer-source-capable buffer. */
    bool (*wait_timeline)(void *device, VkSemaphore semaphore, uint64_t value);
    bool (*copy_external)(void *command, VkBuffer source, uint64_t source_bytes,
                          void *destination, uint32_t bytes);
} StarfoxSdlVulkanBridgeV2;

#define STARFOX_SDL_VULKAN_GEOMETRY_BRIDGE "starfox.gpu.vulkan.geometry.v1"
typedef struct StarfoxSdlVulkanGeometryBridgeV1 {
    uint32_t version;
    /* Destination is externally owned, transfer-destination-capable. Acquire
     * and release ownership around the copy; caller waits before tracing. */
    bool (*copy_to_external)(void *command, void *source, VkBuffer destination,
                             uint64_t destination_bytes, uint32_t bytes);
    bool (*signal_timeline)(void *device, VkSemaphore semaphore, uint64_t value);
} StarfoxSdlVulkanGeometryBridgeV1;

#define STARFOX_SDL_VULKAN_RAY_BRIDGE "starfox.gpu.vulkan.ray.bridge.v3"
typedef struct StarfoxSdlVulkanRayBridgeV3 {
    uint32_t version;
    /* Record native Vulkan work inside one SDL command buffer. The caller
     * must be outside SDL passes and finish every prepared output. */
    VkCommandBuffer (*command)(void *sdl_command);
    VkBuffer (*prepare_write)(void *sdl_command, void *sdl_buffer);
    bool (*finish_write)(void *sdl_command, void *sdl_buffer);
    /* Copy submitted SDL geometry into a native AS input in the same command
     * buffer, with transfer-to-AS visibility and no queue ownership transfer. */
    bool (*copy_ray_range)(void *sdl_command, void *sdl_source,
                           uint64_t source_offset, VkBuffer destination,
                           uint64_t capacity, uint32_t bytes);
} StarfoxSdlVulkanRayBridgeV3;
