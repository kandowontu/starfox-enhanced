#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace starfox::render {

enum class HudElement : std::uint8_t {
    lives,
    shield,
    bombs_boost,
    comms,
    count,
};

struct HudOffset {
    std::int16_t x{};
    std::int16_t y{};
};

struct HudLayout {
    std::array<HudOffset, static_cast<std::size_t>(HudElement::count)> offsets{};

    [[nodiscard]] HudOffset& operator[](HudElement element) noexcept {
        return offsets[static_cast<std::size_t>(element)];
    }
    [[nodiscard]] const HudOffset& operator[](HudElement element) const noexcept {
        return offsets[static_cast<std::size_t>(element)];
    }
};

// One independent profile for 4:3, 16:9, 16:10, 21:9, and 32:9, in the
// same order as simulation::DisplayMode.
using HudLayoutProfiles = std::array<HudLayout, 5>;

} // namespace starfox::render
