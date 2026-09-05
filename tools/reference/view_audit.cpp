// Compare the host's post-strategy view flags with actual cartridge routines.
// Ares executes the GSU transform/sort; RetroCPU executes SHOWVIEW_L and
// ALIENFLAGS_L in a disposable snapshot. The running game is never modified
// by the reference calculation. These are sampled stages, not campaigns.
#include "ares_gsu.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include <fstream>
#include <iostream>
#include <memory>

int main(int argc, char** argv) { try {
    if (argc != 6) {
        std::cerr << "Usage: view_audit ROM SYMBOLS MAP TICKS OUTPUT.csv\n";
        return 2;
    }
    const auto ticks = std::stoul(argv[4]);
    if (ticks < 100 || ticks > 100000) throw std::invalid_argument("TICKS must be 100..100000");
    const auto rom = starfox::assets::RomImage::load(argv[1]);
    const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    const auto address = [&](const char* name) { return symbols.find(name).at(0); };
    auto game = std::make_unique<starfox::simulation::GameSimulation>(rom, symbols, argv[3]);
    game->set_timing_mode(starfox::simulation::TimingMode::original_speed);
    game->set_god_mode(true);
    starfox::simulation::Wdc65816 cpu(rom, &symbols);
    std::vector<std::uint8_t> ram(0x10000);
    starfox::reference::AresGsu gsu(rom.bytes(), ram);
    const auto put = [&](unsigned location, unsigned value) {
        ram[location & 65535] = static_cast<std::uint8_t>(value);
        ram[(location + 1) & 65535] = static_cast<std::uint8_t>(value >> 8);
    };
    std::ofstream output(argv[5]);
    if (!output) throw std::runtime_error("Cannot create view CSV");
    output << "tick,object,shape,invisible,host_flags,native_flags,sort_clocks\n";
    unsigned compared = 0, differences = 0, invisible = 0, behind = 0;
    for (unsigned tick = 0; tick < ticks; ++tick) {
        static_cast<void>(game->tick({}));
        if (tick % 10 != 0) continue;
        for (unsigned i = 0; i < 0x20000; ++i)
            cpu.write8(0x7e0000 + i, game->map().read_native_byte(0x7e0000 + i));
        for (unsigned i = 0; i < 0x10000; ++i)
            cpu.write8(0x700000 + i, game->map().read_native_byte(0x700000 + i));
        starfox::simulation::Wdc65816Registers registers;
        registers.status = 0x24;
        cpu.call_long(address("SHOWVIEW_L"), registers);
        for (unsigned i = 0; i < 0x10000; ++i) ram[i] = cpu.read8(0x700000 + i);
        // GETVIEW's authoritative word matrix lives in CPU WMAT11W;
        // WMAT11 names its high byte. Supply it where native GETVIEW places
        // it for MALLROTZSORT; other GSU calls can reuse M_WMAT11 as scratch.
        for (unsigned i = 0; i < 9; ++i)
            put(address("M_WMAT11") + 2 * i,
                game->map().read_native_word(address("WMAT11W") + 2 * i));
        const auto sorted = gsu.run(address("MALLROTZSORT"));
        for (unsigned i = 0; i < 0x10000; ++i) cpu.write8(0x700000 + i, ram[i]);
        registers = {};
        registers.status = 0x24;
        cpu.call_long(address("ALIENFLAGS_L"), registers);
        auto object = game->map().read_native_word(address("ALLST"));
        unsigned guard = 0;
        while (object != 0) {
            if (++guard > 100) throw std::runtime_error("Invalid source object list");
            const auto base = 0x7e0000 + object;
            const auto host = game->map().read_native_byte(base + address("AL_FLAGS")) & 0x1e;
            const auto native = cpu.read8(base + address("AL_FLAGS")) & 0x1e;
            const bool hidden = (cpu.read8(base + address("AL_SFLAGS4")) & 8) != 0;
            output << tick << ',' << object << ',' << cpu.read16(base + address("AL_SHAPE"))
                << ',' << hidden << ',' << host << ',' << native << ',' << sorted.master_clocks << '\n';
            ++compared;
            invisible += hidden;
            behind += !hidden && (native & 8) == 0;
            differences += host != native;
            object = game->map().read_native_word(base);
        }
    }
    std::cout << argv[3] << ": " << compared << " object views, " << invisible
        << " invisible, " << behind << " behind, " << differences << " different\n";
    if (!compared || !invisible || !behind) throw std::runtime_error("View coverage is incomplete");
    return differences ? 1 : 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
} }
