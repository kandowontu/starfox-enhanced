#include "starfox/render/sprite_renderer.hpp"

#include "starfox/simulation/game_simulation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace starfox::render {
namespace {

constexpr std::array<std::array<std::uint8_t, 2>, 6> kObjectSizes{{
    {{8, 16}}, {{8, 32}}, {{8, 64}}, {{16, 32}}, {{16, 64}}, {{32, 64}},
}};

std::uint8_t object_pixel(
    const simulation::SnesPpuState& ppu,
    std::uint16_t tile_number,
    std::uint32_t x,
    std::uint32_t y) noexcept {
    const auto table_base = static_cast<std::uint32_t>(ppu.object_select & 7U) * 0x2000U;
    const auto name_gap = static_cast<std::uint32_t>(
        ((ppu.object_select >> 3U) & 3U) + 1U) * 0x1000U;
    auto tile_base = table_base;
    if ((tile_number & 0x100U) != 0U) tile_base += name_gap;
    const auto tile = static_cast<std::uint32_t>(tile_number & 0xffU)
        + (y >> 3U) * 16U + (x >> 3U);
    const auto address = (tile_base * 2U + tile * 32U
        + (y & 7U) * 2U) & 0xffffU;
    const auto mask = static_cast<std::uint8_t>(0x80U >> (x & 7U));
    const auto plane01 = static_cast<std::uint16_t>(ppu.vram[address])
        | (static_cast<std::uint16_t>(ppu.vram[(address + 1U) & 0xffffU]) << 8U);
    const auto plane23 = static_cast<std::uint16_t>(ppu.vram[(address + 16U) & 0xffffU])
        | (static_cast<std::uint16_t>(ppu.vram[(address + 17U) & 0xffffU]) << 8U);
    return static_cast<std::uint8_t>(
        ((plane01 & mask) != 0U ? 1U : 0U)
        | ((plane01 & (static_cast<std::uint16_t>(mask) << 8U)) != 0U ? 2U : 0U)
        | ((plane23 & mask) != 0U ? 4U : 0U)
        | ((plane23 & (static_cast<std::uint16_t>(mask) << 8U)) != 0U ? 8U : 0U));
}

} // namespace

void SpriteRenderer::draw_objects(
    const simulation::SnesPpuState& ppu,
    Framebuffer& target,
    std::optional<std::uint8_t> priority,
    std::int32_t horizontal_origin,
    bool extend_horizontal,
    bool anchor_edge_hud) const noexcept {
    if ((ppu.main_screen & 0x10U) == 0U) return;
    const auto size_selection = static_cast<std::size_t>(
        (ppu.object_select >> 5U) & 7U);
    const auto sizes = kObjectSizes[size_selection < kObjectSizes.size()
        ? size_selection : kObjectSizes.size() - 1U];

    // Lower OAM indices win sprite-to-sprite priority, so paint in reverse.
    for (std::size_t object = 128U; object-- > 0U;) {
        const auto low = object * 4U;
        const auto high = 512U + object / 4U;
        const auto high_bits = static_cast<std::uint8_t>(
            ppu.oam[high] >> ((object & 3U) * 2U));
        // SPRITES.ASM clears unused low-OAM records with STZ but does not
        // always rewrite their packed high-table size bit. Treat the zeroed
        // four-byte record as empty regardless of that stale bit. Otherwise
        // a large tile-zero route dot wraps into a blinking strip at (0, 0).
        if (ppu.oam[low] == 0U
            && ppu.oam[low + 1U] == 0U
            && ppu.oam[low + 2U] == 0U
            && ppu.oam[low + 3U] == 0U) continue;
        auto x = static_cast<std::int32_t>(ppu.oam[low])
            | (static_cast<std::int32_t>(high_bits & 1U) << 8U);
        if (x >= 256) x -= 512;
        const auto y_byte = ppu.oam[low + 1U];
        auto object_origin = horizontal_origin;
        if (anchor_edge_hud && horizontal_origin > 0
            && (y_byte < 32U || y_byte >= 168U)) {
            // Only the top/bottom gameplay OAM bands are edge HUD artwork.
            // Keep those labels, lives and bombs at the expanded edges. The
            // middle band contains the cockpit crosshair and communication
            // portraits, which must remain centred as a single composition.
            object_origin = x < 128 ? 0 : horizontal_origin * 2;
        }
        const auto size = static_cast<std::uint32_t>(
            sizes[(high_bits >> 1U) & 1U]);
        const auto tile = static_cast<std::uint16_t>(ppu.oam[low + 2U])
            | (static_cast<std::uint16_t>(ppu.oam[low + 3U] & 1U) << 8U);
        const auto palette = static_cast<std::uint8_t>(
            (ppu.oam[low + 3U] >> 1U) & 7U);
        const auto object_priority = static_cast<std::uint8_t>(
            (ppu.oam[low + 3U] >> 4U) & 3U);
        if (priority && object_priority != *priority) continue;
        const auto flip_x = (ppu.oam[low + 3U] & 0x40U) != 0U;
        const auto flip_y = (ppu.oam[low + 3U] & 0x80U) != 0U;

        for (std::uint32_t destination_y = 0; destination_y < size; ++destination_y) {
            const auto source_y = flip_y ? size - 1U - destination_y : destination_y;
            const auto raw_y = static_cast<std::int32_t>(y_byte) +
                static_cast<std::int32_t>(destination_y);
            const std::array<std::int32_t, 2> screen_ys{raw_y, raw_y - 256};
            for (const auto screen_y : screen_ys) {
                if (screen_y < 0 || screen_y >= static_cast<std::int32_t>(target.height())) {
                    continue;
                }
                for (std::uint32_t destination_x = 0; destination_x < size; ++destination_x) {
                    const auto screen_x = x + object_origin
                        + static_cast<std::int32_t>(destination_x);
                    if (!extend_horizontal
                        && (screen_x < horizontal_origin
                            || screen_x >= horizontal_origin + 256)) continue;
                    if (screen_x < 0
                        || screen_x >= static_cast<std::int32_t>(target.width())) continue;
                    const auto source_x = flip_x ? size - 1U - destination_x : destination_x;
                    const auto pixel = object_pixel(ppu, tile, source_x, source_y);
                    if (pixel == 0U) continue;
                    target.set(screen_x, screen_y, static_cast<std::uint8_t>(
                        128U + palette * 16U + pixel));
                }
            }
        }
    }
}

