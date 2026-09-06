// Optional audit adapter. This header does not make Ares a game dependency.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace starfox::reference {

struct GsuRun {
    unsigned instructions{};
    std::uint64_t master_clocks{};
    std::uint64_t stop_master_clocks{};
    std::array<std::uint16_t, 16> registers{};
    std::uint16_t status{};
};

// Run an isolated cartridge entry with a cold GSU pipeline/cache. The caller
// owns ROM and SRAM; only SRAM changes. Clocks include any pending SRAM write
// finishing after STOP. CPU/GSU contention, DMA, and video scheduling are
// deliberately outside this isolated measurement.
class AresGsu {
public:
    AresGsu(std::span<const std::uint8_t> rom, std::span<std::uint8_t> ram);
    ~AresGsu();
    AresGsu(const AresGsu&) = delete;
    AresGsu& operator=(const AresGsu&) = delete;
    using BusObserver = std::function<void(std::uint64_t, std::uint32_t, std::uint8_t, bool)>;
    void set_bus_observer(BusObserver observer);
    [[nodiscard]] GsuRun run(unsigned address, unsigned stack = 0,
        unsigned return_address = 0, unsigned instruction_limit = 10'000'000,
        bool fast_clock = false, std::uint8_t cfgr = 0U, bool finish_idle = false);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace starfox::reference
