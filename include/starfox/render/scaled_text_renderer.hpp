#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/software_renderer.hpp"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace starfox::render {

inline constexpr std::uint8_t briefing_text_palette_base = 6U * 16U;

// Renderer for MDSPRITE.MC's 16x16 projected text objects. Strings and glyph
// rows are consumed directly from the assembled ROM rather than substituted
// with a host font.
class ScaledTextRenderer {
public:
    void set_language(std::uint8_t language) noexcept { language_ = language < 5 ? language : 0; }
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
        std::uint8_t colour_index_base = 7U * 16U,
        bool alternate_portraits = false) const;

    void draw_ascii(
        std::string_view text,
        std::int32_t x,
        std::int32_t y,
        Framebuffer& target,
        std::uint8_t colour = 14U,
        std::uint8_t colour_index_base = 7U * 16U) const;

    // Preserve the source font's horizontal metrics while reducing its
    // twelve scanlines to eight for dense host-authored option lists.
    void draw_ascii_compact(
        std::string_view text,
        std::int32_t x,
        std::int32_t y,
        Framebuffer& target,
        std::uint8_t colour = 14U,
        std::uint8_t colour_index_base = 7U * 16U) const;

    [[nodiscard]] std::int32_t measure_ascii(std::string_view text) const;
    void draw_unicode(std::u32string_view text, std::int32_t x, std::int32_t y,
        Framebuffer& target, std::uint8_t colour = 14U,
        std::uint8_t colour_index_base = 7U * 16U, bool menu_size = false) const;
    [[nodiscard]] std::int32_t measure_unicode(std::u32string_view text) const;
    [[nodiscard]] std::vector<std::u32string_view> translated_game_text_lines(
        std::uint32_t address, std::int32_t width, std::size_t max_characters = 256U) const;

private:
    [[nodiscard]] std::int32_t menu_glyph_advance(char32_t code) const;
    [[nodiscard]] std::int32_t measure_menu_unicode(std::u32string_view text) const;
    std::uint8_t language_{};
    std::unordered_map<std::uint32_t, unsigned> dialogue_ids_;
    const assets::RomImage* rom_{};
    std::uint32_t font_{};
    std::uint32_t messages_{};
    std::uint32_t game_font_widths_{};
    std::uint32_t game_font_glyphs_{};
    std::uint32_t game_font_translation_{};
    std::uint32_t face_data_{};
    std::uint32_t face_data_2_{};
};

} // namespace starfox::render
