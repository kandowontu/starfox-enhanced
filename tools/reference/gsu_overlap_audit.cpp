// Development-only overlap oracle. Instruction/device bodies and CPU step /
// scanline bodies are unmodified pinned Ares; libco supplies its stackful waits.
#include "starfox/simulation/gsu_device.hpp"
#include "starfox/simulation/snes_timeline.hpp"
#include <nall/platform.hpp>
#include <nall/memory.hpp>
#include <nall/primitives.hpp>
#include <libco/libco.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <tuple>
#include <vector>

using Event = std::tuple<std::uint64_t,std::uint64_t,std::uint64_t,std::uint32_t,std::uint8_t,bool>;
std::vector<Event> expected_events;
std::vector<std::uint8_t> source_rom(0x8000U), source_ram(0x20000U);
namespace ares {
using namespace nall;
using std::min;
using std::string;
using n4=Natural<4>; using n8=Natural<8>; using n16=Natural<16>;
using n24=Natural<24>; using n32=Natural<32>; using i8=Integer<8>; using i16=Integer<16>;
}
#include <ares/component/processor/gsu/gsu.hpp>
namespace ares {
struct CPU;
struct SuperFX;
extern CPU cpu;
extern SuperFX superfx;
cothread_t driver{}, worker{};
struct Dummy {} smp;
struct Thread {
    std::uint64_t clocks{};
    void step(u32 amount) { clocks += amount; }
    void synchronize(CPU&);
    void synchronize(SuperFX&);
    void synchronize(Dummy&,Dummy&) {}
    void synchronize(Dummy&) {}
};
struct CPU : Thread {
    starfox::simulation::SnesRasterClock raster;
    struct { std::uint64_t cpu{}; } counter;
    struct {
        bool irqLock{}, dramRefresh{}, hdmaSetupTriggered{}, hdmaTriggered{}, hdmaPending{}, hdmaMode{};
        unsigned dramRefreshPosition{538U}, hdmaSetupPosition{12U}, hdmaPosition{1104U}, autoJoypadCounter{};
    } status;
    struct { unsigned version{2U}; } io;
    struct Ppu {
        Dummy dummy;
        unsigned vdisp() const { return 225U; }
        Dummy& thread() { return dummy; }
    } ppu;
    std::array<Dummy*,0> peripherals;
    std::array<SuperFX*,1> coprocessors{&superfx};
    bool irq_line{};
    unsigned hcounter() const { return raster.horizontal(); }
    unsigned vcounter() const { return raster.vertical(); }
    unsigned dmaCounter() const { return counter.cpu & 7U; }
    unsigned joypadCounter() const { return counter.cpu & 127U; }
    void tick() { if (raster.tick(false)) scanline(); }
    void nmiPoll() {} void irqPoll() {} void joypadEdge() {} void aluEdge() {}
    void hdmaReset() {} bool hdmaEnable() const { return false; } bool hdmaActive() const { return false; }
    void irq(unsigned state) { irq_line = state; }
    void step(u32);
    void scanline();
} cpu;
struct SuperFX : GSU, Thread {
    struct Rom {
        n8 read(u32 address) {
            const auto value = source_rom[address];
            expected_events.emplace_back(superfx.clocks,cpu.counter.cpu,cpu.clocks,address,value,false);
            return value;
        }
    } rom;
    struct Ram {
        n8 read(u32 address) {
            const auto value = source_ram[address];
            expected_events.emplace_back(superfx.clocks,cpu.counter.cpu,cpu.clocks,0x700000U+address,value,false);
            return value;
        }
        void write(u32 address,n8 value) {
            source_ram[address] = value;
            expected_events.emplace_back(superfx.clocks,cpu.counter.cpu,cpu.clocks,0x700000U+address,value,true);
        }
    } ram;
    struct Scheduler { bool synchronizing() { return false; } } scheduler;
    unsigned romMask{0x7fffU}, ramMask{0x1ffffU};
    std::uint64_t instructions{};
    void stop() override; n8 color(n8) override; void plot(n8,n8) override; n8 rpix(n8,n8) override;
    void flushPixelCache(PixelCache&);
    n8 read(n24,n8=0U) override; void write(n24,n8) override;
    n8 readOpcode(n16); n8 peekpipe(); n8 pipe() override;
    void flushCache() override; n8 readCache(n16); void writeCache(n16,n8);
    void step(u32) override; void syncROMBuffer() override; n8 readROMBuffer() override;
    void updateROMBuffer(); void syncRAMBuffer() override;
    n8 readRAMBuffer(n16) override; void writeRAMBuffer(n16,n8) override;
    n8 readIO(n24,n8); void writeIO(n24,n8);
    void initialize() {
        GSU::power(); clocks = instructions = 0U;
        for (auto& value : cache.buffer) value = 0U;
        flushCache();
        for (auto& tile : pixelcache) { tile.offset=0xffffU; tile.bitpend=0U; for(auto& value:tile.data)value=0U; }
        regs.romcl=regs.ramcl=0U; regs.romdr=regs.ramar=regs.ramdr=0U;
    }
    void main() {
        if (!regs.sfr.g) return step(6U);
        instruction(peekpipe());
        if (regs.r[14].modified) { regs.r[14].modified=false; updateROMBuffer(); }
        if (regs.r[15].modified) regs.r[15].modified=false; else regs.r[15]++;
        ++instructions;
    }
} superfx;
void Thread::synchronize(CPU& target) { if (clocks >= target.clocks) co_switch(driver); }
void Thread::synchronize(SuperFX& target) { if (target.clocks < clocks) co_switch(worker); }
#include "reference-cpu-step.inl"
#include "reference-cpu-scanline.inl"
#include <ares/component/processor/gsu/instruction.cpp>
#include <ares/component/processor/gsu/instructions.cpp>
#include <ares/sfc/coprocessor/superfx/core.cpp>
#include <ares/sfc/coprocessor/superfx/memory.cpp>
#include <ares/sfc/coprocessor/superfx/timing.cpp>
#include <ares/sfc/coprocessor/superfx/io.cpp>
#include "reference-gsu-power.inl"
void entry() { for (;;) superfx.main(); }
}

