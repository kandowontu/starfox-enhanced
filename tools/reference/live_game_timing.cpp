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
        std::cerr << "Usage: live_game_timing ROM SYMBOLS MAP TICKS [native-draw|native-transfer|native-transfer-sliced|game-transfer]\n";
        return 2;
    }
    const std::string mode = argc == 6 ? argv[5] : "shell";
    const bool native_draw = mode == "native-draw";
    const bool sliced = mode == "native-transfer-sliced";
    const bool native_transfer = mode == "native-transfer" || sliced;
    const bool game_transfer = mode == "game-transfer";
    if (mode != "shell" && !native_draw && !native_transfer && !game_transfer)
        throw std::invalid_argument{"Unknown diagnostic option"};
    const auto ticks = std::stoul(argv[4]);
    if (!ticks || ticks > 10000U) throw std::invalid_argument{"TICKS must be 1..10000"};
    const auto rom = starfox::assets::RomImage::load(argv[1]);
    const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    auto game = std::make_unique<starfox::simulation::GameSimulation>(
        rom,symbols,argv[3],std::span<const std::uint8_t>{},true);
    auto timeline = std::make_shared<starfox::simulation::SnesCpuTimeline>();
    if (!game_transfer) {
        game->map().set_cpu_timeline(timeline);
        game->map().set_gsu_timing(true);
    }
    const starfox::simulation::SnesCpuTimeline* active_timeline = timeline.get();
    if (native_transfer) {
        // INITSCREEN_L's timer setup precedes the first gameplay transfer.
        // The host constructor ran before the timeline was attached.
        game->map().write_native_word(0x4209U,symbols.find("GAMEVW_POS").at(0));
        game->map().write_native_word(0x4207U,0U);
        game->map().write_native_byte(0x4200U,0x31U);
    }
    std::uint64_t tick_begin{};
    std::uint64_t previous_clock{};
    std::uint32_t previous_pc{};
    std::map<std::uint32_t,std::uint64_t> instruction_clocks;
    game->map().set_native_instruction_boundary_callback([&](std::uint64_t) {
        const auto now = active_timeline->raster().elapsed();
        if (previous_pc) instruction_clocks[previous_pc] += now-previous_clock;
        previous_clock = now;
        previous_pc = game->map().native_program_address();
        if (active_timeline->raster().elapsed() - tick_begin > 100'000'000U)
            throw std::runtime_error{"Tick exceeded the diagnostic clock budget"};
    });
    std::cout << "tick,master_clocks,cpu_clocks,dma_clocks,refresh_clocks,wall_ms,shell_clocks,appended_draw_clocks,yields,gameframe,view_z,map_pointer,transfer\n" << std::flush;
    for (unsigned tick = 0; tick < ticks; ++tick) {
        if (game_transfer) {
            game->begin_native_transfer({});
            active_timeline = game->native_transfer_timeline();
        }
        tick_begin = active_timeline->raster().elapsed();
        previous_clock = tick_begin; previous_pc = 0U; instruction_clocks.clear();
        const auto before = active_timeline->totals();
        const auto wall = std::chrono::steady_clock::now();
        std::uint64_t shell_clocks{};
        unsigned yields{};
        try {
            if (game_transfer) {
                while (!game->advance_native_transfer(4096U)) ++yields;
            } else if (native_transfer) {
                starfox::simulation::Wdc65816Registers registers;
                registers.status = 0x20U;
                game->map().call_native_routine(symbols.find("SETBLACK_L").at(0),registers,5'000'000U);
                if (!sliced) game->map().call_native_routine(symbols.find("TRANSFER_L").at(0),registers,5'000'000U);
                else {
                    game->map().hold_native_presentation();
                    game->map().set_task_clock_deadline(timeline->raster().elapsed()+4096U);
                    auto result = game->map().begin_native_task(
                        symbols.find("TRANSFER_L").at(0),registers,{},5'000'000U);
                    while (!result.returned) {
                        if (result.stopped) throw std::runtime_error{"Native transfer entered STP"};
                        ++yields;
                        game->map().set_task_clock_deadline(timeline->raster().elapsed()+4096U);
                        result = game->map().resume_native_task(registers,{},5'000'000U,false,false);
                    }
                    game->map().set_task_clock_deadline({});
                    game->map().release_native_presentation();
                    game->map().restore_objects_from_native();
                }
            } else static_cast<void>(game->tick({}));
            shell_clocks = active_timeline->raster().elapsed()-tick_begin;
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
                << " clocks=" << active_timeline->raster().elapsed() - tick_begin
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
        const auto after = active_timeline->totals();
        std::cout << tick << ',' << active_timeline->raster().elapsed()-tick_begin << ','
            << after.cpu-before.cpu << ',' << after.dma-before.dma << ','
            << after.refresh-before.refresh << ','
            << std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-wall).count()
            << ',' << shell_clocks << ',' << active_timeline->raster().elapsed()-tick_begin-shell_clocks
            << ',' << yields << ',' << game->map().read_native_word(symbols.find("GAMEFRAME").at(0))
            << ',' << game->map().read_native_word(symbols.find("VIEWPOSZ").at(0))
            << ',' << game->map().read_native_word(symbols.find("MAPPTR").at(0))
            << ',' << unsigned(game->map().read_native_byte(0U))
            << '\n' << std::flush;
        std::vector<std::pair<std::uint32_t,std::uint64_t>> costs(instruction_clocks.begin(),instruction_clocks.end());
        std::sort(costs.begin(),costs.end(),[](const auto& a,const auto& b) { return a.second>b.second; });
        std::cerr << "tick=" << tick << " highest instruction clock totals:";
        for (unsigned i=0; i<std::min<std::size_t>(8U,costs.size()); ++i)
            std::cerr << " $" << std::hex << costs[i].first << std::dec << '=' << costs[i].second;
        std::cerr << '\n';
    }
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
