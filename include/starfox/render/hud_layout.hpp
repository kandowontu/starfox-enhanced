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
    boss_health,
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

// Original and Star Fox EX each own an independent profile for 4:3, 16:9,
// 16:10, 21:9, and 32:9. The first five entries are Original and the second
// five are EX, with each group ordered like simulation::DisplayMode.
inline constexpr std::size_t hud_display_profile_count = 5U;
inline constexpr std::size_t hud_experience_profile_count = 2U;
using HudLayoutProfiles = std::array<HudLayout,
    hud_display_profile_count * hud_experience_profile_count>;

} // namespace starfox::render
