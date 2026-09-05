// Optional audit adapter. This header does not make Ares a game dependency.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace starfox::reference {

struct GsuRun {
    unsigned instructions{};
    std::uint64_t master_clocks{};
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
    [[nodiscard]] GsuRun run(unsigned address, unsigned stack = 0,
        unsigned return_address = 0, unsigned instruction_limit = 10'000'000,
        bool fast_clock = false);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace starfox::reference
