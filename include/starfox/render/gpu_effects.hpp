#pragma once
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/effect_types.hpp"
#include "starfox/render/software_renderer.hpp"
#include <memory>
#include <string>
namespace starfox::render {
struct GpuEffectSettings {
    std::uint32_t hdr{}, chromatic{}, smoothing{}, model_effect{}, world_effect{};
    std::uint32_t model_intensity{100},world_intensity{100},anti_aliasing{};
    std::uint32_t lighting{};
    const SurfaceBuffer* surfaces{};
    std::int32_t surface_x{},surface_y{};
    std::uint32_t bloom_model{},bloom_world{};
    std::uint32_t filter{},highlight_filter{};
    bool overlay_filter{};
    std::span<const std::uint8_t> shadow_mask;
    std::uint32_t shadow_width{},shadow_height{};
    std::int32_t shadow_offset_y{};
    // Optional pre/post-bloom snapshots for the separately scaled glow layer.
    std::vector<std::uint8_t>* bloom_base{};
    std::vector<std::uint8_t>* bloom_glow{};
    void* presentation_texture{}; // Optional borrowed D3D11 RGBA8 texture; defer CPU readback.
    void* presentation_glow_texture{};
    void* presentation_model_texture{};
};
// Compute-shader implementation. Device is borrowed from SDL's D3D11 renderer;
// software/other backends decline without changing the input frame.
class GpuEffects {
public:
    GpuEffects();
    ~GpuEffects();
    bool apply(void* device,const Framebuffer&,std::vector<std::uint8_t>&,
        const GpuEffectSettings&);
    const std::string& status() const;
    bool readback(std::vector<std::uint8_t>&);
    void release_device() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
