#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/object_pool.hpp"

#include <cstddef>
#include <cstdint>

namespace starfox::simulation {

struct StrategyTickStats {
    std::size_t objects_run{};
    std::size_t objects_removed{};
    std::size_t instructions{};
};

// Executes the original 65C816 strategy dispatcher against the shared native
// WRAM image. The native PC loop still owns cadence, input, rendering, and
// presentation; this compatibility core preserves strategy behavior while
// routines are translated and equivalence-tested individually.
class NativeStrategyScheduler {
public:
    NativeStrategyScheduler(
        const assets::SymbolMap& symbols,
        ObjectPool& objects,
        MapVm& native_state);

    [[nodiscard]] StrategyTickStats tick_all();
    [[nodiscard]] std::size_t tick_object(ObjectHandle object);
    [[nodiscard]] std::size_t begin_tick();

private:
    ObjectPool* objects_{};
    MapVm* native_state_{};
    std::uint32_t do_strategy_{};
    std::uint32_t initialize_strategies_{};
    std::uint32_t remove_dead_{};
    std::uint32_t alien_dead_{};
    std::uint32_t game_frame_{};
};

} // namespace starfox::simulation
