#include "starfox/assets/rom.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/object_pool.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
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
        std::cerr << "Usage: starfox_map_trace SF.SFC SYMBOLS.TXT MAP_SYMBOL [max waits]\n";
        return 2;
    }
    try {
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const auto maximum = argc == 5 ? std::strtoul(argv[4], nullptr, 10) : 100'000UL;
        starfox::simulation::ObjectPool objects;
        const auto player = objects.allocate_after();
        starfox::simulation::MapVm vm{
            rom, starfox::simulation::MapDatabase{rom, symbols}, objects};
        // Structural tracing follows the taken branch for strategy/game-state
        // predicates; production simulation registers their real callbacks.
        vm.set_unknown_condition_result(true);
        vm.start(rom_symbol(symbols, argv[3]), player);

        unsigned long waits = 0;
        while (!vm.ended() && waits < maximum) {
            const auto countdown = vm.countdown();
            const auto distance = countdown >= 32'767
                ? std::int16_t{32'767}
                : static_cast<std::int16_t>(std::max<int>(1, countdown + 1));
            vm.advance_distance(distance);
            ++waits;
        }
        std::cout << argv[3] << ": " << (vm.ended() ? "ended" : "limit")
                  << ", waits=" << waits
                  << ", objects=" << objects.active_count()
                  << ", boundary-skipped=" << vm.unsupported_controls().size()
                  << "\n";
        return vm.ended() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Map trace failed: " << error.what() << '\n';
        return 1;
    }
}