void SpriteRenderer::draw_meters(
    const simulation::MeterState& meters,
    Framebuffer& target,
    bool anchor_to_edges) const noexcept {
    if (!meters.enabled) return;
    const auto solid = [&target](
        std::int32_t x,
        std::int32_t y,
        std::int32_t width,
        std::int32_t height,
        std::uint8_t colour) {
        for (std::int32_t row = 0; row < height; ++row) {
            for (std::int32_t column = 0; column < width; ++column) {
                target.set(x + column, y + row,
                    static_cast<std::uint8_t>(7U * 16U + colour));
            }
        }
    };
    const auto box = [&solid](std::int32_t x, std::int32_t y,
                              std::int32_t width, std::int32_t height,
                              std::uint8_t colour) {
        if (width <= 0 || height <= 0) return;
        solid(x, y, width, 1, colour);
        solid(x, y + height - 1, width, 1, colour);
        if (height > 2) {
            solid(x, y + 1, 1, height - 2, colour);
            solid(x + width - 1, y + 1, 1, height - 2, colour);
        }
    };
    const auto left_x = anchor_to_edges ? 24 : 8;
    const auto right_x = anchor_to_edges
        ? static_cast<std::int32_t>(target.width()) - 64 : 176;
    box(left_x, 176, 40, 8, 13U);
    solid(left_x + 2, 178, std::min<std::uint8_t>(meters.damage, 36U), 4,
        meters.shield_up ? 7U : 2U);
    box(right_x, 176, 40, 8, 13U);
    solid(right_x + 2, 178, std::min<std::uint8_t>(meters.boost, 36U), 4, 6U);

    auto maximum = meters.boss_max_health;
    auto current = meters.boss_health;
    if (static_cast<unsigned>(current) >= static_cast<unsigned>(maximum) + 10U) {
        current = 0U;
    }
    if (maximum == 0U) return;
    const auto half_scale = (maximum & 0x80U) != 0U;
    auto meter_width = half_scale ? maximum >> 1U : maximum;
    meter_width = static_cast<std::uint8_t>(meter_width + 4U);
    const auto boss_x = anchor_to_edges
        ? static_cast<std::int32_t>(target.width()) - 18
            - static_cast<std::int32_t>(meter_width)
        : 222 - static_cast<std::int32_t>(meter_width);
    box(boss_x, 2, meter_width, 6, 14U);
    if (half_scale) current >>= 1U;
    solid(boss_x + 2, 4, current, 2, 2U);
}

} // namespace starfox::render
