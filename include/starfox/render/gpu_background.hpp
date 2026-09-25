#pragma once
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/gpu_raster.hpp"
#include <vector>

namespace starfox::render {
struct EnvironmentEffects;
struct GpuBackgroundSettings {
    unsigned layer{1}; // BG1, BG2 or BG3.
    TilePriorityPass priority{TilePriorityPass::all};
    int horizontal_origin{};
    bool extend_horizontal{true};
    unsigned horizontal_inset{};
    bool transparent_cgram_black{};
    bool mosaic_staging_inset{}; // BG1 inset applied before a second mosaic composite.
    bool text_outline{}; // Enhanced EX menu only: reserve 254 white / 255 black.
    PixelLayer tag{PixelLayer::background};
    int scroll_x{},scroll_y{}; // BG2's host-interpolated registers.
    bool wrap_horizontal{true};
    bool ending_star_extension{}; // BG_CRED: extend with star-only atlas patches.
    bool game_over_star_extension{}; // BG_AND: keep Andross unique; extend only stars.
    unsigned single_occurrence_top_rows{};
    unsigned sky_source_min{}; // Optional authored top row; extend only outside native view.
    std::vector<BackgroundUniqueRegion> unique_regions;
    // Optional authored BG2 terrain rows [first,last), in source tilemap
    // pixels, not screen coordinates. Empty by default; never applies to tunnels.
    std::array<std::uint32_t,2> terrain_source_rows{};
    std::array<std::uint32_t,2> logical_viewport{}; // optional independent raster dimensions
    std::array<float,2> raster_jitter{};
    const EnvironmentEffects* reflection_environment{}; // borrowed during reflection submission only
};
class GpuBackground {
public:
    GpuBackground();~GpuBackground();
    // Uploads raw VRAM/CGRAM, then decodes/rasterizes on GPU. No CPU tile
    // expansion, submission, wait or readback. Caller cancels on failure and
    // consumes borrowed output before another enqueue/release. All handles
    // share one device. Dimensions are stored pixels, divisible by scale.
    GpuRasterOutput enqueue(void* device,void* command,const simulation::SnesPpuState&,
        unsigned width,unsigned height,unsigned scale,const GpuBackgroundSettings& = {});
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
