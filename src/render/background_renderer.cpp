#include "starfox/render/background_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace starfox::render {
namespace {

std::uint16_t vram_word(
    const simulation::SnesPpuState& ppu, std::uint32_t word_address) noexcept {
    const auto offset = (word_address & 0x7fffU) * 2U;
    return static_cast<std::uint16_t>(ppu.vram[offset])
        | (static_cast<std::uint16_t>(ppu.vram[offset + 1U]) << 8U);
}

std::uint8_t tile_pixel_4bpp(
    const simulation::SnesPpuState& ppu,
    std::uint16_t character_base,
    std::uint16_t tile,
    std::uint32_t x,
    std::uint32_t y) noexcept {
    if ((tile & 0x4000U) != 0U) x = 7U - x;
    if ((tile & 0x8000U) != 0U) y = 7U - y;
    const auto tile_number = static_cast<std::uint32_t>(tile & 0x03ffU);
    const auto base = (static_cast<std::uint32_t>(character_base) * 2U
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

struct TileSample {
    std::uint16_t tile{};
    std::uint32_t x{};
    std::uint32_t y{};
};

TileSample tile_sample(std::uint16_t tile, std::int32_t source_x,
    std::int32_t source_y, bool tile_size_16) noexcept {
    if (!tile_size_16) {
        return {tile, static_cast<std::uint32_t>(source_x) & 7U,
            static_cast<std::uint32_t>(source_y) & 7U};
    }
    auto x = static_cast<std::uint32_t>(source_x) & 15U;
    auto y = static_cast<std::uint32_t>(source_y) & 15U;
    if ((tile & 0x4000U) != 0U) x = 15U - x;
    if ((tile & 0x8000U) != 0U) y = 15U - y;
    // A 16x16 SNES character is four ordinary 8x8 characters. Horizontal
    // neighbours are consecutive; the lower pair starts 16 characters later.
    const auto character = static_cast<std::uint16_t>((tile & 0x03ffU)
        + (x >= 8U ? 1U : 0U) + (y >= 8U ? 16U : 0U));
    return {static_cast<std::uint16_t>((tile & 0x3c00U)
                | (character & 0x03ffU)),
        x & 7U, y & 7U};
}

std::int32_t mosaic_coordinate(
    std::int32_t coordinate,
    std::uint8_t mosaic,
    std::uint8_t layer_mask) noexcept {
    if ((mosaic & layer_mask) == 0U) return coordinate;
    const auto size = static_cast<std::int32_t>((mosaic >> 4U) + 1U);
    auto remainder = coordinate % size;
    if (remainder < 0) remainder += size;
    return coordinate - remainder;
}

} // namespace

std::uint8_t tunnel_border_index(const simulation::SnesPpuState& ppu,
    unsigned screen_y,unsigned screen_x) noexcept {
    const unsigned row=std::min(screen_y,223U);
    const unsigned width=(ppu.bg2_screen_size&1U)?64U:32U;
    const unsigned height=(ppu.bg2_screen_size&2U)?64U:32U;
    const unsigned edge=ppu.bg2_tile_size_16?16U:8U;
    const unsigned x=(screen_x+unsigned(ppu.bg2_horizontal_offsets_enabled
        ?ppu.bg2_horizontal_offsets[row]:ppu.bg2_scroll_x))&(width*edge-1);
    const unsigned y=(row+unsigned(ppu.bg2_scanline_scroll_enabled
        ?ppu.bg2_scanline_scroll_y[row]:ppu.bg2_scroll_y))&(height*edge-1);
    const unsigned tx=x/edge,ty=y/edge;
    const unsigned entry=((ty/32)*(width/32)+tx/32)*1024+(ty%32)*32+tx%32;
    const auto tile=vram_word(ppu,ppu.bg2_screen_base+entry);
    const auto sample=tile_sample(tile,int(x),int(y),ppu.bg2_tile_size_16);
    const auto ink=tile_pixel_4bpp(ppu,ppu.bg2_character_base,sample.tile,sample.x,sample.y);
    if(ink) return std::uint8_t(((tile>>10)&7U)*16+ink);
    unsigned darkest=std::numeric_limits<unsigned>::max();
    std::uint8_t result=0;
    for(unsigned i=0;i<256;++i) {
        const auto c=ppu.cgram[i];
        const unsigned luma=77U*(c&31U)+150U*((c>>5)&31U)+29U*((c>>10)&31U);
        if(luma<darkest) {darkest=luma;result=std::uint8_t(i);}
    }
    return result;
}

std::uint8_t tunnel_wall_index(const simulation::SnesPpuState& ppu) noexcept {
    return tunnel_border_index(ppu,112,0);
}

void BackgroundRenderer::draw_bg1(
    const simulation::SnesPpuState& ppu,
    Framebuffer& target,
    TilePriorityPass priority,
    std::int32_t horizontal_origin,
    bool extend_horizontal,
    std::uint32_t horizontal_inset,
    bool transparent_cgram_black,
    bool mosaic_staging_inset,
    bool text_outline) const noexcept {
    if ((ppu.main_screen & 0x01U) == 0U
        || (ppu.background_mode != 1U && ppu.background_mode != 2U
            && ppu.background_mode != 3U)) return;
    const auto width_tiles = (ppu.bg1_screen_size & 1U) != 0U ? 64U : 32U;
    const auto height_tiles = (ppu.bg1_screen_size & 2U) != 0U ? 64U : 32U;
    const auto pages_wide = width_tiles / 32U;
    const auto tile_edge = ppu.bg1_tile_size_16 ? 16U : 8U;
    const auto width_pixels = static_cast<std::int32_t>(width_tiles * tile_edge);
    const auto height_pixels = static_cast<std::int32_t>(height_tiles * tile_edge);
    const auto wrap = [](std::int32_t value, std::int32_t modulus) {
        value %= modulus;
        return value < 0 ? value + modulus : value;
    };
    const auto outline_sample=[&](int x,int y) {
        if(y<0 || y>=int(target.height()) || x+horizontal_origin<0
            || x+horizontal_origin>=int(target.width())) return false;
        if(!extend_horizontal) {
            const int inset=int(std::min(horizontal_inset,128U));
            const int step=mosaic_staging_inset && (ppu.mosaic&1U)?(ppu.mosaic>>4U)+1:1;
            if(x<((inset+step-1)/step)*step
                || x>=std::min(256,((256-inset+step-1)/step)*step)) return false;
        }
        x=wrap(mosaic_coordinate(x,ppu.mosaic,1)+ppu.bg1_scroll_x,width_pixels);
        y=wrap(mosaic_coordinate(y,ppu.mosaic,1)+ppu.bg1_scroll_y,height_pixels);
        const unsigned tx=unsigned(x)/tile_edge,ty=unsigned(y)/tile_edge;
        const auto tile=vram_word(ppu,ppu.bg1_screen_base
            +((tx>>5)+(ty>>5)*pages_wide)*1024+(ty&31)*32+(tx&31));
        if(!selected_priority(tile,priority)) return false;
        const auto sample=tile_sample(tile,x,y,ppu.bg1_tile_size_16);
        const auto ink=ppu.background_mode==3
            ?tile_pixel_8bpp(ppu,ppu.bg1_character_base,sample.tile,sample.x,sample.y)
            :tile_pixel_4bpp(ppu,ppu.bg1_character_base,sample.tile,sample.x,sample.y);
        const unsigned index=ppu.background_mode==3?ink:((tile>>10)&7)*16+ink;
        return ink && !(transparent_cgram_black && !(ppu.cgram[index]&32767));
    };
    for (std::uint32_t screen_y = 0; screen_y < target.height(); ++screen_y) {
        const auto sample_y = mosaic_coordinate(
            static_cast<std::int32_t>(screen_y), ppu.mosaic, 0x01U);
        const auto source_y = wrap(sample_y
            + ppu.bg1_scroll_y, height_pixels);
        const auto tile_y = static_cast<std::uint32_t>(source_y) / tile_edge;
        const auto inset = static_cast<std::int32_t>(
            std::min(horizontal_inset, 128U));
        const int step=mosaic_staging_inset && (ppu.mosaic&1U)?(ppu.mosaic>>4U)+1:1;
        const int left_inset=((inset+step-1)/step)*step;
        const int right_limit=std::min(256,((256-inset+step-1)/step)*step);
        const auto first_x = extend_horizontal ? 0U
            : static_cast<std::uint32_t>(std::max(
                horizontal_origin + left_inset, 0));
        const auto final_x = extend_horizontal ? target.width()
            : std::min(target.width(), static_cast<std::uint32_t>(
                std::max(horizontal_origin + right_limit, 0)));
        for (auto screen_x = first_x; screen_x < final_x; ++screen_x) {
            const auto logical_x = static_cast<std::int32_t>(screen_x)
                - horizontal_origin;
            if(ppu.tunnel_scene && extend_horizontal && priority==TilePriorityPass::high
                && (logical_x<0 || logical_x>=256)) continue;
            const auto sample_x = mosaic_coordinate(
                logical_x, ppu.mosaic, 0x01U);
            const auto source_x = wrap(sample_x
                + ppu.bg1_scroll_x, width_pixels);
            const auto tile_x = static_cast<std::uint32_t>(source_x) / tile_edge;
            const auto page = (tile_x >> 5U) + (tile_y >> 5U) * pages_wide;
            const auto entry = page * 0x400U
                + (tile_y & 31U) * 32U + (tile_x & 31U);
            const auto tile = vram_word(ppu,
                static_cast<std::uint32_t>(ppu.bg1_screen_base) + entry);
            const bool selected=selected_priority(tile,priority);
            if (!selected && !text_outline) continue;
            const auto sample = tile_sample(
                tile, source_x, source_y, ppu.bg1_tile_size_16);
            const auto colour = !selected?0:ppu.background_mode == 3U
                ? tile_pixel_8bpp(ppu, ppu.bg1_character_base, sample.tile,
                    sample.x, sample.y)
                : tile_pixel_4bpp(ppu, ppu.bg1_character_base, sample.tile,
                    sample.x, sample.y);
            if (colour != 0U) {
                auto indexed_colour = ppu.background_mode == 3U ? colour
                    : static_cast<std::uint8_t>(
                        ((tile >> 10U) & 7U) * 16U + colour);
                if (transparent_cgram_black
                    && (ppu.cgram[indexed_colour] & 0x7fffU) == 0U) {
                    if(text_outline && (outline_sample(logical_x-1,int(screen_y))
                        || outline_sample(logical_x+1,int(screen_y))
                        || outline_sample(logical_x,int(screen_y)-1)
                        || outline_sample(logical_x,int(screen_y)+1))) target.set(int(screen_x),int(screen_y),255);
                    continue;
                }
                if(text_outline) {
                    const auto c=ppu.cgram[indexed_colour];
                    const unsigned r=c&31,g=(c>>5)&31,b=(c>>10)&31;
                    // Sum alone treats dark brown/red/blue as bright text.
                    // Keep vivid highlights, but lift low-luminance letters.
                    if(r+g+b<=24 || 54*r+183*g+19*b<16*256) indexed_colour=254;
                }
                target.set(static_cast<std::int32_t>(screen_x),
                    static_cast<std::int32_t>(screen_y), indexed_colour);
            } else if(text_outline && (outline_sample(logical_x-1,int(screen_y))
                || outline_sample(logical_x+1,int(screen_y))
                || outline_sample(logical_x,int(screen_y)-1)
                || outline_sample(logical_x,int(screen_y)+1))) {
                target.set(int(screen_x),int(screen_y),255);
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
    bool extend_horizontal,
    bool wrap_horizontal,
    bool transparent_cgram_black,
    std::uint32_t single_occurrence_top_rows,
    std::span<const BackgroundUniqueRegion> unique_regions,
    bool ending_star_extension, bool game_over_star_extension,
    std::uint32_t sky_source_min) const noexcept {
    if ((ppu.main_screen & 0x02U) == 0U) return;
    const auto width_tiles = (ppu.bg2_screen_size & 1U) != 0U ? 64U : 32U;
    const auto height_tiles = (ppu.bg2_screen_size & 2U) != 0U ? 64U : 32U;
    const auto pages_wide = width_tiles / 32U;
    const auto tile_edge = ppu.bg2_tile_size_16 ? 16U : 8U;
    const auto width_pixels = static_cast<std::int32_t>(width_tiles * tile_edge);
    const auto height_pixels = static_cast<std::int32_t>(height_tiles * tile_edge);
    const auto wrap = [](std::int32_t value, std::int32_t modulus) {
        value %= modulus;
        return value < 0 ? value + modulus : value;
    };
    auto black_colour = std::uint8_t{};
    auto darkest = std::numeric_limits<unsigned>::max();
    for (std::size_t index = 0U; index < ppu.cgram.size(); ++index) {
        const auto colour = ppu.cgram[index];
        const auto luma = 77U * (colour & 31U)
            + 150U * ((colour >> 5U) & 31U) + 29U * ((colour >> 10U) & 31U);
        // EX palette transitions need not contain exact RGB black. Falling
        // back to index zero in that case can paint the wide margin peach.
        if (luma < darkest) {
            darkest = luma;
            black_colour = static_cast<std::uint8_t>(index);
            if (luma == 0U) break;
        }
    }
    const auto wall_colour=ppu.tunnel_scene?tunnel_wall_index(ppu):black_colour;
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
    // The six cartridge roll tables are integer-quantised samples of one
    // straight horizon. Repeating those steps beyond x=0/255 makes the added
    // columns change angle at each join, and using one edge pair makes the
    // extension warble whenever that pair quantises to a different value.
    // Recover the underlying line from every valid sample. This is used only
    // for expanded presentation; the 256-pixel cartridge raster remains exact.
    auto fitted_intercept = 0.0;
    auto fitted_slope = 0.0;
    auto fitted_samples = std::size_t{};
    auto sum_x = 0.0;
    auto sum_y = 0.0;
    auto sum_xx = 0.0;
    auto sum_xy = 0.0;
    auto previous_raw = std::int32_t{};
    auto previous_unwrapped = std::int32_t{};
    auto have_previous = false;
    for (std::size_t index = 0; index < vertical_offsets.size(); ++index) {
        if (!vertical_valid(index)) continue;
        const auto raw = vertical_value(index);
        const auto unwrapped = have_previous
            ? previous_unwrapped + signed_difference(raw, previous_raw)
            : raw;
        const auto x = static_cast<double>(index + 1U);
        const auto y = static_cast<double>(unwrapped);
        sum_x += x;
        sum_y += y;
        sum_xx += x * x;
        sum_xy += x * y;
        ++fitted_samples;
        previous_raw = raw;
        previous_unwrapped = unwrapped;
        have_previous = true;
    }
    if (fitted_samples != 0U) {
        const auto count = static_cast<double>(fitted_samples);
        const auto denominator = count * sum_xx - sum_x * sum_x;
        fitted_slope = fitted_samples > 1U && denominator != 0.0
            ? (count * sum_xy - sum_x * sum_y) / denominator : 0.0;
        fitted_intercept = (sum_y - fitted_slope * sum_x) / count;
    }
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
    constexpr auto no_column_scroll = std::numeric_limits<std::int32_t>::min();
    if (ppu.background_mode == 2U && ppu.bg2_vertical_offsets_enabled) {
        column_scroll_y.resize(final_x - first_x, no_column_scroll);
        for (auto screen_x = first_x; screen_x < final_x; ++screen_x) {
            const auto logical_x = static_cast<std::int32_t>(screen_x)
                - horizontal_origin;
            const auto column_coordinate = logical_x
                + (static_cast<std::int32_t>(scroll_x) & 7);
            if (expanded_mode2 && fitted_samples != 0U) {
                // Evaluate at pixel precision across the complete wide view.
                // That removes both the 8-pixel staircase in the extensions
                // and the derivative change where they meet the native area.
                const auto visible_column =
                    static_cast<double>(column_coordinate) / 8.0;
                auto value = static_cast<std::int32_t>(std::lround(
                    fitted_intercept + fitted_slope * visible_column));
                value %= 8'192;
                if (value < 0) value += 8'192;
                column_scroll_y[screen_x - first_x] = value;
            } else {
                const auto visible_column = column_coordinate >= 0
                    ? column_coordinate / 8
                    : -((-column_coordinate + 7) / 8);
                column_scroll_y[screen_x - first_x] = extended_vertical_offset(
                    visible_column, no_column_scroll);
            }
        }
    }
    std::vector<std::uint8_t> last_opaque_ground;
    std::vector<std::int32_t> previous_ground_source_y;
    std::vector<std::uint8_t> ground_source_wrapped;
    if (extend_ground_down) {
        // Rolled Corneria ground can live in either BG2 priority pass. Keep a
        // continuation colour for both; tracking only the low pass left the
        // final one or two wide-mode strips transparent whenever the ground
        // tile was high priority, exposing CGRAM colour zero as a blue wedge.
        last_opaque_ground.resize(final_x - first_x, 0U);
        previous_ground_source_y.resize(final_x - first_x, -1);
        ground_source_wrapped.resize(final_x - first_x, false);
    }

    // Decode each referenced 8x8 character and tilemap entry once per pass.
    // Wide Mode 2 otherwise reread four planar VRAM bytes and reconstructed
    // the same nibble for every output pixel—well over 170,000 times per
    // 32:9 frame. Animated VRAM remains exact because this cache lives only
    // for the duration of the current PPU snapshot.
    std::array<std::uint8_t, 1024U * 64U> decoded_characters;
    std::array<std::uint8_t, 1024U> decoded_character_valid{};
    const auto cached_character_pixel = [&ppu, &decoded_characters,
                                             &decoded_character_valid,
                                             character_base =
                                                 ppu.bg2_character_base](
                                            const TileSample& sample) {
        const auto character = static_cast<std::size_t>(sample.tile & 0x03ffU);
        const auto character_offset = character * 64U;
        if (decoded_character_valid[character] == 0U) {
            for (std::uint32_t y = 0U; y < 8U; ++y) {
                for (std::uint32_t x = 0U; x < 8U; ++x) {
                    decoded_characters[character_offset + y * 8U + x] =
                        tile_pixel_4bpp(ppu, character_base,
                            static_cast<std::uint16_t>(character), x, y);
                }
            }
            decoded_character_valid[character] = 1U;
        }
        auto x = sample.x;
        auto y = sample.y;
        if ((sample.tile & 0x4000U) != 0U) x = 7U - x;
        if ((sample.tile & 0x8000U) != 0U) y = 7U - y;
        return decoded_characters[character_offset + y * 8U + x];
    };
    std::array<std::uint16_t, 4096U> decoded_tilemap;
    std::array<std::uint8_t, 4096U> decoded_tilemap_valid{};
    const auto cached_tilemap_word = [&ppu, &decoded_tilemap,
                                         &decoded_tilemap_valid,
                                         screen_base = ppu.bg2_screen_base](
                                        std::uint32_t entry) {
        const auto index = static_cast<std::size_t>(entry & 0x0fffU);
        if (decoded_tilemap_valid[index] == 0U) {
            decoded_tilemap[index] = vram_word(ppu,
                static_cast<std::uint32_t>(screen_base) + entry);
            decoded_tilemap_valid[index] = 1U;
        }
        return decoded_tilemap[index];
    };
    for (std::uint32_t screen_y = 0; screen_y < target.height(); ++screen_y) {
        const auto sample_y = mosaic_coordinate(
            static_cast<std::int32_t>(screen_y), ppu.mosaic, 0x02U);
        const auto row_scroll_x = ppu.bg2_horizontal_offsets_enabled
            && sample_y >= 0
            && static_cast<std::size_t>(sample_y)
                < ppu.bg2_horizontal_offsets.size()
            ? static_cast<std::int32_t>(ppu.bg2_horizontal_offsets[
                static_cast<std::size_t>(sample_y)])
            : scroll_x;
        const auto unique_scroll_x = unique_regions.empty() ? 0
            : wrap(row_scroll_x + width_pixels / 2, width_pixels) - width_pixels / 2;
        for (auto screen_x = first_x; screen_x < final_x; ++screen_x) {
            const auto logical_x = static_cast<std::int32_t>(screen_x)
                - horizontal_origin;
            if (ppu.tunnel_scene && extend_horizontal
                && priority == TilePriorityPass::high
                && (logical_x < 0 || logical_x >= 256)) {
                // The low pass already widens the full tunnel cross-section.
                // Do not paint a second, wrapped high-priority copy over it.
                continue;
            }
            if (ppu.tunnel_scene && extend_horizontal
                && priority != TilePriorityPass::high
                && (logical_x < 0 || logical_x >= 256)) {
                // Widen one authored cross-section rather than tiling tunnels.
                // Transparent outer texels retain the closed side-wall colour.
                target.set(static_cast<std::int32_t>(screen_x),
                    static_cast<std::int32_t>(screen_y), wall_colour);
            }
            const auto sample_x = mosaic_coordinate(
                ppu.tunnel_scene && extend_horizontal && priority!=TilePriorityPass::high
                    ? std::clamp(logical_x, 0, 255) : logical_x,
                ppu.mosaic, 0x02U);
            const auto sampled_screen_x = std::clamp(
                sample_x + horizontal_origin,
                static_cast<std::int32_t>(first_x),
                static_cast<std::int32_t>(final_x - 1U));
            const auto register_scroll_y = ppu.bg2_scanline_scroll_enabled
                ? ppu.bg2_scanline_scroll_y[std::clamp(sample_y, 0, 223)]
                : scroll_y;
            const auto tile_scroll_y = column_scroll_y.empty() ? no_column_scroll
                : column_scroll_y[static_cast<std::size_t>(sampled_screen_x) - first_x];
            // A valid Mode 2 per-tile offset replaces BG2VOFS, including
            // scanline/HDMA writes. Invalid entries still use that register.
            const auto current_scroll_y = tile_scroll_y != no_column_scroll
                ? tile_scroll_y : register_scroll_y;
            auto source_y = wrap(
                sample_y + current_scroll_y,
                height_pixels);
            if(expanded_mode2 && screen_y<144U && (logical_x<0 || logical_x>=256)
                && sky_source_min<unsigned(height_pixels) && source_y<int(sky_source_min))
                source_y=int(sky_source_min);
            const auto column_index = screen_x - first_x;
            if (!previous_ground_source_y.empty()) {
                const auto previous_source_y =
                    previous_ground_source_y[column_index];
                if (screen_y >= 144U && previous_source_y >= 0
                    && source_y < previous_source_y
                    && previous_source_y - source_y > height_pixels / 2) {
                    // A rolled floor that reaches the bottom of its 256-line
                    // tilemap must continue with its last ground colour. The
                    // wrapped source row is opaque sky, so transparency-only
                    // continuation still exposed a blue wedge at the front.
                    ground_source_wrapped[column_index] = true;
                }
                previous_ground_source_y[column_index] = source_y;
                if (ground_source_wrapped[column_index]) {
                    const auto ground = last_opaque_ground[column_index];
                    if (ground != 0U) {
                        target.set(static_cast<std::int32_t>(screen_x),
                            static_cast<std::int32_t>(screen_y), ground);
                    }
                    continue;
                }
            }
            auto unwrapped_source_x = sample_x + row_scroll_x;
            if ((ending_star_extension || game_over_star_extension) && extend_horizontal
                && (logical_x < 0 || logical_x >= 256)
                && (game_over_star_extension || unwrapped_source_x < 0 || unwrapped_source_x >= width_pixels)) {
                // The upper-left 256x128 of BG_CRED contains only stars.
                // Keep the entire first atlas occurrence intact (nebulae
                // included), then extend using stable 32x32 star patches.
                // Hash world cells, not frame/time, to avoid shimmer.
                const auto cell_x = static_cast<std::uint32_t>(unwrapped_source_x) >> 5U;
                const auto cell_y = static_cast<std::uint32_t>(sample_y + current_scroll_y) >> 5U;
                auto seed = cell_x * 0x9e3779b9U ^ cell_y * 0x85ebca6bU;
                seed ^= seed >> 16U; seed *= 0x7feb352dU; seed ^= seed >> 15U;
                unwrapped_source_x = static_cast<std::int32_t>((seed & 7U) * 32U)
                    + (unwrapped_source_x & 31);
                source_y = static_cast<std::int32_t>(((seed >> 3U) & 3U) * 32U)
                    + ((sample_y + current_scroll_y) & 31);
                if(game_over_star_extension) {
                    // BG_AND combines the dense stars and Andross in one
                    // atlas. Only rows 0..31 and 160..223 are star-only.
                    const auto patch=(seed>>3U)%3U;
                    source_y=int(patch?128U+patch*32U:0U)+((sample_y+current_scroll_y)&31);
                }
            }
            const auto tile_y = static_cast<std::uint32_t>(source_y) / tile_edge;
            // Scanline scrolling also drives open water (Titania). It is not
            // evidence of a closed tunnel. Actual tunnel margins were handled
            // above via tunnel_scene; water continues its edge material below.
            // A scrolling 256-pixel title tilemap normally wraps the portion
            // that leaves one side back onto the other. In a wide viewport we
            // instead draw that one tilemap occurrence beyond the native
            // boundary. This exposes the complete EX logo without duplicating
            // the wrapped fragment—or the whole logo—across the margins.
            if (!wrap_horizontal && (unwrapped_source_x < 0
                    || unwrapped_source_x >= width_pixels)) {
                continue;
            }
            if (single_occurrence_top_rows != 0U
                && screen_y < single_occurrence_top_rows
                && (static_cast<std::int32_t>(screen_x) < horizontal_origin
                    || static_cast<std::int32_t>(screen_x)
                        >= horizontal_origin + 256)
                && (unwrapped_source_x < 0
                    || unwrapped_source_x >= width_pixels)) {
                // A few space stages combine a singular distant planet in
                // the upper tilemap with a deliberately repeatable straight
                // horizon below it. Expose the rest of the same authored map
                // occurrence in wide margins, but do not wrap a second moon
                // or planet into view. The native 256-pixel window and the
                // lower horizontal surface retain exact cartridge wrapping.
                target.set(static_cast<std::int32_t>(screen_x),
                    static_cast<std::int32_t>(screen_y), black_colour);
                continue;
            }
            auto source_x = wrap(unwrapped_source_x, width_pixels);
            if (ppu.background_mode == 1U
                && ppu.bg2_scanline_scroll_enabled && !ppu.tunnel_scene
                && extend_horizontal && (logical_x < 0 || logical_x >= 256)) {
                // Mode 1 water's BG2 contains one bridge/floor cross-section; BG3 is
                // its independently repeating mountain/sky backdrop. Expose
                // one scrolled BG2 tilemap, then continue its edge material
                // rather than wrapping a second bridge into ultrawide edges.
                // Mode 2 open water (EX 6-2) is a repeating landscape instead.
                const auto water_x = wrap(128 + row_scroll_x, width_pixels) + sample_x - 128;
                source_x = std::clamp(water_x, 0, width_pixels - 1);
            }
            const auto tile_x = static_cast<std::uint32_t>(source_x) / tile_edge;
            const auto page = (tile_x >> 5U) + (tile_y >> 5U) * pages_wide;
            const auto entry = page * 0x400U
                + (tile_y & 31U) * 32U + (tile_x & 31U);
            const auto tile = cached_tilemap_word(entry);
            if (!selected_priority(tile, ppu.tunnel_scene && extend_horizontal
                    && priority==TilePriorityPass::low ? TilePriorityPass::all : priority)) {
                if (!last_opaque_ground.empty() && screen_y >= 144U) {
                    const auto ground = last_opaque_ground[screen_x - first_x];
                    if (ground != 0U) {
                        target.set(static_cast<std::int32_t>(screen_x),
                            static_cast<std::int32_t>(screen_y), ground);
                    }
                }
                continue;
            }
            const auto sample = tile_sample(
                tile, source_x, source_y, ppu.bg2_tile_size_16);
            auto colour = cached_character_pixel(sample);
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
                auto indexed_colour = static_cast<std::uint8_t>(
                    palette * 16U + colour);
                // Scroll registers wrap; 8191 is -1, not a distant copy of
                // the map. Anchor the unique occurrence around the view.
                const auto unique_source_x = sample_x + unique_scroll_x;
                for (const auto& region : unique_regions) {
                    const bool suppress_every=region.replacement_x_offset>=0 && (region.replacement_x_offset
                        & BackgroundUniqueRegion::suppress_every_copy)!=0;
                    const bool suppress_all=region.replacement_x_offset>=0 && (region.replacement_x_offset
                        & BackgroundUniqueRegion::suppress_all_side_copies)!=0;
                    const auto replacement_offset=(suppress_all || suppress_every)
                        ?region.replacement_x_offset&~(BackgroundUniqueRegion::suppress_all_side_copies|BackgroundUniqueRegion::suppress_every_copy)
                        :region.replacement_x_offset;
                    if ((suppress_every || (extend_horizontal
                        && (logical_x < 0 || logical_x >= 256)
                        && (suppress_all || unique_source_x < 0 || unique_source_x >= width_pixels)))
                        && source_x >= region.left && source_x < region.right
                        && source_y >= region.top && source_y < region.bottom
                        && indexed_colour >= region.first_colour
                        && indexed_colour <= region.last_colour) {
                        indexed_colour = region.replacement_colour;
                        if(replacement_offset) {
                            const auto replacement_x=wrap(source_x+replacement_offset,width_pixels);
                            const auto rx=std::uint32_t(replacement_x)/tile_edge;
                            const auto replacement_entry=((rx>>5U)+(tile_y>>5U)*pages_wide)*0x400U
                                +(tile_y&31U)*32U+(rx&31U);
                            const auto replacement_tile=cached_tilemap_word(replacement_entry);
                            const auto ink=cached_character_pixel(tile_sample(replacement_tile,replacement_x,source_y,ppu.bg2_tile_size_16));
                            indexed_colour=ink?std::uint8_t(((replacement_tile>>10U)&7U)*16U+ink):region.replacement_colour;
                        }
                        break;
                    }
                }
                if (transparent_cgram_black
                    && (ppu.cgram[indexed_colour] & 0x7fffU) == 0U) {
                    continue;
                }
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
    const auto tile_edge = ppu.bg3_tile_size_16 ? 16U : 8U;
    const auto width_pixels = static_cast<std::int32_t>(width_tiles * tile_edge);
    const auto height_pixels = static_cast<std::int32_t>(height_tiles * tile_edge);
    const auto wrap = [](std::int32_t value, std::int32_t modulus) {
        value %= modulus;
        return value < 0 ? value + modulus : value;
    };

    for (std::uint32_t screen_y = 0; screen_y < target.height(); ++screen_y) {
        const auto sample_y = mosaic_coordinate(
            static_cast<std::int32_t>(screen_y), ppu.mosaic, 0x04U);
        const auto source_y = wrap(sample_y
            + ppu.bg3_scroll_y, height_pixels);
        const auto tile_y = static_cast<std::uint32_t>(source_y) / tile_edge;
        const auto first_x = extend_horizontal ? 0U
            : static_cast<std::uint32_t>(std::max(horizontal_origin, 0));
        const auto final_x = extend_horizontal ? target.width()
            : std::min(target.width(), static_cast<std::uint32_t>(
                std::max(horizontal_origin + 256, 0)));
        for (auto screen_x = first_x; screen_x < final_x; ++screen_x) {
            const auto logical_x = static_cast<std::int32_t>(screen_x)
                - horizontal_origin;
            const auto sample_x = mosaic_coordinate(
                logical_x, ppu.mosaic, 0x04U);
            const auto source_x = wrap(sample_x
                + ppu.bg3_scroll_x, width_pixels);
            const auto tile_x = static_cast<std::uint32_t>(source_x) / tile_edge;
            const auto page = (tile_x >> 5U) + (tile_y >> 5U) * pages_wide;
            const auto entry = page * 0x400U
                + (tile_y & 31U) * 32U + (tile_x & 31U);
            const auto tile = vram_word(ppu,
                static_cast<std::uint32_t>(ppu.bg3_screen_base) + entry);
            if (!selected_priority(tile, priority)) continue;
            const auto sample = tile_sample(
                tile, source_x, source_y, ppu.bg3_tile_size_16);
            const auto colour = tile_pixel_2bpp(ppu, ppu.bg3_character_base,
                sample.tile, sample.x, sample.y);
            if (colour == 0U) continue;
            const auto palette = static_cast<std::uint8_t>((tile >> 10U) & 7U);
            target.set(static_cast<std::int32_t>(screen_x),
                static_cast<std::int32_t>(screen_y),
                static_cast<std::uint8_t>(palette * 4U + colour));
        }
    }
}

void BackgroundRenderer::draw_title_foreground(
    const simulation::SnesPpuState& ppu,
    std::int32_t bg2_scroll_x,
    std::int32_t bg2_scroll_y,
    Framebuffer& target,
    std::int32_t horizontal_origin,
    bool include_bg1_overlay,
    bool extend_bg2_unwrapped) const noexcept {
    // TITLE's Mode 1 contract splits CP's BG2 tilemap around the Super FX
    // model: low-priority black/backdrop tiles stay behind it, while the
    // high-priority roster/logo tiles remain in front. Reapplying every BG2
    // tile lets the backdrop cut a black wedge into the model; omitting BG2
    // entirely lets the model cover the roster. Restore only its foreground
    // priority pass (including the retail PUSH START prompt and its black
    // outline), followed by BG1 and high-priority BG3 artwork.
    draw_bg2(ppu, bg2_scroll_x, bg2_scroll_y, target,
        TilePriorityPass::high, horizontal_origin, extend_bg2_unwrapped,
        !extend_bg2_unwrapped, false);
    if (include_bg1_overlay) {
        // Only tile colour zero is transparent. BG2's prompt outline and
        // other opaque black foreground pixels must cover the model too.
        draw_bg1(ppu, target, TilePriorityPass::all,
            horizontal_origin, false, extend_bg2_unwrapped ? 16U : 0U, false);
    }
    draw_bg3(ppu, target, TilePriorityPass::high,
        horizontal_origin, false);
}

} // namespace starfox::render
