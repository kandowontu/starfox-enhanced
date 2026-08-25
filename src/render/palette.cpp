#include "starfox/render/palette.hpp"

#include <algorithm>
#include <stdexcept>

namespace starfox::render {
namespace {

constexpr std::array<Rgba8, 16> kPreviewPalette{{
    {0, 0, 0},       {48, 48, 60},    {112, 28, 32},  {38, 70, 150},
    {188, 72, 24},   {24, 130, 132},  {82, 20, 26},   {35, 52, 120},
    {118, 46, 132},  {38, 110, 52},   {82, 82, 92},   {118, 118, 128},
    {156, 156, 166}, {202, 202, 210}, {238, 238, 242}, {255, 255, 255},
}};

Rgba8 decode_bgr555(std::uint16_t word) noexcept {
    const auto expand = [](std::uint16_t value) {
        const auto five = static_cast<std::uint8_t>(value & 0x1fU);
        return static_cast<std::uint8_t>((five << 3U) | (five >> 2U));
    };
    return {expand(word), expand(word >> 5U), expand(word >> 10U), 255};
}

Rgba8 apply_brightness(Rgba8 colour, std::uint8_t brightness) noexcept {
    const auto scale = [brightness](std::uint8_t component) {
        return static_cast<std::uint8_t>(
            (static_cast<std::uint16_t>(component) * brightness) / 15U);
    };
    return {scale(colour.r), scale(colour.g), scale(colour.b), colour.a};
}

} // namespace

std::span<const Rgba8, 16> preview_palette() noexcept {
    return kPreviewPalette;
}

Palette16 decode_bgr555_palette(
    const std::array<std::uint16_t, 16>& words) noexcept {
    Palette16 result{};
    for (std::size_t index = 0; index < words.size(); ++index) {
        result[index] = decode_bgr555(words[index]);
    }
    return result;
}

Palette256 decode_bgr555_palette(
    const std::array<std::uint16_t, 256>& words) noexcept {
    Palette256 result{};
    for (std::size_t index = 0; index < words.size(); ++index) {
        result[index] = decode_bgr555(words[index]);
    }
    return result;
}

Palette16 apply_snes_brightness(
    std::span<const Rgba8, 16> palette,
    std::uint8_t brightness) noexcept {
    brightness = std::min<std::uint8_t>(brightness, 15U);
    Palette16 result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = apply_brightness(palette[index], brightness);
    }
    return result;
}

Palette256 apply_snes_brightness(
    std::span<const Rgba8, 256> palette,
    std::uint8_t brightness) noexcept {
    brightness = std::min<std::uint8_t>(brightness, 15U);
    Palette256 result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = apply_brightness(palette[index], brightness);
    }
    return result;
}

void expand_rgba(const Framebuffer& source, std::vector<std::uint8_t>& destination) {
    expand_rgba(source, destination, preview_palette());
}

void expand_rgba(
    const Framebuffer& source,
    std::vector<std::uint8_t>& destination,
    std::span<const Rgba8> palette) {
    if (palette.empty()) {
        throw std::invalid_argument{"framebuffer palette is empty"};
    }
    const auto byte_count = source.pixels().size() * 4U;
    destination.resize(byte_count);
    auto output = destination.begin();
    for (const auto pixel : source.pixels()) {
        const auto colour = palette[std::min<std::size_t>(pixel, palette.size() - 1U)];
        *output++ = colour.r;
        *output++ = colour.g;
        *output++ = colour.b;
        *output++ = colour.a;
    }
}

} // namespace starfox::render
