#pragma once

#include <array>
#include <cstdint>

namespace starfox::simulation {

struct SnesPpuState {
    std::array<std::uint8_t, 64U * 1024U> vram{};
    std::array<std::uint16_t, 256> cgram{};
    std::array<std::uint8_t, 544> oam{};
    std::uint8_t background_mode{2U};
    bool bg3_high_priority{};
    bool bg1_tile_size_16{};
    bool bg2_tile_size_16{};
    bool bg3_tile_size_16{};
    bool bg4_tile_size_16{};
    std::uint8_t mosaic{};
    std::uint8_t object_select{3U};
    std::uint16_t bg1_character_base{};
    std::uint16_t bg1_screen_base{};
    std::uint8_t bg1_screen_size{};
    std::int16_t bg1_scroll_x{};
    std::int16_t bg1_scroll_y{};
    std::uint16_t bg2_character_base{0x5000U};
    std::uint16_t bg2_screen_base{0x7000U};
    std::uint8_t bg2_screen_size{3U};
    std::int16_t bg2_scroll_x{};
    std::int16_t bg2_scroll_y{};
    std::uint16_t bg3_screen_base{0x2c00U};
    std::uint8_t bg3_screen_size{3U};
    std::uint16_t bg3_character_base{0x7000U};
    std::int16_t bg3_scroll_x{};
    std::int16_t bg3_scroll_y{};
    std::uint8_t main_screen{0x13U};
    bool bg2_vertical_offsets_enabled{};
    std::array<std::int16_t, 224> bg2_horizontal_offsets{};
    bool bg2_horizontal_offsets_enabled{};
    // IRQ SETBG2VOFS: tunnel floor/ceiling rows select one of two tilemap
    // pages. These are discrete HDMA page selections, not roll offsets.
    std::array<std::int16_t, 224> bg2_scanline_scroll_y{};
    bool bg2_scanline_scroll_enabled{};
};

} // namespace starfox::simulation
