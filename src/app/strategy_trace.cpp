#include "starfox/assets/rom.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/simulation/strategy_scheduler.hpp"

#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::uint32_t rom_symbol(
    const starfox::assets::SymbolMap& symbols, const std::string& name) {
    for (const auto address : symbols.find(name)) {
        if ((address & 0xffffU) >= 0x8000U && ((address >> 16U) & 0xffU) < 0x7eU) {
            return address;
        }
    }
    throw std::runtime_error{"missing ROM symbol: " + name};
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4 || argc > 5) {
        std::cerr << "Usage: starfox_strategy_trace SF.SFC SYMBOLS.TXT MAP_SYMBOL [ticks]\n";
        return 2;
    }
    try {
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        starfox::simulation::ObjectPool objects;
        const auto player = objects.allocate_after();
        starfox::simulation::MapVm map{
            rom, starfox::simulation::MapDatabase{rom, symbols}, objects};
        map.start(rom_symbol(symbols, argv[3]), player);
        map.advance_distance(1);

        starfox::simulation::ObjectHandle selected{};
        for (const auto handle : objects.active_handles()) {
            if (objects.at(handle).strategy_address != 0) {
                selected = handle;
                break;
            }
        }
        if (selected == 0) {
            throw std::runtime_error{"map did not spawn an object with a strategy"};
        }
        const auto before = objects.at(selected);
        starfox::simulation::NativeStrategyScheduler scheduler{symbols, objects, map};
        if (argc == 5) {
            const auto ticks = std::stoul(argv[4]);
            starfox::simulation::StrategyTickStats total;
            for (unsigned long tick = 0; tick < ticks; ++tick) {
                const auto stats = scheduler.tick_all();
                total.objects_run += stats.objects_run;
                total.objects_removed += stats.objects_removed;
                total.instructions += stats.instructions;
            }
            std::cout << argv[3] << ": ticks=" << ticks
                      << ", strategies=" << total.objects_run
                      << ", removed=" << total.objects_removed
                      << ", instructions=" << total.instructions
                      << ", active=" << objects.active_count() << '\n';
            return 0;
        }
        const auto instructions = scheduler.tick_object(selected);
        const auto& after = objects.at(selected);
        std::cout << argv[3] << ": object=" << selected
                  << ", strategy=$" << std::hex << before.strategy_address << std::dec
                  << ", instructions=" << instructions
                  << ", position=(" << after.world_x << ',' << after.world_y << ','
                  << after.world_z << "), active=" << objects.active_count() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Strategy trace failed: " << error.what() << '\n';
        return 1;
    }
}
