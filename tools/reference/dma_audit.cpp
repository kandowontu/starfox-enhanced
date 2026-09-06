// Development-only oracle: unmodified pinned Ares DMA and dmaEdge bodies.
#include "starfox/simulation/snes_dma.hpp"
#include <nall/platform.hpp>
#include <nall/memory.hpp>
#include <nall/primitives.hpp>
#include <nall/range.hpp>
#include <iostream>
#include <map>
#include <tuple>
#include <vector>

using starfox::simulation::SnesCpuTimeline;
using starfox::simulation::SnesClockWork;
struct Memory {
    using Event = std::tuple<std::uint64_t,std::uint32_t,std::uint8_t,bool>;
    SnesCpuTimeline* clock{};
    std::map<std::uint32_t,std::uint8_t> values;
    std::vector<Event> events;
    std::uint8_t read(std::uint32_t address) {
        const auto found = values.find(address);
        const auto value = found == values.end() ? std::uint8_t(address ^ (address >> 8U) ^ 0x5aU) : found->second;
        events.emplace_back(clock->raster().elapsed(), address, value, false);
        return value;
    }
    void write(std::uint32_t address, std::uint8_t value) {
        values[address] = value;
        events.emplace_back(clock->raster().elapsed(), address, value, true);
    }
};
namespace oracle {
using namespace nall;
using u32 = std::uint32_t;
using n2 = Natural<2>; using n3 = Natural<3>; using n7 = Natural<7>;
using n8 = Natural<8>; using n16 = Natural<16>; using n24 = Natural<24>;
struct CPU {
    struct Channel {
        n8 id{}, targetAddress{}, sourceBank{}, indirectBank{}, lineCounter{};
        n16 sourceAddress{}, hdmaAddress{};
        union { n16 transferSize{}; n16 indirectAddress; };
        n3 transferMode{};
        bool direction{}, fixedTransfer{}, reverseTransfer{}, dmaEnable{}, hdmaEnable{};
        bool hdmaCompleted{}, hdmaDoTransfer{}, indirect{};
        Channel* next{};
        auto step(u32) -> void; auto edge() -> void;
        auto validA(n24) -> bool; auto readA(n24) -> n8; auto readB(n8,bool) -> n8;
        auto writeA(n24,n8) -> void; auto writeB(n8,n8,bool) -> void;
        auto transfer(n24,n2) -> void; auto dmaRun() -> void;
        auto hdmaActive() -> bool; auto hdmaFinished() -> bool;
        auto hdmaReset() -> void; auto hdmaSetup() -> void; auto hdmaReload() -> void;
        auto hdmaTransfer() -> void; auto hdmaAdvance() -> void;
    };
    std::array<Channel,8> channels{};
    struct { std::uint64_t cpu{}, dma{}; } counter;
    struct {
        bool dmaActive{}, dmaPending{}, hdmaPending{}, irqLock{};
        bool hdmaSetupTriggered{}, hdmaTriggered{}, dramRefresh{};
        u32 clockCount{}, hdmaMode{}, hdmaSetupPosition{12}, hdmaPosition{1104};
        u32 autoJoypadCounter{}, dramRefreshPosition{};
    } status;
    struct { unsigned version{2}; } io;
    struct { unsigned display{225}; unsigned vdisp() const { return display; } } ppu;
    bool scheduled{};
    struct { n8 mdr{}; n24 mar{}; } r;
    struct { void dma(unsigned,unsigned,unsigned,unsigned) {} } debugger;
    SnesCpuTimeline clock{};
    unsigned hcounter() const { return clock.raster().horizontal(); }
    unsigned vcounter() const { return clock.raster().vertical(); }
    void step(u32 clocks, SnesClockWork work = SnesClockWork::dma) {
        const auto previous_line = vcounter();
        clock.step(clocks, work);
        counter.cpu = clock.raster().elapsed(); status.irqLock = false;
        if (scheduled) {
            if (previous_line != vcounter()) {
                const auto end = counter.cpu;
                counter.cpu -= hcounter(); // scanline callback runs at H=0
                scanline();
                counter.cpu = end;
            }
            events();
        }
    }
    void events() {
#include "reference-hdma-events.inl"
    }
    void scanline() {
#include "reference-hdma-scanline.inl"
    }
    unsigned dmaCounter() const { return unsigned(counter.cpu & 7U); }
    auto dmaEnable() -> bool; auto hdmaEnable() -> bool; auto hdmaActive() -> bool;
    auto dmaRun() -> void; auto hdmaReset() -> void; auto hdmaSetup() -> void; auto hdmaRun() -> void;
    auto dmaEdge() -> void;
} cpu;
struct Bus {
    Memory* memory{};
    n8 read(n24 address, n8) { return memory->read(address); }
    void write(n24 address, n8 value) { memory->write(address, value); }
} bus;
#include <ares/sfc/cpu/dma.cpp>
#include "reference-dma-edge.inl"
}


