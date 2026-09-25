#pragma once
#include "starfox/render/portable_shadows.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include <array>
#include <memory>
#include <span>
#include <string>

namespace starfox::render::shadows {
// Vulkan acceleration structures + ray-query compute. The SDL GPU buffer
// stays resident for the existing shadow compositor; unsupported devices
// simply leave the portable traversal path available.
class VulkanHardwareRt {
public:
    VulkanHardwareRt();
    ~VulkanHardwareRt();
    VulkanHardwareRt(const VulkanHardwareRt&)=delete;
    VulkanHardwareRt& operator=(const VulkanHardwareRt&)=delete;
    bool available(void* sdl_device) const noexcept;
    bool render_shadows(void* sdl_device,const Scene&,Camera,Vec3,
        std::optional<ReceiverPlane>,const GpuScene::RayGeometryOutput* geometry=nullptr);
    bool render_reflections(void* sdl_device,
        const GpuScene::RayGeometryOutput& geometry,Camera camera,
        std::span<const std::uint32_t,256> palette,std::uint32_t environment,
        std::uint8_t quality,float roughness,std::uint32_t metallic,
        std::optional<ReceiverPlane> ground,const GpuBackgroundDraw* background=nullptr,
        const RayWater* water=nullptr);
    GpuShadowOutput shadow_output() const noexcept;
    GpuReflectionOutput reflection_output() const noexcept;
    const std::string& status() const noexcept;
    void release_device() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
