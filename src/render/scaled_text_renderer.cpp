#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/localization/bitmap_font.hpp"
#include "starfox/localization/menu_catalog.hpp"
#include "starfox/localization/dialogue_catalog.hpp"
#include "starfox/localization/ex_dialogue_catalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace starfox::render {
namespace {
struct MenuLatinGlyph { char32_t base; char32_t accent; };
MenuLatinGlyph menu_latin_glyph(char32_t code) {
    constexpr std::u32string_view accented = U"\u00c0\u00c1\u00c2\u00c3\u00c4\u00c5\u00c7\u00c8\u00c9\u00ca\u00cb\u00cc\u00cd\u00ce\u00cf\u00d1\u00d2\u00d3\u00d4\u00d5\u00d6\u00d9\u00da\u00db\u00dc\u00dd\u00e0\u00e1\u00e2\u00e3\u00e4\u00e5\u00e7\u00e8\u00e9\u00ea\u00eb\u00ec\u00ed\u00ee\u00ef\u00f1\u00f2\u00f3\u00f4\u00f5\u00f6\u00f9\u00fa\u00fb\u00fc\u00fd\u00ff";
    constexpr std::u32string_view bases = U"AAAAAACEEEEIIIINOOOOOUUUUYaaaaaaceeeeiiiinooooouuuuyy";
    constexpr std::u32string_view accents = U"\u0300\u0301\u0302\u0303\u0308\u030a\u0327\u0300\u0301\u0302\u0308\u0300\u0301\u0302\u0308\u0303\u0300\u0301\u0302\u0303\u0308\u0300\u0301\u0302\u0308\u0301\u0300\u0301\u0302\u0303\u0308\u030a\u0327\u0300\u0301\u0302\u0308\u0300\u0301\u0302\u0308\u0303\u0300\u0301\u0302\u0303\u0308\u0300\u0301\u0302\u0308\u0301\u0308";
    static_assert(accented.size() == bases.size() && bases.size() == accents.size());
    const auto found = accented.find(code);
    return found == std::u32string_view::npos ? MenuLatinGlyph{code, 0}
        : MenuLatinGlyph{bases[found], accents[found]};
}
}

std::int32_t ScaledTextRenderer::menu_glyph_advance(char32_t code) const {
    const auto latin = menu_latin_glyph(code);
    if (latin.base >= 32 && latin.base < 127) {
        if (latin.base == U' ' || latin.base == U'/') return 5;
        const auto index = rom_->read8(game_font_translation_ + latin.base - 32U);
        return rom_->read8(game_font_widths_ + index);
    }
    const auto* glyph = localization::glyph(code);
    if (!glyph) glyph = localization::glyph(U'?');
    return glyph ? (glyph->advance * 3 + 1) / 2 + 1 : 0;
}

std::int32_t ScaledTextRenderer::measure_menu_unicode(std::u32string_view text) const {
    std::int32_t width = 0, widest = 0;
    for (auto code : text) {
        if (code == U'\n') { widest = std::max(widest, width); width = 0; }
        else width += menu_glyph_advance(code);
    }
    return std::max(widest, width);
}

