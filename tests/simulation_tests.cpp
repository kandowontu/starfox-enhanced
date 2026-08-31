#include "starfox/simulation/math.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/simulation/path_vm.hpp"
#include "starfox/simulation/particle_system.hpp"
#include "starfox/simulation/prng.hpp"
#include "starfox/simulation/strategy_scheduler.hpp"
#include "starfox/simulation/wdc65816.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/assets/decrunch.hpp"
#include "starfox/input/buttons.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool started_msu_track(
    std::span<const starfox::simulation::MsuRegisterWrite> writes,
    std::uint16_t expected_track, bool repeat) {
    std::uint16_t selected_track{};
    for (const auto& write : writes) {
        if (write.address == 0x2004U) {
            selected_track = static_cast<std::uint16_t>(
                (selected_track & 0xff00U) | write.value);
        } else if (write.address == 0x2005U) {
            selected_track = static_cast<std::uint16_t>(
                (selected_track & 0x00ffU)
                | (static_cast<std::uint16_t>(write.value) << 8U));
        } else if (write.address == 0x2007U
            && write.value == (repeat ? 3U : 1U)
            && selected_track == expected_track) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    starfox::simulation::OriginalPrng random;
    constexpr std::array<std::uint8_t, 16> expected{
        0x82, 0x72, 0x66, 0xd3, 0xce, 0x92, 0xbd, 0x4c,
        0x93, 0xf3, 0xc0, 0x6d, 0xd2, 0x03, 0x90, 0x80};
    for (const auto value : expected) {
        require(random.next() == value, "original RNG sequence diverged");
    }

    require(starfox::simulation::add16(32'767, 1) == -32'768,
            "16-bit addition did not wrap");
    require(starfox::simulation::subtract16(-32'768, 1) == 32'767,
            "16-bit subtraction did not wrap");
    require(starfox::simulation::arithmetic_shift_right(-3, 1) == -2,
            "arithmetic right shift did not preserve 65816 sign behavior");
    require(starfox::simulation::multiply_q15(16'384, 16'384) == 8'192,
            "Q15 multiplication is wrong");

    std::array<std::uint16_t, 16> palette_words{};
    palette_words[1] = 0x001f;
    palette_words[2] = 0x03e0;
    palette_words[3] = 0x7c00;
    const auto palette = starfox::render::decode_bgr555_palette(palette_words);
    require(palette[1].r == 255 && palette[1].g == 0 && palette[1].b == 0
                && palette[2].g == 255 && palette[3].b == 255,
            "SNES BGR555 palette expansion is wrong");
    const auto half_palette = starfox::render::apply_snes_brightness(palette, 7);
    require(half_palette[1].r == 119 && half_palette[2].g == 119,
            "SNES master brightness scaling is wrong");
    starfox::render::Framebuffer superfx_mosaic_source{6U, 3U};
    superfx_mosaic_source.set(0, 0, 5U);
    superfx_mosaic_source.set(3, 0, 8U);
    starfox::render::Framebuffer superfx_mosaic_target{10U, 3U};
    starfox::render::LayerCompositeSettings superfx_mosaic_settings;
    superfx_mosaic_settings.offset_x = 2;
    superfx_mosaic_settings.mosaic_origin_x = 2;
    superfx_mosaic_settings.mosaic = 0x21U; // 3x3, BG1 enabled.
    superfx_mosaic_settings.mosaic_layer_mask = 0x01U;
    starfox::render::composite_transparent_layer(superfx_mosaic_source,
        superfx_mosaic_target, superfx_mosaic_settings);
    require(superfx_mosaic_target.get(2, 0) == 5U
                && superfx_mosaic_target.get(4, 2) == 5U
                && superfx_mosaic_target.get(5, 0) == 8U
                && superfx_mosaic_target.get(7, 2) == 8U,
            "host Super FX BG1 layers did not use SNES mosaic sampling");
    superfx_mosaic_target.clear();
    superfx_mosaic_settings.mosaic_layer_mask = 0x02U;
    starfox::render::composite_transparent_layer(superfx_mosaic_source,
        superfx_mosaic_target, superfx_mosaic_settings);
    require(superfx_mosaic_target.get(2, 0) == 5U
                && superfx_mosaic_target.get(3, 0) == 0U
                && superfx_mosaic_target.get(5, 0) == 8U,
            "BG1 geometry mosaic ignored the selected SNES layer mask");
    starfox::render::Framebuffer meter_frame{224, 192};
    const starfox::render::SpriteRenderer sprite_renderer;
    sprite_renderer.draw_meters({0, 0, false, true, 10, 20}, meter_frame);
    require(meter_frame.get(200, 4) == 7U * 16U + 2U
                && meter_frame.get(198, 2) == 7U * 16U + 14U,
            "source boss meter geometry was not rendered");
    starfox::render::Framebuffer wide_meter_frame{400, 192};
    sprite_renderer.draw_meters(
        {36, 36, false, true, 0, 0}, wide_meter_frame, true);
    require(wide_meter_frame.get(24, 178) == 7U * 16U + 13U
                && wide_meter_frame.get(336, 176) == 7U * 16U + 13U,
            "expanded HUD meters did not anchor to the outer screen edges");
    starfox::render::HudLayout shifted_hud;
    shifted_hud[starfox::render::HudElement::shield] = {10, -5};
    shifted_hud[starfox::render::HudElement::bombs_boost] = {-12, 3};
    shifted_hud[starfox::render::HudElement::boss_health] = {-20, 6};
    wide_meter_frame.clear(0U);
    sprite_renderer.draw_meters(
        {36, 36, false, true, 0, 0}, wide_meter_frame, true, &shifted_hud);
    require(wide_meter_frame.get(34, 173) == 7U * 16U + 13U
                && wide_meter_frame.get(324, 179) == 7U * 16U + 13U
                && wide_meter_frame.get(24, 176) == 0U,
            "custom HUD layout did not move shield and bombs/boost meters");
    wide_meter_frame.clear(0U);
    sprite_renderer.draw_meters(
        {0, 0, false, true, 10, 20}, wide_meter_frame, true, &shifted_hud);
    require(wide_meter_frame.get(342, 8) == 7U * 16U + 14U
                && wide_meter_frame.get(362, 2) == 0U,
            "custom HUD layout did not move the boss health meter independently");
    starfox::simulation::MeterState ex_meters{
        80U, 20U, true, true, 10U, 20U};
    ex_meters.extended = true;
    ex_meters.boost_enabled = false;
    ex_meters.player_two_activated = true;
    ex_meters.damage_two = 60U;
    ex_meters.player_health_width = 104U;
    ex_meters.player_health_max = 100U;
    wide_meter_frame.clear(0U);
    sprite_renderer.draw_meters(ex_meters, wide_meter_frame, true);
    require(wide_meter_frame.get(17, 7) == 7U * 16U + 13U
                && wide_meter_frame.get(19, 9) == 7U * 16U + 7U
                && wide_meter_frame.get(275, 7) == 7U * 16U + 13U
                && wide_meter_frame.get(277, 9) == 7U * 16U + 2U
                && wide_meter_frame.get(336, 176) == 0U
                && wide_meter_frame.get(370, 178) == 7U * 16U + 14U,
            "Star Fox EX two-player meter geometry diverged from MDRAWLIS.MC");
    ex_meters.damage_two = 0U;
    wide_meter_frame.clear(0U);
    sprite_renderer.draw_meters(ex_meters, wide_meter_frame, true);
    require(wide_meter_frame.get(275, 7) == 7U * 16U + 11U
                && wide_meter_frame.get(277, 9) == 7U * 16U,
            "Star Fox EX inactive player-two meter did not use its wipe state");

    starfox::simulation::SnesPpuState priority_ppu;
    priority_ppu.main_screen = 0x12U;
    const auto bg2_map_byte = static_cast<std::size_t>(
        priority_ppu.bg2_screen_base) * 2U;
    priority_ppu.vram[bg2_map_byte] = 1U;
    priority_ppu.vram[bg2_map_byte + 1U] = 0x20U;
    const auto bg2_character_byte = static_cast<std::size_t>(
        priority_ppu.bg2_character_base) * 2U + 32U;
    priority_ppu.vram[bg2_character_byte] = 0x80U;
    starfox::render::Framebuffer priority_frame{8, 8};
    priority_frame.clear(42U);
    const starfox::render::BackgroundRenderer background_renderer;
    background_renderer.draw_bg2(priority_ppu, 0, 0, priority_frame,
        starfox::render::TilePriorityPass::low);
    require(priority_frame.get(0, 0) == 42U,
            "high-priority BG2 tile leaked into the low-priority pass");
    background_renderer.draw_bg2(priority_ppu, 0, 0, priority_frame,
        starfox::render::TilePriorityPass::high);
    require(priority_frame.get(0, 0) == 1U,
            "high-priority BG2 tile was not composited in its own pass");
    starfox::render::Framebuffer upscaled_priority_frame{8, 8, 10U};
    upscaled_priority_frame.clear(42U);
    background_renderer.draw_bg2(priority_ppu, 0, 0,
        upscaled_priority_frame, starfox::render::TilePriorityPass::high);
    auto complete_upscaled_background_cell = true;
    for (std::uint32_t y = 0; y < 10U; ++y) {
        for (std::uint32_t x = 0; x < 10U; ++x) {
            complete_upscaled_background_cell =
                complete_upscaled_background_cell
                && upscaled_priority_frame.get_stored(x, y) == 1U;
        }
    }
    require(complete_upscaled_background_cell,
            "10x Render Upscale fragmented a cartridge background pixel");
    starfox::render::Framebuffer centred_background_frame{400, 8};
    centred_background_frame.clear(42U);
    background_renderer.draw_bg2(priority_ppu, 0, 0,
        centred_background_frame, starfox::render::TilePriorityPass::high,
        72, false);
    require(centred_background_frame.get(72, 0) == 1U
                && centred_background_frame.get(0, 0) == 42U
                && centred_background_frame.get(328, 0) == 42U,
            "centred cartridge background repeated into widescreen margins");
    auto mosaic_bg2_ppu = priority_ppu;
    mosaic_bg2_ppu.mosaic = 0x12U; // 2x2, BG2 enabled.
    starfox::render::Framebuffer mosaic_bg2_frame{8, 8};
    mosaic_bg2_frame.clear(42U);
    background_renderer.draw_bg2(mosaic_bg2_ppu, 0, 0, mosaic_bg2_frame,
        starfox::render::TilePriorityPass::high);
    require(mosaic_bg2_frame.get(0, 0) == 1U
                && mosaic_bg2_frame.get(1, 0) == 1U
                && mosaic_bg2_frame.get(0, 1) == 1U,
            "SNES BG2 mosaic did not repeat its source pixel");

    starfox::simulation::SnesPpuState title_priority_ppu;
    title_priority_ppu.background_mode = 1U;
    title_priority_ppu.main_screen = 0x07U;
    title_priority_ppu.bg3_screen_base = 0x1000U;
    title_priority_ppu.bg3_character_base = 0x2000U;
    title_priority_ppu.bg1_screen_base = 0x3000U;
    title_priority_ppu.bg1_character_base = 0x4000U;
    title_priority_ppu.bg2_screen_base = 0x5000U;
    title_priority_ppu.bg2_character_base = 0x6000U;
    title_priority_ppu.cgram[1U] = 0x7fffU;
    const auto title_bg3_map = static_cast<std::size_t>(
        title_priority_ppu.bg3_screen_base) * 2U;
    title_priority_ppu.vram[title_bg3_map] = 1U;
    title_priority_ppu.vram[title_bg3_map + 2U] = 1U;
    title_priority_ppu.vram[title_bg3_map + 3U] = 0x20U;
    title_priority_ppu.vram[static_cast<std::size_t>(
        title_priority_ppu.bg3_character_base) * 2U + 16U] = 0x80U;
    const auto title_bg1_map = static_cast<std::size_t>(
        title_priority_ppu.bg1_screen_base) * 2U;
    title_priority_ppu.vram[title_bg1_map + 4U] = 1U;
    title_priority_ppu.vram[static_cast<std::size_t>(
        title_priority_ppu.bg1_character_base) * 2U + 32U] = 0x80U;
    const auto title_bg2_map = static_cast<std::size_t>(
        title_priority_ppu.bg2_screen_base) * 2U;
    title_priority_ppu.vram[title_bg2_map + 6U] = 1U;
    title_priority_ppu.vram[title_bg2_map + 7U] = 0x20U;
    title_priority_ppu.vram[static_cast<std::size_t>(
        title_priority_ppu.bg2_character_base) * 2U + 32U] = 0x80U;
    starfox::render::Framebuffer title_priority_frame{32U, 8U};
    title_priority_frame.clear(42U);
    background_renderer.draw_title_foreground(
        title_priority_ppu, 0, 0, title_priority_frame, 0, false);
    require(title_priority_frame.get(0, 0) == 42U
                && title_priority_frame.get(8, 0) == 1U
                && title_priority_frame.get(16, 0) == 42U
                && title_priority_frame.get(24, 0) == 1U,
            "title foreground did not split backdrop and roster priorities");
    title_priority_frame.clear(42U);
    background_renderer.draw_title_foreground(
        title_priority_ppu, 0, 0, title_priority_frame);
    require(title_priority_frame.get(0, 0) == 42U
                && title_priority_frame.get(8, 0) == 1U
                && title_priority_frame.get(16, 0) == 1U
                && title_priority_frame.get(24, 0) == 1U,
            "title foreground dropped PRESS START or native BG1 text");
    title_priority_ppu.cgram[1U] = 0U;
    title_priority_frame.clear(42U);
    background_renderer.draw_title_foreground(
        title_priority_ppu, 0, 0, title_priority_frame);
    require(title_priority_frame.get(16, 0) == 42U
                && title_priority_frame.get(24, 0) == 42U,
            "indexed-black title BG1/BG2 tile cut through a Super FX model");

    starfox::simulation::SnesPpuState unwrapped_title_ppu;
    unwrapped_title_ppu.background_mode = 1U;
    unwrapped_title_ppu.main_screen = 0x02U;
    unwrapped_title_ppu.bg2_screen_base = 0x5000U;
    unwrapped_title_ppu.bg2_character_base = 0x6000U;
    const auto unwrapped_map_byte = static_cast<std::size_t>(
        unwrapped_title_ppu.bg2_screen_base) * 2U;
    unwrapped_title_ppu.vram[unwrapped_map_byte] = 1U;
    unwrapped_title_ppu.vram[static_cast<std::size_t>(
        unwrapped_title_ppu.bg2_character_base) * 2U + 32U] = 0x80U;
    starfox::render::Framebuffer unwrapped_title_frame{400U, 8U};
    unwrapped_title_frame.clear(42U);
    background_renderer.draw_bg2(unwrapped_title_ppu, -8, 0,
        unwrapped_title_frame, starfox::render::TilePriorityPass::all,
        72, true, false);
    require(unwrapped_title_frame.get(80, 0) == 1U
                && unwrapped_title_frame.get(72, 0) == 42U
                && unwrapped_title_frame.get(336, 0) == 42U,
            "wide title tilemap wrapped into duplicate outer columns");

    starfox::simulation::SnesPpuState tall_bg_ppu;
    tall_bg_ppu.main_screen = 0x02U;
    tall_bg_ppu.bg2_screen_size = 2U; // 32x64 tiles: page 1 is below page 0.
    const auto lower_page_byte = (static_cast<std::size_t>(
        tall_bg_ppu.bg2_screen_base) + 0x400U) * 2U;
    tall_bg_ppu.vram[lower_page_byte] = 1U;
    const auto tall_bg_character_byte = static_cast<std::size_t>(
        tall_bg_ppu.bg2_character_base) * 2U + 32U;
    tall_bg_ppu.vram[tall_bg_character_byte] = 0x80U;
    starfox::render::Framebuffer tall_bg_frame{1, 1};
    tall_bg_frame.clear(42U);
    background_renderer.draw_bg2(tall_bg_ppu, 0, 256, tall_bg_frame);
    require(tall_bg_frame.get(0, 0) == 1U,
            "32x64 BG tilemap lower page used the 64x64 page stride");

    starfox::simulation::SnesPpuState large_tile_ppu;
    large_tile_ppu.background_mode = 2U;
    large_tile_ppu.main_screen = 0x02U;
    large_tile_ppu.bg2_tile_size_16 = true;
    large_tile_ppu.bg2_screen_size = 0U;
    const auto large_tile_map = static_cast<std::size_t>(
        large_tile_ppu.bg2_screen_base) * 2U;
    large_tile_ppu.vram[large_tile_map] = 1U;
    const auto large_tile_characters = static_cast<std::size_t>(
        large_tile_ppu.bg2_character_base) * 2U;
    large_tile_ppu.vram[large_tile_characters + 1U * 32U] = 0x80U;
    large_tile_ppu.vram[large_tile_characters + 2U * 32U + 1U] = 0x80U;
    large_tile_ppu.vram[large_tile_characters + 17U * 32U] = 0x80U;
    large_tile_ppu.vram[large_tile_characters + 17U * 32U + 1U] = 0x80U;
    starfox::render::Framebuffer large_tile_frame{16U, 16U};
    background_renderer.draw_bg2(
        large_tile_ppu, 0, 0, large_tile_frame);
    require(large_tile_frame.get(0, 0) == 1U
                && large_tile_frame.get(8, 0) == 2U
                && large_tile_frame.get(0, 8) == 3U,
            "Mode 2 special tunnel did not decode 16x16 BG2 characters");

    starfox::simulation::SnesPpuState mode2_edge_ppu;
    mode2_edge_ppu.background_mode = 2U;
    mode2_edge_ppu.main_screen = 0x02U;
    mode2_edge_ppu.bg2_character_base = 0x1000U;
    mode2_edge_ppu.bg2_screen_base = 0x4000U;
    mode2_edge_ppu.bg2_vertical_offsets_enabled = true;
    const auto mode2_map = static_cast<std::size_t>(
        mode2_edge_ppu.bg2_screen_base) * 2U;
    mode2_edge_ppu.vram[mode2_map] = 1U;
    mode2_edge_ppu.vram[mode2_map + 32U * 2U] = 2U;
    mode2_edge_ppu.vram[mode2_map + 33U * 2U] = 2U;
    const auto mode2_characters = static_cast<std::size_t>(
        mode2_edge_ppu.bg2_character_base) * 2U;
    mode2_edge_ppu.vram[mode2_characters + 32U] = 0x80U;
    mode2_edge_ppu.vram[mode2_characters + 64U] = 0x80U;
    mode2_edge_ppu.vram[mode2_characters + 65U] = 0x80U;
    const auto mode2_offset = 0x2fa0U * 2U;
    mode2_edge_ppu.vram[mode2_offset] = 8U;
    mode2_edge_ppu.vram[mode2_offset + 1U] = 0x40U;
    starfox::render::Framebuffer mode2_edge_frame{16, 1};
    background_renderer.draw_bg2(
        mode2_edge_ppu, 0, 0, mode2_edge_frame);
    require(mode2_edge_frame.get(0, 0) == mode2_edge_frame.get(8, 0)
                && mode2_edge_frame.get(0, 0) == 3U,
            "Mode 2 edge guard exposed an unshifted left raster strip");

    auto wide_slope_ppu = mode2_edge_ppu;
    wide_slope_ppu.bg2_screen_size = 0U;
    wide_slope_ppu.vram[mode2_offset] = 8U;
    wide_slope_ppu.vram[mode2_offset + 1U] = 0x40U;
    wide_slope_ppu.vram[mode2_offset + 2U] = 7U;
    wide_slope_ppu.vram[mode2_offset + 3U] = 0x40U;
    for (std::size_t entry = 0; entry < 32U * 32U; ++entry) {
        wide_slope_ppu.vram[mode2_map + entry * 2U] = 1U;
        wide_slope_ppu.vram[mode2_map + entry * 2U + 1U] = 0U;
    }
    for (std::size_t column = 0; column < 32U; ++column) {
        wide_slope_ppu.vram[mode2_map + (64U + column) * 2U] = 2U;
    }
    for (std::size_t row = 0; row < 8U; ++row) {
        wide_slope_ppu.vram[mode2_characters + 32U + row * 2U] = 0x80U;
        wide_slope_ppu.vram[mode2_characters + 33U + row * 2U] = 0x80U;
        wide_slope_ppu.vram[mode2_characters + 64U + row * 2U] = 0x80U;
        wide_slope_ppu.vram[mode2_characters + 65U + row * 2U] = 0U;
    }
    starfox::render::Framebuffer wide_slope_frame{400, 1};
    wide_slope_frame.clear(42U);
    background_renderer.draw_bg2(wide_slope_ppu, 0, 0, wide_slope_frame,
        starfox::render::TilePriorityPass::all, 72, true);
    require(wide_slope_frame.get(72, 0) == 3U
                && wide_slope_frame.get(0, 0) == 1U,
            "Mode 2 ground slope froze across the widescreen extension");

    auto left_join_ppu = mode2_edge_ppu;
    left_join_ppu.bg2_screen_size = 0U;
    left_join_ppu.vram[mode2_offset + 2U] = 7U;
    left_join_ppu.vram[mode2_offset + 3U] = 0x40U;
    for (std::size_t entry = 0; entry < 32U * 32U; ++entry) {
        left_join_ppu.vram[mode2_map + entry * 2U] = 1U;
        left_join_ppu.vram[mode2_map + entry * 2U + 1U] = 0U;
    }
    std::fill_n(left_join_ppu.vram.begin() + mode2_characters + 32U,
        32U, 0U);
    left_join_ppu.vram[mode2_characters + 32U] = 0xffU;
    left_join_ppu.vram[mode2_characters + 32U + 2U + 1U] = 0xffU;
    starfox::render::Framebuffer left_join_frame{400, 1};
    left_join_frame.clear(42U);
    background_renderer.draw_bg2(left_join_ppu, 0, 0, left_join_frame,
        starfox::render::TilePriorityPass::all, 72, true);
    require(left_join_frame.get(72, 0) == 2U
                && left_join_frame.get(80, 0) == 1U,
            "expanded Mode 2 left guard duplicated a tile and bent the ground");

    auto quantized_slope_ppu = mode2_edge_ppu;
    quantized_slope_ppu.bg2_screen_size = 0U;
    for (std::size_t entry = 0; entry < 32U * 32U; ++entry) {
        quantized_slope_ppu.vram[mode2_map + entry * 2U] = 1U;
        quantized_slope_ppu.vram[mode2_map + entry * 2U + 1U] = 0U;
    }
    std::fill_n(quantized_slope_ppu.vram.begin() + mode2_characters + 32U,
        32U, 0U);
    quantized_slope_ppu.vram[mode2_characters + 32U + 2U * 2U] = 0xffU;
    quantized_slope_ppu.vram[mode2_characters + 32U + 4U * 2U + 1U]
        = 0xffU;
    constexpr std::array<std::uint8_t, 32> quantized_offsets{
        20U, 19U, 19U, 19U, 18U, 18U, 18U, 18U,
        18U, 17U, 17U, 17U, 17U, 16U, 16U, 16U,
        16U, 16U, 15U, 15U, 15U, 15U, 14U, 14U,
        14U, 14U, 14U, 13U, 13U, 13U, 12U, 12U,
    };
    for (std::size_t index = 0; index < quantized_offsets.size(); ++index) {
        quantized_slope_ppu.vram[mode2_offset + index * 2U]
            = quantized_offsets[index];
        quantized_slope_ppu.vram[mode2_offset + index * 2U + 1U] = 0x40U;
    }
    starfox::render::Framebuffer quantized_slope_frame{400, 1};
    quantized_slope_frame.clear(42U);
    background_renderer.draw_bg2(quantized_slope_ppu, 0, 0,
        quantized_slope_frame, starfox::render::TilePriorityPass::all, 72, true);
    require(quantized_slope_frame.get(328, 0) == 2U
                && quantized_slope_frame.get(399, 0) == 1U,
            "quantised Mode 2 edge pair flattened the ultrawide background");

    auto extended_ground_ppu = mode2_edge_ppu;
    extended_ground_ppu.bg2_screen_size = 0U;
    for (std::size_t index = 0; index < 32U; ++index) {
        extended_ground_ppu.vram[mode2_offset + index * 2U] = 0U;
        extended_ground_ppu.vram[mode2_offset + index * 2U + 1U] = 0x40U;
    }
    std::fill_n(extended_ground_ppu.vram.begin() + mode2_characters + 32U,
        32U, 0U);
    for (std::size_t row = 0; row < 8U; ++row) {
        extended_ground_ppu.vram[mode2_characters + 32U + row * 2U] = 0xffU;
    }
    for (std::size_t row = 0; row < 32U; ++row) {
        for (std::size_t column = 0; column < 32U; ++column) {
            const auto entry = row * 32U + column;
            extended_ground_ppu.vram[mode2_map + entry * 2U]
                = row < 18U ? 1U : 0U;
            extended_ground_ppu.vram[mode2_map + entry * 2U + 1U] = 0U;
        }
    }
    starfox::render::Framebuffer retail_ground_frame{256, 224};
    retail_ground_frame.clear(42U);
    background_renderer.draw_bg2(extended_ground_ppu, 0, 0,
        retail_ground_frame, starfox::render::TilePriorityPass::all, 0, true);
    starfox::render::Framebuffer extended_ground_frame{400, 224};
    extended_ground_frame.clear(42U);
    background_renderer.draw_bg2(extended_ground_ppu, 0, 0,
        extended_ground_frame, starfox::render::TilePriorityPass::all, 72, true);
    require(retail_ground_frame.get(0, 223) == 42U
                && extended_ground_frame.get(72, 223) == 1U,
            "expanded Mode 2 scene exposed colour zero below the ground");
    auto high_ground_ppu = extended_ground_ppu;
    for (std::size_t row = 0; row < 18U; ++row) {
        for (std::size_t column = 0; column < 32U; ++column) {
            const auto entry = row * 32U + column;
            high_ground_ppu.vram[mode2_map + entry * 2U + 1U] = 0x20U;
        }
    }
    starfox::render::Framebuffer high_ground_frame{400, 224};
    high_ground_frame.clear(42U);
    background_renderer.draw_bg2(high_ground_ppu, 0, 0,
        high_ground_frame, starfox::render::TilePriorityPass::low, 72, true);
    background_renderer.draw_bg2(high_ground_ppu, 0, 0,
        high_ground_frame, starfox::render::TilePriorityPass::high, 72, true);
    require(high_ground_frame.get(72, 223) == 1U,
            "high-priority wide ground stopped before the bottom strips");

    auto opaque_wrap_ppu = extended_ground_ppu;
    for (std::size_t index = 0; index < 32U; ++index) {
        opaque_wrap_ppu.vram[mode2_offset + index * 2U] = 100U;
        opaque_wrap_ppu.vram[mode2_offset + index * 2U + 1U] = 0x40U;
    }
    std::fill_n(opaque_wrap_ppu.vram.begin() + mode2_characters + 32U,
        64U, 0U);
    for (std::size_t row = 0; row < 8U; ++row) {
        opaque_wrap_ppu.vram[mode2_characters + 32U + row * 2U] = 0xffU;
        opaque_wrap_ppu.vram[mode2_characters + 64U + row * 2U] = 0xffU;
        opaque_wrap_ppu.vram[mode2_characters + 64U + row * 2U + 1U] = 0xffU;
    }
    for (std::size_t row = 0; row < 32U; ++row) {
        for (std::size_t column = 0; column < 32U; ++column) {
            const auto entry = row * 32U + column;
            opaque_wrap_ppu.vram[mode2_map + entry * 2U]
                = row < 13U ? 2U : 1U;
            opaque_wrap_ppu.vram[mode2_map + entry * 2U + 1U] = 0U;
        }
    }
    starfox::render::Framebuffer opaque_retail_frame{256U, 224U};
    background_renderer.draw_bg2(opaque_wrap_ppu, 0, 0,
        opaque_retail_frame, starfox::render::TilePriorityPass::all, 0, true);
    require(opaque_retail_frame.get(128, 223) == 3U,
            "opaque-wrap fixture did not reach its sky tile");
    for (const auto width : {360U, 400U, 520U, 800U}) {
        starfox::render::Framebuffer opaque_wide_frame{width, 224U};
        const auto origin = static_cast<std::int32_t>((width - 256U) / 2U);
        background_renderer.draw_bg2(opaque_wrap_ppu, 0, 0,
            opaque_wide_frame, starfox::render::TilePriorityPass::all,
            origin, true);
        require(opaque_wide_frame.get(0, 223) == 1U
                    && opaque_wide_frame.get(width / 2U, 223) == 1U
                    && opaque_wide_frame.get(width - 1U, 223) == 1U,
                "expanded ground wrapped back to opaque blue sky");
    }

    starfox::simulation::SnesPpuState empty_oam_ppu;
    empty_oam_ppu.main_screen = 0x10U;
    empty_oam_ppu.vram[0U] = 0x80U;
    // Route OAM can retain a packed large-size bit after its low record is
    // cleared. The low record remains the source empty sentinel.
    empty_oam_ppu.oam[512U] = 0x02U;
    starfox::render::Framebuffer empty_oam_frame{8, 8};
    empty_oam_frame.clear(42U);
    sprite_renderer.draw_objects(empty_oam_ppu, empty_oam_frame);
    require(empty_oam_frame.get(0, 0) == 42U,
            "zeroed unused OAM with stale size exposed OBJ tile zero");
    empty_oam_ppu.oam[0U] = 0xf8U;
    empty_oam_ppu.oam[1U] = 0xf8U;
    empty_oam_ppu.oam[2U] = 0U;
    empty_oam_ppu.oam[3U] = 0U;
    empty_oam_frame.clear(42U);
    sprite_renderer.draw_objects(empty_oam_ppu, empty_oam_frame);
    require(empty_oam_frame.get(0, 0) == 42U,
            "hidden route OBJ wrapped into the top-left map corner");

    starfox::simulation::SnesPpuState boss_label_ppu;
    boss_label_ppu.main_screen = 0x10U;
    boss_label_ppu.object_select = 0U;
    boss_label_ppu.oam[0U] = 100U;
    boss_label_ppu.oam[1U] = 16U;
    boss_label_ppu.oam[2U] = 0x71U;
    boss_label_ppu.vram[0x71U * 32U] = 0x80U;
    starfox::render::HudLayout boss_label_layout;
    boss_label_layout[starfox::render::HudElement::boss_health] = {-10, 3};
    starfox::render::Framebuffer boss_label_frame{400U, 224U};
    sprite_renderer.draw_objects(boss_label_ppu, boss_label_frame,
        std::nullopt, 72, true, true, &boss_label_layout);
    require(boss_label_frame.get(234, 19) == 129U
                && boss_label_frame.get(100, 16) == 0U,
            "ENEMY label did not remain grouped with the wide boss meter");

    starfox::simulation::SnesPpuState crosshair_ppu;
    crosshair_ppu.main_screen = 0x10U;
    crosshair_ppu.object_select = 0U;
    crosshair_ppu.oam[0U] = 100U;
    crosshair_ppu.oam[1U] = 180U;
    // VOBJBASE makes the source cartridge emit tile byte $e1 for logical
    // crosshair tile $61 in normal gameplay.
    crosshair_ppu.oam[2U] = 0xe1U;
    crosshair_ppu.vram[0xe1U * 32U] = 0x80U;
    starfox::render::HudLayout separated_hud;
    separated_hud[starfox::render::HudElement::shield] = {23, -9};
    starfox::render::Framebuffer crosshair_frame{400U, 224U};
    sprite_renderer.draw_objects(crosshair_ppu, crosshair_frame,
        std::nullopt, 72, true, true, &separated_hud);
    require(crosshair_frame.get(172, 180) == 129U
                && crosshair_frame.get(123, 171) == 0U
                && crosshair_frame.get(100, 180) == 0U,
            "cockpit crosshair fragment was mistaken for edge HUD artwork");

    // Crosshair OBJ slots can move when the source sprite list changes. Match
    // the complete four-quadrant group rather than assuming stable indices,
    // and interpolate it as one rigid reticle between 20 Hz source updates.
    const auto write_crosshair = [](auto& oam, std::size_t first,
                                     std::uint8_t x, std::uint8_t y) {
        constexpr std::array<std::uint8_t, 4> x_offsets{0U, 16U, 0U, 16U};
        constexpr std::array<std::uint8_t, 4> y_offsets{0U, 0U, 16U, 16U};
        constexpr std::array<std::uint8_t, 4> attributes{
            0x28U, 0x68U, 0xa8U, 0xe8U};
        for (std::size_t quadrant = 0U; quadrant < 4U; ++quadrant) {
            const auto low = (first + quadrant) * 4U;
            oam[low] = static_cast<std::uint8_t>(x + x_offsets[quadrant]);
            oam[low + 1U] = static_cast<std::uint8_t>(y + y_offsets[quadrant]);
            oam[low + 2U] = 0xe1U;
            oam[low + 3U] = attributes[quadrant];
        }
    };
    auto previous_crosshair_ppu =
        std::make_unique<starfox::simulation::SnesPpuState>();
    write_crosshair(previous_crosshair_ppu->oam, 4U, 100U, 70U);
    auto current_crosshair_ppu =
        std::make_unique<starfox::simulation::SnesPpuState>();
    write_crosshair(current_crosshair_ppu->oam, 20U, 112U, 76U);
    current_crosshair_ppu->oam[0U] = 37U;
    current_crosshair_ppu->oam[1U] = 41U;
    starfox::render::interpolate_crosshair_oam(
        previous_crosshair_ppu->oam, 0.5, *current_crosshair_ppu);
    require(current_crosshair_ppu->oam[20U * 4U] == 106U
                && current_crosshair_ppu->oam[20U * 4U + 1U] == 73U
                && current_crosshair_ppu->oam[21U * 4U] == 122U
                && current_crosshair_ppu->oam[22U * 4U + 1U] == 89U
                && current_crosshair_ppu->oam[0U] == 37U
                && current_crosshair_ppu->oam[1U] == 41U,
            "crosshair presentation interpolation jittered or moved other OAM");

    starfox::simulation::SnesPpuState mode3_ppu;
    mode3_ppu.background_mode = 3U;
    mode3_ppu.main_screen = 0x01U;
    mode3_ppu.bg1_character_base = 0x1000U;
    mode3_ppu.bg1_screen_base = 0x3000U;
    const auto mode3_map_byte = static_cast<std::size_t>(
        mode3_ppu.bg1_screen_base) * 2U;
    mode3_ppu.vram[mode3_map_byte] = 1U;
    const auto mode3_character_byte = static_cast<std::size_t>(
        mode3_ppu.bg1_character_base) * 2U + 64U;
    mode3_ppu.vram[mode3_character_byte] = 0x80U;
    mode3_ppu.vram[mode3_character_byte + 17U] = 0x80U;
    mode3_ppu.vram[mode3_character_byte + 32U] = 0x80U;
    mode3_ppu.vram[mode3_character_byte + 49U] = 0x80U;
    starfox::render::Framebuffer mode3_frame{8, 8};
    background_renderer.draw_bg1(mode3_ppu, mode3_frame);
    require(mode3_frame.get(0, 0) == 0x99U,
            "Mode 3 BG1 did not decode all eight SNES bitplanes");

    auto mode2_bg1_ppu = mode3_ppu;
    mode2_bg1_ppu.background_mode = 2U;
    mode2_bg1_ppu.vram.fill(0U);
    mode2_bg1_ppu.vram[mode3_map_byte] = 1U;
    mode2_bg1_ppu.vram[mode3_map_byte + 1U] = 0x18U;
    const auto mode2_bg1_character_byte = static_cast<std::size_t>(
        mode2_bg1_ppu.bg1_character_base) * 2U + 32U;
    mode2_bg1_ppu.vram[mode2_bg1_character_byte] = 0x80U;
    mode2_bg1_ppu.vram[mode2_bg1_character_byte + 1U] = 0x80U;
    mode2_bg1_ppu.vram[mode2_bg1_character_byte + 17U] = 0x80U;
    starfox::render::Framebuffer mode2_bg1_frame{8, 8};
    background_renderer.draw_bg1(mode2_bg1_ppu, mode2_bg1_frame);
    require(mode2_bg1_frame.get(0, 0) == 0x6bU,
            "Mode 2 BG1 did not decode its 4-bpp Super FX bitmap");
    const auto set_mode2_bg1_entry = [&mode2_bg1_ppu, mode3_map_byte](
                                         std::uint32_t tile_x) {
        const auto byte = mode3_map_byte + tile_x * 2U;
        mode2_bg1_ppu.vram[byte] = 1U;
        mode2_bg1_ppu.vram[byte + 1U] = 0x18U;
    };
    set_mode2_bg1_entry(2U);
    set_mode2_bg1_entry(29U);
    starfox::render::Framebuffer inset_bg1_frame{256U, 8U};
    inset_bg1_frame.clear(42U);
    background_renderer.draw_bg1(mode2_bg1_ppu, inset_bg1_frame,
        starfox::render::TilePriorityPass::all, 0, false, 16U);
    require(inset_bg1_frame.get(0, 0) == 42U
                && inset_bg1_frame.get(16, 0) == 0x6bU
                && inset_bg1_frame.get(232, 0) == 0x6bU
                && inset_bg1_frame.get(240, 0) == 42U,
            "centred BG1 inset did not clip both 16-pixel guard columns");
    auto mosaic_bg1_ppu = mode2_bg1_ppu;
    mosaic_bg1_ppu.mosaic = 0x11U; // 2x2, BG1 enabled.
    starfox::render::Framebuffer mosaic_bg1_frame{8, 8};
    background_renderer.draw_bg1(mosaic_bg1_ppu, mosaic_bg1_frame);
    require(mosaic_bg1_frame.get(0, 0) == 0x6bU
                && mosaic_bg1_frame.get(1, 0) == 0x6bU
                && mosaic_bg1_frame.get(0, 1) == 0x6bU
                && mosaic_bg1_frame.get(1, 1) == 0x6bU,
            "SNES BG1 mosaic did not repeat the source pixel as a 2x2 block");
    mosaic_bg1_ppu.mosaic = 0x12U; // Same size, BG2 only.
    mosaic_bg1_frame.clear(0U);
    background_renderer.draw_bg1(mosaic_bg1_ppu, mosaic_bg1_frame);
    require(mosaic_bg1_frame.get(0, 0) == 0x6bU
                && mosaic_bg1_frame.get(1, 0) == 0U,
            "SNES mosaic affected a background whose enable bit was clear");

    for (std::size_t object = 0; object < 128U; ++object) {
        priority_ppu.oam[object * 4U + 1U] = 224U;
    }
    priority_ppu.oam[1U] = 0U;
    priority_ppu.oam[2U] = 1U;
    priority_ppu.oam[3U] = 0x20U;
    priority_ppu.vram[0xc000U + 32U] = 0x80U;
    priority_frame.clear(42U);
    sprite_renderer.draw_objects(priority_ppu, priority_frame, 1U);
    require(priority_frame.get(0, 0) == 42U,
            "OBJ priority filter rendered a sprite in the wrong pass");
    sprite_renderer.draw_objects(priority_ppu, priority_frame, 2U);
    require(priority_frame.get(0, 0) == 129U,
            "OBJ priority filter omitted the selected source sprite");
    starfox::render::Framebuffer centred_sprite_frame{400, 8};
    centred_sprite_frame.clear(42U);
    sprite_renderer.draw_objects(
        priority_ppu, centred_sprite_frame, 2U, 72, false);
    require(centred_sprite_frame.get(72, 0) == 129U
                && centred_sprite_frame.get(0, 0) == 42U,
            "centred cartridge sprite escaped its widescreen canvas");
    centred_sprite_frame.clear(42U);
    sprite_renderer.draw_objects(
        priority_ppu, centred_sprite_frame, 2U, 72, true, true);
    require(centred_sprite_frame.get(0, 0) == 129U,
            "left-side gameplay HUD did not anchor to the expanded edge");
    priority_ppu.oam[0U] = 176U;
    centred_sprite_frame.clear(42U);
    sprite_renderer.draw_objects(
        priority_ppu, centred_sprite_frame, 2U, 72, true, true);
    require(centred_sprite_frame.get(320, 0) == 129U,
            "right-side gameplay HUD did not anchor to the expanded edge");
    priority_ppu.oam[0U] = 112U;
    priority_ppu.oam[1U] = 80U;
    centred_sprite_frame.resize(400, 192);
    centred_sprite_frame.clear(42U);
    sprite_renderer.draw_objects(
        priority_ppu, centred_sprite_frame, 2U, 72, true, true);
    require(centred_sprite_frame.get(184, 80) == 129U
                && centred_sprite_frame.get(112, 80) == 42U,
            "centre-band cockpit/comms OAM was pulled to a widescreen edge");
    priority_ppu.oam[0U] = 0U;
    priority_ppu.oam[1U] = 0U;
    shifted_hud[starfox::render::HudElement::lives] = {7, 5};
    centred_sprite_frame.clear(42U);
    sprite_renderer.draw_objects(priority_ppu, centred_sprite_frame,
        2U, 72, true, true, &shifted_hud);
    require(centred_sprite_frame.get(7, 5) == 129U
                && centred_sprite_frame.get(0, 0) == 42U,
            "custom HUD layout did not move the lives sprite group");
    // EX places its life counter in the same lower-left band as Shield. The
    // source life tiles must still follow the independent Lives offset.
    priority_ppu.oam[0U] = 27U;
    priority_ppu.oam[1U] = 175U;
    priority_ppu.oam[2U] = 189U;
    priority_ppu.vram[0xc000U + 189U * 32U] = 0x80U;
    shifted_hud[starfox::render::HudElement::shield] = {40, 20};
    centred_sprite_frame.resize(400, 224);
    centred_sprite_frame.clear(42U);
    sprite_renderer.draw_objects(priority_ppu, centred_sprite_frame,
        2U, 72, true, true, &shifted_hud);
    require(centred_sprite_frame.get(34, 180) == 129U
                && centred_sprite_frame.get(67, 195) == 42U,
            "EX lower-left lives were tied to the Shield layout");

    starfox::simulation::ObjectPool objects;
    const auto first = objects.allocate_after();
    const auto second = objects.allocate_after(first);
    const auto head = objects.allocate_after();
    require((objects.active_handles() == std::vector<starfox::simulation::ObjectHandle>{head, first, second}),
            "object insertion order differs from l_add");
    objects.at(second).attached = first;
    require(objects.remove(first), "active object could not be removed");
    require(objects.at(second).attached == 0, "object references were not divorced on removal");
    const auto reused = objects.allocate_after(head);
    require(reused == first, "freed object was not reused from the LIFO free-list head");
    require(objects.active_count() == 3, "object count is wrong after reuse");
    objects.write_path_word(second, 0x80, 0x1234);
    require(objects.read_path_word(second, 0x80) == 0x1234,
            "extended alien-block addressing is wrong");

    starfox::simulation::ObjectPool ex_objects{
        starfox::simulation::kOriginalMaximumObjects,
        starfox::simulation::ObjectMemoryLayout::starfox_ex};
    const auto ex_object = ex_objects.allocate_after();
    ex_objects.write_base_byte(ex_object, 44U, 7U);
    ex_objects.write_base_word(ex_object, 46U, 0x1234U);
    ex_objects.write_base_byte(ex_object, 53U, 5U);
    ex_objects.write_base_byte(ex_object, 54U, 9U);
    ex_objects.write_base_byte(ex_object, 56U, 0xaaU);
    require(ex_objects.at(ex_object).collision_count == 7U
                && std::bit_cast<std::uint16_t>(
                       ex_objects.at(ex_object).velocity_x) == 0x1234U
                && ex_objects.at(ex_object).weapon_type == 5U
                && ex_objects.at(ex_object).open_al == 9U
                && std::bit_cast<std::uint8_t>(
                       ex_objects.at(ex_object).scratch_bytes[5]) == 0xaaU,
            "Star Fox EX base alien-block layout is wrong");
    ex_objects.write_path_byte(ex_object, 0x9eU, 3U);
    ex_objects.write_path_byte(ex_object, 0x9fU, 4U);
    ex_objects.write_path_byte(ex_object, 0xa0U, 5U);
    ex_objects.write_path_byte(ex_object, 0xa1U, 6U);
    ex_objects.write_path_word(ex_object, 0xa2U, 0xbeefU);
    ex_objects.write_path_byte(ex_object, 0xacU, 7U);
    ex_objects.write_path_byte(ex_object, 0xadU, 8U);
    require(ex_objects.at(ex_object).colour_frame == 3U
                && ex_objects.at(ex_object).animation_frame == 4U
                && ex_objects.at(ex_object).sound1 == 5U
                && ex_objects.at(ex_object).sound2 == 6U
                && ex_objects.at(ex_object).colour_table == 0xbeefU
                && ex_objects.at(ex_object).texture_scroll_x == 7U
                && ex_objects.at(ex_object).texture_scroll_y == 8U,
            "Star Fox EX extended alien-block layout is wrong");

    std::vector<std::uint8_t> rom_bytes(0x20000);
    const auto to_offset = [](std::uint32_t address) {
        return static_cast<std::size_t>((address >> 16U) & 0x7fU) * 0x8000U
            + static_cast<std::size_t>(address & 0x7fffU);
    };
    const auto put8 = [&](std::uint32_t address, std::uint8_t value) {
        rom_bytes[to_offset(address)] = value;
    };
    const auto put16 = [&](std::uint32_t address, std::uint16_t value) {
        put8(address, static_cast<std::uint8_t>(value));
        put8(address + 1U, static_cast<std::uint8_t>(value >> 8U));
    };
    put16(0x018000 + 3U * 2U, 0x9000); // shape table entry 3
    put16(0x018100 + 4U * 4U, 0x9111); // strategy table entry 4
    put8(0x018100 + 4U * 4U + 2U, 0x02);
    put8(0x018100 + 4U * 4U + 3U, 3);
    auto map = std::uint32_t{0x018200};
    put8(map++, 0); put16(map, 0); map += 2;
    put16(map, 100); map += 2; put16(map, static_cast<std::uint16_t>(-20)); map += 2;
    put16(map, 300); map += 2; put8(map++, 3); put8(map++, 4);
    put8(map++, 50); put8(map++, 64);
    put8(map++, 18); put16(map, 10); map += 2;
    put8(map++, 0); put16(map, 5); map += 2;
    put16(map, 200); map += 2; put16(map, 0); map += 2; put16(map, 400); map += 2;
    put8(map++, 3); put8(map++, 4); put8(map++, 2);

    auto inline_map = std::uint32_t{0x018300};
    put8(inline_map++, 0); put16(inline_map, 0); inline_map += 2;
    put16(inline_map, 100); inline_map += 2; put16(inline_map, 200); inline_map += 2;
    put16(inline_map, 300); inline_map += 2; put8(inline_map++, 3); put8(inline_map++, 4);
    put8(inline_map++, 120);
    put8(inline_map++, 0xa9); put8(inline_map++, 0x7f); // LDA #$7f (A8)
    put8(inline_map++, 0x9d); put16(inline_map, 0x000e); inline_map += 2; // STA al_worldy,x
    put8(inline_map++, 0xc2); put8(inline_map++, 0x20); // REP #$20 (A16)
    put8(inline_map++, 0xac); put16(inline_map, 0x12af); inline_map += 2; // LDY alfreelst
    put8(inline_map++, 0xb9); put16(inline_map, 0x0000); inline_map += 2; // LDA _next,y
    put8(inline_map++, 0x8d); put16(inline_map, 0x12af); inline_map += 2; // STA alfreelst
    put8(inline_map++, 0xbd); put16(inline_map, 0x0000); inline_map += 2; // LDA _next,x
    put8(inline_map++, 0x99); put16(inline_map, 0x0000); inline_map += 2; // STA _next,y
    put8(inline_map++, 0x8a); // TXA
    put8(inline_map++, 0x99); put16(inline_map, 0x0002); inline_map += 2; // STA _prev,y
    put8(inline_map++, 0x98); // TYA
    put8(inline_map++, 0x9d); put16(inline_map, 0x0000); inline_map += 2; // STA _next,x
    put8(inline_map++, 0xa9); put16(inline_map, 0x7777); inline_map += 2;
    put8(inline_map++, 0x99); put16(inline_map, 0x000c); inline_map += 2; // STA al_worldx,y
    put8(inline_map++, 0xa2);
    const auto inline_return_operand = inline_map;
    put16(inline_map, 0); inline_map += 2;
    put8(inline_map++, 0x6b); // RTL
    put16(inline_return_operand, static_cast<std::uint16_t>(inline_map & 0x7fffU));
    put8(inline_map++, 122);
    put16(inline_map, 0x81ff); inline_map += 2; put8(inline_map++, 0x02); // JSL $028200
    put8(inline_map++, 2);

    put8(0x028200, 0xa9); put8(0x028201, 42); // LDA #42 (A8)
    put8(0x028202, 0x9d); put16(0x028203, 0x000c); // STA al_worldx,x
    put8(0x028205, 0x6b); // RTL

    put8(0x018380U, 20U); // SETBGM
    put8(0x018381U, 0x42U);
    put8(0x018382U, 2U); // END

    const starfox::assets::RomImage map_rom{rom_bytes};
    starfox::simulation::ObjectPool map_objects;
    const auto player = map_objects.allocate_after();
    starfox::simulation::MapVm map_vm{
        map_rom, starfox::simulation::MapDatabase{map_rom, 0x018000, 0x018100}, map_objects};
    map_vm.start(0x018200, player);
    map_vm.advance_distance(1);
    require(map_objects.active_count() == 2, "zero-distance map object did not spawn");
    require(map_objects.at(map_vm.last_spawned()).shape == 0x9000,
            "map shape lookup is wrong");
    require(map_objects.at(map_vm.last_spawned()).rotation_y == 64,
            "map rotation control did not modify the last object");
    require(map_vm.countdown() == 10, "map wait did not stop bytecode execution");
    map_vm.advance_distance(11);
    require(map_objects.active_count() == 3, "timed map object did not spawn");
    require(map_vm.countdown() == 5, "spawn distance was not loaded exactly");

    starfox::simulation::ObjectPool inline_objects;
    const auto inline_player = inline_objects.allocate_after();
    starfox::simulation::MapVm inline_vm{
        map_rom, starfox::simulation::MapDatabase{map_rom, 0x018000, 0x018100}, inline_objects};
    inline_vm.start(0x018300, inline_player);
    inline_vm.advance_distance(1);
    require(inline_vm.ended(), "inline 65C816 map stream did not return to bytecode");
    require(inline_objects.at(inline_vm.last_spawned()).world_y == 0x007f,
            "inline 65C816 code did not receive the original object pointer in X");
    require(inline_objects.at(inline_vm.last_spawned()).world_x == 42,
            "mapcode JSL did not synchronize object memory through WRAM");
    require(inline_objects.active_count() == 3 && inline_objects.at(3).world_x == 0x7777,
            "native 65C816 allocation did not synchronize the active/free lists");
    require(inline_vm.unsupported_controls().empty(),
            "native map code was still treated as a skipped boundary");

    starfox::simulation::ObjectPool music_objects;
    const auto music_player = music_objects.allocate_after();
    starfox::simulation::MapVm music_vm{
        map_rom,
        starfox::simulation::MapDatabase{map_rom, 0x018000, 0x018100},
        music_objects};
    music_vm.write_native_byte(0x001a49U, 2U);
    music_vm.start(0x018380U, music_player);
    music_vm.advance_distance(1);
    require(music_vm.ended() && music_vm.background_music() == 0x42U
                && music_vm.read_native_byte(0x001a4aU) == 0x42U
                && music_vm.read_native_byte(0x001a49U) == 0U,
            "SETBGM did not arm the native IRQ music handshake");
    music_vm.write_native_byte(0x001562U, 0x80U);
    music_vm.write_native_byte(0x001a49U, 2U);
    music_vm.write_native_byte(0x001a4aU, 0x17U);
    music_vm.start(0x018380U, music_player);
    music_vm.advance_distance(1);
    require(music_vm.read_native_byte(0x001a4aU) == 0x17U
                && music_vm.read_native_byte(0x001a49U) == 2U,
            "SETBGM ignored the source player-HP-zero guard");

    auto path_address = std::uint32_t{0x018400};
    put8(path_address++, 17); put16(path_address, 100); path_address += 2; put8(path_address++, 12);
    put8(path_address++, 138); put8(path_address++, 3);
    put8(path_address++, 162); put8(path_address++, 2);
    put8(path_address++, 87);
    put8(path_address++, 166);
    put8(0x018400 + 20U, 115); // P_PARTICLES
    put8(0x018400 + 21U, 166); // P_WAIT1
    auto global_path = std::uint32_t{0x018400 + 30U};
    put8(global_path++, 123); put8(global_path++, 0x80U);
    put16(global_path, 0x1234U); global_path += 2U; // IMPORTB
    put8(global_path++, 124); put8(global_path++, 0x82U);
    put16(global_path, 0x1236U); global_path += 2U; // IMPORTW
    put8(global_path++, 125); put8(global_path++, 0x80U);
    put16(global_path, 0x2234U); global_path += 2U; // EXPORTB
    put8(global_path++, 126); put8(global_path++, 0x82U);
    put16(global_path, 0x2236U); global_path += 2U; // EXPORTW
    put8(global_path++, 166); // P_WAIT1
    auto score_path = std::uint32_t{0x018400 + 50U};
    put8(score_path++, 167); // semantic P_SCORE
    put16(score_path, 0x1234U); score_path += 2U;
    put8(score_path++, 167);
    put16(score_path, 0x0102U); score_path += 2U;
    put8(score_path++, 166); // P_WAIT1
    const starfox::assets::RomImage path_rom{rom_bytes};
    starfox::simulation::ObjectPool path_objects;
    const auto path_player = path_objects.allocate_after();
    const auto path_actor = path_objects.allocate_after(path_player);
    starfox::simulation::OriginalPrng path_random;
    starfox::simulation::PathVm path_vm{
        path_rom, 0x018400, 0x029999, path_objects,
        starfox::simulation::TrigTables{}, path_random};
    path_vm.set_player(path_player);
    path_vm.attach(path_actor, 0);
    path_vm.tick(path_actor);
    require(path_objects.at(path_actor).world_x == 102,
            "PATH DO/NEXT first iteration is wrong");
    path_vm.tick(path_actor);
    path_vm.tick(path_actor);
    require(path_objects.at(path_actor).world_x == 106,
            "PATH DO/NEXT loop count is wrong");
    require(path_vm.path_offset(path_actor) == 10,
            "PATH wait1 did not yield at the correct byte boundary");

    const auto global_actor = path_objects.allocate_after(path_actor);
    starfox::simulation::PathVm global_path_vm{
        path_rom, 0x018400, 0x029999, path_objects,
        starfox::simulation::TrigTables{}, path_random};
    global_path_vm.set_player(path_player);
    global_path_vm.write_global_byte(0x1234U, 0x5aU);
    global_path_vm.write_global_word(0x1236U, 0xbeefU);
    global_path_vm.attach(global_actor, 30U);
    global_path_vm.tick(global_actor);
    require(path_objects.read_path_byte(global_actor, 0x80U) == 0x5aU
                && path_objects.read_path_word(global_actor, 0x82U) == 0xbeefU,
            "PATH IMPORTB/IMPORTW did not read shared bank-$7e state");
    require(global_path_vm.read_global_byte(0x2234U) == 0x5aU
                && global_path_vm.read_global_word(0x2236U) == 0xbeefU,
            "PATH EXPORTB/EXPORTW did not update shared bank-$7e state");
    require(global_path_vm.unsupported_opcodes().empty(),
            "PATH global import/export still crossed an unsupported boundary");

    const auto score_actor = path_objects.allocate_after(global_actor);
    starfox::simulation::PathVm score_path_vm{
        path_rom, 0x018400, 0x029999, path_objects,
        starfox::simulation::TrigTables{}, path_random};
    score_path_vm.set_player(path_player);
    score_path_vm.attach(score_actor, 50U);
    score_path_vm.tick(score_actor);
    require(score_path_vm.player_score() == 0x1336U,
            "Star Fox EX P_SCORE did not add its 16-bit operands exactly");

    const auto particle_source = path_objects.allocate_after(path_actor);
    path_objects.at(particle_source).world_x = 111;
    path_objects.at(particle_source).world_y = -222;
    path_objects.at(particle_source).world_z = 333;
    starfox::simulation::PathVm particle_path_vm{
        path_rom, 0x018400, 0x029999, path_objects,
        starfox::simulation::TrigTables{}, path_random, 0x06badfU, 0x9500U};
    particle_path_vm.set_player(path_player);
    particle_path_vm.attach(particle_source, 20U);
    particle_path_vm.tick(particle_source);
    const auto particle_object = path_objects.next_active(particle_source);
    require(particle_object != 0U
                && path_objects.at(particle_object).shape == 0x9500U
                && path_objects.at(particle_object).strategy_address == 0x06badfU,
            "P_PARTICLES did not create its source strategy object");
    require(path_objects.at(particle_object).world_x == 111
                && path_objects.at(particle_object).world_y == -222
                && path_objects.at(particle_object).world_z == 333,
            "P_PARTICLES did not copy the source position");

    starfox::simulation::ObjectPool particle_objects;
    const auto particle_owner = particle_objects.allocate_after();
    auto& particle_emitter = particle_objects.at(particle_owner);
    particle_emitter.strategy_flags[0] = 0x10U;
    particle_emitter.scratch_bytes[0] = 1;
    particle_emitter.scratch_bytes[1] = 10;
    particle_emitter.scratch_bytes[2] = 4;
    starfox::simulation::ParticleSystem particle_system{
        path_rom, 0x018600U, 0x018700U};
    particle_system.tick(particle_objects, true);
    require(particle_system.active_count() == 1U,
            "source particle pool did not allocate an emitter particle");
    const auto generated_particle = std::find_if(
        particle_system.particles().begin(), particle_system.particles().end(),
        [](const auto& particle) { return particle.life != 0U; });
    require(generated_particle != particle_system.particles().end()
                && generated_particle->owner == particle_owner
                && generated_particle->life == 3U,
            "source particle lifetime/update order is wrong");
    const auto first_particle_velocity = std::array{
        generated_particle->velocity_x,
        generated_particle->velocity_y,
        generated_particle->velocity_z};
    particle_system.reset();
    require(particle_system.active_count() == 0U,
            "particle reset did not clear the source Super FX pool");
    particle_system.tick(particle_objects, true);
    const auto reset_particle = std::find_if(
        particle_system.particles().begin(), particle_system.particles().end(),
        [](const auto& particle) { return particle.life != 0U; });
    require(reset_particle != particle_system.particles().end()
                && std::array{reset_particle->velocity_x,
                       reset_particle->velocity_y,
                       reset_particle->velocity_z} == first_particle_velocity,
            "particle reset did not restore the INITGAME3D random seed");
    particle_emitter.scratch_bytes[2] = 0;
    particle_system.tick(particle_objects, true);
    require(particle_system.active_count() == 1U,
            "particle owner did not keep its existing pool entries alive");
    require(particle_objects.remove(particle_owner),
            "particle test owner could not be removed");
    particle_system.tick(particle_objects, true);
    require(particle_system.active_count() == 0U,
            "orphaned source particles were not retired");

    std::vector<std::uint8_t> cpu_rom_bytes(0x8000U);
    cpu_rom_bytes[0] = 0xa9; cpu_rom_bytes[1] = 0x34; cpu_rom_bytes[2] = 0x12; // LDA #$1234
    cpu_rom_bytes[3] = 0x69; cpu_rom_bytes[4] = 0x01; cpu_rom_bytes[5] = 0x00; // ADC #1
    cpu_rom_bytes[6] = 0x6b; // RTL
    cpu_rom_bytes[0x10] = 0xa9; cpu_rom_bytes[0x11] = 0x78;
    cpu_rom_bytes[0x12] = 0x56; // LDA #$5678
    cpu_rom_bytes[0x13] = 0x60; // RTS
    cpu_rom_bytes[0x20] = 0xee; cpu_rom_bytes[0x21] = 0x00;
    cpu_rom_bytes[0x22] = 0x10; // INC $1000
    cpu_rom_bytes[0x23] = 0x80; cpu_rom_bytes[0x24] = 0xfb; // BRA $8020
    const starfox::assets::RomImage cpu_rom{std::move(cpu_rom_bytes)};
    starfox::simulation::Wdc65816 cpu{cpu_rom};
    cpu.write8(0x004218, 0x5a);
    require(cpu.read8(0x004218) == 0x5a,
            "native 65C816 I/O mirror did not preserve controller state");
    cpu.write8(0x002142, 0xa5);
    require((cpu.take_apu_port_writes()
                == std::vector<starfox::simulation::ApuPortWrite>{{2, 0xa5}}),
            "native 65C816 APU command writes were not captured");
    cpu.write8(0x004202, 0x7f);
    cpu.write8(0x004203, 0x81);
    require(cpu.read16(0x004216) == 0x3fff,
            "SNES 8-bit hardware multiplication is wrong");
    cpu.write16(0x004204, 50'000U);
    cpu.write8(0x004206, 37U);
    require(cpu.read16(0x004214) == 1'351U
                && cpu.read16(0x004216) == 13U,
            "SNES 16-by-8 hardware division is wrong");
    cpu.write8(0x004206, 0U);
    require(cpu.read16(0x004214) == 0xffffU
                && cpu.read16(0x004216) == 50'000U,
            "SNES divide-by-zero register results are wrong");
    cpu.write8(0x002181, 0xfeU);
    cpu.write8(0x002182, 0xffU);
    cpu.write8(0x002183, 1U);
    cpu.write8(0x002180, 0x31U);
    cpu.write8(0x002180, 0x42U);
    require(cpu.read8(0x7ffffeU) == 0x31U && cpu.read8(0x7fffffU) == 0x42U,
            "SNES WRAM data port did not write and increment across its boundary");
    auto cartridge_ram = std::array<std::uint8_t,
        starfox::simulation::Wdc65816::cartridge_ram_size>{};
    cartridge_ram[0xf000U] = 0x5aU;
    cartridge_ram[0xfffcU] = 'S';
    require(cpu.load_cartridge_ram(cartridge_ram)
                && cpu.read8(0x71f000U) == 0x5aU
                && cpu.read8(0x71fffcU) == 'S',
            "native 65C816 bridge did not map Star Fox EX cartridge RAM");
    cpu.write8(0x71f006U, 1U);
    require(cpu.cartridge_ram()[0xf006U] == 1U
                && !cpu.load_cartridge_ram(
                    std::span<const std::uint8_t>{cartridge_ram}.first(32U)),
            "native 65C816 cartridge RAM did not export or reject bad sizes");
    starfox::simulation::Wdc65816Registers cpu_registers;
    const auto cpu_instructions = cpu.call_long(0x008000, cpu_registers);
    require(cpu_registers.a == 0x1235 && cpu_instructions == 3,
            "native 65C816 bridge did not execute a 16-bit RTL routine exactly");
    starfox::simulation::Wdc65816Registers returned_task_registers;
    constexpr std::array<std::uint32_t, 1> unreachable_task_stop{0x008030U};
    const auto returned_task = cpu.begin_long_task(0x008000U,
        returned_task_registers, unreachable_task_stop);
    require(returned_task.returned && returned_task.stop_address == 0U
                && returned_task_registers.a == 0x1235U,
            "resumable 65C816 task did not report an ordinary RTL return");
    cpu.write16(0x001000U, 0U);
    starfox::simulation::Wdc65816Registers loop_task_registers;
    constexpr std::array<std::uint32_t, 1> loop_task_stop{0x008020U};
    const auto first_task_frame = cpu.begin_long_task(0x008020U,
        loop_task_registers, loop_task_stop);
    const auto second_task_frame = cpu.resume_task(
        loop_task_registers, loop_task_stop);
    require(!first_task_frame.returned
                && first_task_frame.stop_address == 0x008020U
                && !second_task_frame.returned
                && second_task_frame.stop_address == 0x008020U
                && cpu.read16(0x001000U) == 2U,
            "resumable 65C816 task did not preserve CPU state across yields");
    starfox::simulation::Wdc65816Registers near_registers;
    near_registers.status = 0x04U;
    const auto near_instructions = cpu.call_near(0x008010U, near_registers);
    require(near_registers.a == 0x5678U && near_instructions == 2U,
            "native 65C816 bridge did not execute a same-bank RTS routine exactly");

    if (argc == 3) {
        const auto upstream_rom = starfox::assets::RomImage::load(argv[1]);
        const auto upstream_symbols = starfox::assets::SymbolMap::load(argv[2]);
        const auto starfox_ex_cartridge =
            !upstream_symbols.find("PLANETSEQ2_L").empty();

        if (!starfox_ex_cartridge) {
            struct FrontendMsuCase {
                const char* entry;
                std::uint16_t track;
                bool repeat;
                const char* failure;
            };
            constexpr std::array frontend_msu_cases{
                FrontendMsuCase{"INTROMAP", 1U, false,
                    "intro did not start the MSU title-demonstration track"},
                FrontendMsuCase{"TITLEMAP", 2U, true,
                    "title map did not start its source MSU track"},
                FrontendMsuCase{"CONTMAP", 3U, true,
                    "control screen did not start the MSU controls track"},
                FrontendMsuCase{"PLANETSELECT", 5U, true,
                    "planet selector did not start the MSU map track"},
                FrontendMsuCase{"CONTINUE", 41U, true,
                    "continue screen did not start its MSU track"},
                FrontendMsuCase{"CREDITSMAP", 49U, false,
                    "credits did not start the MSU staff-roll track"},
            };
            for (const auto& test : frontend_msu_cases) {
                starfox::simulation::GameSimulation game{
                    upstream_rom, upstream_symbols, test.entry};
                auto writes = game.map().take_msu_register_writes();
                const auto deferred_frontend_track =
                    std::string_view{test.entry} == "CONTMAP"
                    || std::string_view{test.entry} == "PLANETSELECT";
                if (deferred_frontend_track) {
                    require(!started_msu_track(writes, test.track, test.repeat),
                        "MSU frontend track began during hidden scene setup");
                }
                std::optional<std::size_t> first_started_frame;
                // Background scripts can finish their request on a raster
                // boundary rather than during the wrapper constructor.
                for (std::size_t frame = 0U; frame < 120U; ++frame) {
                    game.present_frame();
                    if (game.logic_tick_ready()) {
                        static_cast<void>(game.tick({}));
                    }
                    auto frame_writes = game.map().take_msu_register_writes();
                    writes.insert(writes.end(),
                        frame_writes.begin(), frame_writes.end());
                    if (!first_started_frame
                        && started_msu_track(writes, test.track, test.repeat)) {
                        first_started_frame = frame;
                    }
                }
                require(started_msu_track(writes, test.track, test.repeat),
                    test.failure);
                if (std::string_view{test.entry} == "CONTMAP") {
                    require(first_started_frame && *first_started_frame >= 89U,
                        "MSU controls track began before CONTMAP reveal");
                }
                if (std::string_view{test.entry} == "PLANETSELECT") {
                    require(first_started_frame && *first_started_frame == 0U,
                        "MSU map track did not begin on its first fade raster");
                }
            }
        }

        const auto trig = starfox::simulation::TrigTables::load(
            upstream_rom, upstream_symbols);
        require(trig.sin8(0) == 0 && trig.sin8(64) == 127
                    && trig.sin8(128) == 0 && trig.sin8(192) == -127,
                "ROM 8-bit sine table quadrants are wrong");
        require(trig.cos8(0) == 127 && trig.cos8(64) == 0
                    && trig.cos8(128) == -127 && trig.cos8(192) == 0,
                "ROM 8-bit cosine table quadrants are wrong");
        require(trig.sin_q15(0x4000) == 32'767 && trig.cos_q15(0) == 32'767,
                "ROM Q15 trigonometry is wrong");

        if (!upstream_symbols.find("PLANETSEQ2_L").empty()) {
            const auto map2 = upstream_symbols.find("MAP2");
            const auto which_route = upstream_symbols.find("WHICHROUTE");
            const auto actual_route = upstream_symbols.find("ACTUALROUTE");
            const auto ship_angle = upstream_symbols.find("SHIPANGLE");
            require(!map2.empty() && !which_route.empty()
                        && !actual_route.empty() && !ship_angle.empty(),
                    "Star Fox EX planet campaign flags are missing");

            // MODE2SP is a separate IRQ request used when EX enters several
            // tunnels and special stages. It is Mode 2 with 16x16 BG2 tiles,
            // not an alias for the ordinary 8x8 Mode 2 setup.
            {
                starfox::simulation::Wdc65816 mode2sp_cpu{
                    upstream_rom, &upstream_symbols};
                starfox::simulation::Wdc65816Registers registers;
                registers.status = 0x24U;
                // BG_6_2_1 is a background task, not a callable routine: it
                // consumes inline records and eventually tail-dispatches
                // through the map scheduler instead of RTL. Exercise the
                // actual transfer request it uses so this check cannot run
                // off that task into an uninitialised scheduler continuation.
                mode2sp_cpu.call_near(
                    upstream_symbols.find("INITMODE2SP").front(), registers,
                    50'000'000U, true);
                require(mode2sp_cpu.ppu_state().background_mode == 2U
                            && mode2sp_cpu.ppu_state().bg2_tile_size_16,
                        "EX MODE2SP tunnel transfer did not enable 16x16 BG2");
            }

            // Exercise every EX-only horizontal-offset GSU entry directly.
            // Long stage traces reach these only on particular late routes,
            // so a dispatch regression should fail immediately here too.
            {
                starfox::simulation::Wdc65816 offsets_cpu{
                    upstream_rom, &upstream_symbols};
                const auto launch = [&](const char* symbol) {
                    const auto addresses = upstream_symbols.find(symbol);
                    require(!addresses.empty(),
                        "Star Fox EX horizontal-offset symbol is missing");
                    const auto address = addresses.front();
                    offsets_cpu.write8(0x003034U,
                        static_cast<std::uint8_t>(address >> 16U));
                    offsets_cpu.write8(0x00301eU,
                        static_cast<std::uint8_t>(address));
                    offsets_cpu.write8(0x00301fU,
                        static_cast<std::uint8_t>(address >> 8U));
                };
                const auto sine_offset =
                    upstream_symbols.find("M_SINEOFFSET").front();
                const auto scroll_buffer =
                    upstream_symbols.find("BG_SCROLLBUFFER").front();
                const auto black_hole_table =
                    upstream_symbols.find("BHOLETAB").front();
                const auto black_hole_table_end =
                    upstream_symbols.find("BHOLETABEND").front();
                offsets_cpu.write16(sine_offset, 0U);
                launch("MOSC");
                const auto final_phase = static_cast<std::uint16_t>(
                    black_hole_table_end - black_hole_table - 1U);
                const auto wobble = static_cast<std::int8_t>(
                    upstream_rom.read8(black_hole_table + final_phase));
                const auto expected_offset = static_cast<std::uint16_t>(
                    128 + static_cast<std::int16_t>(wobble));
                require(offsets_cpu.read16(sine_offset) == final_phase,
                    "MOSC did not wrap its source phase exactly");
                for (const auto line : {126U, 127U}) {
                    const auto record = scroll_buffer + line * 3U;
                    require(offsets_cpu.read8(record)
                                    == static_cast<std::uint8_t>(expected_offset)
                                && offsets_cpu.read16(record + 1U)
                                    == expected_offset,
                        "MOSC scanline record diverged from MHOFS.MC");
                }

                offsets_cpu.write16(sine_offset, 0U);
                for (const auto* symbol : {
                         "MWATER", "MNOISE", "MLACED", "MZIGZAG"}) {
                    launch(symbol);
                }
                require(offsets_cpu.unknown_superfx_launches().empty(),
                    "an EX horizontal-offset GSU entry remains untranslated");

                const auto bitmap = static_cast<std::uint16_t>(
                    upstream_symbols.find("BITMAP1").front());
                const auto bitmap_pixel = [&](std::int32_t x, std::int32_t y) {
                    const auto tile = static_cast<std::uint16_t>(
                        (x >> 3) * 24 + (y >> 3));
                    const auto row = static_cast<std::uint16_t>(y & 7);
                    const auto mask = static_cast<std::uint8_t>(
                        0x80U >> (x & 7));
                    const auto base = static_cast<std::uint16_t>(
                        bitmap + tile * 32U + row * 2U);
                    auto pixel = std::uint8_t{};
                    for (std::uint8_t plane = 0U; plane < 4U; ++plane) {
                        const auto address = static_cast<std::uint16_t>(base
                            + (plane >> 1U) * 16U + (plane & 1U));
                        if ((offsets_cpu.read8(0x700000U | address) & mask)
                            != 0U) {
                            pixel |= static_cast<std::uint8_t>(1U << plane);
                        }
                    }
                    return pixel;
                };
                offsets_cpu.write16(
                    upstream_symbols.find("M_X1").front(), 16U);
                offsets_cpu.write16(
                    upstream_symbols.find("M_Y1").front(), 24U);
                offsets_cpu.write16(
                    upstream_symbols.find("M_TXTDATA").front(),
                    static_cast<std::uint16_t>(
                        upstream_symbols.find("SCORETXT").front()));
                launch("MPRINTSTR");
                auto text_pixels = std::size_t{};
                for (std::int32_t y = 24; y < 36; ++y) {
                    for (std::int32_t x = 16; x < 80; ++x) {
                        text_pixels += bitmap_pixel(x, y) != 0U ? 1U : 0U;
                    }
                }
                require(text_pixels > 20U,
                    "MPRINTSTR did not draw source font pixels into BITMAP1");
                for (const auto* text_entry : {
                         "MPRINTCLIPPEDSTR", "MFPRINTSTR", "MSPRINTSTR"}) {
                    offsets_cpu.write16(
                        upstream_symbols.find("M_X1").front(), 16U);
                    offsets_cpu.write16(
                        upstream_symbols.find("M_Y1").front(), 40U);
                    offsets_cpu.write16(
                        upstream_symbols.find("M_TXTDATA").front(),
                        static_cast<std::uint16_t>(
                            upstream_symbols.find("SCORETXT").front()));
                    offsets_cpu.write16(
                        upstream_symbols.find("M_SPRX").front(), 80U);
                    launch(text_entry);
                }

                const auto m_x1 = upstream_symbols.find("M_X1").front();
                const auto m_y1 = upstream_symbols.find("M_Y1").front();
                const auto m_z1 = upstream_symbols.find("M_Z1").front();
                const auto m_xp2 = upstream_symbols.find("M_XP2").front();
                offsets_cpu.write16(m_x1, 3U);
                offsets_cpu.write16(m_y1, 4U);
                launch("MCALCPERC");
                require(offsets_cpu.read16(m_x1) == 75U,
                    "MCALCPERC did not calculate the source stage percentage");
                offsets_cpu.write16(m_x1, 50U);
                offsets_cpu.write16(m_y1, 10U);
                launch("MKRISDIVU3115");
                require(offsets_cpu.read16(m_x1) == 5U,
                    "MKRISDIVU3115 did not preserve the source quotient");

                for (std::uint32_t index = 0; index < 16U * 24U * 32U;
                     ++index) {
                    offsets_cpu.write8(0x700000U | static_cast<std::uint16_t>(
                        bitmap + index), 0xffU);
                }
                offsets_cpu.write8(0x700000U | static_cast<std::uint16_t>(
                    bitmap + 16U * 24U * 32U), 0x5aU);
                launch("MCLRPEPPERSCREEN");
                require(offsets_cpu.read8(0x700000U | bitmap) == 0U
                            && offsets_cpu.read8(0x700000U
                                | static_cast<std::uint16_t>(
                                    bitmap + 16U * 24U * 32U - 1U)) == 0U
                            && offsets_cpu.read8(0x700000U
                                | static_cast<std::uint16_t>(
                                    bitmap + 16U * 24U * 32U)) == 0x5aU,
                    "MCLRPEPPERSCREEN cleared outside its source bitmap span");

                offsets_cpu.write16(m_x1, 16U);
                offsets_cpu.write16(m_y1, 24U);
                offsets_cpu.write16(
                    upstream_symbols.find("M_TXTDATA").front(),
                    static_cast<std::uint16_t>(
                        upstream_symbols.find("SCORETXT").front()));
                offsets_cpu.write16(
                    upstream_symbols.find("M_TEXTRIGHTCLIP").front(), 80U);
                offsets_cpu.write16(
                    upstream_symbols.find("M_TEXTCOLOUR").front(), 5U);
                offsets_cpu.write16(
                    upstream_symbols.find("M_TOTALCHARS").front(), 3U);
                launch("MGPRINTSTR");
                require(upstream_symbols.find("M_LASTCHAR").empty()
                            || offsets_cpu.read16(
                                upstream_symbols.find("M_LASTCHAR").front())
                                != 0U,
                    "MGPRINTSTR did not retain its progressive-text cursor");

                offsets_cpu.write16(m_x1, 20U);
                offsets_cpu.write16(m_y1, 60U);
                offsets_cpu.write16(m_xp2, 40U);
                launch("MSHOWPERCGRAPH");
                require(bitmap_pixel(20, 60) == 14U
                            && bitmap_pixel(22, 62) == 7U
                            && bitmap_pixel(70, 62) == 0U,
                    "MSHOWPERCGRAPH did not reproduce its source geometry");
                offsets_cpu.write16(m_x1, 100U);
                offsets_cpu.write16(m_y1, 80U);
                offsets_cpu.write16(m_z1, 100U);
                launch("MPRTPERC");
                require(offsets_cpu.read16(m_x1) == 116U,
                    "MPRTPERC did not apply its source three-digit alignment");
                offsets_cpu.write16(m_x1, 10U);
                launch("MPRT2ZEROS");
                require(offsets_cpu.read16(m_x1) == 26U,
                    "MPRT2ZEROS did not advance by two fixed-width digits");
                launch("MALLROTZSORT");
                const auto wipe_pointer =
                    upstream_symbols.find("M_WINTABPTR").front();
                const auto wipe_logic =
                    upstream_symbols.find("M_WINWBGLOG").front();
                const auto wipe_left =
                    upstream_symbols.find("M_WINBUF").front();
                const auto wipe_right =
                    upstream_symbols.find("M_WINBUF2").front();
                offsets_cpu.write16(wipe_pointer, static_cast<std::uint16_t>(
                    upstream_symbols.find("MSCRAMWIPE").front()));
                launch("MBUMWIPE");
                require((offsets_cpu.read16(wipe_logic) & 0xffU) == 0xaaU
                            && offsets_cpu.read16(wipe_left) == 16U
                            && offsets_cpu.read16(wipe_right) == 239U,
                    "MBUMWIPE did not rasterize the source scanline window");
                for (std::size_t frame = 1U;
                     offsets_cpu.read16(wipe_pointer) != 1U && frame < 20U;
                     ++frame) {
                    launch("MBUMWIPE");
                }
                require(offsets_cpu.read16(wipe_pointer) == 1U,
                    "MBUMWIPE did not terminate at the source table sentinel");

                // EX occasionally presents MDECRUNCH with the bank written
                // one update before its 16-bit end pointer.  The SNES sees
                // open bus at $xx:0000; the native runtime must retain the
                // previous work buffer instead of terminating the process.
                offsets_cpu.write16(
                    upstream_symbols.find("M_ENDDATABNK").front(), 0x002dU);
                offsets_cpu.write16(
                    upstream_symbols.find("M_ENDDATA").front(), 0U);
                launch("MDECRUNCH");

                offsets_cpu.write16(
                    upstream_symbols.find("M_X1").front(), 11U);
                offsets_cpu.write16(
                    upstream_symbols.find("M_Y1").front(), 138U);
                offsets_cpu.write16(
                    upstream_symbols.find("M_Z1").front(), 40U);
                launch("MSHOWTEAMMATE");
                require(bitmap_pixel(11, 138) == 14U
                            && bitmap_pixel(13, 140) == 2U
                            && bitmap_pixel(54, 149) == 14U,
                    "MSHOWTEAMMATE did not reproduce its source meter geometry");
                offsets_cpu.write16(
                    upstream_symbols.find("M_X1").front(), 91U);
                offsets_cpu.write16(
                    upstream_symbols.find("M_Y1").front(), 138U);
                offsets_cpu.write16(
                    upstream_symbols.find("M_Z1").front(), 0U);
                launch("MSHOWTEAMMATE2");
                require(bitmap_pixel(91, 138) == 14U
                            && bitmap_pixel(93, 140) == 0U,
                    "MSHOWTEAMMATE2 did not preserve its dead meter outline");
                require(offsets_cpu.unknown_superfx_launches().empty(),
                    "an EX bitmap text/meter GSU entry remains untranslated");
            }

            // Exercise WORLD.ASM's complete EX message-control extension in
            // one deterministic map stream.  Controls 142/144/146 used to be
            // absent from the host dispatch and would terminate later EX
            // routes the first time one of these conversations was reached.
            {
                auto message_rom_bytes = upstream_rom.bytes();
                constexpr std::uint32_t message_map = 0x3fff00U;
                const auto message_map_offset =
                    upstream_rom.lorom_offset(message_map);
                constexpr std::array<std::uint8_t, 9> message_controls{
                    130U, 1U, 142U, 1U, 144U, 1U, 146U, 1U, 2U};
                std::copy(message_controls.begin(), message_controls.end(),
                    message_rom_bytes.begin() + message_map_offset);
                const starfox::assets::RomImage message_rom{
                    std::move(message_rom_bytes)};
                starfox::simulation::ObjectPool message_objects{
                    static_cast<std::size_t>(
                        upstream_symbols.find("NUMBER_AL").front()),
                    starfox::simulation::ObjectMemoryLayout::starfox_ex};
                const auto message_player = message_objects.allocate_after();
                starfox::simulation::MapVm message_map_vm{
                    message_rom,
                    starfox::simulation::MapDatabase{
                        message_rom, upstream_symbols},
                    message_objects, &upstream_symbols};
                message_map_vm.start(message_map, message_player);
                message_map_vm.advance_distance(1);
                require(message_map_vm.ended()
                            && message_map_vm.messages()
                                == std::vector<std::uint8_t>{1U, 1U, 1U, 1U}
                            && message_map_vm.read_native_byte(
                                upstream_symbols.find("MSG_COUNT1").front())
                                == 50U
                            && message_map_vm.read_native_byte(
                                upstream_symbols.find("MSG_COUNT12").front())
                                == 50U
                            && message_map_vm.unsupported_controls().empty(),
                    "EX map message controls did not execute their source queues");
            }

            starfox::simulation::GameSimulation ex_planets{
                upstream_rom, upstream_symbols, "PLANETSELECT"};
            require(ex_planets.map().read_native_byte(map2.front()) == 1U
                        && ex_planets.map().read_native_byte(
                            which_route.front()) == 4U
                        && ex_planets.map().read_native_byte(
                            ship_angle.front()) == 2U,
                    "Star Fox EX did not start on PLANETS2 route 4");
            for (std::size_t frame = 0U; frame < 8U; ++frame) {
                ex_planets.present_frame();
            }
            static_cast<void>(ex_planets.tick(
                {0U, starfox::input::right_shoulder, 0U}));
            require(ex_planets.map().read_native_byte(map2.front()) == 0U
                        && ex_planets.map().read_native_byte(
                            which_route.front()) == 1U
                        && ex_planets.map().read_native_byte(
                            ship_angle.front()) == 1U,
                    "Star Fox EX R switch did not enter PLANETS route 1");
            for (std::size_t frame = 0U; frame < 8U; ++frame) {
                ex_planets.present_frame();
            }
            static_cast<void>(ex_planets.tick(
                {0U, starfox::input::left_shoulder, 0U}));
            require(ex_planets.map().read_native_byte(map2.front()) == 1U
                        && ex_planets.map().read_native_byte(
                            which_route.front()) == 4U
                        && ex_planets.map().read_native_byte(
                            ship_angle.front()) == 2U,
                    "Star Fox EX L switch did not return to PLANETS2 route 4");
            for (std::size_t frame = 0U; frame < 8U; ++frame) {
                ex_planets.present_frame();
            }
            static_cast<void>(ex_planets.tick(
                {0U, starfox::input::up, 0U}));
            require(ex_planets.map().read_native_byte(which_route.front()) == 6U
                        && ex_planets.map().read_native_byte(
                            actual_route.front()) == 6U,
                    "PLANETS2 UP did not wrap to the previous route");
            static_cast<void>(ex_planets.tick(
                {0U, starfox::input::down, 0U}));
            require(ex_planets.map().read_native_byte(which_route.front()) == 4U
                        && ex_planets.map().read_native_byte(
                            actual_route.front()) == 4U,
                    "PLANETS2 DOWN did not advance to the next route");
            static_cast<void>(ex_planets.tick(
                {0U, starfox::input::select, 0U}));
            require(ex_planets.map().read_native_byte(which_route.front()) == 6U
                        && ex_planets.map().read_native_byte(
                            actual_route.front()) == 6U,
                    "PLANETS2 SELECT did not choose the previous route");

            starfox::simulation::GameSimulation first_map_routes{
                upstream_rom, upstream_symbols, "PLANETSELECT"};
            for (std::size_t frame = 0U; frame < 8U; ++frame) {
                first_map_routes.present_frame();
            }
            static_cast<void>(first_map_routes.tick(
                {0U, starfox::input::right_shoulder, 0U}));
            for (std::size_t frame = 0U; frame < 8U; ++frame) {
                first_map_routes.present_frame();
            }
            constexpr std::array<std::uint8_t, 4> first_map_order{
                1U, 2U, 3U, 0U,
            };
            for (std::size_t step = 0U; step < first_map_order.size(); ++step) {
                require(first_map_routes.map().read_native_byte(
                            which_route.front()) == first_map_order[step],
                        "PLANETS did not expose routes 0-3 in source order");
                if (step + 1U != first_map_order.size()) {
                    static_cast<void>(first_map_routes.tick(
                        {0U, starfox::input::right, 0U}));
                    require(first_map_routes.map().read_native_byte(
                                actual_route.front()) == first_map_order[step + 1U],
                            "PLANETS did not preserve ACTUALROUTE while cycling");
                }
            }

            for (std::uint8_t route_offset = 0U; route_offset < 3U;
                 ++route_offset) {
                starfox::simulation::GameSimulation route_game{
                    upstream_rom, upstream_symbols, "PLANETSELECT"};
                for (std::size_t frame = 0U; frame < 8U; ++frame) {
                    route_game.present_frame();
                }
                for (std::uint8_t step = 0U; step < route_offset; ++step) {
                    static_cast<void>(route_game.tick(
                        {0U, starfox::input::right, 0U}));
                }
                require(route_game.map().read_native_byte(which_route.front())
                            == static_cast<std::uint8_t>(4U + route_offset),
                        "PLANETS2 did not expose routes 4-6 in source order");
                if (route_offset != 0U) {
                    require(route_game.map().read_native_byte(
                                actual_route.front())
                                == static_cast<std::uint8_t>(4U + route_offset),
                            "PLANETS2 did not preserve ACTUALROUTE while cycling");
                }
                static_cast<void>(route_game.tick(
                    {0U, starfox::input::a, 0U}));
                require(route_game.flow_state()
                            == starfox::simulation::GameFlowState::planet_travel,
                        "PLANETS2 route confirmation did not begin ship travel");
            }

            // The shipped EX campaigns are PLANETS (routes 1-4) and
            // PLANETS2 (routes 5-7).  Boot every stage label reachable from
            // those two maps in one process so a newly encountered strategy,
            // relocated pointer, or native routine cannot remain hidden until
            // a long playthrough.  PLANETS3 is deliberately not represented:
            // it is the author's unused test map rather than shipped content.
            const auto shipped_stage_ticks = [] {
                const auto* value = std::getenv("STARFOX_EX_ROUTE_AUDIT_TICKS");
                return value == nullptr ? std::size_t{12U}
                    : std::clamp<std::size_t>(
                        std::strtoul(value, nullptr, 10), 12U, 2'000U);
            }();
            const starfox::assets::ShapeDecoder shipped_shape_decoder{
                upstream_rom, upstream_symbols};
            std::vector<std::uint32_t> decoded_shipped_shapes;
            const auto special_colour = static_cast<std::uint16_t>(
                upstream_symbols.find("ID_1_C").front());
            const auto red_colour = static_cast<std::uint16_t>(
                upstream_symbols.find("RED_C").front());
            const auto white_colour = static_cast<std::uint16_t>(
                upstream_symbols.find("WHITE_C").front());
            std::size_t shipped_ex_stages{};
            for (std::uint8_t route = 1U; route <= 7U; ++route) {
                for (std::uint8_t stage = 1U; stage <= 9U; ++stage) {
                    const auto stage_name = std::string{"LEVEL"}
                        + static_cast<char>('0' + route) + '_'
                        + static_cast<char>('0' + stage);
                    if (upstream_symbols.find(stage_name).empty()) continue;
                    try {
                        starfox::simulation::GameSimulation shipped_stage{
                            upstream_rom, upstream_symbols, stage_name};
                        // Long unattended traces otherwise stop at the first
                        // stage whose authored hazards eventually kill a
                        // motionless player (Macbeth is the earliest). Keep
                        // the player alive so this audit continues exercising
                        // late map records and EX-only strategies.
                        shipped_stage.set_god_mode(true);
                        for (std::size_t tick = 0U;
                             tick < shipped_stage_ticks; ++tick) {
                            shipped_stage.present_frame();
                            shipped_stage.present_frame();
                            shipped_stage.present_frame();
                            static_cast<void>(shipped_stage.tick({}));
                            for (const auto handle : shipped_stage.draw_order()) {
                                if (!shipped_stage.objects().is_active(handle)) {
                                    continue;
                                }
                                const auto& object =
                                    shipped_stage.objects().at(handle);
                                const auto flags = object.strategy_flags[0];
                                if (object.shape == 0U
                                    || (object.strategy_flags[3] & 0x08U) != 0U
                                    || (flags & 0x50U) != 0U) {
                                    continue;
                                }
                                auto colour_table = object.colour_table;
                                if (const auto override =
                                        shipped_stage.model_colour_table_override()) {
                                    colour_table = *override;
                                } else if ((flags & 0x02U) != 0U
                                           && (flags & 0x20U) == 0U) {
                                    colour_table = (flags & 0x01U) != 0U
                                        ? red_colour : white_colour;
                                } else if ((flags & 0x01U) != 0U) {
                                    colour_table = special_colour;
                                }
                                const auto key =
                                    (static_cast<std::uint32_t>(object.shape)
                                        << 16U) | colour_table;
                                if (std::find(decoded_shipped_shapes.begin(),
                                        decoded_shipped_shapes.end(), key)
                                    != decoded_shipped_shapes.end()) {
                                    continue;
                                }
                                try {
                                    static_cast<void>(shipped_shape_decoder.decode(
                                        object.shape, {}, colour_table));
                                } catch (const std::exception& error) {
                                    std::cerr << "Star Fox EX active model decode failed at "
                                              << stage_name << " shape=$" << std::hex
                                              << object.shape << " colour=$"
                                              << colour_table << std::dec << ": "
                                              << error.what() << '\n';
                                    throw;
                                }
                                decoded_shipped_shapes.push_back(key);
                            }
                        }
                        if (shipped_stage.flow_state()
                                != starfox::simulation::GameFlowState::gameplay
                            || shipped_stage.map().ended()
                            || !shipped_stage.objects().is_active(
                                shipped_stage.player())
                            || !shipped_stage.map().unknown_superfx_launches().empty()) {
                            throw std::runtime_error{
                                "stage did not reach stable gameplay"};
                        }
                    } catch (const std::exception& error) {
                        std::cerr << "Star Fox EX shipped-stage audit failed at "
                                  << stage_name << ": " << error.what() << '\n';
                        throw;
                    }
                    ++shipped_ex_stages;
                }
            }
            require(shipped_ex_stages == 40U,
                    "Star Fox EX shipped-stage audit did not cover all 40 route labels");
        }

        starfox::render::RenderSettings cockpit_settings;
        cockpit_settings.colour_index_base = 7U * 16U;
        const starfox::render::SoftwareRenderer cockpit_renderer{
            cockpit_settings};
        starfox::render::Framebuffer cockpit_hud_frame{256U, 192U};
        cockpit_renderer.draw_cockpit_hud(
            trig, 0U, 15U, 0U, 16, cockpit_hud_frame);
        require(cockpit_hud_frame.get(128U, 145U) == 7U * 16U + 15U
                    && cockpit_hud_frame.get(190U, 96U)
                        == 7U * 16U + 15U
                    && cockpit_hud_frame.get(66U, 96U)
                        == 7U * 16U + 15U,
                "source cockpit direction indicators were not reconstructed");
        starfox::render::Framebuffer damaged_cockpit_hud{256U, 192U};
        cockpit_renderer.draw_cockpit_hud(
            trig, 0U, 15U, 1U, 16, damaged_cockpit_hud);
        require(damaged_cockpit_hud.get(66U, 96U) == 7U * 16U + 2U
                    && damaged_cockpit_hud.get(190U, 96U)
                        == 7U * 16U + 15U,
                "cockpit HUD did not preserve broken-wing colour sides");
        starfox::render::Framebuffer coloured_cockpit_hud{256U, 192U};
        cockpit_renderer.draw_cockpit_hud(
            trig, 0U, 15U, 1U, 16, coloured_cockpit_hud, 207U);
        require(coloured_cockpit_hud.get(66U, 96U) == 7U * 16U + 2U
                    && coloured_cockpit_hud.get(190U, 96U) == 207U,
                "custom crosshair colour did not recolour intact cockpit triangles");

        const auto stp_char_end = upstream_symbols.find("BGSTPCCR");
        const auto stp_screen_end = upstream_symbols.find("BGSTPPCR");
        require(!stp_char_end.empty() && !stp_screen_end.empty(),
                "Corneria background archive symbols are missing");
        const auto stp_chars = starfox::assets::decrunch_reverse(
            upstream_rom, stp_char_end.front());
        const auto stp_screen = starfox::assets::decrunch_reverse(
            upstream_rom, stp_screen_end.front());
        require(stp_chars.bytes.size() == 5632U
                    && std::equal(stp_chars.bytes.begin(),
                        stp_chars.bytes.begin() + 16,
                        std::array<std::uint8_t, 16>{
                            0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00,
                            0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff}.begin()),
                "MDECRU-compatible Corneria character decode diverged");
        require(stp_screen.bytes.size() == 8192U
                    && std::all_of(stp_screen.bytes.begin(),
                        stp_screen.bytes.begin() + 16,
                        [index = std::size_t{0}](std::uint8_t value) mutable {
                            return value == ((index++ & 1U) == 0U ? 0x6aU : 0x14U);
                        }),
                "MDECRU-compatible Corneria tilemap decode diverged");

        {
            starfox::simulation::Wdc65816 planet_cpu{
                upstream_rom, &upstream_symbols};
            planet_cpu.write16(upstream_symbols.find("M_RADIUS").front(), 24U);
            // Venom's native PLANETS path can retain rotation scratch bits in
            // the high byte along with the bit-7 sphere marker. The GSU uses
            // only the low seven-bit texture number.
            planet_cpu.draw_planet_sphere(0x0838U);
        }
        {
            starfox::simulation::Wdc65816 rotate_cpu{
                upstream_rom, &upstream_symbols};
            const auto matrix = upstream_symbols.find("M_WMAT11").front();
            for (std::size_t index = 0; index < 9U; ++index) {
                rotate_cpu.write16(matrix + static_cast<std::uint32_t>(index * 2U),
                    index == 0U || index == 4U || index == 8U ? 0x7fffU : 0U);
            }
            rotate_cpu.write16(upstream_symbols.find("M_X1").front(), 320U);
            rotate_cpu.write16(upstream_symbols.find("M_Y1").front(), 160U);
            rotate_cpu.write16(upstream_symbols.find("M_Z1").front(), 80U);
            const auto rotate = upstream_symbols.find("MWMATROTP16").front();
            rotate_cpu.write8(0x003034U, static_cast<std::uint8_t>(rotate >> 16U));
            rotate_cpu.write8(0x00301eU, static_cast<std::uint8_t>(rotate));
            rotate_cpu.write8(0x00301fU, static_cast<std::uint8_t>(rotate >> 8U));
            require(rotate_cpu.read16(upstream_symbols.find("M_BIGX").front()) == 319U
                        && rotate_cpu.read16(
                               upstream_symbols.find("M_BIGY").front()) == 159U
                        && rotate_cpu.read16(
                               upstream_symbols.find("M_BIGZ").front()) == 79U,
                    "MWMATROTP16 did not rotate a point through the world matrix");
        }

        const auto map_addresses = upstream_symbols.find("MAP1_1A");
        require(!map_addresses.empty(), "MAP1_1A symbol is missing");
        starfox::simulation::ObjectPool upstream_objects{
            static_cast<std::size_t>(
                upstream_symbols.find("NUMBER_AL").front()),
            starfox_ex_cartridge
                ? starfox::simulation::ObjectMemoryLayout::starfox_ex
                : starfox::simulation::ObjectMemoryLayout::original};
        const auto upstream_player = upstream_objects.allocate_after();
        starfox::simulation::MapVm upstream_map{
            upstream_rom,
            starfox::simulation::MapDatabase{upstream_rom, upstream_symbols},
            upstream_objects,
            &upstream_symbols};
        upstream_map.start(map_addresses.front(), upstream_player);
        upstream_map.advance_distance(1);
        require(upstream_objects.active_count() > 1,
                "real Corneria map did not create its initial objects");
        require(upstream_map.countdown() > 0,
                "real Corneria map did not reach its first distance wait");
        require(upstream_map.unsupported_controls().empty(),
                "real Corneria map initialization used a boundary-only map control");
        starfox::simulation::NativeStrategyScheduler upstream_strategies{
            upstream_symbols, upstream_objects, upstream_map};
        const auto strategy_stats = upstream_strategies.tick_all();
        require(strategy_stats.objects_run > 1 && strategy_stats.instructions > 0,
                "real Corneria native strategies did not execute");

        // Field report captured this exact retail PATH_ISTRAT state before a
        // corrupted long return escaped into the mirrored $FF ROM bank. Keep
        // it as a direct native-dispatch fixture so the compatibility core
        // cannot regress into an instruction-limit crash here again.
        if (!starfox_ex_cartridge) {
            starfox::simulation::ObjectPool reported_crash_objects;
            starfox::simulation::ObjectHandle reported_object{};
            for (std::size_t index = 0; index < 7U; ++index) {
                reported_object = reported_crash_objects.allocate_after();
            }
            auto& reported_state = reported_crash_objects.at(reported_object);
            reported_state.shape = 0xcb07U;
            reported_state.strategy_address = 0x04805bU;
            reported_state.scratch_words[1] = 0x0594;
            reported_state.rotation_y = 20U;
            reported_state.strategy_flags = {0x0bU, 0x10U, 0x08U, 0x00U};
            reported_state.extended[48] = 0xdaU;
            reported_state.extended[49] = 0x4fU;
            starfox::simulation::MapVm reported_crash_map{
                upstream_rom,
                starfox::simulation::MapDatabase{upstream_rom, upstream_symbols},
                reported_crash_objects,
                &upstream_symbols};
            constexpr std::array<std::uint8_t, 16> reported_heap{
                0x03U, 0x04U, 0x0eU, 0x05U, 0x09U, 0x90U, 0x05U, 0x0cU,
                0x97U, 0x05U, 0x0cU, 0x00U, 0x00U, 0x00U, 0x01U, 0x1fU};
            for (std::size_t index = 0; index < reported_heap.size(); ++index) {
                reported_crash_map.write_native_byte(
                    0x7ea12fU + 0x4fdaU + static_cast<std::uint32_t>(index),
                    reported_heap[index]);
            }
            starfox::simulation::NativeStrategyScheduler reported_crash_scheduler{
                upstream_symbols, reported_crash_objects,
                reported_crash_map, 1U};
            static_cast<void>(
                reported_crash_scheduler.tick_object(reported_object));
            require(!reported_crash_objects.is_active(reported_object),
                    "failed PATH_ISTRAT object was not recovered and removed");
        }

        const auto map1_1b = upstream_symbols.find("MAP1_1B");
        const auto boss_ptr = upstream_symbols.find("BOSS_PTR");
        require(!map1_1b.empty() && !boss_ptr.empty(),
                "boss-map CPU integration symbols are missing");
        starfox::simulation::ObjectPool boss_objects{
            static_cast<std::size_t>(
                upstream_symbols.find("NUMBER_AL").front()),
            starfox_ex_cartridge
                ? starfox::simulation::ObjectMemoryLayout::starfox_ex
                : starfox::simulation::ObjectMemoryLayout::original};
        const auto boss_player = boss_objects.allocate_after();
        starfox::simulation::MapVm boss_map{
            upstream_rom,
            starfox::simulation::MapDatabase{upstream_rom, upstream_symbols},
            boss_objects,
            &upstream_symbols};
        boss_map.set_unknown_condition_result(true);
        boss_map.start(map1_1b.front(), boss_player);
        for (std::size_t waits = 0; !boss_map.ended() && waits < 100; ++waits) {
            boss_map.advance_distance(static_cast<std::int16_t>(
                std::max<int>(1, boss_map.countdown() + 1)));
        }
        require(boss_map.ended() && boss_map.read_native_byte(boss_ptr.front()) == 2,
                "real markboss inline code did not update original WRAM state");
        require(boss_map.unsupported_controls().empty(),
                "real boss map still skipped native map controls");

        const auto walk_paths = upstream_symbols.find("PATH_E_WALK_1");
        require(!walk_paths.empty(), "PATH_E_WALK_1 symbol is missing");
        starfox::simulation::OriginalPrng upstream_random;
        starfox::simulation::PathVm upstream_paths{
            upstream_rom, upstream_symbols, upstream_objects, upstream_random};
        upstream_paths.set_player(upstream_player);
        const auto walker = upstream_objects.allocate_after(upstream_player);
        upstream_paths.attach(walker, static_cast<std::uint16_t>(walk_paths.front()));
        upstream_paths.tick(walker);
        require(upstream_objects.at(walker).rotation_y > 64,
                "real walking-enemy PATH did not enter its turn chase");
        require(!upstream_paths.events().empty(),
                "real walking-enemy PATH did not emit its positional sound");

        starfox::simulation::GameSimulation game{upstream_rom, upstream_symbols, "LEVEL1_1"};
        // This fixture audits the deterministic three-raster 20 Hz path.
        // The user-facing cold default is Original pace, so select the
        // unlocked deterministic mode explicitly instead of inheriting a UI
        // preference into low-level timing assertions.
        game.set_timing_mode(
            starfox::simulation::TimingMode::unlocked_20_fps);
        {
            // The desktop CLI's direct-level argument is a real source level
            // launch, not the low-level deterministic map-VM fixture used by
            // most tests in this file. It must pass through INITGAME_L so the
            // Mode 2 playfield and the sound-driver handshake are installed.
            starfox::simulation::GameSimulation direct_cli_game{
                upstream_rom, upstream_symbols, "LEVEL1_1", {}, true};
            require(direct_cli_game.map().ppu_state().background_mode == 2U
                        && (direct_cli_game.map().ppu_state().main_screen
                            & 0x02U) != 0U,
                    "direct-level CLI launch did not initialize the gameplay display mode");
            const auto first_cli_tick = direct_cli_game.tick({});
            require(!first_cli_tick.audio_port_writes.empty(),
                    "direct-level CLI launch did not initialize the source audio pipeline");
        }
        if (starfox_ex_cartridge) {
            // DARKMODE is source-save item 17 (two bytes per SAVEMEM entry).
            // It is restored before IRQSETMODE1 chooses the initial depth
            // thresholds, so a cold PC runtime must not overwrite that choice
            // with the retail normal table before its first transfer.
            auto dark_save = std::vector<std::uint8_t>{
                game.ex_save_ram().begin(), game.ex_save_ram().end()};
            dark_save[0xf000U + 17U * 2U] = 1U;
            starfox::simulation::GameSimulation dark_start_game{
                upstream_rom, upstream_symbols, "LEVEL1_1", dark_save};
            const auto depth_tables = upstream_symbols.find("DEPTHTABLES").front();
            const auto depth_pointer = upstream_symbols.find("M_DEPTHTABLE").front();
            const auto dark_mode = upstream_symbols.find("DARKMODE").front();
            require(dark_start_game.map().read_native_byte(dark_mode) == 1U
                        && dark_start_game.map().read_native_word(depth_pointer)
                            == static_cast<std::uint16_t>(depth_tables + 7U * 4U),
                    "EX DARK MODE save did not select its source depth table at boot");

            // SKIPSCRAMBLE is source-save item 14. LEVEL1_1 begins with the
            // hack's own scrambleskip map predicate, so loading this option
            // must branch the native map stream before the 100-tick dock
            // wait rather than relying on a host-side scene shortcut.
            auto skip_save = std::vector<std::uint8_t>{
                game.ex_save_ram().begin(), game.ex_save_ram().end()};
            skip_save[0xf000U + 14U * 2U] = 1U;
            starfox::simulation::GameSimulation skip_scramble_game{
                upstream_rom, upstream_symbols, "LEVEL1_1", skip_save};
            starfox::simulation::GameSimulation keep_scramble_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            const auto skip_scramble =
                upstream_symbols.find("SKIPSCRAMBLE").front();
            const auto map_pointer = upstream_symbols.find("MAPPTR").front();
            static_cast<void>(skip_scramble_game.tick({}));
            static_cast<void>(keep_scramble_game.tick({}));
            require(skip_scramble_game.map().read_native_byte(skip_scramble)
                            == 1U
                        && skip_scramble_game.map().read_native_word(map_pointer)
                            != keep_scramble_game.map().read_native_word(map_pointer)
                        && skip_scramble_game.map().unsupported_controls().empty(),
                    "EX SKIP SCRAMBLE save did not take its source map branch");

            starfox::simulation::GameSimulation meter_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            const auto address = [&upstream_symbols](const char* symbol) {
                const auto addresses = upstream_symbols.find(symbol);
                require(!addresses.empty(),
                    "Star Fox EX meter symbol is missing");
                return addresses.front();
            };
            meter_game.map().write_native_byte(address("M_DAMAGE"), 80U);
            meter_game.map().write_native_byte(address("M_BOOSTANIM"), 20U);
            meter_game.map().write_native_word(address("M_METERS"), 1U);
            meter_game.map().write_native_byte(address("M_BOSSHP"), 10U);
            meter_game.map().write_native_byte(address("M_BOSSMAXHP"), 20U);
            meter_game.map().write_native_byte(
                address("M_DOBOOSTMETER"), 0U);
            meter_game.map().write_native_word(
                address("M_PLAYERTWOACTIVATED"), 0x0101U);
            meter_game.map().write_native_byte(address("M_PLAYERTWO"), 0U);
            meter_game.map().write_native_word(
                address("M_PLAYERONEDEAD"), 0U);
            meter_game.map().write_native_byte(address("M_DAMAGETWO"), 60U);
            meter_game.map().write_native_byte(
                address("M_TWOEXTRABYTES"), 1U);
            meter_game.map().write_native_byte(address("M_PLAYERB_HP"), 104U);
            meter_game.map().write_native_byte(
                address("M_PLAYERB_HPACT"), 100U);
            const auto meters = meter_game.meter_state();
            require(meters.extended && !meters.boost_enabled
                        && meters.player_two_activated
                        && !meters.second_player_view
                        && !meters.player_one_dead && meters.damage == 80U
                        && meters.damage_two == 60U && meters.shield_up
                        && meters.shield_up_two
                        && meters.player_health_width == 104U
                        && meters.player_health_max == 100U,
                    "Star Fox EX meter state diverged from its Super FX variables");
        }
        {
            starfox::simulation::GameSimulation launch_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            const auto view_point = upstream_symbols.find("VIEWPT");
            const auto view_block = upstream_symbols.find("VIEWBLK");
            const auto exit_base_follow =
                upstream_symbols.find("PLAYEREXITBASEFOLLOW_STRAT");
            const auto player_on_planet =
                upstream_symbols.find("PLAYERONPLANET_STRAT");
            require(!view_point.empty() && !view_block.empty()
                        && !exit_base_follow.empty() && !player_on_planet.empty(),
                    "Corneria launch camera symbols are missing");
            bool saw_follow = false;
            bool saw_normal_after_follow = false;
            for (std::size_t tick = 0; tick < 360U; ++tick) {
                static_cast<void>(launch_game.tick({}));
                require(launch_game.map().read_native_word(view_point.front())
                            == static_cast<std::uint16_t>(view_block.front()),
                        "GETVIEW did not publish VIEWBLK through VIEWPT");
                const auto strategy =
                    launch_game.objects().at(launch_game.player()).strategy_address;
                saw_follow = saw_follow || strategy == exit_base_follow.front();
                saw_normal_after_follow = saw_normal_after_follow
                    || (saw_follow && strategy == player_on_planet.front());
            }
            require(saw_follow && saw_normal_after_follow,
                    "Corneria launch skipped its source camera-follow pullback");
        }
        {
            starfox::simulation::GameSimulation scramble_fade_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            auto saw_scramble_fade = false;
            for (std::size_t tick = 0; tick < 260U; ++tick) {
                static_cast<void>(scramble_fade_game.tick({}));
                if (scramble_fade_game.map().fade_direction() < 0) {
                    saw_scramble_fade = true;
                    require(scramble_fade_game.map().screen_enabled()
                                && scramble_fade_game.map().display_brightness() == 15U,
                            "scramble fade started from stale forced-black state");
                    scramble_fade_game.present_frame();
                    require(scramble_fade_game.map().display_brightness() == 14U,
                            "scramble fade did not advance through a visible step");
                    break;
                }
            }
            require(saw_scramble_fade,
                    "Corneria scramble never reached its source fade-down");
        }
        {
            starfox::simulation::GameSimulation first_person_game{
                upstream_rom, upstream_symbols, "LEVEL1_3"};
            const auto starfox_ex =
                !upstream_symbols.find("PLANETSEQ2_L").empty();
            const auto fly_mode = upstream_symbols.find("SPLAYERFLYMODE");
            const auto crosshair_x = upstream_symbols.find("ARSEBANDX");
            const auto crosshair_on = upstream_symbols.find("CROSSHAIRON");
            const auto null_player = upstream_symbols.find("NULLPLAYER");
            const auto arrows = upstream_symbols.find("ARROWS");
            const auto meters = upstream_symbols.find("M_METERS");
            const auto hud_rotation = upstream_symbols.find("HUDROT");
            require(!fly_mode.empty() && !crosshair_x.empty()
                        && !crosshair_on.empty() && !null_player.empty()
                        && !arrows.empty() && !meters.empty()
                        && !hud_rotation.empty(),
                    "first-person view symbols are missing");
            auto saw_transition = false;
            auto saw_inside = false;
            if (starfox_ex_cartridge) {
                // EX comments out the retail tunnel's automatic 2 -> 3
                // transition. Its global view control enters first person
                // directly on Select and keeps the selected ship shape while
                // setting the object's invisible strategy flag.
                for (std::size_t tick = 0; tick < 240U && !saw_inside; ++tick) {
                    const auto try_select = (tick % 6U) == 0U;
                    static_cast<void>(first_person_game.tick({
                        static_cast<starfox::input::ButtonMask>(
                            try_select ? starfox::input::select : 0U),
                        static_cast<starfox::input::ButtonMask>(
                            try_select ? starfox::input::select : 0U),
                        0U,
                    }));
                    saw_inside = first_person_game.map().read_native_byte(
                        fly_mode.front()) == 3U;
                }
                require(saw_inside
                            && first_person_game.objects().at(
                                first_person_game.player()).shape
                                != static_cast<std::uint16_t>(null_player.front())
                            && (first_person_game.objects().at(
                                    first_person_game.player()).strategy_flags[3]
                                & 0x08U) != 0U,
                        "Star Fox EX Select view did not enter its immediate "
                        "invisible-player cockpit mode");
                for (std::size_t tick = 0U; tick < 240U; ++tick) {
                    static_cast<void>(first_person_game.tick({
                        starfox::input::right,
                        static_cast<starfox::input::ButtonMask>(
                            tick == 0U ? starfox::input::right : 0U),
                        0U,
                    }));
                    if (first_person_game.objects().at(
                            first_person_game.player()).world_x >= 600) break;
                }
            } else {
                for (std::size_t tick = 0; tick < 240U; ++tick) {
                    const auto inside = first_person_game.map().read_native_byte(
                        fly_mode.front()) == 3U;
                    static_cast<void>(first_person_game.tick({starfox::input::right,
                        static_cast<starfox::input::ButtonMask>(
                            tick == 0U ? starfox::input::right : 0U),
                        0}));
                    const auto mode = first_person_game.map().read_native_byte(
                        fly_mode.front());
                    saw_transition = saw_transition || mode == 2U;
                    saw_inside = saw_inside || mode == 3U;
                    if (saw_inside && inside
                        && first_person_game.objects().at(
                            first_person_game.player()).world_x >= 600) {
                        break;
                    }
                }
                require(saw_transition && saw_inside
                            && first_person_game.objects().at(
                                first_person_game.player()).shape
                            == static_cast<std::uint16_t>(null_player.front()),
                        "cockpit entry did not complete its native transition");
            }
            require(static_cast<std::int16_t>(first_person_game.map().read_native_word(
                        crosshair_x.front())) != 0
                        && first_person_game.map().read_native_byte(
                            crosshair_on.front()) != 0U
                        && (first_person_game.map().read_native_word(
                            hud_rotation.front()) & 0x8000U) != 0U,
                    "first-person aim did not move the native crosshair");
            if (starfox_ex) {
                // EX's Ktunnel_pmovelimitAND deliberately omits the right
                // body-limit bit, so this segment must not synthesize the
                // retail right-edge arrow.
                require((first_person_game.map().read_native_byte(arrows.front())
                            & 8U) == 0U,
                        "Star Fox EX showed a retail-only K-tunnel right arrow");
            } else {
                require((first_person_game.map().read_native_byte(arrows.front())
                            & 8U) != 0U,
                        "cockpit right-bound indicator was not raised at its limit");
            }

            // Meters gate the original OAM reticle. Direct sub-map entry does
            // not execute the parent route's METERS_ON, so reproduce that
            // outer-map state before checking all four reticle quadrants.
            first_person_game.map().write_native_word(meters.front(), 1U);
            if (starfox_ex) {
                // EX defaults its three-state crosshair option to fully off;
                // explicitly enable it when verifying EX's source OAM shape.
                first_person_game.map().write_native_byte(
                    upstream_symbols.find("NOCROSSHAIRPLS").front(), 0U);
            }
            static_cast<void>(first_person_game.tick({}));
            const auto& cockpit_oam = first_person_game.map().ppu_state().oam;
            auto reticle_tiles = std::size_t{};
            const auto reticle_tile = static_cast<std::uint8_t>(0xe1U);
            for (std::size_t object = 0; object < 128U; ++object) {
                if (cockpit_oam[object * 4U + 2U] == reticle_tile) {
                    ++reticle_tiles;
                }
            }
            if (reticle_tiles < 4U) {
                std::array<std::size_t, 256> tile_counts{};
                for (std::size_t object = 0; object < 128U; ++object) {
                    ++tile_counts[cockpit_oam[object * 4U + 2U]];
                }
                std::cerr << "cockpit OAM tiles:";
                for (std::size_t tile = 0; tile < tile_counts.size(); ++tile) {
                    if (tile_counts[tile] != 0U) {
                        std::cerr << " $" << std::hex << tile << std::dec
                                  << 'x' << tile_counts[tile];
                    }
                }
                std::cerr << '\n';
            }
            require(reticle_tiles >= 4U,
                    "cockpit OAM omitted one or more reticle quadrants");

            if (starfox_ex) {
                static_cast<void>(first_person_game.tick({}));
                static_cast<void>(first_person_game.tick(
                    {starfox::input::select, starfox::input::select, 0U}));
                require(first_person_game.map().read_native_byte(
                            fly_mode.front()) == 0U
                            && (first_person_game.objects().at(
                                    first_person_game.player()).strategy_flags[3]
                                & 0x08U) == 0U
                            && (first_person_game.map().read_native_word(
                                hud_rotation.front()) & 0x8000U) == 0U,
                        "Star Fox EX Select view did not restore its external "
                        "player and HUD immediately");
            } else {
                static_cast<void>(first_person_game.tick(
                    {starfox::input::select, starfox::input::select, 0U}));
                static_cast<void>(first_person_game.tick(
                    {0U, 0U, starfox::input::select}));
                auto saw_exit_transition = false;
                for (std::size_t tick = 0; tick < 80U; ++tick) {
                    const auto mode = first_person_game.map().read_native_byte(
                        fly_mode.front());
                    saw_exit_transition = saw_exit_transition || mode == 4U;
                    if (mode == 0U) break;
                    static_cast<void>(first_person_game.tick({}));
                }
                require(saw_exit_transition
                            && first_person_game.map().read_native_byte(
                                fly_mode.front()) == 0U
                            && first_person_game.objects().at(
                                first_person_game.player()).shape
                                != static_cast<std::uint16_t>(null_player.front())
                            && (first_person_game.map().read_native_word(
                                hud_rotation.front()) & 0x8000U) == 0U,
                        "cockpit exit did not restore the external ship and HUD");
            }
        }
        if (starfox_ex_cartridge) {
            // EX adds three source-defined Select+direction camera presets in
            // GSTRATS.ASM. Exercise their actual strategy input path so the
            // desktop port cannot silently collapse them into the ordinary
            // Select cockpit toggle.
            const auto verify_iso_view = [&upstream_rom, &upstream_symbols](
                                             starfox::input::ButtonMask direction,
                                             std::int16_t expected_x,
                                             std::int16_t expected_y,
                                             std::uint16_t expected_distance,
                                             std::string_view name) {
                starfox::simulation::GameSimulation view_game{
                    upstream_rom, upstream_symbols, "LEVEL1_1"};
                const auto normal_strategy =
                    upstream_symbols.find("PLAYERONPLANET_STRAT").front();
                for (std::size_t tick = 0; tick < 480U
                     && view_game.objects().at(
                         view_game.player()).strategy_address
                         != normal_strategy; ++tick) {
                    static_cast<void>(view_game.tick({}));
                }
                require(view_game.objects().at(
                            view_game.player()).strategy_address
                            == normal_strategy,
                        "EX angled-view test never reached controllable flight");

                const auto combination = static_cast<
                    starfox::input::ButtonMask>(
                        direction | starfox::input::select);
                static_cast<void>(view_game.tick(
                    {combination, combination, 0U}));
                const auto iso_mode = upstream_symbols.find("ISOVIEWMODE").front();
                const auto cockpit_mode =
                    upstream_symbols.find("COCKPITMODE").front();
                const auto output_x = upstream_symbols.find("OUTVX").front();
                const auto output_y = upstream_symbols.find("OUTVY").front();
                const auto output_distance =
                    upstream_symbols.find("OUTDIST").front();
                const auto crosshair_on =
                    upstream_symbols.find("CROSSHAIRON").front();
                const auto entered_message = std::string{"EX Select+"}
                    + std::string{name}
                    + " did not enter its exact source camera preset";
                require(view_game.map().read_native_byte(iso_mode) == 1U
                            && view_game.map().read_native_byte(cockpit_mode) == 0U
                            && std::bit_cast<std::int16_t>(
                                view_game.map().read_native_word(output_x))
                                == expected_x
                            && std::bit_cast<std::int16_t>(
                                view_game.map().read_native_word(output_y))
                                == expected_y
                            && view_game.map().read_native_word(output_distance)
                                == expected_distance
                            && view_game.map().read_native_byte(crosshair_on) == 0U,
                        entered_message.c_str());

                // A plain Select edge exits any isometric preset through
                // CHANGEVIEWMODE_L and restores the normal external camera.
                static_cast<void>(view_game.tick({}));
                static_cast<void>(view_game.tick({
                    starfox::input::select, starfox::input::select, 0U}));
                const auto exited_message = std::string{"EX Select+"}
                    + std::string{name}
                    + " did not return to the normal source camera";
                require(view_game.map().read_native_byte(iso_mode) == 0U
                            && view_game.map().read_native_word(output_x) == 0U
                            && view_game.map().read_native_word(output_y) == 0U
                            && view_game.map().read_native_word(output_distance) == 120U
                            && view_game.map().read_native_byte(crosshair_on) != 0U,
                        exited_message.c_str());
            };
            verify_iso_view(starfox::input::left,
                static_cast<std::int16_t>(-32 * 256),
                static_cast<std::int16_t>(32 * 256), 3000U, "LEFT");
            verify_iso_view(starfox::input::right,
                static_cast<std::int16_t>(-32 * 256),
                static_cast<std::int16_t>(-32 * 256), 3000U, "RIGHT");
            verify_iso_view(starfox::input::up,
                static_cast<std::int16_t>(-16 * 256), 0, 2000U, "UP");

            // EX deliberately gives controller two a free camera while only
            // one player is active. Drive the exact GSTRATS.ASM inputs here;
            // this proves the desktop secondary-device latch reaches source
            // strategy code, not merely the exposed controller variables.
            starfox::simulation::GameSimulation camera_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            const auto normal_strategy =
                upstream_symbols.find("PLAYERONPLANET_STRAT").front();
            for (std::size_t tick = 0; tick < 480U
                 && camera_game.objects().at(
                     camera_game.player()).strategy_address
                     != normal_strategy; ++tick) {
                static_cast<void>(camera_game.tick({}));
            }
            require(camera_game.objects().at(
                        camera_game.player()).strategy_address
                            == normal_strategy,
                    "EX camera test never reached controllable Corneria flight");
            const auto output_x = upstream_symbols.find("OUTVX").front();
            const auto rotation_before = camera_game.map().read_native_word(
                output_x);
            std::array<starfox::input::TickInput, 4> secondary{};
            secondary[0] = {
                starfox::input::down, starfox::input::down, 0U};
            camera_game.set_secondary_inputs(secondary);
            static_cast<void>(camera_game.tick({}));
            require(camera_game.map().read_native_word(output_x)
                        == static_cast<std::uint16_t>(rotation_before + 512U),
                    "EX player-two DOWN did not rotate its source free camera");

            secondary = {};
            camera_game.set_secondary_inputs(secondary);
            static_cast<void>(camera_game.tick({}));
            constexpr auto reset_camera = static_cast<
                starfox::input::ButtonMask>(
                    starfox::input::left_shoulder
                    | starfox::input::right_shoulder
                    | starfox::input::right);
            secondary[0] = {
                reset_camera, starfox::input::right, 0U};
            camera_game.set_secondary_inputs(secondary);
            static_cast<void>(camera_game.tick({}));
            require(camera_game.map().read_native_word(output_x) == 0U,
                    "EX player-two L+R+RIGHT did not reset the source camera");

            // Drive EX's actual PSTRATS mouse branches, not just the native
            // packet registers. Positive X/Y must steer right/down and the
            // held left switch must enter DOFIREPLEASE_L without a joypad Y.
            starfox::simulation::GameSimulation mouse_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            starfox::simulation::GameSimulation mouse_control{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            for (std::size_t tick = 0; tick < 480U
                 && (mouse_game.objects().at(mouse_game.player()).strategy_address
                         != normal_strategy
                     || mouse_control.objects().at(
                            mouse_control.player()).strategy_address
                         != normal_strategy); ++tick) {
                static_cast<void>(mouse_game.tick({}));
                static_cast<void>(mouse_control.tick({}));
            }
            require(mouse_game.objects().at(mouse_game.player()).strategy_address
                            == normal_strategy
                        && mouse_control.objects().at(
                            mouse_control.player()).strategy_address
                            == normal_strategy,
                    "EX mouse test never reached controllable Corneria flight");
            const auto mouse_mode_address =
                upstream_symbols.find("MOUSEMODE").front();
            const auto mouse_mode_temp =
                upstream_symbols.find("MOUSEMODETEMP").front();
            const auto player_rotation_x =
                upstream_symbols.find("PLROTX").front();
            const auto player_rotation_y =
                upstream_symbols.find("PLROTY").front();
            const auto fire_delay =
                upstream_symbols.find("FIREDELAY").front();
            const auto fire_count =
                upstream_symbols.find("FIRECNT").front();
            for (auto* instance : {&mouse_game, &mouse_control}) {
                instance->map().write_native_byte(mouse_mode_address, 1U);
                instance->map().write_native_byte(mouse_mode_temp, 1U);
                instance->map().write_native_byte(fire_delay, 1U);
                instance->map().write_native_byte(fire_count, 1U);
            }
            const auto control_objects_before =
                mouse_control.objects().active_handles().size();
            mouse_game.set_mouse_input({6, 6, 0x01U});
            mouse_control.set_mouse_input({});
            static_cast<void>(mouse_game.tick({}));
            static_cast<void>(mouse_control.tick({}));
            require(std::bit_cast<std::int16_t>(mouse_game.map().read_native_word(
                            player_rotation_y))
                            < std::bit_cast<std::int16_t>(
                                mouse_control.map().read_native_word(
                                    player_rotation_y))
                        && std::bit_cast<std::int16_t>(
                            mouse_game.map().read_native_word(player_rotation_x))
                            > std::bit_cast<std::int16_t>(
                                mouse_control.map().read_native_word(
                                    player_rotation_x)),
                    "EX native mouse packet did not steer through PSTRATS");
            require(mouse_game.objects().active_handles().size()
                        > mouse_control.objects().active_handles().size()
                        && mouse_control.objects().active_handles().size()
                            >= control_objects_before,
                    "EX left mouse button did not fire through DOFIREPLEASE_L");
            const auto special_delay =
                upstream_symbols.find("SPECIALDELAY").front();
            const auto mouse_bombs =
                upstream_symbols.find("SPECWEPCNTONE").front();
            const auto nuke_shape = static_cast<std::uint16_t>(
                upstream_symbols.find("NUKE").front());
            const auto count_nukes = [&mouse_game, nuke_shape] {
                auto count = std::size_t{};
                for (const auto handle : mouse_game.objects().active_handles()) {
                    if (mouse_game.objects().at(handle).shape == nuke_shape) {
                        ++count;
                    }
                }
                return count;
            };
            const auto nukes_before = count_nukes();
            mouse_game.map().write_native_byte(mouse_mode_address, 1U);
            mouse_game.map().write_native_byte(special_delay, 1U);
            mouse_game.map().write_native_word(mouse_bombs, 5U);
            mouse_game.set_mouse_input({0, 0, 0x02U});
            static_cast<void>(mouse_game.tick({}));
            const auto nukes_after = count_nukes();
            require(nukes_after > nukes_before
                        && mouse_game.map().read_native_word(mouse_bombs) == 4U,
                    "EX right mouse button did not fire a native Nova Bomb");

            // Verify the active one-player command chords in the current EX
            // source. (The legacy helper-ship block is wrapped in IFEQ 1 and
            // therefore deliberately omitted by the assembler.)
            {
                starfox::simulation::GameSimulation skip_game{
                    upstream_rom, upstream_symbols, "LEVEL1_1"};
                for (std::size_t tick = 0; tick < 480U
                     && skip_game.objects().at(
                         skip_game.player()).strategy_address
                         != normal_strategy; ++tick) {
                    static_cast<void>(skip_game.tick({}));
                }
                require(skip_game.objects().at(
                            skip_game.player()).strategy_address
                            == normal_strategy,
                        "EX level-skip test never reached controllable flight");
                constexpr auto skip_chord = static_cast<
                    starfox::input::ButtonMask>(
                        starfox::input::left_shoulder
                        | starfox::input::right_shoulder
                        | starfox::input::select);
                static_cast<void>(skip_game.tick(
                    {skip_chord, starfox::input::select, 0U}));
                require(skip_game.flow_state()
                            == starfox::simulation::GameFlowState::stage_results,
                        "EX L+R+Select did not complete the level through its "
                        "source exit path");
            }
            {
                starfox::simulation::GameSimulation self_destruct_game{
                    upstream_rom, upstream_symbols, "LEVEL1_1"};
                for (std::size_t tick = 0; tick < 480U
                     && self_destruct_game.objects().at(
                         self_destruct_game.player()).strategy_address
                         != normal_strategy; ++tick) {
                    static_cast<void>(self_destruct_game.tick({}));
                }
                require(self_destruct_game.objects().at(
                            self_destruct_game.player()).strategy_address
                            == normal_strategy,
                        "EX self-destruct test never reached controllable flight");
                // The first PLAYERONPLANET_STRAT dispatch is its initializer
                // and installs the normal HP value. Let that source frame
                // complete before applying the kill chord.
                static_cast<void>(self_destruct_game.tick({}));
                self_destruct_game.map().write_native_byte(
                    upstream_symbols.find("WHICHROUTE").front(), 0U);
                constexpr auto self_destruct_chord = static_cast<
                    starfox::input::ButtonMask>(
                        starfox::input::left_shoulder
                        | starfox::input::a | starfox::input::start);
                for (std::size_t tick = 0; tick < 120U
                     && self_destruct_game.objects().at(
                         self_destruct_game.player()).health != 0U; ++tick) {
                    const auto try_kill = (tick % 6U) == 0U;
                    static_cast<void>(self_destruct_game.tick({
                        static_cast<starfox::input::ButtonMask>(
                            try_kill ? self_destruct_chord : 0U),
                        static_cast<starfox::input::ButtonMask>(
                            try_kill ? starfox::input::start : 0U),
                        0U,
                    }));
                    self_destruct_game.map().write_native_byte(
                        upstream_symbols.find("WHICHROUTE").front(), 0U);
                }
                require(self_destruct_game.objects().at(
                            self_destruct_game.player()).health == 0U,
                        "EX L+A+Start did not set the source player HP to zero");
            }
        }
        const auto palette_address = upstream_symbols.find("PALADDR");
        const auto controller_high_address = upstream_symbols.find("CONT0");
        const auto controller_low_address = upstream_symbols.find("CONTL0");
        require(!palette_address.empty() && !controller_high_address.empty()
                    && !controller_low_address.empty(),
                "runtime palette/input symbols are missing");
        const auto game_palette = game.palette_words();
        require(game_palette[1] == upstream_rom.read16(palette_address.front() + 2U),
                "game initialization did not load the original 3D palette");
        const auto dust_addresses = upstream_symbols.find("M_DUSTPNTS");
        require(!dust_addresses.empty(), "Super FX dust point symbol is missing");
        for (std::size_t point = 0; point < 8U; ++point) {
            const auto& native_point = game.dust().points()[point];
            require(game.map().read_native_word(dust_addresses.front()
                        + static_cast<std::uint32_t>(point * 6U))
                        == std::bit_cast<std::uint16_t>(native_point.x)
                    && game.map().read_native_word(dust_addresses.front()
                        + static_cast<std::uint32_t>(point * 6U + 2U))
                        == std::bit_cast<std::uint16_t>(native_point.y)
                    && game.map().read_native_word(dust_addresses.front()
                        + static_cast<std::uint32_t>(point * 6U + 4U))
                        == std::bit_cast<std::uint16_t>(native_point.z),
                    "native dust initialization diverged from MINITDUST");
        }
        const auto x1_addresses = upstream_symbols.find("X1");
        const auto y1_addresses = upstream_symbols.find("Y1");
        const auto arctangent_routines = upstream_symbols.find("ARCTAN16_L");
        require(!x1_addresses.empty() && !y1_addresses.empty()
                    && !arctangent_routines.empty(),
                "native arctangent bridge symbols are missing");
        const auto x1_address = *std::find_if(x1_addresses.begin(), x1_addresses.end(),
            [](std::uint32_t address) { return (address >> 16U) == 0U; });
        const auto y1_address = *std::find_if(y1_addresses.begin(), y1_addresses.end(),
            [](std::uint32_t address) { return (address >> 16U) == 0U; });
        const auto arctangent = *std::find_if(arctangent_routines.begin(),
            arctangent_routines.end(), [](std::uint32_t address) {
                return (address & 0xffffU) >= 0x8000U;
            });
        const auto check_angle = [&](std::int16_t x, std::int16_t y,
                                     std::uint16_t expected_angle) {
            game.map().write_native_word(x1_address,
                std::bit_cast<std::uint16_t>(x));
            game.map().write_native_word(y1_address,
                std::bit_cast<std::uint16_t>(y));
            starfox::simulation::Wdc65816Registers registers;
            registers.status = 0x24U;
            game.map().call_native_routine(arctangent, registers);
            return registers.a == expected_angle;
        };
        require(check_angle(0, 1, 0x0000U)
                    && check_angle(1, 0, 0x4000U)
                    && check_angle(1, 1, 0x2000U)
                    && check_angle(-1, 1, 0xe000U)
                    && check_angle(0, -1, 0x8000U),
                "Super FX arctangent bridge produced wrong quadrant angles");
        const auto matxw = upstream_symbols.find("MATXW");
        const auto matyw = upstream_symbols.find("MATYW");
        const auto matzw = upstream_symbols.find("MATZW");
        const auto mat11w = upstream_symbols.find("MAT11W");
        const auto crotmat16 = upstream_symbols.find("CROTMAT16_L");
        require(!matxw.empty() && !matyw.empty() && !matzw.empty()
                    && !mat11w.empty() && !crotmat16.empty(),
                "native world-matrix bridge symbols are missing");
        constexpr std::array<std::uint16_t, 3> matrix_angles{
            0x1234U, 0x4567U, 0x89abU};
        game.map().write_native_word(matxw.front(), matrix_angles[0]);
        game.map().write_native_word(matyw.front(), matrix_angles[1]);
        game.map().write_native_word(matzw.front(), matrix_angles[2]);
        starfox::simulation::Wdc65816Registers matrix_registers;
        matrix_registers.status = 0x24U;
        game.map().call_native_routine(crotmat16.front(), matrix_registers);
        const auto expected_matrix = starfox::simulation::rotation_matrix_q15(
            trig, std::bit_cast<std::int16_t>(matrix_angles[0]),
            std::bit_cast<std::int16_t>(matrix_angles[1]),
            std::bit_cast<std::int16_t>(matrix_angles[2]));
        for (std::size_t index = 0; index < expected_matrix.size(); ++index) {
            require(game.map().read_native_word(mat11w.front()
                        + static_cast<std::uint32_t>(index * 2U))
                        == std::bit_cast<std::uint16_t>(expected_matrix[index]),
                    "Super FX world-matrix bridge diverged from source Q15 math");
        }
        const auto m_vanishx = upstream_symbols.find("M_VANISHX");
        const auto m_vanishy = upstream_symbols.find("M_VANISHY");
        const auto m_xright = upstream_symbols.find("M_XRIGHT");
        const auto m_ybot = upstream_symbols.find("M_YBOT");
        require(!m_vanishx.empty() && !m_vanishy.empty()
                    && !m_xright.empty() && !m_ybot.empty()
                    && game.map().read_native_word(m_vanishx.front()) == 112U
                    && game.map().read_native_word(m_vanishy.front()) == 96U
                    && game.map().read_native_word(m_xright.front()) == 223U
                    && game.map().read_native_word(m_ybot.front()) == 191U,
                "original game viewport was not mirrored into Super FX state");
        const auto m_depthtable = upstream_symbols.find("M_DEPTHTABLE");
        const auto m_depthstab = upstream_symbols.find("M_DEPTHSTAB");
        const auto depthtables = upstream_symbols.find("DEPTHTABLES");
        require(!m_depthtable.empty() && !m_depthstab.empty()
                    && !depthtables.empty()
                    && game.map().read_native_word(m_depthtable.front())
                        == static_cast<std::uint16_t>(depthtables.front() + 16U),
                "normal IRQ depth thresholds were not initialized");
        constexpr auto test_input = static_cast<starfox::input::ButtonMask>(
            starfox::input::left | starfox::input::a);
        starfox::audio::Spc700Audio audio;
        auto first_tick = game.tick({test_input, test_input, 0});
        const auto strategy_frame_rate = upstream_symbols.find("FRAMERATE");
        require(!strategy_frame_rate.empty()
                    && game.map().read_native_byte(strategy_frame_rate.front()) == 3U,
                "20 Hz strategy timing did not retain three NTSC video phases");
        auto pcm = audio.render_logic_tick(first_tick.audio_port_writes);
        auto heard_audio = std::any_of(pcm.begin(), pcm.end(),
            [](std::int16_t sample) { return sample != 0; });
        require(game.map().read_native_byte(controller_high_address.front())
                        == static_cast<std::uint8_t>(test_input >> 8U)
                    && game.map().read_native_byte(controller_low_address.front())
                        == static_cast<std::uint8_t>(test_input),
                "latched native input did not reach original controller WRAM");
        std::size_t boot_audio_writes = 0;
        for (std::size_t tick = 1; tick < 6; ++tick) {
            const auto tick_result = game.tick({});
            boot_audio_writes += tick_result.audio_port_writes.size();
            pcm = audio.render_logic_tick(tick_result.audio_port_writes);
            heard_audio = heard_audio || std::any_of(pcm.begin(), pcm.end(),
                [](std::int16_t sample) { return sample != 0; });
        }
        starfox::render::RenderPose depth_pose;
        const auto depth_colour_pointer = game.map().read_native_word(
            m_depthstab.front());
        starfox::render::apply_source_depth_tables(upstream_rom,
            depthtables.front(),
            game.map().read_native_word(m_depthtable.front()),
            depth_colour_pointer, 0U, depth_pose);
        require(depth_pose.has_depth_colour_tables
                    && depth_pose.depth_colour_tables[0][0]
                        == upstream_rom.read8(
                            (depthtables.front() & 0xff0000U)
                            | depth_colour_pointer),
                "model depth colours were read from the wrong cartridge bank");
        const auto current_background = upstream_symbols.find("CURRENTBG");
        const auto player_opening = upstream_symbols.find("PLAYEROPENING_ISTRAT");
        require(!current_background.empty()
                    && game.map().read_native_word(current_background.front()) == 3,
                "transfer bridge did not run Corneria's original background request");
        require(game.map().display_brightness() == 15,
                "player-opening strategy did not drive the original quick fade-up");
        require(!player_opening.empty()
                    && (game.objects().at(game.player()).strategy_address >> 16U)
                        == (player_opening.front() >> 16U),
                "background info request did not install playeropening_Istrat");
        require(boot_audio_writes != 0,
                "Corneria background initialization did not execute the SPC upload protocol");
        require(audio.driver_loaded() && audio.uploaded_bytes() > 4'096U,
                "Corneria SPC700 driver/sample bank was not reconstructed from the upload");
        const auto& ppu = game.map().ppu_state();
        const auto populated_vram = std::count_if(ppu.vram.begin(), ppu.vram.end(),
            [](std::uint8_t value) { return value != 0U; });
        const auto minimum_populated_vram =
            upstream_symbols.find("PLANETSEQ2_L").empty() ? 2'000 : 1'500;
        require(populated_vram > minimum_populated_vram,
                "source background/OBJ DMA did not populate emulated VRAM");
        require(ppu.cgram[7U * 16U + 1U] == game.palette_words()[1],
                "3D game palette was not synchronized into CGRAM line 7");
        const auto ppu_palette = upstream_symbols.find("PAL0PALETTE").front();
        auto source_background_palette_is_exact = true;
        for (std::size_t colour = 0; colour < 8U * 16U; ++colour) {
            source_background_palette_is_exact =
                source_background_palette_is_exact
                && ppu.cgram[colour] == game.map().read_native_word(
                    ppu_palette + static_cast<std::uint32_t>(colour) * 2U);
        }
        require(source_background_palette_is_exact,
                "live PAL0PALETTE did not reach all eight PPU background rows");
        // Corneria deliberately suppresses the HUD during the opening fly-in.
        // Advance to the first stable gameplay presentation before checking the
        // original DO_SPRITES_L output rather than treating that suppression as
        // a failed OAM transfer.
        for (std::size_t tick = 6; tick < 360; ++tick) {
            const auto tick_result = game.tick({});
            pcm = audio.render_logic_tick(tick_result.audio_port_writes);
            heard_audio = heard_audio || std::any_of(pcm.begin(), pcm.end(),
                [](std::int16_t sample) { return sample != 0; });
        }
        if (!heard_audio) {
            const auto state = audio.state();
            std::cerr << "SPC state: pc=$" << std::hex << state.program_counter
                      << ", a=$" << static_cast<unsigned>(state.accumulator)
                      << ", x=$" << static_cast<unsigned>(state.x)
                      << ", y=$" << static_cast<unsigned>(state.y)
                      << ", psw=$" << static_cast<unsigned>(state.status)
                      << ", sp=$" << static_cast<unsigned>(state.stack)
                      << ", flg=$" << static_cast<unsigned>(state.dsp_flags)
                      << ", kon=$" << static_cast<unsigned>(state.dsp_key_on)
                      << std::dec << ", mvol=("
                      << static_cast<int>(state.main_volume_left) << ','
                      << static_cast<int>(state.main_volume_right) << ")\n";
        }
        require(heard_audio,
                "original SPC700 driver produced only silence during Corneria");
        require(std::any_of(ppu.oam.begin(), ppu.oam.begin() + 328U,
                    [](std::uint8_t value) { return value != 0U; }),
                "original HUD builder did not reach emulated OAM");
        const auto game_palette_selector = upstream_symbols.find("GAMEPAL");
        require(!game_palette_selector.empty()
                    && game.map().read_native_byte(
                           game_palette_selector.front()) == 2U
                    && game.palette_words()[1]
                        == upstream_rom.read16(palette_address.front() + 64U + 2U),
                "Corneria did not retain BGS.ASM's blue 3D palette");
        {
            starfox::simulation::GameSimulation red_palette_game{
                upstream_rom, upstream_symbols, "LEVEL1_6"};
            for (std::size_t tick = 0; tick < 8U; ++tick) {
                static_cast<void>(red_palette_game.tick({}));
            }
            require(red_palette_game.map().read_native_byte(
                        game_palette_selector.front()) == 1U
                        && red_palette_game.palette_words()[1]
                            == upstream_rom.read16(
                                palette_address.front() + 32U + 2U),
                    "Venom did not apply BGS.ASM's red 3D palette");
        }

        {
            starfox::simulation::GameSimulation god_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            god_game.set_god_mode(true);
            for (std::size_t tick = 0; tick < 360U; ++tick) {
                static_cast<void>(god_game.tick({}));
            }
            const auto ship_flags_3 =
                upstream_symbols.find("PSHIPFLAGS3").front();
            const auto bomb_count_symbols = upstream_symbols.find("SPECWEPCNT");
            const auto bomb_count = !bomb_count_symbols.empty()
                ? bomb_count_symbols.front()
                : upstream_symbols.find("SPECWEPCNTONE").front();
            const auto bomb_delay =
                upstream_symbols.find("SPECIALDELAY").front();
            const auto nuke_shape = static_cast<std::uint16_t>(
                upstream_symbols.find("NUKE").front());
            const auto null_shape = static_cast<std::uint16_t>(
                upstream_symbols.find("NULLSHAPE").front());
            const auto nuke_explosion =
                upstream_symbols.find("NUKEEXP_STRAT").front();
            god_game.map().write_native_word(bomb_count, 5U);
            god_game.map().write_native_byte(bomb_delay, 1U);
            static_cast<void>(god_game.tick(
                {starfox::input::a, starfox::input::a, 0}));
            require((god_game.map().read_native_byte(ship_flags_3) & 0x08U)
                        != 0U
                        && god_game.map().read_native_word(bomb_count) == 5U
                        && god_game.map().read_native_byte(bomb_delay) == 4U,
                    "God Mode did not disable collision and preserve regular bombs");

            auto regular_bomb = starfox::simulation::ObjectHandle{};
            for (const auto handle : god_game.objects().active_handles()) {
                if (god_game.objects().at(handle).shape == nuke_shape) {
                    regular_bomb = handle;
                    break;
                }
            }
            require(regular_bomb != 0U,
                    "God Mode did not fire a regular infinite bomb with A");
            static_cast<void>(god_game.objects().remove(regular_bomb));
            static_cast<void>(god_game.tick({}));
            god_game.map().write_native_byte(bomb_delay, 1U);
            static_cast<void>(god_game.tick({
                static_cast<starfox::input::ButtonMask>(
                    starfox::input::right_shoulder | starfox::input::a),
                starfox::input::a, 0}));

            auto armed_nuke = starfox::simulation::ObjectHandle{};
            for (const auto handle : god_game.objects().active_handles()) {
                if (god_game.objects().at(handle).shape == nuke_shape) {
                    armed_nuke = handle;
                    break;
                }
            }
            require(armed_nuke != 0U,
                    "holding R while pressing A did not fire a God Nuke");

            const auto target = god_game.objects().allocate_after(
                god_game.objects().active_handles().back());
            require(target != 0U, "God Nuke regression could not allocate a target");
            auto& target_object = god_game.objects().at(target);
            target_object.shape = static_cast<std::uint16_t>(
                upstream_symbols.find("ELASER2A").front());
            target_object.strategy_address =
                upstream_symbols.find("NULL_STRAT").front();
            target_object.health = 25U;
            target_object.collision_flags = 0x10U;
            auto& nuke_object = god_game.objects().at(armed_nuke);
            nuke_object.shape = null_shape;
            nuke_object.strategy_address = nuke_explosion;
            static_cast<void>(god_game.tick({}));
            require(god_game.objects().is_active(target)
                        && god_game.objects().at(target).health == 0U
                        && (god_game.objects().at(target).strategy_flags[1]
                            & 0x01U) != 0U,
                    "the R+A God Nuke did not kill its active object target");
        }

        {
            starfox::simulation::GameSimulation effect_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            starfox::simulation::GameSimulation control_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            starfox::audio::Spc700Audio effect_audio;
            starfox::audio::Spc700Audio control_audio;
            for (std::size_t tick = 0; tick < 360U; ++tick) {
                const auto effect_tick = effect_game.tick({});
                const auto control_tick = control_game.tick({});
                static_cast<void>(effect_audio.render_logic_tick(
                    effect_tick.audio_port_writes));
                static_cast<void>(control_audio.render_logic_tick(
                    control_tick.audio_port_writes));
                effect_game.synchronize_apu_output_ports(
                    effect_audio.output_ports());
                control_game.synchronize_apu_output_ports(
                    control_audio.output_ports());
            }
            // Retail Corneria has handed control to PLAYER_ISTRAT after this
            // direct-map warmup. EX deliberately extends/reworks the opening,
            // and direct diagnostic entry bypasses the planet initializer it
            // expects, so it is still running PLAYEROPENING_ISTRAT here. Queue
            // the same laser command through EX's source SETPORT3_L routine;
            // this keeps the SPC handshake regression independent of that
            // intentionally different cutscene timing.
            if (starfox_ex_cartridge) {
                const auto& set_port = upstream_symbols.find("SETPORT3_L");
                const auto& no_sfx = upstream_symbols.find("NOSFX");
                const auto& bgm_sfx = upstream_symbols.find("BGMSFX");
                const auto& no_set_port = upstream_symbols.find("NOSETPORT3");
                require(!set_port.empty() && !no_sfx.empty()
                            && !bgm_sfx.empty() && !no_set_port.empty(),
                    "Star Fox EX sound queue symbols are missing");
                const auto sound_read = upstream_symbols.find("SDGPT3").front();
                const auto sound_write = upstream_symbols.find("SDSPT3").front();
                const auto sound_pending = upstream_symbols.find("SDPCK3").front();
                effect_game.map().write_native_byte(sound_read,
                    effect_game.map().read_native_byte(sound_write));
                effect_game.map().write_native_byte(sound_pending, 0U);
                effect_game.map().write_native_byte(0x002143U, 0U);
                effect_game.map().write_native_byte(no_sfx.front(), 0U);
                effect_game.map().write_native_byte(bgm_sfx.front(), 0U);
                effect_game.map().write_native_byte(no_set_port.front(), 0U);
                starfox::simulation::Wdc65816Registers sound_registers;
                sound_registers.a = 0x35U;
                sound_registers.status = 0x24U;
                effect_game.map().call_native_routine(
                    set_port.front(), sound_registers, 5'000'000, true);
            }
            const auto fired_tick = effect_game.tick(starfox_ex_cartridge
                ? starfox::input::TickInput{}
                : starfox::input::TickInput{starfox::input::y,
                    starfox::input::y, 0});
            const auto idle_tick = control_game.tick({});
            const auto fired_pcm = effect_audio.render_logic_tick(
                fired_tick.audio_port_writes);
            const auto idle_pcm = control_audio.render_logic_tick(
                idle_tick.audio_port_writes);
            effect_game.synchronize_apu_output_ports(
                effect_audio.output_ports());
            control_game.synchronize_apu_output_ports(
                control_audio.output_ports());
            bool saw_laser_command = std::find(
                fired_tick.sound_effect_commands.begin(),
                fired_tick.sound_effect_commands.end(), 0x35U)
                != fired_tick.sound_effect_commands.end();
            bool saw_laser_acknowledgement =
                effect_audio.output_ports()[3] == 0x35U;
            bool heard_laser_difference = std::inner_product(
                fired_pcm.begin(), fired_pcm.end(), idle_pcm.begin(),
                std::uint64_t{}, std::plus<>{},
                [](std::int16_t left, std::int16_t right) {
                    return static_cast<std::uint64_t>(
                        std::abs(static_cast<int>(left)
                            - static_cast<int>(right)));
                }) != 0U;
            for (std::size_t tick = 0; tick < 50U; ++tick) {
                const auto effect_tick = effect_game.tick({});
                const auto control_tick = control_game.tick({});
                const auto pcm = effect_audio.render_logic_tick(
                    effect_tick.audio_port_writes);
                const auto control_pcm = control_audio.render_logic_tick(
                    control_tick.audio_port_writes);
                effect_game.synchronize_apu_output_ports(
                    effect_audio.output_ports());
                control_game.synchronize_apu_output_ports(
                    control_audio.output_ports());
                const auto difference = std::inner_product(
                    pcm.begin(), pcm.end(), control_pcm.begin(), std::uint64_t{},
                    std::plus<>{}, [](std::int16_t left, std::int16_t right) {
                        return static_cast<std::uint64_t>(
                            std::abs(static_cast<int>(left)
                                - static_cast<int>(right)));
                    });
                saw_laser_command = saw_laser_command
                    || std::find(effect_tick.sound_effect_commands.begin(),
                           effect_tick.sound_effect_commands.end(), 0x35U)
                        != effect_tick.sound_effect_commands.end();
                saw_laser_acknowledgement = saw_laser_acknowledgement
                    || effect_audio.output_ports()[3] == 0x35U;
                heard_laser_difference = heard_laser_difference
                    || difference != 0U;
            }
            if (!(saw_laser_command && saw_laser_acknowledgement
                    && heard_laser_difference)) {
                std::cerr << "diagnostic: laser command=" << saw_laser_command
                          << ", acknowledgement=" << saw_laser_acknowledgement
                          << ", audible difference=" << heard_laser_difference
                          << '\n';
            }
            require(saw_laser_command && saw_laser_acknowledgement
                        && heard_laser_difference,
                    "player laser did not traverse the source SPC acknowledgement path");

            starfox::simulation::GameSimulation comm_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            starfox::audio::Spc700Audio comm_audio;
            for (std::size_t tick = 0; tick < 360U; ++tick) {
                const auto comm_tick = comm_game.tick({});
                static_cast<void>(comm_audio.render_logic_tick(
                    comm_tick.audio_port_writes));
                comm_game.synchronize_apu_output_ports(
                    comm_audio.output_ports());
            }
            if (starfox_ex_cartridge) {
                const auto sound_read = upstream_symbols.find("SDGPT3").front();
                const auto sound_write = upstream_symbols.find("SDSPT3").front();
                comm_game.map().write_native_byte(sound_read,
                    comm_game.map().read_native_byte(sound_write));
                comm_game.map().write_native_byte(
                    upstream_symbols.find("SDPCK3").front(), 0U);
                comm_game.map().write_native_byte(0x002143U, 0U);
                comm_game.map().write_native_byte(
                    upstream_symbols.find("NOSFX").front(), 0U);
                comm_game.map().write_native_byte(
                    upstream_symbols.find("BGMSFX").front(), 0U);
                comm_game.map().write_native_byte(
                    upstream_symbols.find("NOSETPORT3").front(), 0U);
            }
            starfox::simulation::Wdc65816Registers comm_registers;
            // Message 36 is Rabbit's "incoming enemy craft" warning. Its
            // voice command ducks the SPC BGM until FRIENDS_MESSAGES closes
            // the portrait with command $64.
            comm_registers.a = 36U;
            comm_registers.status = 0x24U;
            comm_game.map().call_native_routine(
                upstream_symbols.find("SEND_MESSAGE_L").front(),
                comm_registers, 2'000'000, true);
            {
                // Advance the complete source portrait lifetime directly so
                // unrelated stage messages cannot replace this diagnostic
                // warning before its restore command is issued.
                const auto friends_messages =
                    upstream_symbols.find("FRIENDS_MESSAGES_L").front();
                for (std::size_t frame = 0; frame < 55U; ++frame) {
                    if (starfox_ex_cartridge) {
                    comm_game.map().write_native_byte(
                        upstream_symbols.find("NOSFX").front(), 0U);
                    comm_game.map().write_native_byte(
                        upstream_symbols.find("BGMSFX").front(), 0U);
                    comm_game.map().write_native_byte(
                        upstream_symbols.find("NOSETPORT3").front(), 0U);
                    }
                    starfox::simulation::Wdc65816Registers message_registers;
                    message_registers.status = 0x24U;
                    comm_game.map().call_native_routine(
                        friends_messages, message_registers, 5'000'000, true);
                }
            }
            bool saw_comm_command = false;
            bool saw_comm_acknowledgement = false;
            bool saw_comm_restore_command = false;
            bool saw_comm_restore_acknowledgement = false;
            for (std::size_t tick = 0; tick < 24U; ++tick) {
                const auto comm_tick = comm_game.tick({});
                static_cast<void>(comm_audio.render_logic_tick(
                    comm_tick.audio_port_writes));
                comm_game.synchronize_apu_output_ports(
                    comm_audio.output_ports());
                saw_comm_command = saw_comm_command
                    || std::find(comm_tick.sound_effect_commands.begin(),
                           comm_tick.sound_effect_commands.end(), 0x62U)
                        != comm_tick.sound_effect_commands.end();
                saw_comm_acknowledgement = saw_comm_acknowledgement
                    || comm_audio.output_ports()[3] == 0x62U;
                saw_comm_restore_command = saw_comm_restore_command
                    || std::find(comm_tick.sound_effect_commands.begin(),
                           comm_tick.sound_effect_commands.end(), 0x64U)
                        != comm_tick.sound_effect_commands.end();
                saw_comm_restore_acknowledgement =
                    saw_comm_restore_acknowledgement
                    || comm_audio.output_ports()[3] == 0x64U;
            }
            require(saw_comm_command && saw_comm_acknowledgement,
                    "teammate comm sample did not complete its SPC acknowledgement");
            if (!(saw_comm_restore_command
                    && saw_comm_restore_acknowledgement)) {
                std::cerr << "diagnostic: comm restore command="
                          << saw_comm_restore_command << ", acknowledgement="
                          << saw_comm_restore_acknowledgement << ", count1="
                          << static_cast<unsigned>(comm_game.map().read_native_byte(
                              upstream_symbols.find("MSG_COUNT1").front()))
                          << ", count2="
                          << static_cast<unsigned>(comm_game.map().read_native_byte(
                              upstream_symbols.find("MSG_COUNT2").front())) << '\n';
            }
            require(saw_comm_restore_command
                        && saw_comm_restore_acknowledgement,
                    "teammate comm close did not restore BGM volume through the SPC");
        }

        if (starfox_ex_cartridge) {
            const auto multitap_mode =
                upstream_symbols.find("MULTITAPMODE").front();
            const auto number_players =
                upstream_symbols.find("NUMPLAYERS").front();
            const auto controller_2_high =
                upstream_symbols.find("CONT1").front();
            const auto controller_2_low =
                upstream_symbols.find("CONTL1").front();
            const auto controller_2_trigger =
                upstream_symbols.find("TRIG1").front();
            const auto joypad_2 = upstream_symbols.find("JOY2L").front();
            const std::array<starfox::input::ButtonMask, 5> player_buttons{
                static_cast<starfox::input::ButtonMask>(
                    starfox::input::up | starfox::input::b),
                static_cast<starfox::input::ButtonMask>(
                    starfox::input::down | starfox::input::a),
                static_cast<starfox::input::ButtonMask>(
                    starfox::input::left | starfox::input::x),
                static_cast<starfox::input::ButtonMask>(
                    starfox::input::right | starfox::input::left_shoulder),
                static_cast<starfox::input::ButtonMask>(
                    starfox::input::y | starfox::input::right_shoulder),
            };
            std::array<starfox::input::TickInput, 4> secondary{};
            for (std::size_t player = 0; player < secondary.size(); ++player) {
                secondary[player] = {
                    player_buttons[player + 1U],
                    player_buttons[player + 1U],
                    0U,
                };
            }
            game.set_secondary_inputs(secondary);
            game.map().write_native_byte(multitap_mode, 1U);
            game.map().write_native_byte(number_players, 5U);
            static_cast<void>(game.tick({
                player_buttons[0], player_buttons[0], 0U}));
            require(game.map().read_native_byte(controller_2_high)
                            == static_cast<std::uint8_t>(player_buttons[1] >> 8U)
                        && game.map().read_native_byte(controller_2_low)
                            == static_cast<std::uint8_t>(player_buttons[1])
                        && game.map().read_native_word(controller_2_trigger)
                            == player_buttons[1]
                        && game.map().read_native_word(joypad_2)
                            == player_buttons[1],
                    "EX player-two input did not reach its native joypad state");
            for (std::size_t player = 0; player < player_buttons.size(); ++player) {
                const auto name = std::string{"CON"} + std::to_string(player + 1U);
                require(game.map().read_native_word(
                            upstream_symbols.find(name).front())
                                == player_buttons[player],
                        "EX distinct multitap controller input was not preserved");
            }
            require(game.map().read_native_byte(
                        upstream_symbols.find("LASTCONT1").front())
                            == static_cast<std::uint8_t>(player_buttons[1] >> 8U)
                        && game.map().read_native_byte(
                            upstream_symbols.find("LASTCONTL1").front())
                            == static_cast<std::uint8_t>(player_buttons[1])
                        && game.map().read_native_word(
                            upstream_symbols.find("LASTCON3").front())
                            == player_buttons[2]
                        && game.map().read_native_word(
                            upstream_symbols.find("LASTCON4").front())
                            == player_buttons[3]
                        && game.map().read_native_word(
                            upstream_symbols.find("LASTCON5").front())
                            == player_buttons[4],
                    "EX previous-frame multitap state did not follow TRANS.ASM");

            game.map().write_native_byte(number_players, 1U);
            static_cast<void>(game.tick({player_buttons[0], 0U, 0U}));
            for (std::size_t player = 0; player < player_buttons.size(); ++player) {
                const auto name = std::string{"CON"} + std::to_string(player + 1U);
                require(game.map().read_native_word(
                            upstream_symbols.find(name).front())
                                == player_buttons[0],
                        "EX one-controller multitap mode did not mirror player one");
            }
            game.map().write_native_byte(multitap_mode, 0U);
            game.set_secondary_inputs({});
            static_cast<void>(game.tick({}));
        }

        const auto game_frame_address = upstream_symbols.find("GAMEFRAME");
        require(!game_frame_address.empty(), "GAMEFRAME symbol is missing");
        static_cast<void>(game.tick({starfox::input::start,
            starfox::input::start, 0}));
        require(game.paused(), "eligible gameplay START did not pause the port");
        const auto paused_frame = game.map().read_native_word(
            game_frame_address.front());
        const auto paused_tick = game.tick({});
            require(game.map().read_native_word(game_frame_address.front()) == paused_frame
                    && paused_tick.strategies.objects_run == 0U,
                "paused gameplay advanced original strategy state");
        if (starfox_ex_cartridge) {
            const auto freeze_strategies =
                upstream_symbols.find("FREEZESTRATS").front();
            const auto menu_selected =
                upstream_symbols.find("MENUSELECTED").front();
            const auto single_double =
                upstream_symbols.find("SINGDOUB").front();
            require(game.map().read_native_byte(freeze_strategies) == 1U
                        && game.map().read_native_byte(menu_selected) == 0U,
                    "EX pause menu did not enter its source frozen state");
            starfox::render::Framebuffer pause_bitmap{256U, 192U};
            background_renderer.draw_bg1(game.map().ppu_state(), pause_bitmap,
                starfox::render::TilePriorityPass::all);
            require(std::count_if(pause_bitmap.pixels().begin(),
                        pause_bitmap.pixels().end(),
                        [](std::uint8_t pixel) { return pixel != 0U; }) > 100U,
                    "EX source pause menu did not reach its native BG1 bitmap");
            static_cast<void>(game.tick({starfox::input::down,
                starfox::input::down, 0}));
            require(game.map().read_native_byte(menu_selected) == 1U,
                    "EX pause menu DOWN did not select DOUBLE");
            const auto double_before =
                game.map().read_native_byte(single_double);
            static_cast<void>(game.tick({starfox::input::right,
                starfox::input::right, 0}));
            require(game.map().read_native_byte(single_double) != double_before,
                    "EX pause menu did not change DOUBLE through STRATDEBUG_L");
            require(game.map().unknown_superfx_launches().empty(),
                    "EX pause menu reached an untranslated Super FX launch");

            // MODEL is item 2. DEBUG.ASM changes CURR_SHIP and sets
            // FREEZESTRATS bit 2; TRANS.ASM then calls only SETSHIP, updating
            // PLAYPT's shape without advancing GAMEFRAME or any strategy.
            static_cast<void>(game.tick({starfox::input::down,
                starfox::input::down, 0U}));
            require(game.map().read_native_byte(menu_selected) == 2U,
                    "EX pause menu did not reach MODEL");
            const auto current_ship =
                upstream_symbols.find("CURR_SHIP").front();
            const auto ship_shapes =
                upstream_symbols.find("SHIPLISTSHAPE").front();
            const auto ship_before = game.map().read_native_byte(current_ship);
            const auto model_frame = game.map().read_native_word(
                game_frame_address.front());
            const auto model_tick = game.tick({starfox::input::right,
                starfox::input::right, 0U});
            const auto ship_after = game.map().read_native_byte(current_ship);
            require(game.paused() && ship_after != ship_before
                        && game.objects().at(game.player()).shape
                            == upstream_rom.read16(
                                ship_shapes + ship_after * 2U)
                        && game.map().read_native_word(game_frame_address.front())
                            == model_frame
                        && model_tick.strategies.objects_run == 0U
                        && game.map().read_native_byte(freeze_strategies) == 1U,
                    "EX paused MODEL change did not run DOSTRATS2 exactly");

            // DEBUG.ASM item 8 clears FREEZESTRATS for precisely the current
            // DOPAUSE transfer. That transfer must advance one strategy frame,
            // then return to the still-active menu with strategies frozen.
            for (std::uint8_t selection = 3U; selection <= 8U; ++selection) {
                static_cast<void>(game.tick({starfox::input::down,
                    starfox::input::down, 0U}));
            }
            require(game.map().read_native_byte(menu_selected) == 8U,
                    "EX pause menu did not reach STEP BY STEP");
            const auto before_step = game.map().read_native_word(
                game_frame_address.front());
            const auto stepped_tick = game.tick({starfox::input::right,
                starfox::input::right, 0U});
            require(game.paused()
                        && game.map().read_native_word(game_frame_address.front())
                            == static_cast<std::uint16_t>(before_step + 1U)
                        && stepped_tick.strategies.objects_run != 0U
                        && game.map().read_native_byte(freeze_strategies) == 1U,
                    "EX STEP BY STEP did not execute exactly one frozen transfer");
            const auto after_step = game.map().read_native_word(
                game_frame_address.front());
            const auto refrozen_tick = game.tick({});
            require(game.map().read_native_word(game_frame_address.front())
                            == after_step
                        && refrozen_tick.strategies.objects_run == 0U,
                    "EX STEP BY STEP did not refreeze after one transfer");
        } else {
            const auto before_direction = game.map().read_native_word(
                game_frame_address.front());
            const auto direction_tick = game.tick({starfox::input::right,
                starfox::input::right, 0U});
            require(game.paused()
                        && game.map().read_native_word(game_frame_address.front())
                            == before_direction
                        && direction_tick.strategies.objects_run == 0U,
                    "Original pause acquired EX step-by-step behavior");
        }
        static_cast<void>(game.tick({starfox::input::start,
            starfox::input::start, 0}));
        require(!game.paused(), "second START edge did not resume gameplay");
        if (starfox_ex_cartridge) {
            require(game.map().read_native_byte(
                        upstream_symbols.find("FREEZESTRATS").front()) == 0U,
                    "EX pause menu did not release its source strategy freeze");
        }

        const auto circle_animation = upstream_symbols.find("CIRCLEANIM");
        const auto circle_object = upstream_symbols.find("CIRCLEOBJ");
        require(!circle_animation.empty() && !circle_object.empty(),
                "circle state symbols are missing");
        game.map().write_native_word(circle_object.front(), 0U);
        game.map().write_native_word(circle_animation.front(), 0x39U);
        static_cast<void>(game.tick({}));
        const auto circle = game.circle_effect_state();
        if (!circle.active || circle.radius == 0U
            || circle.centre_x != 128 || circle.centre_y != 112) {
            std::cerr << "circle active=" << circle.active
                      << " radius=" << circle.radius
                      << " centre=(" << circle.centre_x << ','
                      << circle.centre_y << ") rgb=("
                      << static_cast<unsigned>(circle.red) << ','
                      << static_cast<unsigned>(circle.green) << ','
                      << static_cast<unsigned>(circle.blue) << ")\n";
        }
        require(circle.active && circle.radius != 0U
                    && circle.centre_x == 128 && circle.centre_y == 112,
                "smart-bomb circle did not advance through TRANS.ASM");

        // Boss-death circles retain a live object pointer. The original
        // ROTPROJ_L logarithmic projection can loop forever when the host
        // geometry bridge produces its zero-coordinate edge case. The port
        // projects this centre itself, so a tracked circle must advance while
        // preserving the native pointer rather than entering that loop.
        const auto tracked_circle_handles = game.objects().active_handles();
        require(!tracked_circle_handles.empty(),
                "tracked-circle regression has no active object");
        const auto tracked_circle_pointer = static_cast<std::uint16_t>(
            0x0338U + (tracked_circle_handles.front() - 1U) * 56U);
        auto& tracked_circle_object_state =
            game.objects().at(tracked_circle_handles.front());
        tracked_circle_object_state.world_x =
            std::numeric_limits<std::int16_t>::min();
        tracked_circle_object_state.world_y = 0;
        tracked_circle_object_state.world_z = 0;
        game.map().write_native_word(
            circle_object.front(), tracked_circle_pointer);
        game.map().write_native_word(circle_animation.front(), 0x39U);
        static_cast<void>(game.tick({}));
        const auto tracked_circle = game.circle_effect_state();
        require(tracked_circle.active
                    && game.map().read_native_word(circle_object.front())
                        == tracked_circle_pointer,
                "tracked boss-death circle did not survive native projection edge case");

        game.map().write_native_word(circle_object.front(), 0U);
        game.map().write_native_word(circle_animation.front(),
            static_cast<std::uint16_t>(upstream_symbols.find(
                "MSCRAMWIPE_CIRCLE").front()));
        auto saw_window_wipe = false;
        starfox::simulation::WindowWipeState last_window_wipe{};
        for (std::size_t frame = 0; frame < 6U && !saw_window_wipe; ++frame) {
            static_cast<void>(game.tick({}));
            const auto wipe = game.window_wipe_state();
            last_window_wipe = wipe;
            saw_window_wipe = wipe.active && wipe.logic == 0xaaU
                && wipe.left[0] == 16U && wipe.right[0] == 239U;
        }
        if (!saw_window_wipe) {
            std::cerr << "wipe active=" << last_window_wipe.active
                      << " logic=" << static_cast<unsigned>(last_window_wipe.logic)
                      << " bounds=" << last_window_wipe.left[0] << ','
                      << last_window_wipe.right[0]
                      << " doing=" << static_cast<unsigned>(
                          game.map().read_native_byte(
                              upstream_symbols.find("DOINGWIPE").front()))
                      << " do=" << static_cast<unsigned>(
                          game.map().read_native_byte(
                              upstream_symbols.find("DOAWIPE").front()))
                      << " anim=" << game.map().read_native_word(
                          circle_animation.front()) << '\n';
        }
        require(saw_window_wipe
                    && game.map().unknown_superfx_launches().empty(),
                "table-driven gameplay wipe was not advanced for presentation");

        const auto send_message = upstream_symbols.find("SEND_MESSAGE_L");
        require(!send_message.empty(), "SEND_MESSAGE_L symbol is missing");
        starfox::simulation::Wdc65816Registers message_registers;
        message_registers.a = 1U;
        message_registers.status = 0x24U;
        game.map().call_native_routine(
            send_message.front(), message_registers, 2'000'000, true);
        for (std::size_t tick = 0; tick < 6U; ++tick) {
            if (starfox_ex_cartridge) {
                starfox::simulation::Wdc65816Registers friends_registers;
                friends_registers.status = 0x24U;
                game.map().call_native_routine(
                    upstream_symbols.find("FRIENDS_MESSAGES_L").front(),
                    friends_registers, 5'000'000, true);
            } else {
                static_cast<void>(game.tick({}));
            }
        }
        const auto dialogue = game.dialogue_state();
        const auto minimum_dialogue_frame = static_cast<std::uint8_t>(
            starfox_ex_cartridge ? 4U : 5U);
        if (!(dialogue.active && dialogue.text_visible
                && dialogue.portrait_frame >= minimum_dialogue_frame
                && dialogue.text_address != 0U
                && game.map().unknown_superfx_launches().empty())) {
            std::cerr << "dialogue diagnostic: active=" << dialogue.active
                      << ", text=" << dialogue.text_visible
                      << ", frame="
                      << static_cast<unsigned>(dialogue.portrait_frame)
                      << ", address=$" << std::hex << dialogue.text_address
                      << std::dec << ", unknown-gsu="
                      << game.map().unknown_superfx_launches().size() << '\n';
        }
        require(dialogue.active && dialogue.text_visible
                    && dialogue.portrait_frame >= minimum_dialogue_frame
                    && dialogue.text_address != 0U
                    && game.map().unknown_superfx_launches().empty(),
                "original teammate communication state was not presented");
        if (starfox_ex_cartridge) {
            require(game.map().read_native_byte(
                        upstream_symbols.find("MACHINETYPE").front()) == 10U,
                "EX native runtime did not enable its intro machine message");
            starfox::simulation::Wdc65816Registers alternate_registers;
            alternate_registers.a = 1U;
            alternate_registers.status = 0x24U;
            game.map().call_native_routine(
                upstream_symbols.find("SEND_MESSAGEX2_L").front(),
                alternate_registers, 2'000'000, true);
            for (std::size_t frame = 0; frame < 6U; ++frame) {
                starfox::simulation::Wdc65816Registers animate_registers;
                animate_registers.status = 0x24U;
                game.map().call_native_routine(
                    upstream_symbols.find("FRIENDS_MESSAGES2_L").front(),
                    animate_registers, 5'000'000, true);
            }
            const auto alternate_dialogue = game.dialogue_state();
            const auto alternate_messages =
                upstream_symbols.find("MESSAGES2").front();
            const auto alternate_face_data =
                upstream_symbols.find("FACEDATA2").front();
            const auto alternate_face_pointer = game.map().read_native_word(
                upstream_symbols.find("M_FACEPTR").front());
            const auto alternate_face_base =
                static_cast<std::uint16_t>(alternate_face_data);
            const auto expected_alternate_frame = static_cast<std::uint8_t>(
                (alternate_face_pointer - alternate_face_base) / 640U);
            require(alternate_dialogue.active
                        && alternate_dialogue.text_visible
                        && alternate_dialogue.alternate_portraits
                        && alternate_dialogue.text_address != 0U
                        && (alternate_dialogue.text_address & 0xff0000U)
                            == (alternate_messages & 0xff0000U)
                        && alternate_dialogue.portrait_frame
                            == expected_alternate_frame,
                "EX secondary dialogue channel was not presented");

            const starfox::render::ScaledTextRenderer ex_text_renderer{
                upstream_rom, upstream_symbols};
            starfox::render::Framebuffer alternate_face{32U, 40U};
            ex_text_renderer.draw_face(
                0U, 0, 0, alternate_face, 7U * 16U, true);
            require(std::any_of(alternate_face.pixels().begin(),
                        alternate_face.pixels().end(),
                        [](std::uint8_t pixel) { return pixel != 0U; }),
                "EX FACEDATA2 portrait sheet was not rendered");

            const auto scored = upstream_symbols.find("SCORED").front();
            const auto ship_flags =
                upstream_symbols.find("PSHIPFLAGS").front();
            const auto mario_text =
                upstream_symbols.find("M_TXTDATA").front();
            const auto score_text =
                upstream_symbols.find("SCORETXT2").front();
            starfox::simulation::GameSimulation score_off_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            starfox::simulation::GameSimulation score_on_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            score_off_game.map().write_native_byte(scored, 0U);
            score_on_game.map().write_native_byte(scored, 1U);
            const auto score_off_tick = score_off_game.tick({});
            const auto score_on_tick = score_on_game.tick({});
            require(score_on_tick.prelude_instructions
                        > score_off_tick.prelude_instructions,
                "EX gameplay loop did not invoke CESTIMER_L in scored mode");

            score_on_game.map().write_native_byte(ship_flags,
                static_cast<std::uint8_t>(score_on_game.map().read_native_byte(
                    ship_flags) & ~0x20U));
            score_on_game.map().write_native_word(mario_text, 0U);
            starfox::simulation::Wdc65816Registers score_registers;
            score_registers.status = 0x24U;
            score_on_game.map().call_native_routine(
                upstream_symbols.find("CESTIMER_L").front(),
                score_registers, 5'000'000, true);
            if (score_on_game.map().read_native_word(mario_text)
                    != (score_text & 0xffffU)
                || !score_on_game.map().unknown_superfx_launches().empty()) {
                std::cerr << "EX scored HUD diagnostic: text=$" << std::hex
                          << score_on_game.map().read_native_word(mario_text)
                          << ", expected=$" << (score_text & 0xffffU)
                          << std::dec << ", unknown-gsu="
                          << score_on_game.map().unknown_superfx_launches().size()
                          << '\n';
                for (const auto launch :
                    score_on_game.map().unknown_superfx_launches()) {
                    std::cerr << "  scored HUD GSU launch $" << std::hex
                              << launch << std::dec << '\n';
                }
            }
            require(score_on_game.map().read_native_word(mario_text)
                        == (score_text & 0xffffU)
                    && score_on_game.map().unknown_superfx_launches().empty(),
                "EX scored-mode HUD source routine was not fully translated");

            starfox::simulation::GameSimulation ex_options_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            static_cast<void>(ex_options_game.tick({}));
            const auto model_double =
                upstream_symbols.find("M_BIGHEADMODE").front();
            const auto model_quadruple =
                upstream_symbols.find("M_BIGGERHEADMODE").front();
            ex_options_game.map().write_native_byte(model_double, 0U);
            ex_options_game.map().write_native_byte(model_quadruple, 0U);
            require(ex_options_game.model_scale_multiplier() == 1U,
                    "EX MODEL SIZE normal option did not preserve 1x geometry");
            ex_options_game.map().write_native_byte(model_double, 1U);
            require(ex_options_game.model_scale_multiplier() == 2U,
                    "EX MODEL SIZE 2x option did not reach the renderer");
            ex_options_game.map().write_native_byte(model_quadruple, 1U);
            require(ex_options_game.model_scale_multiplier() == 4U,
                    "EX MODEL SIZE 4x option did not take source priority");
            ex_options_game.map().write_native_byte(model_double, 0U);
            ex_options_game.map().write_native_byte(model_quadruple, 0U);
            const auto nan_mode = upstream_symbols.find("M_NANMODE").front();
            constexpr std::array<const char*, 5> nan_tables{
                "NAN_C", "FIREBODY_C", "BLUELAVABODY_C", "STEALTH_C",
                "TREVORTEX_C",
            };
            ex_options_game.map().write_native_byte(nan_mode, 0U);
            require(!ex_options_game.model_colour_table_override(),
                    "EX -NAN mode zero replaced ordinary object materials");
            for (std::size_t mode = 1U; mode <= nan_tables.size(); ++mode) {
                ex_options_game.map().write_native_byte(
                    nan_mode, static_cast<std::uint8_t>(mode));
                const auto expected_table = static_cast<std::uint16_t>(
                    upstream_symbols.find(nan_tables[mode - 1U]).front());
                require(ex_options_game.model_colour_table_override()
                            == expected_table,
                        "EX -NAN texture mode did not reach the renderer");
            }
            ex_options_game.map().write_native_byte(nan_mode, 6U);
            require(!ex_options_game.model_colour_table_override(),
                    "EX wobble mode incorrectly replaced its colour table");
            ex_options_game.map().write_native_byte(nan_mode, 0U);
            const auto more_dots =
                upstream_symbols.find("M_MOREDOTS").front();
            ex_options_game.map().write_native_word(more_dots, 0U);
            require(ex_options_game.dust_point_count()
                        == starfox::simulation::kNormalDustPoints,
                    "EX normal dust count did not retain 120 source points");
            ex_options_game.map().write_native_word(more_dots, 1U);
            require(ex_options_game.dust_point_count()
                        == starfox::simulation::kMaximumDustPoints,
                    "EX MORE DOTS did not expose all 511 source points");
            ex_options_game.map().write_native_word(more_dots, 0U);

            const auto fps_counter =
                upstream_symbols.find("FPSCOUNTERON").front();
            const auto measured_fps =
                upstream_symbols.find("FRAMESB").front();
            const auto clear_bitmaps =
                upstream_symbols.find("M_CLRBITMAPS").front();
            const auto bitmap1 = upstream_symbols.find("BITMAP1").front();
            const auto draw_map_address =
                upstream_symbols.find("DRAWMAP").front();
            const auto count_bitmap_bytes = [&] {
                auto result = std::size_t{};
                for (std::uint32_t byte = 0U;
                     byte < 28U * 24U * 32U; ++byte) {
                    result += ex_options_game.map().read_native_byte(
                        0x700000U | static_cast<std::uint16_t>(
                            bitmap1 + byte)) != 0U;
                }
                return result;
            };
            ex_options_game.map().write_native_word(clear_bitmaps, 1U);
            ex_options_game.map().write_native_word(
                draw_map_address, static_cast<std::uint16_t>(bitmap1));
            ex_options_game.map().write_native_word(fps_counter, 0U);
            static_cast<void>(ex_options_game.tick({}));
            const auto fps_off_bytes = count_bitmap_bytes();
            ex_options_game.map().write_native_byte(measured_fps, 37U);
            ex_options_game.map().write_native_word(fps_counter, 1U);
            const auto fps_tick = ex_options_game.tick({});
            const auto fps_on_bytes = count_bitmap_bytes();
            constexpr auto expected_fps_37_bitmap_bytes = 105U;
            if (fps_on_bytes != expected_fps_37_bitmap_bytes) {
                std::cerr << "EX FPS bitmap diagnostic: off=" << fps_off_bytes
                          << ", on=" << fps_on_bytes << ", measured="
                          << static_cast<unsigned>(ex_options_game.map()
                                 .read_native_byte(measured_fps))
                          << ", unknown-gsu=" << ex_options_game.map()
                                 .unknown_superfx_launches().size()
                          << ", drawmap=$" << std::hex
                          << ex_options_game.map().read_native_word(
                                 draw_map_address) << std::dec
                          << '\n';
            }
            require(fps_on_bytes == expected_fps_37_bitmap_bytes
                        && fps_tick.prelude_instructions != 0U
                        && ex_options_game.map().unknown_superfx_launches().empty(),
                    "EX FPS COUNTER did not reproduce TRANS.ASM's native bitmap text");

            starfox::simulation::GameSimulation no_objects_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            const auto no_objects = upstream_symbols.find("NOOBJMODE").front();
            no_objects_game.map().write_native_word(no_objects, 1U);
            const auto no_objects_tick = no_objects_game.tick({});
            const auto no_objects_handle = [](std::uint16_t pointer) {
                return pointer < 0x0338U ? starfox::simulation::ObjectHandle{}
                    : static_cast<starfox::simulation::ObjectHandle>(
                        (pointer - 0x0338U) / 56U + 1U);
            };
            const std::array protected_no_objects{
                no_objects_game.player(),
                no_objects_handle(
                    no_objects_game.map().read_native_word(
                        upstream_symbols.find("PCBOXOBJ_B").front())),
                no_objects_handle(
                    no_objects_game.map().read_native_word(
                        upstream_symbols.find("PCBOXOBJ_LW").front())),
                no_objects_handle(
                    no_objects_game.map().read_native_word(
                        upstream_symbols.find("PCBOXOBJ_RW").front())),
            };
            const auto remaining_no_objects =
                no_objects_game.objects().active_handles();
            require(no_objects_tick.strategies.objects_removed != 0U
                        && no_objects_tick.strategies.objects_run >= 1U
                        && no_objects_tick.strategies.objects_run <= 4U
                        && no_objects_game.objects().is_active(
                            no_objects_game.player())
                        && std::all_of(remaining_no_objects.begin(),
                            remaining_no_objects.end(),
                            [&protected_no_objects](auto handle) {
                                return std::find(protected_no_objects.begin(),
                                           protected_no_objects.end(), handle)
                                    != protected_no_objects.end();
                            }),
                    "EX NO OBJECTS did not preserve only PLAYPT and player collision objects");

            const auto mouse_mode =
                upstream_symbols.find("MOUSEMODE").front();
            const auto mouse_connected =
                upstream_symbols.find("MOUSE_CON1").front();
            const auto mouse_x = upstream_symbols.find("MOUSE_X1").front();
            const auto mouse_y = upstream_symbols.find("MOUSE_Y1").front();
            const auto mouse_buttons =
                upstream_symbols.find("MOUSE_SW1").front();
            const auto mouse_trigger =
                upstream_symbols.find("MOUSE_SWT1").front();
            const auto mouse_previous =
                upstream_symbols.find("MOUSE_SB1").front();
            ex_options_game.map().write_native_byte(mouse_mode, 1U);
            require(ex_options_game.ex_mouse_control_enabled(),
                    "EX MOUSE CONTROL option was not exposed to the PC input path");
            ex_options_game.set_mouse_input({200, -200, 0x01U});
            static_cast<void>(ex_options_game.tick({}));
            require(ex_options_game.map().read_native_byte(mouse_connected) == 1U
                        && ex_options_game.map().read_native_byte(mouse_x) == 0x7fU
                        && ex_options_game.map().read_native_byte(mouse_y) == 0xffU
                        && ex_options_game.map().read_native_byte(mouse_buttons)
                            == 0x01U
                        && ex_options_game.map().read_native_byte(mouse_trigger)
                            == 0x01U
                        && ex_options_game.map().read_native_byte(mouse_previous)
                            == 0x01U,
                    "PC mouse did not produce the exact port-two SNES mouse packet");
            ex_options_game.set_mouse_input({0, 0, 0x01U});
            ex_options_game.map().write_native_byte(mouse_mode, 1U);
            static_cast<void>(ex_options_game.tick({}));
            require(ex_options_game.map().read_native_byte(mouse_x) == 0U
                        && ex_options_game.map().read_native_byte(mouse_y) == 0U
                        && ex_options_game.map().read_native_byte(mouse_buttons)
                            == 0x01U
                        && ex_options_game.map().read_native_byte(mouse_trigger)
                            == 0U,
                    "held EX mouse button was incorrectly repeated as a trigger");
            ex_options_game.set_mouse_input({-5, 6, 0x03U});
            ex_options_game.map().write_native_byte(mouse_mode, 1U);
            static_cast<void>(ex_options_game.tick({}));
            if (ex_options_game.map().read_native_byte(mouse_x) != 0x85U
                || ex_options_game.map().read_native_byte(mouse_y) != 0x06U
                || ex_options_game.map().read_native_byte(mouse_buttons) != 0x03U
                || ex_options_game.map().read_native_byte(mouse_trigger) != 0x03U) {
                std::cerr << "EX mouse diagnostic: x=$" << std::hex
                          << static_cast<unsigned>(ex_options_game.map().read_native_byte(mouse_x))
                          << " y=$"
                          << static_cast<unsigned>(ex_options_game.map().read_native_byte(mouse_y))
                          << " buttons=$"
                          << static_cast<unsigned>(ex_options_game.map().read_native_byte(mouse_buttons))
                          << " trigger=$"
                          << static_cast<unsigned>(ex_options_game.map().read_native_byte(mouse_trigger))
                          << std::dec << '\n';
            }
            require(ex_options_game.map().read_native_byte(mouse_x) == 0x85U
                        && ex_options_game.map().read_native_byte(mouse_y) == 0x06U
                        && ex_options_game.map().read_native_byte(mouse_buttons)
                            == 0x03U
                        && ex_options_game.map().read_native_byte(mouse_trigger)
                            == 0x03U,
                    "EX both-button boost/brake edge or signed movement was lost");
            ex_options_game.set_mouse_input({0, 0, 0U});
            ex_options_game.map().write_native_byte(mouse_mode, 1U);
            static_cast<void>(ex_options_game.tick({}));
            require(ex_options_game.map().read_native_byte(mouse_buttons) == 0U
                        && ex_options_game.map().read_native_byte(mouse_trigger)
                            == 0U
                        && ex_options_game.map().read_native_byte(mouse_previous)
                            == 0U,
                    "EX mouse-button release left a stuck native switch");
            ex_options_game.map().write_native_byte(mouse_mode, 0U);
            require(!ex_options_game.ex_mouse_control_enabled(),
                    "disabled EX MOUSE CONTROL still captured the PC mouse");

            const auto scope_mode =
                upstream_symbols.find("SCOPEMODE").front();
            const auto scope_no_latch =
                upstream_symbols.find("SCOPE_NO_LATCH_FLAG").front();
            const auto scope_held =
                upstream_symbols.find("SCOPE_HELD").front();
            const auto scope_new =
                upstream_symbols.find("SCOPE_NEW").front();
            const auto scope_previous =
                upstream_symbols.find("SCOPE_PREV").front();
            const auto scope_h = upstream_symbols.find("SCOPE_H").front();
            const auto scope_v = upstream_symbols.find("SCOPE_V").front();
            ex_options_game.map().write_native_byte(scope_mode, 1U);
            require(ex_options_game.ex_scope_control_enabled()
                        && ex_options_game.ex_pointing_control_enabled(),
                    "EX SUPER SCOPE option was not exposed to the PC mouse");
            ex_options_game.set_mouse_input({0, 0, 0x0fU, 0x55U, 0x66U});
            static_cast<void>(ex_options_game.tick({}));
            require(ex_options_game.map().read_native_word(scope_no_latch) == 0U
                        && ex_options_game.map().read_native_word(scope_h)
                            == 0x55U
                        && ex_options_game.map().read_native_word(scope_v)
                            == 0x66U
                        && ex_options_game.map().read_native_word(scope_held)
                            == 0xf000U
                        && ex_options_game.map().read_native_word(scope_new)
                            == 0xf000U
                        && ex_options_game.map().read_native_word(scope_previous)
                            == 0xf000U,
                    "PC mouse did not produce EX's native Super Scope latch packet");
            ex_options_game.set_mouse_input({0, 0, 0x0fU, 0x56U, 0x67U});
            ex_options_game.map().write_native_byte(scope_mode, 1U);
            static_cast<void>(ex_options_game.tick({}));
            require(ex_options_game.map().read_native_word(scope_new) == 0U
                        && ex_options_game.map().read_native_word(scope_h)
                            == 0x56U
                        && ex_options_game.map().read_native_word(scope_v)
                            == 0x67U,
                    "held EX Super Scope buttons repeated or aim stopped updating");
            ex_options_game.set_mouse_input({0, 0, 0U, 0x56U, 0x67U});
            ex_options_game.map().write_native_byte(scope_mode, 1U);
            static_cast<void>(ex_options_game.tick({}));
            require(ex_options_game.map().read_native_word(scope_held) == 0U
                        && ex_options_game.map().read_native_word(scope_new) == 0U
                        && ex_options_game.map().read_native_word(scope_previous)
                            == 0U,
                    "EX Super Scope release left a stuck native button");
            ex_options_game.map().write_native_byte(scope_mode, 0U);

            const auto ntt_mode = upstream_symbols.find("NTTMODE").front();
            const auto ntt_read = upstream_symbols.find("JPREAD").front();
            const auto ntt_trigger = upstream_symbols.find("JPTRIG").front();
            const auto ntt_previous = upstream_symbols.find("JPPREV").front();
            ex_options_game.map().write_native_byte(ntt_mode, 1U);
            ex_options_game.set_ntt_input(0xa55aU);
            static_cast<void>(ex_options_game.tick({}));
            require(ex_options_game.map().read_native_word(ntt_read) == 0xa55aU
                        && ex_options_game.map().read_native_word(ntt_trigger)
                            == 0xa55aU
                        && ex_options_game.map().read_native_word(ntt_previous)
                            == 0xa55aU,
                    "PC keyboard did not produce EX's native NTT Data Pad packet");
            ex_options_game.map().write_native_byte(ntt_mode, 1U);
            ex_options_game.set_ntt_input(0xa55aU);
            static_cast<void>(ex_options_game.tick({}));
            require(ex_options_game.map().read_native_word(ntt_trigger) == 0U,
                    "held EX NTT Data Pad key was repeated as a trigger");
            ex_options_game.map().write_native_byte(ntt_mode, 1U);
            ex_options_game.set_ntt_input(0U);
            static_cast<void>(ex_options_game.tick({}));
            require(ex_options_game.map().read_native_word(ntt_read) == 0U
                        && ex_options_game.map().read_native_word(ntt_previous)
                            == 0U,
                    "EX NTT Data Pad release left a stuck native key");

            const auto no_hud = upstream_symbols.find("NOHUD").front();
            const auto meters = upstream_symbols.find("M_METERS").front();
            const auto no_sfx = upstream_symbols.find("NOSFX").front();
            const auto no_set_port =
                upstream_symbols.find("NOSETPORT3").front();
            const auto dots_stars =
                upstream_symbols.find("DOTSSTARS").front();
            const auto dots_flag =
                upstream_symbols.find("DOTSFLAG").front();
            ex_options_game.map().write_native_byte(no_hud, 1U);
            ex_options_game.map().write_native_word(meters, 1U);
            ex_options_game.map().write_native_byte(no_sfx, 1U);
            ex_options_game.map().write_native_byte(no_set_port, 0U);
            ex_options_game.map().write_native_byte(dots_stars, 1U);
            ex_options_game.map().write_native_word(dots_flag, 0xffffU);
            static_cast<void>(ex_options_game.tick({}));
            require(ex_options_game.map().read_native_word(meters) == 0U
                        && ex_options_game.map().read_native_byte(no_set_port)
                            == 1U
                        && ex_options_game.map().read_native_word(dots_flag)
                            == 0U,
                "EX gameplay option gates diverged from MAIN.ASM");

            starfox::simulation::GameSimulation palette_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            starfox::simulation::GameSimulation palette_blocked_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            for (std::size_t tick = 0; tick < 360U; ++tick) {
                static_cast<void>(palette_game.tick({}));
                static_cast<void>(palette_blocked_game.tick({}));
            }
            const auto no_background_mode =
                upstream_symbols.find("NOBGMODE").front();
            const auto palette_slow = upstream_symbols.find("TEMPVAL5").front();
            const auto palette_slower =
                upstream_symbols.find("TEMPVAL6").front();
            const auto fade_yamao =
                upstream_symbols.find("FADEPALTOYAMAO").front();
            const auto fade_corneria_night =
                upstream_symbols.find("FADEPALTOCORNNITE").front();
            const auto fade_fx_pink =
                upstream_symbols.find("FADEPALTOFXPINK").front();
            const auto prime_palette_transfer = [&](auto& instance,
                                                     std::uint8_t blocked) {
                instance.map().write_native_byte(no_background_mode, blocked);
                instance.map().write_native_byte(fade_yamao, 2U);
                instance.map().write_native_byte(palette_slow, 2U);
                instance.map().write_native_byte(fade_corneria_night, 2U);
                instance.map().write_native_byte(palette_slower, 9U);
                instance.map().write_native_byte(fade_fx_pink, 2U);
            };
            prime_palette_transfer(palette_game, 0U);
            prime_palette_transfer(palette_blocked_game, 1U);
            static_cast<void>(palette_game.tick({}));
            static_cast<void>(palette_blocked_game.tick({}));
            require(palette_game.map().read_native_byte(fade_yamao) == 1U
                        && palette_game.map().read_native_byte(palette_slow) == 3U
                        && palette_game.map().read_native_byte(
                            fade_corneria_night) == 2U
                        && palette_game.map().read_native_byte(palette_slower)
                            == 10U
                        && palette_game.map().read_native_byte(fade_fx_pink) == 2U,
                "EX per-transfer palette updates or cadence counters diverged");
            require(palette_blocked_game.map().read_native_byte(fade_yamao) == 2U
                        && palette_blocked_game.map().read_native_byte(palette_slow)
                            == 2U
                        && palette_blocked_game.map().read_native_byte(
                            fade_corneria_night) == 2U
                        && palette_blocked_game.map().read_native_byte(
                            palette_slower) == 9U
                        && palette_blocked_game.map().read_native_byte(
                            fade_fx_pink) == 2U,
                "EX NOBGMODE did not suppress the complete palette update block");
            static_cast<void>(palette_game.tick({}));
            require(palette_game.map().read_native_byte(palette_slow) == 0U
                        && palette_game.map().read_native_byte(
                            fade_corneria_night) == 1U
                        && palette_game.map().read_native_byte(palette_slower)
                            == 0U
                        && palette_game.map().read_native_byte(fade_fx_pink) == 1U,
                "EX divided palette groups did not run on source cadence");

            starfox::simulation::GameSimulation ex_bgm_game{
                upstream_rom, upstream_symbols, "LEVEL1_1"};
            const auto upload_before =
                ex_bgm_game.map().apu_upload_generation();
            const auto bgm_sfx = upstream_symbols.find("BGMSFX").front();
            const auto set_new_bgm =
                upstream_symbols.find("SETNEWBGM").front();
            const auto cursed_bgm =
                upstream_symbols.find("CURSEDBGM").front();
            const auto bgm_test =
                upstream_symbols.find("BGMTEST").front();
            ex_bgm_game.map().write_native_byte(bgm_sfx, 0U);
            ex_bgm_game.map().write_native_byte(cursed_bgm, 0U);
            ex_bgm_game.map().write_native_word(bgm_test, 1U);
            ex_bgm_game.map().write_native_byte(set_new_bgm, 1U);
            const auto ex_bgm_tick = ex_bgm_game.tick({});
            require(ex_bgm_game.map().read_native_byte(set_new_bgm) == 0U
                        && ex_bgm_game.map().apu_upload_generation()
                            >= upload_before + 2U
                        && !ex_bgm_tick.audio_port_writes.empty(),
                "EX live BGM option did not reset and load its selected SPC bank");
        }

        const auto level_finished = upstream_symbols.find("LEVELFINISHED");
        const auto stage_address = upstream_symbols.find("STAGE");
        const auto new_map_address = upstream_symbols.find("NEWMAP");
        const auto level1_2 = upstream_symbols.find("LEVEL1_2");
        require(!level_finished.empty() && !stage_address.empty()
                    && !new_map_address.empty() && !level1_2.empty(),
                "route-transition symbols are missing");
        game.map().write_native_word(level_finished.front(), 1U);
        static_cast<void>(game.tick({}));
        const auto stage_results = game.stage_results_state();
        require(game.flow_state() == starfox::simulation::GameFlowState::stage_results
                    && stage_results.active
                    && game.map().read_native_word(stage_address.front()) == 1U,
                "completed Corneria did not enter the mission tally");
        const auto result_pointer_address = upstream_symbols.find("SPECPTR");
        require(!result_pointer_address.empty(),
                "stage-result percentage pointer is missing");
        const auto result_pointer_before = game.map().read_native_word(
            result_pointer_address.front());
        auto saw_results_fade = false;
        for (std::size_t tick = 0;
             tick < 200U
                  && game.flow_state()
                     == starfox::simulation::GameFlowState::stage_results;
             ++tick) {
            if (starfox_ex_cartridge
                && game.map().read_native_word(result_pointer_address.front())
                    == static_cast<std::uint16_t>(result_pointer_before + 1U)) {
                // In a real clear, bossbgm*.ASM keeps advancing inside
                // TRANSFER_L and mapendwipe sets CLB2 after the tally has
                // finished. This focused transition test injects
                // LEVELFINISHED directly, so supply that final map-script
                // signal only after END_LEVEL_SEQ has recorded its result.
                game.map().write_native_byte(
                    upstream_symbols.find("CLB2").front(), 1U);
            }
            game.present_frame();
            game.present_frame();
            game.present_frame();
            saw_results_fade = saw_results_fade
                || (game.map().display_brightness() > 0U
                    && game.map().display_brightness() < 15U);
            static_cast<void>(game.tick({}));
        }
        const auto selected_map = static_cast<std::uint32_t>(
            game.map().read_native_byte(new_map_address.front()))
            | (static_cast<std::uint32_t>(
                   game.map().read_native_byte(new_map_address.front() + 1U)) << 8U)
            | (static_cast<std::uint32_t>(
                   game.map().read_native_byte(new_map_address.front() + 2U)) << 16U);
        require(game.flow_state() == starfox::simulation::GameFlowState::planet_travel
                    && game.map().read_native_word(stage_address.front()) == 1U
                    && selected_map == level1_2.front()
                    && game.map().read_native_word(result_pointer_address.front())
                        == static_cast<std::uint16_t>(result_pointer_before + 1U),
                "completed Corneria did not enter the original route travel screen");
        require(game.map().ppu_state().background_mode == 3U,
                "results-to-route transition did not restore Mode 3");
        require((game.map().ppu_state().main_screen & 0x03U) == 0x03U,
                "results-to-route transition did not enable both map backgrounds");
        require(saw_results_fade,
                "results-to-route transition skipped the source fade");
        for (const auto launch : game.map().unknown_superfx_launches()) {
            std::cerr << "  result-screen GSU launch $" << std::hex << launch
                      << std::dec << '\n';
        }
        require(game.map().unknown_superfx_launches().empty(),
                "results sequence launched an unimplemented Super FX routine");
        const auto planet_palette = upstream_symbols.find("PLANSELPAL").front();
        auto source_planet_palette_is_exact = true;
        for (std::size_t colour = 0U;
             colour < game.map().ppu_state().cgram.size(); ++colour) {
            source_planet_palette_is_exact = source_planet_palette_is_exact
                && game.map().ppu_state().cgram[colour]
                    == upstream_rom.read16(planet_palette
                        + static_cast<std::uint32_t>(colour * 2U));
        }
        require(source_planet_palette_is_exact,
                "post-level map did not load PLANSELPAL exactly into CGRAM");
        const auto current_planet_address =
            upstream_symbols.find("CURRENTPLANET").front();
        const auto planet_positions = upstream_symbols.find("PLANETPOS").front();
        const auto selected_planet = game.map().read_native_byte(
            current_planet_address);
        const auto selected_record = planet_positions
            + static_cast<std::uint32_t>(selected_planet) * 4U;
        const auto selected_x = upstream_rom.read8(selected_record + 2U);
        const auto selected_y = upstream_rom.read8(selected_record + 3U);
        starfox::render::Framebuffer route_planets{256U, 224U};
        background_renderer.draw_bg1(game.map().ppu_state(), route_planets,
            starfox::render::TilePriorityPass::low);
        background_renderer.draw_bg1(game.map().ppu_state(), route_planets,
            starfox::render::TilePriorityPass::high);
        std::size_t selected_planet_pixels{};
        for (std::uint32_t y = selected_y; y < selected_y + 32U; ++y) {
            for (std::uint32_t x = selected_x; x < selected_x + 32U; ++x) {
                if (route_planets.get(x, y) != 0U) ++selected_planet_pixels;
            }
        }
        require(selected_planet_pixels > 256U,
                "post-level route redraw omitted the selected destination planet");
        const auto ship_position_address =
            upstream_symbols.find("SHIPXY").front();
        const auto initial_route_ship_position =
            game.map().read_native_word(ship_position_address);
        auto saw_route_ship_move = false;
        for (std::size_t frame = 0; frame < 900U; ++frame) {
            game.present_frame();
            saw_route_ship_move = saw_route_ship_move
                || game.map().read_native_word(ship_position_address)
                    != initial_route_ship_position;
            if ((frame % 3U) == 2U) static_cast<void>(game.tick({}));
        }
        require(game.flow_state()
                        == starfox::simulation::GameFlowState::planet_travel
                    && saw_route_ship_move && !game.briefing_state().active,
                "post-level route did not hold at the arrived planet for A");
        static_cast<void>(game.tick(
            {0U, starfox::input::a, 0U}));
        auto saw_valid_post_level_briefing = false;
        for (std::size_t frame = 0;
             frame < 3'000U
                 && game.flow_state()
                     == starfox::simulation::GameFlowState::planet_travel;
             ++frame) {
            game.present_frame();
            if (const auto briefing = game.briefing_state(); briefing.active) {
                saw_valid_post_level_briefing =
                    briefing.message_address != 0U
                    && briefing.planet_name_address != 0U;
            }
            if ((frame % 3U) == 2U) static_cast<void>(game.tick({}));
        }
        require(game.flow_state() == starfox::simulation::GameFlowState::gameplay
                    && saw_valid_post_level_briefing,
                "A did not advance the post-level route through a valid briefing");
        require(!game.map().ended() && game.objects().is_active(game.player()),
                "next-stage INITGAME_L did not rebuild a playable object/map state");

        const auto game_shape = upstream_symbols.find("GAMESH");
        const auto over_shape = upstream_symbols.find("OVERSH");
        require(!game_shape.empty() && !over_shape.empty(),
                "game-over model symbols are missing");
        game.map().write_native_word(level_finished.front(), 10U);
        static_cast<void>(game.tick({}));
        const auto has_shape = [&game](std::uint16_t shape) {
            for (const auto handle : game.objects().active_handles()) {
                if (game.objects().at(handle).shape == shape) return true;
            }
            return false;
        };
        require(game.flow_state() == starfox::simulation::GameFlowState::game_over
                    && has_shape(static_cast<std::uint16_t>(game_shape.front()))
                    && has_shape(static_cast<std::uint16_t>(over_shape.front())),
                "game-over exit did not initialize the original GAME/OVER scene");
        require(game.game_over_background_subtract() == 31U,
                "game-over scene omitted its fixed-white background hold");
        static_cast<void>(game.tick({0, starfox::input::start, 0}));
        require(game.flow_state() == starfox::simulation::GameFlowState::game_over,
                "game-over START lock accepted input before its reveal");
        require(game.game_over_background_subtract() == 31U,
                "game-over background reveal began during its START lock");
        // The source lock is 50 black TRANSFERs plus 31 HALFFADE TRANSFERs.
        // The early tick above may synthesize between three and six missing
        // rasters in original-pace mode. Sixty more is still safely below 81.
        for (std::size_t frame = 0U; frame < 60U; ++frame) {
            game.present_frame();
        }
        static_cast<void>(game.tick({0, starfox::input::start, 0}));
        require(game.flow_state() == starfox::simulation::GameFlowState::game_over
                    && game.map().fade_direction() == 0,
                "game-over START lock accepted input before raster 81");
        for (std::size_t frame = 0U; frame < 21U; ++frame) {
            game.present_frame();
        }
        static_cast<void>(game.tick({0, starfox::input::start, 0}));
        require(game.flow_state() == starfox::simulation::GameFlowState::game_over
                    && game.map().fade_direction() == -1
                    && game.game_over_background_subtract() == 0U,
                "game-over START did not begin the source fade after its reveal");
        for (std::size_t frame = 0; frame < 15U; ++frame) {
            game.present_frame();
        }
        static_cast<void>(game.tick({}));
        const auto my_demo = upstream_symbols.find("MY_DEMO").front();
        const auto foxy_option = upstream_symbols.find("FOXY_OPTION").front();
        require(game.flow_state()
                        == starfox::simulation::GameFlowState::continue_choice
                    && game.map().ppu_state().background_mode == 1U
                    && game.map().native_model_draw().active
                    && game.map().native_model_draw().shape
                        == static_cast<std::uint16_t>(my_demo)
                    && std::any_of(game.map().ppu_state().vram.begin(),
                        game.map().ppu_state().vram.end(),
                        [](std::uint8_t byte) { return byte != 0U; }),
                "game-over lock did not open the original Fox continue screen");
        require(game.map().read_native_byte(
                    upstream_symbols.find("LIVES").front()) == 4U,
                "continue screen did not restore the source's visible x3 reserve");
        if (starfox_ex_cartridge) {
            for (const auto* reserve : {
                     "LIVESTWO", "LIVESTHREE", "LIVESFOUR", "LIVESFIVE"}) {
                require(game.map().read_native_byte(
                            upstream_symbols.find(reserve).front()) == 4U,
                        "EX continue screen did not restore every ship reserve");
            }
        }
        // CONTINUE.ASM holds one black raster, then manually presents 1..15.
        for (std::size_t frame = 0; frame < 16U; ++frame) {
            game.present_frame();
        }
        static_cast<void>(game.tick({0, starfox::input::down, 0}));
        require(game.map().read_native_byte(foxy_option) == 0xffU,
                "continue screen DOWN did not select NO");
        static_cast<void>(game.tick({0, starfox::input::up, 0}));
        require(game.map().read_native_byte(foxy_option) == 0U,
                "continue screen UP did not restore YES");
        static_cast<void>(game.tick({0, starfox::input::start, 0}));
        require(game.flow_state()
                        == starfox::simulation::GameFlowState::continue_choice,
                "continue YES cut away before its source fade-out");
        // FADELOOP2 presents full brightness once, followed by 14..0.
        for (std::size_t frame = 0; frame < 16U; ++frame) {
            game.present_frame();
        }
        static_cast<void>(game.tick({}));
        require(game.flow_state() == starfox::simulation::GameFlowState::planet_travel
                    && game.map().read_native_word(stage_address.front()) == 1U
                    && game.map().ppu_state().background_mode == 3U,
                "continue YES did not return through the route screen");
        for (std::size_t frame = 0;
             frame < 3'000U
                 && game.flow_state()
                     == starfox::simulation::GameFlowState::planet_travel;
             ++frame) {
            game.present_frame();
            if ((frame % 3U) == 2U) static_cast<void>(game.tick({}));
        }
        require(game.flow_state() == starfox::simulation::GameFlowState::gameplay
                    && game.objects().is_active(game.player()),
                "game-over route screen did not restart the same stage");

        game.map().write_native_word(level_finished.front(), 6U);
        static_cast<void>(game.tick({}));
        if (!starfox_ex_cartridge
            && game.flow_state()
                == starfox::simulation::GameFlowState::stage_results) {
            static_cast<void>(game.tick({}));
        }
        if (!(game.flow_state() == starfox::simulation::GameFlowState::credits
                && !game.map().ended() && !game.meter_state().enabled)) {
            std::cerr << "ending diagnostic: flow="
                      << static_cast<unsigned>(game.flow_state())
                      << ", ended=" << game.map().ended()
                      << ", meters=" << game.meter_state().enabled << '\n';
        }
        require(game.flow_state() == starfox::simulation::GameFlowState::credits
                    && !game.map().ended() && !game.meter_state().enabled,
                "end-of-game exit did not enter the native score/ending sequence");
        const auto credits_map = upstream_symbols.find("CREDITSMAP").front();
        auto reached_credits_map = false;
        for (std::size_t tick = 0U; tick < 12'000U; ++tick) {
            static_cast<void>(game.tick({}));
            const auto native_map =
                (static_cast<std::uint32_t>(game.map().read_native_byte(
                     upstream_symbols.find("MAPBANK").front())) << 16U)
                | 0x8000U
                | (game.map().read_native_word(
                       upstream_symbols.find("MAPPTR").front()) & 0x7fffU);
            reached_credits_map = reached_credits_map
                || ((native_map & 0xff0000U) == (credits_map & 0xff0000U)
                    && native_map >= credits_map
                    && native_map - credits_map < 0x4000U);
            if ((!starfox_ex_cartridge
                    && game.flow_state()
                        == starfox::simulation::GameFlowState::finished)
                || (starfox_ex_cartridge && reached_credits_map)) {
                break;
            }
        }
        require(starfox_ex_cartridge
                    ? reached_credits_map
                    : game.flow_state()
                        == starfox::simulation::GameFlowState::finished,
                "native ending did not reach credits/final score in bounded source time");

        starfox::simulation::GameSimulation completed_credits{
            upstream_rom, upstream_symbols, "CREDITSMAP"};
        completed_credits.map().write_native_word(level_finished.front(), 8U);
        static_cast<void>(completed_credits.tick({}));
        if (starfox_ex_cartridge) {
            static_cast<void>(completed_credits.tick(
                {0, starfox::input::start, 0}));
            if (completed_credits.flow_state()
                    != starfox::simulation::GameFlowState::ex_pregame_menu) {
                std::cerr << "EX credits return diagnostic: flow="
                          << static_cast<unsigned>(
                              completed_credits.flow_state())
                          << ", exit=" << completed_credits.map().read_native_word(
                              level_finished.front()) << '\n';
            }
            require(completed_credits.flow_state()
                            == starfox::simulation::GameFlowState::ex_pregame_menu,
                    "EX completed credits did not return to its source menu");
        } else {
            for (std::size_t frame = 0; frame < 500U
                    && completed_credits.flow_state()
                        != starfox::simulation::GameFlowState::finished;
                 ++frame) {
                static_cast<void>(completed_credits.tick({}));
            }
            require(completed_credits.flow_state()
                            == starfox::simulation::GameFlowState::finished
                        && !completed_credits.draw_order().empty(),
                    "retail credits did not settle on its final total-score presentation");
        }

        starfox::simulation::GameSimulation title_game{
            upstream_rom, upstream_symbols, "TITLEMAP"};
        starfox::audio::Spc700Audio title_audio;
        auto title_tick = title_game.tick({0, starfox::input::start, 0});
        auto title_pcm = title_audio.render_logic_tick(title_tick.audio_port_writes);
        auto heard_title_music = std::any_of(title_pcm.begin(), title_pcm.end(),
            [](std::int16_t sample) { return sample != 0; });
        require(title_game.flow_state() == starfox::simulation::GameFlowState::title,
                "title accepted START before the source GAMEFRAME lock");
        for (std::size_t tick = 1; tick < 39; ++tick) {
            title_tick = title_game.tick({});
            title_pcm = title_audio.render_logic_tick(title_tick.audio_port_writes);
            heard_title_music = heard_title_music
                || std::any_of(title_pcm.begin(), title_pcm.end(),
                    [](std::int16_t sample) { return sample != 0; });
        }
        if (starfox_ex_cartridge) {
            // The first post-lock START belongs to EX's logo animation and
            // must land on the regular title, not the pre-game menu.
            title_tick = title_game.tick({0, starfox::input::start, 0});
            static_cast<void>(title_audio.render_logic_tick(
                title_tick.audio_port_writes));
            title_game.synchronize_apu_output_ports(title_audio.output_ports());
            for (std::size_t tick = 0; tick < 120U; ++tick) {
                title_tick = title_game.tick({});
                static_cast<void>(title_audio.render_logic_tick(
                    title_tick.audio_port_writes));
                title_game.synchronize_apu_output_ports(
                    title_audio.output_ports());
            }
        }
        if (starfox_ex_cartridge) {
            starfox::simulation::GameSimulation showcase_game{
                upstream_rom, upstream_symbols, "TITLEMAP"};
            const auto random_state = upstream_symbols.find("RAND").front();
            require(showcase_game.map().read_native_word(random_state) != 0U,
                    "EX boot did not seed its native random state");
            for (std::size_t tick = 0; tick < 150U; ++tick) {
                static_cast<void>(showcase_game.tick({}));
            }
            struct ShowcasePose {
                starfox::simulation::ObjectHandle handle{};
                std::uint8_t rotation_x{};
                std::uint8_t rotation_y{};
            };
            const auto showcase_strategy =
                upstream_symbols.find("TIT4_STRAT").front();
            auto showcase_poses = std::vector<ShowcasePose>{};
            for (const auto handle : showcase_game.draw_order()) {
                if (!showcase_game.objects().is_active(handle)) continue;
                const auto& object = showcase_game.objects().at(handle);
                if (object.strategy_address != showcase_strategy) continue;
                require(object.shape != 0U,
                        "EX title showcase selected an empty model");
                showcase_poses.push_back({
                    handle, object.rotation_x, object.rotation_y});
            }
            require(showcase_poses.size() == 3U,
                    "EX title did not spawn all three showcase models");
            for (std::size_t tick = 0; tick < 5U; ++tick) {
                static_cast<void>(showcase_game.tick({}));
            }
            auto showcase_rotated = false;
            for (const auto& pose : showcase_poses) {
                if (!showcase_game.objects().is_active(pose.handle)) continue;
                const auto& object = showcase_game.objects().at(pose.handle);
                showcase_rotated = showcase_rotated
                    || object.rotation_x != pose.rotation_x
                    || object.rotation_y != pose.rotation_y;
            }
            require(showcase_rotated,
                    "EX title showcase models remained locked edge-on");
        }
        starfox::simulation::GameSimulation title_music_game{
            upstream_rom, upstream_symbols, "TITLEMAP"};
        starfox::audio::Spc700Audio long_title_audio;
        bool heard_long_title_music = false;
        const auto trace_ex_title = starfox_ex_cartridge
            && std::getenv("STARFOX_TRACE_EX_TITLE") != nullptr;
        const auto title_music_ticks = trace_ex_title ? 870U : 300U;
        for (std::size_t tick = 0; tick < title_music_ticks; ++tick) {
            const auto music_tick = title_music_game.tick({});
            const auto music_pcm = long_title_audio.render_logic_tick(
                music_tick.audio_port_writes);
            heard_long_title_music = heard_long_title_music
                || std::any_of(music_pcm.begin(), music_pcm.end(),
                    [](std::int16_t sample) { return sample != 0; });
            if (trace_ex_title && tick % 25U == 24U) {
                const auto& title_ppu = title_music_game.map().ppu_state();
                std::cerr << "EX title tick=" << (tick + 1U)
                          << " flow=" << static_cast<unsigned>(
                              title_music_game.flow_state())
                          << " countdown=" << title_music_game.map().countdown()
                          << " bg=$" << std::hex
                          << title_music_game.map().background()
                          << " temp8=$" << static_cast<unsigned>(
                              title_music_game.map().read_native_byte(
                                  upstream_symbols.find("TEMPVAR8").front()))
                          << " mode=$" << static_cast<unsigned>(
                              title_ppu.background_mode)
                          << " tm=$" << static_cast<unsigned>(
                              title_ppu.main_screen)
                          << " b2c=$" << title_ppu.bg2_character_base
                          << " b2s=$" << title_ppu.bg2_screen_base
                          << " b3c=$" << title_ppu.bg3_character_base
                          << " b3s=$" << title_ppu.bg3_screen_base
                          << std::dec
                          << " bright=" << static_cast<unsigned>(
                              title_music_game.map().display_brightness())
                          << " objects=" << title_music_game.objects().active_count()
                          << " draw=" << title_music_game.draw_order().size()
                          << '\n';
            }
        }
        const auto& title_ppu = title_music_game.map().ppu_state();
        starfox::render::Framebuffer title_bg1{256U, 224U};
        starfox::render::Framebuffer title_bg2{256U, 224U};
        starfox::render::Framebuffer title_bg3{256U, 224U};
        const auto title_scroll_x = static_cast<std::int16_t>(
            title_music_game.map().read_native_word(
                upstream_symbols.find("BG2XSCROLL").front()));
        const auto title_scroll_y = static_cast<std::int16_t>(
            title_music_game.map().read_native_word(
                upstream_symbols.find("BG2SCROLL").front()));
        background_renderer.draw_bg2(title_ppu, title_scroll_x,
            title_scroll_y, title_bg2,
            starfox::render::TilePriorityPass::all);
        background_renderer.draw_bg1(title_ppu, title_bg1,
            starfox::render::TilePriorityPass::all);
        background_renderer.draw_bg3(title_ppu, title_bg3,
            starfox::render::TilePriorityPass::all);
        const auto visible_pixels = [](const auto& framebuffer) {
            return std::count_if(framebuffer.pixels().begin(),
                framebuffer.pixels().end(),
                [](std::uint8_t pixel) { return pixel != 0U; });
        };
        if (trace_ex_title) {
            const auto nonzero_vram = [&title_ppu](std::size_t begin,
                                                   std::size_t end) {
                return std::count_if(title_ppu.vram.begin() + begin,
                    title_ppu.vram.begin() + end,
                    [](std::uint8_t byte) { return byte != 0U; });
            };
            std::cerr << "EX title layers scroll=" << title_scroll_x << ','
                      << title_scroll_y << " bg1=" << visible_pixels(title_bg1)
                      << " bg2=" << visible_pixels(title_bg2)
                      << " bg3=" << visible_pixels(title_bg3)
                      << " vram[a,c)=" << nonzero_vram(0xa000U, 0xc000U)
                      << " [d,e)=" << nonzero_vram(0xd000U, 0xe000U)
                      << " [e,f)=" << nonzero_vram(0xe000U, 0xf000U)
                      << " [f,10)=" << nonzero_vram(0xf000U, 0x10000U)
                      << '\n';
        }
        if (starfox_ex_cartridge) {
            require(visible_pixels(title_bg1) > 50U
                        && visible_pixels(title_bg2) > 1'000U
                        && visible_pixels(title_bg3) > 1'000U,
                    "EX title layers did not preserve relocated transfers and native text");
        }
        if (!heard_long_title_music) {
            const auto music = upstream_symbols.find("BGM_MUSIC").front();
            const auto count = upstream_symbols.find("BGMCNT").front();
            const auto state = long_title_audio.state();
            std::cerr << "title BGM=$" << std::hex
                      << static_cast<unsigned>(
                          title_music_game.map().read_native_byte(music))
                      << " count=" << static_cast<unsigned>(
                          title_music_game.map().read_native_byte(count))
                      << " pc=$" << state.program_counter
                      << " kon=$" << static_cast<unsigned>(state.dsp_key_on)
                      << " flg=$" << static_cast<unsigned>(state.dsp_flags)
                      << " mvol=" << static_cast<int>(state.main_volume_left)
                      << ',' << static_cast<int>(state.main_volume_right)
                      << " ports=$" << static_cast<unsigned>(state.output_ports[0])
                      << ',' << static_cast<unsigned>(state.output_ports[1])
                      << ',' << static_cast<unsigned>(state.output_ports[2])
                      << ',' << static_cast<unsigned>(state.output_ports[3])
                      << std::dec << '\n';
        }
        require(title_audio.driver_loaded() && long_title_audio.driver_loaded()
                    && heard_long_title_music,
                "title sequence did not produce its original SPC music");

        if (starfox_ex_cartridge) {
            // TITLE.ASM's documented L+Select shortcut fades directly into
            // FOXY_CONTINUE_L with STOPCOUNTING=10. This must cross the
            // bounded title-map boundary without dropping the source model
            // viewer or following its persistent tail jump inside one call.
            starfox::simulation::GameSimulation title_model_game{
                upstream_rom, upstream_symbols, "TITLEMAP"};
            // EX first plays its source logo map; the shortcut belongs to the
            // regular title map that follows it.
            for (std::size_t tick = 0U; tick < 400U; ++tick) {
                static_cast<void>(title_model_game.tick({}));
            }
            constexpr auto title_model_chord = static_cast<
                starfox::input::ButtonMask>(
                    starfox::input::left_shoulder | starfox::input::select);
            auto title_model_tick = title_model_game.tick({
                title_model_chord, starfox::input::select, 0U});
            const auto stop_counting =
                upstream_symbols.find("STOPCOUNTING").front();
            const auto menu_selected =
                upstream_symbols.find("MENUSELECTED").front();
            const auto page_number =
                upstream_symbols.find("PAGENUMBER").front();
            const auto foxy_pointer =
                upstream_symbols.find("FOXY_PTR").front();
            const auto foxy_shape =
                upstream_symbols.find("FOXY_SHAPE").front();
            const auto a_wing = static_cast<std::uint16_t>(
                upstream_symbols.find("A_WING").front());
            auto heard_model_entry = std::find(
                title_model_tick.sound_effect_commands.begin(),
                title_model_tick.sound_effect_commands.end(), 0xabU)
                != title_model_tick.sound_effect_commands.end();
            require(title_model_game.flow_state()
                            == starfox::simulation::GameFlowState::title
                        && title_model_game.map().fade_direction() < 0
                        && title_model_game.map().read_native_byte(stop_counting)
                            == 10U
                        && title_model_game.map().read_native_byte(menu_selected)
                            == 1U
                        && title_model_game.map().read_native_byte(page_number)
                            == 3U,
                    "EX title L+Select did not begin its source model-test fade");
            for (std::size_t tick = 0U; tick < 48U
                 && title_model_game.flow_state()
                    == starfox::simulation::GameFlowState::title; ++tick) {
                title_model_tick = title_model_game.tick({});
                heard_model_entry = heard_model_entry
                    || std::find(title_model_tick.sound_effect_commands.begin(),
                        title_model_tick.sound_effect_commands.end(), 0xabU)
                        != title_model_tick.sound_effect_commands.end();
            }
            require(title_model_game.flow_state()
                            == starfox::simulation::GameFlowState::ex_pregame_menu
                        && title_model_game.map().read_native_byte(stop_counting)
                            == 10U
                        && title_model_game.map().read_native_byte(page_number)
                            == 3U
                        && title_model_game.map().read_native_word(foxy_pointer)
                            == 2U
                        && title_model_game.map().read_native_word(foxy_shape)
                            == a_wing
                        && title_model_game.map().native_model_draw().active
                        && title_model_game.map().native_model_draw().shape
                            == a_wing
                        && heard_model_entry
                        && title_model_game.map().unknown_superfx_launches().empty(),
                    "EX title shortcut did not enter the source model-test loop");
            // FOXY_CONTINUE draws the first model-test page under forced black
            // and manually reveals brightness 1..15 before accepting input.
            for (std::size_t frame = 0U; frame < 16U; ++frame) {
                title_model_game.present_frame();
            }
            static_cast<void>(title_model_game.tick({}));
            static_cast<void>(title_model_game.tick({
                starfox::input::start, starfox::input::start, 0U}));
            require(title_model_game.map().read_native_byte(stop_counting) == 2U
                        && title_model_game.map().read_native_byte(menu_selected)
                            == 2U
                        && title_model_game.map().read_native_byte(page_number)
                            == 3U
                        && !title_model_game.map().native_model_draw().active,
                    "EX title model test did not return to source menu page three");

            // TITLE.ASM's R+Select shortcut advances the live SNES mosaic
            // register once per fresh chord, from $11 through $ff and then
            // back to zero. Preserve both its LDOWN debounce and PPU write.
            starfox::simulation::GameSimulation title_mosaic_game{
                upstream_rom, upstream_symbols, "TITLEMAP"};
            for (std::size_t tick = 0U; tick < 300U; ++tick) {
                static_cast<void>(title_mosaic_game.tick({}));
            }
            constexpr auto title_mosaic_chord = static_cast<
                starfox::input::ButtonMask>(
                    starfox::input::right_shoulder | starfox::input::select);
            const auto mosaic_on =
                upstream_symbols.find("MOSAICON").front();
            static_cast<void>(title_mosaic_game.tick({
                title_mosaic_chord, starfox::input::select, 0U}));
            if (title_mosaic_game.map().read_native_byte(mosaic_on) != 0x11U
                || title_mosaic_game.map().ppu_state().mosaic != 0x11U) {
                std::cerr << "EX title mosaic diagnostic: state=$" << std::hex
                          << static_cast<unsigned>(
                              title_mosaic_game.map().read_native_byte(mosaic_on))
                          << " ppu=$" << static_cast<unsigned>(
                              title_mosaic_game.map().ppu_state().mosaic)
                          << " ldown=$" << static_cast<unsigned>(
                              title_mosaic_game.map().read_native_byte(
                                  upstream_symbols.find("LDOWN").front()))
                          << std::dec << '\n';
            }
            require(title_mosaic_game.map().read_native_byte(mosaic_on) == 0x11U
                        && title_mosaic_game.map().ppu_state().mosaic == 0x11U,
                    "EX title R+Select did not enable its first mosaic step");
            static_cast<void>(title_mosaic_game.tick({
                title_mosaic_chord, 0U, 0U}));
            require(title_mosaic_game.map().read_native_byte(mosaic_on) == 0x11U,
                    "EX title mosaic repeated while the chord remained held");
            static_cast<void>(title_mosaic_game.tick({}));
            static_cast<void>(title_mosaic_game.tick({
                title_mosaic_chord, starfox::input::select, 0U}));
            require(title_mosaic_game.map().read_native_byte(mosaic_on) == 0x12U
                        && title_mosaic_game.map().ppu_state().mosaic == 0x12U,
                    "EX title mosaic did not advance after a released chord");
            title_mosaic_game.map().write_native_byte(mosaic_on, 0xffU);
            title_mosaic_game.map().write_native_byte(0x002106U, 0xffU);
            static_cast<void>(title_mosaic_game.tick({}));
            static_cast<void>(title_mosaic_game.tick({
                title_mosaic_chord, starfox::input::select, 0U}));
            require(title_mosaic_game.map().read_native_byte(mosaic_on) == 0U
                        && title_mosaic_game.map().ppu_state().mosaic == 0U,
                    "EX title mosaic did not wrap from $ff back to disabled");

            // TITLE.ASM's R+A+B chord is the source-only entrance to Super
            // High Poly mode.  `right` is the SNES R shoulder here (the
            // d-pad direction is `tright` in the EX macros), and B must be a
            // fresh edge while all three buttons are held.
            starfox::simulation::GameSimulation title_high_poly_game{
                upstream_rom, upstream_symbols, "TITLEMAP"};
            for (std::size_t tick = 0U; tick < 300U; ++tick) {
                static_cast<void>(title_high_poly_game.tick({}));
            }
            constexpr auto title_high_poly_chord = static_cast<
                starfox::input::ButtonMask>(
                    starfox::input::right_shoulder
                    | starfox::input::a | starfox::input::b);
            const auto shp_mode = upstream_symbols.find("SHPMODE").front();
            auto title_high_poly_tick = title_high_poly_game.tick({
                title_high_poly_chord, starfox::input::b, 0U});
            // TRIGSE runs after this tick's three source IRQ phases, so its
            // queue entry reaches the SPC ports on the following tick.
            title_high_poly_tick = title_high_poly_game.tick({
                title_high_poly_chord, 0U, 0U});
            require(title_high_poly_game.map().read_native_byte(shp_mode) == 1U
                        && std::find(
                            title_high_poly_tick.sound_effect_commands.begin(),
                            title_high_poly_tick.sound_effect_commands.end(),
                            14U)
                            != title_high_poly_tick.sound_effect_commands.end(),
                    "EX title R+A+B did not enable Super High Poly mode");

            starfox::simulation::GameSimulation title_high_poly_edge_game{
                upstream_rom, upstream_symbols, "TITLEMAP"};
            for (std::size_t tick = 0U; tick < 300U; ++tick) {
                static_cast<void>(title_high_poly_edge_game.tick({}));
            }
            // B was already held on the preceding source frame, so adding
            // R+A now must fail JMP_LASTKEYDOWN's fresh-edge test.
            static_cast<void>(title_high_poly_edge_game.tick({
                starfox::input::b, starfox::input::b, 0U}));
            title_high_poly_tick = title_high_poly_edge_game.tick({
                title_high_poly_chord, 0U, 0U});
            require(title_high_poly_edge_game.map().read_native_byte(shp_mode)
                            == 0U
                        && std::find(
                            title_high_poly_tick.sound_effect_commands.begin(),
                            title_high_poly_tick.sound_effect_commands.end(),
                            14U)
                            == title_high_poly_tick.sound_effect_commands.end(),
                    "EX title Super High Poly shortcut ignored its B-edge gate");

            // Exercise the complete native Scope entrance, calibration and
            // restart path. This catches more than a synthetic packet test:
            // CONTINUE.ASM must select SCOPEMODE, branch to STOPCOUNTING=5,
            // GSTRATS2.ASM must consume Cursor as a fresh edge, and START
            // must then leave calibration through the normal EX restart.
            starfox::simulation::GameSimulation scope_menu_game{
                upstream_rom, upstream_symbols, "TITLEMAP"};
            for (std::size_t tick = 0U; tick < 300U; ++tick) {
                static_cast<void>(scope_menu_game.tick({}));
            }
            static_cast<void>(scope_menu_game.tick({
                starfox::input::start, starfox::input::start, 0U}));
            for (std::size_t tick = 0U; tick < 48U
                 && scope_menu_game.flow_state()
                    == starfox::simulation::GameFlowState::title; ++tick) {
                static_cast<void>(scope_menu_game.tick({}));
            }
            require(scope_menu_game.flow_state()
                            == starfox::simulation::GameFlowState::ex_pregame_menu
                        && scope_menu_game.map().read_native_byte(menu_selected)
                            == 15U,
                    "EX Scope test could not reach source pre-game page one");
            // CONTINUE.ASM owns a forced-black raster followed by brightness
            // 1..15 before it begins accepting page input.
            for (std::size_t frame = 0U; frame < 16U; ++frame) {
                scope_menu_game.present_frame();
            }
            for (std::size_t item = 0U; item < 12U; ++item) {
                static_cast<void>(scope_menu_game.tick({
                    starfox::input::down, starfox::input::down, 0U}));
            }
            require(scope_menu_game.map().read_native_byte(menu_selected) == 11U,
                    "EX source menu did not select SUPER SCOPE MODE");
            static_cast<void>(scope_menu_game.tick({
                starfox::input::right, starfox::input::right, 0U}));
            require(scope_menu_game.ex_scope_control_enabled(),
                    "EX source menu did not enable SUPER SCOPE MODE");
            for (std::size_t item = 0U; item < 4U; ++item) {
                static_cast<void>(scope_menu_game.tick({
                    starfox::input::down, starfox::input::down, 0U}));
            }
            static_cast<void>(scope_menu_game.tick({
                starfox::input::start, starfox::input::start, 0U}));
            require(scope_menu_game.flow_state()
                            == starfox::simulation::GameFlowState::ex_pregame_menu
                        && scope_menu_game.map().read_native_byte(stop_counting)
                            == 5U,
                    "EX Scope mode skipped its source calibration screen");
            const auto calibrated = upstream_symbols.find("CALIBRATED").front();
            const auto scope_h_offset =
                upstream_symbols.find("SCOPE_H_OFFSET").front();
            const auto scope_v_offset =
                upstream_symbols.find("SCOPE_V_OFFSET").front();
            scope_menu_game.set_mouse_input({0, 0, 0x02U, 0x70U, 0x50U});
            static_cast<void>(scope_menu_game.tick({}));
            require(scope_menu_game.map().read_native_byte(calibrated) == 1U
                        && scope_menu_game.map().read_native_byte(scope_h_offset)
                            == 0x1aU
                        && scope_menu_game.map().read_native_byte(scope_v_offset)
                            == 0x12U,
                    "PC Cursor button did not calibrate EX's native Scope offsets");
            scope_menu_game.set_mouse_input({0, 0, 0U, 0x70U, 0x50U});
            static_cast<void>(scope_menu_game.tick({
                starfox::input::start, starfox::input::start, 0U}));
            require(scope_menu_game.flow_state()
                            == starfox::simulation::GameFlowState::ex_pregame_menu,
                    "EX Scope calibration cut away before its source fade-out");
            for (std::size_t frame = 0U; frame < 16U; ++frame) {
                scope_menu_game.present_frame();
            }
            static_cast<void>(scope_menu_game.tick({}));
            require(scope_menu_game.flow_state()
                            == starfox::simulation::GameFlowState::controls_type,
                    "EX Scope calibration did not return through source restart");
        }

        starfox::simulation::GameSimulation controls_music_game{
            upstream_rom, upstream_symbols, "TITLEMAP"};
        starfox::audio::Spc700Audio controls_audio;
        for (std::size_t tick = 0; tick < 45U; ++tick) {
            const auto music_tick = controls_music_game.tick({});
            static_cast<void>(controls_audio.render_logic_tick(
                music_tick.audio_port_writes));
            controls_music_game.synchronize_apu_output_ports(
                controls_audio.output_ports());
        }
        const auto before_controls_upload =
            controls_music_game.map().apu_upload_generation();
        auto controls_music_tick = controls_music_game.tick(
            {0, starfox::input::start, 0});
        static_cast<void>(controls_audio.render_logic_tick(
            controls_music_tick.audio_port_writes));
        controls_music_game.synchronize_apu_output_ports(
            controls_audio.output_ports());
        if (starfox_ex_cartridge) {
            // START during EX's logo animation only skips to the regular title.
            // Wait for that handoff, then press START there to open the menu.
            for (std::size_t tick = 0; tick < 120U; ++tick) {
                controls_music_tick = controls_music_game.tick({});
                static_cast<void>(controls_audio.render_logic_tick(
                    controls_music_tick.audio_port_writes));
                controls_music_game.synchronize_apu_output_ports(
                    controls_audio.output_ports());
            }
            controls_music_tick = controls_music_game.tick(
                {0, starfox::input::start, 0});
            static_cast<void>(controls_audio.render_logic_tick(
                controls_music_tick.audio_port_writes));
            controls_music_game.synchronize_apu_output_ports(
                controls_audio.output_ports());
            for (std::size_t tick = 0U; tick < 48U
                 && controls_music_game.flow_state()
                    == starfox::simulation::GameFlowState::title; ++tick) {
                controls_music_tick = controls_music_game.tick({});
                static_cast<void>(controls_audio.render_logic_tick(
                    controls_music_tick.audio_port_writes));
                controls_music_game.synchronize_apu_output_ports(
                    controls_audio.output_ports());
            }
            for (std::size_t frame = 0U; frame < 16U; ++frame) {
                controls_music_game.present_frame();
            }
        }
        bool heard_controls_music = false;
        bool left_ex_menu = false;
        for (std::size_t tick = 0; tick < 300U; ++tick) {
            const auto action = starfox_ex_cartridge
                    && controls_music_game.flow_state()
                        == starfox::simulation::GameFlowState::ex_pregame_menu
                    && !left_ex_menu
                ? starfox::input::TickInput{
                    starfox::input::start, starfox::input::start, 0}
                : starfox::input::TickInput{};
            left_ex_menu = left_ex_menu
                || action.pressed == starfox::input::start;
            controls_music_tick = controls_music_game.tick(action);
            const auto controls_pcm = controls_audio.render_logic_tick(
                controls_music_tick.audio_port_writes);
            controls_music_game.synchronize_apu_output_ports(
                controls_audio.output_ports());
            heard_controls_music = heard_controls_music
                || std::any_of(controls_pcm.begin(), controls_pcm.end(),
                    [](std::int16_t sample) { return sample != 0; });
        }
        require(controls_music_game.flow_state()
                        == starfox::simulation::GameFlowState::controls_type
                    && controls_music_game.map().apu_upload_generation()
                        > before_controls_upload
                    && controls_audio.driver_loaded() && heard_controls_music,
                "controller screen did not replace title audio with OPS music");

        const auto expect_control_sound = [&](starfox::input::ButtonMask button,
                                               std::uint8_t expected_sound) {
            starfox::simulation::GameSimulation controls_action_game{
                upstream_rom, upstream_symbols, "CONTMAP"};
            starfox::audio::Spc700Audio controls_action_audio;
            for (std::size_t tick = 0; tick < 80U; ++tick) {
                const auto action_tick = controls_action_game.tick({});
                static_cast<void>(controls_action_audio.render_logic_tick(
                    action_tick.audio_port_writes));
                controls_action_game.synchronize_apu_output_ports(
                    controls_action_audio.output_ports());
            }
            auto saw = false;
            for (std::size_t tick = 0; tick < 8U; ++tick) {
                const auto held = tick == 0U ? button
                    : static_cast<starfox::input::ButtonMask>(0U);
                const auto pressed = tick == 0U ? button
                    : static_cast<starfox::input::ButtonMask>(0U);
                const auto action_tick = controls_action_game.tick(
                    {held, pressed, 0});
                saw = saw || std::find(action_tick.sound_effect_commands.begin(),
                    action_tick.sound_effect_commands.end(), expected_sound)
                        != action_tick.sound_effect_commands.end();
                static_cast<void>(controls_action_audio.render_logic_tick(
                    action_tick.audio_port_writes));
                controls_action_game.synchronize_apu_output_ports(
                    controls_action_audio.output_ports());
            }
            return saw;
        };
        require(expect_control_sound(starfox::input::y, 0x35U),
                "controller demo did not expose laser SFX");
        require(expect_control_sound(starfox::input::a, 0x31U),
                "controller demo did not expose bomb SFX");
        require(expect_control_sound(starfox::input::b, 0x33U),
                "controller demo did not expose brake SFX");
        require(expect_control_sound(starfox::input::x, 0x32U),
                "controller demo did not expose boost SFX");
        require(title_game.map().ppu_state().background_mode == 1U
                    && (title_game.map().ppu_state().main_screen & 0x04U) != 0U
                    && std::any_of(title_game.map().ppu_state().vram.begin(),
                        title_game.map().ppu_state().vram.end(),
                        [](std::uint8_t byte) { return byte != 0U; }),
                "title map did not install its original BG2/BG3 assets");
        const auto title_upload_generation =
            title_game.map().apu_upload_generation();
        if (starfox_ex_cartridge) {
            // Prove the host-owned title fade still performs TITLE.ASM's
            // final RANDOMIZEBG handoff instead of carrying stale menu state.
            title_game.map().write_native_byte(
                upstream_symbols.find("PGBG").front(), 99U);
        }
        const auto controls_transition = title_game.tick(
            {0, starfox::input::start, 0});
        static_cast<void>(title_audio.render_logic_tick(
            controls_transition.audio_port_writes));
        title_game.synchronize_apu_output_ports(title_audio.output_ports());
        const auto controls_map = upstream_symbols.find("CONTMAP").front();
        const auto map_bank = upstream_symbols.find("MAPBANK").front();
        const auto control_type = upstream_symbols.find("C_TYPE").front();
        const auto which_route = upstream_symbols.find("WHICHROUTE").front();
        const auto vanish_x = upstream_symbols.find("M_VANISHX").front();
        const auto vanish_y = upstream_symbols.find("M_VANISHY").front();
        require(title_game.flow_state()
                        == starfox::simulation::GameFlowState::title
                    && title_game.map().fade_direction() < 0,
                "title START did not begin its source fade-out");
        for (std::size_t tick = 0; tick < 48U
             && title_game.flow_state()
                 == starfox::simulation::GameFlowState::title; ++tick) {
            static_cast<void>(title_game.tick({}));
        }
        if (starfox_ex_cartridge) {
            const auto stop_counting = upstream_symbols.find("STOPCOUNTING").front();
            const auto menu_selected = upstream_symbols.find("MENUSELECTED").front();
            const auto page_number = upstream_symbols.find("PAGENUMBER").front();
            const auto ex_god_mode = upstream_symbols.find("GODMODE").front();
            const auto pgbg = upstream_symbols.find("PGBG").front();
            const auto vmap2 = upstream_symbols.find("VMAP2").front();
            const auto displayed_bitmap_base = static_cast<std::uint16_t>(
                title_game.map().read_native_word(vmap2) & 0xf000U);
            starfox::render::Framebuffer menu_background{256U, 224U};
            const auto bg2xscroll = upstream_symbols.find("BG2XSCROLL").front();
            const auto bg2scroll = upstream_symbols.find("BG2SCROLL").front();
            background_renderer.draw_bg2(title_game.map().ppu_state(),
                static_cast<std::int16_t>(title_game.map().read_native_word(
                    bg2xscroll)),
                static_cast<std::int16_t>(title_game.map().read_native_word(
                    bg2scroll)),
                menu_background, starfox::render::TilePriorityPass::low);
            background_renderer.draw_bg2(title_game.map().ppu_state(),
                static_cast<std::int16_t>(title_game.map().read_native_word(
                    bg2xscroll)),
                static_cast<std::int16_t>(title_game.map().read_native_word(
                    bg2scroll)),
                menu_background, starfox::render::TilePriorityPass::high);
            // The deterministic EX menu background is one of CONTINUE.ASM's
            // Mode 1 variants. Exercise the same wide BG2 sampling used by
            // the runtime so the added bands cannot silently fall back to
            // colour zero/black while the source canvas remains correct.
            starfox::render::Framebuffer expanded_menu_background{400U, 224U};
            background_renderer.draw_bg2(title_game.map().ppu_state(),
                static_cast<std::int16_t>(title_game.map().read_native_word(
                    bg2xscroll)),
                static_cast<std::int16_t>(title_game.map().read_native_word(
                    bg2scroll)),
                expanded_menu_background,
                starfox::render::TilePriorityPass::low, 72, true);
            background_renderer.draw_bg2(title_game.map().ppu_state(),
                static_cast<std::int16_t>(title_game.map().read_native_word(
                    bg2xscroll)),
                static_cast<std::int16_t>(title_game.map().read_native_word(
                    bg2scroll)),
                expanded_menu_background,
                starfox::render::TilePriorityPass::high, 72, true);
            auto expanded_side_pixels = std::size_t{};
            for (std::uint32_t y = 0U;
                 y < expanded_menu_background.height(); ++y) {
                for (std::uint32_t x = 0U;
                     x < expanded_menu_background.width(); ++x) {
                    if (x >= 72U && x < 328U) continue;
                    if (expanded_menu_background.get(
                            static_cast<std::int32_t>(x),
                            static_cast<std::int32_t>(y)) != 0U) {
                        ++expanded_side_pixels;
                    }
                }
            }
            starfox::render::Framebuffer menu_text{256U, 224U};
            background_renderer.draw_bg1(title_game.map().ppu_state(), menu_text,
                starfox::render::TilePriorityPass::low);
            background_renderer.draw_bg1(title_game.map().ppu_state(), menu_text,
                starfox::render::TilePriorityPass::high);
            require(title_game.flow_state()
                            == starfox::simulation::GameFlowState::ex_pregame_menu
                        && title_game.map().read_native_byte(stop_counting) == 8U
                        && title_game.map().read_native_byte(menu_selected) == 15U
                        && title_game.map().read_native_byte(page_number) == 1U
                        && title_game.map().read_native_byte(pgbg) <= 36U
                        && title_game.map().ppu_state().background_mode == 1U
                        && (title_game.map().ppu_state().main_screen & 0x01U)
                            != 0U
                        && title_game.map().ppu_state().bg1_character_base
                            == displayed_bitmap_base
                        && std::count_if(menu_background.pixels().begin(),
                            menu_background.pixels().end(),
                            [](std::uint8_t pixel) { return pixel != 0U; })
                            > 10'000
                        && std::count_if(menu_text.pixels().begin(),
                            menu_text.pixels().end(),
                            [](std::uint8_t pixel) { return pixel != 0U; })
                            > 1'000
                        && expanded_side_pixels > 1'000U
                        && std::any_of(title_game.map().ppu_state().vram.begin(),
                            title_game.map().ppu_state().vram.end(),
                            [](std::uint8_t byte) { return byte != 0U; }),
                    "EX title START did not open source pre-game menu page one");
            for (std::size_t frame = 0U; frame < 16U; ++frame) {
                title_game.present_frame();
            }
            const auto menu_scroll_before =
                title_game.map().ppu_state().bg2_scroll_x;
            static_cast<void>(title_game.tick({}));
            require(title_game.map().ppu_state().bg2_scroll_x
                            != menu_scroll_before,
                    "EX source menu did not advance its BG2 scroll register");
            const auto fps_speed = upstream_symbols.find("FPSSPEED").front();
            const auto ntsc_pal_swap =
                upstream_symbols.find("NTSCPALSWAP").front();
            title_game.map().write_native_byte(stop_counting, 20U);
            title_game.map().write_native_byte(menu_selected, 0U);
            title_game.map().write_native_byte(ntsc_pal_swap, 1U);
            static_cast<void>(title_game.tick(
                {starfox::input::right, starfox::input::right, 0U}));
            require(title_game.map().read_native_byte(ntsc_pal_swap) == 0U,
                    "disabled EX REGION option escaped the PC's NTSC clock");
            title_game.map().write_native_byte(menu_selected, 1U);
            title_game.map().write_native_byte(fps_speed, 2U);
            static_cast<void>(title_game.tick(
                {starfox::input::right, starfox::input::right, 0U}));
            require(title_game.map().read_native_byte(fps_speed) == 0U,
                    "disabled EX MAX FPS option escaped its stable 20 mode");
            title_game.map().write_native_byte(stop_counting, 8U);
            title_game.map().write_native_byte(menu_selected, 15U);
            title_game.map().write_native_byte(page_number, 1U);
            static_cast<void>(title_game.tick({}));
            static_cast<void>(title_game.tick(
                {starfox::input::down, starfox::input::down, 0}));
            require(title_game.map().read_native_byte(menu_selected) == 0U,
                    "EX pre-game menu DOWN did not wrap to God Mode");
            static_cast<void>(title_game.tick(
                {starfox::input::right, starfox::input::right, 0}));
            require(title_game.map().read_native_byte(ex_god_mode) != 0U
                        && title_game.god_mode(),
                    "EX pre-game menu did not enable its source God Mode");
            static_cast<void>(title_game.tick(
                {starfox::input::left, starfox::input::left, 0}));
            require(title_game.map().read_native_byte(ex_god_mode) == 0U
                        && !title_game.god_mode(),
                    "EX pre-game menu did not disable its source God Mode");
            static_cast<void>(title_game.tick(
                {starfox::input::right, starfox::input::right, 0}));
            require(title_game.map().read_native_byte(ex_god_mode) != 0U
                        && title_game.god_mode(),
                    "EX pre-game menu did not restore its source God Mode");
            static_cast<void>(title_game.tick(
                {starfox::input::right_shoulder,
                    starfox::input::right_shoulder, 0}));
            require(title_game.map().read_native_byte(stop_counting) == 9U
                        && title_game.map().read_native_byte(page_number) == 2U,
                    "EX pre-game menu R did not open source page two");
            static_cast<void>(title_game.tick(
                {starfox::input::right_shoulder,
                    starfox::input::right_shoulder, 0}));
            require(title_game.map().read_native_byte(stop_counting) == 2U
                        && title_game.map().read_native_byte(page_number) == 3U
                        && title_game.map().read_native_byte(menu_selected) == 18U,
                    "EX pre-game menu R did not open source page three");
            static_cast<void>(title_game.tick(
                {starfox::input::left_shoulder,
                    starfox::input::left_shoulder, 0}));
            require(title_game.map().read_native_byte(stop_counting) == 9U
                        && title_game.map().read_native_byte(page_number) == 2U
                        && title_game.map().read_native_byte(menu_selected) == 18U,
                    "EX pre-game menu L did not return to source page two");
            static_cast<void>(title_game.tick(
                {starfox::input::right_shoulder,
                    starfox::input::right_shoulder, 0}));
            require(title_game.map().read_native_byte(stop_counting) == 2U
                        && title_game.map().read_native_byte(page_number) == 3U
                        && title_game.map().read_native_byte(menu_selected) == 18U,
                    "EX pre-game menu R did not return to source page three");
            for (std::size_t item = 0; item < 4U; ++item) {
                static_cast<void>(title_game.tick(
                    {starfox::input::down, starfox::input::down, 0}));
            }
            require(title_game.map().read_native_byte(menu_selected) == 2U,
                    "EX pre-game menu did not select MODEL VIEWER");
            const auto unknown_before_model =
                title_game.map().unknown_superfx_launches().size();
            static_cast<void>(title_game.tick(
                {starfox::input::start, starfox::input::start, 0}));
            const auto a_wing = static_cast<std::uint16_t>(
                upstream_symbols.find("A_WING").front());
            require(title_game.map().read_native_byte(stop_counting) == 12U
                        && title_game.map().native_model_draw().active
                        && title_game.map().native_model_draw().shape == a_wing
                        && title_game.map().native_model_draw().z == 350
                        && title_game.map().native_model_draw().colour_table == 0U
                        && title_game.map().unknown_superfx_launches().size()
                            == unknown_before_model,
                    "EX MODEL VIEWER did not submit its source-header MSHOWOBJ3 model");
            const auto model_z_before =
                title_game.map().read_native_word(
                    upstream_symbols.find("M_BIGZ").front());
            static_cast<void>(title_game.tick(
                {starfox::input::left_shoulder,
                    starfox::input::left_shoulder, 0}));
            const auto zoomed_in_model_z =
                title_game.map().read_native_word(
                    upstream_symbols.find("M_BIGZ").front());
            require(zoomed_in_model_z < model_z_before,
                    "EX MODEL VIEWER L did not zoom the source model in");
            static_cast<void>(title_game.tick(
                {starfox::input::right_shoulder,
                    starfox::input::right_shoulder, 0}));
            require(title_game.map().read_native_word(
                        upstream_symbols.find("M_BIGZ").front())
                            > zoomed_in_model_z,
                    "EX MODEL VIEWER R did not zoom the source model out");
            static_cast<void>(title_game.tick({}));
            static_cast<void>(title_game.tick(
                {starfox::input::start, starfox::input::start, 0}));
            require(title_game.map().read_native_byte(stop_counting) == 2U
                        && title_game.map().read_native_byte(page_number) == 3U
                        && title_game.map().read_native_byte(menu_selected) == 2U
                        && !title_game.map().native_model_draw().active,
                    "EX MODEL VIEWER did not return to source page three");
            static_cast<void>(title_game.tick(
                {starfox::input::right_shoulder,
                    starfox::input::right_shoulder, 0}));
            require(title_game.map().read_native_byte(stop_counting) == 8U
                        && title_game.map().read_native_byte(page_number) == 1U
                        && title_game.map().read_native_byte(menu_selected) == 14U,
                    "EX pre-game menu L did not return to source page one");
            static_cast<void>(title_game.tick(
                {starfox::input::down, starfox::input::down, 0}));
            require(title_game.map().read_native_byte(menu_selected) == 15U,
                    "EX pre-game menu did not reselect START GAME");
            static_cast<void>(title_game.tick(
                {starfox::input::start, starfox::input::start, 0}));
            require(title_game.map().read_native_byte(ex_god_mode) != 0U
                        && title_game.god_mode()
                        && title_game.flow_state()
                            == starfox::simulation::GameFlowState::ex_pregame_menu,
                    "EX restart did not preserve its source menu options");
            for (std::size_t frame = 0U; frame < 16U; ++frame) {
                title_game.present_frame();
            }
            static_cast<void>(title_game.tick({}));
            const auto committed_ex_ram = std::vector<std::uint8_t>{
                title_game.ex_save_ram().begin(),
                title_game.ex_save_ram().end()};
            require(committed_ex_ram.size()
                            == starfox::simulation::Wdc65816::cartridge_ram_size
                        && committed_ex_ram[0xfffcU] == 'S'
                        && committed_ex_ram[0xfffdU] == 'F'
                        && committed_ex_ram[0xfffeU] == 'E'
                        && committed_ex_ram[0xffffU] == 'X'
                        && committed_ex_ram[0xf006U] != 0U,
                    "EX restart did not commit God Mode to source SRAM");
            starfox::simulation::GameSimulation restored_ex_game{
                upstream_rom, upstream_symbols, "LEVEL1_1", committed_ex_ram};
            require(restored_ex_game.map().read_native_byte(ex_god_mode) != 0U
                        && std::equal(committed_ex_ram.begin(),
                            committed_ex_ram.end(),
                            restored_ex_game.ex_save_ram().begin()),
                    "EX source SRAM settings did not survive a new runtime");

            // INTRO.ASM owns EX's documented L+R+DOWN+B SRAM reset. Drive
            // the host boot fade while holding that exact chord so the
            // inline source code sees it at the first intro map record.
            starfox::simulation::GameSimulation reset_ex_game{
                upstream_rom, upstream_symbols, "BOOT", committed_ex_ram};
            reset_ex_game.set_experience(
                starfox::simulation::Experience::starfox_ex);
            constexpr auto reset_chord = static_cast<
                starfox::input::ButtonMask>(
                    starfox::input::left_shoulder
                    | starfox::input::right_shoulder
                    | starfox::input::down | starfox::input::b);
            auto heard_reset_sound = false;
            for (std::size_t tick = 0; tick < 400U; ++tick) {
                const auto reset_tick = reset_ex_game.tick({
                    reset_chord,
                    static_cast<starfox::input::ButtonMask>(
                        tick == 0U ? starfox::input::start : 0U),
                    0U});
                heard_reset_sound = heard_reset_sound
                    || std::find(reset_tick.sound_effect_commands.begin(),
                        reset_tick.sound_effect_commands.end(), 0xa0U)
                        != reset_tick.sound_effect_commands.end();
                if (reset_ex_game.ex_save_ram()[0xf006U] == 0U
                    && heard_reset_sound) break;
            }
            require(reset_ex_game.flow_state()
                            == starfox::simulation::GameFlowState::intro
                        && reset_ex_game.ex_save_ram()[0xf006U] == 0U
                        && reset_ex_game.ex_save_ram()[0xfffcU] == 'S'
                        && heard_reset_sound,
                    "EX intro did not execute its source SRAM-reset chord");
            auto saw_ex_intro_dialogue = false;
            auto saw_ex_intro_secondary_portrait = false;
            for (std::size_t tick = 0; tick < 400U; ++tick) {
                static_cast<void>(reset_ex_game.tick({}));
                const auto intro_dialogue = reset_ex_game.dialogue_state();
                saw_ex_intro_dialogue = saw_ex_intro_dialogue
                    || (intro_dialogue.active
                        && intro_dialogue.text_visible
                        && intro_dialogue.text_address != 0U);
                saw_ex_intro_secondary_portrait =
                    saw_ex_intro_secondary_portrait
                    || (intro_dialogue.active
                        && intro_dialogue.text_visible
                        && intro_dialogue.alternate_portraits);
            }
            require(saw_ex_intro_dialogue
                        && saw_ex_intro_secondary_portrait,
                "EX intro did not present its source communication channel");
        }
        const auto model_palette =
            upstream_symbols.find("PAL0PALETTE").front() + 7U * 32U;
        auto controller_model_palette_present = false;
        for (std::uint32_t colour = 0U; colour < 16U; ++colour) {
            controller_model_palette_present =
                controller_model_palette_present
                || (title_game.map().read_native_word(
                        model_palette + colour * 2U) & 0x7fffU) != 0U;
        }
        require(title_game.flow_state()
                        == starfox::simulation::GameFlowState::controls_type
                    && title_game.map().read_native_byte(map_bank)
                        == static_cast<std::uint8_t>(controls_map >> 16U)
                    && title_game.map().read_native_word(vanish_x) == 64U
                    && title_game.map().read_native_word(vanish_y) == 48U
                    && std::any_of(
                        title_game.map().ppu_state().vram.begin() + 0xd000U,
                        title_game.map().ppu_state().vram.end(),
                        [](std::uint8_t byte) { return byte != 0U; })
                    && controller_model_palette_present
                    && title_game.map().apu_upload_generation()
                        > title_upload_generation,
                "title/menu START did not install the original controller screen");
        // CONT_L's SPC upload and hidden CONTMAP setup are presentation-time
        // work. Let the forced-black reveal interval finish before driving
        // the visible controller menu.
        for (std::size_t frame = 0; frame < 90U; ++frame) {
            title_game.present_frame();
        }
        if (starfox_ex_cartridge) {
            const auto current_ship = upstream_symbols.find("CURR_SHIP").front();
            const auto key_right = upstream_symbols.find("KEYRDOWN").front();
            const auto key_left = upstream_symbols.find("KEYLDOWN").front();
            const auto original_ship = title_game.map().read_native_byte(
                current_ship);
            constexpr auto next_ship_combo =
                static_cast<starfox::input::ButtonMask>(
                    starfox::input::x | starfox::input::right_shoulder);
            static_cast<void>(title_game.tick(
                {next_ship_combo, next_ship_combo, 0}));
            const auto next_ship = title_game.map().read_native_byte(current_ship);
            require(next_ship != original_ship
                        && title_game.map().read_native_word(key_right) == 1U,
                    "EX controller X+R did not invoke its source next-ship selector");
            static_cast<void>(title_game.tick({next_ship_combo, 0, 0}));
            require(title_game.map().read_native_byte(current_ship) == next_ship,
                    "EX controller ship selector repeated before X was released");
            static_cast<void>(title_game.tick(
                {0, 0, starfox::input::x}));
            require(title_game.map().read_native_word(key_right) == 0U,
                    "EX controller next-ship debounce did not release with X");

            constexpr auto previous_ship_combo =
                static_cast<starfox::input::ButtonMask>(
                    starfox::input::x | starfox::input::left_shoulder);
            static_cast<void>(title_game.tick(
                {previous_ship_combo, previous_ship_combo, 0}));
            require(title_game.map().read_native_byte(current_ship) == original_ship
                        && title_game.map().read_native_word(key_left) == 1U,
                    "EX controller X+L did not invoke its source previous-ship selector");
            static_cast<void>(title_game.tick({}));
        }
        const auto old_control_type = title_game.map().read_native_byte(control_type);
        static_cast<void>(title_game.tick(
            {0, starfox::input::select, 0}));
        require(title_game.map().read_native_byte(control_type)
                    == static_cast<std::uint8_t>((old_control_type + 1U) & 3U),
                "controller screen SELECT did not cycle the source control type");
        for (std::size_t tick = 1; tick < 16; ++tick) {
            static_cast<void>(title_game.tick({}));
        }
        static_cast<void>(title_game.tick({0, starfox::input::start, 0}));
        require(title_game.flow_state()
                    == starfox::simulation::GameFlowState::controls_choice,
                "controller screen START did not enter training/game selection");
        static_cast<void>(title_game.tick({0, starfox::input::start, 0}));
        require(title_game.flow_state()
                        == starfox::simulation::GameFlowState::controls_choice
                    && title_game.map().fade_direction() < 0,
                "TRAINING confirmation did not begin its source fade-out");
        for (std::size_t tick = 0; tick < 12U
             && title_game.flow_state()
                 == starfox::simulation::GameFlowState::controls_choice; ++tick) {
            static_cast<void>(title_game.tick({}));
        }
        require(title_game.flow_state()
                        == starfox::simulation::GameFlowState::training
                    && title_game.map().read_native_byte(map_bank)
                        == static_cast<std::uint8_t>(
                            upstream_symbols.find("TRAININGMAP").front() >> 16U),
                "default TRAINING choice did not enter the original training map");
        for (std::size_t tick = 1; tick < 20; ++tick) {
            static_cast<void>(title_game.tick({}));
        }
        static_cast<void>(title_game.tick({0, starfox::input::start, 0}));
        for (std::size_t tick = 0; tick < 12U
             && title_game.flow_state()
                 == starfox::simulation::GameFlowState::training; ++tick) {
            static_cast<void>(title_game.tick({}));
        }
        require(title_game.flow_state()
                    == starfox::simulation::GameFlowState::controls_choice,
                "training START exit did not return to the source GAME/TRAINING choice");
        for (std::size_t frame = 0; frame < 90U; ++frame) {
            title_game.present_frame();
        }
        const auto unknown_superfx_before_planets =
            title_game.map().unknown_superfx_launches().size();
        static_cast<void>(title_game.tick({0,
            static_cast<starfox::input::ButtonMask>(
                starfox::input::down | starfox::input::start), 0}));
        for (std::size_t tick = 0; tick < 12U
             && title_game.flow_state()
                 == starfox::simulation::GameFlowState::controls_choice; ++tick) {
            static_cast<void>(title_game.tick({}));
        }
        const auto entered_planet_selector = title_game.flow_state()
                        == starfox::simulation::GameFlowState::planet_select
                    && title_game.map().read_native_word(stage_address.front()) == 0U
                    && title_game.map().read_native_byte(which_route)
                        == static_cast<std::uint8_t>(
                            starfox_ex_cartridge ? 4U : 1U)
                    && title_game.map().ppu_state().background_mode == 3U
                    && title_game.map().unknown_superfx_launches().size()
                        == unknown_superfx_before_planets;
        require(entered_planet_selector,
                "GAME choice did not enter the original planet route selector");
        // PLANETS.ASM's bespoke fade-in takes eight 60 Hz presentations;
        // route input is intentionally ignored until it completes.
        for (std::size_t frame = 0; frame < 8U; ++frame) {
            title_game.present_frame();
        }
        static_cast<void>(title_game.tick({0, starfox::input::start, 0}));
        const auto title_selected_map = static_cast<std::uint32_t>(
            title_game.map().read_native_byte(new_map_address.front()))
            | (static_cast<std::uint32_t>(title_game.map().read_native_byte(
                   new_map_address.front() + 1U)) << 8U)
            | (static_cast<std::uint32_t>(title_game.map().read_native_byte(
                   new_map_address.front() + 2U)) << 16U);
        require(title_game.flow_state()
                        == starfox::simulation::GameFlowState::planet_travel
                    && title_selected_map == upstream_symbols.find(
                        starfox_ex_cartridge ? "LEVEL5_1" : "LEVEL1_1").front(),
                "route selector did not enter the original briefing travel");
        auto saw_pepper_briefing = false;
        auto saw_briefing_graphics = false;
        auto saw_full_size_zoom_planet = false;
        auto briefing_cadence_matches_ntsc = true;
        std::size_t briefing_presentation_frames = 0U;
        std::uint16_t largest_planet_radius = 0U;
        const auto planet_radius = upstream_symbols.find("M_RADIUS").front();
        for (std::size_t frame = 0;
             frame < 3'000U
                 && title_game.flow_state()
                    == starfox::simulation::GameFlowState::planet_travel;
             ++frame) {
            title_game.present_frame();
            const auto briefing = title_game.briefing_state();
            saw_pepper_briefing = saw_pepper_briefing || briefing.active;
            if (briefing.active) {
                ++briefing_presentation_frames;
                if (briefing_presentation_frames <= 60U
                    && (briefing.visible_planet_characters != 0U
                        || briefing.visible_message_characters != 0U)) {
                    briefing_cadence_matches_ntsc = false;
                }
                if (briefing_presentation_frames == 63U
                    && briefing.visible_planet_characters != 1U) {
                    briefing_cadence_matches_ntsc = false;
                }
            }
            largest_planet_radius = std::max(largest_planet_radius,
                title_game.map().read_native_word(planet_radius));
            if (!saw_full_size_zoom_planet
                && title_game.map().read_native_word(planet_radius) >= 55U
                && title_game.map().ppu_state().background_mode == 3U) {
                starfox::render::Framebuffer zoom_frame{256, 224};
                background_renderer.draw_bg1(
                    title_game.map().ppu_state(), zoom_frame);
                std::int32_t min_x = 256;
                std::int32_t min_y = 224;
                std::int32_t max_x = -1;
                std::int32_t max_y = -1;
                for (std::int32_t y = 0; y < 224; ++y) {
                    for (std::int32_t x = 0; x < 256; ++x) {
                        if (zoom_frame.get(x, y) == 0U) continue;
                        min_x = std::min(min_x, x);
                        min_y = std::min(min_y, y);
                        max_x = std::max(max_x, x);
                        max_y = std::max(max_y, y);
                    }
                }
                saw_full_size_zoom_planet = max_x - min_x >= 100
                    && max_y - min_y >= 100;
            }
            saw_briefing_graphics = saw_briefing_graphics
                || std::any_of(
                    title_game.map().ppu_state().vram.begin() + 0xd040U,
                    title_game.map().ppu_state().vram.begin() + 0xe000U,
                    [](std::uint8_t byte) { return byte != 0U; });
            if ((frame % 3U) == 2U) static_cast<void>(title_game.tick({}));
        }
        require(title_game.flow_state() == starfox::simulation::GameFlowState::gameplay,
                "first Pepper briefing did not launch Corneria");
        require(saw_pepper_briefing && saw_briefing_graphics
                    && saw_full_size_zoom_planet
                    && briefing_cadence_matches_ntsc
                    && largest_planet_radius > 32U,
                "planet selection skipped or mistimed its Fox/Pepper presentation");

        starfox::simulation::GameSimulation planet_sprite_game{
            upstream_rom, upstream_symbols, "PLANETSELECT"};
        for (std::size_t frame = 0U; frame < 8U; ++frame) {
            planet_sprite_game.present_frame();
        }
        const auto object_record_is_empty = [](const auto& oam,
                                                std::size_t object) {
            const auto offset = object * 4U;
            return oam[offset] == 0U && oam[offset + 1U] == 0U
                && oam[offset + 2U] == 0U && oam[offset + 3U] == 0U;
        };
        const auto& initial_planet_oam =
            planet_sprite_game.map().ppu_state().oam;
        auto has_ship_pieces = true;
        for (std::size_t object = 4U; object < 8U; ++object) {
            has_ship_pieces = has_ship_pieces
                && !object_record_is_empty(initial_planet_oam, object);
        }
        auto has_route_dots = false;
        for (std::size_t object = 8U; object < 28U; ++object) {
            const auto offset = object * 4U;
            has_route_dots = has_route_dots
                || (initial_planet_oam[offset] != 0xf8U
                    && initial_planet_oam[offset + 1U] != 0xf8U);
        }
        starfox::render::Framebuffer planet_objects{256, 224};
        sprite_renderer.draw_objects(
            planet_sprite_game.map().ppu_state(), planet_objects);
        auto has_visible_planet_objects = false;
        for (std::int32_t y = 0; y < 224 && !has_visible_planet_objects; ++y) {
            for (std::int32_t x = 0; x < 256; ++x) {
                if (planet_objects.get(x, y) != 0U) {
                    has_visible_planet_objects = true;
                    break;
                }
            }
        }
        require(has_ship_pieces && has_route_dots && has_visible_planet_objects,
                "planet selector omitted the source ship/route OBJ tiles");

        for (std::size_t frame = 0U; frame < 6U; ++frame) {
            planet_sprite_game.present_frame();
        }
        auto route_is_hidden = true;
        for (std::size_t object = 8U; object < 28U; ++object) {
            const auto offset = object * 4U;
            const auto& oam = planet_sprite_game.map().ppu_state().oam;
            route_is_hidden = route_is_hidden && oam[offset] == 0xf8U
                && oam[offset + 1U] == 0xf8U;
        }
        for (std::size_t frame = 0U; frame < 6U; ++frame) {
            planet_sprite_game.present_frame();
        }
        auto route_is_visible_again = false;
        for (std::size_t object = 8U; object < 28U; ++object) {
            const auto offset = object * 4U;
            const auto& oam = planet_sprite_game.map().ppu_state().oam;
            route_is_visible_again = route_is_visible_again
                || (oam[offset] != 0xf8U && oam[offset + 1U] != 0xf8U);
        }
        require(route_is_hidden && route_is_visible_again,
                "planet route did not preserve its source blinking cadence");

        static_cast<void>(planet_sprite_game.tick(
            {0, starfox::input::start, 0}));
        planet_sprite_game.present_frame();
        const auto first_flash_visible = !object_record_is_empty(
            planet_sprite_game.map().ppu_state().oam, 4U);
        planet_sprite_game.present_frame();
        auto second_flash_hidden = true;
        for (std::size_t object = 4U; object < 8U; ++object) {
            second_flash_hidden = second_flash_hidden && object_record_is_empty(
                planet_sprite_game.map().ppu_state().oam, object);
        }
        require(first_flash_visible && second_flash_hidden,
                "selected planet ship did not flash before the zoom sequence");

        starfox::simulation::GameSimulation planet_music_game{
            upstream_rom, upstream_symbols, "PLANETSELECT"};
        const auto planet_rotations = upstream_symbols.find("ROTY1").front();
        std::array<std::uint16_t, 6> initial_planet_rotations{};
        for (std::size_t planet = 0; planet < initial_planet_rotations.size();
             ++planet) {
            initial_planet_rotations[planet] =
                planet_music_game.map().read_native_word(
                    planet_rotations + static_cast<std::uint32_t>(planet * 2U));
        }
        planet_music_game.present_frame();
        planet_music_game.present_frame();
        auto planet_rotation_waited_for_20_hz_phase = true;
        for (std::size_t planet = 0; planet < initial_planet_rotations.size();
             ++planet) {
            planet_rotation_waited_for_20_hz_phase =
                planet_rotation_waited_for_20_hz_phase
                && planet_music_game.map().read_native_word(
                    planet_rotations + static_cast<std::uint32_t>(planet * 2U))
                    == initial_planet_rotations[planet];
        }
        planet_music_game.present_frame();
        constexpr std::array<std::int32_t, 6> planet_rotation_steps{
            6 * 256, -3 * 256, 4 * 256, 3 * 256, -5 * 256, -5 * 256,
        };
        auto planet_rotation_advanced_at_20_hz = true;
        for (std::size_t planet = 0; planet < initial_planet_rotations.size();
             ++planet) {
            const auto expected = static_cast<std::uint16_t>(
                initial_planet_rotations[planet]
                + planet_rotation_steps[planet] / 2);
            planet_rotation_advanced_at_20_hz =
                planet_rotation_advanced_at_20_hz
                && planet_music_game.map().read_native_word(
                    planet_rotations + static_cast<std::uint32_t>(planet * 2U))
                    == expected;
        }
        for (std::size_t frame = 0; frame < 3U; ++frame) {
            planet_music_game.present_frame();
        }
        auto planet_rotation_cadence_matches = true;
        for (std::size_t planet = 0; planet < initial_planet_rotations.size();
             ++planet) {
            const auto expected = static_cast<std::uint16_t>(
                initial_planet_rotations[planet] + planet_rotation_steps[planet]);
            planet_rotation_cadence_matches = planet_rotation_cadence_matches
                && planet_music_game.map().read_native_word(
                    planet_rotations + static_cast<std::uint32_t>(planet * 2U))
                    == expected;
        }
        require(planet_rotation_waited_for_20_hz_phase
                    && planet_rotation_advanced_at_20_hz
                    && planet_rotation_cadence_matches,
                "planet rotation did not preserve one source SPINPLANETS step "
                "on a fixed 20 Hz cadence across six 60 Hz presentations");
        starfox::audio::Spc700Audio planet_audio;
        auto heard_planet_music = false;
        for (std::size_t frame = 0; frame < 600U; ++frame) {
            planet_music_game.present_frame();
            if ((frame % 3U) != 2U) continue;
            const auto planet_tick = planet_music_game.tick({});
            const auto planet_pcm = planet_audio.render_logic_tick(
                planet_tick.audio_port_writes);
            planet_music_game.synchronize_apu_output_ports(
                planet_audio.output_ports());
            heard_planet_music = heard_planet_music
                || std::any_of(planet_pcm.begin(), planet_pcm.end(),
                    [](std::int16_t sample) { return sample != 0; });
        }
        require(planet_audio.driver_loaded() && heard_planet_music,
                "planet selector did not produce its source map music");

        starfox::simulation::GameSimulation attract_game{
            upstream_rom, upstream_symbols, "TITLEMAP"};
        for (std::size_t tick = 0; tick < 880U; ++tick) {
            static_cast<void>(attract_game.tick({}));
        }
        for (std::size_t tick = 0; tick < 48U
             && attract_game.flow_state()
                 == starfox::simulation::GameFlowState::title; ++tick) {
            static_cast<void>(attract_game.tick({}));
        }
        require(attract_game.flow_state() == starfox::simulation::GameFlowState::intro
                    && attract_game.map().read_native_byte(map_bank)
                        == static_cast<std::uint8_t>(
                            upstream_symbols.find("INTROMAP").front() >> 16U),
                "title timeout did not enter the original attract-mode map");
        const auto attract_skip_ticks = starfox_ex_cartridge ? 44U : 29U;
        for (std::size_t tick = 0; tick < attract_skip_ticks; ++tick) {
            static_cast<void>(attract_game.tick({}));
        }
        static_cast<void>(attract_game.tick({0, starfox::input::start, 0}));
        for (std::size_t tick = 0; tick < 12U
             && attract_game.flow_state()
                 == starfox::simulation::GameFlowState::intro; ++tick) {
            static_cast<void>(attract_game.tick({}));
        }
        require(attract_game.flow_state() == starfox::simulation::GameFlowState::title
                    && attract_game.map().read_native_byte(map_bank)
                        == static_cast<std::uint8_t>(
                            upstream_symbols.find("TITLEMAP").front() >> 16U),
                "attract-mode input did not return to the title map");

        starfox::simulation::GameSimulation boot_game{
            upstream_rom, upstream_symbols, "BOOT"};
        require(boot_game.flow_state()
                    == starfox::simulation::GameFlowState::pregame_menu
                    && boot_game.timing_mode()
                        == starfox::simulation::TimingMode::original_speed
                    && boot_game.display_mode()
                        == starfox::simulation::DisplayMode::standard_4_3
                    && boot_game.presentation_fps() == 60U
                    && boot_game.pregame_selection() == 0U
                    && boot_game.pregame_page()
                        == starfox::simulation::PregamePage::main
                    && boot_game.experience()
                        == starfox::simulation::Experience::original
                    && !boot_game.god_mode()
                    && !boot_game.show_fps()
                     && !boot_game.anti_aliasing()
                     && !boot_game.enhanced_graphics()
                     && !boot_game.smooth_polys()
                    && !boot_game.rtx_lighting()
                    && !boot_game.vsync()
                    && boot_game.renderer_mode()
                        == starfox::simulation::RendererMode::gpu
                    && boot_game.crosshair_colour()
                        == starfox::simulation::CrosshairColour::green,
                "cold boot did not begin with the default pre-game settings");
        starfox::simulation::GameSimulation missing_msu_game{
            upstream_rom, upstream_symbols, "BOOT"};
        missing_msu_game.set_msu1_available(false);
        for (std::size_t row = 0U; row < 5U; ++row) {
            static_cast<void>(missing_msu_game.tick(
                {0, starfox::input::down, 0}));
        }
        static_cast<void>(missing_msu_game.tick(
            {0, starfox::input::a, 0}));
        require(!missing_msu_game.msu1_available()
                    && !missing_msu_game.msu1_music(),
                "missing MSU-1 companion did not disable its menu option");
        starfox::audio::Spc700Audio boot_audio;
        bool heard_start_sound = false;
        const auto drive_boot = [&](const starfox::input::TickInput& input) {
            const auto boot_tick = boot_game.tick(input);
            heard_start_sound = heard_start_sound
                || std::find(boot_tick.sound_effect_commands.begin(),
                        boot_tick.sound_effect_commands.end(), 0x10U)
                    != boot_tick.sound_effect_commands.end();
            static_cast<void>(boot_audio.render_logic_tick(
                boot_tick.audio_port_writes));
            boot_game.synchronize_apu_output_ports(boot_audio.output_ports());
        };
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.experience()
                    == starfox::simulation::Experience::starfox_ex,
                "pre-game experience selector did not enable Star Fox EX");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.experience()
                    == starfox::simulation::Experience::original,
                "pre-game experience selector did not return to Original");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 1U,
                "pre-game cursor did not reach PACE/SPEED");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.timing_mode()
                    == starfox::simulation::TimingMode::unlocked_20_fps,
                "pre-game pace selector did not leave the Original default");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 2U,
                "pre-game cursor did not reach RENDER FPS");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.presentation_fps() == 90U,
                "pre-game render selector did not place 90 FPS after 60 FPS");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.presentation_fps() == 60U,
                "pre-game render selector did not step backward to 60 FPS");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.presentation_fps() == 30U,
                "pre-game render selector did not expose 30 FPS");
        constexpr std::array<std::uint16_t, 9> presentation_cycle{
            60U, 90U, 120U, 240U, 360U, 480U, 20U, 30U, 60U};
        for (const auto expected_fps : presentation_cycle) {
            drive_boot({0, starfox::input::right, 0});
            require(boot_game.presentation_fps() == expected_fps,
                    "pre-game render selector skipped a supported FPS value");
        }
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 3U,
                "pre-game cursor did not reach DISPLAY");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.display_mode()
                    == starfox::simulation::DisplayMode::widescreen_16_9,
                "pre-game display selector did not enable widescreen");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.display_mode()
                    == starfox::simulation::DisplayMode::widescreen_16_10,
                "pre-game display selector did not place 16:10 after 16:9");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.display_mode()
                    == starfox::simulation::DisplayMode::ultrawide_21_9,
                "pre-game display selector did not enable 21:9 ultrawide");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.display_mode()
                    == starfox::simulation::DisplayMode::super_ultrawide_32_9,
                "pre-game display selector did not enable 32:9 super ultrawide");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.display_mode()
                    == starfox::simulation::DisplayMode::standard_4_3,
                "pre-game display selector did not wrap to standard mode");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.display_mode()
                    == starfox::simulation::DisplayMode::super_ultrawide_32_9,
                "pre-game display selector did not step backward to 32:9");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 4U,
                "pre-game cursor did not reach RENDERER");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.renderer_mode()
                    == starfox::simulation::RendererMode::software,
                "pre-game renderer option did not select software");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.renderer_mode()
                    == starfox::simulation::RendererMode::gpu,
                "pre-game renderer option did not return to GPU");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 5U,
                "pre-game cursor did not reach MSU-1 MUSIC");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.msu1_music(),
                "pre-game MSU-1 music option did not enable");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 6U,
                "pre-game cursor did not reach RUMBLE");
        require(boot_game.rumble(),
                "pre-game rumble option did not default on");
        drive_boot({0, starfox::input::a, 0});
        require(!boot_game.rumble(),
                "pre-game rumble option did not disable");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.rumble(),
                "pre-game rumble option did not re-enable");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 7U,
                "pre-game cursor did not reach ANTI-ALIASING");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.anti_aliasing_mode()
                    == starfox::simulation::AntiAliasingMode::light,
                "pre-game Anti-Aliasing option did not select light FXAA");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.anti_aliasing_mode()
                    == starfox::simulation::AntiAliasingMode::medium,
                "pre-game Anti-Aliasing option did not select medium FXAA");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.anti_aliasing_mode()
                    == starfox::simulation::AntiAliasingMode::heavy,
                "pre-game Anti-Aliasing option did not select heavy FXAA");
        drive_boot({0, starfox::input::right, 0});
        require(!boot_game.anti_aliasing(),
                "pre-game Anti-Aliasing choices did not wrap to off");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.anti_aliasing_mode()
                    == starfox::simulation::AntiAliasingMode::heavy,
                "pre-game Anti-Aliasing choices did not step backward");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 8U,
                "pre-game cursor did not reach ENHANCED TEXTURES");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.enhanced_graphics(),
                "pre-game enhanced texture filtering did not enable");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 9U,
                "pre-game cursor did not reach RENDER UPSCALE");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.render_scale()
                    == starfox::simulation::RenderScale::scale_2x,
                "Render Upscale did not advance to 2x");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.render_scale()
                    == starfox::simulation::RenderScale::scale_1x,
                "Render Upscale did not step back to native");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.render_scale()
                    == starfox::simulation::RenderScale::scale_10x,
                "Render Upscale did not wrap backward to 10x");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.render_scale()
                    == starfox::simulation::RenderScale::scale_1x
                    && !boot_game.smooth_polys(),
                "Render Upscale enabled the replaced legacy effect");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 10U,
                "pre-game cursor did not reach RTX LIGHTING");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.rtx_lighting(),
                "pre-game RTX lighting option did not enable");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 11U,
                "pre-game cursor did not reach VSYNC");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.vsync(),
                "pre-game VSync option did not enable");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 12U,
                "pre-game cursor did not reach CONTROLLER");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.flow_state()
                    == starfox::simulation::GameFlowState::pregame_menu,
                "CONTROLLER selection incorrectly started the game");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 13U,
                "pre-game cursor did not reach OPTIONS");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.pregame_page()
                    == starfox::simulation::PregamePage::options
                    && boot_game.pregame_selection() == 0U,
                "OPTIONS did not open its second pre-game page");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.god_mode(),
                "God Mode could not be enabled from OPTIONS");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 1U,
                "pre-game cursor did not reach ON-SCREEN FPS");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.show_fps(),
                "live on-screen FPS could not be enabled from OPTIONS");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 2U
                    && boot_game.crosshair_colour()
                        == starfox::simulation::CrosshairColour::green,
                "pre-game cursor did not reach the default green crosshair colour");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.crosshair_colour()
                    == starfox::simulation::CrosshairColour::white,
                "crosshair colour selector did not advance to white");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.crosshair_colour()
                    == starfox::simulation::CrosshairColour::green,
                "crosshair colour selector did not return to green");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.crosshair_colour()
                    == starfox::simulation::CrosshairColour::orange,
                "crosshair colour selector did not wrap backward");
        drive_boot({0, starfox::input::right, 0});
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 3U,
                "pre-game cursor did not reach CUSTOMIZE SCREEN");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 4U,
                "pre-game cursor did not reach MUSIC VOLUME");
        require(boot_game.music_volume() == 100U,
                "pre-game music volume did not default to 100 percent");
        drive_boot({starfox::input::left, starfox::input::left, 0});
        require(boot_game.music_volume() == 90U,
                "pre-game music volume did not step left by 10 percent");
        drive_boot({starfox::input::left, starfox::input::left, 0});
        require(boot_game.music_volume() == 90U,
                "held pre-game direction changed an option more than once");
        drive_boot({0, 0, starfox::input::left});
        drive_boot({starfox::input::right, starfox::input::right, 0});
        require(boot_game.music_volume() == 100U,
                "pre-game music volume did not step right after release");
        drive_boot({0, 0, starfox::input::right});
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 5U
                    && boot_game.sfx_volume() == 100U,
                "pre-game cursor did not reach default SFX VOLUME");
        drive_boot({0, starfox::input::left, 0});
        require(boot_game.sfx_volume() == 90U,
                "pre-game SFX volume did not step left by 10 percent");
        drive_boot({0, starfox::input::right, 0});
        require(boot_game.sfx_volume() == 100U,
                "pre-game SFX volume did not step right by 10 percent");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 6U,
                "pre-game cursor did not reach OPTIONS BACK");
        drive_boot({0, starfox::input::a, 0});
        require(boot_game.pregame_page()
                     == starfox::simulation::PregamePage::main
                     && boot_game.pregame_selection() == 13U
                    && boot_game.god_mode()
                    && boot_game.show_fps()
                     && boot_game.anti_aliasing()
                     && boot_game.enhanced_graphics()
                     && !boot_game.smooth_polys()
                     && boot_game.rtx_lighting()
                    && boot_game.vsync()
                    && boot_game.crosshair_colour()
                        == starfox::simulation::CrosshairColour::green
                    && boot_game.render_scale()
                        == starfox::simulation::RenderScale::scale_1x,
                "OPTIONS did not retain its toggles when returning to setup");
        drive_boot({0, starfox::input::down, 0});
        require(boot_game.pregame_selection() == 14U,
                "pre-game cursor did not reach START GAME");
        drive_boot({0, starfox::input::start, 0});
        require(boot_game.flow_state()
                    == starfox::simulation::GameFlowState::pregame_menu,
                "START GAME skipped its fade-out");
        for (std::size_t tick = 0; tick < 60U
             && boot_game.flow_state()
                 == starfox::simulation::GameFlowState::pregame_menu; ++tick) {
            drive_boot({});
        }
        require(heard_start_sound
                    && boot_game.flow_state()
                        == starfox::simulation::GameFlowState::intro
                    && boot_game.map().read_native_byte(map_bank)
                        == static_cast<std::uint8_t>(
                            upstream_symbols.find("INTROMAP").front() >> 16U),
                "START GAME did not sound, fade, and enter the original intro");
        bool heard_intro_music = false;
        for (std::size_t tick = 0; tick < 300U; ++tick) {
            const auto boot_tick = boot_game.tick({});
            const auto boot_pcm = boot_audio.render_logic_tick(
                boot_tick.audio_port_writes);
            boot_game.synchronize_apu_output_ports(boot_audio.output_ports());
            heard_intro_music = heard_intro_music
                || std::any_of(boot_pcm.begin(), boot_pcm.end(),
                    [](std::int16_t sample) { return sample != 0; });
        }
        require(boot_audio.driver_loaded() && heard_intro_music,
                "cold-boot intro did not produce its original SPC music");

        starfox::simulation::GameSimulation intro_ending_game{
            upstream_rom, upstream_symbols, "INTROMAP"};
        for (std::size_t frame = 0; frame < 24U; ++frame) {
            intro_ending_game.present_frame();
        }
        const auto exit_intro = upstream_symbols.find("EXITINTRO").front();
        auto reached_intro_exit = false;
        const auto intro_exit_limit = static_cast<std::size_t>(
            starfox_ex_cartridge ? 2'000U : 500U);
        for (std::size_t tick = 0; tick < intro_exit_limit; ++tick) {
            static_cast<void>(intro_ending_game.tick({}));
            if (intro_ending_game.map().read_native_byte(exit_intro) != 0U) {
                reached_intro_exit = true;
                break;
            }
        }
        require(reached_intro_exit
                    && intro_ending_game.map().fade_direction() == 0
                    && intro_ending_game.map().display_brightness() == 15U,
                "automatic intro exit hid its completed near-camera frame");
        intro_ending_game.present_frame();
        require(intro_ending_game.map().fade_direction() == 0
                    && intro_ending_game.map().display_brightness() == 15U,
                "intro fade consumed the source's final full-bright raster");
        intro_ending_game.present_frame();
        require(intro_ending_game.map().fade_direction() == -2
                    && intro_ending_game.map().display_brightness() == 11U,
                "intro did not begin its source quick fade after the final raster");

        starfox::simulation::GameSimulation restart_game{
            upstream_rom, upstream_symbols, "LEVEL1_1"};
        starfox::audio::Spc700Audio restart_audio;
        const auto restart_pointer = upstream_symbols.find("MAPRESTART").front();
        for (std::size_t tick = 0; tick < 3'000U
             && restart_game.map().read_native_word(restart_pointer) == 0U;
             ++tick) {
            const auto restart_tick = restart_game.tick({});
            static_cast<void>(restart_audio.render_logic_tick(
                restart_tick.audio_port_writes));
            restart_game.synchronize_apu_output_ports(
                restart_audio.output_ports());
        }
        const auto background_flags = upstream_symbols.find("BGFLAGS").front();
        const auto player_ship_flags = upstream_symbols.find("PSHIPFLAGS").front();
        const auto game_flags = upstream_symbols.find("GAMEFLAGS").front();
        const auto fade_to_red = upstream_symbols.find("FADETORED").front();
        const auto boss_max_health = upstream_symbols.find("M_BOSSMAXHP").front();
        const auto background_music = upstream_symbols.find("BGM_MUSIC").front();
        const auto background_music_count = upstream_symbols.find("BGMCNT").front();
        // Capture the active encounter track immediately before the synthetic
        // death. The native death sequence replaces it with $11.
        restart_game.map().write_native_byte(boss_max_health, 70U);
        restart_game.map().write_native_byte(background_music, 0x66U);
        restart_game.map().write_native_byte(background_music_count, 2U);
        static_cast<void>(restart_game.tick({}));
        const auto message_count_1 = upstream_symbols.find("MSG_COUNT1").front();
        const auto message_count_2 = upstream_symbols.find("MSG_COUNT2").front();
        restart_game.map().write_native_byte(boss_max_health, 0U);
        restart_game.map().write_native_byte(message_count_1, 50U);
        restart_game.map().write_native_byte(message_count_2, 5U);
        restart_game.set_timing_mode(
            starfox::simulation::TimingMode::original_speed);
        std::size_t post_boss_updates{};
        for (std::size_t presentation = 0; presentation < 150U;
             ++presentation) {
            restart_game.present_frame();
            if (restart_game.logic_tick_ready()) {
                static_cast<void>(restart_game.tick({}));
                ++post_boss_updates;
            }
        }
        require(post_boss_updates == 50U,
                "post-boss dialogue did not retain its original 20 Hz cadence");
        restart_game.set_timing_mode(
            starfox::simulation::TimingMode::unlocked_20_fps);
        const auto restart_ppu_palette =
            upstream_symbols.find("PAL0PALETTE").front();
        restart_game.map().write_native_byte(game_flags,
            static_cast<std::uint8_t>(
                restart_game.map().read_native_byte(game_flags) & ~0x42U));
        restart_game.map().write_native_byte(fade_to_red, 0U);
        std::array<std::uint16_t, 8U * 16U> pre_death_palette{};
        for (std::size_t index = 0U; index < pre_death_palette.size(); ++index) {
            pre_death_palette[index] = static_cast<std::uint16_t>(
                (index * 0x0081U) & 0x7fffU);
            restart_game.map().write_native_word(
                restart_ppu_palette + static_cast<std::uint32_t>(index * 2U),
                pre_death_palette[index]);
        }
        restart_game.map().write_cgram(0U, pre_death_palette);
        // A healthy gameplay boundary owns the palette snapshot. The next
        // tick simulates FADERED having destructively replaced every row.
        static_cast<void>(restart_game.tick({}));
        restart_game.map().write_native_byte(game_flags,
            static_cast<std::uint8_t>(
                restart_game.map().read_native_byte(game_flags) | 0x42U));
        restart_game.map().write_native_byte(player_ship_flags,
            static_cast<std::uint8_t>(
                restart_game.map().read_native_byte(player_ship_flags) | 0x60U));
        restart_game.map().write_native_byte(background_flags,
            static_cast<std::uint8_t>(
                restart_game.map().read_native_byte(background_flags) | 1U));
        restart_game.map().write_native_byte(fade_to_red, 1U);
        restart_game.map().write_native_byte(background_music, 0x11U);
        const std::array<std::uint16_t, 8U * 16U> death_red_palette = [] {
            std::array<std::uint16_t, 8U * 16U> palette{};
            palette.fill(0x001fU);
            return palette;
        }();
        for (std::size_t index = 0U; index < death_red_palette.size(); ++index) {
            restart_game.map().write_native_word(
                restart_ppu_palette + static_cast<std::uint32_t>(index * 2U),
                death_red_palette[index]);
        }
        restart_game.map().write_cgram(0U, death_red_palette);
        require(restart_game.map().read_native_word(restart_pointer) != 0U,
                "Corneria did not establish its native death checkpoint");
        auto restart_tick = restart_game.tick({});
        static_cast<void>(restart_audio.render_logic_tick(
            restart_tick.audio_port_writes));
        restart_game.synchronize_apu_output_ports(restart_audio.output_ports());
        require(restart_game.objects().is_active(restart_game.player())
                    && (restart_game.map().read_native_byte(player_ship_flags)
                        & 0x60U) == 0U
                    && (restart_game.map().read_native_byte(game_flags)
                        & 0x42U) == 0U
                    && restart_game.map().read_native_byte(fade_to_red) == 0U,
                "death restart did not restore control and palette ownership");
        auto palette_was_restored = true;
        for (std::size_t index = 0U; index < pre_death_palette.size(); ++index) {
            const auto restored_ram = restart_game.map().read_native_word(
                    restart_ppu_palette
                        + static_cast<std::uint32_t>(index * 2U));
            const auto restored_cgram =
                restart_game.map().ppu_state().cgram[index];
            if (restored_ram != pre_death_palette[index]
                || restored_cgram != pre_death_palette[index]) {
                if (palette_was_restored) {
                    std::cerr << "death palette mismatch index=" << index
                              << " expected=$" << std::hex
                              << pre_death_palette[index] << " ram=$"
                              << restored_ram << " cgram=$" << restored_cgram
                              << std::dec << '\n';
                }
                palette_was_restored = false;
            }
        }
        require(palette_was_restored,
            "death restart retained the red death palette");
        require(restart_game.map().read_native_byte(background_music) == 0x66U
                    && restart_game.map().read_native_byte(
                        background_music_count) == 0U,
                "boss restart did not replace the death drumroll with the encounter track");
        const auto restart_x = restart_game.objects().at(
            restart_game.player()).world_x;
        for (std::size_t tick = 0; tick < 80U; ++tick) {
            restart_tick = restart_game.tick({starfox::input::left,
                static_cast<starfox::input::ButtonMask>(
                    tick == 0U ? starfox::input::left : 0U), 0});
            static_cast<void>(restart_audio.render_logic_tick(
                restart_tick.audio_port_writes));
            restart_game.synchronize_apu_output_ports(
                restart_audio.output_ports());
        }
        require(restart_game.objects().at(restart_game.player()).world_x
                    != restart_x,
                "rebuilt player ignored directional input after death restart");

        const starfox::assets::ShapeDecoder textured_decoder{
            upstream_rom, upstream_symbols};
        const auto andross = textured_decoder.decode_by_name(upstream_symbols, "ANDROSS");
        const auto lfdie = textured_decoder.decode_by_name(upstream_symbols, "LFDIE");
        const auto ship4 = textured_decoder.decode_by_name(upstream_symbols, "SHIP_4");
        require(!andross.textures.empty(),
                "original texture address/coordinate tables were not decoded");
        require(std::any_of(lfdie.faces.begin(), lfdie.faces.end(),
                    [](const auto& face) { return face.sprite; }),
                "original sprite-face commands were not preserved");
        const auto sprite_face = std::find_if(lfdie.faces.begin(), lfdie.faces.end(),
            [](const auto& face) { return face.sprite; });
        require(sprite_face->vertex_indices == std::vector<std::uint8_t>{4U}
                    && sprite_face->colour_id == 0U && sprite_face->sprite_size == 1U,
                "s_sprite point/colour/size operands were not decoded in source order");
        require(starfox::assets::ShapeDecoder::select_lod_pointer(ship4.header, 999.0)
                        == static_cast<std::uint16_t>(ship4.header.address)
                    && starfox::assets::ShapeDecoder::select_lod_pointer(ship4.header, 2000.0)
                        == ship4.header.lod2_pointer,
                "Super FX z=1000/2000/3000 LOD thresholds were not preserved");
        const auto ship4_lod = textured_decoder.decode_lod(
            ship4.header, ship4.header.lod2_pointer);
        require(ship4_lod.header.shift == ship4.header.shift
                    && ship4_lod.header.colour_pointer == ship4.header.colour_pointer,
                "selected LOD did not inherit its base shift/colour state");
        starfox::render::Framebuffer textured_frame{224, 192};
        starfox::render::SoftwareRenderer textured_renderer;
        textured_renderer.draw(andross, {}, textured_frame);
        std::array<bool, 16> used_texels{};
        for (const auto pixel : textured_frame.pixels()) {
            used_texels[pixel & 15U] = true;
        }
        require(std::count(used_texels.begin(), used_texels.end(), true) > 4,
                "original packed 4-bit texture did not reach the software rasterizer");
        const auto sprite_colour = std::find_if(andross.colour_words.begin(),
            andross.colour_words.end(), [](std::uint16_t word) {
                return (word & 0xc000U) == 0x4000U;
            });
        require(sprite_colour != andross.colour_words.end(),
                "textured test shape has no software-sprite material");
        starfox::render::RenderPose simple_sprite_pose;
        simple_sprite_pose.simple_scaled_sprite = true;
        simple_sprite_pose.simple_sprite_colour = static_cast<std::uint8_t>(
            std::distance(andross.colour_words.begin(), sprite_colour));
        simple_sprite_pose.simple_sprite_world_size = 64;
        starfox::render::Framebuffer simple_sprite_frame{224, 192};
        textured_renderer.draw(andross, simple_sprite_pose, simple_sprite_frame);
        require(std::any_of(simple_sprite_frame.pixels().begin(),
                    simple_sprite_frame.pixels().end(),
                    [](std::uint8_t pixel) { return pixel != 0U; }),
                "original simple scaled-sprite material did not render");
        auto clipped_sprite_pose = simple_sprite_pose;
        clipped_sprite_pose.effect_clip_left = 112;
        clipped_sprite_pose.effect_clip_right = 224;
        starfox::render::Framebuffer clipped_sprite_frame{224, 192};
        textured_renderer.draw(
            andross, clipped_sprite_pose, clipped_sprite_frame);
        require(std::any_of(clipped_sprite_frame.pixels().begin(),
                    clipped_sprite_frame.pixels().end(),
                    [](std::uint8_t pixel) { return pixel != 0U; }),
                "horizontal effect clip suppressed the visible sprite half");
        for (std::uint32_t y = 0; y < clipped_sprite_frame.height(); ++y) {
            for (std::uint32_t x = 0; x < 112U; ++x) {
                require(clipped_sprite_frame.get(x, y) == 0U,
                    "transient sprite effect escaped its horizontal camera clip");
            }
        }
        const auto starfox_message = upstream_symbols.find("MSG_STARFOX");
        require(!starfox_message.empty(), "MSG_STARFOX symbol is missing");
        starfox::render::ScaledTextRenderer text_renderer{
            upstream_rom, upstream_symbols};
        starfox::render::RenderPose text_pose;
        text_pose.z = 1'000.0;
        starfox::render::Framebuffer text_frame{224, 192};
        text_renderer.draw(0U, 14U, 0, text_pose, text_frame);
        require(std::all_of(text_frame.pixels().begin(), text_frame.pixels().end(),
                    [](std::uint8_t pixel) { return pixel == 0U; }),
                "uninitialized projected-text pointer was not ignored");
        text_renderer.draw(static_cast<std::uint16_t>(starfox_message.front()),
            14U, 0, text_pose, text_frame);
        require(std::any_of(text_frame.pixels().begin(), text_frame.pixels().end(),
                    [](std::uint8_t pixel) { return pixel == 7U * 16U + 14U; }),
                "original projected 16x16 message font did not render");
        const auto pause_text = upstream_symbols.find("PAUSETXT");
        require(!pause_text.empty(), "PAUSETXT symbol is missing");
        starfox::render::Framebuffer pause_frame{224, 192};
        text_renderer.draw_game_text(
            0x2d0000U, 90, 90, pause_frame);
        require(std::all_of(pause_frame.pixels().begin(),
                    pause_frame.pixels().end(),
                    [](std::uint8_t pixel) { return pixel == 0U; }),
                "banked null dialogue pointer was not ignored");
        text_renderer.draw_game_text(
            pause_text.front(), 90, 90, pause_frame);
        const auto pause_colour = static_cast<std::uint8_t>(7U * 16U
            + (starfox_ex_cartridge ? 4U : 14U));
        require(std::count(pause_frame.pixels().begin(), pause_frame.pixels().end(),
                    pause_colour) > 40,
                "original variable-width PAUSED font did not render");
        starfox::render::Framebuffer face_frame{224, 192};
        text_renderer.draw_face(5U, 48, 152, face_frame);
        require(std::count_if(face_frame.pixels().begin(), face_frame.pixels().end(),
                    [](std::uint8_t pixel) { return pixel != 7U * 16U; }) > 300,
                "original 4-bpp Fox communication portrait did not render");

        starfox::render::DustRenderer dot_renderer{
            upstream_rom, upstream_symbols};
        starfox::render::Framebuffer grid_frame{224, 192};
        const auto identity = starfox::simulation::rotation_matrix_q15(
            trig, 0, 0, 0);
        dot_renderer.draw_grid({0.0, -256.0, 0.0, 0.0, 0.0, 0.0},
            identity, grid_frame);
        require(std::count(grid_frame.pixels().begin(), grid_frame.pixels().end(),
                    static_cast<std::uint8_t>(7U * 16U + 14U)) >= 15,
                "original 15x15 ground-dot lattice did not render");
    }

    std::cout << "All simulation substrate tests passed.\n";
    return 0;
}
