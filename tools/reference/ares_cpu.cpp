// SPDX-License-Identifier: ISC
// Bus timing expression adapted from ares v148 CPU::wait. Instruction bodies
// are included from the unmodified pinned checkout.
//
// Copyright (c) 2004-2025 ares team, Near et al
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "ares_cpu.hpp"
#include <stdexcept>
#include <nall/platform.hpp>
#include <nall/memory.hpp>
#include <nall/primitives.hpp>

namespace ares {
using namespace nall;
using std::string;
using n8 = Natural<8>;
using n16 = Natural<16>;
using n24 = Natural<24>;
using n32 = Natural<32>;
using i8 = Integer<8>;
using i16 = Integer<16>;
}
#include <ares/component/processor/wdc65816/wdc65816.hpp>
namespace ares {
#include <ares/component/processor/wdc65816/registers.hpp>
#include <ares/component/processor/wdc65816/memory.cpp>
#include <ares/component/processor/wdc65816/algorithms.cpp>
#include <ares/component/processor/wdc65816/instructions-read.cpp>
#include <ares/component/processor/wdc65816/instructions-write.cpp>
#include <ares/component/processor/wdc65816/instructions-modify.cpp>
#include <ares/component/processor/wdc65816/instructions-pc.cpp>
#include <ares/component/processor/wdc65816/instructions-other.cpp>
#include <ares/component/processor/wdc65816/instruction.cpp>
#include <ares/component/processor/wdc65816/registers.hpp>
}

namespace starfox::reference {
struct AresCpu::Impl : ares::WDC65816 {
    simulation::Wdc65816& memory;
    std::uint64_t clocks{};
    bool fast_rom{};
    explicit Impl(simulation::Wdc65816& bus) : memory(bus) {}
    unsigned access(unsigned address) const {
        if (address & 0x408000U)
            return address & 0x800000U ? (fast_rom ? 6U : 8U) : 8U;
        if ((address + 0x6000U) & 0x4000U) return 8;
        if ((address - 0x4000U) & 0x7e00U) return 6;
        return 12;
    }
    void idle() override { clocks += 6; }
    ares::n8 read(ares::n24 address) override {
        clocks += access(address);
        return memory.read8(address);
    }
    void write(ares::n24 address, ares::n8 data) override {
        clocks += access(address);
        if ((address & 0x40ffffU) == 0x00420dU) fast_rom = (data & 1U) != 0U;
        memory.write8(address, data);
    }
    void lastCycle() override {}
    bool interruptPending() const override { return false; }
    bool synchronizing() const override { return true; }
};

AresCpu::AresCpu(simulation::Wdc65816& memory) : impl_(std::make_unique<Impl>(memory)) {}
AresCpu::~AresCpu() = default;
CpuInterruptRun AresCpu::enter_interrupt(std::uint32_t interrupted_pc,
    simulation::Wdc65816Registers& registers, std::uint16_t vector, bool fast_rom) {
    auto& cpu = *impl_;
    auto& r = cpu.r;
    cpu.fast_rom = fast_rom;
    r.pc = interrupted_pc;
    r.a = registers.a; r.x = registers.x; r.y = registers.y;
    r.d = registers.direct; r.s = registers.stack; r.b = registers.data_bank;
    r.p = registers.status; r.e = false;
    r.irq = r.wai = r.stp = false; r.z = 0; r.vector = vector;
    cpu.pushN(0x7e); cpu.pushN(0x01); cpu.pushN(0xef);
    cpu.clocks = 0;
    cpu.interrupt();
    registers.a = r.a.w; registers.x = r.x.w; registers.y = r.y.w;
    registers.direct = r.d.w; registers.stack = r.s.w;
    registers.data_bank = r.b; registers.status = static_cast<unsigned>(r.p);
    return {static_cast<unsigned>(r.pc.d), cpu.clocks};
}
void AresCpu::decimal(simulation::Wdc65816Registers& registers,
    std::uint16_t operand, bool subtract) {
    auto& cpu = *impl_;
    cpu.r.a = registers.a;
    cpu.r.p = registers.status | 8U;
    if (cpu.r.p.m) {
        if (subtract) cpu.algorithmSBC8(operand);
        else cpu.algorithmADC8(operand);
    } else {
        if (subtract) cpu.algorithmSBC16(operand);
        else cpu.algorithmADC16(operand);
    }
    registers.a = cpu.r.a.w;
    registers.status = static_cast<unsigned>(cpu.r.p);
}
CpuRun AresCpu::run(std::uint32_t entry, simulation::Wdc65816Registers& registers,
    std::uint32_t stop, unsigned instruction_limit, bool fast_rom) {
    auto& cpu = *impl_;
    auto& r = cpu.r;
    cpu.fast_rom = fast_rom;
    r.pc = entry;
    r.a = registers.a;
    r.x = registers.x;
    r.y = registers.y;
    r.d = registers.direct;
    r.s = registers.stack;
    r.b = registers.data_bank;
    r.p = registers.status;
    r.e = false;
    r.irq = r.wai = r.stp = false;
    r.z = 0;
    // Same synthetic long-call entry as Wdc65816, excluded from the clocks.
    cpu.pushN(0x7e);
    cpu.pushN(0x01);
    cpu.pushN(0xef);
    cpu.clocks = 0;
    unsigned count = 0;
    do {
        if (count == instruction_limit) throw std::runtime_error("Ares CPU audit instruction limit");
        cpu.instruction();
        ++count;
    } while (static_cast<unsigned>(r.pc.d) != stop);
    registers.a = r.a.w;
    registers.x = r.x.w;
    registers.y = r.y.w;
    registers.direct = r.d.w;
    registers.stack = r.s.w;
    registers.data_bank = r.b;
    registers.status = static_cast<unsigned>(r.p);
    return {count, cpu.clocks};
}
}