void ScaledTextRenderer::draw_unicode(std::u32string_view text,
    std::int32_t x, std::int32_t y, Framebuffer& target,
    std::uint8_t colour, std::uint8_t colour_index_base, bool menu_size) const {
    const auto start_x = x;
    for (const auto code : text) {
        if (code == U'\n') { x = start_x; y += menu_size ? 15 : 10; continue; }
        const auto latin = menu_latin_glyph(code);
        if (menu_size && latin.base >= 32 && latin.base < 127) {
            // Use the same full-height cartridge font as English, rather than
            // magnifying Misaki's three-pixel-wide Latin letters.
            const auto advance = menu_glyph_advance(code);
            const auto ink = static_cast<std::uint8_t>(colour_index_base + colour);
            if (latin.base == U'/') {
                for (int row = 0; row < 12; ++row) target.set(x + 3 - row / 3, y + row, ink);
            } else if (latin.base != U' ') {
                const auto index = rom_->read8(game_font_translation_ + latin.base - 32U);
                for (int row = 0; row < 12; ++row) {
                    const auto bits = rom_->read16(game_font_glyphs_ + index * 24U + row * 2U);
                    for (int column = 0; column < advance; ++column)
                        if (bits & (0x8000U >> column)) target.set(x + column, y + row, ink);
                }
                const auto centre = x + std::max(1, (advance - 1) / 2);
                switch (latin.accent) {
                case U'\u0301': target.set(centre + 1,y - 2,ink); target.set(centre,y - 1,ink); break;
                case U'\u0300': target.set(centre - 1,y - 2,ink); target.set(centre,y - 1,ink); break;
                case U'\u0302': target.set(centre,y - 2,ink); target.set(centre - 1,y - 1,ink); target.set(centre + 1,y - 1,ink); break;
                case U'\u0308': target.set(centre - 1,y - 1,ink); target.set(centre + 1,y - 1,ink); break;
                case U'\u0303': target.set(centre - 1,y - 2,ink); target.set(centre,y - 2,ink); target.set(centre,y - 1,ink); target.set(centre + 1,y - 1,ink); break;
                case U'\u030a': target.set(centre,y - 3,ink); target.set(centre - 1,y - 2,ink); target.set(centre + 1,y - 2,ink); target.set(centre,y - 1,ink); break;
                case U'\u0327': target.set(centre,y + 12,ink); target.set(centre - 1,y + 13,ink); break;
                default: break;
                }
            }
            x += advance;
            continue;
        }
        const auto* glyph = localization::glyph(code);
        if (glyph == nullptr) glyph = localization::glyph(U'?');
        if (glyph == nullptr) continue;
        const auto edge = menu_size ? 12 : 8;
        for (int row = 0; row < edge; ++row)
            for (int column = 0; column < edge; ++column)
                if ((glyph->rows[row * 8 / edge] & (0x80U >> (column * 8 / edge))) != 0U)
                    target.set(x + column, y + row,
                        static_cast<std::uint8_t>(colour_index_base + colour));
        x += menu_size ? menu_glyph_advance(code) : glyph->advance;
    }
}

std::int32_t ScaledTextRenderer::measure_unicode(std::u32string_view text) const {
    std::int32_t width = 0, widest = 0;
    for (const auto code : text) {
        if (code == U'\n') { widest = std::max(widest, width); width = 0; continue; }
        const auto* glyph = localization::glyph(code);
        if (glyph == nullptr) glyph = localization::glyph(U'?');
        if (glyph != nullptr) width += glyph->advance;
    }
    return std::max(widest, width);
}

namespace {

std::uint32_t rom_symbol(
    const assets::SymbolMap& symbols,
    const std::string& name) {
    for (const auto address : symbols.find(name)) {
        if ((address & 0xffffU) >= 0x8000U
            && ((address >> 16U) & 0xffU) < 0x70U) {
            return address;
        }
    }
    throw std::runtime_error{"missing scaled-text ROM symbol: " + name};
}

} // namespace

ScaledTextRenderer::ScaledTextRenderer(
    const assets::RomImage& rom,
    const assets::SymbolMap& symbols)
    : rom_(&rom),
      font_(rom_symbol(symbols, "MSCALECHARS")),
      messages_(rom_symbol(symbols, "MARIOMSGS")),
      game_font_widths_(rom_symbol(symbols, "FONT0WID")),
      game_font_glyphs_(rom_symbol(symbols, "FONT0FON")),
      game_font_translation_(rom_symbol(symbols, "FONT0TRN")),
      face_data_(rom_symbol(symbols, "FACEDATA")) {
    for (const auto address : symbols.find("FACEDATA2")) {
        if ((address & 0xffffU) >= 0x8000U
            && ((address >> 16U) & 0xffU) < 0x70U) {
            face_data_2_ = address;
            break;
        }
    }
    // EX uses different message tables; do not apply original IDs to them.
    if (face_data_2_ == 0U) {
        for (const auto table : symbols.find("MESSAGES")) {
            if ((table & 0xffffU) < 0x8000U) continue;
            for (unsigned index = 0; index < std::size(localization::original_dialogue); ++index) {
                const auto pointer = rom.read16(table + index * 2U);
                if (pointer < 0x8000U) continue;
                dialogue_ids_.emplace((table & 0xff0000U) | (pointer + 2U), index + 1U);
            }
            break;
        }
    }
}

