#include "starfox/state/archive.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/state/container.hpp"
#include "starfox/state/files.hpp"
#include "starfox/assets/bps.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/simulation/dust_system.hpp"
#include "starfox/simulation/particle_system.hpp"
#include "starfox/simulation/wdc65816.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/render/environment_effects.hpp"
#include <iostream>
#include <cstdlib>
#include <chrono>

namespace {
void require(bool ok,const char* message) {
    if (!ok) { std::cerr<<message<<'\n'; std::exit(1); }
}
template<class F> void rejects(F&& action) {
    bool rejected{};
    try { action(); } catch(const std::exception&) { rejected=true; }
    require(rejected,"invalid state was accepted");
}
}

int main(int argc,char** argv) {
    using namespace starfox;
    {
        std::vector<std::uint8_t> ram(1024*1024);
        for(std::size_t i=0;i<ram.size();++i) ram[i]=std::uint8_t(i*37+i/251);
        state::Writer scalar,bulk,vector;
        for(const auto byte:ram) scalar(byte);
        bulk.fixed(std::span<const std::uint8_t>{ram});vector(ram);
        require(bulk.bytes()==scalar.bytes(),"Bulk RAM writer changed encoded bytes");
        require(vector.bytes().size()==ram.size()+4
            && std::equal(ram.begin(),ram.end(),vector.bytes().begin()+4),"Bulk vector encoding changed");
        std::vector<std::uint8_t> restored(ram.size()),restored_vector;
        state::Reader fixed_reader{bulk.bytes()},vector_reader{vector.bytes()};
        fixed_reader.fixed(std::span<std::uint8_t>{restored});fixed_reader.finish();
        vector_reader(restored_vector);vector_reader.finish();
        require(restored==ram && restored_vector==ram,"Bulk RAM round trip failed");
        std::array<std::uint8_t,3> sentinel{9,8,7};
        rejects([&] {state::Reader short_reader{std::span<const std::uint8_t>{ram.data(),2}};
            short_reader.fixed(std::span{sentinel});});
        require(sentinel==std::array<std::uint8_t,3>{9,8,7},"Truncated bulk RAM changed output");
        const std::array<std::uint8_t,5> short_vector{2,0,0,0,1};
        auto stable=restored_vector;
        rejects([&] {state::Reader{short_vector}(restored_vector);});
        require(restored_vector==stable,"Truncated bulk vector changed output");
        if(argc==2 && std::string_view(argv[1])=="--archive-benchmark") {
            std::uint64_t checksum=0;
            const auto measure=[&](auto operation) {
                std::array<double,9> samples{};
                for(unsigned batch=0;batch<11;++batch) {
                    const auto start=std::chrono::steady_clock::now();
                    for(unsigned repeat=0;repeat<32;++repeat) checksum+=operation();
                    const auto duration=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/32;
                    if(batch>=2) samples[batch-2]=duration;
                }
                std::sort(samples.begin(),samples.end());return samples[4];
            };
            const auto scalar_write=measure([&] {state::Writer output;for(auto byte:ram) output(byte);return output.bytes().size()+output.bytes().back();});
            const auto bulk_write=measure([&] {state::Writer output;output.fixed(std::span<const std::uint8_t>{ram});return output.bytes().size()+output.bytes().back();});
            const auto scalar_read=measure([&] {state::Reader input{ram};for(auto& byte:restored) input(byte);input.finish();return restored.size()+restored.back();});
            const auto bulk_read=measure([&] {state::Reader input{ram};input.fixed(std::span<std::uint8_t>{restored});input.finish();return restored.size()+restored.back();});
            std::cout<<"1 MiB archive median microseconds (9 x 32, 2 warmups): write scalar="<<scalar_write
                <<" bulk="<<bulk_write<<"; read scalar="<<scalar_read<<" bulk="<<bulk_read<<"; checksum="<<checksum<<'\n';
            return 0;
        }
    }
    state::Writer writer;
    writer(std::int16_t{-2},std::uint32_t{0x12345678},true,
        std::vector<std::uint8_t>{9,8},std::optional<std::uint8_t>{7});
    require(writer.bytes()==std::vector<std::uint8_t>{254,255,120,86,52,18,1,2,0,0,0,9,8,1,7},
        "archive is not canonical little endian");
    state::Reader reader{writer.bytes()};
    std::int16_t signed_value{}; std::uint32_t word{}; bool flag{};
    std::vector<std::uint8_t> sequence; std::optional<std::uint8_t> optional;
    reader(signed_value,word,flag,sequence,optional); reader.finish();
    require(signed_value==-2 && word==0x12345678 && flag && sequence.size()==2 && optional==7,
        "archive round trip failed");
    rejects([] { const std::array<std::uint8_t,1> bytes{2}; bool value{}; state::Reader{bytes}(value); });
    rejects([] { const std::array<std::uint8_t,4> bytes{255,255,255,255};
        std::vector<std::uint8_t> value; state::Reader{bytes}(value); });
    rejects([&] { state::Reader{writer.bytes()}.finish(); });
    {
        const std::unordered_map<std::uint32_t,std::uint16_t> first{{42,5},{7,8},{99,3}};
        const std::unordered_map<std::uint32_t,std::uint16_t> second{{99,3},{7,8},{42,5}};
        state::Writer one,two; one(first); two(second);
        require(one.bytes()==two.bytes(),"map encoding depends on hash insertion order");
        std::unordered_map<std::uint32_t,std::uint16_t> decoded;
        state::Reader map_reader{one.bytes()}; map_reader(decoded); map_reader.finish();
        require(decoded==first,"archive map round trip failed");
    }
    const auto envelope=state::pack(3,0xabcdef,writer.bytes());
    {
        const auto directory = std::filesystem::temp_directory_path() /
            ("sfe-slot-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(std::filesystem::create_directory(directory), "Could not reserve slot test directory");
        const auto slot = directory / "slot-00.sfe";
        state::write_atomic(slot, envelope);
        require(state::read_file(slot) == envelope, "First slot write failed");
        const auto replacement = state::pack(3, 0xabcdef, std::array<std::uint8_t,3>{3,2,1});
        state::write_atomic(slot, replacement);
        require(state::read_file(slot) == replacement, "Atomic slot replacement failed");
        rejects([&] { state::write_atomic(slot, {}); });
        require(state::read_file(slot) == replacement, "Failed save destroyed previous slot");
        const auto blocked = directory / "blocked";
        std::filesystem::create_directory(blocked);
        state::write_atomic(blocked / "sentinel", envelope);
        rejects([&] { state::write_atomic(blocked, replacement); });
        require(state::read_file(blocked / "sentinel") == envelope,
            "Failed replacement altered destination directory");
        rejects([&] { static_cast<void>(state::read_file(directory / "missing")); });
        std::size_t entries{};
        for (const auto& unused : std::filesystem::directory_iterator(directory)) {
            static_cast<void>(unused); ++entries;
        }
        require(entries == 2, "Save operation leaked temporary files");
        std::filesystem::remove(blocked / "sentinel");
        std::filesystem::remove(blocked);
        std::filesystem::remove(slot);
        std::filesystem::remove(directory);
    }
    const auto unpacked=state::unpack(envelope,3,0xabcdef);
    require(std::equal(unpacked.begin(),unpacked.end(),writer.bytes().begin(),writer.bytes().end()),
        "state container changed payload");
    rejects([&] { static_cast<void>(state::unpack(envelope,4,0xabcdef)); });
    rejects([&] { static_cast<void>(state::unpack(envelope,3,0xabcdee)); });
    for(std::size_t i=0;i<envelope.size();++i) {
        auto corrupt=envelope; corrupt[i]^=1;
        rejects([&] { static_cast<void>(state::unpack(corrupt,3,0xabcdef)); });
        rejects([&] { static_cast<void>(state::unpack(std::span{envelope}.first(i),3,0xabcdef)); });
    }

    for(const auto layout:{simulation::ObjectMemoryLayout::original,simulation::ObjectMemoryLayout::starfox_ex}) {
        simulation::ObjectPool pool{layout==simulation::ObjectMemoryLayout::original ? 70U : 80U,layout};
        const auto first=pool.allocate_after(); const auto second=pool.allocate_after(first);
        const auto third=pool.allocate_after(first);
        require(pool.remove(second),"pool fixture removal failed");
        auto& object=pool.at(first);
        object.world_x=-32768; object.world_y=32767; object.world_z=-77;
        object.attached=third; object.fire_object=third; object.immune_object=third;
        object.collision_object=third; object.shape=123; object.strategy_address=0x128abc;
        object.scratch_bytes={-1,2,-3,4,-5,6}; object.scratch_words={-300,700};
        object.extended.fill(173); object.strategy_flags={7,11,23,48};
        object.velocity_x=-30000; object.velocity_y=456; object.velocity_z=1234;
        object.weapon_type=4; object.colour_table=0xabcd; object.rotation_x=217;
        // The source map can store a raw initializer word here before a
        // strategy resolves it; state loading must not reject that phase.
        pool.at(third).attached=0xabcd;
        const auto saved=pool.save_state(); const auto expected=pool;
        pool.reset(); static_cast<void>(pool.allocate_after());
        pool.load_state(saved);
        require(pool.save_state()==saved && pool.at(first)==expected.at(first)
            && pool.at(third)==expected.at(third)
            && pool.active_handles()==expected.active_handles() && pool.free_handles()==expected.free_handles()
            && pool.generation(first)==expected.generation(first),"pool restore changed object identity/order");
        auto continuation=expected;
        const auto allocated=pool.allocate_after(first), expected_allocated=continuation.allocate_after(first);
        require(allocated==expected_allocated && pool.generation(allocated)==continuation.generation(expected_allocated),
            "restored pool changed next allocation or generation");
        const auto current=pool.save_state();
        for(const auto length:{std::size_t{0},saved.size()/2,saved.size()-1}) {
            rejects([&] { pool.load_state(std::span{saved}.first(length)); });
            require(pool.save_state()==current,"failed pool load partially changed live state");
        }
        auto corrupt=saved; corrupt[11]=0; corrupt[12]=0; // First active handle is null.
        rejects([&] { pool.load_state(corrupt); });
        require(pool.save_state()==current,"invalid pool list mutated live state");
    }

    const simulation::MatrixQ15 identity{32767,0,0,0,32767,0,0,0,32767};
    simulation::DustSystem dust;
    dust.tick({200,-300,1000},identity,true);
    const auto dust_saved=dust.save_state();
    auto expected_dust=dust;
    dust.tick({30000,-30000,30000},identity,true);
    dust.load_state(dust_saved);
    require(dust.points()==expected_dust.points(),"dust points did not restore");
    for(int frame=0;frame<120;++frame) {
        const std::array<std::int16_t,3> camera{static_cast<std::int16_t>(frame*200),-300,static_cast<std::int16_t>(frame*250)};
        dust.tick(camera,identity,true); expected_dust.tick(camera,identity,true);
        require(dust.save_state()==expected_dust.save_state(),"dust random continuation diverged");
    }
    const auto dust_before_bad=dust.save_state();
    auto bad_dust=dust_saved; bad_dust[6]=2;
    rejects([&] { dust.load_state(bad_dust); });
    require(dust.save_state()==dust_before_bad,"failed dust load mutated state");

    assets::RomImage rom{std::vector<std::uint8_t>(32768,1)};
    simulation::ParticleSystem particles{rom,0x8000,0x8100};
    simulation::ObjectPool owners;
    const auto owner=owners.allocate_after();
    owners.at(owner).strategy_flags[0]=0x10;
    owners.at(owner).scratch_bytes[0]=12;
    owners.at(owner).scratch_bytes[1]=40;
    owners.at(owner).scratch_bytes[2]=1;
    particles.tick(owners,true);
    require(particles.active_count()>0,"particle restore fixture has no live particles");
    const auto particle_saved=particles.save_state(); auto expected_particles=particles;
    particles.reset(); particles.load_state(particle_saved);
    require(particles.particles()==expected_particles.particles(),"particles did not restore");
    for(int frame=0;frame<60;++frame) {
        particles.tick(owners,true); expected_particles.tick(owners,true);
        require(particles.save_state()==expected_particles.save_state(),"particle continuation diverged");
    }
    const auto particle_before_bad=particles.save_state();
    rejects([&] { particles.load_state(std::span{particle_saved}.first(particle_saved.size()-1)); });
    require(particles.save_state()==particle_before_bad,"failed particle load mutated state");
    {
        simulation::Wdc65816 cpu{rom};
        // REP #$30; LDA #$1234; STA $7e2000; INX; INC $2000; RTL.
        constexpr std::array<std::uint8_t,14> program{
            0xc2,0x30,0xa9,0x34,0x12,0x8f,0x00,0x20,0x7e,0xe8,0xee,0x00,0x20,0x6b};
        for(std::size_t i=0;i<program.size();++i) cpu.write8(0x7e1000U+static_cast<std::uint32_t>(i),program[i]);
        cpu.write8(0x700abc,83); cpu.write8(0x71f010,97);
        cpu.write8(0x002121,5); cpu.write8(0x002122,0x34); // Half a CGRAM word.
        cpu.write8(0x002115,0x80); // Increment VRAM address after the high byte.
        cpu.write8(0x002116,0x23); cpu.write8(0x002117,0x01);
        cpu.write8(0x002118,0x56); // Low VRAM byte, high still pending.
        cpu.write8(0x002181,0x45); cpu.write8(0x002182,0x23); cpu.write8(0x002183,1);
        cpu.set_apu_output_ports({1,2,3,4});
        cpu.set_apu_clock_offset(172);
        cpu.write8(0x002142,55); cpu.write8(0x002004,12); cpu.write8(0x002007,3);
        simulation::Wdc65816Registers regs;
        regs.x=5; regs.data_bank=0x7e;
        constexpr std::array<std::uint32_t,1> stop{0x7e1009};
        const auto paused=cpu.begin_long_task(0x7e1000,regs,stop);
        require(!paused.returned && paused.stop_address==stop[0],"CPU save fixture did not suspend");
        const auto cpu_saved=cpu.save_state();
        const auto complete=[&](simulation::Wdc65816& machine) {
            machine.write8(0x002122,0x12);
            machine.write8(0x002119,0x78);
            machine.write8(0x002180,0x9a);
            simulation::Wdc65816Registers output;
            const auto resumed=machine.resume_task(output,{});
            require(resumed.returned && output.a==0x1234 && output.x==6,
                "saved CPU task did not resume with its registers");
            require(machine.read16(0x7e2000)==0x1235
                && machine.read8(0x700abc)==83 && machine.read8(0x71f010)==97
                && machine.read8(0x7f2345)==0x9a && machine.ppu_state().cgram[5]==0x1234,
                "CPU memory or half-written PPU latch was not restored");
            require(machine.ppu_state().vram[0x246]==0x56 && machine.ppu_state().vram[0x247]==0x78,
                "VRAM address/increment latch was not restored");
            return machine.save_state();
        };
        const auto expected_cpu=complete(cpu);
        cpu.load_state(cpu_saved);
        require(cpu.save_state()==cpu_saved,"CPU snapshot changed immediately after restore");
        require(complete(cpu)==expected_cpu,"same-instance CPU continuation diverged");
        simulation::Wdc65816 fresh{rom};
        fresh.load_state(cpu_saved);
        require(complete(fresh)==expected_cpu,"fresh CPU continuation diverged or bus mappings were stale");
        const auto saved_apu=cpu.take_apu_port_writes();
        const auto saved_msu=cpu.take_msu_register_writes();
        require(!saved_apu.empty() && !saved_msu.empty()
            && saved_apu==fresh.take_apu_port_writes() && saved_msu==fresh.take_msu_register_writes(),
            "restored audio command queues diverged");
        const auto before_bad=cpu.save_state();
        auto corrupt=cpu_saved; corrupt[200]^=1;
        rejects([&] {cpu.load_state(corrupt);});
        require(cpu.save_state()==before_bad,"corrupt CPU load mutated the live machine");
        const auto body=state::unpack(cpu_saved,0x43505501U,assets::crc32(rom.bytes()));
        const auto short_body=state::pack(0x43505501U,assets::crc32(rom.bytes()),body.first(body.size()-1));
        rejects([&] {cpu.load_state(short_body);});
        require(cpu.save_state()==before_bad,"failed CPU decode partially committed");
        assets::RomImage another_rom{std::vector<std::uint8_t>(32768,2)};
        simulation::Wdc65816 wrong_cartridge{another_rom};
        rejects([&] {wrong_cartridge.load_state(cpu_saved);});
    }
    {
        auto bytes=std::vector<std::uint8_t>(32768,0);
        constexpr std::array<std::uint8_t,5> caller{40,0x10,0,0,2};
        constexpr std::array<std::uint8_t,9> callee{18,2,0,4,0x10,0,3,0,42};
        std::copy(caller.begin(),caller.end(),bytes.begin());
        std::copy(callee.begin(),callee.end(),bytes.begin()+16);
        assets::RomImage map_rom{std::move(bytes)};
        simulation::ObjectPool a,b;
        const simulation::MapDatabase database{map_rom,0x8100,0x8200};
        simulation::MapVm original{map_rom,database,a}, restored{map_rom,database,b};
        original.start(0x8000,0);
        original.advance_distance(1); original.advance_distance(3);
        const auto saved=original.save_state();
        restored.load_state(saved);
        require(restored.save_state()==saved,"suspended map call/loop did not restore");
        for(unsigned frame=0;frame<12;++frame) {
            original.advance_distance(1); restored.advance_distance(1);
            for(auto* machine:{&original,&restored}) {
                machine->write_native_byte(0x7e6000,0x6b);
                simulation::Wdc65816Registers output;
                static_cast<void>(machine->call_native_routine(0x7e6000,output));
            }
            require(original.save_state()==restored.save_state(),"restored map loop/native mirror diverged");
        }
        require(original.ended() && restored.ended(),"restored map lost its return stack");
    }
    if(argc>=3) {
        const auto cartridge=assets::RomImage::load(argv[1]);
        const auto symbols=assets::SymbolMap::load(argv[2]);
        simulation::GameSimulation game{cartridge,symbols,"LEVEL1_1",{},true};
        game.set_god_mode(true);
        for(unsigned tick=0;tick<600;++tick) static_cast<void>(game.tick({}));
        const auto full_saved = game.save_state();
        {
            // Exercise real cartridge archives, not just effect enum helpers.
            // New IDs are appended; existing state layouts must stay readable.
            const auto crc=assets::crc32(cartridge.bytes());
            auto styled=game.restored_state(full_saved);
            for(bool world:{false,true}) for(auto effect:render::effect_order) {
                const auto id=static_cast<std::uint8_t>(effect);
                if(!render::selectable_effect(id,world)) continue;
                styled->set_effect(world?0:id);
                styled->set_world_effect(world?id:0);
                const auto saved=styled->save_state();
                auto restored=game.restored_state(saved);
                require(restored->effect()==(world?0:id)
                    && restored->world_effect()==(world?id:0)
                    && restored->save_state()==saved,"selectable style state changed during restore");
            }
            for(bool world:{false,true}) {
                const auto set=[&](render::Effect effect) {
                    if(world) styled->set_world_effect(static_cast<std::uint8_t>(effect));
                    else styled->set_effect(static_cast<std::uint8_t>(effect));
                };
                set(render::Effect::off);const auto off=styled->save_state();
                set(render::Effect::cyanotype);const auto cyan=styled->save_state();
                const auto before=state::unpack(off,0x47414d01U,crc);
                const auto after=state::unpack(cyan,0x47414d01U,crc);
                require(before.size()==after.size(),"style selection changed archive layout");
                std::size_t changed=0,offset=0;
                for(std::size_t i=0;i<before.size();++i) if(before[i]!=after[i]) {++changed;offset=i;}
                require(changed==1,"could not isolate serialized effect byte");
                auto payload=std::vector<std::uint8_t>(after.begin(),after.end());
                payload[offset]=static_cast<std::uint8_t>(render::Effect::ice);
                auto migrated=game.restored_state(state::pack(0x47414d01U,crc,payload));
                require((world?migrated->world_effect():migrated->effect())==static_cast<std::uint8_t>(render::Effect::cyanotype),
                    "legacy Ice archive did not migrate to Cyanotype");
                require(migrated->save_state()==cyan,"legacy style migration altered unrelated state");
                payload[offset]=static_cast<std::uint8_t>(render::Effect::crosshatch);
                auto removed=game.restored_state(state::pack(0x47414d01U,crc,payload));
                require(removed->save_state()==off,"removed Crosshatch state did not migrate cleanly to Off");
                for(auto invalid:{std::uint8_t{255},render::effect_count,
                        static_cast<std::uint8_t>(world?render::Effect::gold_metal:render::Effect::blueprint)}) {
                    payload[offset]=invalid;
                    rejects([&] {static_cast<void>(game.restored_state(state::pack(0x47414d01U,crc,payload)));});
                }
                if(world) for(auto temporal:{render::Effect::trails,render::Effect::long_exposure}) {
                    payload[offset]=static_cast<std::uint8_t>(temporal);
                    rejects([&] {static_cast<void>(game.restored_state(state::pack(0x47414d01U,crc,payload)));});
                }
            }
            require(game.save_state()==full_saved,"style archive tests mutated the running game");
        }
        {
            // New optional cheat fields must not invalidate pre-extension
            // archives. Repack genuine payloads so checksum failures cannot
            // accidentally stand in for schema/boolean validation here.
            const auto crc=assets::crc32(cartridge.bytes());
            const auto payload=state::unpack(full_saved,0x47414d01U,crc);
            for(unsigned tail=0;tail<2;++tail) {
                auto legacy=std::vector<std::uint8_t>(payload.begin(),payload.end()-13+tail);
                if(tail) legacy.back()=1;
                auto migrated=game.restored_state(state::pack(0x47414d01U,crc,legacy));
                require(migrated->god_mode(),"Legacy state lost active God Mode");
                require(migrated->infinite_lives()==bool(tail),"Optional lives state migration failed");
            }
            auto cheat=game.restored_state(full_saved);
            cheat->set_effect(1);
            cheat->set_manipulation(static_cast<std::uint8_t>(render::Effect::checker_fold));
            cheat->set_manipulation_intensity(60);
            cheat->set_material(static_cast<std::uint8_t>(render::Effect::pearl));
            cheat->set_infinite_lives(true);
            auto restored_cheat=game.restored_state(cheat->save_state());
            require(restored_cheat->effect()==1 && restored_cheat->manipulation()==static_cast<std::uint8_t>(render::Effect::checker_fold)
                && restored_cheat->manipulation_intensity()==60 && restored_cheat->material()==static_cast<std::uint8_t>(render::Effect::pearl),"independent manipulation/material state lost");
            require(restored_cheat->infinite_lives() && restored_cheat->god_mode(),
                "Cheat state roundtrip lost enabled settings");
            for(unsigned index=0;index<2;++index) {
                auto invalid=std::vector<std::uint8_t>(payload.begin(),payload.end());
                invalid[invalid.size()-13+index]=2;
                rejects([&] {static_cast<void>(game.restored_state(state::pack(0x47414d01U,crc,invalid)));});
            }
            auto trailing=std::vector<std::uint8_t>(payload.begin(),payload.end());
            trailing.push_back(0);
            rejects([&] {static_cast<void>(game.restored_state(state::pack(0x47414d01U,crc,trailing)));});
            require(game.save_state()==full_saved,"Cheat archive validation mutated live state");
            auto menu=game.restored_state(full_saved);
            require(menu->toggle_runtime_options(),"Could not open saved cheats fixture");
            const auto press=[&](input::ButtonMask button) {
                static_cast<void>(menu->tick({}));
                static_cast<void>(menu->tick({button,button,0}));
            };
            press(input::a); // Main OPTIONS selection.
            press(input::a); // OPTIONS > CHEATS.
            for(unsigned row=0;row<5;++row) press(input::down);
            require(menu->pregame_page()==simulation::PregamePage::cheats
                && menu->pregame_selection()==5,"Legacy Back fixture did not reach row five");
            const auto menu_saved=menu->save_state();
            const auto menu_payload=state::unpack(menu_saved,0x47414d01U,crc);
            auto migrated_menu=game.restored_state(state::pack(0x47414d01U,crc,
                menu_payload.first(menu_payload.size()-13)));
            require(migrated_menu->pregame_selection()==6 && !migrated_menu->infinite_lives(),
                "Legacy Cheats Back became Infinite Lives after restore");
        }
        {
            auto environment_game=game.restored_state(full_saved);
            const auto crc=assets::crc32(cartridge.bytes());
            environment_game->set_environment({});
            const auto base=environment_game->save_state();
            const auto raw=state::unpack(base,0x47414d01U,crc);
            std::size_t first_environment=raw.size();
            for(unsigned field=0;field<render::environment_limits.size();++field) {
                std::array<std::uint8_t,6> options{};options[field]=1;
                environment_game->set_environment(options);
                const auto marked=environment_game->save_state();
                const auto marked_raw=state::unpack(marked,0x47414d01U,crc);
                require(raw.size()==marked_raw.size(),"environment archive layout changed size");
                std::size_t offset=raw.size();unsigned differences=0;
                for(std::size_t i=0;i<raw.size();++i) if(raw[i]!=marked_raw[i]) {offset=i;++differences;}
                require(differences==1,"environment option did not isolate one archive byte");
                first_environment=std::min(first_environment,offset);
                for(unsigned value=0;value<render::environment_limits[field];++value) {
                    options[field]=std::uint8_t(value);environment_game->set_environment(options);
                    const auto saved=environment_game->save_state();
                    const auto restored=game.restored_state(saved);
                    require(restored->environment()==options && restored->save_state()==saved,
                        "environment selection changed during state roundtrip");
                }
                for(unsigned invalid:{render::environment_limits[field],255u}) {
                    std::vector<std::uint8_t> damaged(raw.begin(),raw.end());
                    damaged[offset]=std::uint8_t(invalid);
                    rejects([&]{static_cast<void>(game.restored_state(state::pack(0x47414d01U,crc,damaged)));});
                }
            }
            const std::array<std::uint8_t,6> combined{1,7,3,1,3,2};
            environment_game->set_environment(combined);
            const auto saved=environment_game->save_state();
            require(game.restored_state(saved)->environment()==combined,"combined environment settings lost");
            const auto legacy=game.restored_state(state::pack(0x47414d01U,crc,raw.first(first_environment)));
            require(legacy->environment()==std::array<std::uint8_t,6>{},"legacy state enabled environment enhancements");
            for(std::size_t count=1;count<6;++count)
                rejects([&]{static_cast<void>(game.restored_state(state::pack(0x47414d01U,crc,raw.first(first_environment+count))));});
            require(game.save_state()==full_saved,"environment archive validation mutated live state");
        }
        game.set_stereo_output(2);
        game.set_reflective_surfaces(3);
        game.set_dlss_mode(1);
        game.set_fsr1_mode(4);
        game.set_fsr1_menu(true);
        auto full_restore = game.restored_state(full_saved);
        require(full_restore->stereo_output()==2,"state restore reset current stereo output");
        require(full_restore->reflective_surfaces_setting()==3,"state restore reset reflection preference");
        require(full_restore->dlss_mode()==1 && full_restore->fsr1_mode()==4 && full_restore->fsr1_menu(),
            "state restore changed independent host upscaler preferences");
        require(game.save_state()==full_saved,"stereo output changed cartridge save-state bytes");
        require(full_restore->save_state() == full_saved, "Full game restore changed initial state");
        for (unsigned tick = 0; tick < 90; ++tick) {
            const input::TickInput controls{static_cast<input::ButtonMask>(tick % 2 ? 0x100 : 0),0,0};
            const auto expected = game.tick(controls);
            const auto actual = full_restore->tick(controls);
            require(expected.audio_port_writes == actual.audio_port_writes,
                "Full restored game audio commands diverged");
            for (unsigned phase = 0; phase < 3; ++phase) {
                game.present_frame(); full_restore->present_frame();
            }
            require(game.save_state() == full_restore->save_state(),
                "Full restored gameplay continuation diverged");
        }
        const auto live_before_bad = game.save_state();
        auto bad_game = full_saved; bad_game.back() ^= 1U;
        rejects([&] { static_cast<void>(game.restored_state(bad_game)); });
        require(game.save_state() == live_before_bad, "Bad whole-game restore mutated live game");
        auto rewind = game.restored_state(full_saved);
        game.swap_state(*rewind);
        require(game.save_state() == full_saved && rewind->save_state() == live_before_bad,
            "Game restore commit did not swap complete state");
        rewind.reset(); // Destroy old storage before exercising rebound pointers.
        auto reference = game.restored_state(full_saved);
        for (unsigned tick = 0; tick < 12; ++tick) {
            static_cast<void>(game.tick({})); static_cast<void>(reference->tick({}));
            require(game.save_state() == reference->save_state(), "Committed restore has stale sibling references");
        }
        auto restored_objects=game.objects();
        restored_objects.load_state(game.objects().save_state());
        simulation::MapVm restored{cartridge,simulation::MapDatabase{cartridge,symbols},restored_objects,&symbols};
        const auto saved_map=game.map().save_state();
        restored.load_state(saved_map);
        require(restored.save_state()==saved_map,"real cartridge map/CPU state failed its round trip");
        for(unsigned tick=0;tick<20;++tick) {
            game.map().advance_distance(25); restored.advance_distance(25);
            game.map().tick_video_phase(); restored.tick_video_phase();
            require(game.map().save_state()==restored.save_state()
                && game.objects().save_state()==restored_objects.save_state(),
                "restored cartridge map continuation diverged");
        }
        const auto before_bad=restored.save_state();
        auto bad=saved_map; bad[50]^=1;
        rejects([&] {restored.load_state(bad);});
        require(restored.save_state()==before_bad,"failed map load changed live state");
        if(!symbols.find("LEVEL4_4").empty()) {
            simulation::GameSimulation transition{cartridge,symbols,"LEVEL4_4",{},true};
            transition.set_timing_mode(simulation::TimingMode::unlocked_20_fps);
            transition.set_god_mode(true);transition.set_msu1_available(false);transition.set_msu1_music(false);
            audio::Spc700Audio sound;
            for(unsigned tick=0;tick<340;++tick) {
                if(tick==186 || tick==327) {
                    const auto background_before=transition.map().background();
                    auto resumed=transition.restored_state(transition.save_state());
                    audio::Spc700Audio resumed_sound;resumed_sound.load_state(sound.save_state());
                    for(unsigned step=0;step<12;++step,++tick) {
                        const auto expected=transition.tick({}),actual=resumed->tick({});
                        require(sound.render_logic_tick(expected.audio_port_writes)
                            ==resumed_sound.render_logic_tick(actual.audio_port_writes),
                            "Restored background transition audio diverged");
                        transition.synchronize_apu_output_ports(sound.output_ports());
                        resumed->synchronize_apu_output_ports(resumed_sound.output_ports());
                        static_cast<void>(transition.map().take_msu_register_writes());
                        static_cast<void>(resumed->map().take_msu_register_writes());
                        require(transition.save_state()==resumed->save_state()
                            && sound.save_state()==resumed_sound.save_state(),
                            "Restored background transition state diverged");
                        if(step==0) require(transition.map().background()!=background_before,
                            "Background restore fixture missed its transition boundary");
                    }
                }
                const auto update=transition.tick({});
                static_cast<void>(sound.render_logic_tick(update.audio_port_writes));
                transition.synchronize_apu_output_ports(sound.output_ports());
                static_cast<void>(transition.map().take_msu_register_writes());
            }
        }
        for (const auto* entry : {"INTROMAP", "CONTINUE", "PLANETSELECT"}) {
            simulation::GameSimulation scene{cartridge, symbols, entry};
            audio::Spc700Audio sound;
            (void)sound.prime_upload_sequence(scene.map().take_apu_port_writes());
            scene.synchronize_apu_output_ports(sound.output_ports());
            for (unsigned tick = 0; tick < 20; ++tick) {
                const auto update = scene.tick({});
                (void)sound.render_logic_tick(update.audio_port_writes);
                scene.synchronize_apu_output_ports(sound.output_ports());
            }
            auto resumed = scene.restored_state(scene.save_state());
            audio::Spc700Audio resumed_sound;
            resumed_sound.load_state(sound.save_state());
            for (unsigned tick = 0; tick < 24; ++tick) {
                const auto expected = scene.tick({});
                const auto actual = resumed->tick({});
                require(sound.render_logic_tick(expected.audio_port_writes)
                    == resumed_sound.render_logic_tick(actual.audio_port_writes),
                    "Restored frontend audio differs");
                scene.synchronize_apu_output_ports(sound.output_ports());
                resumed->synchronize_apu_output_ports(resumed_sound.output_ports());
                scene.present_frame(); resumed->present_frame();
                require(scene.save_state() == resumed->save_state(),
                    "Restored frontend game/audio feedback differs");
            }
        }
    }
    std::cout<<"State archive, component and suspended CPU/PPU restore tests passed.\n";
}
