#include "starfox/assets/rom.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/simulation/game_simulation.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc < 3 || argc > 4) {
            std::cerr << "usage: starfox_planet_probe <SF.SFC> <SYMBOLS.TXT> [output.bmp]\n";
            return 2;
        }
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const auto symbol = [&symbols](const char* name) {
            const auto values = symbols.find(name);
            if (values.empty()) throw std::runtime_error{name};
            return values.front();
        };
        starfox::simulation::GameSimulation game{rom, symbols, "LEVEL1_1"};
        starfox::simulation::Wdc65816Registers registers;
        registers.status = 0x24U;
        const auto setup_instructions = game.map().call_native_routine(
            symbol("SETUP_PLANETS_L"), registers, 20'000'000, true);
        registers = {};
        registers.status = 0x24U;
        game.map().call_native_routine(
            symbol("SETUPPLANETPAL_L"), registers, 2'000'000, true);
        game.map().write_native_word(symbol("M_LXPOS"), 0U);
        game.map().write_native_word(symbol("M_LYPOS"), 0U);
        game.map().write_native_word(symbol("M_LZPOS"), 150U);
        game.map().write_native_word(symbol("CURRENTPLANET"), 0xfffeU);
        game.map().write_native_word(symbol("MSPR_PAL"), 6U);
        registers = {};
        registers.status = 0x24U;
        const auto draw_instructions = game.map().call_native_near_routine(
            symbol("DRAWPLANETSPRITES"), registers, 20'000'000, true);
        registers = {};
        registers.status = 0x24U;
        game.map().call_native_near_routine(
            symbol("DMA256SCREEN"), registers, 2'000'000, true);
        registers = {};
        registers.status = 0x24U;
        game.map().call_native_near_routine(
            symbol("SWITCHBUFFER_FAST"), registers, 2'000'000, true);
        std::cout << "setup-instructions=" << setup_instructions
                  << " draw-instructions=" << draw_instructions
                  << " unknown-superfx=";
        for (const auto address : game.map().unknown_superfx_launches()) {
            std::cout << " $" << std::hex << std::setw(6) << std::setfill('0')
                      << address;
        }
        std::cout << '\n';
        if (argc == 4) {
            starfox::render::Framebuffer framebuffer{224, 192};
            const starfox::render::BackgroundRenderer backgrounds;
            const starfox::render::SpriteRenderer sprites;
            const auto& ppu = game.map().ppu_state();
            const auto bg1_only = std::getenv("STARFOX_PLANET_BG1_ONLY") != nullptr;
            if (!bg1_only) {
                backgrounds.draw_bg2(ppu, 0, 0, framebuffer,
                    starfox::render::TilePriorityPass::low);
                sprites.draw_objects(ppu, framebuffer, 0U);
            }
            backgrounds.draw_bg1(ppu, framebuffer,
                starfox::render::TilePriorityPass::low);
            if (!bg1_only) {
                sprites.draw_objects(ppu, framebuffer, 1U);
                backgrounds.draw_bg2(ppu, 0, 0, framebuffer,
                    starfox::render::TilePriorityPass::high);
                sprites.draw_objects(ppu, framebuffer, 2U);
            }
            backgrounds.draw_bg1(ppu, framebuffer,
                starfox::render::TilePriorityPass::high);
            if (!bg1_only) sprites.draw_objects(ppu, framebuffer, 3U);
            const auto palette = starfox::render::decode_bgr555_palette(ppu.cgram);
            starfox::render::write_bmp(framebuffer, argv[3], palette);
        }
        return game.map().unknown_superfx_launches().empty() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "planet probe failed: " << error.what() << '\n';
        return 1;
    }
}