void ScaledTextRenderer::draw(
    std::uint16_t message_pointer,
    std::uint8_t colour,
    std::int8_t size_adjustment,
    const RenderPose& pose,
    Framebuffer& target,
    std::uint8_t colour_index_base) const {
    // A text object may remain in the object list for one update before its
    // message pointer is assigned (Star Fox EX does this during its intro).
    // The Super FX sees the lower half of a LoROM bank as non-ROM/open bus;
    // it is not a valid projected-message string.  Treat that transient state
    // as invisible instead of asking RomImage to translate e.g. $2d:0000.
    if (pose.z < 128.0 || message_pointer < 0x8000U) return;
    const auto message_address = (messages_ & 0xff0000U) | message_pointer;
    std::vector<std::uint8_t> characters;
    characters.reserve(32U);
    for (std::uint32_t index = 0; index < 256U; ++index) {
        const auto character = rom_->read8(message_address + index);
        if (character == 0U) break;
        characters.push_back(character);
    }
    if (characters.empty()) return;

    // Project directly into the stored raster at enhanced resolutions. Doing
    // the projection at logical resolution first discarded subpixel movement
    // and expanded each coarse output pixel into a scale-by-scale block.
    const auto raster_scale = target.draw_scale();
    struct RestoreTextScale {
        Framebuffer& target;
        std::uint32_t scale;
        ~RestoreTextScale() { target.set_draw_scale(scale); }
    } restore{target, raster_scale};
    target.set_draw_scale(1U);
    const double focal_length = 256.0 * raster_scale;
    const auto world_character_size = 127 + static_cast<int>(size_adjustment);
    if (world_character_size <= 0) return;
    const auto dimension = static_cast<int>(std::trunc(
        world_character_size * focal_length / pose.z));
    if (dimension <= 0) return;
    const auto centre_x = static_cast<int>(target.width() / 2U) + static_cast<int>(
        std::trunc(pose.x * focal_length / pose.z));
    const auto centre_y = static_cast<int>(target.height() / 2U) + static_cast<int>(
        std::trunc(pose.y * focal_length / pose.z));
    const auto string_width = dimension * static_cast<int>(characters.size());
    const auto left = centre_x - string_width / 2;
    const auto top = centre_y - dimension / 2;
    const auto output_colour = static_cast<std::uint8_t>(colour_index_base + colour);

    for (std::size_t character_index = 0;
         character_index < characters.size(); ++character_index) {
        const auto token = characters[character_index];
        if (token == 0U || token > 41U) continue;
        const auto glyph = font_ + static_cast<std::uint32_t>(token - 1U) * 32U;
        for (auto y = 0; y < dimension; ++y) {
            const auto source_y = std::min(15, y * 16 / dimension);
            const auto row = rom_->read16(glyph + static_cast<std::uint32_t>(source_y * 2));
            for (auto x = 0; x < dimension; ++x) {
                const auto source_x = std::min(15, x * 16 / dimension);
                if ((row & (0x8000U >> source_x)) == 0U) continue;
                target.set(left + static_cast<int>(character_index) * dimension + x,
                    top + y, output_colour);
            }
        }
    }
}

