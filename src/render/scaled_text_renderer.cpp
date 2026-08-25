#include "starfox/render/scaled_text_renderer.hpp"

#include <algorithm>
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
      game_font_translation_(rom_symbol(symbols, "FONT0TRN")) {}

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
    std::uint8_t colour_index_base) const {
    const auto colour = rom_->read8(text_address++);
    const auto output_colour = static_cast<std::uint8_t>(
        colour_index_base + (colour & 0x0fU));
    for (std::size_t character = 0; character < 256U; ++character) {
        const auto ascii = rom_->read8(text_address++);
        if (ascii == 0U) return;
        if (ascii < 32U) continue;
        const auto translated = rom_->read8(
            game_font_translation_ + static_cast<std::uint32_t>(ascii - 32U));
        const auto width = rom_->read8(game_font_widths_ + translated);
        if (ascii == 32U || width == 0U) {
            x += ascii == 32U ? 5 : width;
            continue;
        }
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
        x += width;
    }
}

} // namespace starfox::render
