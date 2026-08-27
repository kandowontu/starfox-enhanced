#include "starfox/render/scaled_text_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace starfox::render {
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
      face_data_(rom_symbol(symbols, "FACEDATA")) {}

void ScaledTextRenderer::draw(
    std::uint16_t message_pointer,
    std::uint8_t colour,
    std::int8_t size_adjustment,
    const RenderPose& pose,
    Framebuffer& target,
    std::uint8_t colour_index_base) const {
    if (pose.z < 128.0) return;
    const auto message_address = (messages_ & 0xff0000U) | message_pointer;
    std::vector<std::uint8_t> characters;
    characters.reserve(32U);
    for (std::uint32_t index = 0; index < 256U; ++index) {
        const auto character = rom_->read8(message_address + index);
        if (character == 0U) break;
        characters.push_back(character);
    }
    if (characters.empty()) return;

    constexpr double focal_length = 256.0;
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

void ScaledTextRenderer::draw_game_text(
    std::uint32_t text_address,
    std::int32_t x,
    std::int32_t y,
    Framebuffer& target,
    std::uint8_t colour_index_base,
    std::optional<std::uint8_t> forced_colour,
    std::int32_t right_clip,
    std::size_t max_characters) const {
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
    std::uint8_t colour_index_base) const {
    const auto frame_address = face_data_ + static_cast<std::uint32_t>(frame) * 640U;
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
    const auto output_colour = static_cast<std::uint8_t>(
        colour_index_base + (colour & 0x0fU));
    for (const auto character : text) {
        const auto ascii = static_cast<std::uint8_t>(character);
        if (ascii == '\n') {
            y += 13;
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

} // namespace starfox::render
