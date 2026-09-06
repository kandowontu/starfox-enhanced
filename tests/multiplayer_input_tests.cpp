#include "starfox/simulation/game_simulation.hpp"
#include "starfox/input/buttons.hpp"
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace starfox;

int main(int argc, char** argv) try {
    const bool held_control = argc == 4 && std::string{argv[3]} == "--held-control";
    if (argc != 3 && !held_control) throw std::runtime_error{"Expected EX ROM and symbols [--held-control]"};
    const auto rom = assets::RomImage::load(argv[1]);
    const auto symbols = assets::SymbolMap::load(argv[2]);
    const auto address = [&](const char* name) { return symbols.find(name).at(0); };
    const std::array rolls{address("PLAYER_ROLLZVEL"), address("PLAYERTWO_ROLLZVEL"),
        address("PLAYERTHREE_ROLLZVEL"), address("PLAYERFOUR_ROLLZVEL"), address("PLAYERFIVE_ROLLZVEL")};
    unsigned cases{};
    for (const auto mode : {simulation::TimingMode::original_speed,
                           simulation::TimingMode::unlocked_20_fps}) {
        for (const unsigned players : {2U, 5U, 1U}) {
            for (unsigned selected = players == 1 ? 0 : 1;
                 selected < (players == 1 ? 1 : players); ++selected) {
                for (const auto shoulder : {input::left_shoulder, input::right_shoulder}) {
                    auto game = std::make_unique<simulation::GameSimulation>(rom, symbols, "LEVEL2_1");
                    game->set_timing_mode(mode);
                    game->set_god_mode(true);
                    const auto step = [&](input::TickInput primary,
                        const std::array<input::TickInput, 4>& secondary) {
                        game->set_secondary_inputs(secondary);
                        unsigned phases{};
                        do {
                            game->present_frame();
                            if (++phases > 120) throw std::runtime_error{"Multiplayer input did not advance"};
                        } while (!game->logic_tick_ready());
                        static_cast<void>(game->tick(primary));
                    };
                    unsigned launch{};
                    do {
                        step({}, {});
                        if (++launch > 1000) throw std::runtime_error{"Launch did not enable controls"};
                    } while (game->map().read_native_byte(address("PSHIPFLAGS")) & 0xe0);
                    game->map().write_native_byte(address("MULTITAPMODE"), players != 2);
                    game->map().write_native_byte(address("NUMPLAYERS"), players);
                    game->map().write_native_byte(address("PLAYERTWOACTIVATED"), 1);
                    game->map().write_native_byte(address("M_PLAYERTWOACTIVATED"), 1);
                    simulation::Wdc65816Registers registers;
                    registers.status = 0x24;
                    registers.data_bank = 0x7e;
                    game->map().call_native_routine(address("ACTIVATESECONDPLAYER_L"), registers);
                    for (unsigned settle = 0; settle < 20; ++settle) step({}, {});
                    if (!game->map().read_native_word(address("PLAYPTTWO")))
                        throw std::runtime_error{"Native activation did not create player two"};
                    std::array<input::TickInput, 4> secondary{};
                    input::TickInput primary{};
                    const input::TickInput tap{
                        static_cast<input::ButtonMask>(held_control ? shoulder : 0), shoulder,
                        static_cast<input::ButtonMask>(held_control ? 0 : shoulder)};
                    if (selected == 0) primary = tap;
                    else secondary[selected - 1] = tap;
                    step(primary, secondary);
                    for (const auto roll : rolls)
                        if (game->map().read_native_byte(roll))
                            throw std::runtime_error{"A single multiplayer tap started a roll"};
                    if (held_control) step({}, {});
                    step(primary, secondary);
                    for (unsigned player = 0; player < (players == 2 ? 2 : 5); ++player) {
                        const bool expected = players == 1 || player == selected;
                        const bool rolled = game->map().read_native_byte(rolls[player]) != 0;
                        if (rolled != expected)
                            throw std::runtime_error{"Multiplayer roll differs: players=" + std::to_string(players)
                                + " selected=" + std::to_string(selected + 1) + " observed="
                                + std::to_string(player + 1) + " rolled=" + std::to_string(rolled)};
                    }
                    ++cases;
                }
            }
        }
    }
    std::cout << cases << " native multiplayer " << (held_control ? "held-control" : "short-tap")
        << " cases pass\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
