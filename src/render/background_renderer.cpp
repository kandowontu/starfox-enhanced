#include "starfox/render/background_renderer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace starfox::render {
namespace {

std::uint16_t vram_word(
    const simulation::SnesPpuState& ppu, std::uint32_t word_address) noexcept {
    const auto offset = (word_address & 0x7fffU) * 2U;
    return static_cast<std::uint16_t>(ppu.vram[offset])
        | (static_cast<std::uint16_t>(ppu.vram[offset + 1U]) << 8U);
}

std::uint8_t tile_pixel(
    const simulation::SnesPpuState& ppu,
    std::uint16_t tile,
    std::uint32_t x,
    std::uint32_t y) noexcept {
    if ((tile & 0x4000U) != 0U) x = 7U - x;
    if ((tile & 0x8000U) != 0U) y = 7U - y;
    const auto tile_number = static_cast<std::uint32_t>(tile & 0x03ffU);
    const auto base = (static_cast<std::uint32_t>(ppu.bg2_character_base) * 2U
        + tile_number * 32U + y * 2U) & 0xffffU;
    const auto plane01 = static_cast<std::uint16_t>(ppu.vram[base])
        | (static_cast<std::uint16_t>(ppu.vram[(base + 1U) & 0xffffU]) << 8U);
    const auto plane23 = static_cast<std::uint16_t>(ppu.vram[(base + 16U) & 0xffffU])
        | (static_cast<std::uint16_t>(ppu.vram[(base + 17U) & 0xffffU]) << 8U);
    const auto mask = static_cast<std::uint8_t>(0x80U >> x);
    return static_cast<std::uint8_t>(
        ((plane01 & mask) != 0U ? 1U : 0U)
        | ((plane01 & (static_cast<std::uint16_t>(mask) << 8U)) != 0U ? 2U : 0U)
        | ((plane23 & mask) != 0U ? 4U : 0U)
        | ((plane23 & (static_cast<std::uint16_t>(mask) << 8U)) != 0U ? 8U : 0U));
}

std::uint8_t tile_pixel_2bpp(
    const simulation::SnesPpuState& ppu,
    std::uint16_t character_base,
    std::uint16_t tile,
    std::uint32_t x,
    std::uint32_t y) noexcept {
    if ((tile & 0x4000U) != 0U) x = 7U - x;
    if ((tile & 0x8000U) != 0U) y = 7U - y;
    const auto tile_number = static_cast<std::uint32_t>(tile & 0x03ffU);
    const auto base = (static_cast<std::uint32_t>(character_base) * 2U
        + tile_number * 16U + y * 2U) & 0xffffU;
    const auto planes = static_cast<std::uint16_t>(ppu.vram[base])
        | (static_cast<std::uint16_t>(ppu.vram[(base + 1U) & 0xffffU]) << 8U);
    const auto mask = static_cast<std::uint8_t>(0x80U >> x);
    return static_cast<std::uint8_t>(
        ((planes & mask) != 0U ? 1U : 0U)
        | ((planes & (static_cast<std::uint16_t>(mask) << 8U)) != 0U ? 2U : 0U));
}

std::uint8_t tile_pixel_8bpp(
    const simulation::SnesPpuState& ppu,
    std::uint16_t character_base,
    std::uint16_t tile,
    std::uint32_t x,
    std::uint32_t y) noexcept {
    if ((tile & 0x4000U) != 0U) x = 7U - x;
    if ((tile & 0x8000U) != 0U) y = 7U - y;
    const auto tile_number = static_cast<std::uint32_t>(tile & 0x03ffU);
    const auto base = (static_cast<std::uint32_t>(character_base) * 2U
        + tile_number * 64U + y * 2U) & 0xffffU;
    const auto mask = static_cast<std::uint8_t>(0x80U >> x);
    std::uint8_t colour{};
    for (std::uint32_t pair = 0; pair < 4U; ++pair) {
        const auto pair_base = (base + pair * 16U) & 0xffffU;
        if ((ppu.vram[pair_base] & mask) != 0U) {
            colour = static_cast<std::uint8_t>(colour | (1U << (pair * 2U)));
        }
        if ((ppu.vram[(pair_base + 1U) & 0xffffU] & mask) != 0U) {
            colour = static_cast<std::uint8_t>(colour | (2U << (pair * 2U)));
        }
    }
    return colour;
}

bool selected_priority(std::uint16_t tile, TilePriorityPass pass) noexcept {
    if (pass == TilePriorityPass::all) return true;
    const auto high = (tile & 0x2000U) != 0U;
    return high == (pass == TilePriorityPass::high);
}

} // namespace

