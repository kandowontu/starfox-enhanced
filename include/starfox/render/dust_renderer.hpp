#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/simulation/dust_system.hpp"
#include "starfox/timing/fixed_step.hpp"

namespace starfox::render {

class DustRenderer {
public:
    DustRenderer(
        const assets::RomImage& rom,
        const assets::SymbolMap& symbols);

    void draw(
        const simulation::DustSystem& dust,
        const timing::RenderTransform& camera,
        const simulation::MatrixQ15& view_matrix,
        Framebuffer& target) const noexcept;

    void draw_grid(
        const timing::RenderTransform& camera,
        const simulation::MatrixQ15& view_matrix,
        Framebuffer& target) const noexcept;

private:
    const assets::RomImage* rom_{};
    std::uint32_t star_colours_{};
};

} // namespace starfox::render
