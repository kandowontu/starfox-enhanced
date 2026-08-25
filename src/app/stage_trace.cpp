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
    if (argc < 4 || argc > 6) {
        std::cerr << "Usage: starfox_stage_trace SF.SFC SYMBOLS.TXT MAP_SYMBOL [ticks] [held-input]\n";
        return 2;
    }
    try {
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const auto ticks = argc >= 5 ? std::strtoul(argv[4], nullptr, 10) : 1'000UL;
        const auto held_input = argc == 6
            ? static_cast<starfox::input::ButtonMask>(std::strtoul(argv[5], nullptr, 0))
            : starfox::input::ButtonMask{};
        starfox::simulation::GameSimulation game{rom, symbols, argv[3]};
        std::size_t instructions = 0;
        std::size_t audio_writes = 0;
        std::size_t sound_effects = 0;
        for (unsigned long tick = 0; tick < ticks && !game.map().ended(); ++tick) {
            const auto result = game.tick({
                held_input, tick == 0 ? held_input : starfox::input::ButtonMask{}, 0});
            instructions += result.prelude_instructions + result.strategies.instructions;
            audio_writes += result.audio_port_writes.size();
            sound_effects += result.sound_effect_commands.size();
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
                  << ", pstrat=$" << game.map().read_native_word(0x000338U + 22U)
                  << '/' << static_cast<unsigned>(game.map().read_native_byte(0x000338U + 24U))
                  << ", newpstrat=$" << game.map().read_native_word(0x00180bU)
                  << '/' << static_cast<unsigned>(game.map().read_native_byte(0x00180dU))
                  << std::dec
                  << ", hp=" << static_cast<unsigned>(player.health)
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
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Stage trace failed: " << error.what() << '\n';
        return 1;
    }
}
