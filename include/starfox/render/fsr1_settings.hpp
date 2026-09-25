#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace starfox::render {

// Separate from DLSS: its fourth active mode is DLAA, not FSR Performance.
enum class Fsr1Mode : std::uint8_t {
    off, ultra_quality, quality, balanced, performance
};

struct Fsr1Extent {
    std::uint32_t width{}, height{};
    constexpr bool operator==(const Fsr1Extent&) const = default;
};

inline constexpr std::array<std::string_view,5> fsr1_mode_names{
    "OFF", "ULTRA QUALITY", "QUALITY", "BALANCED", "PERFORMANCE"};

// AMD's linear scaling ratios: 1.3x, 1.5x, 1.7x, 2x. Round input dimensions
// upward so odd/ultrawide outputs never request more aggressive downscaling.
// Widen arithmetic before multiplication; retain zero extents for minimized
// windows, which callers must skip rather than allocating a bogus 1x1 image.
constexpr Fsr1Extent fsr1_input_extent(Fsr1Extent output, Fsr1Mode mode) {
    constexpr std::array<std::uint32_t,5> denominators{10,13,15,17,20};
    const auto index=static_cast<unsigned>(mode);
    if(index>=denominators.size() || mode==Fsr1Mode::off) return output;
    const auto divisor=denominators[index];
    const auto scale=[divisor](std::uint32_t value) {
        return static_cast<std::uint32_t>((std::uint64_t(value)*10+divisor-1)/divisor);
    };
    return {scale(output.width),scale(output.height)};
}

} // namespace starfox::render
