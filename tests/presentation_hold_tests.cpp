#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/snes_timeline.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace starfox::simulation;
void require(bool value,const char* message) { if (!value) throw std::runtime_error{message}; }
int main() try {
    for (bool extended : {false,true}) {
        std::vector<std::uint8_t> bytes(0x8000U);
        bytes[0x7fbdU]=6U; bytes[0x7fd8U]=extended ? 6U : 0U;
        const starfox::assets::RomImage rom{std::move(bytes)};
        ObjectPool objects;
        MapVm map{rom,MapDatabase{rom,0x8000U,0x8100U},objects};
        auto timeline=std::make_shared<SnesCpuTimeline>();
        map.set_cpu_timeline(timeline); map.set_gsu_timing(true);
        map.write_native_byte(0x7f1234U,0x11U);
        map.write_native_byte(0x701234U,0x33U);
        map.write_native_byte(0x711234U,0x55U);
        map.write_native_byte(0x7e0010U,0x77U);
        map.write_vram(0U,std::array<std::uint8_t,1>{0x12U});
        map.write_cgram(0U,std::array<std::uint16_t,1>{0x1234U});
        NativeModelDrawState model; model.active=true; model.shape=123U;
        map.set_native_model_draw(model);
        // Two native stores separated by a real instruction boundary.
        const std::array<std::uint8_t,13> program{
            0xa9U,0x22U,0x8fU,0x34U,0x12U,0x7fU,
            0xa9U,0x44U,0x8fU,0x34U,0x12U,0x70U,0x6bU};
        for (unsigned i=0;i<program.size();++i) map.write_native_byte(0x7e6000U+i,program[i]);
        map.hold_native_presentation();
        bool rejected{};
        try { map.hold_native_presentation(); } catch (const std::logic_error&) { rejected=true; }
        require(rejected,"nested presentation hold replaced the published frame");
        Wdc65816Registers registers; registers.status=0x24U;
        const std::array stop{0x7e6006U};
        auto task=map.begin_native_task(0x7e6000U,registers,stop,100U);
        require(!task.returned && map.read_native_byte(0x7f1234U)==0x11U,
            "unfinished native WRAM store leaked into presentation");
        map.write_vram(0U,std::array<std::uint8_t,1>{0x56U});
        map.write_cgram(0U,std::array<std::uint16_t,1>{0x5678U});
        model.shape=456U; map.set_native_model_draw(model);
        require(map.ppu_state().vram[0]==0x12U && map.ppu_state().cgram[0]==0x1234U
            && map.native_model_draw().shape==123U,
            "partial raster/model changes leaked into held presentation");
        require(map.read_native_byte(0x800010U)==0x77U
            && map.read_native_byte(0xf11234U)==0x55U,
            "held presentation lost native RAM aliases");
        task=map.resume_native_task(registers,{},100U);
        require(task.returned && map.read_native_byte(0x701234U)==(extended ? 0x33U : 0x55U),
            "task completion published its GSU RAM before release");
        map.release_native_presentation();
        require(map.read_native_byte(0x7f1234U)==0x22U && map.read_native_byte(0x701234U)==0x44U
            && map.ppu_state().vram[0]==0x56U && map.ppu_state().cgram[0]==0x5678U
            && map.native_model_draw().shape==456U,
            "presentation release did not expose completed native state");
        map.hold_native_presentation();
        require(map.read_native_byte(0x7f1234U)==0x22U,"snapshot storage was not refreshed for the next frame");
        map.release_native_presentation();

        Wdc65816 cpu{rom};
        auto clock=std::make_shared<SnesCpuTimeline>();
        cpu.set_cpu_timeline(clock); cpu.set_gsu_timing(true);
        cpu.write8(0x701234U,0x33U); cpu.write8(0x303aU,8U); cpu.write16(0x301eU,0x8000U);
        cpu.write8(0x1000U,0x5aU);
        auto snapshot=std::make_unique<NativePresentationSnapshot>();
        cpu.capture_presentation(*snapshot);
        require(snapshot->read_ram(0x701234U)==0x33U && cpu.read8(0x701234U)==0x5aU
            && clock->raster().elapsed()==0U,
            "presentation capture touched the CPU bus or obeyed GSU read ownership");
        require(!snapshot->read_ram(0x3031U),"presentation RAM lookup consumed live I/O");
    }
    std::cout << "held WRAM/GSU RAM, raster/model publication, aliases and side-effect-free capture passed\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
