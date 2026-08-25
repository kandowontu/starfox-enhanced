#pragma once

#include "starfox/render/framebuffer.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace starfox::render {

struct Rgba8 {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};
};

using Palette16 = std::array<Rgba8, 16>;
using Palette256 = std::array<Rgba8, 256>;

[[nodiscard]] std::span<const Rgba8, 16> preview_palette() noexcept;
[[nodiscard]] Palette16 decode_bgr555_palette(
    const std::array<std::uint16_t, 16>& words) noexcept;
[[nodiscard]] Palette256 decode_bgr555_palette(
    const std::array<std::uint16_t, 256>& words) noexcept;
[[nodiscard]] Palette16 apply_snes_brightness(
    std::span<const Rgba8, 16> palette,
    std::uint8_t brightness) noexcept;
[[nodiscard]] Palette256 apply_snes_brightness(
    std::span<const Rgba8, 256> palette,
    std::uint8_t brightness) noexcept;
void expand_rgba(const Framebuffer& source, std::vector<std::uint8_t>& destination);
void expand_rgba(
    const Framebuffer& source,
    std::vector<std::uint8_t>& destination,
    std::span<const Rgba8> palette);

} // namespace starfox::render
