#include "starfox/render/background_renderer.hpp"

#include <cstddef>

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
    TilePriorityPass priority) const noexcept {
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
        for (std::uint32_t screen_x = 0; screen_x < target.width(); ++screen_x) {
            const auto source_x = wrap(static_cast<std::int32_t>(screen_x)
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
    TilePriorityPass priority) const noexcept {
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

    for (std::uint32_t screen_y = 0; screen_y < target.height(); ++screen_y) {
        const auto row_scroll_x = ppu.bg2_horizontal_offsets_enabled
            && screen_y < ppu.bg2_horizontal_offsets.size()
            ? static_cast<std::int32_t>(ppu.bg2_horizontal_offsets[screen_y])
            : scroll_x;
        for (std::uint32_t screen_x = 0; screen_x < target.width(); ++screen_x) {
            auto column_scroll_y = scroll_y;
            if (ppu.background_mode == 2U && ppu.bg2_vertical_offsets_enabled) {
                const auto visible_column = (screen_x
                    + (static_cast<std::uint32_t>(scroll_x) & 7U)) >> 3U;
                if (visible_column != 0U && visible_column <= 32U) {
                    // Star Fox positions BG3 so its second offset row begins
                    // at BG2OFFSETS+32 ($2fa0). In Mode 2 bit 14 selects BG2,
                    // and a vertical entry replaces VOFS rather than adding
                    // to it.
                    const auto offset = vram_word(
                        ppu, 0x2fa0U + visible_column - 1U);
                    if ((offset & 0x4000U) != 0U) {
                        column_scroll_y = static_cast<std::int32_t>(offset & 0x1fffU);
                    }
                }
            }
            const auto source_y = wrap(
                static_cast<std::int32_t>(screen_y) + column_scroll_y,
                height_pixels);
            const auto tile_y = static_cast<std::uint32_t>(source_y) >> 3U;
            const auto source_x = wrap(
                static_cast<std::int32_t>(screen_x) + row_scroll_x, width_pixels);
            const auto tile_x = static_cast<std::uint32_t>(source_x) >> 3U;
            const auto page = (tile_x >> 5U) + (tile_y >> 5U) * pages_wide;
            const auto entry = page * 0x400U
                + (tile_y & 31U) * 32U + (tile_x & 31U);
            const auto tile = vram_word(ppu,
                static_cast<std::uint32_t>(ppu.bg2_screen_base) + entry);
            if (!selected_priority(tile, priority)) continue;
            const auto colour = tile_pixel(ppu, tile,
                static_cast<std::uint32_t>(source_x) & 7U,
                static_cast<std::uint32_t>(source_y) & 7U);
            const auto palette = static_cast<std::uint8_t>((tile >> 10U) & 7U);
            if (colour != 0U) {
                target.set(static_cast<std::int32_t>(screen_x),
                    static_cast<std::int32_t>(screen_y),
                    static_cast<std::uint8_t>(palette * 16U + colour));
            }
        }
    }
}

void BackgroundRenderer::draw_bg3(
    const simulation::SnesPpuState& ppu,
    Framebuffer& target,
    TilePriorityPass priority) const noexcept {
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
        for (std::uint32_t screen_x = 0; screen_x < target.width(); ++screen_x) {
            const auto source_x = wrap(static_cast<std::int32_t>(screen_x)
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