unsigned hdma_cases() {
    using namespace starfox::simulation;
    unsigned cases{};
    for (unsigned mode = 0; mode < 8; ++mode)
    for (unsigned flags : {0U, 64U, 128U, 192U})
    for (unsigned mask : {1U, 0x89U, 0xffU})
    for (unsigned cpu_clocks : {6U, 8U, 12U})
    for (unsigned lines : {0U, 1U, 2U, 0x82U})
    for (unsigned collision : {0U, 1U, 2U, 3U, 4U})
    for (bool gate : {false, true}) {
        SnesDma host;
        SnesCpuTimeline clock;
        clock.set_hdma_state(host.hdma_state());
        oracle::cpu = {};
        oracle::cpu.scheduled = true;
        Memory actual{&clock}, expected{&oracle::cpu.clock};
        oracle::bus.memory = &expected;
        constexpr std::array lengths{1U,2U,2U,4U,4U,4U,2U,4U};
        for (unsigned channel = 0; channel < 8; ++channel) {
            const auto base = channel * 16U;
            const unsigned address = 0x1000U + channel * 0x100U;
            host.registers[base] = mode | flags;
            host.registers[base + 1] = 0x18U;
            host.registers[base + 2] = address;
            host.registers[base + 3] = address >> 8U;
            host.registers[base + 4] = 0x7eU;
            host.registers[base + 7] = 0x7fU;
            auto& native = oracle::cpu.channels[channel];
            native.id = channel; native.targetAddress = 0x18U;
            native.sourceAddress = address; native.sourceBank = 0x7eU;
            native.indirectBank = 0x7fU; native.transferMode = mode;
            native.direction = flags & 128U; native.indirect = flags & 64U;
            native.hdmaEnable = mask & (1U << channel);
            native.next = channel < 7U ? &oracle::cpu.channels[channel + 1U] : nullptr;
            auto put = [&](unsigned offset, unsigned value) {
                actual.values[0x7e0000U | (address + offset)] = value;
                expected.values[0x7e0000U | (address + offset)] = value;
            };
            put(0U, lines);
            if (flags & 64U) {
                put(1U, 0xfeU); put(2U, 0xffU); // indirect pointer wraps at FFFF
                put(3U, 1U); put(4U, 0x34U); put(5U, 0x12U); put(6U, 0U);
            } else {
                const auto payload = lengths[mode] * (lines == 0x82U ? 2U : 1U);
                for (unsigned i = 1; i <= payload; ++i) put(i, 0x30U + i);
                put(payload + 1U, 1U);
                for (unsigned i = 1; i <= lengths[mode]; ++i) put(payload + 1U + i, 0x80U + i);
                put(payload + 2U + lengths[mode], 0U);
            }
        }
        host.enable_hdma(mask);
        bool injected{}, disabled{}, reenabled{};
        while (clock.raster().vertical() < 6U) {
            if (gate && ((!disabled && clock.raster().vertical() >= 1U)
                    || (disabled && !reenabled && clock.raster().vertical() >= 2U))) {
                const unsigned enable = disabled ? mask : 0U;
                if (disabled) reenabled = true;
                disabled = true;
                host.enable_hdma(enable);
                for (unsigned channel = 0; channel < 8U; ++channel)
                    oracle::cpu.channels[channel].hdmaEnable = enable & (1U << channel);
            }
            if (collision && !injected && (collision >= 3U || clock.raster().horizontal() >= 1040U)) {
                injected = true;
                const unsigned channel = collision & 1U ? 0U : 1U;
                const unsigned base = channel * 16U;
                host.registers[base + 5U] = 64U; host.registers[base + 6U] = 0U;
                oracle::cpu.channels[channel].transferSize = 64U;
                oracle::cpu.channels[channel].dmaEnable = true;
                oracle::cpu.status.dmaPending = true;
                host.request(1U << channel);
            }
            host.edge(cpu_clocks, clock, [&](auto a) { return actual.read(a); },
                [&](auto a, auto v) { actual.write(a,v); });
            oracle::cpu.status.clockCount = cpu_clocks;
            oracle::cpu.dmaEdge();
            if (actual.events != expected.events || actual.values != expected.values
                    || clock.raster().elapsed() != oracle::cpu.clock.raster().elapsed()
                    || clock.totals().dma != oracle::cpu.clock.totals().dma)
                throw std::runtime_error{"HDMA bus/timeline mismatch case " + std::to_string(cases)
                    + " at " + std::to_string(clock.raster().elapsed())};
            for (unsigned channel = 0; channel < 8U; ++channel) {
                const auto base = channel * 16U;
                const auto word = [&](unsigned offset) {
                    return host.registers[base + offset] | (unsigned(host.registers[base + offset + 1U]) << 8U);
                };
                const auto& native = oracle::cpu.channels[channel];
                if (word(2) != native.sourceAddress || word(5) != native.transferSize
                        || word(8) != native.hdmaAddress || host.registers[base + 10U] != native.lineCounter
                        || bool(host.hdma_state()->completed & (1U << channel)) != native.hdmaCompleted
                        || bool(host.hdma_state()->transfer & (1U << channel)) != native.hdmaDoTransfer)
                    throw std::runtime_error{"HDMA register mismatch case " + std::to_string(cases)
                        + " channel " + std::to_string(channel)};
            }
            clock.step(cpu_clocks);
            oracle::cpu.step(cpu_clocks, SnesClockWork::cpu);
        }
        ++cases;
    }
    return cases;
}