std::vector<std::u32string_view> ScaledTextRenderer::translated_game_text_lines(
    std::uint32_t address, std::int32_t width, std::size_t max_characters) const {
    std::vector<std::u32string_view> lines;
    if (language_ == 0 || width <= 0) return lines;
    const auto found=dialogue_ids_.find(address);
    auto text=face_data_2_ != 0
        ? localization::translated_ex_dialogue(address,language_)
        : found != dialogue_ids_.end()
            ? localization::translated_dialogue(found->second,language_) : std::u32string_view{};
    text=text.substr(0,max_characters);
    while (!text.empty()) {
        std::size_t end=0, last_space=std::u32string_view::npos;
        int used=0;
        while (end<text.size()) {
            const auto advance=measure_unicode(text.substr(end,1));
            if (used+advance>width && end>0) break;
            if (text[end]==U' ') last_space=end;
            used+=advance;
            ++end;
        }
        auto next=end;
        if (end<text.size() && last_space!=std::u32string_view::npos) {
            end=last_space; next=last_space+1;
        }
        lines.push_back(text.substr(0,end));
        text.remove_prefix(next);
    }
    return lines;
}

void ScaledTextRenderer::draw_game_text(
    std::uint32_t text_address,
    std::int32_t x,
    std::int32_t y,
    Framebuffer& target,
    std::uint8_t colour_index_base,
    std::optional<std::uint8_t> forced_colour,
    std::int32_t right_clip,
    std::size_t max_characters) const {
    // EX can open the portrait/message window one source update before it
    // assigns FRIENDS_MESSAGE. dialogue_state() retains MARIOMSGS' bank, so
    // that transient null pointer arrives here as e.g. $2d:0000 instead of
    // integer zero. The SNES sees open bus in the lower half of a LoROM bank;
    // it does not try to read a string there.
    if ((text_address & 0xffffU) < 0x8000U) return;
    if (language_ != 0U) {
        const auto found = dialogue_ids_.find(text_address);
        auto translated = face_data_2_ != 0U
            ? localization::translated_ex_dialogue(text_address, language_)
            : found != dialogue_ids_.end()
                ? localization::translated_dialogue(found->second, language_)
                : std::u32string_view{};
        if (!translated.empty()) {
            const auto colour = forced_colour.value_or(rom_->read8(text_address)) & 15U;
            for (const auto line : translated_game_text_lines(text_address,right_clip-x,max_characters)) {
                draw_unicode(line, x, y, target,
                    static_cast<std::uint8_t>(colour), colour_index_base);
                y += 10;
            }
            return;
        }
    }
    const auto colour = rom_->read8(text_address++);
    const auto output_colour = static_cast<std::uint8_t>(
        colour_index_base + (forced_colour.value_or(colour) & 0x0fU));
    std::vector<std::uint8_t> text;
    text.reserve(256U);
    for (std::size_t character = 0;
         character < std::min<std::size_t>(256U, max_characters); ++character) {
        const auto ascii = rom_->read8(text_address + character);
        if (ascii == 0U) break;
        text.push_back(ascii);
    }
    const auto glyph_width = [this](std::uint8_t ascii) {
        if (ascii == 32U) return std::uint8_t{5U};
        if (ascii < 32U) return std::uint8_t{};
        const auto translated = rom_->read8(
            game_font_translation_ + static_cast<std::uint32_t>(ascii - 32U));
        return rom_->read8(game_font_widths_ + translated);
    };
    const auto draw_character = [this, &target, output_colour, &glyph_width](
                                    std::uint8_t ascii,
                                    std::int32_t draw_x,
                                    std::int32_t draw_y) {
        const auto width = glyph_width(ascii);
        if (ascii <= 32U || width == 0U) return;
        const auto translated = rom_->read8(
            game_font_translation_ + static_cast<std::uint32_t>(ascii - 32U));
        const auto glyph = game_font_glyphs_
            + static_cast<std::uint32_t>(translated) * 24U;
        for (std::int32_t row = 0; row < 12; ++row) {
            const auto bits = rom_->read16(glyph + static_cast<std::uint32_t>(row * 2));
            for (std::int32_t column = 0; column < width; ++column) {
                if ((bits & (0x8000U >> column)) != 0U) {
                    target.set(draw_x + column, draw_y + row, output_colour);
                }
            }
        }
    };

    std::size_t line_start = 0U;
    while (line_start < text.size() && y < static_cast<std::int32_t>(target.height())) {
        auto line_end = text.size();
        auto next_line = text.size();
        std::size_t last_space = text.size();
        std::int32_t line_width = 0;
        for (std::size_t index = line_start; index < text.size(); ++index) {
            const auto width = static_cast<std::int32_t>(glyph_width(text[index]));
            if (text[index] == 32U) last_space = index;
            if (x + line_width + width > right_clip) {
                if (last_space != text.size() && last_space >= line_start) {
                    line_end = last_space;
                    next_line = last_space + 1U;
                } else {
                    line_end = index;
                    next_line = index;
                }
                break;
            }
            line_width += width;
        }

        auto draw_x = x;
        for (auto index = line_start; index < line_end; ++index) {
            draw_character(text[index], draw_x, y);
            draw_x += glyph_width(text[index]);
        }
        if (next_line == text.size()) break;
        if (next_line <= line_start) ++next_line;
        line_start = next_line;
        y += 13;
    }
}

