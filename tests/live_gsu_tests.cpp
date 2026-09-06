#include "starfox/simulation/wdc65816.hpp"
#include "starfox/simulation/snes_timeline.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace starfox::simulation;
void require(bool value, const char* message) { if (!value) throw std::runtime_error{message}; }
starfox::assets::RomImage image(bool extended = false, bool bank1 = false) {
    std::vector<std::uint8_t> bytes(0x8000U,0x01U);
    std::vector<std::uint8_t> code;
    if (bank1) code = {0xf0U,1U,0U,0x3eU,0xdfU}; // IWT R0,1; ALT2; RAMB
    const std::array<std::uint8_t,9> store{0xf0U,0xcdU,0xabU,0xf1U,0x34U,0x12U,0x31U,0x01U,0U};
    code.insert(code.end(),store.begin(),store.end());
    std::copy(code.begin(),code.end(),bytes.begin());
    bytes[0x7fbdU] = 6U; bytes[0x7fd8U] = extended ? 6U : 0U;
    bytes[0x7feeU] = 0U; bytes[0x7fefU] = 0x11U;
    return starfox::assets::RomImage{std::move(bytes)};
}
void put(Wdc65816& cpu, unsigned address, std::span<const std::uint8_t> code) {
    for (const auto value : code) cpu.write8(address++,value);
}
int main() try {
    for (bool extended : {false,true}) {
        const auto rom = image(extended,extended);
        Wdc65816 cpu{rom};
        auto clock = std::make_shared<SnesCpuTimeline>();
        cpu.set_cpu_timeline(clock); cpu.set_gsu_timing(true);
        cpu.write8(0x701111U,0x34U); cpu.write8(0x711111U,0x56U);
        require(cpu.read8(0xf01111U) == (extended ? 0x34U : 0x56U)
                && cpu.read8(0xf11111U) == 0x56U,
            "GSU full-bank RAM mirrors do not follow the cartridge size");
        cpu.write8(0x806123U,0x77U);
        require(cpu.read8(0x700123U) == 0x77U && cpu.read8(0x3f6123U) == 0x77U,
            "GSU low-bank SRAM windows do not mirror the first 8 KiB");
        require(cpu.read8(0x408000U) == cpu.read8(0xc08000U), "linear GSU ROM mirrors differ");
        const std::array<std::uint8_t,25> program{
            0xa9U,0x18U,0x8dU,0x3aU,0x30U, // enable GSU buses
            0xa9U,0U,0x8dU,0x1eU,0x30U,
            0xa9U,0x80U,0x8dU,0x1fU,0x30U, // launch GSU at 008000
            0xadU,0x30U,0x30U,0x29U,0x20U,0xd0U,0xf9U,0x6bU,0xeaU,0xeaU};
        put(cpu,0x1000U,program);
        Wdc65816Registers registers; registers.status = 0x24U;
        cpu.call_long(0x1000U,registers,1000U);
        require(cpu.read16(extended ? 0x711234U : 0x701234U) == 0xabcdU,
            "native CPU did not wait for the GSU's shared-RAM result");
        require(clock->device_clock() == clock->raster().elapsed() && clock->totals().cpu == cpu.executed_master_clocks(),
            "GSU work was added to CPU time instead of overlapping it");
        bool guarded{};
        try { cpu.set_cpu_timeline({}); } catch (const std::logic_error&) { guarded = true; }
        require(guarded, "GSU lost its CPU timeline while enabled");
        guarded = false;
        try { cpu.set_gsu_timing(false); } catch (const std::logic_error&) { guarded = true; }
        require(guarded && (cpu.read8(0x3031U) & 0x80U), "pending GSU IRQ was lost while changing timing");
        cpu.set_gsu_timing(false);
        require(!cpu.gsu_timing_enabled() && cpu.read16(extended ? 0x711234U : 0x701234U) == 0xabcdU,
            "disabling GSU timing changed shared RAM");
        cpu.set_cpu_timeline({});
    }
    for (bool rom_source : {false,true}) {
        const auto rom = image();
        Wdc65816 cpu{rom};
        auto clock = std::make_shared<SnesCpuTimeline>();
        cpu.set_cpu_timeline(clock); cpu.set_gsu_timing(true);
        cpu.write8(0x701234U,0x66U);
        cpu.write8(0x303aU,rom_source ? 0x18U : 8U);
        cpu.write16(0x301eU,0x8000U);
        cpu.write8(0x4301U,0x40U); cpu.write16(0x4302U,rom_source ? 0x8000U : 0x1234U);
        cpu.write8(0x4304U,rom_source ? 0U : 0x70U); cpu.write16(0x4305U,1U);
        cpu.write8(0x420bU,1U); cpu.write8(0x1000U,0xeaU);
        Wdc65816Registers registers;
        const std::array stops{0x1001U};
        cpu.begin_long_task(0x1000U,registers,stops,1U);
        require(cpu.read8(0x2140U) == (rom_source ? 0U : 0xeaU),
            "DMA bypassed GSU ROM/RAM bus ownership");
        require(clock->totals().dma == 36U && clock->device_clock() == 50U,
            "GSU did not follow DMA ownership and CPU resynchronization clocks");
    }
    {
        auto bytes = image().bytes();
        std::fill(bytes.begin(),bytes.begin()+200U,0x01U); bytes[200U] = 0U;
        const starfox::assets::RomImage rom{std::move(bytes)};
        Wdc65816 cpu{rom};
        auto clock = std::make_shared<SnesCpuTimeline>();
        cpu.set_cpu_timeline(clock); cpu.set_gsu_timing(true);
        cpu.write8(0x303aU,0x18U); cpu.write16(0x301eU,0x8000U);
        cpu.write8(0x1000U,0xcbU); cpu.write8(0x1001U,0xeaU); cpu.write8(0x1100U,0x40U);
        Wdc65816Registers registers; registers.status = 0x20U;
        const std::array handler{0x1100U};
        auto task = cpu.begin_long_task(0x1000U,registers,handler,1U);
        require(task.waiting, "WAI did not yield while the GSU was running");
        for (unsigned guard = 0; guard < 1000U && cpu.interrupts_taken() == 0U; ++guard)
            task = cpu.resume_task(registers,handler,1U);
        require(cpu.interrupts_taken() == 1U && !task.waiting && cpu.program_address() == 0x1100U
                && clock->totals().refresh > 0U, "GSU IRQ did not wake WAI through refresh and enter its handler");
        require(cpu.read8(0x3031U) & 0x80U, "GSU IRQ source did not remain latched until acknowledgement");
        const std::array returned{0x1001U};
        cpu.resume_task(registers,returned,8U);
        require(cpu.interrupts_taken() == 1U && cpu.program_address() == 0x1001U,
            "acknowledged GSU IRQ retriggered or corrupted RTI");
        cpu.set_gsu_timing(false);
    }
    std::cout << "native CPU/GSU waits, split RAM, cartridge mirrors, DMA ownership, IRQ/WAI and detach passed\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
