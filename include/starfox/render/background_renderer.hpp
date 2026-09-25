#pragma once

#include "starfox/render/framebuffer.hpp"
#include "starfox/simulation/snes_ppu.hpp"

#include <cstdint>
#include <span>

namespace starfox::render {

// Center-height left wall, including source HDMA scroll. Transparent wall
// texels fall back to the darkest palette entry, never the backdrop color.
[[nodiscard]] std::uint8_t tunnel_wall_index(
    const simulation::SnesPpuState& ppu) noexcept;
[[nodiscard]] std::uint8_t tunnel_border_index(
    const simulation::SnesPpuState& ppu,
    unsigned screen_y, unsigned screen_x) noexcept;

enum class TilePriorityPass {
    all,
    low,
    high,
};

// Authored non-repeating artwork embedded in an otherwise repeating tilemap.
// Only matching indexed pixels outside the native window are replaced.
struct BackgroundUniqueRegion {
    static constexpr std::int32_t suppress_all_side_copies = 0x40000000;
    // Remove the source artwork when an enhancement supplies its replacement.
    static constexpr std::int32_t suppress_every_copy = 0x20000000;
    std::int32_t left, top, right, bottom;
    std::uint8_t first_colour, last_colour, replacement_colour;
    // Nonzero selects a same-row, moon-free atlas sample instead of a flat
    // fill. Preserve source palette/gradient/clouds in landscape surrounds.
    // The suppress_all_side_copies bit additionally removes every matching
    // occurrence outside the native viewport when the first atlas page also
    // contains a wrapped duplicate (Original 1-4's green planet).
    std::int32_t replacement_x_offset{};
};

class BackgroundRenderer {
public:
    void draw_bg1(
        const simulation::SnesPpuState& ppu,
        Framebuffer& target,
        TilePriorityPass priority = TilePriorityPass::all,
        std::int32_t horizontal_origin = 0,
        bool extend_horizontal = true,
        std::uint32_t horizontal_inset = 0,
        bool transparent_cgram_black = false,
        // Match drawing into an inset staging bitmap followed by mosaic
        // compositing: inset edges round forward to whole mosaic blocks.
        bool mosaic_staging_inset = false,
        bool text_outline = false) const noexcept;
    void draw_bg2(
        const simulation::SnesPpuState& ppu,
        std::int32_t scroll_x,
        std::int32_t scroll_y,
        Framebuffer& target,
        TilePriorityPass priority = TilePriorityPass::all,
        std::int32_t horizontal_origin = 0,
        bool extend_horizontal = true,
        bool wrap_horizontal = true,
        bool transparent_cgram_black = false,
        std::uint32_t single_occurrence_top_rows = 0U,
        std::span<const BackgroundUniqueRegion> unique_regions = {},
        bool ending_star_extension = false, bool game_over_star_extension = false,
        std::uint32_t sky_source_min = 0) const noexcept;
    void draw_bg3(
        const simulation::SnesPpuState& ppu,
        Framebuffer& target,
        TilePriorityPass priority = TilePriorityPass::all,
        std::int32_t horizontal_origin = 0,
        bool extend_horizontal = true) const noexcept;
    void draw_title_foreground(
        const simulation::SnesPpuState& ppu,
        std::int32_t bg2_scroll_x,
        std::int32_t bg2_scroll_y,
        Framebuffer& target,
        std::int32_t horizontal_origin = 0,
        bool include_bg1_overlay = true,
        bool extend_bg2_unwrapped = false) const noexcept;
};

} // namespace starfox::render
