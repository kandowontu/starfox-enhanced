#pragma once
#include "starfox/simulation/wdc65816.hpp"
#include <memory>

namespace starfox::reference {
struct CpuRun {
    unsigned instructions{};
    std::uint64_t master_clocks{};
};

// Development-only independent instruction engine, using a separate adapter
// instance for memory/I/O. No concurrent DMA, interrupts or coprocessors.
class AresCpu {
public:
    explicit AresCpu(simulation::Wdc65816& memory);
    ~AresCpu();
    CpuRun run(std::uint32_t entry, simulation::Wdc65816Registers& registers,
        std::uint32_t stop, unsigned instruction_limit = 10000, bool fast_rom = false);
    // Independent ALU oracle for exhaustive decimal tests; no bus operations.
    void decimal(simulation::Wdc65816Registers& registers, std::uint16_t operand,
        bool subtract);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
