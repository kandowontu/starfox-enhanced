#include "starfox/simulation/strategy_scheduler.hpp"

#include <stdexcept>
#include <sstream>
#include <string>

namespace starfox::simulation {
namespace {

std::uint32_t rom_symbol(const assets::SymbolMap& symbols, const std::string& name) {
    for (const auto address : symbols.find(name)) {
        if ((address & 0xffffU) >= 0x8000U && ((address >> 16U) & 0xffU) < 0x7eU) {
            return address;
        }
    }
    throw std::runtime_error{"missing strategy ROM symbol: " + name};
}

std::uint32_t ram_symbol(const assets::SymbolMap& symbols, const std::string& name) {
    for (const auto address : symbols.find(name)) {
        if ((address >> 16U) == 0 || (address >> 16U) == 0x7eU) {
            return address;
        }
    }
    throw std::runtime_error{"missing strategy RAM symbol: " + name};
}

} // namespace

NativeStrategyScheduler::NativeStrategyScheduler(
    const assets::SymbolMap& symbols,
    ObjectPool& objects,
    MapVm& native_state)
    : objects_(&objects),
      native_state_(&native_state),
      do_strategy_(rom_symbol(symbols, "DO_STRAT_L")),
      initialize_strategies_(rom_symbol(symbols, "INIT_STRATS_L")),
      remove_dead_(rom_symbol(symbols, "REMOVEDEADAL_L")),
      alien_dead_(ram_symbol(symbols, "ALDEAD")),
      game_frame_(ram_symbol(symbols, "GAMEFRAME")) {}

std::size_t NativeStrategyScheduler::begin_tick() {
    native_state_->write_native_word(
        game_frame_, static_cast<std::uint16_t>(native_state_->read_native_word(game_frame_) + 1U));
    Wdc65816Registers registers;
    registers.data_bank = 0x7e;
    registers.status = 0x24;
    return native_state_->call_native_routine(initialize_strategies_, registers);
}

std::size_t NativeStrategyScheduler::tick_object(ObjectHandle object) {
    native_state_->write_native_byte(alien_dead_, 0);
    try {
        return native_state_->call_native_object_routine(do_strategy_, object);
    } catch (const std::exception& error) {
        std::ostringstream message;
        const auto& state = objects_->at(object);
        const auto stratmem = static_cast<std::uint16_t>(state.extended[48])
            | (static_cast<std::uint16_t>(state.extended[49]) << 8U);
        message << "native strategy dispatch failed for object " << object
                << " at $" << std::hex << state.strategy_address
                << " shape=$" << state.shape
                << " sword2=$" << static_cast<std::uint16_t>(state.scratch_words[1])
                << " rot=(" << static_cast<unsigned>(state.rotation_x)
                << ',' << static_cast<unsigned>(state.rotation_y)
                << ',' << static_cast<unsigned>(state.rotation_z) << ')'
                << " sflags=(" << static_cast<unsigned>(state.strategy_flags[0])
                << ',' << static_cast<unsigned>(state.strategy_flags[1])
                << ',' << static_cast<unsigned>(state.strategy_flags[2])
                << ',' << static_cast<unsigned>(state.strategy_flags[3]) << ')'
                << " coll=$" << static_cast<std::uint16_t>(state.collision_object)
                << " stratmem=$" << stratmem
                << " pathptr=$" << native_state_->read_native_word(0x7ef13bU)
                << " heap=";
        for (std::uint16_t offset = 0; offset < 16U; ++offset) {
            message << static_cast<unsigned>(native_state_->read_native_byte(
                0x7ea12fU + stratmem + offset)) << ',';
        }
        message
                << ": " << error.what();
        throw std::runtime_error{message.str()};
    }
}

StrategyTickStats NativeStrategyScheduler::tick_all() {
    StrategyTickStats result;
    auto object = objects_->first_active();
    for (std::size_t guard = 0; object != 0 && guard < 4096; ++guard) {
        const auto prior_next = objects_->next_active(object);
        result.instructions += tick_object(object);
        ++result.objects_run;

        if (!objects_->is_active(object)) {
            object = objects_->is_active(prior_next) ? prior_next : objects_->first_active();
            continue;
        }
        const auto next = objects_->next_active(object);
        if (native_state_->read_native_byte(alien_dead_) != 0) {
            result.instructions += native_state_->call_native_object_routine(remove_dead_, object);
            ++result.objects_removed;
        }
        object = next;
    }
    if (object != 0) {
        throw std::runtime_error{"native strategy list exceeded the per-tick execution limit"};
    }
    return result;
}

} // namespace starfox::simulation