void BackgroundRenderer::draw_bg1(
    const simulation::SnesPpuState& ppu,
    Framebuffer& target,
    TilePriorityPass priority,
    std::int32_t horizontal_origin,
    bool extend_horizontal) const noexcept {
    if ((ppu.main_screen & 0x01U) == 0U || ppu.background_mode != 3U) return;
    const auto width_tiles = (ppu.bg1_screen_size & 1U) != 0U ? 64U : 32U;
    const auto height_tiles = (ppu.bg1_screen_size & 2U) != 0U ? 64U : 32U;
    const auto pages_wide = width_tiles / 32U;
    const auto width_pixels = static_cast<std::int32_t>(width_tiles * 8U);
    const auto height_pixels = static_cast<std::int32_t>(height_tiles * 8U);
    const auto wrap = [](std::int32_t value, std::int32_t modulus) {
        value %= modulus;
        return value < 0 ? value + modulus : value;
    };
    for (std::uint32_t screen_y = 0; screen_y < target.height(); ++screen_y) {
        const auto source_y = wrap(static_cast<std::int32_t>(screen_y)
            + ppu.bg1_scroll_y, height_pixels);
        const auto tile_y = static_cast<std::uint32_t>(source_y) >> 3U;
        const auto first_x = extend_horizontal ? 0U
            : static_cast<std::uint32_t>(std::max(horizontal_origin, 0));
        const auto final_x = extend_horizontal ? target.width()
            : std::min(target.width(), static_cast<std::uint32_t>(
                std::max(horizontal_origin + 256, 0)));
        for (auto screen_x = first_x; screen_x < final_x; ++screen_x) {
            const auto logical_x = static_cast<std::int32_t>(screen_x)
                - horizontal_origin;
            const auto source_x = wrap(logical_x
                + ppu.bg1_scroll_x, width_pixels);
            const auto tile_x = static_cast<std::uint32_t>(source_x) >> 3U;
            const auto page = (tile_x >> 5U) + (tile_y >> 5U) * pages_wide;
            const auto entry = page * 0x400U
                + (tile_y & 31U) * 32U + (tile_x & 31U);
            const auto tile = vram_word(ppu,
                static_cast<std::uint32_t>(ppu.bg1_screen_base) + entry);
            if (!selected_priority(tile, priority)) continue;
            const auto colour = tile_pixel_8bpp(ppu, ppu.bg1_character_base, tile,
                static_cast<std::uint32_t>(source_x) & 7U,
                static_cast<std::uint32_t>(source_y) & 7U);
            if (colour != 0U) {
                target.set(static_cast<std::int32_t>(screen_x),
                    static_cast<std::int32_t>(screen_y), colour);
            }
        }
    }
}

