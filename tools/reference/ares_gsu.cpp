// SPDX-License-Identifier: ISC
// GSU initialization below is adapted from ares v148, commit
// 0aafd85789215e84e1e43415c07d4c88461b7899. Instruction, cache, pixel and
// transfer timing implementations are included from that unmodified checkout.
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

#include "ares_gsu.hpp"
#include "starfox/simulation/gsu_device.hpp"
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <string>
#include <nall/platform.hpp>
#include <nall/memory.hpp>
#include <nall/primitives.hpp>

namespace ares {
using namespace nall;
using std::min;
using std::string;
using n4 = Natural<4>;
using n8 = Natural<8>;
using n16 = Natural<16>;
using n24 = Natural<24>;
using n32 = Natural<32>;
using i8 = Integer<8>;
using i16 = Integer<16>;
}
#include <ares/component/processor/gsu/gsu.hpp>

namespace ares {
// Stock SuperFX timing calls these when advancing its coprocessor thread.
// Both buses stay available in this adapter. Count clocks without running
// a CPU thread; fail explicitly if a future fixture attempts bus contention.
struct AuditCpu { void irq(unsigned) {} };
struct AuditScheduler {
    bool synchronizing() {
        throw std::runtime_error("Isolated GSU audit cannot wait for the CPU bus");
    }
};
struct Thread {
    std::uint64_t clocks{};
    starfox::reference::AresGsu::BusObserver observer;
    void step(u32 amount) { clocks += amount; }
    void synchronize(AuditCpu&) {}
};

struct SuperFX : GSU, Thread {
    struct Rom {
        Thread* timing;
        std::span<const std::uint8_t> bytes;
        n8 read(u32 address) { const auto value = bytes[address]; if(timing->observer) timing->observer(timing->clocks,address,value,false); return value; }
    } rom;
    struct Ram {
        Thread* timing;
        std::span<std::uint8_t> bytes;
        n8 read(u32 address) { const auto value = bytes[address]; if(timing->observer) timing->observer(timing->clocks,0x700000U+address,value,false); return value; }
        void write(u32 address, n8 data) { bytes[address] = data; if(timing->observer) timing->observer(timing->clocks,0x700000U+address,data,true); }
    } ram;
    AuditCpu cpu;
    AuditScheduler scheduler;
    u32 romMask;
    u32 ramMask;

    SuperFX(std::span<const std::uint8_t> rom_bytes,
        std::span<std::uint8_t> ram_bytes)
        : rom{this,rom_bytes}, ram{this,ram_bytes},
          romMask(static_cast<u32>(rom_bytes.size() - 1)),
          ramMask(static_cast<u32>(ram_bytes.size() - 1)) {}
    void stop() override;
    n8 color(n8) override;
    void plot(n8, n8) override;
    n8 rpix(n8, n8) override;
    void flushPixelCache(PixelCache&);
    n8 read(n24, n8 = 0) override;
    void write(n24, n8) override;
    n8 readOpcode(n16);
    n8 peekpipe();
    n8 pipe() override;
    void flushCache() override;
    n8 readCache(n16);
    void writeCache(n16, n8);
    void step(u32) override;
    void syncROMBuffer() override;
    n8 readROMBuffer() override;
    void updateROMBuffer();
    void syncRAMBuffer() override;
    n8 readRAMBuffer(n16) override;
    void writeRAMBuffer(n16, n8) override;

