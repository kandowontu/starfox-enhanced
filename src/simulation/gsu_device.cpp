// SPDX-License-Identifier: ISC
// Resumable adaptation of Ares v148 GSU scheduling and CPU bus access.
// Copyright (c) 2004-2025 ares team, Near et al. See gsu/LICENSE-ARES.txt.
#include "starfox/simulation/gsu_device.hpp"
#include "gsu/task.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace starfox::simulation::gsu {
using std::min;

struct Core {
#include "gsu/generated-registers.inl"

    GsuDevice::BusObserver observer;
    void observe(u32 address, n8 value, bool writing) {
        if (observer) observer(clocks,address,value,writing);
    }
    struct Rom {
        Core& owner;
        std::span<const std::uint8_t> bytes;
        n8 read(u32 address) const { const auto value = bytes[address]; owner.observe(address,value,false); return value; }
    } rom;
    struct Ram {
        Core& owner;
        std::span<std::uint8_t> bytes, additional;
        n8 peek(u32 address) const { return address < bytes.size() ? bytes[address] : additional[address - bytes.size()]; }
        void poke(u32 address, n8 value) { (address < bytes.size() ? bytes[address] : additional[address - bytes.size()]) = value; }
        n8 read(u32 address) const { const auto value = peek(address); owner.observe(0x700000U + address,value,false); return value; }
        void write(u32 address, n8 value) { poke(address,value); owner.observe(0x700000U + address,value,true); }
    } ram;
    u32 romMask, ramMask;
    std::uint64_t clocks{}, target{}, instruction_count{}, stopped_at{};
    std::uint16_t stopped_status{};
    bool irq_line{}, advancing{};
    void require_idle() const { if (advancing) throw std::logic_error{"Cannot reenter an advancing GSU device"}; }
    std::coroutine_handle<> suspended;

    struct ClockWait {
        Core& core;
        u32 clocks;
        bool await_ready() {
            if (core.clocks > std::numeric_limits<std::uint64_t>::max() - clocks)
                throw std::overflow_error{"GSU clock overflow"};
            core.clocks += clocks;
            return core.clocks < core.target;
        }
        void await_suspend(std::coroutine_handle<> handle) noexcept { core.suspended = handle; }
        void await_resume() const noexcept {}
    };
    ClockWait clock_wait(u32 clocks) noexcept { return {*this, clocks}; }
#include "gsu/generated-declarations.inl"

    Task<> main() {
        for (;;) {
            if (!regs.sfr.g) { co_await step(6U); continue; }
            const auto opcode = co_await peekpipe();
            co_await instruction(opcode);
            if (regs.r[14].modified) {
                regs.r[14].modified = false;
                updateROMBuffer();
            }
            if (regs.r[15].modified) regs.r[15].modified = false;
            else regs.r[15]++;
            ++instruction_count;
            if (!regs.sfr.g) { stopped_at = clocks; stopped_status = regs.sfr; }
        }
    }
    Task<> execution;

    Core(std::span<const std::uint8_t> rom_bytes, std::span<std::uint8_t> ram_bytes, std::span<std::uint8_t> additional)
        : regs{}, cache{}, pixelcache{}, rom{*this,rom_bytes}, ram{*this,ram_bytes,additional},
          romMask(u32(rom_bytes.size() - 1U)), ramMask(u32(ram_bytes.size() + additional.size() - 1U)) {
        regs.pipeline = 0x01U;
        regs.vcr = 0x04U;
        regs.reset();
        for (auto& tile : pixelcache) tile.offset = 0xffffU;
        execution = main();
        suspended = execution.handle();
    }
    void run_until(std::uint64_t deadline) {
        require_idle();
        execution.check();
        if (deadline < target) throw std::invalid_argument{"GSU timeline cannot run backwards"};
        target = deadline;
        struct Guard { bool& active; ~Guard() { active = false; } } guard{advancing};
        advancing = true;
        if (clocks < target) {
            suspended.resume();
            execution.check();
        }
    }
};

#include "gsu/generated-core.inl"
} // namespace starfox::simulation::gsu

namespace starfox::simulation {
struct GsuDevice::Impl { gsu::Core core; Impl(std::span<const std::uint8_t> rom, std::span<std::uint8_t> ram, std::span<std::uint8_t> additional) : core(rom,ram,additional) {} };
GsuDevice::GsuDevice(std::span<const std::uint8_t> rom, std::span<std::uint8_t> ram, std::span<std::uint8_t> additional) {
    if (!std::has_single_bit(rom.size()) || rom.size() < 0x8000U || rom.size() > 0x200000U
            || !((additional.empty() && (ram.size() == 0x10000U || ram.size() == 0x20000U))
                || (ram.size() == 0x10000U && additional.size() == 0x10000U)))
        throw std::invalid_argument{"Unsupported GSU memory layout"};
    impl_ = std::make_unique<Impl>(rom,ram,additional);
}
GsuDevice::~GsuDevice() = default;
void GsuDevice::set_bus_observer(BusObserver observer) { impl_->core.require_idle(); impl_->core.observer = std::move(observer); }
void GsuDevice::run_until(std::uint64_t clock) { impl_->core.run_until(clock); }
std::uint64_t GsuDevice::master_clock() const noexcept { return impl_->core.clocks; }
std::uint64_t GsuDevice::last_stop_master_clock() const noexcept { return impl_->core.stopped_at; }
std::uint16_t GsuDevice::last_stop_status() const noexcept { return impl_->core.stopped_status; }
std::uint64_t GsuDevice::instructions() const noexcept { return impl_->core.instruction_count; }
bool GsuDevice::running() const noexcept { return impl_->core.regs.sfr.g; }
bool GsuDevice::owns_rom() const noexcept { return impl_->core.regs.sfr.g && impl_->core.regs.scmr.ron; }
bool GsuDevice::owns_ram() const noexcept { return impl_->core.regs.sfr.g && impl_->core.regs.scmr.ran; }
std::uint32_t GsuDevice::ram_size() const noexcept { return impl_->core.ramMask + 1U; }
bool GsuDevice::irq() const noexcept { return impl_->core.irq_line; }
std::uint32_t GsuDevice::pending_ram_clocks() const noexcept { return impl_->core.regs.ramcl; }
std::uint8_t GsuDevice::read_io(std::uint32_t address) { impl_->core.require_idle(); return impl_->core.readIO(address,0U); }
void GsuDevice::write_io(std::uint32_t address, std::uint8_t value) { impl_->core.require_idle(); impl_->core.writeIO(address,value); }
std::uint8_t GsuDevice::read_cpu_rom(std::uint32_t offset) const {
    const auto& core = impl_->core;
    if (core.regs.sfr.g && core.regs.scmr.ron) {
        constexpr std::uint8_t vector[16]{0,1,0,1,4,1,0,1,0,1,8,1,0,1,12,1};
        return vector[offset & 15U];
    }
    return core.rom.bytes[offset & core.romMask];
}
std::uint8_t GsuDevice::read_cpu_ram(std::uint32_t offset, std::uint8_t open_bus) const {
    const auto& core = impl_->core;
    if (core.regs.sfr.g && core.regs.scmr.ran) return open_bus;
    return core.ram.peek(offset & core.ramMask);
}
void GsuDevice::write_cpu_ram(std::uint32_t offset, std::uint8_t value) {
    auto& core = impl_->core;
    core.require_idle();
    core.ram.poke(offset & core.ramMask,value);
}
} // namespace starfox::simulation