void ScaledTextRenderer::draw_face(
    std::uint8_t frame,
    std::int32_t x,
    std::int32_t y,
    Framebuffer& target,
    std::uint8_t colour_index_base,
    bool alternate_portraits) const {
    const auto data = alternate_portraits && face_data_2_ != 0U
        ? face_data_2_ : face_data_;
    const auto frame_address = data + static_cast<std::uint32_t>(frame) * 640U;
    for (std::int32_t tile_x = 0; tile_x < 4; ++tile_x) {
        for (std::int32_t tile_y = 0; tile_y < 5; ++tile_y) {
            const auto tile = frame_address
                + static_cast<std::uint32_t>(tile_x * 5 + tile_y) * 32U;
            for (std::int32_t row = 0; row < 8; ++row) {
                const std::array<std::uint8_t, 4> plane_pairs{
                    rom_->read8(tile + static_cast<std::uint32_t>(row * 2)),
                    rom_->read8(tile + static_cast<std::uint32_t>(row * 2 + 1)),
                    rom_->read8(tile + 16U + static_cast<std::uint32_t>(row * 2)),
                    rom_->read8(tile + 16U + static_cast<std::uint32_t>(row * 2 + 1)),
                };
                for (std::int32_t column = 0; column < 8; ++column) {
                    const auto mask = static_cast<std::uint8_t>(0x80U >> column);
                    std::uint8_t pixel{};
                    for (std::uint8_t plane = 0; plane < 4U; ++plane) {
                        if ((plane_pairs[plane] & mask) != 0U) {
                            pixel |= static_cast<std::uint8_t>(1U << plane);
                        }
                    }
                    target.set(x + tile_x * 8 + column, y + tile_y * 8 + row,
                        static_cast<std::uint8_t>(colour_index_base + pixel));
                }
            }
        }
    }
}

void ScaledTextRenderer::draw_ascii(
    std::string_view text,
    std::int32_t x,
    std::int32_t y,
    Framebuffer& target,
    std::uint8_t colour,
    std::uint8_t colour_index_base) const {
    if (const auto translated = localization::menu_translation(text, language_); !translated.empty()) {
        draw_unicode(translated, x, y, target, colour, colour_index_base, true);
        return;
    }
    if (language_ != 0U) {
        draw_unicode(std::u32string{text.begin(), text.end()}, x, y, target, colour, colour_index_base, true);
        return;
    }
    const auto output_colour = static_cast<std::uint8_t>(
        colour_index_base + (colour & 0x0fU));
    for (const auto character : text) {
        const auto ascii = static_cast<std::uint8_t>(character);
        if (ascii == '\n') {
            y += 13;
            continue;
        }
        if (ascii == '/') {
            for (std::int32_t row = 0; row < 12; ++row) {
                target.set(x + 3 - row / 3, y + row, output_colour);
            }
            x += 5;
            continue;
        }
        if (ascii < 32U) continue;
        const auto translated = rom_->read8(
            game_font_translation_ + static_cast<std::uint32_t>(ascii - 32U));
        const auto width = ascii == 32U ? std::uint8_t{5U}
            : rom_->read8(game_font_widths_ + translated);
        if (ascii != 32U && width != 0U) {
            const auto glyph = game_font_glyphs_
                + static_cast<std::uint32_t>(translated) * 24U;
            for (std::int32_t row = 0; row < 12; ++row) {
                const auto bits = rom_->read16(
                    glyph + static_cast<std::uint32_t>(row * 2));
                for (std::int32_t column = 0; column < width; ++column) {
                    if ((bits & (0x8000U >> column)) != 0U) {
                        target.set(x + column, y + row, output_colour);
                    }
                }
            }
        }
        x += width;
    }
}

