#include "starfox/simulation/game_simulation.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>

using namespace starfox;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<unsigned> run(const assets::RomImage& rom, const assets::SymbolMap& symbols,
    const char* stage, unsigned fps, bool original, int fixed_faces = -1) {
    auto game = std::make_unique<simulation::GameSimulation>(rom, symbols, stage);
    game->set_god_mode(true);
    game->set_timing_mode(original ? simulation::TimingMode::original_speed
        : simulation::TimingMode::unlocked_20_fps);
    assets::ShapeDecoder decoder{rom, symbols};
    std::unordered_map<std::uint32_t, std::uint32_t> counts;
    std::unordered_set<std::uint32_t> invalid;
    game->set_shape_face_counts(&counts);
    std::vector<unsigned> updates;
    unsigned accumulator{}, phase{}, previous{};
    for (unsigned frame = 0; frame < fps * 60; ++frame) {
        accumulator += 60;
        while (accumulator >= fps) {
            accumulator -= fps;
            for (auto handle : game->draw_order()) {
                if (!game->objects().is_active(handle)) continue;
                const auto shape = game->objects().at(handle).shape;
                if (counts.contains(shape) || invalid.contains(shape)) continue;
                try {
                    counts.emplace(shape, fixed_faces < 0
                        ? static_cast<unsigned>(decoder.decode(shape).faces.size())
                        : static_cast<unsigned>(fixed_faces));
                } catch (const std::exception&) { invalid.insert(shape); }
            }
            game->present_frame(); ++phase;
            const auto ready = game->logic_tick_ready();
            // Rendering can query interpolation/readiness multiple times per update.
            (void)game->logic_interpolation_alpha(0.5);
            require(ready == game->logic_tick_ready(), "Pace query changed timing state");
            if (ready) {
                const auto spent = phase - previous;
                require(spent >= 3 && spent <= 7, "Pace escaped raster bounds");
                if (!original) require(spent == 3, "Geometry changed unlocked pace");
                updates.push_back(phase); previous = phase;
                (void)game->tick({});
            }
        }
    }
    require(!counts.empty(), "Geometry pacing test never decoded shapes");
    return updates;
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const auto rom = assets::RomImage::load(argv[1]);
        const auto symbols = assets::SymbolMap::load(argv[2]);
        for (const auto* stage : {"LEVEL1_1", "LEVEL1_2", "LEVEL1_3"}) {
            const auto reference = run(rom, symbols, stage, 60, true);
            require(reference == run(rom, symbols, stage, 30, true), "30 FPS altered Original pace");
            require(reference == run(rom, symbols, stage, 120, true), "120 FPS altered Original pace");
            std::cout << stage << ": " << reference.size() << " updates, identical at 30/60/120 FPS\n";
        }
        require(run(rom, symbols, "LEVEL1_1", 60, true, 0).size()
            > run(rom, symbols, "LEVEL1_1", 60, true, 500).size(),
            "Geometry did not affect Original pace");
        require(run(rom, symbols, "LEVEL1_1", 60, false, 0)
            == run(rom, symbols, "LEVEL1_1", 60, false, 500),
            "Geometry affected unlocked gameplay");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
