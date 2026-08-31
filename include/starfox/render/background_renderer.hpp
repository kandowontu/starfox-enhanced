#pragma once

#include "starfox/render/framebuffer.hpp"
#include "starfox/simulation/snes_ppu.hpp"

#include <cstdint>

namespace starfox::render {

enum class TilePriorityPass {
    all,
    low,
    high,
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
        bool transparent_cgram_black = false) const noexcept;
    void draw_bg2(
        const simulation::SnesPpuState& ppu,
        std::int32_t scroll_x,
        std::int32_t scroll_y,
        Framebuffer& target,
        TilePriorityPass priority = TilePriorityPass::all,
        std::int32_t horizontal_origin = 0,
        bool extend_horizontal = true,
        bool wrap_horizontal = true,
        bool transparent_cgram_black = false) const noexcept;
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
