#include "starfox/render/sprite_renderer.hpp"

#include "starfox/simulation/game_simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace starfox::render {
namespace {

constexpr std::array<std::array<std::uint8_t, 2>, 6> kObjectSizes{{
    {{8, 16}}, {{8, 32}}, {{8, 64}}, {{16, 32}}, {{16, 64}}, {{32, 64}},
}};

struct CrosshairOamGroup {
    std::size_t first_object{};
    std::int32_t centre_x{};
    std::int32_t centre_y{};
};

std::int32_t object_x(
    const std::array<std::uint8_t, 544>& oam,
    std::size_t object) noexcept {
    const auto low = object * 4U;
    const auto shift = static_cast<unsigned>((object & 3U) * 2U);
    auto x = static_cast<std::int32_t>(oam[low])
        | (static_cast<std::int32_t>((oam[512U + object / 4U] >> shift) & 1U)
            << 8U);
    if (x >= 256) x -= 512;
    return x;
}

std::int32_t wrapped_difference(
    std::int32_t from, std::int32_t to, std::int32_t modulus) noexcept {
    auto difference = (to - from) % modulus;
    if (difference > modulus / 2) difference -= modulus;
    if (difference < -modulus / 2) difference += modulus;
    return difference;
}

std::optional<CrosshairOamGroup> find_crosshair_oam_group(
    const std::array<std::uint8_t, 544>& oam) noexcept {
    // DO_CROSSHAIR emits the four tile-$61 quadrants consecutively with the
    // flip pattern 00, H, V, HV. Depending on VOBJBASE the source tile byte is
    // either $61 or $e1, hence the low-seven-bit comparison.
    constexpr std::array<std::uint8_t, 4> flips{0U, 0x40U, 0x80U, 0xc0U};
    for (std::size_t first = 0U; first + 3U < 128U; ++first) {
        const auto first_low = first * 4U;
        const auto common_attributes = static_cast<std::uint8_t>(
            oam[first_low + 3U] & 0x3fU);
        auto matches = true;
        for (std::size_t quadrant = 0U; quadrant < 4U; ++quadrant) {
            const auto low = (first + quadrant) * 4U;
            matches = matches
                && (oam[low + 2U] & 0x7fU) == 0x61U
                && (oam[low + 3U] & 0x3fU) == common_attributes
                && (oam[low + 3U] & 0xc0U) == flips[quadrant];
        }
        if (!matches) continue;

        const auto x0 = object_x(oam, first);
        const auto x1 = object_x(oam, first + 1U);
        const auto x2 = object_x(oam, first + 2U);
        const auto x3 = object_x(oam, first + 3U);
        const auto y0 = static_cast<std::int32_t>(oam[first_low + 1U]);
        const auto y1 = static_cast<std::int32_t>(oam[first_low + 5U]);
        const auto y2 = static_cast<std::int32_t>(oam[first_low + 9U]);
        const auto y3 = static_cast<std::int32_t>(oam[first_low + 13U]);
        if (wrapped_difference(x0, x1, 512) != 16
            || wrapped_difference(x0, x2, 512) != 0
            || wrapped_difference(x1, x3, 512) != 0
            || wrapped_difference(y0, y1, 256) != 0
            || wrapped_difference(y0, y2, 256) != 16
            || wrapped_difference(y1, y3, 256) != 16) {
            continue;
        }
        return CrosshairOamGroup{first, x0 + 8, y0 + 8};
    }
    return std::nullopt;
}

void set_object_position(
    std::array<std::uint8_t, 544>& oam,
    std::size_t object,
    std::int32_t x,
    std::int32_t y) noexcept {
    auto wrapped_x = x % 512;
    if (wrapped_x < 0) wrapped_x += 512;
    auto wrapped_y = y % 256;
    if (wrapped_y < 0) wrapped_y += 256;
    const auto low = object * 4U;
    oam[low] = static_cast<std::uint8_t>(wrapped_x);
    oam[low + 1U] = static_cast<std::uint8_t>(wrapped_y);
    const auto high = 512U + object / 4U;
    const auto shift = static_cast<unsigned>((object & 3U) * 2U);
    const auto x_high_mask = static_cast<std::uint8_t>(1U << shift);
    oam[high] = static_cast<std::uint8_t>(
        (oam[high] & ~x_high_mask)
        | (((wrapped_x >> 8U) & 1U) << shift));
}

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

void interpolate_crosshair_oam(
    const std::array<std::uint8_t, 544>& previous_oam,
    double interpolation_alpha,
    simulation::SnesPpuState& current_ppu) noexcept {
    const auto previous = find_crosshair_oam_group(previous_oam);
    const auto current = find_crosshair_oam_group(current_ppu.oam);
    if (!previous || !current) return;

    const auto alpha = std::clamp(interpolation_alpha, 0.0, 1.0);
    const auto centre_x = previous->centre_x + static_cast<std::int32_t>(
        std::lround(static_cast<double>(wrapped_difference(
            previous->centre_x, current->centre_x, 512)) * alpha));
    const auto centre_y = previous->centre_y + static_cast<std::int32_t>(
        std::lround(static_cast<double>(wrapped_difference(
            previous->centre_y, current->centre_y, 256)) * alpha));
    constexpr std::array<std::int32_t, 4> x_offsets{-8, 8, -8, 8};
    constexpr std::array<std::int32_t, 4> y_offsets{-8, -8, 8, 8};
    for (std::size_t quadrant = 0U; quadrant < 4U; ++quadrant) {
        set_object_position(current_ppu.oam,
            current->first_object + quadrant,
            centre_x + x_offsets[quadrant],
            centre_y + y_offsets[quadrant]);
    }
}

void suppress_crosshair_oam(simulation::SnesPpuState& ppu) noexcept {
    const auto group = find_crosshair_oam_group(ppu.oam);
    if (!group) return;
    for (std::size_t quadrant = 0U; quadrant < 4U; ++quadrant) {
        const auto object = group->first_object + quadrant;
        const auto low = object * 4U;
        std::fill_n(ppu.oam.begin() + static_cast<std::ptrdiff_t>(low), 4U, 0U);
        const auto high = 512U + object / 4U;
        const auto shift = static_cast<unsigned>((object & 3U) * 2U);
        ppu.oam[high] = static_cast<std::uint8_t>(
            ppu.oam[high] & ~(0x03U << shift));
    }
}

void SpriteRenderer::draw_objects(
    const simulation::SnesPpuState& ppu,
    Framebuffer& target,
    std::optional<std::uint8_t> priority,
    std::int32_t horizontal_origin,
    bool extend_horizontal,
    bool anchor_edge_hud,
    const HudLayout* hud_layout,
    bool suppress_configurable_hud) const noexcept {
    if ((ppu.main_screen & 0x10U) == 0U) return;
    const auto size_selection = static_cast<std::size_t>(
        (ppu.object_select >> 5U) & 7U);
    const auto sizes = kObjectSizes[size_selection < kObjectSizes.size()
        ? size_selection : kObjectSizes.size() - 1U];

    // The source border-warning animation briefly contains both the old and
    // new row of its vertical arrow. At 20 Hz that is a one-frame motion
    // smear; on a high-rate widescreen presentation it reads as two separate
    // arrows for several displayed frames. Detect that middle phase so the
    // expanded renderer can show the new row only, while leaving native 4:3
    // output byte-for-byte faithful to the source OAM.
    auto up_new_row_pieces = std::uint8_t{};
    auto down_new_row_pieces = std::uint8_t{};
    if (anchor_edge_hud && horizontal_origin > 0) {
        for (std::size_t object = 0U; object < 128U; ++object) {
            const auto low = object * 4U;
            const auto tile = static_cast<std::uint16_t>(ppu.oam[low + 2U])
                | (static_cast<std::uint16_t>(ppu.oam[low + 3U] & 1U) << 8U);
            const auto x = ppu.oam[low];
            const auto y = ppu.oam[low + 1U];
            if ((tile & 0x7fU) != 0x3eU || (x != 119U && x != 127U)) {
                continue;
            }
            if (y == 33U) ++up_new_row_pieces;
            else if (y == 184U) ++down_new_row_pieces;
        }
    }

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
        // PLANETS hides all unused/blinking route records at (-8,-8). A stale
        // packed large-size bit otherwise wraps the lower half of that object
        // into a short green segment at the top-left corner.
        if (ppu.oam[low] == 0xf8U && ppu.oam[low + 1U] == 0xf8U) continue;
        auto x = static_cast<std::int32_t>(ppu.oam[low])
            | (static_cast<std::int32_t>(high_bits & 1U) << 8U);
        if (x >= 256) x -= 512;
        const auto y_byte = ppu.oam[low + 1U];
        const auto tile = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(ppu.oam[low + 2U])
            | (static_cast<std::uint16_t>(ppu.oam[low + 3U] & 1U) << 8U));
        // SPRITES.ASM builds the first-person reticle from four tile-$61
        // objects. Its Y position legitimately enters the top and bottom HUD
        // bands as the player aims. Classifying those quadrants as lives,
        // shield, or bombs moved each one toward a different widescreen edge
        // (and applied unrelated saved HUD offsets), fragmenting the reticle.
        const auto cockpit_crosshair_tile = (tile & 0x7fU) == 0x61U;
        const auto vertical_warning_arrow = (tile & 0x7fU) == 0x3eU
            && (x == 119 || x == 127)
            && (y_byte == 24U || y_byte == 33U
                || y_byte == 184U || y_byte == 193U);
        if (anchor_edge_hud && horizontal_origin > 0
            && vertical_warning_arrow
            && ((y_byte == 24U && up_new_row_pieces >= 2U)
                || (y_byte == 193U && down_new_row_pieces >= 2U))) {
            continue;
        }
        const auto boss_label_tile = y_byte < 32U
            && tile >= 0x71U && tile <= 0x74U;
        auto object_origin = horizontal_origin;
        if (anchor_edge_hud && horizontal_origin > 0
            && !cockpit_crosshair_tile
            && !vertical_warning_arrow
            && (y_byte < 32U || y_byte >= 168U)) {
            // Only the top/bottom gameplay OAM bands are edge HUD artwork.
            // Keep those labels, lives and bombs at the expanded edges. The
            // middle band contains the cockpit crosshair and communication
            // portraits, which must remain centred as a single composition.
            object_origin = boss_label_tile
                ? horizontal_origin * 2
                : (x < 128 ? 0 : horizontal_origin * 2);
        }
        // EX deliberately relocates the life icon/count beside the lower-left
        // shield readout. Position-only classification therefore tied both
        // groups to the Shield layout. These are the source HUD life tiles in
        // retail and EX (EX substitutes tile 230 for the final digit glyph),
        // so identify the independent group before applying band heuristics.
        const auto lives_tile = tile == 189U || tile == 226U
            || tile == 229U || tile == 230U;
        auto element = std::optional<HudElement>{};
        if (cockpit_crosshair_tile || vertical_warning_arrow) {
            // Reticles and border warnings are gameplay geometry, not a
            // configurable HUD group.
        } else if (boss_label_tile) {
            element = HudElement::boss_health;
        } else if (lives_tile || (y_byte < 32U && x < 128)) {
            element = HudElement::lives;
        } else if (y_byte >= 168U) {
            element = x < 128
                ? HudElement::shield : HudElement::bombs_boost;
        } else if (y_byte >= 128U && x < 128) {
            element = HudElement::comms;
        }
        if (suppress_configurable_hud && element) continue;
        const auto object_offset = hud_layout != nullptr && element
            ? (*hud_layout)[*element] : HudOffset{};
        const auto size = static_cast<std::uint32_t>(
            sizes[(high_bits >> 1U) & 1U]);
        const auto palette = static_cast<std::uint8_t>(
            (ppu.oam[low + 3U] >> 1U) & 7U);
        const auto object_priority = static_cast<std::uint8_t>(
            (ppu.oam[low + 3U] >> 4U) & 3U);
        if (priority && object_priority != *priority) continue;
        const auto flip_x = (ppu.oam[low + 3U] & 0x40U) != 0U;
        const auto flip_y = (ppu.oam[low + 3U] & 0x80U) != 0U;