int main() try {
    using namespace starfox::simulation;
    unsigned cases{};
    for (unsigned version : {1U,2U}) for (bool fast : {false,true})
    for (unsigned phase : {0U,520U,530U,536U,1100U,1360U})
    for (unsigned cycle : {6U,8U,12U}) for (unsigned gate : {0U,1U,2U}) {
        ares::cpu = {};
        ares::cpu.io.version=version;
        ares::cpu.status.dramRefreshPosition=version==1U ? 530U : 538U;
        ares::cpu.status.hdmaSetupPosition=version==1U ? 20U : 12U;
        ares::superfx.initialize();
        std::fill(source_rom.begin(),source_rom.end(),0U);
        std::fill(source_ram.begin(),source_ram.end(),0x55U);
        // RAMB=1, buffered word store, then a cached counted loop.
        const std::array<std::uint8_t,24> code{0x02U,0xf0U,1U,0U,0x3eU,0xdfU,
            0xf0U,0xcdU,0xabU,0xf1U,0x34U,0x12U,0x31U,0xfcU,0U,2U,
            0xfdU,19U,0x80U,0x01U,0x3cU,0x01U,0U,0U};
        std::copy(code.begin(),code.end(),source_rom.begin());
        std::vector<std::uint8_t> first(0x10000U,0x55U), second(0x10000U,0x55U);
        SnesCpuTimeline clock{SnesRegion::ntsc,std::uint8_t(version)};
        auto device=std::make_shared<GsuDevice>(source_rom,first,second);
        clock.set_gsu_device(device);
        std::vector<Event> actual_events;
        expected_events.clear();
        device->set_bus_observer([&](auto t,auto a,auto v,auto w) {
            actual_events.emplace_back(t,clock.raster().elapsed(),clock.device_clock(),a,v,w);
        });
        ares::driver=co_active(); ares::worker=co_create(1U<<20U,ares::entry);
        if(!ares::worker) throw std::runtime_error{"Cannot allocate source GSU fiber"};
        struct Fiber { ~Fiber() { co_delete(ares::worker); } } fiber;
        const auto compare=[&]() {
            if (actual_events != expected_events || clock.raster().elapsed()!=ares::cpu.counter.cpu
                    || clock.device_clock()!=ares::cpu.clocks || device->master_clock()!=ares::superfx.clocks
                    || device->instructions()!=ares::superfx.instructions || device->irq()!=ares::cpu.irq_line)
                throw std::runtime_error{"GSU overlap mismatch case "+std::to_string(cases)
                    +" CPU "+std::to_string(clock.raster().elapsed())+" GSU "+std::to_string(device->master_clock())
                    +"/"+std::to_string(ares::superfx.clocks)+" events "+std::to_string(actual_events.size())+"/"+std::to_string(expected_events.size())};
        };
        const auto step=[&](unsigned clocks) {
            clock.step(clocks,(cases&1U) ? SnesClockWork::dma : SnesClockWork::cpu);
            ares::cpu.step(clocks); compare();
        };
        const auto write=[&](unsigned address,unsigned value) {
            step(cycle); device->write_io(address,value); ares::superfx.writeIO(address,value); compare();
        };
        for(unsigned elapsed=0;elapsed<phase;elapsed+=2U) step(2U);
        write(0x303aU,gate==1U ? 8U : gate==2U ? 0x10U : 0x18U);
        write(0x3037U,0U); write(0x3039U,fast);
        write(0x301eU,0U); write(0x301fU,0x80U);
        for(unsigned access=0;access<1000U;++access) {
            step(cycle-4U);
            const auto address=access%7U==0U ? 0x3031U : 0x3030U;
            if(device->read_io(address)!=ares::superfx.readIO(address,0U))
                throw std::runtime_error{"GSU live I/O mismatch"};
            compare(); step(4U);
            if(access==30U && gate) write(0x303aU,0x18U);
        }
        if(device->running() || !std::equal(first.begin(),first.end(),source_ram.begin())
                || !std::equal(second.begin(),second.end(),source_ram.begin()+0x10000U)
                || second[0x1234U]!=0xcdU || second[0x1235U]!=0xabU)
            throw std::runtime_error{"GSU overlap did not finish its split-RAM store"};
        ++cases;
    }
    std::cout<<cases<<" source CPU/GSU overlap schedules; zero clock, bus, state or memory differences\n";
} catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
