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
        TilePriorityPass priority = TilePriorityPass::all) const noexcept;
    void draw_bg2(
        const simulation::SnesPpuState& ppu,
        std::int32_t scroll_x,
        std::int32_t scroll_y,
        Framebuffer& target,
        TilePriorityPass priority = TilePriorityPass::all) const noexcept;
    void draw_bg3(
        const simulation::SnesPpuState& ppu,
        Framebuffer& target,
        TilePriorityPass priority = TilePriorityPass::all) const noexcept;
};

} // namespace starfox::render
