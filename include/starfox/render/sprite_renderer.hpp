#pragma once

#include "starfox/render/framebuffer.hpp"
#include "starfox/simulation/snes_ppu.hpp"

#include <cstdint>
#include <optional>

namespace starfox::simulation {
struct MeterState;
}

namespace starfox::render {

class SpriteRenderer {
public:
    void draw_objects(
        const simulation::SnesPpuState& ppu,
        Framebuffer& target,
        std::optional<std::uint8_t> priority = std::nullopt) const noexcept;
    void draw_meters(
        const simulation::MeterState& meters,
        Framebuffer& target) const noexcept;
};

} // namespace starfox::render
