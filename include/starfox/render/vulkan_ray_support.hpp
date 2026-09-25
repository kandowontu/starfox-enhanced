#pragma once

#include <cstdint>
#include <string>

namespace starfox::render::shadows {

struct VulkanRaySupport {
    // Device capability only. The menu must not call this hardware RT until
    // acceleration-structure construction and ray dispatch are integrated.
    bool available{};
    std::string status{"Vulkan ray query unavailable"};
};

// Request the native Vulkan features before SDL creates its GPU device. A
// failed request is optional: the caller must retry the ordinary GPU device.
bool request_vulkan_ray_query(std::uint32_t properties);
VulkanRaySupport query_vulkan_ray_query(void* gpu_device);

} // namespace starfox::render::shadows
