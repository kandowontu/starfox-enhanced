#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/software_renderer.hpp"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string_view>

namespace starfox::render {

// Renderer for MDSPRITE.MC's 16x16 projected text objects. Strings and glyph
// rows are consumed directly from the assembled ROM rather than substituted
// with a host font.
class ScaledTextRenderer {
public:
    ScaledTextRenderer(
        const assets::RomImage& rom,
        const assets::SymbolMap& symbols);

    void draw(
        std::uint16_t message_pointer,
        std::uint8_t colour,
        std::int8_t size_adjustment,
        const RenderPose& pose,
        Framebuffer& target,
        std::uint8_t colour_index_base = 7U * 16U) const;

    // Draw MTXTPRT.MC's variable-width 12-pixel game text directly from a
    // source `txt` record (colour byte, ASCII bytes, zero terminator).
    void draw_game_text(
        std::uint32_t text_address,
        std::int32_t x,
        std::int32_t y,
        Framebuffer& target,
        std::uint8_t colour_index_base = 7U * 16U,
        std::optional<std::uint8_t> forced_colour = std::nullopt,
        std::int32_t right_clip = 224,
        std::size_t max_characters = 256U) const;

    // Draw one of FACEDATA's original 32x40, 4-bpp portrait frames.
    void draw_face(
        std::uint8_t frame,
        std::int32_t x,
        std::int32_t y,
        Framebuffer& target,
        std::uint8_t colour_index_base = 7U * 16U) const;

    void draw_ascii(
        std::string_view text,
        std::int32_t x,
        std::int32_t y,
        Framebuffer& target,
        std::uint8_t colour = 14U,
        std::uint8_t colour_index_base = 7U * 16U) const;

private:
    const assets::RomImage* rom_{};
    std::uint32_t font_{};
    std::uint32_t messages_{};
    std::uint32_t game_font_widths_{};
    std::uint32_t game_font_glyphs_{};
    std::uint32_t game_font_translation_{};
    std::uint32_t face_data_{};
};

} // namespace starfox::render
