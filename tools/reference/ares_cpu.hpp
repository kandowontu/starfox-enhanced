#pragma once
#include "starfox/simulation/wdc65816.hpp"
#include <memory>

namespace starfox::reference {
struct CpuRun {
    unsigned instructions{};
    std::uint64_t master_clocks{};
};
struct CpuInterruptRun {
    std::uint32_t program_address{};
    std::uint64_t master_clocks{};
};
struct CpuInterruptSample {
    std::uint32_t instruction_address{};
    std::uint64_t master_clocks{};
    bool masked{};
};

// Development-only independent instruction engine, using a separate adapter
// instance for memory/I/O. No concurrent DMA, automatic interrupt delivery or
// coprocessors. Sampling callbacks can select pending-interrupt bus behavior.
class AresCpu {
public:
    explicit AresCpu(simulation::Wdc65816& memory);
    ~AresCpu();
    [[nodiscard]] std::uint8_t status_register() const noexcept;
    void set_bus_clock_callback(simulation::Wdc65816::BusClockCallback callback);
    // Observe the source's actual last-cycle polling point. Returning true
    // also selects its pending-interrupt bus behavior (idleIRQ dummy reads).
    using InterruptSampleCallback = std::function<bool(const CpuInterruptSample&)>;
    void set_interrupt_sample_callback(InterruptSampleCallback callback);
    CpuRun run(std::uint32_t entry, simulation::Wdc65816Registers& registers,
        std::uint32_t stop, unsigned instruction_limit = 10000, bool fast_rom = false);
    // One architectural native interrupt entry after the same synthetic
    // caller frame as run(). Interrupt detection/line timing is not modeled.
    CpuInterruptRun enter_interrupt(std::uint32_t interrupted_pc,
        simulation::Wdc65816Registers& registers, std::uint16_t vector,
        bool fast_rom = false);
    // Independent ALU oracle for exhaustive decimal tests; no bus operations.
    void decimal(simulation::Wdc65816Registers& registers, std::uint16_t operand,
        bool subtract);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
