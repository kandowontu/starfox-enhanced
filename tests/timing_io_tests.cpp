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
        bytes[1024] = 0x6bU;
        bytes[0x1000] = 0x82U; bytes[0x1001] = 0x31U; bytes[0x1002] = 0x72U;
        bytes[0x1003] = 0U;
        const starfox::assets::RomImage rom{bytes};
        Wdc65816 cpu{rom};
        auto clock = std::make_shared<SnesCpuTimeline>();
        cpu.write8(0x4301U, 0x40U); cpu.write16(0x4302U, 0x9000U);
        cpu.write8(0x420cU, 1U);
        cpu.set_cpu_timeline(clock); // binding can follow initial register setup
        bool guarded{};
        try { cpu.set_cpu_timeline({}); } catch (const std::logic_error&) { guarded = true; }
        require(guarded, "HDMA ownership was lost while enabled");
        std::vector<std::uint8_t> values;
        std::uint8_t previous = cpu.read8(0x2140U);
        cpu.set_bus_clock_callback([&](std::uint32_t) {
            const auto value = cpu.read8(0x2140U);
            if (value != previous) { values.push_back(value); previous = value; }
        });
        Wdc65816Registers registers;
        cpu.call_long(0x8000U, registers, 2000U);
        require(values == std::vector<std::uint8_t>{0x31U, 0x72U}
                && cpu.read16(0x4308U) == 0x9004U && cpu.read8(0x430aU) == 0U
                && clock->totals().dma > 0U && clock->totals().cpu == cpu.executed_master_clocks(),
            "native HDMA did not follow its repeat table and stop at the terminator");
        cpu.write8(0x420cU, 0U);
        cpu.set_cpu_timeline({});
        // An expired owner cannot leave callbacks or prevent timeline reuse.
        {
            Wdc65816 temporary{rom};
            temporary.set_cpu_timeline(clock);
        }
        cpu.set_cpu_timeline(clock);
        Wdc65816 contender{rom};
        bool collision_rejected{};
        try { contender.set_cpu_timeline(clock); } catch (const std::logic_error&) { collision_rejected = true; }
        require(collision_rejected, "two CPUs were allowed to own one timeline's HDMA state");
        cpu.set_cpu_timeline({});
        contender.set_cpu_timeline(clock);
    }

    {
        std::vector<std::uint8_t> bytes(0x8000U, 0xeaU);
        const std::array<std::uint8_t, 7> code{0xa9U,1U,0x8dU,0x0bU,0x42U,0xeaU,0x6bU};
        std::copy(code.begin(), code.end(), bytes.begin());
        bytes[0x10] = 0xabU; bytes[0x11] = 0xcdU;
        const starfox::assets::RomImage rom{bytes};
        Wdc65816 cpu{rom};
        auto clock = std::make_shared<SnesCpuTimeline>();
        cpu.set_cpu_timeline(clock);
        cpu.write8(0x4301U, 0x80U); cpu.write16(0x4302U, 0x8010U);
        cpu.write16(0x4305U, 2U); cpu.write16(0x2181U, 0x1234U);
        std::uint64_t observed_cpu{};
        bool deferred{}, transferred{};
        cpu.set_bus_clock_callback([&](std::uint32_t count) {
            observed_cpu += count;
            if (observed_cpu == 54U) deferred = cpu.read8(0x7e1234U) == 0U;
            if (observed_cpu == 60U) transferred = cpu.read16(0x7e1234U) == 0xcdabU;
        });
        Wdc65816Registers registers;
        registers.status = 0x24U;
        const std::array stops{0x8006U};
        const auto task = cpu.begin_long_task(0x8000U, registers, stops, 3);
        require(task.instructions == 3U && deferred && transferred
            && observed_cpu == 60U && cpu.executed_master_clocks() == 60U
            && clock->totals().dma == 36U && clock->raster().elapsed() == 96U
            && cpu.read16(0x4302U) == 0x8012U && cpu.read16(0x4305U) == 0U,
            "native DMA did not defer a CPU cycle, transfer at its bus edge or keep DMA clocks separate");
        {
            auto irq_bytes = bytes;
            irq_bytes[6] = 0xeaU;
            irq_bytes[0x7fee] = 0x40U; irq_bytes[0x7fef] = 0x80U;
            const starfox::assets::RomImage irq_rom{std::move(irq_bytes)};
            Wdc65816 irq_cpu{irq_rom};
            auto irq_clock = std::make_shared<SnesCpuTimeline>();
            irq_cpu.set_cpu_timeline(irq_clock);
            irq_cpu.write8(0x4301U, 0x80U); irq_cpu.write16(0x4302U, 0x8010U);
            irq_cpu.write16(0x4305U, 2U);
            irq_cpu.write8(0x4207U, 15U); irq_cpu.write8(0x4208U, 0U);
            irq_cpu.write8(0x4200U, 0x10U);
            Wdc65816Registers irq_registers;
            irq_registers.status = 0x20U;
            const std::array irq_stop{0x8040U};
            const auto interrupted = irq_cpu.begin_long_task(0x8000U, irq_registers, irq_stop, 8);
            require(interrupted.instructions == 4U && irq_cpu.interrupts_taken() == 1U
                && irq_cpu.executed_master_clocks() == 138U && irq_clock->raster().elapsed() == 174U,
                "IRQ raised during DMA was delivered before the following last-cycle sample");
        }
        for (const bool reverse : {false,true}) for (const bool live : {false,true}) {
            bytes[0] = 0xeaU;
            const starfox::assets::RomImage nop_rom{bytes};
            Wdc65816 dma_cpu{nop_rom};
            auto dma_clock = std::make_shared<SnesCpuTimeline>();
            if (live) dma_cpu.set_cpu_timeline(dma_clock);
            dma_cpu.write8(0x4300U, reverse ? 0x81U : 0U);
            dma_cpu.write8(0x4301U, reverse ? 0x40U : 0x80U);
            dma_cpu.write16(0x4302U, 0x1234U); dma_cpu.write8(0x4304U, 0x7eU);
            dma_cpu.write16(0x4305U, reverse ? 2U : 1U);
            dma_cpu.write16(0x2181U, 0x1234U);
            dma_cpu.write8(0x420bU, 1U);
            bool guarded{};
            try { dma_cpu.set_cpu_timeline({}); }
            catch (const std::logic_error&) { guarded = true; }
            const std::array done{0x8001U};
            dma_cpu.begin_long_task(0x8000U, registers, done, 1);
            require(guarded == live && dma_clock->totals().dma == (live ? (reverse ? 42U : 36U) : 0U),
                "pending DMA lost its timeline or charged an incorrect synchronization delay");
            if (reverse) require(dma_cpu.read16(0x7e1234U) == 0xbbaaU,
                "reverse DMA did not read B-bus data into WRAM");
            else {
                dma_cpu.write8(0x2180U, 0x77U);
                require(dma_cpu.read8(0x7e1234U) == 0x77U && dma_cpu.read8(0x7e1235U) == 0U,
                    "invalid WRAM-to-WRAM DMA advanced the WRAM port");
            }
            dma_cpu.set_cpu_timeline({});
        }
    }
    {
        for (const bool stop : {false, true}) {
            std::vector<std::uint8_t> bytes(0x8000U, 0xeaU);
            bytes[0] = stop ? 0xdbU : 0xcbU;
            const starfox::assets::RomImage halt_rom{bytes};
            Wdc65816 cpu{halt_rom};
            auto clock = std::make_shared<SnesCpuTimeline>();
            cpu.set_cpu_timeline(clock);
            Wdc65816Registers registers;
            const std::array stops{0x8001U};
            auto task = cpu.begin_long_task(0x8000U, registers, stops, 1);
            require(task.instructions == 1U && !task.returned && task.waiting == !stop
                && task.stopped == stop && cpu.executed_master_clocks() == 14U,
                "WAI/STP did not yield after its first sampling idle");
            bool replacement_rejected{};
            try { cpu.begin_long_task(0x8100U, registers, stops, 1); }
            catch (const std::logic_error&) { replacement_rejected = true; }
            require(replacement_rejected && cpu.program_address() == 0x8001U
                && cpu.executed_master_clocks() == 14U,
                "starting another call modified the halted CPU instead of preserving it");
            for (unsigned i = 0; i < 3U; ++i) {
                task = cpu.resume_task(registers, stops, 1);
                require(task.instructions == 0U && task.waiting == !stop && task.stopped == stop
                    && cpu.executed_master_clocks() == 20U + i * 6U,
                    "resuming a halted task executed an opcode or lost its six-clock polling cadence");
            }
            cpu.set_cpu_timeline({});
            cpu.set_irq_line(true); // I is set: wakes WAI without taking IRQ.
            task = cpu.resume_task(registers, stops, 1);
            require(task.instructions == 0U && !task.waiting && task.stopped == stop
                && cpu.interrupts_taken() == 0U
                && cpu.executed_master_clocks() == (stop ? 38U : 44U),
                "masked IRQ wake, WAI trailing idle or STP persistence differs after detach");
            if (stop) {
                cpu.pulse_nmi();
                task = cpu.resume_task(registers, stops, 1);
                require(task.stopped && task.instructions == 0U && cpu.interrupts_taken() == 0U,
                    "NMI restarted STP without a reset");
            }
            Wdc65816 bounded{halt_rom};
            bounded.set_cpu_timeline(std::make_shared<SnesCpuTimeline>());
            bool rejected{};
            try { bounded.call_long(0x8000U, registers, 3); }
            catch (const Wdc65816ExecutionError&) { rejected = true; }
            require(rejected, "synchronous halted call did not honor its finite budget");
            Wdc65816 sentinel{halt_rom};
            sentinel.set_cpu_timeline(std::make_shared<SnesCpuTimeline>());
            sentinel.write8(0x7e01efU, stop ? 0xdbU : 0xcbU);
            auto at_return = sentinel.begin_long_task(0x7e01efU, registers, stops, 1);
            at_return = sentinel.resume_task(registers, stops, 1);
            require(!at_return.returned && (at_return.waiting || at_return.stopped)
                && at_return.instructions == 0U,
                "halt at the synthetic return PC was mistaken for a completed task");
        }
    }
    {
        std::vector<std::uint8_t> bytes(0x8000U, 0xeaU);
        bytes[0] = 0xcbU;
        bytes[0x7fee] = 0x40U; bytes[0x7fef] = 0x80U;
        const starfox::assets::RomImage rom{std::move(bytes)};
        Wdc65816 cpu{rom};
        cpu.set_cpu_timeline(std::make_shared<SnesCpuTimeline>());
        cpu.write8(0x4207U, 5U); cpu.write8(0x4208U, 0U);
        cpu.write8(0x4200U, 0x10U);
        Wdc65816Registers registers;
        registers.status = 0U;
        const std::array stops{0x8040U};
        auto task = cpu.begin_long_task(0x8000U, registers, stops, 1);
        for (unsigned steps = 0; task.waiting && steps < 10U; ++steps)
            task = cpu.resume_task(registers, stops, 1);
        require(!task.waiting && task.stop_address == 0x8040U && task.instructions == 0U
            && cpu.interrupts_taken() == 1U && cpu.executed_master_clocks() == 112U,
            "live horizontal timer did not wake WAI and enter IRQ after its trailing idle");
        Wdc65816 immediate{rom};
        immediate.set_cpu_timeline(std::make_shared<SnesCpuTimeline>());
        immediate.set_irq_line(true);
        registers = {};
        registers.status = 0U;
        task = immediate.begin_long_task(0x8000U, registers, stops, 1);
        require(task.instructions == 1U && task.stop_address == 0x8040U
            && immediate.executed_master_clocks() == 82U && immediate.interrupts_taken() == 1U,
            "accepted IRQ after an immediate WAI wake consumed another instruction-budget slot");
    }
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