unsigned hdma_fields() {
    using namespace starfox::simulation;
    unsigned cases{};
    for (const auto region : {SnesRegion::ntsc, SnesRegion::pal})
    for (unsigned version : {1U, 2U}) for (unsigned display : {225U, 240U})
    for (bool interlace : {false, true}) for (unsigned cpu_clocks : {6U, 8U, 12U}) {
        SnesDma host;
        SnesCpuTimeline clock{region, std::uint8_t(version)};
        clock.set_display(interlace, display);
        clock.set_hdma_state(host.hdma_state());
        oracle::cpu = {};
        oracle::cpu.clock = SnesCpuTimeline{region, std::uint8_t(version)};
        oracle::cpu.clock.set_display(interlace, display);
        oracle::cpu.io.version = version;
        oracle::cpu.ppu.display = display;
        oracle::cpu.status.hdmaSetupPosition = version == 1U ? 20U : 12U;
        oracle::cpu.scheduled = true;
        Memory actual{&clock}, expected{&oracle::cpu.clock};
        oracle::bus.memory = &expected;
        host.registers[1] = 0x18U; host.registers[3] = 0x10U; host.registers[4] = 0x7eU;
        auto& native = oracle::cpu.channels[0];
        native.targetAddress = 0x18U; native.sourceAddress = 0x1000U;
        native.sourceBank = 0x7eU; native.hdmaEnable = true;
        for (unsigned offset = 0; offset <= 256U; ++offset) {
            const auto value = offset == 256U ? 0U : offset % 128U == 0U ? 0xffU : 0x55U;
            actual.values[0x7e1000U + offset] = value;
            expected.values[0x7e1000U + offset] = value;
        }
        host.enable_hdma(1U);
        std::size_t previous_events{};
        unsigned writes{};
        while (clock.raster().fields() < 3U) {
            host.edge(cpu_clocks, clock, [&](auto a) { return actual.read(a); },
                [&](auto a, auto v) {
                    if (clock.raster().vertical() >= display) throw std::runtime_error{"HDMA transferred in Vblank"};
                    ++writes; actual.write(a,v);
                });
            oracle::cpu.status.clockCount = cpu_clocks;
            oracle::cpu.dmaEdge();
            if (clock.raster().elapsed() != oracle::cpu.clock.raster().elapsed()
                    || clock.totals().dma != oracle::cpu.clock.totals().dma
                    || actual.events.size() != expected.events.size()
                    || (actual.events.size() != previous_events && actual.events != expected.events))
                throw std::runtime_error{"HDMA field schedule mismatch case " + std::to_string(cases)};
            previous_events = actual.events.size();
            clock.step(cpu_clocks);
            oracle::cpu.step(cpu_clocks, SnesClockWork::cpu);
        }
        if (writes != display * 3U) throw std::runtime_error{"HDMA did not transfer once per visible line"};
        ++cases;
    }
    return cases;
}

