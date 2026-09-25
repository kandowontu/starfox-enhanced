#include "starfox/vr/game_input.hpp"
#include "starfox/vr/game_frame_driver.hpp"
#include "starfox/vr/source_models.hpp"
#include "starfox/vr/vulkan_compute_ray_scene.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/render/grid_projection.hpp"
#include "starfox/render/grid_line_sample.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/vr/source_sprites.hpp"
#include "starfox/vr/pause_sandbox.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) try {
    {
        using namespace starfox::input;
        starfox::vr::MenuStick stick;
        if(stick.sample(.9F,.55F)!=right || stick.sample(.55F,.9F)!=right)
            throw std::runtime_error("Menu gesture switched axis while held");
        if(stick.sample(.2F,.2F)!=0 || stick.sample(.55F,.9F)!=up)
            throw std::runtime_error("Neutral did not release menu axis lock");
        stick.reset();
        if(stick.sample(.8F,.8F)!=0 || stick.sample(-.9F,.3F)!=left)
            throw std::runtime_error("Ambiguous diagonal selected a menu action");
        starfox::vr::VrControls controls;controls.steer={.9F,.55F};
        starfox::vr::VrGameInput menu,flight;
        menu.sample(controls,true);flight.sample(controls);
        if(menu.consume().held!=right || flight.consume().held!=(right|up))
            throw std::runtime_error("Menu cardinal filtering affected flight diagonals");
        menu.sample(controls,true);
        if(menu.consume().pressed) throw std::runtime_error("Held menu stick generated another press");
        menu.reset();controls.steer={0,0};menu.sample(controls,true);(void)menu.consume();
        controls.steer={0,-.8F};menu.sample(controls,true);
        if(menu.consume().pressed!=down) throw std::runtime_error("Menu input failed after reset");
        std::cout<<"VR menu cardinal gestures and unchanged flight diagonals passed\n";
    }
    if(argc!=3) throw std::runtime_error("Usage: starfox_vr_game_input_check ROM SYMBOLS");
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    {
        starfox::simulation::GameSimulation selected(rom,symbols,"INTROMAP",{},true);
        const auto initial=selected.save_state();
        if(selected.launch_selected_level() || selected.save_state()!=initial)
            throw std::runtime_error("OFF level selection changed the running game");
        const auto route=symbols.find("WHICHROUTE").at(0);
        const auto stage=symbols.find("STAGE").at(0);
        unsigned launches=0;
        for(const auto level:selected.selectable_levels()) if(level) {
            selected.set_selected_level(level);
            try {
                if(selected.selected_level()!=level || !selected.launch_selected_level()
                    || selected.flow_state()!=starfox::simulation::GameFlowState::gameplay)
                    throw std::runtime_error("Shared selected-level launch failed");
                if(selected.map().read_native_byte(route)!=level/10-1
                    || selected.map().read_native_word(stage)!=level%10-1)
                    throw std::runtime_error("Selected-level launch did not initialize route/stage");
            } catch(const std::exception& error) {
                throw std::runtime_error(selected.selected_level_name()+": "+error.what());
            }
            ++launches;
        }
        std::cout<<"Selected-level route initialization: "<<launches<<" stages passed\n";
        starfox::vr::GameSceneHistory coloured(selected,rom,symbols);
        for(unsigned colour=0;colour<8;++colour) {
            selected.set_crosshair_colour(static_cast<starfox::simulation::CrosshairColour>(colour));
            const auto source=selected.save_state();
            coloured.capture();
            const auto frame=coloured.current();
            if(selected.save_state()!=source || frame->cgram!=frame->ppu->cgram)
                throw std::runtime_error("Crosshair presentation changed native state or palette views diverged");
            if(colour==1 && frame->cgram[207]!=0x7fff)
                throw std::runtime_error("White VR crosshair palette not applied");
            if(colour==0 && frame->meters.extended && frame->cgram[207]!=0x03e0)
                throw std::runtime_error("EX green model reticle palette not applied");
        }
    }
    {
        starfox::simulation::GameSimulation menu(rom,symbols,"LEVEL1_1",{},true);
        const auto flow=menu.flow_state();const auto experience=menu.experience();
        if(!menu.toggle_runtime_options() || !menu.in_setup_menu() || !menu.runtime_options_open())
            throw std::runtime_error("Runtime options failed to open");
        (void)menu.tick({}); // Drain already queued audio without advancing native logic.
        const auto paused_map=menu.map().save_state();
        const auto paused_objects=menu.objects().save_state();
        for(unsigned frame=0;frame<240;++frame) {
            menu.present_frame();
            if(frame%12==0) (void)menu.tick({});
        }
        if(menu.map().save_state()!=paused_map || menu.objects().save_state()!=paused_objects)
            throw std::runtime_error("Runtime options advanced paused native state");
        for(unsigned i=0;i<40 && menu.pregame_selection()!=0;++i) {
            (void)menu.tick({});(void)menu.tick({starfox::input::down,starfox::input::down});
        }
        if(menu.pregame_selection()!=0) throw std::runtime_error("Experience row not reachable");
        (void)menu.tick({});(void)menu.tick({starfox::input::right,starfox::input::right});
        if(menu.experience()!=experience || menu.flow_state()!=flow || menu.preview_start_requested())
            throw std::runtime_error("Runtime options changed locked experience/flow");
        (void)menu.tick({});(void)menu.tick({starfox::input::start,starfox::input::start});
        if(menu.runtime_options_open() || menu.in_setup_menu() || menu.preview_start_requested() || menu.flow_state()!=flow)
            throw std::runtime_error("Runtime options did not resume existing game");
        if(!menu.toggle_runtime_options() || !menu.toggle_runtime_options() || menu.in_setup_menu())
            throw std::runtime_error("Runtime options hotkey toggle failed");
        std::cout<<"Runtime options open/resume and experience lock passed\n";
        unsigned audio_blocks=0;
        starfox::vr::GameFrameDriver menu_driver(menu,[&](auto,auto) {
            ++audio_blocks;return std::array<uint8_t,4>{};
        });
        (void)menu_driver.advance(0,{},true);
        if(!menu.toggle_runtime_options()) throw std::runtime_error("VR driver options setup failed");
        for(unsigned frame=1;frame<=120;++frame)
            (void)menu_driver.advance(1'000'000+frame*16'666'667LL,{},true);
        if(!menu.runtime_options_open() || audio_blocks) throw std::runtime_error("VR paused audio advanced");
        (void)menu_driver.advance(2'100'000'000,{},true);
        (void)menu.toggle_runtime_options();
        (void)menu_driver.advance(2'110'000'000,{},true);
        if(menu.runtime_options_open() || menu.preview_start_requested() || menu.flow_state()!=flow)
            throw std::runtime_error("VR options failed to resume");
        menu_driver.reset_for_scene_change();
        const auto audio_before_reset=audio_blocks;
        const auto rebased=menu_driver.advance(9'000'000'000,{},true);
        if(rebased.logic_ticks || rebased.audio_blocks || rebased.time_clamped || audio_blocks!=audio_before_reset)
            throw std::runtime_error("Scene change replayed old frame time or pending audio");
        std::cout<<"VR frame driver freezes audio emulation during runtime options\n";
    }
    {
        std::array<uint16_t,256> palette{};palette[1]=0x7fff;
        starfox::render::ScaledTextRenderer font(rom,symbols);
        for(const auto text:{U"日本語",U"FRANÇAIS",U"ESPAÑOL",U"SPRACHE",U"START GAME"}) {
            starfox::render::Framebuffer expected(256,224),decoded(256,224);
            font.draw_unicode(text,17,67,expected,1,0);
            const auto packet=starfox::vr::source_unicode_ui_text_packet(rom,symbols,text,17,67,1,palette);
            const auto vertices=packet.geometry.vertex_view();
            if(vertices.empty() || vertices.size()%6) throw std::runtime_error("Missing Unicode menu tiles");
            for(size_t i=0;i<vertices.size();i+=6) {
                const auto& v=vertices[i];const auto offset=v.texture[0];
                if(offset+9>packet.geometry.texels.size() || v.texture[3]!=1024) throw std::runtime_error("Invalid Unicode tile data");
                for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
                    const auto row=packet.geometry.texels[offset+1+y/2]>>((y&1)*16);
                    if(row&(0x8000U>>x)) decoded.set(int(v.position[0])+x,int(v.position[1])+y,1);
                }
            }
            if(!std::equal(expected.pixels().begin(),expected.pixels().end(),decoded.pixels().begin()))
                throw std::runtime_error("Unicode VR tile masks differ from localized font");
        }
        std::cout<<"Five Unicode menu tile masks match the localized font\n";
        const auto punctuation=starfox::vr::source_ui_text_packet(rom,symbols,":/>",0,0,240,1,palette);
        if(punctuation.geometry.vertices.size()!=18 || punctuation.geometry.texels.size()!=27)
            throw std::runtime_error("Missing host punctuation");
        for(unsigned glyph=0;glyph<3;++glyph) for(unsigned row=0;row<16;++row) {
            const unsigned actual=(punctuation.geometry.texels[glyph*9+1+row/2]>>((row&1)*16))&65535;
            const unsigned expected=row>=12?0:glyph==0?(row==3 || row==4 || row==8 || row==9?0x6000:0)
                :glyph==1?(0x8000U>>(3-row*4/12))
                :(row>=2 && row<=9?0x8000U>>(row<=5?row-2:9-row):0);
            if(actual!=expected) throw std::runtime_error("Host punctuation uses a cartridge alias");
        }
    }
    {
        starfox::simulation::Wdc65816 cpu(rom,&symbols);
        cpu.write16(0x7e0100,0x1234);cpu.write16(0x700100,0xabcd);cpu.write16(0x710100,0x5678);
        cpu.write8(0x7effff,0xaa);cpu.write8(0x7f0000,0xbb);
        const auto before=cpu.save_state();
        if(cpu.peek_ram16(0x000100)!=0x1234 || cpu.peek_ram16(0x800100)!=0x1234
            || cpu.peek_ram16(0x700100)!=0xabcd || cpu.peek_ram16(0x710100)!=0x5678
            || cpu.peek_ram16(0x7effff)!=0xbbaa || cpu.peek_ram16(0x001fff)
            || cpu.peek_ram8(0x00213f) || cpu.peek_ram8(0x002140) || cpu.peek_ram8(0x008000)
            || cpu.peek_ram16(0xffffffff) || cpu.peek_ram16(0x71ffff))
            throw std::runtime_error("Side-effect-free RAM address mapping failed");
        if(cpu.save_state()!=before) throw std::runtime_error("RAM peek changed CPU bus or I/O state");
    }
    starfox::simulation::GameSimulation vr_game(rom,symbols,"LEVEL1_1",{},true),pad_game(rom,symbols,"LEVEL1_1",{},true);
    starfox::audio::Spc700Audio vr_audio,pad_audio;
    vr_game.set_timing_mode(starfox::simulation::TimingMode::unlocked_20_fps);
    pad_game.set_timing_mode(starfox::simulation::TimingMode::unlocked_20_fps);
    starfox::vr::VrGameInput vr_input;starfox::input::InputLatch pad_input;
    using namespace starfox::input;
    const auto advance=[](auto& game,auto& audio,const TickInput& input) {
        const auto result=game.tick(input);
        static_cast<void>(audio.render_logic_tick(result.audio_port_writes));
        game.synchronize_apu_output_ports(audio.output_ports());
    };
    {
        starfox::simulation::GameSimulation hole(rom,symbols,"LEVEL_BLACKHOLE",{},true);
        starfox::audio::Spc700Audio audio;
        hole.set_god_mode(true);
        hole.set_timing_mode(starfox::simulation::TimingMode::unlocked_20_fps);
        for(unsigned tick=0;tick<6000;++tick) advance(hole,audio,{});
        starfox::vr::GameSceneHistory history(hole,rom,symbols);
        if(!history.current()->background_star_sphere || !history.current()->background_retain_sky_scroll)
            throw std::runtime_error("Black Hole lost its scrolling surround in the late live video mode");
    }
    {
        starfox::simulation::GameSimulation titania(rom,symbols,"LEVEL2_3",{},true);
        starfox::audio::Spc700Audio audio;
        titania.set_god_mode(true);
        for(unsigned tick=0;tick<200;++tick) advance(titania,audio,{});
        const bool extended=titania.peek_meter_state().extended;
        // The setup choice can differ from the currently loaded cartridge.
        // It must not change that cartridge's environment interpretation.
        titania.set_experience(starfox::simulation::Experience::original);
        starfox::vr::GameSceneHistory history(titania,rom,symbols);
        const bool landscape=history.current()->background_landscape;
        titania.set_experience(starfox::simulation::Experience::starfox_ex);
        history.capture();
        if(history.current()->background_landscape!=landscape || !landscape
            || history.current()->meters.extended!=extended)
            throw std::runtime_error("Titania mapping depends on menu choice instead of cartridge identity");
        std::cout<<"Titania landscape classification follows loaded cartridge, not menu experience\n";
    }
    {
        starfox::simulation::GameSimulation sector_x(rom,symbols,"LEVEL2_2",{},true);
        starfox::audio::Spc700Audio sector_audio;
        for(unsigned tick=0;tick<42;++tick) advance(sector_x,sector_audio,{});
        starfox::vr::GameSceneHistory sector_history(sector_x,rom,symbols);
        if(!sector_history.current()->background_space_horizon
            || !sector_history.current()->background_orbital_planet
            || !sector_history.current()->background_orbital_entry)
            throw std::runtime_error("Sector X lost its planet hemisphere/single-planet surround");
        starfox::simulation::GameSimulation meteor(rom,symbols,"LEVEL3_4",{},true);
        starfox::audio::Spc700Audio meteor_audio;
        for(unsigned tick=0;tick<42;++tick) advance(meteor,meteor_audio,{});
        starfox::vr::GameSceneHistory meteor_history(meteor,rom,symbols);
        if(!meteor_history.current()->background_unique_space
            || meteor_history.current()->background_planet_rect!=std::array<unsigned,4>{320,88,120,120})
            throw std::runtime_error("Meteor lost its star surround/single-planet patch");
        for(const char* level:{"LEVEL1_6","LEVEL3_7","LEVEL3_3","LEVEL3_5"}) {
        starfox::simulation::GameSimulation venom(rom,symbols,level,{},true);
        starfox::audio::Spc700Audio audio;
        for(unsigned tick=0;tick<42;++tick) advance(venom,audio,{});
        starfox::vr::GameSceneHistory history(venom,rom,symbols);
        if(!history.current()->background_landscape || history.current()->background_water_surround)
            throw std::runtime_error("Venom outdoor scene did not select landscape surround");
        if(history.current()->background_landscape_unique_half!=(std::string_view(level)=="LEVEL3_3"))
            throw std::runtime_error("Unique moon atlas policy selected for wrong landscape");
        const unsigned expected_origin=std::string_view(level)=="LEVEL3_5"?(venom.peek_meter_state().extended?16U:272U):232U;
        if(history.current()->landscape_atlas_origin!=expected_origin)
            throw std::runtime_error("Incorrect cartridge landscape atlas origin");
        }
    }
    {
        starfox::simulation::GameSimulation demo(rom,symbols,"CONTMAP",{},true);
        starfox::audio::Spc700Audio demo_audio;
        starfox::vr::VrGameInput demo_input;
        const auto type_address=symbols.find("C_TYPE").at(0);
        const auto initial=demo.map().peek_ram_byte(type_address).value();
        starfox::vr::VrControls controls;controls.select=true;controls.select_pressed=true;
        demo_input.sample(controls);advance(demo,demo_audio,demo_input.consume());
        const auto selected=uint8_t((initial+1)&3);
        if(demo.map().peek_ram_byte(type_address).value()!=selected)
            throw std::runtime_error("VR Select did not cycle native control type");
        controls.select_pressed=false;
        for(unsigned held_tick=0;held_tick<4;++held_tick) {
            demo_input.sample(controls);advance(demo,demo_audio,demo_input.consume());
        }
        if(demo.map().peek_ram_byte(type_address).value()!=selected)
            throw std::runtime_error("Held VR Select repeated control selection");
        std::cout<<"VR Select cycles native control screen once per press\n";
    }
    const auto& restarts=symbols.find("MAPRESTART");
    if(restarts.empty()) throw std::runtime_error("Missing stage checkpoint symbol");
    unsigned warmup=0;
    while(warmup<3000 && vr_game.map().read_native_word(restarts.front())==0) {
        advance(vr_game,vr_audio,{});advance(pad_game,pad_audio,{});++warmup;
    }
    std::cout<<"Checkpoint warmup: "<<warmup<<" ticks\n";
    if(vr_game.map().read_native_word(restarts.front())==0) throw std::runtime_error("Stage checkpoint never became ready");
    const auto initial_x=vr_game.objects().at(vr_game.player()).world_x;
    auto minimum_x=initial_x,maximum_x=initial_x;
    for(unsigned tick=0;tick<40;++tick) {
        starfox::vr::VrControls controls;
        controls.steer={tick<20?-1.F:1.F,0};controls.fire=tick%3==0;
        controls.boost=tick>=10 && tick<15;controls.roll_left=tick==17 || tick==19;
        controls.select=tick==25;controls.select_pressed=tick==25;
        ButtonMask expected=tick<20?left:right;
        if(tick%3==0) expected|=y;
        if(tick>=10 && tick<15) expected|=x;
        if(tick==17 || tick==19) expected|=left_shoulder;
        if(tick==25) expected|=starfox::input::select;
        // Presentation polls multiple times, but source input advances once.
        for(unsigned poll=0;poll<12;++poll) {vr_input.sample(controls);pad_input.sample(expected);}
        const auto a=vr_input.consume(),b=pad_input.consume();
        if(a.held!=b.held || a.pressed!=b.pressed || a.released!=b.released) throw std::runtime_error("VR tick differs from native pad");
        advance(vr_game,vr_audio,a);advance(pad_game,pad_audio,b);
        const auto player_x=vr_game.objects().at(vr_game.player()).world_x;
        minimum_x=std::min(minimum_x,player_x);maximum_x=std::max(maximum_x,player_x);
        if(vr_game.save_state()!=pad_game.save_state()) throw std::runtime_error("VR input changed deterministic gameplay state");
    }
    if(minimum_x==maximum_x) throw std::runtime_error("Input fixture never moved the player");
    std::cout<<"40 gameplay ticks: VR and native-pad complete states match; player X range "<<minimum_x<<".."<<maximum_x<<'\n';
    const auto before_scene=vr_game.save_state();
    {
        // Exercise real cartridge pause/resume and real player geometry, not
        // only an artificial bounding box and an isolated object pool.
        auto clone=vr_game.restored_state(before_scene);
        starfox::audio::Spc700Audio audio;audio.load_state(vr_audio.save_state());
        advance(*clone,audio,{start,start,0});
        if(!clone->paused()) throw std::runtime_error("Sandbox fixture did not enter native pause");
        starfox::vr::GameSceneHistory history(*clone,rom,symbols);
        history.capture();
        starfox::vr::SourceModels models(rom,symbols,false,false);
        auto packets=models.assemble(*history.current());
        const auto player=clone->player();
        const auto found=std::find(packets.handles.begin(),packets.handles.end(),player);
        if(found==packets.handles.end()) throw std::runtime_error("Sandbox fixture has no player geometry");
        const auto index=size_t(found-packets.handles.begin());
        // Isolate this real mesh so a nearer projectile cannot win the pick.
        starfox::vr::SourceModelPackets target;
        target.handles={player};target.packets={packets.packets[index]};
        starfox::vr::PauseSandbox sandbox;sandbox.begin(target,*history.current());
        const auto& matrix=target.packets[0].model;
        XrPosef hand{};hand.orientation.w=1;
        hand.position={matrix[12],matrix[13],matrix[14]+.75F};
        std::array<std::optional<XrPosef>,2> hands{hand,std::nullopt};
        sandbox.update(hands,{false,false});sandbox.update(hands,{true,false});
        const auto frozen=clone->objects().at(player);
        hands[0]->position.x+=.25F;sandbox.update(hands,{true,false});
        auto moved=target;sandbox.apply(moved);
        if(std::abs(moved.packets[0].model[12]-matrix[12]-.25F)>.001F)
            throw std::runtime_error("Sandbox could not grab real player geometry");
        if(clone->objects().at(player)!=frozen)
            throw std::runtime_error("Sandbox changed live simulation before unpausing");
        advance(*clone,audio,{});advance(*clone,audio,{start,start,0});
        if(clone->paused()) throw std::runtime_error("Sandbox fixture did not resume native game");
        sandbox.commit(clone->objects());history.capture();
        const auto& edited=clone->objects().at(player);
        if(edited.world_x==frozen.world_x && edited.world_y==frozen.world_y && edited.world_z==frozen.world_z)
            throw std::runtime_error("Sandbox resume discarded the edit");
        const auto current=history.current()->transforms.at(player).transform;
        if(current.x!=edited.world_x || current.y!=edited.world_y || current.z!=edited.world_z)
            throw std::runtime_error("Sandbox committed position did not reach scene history");
        advance(*clone,audio,{});
        if(clone->paused()) throw std::runtime_error("Sandbox resume left gameplay frozen");
        std::cout<<"Native pause sandbox: actual player pick, frozen simulation, resume commit and scene capture passed\n";
    }
    {
        auto clone=vr_game.restored_state(before_scene);
        clone->set_god_mode(false);clone->set_infinite_lives(true);
        const auto lives=symbols.find("LIVES").at(0);
        clone->map().write_native_byte(lives,1);
        static_cast<void>(clone->tick({}));
        if(clone->map().peek_ram_byte(lives).value()<2 || clone->god_mode()
            || !clone->restored_state(clone->save_state())->infinite_lives())
            throw std::runtime_error("Infinite lives failed reserve/state isolation");
        clone->set_infinite_lives(false);clone->map().write_native_byte(lives,1);
        static_cast<void>(clone->tick({}));
        if(clone->map().peek_ram_byte(lives).value()!=1)
            throw std::runtime_error("Disabled infinite lives still replenished reserves");
        clone->set_god_mode(true);
        const auto& native_god=symbols.find("GODMODE");
        const bool ex=clone->meter_state().extended;
        if(ex && !native_god.empty()) clone->map().write_native_byte(native_god[0],0);
        static_cast<void>(clone->tick({}));
        if(!clone->god_mode() || (ex && !native_god.empty() && clone->map().peek_ram_byte(native_god[0]).value()!=1))
            throw std::runtime_error("EX native setting defeated host God Mode");
    }
    {
        auto clone=vr_game.restored_state(before_scene);
        clone->map().write_native_word(symbols.find("CIRCLERAD").at(0),171);
        clone->map().write_native_byte(symbols.find("CIRCLESRCRED").at(0),31);
        clone->map().write_native_word(symbols.find("CIRCLEANIM").at(0),
            static_cast<uint16_t>(symbols.find("MSCRAMWIPE_CIRCLE").at(0)));
        static_cast<void>(clone->tick({}));
        if(!clone->window_wipe_state().active || clone->circle_effect_state().active)
            throw std::runtime_error("Window wipe reused stale colour-circle state");
    }
    {
        auto clone=vr_game.restored_state(before_scene);
        clone->map().write_native_byte(symbols.find("BUNNY").at(0),7);
        clone->map().write_native_byte(symbols.find("FROG").at(0),13);
        static_cast<void>(clone->map().read_native_byte(symbols.find("BUNNY").at(0)));
        const auto saved=clone->save_state();
        const auto results=clone->stage_results_state();
        if(results.teammate_health[0]!=7 || results.teammate_health[2]!=13
            || clone->save_state()!=saved)
            throw std::runtime_error("Results display snapshot changed emulated state or teammate health");
        static_cast<void>(clone->dust_point_count());
        if(clone->save_state()!=saved)
            throw std::runtime_error("Dust count snapshot changed emulated state");
        clone->set_timing_mode(starfox::simulation::TimingMode::original_speed);
        static_cast<void>(clone->map().read_native_byte(symbols.find("BUNNY").at(0)));
        const auto timing_saved=clone->save_state();
        for(unsigned frame=0;frame<16;++frame) {
            static_cast<void>(clone->logic_tick_ready());
            static_cast<void>(clone->logic_interpolation_alpha(double(frame)/16));
        }
        if(clone->save_state()!=timing_saved)
            throw std::runtime_error("Presentation timing observers changed emulated state");
    }
    {
        auto clone=vr_game.restored_state(before_scene);
        const auto reference=clone->meter_state();
        if(reference.extended) {
            const auto health=vr_game.map().peek_ram_byte(symbols.find("PLAYERB_HP").at(0)).value();
            const auto boost=vr_game.map().peek_ram_byte(symbols.find("DOBOOSTMETER").at(0)).value();
            if(reference.player_health_max!=health
                || reference.player_health_width!=static_cast<uint8_t>(health+4U)
                || reference.boost_enabled!=(boost!=0))
                throw std::runtime_error("EX direct-level meter bootstrap differs from cartridge inputs");
        }
        const auto before_peek=vr_game.save_state();
        if(vr_game.peek_meter_state()!=reference || vr_game.save_state()!=before_peek)
            throw std::runtime_error("RAM-only meter snapshot differs from native getter or mutates state");
    }
    {
        auto clone=vr_game.restored_state(before_scene);
        clone->map().write_native_byte(symbols.find("BG2VOFSOVERRIDE").at(0),1);
        clone->map().write_native_word(symbols.find("BG2HOFSREQ").at(0),0xff80);
        clone->map().write_native_word(symbols.find("BG2VOFSREQ").at(0),0x0123);
        const auto saved=clone->save_state();
        const auto scroll=clone->map().peek_background_scroll_override();
        if(!scroll || (*scroll)[0]!=-128 || (*scroll)[1]!=0x123 || clone->save_state()!=saved)
            throw std::runtime_error("Active display-scroll snapshot changed bus state or signed offsets");
        starfox::vr::GameSceneHistory captured(*clone,rom,symbols);
        if(captured.current()->background_scroll_override!=scroll || clone->save_state()!=saved)
            throw std::runtime_error("Active display-scroll capture changed full game state");
    }
    starfox::vr::GameSceneHistory scenes(vr_game,rom,symbols);
    if(vr_game.save_state()!=before_scene) throw std::runtime_error("Scene capture changed CPU or game state");
    const auto retained_scene=scenes.current();
    if(retained_scene->native_ex_bitmap!=(vr_game.peek_meter_state().extended
        && (vr_game.flow_state()==starfox::simulation::GameFlowState::gameplay
            || vr_game.flow_state()==starfox::simulation::GameFlowState::training
            || vr_game.flow_state()==starfox::simulation::GameFlowState::ex_pregame_menu
            || vr_game.flow_state()==starfox::simulation::GameFlowState::intro
            || vr_game.flow_state()==starfox::simulation::GameFlowState::stage_results)))
        throw std::runtime_error("Native EX bitmap scene gate differs from source flow");
    auto expected_display=vr_game.map().ppu_state();
    if(vr_game.peek_meter_state().extended) expected_display.cgram[207]=0x03e0;
    if(!retained_scene->ppu || *retained_scene->ppu!=expected_display
        || retained_scene->background_scroll_override!=vr_game.map().peek_background_scroll_override()
        || retained_scene->display_brightness!=vr_game.map().display_brightness())
        throw std::runtime_error("Scene display snapshot differs from completed source tick");
    const auto retained_ppu=*retained_scene->ppu;
    const auto retained_dust=retained_scene->dust_points;
    if(retained_dust!=vr_game.dust().points()) throw std::runtime_error("Dust snapshot differs from source points");
    {
        auto clone=vr_game.restored_state(vr_game.save_state());
        if(retained_scene->dust_point_count!=clone->dust_point_count()
            || retained_scene->dots_mode!=clone->map().dots_mode())
            throw std::runtime_error("Dust metadata snapshot differs from native getters");
        if(retained_scene->meters.extended) {
            clone->map().write_native_word(symbols.find("M_MOREDOTS").at(0),1);
            clone->map().write_native_word(symbols.find("M_GRIDLINES").at(0),1);
            const auto before=clone->save_state();
            starfox::vr::GameSceneHistory extended(*clone,rom,symbols);
            if(extended.current()->dust_point_count!=starfox::simulation::kMaximumDustPoints
                || !extended.current()->grid_lines || clone->save_state()!=before)
                throw std::runtime_error("EX extended dust/grid capture mismatch or state mutation");
            const auto first=extended.current();
            if(first->dots_mode<=0) throw std::runtime_error("Grid history fixture did not enable ground grid");
            {
                const auto projected=starfox::render::project_source_grid(
                    starfox::timing::interpolate(first->camera,first->camera,1.),first->view_matrix,224,192);
                if(!projected.count) throw std::runtime_error("Grid history fixture had no visible endpoint");
                auto expected=first->grid_line_start;
                if(projected.count) {
                    const auto& last=projected.points[projected.count-1];
                    expected={static_cast<int16_t>(last.x-1),last.y};
                }
                extended.capture();
                if(extended.current()->grid_line_start!=expected || clone->save_state()!=before
                    || first->grid_line_start!=std::array<int16_t,2>{0,0})
                    throw std::runtime_error("VR grid history capture mutated VM/retained frame or lost endpoint");
            }
        }
    }
    starfox::vr::SourceModels source_models(rom,symbols);
    {
        starfox::vr::SourceModels uncached(rom,symbols,false);
        for(const auto brightness:{15,7,0,15}) {
            auto faded=*retained_scene;faded.display_brightness=static_cast<uint8_t>(brightness);
            const auto actual=source_models.assemble_interpolated(faded,faded,1.,true);
            const auto expected=uncached.assemble_interpolated(faded,faded,1.,true);
            if(actual.handles!=expected.handles || !actual.pending.empty() || !expected.pending.empty()
                || !starfox::vr::same_draw_geometry(actual.packets,expected.packets))
                throw std::runtime_error("Cached models retained stale display brightness during fade");
        }
    }
    {
        for(bool connected:{false,true}) {
            auto fixture=*retained_scene;
            fixture.dots_mode=1;fixture.grid_lines=connected;
            fixture.flow=starfox::simulation::GameFlowState::gameplay;
            const auto immersive=source_models.assemble_world_interpolated(fixture,fixture,.5,true,true);
            const auto models=source_models.assemble_interpolated(fixture,fixture,.5,true);
            if(immersive.handles.size()!=models.handles.size()+2 || immersive.handles[0]!=0x30000U || immersive.handles[1]!=0x20000U)
                throw std::runtime_error("Immersive scene must prepend restored ground grid and stars");
            const auto grid=fixture.grid_lines
                ?source_models.assemble_connected_grid_interpolated(fixture,fixture,.5,true)
                :source_models.assemble_grid_interpolated(fixture,fixture,.5,true,256,true,false);
            const auto actual=immersive.packets[0].geometry.vertex_view(),expected_grid=grid.geometry.vertex_view();
            if(actual.empty() || actual.size()!=expected_grid.size()
                || !std::equal(actual.begin(),actual.end(),expected_grid.begin()) || immersive.packets[0].model!=grid.model
                || immersive.packets[0].geometry.texels!=grid.geometry.texels)
                throw std::runtime_error("Immersive grid differs from native grid mode/camera placement");
            for(size_t i=0;i<models.handles.size();++i)
                if(immersive.handles[i+2]!=models.handles[i])
                    throw std::runtime_error("Restoring immersive grid changed model painter order");
            if(immersive.compute_models.size()!=models.compute_models.size())
                throw std::runtime_error("Restoring immersive grid lost compute models");
            for(size_t i=0;i<models.compute_models.size();++i)
                if(immersive.compute_models[i].packet_index!=models.compute_models[i].packet_index+2)
                    throw std::runtime_error("Immersive compute model index omitted restored grid prefix");
        }
    }
    {
        const auto world=source_models.assemble_world_interpolated(*retained_scene,*retained_scene,.5,true);
        auto expected=source_models.assemble_interpolated(*retained_scene,*retained_scene,.5,true);
        auto dust=source_models.assemble_dust_interpolated(*retained_scene,*retained_scene,.5,true,256,true);
        auto grid=retained_scene->grid_lines
            ?source_models.assemble_connected_grid_interpolated(*retained_scene,*retained_scene,.5,true)
            :source_models.assemble_grid_interpolated(*retained_scene,*retained_scene,.5,true,256,true);
        expected.packets.insert(expected.packets.begin(),std::move(dust));
        expected.handles.insert(expected.handles.begin(),0x20000U);
        expected.packets.insert(expected.packets.begin(),std::move(grid));
        expected.handles.insert(expected.handles.begin(),0x30000U);
        if(world.handles!=expected.handles || world.pending.size()!=expected.pending.size()
            || !starfox::vr::same_draw_geometry(world.packets,expected.packets))
            throw std::runtime_error("Shared world assembly changed painter order or geometry");
        for(size_t i=0;i<world.packets.size();++i) if(world.packets[i].model!=expected.packets[i].model)
            throw std::runtime_error("Shared world assembly changed transforms");
    }
    {
        auto fixture=*retained_scene;fixture.dots_mode=1;fixture.grid_lines=false;
        fixture.flow=starfox::simulation::GameFlowState::gameplay;fixture.camera={};
        fixture.camera.y=-64;fixture.view_matrix={32767,0,0,0,32767,0,0,0,32767};
        const auto packet=source_models.assemble_grid(fixture);
        {
            auto faded=fixture;faded.display_brightness=0;
            const auto black=source_models.assemble_grid_gpu(faded);
            if(black.geometry.vertex_view().empty())
                throw std::runtime_error("Fade test requires real grid geometry");
            for(const auto& vertex:black.geometry.vertex_view())
                if(vertex.color[0]!=0 || vertex.color[1]!=0 || vertex.color[2]!=0)
                    throw std::runtime_error("Grid remains lit during source forced black");
            faded.display_brightness=15;
            const auto restored=source_models.assemble_grid_gpu(faded);
            auto full=fixture;full.display_brightness=15;
            const auto expected=source_models.assemble_grid_gpu(full);
            if(!starfox::vr::same_draw_geometry(std::span(&restored,1),std::span(&expected,1)))
                throw std::runtime_error("Restoring display brightness retained faded grid data");
        }
        const auto shared_grid=source_models.assemble_grid_gpu(fixture);
        auto moved_grid=fixture;moved_grid.camera.x=16;
        const auto shared_moved=source_models.assemble_grid_gpu(moved_grid);
        auto one_unit=fixture;one_unit.camera.x=1;
        const auto half_unit=source_models.assemble_grid_interpolated(fixture,one_unit,.5,false,256,true);
        if(half_unit.geometry.texels.size()!=15
            || std::bit_cast<float>(half_unit.geometry.texels[12])!=-0.5F
            || std::bit_cast<float>(half_unit.geometry.texels[13])!=0.F
            || std::bit_cast<float>(half_unit.geometry.texels[14])!=0.F)
            throw std::runtime_error("GPU VR grid discarded fractional camera motion");
        if(!shared_grid.geometry.shared_vertices || !shared_grid.geometry.vertices.empty()
            || shared_grid.geometry.shared_vertices!=shared_moved.geometry.shared_vertices)
            throw std::runtime_error("GPU grid camera update copied immutable vertices");
        const auto retained_vertex=shared_grid.geometry.vertex_view()[0];
        auto recoloured=fixture;recoloured.model_palette[14]^=0x7fff;
        const auto changed_grid=source_models.assemble_grid_gpu(recoloured);
        if(changed_grid.geometry.shared_vertices==shared_grid.geometry.shared_vertices
            || !(shared_grid.geometry.vertex_view()[0]==retained_vertex))
            throw std::runtime_error("GPU grid material update mutated retained geometry");
        if(packet.geometry.vertices.size()!=720 || !packet.geometry.texels.empty()
            || packet.model[12]!=0 || packet.model[13]!=0 || packet.model[14]!=0)
            throw std::runtime_error("Grid source lattice/near-secondary count mismatch");
        for(const auto& vertex:packet.geometry.vertices)
            if(vertex.position[2]<=256 || vertex.position[1]<63 || vertex.position[1]>64)
                throw std::runtime_error("Grid point escaped source ground/depth constraints");
        {
            const auto previous=fixture;auto moving=fixture;moving.camera.x=16;
            for(unsigned phase=0;phase<=16;++phase) {
                const auto interpolated=source_models.assemble_grid_interpolated(previous,moving,double(phase)/16);
                auto expected_scene=fixture;expected_scene.camera.x=static_cast<int16_t>(phase);
                const auto expected=source_models.assemble_grid(expected_scene);
                const auto gpu=source_models.assemble_grid_interpolated(previous,moving,double(phase)/16,false,256,true);
                const auto expected_gpu=source_models.assemble_grid_gpu(expected_scene);
                if(!starfox::vr::same_draw_geometry(std::span(&gpu,1),std::span(&expected_gpu,1)))
                    throw std::runtime_error("GPU grid interpolation differs from source lattice payload");
                if(!starfox::vr::same_draw_geometry(std::span(&interpolated,1),std::span(&expected,1)))
                    throw std::runtime_error("Grid interpolation differs from source lattice phase");
            }
            auto transitioned=previous;transitioned.flow=starfox::simulation::GameFlowState::intro;
            const auto snapped=source_models.assemble_grid_interpolated(transitioned,moving,0.);
            const auto current=source_models.assemble_grid(moving);
            if(!starfox::vr::same_draw_geometry(std::span(&snapped,1),std::span(&current,1)))
                throw std::runtime_error("Grid interpolated across scene transition");
        }
        fixture.dots_mode=0;
        {
            auto connected=fixture;connected.dots_mode=1;connected.grid_lines=true;connected.grid_line_start={0,0};
            starfox::render::DustRenderer renderer(rom,symbols);
            for(unsigned source_frame=0;source_frame<16;++source_frame) {
            connected.camera.x=int16_t(source_frame*17);
            connected.camera.z=int16_t(source_frame*31);
            connected.view_matrix=source_frame%2
                ?starfox::simulation::MatrixQ15{23170,23170,0,-23170,23170,0,0,0,32767}
                :starfox::simulation::MatrixQ15{32767,0,0,0,32767,0,0,0,32767};
            const auto lines=source_models.assemble_connected_grid(connected);
            starfox::render::Framebuffer expected{224,192},actual{224,192};
            renderer.draw_grid_lines(starfox::timing::interpolate(connected.camera,connected.camera,1.),connected.view_matrix,source_frame,expected);
            starfox::render::Framebuffer repeated{224,192};
            renderer.draw_grid_lines(starfox::timing::interpolate(connected.camera,connected.camera,1.),connected.view_matrix,source_frame,repeated);
            if(repeated.pixels()!=expected.pixels()) throw std::runtime_error("Connected grid advanced history between eyes");
            const auto vertices=lines.geometry.vertex_view();
            if(vertices.empty()) throw std::runtime_error("Connected grid packet was empty");
            for(size_t i=0;i<vertices.size();i+=6) {
                const auto& v=vertices[i];const auto& end=vertices[i+2];
                for(int y=int(v.position[1])-16;y<int(end.position[1])-16;++y)
                    for(int x=int(v.position[0])-16;x<int(end.position[0])-16;++x) {
                        bool visible=true;
                        if(v.texture[3]==256) {
                            const auto offset=v.texture[0];
                            const std::array<int16_t,2> current{int16_t(lines.geometry.texels[offset]),int16_t(lines.geometry.texels[offset+1])};
                            const std::array<int16_t,2> previous{int16_t(lines.geometry.texels[offset+2]),int16_t(lines.geometry.texels[offset+3])};
                            const int step=current[0]-2-x;
                            const auto sample=starfox::render::grid_line_sample(current,previous,uint32_t(step));
                            visible=step>=0 && sample && (*sample)[1]==y;
                        }
                        if(visible) actual.set(x,y,126);
                    }
            }
            if(actual.pixels()!=expected.pixels()) throw std::runtime_error("Connected grid packet rectangles/markers differ from source raster");
            const auto binned=source_models.assemble_connected_grid_binned(connected);
            if(binned.geometry.vertex_view().size()!=6) throw std::runtime_error("Binned grid is not one quad");
            const auto& words=binned.geometry.texels;
            starfox::render::Framebuffer binned_raster{224,192};
            for(unsigned y=0;y<192;++y) for(unsigned x=0;x<224;++x) {
                const auto offset=words.at(y*2),count=words.at(y*2+1);
                for(unsigned i=0;i<count;++i) {
                    const auto record=words.at(offset+i);
                    const std::array<int16_t,2> current{int16_t(words.at(record+1)),int16_t(words.at(record+2))};
                    bool visible=false;
                    if(words.at(record)==1) visible=current[0]==int(x) && current[1]==int(y);
                    else {
                        const std::array<int16_t,2> previous{int16_t(words.at(record+3)),int16_t(words.at(record+4))};
                        const int step=current[0]-2-int(x);
                        const auto sample=starfox::render::grid_line_sample(current,previous,uint32_t(step));
                        visible=step>=0 && sample && (*sample)[1]==int(y);
                    }
                    if(visible) {binned_raster.set(x,y,126);break;}
                }
            }
            if(binned_raster.pixels()!=expected.pixels()) throw std::runtime_error("Connected grid bins omit source pixels");
            const auto repeated_packet=source_models.assemble_connected_grid_binned(connected);
            if(!starfox::vr::same_draw_geometry(std::span(&binned,1),std::span(&repeated_packet,1)))
                throw std::runtime_error("Repeated connected-grid assembly mutated geometry");
            auto interpolation_start=connected,interpolation_end=connected;
            interpolation_start.camera.x=0;interpolation_end.camera.x=16;
            interpolation_start.grid_line_start={-100,100};
            for(unsigned phase=0;phase<=16;++phase) {
                const auto interpolated=source_models.assemble_connected_grid_interpolated(
                    interpolation_start,interpolation_end,double(phase)/16);
                auto reference=interpolation_end;reference.camera.x=int16_t(phase);
                const auto direct=source_models.assemble_connected_grid_gpu(reference);
                if(!starfox::vr::same_draw_geometry(std::span(&interpolated,1),std::span(&direct,1)))
                    throw std::runtime_error("Connected-grid interpolation changes source endpoint or camera quantization");
            }
            auto transition=interpolation_start;transition.flow=starfox::simulation::GameFlowState::intro;
            const auto snapped=source_models.assemble_connected_grid_interpolated(transition,interpolation_end,0.);
            const auto final=source_models.assemble_connected_grid_gpu(interpolation_end);
            if(!starfox::vr::same_draw_geometry(std::span(&snapped,1),std::span(&final,1)))
                throw std::runtime_error("Connected grid interpolated across transition");
            const auto points=starfox::render::project_source_grid(
                starfox::timing::interpolate(connected.camera,connected.camera,1.),connected.view_matrix,224,192);
            if(points.count) {
                const auto& last=points.points[points.count-1];
                connected.grid_line_start={int16_t(last.x-1),last.y};
            }
            }
        }
        if(!source_models.assemble_grid(fixture).geometry.vertices.empty()) throw std::runtime_error("Disabled grid emitted points");
        fixture.dots_mode=1;fixture.grid_lines=true;bool rejected=false;
        try {(void)source_models.assemble_grid(fixture);} catch(const std::runtime_error&) {rejected=true;}
        if(!rejected) throw std::runtime_error("Connected grid silently substituted dots");
    }
    {
        auto fixture=*retained_scene;fixture.dots_mode=-1;fixture.dust_point_count=4;
        fixture.flow=starfox::simulation::GameFlowState::gameplay;fixture.camera={};
        fixture.view_matrix={32767,0,0,0,32767,0,0,0,32767};
        fixture.dust_points[0]={0,0,128};fixture.dust_points[1]={32,16,512};
        fixture.dust_points[2]={-32,-16,2048};fixture.dust_points[3]={0,0,8192};
        const auto packet=source_models.assemble_dust(fixture);
        const auto gpu_points=source_models.assemble_dust_gpu(fixture);
        auto camera_only=fixture;camera_only.camera.x=13;
        const auto same_points=source_models.assemble_dust_gpu(camera_only);
        if(!gpu_points.geometry.shared_vertices || gpu_points.geometry.shared_vertices!=same_points.geometry.shared_vertices)
            throw std::runtime_error("GPU dust camera update copied source vertices");
        const auto retained_point=gpu_points.geometry.vertex_view()[0];
        auto recycled_point=fixture;recycled_point.dust_points[0].x+=1;
        const auto recycled_gpu=source_models.assemble_dust_gpu(recycled_point);
        if(recycled_gpu.geometry.shared_vertices==gpu_points.geometry.shared_vertices
            || !(gpu_points.geometry.vertex_view()[0]==retained_point))
            throw std::runtime_error("GPU dust recycled point reused or mutated old geometry");
        {
            auto changing=fixture;
            for(const size_t count:{size_t(0),size_t(1),size_t(120),size_t(511),size_t(4)}) {
                changing.dust_point_count=count;
                const auto tested=source_models.assemble_dust_gpu(changing);
                if(tested.geometry.vertex_view().size()!=count*12)
                    throw std::runtime_error("GPU dust count change retained stale vertices");
                for(size_t i=0;i<count;++i)
                    if(tested.geometry.vertex_view()[i*12].texture[1]!=((count-i)&3))
                        throw std::runtime_error("GPU dust count change retained stale colour phase");
            }
            const auto before_colour=source_models.assemble_dust_gpu(fixture);
            auto recoloured=fixture;recoloured.model_palette.fill(0x7fff);
            const auto after_colour=source_models.assemble_dust_gpu(recoloured);
            if(before_colour.geometry.shared_vertices!=after_colour.geometry.shared_vertices
                || before_colour.geometry.texels==after_colour.geometry.texels)
                throw std::runtime_error("GPU dust palette change failed to update only payload");
            changing.dust_point_count=512;bool rejected=false;
            try {(void)source_models.assemble_dust_gpu(changing);} catch(const std::invalid_argument&) {rejected=true;}
            if(!rejected) throw std::runtime_error("GPU dust accepted oversized source point count");
        }
        {
            const auto previous=fixture;auto moving=fixture;moving.camera.x=13;
            for(unsigned phase=0;phase<=12;++phase) {
                const auto interpolated=source_models.assemble_dust_interpolated(previous,moving,double(phase)/12);
                const double expected=(32.-13.*phase/12)*32767./32768.;
                if(interpolated.geometry.vertices.size()!=24
                    || std::abs(interpolated.geometry.vertices[0].position[0]-expected)>.00001)
                    throw std::runtime_error("Dust camera interpolation lost fractional motion");
            }
            auto transitioned=previous;transitioned.flow=starfox::simulation::GameFlowState::intro;
            const auto snapped=source_models.assemble_dust_interpolated(transitioned,moving,0.);
            const auto current=source_models.assemble_dust(moving);
            if(!starfox::vr::same_draw_geometry(std::span(&snapped,1),std::span(&current,1)))
                throw std::runtime_error("Dust interpolated across scene transition");
        }
        {
            auto colour_fixture=fixture;colour_fixture.dust_points.fill({0,0,0});
            for(unsigned i=0;i<16;++i) colour_fixture.model_palette[i]=static_cast<uint16_t>(i|((15-i)<<5)|((i+8)<<10));
            const auto palette=starfox::render::decode_bgr555_palette(colour_fixture.model_palette);
            const auto table=symbols.find("STAR_COLS").at(0);
            for(unsigned phase=0;phase<4;++phase) for(unsigned bucket=0;bucket<16;++bucket) {
                colour_fixture.dust_point_count=4+phase;
                colour_fixture.dust_points[0]={0,0,static_cast<int16_t>(bucket*256+128)};
                const auto tested=source_models.assemble_dust(colour_fixture);
                if(bucket==0) {
                    if(!tested.geometry.vertices.empty()) throw std::runtime_error("Near dust bucket emitted ink");
                    continue;
                }
                const auto shade=rom.read8(table+phase*16+bucket);
                if(shade>=palette.size()) throw std::runtime_error("Unexpected source star palette index");
                const auto expected=palette[shade];
                if(tested.geometry.vertices.empty()) throw std::runtime_error("Visible dust bucket omitted");
                for(const auto& v:tested.geometry.vertices)
                    if(std::abs(v.color[0]*255-expected.r)>.001F
                        || std::abs(v.color[1]*255-expected.g)>.001F
                        || std::abs(v.color[2]*255-expected.b)>.001F)
                        throw std::runtime_error("Dust STAR_COLS phase/depth colour mismatch");
            }
        }
        if(packet.geometry.vertices.size()!=24 || !packet.geometry.texels.empty()
            || packet.model[12]!=0 || packet.model[13]!=0 || packet.model[14]!=0)
            throw std::runtime_error("Dust near/far billboard count or CPU pixel upload mismatch");
        if(packet.geometry.vertices[18].position[2]!=4095
            || packet.geometry.vertices[6].billboard[0]>=0
            || packet.geometry.vertices[6].billboard[1]>=0)
            throw std::runtime_error("Dust far-depth cap or near secondary dot offset mismatch");
        for(auto flow:{starfox::simulation::GameFlowState::planet_select,
            starfox::simulation::GameFlowState::planet_travel,starfox::simulation::GameFlowState::continue_choice}) {
            fixture.flow=flow;
            if(!source_models.assemble_dust(fixture).geometry.vertices.empty())
                throw std::runtime_error("Excluded dust scene emitted geometry");
        }
    }
    const auto assembled=source_models.assemble(*retained_scene);
    {
        auto fixture=*retained_scene;fixture.objects.resize(1);fixture.shadows_enabled=false;
        auto& item=fixture.objects.front();item.object.strategy_flags={};item.object.strategy_flags[0]=0x40;
        item.object.shape=0;item.object.colour_table=static_cast<uint16_t>(symbols.find("MSG_NINTENDO").at(0));
        item.object.extended[21]=3;item.object.texture_scroll_x=0;item.source_pose={};item.source_pose.z=512;
        fixture.model_palette[3]=0x03e0;
        const auto rendered=source_models.assemble(fixture);
        if(!rendered.pending.empty() || rendered.packets.size()!=1 || rendered.packets[0].geometry.vertices.empty())
            throw std::runtime_error("Source projected text missing without model shape");
        const auto& geometry=rendered.packets[0].geometry;
        const auto font=symbols.find("MSCALECHARS").at(0),message=symbols.find("MSG_NINTENDO").at(0);
        size_t vertex_index=0;
        for(unsigned character=0;character<256;++character) {
            const auto token=rom.read8(message+character);if(!token) break;if(token>41) continue;
            if(vertex_index+6>geometry.vertices.size()) throw std::runtime_error("Projected text glyph count mismatch");
            const auto offset=geometry.vertices[vertex_index].texture[0];
            if((geometry.vertices[vertex_index].texture[3]&1024U)==0
                || (geometry.vertices[vertex_index].texture[3]&134217728U)==0
                || geometry.vertices[vertex_index].group_a[0]!=127 || geometry.vertices[vertex_index].group_a[1]!=512
                || offset+9>geometry.texels.size() || geometry.texels.at(offset)!=0xff00ff00U)
                throw std::runtime_error("Projected text packed glyph header mismatch");
            for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
                const auto expected=(rom.read16(font+uint32_t(token-1)*32+y*2)&(0x8000U>>x))?0xff00ff00U:0U;
                const auto row=geometry.texels.at(offset+1+y/2)>>((y%2)*16);
                const auto actual=(row&(0x8000U>>x))?geometry.texels.at(offset):0U;
                if(actual!=expected) throw std::runtime_error("Projected text ROM glyph mismatch");
            }
            vertex_index+=6;
        }
        if(vertex_index!=geometry.vertices.size()) throw std::runtime_error("Extra projected glyph vertices");
        item.object.colour_table=0;
        if(!source_models.assemble(fixture).packets[0].geometry.vertices.empty()) throw std::runtime_error("Uninitialized text pointer rendered");
        item.object.colour_table=static_cast<uint16_t>(message);item.object.texture_scroll_x=128;
        const auto zero_size=source_models.assemble(fixture);
        if(zero_size.packets[0].geometry.vertices.empty() || zero_size.packets[0].geometry.vertices[0].group_a[0]!=-1)
            throw std::runtime_error("Nonpositive text size not sent to GPU rejection gate");
        item.object.texture_scroll_x=0;item.source_pose.z=127;
        const auto near_text=source_models.assemble(fixture);
        if(near_text.packets[0].geometry.vertices.empty() || near_text.packets[0].geometry.vertices[0].group_a[1]!=127)
            throw std::runtime_error("Projected text near depth not sent to GPU rejection gate");
    }
    {
        auto fixture=*retained_scene;fixture.objects.resize(1);fixture.shadows_enabled=true;fixture.shadow_height=300;
        fixture.camera={};fixture.view_matrix={32767,0,0,0,32767,0,0,0,32767};
        auto& item=fixture.objects.front();item.object.shape=static_cast<uint16_t>(symbols.find("MYSHIP_4").at(0));
        item.object.strategy_flags={};item.object.strategy_flags[0]=9;item.object.colour_table=0;
        item.source_pose={};item.source_pose.z=1000;item.source_pose.source_depth=1000;
        item.presentation.transform={100,-200,1000,0,0,0};
        item.presentation.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
        fixture.transforms.clear();fixture.transforms[item.handle]=item.presentation;
        fixture.model_palette[9]=0x03e0;
        const auto poses=starfox::vr::interpolate_scene_poses(fixture,fixture,1.,{},true);
        if(poses.size()!=1 || !poses[0].force_colour || poses[0].forced_colour!=9
            || std::abs(poses[0].y-300.*32767/32768.)>.001
            || poses[0].rotation_matrix[1]!=0 || poses[0].rotation_matrix[4]!=0 || poses[0].rotation_matrix[7]!=0)
            throw std::runtime_error("Native shadow placement/flattening mismatch");
        const auto shadowed=source_models.assemble(fixture);
        if(!shadowed.pending.empty() || shadowed.packets.size()!=2
            || shadowed.handles[0]!=(uint32_t(item.handle)|0x10000U) || shadowed.handles[1]!=item.handle)
            throw std::runtime_error("Native shadows not ordered before models");
        const auto& shadow=shadowed.packets[0].geometry;
        if(shadow.vertex_view().empty() && shadow.line_vertices.empty()) throw std::runtime_error("Empty source shadow geometry");
        for(const auto& vertex:shadow.vertex_view())
            if(vertex.color[0]!=0 || vertex.color[1]!=1 || vertex.color[2]!=0 || vertex.texture[3]!=0)
                throw std::runtime_error("Native shadow forced colour mismatch");
        fixture.shadows_enabled=false;
        if(source_models.assemble(fixture).packets.size()!=1) throw std::runtime_error("Source shadow gate ignored");
        fixture.shadows_enabled=true;item.object.strategy_flags[0]=5;
        const auto true_colour=starfox::vr::interpolate_scene_poses(fixture,fixture,1.,{},true);
        if(true_colour[0].force_colour || std::abs(true_colour[0].y+200.*32767/32768.)>.001
            || source_models.assemble(fixture).packets.size()!=1)
            throw std::runtime_error("True-colour shadow position or exclusive pass mismatch");
    }
    if(!symbols.find("M_NANMODE").empty()) {
        starfox::vr::SourceModels compute(rom,symbols,true,true);
        auto fixture=*retained_scene;fixture.objects.resize(1);fixture.shadows_enabled=false;
        fixture.camera={};fixture.view_matrix={32767,0,0,0,32767,0,0,0,32767};
        auto& item=fixture.objects.front();
        item.object.shape=static_cast<uint16_t>(symbols.find("MYSHIP_4").at(0));
        item.object.strategy_flags={};item.object.colour_table=0;
        item.source_pose={};item.source_pose.z=item.source_pose.source_depth=1000;
        item.presentation.transform={0,0,1000,0,0,0};
        item.presentation.rotation_matrix=fixture.view_matrix;
        fixture.transforms.clear();fixture.transforms[item.handle]=item.presentation;
        const auto accepted_compute=[](const auto& assembled) {
            starfox::vr::VulkanComputeRayScene::Plan plan;
            starfox::vr::SourceRayCoverage coverage;
            return assembled.pending.empty() && assembled.compute_fallbacks.empty()
                && assembled.compute_models.size()==1
                && starfox::vr::VulkanComputeRayScene::plan(assembled.compute_models,256,plan)
                && starfox::vr::VulkanComputeRayScene::coverage(assembled.compute_models,plan,coverage);
        };
        for(const auto* table:{"NAN_C","FIREBODY_C","BLUELAVABODY_C","STEALTH_C","TREVORTEX_C"}) {
            fixture.colour_table_override=static_cast<uint16_t>(symbols.find(table).at(0));
            const auto assembled=compute.assemble(fixture);
            if(!accepted_compute(assembled))
                throw std::runtime_error(std::string("EX alternate material left GPU model path: ")+table);
        }
        fixture.colour_table_override.reset();
        for(unsigned mode=0;mode<4;++mode) {
            item.source_pose.wobble_mode=mode==0?1:0;
            item.source_pose.wave_mode=mode==1;
            item.source_pose.cel_mode=mode==2;
            item.source_pose.wireframe_mode=mode==3?1:0;
            item.source_pose.wave_offset=97;
            const auto assembled=compute.assemble(fixture);
            if(!accepted_compute(assembled))
                throw std::runtime_error("EX alternate geometry left GPU model path: "+std::to_string(mode));
        }
    }
    if(retained_scene->particles!=vr_game.particles().particles())
        throw std::runtime_error("Scene snapshot omitted source particle state");
    {
        auto fixture=*retained_scene;fixture.objects.resize(1);fixture.particles={};
        auto& owner=fixture.objects.front();owner.object.strategy_flags={};owner.object.strategy_flags[0]=0x10;
        owner.source_pose={};owner.source_pose.z=512;
        fixture.model_palette[3]=0x03e0;
        auto& dot=fixture.particles[0];dot.life=10;dot.owner=owner.handle;dot.colour=3;
        dot.previous_x=10;dot.x=30;
        auto& trail=fixture.particles[1];trail=dot;trail.flags=4;
        fixture.particles[2]=dot;fixture.particles[2].life=0;
        fixture.particles[3]=dot;fixture.particles[3].owner=owner.handle+1;
        fixture.particles[4]=dot;fixture.particles[4].z=-257;fixture.particles[4].previous_z=-257;
        const auto saved_particles=fixture.particles;
        const auto result=source_models.assemble(fixture);
        if(!result.pending.empty() || result.packets.size()!=1)
            throw std::runtime_error("Owner particles still deferred");
        const auto& geometry=result.packets.front().geometry;
        if(geometry.vertices.size()!=12 || geometry.line_vertices.size()!=2 || !geometry.texels.empty())
            throw std::runtime_error("Particle owner/life filtering or GPU near-depth submission mismatch");
        if(geometry.vertices[0].group_a[0]!=10 || geometry.vertices[0].group_b[0]!=30
            || geometry.vertices[2].billboard[0]!=1 || geometry.vertices[2].billboard[1]!=-1
            || geometry.vertices[0].color[1]!=1 || geometry.vertices[0].texture[3]!=0x80000004U
            || geometry.line_vertices[0].group_c[2]!=1 || geometry.line_vertices[1].group_c[2]!=2)
            throw std::runtime_error("Particle GPU endpoints, palette or corner template mismatch");
        const auto interpolated=source_models.assemble_interpolated(fixture,fixture,.5);
        if(interpolated.packets.size()!=1 || interpolated.packets[0].geometry.vertices[0].group_c[0]!=.5F
            || interpolated.packets[0].geometry.line_vertices[1].group_c[0]!=.5F)
            throw std::runtime_error("Particle GPU interpolation parameter mismatch");
        if(fixture.particles!=saved_particles || retained_scene->particles!=vr_game.particles().particles())
            throw std::runtime_error("Particle packet assembly mutated retained state");
    }
    if(assembled.packets.empty() || assembled.packets.size()!=assembled.handles.size())
        throw std::runtime_error("Live source model assembly produced no packets");
    std::cout<<"Live source model packets: "<<assembled.packets.size()<<", explicit pending passes: "<<assembled.pending.size()<<'\n';
    for(const auto& issue:assembled.pending) std::cout<<"  object "<<issue.handle<<": "<<issue.reason<<'\n';
    if(const auto& lasers=symbols.find("RELFASTELASER");!lasers.empty()) {
        auto intro=*retained_scene;intro.objects.resize(1);
        auto& beam=intro.objects.front();beam.object.shape=static_cast<uint16_t>(lasers.front());
        beam.object.strategy_flags={};beam.object.colour_table=0;
        beam.source_pose={};beam.source_pose.z=512;beam.source_pose.source_depth=512;
        intro.colour_table_override.reset();intro.flow=starfox::simulation::GameFlowState::intro;
        auto collapsed=source_models.assemble(intro);
        if(!collapsed.pending.empty() || collapsed.packets.size()!=1
            || collapsed.packets[0].geometry.line_vertices.size()!=2 || !collapsed.packets[0].geometry.vertices.empty())
            throw std::runtime_error("Intro near-camera laser did not use native axis line");
        intro.flow=starfox::simulation::GameFlowState::gameplay;
        const auto solid=source_models.assemble(intro);
        if(!solid.pending.empty() || solid.packets.size()!=1 || solid.packets[0].geometry.vertex_view().empty())
            throw std::runtime_error("Intro beam collapse leaked into gameplay");
        const auto& crosshairs=symbols.find("TEST_ISTRAT");
        if(!symbols.find("M_NANMODE").empty() && !crosshairs.empty()) {
            beam.object.strategy_address=crosshairs.front();
            beam.source_pose.colour_warp=true;
            beam.source_pose.wireframe_mode=1;beam.source_pose.wobble_mode=1;
            beam.source_pose.wave_mode=true;beam.source_pose.cel_mode=true;
            beam.source_pose.wave_offset=97;
            intro.cgram[207]=0x03e0; // Dedicated reticle colour: pure green.
            const auto marks=source_models.assemble(intro);
            if(!marks.pending.empty() || marks.packets.size()!=1 || marks.packets[0].geometry.vertex_view().empty())
                throw std::runtime_error("EX reticle was rejected by scene colour warp");
            if(!marks.packets[0].preserve_native_colour)
                throw std::runtime_error("EX reticle lost GPU style exemption");
            for(const auto& vertex:marks.packets[0].geometry.vertex_view())
                if(vertex.texture[3]!=0 || vertex.color[0]!=0 || vertex.color[1]!=1 || vertex.color[2]!=0)
                    throw std::runtime_error("EX reticle ignored dedicated palette colour");
            for(const auto* table:{"NAN_C","FIREBODY_C","BLUELAVABODY_C","STEALTH_C","TREVORTEX_C"}) {
                intro.colour_table_override=static_cast<uint16_t>(symbols.find(table).at(0));
                const auto alternate=source_models.assemble(intro);
                if(!alternate.pending.empty() || alternate.packets.size()!=1)
                    throw std::runtime_error("NAN material rejected the reticle");
                const auto expected=marks.packets[0].geometry.vertex_view();
                const auto actual=alternate.packets[0].geometry.vertex_view();
                if(expected.size()!=actual.size()) throw std::runtime_error("NAN material changed reticle geometry");
                for(size_t vertex=0;vertex<expected.size();++vertex)
                    if(expected[vertex].position!=actual[vertex].position
                        || expected[vertex].color!=actual[vertex].color
                        || expected[vertex].texture!=actual[vertex].texture)
                        throw std::runtime_error("NAN mode changed reticle appearance");
            }
            intro.colour_table_override.reset();
            if(!beam.source_pose.colour_warp || beam.source_pose.palette_override)
                throw std::runtime_error("EX reticle override mutated retained scene pose");
            const auto& stations=symbols.find("XHAIR2");
            const auto station=std::find_if(stations.begin(),stations.end(),[](auto address){return address<65536;});
            if(station==stations.end()) throw std::runtime_error("Missing EX reticle sprite fixture");
            beam.object.shape=static_cast<uint16_t>(*station);
            beam.source_pose.simple_scaled_sprite=true;
            const auto sprites=source_models.assemble(intro);
            if(!sprites.pending.empty() || sprites.packets.size()!=1
                || sprites.packets[0].geometry.vertex_view().empty())
                throw std::runtime_error("EX native reticle sprite produced no geometry");
            for(const auto& vertex:sprites.packets[0].geometry.vertex_view())
                if((vertex.texture[3]&134217728U)==0 || vertex.group_b[2]!=1)
                    throw std::runtime_error("EX reticle sprite lost its game-plane orientation marker");
            for(const auto* table:{"NAN_C","FIREBODY_C","BLUELAVABODY_C","STEALTH_C","TREVORTEX_C"}) {
                intro.colour_table_override=static_cast<uint16_t>(symbols.find(table).at(0));
                const auto alternate=source_models.assemble(intro);
                if(!alternate.pending.empty() || alternate.packets.size()!=1
                    || alternate.packets[0].geometry.texels!=sprites.packets[0].geometry.texels)
                    throw std::runtime_error("NAN mode replaced reticle sprite texels");
                const auto expected=sprites.packets[0].geometry.vertex_view();
                const auto actual=alternate.packets[0].geometry.vertex_view();
                if(expected.size()!=actual.size()) throw std::runtime_error("NAN mode changed reticle sprite size");
                for(size_t vertex=0;vertex<expected.size();++vertex)
                    if(expected[vertex].position!=actual[vertex].position
                        || expected[vertex].color!=actual[vertex].color
                        || expected[vertex].texture!=actual[vertex].texture
                        || expected[vertex].group_b!=actual[vertex].group_b)
                        throw std::runtime_error("NAN mode changed reticle sprite appearance/orientation");
            }
            intro.colour_table_override.reset();
            intro.flow=starfox::simulation::GameFlowState::intro;
            const auto hidden=source_models.assemble(intro);
            if(!hidden.packets.empty() || !hidden.pending.empty())
                throw std::runtime_error("EX reticle leaked into intro presentation");
            beam.object.strategy_address=symbols.find("ZACOINTRO_STRAT").at(0);
            beam.object.shape=static_cast<uint16_t>(lasers.front());
            beam.source_pose.simple_scaled_sprite=false;
            beam.source_pose.colour_warp=false;
            beam.source_pose.z=beam.source_pose.source_depth=1024;
            intro.shadows_enabled=false;
            const auto baseline=source_models.assemble_interpolated(intro,intro,.5);
            const auto closer=source_models.assemble_world_interpolated(intro,intro,.5,false,true);
            if(baseline.packets.size()!=1 || closer.packets.size()!=3
                || closer.packets[2].model[14]!=baseline.packets[0].model[14]*.375F)
                throw std::runtime_error("EX intro showcase did not use the enlarged presentation distance");
            intro.flow=starfox::simulation::GameFlowState::title;
            const auto title=source_models.assemble_world_interpolated(intro,intro,.5,false,true);
            if(title.packets.size()!=3 || title.packets[2].model[14]!=baseline.packets[0].model[14])
                throw std::runtime_error("Intro showcase adjustment leaked into title placement");
            intro.meters.extended=false;
            const auto original_title=source_models.assemble_world_interpolated(intro,intro,.5,false,true);
            if(original_title.packets.size()!=3 || original_title.packets[2].model[14]!=baseline.packets[0].model[14]*2.F)
                throw std::runtime_error("Original title ship did not retain its separate farther distance");
        }
    }
    if(vr_game.save_state()!=before_scene) throw std::runtime_error("Native model assembly changed game state");
    const auto retained_objects=retained_scene->objects;
    if(retained_scene!=scenes.previous() || retained_objects.empty()) throw std::runtime_error("Initial live scene is empty or has stale history");
    bool wrong_game_rejected=false;
    try {starfox::vr::GameFrameDriver wrong(pad_game,{},&scenes);}
    catch(const std::invalid_argument&) {wrong_game_rejected=true;}
    if(!wrong_game_rejected) throw std::runtime_error("Scene history accepted the wrong simulation");
    starfox::vr::GameFrameDriver driver(vr_game,[&](auto apu,auto msu) {
        (void)msu; // This fixture runs native SPC, never selects MSU playback.
        static_cast<void>(vr_audio.render_logic_tick(apu));return vr_audio.output_ports();
    },&scenes);
    starfox::timing::FixedStepClock raster(60);
    starfox::input::InputLatch reference_input;
    std::vector<starfox::simulation::ApuPortWrite> pending;
    unsigned audio_phase=0,total_phases=0,total_audio=0,total_logic=0;
    unsigned geometry_reuse=0;std::vector<starfox::vr::DrawPacket> previous_packets;
    std::size_t object_reuse=0,object_pairs=0;
    XrTime previous=1'000'000'000;
    starfox::vr::VrControls held;held.steer={-1,0};
    static_cast<void>(driver.advance(previous,held,true));reference_input.sample(left);
    for(unsigned frame=1;frame<=180;++frame) {
        const XrTime now=1'000'000'000+int64_t(frame)*1'000'000'000/90;
        const auto step=driver.advance(now,held,true);
        total_phases+=step.video_phases;total_audio+=step.audio_blocks;total_logic+=step.logic_ticks;
        const auto eye_scene=scenes.current();
        auto expected_eye_display=vr_game.map().ppu_state();
        if(vr_game.peek_meter_state().extended) expected_eye_display.cgram[207]=0x03e0;
        if(!eye_scene->ppu || *eye_scene->ppu!=expected_eye_display
            || eye_scene->cgram!=eye_scene->ppu->cgram)
            throw std::runtime_error("Stereo display snapshot has mixed source ticks");
        {
            const auto presented=source_models.assemble_interpolated(*scenes.previous(),*eye_scene,
                vr_game.logic_interpolation_alpha(step.raster_fraction));
            if(presented.packets.empty() || presented.packets.size()!=presented.handles.size())
                throw std::runtime_error("Interpolated live model assembly failed");
            if(!previous_packets.empty() && starfox::vr::same_draw_geometry(previous_packets,presented.packets)) ++geometry_reuse;
            if(frame>1) {
                std::vector<const starfox::vr::DrawPacket*> before,after;
                const auto visible=[](const auto& packet) {return !packet.geometry.vertex_view().empty() || !packet.geometry.line_vertices.empty();};
                for(const auto& packet:previous_packets) if(visible(packet)) before.push_back(&packet);
                for(const auto& packet:presented.packets) if(visible(packet)) after.push_back(&packet);
                object_pairs+=after.size();
                for(size_t i=0;i<std::min(before.size(),after.size());++i)
                    if(starfox::vr::same_draw_geometry(std::span<const starfox::vr::DrawPacket>(before[i],1),std::span<const starfox::vr::DrawPacket>(after[i],1))) ++object_reuse;
            }
            previous_packets=presented.packets;
        }
        const auto duplicate=driver.advance(now,{},true);
        if(scenes.current()!=eye_scene || eye_scene->revision!=total_logic)
            throw std::runtime_error("Stereo scene identity or logic revision changed");
        if(total_logic && scenes.previous()->revision+1!=eye_scene->revision)
            throw std::runtime_error("Scene history lost a source tick");
        if(eye_scene->cgram!=expected_eye_display.cgram || eye_scene->model_palette!=vr_game.palette_words())
            throw std::runtime_error("Scene palettes differ from native game");
        std::vector<starfox::simulation::ObjectHandle> expected_order;
        for(const auto handle:vr_game.draw_order()) {
            if(vr_game.objects().is_active(handle) && (vr_game.objects().at(handle).strategy_flags[3]&8U)==0)
                expected_order.push_back(handle);
        }
        if(expected_order.size()!=eye_scene->objects.size()) throw std::runtime_error("Scene draw-list size differs");
        for(size_t i=0;i<expected_order.size();++i) {
            const auto& item=eye_scene->objects[i];
            if(item.handle!=expected_order[i] || item.object!=vr_game.objects().at(item.handle)
                || item.presentation.generation!=vr_game.objects().generation(item.handle))
                throw std::runtime_error("Scene object identity, order or data differs");
            const auto& pose=item.source_pose;
            const auto& object=item.object;
            const auto& camera=eye_scene->camera;
            const auto& view=eye_scene->view_matrix;
            const double dx=starfox::simulation::wrap16(int64_t(object.world_x)-camera.x);
            const double dy=starfox::simulation::wrap16(int64_t(object.world_y)-camera.y);
            const double dz=starfox::simulation::wrap16(int64_t(object.world_z)-camera.z);
            if(pose.x!=(dx*view[0]+dy*view[3]+dz*view[6])/32768.
                || pose.y!=(dx*view[1]+dy*view[4]+dz*view[7])/32768.
                || pose.z!=(dx*view[2]+dy*view[5]+dz*view[8])/32768.)
                throw std::runtime_error("Captured pose changed source camera wrapping/projection basis");
            const auto expected_matrix=starfox::simulation::multiply_matrix_q15(item.presentation.rotation_matrix,eye_scene->view_matrix);
            if(!pose.use_rotation_matrix || !pose.use_source_lighting_state || pose.rotation_matrix!=expected_matrix
                || pose.source_lighting_matrix!=expected_matrix || pose.source_depth!=pose.z || pose.scale!=eye_scene->model_scale
                || pose.animation_frame!=((object.animation_frame&0x80U)?(object.animation_frame&0x7fU):eye_scene->game_frame)
                || pose.colour_frame!=((object.colour_frame&0x80U)?(object.colour_frame&0x7fU):eye_scene->game_frame)
                || pose.texture_scroll_x!=object.texture_scroll_x || pose.texture_scroll_y!=object.texture_scroll_y
                || pose.explosion_progress!=((object.flags&1U)?object.count:0))
                throw std::runtime_error("Captured source pose lost native object state");
        }
        if(!duplicate.duplicate || duplicate.video_phases || duplicate.logic_ticks || duplicate.audio_blocks || duplicate.raster_fraction!=step.raster_fraction)
            throw std::runtime_error("Second eye advanced gameplay or changed interpolation");
        reference_input.sample(left);
        const auto phases=raster.advance(std::chrono::nanoseconds(now-previous));previous=now;
        for(unsigned phase=0;phase<phases.simulation_steps;++phase) {
            pad_game.present_frame();
            if(pad_game.logic_tick_ready()) {
                const auto tick=pad_game.tick(reference_input.consume());
                pending.insert(pending.end(),tick.audio_port_writes.begin(),tick.audio_port_writes.end());
                static_cast<void>(pad_game.map().take_msu_register_writes());
            }
            if(++audio_phase==3) {
                static_cast<void>(pad_audio.render_logic_tick(pending));
                pad_game.synchronize_apu_output_ports(pad_audio.output_ports());pending.clear();audio_phase=0;
            }
        }
        if(frame%9==0 && vr_game.save_state()!=pad_game.save_state()) throw std::runtime_error("VR frame pacing differs from native raster loop");
    }
    if(total_phases!=120 || total_audio!=40 || total_logic!=40) throw std::runtime_error("90 Hz headset changed source cadence");
    if(!geometry_reuse) throw std::runtime_error("Live interpolation never reused native packet geometry");
    std::cout<<"Transform-only upload candidates: "<<geometry_reuse<<"/179 consecutive frame pairs\n";
    std::cout<<"Per-object upload reuse candidates: "<<object_reuse<<'/'<<object_pairs<<'\n';
    if(*retained_scene->ppu!=retained_ppu) throw std::runtime_error("Later ticks mutated retained VRAM/OAM/HDMA state");
    if(retained_scene->dust_points!=retained_dust) throw std::runtime_error("Later ticks mutated retained dust points");
    if(retained_scene->revision!=0 || retained_scene->objects.size()!=retained_objects.size())
        throw std::runtime_error("Retained scene mutated during gameplay");
    for(size_t i=0;i<retained_objects.size();++i)
        if(retained_scene->objects[i].object!=retained_objects[i].object)
            throw std::runtime_error("Retained object aliases mutable game state");
    const auto focus_scene=scenes.current();
    const auto paused_state=vr_game.save_state();
    for(unsigned interrupted_phase:{1U,2U}) {
        auto continuous=vr_game.restored_state(paused_state),interrupted=vr_game.restored_state(paused_state);
        starfox::audio::Spc700Audio continuous_audio,interrupted_audio;
        continuous_audio.load_state(vr_audio.save_state());interrupted_audio.load_state(vr_audio.save_state());
        std::vector<int16_t> expected_pcm,actual_pcm;
        unsigned expected_blocks=0,actual_blocks=0;
        const auto callback=[](auto& audio,auto& pcm,auto& blocks) {
            return [&audio,&pcm,&blocks](auto apu,auto) {
                const auto samples=audio.render_logic_tick(apu);
                pcm.insert(pcm.end(),samples.begin(),samples.end());++blocks;
                return audio.output_ports();
            };
        };
        starfox::vr::GameFrameDriver reference_driver(*continuous,callback(continuous_audio,expected_pcm,expected_blocks));
        starfox::vr::GameFrameDriver interrupted_driver(*interrupted,callback(interrupted_audio,actual_pcm,actual_blocks));
        static_cast<void>(reference_driver.advance(0,{},true));
        static_cast<void>(interrupted_driver.advance(0,{},true));
        XrTime pause_offset=0;
        for(unsigned phase=1;phase<=12;++phase) {
            const XrTime time=XrTime(phase)*16'666'667;
            static_cast<void>(reference_driver.advance(time,{},true));
            static_cast<void>(interrupted_driver.advance(time+pause_offset,{},true));
            if(phase==interrupted_phase) {
                const auto game_before=interrupted->save_state(),audio_before=interrupted_audio.save_state();
                const auto blocks_before=actual_blocks;
                starfox::vr::VrControls ignored;ignored.fire=true;ignored.menu=true;ignored.menu_pressed=true;
                static_cast<void>(interrupted_driver.advance(time+1'000'000'000,ignored,false));
                pause_offset=10'000'000'000;
                const auto resume=interrupted_driver.advance(time+pause_offset,{},true);
                if(resume.video_phases || actual_blocks!=blocks_before || interrupted->save_state()!=game_before
                    || interrupted_audio.save_state()!=audio_before)
                    throw std::runtime_error("Partial audio phase advanced while unfocused");
            }
        }
        if(actual_blocks!=4 || actual_blocks!=expected_blocks || actual_pcm!=expected_pcm
            || continuous->save_state()!=interrupted->save_state()
            || continuous_audio.save_state()!=interrupted_audio.save_state())
            throw std::runtime_error("Focus interruption lost or duplicated a partial native audio block");
    }
    static_cast<void>(driver.advance(10'000'000'000,held,false));
    const auto resumed=driver.advance(20'000'000'000,{},true);
    if(resumed.video_phases || vr_game.save_state()!=paused_state) throw std::runtime_error("Focus regain advanced paused game time");
    if(scenes.current()!=focus_scene) throw std::runtime_error("Focus resume published a false logic tick");
    if(scenes.previous()!=scenes.current()) throw std::runtime_error("Focus resume retained an old interpolation endpoint");
    const auto delayed=driver.advance(25'000'000'000,{},true);
    if(!delayed.time_clamped || delayed.video_phases!=15) throw std::runtime_error("Long frame catch-up was not bounded");
    if(scenes.current()->revision!=total_logic+delayed.logic_ticks || scenes.previous()->revision+1!=scenes.current()->revision)
        throw std::runtime_error("Catch-up scene history skipped source ticks");
    const auto before_rebase=vr_game.save_state();
    if(driver.advance(1,{},true).video_phases || vr_game.save_state()!=before_rebase) throw std::runtime_error("Rebased XR clock changed gameplay");
    starfox::vr::GameFrameDriver failing(vr_game,[](auto,auto)->std::array<uint8_t,4> {throw std::runtime_error("Injected audio failure");});
    static_cast<void>(failing.advance(0,{},true));
    bool failed=false;
    try {static_cast<void>(failing.advance(50'000'000,{},true));} catch(const std::runtime_error&) {failed=true;}
    if(!failed) throw std::runtime_error("Audio failure was swallowed");
    const auto after_failure=vr_game.save_state();failed=false;
    try {static_cast<void>(failing.advance(100'000'000,{},true));} catch(const std::runtime_error&) {failed=true;}
    if(!failed || vr_game.save_state()!=after_failure) throw std::runtime_error("Failed tick was partially replayed");
    std::cout<<"90 Hz stereo pacing: 120 rasters, 40 logic/audio ticks; duplicate eyes, focus resume and partial audio-block preservation verified\n";
    std::cout<<"Live scene snapshots: ordered objects, generations, palettes, retained ownership and catch-up history verified\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