void BackgroundRenderer::draw_bg2(
    const simulation::SnesPpuState& ppu,
    std::int32_t scroll_x,
    std::int32_t scroll_y,
    Framebuffer& target,
    TilePriorityPass priority,
    std::int32_t horizontal_origin,
    bool extend_horizontal) const noexcept {
    if ((ppu.main_screen & 0x02U) == 0U) return;
    const auto width_tiles = (ppu.bg2_screen_size & 1U) != 0U ? 64U : 32U;
    const auto height_tiles = (ppu.bg2_screen_size & 2U) != 0U ? 64U : 32U;
    const auto pages_wide = width_tiles / 32U;
    const auto width_pixels = static_cast<std::int32_t>(width_tiles * 8U);
    const auto height_pixels = static_cast<std::int32_t>(height_tiles * 8U);
    const auto wrap = [](std::int32_t value, std::int32_t modulus) {
        value %= modulus;
        return value < 0 ? value + modulus : value;
    };
    std::array<std::uint16_t, 32> vertical_offsets{};
    if (ppu.background_mode == 2U && ppu.bg2_vertical_offsets_enabled) {
        for (std::size_t index = 0; index < vertical_offsets.size(); ++index) {
            vertical_offsets[index] = vram_word(
                ppu, 0x2fa0U + static_cast<std::uint32_t>(index));
        }
    }
    const auto vertical_value = [&vertical_offsets](std::size_t index) {
        return static_cast<std::int32_t>(vertical_offsets[index] & 0x1fffU);
    };
    const auto vertical_valid = [&vertical_offsets](std::size_t index) {
        return (vertical_offsets[index] & 0x4000U) != 0U;
    };
    const auto signed_difference = [](std::int32_t to, std::int32_t from) {
        auto difference = (to - from) & 0x1fff;
        if (difference > 4'095) difference -= 8'192;
        return difference;
    };
    auto first_valid = vertical_offsets.size();
    auto last_valid = vertical_offsets.size();
    for (std::size_t index = 0; index < vertical_offsets.size(); ++index) {
        if (!vertical_valid(index)) continue;
        if (first_valid == vertical_offsets.size()) first_valid = index;
        last_valid = index;
    }
    const auto extrapolated_delta = first_valid != vertical_offsets.size()
            && last_valid != first_valid
        ? signed_difference(vertical_value(last_valid), vertical_value(first_valid))
        : 0;
    const auto extrapolated_span = first_valid != vertical_offsets.size()
            && last_valid != first_valid
        ? static_cast<std::int32_t>(last_valid - first_valid)
        : 1;
    const auto expanded_mode2 = extend_horizontal && target.width() > 256U
        && ppu.background_mode == 2U && ppu.bg2_vertical_offsets_enabled;
    const auto extended_vertical_offset = [&vertical_value, &vertical_valid,
                                               extrapolated_delta,
                                               extrapolated_span,
                                               expanded_mode2](
                                              std::int32_t visible_column,
                                              std::int32_t fallback) {
        if (visible_column >= 1 && visible_column <= 32) {
            const auto index = static_cast<std::size_t>(visible_column - 1);
            return vertical_valid(index) ? vertical_value(index) : fallback;
        }
        const auto wrap_offset = [](std::int32_t offset) {
            offset %= 8'192;
            return offset < 0 ? offset + 8'192 : offset;
        };
        const auto extend_slope = [extrapolated_delta, extrapolated_span](
                                      std::int32_t anchor,
                                      std::int32_t distance) {
            // The cartridge's six roll tables are deliberately quantised
            // staircases. An edge pair can therefore be equal even though the
            // table as a whole still slopes. Continue the full-table gradient
            // instead of magnifying one quantisation step (or freezing it)
            // across an ultrawide margin.
            return anchor + extrapolated_delta * distance / extrapolated_span;
        };
        if (visible_column <= 0 && vertical_valid(0)) {
            // Retail Mode 2's left guard and its first offset column share an
            // entry. Repeating that guard into the added margin produces a
            // conspicuous flat tile and then a bend. Expanded presentation
            // instead treats the first table entry as virtual column one and
            // continues through column zero without duplicating its phase.
            // Preserve the cartridge-width guard exactly in 4:3.
            const auto distance = expanded_mode2
                ? visible_column - 1 : std::min(visible_column + 1, 0);
            return wrap_offset(extend_slope(vertical_value(0), distance));
        }
        if (visible_column > 32 && vertical_valid(31)) {
            return wrap_offset(extend_slope(
                vertical_value(31), visible_column - 32));
        }
        return fallback;
    };

    const auto extend_ground_down = expanded_mode2 && target.height() > 192U;
    const auto first_x = extend_horizontal ? 0U
        : static_cast<std::uint32_t>(std::max(horizontal_origin, 0));
    const auto final_x = extend_horizontal ? target.width()
        : std::min(target.width(), static_cast<std::uint32_t>(
            std::max(horizontal_origin + 256, 0)));
    std::vector<std::int32_t> column_scroll_y;
    if (ppu.background_mode == 2U && ppu.bg2_vertical_offsets_enabled) {
        column_scroll_y.resize(final_x - first_x, scroll_y);
        for (auto screen_x = first_x; screen_x < final_x; ++screen_x) {
            const auto logical_x = static_cast<std::int32_t>(screen_x)
                - horizontal_origin;
            const auto column_coordinate = logical_x
                + (static_cast<std::int32_t>(scroll_x) & 7);
            const auto visible_column = column_coordinate >= 0
                ? column_coordinate / 8
                : -((-column_coordinate + 7) / 8);
            column_scroll_y[screen_x - first_x] = extended_vertical_offset(
                visible_column, scroll_y);
        }
    }
    std::vector<std::uint8_t> last_opaque_ground;
    if (extend_ground_down) {
        // Rolled Corneria ground can live in either BG2 priority pass. Keep a
        // continuation colour for both; tracking only the low pass left the
        // final one or two wide-mode strips transparent whenever the ground
        // tile was high priority, exposing CGRAM colour zero as a blue wedge.
        last_opaque_ground.resize(final_x - first_x, 0U);
    }

    for (std::uint32_t screen_y = 0; screen_y < target.height(); ++screen_y) {
        const auto row_scroll_x = ppu.bg2_horizontal_offsets_enabled
            && screen_y < ppu.bg2_horizontal_offsets.size()
            ? static_cast<std::int32_t>(ppu.bg2_horizontal_offsets[screen_y])
            : scroll_x;
        for (auto screen_x = first_x; screen_x < final_x; ++screen_x) {
            const auto logical_x = static_cast<std::int32_t>(screen_x)
                - horizontal_origin;
            const auto current_scroll_y = column_scroll_y.empty() ? scroll_y
                : column_scroll_y[screen_x - first_x];
            const auto source_y = wrap(
                static_cast<std::int32_t>(screen_y) + current_scroll_y,
                height_pixels);
            const auto tile_y = static_cast<std::uint32_t>(source_y) >> 3U;
            const auto source_x = wrap(
                logical_x + row_scroll_x, width_pixels);
            const auto tile_x = static_cast<std::uint32_t>(source_x) >> 3U;
            const auto page = (tile_x >> 5U) + (tile_y >> 5U) * pages_wide;
            const auto entry = page * 0x400U
                + (tile_y & 31U) * 32U + (tile_x & 31U);
            const auto tile = vram_word(ppu,
                static_cast<std::uint32_t>(ppu.bg2_screen_base) + entry);
            if (!selected_priority(tile, priority)) {
                if (!last_opaque_ground.empty() && screen_y >= 144U) {
                    const auto ground = last_opaque_ground[screen_x - first_x];
                    if (ground != 0U) {
                        target.set(static_cast<std::int32_t>(screen_x),
                            static_cast<std::int32_t>(screen_y), ground);
                    }
                }
                continue;
            }
            auto colour = tile_pixel(ppu, tile,
                static_cast<std::uint32_t>(source_x) & 7U,
                static_cast<std::uint32_t>(source_y) & 7U);
            auto palette = static_cast<std::uint8_t>((tile >> 10U) & 7U);
            if (colour == 0U && !last_opaque_ground.empty()
                && screen_y >= 144U) {
                const auto ground = last_opaque_ground[screen_x - first_x];
                if (ground != 0U) {
                    target.set(static_cast<std::int32_t>(screen_x),
                        static_cast<std::int32_t>(screen_y), ground);
                }
                continue;
            }
            if (colour != 0U) {
                const auto indexed_colour = static_cast<std::uint8_t>(
                    palette * 16U + colour);
                if (!last_opaque_ground.empty()) {
                    last_opaque_ground[screen_x - first_x] = indexed_colour;
                }
                target.set(static_cast<std::int32_t>(screen_x),
                    static_cast<std::int32_t>(screen_y), indexed_colour);
            }
        }
    }
}

void BackgroundRenderer::draw_bg3(
    const simulation::SnesPpuState& ppu,
    Framebuffer& target,
    TilePriorityPass priority,
    std::int32_t horizontal_origin,
    bool extend_horizontal) const noexcept {
    if ((ppu.main_screen & 0x04U) == 0U) return;
    const auto width_tiles = (ppu.bg3_screen_size & 1U) != 0U ? 64U : 32U;
    const auto height_tiles = (ppu.bg3_screen_size & 2U) != 0U ? 64U : 32U;
    const auto pages_wide = width_tiles / 32U;
    const auto width_pixels = static_cast<std::int32_t>(width_tiles * 8U);
    const auto height_pixels = static_cast<std::int32_t>(height_tiles * 8U);
    const auto wrap = [](std::int32_t value, std::int32_t modulus) {
        value %= modulus;
        return value < 0 ? value + modulus : value;
    };

    for (std::uint32_t screen_y = 0; screen_y < target.height(); ++screen_y) {
        const auto source_y = wrap(static_cast<std::int32_t>(screen_y)
            + ppu.bg3_scroll_y, height_pixels);
        const auto tile_y = static_cast<std::uint32_t>(source_y) >> 3U;
        const auto first_x = extend_horizontal ? 0U
            : static_cast<std::uint32_t>(std::max(horizontal_origin, 0));
        const auto final_x = extend_horizontal ? target.width()
            : std::min(target.width(), static_cast<std::uint32_t>(
                std::max(horizontal_origin + 256, 0)));
        for (auto screen_x = first_x; screen_x < final_x; ++screen_x) {
            const auto logical_x = static_cast<std::int32_t>(screen_x)
                - horizontal_origin;
            const auto source_x = wrap(logical_x
                + ppu.bg3_scroll_x, width_pixels);
            const auto tile_x = static_cast<std::uint32_t>(source_x) >> 3U;
            const auto page = (tile_x >> 5U) + (tile_y >> 5U) * pages_wide;
            const auto entry = page * 0x400U
                + (tile_y & 31U) * 32U + (tile_x & 31U);
            const auto tile = vram_word(ppu,
                static_cast<std::uint32_t>(ppu.bg3_screen_base) + entry);
            if (!selected_priority(tile, priority)) continue;
            const auto colour = tile_pixel_2bpp(ppu, ppu.bg3_character_base, tile,
                static_cast<std::uint32_t>(source_x) & 7U,
                static_cast<std::uint32_t>(source_y) & 7U);
            if (colour == 0U) continue;
            const auto palette = static_cast<std::uint8_t>((tile >> 10U) & 7U);
            target.set(static_cast<std::int32_t>(screen_x),
                static_cast<std::int32_t>(screen_y),
                static_cast<std::uint8_t>(palette * 4U + colour));
        }
    }
}

} // namespace starfox::render
