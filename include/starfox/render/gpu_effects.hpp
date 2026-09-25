#pragma once
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/effect_types.hpp"
#include "starfox/render/environment_effects.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/portable_shadows.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include <memory>
#include <optional>
#include <string>
namespace starfox::render {
struct GpuEffectSettings {
    EnvironmentEffects environment;
    unsigned manipulation{},manipulation_intensity{100};
    unsigned material{};
    // Temporal manipulations: one effects instance per eye/output.
    unsigned persistence_mode{}; // 0 off, 1 fading trails, 2 long exposure.
    bool persistence_models{},persistence_world{};
    unsigned persistence_intensity{100};
    double presentation_seconds{};
    std::uint64_t scene_epoch{};
    unsigned persistence_slot{}; // 0 mono, 1 left eye, 2 right eye.
    // Intermediate passes in a multi-pass presentation neither advance nor
    // discard the final pass's history. Normal disabled presentation frees it.
    bool preserve_persistence{};
    // Stored-pixel Y bounds for the source shutter and its open band. X guard
    // uses the source's exact native-window rule unless expanded is selected.
    struct HorizontalWipe {
        std::int32_t band_top{},band_bottom{},open_top{},open_bottom{};
        std::int32_t guard_width{},origin_x{};
        bool expanded{};
    };
    std::optional<HorizontalWipe> horizontal_wipe;
    struct Circle {
        static constexpr std::int32_t exact_limit=1048575;
        std::int32_t x{}, y{}, radius{}; // Stored pixels.
        std::int32_t left{}, top{}, right{}, bottom{}; // Stored clip bounds.
        std::uint32_t red{}, green{}, blue{}; // Brightness-adjusted five-bit addends.
        bool subtract{}, half{}, affect_sprites{};
    };
    std::optional<Circle> circle;
    std::uint32_t background_subtract{};
    bool background_subtract_protect_models{};
    struct ColourMath {
        std::uint8_t red{}, green{}, blue{}; // Expanded fixed colour, 0..255.
        bool subtract{}, half{}, affect_sprites{};
    };
    std::optional<ColourMath> colour_math;
    struct PlanetFade {
        std::int32_t left{},top{},right{},bottom{}; // Inclusive logical bounds.
        std::uint32_t isolate_amount{},level_amount{};
        bool isolate{},level{};
        bool coverage{};
        std::array<std::uint32_t,32> rows{};
    };
    std::optional<PlanetFade> planet_fade;
    struct SubtractiveOverlay {
        const Framebuffer* frame{}; // Logical indexed artwork, borrowed for apply().
        std::uint32_t brightness{30};
        // Optional resident packed indices at logical resolution, on the same
        // device. When present, frame defines extent only; its pixels are not
        // uploaded. Keep the borrowed buffers alive until effects complete.
        GpuRasterOutput resident{};
    };
    // Ordered planet artwork, then briefing text. Filter each independently
    // before five-bit subtraction and alpha composition onto the main scene.
    std::array<std::optional<SubtractiveOverlay>,2> subtractive_overlays{};
    std::span<const Rgba8> overlay_palette;
    struct WindowMask {
        std::array<std::uint32_t,192> rows{}; // left | (right << 8), cartridge bytes.
        std::int32_t origin_x{}, origin_y{16};
        std::uint32_t logic{};
        bool expand_x{}, expand_y{};
    };
    std::optional<WindowMask> window_mask;
    struct HostOverlay {
        std::array<std::uint32_t,192> bits{}; // Row-major opaque glyph bits.
        std::int32_t x{},y{}; // Logical pixels, one-pixel black shadow.
        std::uint32_t width{},height{};
    };
    std::optional<HostOverlay> host_overlay;
    std::optional<HostOverlay> confirmation_overlay; // White border, opaque black panel, white ink.
    struct SetupOverlay {
        const Framebuffer* frame{}; // Logical indexed ink; borrowed for apply().
        std::int32_t left{},right{}; // Inclusive, relative to centred 256px canvas.
        std::uint32_t brightness{15};
    };
    std::optional<SetupOverlay> setup_overlay;
    bool touch_controls{};
    std::uint32_t hdr{}, chromatic{}, smoothing{}, model_effect{}, world_effect{};
    std::uint32_t model_intensity{100},world_intensity{100},anti_aliasing{};
    std::uint32_t lighting{};
    const SurfaceBuffer* surfaces{};
    std::int32_t surface_x{},surface_y{};
    std::uint32_t bloom_model{},bloom_world{};
    std::uint32_t filter{},highlight_filter{};
    bool overlay_filter{};
    std::span<const std::uint8_t> shadow_mask;
    shadows::GpuShadowOutput resident_shadow;
    // Borrowed RGBA ray result. Only visible model surfaces receive it; alpha
    // zero denotes a primary-ray miss. Never interpreted as a shadow mask.
    shadows::GpuReflectionOutput resident_reflection;
    std::int32_t reflection_offset_y{};
    std::uint32_t reflection_intensity{100};
    std::uint32_t shadow_width{},shadow_height{};
    std::int32_t shadow_offset_y{};
    bool shadow_before_style{}; // Combined native pipeline preserves early shadow order.
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
