#include "starfox/vr/game_scene.hpp"
#include "starfox/render/grid_projection.hpp"
#include <bit>
#include <limits>
#include <stdexcept>

namespace starfox::vr {
GameSceneHistory::GameSceneHistory(const simulation::GameSimulation& game,
    const assets::RomImage& rom,const assets::SymbolMap& symbols)
    :game_(game),rom_(rom),trig_(simulation::TrigTables::load(rom,symbols)) {
    constexpr std::array names{"VIEWPOSX","VIEWPOSY","VIEWPOSZ",
        "VIEWROTXW","VIEWROTYW","VIEWROTZW","VIEWFLOATY","GAMEFRAME","PLAYERFLYMODE","SHADOWHEIGHT","BG2SCROLL","PVIEWPOSY"};
    for(size_t i=0;i<names.size();++i) {
        bool found=false;
        for(const auto address:symbols.find(names[i])) {
            if((address>>16U)==0 || (address>>16U)==0x7eU) {
                addresses_[i]=address;found=true;break;
            }
        }
        if(!found) throw std::runtime_error(std::string("Missing scene RAM symbol: ")+names[i]);
    }
    constexpr std::array tracking_names{"PLAYERONPLANET_STRAT","PLAYERINSPACE_STRAT"};
    for(size_t i=0;i<tracking_names.size();++i) {
        const auto& entries=symbols.find(tracking_names[i]);
        if(!entries.empty()) tracking_strategies_[i]=entries.front();
    }
    constexpr std::array model_names{"M_VANISHX","M_VANISHY","M_DEPTHTABLE","M_DEPTHSTAB",
        "M_WIREMODE","M_WOBBLEMODE","M_WABBLEMODE","M_CELMODE","M_SINEOFFSET","M_COLORWARP","M_PROJPNTS"};
    for(size_t i=0;i<model_names.size();++i) {
        for(const auto address:symbols.find(model_names[i])) if((address>>16U)==0x70U) {model_addresses_[i]=address;break;}
        if(i<4 && !model_addresses_[i]) throw std::runtime_error(std::string("Missing scene model symbol: ")+model_names[i]);
    }
    const auto& tables=symbols.find("DEPTHTABLES");
    if(tables.empty()) throw std::runtime_error("Missing scene depth tables");
    depth_tables_=tables.front();
    constexpr std::array dust_names{"DOTSFLAG","M_MOREDOTS","M_GRIDLINES"};
    for(size_t i=0;i<dust_names.size();++i) {
        for(const auto address:symbols.find(dust_names[i])) {
            const auto bank=address>>16;
            if((i==0 && (bank==0 || bank==0x7e)) || (i!=0 && bank==0x70)) {
                dust_addresses_[i]=address;break;
            }
        }
        if(i==0 && !dust_addresses_[i]) throw std::runtime_error("Missing scene DOTSFLAG");
    }
    const auto& title_intro=symbols.find("BG_TITLEI");
    const auto& background_lists=symbols.find("BGLISTS");
    const auto& menu_background=symbols.find("BG_TITLE");
    const auto& menu_choice=symbols.find("PGBG");
    if(!menu_choice.empty()) ex_menu_background_choice_=menu_choice.front();
    if(!menu_background.empty() && !background_lists.empty()
        && (menu_background.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        ex_menu_background_=static_cast<uint16_t>(menu_background.front()-background_lists.front());
    const auto& ex_intro_stars=symbols.find("BG_CRED");
    if(!ex_intro_stars.empty() && !background_lists.empty()
        && (ex_intro_stars.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        ex_intro_star_background_=static_cast<uint16_t>(ex_intro_stars.front()-background_lists.front());
    const auto& sector_y_stars=symbols.find("BG_2_4");
    if(!sector_y_stars.empty() && !background_lists.empty()
        && (sector_y_stars.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        sector_y_star_background_=static_cast<uint16_t>(sector_y_stars.front()-background_lists.front());
    const auto& asteroid_stars=symbols.find("BG_1_2");
    if(!asteroid_stars.empty() && !background_lists.empty()
        && (asteroid_stars.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        asteroid_star_background_=static_cast<uint16_t>(asteroid_stars.front()-background_lists.front());
    if(!title_intro.empty() && !background_lists.empty()
        && (title_intro.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        ex_title_intro_background_=static_cast<uint16_t>(title_intro.front()-background_lists.front());
    constexpr std::array unique_names{"BG_2_2","BG_3_4B","BG_3_4D","BG_INTRO","BG_1_3I","BG_3_2","BG_1_3C","BG_1_3A"};
    constexpr std::array orbital_names{"BG_1_5","BG_3_6","BG_5_1E","BG_5_1I","BG_2_2"};
    for(size_t i=0;i<orbital_names.size();++i) {
        const auto& value=symbols.find(orbital_names[i]);
        if(!value.empty() && !background_lists.empty()
            && (value.front()&0xff0000U)==(background_lists.front()&0xff0000U))
            orbital_backgrounds_[i]=static_cast<uint16_t>(value.front()-background_lists.front());
    }
    constexpr std::array ex_star_names{"BG_5_3","BG_6_3","BG_6_3H","BG_TRAINING"};
    for(size_t i=0;i<ex_star_names.size();++i) {
        const auto& value=symbols.find(ex_star_names[i]);
        if(!value.empty() && !background_lists.empty()
            && (value.front()&0xff0000U)==(background_lists.front()&0xff0000U))
            ex_star_backgrounds_[i]=static_cast<uint16_t>(value.front()-background_lists.front());
    }
    constexpr std::array landscape_names{"BG_1_1C","BG_TRAINING","BG_2_3A","BG_1_6A","BG_3_7A","BG_3_3A","BG_3_5","BG_3_1C","BG_1_4","BG_7_1","BG_7_2","BG_7_3","BG_7_4","BG_5_4","BG_5_1","BG_6_1","BG_6_5","BG_6_2","BG_6_4","BG_5_5","BG_7_5","BG_6_6","BG_5_2","BG_1_14","BG_1_7B"};
    const auto& water=symbols.find("BG_2_3B");
    const auto& colony=symbols.find("BG_2_6A");
    if(!colony.empty() && !background_lists.empty()
        && (colony.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        colony_background_=static_cast<uint16_t>(colony.front()-background_lists.front());
    if(!water.empty() && !background_lists.empty()
        && (water.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        water_background_=static_cast<uint16_t>(water.front()-background_lists.front());
    for(size_t i=0;i<landscape_names.size();++i) {
        const auto& value=symbols.find(landscape_names[i]);
        if(!value.empty() && !background_lists.empty()
            && (value.front()&0xff0000U)==(background_lists.front()&0xff0000U))
            landscape_backgrounds_[i]=static_cast<uint16_t>(value.front()-background_lists.front());
    }
    const auto& twin=symbols.find("BG_5_4");
    const auto& faces=symbols.find("BG_6_3H");
    const auto& dimension=symbols.find("BG_SPECIAL");
    const auto& hole=symbols.find("BG_HOLE");
    const auto& comet=symbols.find("BG_COMET");
    if(!comet.empty() && !background_lists.empty()
        && (comet.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        comet_background_=static_cast<uint16_t>(comet.front()-background_lists.front());
    if(!hole.empty() && !background_lists.empty()
        && (hole.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        blackhole_background_=static_cast<uint16_t>(hole.front()-background_lists.front());
    constexpr std::array vortex_names{"BG_3_7C","BG_6_6C","BG_6_6D","BG_6_6E"};
    for(size_t i=0;i<vortex_names.size();++i) {
        const auto& value=symbols.find(vortex_names[i]);
        if(!value.empty() && !background_lists.empty()
            && (value.front()&0xff0000U)==(background_lists.front()&0xff0000U))
            final_vortex_backgrounds_[i]=static_cast<uint16_t>(value.front()-background_lists.front());
    }
    if(!dimension.empty() && !background_lists.empty()
        && (dimension.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        dimension_background_=static_cast<uint16_t>(dimension.front()-background_lists.front());
    if(!faces.empty() && !background_lists.empty()
        && (faces.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        ex_face_planet_background_=static_cast<uint16_t>(faces.front()-background_lists.front());
    if(!twin.empty() && !background_lists.empty()
        && (twin.front()&0xff0000U)==(background_lists.front()&0xff0000U))
        ex_twin_planet_background_=static_cast<uint16_t>(twin.front()-background_lists.front());
    for(size_t i=0;i<unique_names.size();++i) {
        const auto& entry=symbols.find(unique_names[i]);
        if(!entry.empty() && !background_lists.empty()
            && (entry.front()&0xff0000U)==(background_lists.front()&0xff0000U))
            unique_backgrounds_[i]=static_cast<uint16_t>(entry.front()-background_lists.front());
    }
    capture();previous_=current_;
}

void GameSceneHistory::capture() {
    if(current_ && current_->revision==std::numeric_limits<uint64_t>::max())
        throw std::overflow_error("VR scene revision exhausted");
    auto next=std::make_shared<GameSceneSnapshot>();
    next->revision=current_?current_->revision+1:0;
    const auto word=[&](size_t index) {return game_.map().peek_ram_word(addresses_[index]).value();};
    next->camera={std::bit_cast<int16_t>(word(0)),std::bit_cast<int16_t>(word(1)),
        std::bit_cast<int16_t>(word(2)),word(3),word(4),word(5)};
    next->view_matrix=simulation::rotation_matrix_q15(trig_,
        std::bit_cast<int16_t>(word(3)),std::bit_cast<int16_t>(word(4)),std::bit_cast<int16_t>(word(5)));
    next->view_float_y=std::bit_cast<int16_t>(word(6));
    next->background_vertical_scroll=std::bit_cast<int16_t>(word(10));
    next->shadows_enabled=(game_.map().peek_ram_byte(addresses_[8]).value()&8U)!=0;
    next->shadow_height=std::bit_cast<int16_t>(word(9));
    next->game_frame=game_.map().peek_ram_byte(addresses_[7]).value()&0x7fU;
    next->flow=game_.flow_state();next->player=game_.player();
    next->background_colour_subtract=game_.game_over_background_subtract();
    next->particles=game_.particles().particles();
    next->model_scale=game_.model_scale_multiplier();
    next->colour_table_override=game_.model_colour_table_override();
    next->meters=game_.peek_meter_state();
    auto presentation_ppu=std::make_shared<simulation::SnesPpuState>(game_.map().ppu_state());
    // The authored final room switches to the Mode-2 abstract/vortex sky while
    // INATUNNEL can remain set. Its background is not corridor geometry: only
    // presentation drops the tunnel mask, leaving native gameplay untouched.
    const bool final_vortex_sky=presentation_ppu->background_mode==2
        && std::any_of(final_vortex_backgrounds_.begin(),final_vortex_backgrounds_.end(),
            [&](uint16_t id){return id && game_.map().background()==id;});
    if(final_vortex_sky) presentation_ppu->tunnel_scene=false;
    // The colony cross-section is authored with WATER, not INATUNNEL=1.
    // In VR it is still an enclosed center-window scene, unlike open Titania water.
    if(presentation_ppu->background_mode==1 && colony_background_
        && game_.map().background()==colony_background_) presentation_ppu->tunnel_scene=true;
    // Presentation-only reticle palette; never mutate source CGRAM. Preserve
    // the original green sprite shading, but EX's model marks need an explicit
    // bright green entry because they do not use the sprite tiles.
    constexpr std::array<std::array<unsigned,3>,8> reticle_colours{{
        {0,255,0},{255,255,255},{72,136,255},{255,64,64},
        {255,232,64},{64,240,255},{255,96,255},{255,152,48}}};
    const auto selected_colour=std::min(unsigned(game_.crosshair_colour()),7U);
    const auto tint=reticle_colours[selected_colour];
    const auto pack_colour=[](unsigned r,unsigned g,unsigned b) {
        return uint16_t((r>>3)|((g>>3)<<5)|((b>>3)<<10));
    };
    if(selected_colour) for(size_t index=193;index<208;++index) {
        const auto word=presentation_ppu->cgram[index];
        const auto intensity=std::max({unsigned(word&31),unsigned((word>>5)&31),unsigned((word>>10)&31)});
        presentation_ppu->cgram[index]=pack_colour(tint[0]*intensity/31,tint[1]*intensity/31,tint[2]*intensity/31);
    }
    if(selected_colour || next->meters.extended)
        presentation_ppu->cgram[207]=pack_colour(tint[0],tint[1],tint[2]);
    next->ppu=std::move(presentation_ppu);
    next->background_id=game_.map().background();
    next->wipe=game_.window_wipe_state();
    if(next->ppu->background_mode==2) for(size_t i=0;i<unique_backgrounds_.size();++i)
        if(unique_backgrounds_[i]!=0 && game_.map().background()==unique_backgrounds_[i])
        {
            next->background_unique_top_rows=i==0?168:i>=4?512:224;
            // BG_3_4B/D share the Meteor atlas: only stars in its left half,
            // with one large meteor in the right. Keep that artwork planar
            // and unique while the planet-free stars fill the surround.
            next->background_unique_space=i>=4 || i==1 || i==2;
            if(i==1 || i==2 || i==4 || i==6 || i==7) next->background_planet_rect={320,88,120,120};
            if(i==5) next->background_planet_rect={56,360,112,112};
        }
    next->background_scroll_override=game_.map().peek_background_scroll_override();
    next->background_water_surround=next->ppu->background_mode==1 && water_background_!=0
        && game_.map().background()==water_background_;
    // Only verified landscape atlases. Titania's water and tunnel
    // backgrounds use different projection schemes and must not join this set.
    if(next->ppu->background_mode==2) for(std::size_t i=0;i<landscape_backgrounds_.size();++i)
        if(landscape_backgrounds_[i]!=0 && game_.map().background()==landscape_backgrounds_[i]
            && !(i==1 && next->meters.extended)) {
            next->background_landscape=true;
            // The original 1-4 atlas places its unique planets in the left
            // half; its right half contains only repeatable sky and ground.
            next->background_landscape_unique_half=i==5 || i==8 || i==18 || i==23;
            next->background_landscape_unique_right_half=i==14 || i==15 || i==19 || i==20 || i==22;
            next->background_ex_ocean_island=i==17;
            next->background_ex_volcanic_horizon=i==21;
            next->background_ex_city_planets=i==22;
            // The ocean shoreline is row 352, eight pixels below the usual
            // landscape horizon. Keep its island above the flattened receiver.
            next->landscape_atlas_origin=i==24?224:(i==19 || i==20 || i==22)?248:(i==17 || i==18 || i==21)?240:i==6?(next->meters.extended?16:272):232;
            // EX's snowy entry landscapes end mountains at row 351. The
            // shared 112-row horizon must begin the receiver at row 352,
            // otherwise enhanced sky leaves eight native mountain rows below it.
            if(next->meters.extended && (i==14 || i==15)) next->landscape_atlas_origin=240;
        }
    if(next->meters.extended && next->ppu->background_mode==2
        && comet_background_!=0 && game_.map().background()==comet_background_) {
        // BG_COMET's hot horizon begins at atlas row 384; the native
        // landscape receiver uses logical row 112. No unique sky motifs.
        next->background_landscape=true;
        next->landscape_atlas_origin=272;
    }
    next->background_ex_twin_planets=next->ppu->background_mode==2
        && ex_twin_planet_background_!=0 && game_.map().background()==ex_twin_planet_background_;
    next->background_ex_face_planets=next->ppu->background_mode==2
        && ((ex_face_planet_background_!=0 && game_.map().background()==ex_face_planet_background_)
            || (dimension_background_!=0 && game_.map().background()==dimension_background_));
    // EX replaces title tiles without changing the map background ID. PGBG
    // identifies the actual native choice; preserve each verified atlas's
    // horizon and single-occurrence artwork. Keep UI layers planar.
    bool menu_pattern_sphere=false;
    bool menu_dimension_sphere=false;
    bool menu_orbital=false;
    if(next->meters.extended && next->flow==simulation::GameFlowState::ex_pregame_menu
        && ex_menu_background_!=0 && ex_menu_background_choice_!=0
        && next->background_id==ex_menu_background_) {
        const auto choice=game_.map().peek_ram_byte(ex_menu_background_choice_).value_or(0);
        std::optional<uint16_t> origin;
        bool unique_left=false,unique_right=false;
        if(next->ppu->background_mode==((choice<7 || choice==36)?2:1)) switch(choice) {
        case 0: origin=320;break; // Macbeth menu's lower horizon.
        case 1: case 3: case 4: case 5: origin=320;unique_left=true;break;
        case 7: case 8: case 22: case 26: case 34: origin=240;break;
        // Native menu 9 uses the BG_5_2 city atlas: preserve its forward
        // planet patch while repeating only the surrounding stars/city.
        case 9: origin=248;unique_right=true;next->background_ex_city_planets=true;break;
        case 10: origin=248;unique_right=true;break;
        case 11: origin=312;break;
        case 12: origin=240;unique_right=true;break;
        case 13: case 14: case 15: case 16: case 33: origin=248;break;
        case 32: origin=248;unique_left=true;break;
        case 20: origin=288;unique_left=true;break;
        case 36: origin=328;break;
        case 2: case 6: case 17: case 23: case 24: case 29: case 30: case 99:
            menu_pattern_sphere=true;break;
        case 18:
            // The native Dimension menu choice changes the tiles while keeping
            // the pre-game map ID; it cannot be recognized by BG_SPECIAL.
            menu_pattern_sphere=true;menu_dimension_sphere=true;break;
        case 19: case 27: case 28: case 31:
            next->background_unique_space=true;
            next->background_unique_top_rows=512;
            next->background_planet_rect=choice==27?std::array<unsigned,4>{56,360,112,112}
                :choice==31?std::array<unsigned,4>{88,328,72,80}:std::array<unsigned,4>{320,88,120,120};
            break;
        case 21: case 25: case 35:
            menu_orbital=true;
            next->background_orbital_planet=true;
            next->background_orbital_entry=choice==25;
            break;
        default: break;
        }
        if(origin) {
            next->background_landscape=true;
            next->background_landscape_unique_half=unique_left;
            next->background_landscape_unique_right_half=unique_right;
            next->landscape_atlas_origin=*origin;
        }
    }
    if(next->background_landscape)
        next->landscape_grid_height=next->camera.y;
    next->display_brightness=game_.map().display_brightness();
    next->background_space_horizon=!next->meters.extended
        && next->ppu->background_mode==2 && next->background_unique_top_rows==168;
    next->background_space_horizon|=next->background_unique_space || menu_orbital;
    // Verified Original/EX LSB atlases match: stars above, planet below.
    if(next->ppu->background_mode==2 && !next->ppu->tunnel_scene)
        for(const auto id:orbital_backgrounds_) if(id && id==next->background_id) {
            // EX BG_2_2 also has the row-424 horizon and the small planet in
            // 336..392 x 320..384. The entry sphere suppresses that patch and
            // emits it once separately, so both cartridges can use this path.
            next->background_space_horizon=true;
            next->background_orbital_planet=true;
            next->background_orbital_thin=id==orbital_backgrounds_[2];
            next->background_orbital_entry=id==orbital_backgrounds_[3] || id==orbital_backgrounds_[4];
        }
    const bool credit_stars=ex_intro_star_background_!=0
        && game_.map().background()==ex_intro_star_background_;
    // Original's live Black Hole also reaches Mode 1 while retaining BG_HOLE.
    // Both modes decode BG2 as 4bpp; do not drop the surround on that switch.
    const bool blackhole_sky=(next->ppu->background_mode==1 || next->ppu->background_mode==2)
        && blackhole_background_!=0
        && game_.map().background()==blackhole_background_;
    // The asteroid atlas's meteor belt is centered by its native scroll.
    // Zeroing that offset puts the belt near the sphere's vertical extremes.
    const bool asteroid_sky=asteroid_star_background_!=0
        && next->background_id==asteroid_star_background_;
    next->background_retain_sky_scroll=next->background_ex_face_planets || blackhole_sky || final_vortex_sky
        || menu_dimension_sphere || asteroid_sky;
    next->background_star_sphere=menu_pattern_sphere || next->background_retain_sky_scroll || (next->ppu->background_mode==2
        && ((credit_stars && (!next->meters.extended || next->flow==simulation::GameFlowState::intro
                || next->flow==simulation::GameFlowState::ex_pregame_menu))
            // Both cartridges' verified atlases contain repeatable stars,
            // asteroid dust or nebulae here, with no unique planet artwork.
            || (sector_y_star_background_!=0 && next->background_id==sector_y_star_background_)
            || (asteroid_star_background_!=0 && next->background_id==asteroid_star_background_)));
    next->briefing=game_.briefing_state();
    next->dialogue=game_.dialogue_state();
    next->paused=game_.paused();
    if(next->meters.extended && next->ppu->background_mode==2 && !next->ppu->tunnel_scene)
        for(const auto id:ex_star_backgrounds_) if(id && id==next->background_id)
            next->background_star_sphere=true;
    next->circle=game_.circle_effect_state();
    next->ex_title_logo_screen=next->meters.extended
        && next->flow==simulation::GameFlowState::title
        && ex_title_intro_background_!=0
        && game_.map().background()==ex_title_intro_background_;
    next->dust_points=game_.dust().points();
    next->dots_mode=std::bit_cast<int8_t>(game_.map().peek_ram_byte(dust_addresses_[0]).value());
    if(next->meters.extended) {
        if(dust_addresses_[1] && game_.map().peek_ram_word(dust_addresses_[1]).value()!=0)
            next->dust_point_count=simulation::kMaximumDustPoints;
        next->grid_lines=dust_addresses_[2] && game_.map().peek_ram_word(dust_addresses_[2]).value()!=0;
    }
    // Direct-ROM diagnostics do not pass through the desktop experience menu.
    // Meter decoding already identifies the loaded cartridge, not a UI default.
    next->native_ex_bitmap=next->meters.extended
        && (next->flow==simulation::GameFlowState::gameplay
            || next->flow==simulation::GameFlowState::training
            || next->flow==simulation::GameFlowState::ex_pregame_menu
            || next->flow==simulation::GameFlowState::intro
            || next->flow==simulation::GameFlowState::stage_results);
    next->model_palette=game_.palette_words();next->cgram=next->ppu->cgram;
    next->transforms=render::capture_object_snapshots(game_.objects(),trig_);
    if(const auto player=next->transforms.find(next->player);player!=next->transforms.end()) {
        const auto strategy=player->second.strategy_address;
        if(strategy && (strategy==tracking_strategies_[0] || strategy==tracking_strategies_[1])) {
            // Normal source flight follows only a fraction of player Y.
            // Remove that deliberate screen drift from the shared VR camera,
            // preserving shake, camera orbit and every scripted strategy.
            const int correction=int(player->second.transform.y)-std::bit_cast<int16_t>(word(11));
            next->camera.y=simulation::wrap16(int(next->camera.y)+correction);
            if(next->background_landscape) next->landscape_grid_height=next->camera.y;
        }
    }
    const auto model_word=[&](size_t i) {return model_addresses_[i]?game_.map().peek_ram_word(model_addresses_[i]).value():uint16_t{};};
    const auto model_byte=[&](size_t i) {return model_addresses_[i]?game_.map().peek_ram_byte(model_addresses_[i]).value():uint8_t{};};
    render::RenderPose common;
    common.vanish_x=std::bit_cast<int16_t>(model_word(0));common.vanish_y=std::bit_cast<int16_t>(model_word(1));
    next->source_vanishing_point={static_cast<int16_t>(common.vanish_x),static_cast<int16_t>(common.vanish_y)};
    common.scale=next->model_scale;
    common.wireframe_mode=model_byte(4);common.wobble_mode=model_byte(5);
    common.wave_mode=model_byte(6)!=0;common.cel_mode=model_byte(7)!=0;
    common.wave_offset=std::bit_cast<int16_t>(model_word(8));common.colour_warp=model_word(9)!=0;
    if(model_addresses_[10]) common.projected_points_address=static_cast<uint16_t>(model_addresses_[10]);
    const auto thresholds=model_word(2),colours=model_word(3);
    next->objects.reserve(game_.draw_order().size());
    for(const auto handle:game_.draw_order()) {
        if(!game_.objects().is_active(handle)) continue;
        const auto pose=next->transforms.find(handle);
        if(pose==next->transforms.end()) continue; // Source invisible flag.
        const auto& object=game_.objects().at(handle);
        auto source=common;
        const auto& transform=pose->second.transform;
        const double x=simulation::wrap16(int64_t(transform.x)-next->camera.x);
        const double y=simulation::wrap16(int64_t(transform.y)-next->camera.y);
        const double z=simulation::wrap16(int64_t(transform.z)-next->camera.z);
        const auto& m=next->view_matrix;
        source.x=(x*m[0]+y*m[3]+z*m[6])/32768.;
        source.y=(x*m[1]+y*m[4]+z*m[7])/32768.;
        source.z=(x*m[2]+y*m[5]+z*m[8])/32768.;
        source.pitch=double(transform.pitch)-next->camera.pitch;
        source.yaw=double(transform.yaw)-next->camera.yaw;
        source.roll=double(transform.roll)-next->camera.roll;
        source.rotation_matrix=simulation::multiply_matrix_q15(pose->second.rotation_matrix,m);
        source.use_rotation_matrix=true;
        source.source_lighting_matrix=source.rotation_matrix;source.source_depth=source.z;
        source.use_source_lighting_state=true;
        const auto frame=[&](uint8_t value) {return (value&0x80U)?uint32_t(value&0x7fU):uint32_t(next->game_frame);};
        source.animation_frame=frame(object.animation_frame);source.colour_frame=frame(object.colour_frame);
        source.texture_scroll_x=object.texture_scroll_x;source.texture_scroll_y=object.texture_scroll_y;
        source.explosion_progress=(object.flags&1U)?object.count:0;
        source.simple_scaled_sprite=(object.strategy_flags[0]&0x20U)!=0; // Size needs decoded shape header.
        render::apply_source_depth_tables(rom_,depth_tables_,thresholds,colours,object.extended[21],source);
        next->objects.push_back({handle,object,pose->second,source});
    }
    auto grid_history=grid_line_history_;
    if(next->grid_lines && next->dots_mode>0
        && next->flow!=simulation::GameFlowState::planet_select
        && next->flow!=simulation::GameFlowState::planet_travel
        && next->flow!=simulation::GameFlowState::continue_choice) {
        const auto frame=grid_history.begin(next->revision);
        next->grid_line_start=frame.start;
        const auto projected=render::project_source_grid(
            timing::interpolate(next->camera,next->camera,1.),next->view_matrix,224,192);
        auto endpoint=frame.start;
        if(projected.count) {
            const auto& last=projected.points[projected.count-1];
            endpoint={static_cast<int16_t>(last.x-1),last.y};
        }
        grid_history.finish(frame,endpoint);
    }
    previous_=current_;current_=std::move(next);grid_line_history_=grid_history;
}
}
