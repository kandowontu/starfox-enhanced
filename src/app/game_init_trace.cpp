#include "starfox/assets/rom.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/simulation/strategy_scheduler.hpp"

#include <cstdint>
#include <exception>
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

std::uint32_t ram_symbol(
    const starfox::assets::SymbolMap& symbols, const std::string& name) {
    for (const auto address : symbols.find(name)) {
        if ((address >> 16U) == 0 || (address >> 16U) == 0x7eU) {
            return address;
        }
    }
    throw std::runtime_error{"missing RAM symbol: " + name};
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: starfox_game_init_trace SF.SFC SYMBOLS.TXT [ticks]\n";
        return 2;
    }
    try {
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        starfox::simulation::ObjectPool objects;
        starfox::simulation::MapVm state{
            rom, starfox::simulation::MapDatabase{rom, symbols}, objects};
        state.start(rom_symbol(symbols, "MAPP"), 0);
        state.advance_distance(1);
        if (!state.ended() || objects.active_count() != 4) {
            throw std::runtime_error{"original player map did not create its four objects"};
        }
        const auto player = objects.first_active();
        starfox::simulation::Wdc65816Registers registers;
        registers.x = static_cast<std::uint16_t>(0x0338U + (player - 1U) * 56U);
        const auto playpt_address = ram_symbol(symbols, "PLAYPT");
        const auto internal_playpt_address = ram_symbol(symbols, "INTERNALPLAYPT");
        state.write_native_byte(playpt_address, static_cast<std::uint8_t>(registers.x));
        state.write_native_byte(playpt_address + 1U, static_cast<std::uint8_t>(registers.x >> 8U));
        state.write_native_byte(internal_playpt_address, static_cast<std::uint8_t>(registers.x));
        state.write_native_byte(internal_playpt_address + 1U,
                                static_cast<std::uint8_t>(registers.x >> 8U));
        registers.status = 0x24;
        const auto instructions = state.call_native_routine(
            rom_symbol(symbols, "INITGAME_STRATS_L"), registers, 5'000'000);
        const auto play_pointer = state.read_native_word(playpt_address);
        const auto dummy_pointer = state.read_native_word(ram_symbol(symbols, "DUMMYOBJ"));
        starfox::simulation::NativeStrategyScheduler scheduler{symbols, objects, state};
        const auto ticks = argc == 4 ? std::stoul(argv[3]) : 1UL;
        starfox::simulation::StrategyTickStats stats;
        std::size_t prelude_instructions = 0;
        for (unsigned long tick = 0; tick < ticks; ++tick) {
            prelude_instructions += scheduler.begin_tick();
            const auto current = scheduler.tick_all();
            stats.objects_run += current.objects_run;
            stats.objects_removed += current.objects_removed;
            stats.instructions += current.instructions;
        }
        std::cout << "initgame: instructions=" << instructions
                  << ", objects=" << objects.active_count()
                  << ", playpt=$" << std::hex << play_pointer
                  << ", dummy=$" << dummy_pointer << std::dec
                  << ", ticks=" << ticks << ", strategies=" << stats.objects_run
                  << ", prelude-instructions=" << prelude_instructions
                  << ", strategy-instructions=" << stats.instructions << '\n';
        return objects.active_count() >= 5 && play_pointer != 0 && dummy_pointer != 0
                && stats.objects_run >= 5 && stats.instructions != 0
            ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Game init trace failed: " << error.what() << '\n';
        return 1;
    }
}