    starfox::reference::GsuRun run(unsigned address, unsigned stack,
        unsigned return_address, unsigned limit, bool fast_clock, std::uint8_t cfgr, bool finish_idle) {
        GSU::power();
        clocks = 0;
        for (auto& byte : cache.buffer) byte = 0;
        flushCache();
        for (auto& tile : pixelcache) {
            tile.offset = 0xffff;
            tile.bitpend = 0;
            for (auto& pixel : tile.data) pixel = 0;
        }
        regs.romcl = regs.ramcl = 0;
        regs.romdr = regs.ramar = regs.ramdr = 0;
        regs.scmr = 0x39; // both buses, 4bpp, 192 rows
        regs.scbr = 0x10; // screen base $4000
        regs.clsr = fast_clock;
        regs.cfgr = cfgr;
        regs.r[10] = stack;
        regs.r[11] = return_address;
        regs.pbr = address >> 16;
        regs.r[15] = address;
        regs.sfr.g = 1;
        unsigned instructions = 0;
        while (regs.sfr.g && instructions < limit) {
            instruction(peekpipe());
            if (regs.r[14].modified) {
                regs.r[14].modified = false;
                updateROMBuffer();
            }
            if (regs.r[15].modified) regs.r[15].modified = false;
            else regs.r[15]++;
            ++instructions;
        }
        if (regs.sfr.g) throw std::runtime_error("Isolated GSU instruction limit exceeded");
        // STOP releases the CPU but does not cancel an outstanding SRAM
        // write. Ares' idle coprocessor thread normally completes it. Our
        // isolated caller reads RAM immediately, so finish that transfer
        // explicitly and include its remaining clocks in the measurement.
        // This does not flush the pixel cache: fixtures must execute RPIX.
        starfox::reference::GsuRun result;
        result.instructions = instructions;
        result.stop_master_clocks = clocks;
        result.status = regs.sfr;
        for (unsigned n = 0; n < 16U; ++n) result.registers[n] = regs.r[n];
        if (finish_idle) step(6U); else syncRAMBuffer();
        result.master_clocks = clocks;
        return result;
    }
};

#include <ares/component/processor/gsu/instruction.cpp>
#include <ares/component/processor/gsu/instructions.cpp>
#include <ares/sfc/coprocessor/superfx/core.cpp>
#include <ares/sfc/coprocessor/superfx/memory.cpp>
#include <ares/sfc/coprocessor/superfx/timing.cpp>

// GSU::power from the pinned gsu.cpp, separated from its disassembler and
// full-emulator headers. The register values and modified flags are unchanged.
auto GSU::power() -> void {
    for (auto& r : regs.r) {
        r.data = 0;
        r.modified = false;
    }
    regs.sfr = 0;
    regs.pbr = 0;
    regs.rombr = 0;
    regs.rambr = 0;
    regs.cbr = 0;
    regs.scbr = 0;
    regs.scmr = 0;
    regs.colr = 0;
    regs.por = 0;
    regs.bramr = 0;
    regs.vcr = 0x04;
    regs.cfgr = 0;
    regs.clsr = 0;
    regs.pipeline = 0x01;
    regs.ramaddr = 0;
    regs.reset();
}
} // namespace ares

namespace starfox::reference {
struct AresGsu::Impl {
    ares::SuperFX core;
    Impl(std::span<const std::uint8_t> rom, std::span<std::uint8_t> ram)
        : core(rom, ram) {}
};

AresGsu::AresGsu(std::span<const std::uint8_t> rom, std::span<std::uint8_t> ram) {
    if (!std::has_single_bit(rom.size()) || rom.size() < 0x8000
        || rom.size() > 0x200000 || (ram.size() != 0x10000 && ram.size() != 0x20000))
        throw std::invalid_argument("Unsupported isolated GSU memory layout");
    impl_ = std::make_unique<Impl>(rom, ram);
}
AresGsu::~AresGsu() = default;
void AresGsu::set_bus_observer(BusObserver observer) { impl_->core.observer = std::move(observer); }
GsuRun AresGsu::run(unsigned address, unsigned stack, unsigned return_address,
    unsigned instruction_limit, bool fast_clock, std::uint8_t cfgr, bool finish_idle) {
    if (address > 0x5fffff || stack > 0xffff || return_address > 0xffff)
        throw std::invalid_argument("Unsupported isolated GSU entry registers");
    const auto option = std::getenv("STARFOX_COMPARE_GSU_DEVICE");
    const bool compare = option && std::string{option} == "1";
    std::vector<std::uint8_t> host_ram;
    if (compare) host_ram.assign(impl_->core.ram.bytes.begin(), impl_->core.ram.bytes.end());
    const auto result = impl_->core.run(address, stack, return_address, instruction_limit, fast_clock, cfgr, finish_idle);
    if (compare) {
        starfox::simulation::GsuDevice host{impl_->core.rom.bytes,host_ram};
        host.write_io(0x303aU,0x39U); host.write_io(0x3038U,0x10U);
        host.write_io(0x3039U,fast_clock); host.write_io(0x3037U,cfgr);
        host.write_io(0x3014U,stack); host.write_io(0x3015U,stack >> 8U);
        host.write_io(0x3016U,return_address); host.write_io(0x3017U,return_address >> 8U);
        host.write_io(0x3034U,address >> 16U);
        host.write_io(0x301eU,address); host.write_io(0x301fU,address >> 8U);
        host.run_until(result.stop_master_clocks + 1U);
        bool different = host.running() || host.instructions() != result.instructions
            || host.last_stop_master_clock() != result.stop_master_clocks
            || host.last_stop_status() != result.status
            || !std::equal(host_ram.begin(),host_ram.end(),impl_->core.ram.bytes.begin());
        for (unsigned n = 0; n < 16U; ++n)
            different |= (host.read_io(0x3000U+n*2U) | (unsigned(host.read_io(0x3001U+n*2U)) << 8U)) != result.registers[n];
        if (different) throw std::runtime_error{"Resumable GSU differs at entry " + std::to_string(address)};
    }
    return result;
}
} // namespace starfox::reference
