#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/simulation/game_simulation.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <unordered_set>
#include <vector>
#include <string>

int main(int argc, char** argv) {
    if (argc < 4 || argc > 7) {
        std::cerr << "Usage: starfox_stage_trace SF.SFC SYMBOLS.TXT MAP_SYMBOL "
                     "[ticks] [input] [single-press-tick]\n";
        return 2;
    }
    try {
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const auto ticks = argc >= 5 ? std::strtoul(argv[4], nullptr, 10) : 1'000UL;
        const auto held_input = argc >= 6
            ? static_cast<starfox::input::ButtonMask>(std::strtoul(argv[5], nullptr, 0))
            : starfox::input::ButtonMask{};
        const auto press_tick = argc == 7
            ? std::strtoul(argv[6], nullptr, 10) : ~0UL;
        starfox::simulation::GameSimulation game{rom, symbols, argv[3]};
        const auto trace_transitions = std::getenv("STARFOX_TRACE_TRANSITIONS") != nullptr;
        const auto force_exit = std::getenv("STARFOX_FORCE_EXIT") != nullptr;
        std::uint32_t previous_strategy = 0xffffffffU;
        std::size_t instructions = 0;
        std::size_t audio_writes = 0;
        std::size_t sound_effects = 0;
        for (unsigned long tick = 0; tick < ticks && !game.map().ended(); ++tick) {
            if (force_exit && (tick % 10UL) == 0UL) {
                const auto briefing = game.briefing_state();
                std::cerr << "trace-tick=" << tick << " flow="
                          << static_cast<unsigned>(game.flow_state())
                          << " briefing=" << briefing.active << '@'
                          << std::hex << briefing.message_address << '/'
                          << briefing.planet_name_address << std::dec << '\n';
            }
            if (force_exit && tick == 400UL) {
                game.map().write_native_word(
                    symbols.find("LEVELFINISHED").front(), 1U);
            }
            if (trace_transitions && game.objects().is_active(game.player())) {
                const auto strategy = game.objects().at(game.player()).strategy_address;
                if (strategy != previous_strategy) {
                    std::cout << "tick=" << tick << " player-strategy=$"
                              << std::hex << strategy
                              << " viewpt=$" << game.map().read_native_word(0x0012c1U)
                              << std::dec
                              << " view=("
                              << static_cast<std::int16_t>(
                                     game.map().read_native_word(0x0000b4U))
                              << ',' << static_cast<std::int16_t>(
                                     game.map().read_native_word(0x0000b6U))
                              << ',' << static_cast<std::int16_t>(
                                     game.map().read_native_word(0x0000b8U))
                              << ") player-z="
                              << game.objects().at(game.player()).world_z << '\n';
                    previous_strategy = strategy;
                }
            }
            const auto pulse = press_tick != ~0UL && tick >= press_tick
                && ((tick - press_tick) % 300UL) == 0UL;
            const auto input_this_tick = press_tick == ~0UL || pulse
                ? held_input : starfox::input::ButtonMask{};
            const auto result = game.tick({input_this_tick,
                (tick == 0 || pulse) ? input_this_tick
                                                   : starfox::input::ButtonMask{},
                0});
            instructions += result.prelude_instructions + result.strategies.instructions;
            audio_writes += result.audio_port_writes.size();
            sound_effects += result.sound_effect_commands.size();
            if (force_exit) {
                game.present_frame();
                game.present_frame();
                game.present_frame();
            }
        }
        const auto& player = game.objects().at(game.player());
        const auto body_pointer = game.map().read_native_word(0x0015f2U);
        const starfox::assets::ShapeDecoder decoder{rom, symbols};
        std::unordered_set<std::uint16_t> shapes;
        std::size_t decoded_shapes = 0;
        std::vector<std::uint16_t> undecoded_shapes;
        for (const auto handle : game.objects().active_handles()) {
            const auto shape = game.objects().at(handle).shape;
            if (shape == 0 || !shapes.insert(shape).second) continue;
            try {
                static_cast<void>(decoder.decode(shape));
                ++decoded_shapes;
            } catch (const std::exception&) {
                undecoded_shapes.push_back(shape);
            }
        }
        std::cout << argv[3] << ": ticks=" << ticks
                  << ", map=" << (game.map().ended() ? "ended" : "active")
                  << ", cursor=$" << std::hex << game.map().cursor() << std::dec
                  << ", countdown=" << game.map().countdown()
                  << ", objects=" << game.objects().active_count()
                  << ", shapes=" << decoded_shapes << '/' << shapes.size()
                  << ", display=" << (game.map().screen_enabled() ? "on" : "off")
                  << '/' << static_cast<unsigned>(game.map().display_brightness())
                  << " (fade=" << static_cast<unsigned>(game.map().fade_value()) << ')'
                  << ", bgflags=$" << std::hex
                  << static_cast<unsigned>(game.map().read_native_byte(0x001a16U))
                  << ", exit=$" << game.map().read_native_word(0x001ad5U)
                  << ", stage=$" << game.map().read_native_word(0x00175bU)
                  << ", bg=$" << game.map().read_native_word(0x0017c6U)
                  << "/" << game.map().background()
                  << ", gameframe=" << std::dec << static_cast<unsigned>(
                         game.map().read_native_byte(0x001640U)) << std::hex
                  << ", gamepal=" << std::dec << static_cast<unsigned>(
                         game.map().read_native_byte(0x00191cU)) << std::hex
                  << ", flymode=" << std::dec << static_cast<unsigned>(
                         game.map().read_native_byte(0x001566U)) << std::hex
                  << ", viewdist=" << std::dec << static_cast<std::int16_t>(
                         game.map().read_native_word(0x001622U))
                  << '/' << static_cast<std::int16_t>(
                         game.map().read_native_word(0x001948U)) << std::hex
                  << ", crosshair=" << std::dec << static_cast<unsigned>(
                         game.map().read_native_byte(0x001633U))
                  << "@(" << static_cast<std::int16_t>(
                         game.map().read_native_word(0x001777U))
                  << ',' << static_cast<std::int16_t>(
                         game.map().read_native_word(0x001779U)) << ')' << std::hex
                  << ", pstrat=$" << game.map().read_native_word(0x000338U + 22U)
                  << '/' << static_cast<unsigned>(game.map().read_native_byte(0x000338U + 24U))
                  << ", newpstrat=$" << game.map().read_native_word(0x00180bU)
                  << '/' << static_cast<unsigned>(game.map().read_native_byte(0x00180dU))
                  << std::dec
                  << ", hp=" << static_cast<unsigned>(player.health)
                  << ", shape=$" << std::hex << player.shape << std::dec
                  << ", pflags=(" << static_cast<unsigned>(player.flags)
                  << ',' << static_cast<unsigned>(player.strategy_flags[0])
                  << ',' << static_cast<unsigned>(player.strategy_flags[1])
                  << ',' << static_cast<unsigned>(player.strategy_flags[2])
                  << ',' << static_cast<unsigned>(player.strategy_flags[3]) << ')'
                  << ", pcoll=" << player.collision_object
                  << ", body=$" << std::hex << body_pointer << std::dec
                  << '/' << static_cast<unsigned>(
                         game.map().read_native_byte(body_pointer + 42U))
                  << ", player=(" << player.world_x << ',' << player.world_y << ','
                  << player.world_z << "), instructions=" << instructions
                  << ", apu-writes=" << audio_writes << '\n';
        std::cout << "sound-effects=" << sound_effects << '\n';
        for (const auto shape : undecoded_shapes) {
            std::cout << "undecoded-shape=$" << std::hex << shape << std::dec << '\n';
        }
        for (const auto launch : game.map().unknown_superfx_launches()) {
            std::cout << "unhandled-superfx=$" << std::hex << launch << std::dec << '\n';
        }
        if (std::getenv("STARFOX_DUMP_OAM") != nullptr) {
            const auto& oam = game.map().ppu_state().oam;
            for (std::size_t object = 0; object < 128U; ++object) {
                const auto low = object * 4U;
                const auto bits = static_cast<std::uint8_t>(
                    oam[512U + object / 4U] >> ((object & 3U) * 2U));
                if (bits == 0U && oam[low] == 0U && oam[low + 1U] == 0U
                    && oam[low + 2U] == 0U && oam[low + 3U] == 0U) continue;
                auto x = static_cast<std::int32_t>(oam[low])
                    | (static_cast<std::int32_t>(bits & 1U) << 8U);
                if (x >= 256) x -= 512;
                std::cout << "oam[" << object << "] x=" << x
                          << " y=" << static_cast<unsigned>(oam[low + 1U])
                          << " tile=$" << std::hex
                          << static_cast<unsigned>(oam[low + 2U])
                          << " attr=$" << static_cast<unsigned>(oam[low + 3U])
                          << std::dec << " size=" << ((bits >> 1U) & 1U) << '\n';
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Stage trace failed: " << error.what() << '\n';
        return 1;
    }
}
