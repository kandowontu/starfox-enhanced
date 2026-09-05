#include "starfox/simulation/cpu_timing.hpp"
#include "starfox/simulation/wdc65816.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using starfox::simulation::Wdc65816;
using starfox::simulation::Wdc65816Registers;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}
void clocks(const Wdc65816& cpu, std::uint64_t expected, const char* message) {
    if (cpu.executed_master_clocks() != expected)
        throw std::runtime_error{std::string{message} + ": expected "
            + std::to_string(expected) + ", got "
            + std::to_string(cpu.executed_master_clocks())};
}

starfox::assets::RomImage fixture(std::initializer_list<std::uint8_t> code) {
    std::vector<std::uint8_t> bytes(0x8000U);
    std::copy(code.begin(), code.end(), bytes.begin());
    return starfox::assets::RomImage{std::move(bytes)};
}
}

int main() try {
    using starfox::simulation::cpu_access_master_clocks;
    // Independently listed SNES memory regions, including both sides of each
    // timing boundary and all low/high-bank mirrors. Data accesses to WRAM
    // stay slow even when instruction fetches come from FastROM.
    constexpr std::array offsets{0x0000U, 0x1fffU, 0x2000U, 0x3fffU,
        0x4000U, 0x41ffU, 0x4200U, 0x5fffU, 0x6000U, 0x7fffU,
        0x8000U, 0xffffU};
    constexpr std::array<std::uint8_t, 12> low_bank_costs{
        8, 8, 6, 6, 12, 12, 6, 6, 8, 8, 8, 8};
    for (std::uint32_t bank = 0; bank < 256U; ++bank) {
        for (const bool fast : {false, true}) {
            for (std::size_t i = 0; i < offsets.size(); ++i) {
                const auto expected = (bank & 0x40U) != 0U
                    ? (bank >= 0x80U && fast ? 6U : 8U)
                    : (bank >= 0x80U && offsets[i] >= 0x8000U && fast
                        ? 6U : low_bank_costs[i]);
                require(cpu_access_master_clocks(bank << 16U | offsets[i], fast)
                        == expected, "bus timing boundary/mirror mismatch");
            }
        }
    }

    {
        const auto rom = fixture({0xa9, 0x34, 0x12, 0x69, 1, 0, 0x6b});
        Wdc65816 cpu{rom};
        clocks(cpu, 0, "constructor setup leaked into native clocks");
        cpu.write16(0x7e1000U, 0x4321);
        require(cpu.read16(0x7e1000U) == 0x4321U, "host WRAM round trip failed");
        clocks(cpu, 0, "host inspection leaked into native clocks");
        Wdc65816Registers regs;
        cpu.call_long(0x008000U, regs);
        // LDA/ADC: three slow fetches each. RTL: one slow fetch,
        // two internal cycles and three slow stack reads.
        clocks(cpu, 24 + 24 + 44, "slow ROM long call");
        require(regs.a == 0x1235 && regs.stack == 0x1ff, "long-call result differs");
        cpu.write8(0x00420dU, 1);
        regs = {};
        cpu.call_long(0x808000U, regs);
        clocks(cpu, 92 + 18 + 18 + 42, "fast ROM long call");
        regs = {};
        cpu.call_long(0x008000U, regs);
        clocks(cpu, 170 + 92, "FastROM changed low-bank ROM");
        cpu.write8(0x80420dU, 0); // mirrored MEMSEL
        regs = {};
        cpu.call_long(0x808000U, regs);
        clocks(cpu, 262 + 92, "FastROM disable did not restore slow timing");
    }
    {
        const auto rom = fixture({0xea, 0x60}); // NOP; RTS
        Wdc65816 cpu{rom};
        Wdc65816Registers regs;
        cpu.call_near(0x008000U, regs);
        // NOP: fetch + idle. RTS: fetch + three idles + two stack reads.
        clocks(cpu, 14 + 42, "near call includes artificial stack writes");
        constexpr std::array<std::uint32_t, 1> stop{0x008001U};
        regs = {};
        const auto first = cpu.begin_near_task(0x008000U, regs, stop);
        require(!first.returned && first.stop_address == stop[0], "near task stop failed");
        clocks(cpu, 56 + 14, "near task setup or yield affected clocks");
        const auto last = cpu.resume_task(regs, stop);
        require(last.returned, "near task did not return");
        clocks(cpu, 56 + 56, "near task resume affected clocks");
    }
    {
        const auto rom = fixture({0xea, 0x6b}); // NOP; RTL
        Wdc65816 cpu{rom};
        Wdc65816Registers regs;
        constexpr std::array<std::uint32_t, 1> stop{0x008001U};
        const auto first = cpu.begin_long_task(0x008000U, regs, stop);
        require(!first.returned, "long task did not yield");
        clocks(cpu, 14, "long task setup affected clocks");
        require(cpu.resume_task(regs, stop).returned, "long task did not return");
        clocks(cpu, 58, "long task resume affected clocks");
    }
    for (const auto [address, read_cost] : std::array<std::pair<std::uint16_t, unsigned>, 7>{
            {{0x1000U, 16}, {0x2000U, 12}, {0x4000U, 24}, {0x41ffU, 18},
             {0x4200U, 12}, {0x5fffU, 14}, {0x6000U, 16}}}) {
        const auto rom = fixture({0xad, static_cast<std::uint8_t>(address),
            static_cast<std::uint8_t>(address >> 8U), 0x6b}); // LDA abs; RTL
        Wdc65816 cpu{rom};
        Wdc65816Registers regs;
        cpu.call_long(0x008000U, regs);
        clocks(cpu, 24 + read_cost + 44, "16-bit data read across timing boundary");
    }
    {
        // Switch MEMSEL from code in the high ROM mirror. The STA operands
        // are still slow; the next instruction fetch must become fast.
        const auto rom = fixture({0xe2, 0x20, 0xa9, 1, 0x8d, 0x0d, 0x42, 0xea, 0x6b});
        Wdc65816 cpu{rom};
        Wdc65816Registers regs;
        cpu.call_long(0x808000U, regs);
        clocks(cpu, 22 + 16 + 30 + 12 + 42, "executed MEMSEL transition");
    }
    {
        const auto rom = fixture({0xeb, 0x6b}); // XBA; RTL
        Wdc65816 cpu{rom};
        Wdc65816Registers regs;
        regs.status = 0x24;
        regs.a = 0x1234;
        cpu.call_long(0x008000U, regs);
        require(regs.a == 0x3412, "8-bit native call lost hidden B accumulator");
        clocks(cpu, 20 + 44, "XBA internal cycles");
    }
    for (const auto [a, b, status, subtract, result, flags] : std::array{
            std::tuple{0x9999U, 1U, 0x0cU, false, 0U, 3U},
            std::tuple{0U, 1U, 0x0dU, true, 0x9999U, 0x80U},
            std::tuple{0xab50U, 0x50U, 0x2cU, false, 0xab00U, 0x43U},
            std::tuple{0xab00U, 1U, 0x2dU, true, 0xab99U, 0x80U},
            std::tuple{0xab00U, 0xffU, 0x24U, true, 0xab00U, 2U},
            std::tuple{0U, 0xffffU, 0x04U, true, 0U, 2U}}) {
        auto rom = (status & 0x20U) != 0U
            ? fixture({static_cast<std::uint8_t>(subtract ? 0xe9 : 0x69),
                static_cast<std::uint8_t>(b), 0x6b})
            : fixture({static_cast<std::uint8_t>(subtract ? 0xe9 : 0x69),
                static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(b >> 8U), 0x6b});
        Wdc65816 cpu{rom};
        Wdc65816Registers regs;
        regs.status = static_cast<std::uint8_t>(status);
        regs.a = static_cast<std::uint16_t>(a);
        cpu.call_long(0x008000U, regs);
        require(regs.a == result && (regs.status & 0xc3U) == flags,
            "native arithmetic result/flags or hidden accumulator differ");
    }
    for (const bool reverse : {false,true}) {
        const auto rom = fixture({static_cast<std::uint8_t>(reverse ? 0x44 : 0x54),
            0x7f, 0x7e, 0x6b}); // move two bytes across an 8-bit index boundary
        Wdc65816 cpu{rom};
        cpu.write8(0x7e00ff, 0x12); cpu.write8(0x7e0000, 0x34);
        Wdc65816Registers regs;
        regs.status = 0x34; regs.a = 1;
        regs.x = regs.y = reverse ? 0 : 0xff;
        require(cpu.call_long(0x008000U, regs) == 3,
            "block move executed an extra iteration after the final byte");
        require(regs.a == 0xffff && regs.x == (reverse ? 0xfe : 1)
                && regs.y == regs.x && regs.data_bank == 0x7f
                && cpu.read8(0x7f00ff) == 0x12 && cpu.read8(0x7f0000) == 0x34,
            "block move did not wrap the native 8-bit index registers");
        clocks(cpu, 2 * 52 + 44, "block move per-byte bus/internal clocks");
    }
    {
        std::vector<std::uint8_t> bytes(0x200000U);
        bytes[0x1ffffeU] = 0xa9; bytes[0x1fffffU] = 0x12;
        const starfox::assets::RomImage rom{std::move(bytes)};
        Wdc65816 cpu{rom};
        cpu.write8(0x000000U, 0x34);
        Wdc65816Registers regs;
        constexpr std::array<std::uint32_t, 1> stop{0x3f0001U};
        cpu.begin_long_task(0x3ffffeU, regs, stop, 1);
        require(regs.a == 0x3412, "16-bit operand fetch crossed its program bank");
        clocks(cpu, 24, "program-bank wrap bus clocks");
    }
    {
        const auto rom = fixture({0xee, 0x42, 0x21, 0x6b}); // INC $2142; RTL
        Wdc65816 cpu{rom};
        cpu.write16(0x002142, 0x1234);
        (void)cpu.take_apu_port_writes();
        Wdc65816Registers regs;
        cpu.call_long(0x008000U, regs);
        require(cpu.take_apu_port_writes()
                == std::vector<starfox::simulation::ApuPortWrite>{{3,0x12},{2,0x35}},
            "16-bit read-modify-write did not write the high byte first");
        clocks(cpu, 54 + 44, "read-modify-write I/O timing");
    }
    for (const auto bank : {0x7e0000U, 0x7f0000U}) {
        for (const auto offset : {0x7ff0U, 0x8000U, 0xfffaU}) {
            const auto rom = fixture({0x6b});
            Wdc65816 cpu{rom};
            constexpr std::array<std::uint8_t, 7> code{0xa9,0x34,0x12,0x69,1,0,0x6b};
            for (unsigned i = 0; i < code.size(); ++i)
                cpu.write8(bank | ((offset + i) & 0xffffU), code[i]);
            Wdc65816Registers regs;
            regs.status = 0x04;
            cpu.call_long(bank | offset, regs);
            require(regs.a == 0x1235, "native call rejected or misread executable WRAM");
            clocks(cpu, 92, "WRAM instruction fetch clocks");
            constexpr unsigned instruction_bytes = 3;
            const std::array stop{bank | ((offset + instruction_bytes) & 0xffffU)};
            regs = {};
            const auto started = cpu.begin_long_task(bank | offset, regs, stop);
            require(!started.returned && regs.a == 0x1234,
                "WRAM task did not stop after its first instruction");
            const auto finished = cpu.resume_task(regs, {});
            require(finished.returned && regs.a == 0x1235,
                "WRAM task did not resume through the bank's upper half");
            clocks(cpu, 184, "WRAM call/task cumulative clocks");
        }
    }
    {
        const auto rom = fixture({0x6b});
        Wdc65816 cpu{rom};
        Wdc65816Registers regs;
        bool rejected = false;
        try { cpu.call_long(0x018000, regs); }
        catch (const starfox::simulation::Wdc65816ExecutionError&) { rejected = true; }
        require(rejected, "WRAM execution fix disabled the unmapped ROM guard");
    }
    std::cout << "CPU bus regions, FastROM transitions, internal cycles and call/task accounting pass\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
