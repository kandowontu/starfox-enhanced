#pragma once
#include <algorithm>
#include <cstdint>
namespace starfox::render {
// The 256x224 cartridge canvas represents a 4:3 display, not square pixels.
// Expanded host canvases already encode their chosen widescreen proportions.
constexpr std::uint32_t presentation_width(std::uint32_t width,
    std::uint32_t height) noexcept {
    return height && std::uint64_t(width)*224U==std::uint64_t(height)*256U
        ? static_cast<std::uint32_t>((std::uint64_t(height)*4U+1U)/3U) : width;
}
// A mobile widescreen canvas should contain the whole display rather than
// letterbox a desktop preset. Keep the source raster height and grow only its
// horizontal field of view; round to the nearest source pixel.
constexpr std::uint32_t device_fitted_width(std::uint32_t raster_height,
    std::uint32_t display_width,std::uint32_t display_height,
    std::uint32_t minimum_width,std::uint32_t maximum_width) noexcept {
    if (!display_width || !display_height || minimum_width>maximum_width)
        return minimum_width;
    const auto rounded=(std::uint64_t(raster_height)*display_width
        +display_height/2U)/display_height;
    return static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
        rounded,minimum_width,maximum_width));
}
constexpr float presentation_to_raster_x(float x,std::uint32_t width,
    std::uint32_t height) noexcept {
    const auto display= presentation_width(width,height);
    return display ? x*static_cast<float>(width)/static_cast<float>(display) : x;
}
}