        for (std::uint32_t destination_y = 0; destination_y < size; ++destination_y) {
            const auto source_y = flip_y ? size - 1U - destination_y : destination_y;
            const auto raw_y = static_cast<std::int32_t>(y_byte)
                + object_offset.y +
                static_cast<std::int32_t>(destination_y);
            const std::array<std::int32_t, 2> screen_ys{raw_y, raw_y - 256};
            for (const auto screen_y : screen_ys) {
                if (screen_y < 0 || screen_y >= static_cast<std::int32_t>(target.height())) {
                    continue;
                }
                for (std::uint32_t destination_x = 0; destination_x < size; ++destination_x) {
                    const auto screen_x = x + object_origin + object_offset.x
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
    bool anchor_to_edges,
    const HudLayout* hud_layout) const noexcept {
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
    const auto shield = hud_layout == nullptr
        ? HudOffset{} : (*hud_layout)[HudElement::shield];
    const auto boost = hud_layout == nullptr
        ? HudOffset{} : (*hud_layout)[HudElement::bombs_boost];
    const auto boss = hud_layout == nullptr
        ? HudOffset{} : (*hud_layout)[HudElement::boss_health];
    if (meters.extended) {
        const auto source_left_x = [anchor_to_edges](std::int32_t x) {
            return x + (anchor_to_edges ? 16 : 0);
        };
        const auto source_right_x = [anchor_to_edges, &target](std::int32_t x) {
            return anchor_to_edges
                ? x + static_cast<std::int32_t>(target.width()) - 240
                : x;
        };
        if (meters.boost_enabled) {
            const auto x = (meters.player_two_activated
                ? source_left_x(0) : source_right_x(176)) + boost.x;
            box(x, 176 + boost.y, 40, 8, 13U);
            solid(x + 2, 178 + boost.y,
                std::min<std::uint8_t>(meters.boost, 36U), 4, 6U);
        }
        if (!meters.second_player_view) {
            if (!meters.player_one_dead) {
                const auto x = source_left_x(1) + shield.x;
                // Leave two source pixels between the lower-left SHIELD
                // label and its host-rendered meter. Top-row multiplayer
                // meters retain their cartridge coordinates.
                const auto y = (meters.player_two_activated ? 7 : 182)
                    + shield.y;
                box(x, y, meters.player_health_width, 8, 13U);
                solid(x + 2, y + 2,
                    std::min(meters.damage, meters.player_health_max), 4,
                    meters.shield_up ? 7U : 2U);
            }
            if (meters.damage_two != 0U) {
                const auto x = source_right_x(115) + boost.x;
                const auto y = 7 + boost.y;
                box(x, y, meters.player_health_width, 8, 13U);
                solid(x + 2, y + 2,
                    std::min(meters.damage_two, meters.player_health_max), 4,
                    meters.shield_up_two ? 7U : 2U);
            } else if (meters.player_two_activated) {
                const auto x = source_right_x(115) + boost.x;
                const auto y = 7 + boost.y;
                box(x, y, meters.player_health_width, 8, 11U);
                solid(x + 2, y + 2, meters.player_health_max, 4, 0U);
            }
        }
    } else {
        box(left_x + shield.x, 178 + shield.y, 40, 8, 13U);
        solid(left_x + shield.x + 2, 180 + shield.y,
            std::min<std::uint8_t>(meters.damage, 36U), 4,
            meters.shield_up ? 7U : 2U);
        box(right_x + boost.x, 176 + boost.y, 40, 8, 13U);
        solid(right_x + boost.x + 2, 178 + boost.y,
            std::min<std::uint8_t>(meters.boost, 36U), 4, 6U);
    }

    auto maximum = meters.boss_max_health;
    auto current = meters.boss_health;
    if (static_cast<unsigned>(current) >= static_cast<unsigned>(maximum) + 10U) {
        current = 0U;
    }
    if (maximum == 0U) return;
    const auto half_scale = (maximum & 0x80U) != 0U;
    auto meter_width = half_scale ? maximum >> 1U : maximum;
    meter_width = static_cast<std::uint8_t>(meter_width + 4U);
    const auto boss_x = (anchor_to_edges
        ? static_cast<std::int32_t>(target.width()) - 18
            - static_cast<std::int32_t>(meter_width)
        : 222 - static_cast<std::int32_t>(meter_width)) + boss.x;
    const auto boss_y = (meters.extended && meters.player_two_activated
        ? 178 : 2) + boss.y;
    box(boss_x, boss_y, meter_width, 6, 14U);
    if (half_scale) current >>= 1U;
    solid(boss_x + 2, boss_y + 2, current, 2, 2U);
}

} // namespace starfox::render
