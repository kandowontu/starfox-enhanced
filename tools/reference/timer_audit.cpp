// Development-only harness: interrupt algorithms are compiled directly from
// the unmodified pinned Ares checkout (ISC); see THIRD_PARTY_NOTICES.md.
#include "starfox/simulation/snes_interrupts.hpp"
#include <nall/platform.hpp>
#include <nall/memory.hpp>
#include <nall/primitives.hpp>
#include <iostream>
#include <stdexcept>

namespace oracle {
using namespace nall;
using n8 = Natural<8>;
using boolean = Boolean;
struct CPU {
    starfox::simulation::InterruptBeam beam;
    struct {
        bool irqLock{}, interruptPending{};
        boolean nmiHold, nmiTransition, nmiValid, nmiLine, nmiPending;
        boolean irqHold, irqTransition, irqValid, irqLine, irqPending;
    } status;
    struct {
        boolean nmiEnable, hirqEnable, virqEnable, irqEnable;
        Natural<12> htime{2048};
        Natural<9> vtime{511};
    } io;
    struct {
        bool wai{}, irq{};
        struct { bool i{}; } p;
    } r;
    struct {
        unsigned line{225};
        unsigned vdisp() const { return line; }
    } ppu;
    unsigned vcounter(unsigned delay) const {
        return delay == 2 ? beam.vertical_2 : delay == 6 ? beam.vertical_6 : beam.vertical_10;
    }
    unsigned hcounter(unsigned delay) const {
        return delay == 6 ? beam.horizontal_6 : beam.horizontal_10;
    }
    auto nmiPoll() -> void;
    auto irqPoll() -> void;
    auto nmitimenUpdate(n8) -> void;
    auto rdnmi() -> bool;
    auto timeup() -> bool;
    auto nmiTest() -> bool;
    auto irqTest() -> bool;
    auto lastCycle() -> void;
};
#include <ares/sfc/cpu/irq.cpp>
}

int main() try {
    using namespace starfox::simulation;
    SnesInterrupts host;
    oracle::CPU source;
    std::uint32_t random = 0x538e102d;
    auto next = [&] {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        return random;
    };
    unsigned comparisons{};
    for (unsigned event = 0; event < 1000000; ++event) {
        // Bias toward comparator matches, blanking transitions and field
        // boundaries while retaining out-of-range 9-bit timer values.
        const auto h = static_cast<std::uint16_t>(next() % 3 == 0
            ? static_cast<unsigned>(source.io.htime) : (next() % 342) * 4);
        const auto v = static_cast<std::uint16_t>(next() % 3 == 0
            ? static_cast<unsigned>(source.io.vtime) : next() % 313);
        source.beam = {static_cast<std::uint16_t>(next() % 313),
            static_cast<std::uint16_t>(next() % 8 ? h + 4 : 0),
            static_cast<std::uint16_t>(next() % 8 ? v : 0), h, v,
            static_cast<std::uint16_t>(next() & 1 ? 225 : 240)};
        source.ppu.line = source.beam.vblank_start;
        const auto value = static_cast<std::uint8_t>(next());
        const auto operation = next() % 12;
        if (operation < 4) {
            host.poll(source.beam);
            source.nmiPoll();
            source.irqPoll();
        } else if (operation == 4) {
            host.write_control(value);
            source.nmitimenUpdate(value);
        } else if (operation == 5) {
            const auto index = next() & 3;
            host.write_timer(index, value, source.beam);
            // Register decoding only; the independent comparator body is
            // the original irq.cpp, including its nall edge/hold primitives.
            if (index < 2) {
                unsigned raw = (static_cast<unsigned>(source.io.htime) >> 2) - 1;
                raw = index ? (raw & 255) | ((value & 1) << 8) : (raw & 256) | value;
                source.io.htime = (raw + 1) * 4;
            } else if (index == 2) source.io.vtime.bit(0,7) = value;
            else source.io.vtime.bit(8) = value & 1;
            source.irqPoll();
        } else if (operation == 6) {
            const auto expected = (value & 0x7f) | (source.timeup() ? 0x80 : 0);
            if (host.read_irq(value) != expected)
                throw std::runtime_error{"TIMEUP differs at event " + std::to_string(event)};
            ++comparisons;
        } else if (operation == 7) {
            const auto expected = (value & 0x70) | 2 | (source.rdnmi() ? 0x80 : 0);
            if (host.read_nmi(value) != expected)
                throw std::runtime_error{"RDNMI differs at event " + std::to_string(event)};
            ++comparisons;
        } else if (operation == 8) {
            host.clock_step();
            source.status.irqLock = false;
        } else if (operation == 9) {
            host.inhibit();
            source.status.irqLock = true;
        } else {
            source.r.p.i = value & 1;
            source.r.irq = value & 2;
            source.r.wai = true;
            source.status.nmiPending = source.status.irqPending = false;
            source.lastCycle();
            const InterruptRequest expected{static_cast<bool>(source.status.nmiPending),
                static_cast<bool>(source.status.irqPending), !source.r.wai};
            if (host.sample(source.r.p.i, source.r.irq) != expected)
                throw std::runtime_error{"interrupt polling differs at event " + std::to_string(event)};
            ++comparisons;
        }
    }
    std::cout << "1000000 timer events, " << comparisons
        << " acknowledgement/poll comparisons, zero differences against pinned Ares irq.cpp\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
