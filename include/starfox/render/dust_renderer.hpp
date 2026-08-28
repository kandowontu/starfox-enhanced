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
        std::size_t active_count,
        const timing::RenderTransform& camera,
        const simulation::MatrixQ15& view_matrix,
        Framebuffer& target) const noexcept;

    void draw_grid(
        const timing::RenderTransform& camera,
        const simulation::MatrixQ15& view_matrix,
        Framebuffer& target) const noexcept;

    void draw_grid_lines(
        const timing::RenderTransform& camera,
        const simulation::MatrixQ15& view_matrix,
        std::uint64_t source_frame,
        Framebuffer& target) const noexcept;

private:
    const assets::RomImage* rom_{};
    std::uint32_t star_colours_{};
    mutable bool grid_line_state_initialized_{};
    mutable std::uint64_t grid_line_source_frame_{};
    mutable std::int16_t grid_line_previous_x_{};
    mutable std::int16_t grid_line_previous_y_{};
    mutable std::int16_t grid_line_frame_start_x_{};
    mutable std::int16_t grid_line_frame_start_y_{};
};

} // namespace starfox::render