int main() try {
    using starfox::simulation::SnesDma;
    unsigned cases{};
    const std::array addresses{0x7e1234U,0x7efffeU,0x002100U,0x004000U,0x004200U,
        0x004300U,0x404200U,0x800001U,0x7f0000U,0x008000U};
    for (unsigned mode = 0; mode < 8; ++mode) for (unsigned flags : {0U,8U,16U,24U,128U,136U,144U,152U})
    for (unsigned phase : {0U,2U,4U,6U,520U,530U,536U,540U})
    for (unsigned cpu_clocks : {6U,8U,12U}) for (unsigned length : {1U,2U,7U,16U,0U})
    for (unsigned mask : {1U,0x89U,0xffU}) {
        if (length == 0U && !(mode == 0U && (flags == 0U || flags == 128U)
                && phase == 536U && cpu_clocks == 8U && mask == 1U)) continue;
        SnesDma host;
        SnesCpuTimeline clock;
        oracle::cpu = {};
        for (unsigned n = 0; n < phase; n += 2U) {
            clock.step(2U); oracle::cpu.clock.step(2U);
        }
        oracle::cpu.counter.cpu = oracle::cpu.clock.raster().elapsed();
        Memory actual{&clock}, expected{&oracle::cpu.clock};
        oracle::bus.memory = &expected;
        for (unsigned channel = 0; channel < 8; ++channel) {
            const auto address = addresses[(cases + channel) % addresses.size()];
            const auto target = std::array{0x18U,0x80U,0xffU,0x37U}[(cases + channel) % 4U];
            const auto base = channel * 16U;
            host.registers[base] = mode | flags;
            host.registers[base + 1] = target;
            host.registers[base + 2] = address; host.registers[base + 3] = address >> 8U;
            host.registers[base + 4] = address >> 16U;
            host.registers[base + 5] = length; host.registers[base + 6] = length >> 8U;
            auto& native = oracle::cpu.channels[channel];
            native.id = channel; native.targetAddress = target;
            native.sourceAddress = address; native.sourceBank = address >> 16U;
            native.transferSize = length; native.transferMode = mode;
            native.direction = flags & 128U; native.fixedTransfer = flags & 8U;
            native.reverseTransfer = flags & 16U; native.dmaEnable = mask & (1U << channel);
        }
        host.request(mask); oracle::cpu.status.dmaPending = true;
        for (unsigned edge = 0; edge < 2; ++edge) {
            host.edge(cpu_clocks, clock, [&](auto a) { return actual.read(a); },
                [&](auto a, auto v) { actual.write(a,v); });
            oracle::cpu.status.clockCount = cpu_clocks;
            oracle::cpu.dmaEdge();
            if (actual.events != expected.events || actual.values != expected.values
                    || clock.raster().elapsed() != oracle::cpu.clock.raster().elapsed()
                    || clock.totals().dma != oracle::cpu.clock.totals().dma)
                throw std::runtime_error{"DMA phase/data mismatch at case " + std::to_string(cases)};
            clock.step(cpu_clocks); oracle::cpu.clock.step(cpu_clocks);
            oracle::cpu.counter.cpu = oracle::cpu.clock.raster().elapsed();
        }
        for (unsigned channel = 0; channel < 8; ++channel) {
            const auto base = channel * 16U;
            const auto address = host.registers[base + 2] | (unsigned(host.registers[base + 3]) << 8U);
            const auto remaining = host.registers[base + 5] | (unsigned(host.registers[base + 6]) << 8U);
            if (address != oracle::cpu.channels[channel].sourceAddress
                    || remaining != oracle::cpu.channels[channel].transferSize)
                throw std::runtime_error{"DMA register mismatch"};
        }
        ++cases;
    }
    {
        SnesDma host;
        SnesCpuTimeline clock;
        oracle::cpu = {};
        Memory actual{&clock}, expected{&oracle::cpu.clock};
        oracle::bus.memory = &expected;
        host.registers[1] = 0x18U; host.registers[2] = 0x34U;
        host.registers[3] = 0x12U; host.registers[4] = 0x7eU; host.registers[5] = 1U;
        auto& native = oracle::cpu.channels[0];
        native.targetAddress = 0x18U; native.sourceBank = 0x7eU;
        native.sourceAddress = 0x1234U; native.transferSize = 1U;
        // Cancel after arming, then request again. The source's active edge
        // latch survives cancellation; the second request starts immediately.
        for (const unsigned mask : {1U,0U,1U,0U}) {
            host.request(mask); native.dmaEnable = bool(mask);
            if (mask) oracle::cpu.status.dmaPending = true;
            host.edge(8U, clock, [&](auto a) { return actual.read(a); },
                [&](auto a, auto v) { actual.write(a,v); });
            oracle::cpu.status.clockCount = 8U;
            oracle::cpu.dmaEdge();
            if (actual.events != expected.events || clock.raster().elapsed() != oracle::cpu.clock.raster().elapsed())
                throw std::runtime_error{"DMA cancellation/re-request phase mismatch"};
            clock.step(8U); oracle::cpu.clock.step(8U);
            oracle::cpu.counter.cpu = oracle::cpu.clock.raster().elapsed();
        }
        if (actual.events.size() != 2U || host.requested())
            throw std::runtime_error{"DMA cancellation check did not transfer exactly once"};
        ++cases;
    }
    const auto fields = hdma_fields();
    std::cout << fields << " three-field HDMA schedules; zero differences\n";
    const auto hdma = hdma_cases();
    std::cout << hdma << " scheduled HDMA and collision cases; zero differences\n";
    std::cout << cases << " general DMA cases; zero bus, data, register or timeline differences\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