void ScaledTextRenderer::draw_ascii_compact(
    std::string_view text,
    std::int32_t x,
    std::int32_t y,
    Framebuffer& target,
    std::uint8_t colour,
    std::uint8_t colour_index_base) const {
    if (const auto translated = localization::menu_translation(text, language_); !translated.empty()) {
        draw_unicode(translated, x, y, target, colour, colour_index_base, true);
        return;
    }
    if (language_ != 0U) {
        draw_unicode(std::u32string{text.begin(), text.end()}, x, y, target, colour, colour_index_base, true);
        return;
    }
    constexpr std::int32_t output_height = 8;
    const auto output_colour = static_cast<std::uint8_t>(
        colour_index_base + (colour & 0x0fU));
    for (const auto character : text) {
        const auto ascii = static_cast<std::uint8_t>(character);
        if (ascii == '\n') {
            y += output_height + 1;
            continue;
        }
        // The source translation aliases slash to a vertical separator.
        // Host-authored labels need an actual diagonal slash.
        if (ascii == '/') {
            for (std::int32_t row = 0; row < output_height; ++row) {
                target.set(x + 3 - row * 4 / output_height, y + row,
                    output_colour);
            }
            x += 5;
            continue;
        }
        if (ascii < 32U) continue;
        const auto translated = rom_->read8(
            game_font_translation_ + static_cast<std::uint32_t>(ascii - 32U));
        const auto width = ascii == 32U ? std::uint8_t{5U}
            : rom_->read8(game_font_widths_ + translated);
        if (ascii != 32U && width != 0U) {
            const auto glyph = game_font_glyphs_
                + static_cast<std::uint32_t>(translated) * 24U;
            for (std::int32_t row = 0; row < output_height; ++row) {
                const auto source_row = row * 11 / (output_height - 1);
                const auto bits = rom_->read16(
                    glyph + static_cast<std::uint32_t>(source_row * 2));
                for (std::int32_t column = 0; column < width; ++column) {
                    if ((bits & (0x8000U >> column)) != 0U) {
                        target.set(x + column, y + row, output_colour);
                    }
                }
            }
        }
        x += width;
    }
}

std::int32_t ScaledTextRenderer::measure_ascii(std::string_view text) const {
    if (const auto translated = localization::menu_translation(text, language_); !translated.empty())
        return measure_menu_unicode(translated);
    if (language_ != 0U) return measure_menu_unicode(std::u32string{text.begin(), text.end()});
    std::int32_t line_width{};
    std::int32_t maximum_width{};
    for (const auto character : text) {
        const auto ascii = static_cast<std::uint8_t>(character);
        if (ascii == '\n') {
            maximum_width = std::max(maximum_width, line_width);
            line_width = 0;
            continue;
        }
        if (ascii < 32U) continue;
        const auto translated = rom_->read8(
            game_font_translation_ + static_cast<std::uint32_t>(ascii - 32U));
        line_width += (ascii == 32U || ascii == '/') ? 5
            : static_cast<std::int32_t>(
                rom_->read8(game_font_widths_ + translated));
    }
    return std::max(maximum_width, line_width);
}

} // namespace starfox::render
