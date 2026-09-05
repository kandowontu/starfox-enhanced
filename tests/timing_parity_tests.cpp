#include "starfox/input/input_latch.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/timing/fixed_step.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using namespace starfox;
struct Sample {
    unsigned raster;
    std::uint32_t map;
    std::int16_t x, y, z;
    std::uint8_t pitch, yaw, roll, frame_rate, roll_velocity;
    std::size_t active;
    friend bool operator==(const Sample&, const Sample&) = default;
};

std::vector<Sample> replay(const assets::RomImage& rom, const assets::SymbolMap& symbols,
    const char* level, simulation::TimingMode mode, unsigned schedule, bool show_fps) {
    using namespace std::chrono;
    auto game = std::make_unique<simulation::GameSimulation>(rom, symbols, level);
    game->set_timing_mode(mode);
    game->set_presentation_fps(90);
    game->set_show_fps(show_fps);
    game->set_god_mode(true);
    timing::FixedStepClock clock{60};
    timing::LiveFpsCounter counter;
    counter.reset({});
    input::InputLatch controls;
    nanoseconds elapsed{};
    constexpr auto duration = seconds{60};
    constexpr std::array<unsigned, 8> uneven_fps{47, 59, 51, 48, 57, 53, 49, 58};
    std::vector<Sample> result;
    unsigned host_frame = 0, raster = 0;
    while (elapsed < duration) {
        ++host_frame;
        nanoseconds delta;
        if (schedule < 2) {
            const auto fps = schedule == 0 ? 60U : 90U;
            const auto deadline = nanoseconds{host_frame * 1'000'000'000ULL / fps};
            delta = deadline - elapsed;
        } else {
            delta = nanoseconds{1'000'000'000ULL / uneven_fps[host_frame % uneven_fps.size()]};
            if (schedule == 3 && host_frame % 29 == 0) delta = milliseconds{100};
        }
        delta = std::min(delta, duration_cast<nanoseconds>(duration) - elapsed);
        elapsed += delta;
        const auto batch = clock.advance(delta);
        if (show_fps) counter.record_frame(timing::LiveFpsCounter::time_point{} + elapsed);
        for (unsigned phase = 0; phase < batch.simulation_steps; ++phase) {
            ++raster;
            // Script the same physical input timeline independently of host
            // draws. Paired shoulder presses exercise roll input across the
            // varying source cadence, while Y fire changes scene workload.
            const auto t = raster % 360;
            input::ButtonMask held = input::y;
            if (t < 100) held |= input::left;
            else if (t < 200) held |= input::right;
            if ((t >= 240 && t < 247) || (t >= 254 && t < 261)) held |= input::left_shoulder;
            controls.sample(held);
            game->present_frame();
            if (!game->logic_tick_ready()) continue;
            static_cast<void>(game->tick(controls.consume()));
            if (!game->objects().is_active(game->player())) throw std::runtime_error("Lost player");
            const auto& player = game->objects().at(game->player());
            result.push_back({raster, game->map().cursor(), player.world_x, player.world_y,
                player.world_z, player.rotation_x, player.rotation_y, player.rotation_z,
                game->map().read_native_byte(symbols.find("FRAMERATE").at(0)),
                game->map().read_native_byte(symbols.find("PLAYER_ROLLZVEL").at(0)),
                game->objects().active_count()});
        }
        const auto alpha = game->logic_interpolation_alpha(batch.interpolation_alpha);
        if (alpha < 0 || alpha > 1) throw std::runtime_error("Invalid interpolation fraction");
    }
    if (raster != 3600 || result.size() < 500) throw std::runtime_error("Incomplete route replay");
    if (std::none_of(result.begin(), result.end(), [](const auto& sample) {
            return sample.roll_velocity != 0;
        })) throw std::runtime_error("Replay never triggered a barrel roll");
    return result;
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("Expected ROM and symbols");
        const auto rom = assets::RomImage::load(argv[1]);
        const auto symbols = assets::SymbolMap::load(argv[2]);
        for (const auto* level : {"LEVEL2_1", "LEVEL3_1"}) {
            for (const auto mode : {simulation::TimingMode::original_speed,
                     simulation::TimingMode::unlocked_20_fps}) {
                const auto expected = replay(rom, symbols, level, mode, 0, false);
                for (unsigned schedule = 1; schedule <= 3; ++schedule) {
                    for (const auto show_fps : {false, true}) {
                        if (replay(rom, symbols, level, mode, schedule, show_fps) != expected)
                            throw std::runtime_error(std::string{level}
                                + " source movement/roll/cadence changed with host FPS or overlay");
                    }
                }
                std::cout << level << ' ' << (mode == simulation::TimingMode::original_speed
                    ? "original" : "unlocked") << ": " << expected.size()
                    << " identical source updates across seven 60-second replays\n";
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
