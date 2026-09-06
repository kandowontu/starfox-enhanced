// Development probe for installing native timing in the existing game shell.
// Completion reports execution only; it does not certify source pace parity.
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/snes_timeline.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <map>
#include <algorithm>

int main(int argc, char** argv) try {
    if (argc != 5 && argc != 6) {
        std::cerr << "Usage: live_game_timing ROM SYMBOLS MAP TICKS [native-draw]\n";
        return 2;
    }
    const bool native_draw = argc == 6;
    if (native_draw && std::string{argv[5]} != "native-draw")
        throw std::invalid_argument{"Unknown diagnostic option"};
    const auto ticks = std::stoul(argv[4]);
    if (!ticks || ticks > 10000U) throw std::invalid_argument{"TICKS must be 1..10000"};
    const auto rom = starfox::assets::RomImage::load(argv[1]);
    const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    auto game = std::make_unique<starfox::simulation::GameSimulation>(
        rom,symbols,argv[3],std::span<const std::uint8_t>{},true);
    auto timeline = std::make_shared<starfox::simulation::SnesCpuTimeline>();
    game->map().set_cpu_timeline(timeline);
    game->map().set_gsu_timing(true);
    std::uint64_t tick_begin{};
    std::uint64_t previous_clock{};
    std::uint32_t previous_pc{};
    std::map<std::uint32_t,std::uint64_t> instruction_clocks;
    game->map().set_native_instruction_boundary_callback([&](std::uint64_t) {
        const auto now = timeline->raster().elapsed();
        if (previous_pc) instruction_clocks[previous_pc] += now-previous_clock;
        previous_clock = now;
        previous_pc = game->map().native_program_address();
        if (timeline->raster().elapsed() - tick_begin > 100'000'000U)
            throw std::runtime_error{"Tick exceeded the diagnostic clock budget"};
    });
    std::cout << "tick,master_clocks,cpu_clocks,dma_clocks,refresh_clocks,wall_ms,shell_clocks,appended_draw_clocks\n" << std::flush;
    for (unsigned tick = 0; tick < ticks; ++tick) {
        tick_begin = timeline->raster().elapsed();
        previous_clock = tick_begin; previous_pc = 0U; instruction_clocks.clear();
        const auto before = timeline->totals();
        const auto wall = std::chrono::steady_clock::now();
        std::uint64_t shell_clocks{};
        try {
            static_cast<void>(game->tick({}));
            shell_clocks = timeline->raster().elapsed()-tick_begin;
            // Deliberately appended for integration diagnosis. The final
            // scheduler must place these inside TRANSFER_L's source order,
            // replacing host counterparts rather than executing both.
            if (native_draw) for (const auto* name : {"SHOWVIEW_L","BUILD_DRAWLIST_L","DO_3D_DISPLAY_L"}) {
                starfox::simulation::Wdc65816Registers registers;
                registers.status = 0x24U;
                game->map().call_native_routine(symbols.find(name).at(0),registers,5'000'000U);
            }
        }
        catch (const std::exception& error) {
            std::cerr << "tick=" << tick << " pc=$" << std::hex
                << game->map().native_program_address() << std::dec
                << " clocks=" << timeline->raster().elapsed() - tick_begin
                << " transfer=" << unsigned(game->map().read_native_byte(0U))
                << " gsu_pc=$" << std::hex << unsigned(game->map().read_native_byte(0x3034U))
                << ':' << game->map().read_native_word(0x301eU)
                << " sfr=$" << game->map().read_native_word(0x3030U) << std::dec
                << ": " << error.what() << '\n';
            for (unsigned reg=0; reg<16U; ++reg)
                std::cerr << "r" << reg << "=$" << std::hex
                    << game->map().read_native_word(0x3000U+reg*2U) << std::dec << ' ';
            std::cerr << '\n';
            return 1;
        }
        const auto after = timeline->totals();
        std::cout << tick << ',' << timeline->raster().elapsed()-tick_begin << ','
            << after.cpu-before.cpu << ',' << after.dma-before.dma << ','
            << after.refresh-before.refresh << ','
            << std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-wall).count()
            << ',' << shell_clocks << ',' << timeline->raster().elapsed()-tick_begin-shell_clocks
            << '\n' << std::flush;
        std::vector<std::pair<std::uint32_t,std::uint64_t>> costs(instruction_clocks.begin(),instruction_clocks.end());
        std::sort(costs.begin(),costs.end(),[](const auto& a,const auto& b) { return a.second>b.second; });
        std::cerr << "tick=" << tick << " highest instruction clock totals:";
        for (unsigned i=0; i<std::min<std::size_t>(8U,costs.size()); ++i)
            std::cerr << " $" << std::hex << costs[i].first << std::dec << '=' << costs[i].second;
        std::cerr << '\n';
    }
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
