#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace starfox::render {
// Append styles so existing saved effect IDs retain their meaning.
enum class Effect : std::uint8_t {
    off, cel_drawn, ink, neon, monochrome, dithered, blueprint, bloom,
    sepia, thermal, night_vision, pastel, comic, vaporwave, count
};
inline constexpr auto effect_count = static_cast<std::uint8_t>(Effect::count);
inline constexpr bool selectable_effect(std::uint8_t value, bool world) {
    return value < effect_count && value != static_cast<std::uint8_t>(Effect::bloom)
        && value != static_cast<std::uint8_t>(world ? Effect::cel_drawn : Effect::blueprint);
}
inline constexpr std::uint8_t next_effect(std::uint8_t value, bool world, bool backwards) {
    do { value = (value + (backwards ? effect_count - 1U : 1U)) % effect_count; }
    while (!selectable_effect(value, world));
    return value;
}
inline constexpr std::array<std::string_view, 4> bloom_names{"OFF", "LOW", "MEDIUM", "HEAVY"};
inline constexpr std::array<std::string_view, effect_count> effect_names{
    "OFF", "CEL-DRAWN", "INK", "NEON", "MONOCHROME", "DITHERED", "BLUEPRINT", "BLOOM",
    "SEPIA", "THERMAL", "NIGHT VISION", "PASTEL", "COMIC", "VAPORWAVE"};
}
