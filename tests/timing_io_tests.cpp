#include "starfox/simulation/snes_timeline.hpp"
#include "starfox/simulation/wdc65816.hpp"
#include <iostream>
#include <stdexcept>

using namespace starfox::simulation;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error{message};
}
void advance_to(SnesCpuTimeline& clock, unsigned vertical, unsigned horizontal) {
    for (unsigned guard = 0; guard < 400000U; ++guard) {
        if (clock.raster().vertical() == vertical && clock.raster().horizontal() == horizontal) return;
        clock.step(2U);
    }
    throw std::runtime_error{"unreachable test beam position"};
}
int main() try {
    {
        std::vector<std::uint8_t> bytes(0x8000U, 0xeaU);
        bytes[0x40] = bytes[0x50] = 0x40U; // IRQ/NMI handlers: RTI
        bytes[0x7fee] = 0x40U; bytes[0x7fef] = 0x80U;
        bytes[0x7fea] = 0x50U; bytes[0x7feb] = 0x80U;
        const starfox::assets::RomImage interrupt_rom{bytes};
        for (const bool nmi : {false, true}) {
            Wdc65816 cpu{interrupt_rom};
            auto clock = std::make_shared<SnesCpuTimeline>();
            cpu.set_cpu_timeline(clock);
            if (nmi) {
                advance_to(*clock, 225U, 10U);
                cpu.write8(0x4200U, 0x80U);
            } else {
                cpu.write8(0x4207U, 0U);
                cpu.write8(0x4208U, 0U);
                cpu.write8(0x4200U, 0x10U);
            }
            Wdc65816Registers registers;
            registers.status = 0U;
            const std::array stops{nmi ? 0x8050U : 0x8040U};
            const auto task = cpu.begin_long_task(0x8000U, registers, stops, 8);
            require(task.instructions == (nmi ? 1U : 2U) && cpu.interrupts_taken() == 1U
                && cpu.executed_master_clocks() == (nmi ? 78U : 92U),
                "mapped raster timer/NMI requests did not reach native interrupt delivery");
        }
        for (const bool cli : {false, true}) {
            bytes[0] = cli ? 0x58U : 0x78U;
            const starfox::assets::RomImage flag_rom{bytes};
            Wdc65816 cpu{flag_rom};
            cpu.set_cpu_timeline(std::make_shared<SnesCpuTimeline>());
            cpu.set_irq_line(true);
            Wdc65816Registers registers;
            registers.status = cli ? 4U : 0U;
            const std::array stops{0x8040U};
            const auto task = cpu.begin_long_task(0x8000U, registers, stops, 8);
            require(task.stop_address == 0x8040U && task.instructions == (cli ? 2U : 1U)
                && cpu.interrupts_taken() == 1U
                && cpu.executed_master_clocks() == (cli ? 92U : 78U)
                && bool(cpu.read8(0x1f9U) & 4U) == !cli,
                "live IRQ delivery re-tested I after CLI/SEI or stacked the sampled rather than final status");
        }
        for (const bool late : {false, true}) {
            Wdc65816 cpu{interrupt_rom};
            cpu.set_cpu_timeline(std::make_shared<SnesCpuTimeline>());
            std::uint64_t clocks{};
            bool injected{};
            cpu.set_bus_clock_callback([&](std::uint32_t count) {
                clocks += count;
                if (!injected && clocks >= (late ? 14U : 4U)) {
                    injected = true;
                    cpu.pulse_nmi();
                }
            });
            Wdc65816Registers registers;
            const std::array stops{0x8050U};
            const auto task = cpu.begin_long_task(0x8000U, registers, stops, 8);
            require(task.instructions == (late ? 2U : 1U) && cpu.interrupts_taken() == 1U
                && cpu.executed_master_clocks() == (late ? 92U : 78U),
                "an NMI arriving in the final cycle did not wait for the next sample");
        }
        {
            Wdc65816 cpu{interrupt_rom};
            cpu.set_cpu_timeline(std::make_shared<SnesCpuTimeline>());
            cpu.pulse_nmi();
            std::uint64_t clocks{};
            bool injected{};
            cpu.set_bus_clock_callback([&](std::uint32_t count) {
                clocks += count;
                if (!injected && clocks >= 20U) {
                    injected = true;
                    cpu.pulse_nmi();
                }
            });
            Wdc65816Registers registers;
            const std::array stops{0x8001U};
            // First stop is reached before entry; resume then runs both nested
            // handlers. A new edge during entry must survive acknowledgement.
            cpu.begin_long_task(0x8000U, registers, stops, 8);
            cpu.resume_task(registers, stops, 8);
            require(cpu.interrupts_taken() == 2U && registers.stack == 0x1fcU,
                "NMI entry acknowledgement erased a new edge or corrupted nested return frames");
        }
        {
            Wdc65816 cpu{interrupt_rom};
            cpu.set_cpu_timeline(std::make_shared<SnesCpuTimeline>());
            cpu.set_irq_line(true);
            cpu.pulse_nmi();
            Wdc65816Registers registers;
            registers.status = 0U;
            const std::array first{0x8050U};
            cpu.begin_long_task(0x8000U, registers, first, 8);
            require(cpu.interrupts_taken() == 1U, "NMI did not take priority over a simultaneous IRQ");
            cpu.set_irq_line(false);
            cpu.set_cpu_timeline({});
            const std::array second{0x8040U};
            cpu.resume_task(registers, second, 8);
            require(cpu.interrupts_taken() == 2U,
                "accepted IRQ was discarded after NMI entry, source release or timeline detach");
        }
    }
    std::vector<std::uint8_t> bytes(0x8000U);
    // Poll live Vblank, then return. A zero-valued stub would never complete.
    const std::array<std::uint8_t, 6> code{0xad, 0x12, 0x42, 0x10, 0xfb, 0x6b};
    std::copy(code.begin(), code.end(), bytes.begin());
    const starfox::assets::RomImage rom{std::move(bytes)};
    {
        Wdc65816 cpu{rom};
        cpu.write8(0x1000U, 0xeaU);
        cpu.write8(0x1001U, 0x6bU);
        std::vector<Wdc65816InterruptSample> samples;
        unsigned rejected{};
        cpu.set_interrupt_sample_callback([&](const Wdc65816InterruptSample& sample) {
            samples.push_back(sample);
            try { cpu.set_interrupt_sample_callback({}); }
            catch (const std::logic_error&) { ++rejected; }
            try { cpu.set_bus_clock_callback({}); }
            catch (const std::logic_error&) { ++rejected; }
            return false;
        });
        Wdc65816Registers registers;
        cpu.call_long(0x1000U, registers);
        require(samples.size() == 2U && rejected == 4U
            && samples[0] == Wdc65816InterruptSample{0x1000U, 8U, true},
            "sampling-only callback included setup clocks or allowed replacement during execution");
        std::uint64_t bus_clocks{};
        cpu.set_bus_clock_callback([&](std::uint32_t clocks) { bus_clocks += clocks; });
        cpu.call_long(0x1000U, registers);
        require(samples.size() == 4U && rejected == 8U && bus_clocks != 0U,
            "installing a bus observer discarded the sampling callback");
        cpu.set_interrupt_sample_callback({});
        const auto before = bus_clocks;
        cpu.call_long(0x1000U, registers);
        require(samples.size() == 4U && bus_clocks > before,
            "detaching sampling discarded the bus observer");
    }
    {
        Wdc65816 cpu{rom};
        auto clock = std::make_shared<SnesCpuTimeline>();
        std::uint64_t observed{};
        cpu.set_bus_clock_callback([&](std::uint32_t clocks) { observed += clocks; });
        cpu.set_cpu_timeline(clock);
        Wdc65816Registers registers;
        registers.status = 0x24U;
        const auto instructions = cpu.call_long(0x008000U, registers, 100000U);
        require(instructions > 1000U && clock->raster().vertical() == 225U,
            "native blanking wait did not run to the live raster boundary");
        require(clock->totals().cpu == observed && observed == cpu.executed_master_clocks()
            && clock->totals().refresh != 0U, "binding lost bus observations or refresh accounting");
        const auto elapsed = clock->raster().elapsed();
        cpu.set_cpu_timeline({});
        cpu.call_long(0x008005U, registers);
        require(clock->raster().elapsed() == elapsed && observed > clock->totals().cpu,
            "detaching the timeline lost the observer or kept advancing devices");
    }
    {
        Wdc65816 cpu{rom};
        auto clock = std::make_shared<SnesCpuTimeline>();
        cpu.set_cpu_timeline(clock);
        require((cpu.read8(0x4212U) & 0xc1U) == 0x40U, "initial blanking flags differ");
        clock->step(4U);
        require((cpu.read8(0x4212U) & 0xc1U) == 0U, "Hblank did not end after H=2");
        advance_to(*clock, 260U, 1200U);
        static_cast<void>(cpu.read8(0x213fU));
        static_cast<void>(cpu.read8(0x2137U));
        require(cpu.read8(0x213cU) == 44U, "horizontal counter was not latched in dots");
        static_cast<void>(cpu.read8(0x2137U));
        require(cpu.read8(0x213cU) == 45U, "SLHV reset the counter phase or lost PPU2 open-bus bits");
        require(cpu.read8(0x213dU) == 4U && cpu.read8(0x213dU) == 5U,
            "vertical counter lost its ninth bit or independent read phase");
        require((cpu.read8(0x213fU) & 0x40U) != 0U && (cpu.read8(0x213fU) & 0x40U) == 0U,
            "STAT78 did not acknowledge the counter latch");
        advance_to(*clock, 261U, 100U);
        cpu.write8(0x4201U, 0U);
        clock->step(8U);
        static_cast<void>(cpu.read8(0x2137U));
        require(cpu.read8(0x4213U) == 0U && (cpu.read8(0x213fU) & 0x40U) != 0U
            && cpu.read8(0x213cU) == 25U, "PIO falling-edge latch/gating differs");
        cpu.write8(0x4201U, 0x80U);
        static_cast<void>(cpu.read8(0x213fU));
        cpu.set_cpu_timeline({});
        require(cpu.read8(0x213cU) == 95U, "bounded-call counter behavior was not restored");
    }
    {
        Wdc65816 cpu{rom};
        auto clock = std::make_shared<SnesCpuTimeline>();
        cpu.set_cpu_timeline(clock);
        cpu.write8(0x4207U, 100U);
        cpu.write8(0x4208U, 0U);
        cpu.write8(0x4200U, 0x10U);
        advance_to(*clock, 0U, 414U);
        require((cpu.read8(0x4211U) & 0x80U) != 0U
            && clock->interrupts().sample(false) == InterruptRequest{}, "timer hold interval differs");
        clock->step(4U);
        require(clock->interrupts().sample(false) == InterruptRequest{false, true, true},
            "mapped timer writes did not produce an IRQ request");
        require((cpu.read8(0x4211U) & 0x80U) != 0U && (cpu.read8(0x4211U) & 0x80U) == 0U,
            "TIMEUP failed to acknowledge the live timer");
        cpu.write8(0x4200U, 0x80U);
        advance_to(*clock, 225U, 10U);
        require((cpu.read8(0x4210U) & 0x8fU) == 0x82U
            && clock->interrupts().sample(true) == InterruptRequest{true, false, true},
            "mapped NMI acknowledgement lost the pending edge or CPU version");
    }
    std::cout << "live CPU blanking wait, counter latches, timer I/O and detach behavior passed\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
