#pragma once
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/effect_types.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string_view>

namespace starfox::render {

// Apply presentation styles to world pixels, keeping the HUD and menus intact.
inline void apply_effect(Effect model_effect, const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba, std::vector<std::uint8_t>& scratch,
    std::uint8_t model_intensity = 100U, Effect world_effect = Effect::off,
    std::uint8_t world_intensity = 100U) {
    model_intensity = std::min<std::uint8_t>(model_intensity, 100U);
    world_intensity = std::min<std::uint8_t>(world_intensity, 100U);
    if (((model_effect == Effect::off || model_intensity == 0U)
            && (world_effect == Effect::off || world_intensity == 0U)) || !frame.layer_tags_enabled()
        || rgba.size() != frame.pixels().size() * 4U) return;
    scratch = rgba;
    const auto width = frame.stored_width();
    const auto height = frame.stored_height();
    const auto step = frame.draw_scale();
    const auto luma = [&](std::size_t i) {
        return (int(scratch[i * 4]) * 77 + int(scratch[i * 4 + 1]) * 150
            + int(scratch[i * 4 + 2]) * 29) / 256;
    };
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto i = std::size_t(y) * width + x;
            if (frame.layer_tags()[i] == std::uint8_t(PixelLayer::two_d)) continue;
            const auto is_world = [](std::uint8_t tag) {
                return tag == std::uint8_t(PixelLayer::background)
                    || tag == std::uint8_t(PixelLayer::world_geometry);
            };
            const auto background = is_world(frame.layer_tags()[i]);
            const auto effect = background ? world_effect : model_effect;
            const auto intensity = background ? world_intensity : model_intensity;
            if (effect == Effect::off || intensity == 0U) continue;
            const auto light = luma(i);
            bool edge = false;
            if (effect == Effect::cel_drawn || effect == Effect::ink || effect == Effect::neon
                || effect == Effect::blueprint || effect == Effect::comic || effect == Effect::vaporwave) {
                const auto right = std::size_t(y) * width + std::min(x + step, width - 1);
                const auto below = std::size_t(std::min(y + step, height - 1)) * width + x;
                edge = std::abs(light - luma(right)) > 28 || std::abs(light - luma(below)) > 28;
            }
            // A bounded nine-tap bright pass in source-pixel units. Never
            // source light from HUD pixels, nor paint glow over the HUD.
            std::array<int, 3> glow{};
            if (effect == Effect::bloom) {
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                    const auto nx = std::clamp(int(x) + dx * int(step), 0, int(width) - 1);
                    const auto ny = std::clamp(int(y) + dy * int(step), 0, int(height) - 1);
                    const auto n = std::size_t(ny) * width + nx;
                    if (frame.layer_tags()[n] == std::uint8_t(PixelLayer::two_d)
                        || is_world(frame.layer_tags()[n]) != background) continue;
                    const auto peak = std::max({scratch[n*4], scratch[n*4+1], scratch[n*4+2]});
                    if (peak <= 160) continue;
                    for (unsigned c = 0; c < 3; ++c) glow[c] += int(scratch[n*4+c]) * (peak - 160) / 95;
                }
            }
            for (unsigned c = 0; c < 3; ++c) {
                auto value = int(scratch[i * 4 + c]);
                switch (effect) {
                case Effect::cel_drawn: value = edge ? value / 6 : std::min(255, ((value + 21) / 43) * 43); break;
                case Effect::ink: value = edge ? 16 : (light < 30 ? 24 : 235); break;
                case Effect::neon: value = edge ? (c == 0 ? 35 : 255) : value / 5; break;
                case Effect::monochrome: value = light; break;
                case Effect::dithered: {
                    constexpr std::array<int, 16> bayer{0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
                    const auto threshold = bayer[((y / step) % 4) * 4 + (x / step) % 4];
                    value = std::clamp((value * 4 + threshold * 16) / 256, 0, 4) * 255 / 4;
                    break;
                }
                case Effect::blueprint: {
                    constexpr std::array<int, 3> paper{8, 24, 58};
                    constexpr std::array<int, 3> line{130, 220, 255};
                    const auto grid = (x / step) % 16 == 0 || (y / step) % 16 == 0;
                    value = edge ? line[c] : paper[c] + (grid ? 12 : 0);
                    break;
                }
                case Effect::bloom: value = std::min(255, value + glow[c] / 12); break;
                case Effect::sepia: {
                    constexpr std::array<std::array<int, 3>, 3> weights{{
                        {101, 197, 48}, {89, 176, 43}, {70, 137, 34}}};
                    value = std::min(255, (scratch[i*4] * weights[c][0]
                        + scratch[i*4+1] * weights[c][1] + scratch[i*4+2] * weights[c][2]) / 256);
                    break;
                }
                case Effect::thermal: {
                    constexpr std::array<std::array<int, 3>, 5> palette{{
                        {8, 5, 40}, {65, 20, 150}, {220, 30, 70}, {255, 150, 15}, {255, 255, 210}}};
                    const auto band = std::min(light / 64, 3);
                    const auto fraction = light - band * 64;
                    value = (palette[band][c] * (64 - fraction)
                        + palette[band+1][c] * fraction) / 64;
                    break;
                }
                case Effect::night_vision:
                    value = c == 1 ? std::min(255, 24 + light * 6 / 5) : light / (c == 0 ? 7 : 4);
                    if ((y / step) % 2 != 0) value = value * 4 / 5;
                    break;
                case Effect::pastel: value = 96 + value * 5 / 8; break;
                case Effect::comic: {
                    const auto dot = (x / step) % 3 == 1 && (y / step) % 3 == 1 && light < 180;
                    value = edge || dot ? 18 : std::min(255, ((value + 31) / 64) * 64);
                    break;
                }
                case Effect::vaporwave: {
                    constexpr std::array<int, 3> shadow{55, 8, 100}, highlight{70, 250, 245};
                    value = (shadow[c] * (255 - light) + highlight[c] * light) / 255;
                    if (edge) value = c == 1 ? 75 : 255;
                    break;
                }
                default: break;
                }
                rgba[i * 4 + c] = static_cast<std::uint8_t>(
                    (int(scratch[i * 4 + c]) * (100 - intensity) + value * intensity + 50) / 100);
            }
        }
    }
}
}
