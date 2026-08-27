#include "starfox/simulation/game_simulation.hpp"

#include "starfox/assets/decrunch.hpp"
#include "starfox/input/buttons.hpp"

#include <algorithm>
#include <cctype>
#include <bit>
#include <stdexcept>

namespace starfox::simulation {
namespace {

constexpr std::array<std::uint16_t, 7> kPresentationRates{
    20U, 30U, 60U, 90U, 120U, 240U, 360U};
constexpr std::array<DisplayMode, 4> kDisplayModes{
    DisplayMode::standard_4_3,
    DisplayMode::widescreen_16_9,
    DisplayMode::ultrawide_21_9,
    DisplayMode::super_ultrawide_32_9,
};

std::int16_t signed_word(std::uint16_t value) noexcept {
    return std::bit_cast<std::int16_t>(value);
}

} // namespace

GameSimulation::GameSimulation(
    const assets::RomImage& rom,
    const assets::SymbolMap& symbols,
    const std::string& initial_map)
    : rom_(&rom),
      symbols_(&symbols),
      map_(rom, MapDatabase{rom, symbols}, objects_, &symbols),
      strategies_(symbols, objects_, map_),
      trigonometry_(TrigTables::load(rom, symbols)),
      particles_(rom, symbols),
      internal_player_pointer_(ram_symbol("INTERNALPLAYPT")),
      controller_high_(ram_symbol("CONT0")),
      controller_low_(ram_symbol("CONTL0")),
      previous_controller_high_(ram_symbol("CONT0L")),
      previous_controller_low_(ram_symbol("CONTL0L")),
      last_controller_high_(ram_symbol("LASTCONT0")),
      last_controller_low_(ram_symbol("LASTCONTL0")),
      trigger_(ram_symbol("TRIG0")),
      hardware_controller_(ram_symbol("JOY1L")),
      game_palette_(ram_symbol("GAMEPALBUFF")),
      sound_read_(ram_symbol("SDGPT3")),
      sound_write_(ram_symbol("SDSPT3")),
      sound_buffer_(ram_symbol("SDPORT3")),
      sound_pending_(ram_symbol("SDPCK3")),
      pause_sound_(ram_symbol("PAUSESND")),
      single_step_(ram_symbol("SINGLESTEP")),
      player_ship_flags_(ram_symbol("PSHIPFLAGS")),
      player_ship_flags_3_(ram_symbol("PSHIPFLAGS3")),
      special_weapon_count_(ram_symbol("SPECWEPCNT")),
      special_weapon_delay_(ram_symbol("SPECIALDELAY")),
      boss_flags_(ram_symbol("BOSSFLAGS")),
      player_strategy_flags_(ram_symbol("PSTRATFLAGS")),
      doing_wipe_(ram_symbol("DOINGWIPE")),
      stay_black_(ram_symbol("STAYBLACK")),
      background_music_count_(ram_symbol("BGMCNT")),
      background_music_command_(ram_symbol("BGM_MUSIC")),
      background_flags_(ram_symbol("BGFLAGS")),
      calculate_background_scroll_(rom_symbol("CALCBGSCROLL_L")),
      calculate_background_vertical_offsets_(rom_symbol("CALCBG2VOFFSETS_L")),
      upload_background_vertical_offsets_(rom_symbol("DMABG2VOFFSETS_L")),
      vertical_offsets_enabled_(ram_symbol("DOVOFS")),
      calculate_background_horizontal_offsets_(rom_symbol("DO_HPOSITIONS_L")),
      upload_background_horizontal_offsets_(rom_symbol("DMAHPOS_L")),
      horizontal_offsets_enabled_(ram_symbol("DOHOFS")),
      horizontal_offsets_buffer_(ram_symbol("HDMABG2HOFS2")),
      do_sounds_(rom_symbol("DOSOUNDS_L")),
      set_black_(rom_symbol("SETBLACK_L")),
      update_objects_(rom_symbol("UPDATE_OBJECTS_L")),
      palette_goto_(rom_symbol("PALGOTO_L")),
      fade_palette_(rom_symbol("FADEPALTO_L")),
      do_sprites_(rom_symbol("DO_SPRITES_L")),
      do_circle_explosion_(rom_symbol("DO_CIRCLE_EXPLOSION_L")),
      friends_messages_(rom_symbol("FRIENDS_MESSAGES_L")),
      generate_collision_list_(rom_symbol("GENERATE_COLLIST_L")),
      resolve_collisions_(ram_symbol("INIT_STRATS_RAM_L")),
      restart_(rom_symbol("RESTART_L")),
      remove_dead_(rom_symbol("REMOVEDEADAL_L")),
      game_flags_(ram_symbol("GAMEFLAGS")),
      particles_enabled_(ram_symbol("M_PARTICLESON")),
      do_background_request_(rom_symbol("DOBGREQ_L")),
      set_background_info_request_(rom_symbol("SETBGINFOREQ_L")),
      level_finished_(ram_symbol("LEVELFINISHED")),
      stage_(ram_symbol("STAGE")),
      routes_(ram_symbol("ROUTES")),
      which_route_(ram_symbol("WHICHROUTE")),
      current_planet_(ram_symbol("CURRENTPLANET")),
      current_level_(ram_symbol("CURRENTLEVEL")),
      new_map_(ram_symbol("NEWMAP")),
      pepper_message_(ram_symbol("PEPPERMSG")),
      stage_paths_(rom_symbol("STAGEPATHS")),
      initialize_game_(rom_symbol("INITGAME_L")),
      initialize_all_(rom_symbol("INITIALISE_L")),
      controls_map_(rom_symbol("CONTMAP")),
      training_map_(rom_symbol("TRAININGMAP")),
      initialize_planets_(rom_symbol("INITPLANETS_L")),
      setup_planets_(rom_symbol("SETUP_PLANETS_L")),
      setup_planet_palette_(rom_symbol("SETUPPLANETPAL_L")),
      copy_planet_light_(rom_symbol("COPYLIGHT")),
      draw_planet_sprites_(rom_symbol("DRAWPLANETSPRITES")),
      draw_selected_planet_(rom_symbol("DRAWSELECTEDPLANET")),
      draw_planet_in_centre_(rom_symbol("DRAWPLANETINCENTRE")),
      clear_planet_screen_(rom_symbol("CLEARSCREEN")),
      dma_planet_screen_(rom_symbol("DMA256SCREEN")),
      switch_planet_buffer_(rom_symbol("SWITCHBUFFER_FAST")),
      draw_route_name_(rom_symbol("DRAWROUTENAME")),
      draw_planet_lines_(rom_symbol("DRAWPLANETLINES_L")),
      undraw_planet_lines_(rom_symbol("UNDRAWPLANETLINES_L")),
      move_ship_along_path_(rom_symbol("MOVESHIPALONGPATH")),
      start_planet_positions_(rom_symbol("STARTPLANETPOS")),
      planet_object_characters_(rom_symbol("MYCRAPCHARS")),
      ship_position_(ram_symbol("SHIPXY")),
      new_ship_position_(ram_symbol("NEWSHIPXY")),
      flash_ship_(ram_symbol("FLASHSHIP")),
      ship_angle_(ram_symbol("SHIPANGLE")),
      route_x_(ram_symbol("X1")),
      light_x_(ram_symbol("LIGHTX")),
      light_y_(ram_symbol("LIGHTY")),
      light_z_(ram_symbol("LIGHTZ")),
      planet_light_x_(ram_symbol("M_LXPOS")),
      planet_light_y_(ram_symbol("M_LYPOS")),
      planet_light_z_(ram_symbol("M_LZPOS")),
      planet_sprite_palette_(ram_symbol("MSPR_PAL")),
      controls_sprites_(rom_symbol("CONTSPRITES")),
      set_control_type_(rom_symbol("SET_C_TYPE")),
      reset_sprites_(rom_symbol("RESET_SPRITES_L")),
      controls_exit_(ram_symbol("CONTEXIT")),
      control_type_(ram_symbol("C_TYPE")),
      default_training_(ram_symbol("DEFAULTTRAIN")),
      lives_(ram_symbol("LIVES")),
      sprite_position_(ram_symbol("SPRITESPOS")),
      sprite_block_(ram_symbol("SPRITEBLK")),
      object_2_characters_(rom_symbol("OBJ2CCR")),
      object_2_palette_(ram_symbol("OBJ2PAC")),
      vanish_x_(ram_symbol("M_VANISHX")),
      vanish_y_(ram_symbol("M_VANISHY")),
      route_change_1_(rom_symbol("ROUTECHANGE1_L")),
      route_change_black_hole_1_(rom_symbol("ROUTECHANGEBHOLE1_L")),
      route_change_black_hole_2_(rom_symbol("ROUTECHANGEBHOLE2_L")),
      route_change_black_hole_3_(rom_symbol("ROUTECHANGEBHOLE3_L")),
      game_over_initialize_(rom_symbol("GAMEOVERINIT_L")),
      game_over_background_(rom_symbol("BG_GAMEOVER_1")),
      title_map_(rom_symbol("TITLEMAP")),
      intro_map_(rom_symbol("INTROMAP")),
      initialize_music_(rom_symbol("DO_BGM_INIT")),
      intro_music_(rom_symbol("DO_BGM_INTRO")),
      controls_music_(rom_symbol("DO_BGM_OPS")),
      title_music_(rom_symbol("DO_BGM_TITLE")),
      map_music_(rom_symbol("DO_BGM_MAP")),
      exit_intro_(ram_symbol("EXITINTRO")),
      once_wipe_(ram_symbol("ONCEWIPE")),
      set_charmap_fox_(rom_symbol("SETCHARMAPFOX_L")),
      clear_sprites_(rom_symbol("CLEARSPRITES_L")),
      fox_sprites_(rom_symbol("FOX_SPRITES_L")),
      continue_music_(rom_symbol("DO_BGM_CONTINUE")),
      foxy_option_(ram_symbol("FOXY_OPTION")),
      foxy_frame_(ram_symbol("FOXY_FRAME")),
      foxy_foot_(ram_symbol("FOXY_FOOT")),
      bg_fox_palette_(ram_symbol("BGFOXPAC")),
      bg_fox_characters_(rom_symbol("BGFOXCCR")),
      bg_fox_tilemap_(rom_symbol("BGFOXPCR")),
      fox_object_characters_(rom_symbol("FOBJCCR")),
      fox_shape_(static_cast<std::uint16_t>(ram_symbol("MY_DEMO"))),
      vchr_logical_background_(static_cast<std::uint16_t>(
          ram_symbol("VCHR_LOGBACK"))),
      vchr_physical_background_(static_cast<std::uint16_t>(
          ram_symbol("VCHR_PHYSBACK"))),
      vsc_base_2_(static_cast<std::uint16_t>(ram_symbol("VSC_BASE2"))),
      vobj_base_(static_cast<std::uint16_t>(ram_symbol("VOBJ_BASE"))),
      credits_map_(rom_symbol("CREDITSMAP")),
      previous_view_position_(ram_symbol("PVIEWPOSX")),
      view_position_(ram_symbol("VIEWPOSX")),
      view_shake_(ram_symbol("VIEWSHAKEX")),
      view_float_(ram_symbol("VIEWFLOATX")),
      previous_view_z_offset_(ram_symbol("PVIEWPOSZOFF")),
      view_type_(ram_symbol("VIEWTYPE")),
      no_x_rotation_(ram_symbol("NOXROT")),
      output_rotation_(ram_symbol("OUTVX")),
      output_distance_(ram_symbol("OUTDIST")),
      player_turn_rotation_(ram_symbol("PLAYER_TURNROT")),
      player_roll_(ram_symbol("PLROTZ")),
      do_z_rotation_(ram_symbol("DOZROT")),
      view_rotation_(ram_symbol("VIEWROTXW")),
      matrix_(ram_symbol("MAT11W")),
      world_matrix_(ram_symbol("WMAT11")),
      view_to_object_(ram_symbol("VIEWTOOBJ")),
      view_point_(ram_symbol("VIEWPT")),
      view_block_(static_cast<std::uint16_t>(ram_symbol("VIEWBLK"))),
      secondary_player_fly_mode_(ram_symbol("SPLAYERFLYMODE")),
      crosshair_x_(ram_symbol("ARSEBANDX")),
      crosshair_y_(ram_symbol("ARSEBANDY")),
      x_angle_(rom_symbol("XANGLEXY_L")),
      y_angle_(rom_symbol("YANGLEXY_L")),
      player_collision_box_(ram_symbol("PCBOXOBJ_B")),
      player_left_wing_collision_box_(ram_symbol("PCBOXOBJ_LW")),
      player_right_wing_collision_box_(ram_symbol("PCBOXOBJ_RW")),
      shield_up_(ram_symbol("SHIELDUP")),
      boost_count_(ram_symbol("BOOSTCNT")),
      meter_damage_(ram_symbol("M_DAMAGE")),
      meter_boost_(ram_symbol("M_BOOSTANIM")),
      meter_shield_up_(ram_symbol("M_SHIELDUP")),
      meters_enabled_(ram_symbol("M_METERS")),
      boss_health_(ram_symbol("M_BOSSHP")),
      boss_max_health_(ram_symbol("M_BOSSMAXHP")),
      circle_animation_(ram_symbol("CIRCLEANIM")),
      circle_object_(ram_symbol("CIRCLEOBJ")),
      circle_radius_(ram_symbol("CIRCLERAD")),
      circle_source_blue_(ram_symbol("CIRCLESRCBLUE")),
      circle_source_green_(ram_symbol("CIRCLESRCGREEN")),
      circle_source_red_(ram_symbol("CIRCLESRCRED")),
      circle_affected_layers_(ram_symbol("CIRCLEAFF")),
      circle_centre_x_(ram_symbol("M_BIGX")),
      circle_centre_y_(ram_symbol("M_BIGY")),
      friends_message_(ram_symbol("FRIENDS_MSG")),
      message_count_1_(ram_symbol("MSG_COUNT1")),
      message_count_2_(ram_symbol("MSG_COUNT2")),
      which_friend_(ram_symbol("WHICHFRIEND")),
      face_pointer_(ram_symbol("M_FACEPTR")),
      face_data_(rom_symbol("FACEDATA")),
      messages_(rom_symbol("MESSAGES")),
      player_score_(ram_symbol("PLAYERSCORE")),
      special_object_total_(ram_symbol("SPECIALOBJTOTAL")),
      specials_dead_(ram_symbol("SPECIALS_DEAD")),
      peppy_health_(ram_symbol("BUNNY")),
      falco_health_(ram_symbol("COCK")),
      slippy_health_(ram_symbol("FROG")),
      percentage_buffer_(ram_symbol("SPECBUF")),
      percentage_pointer_(ram_symbol("SPECPTR")),
      planet_names_(rom_symbol("PLANETNAMES")),
      dog_characters_(rom_symbol("DOGCCR")),
      dog_tilemap_(rom_symbol("DOGPCR")),
      planet_sprites_(rom_symbol("PLANETSPRS")),
      planet_positions_(rom_symbol("PLANETPOS")),
      planet_radius_(ram_symbol("M_RADIUS")),
      planet_rotation_y_(ram_symbol("M_ROTY")),
      planet_rotation_table_(ram_symbol("ROTY1")),
      video_frame_counter_(ram_symbol("FRAMEC")),
      previous_video_frame_count_(ram_symbol("FRAMER")),
      strategy_frame_rate_(ram_symbol("FRAMERATE")),
      frame_count_(ram_symbol("FRAMECOUNT")),
      rendered_frame_count_(ram_symbol("FRAMES")),
      measured_frame_rate_(ram_symbol("FRAMESB")),
      nuke_shape_(static_cast<std::uint16_t>(rom_symbol("NUKE"))),
      null_shape_(static_cast<std::uint16_t>(rom_symbol("NULLSHAPE"))),
      nuke_explosion_strategy_(rom_symbol("NUKEEXP_STRAT")),
      god_nuke_protected_shapes_{
          static_cast<std::uint16_t>(rom_symbol("BOSS_2_0")),
          static_cast<std::uint16_t>(rom_symbol("BOSS_2_1")),
          static_cast<std::uint16_t>(rom_symbol("BOSS_2_2")),
          static_cast<std::uint16_t>(rom_symbol("BOSS_2_3")),
          static_cast<std::uint16_t>(rom_symbol("BOSS_2_4")),
          static_cast<std::uint16_t>(rom_symbol("BOSS_2_5")),
          static_cast<std::uint16_t>(rom_symbol("ANDROSS")),
          static_cast<std::uint16_t>(rom_symbol("ANDROSSCUBE")),
      } {
    // BOOTNMI.ASM copies the original WRAM-resident IRQ, SuperFX launch and
    // collision routines before gameplay. Native background initializers
    // call RUNMARIO_L inside this block, so reproduce the boot copy rather
    // than substituting a host stub.
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("COPY_TO_0101_L"), registers, 5'000'000);

    // Run the complete persistent game initialization. The compatibility
    // layer implements MDECRU.MC against source ROM bytes, so this also
    // installs the original packed background palettes in bank $7f.
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("INITIALISE_L"), registers, 5'000'000);

    // BOOTNMI sets this before entering any title/game sequence. This direct
    // gameplay host skips that outer loop, so preserve the same first sound
    // download reset semantics explicitly.
    map_.write_native_byte(ram_symbol("FIRSTDNLD"), 1U);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(initialize_music_, registers, 20'000'000);

    // INITSREEN_L normally installs the shared OBJ sheet and its eight
    // palettes before entering INITGAME_L. Run that self-contained portion
    // here; the generic SNES DMA model captures its VRAM/OAM writes.
    map_.write_native_byte(ram_symbol("OBJSEL"), 3U);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("INIT_SPRITES_L"), registers, 5'000'000, true);
    const auto sprite_palette_addresses = symbols.find("SPRITEPAL");
    if (sprite_palette_addresses.empty()) {
        throw std::runtime_error{"missing game RAM symbol: SPRITEPAL"};
    }
    std::array<std::uint16_t, 128> sprite_palette{};
    for (std::size_t index = 0; index < sprite_palette.size(); ++index) {
        sprite_palette[index] = map_.read_native_word(
            sprite_palette_addresses.front() + static_cast<std::uint32_t>(index * 2U));
    }
    map_.write_cgram(128U, sprite_palette);

    // The outer game bootstrap normally establishes the shared 224x192 3D
    // viewport immediately before INIT3D1. The direct gameplay host bypasses
    // that jump, so run the original setters explicitly instead of leaving
    // the CPU and Super FX vanish/clip fields at their zero-filled defaults.
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("GAMECLIPWINDOW_L"), registers);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("INITMARIO3D_L"), registers, 5'000'000);

    // IRQSETMODE1/2 establishes the normal Super FX depth thresholds before
    // any title, controls or gameplay object is drawn. The desktop host does
    // not execute the outer NMI/IRQ transfer loop, so mirror that one source
    // write here. Background requests remain free to replace it with their
    // tunnel, mist or stage-specific table later.
    constexpr std::uint16_t normal_depth_table_offset = 4U * 4U;
    map_.write_native_word(ram_symbol("M_DEPTHTABLE"),
        static_cast<std::uint16_t>(
            rom_symbol("DEPTHTABLES") + normal_depth_table_offset));

    // MAIN.ASM initializes the strategy heap immediately after formatting the
    // alien list and before MAPP creates the player objects. PATH triggers and
    // virtual stacks both use this allocator, so preserving this ordering is
    // required to keep their independently allocated blocks disjoint.
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("INITMEM_L"), registers);

    map_.start(rom_symbol("MAPP"), 0);
    map_.advance_distance(1);
    if (!map_.ended() || objects_.active_count() != 4) {
        throw std::runtime_error{"original player map did not create four objects"};
    }
    player_ = objects_.first_active();
    const auto player_pointer = native_pointer(player_);
    map_.write_native_word(ram_symbol("PLAYPT"), player_pointer);
    map_.write_native_word(internal_player_pointer_, player_pointer);

    registers = {};
    registers.x = player_pointer;
    registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("INITGAME_STRATS_L"), registers, 5'000'000);
    if (objects_.active_count() < 5 || map_.read_native_word(ram_symbol("DUMMYOBJ")) == 0) {
        throw std::runtime_error{"original strategy initialization did not create its dummy object"};
    }
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("SETGAMEPAL_L"), registers);
    std::string initial_upper = initial_map;
    for (auto& character : initial_upper) {
        character = static_cast<char>(std::toupper(
            static_cast<unsigned char>(character)));
    }
    // BOOT and PLANETSELECT are host flow entry points rather than ROM map
    // labels. Menu maps also go through the same INITGAME_L wrappers used by
    // BOOTNMI/ENDSEQ instead of the direct gameplay-map diagnostic path.
    const auto native_flow_entry = initial_upper == "BOOT"
        || initial_upper == "PLANETSELECT"
        || initial_upper == "TITLEMAP"
        || initial_upper == "INTROMAP"
        || initial_upper == "CONTMAP";
    if (!native_flow_entry) {
        start_map(initial_map);
        configure_route_for_map(initial_map);
    }
    // INITGAME_L clears this after installing the player and level maps. The
    // constructor performs those same steps separately so its MAPP bytecode
    // cannot leave the player-map terminator looking like a completed level.
    map_.write_native_word(level_finished_, 0U);

    // The native host presents three NTSC frames for every deterministic
    // strategy update. TRANS.ASM carries the completed transfer's NMI count
    // into the following update; seed that pipeline exactly for direct entry
    // into gameplay, which skips the planet/menu transfer loop.
    map_.write_native_byte(video_frame_counter_, 3U);
    map_.write_native_byte(strategy_frame_rate_, 3U);

    // INITSCREEN_L normally creates and double-buffers these Mode 2 HDMA
    // tables. Direct gameplay entry only installs its self-contained sprite
    // portion, so reproduce the two pointer writes from MAIN.ASM here.
    map_.write_native_word(ram_symbol("HDMABG2HOFS1"),
        static_cast<std::uint16_t>(ram_symbol("XHDMA_BG2HOFS1")));
    map_.write_native_word(horizontal_offsets_buffer_,
        static_cast<std::uint16_t>(ram_symbol("XHDMA_BG2HOFS2")));
    draw_order_ = objects_.active_handles();
    if (initial_upper == "BOOT") {
        enter_pregame_menu();
    } else if (initial_upper == "INTROMAP") {
        enter_intro();
    } else if (initial_upper == "PLANETSELECT") {
        start_initial_route();
    } else if (initial_upper == "TITLEMAP") {
        enter_title();
    } else if (initial_upper == "CONTMAP") {
        enter_controls(GameFlowState::controls_type);
    }
}

void GameSimulation::enter_pregame_menu() {
    paused_ = false;
    draw_order_.clear();
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_word(level_finished_, 0U);
    map_.write_native_byte(doing_wipe_, 0U);
    std::array<std::uint16_t, 16> menu_palette{};
    menu_palette[1] = 0x14a5U;
    menu_palette[4] = 0x7d20U;
    menu_palette[7] = 0x56b5U;
    menu_palette[10] = 0x03ffU;
    menu_palette[13] = 0x6318U;
    menu_palette[14] = 0x7fffU;
    map_.write_cgram(7U * 16U, menu_palette);
    map_.set_display_brightness(15U);
    timing_mode_ = TimingMode::unlocked_20_fps;
    display_mode_ = DisplayMode::standard_4_3;
    presentation_fps_ = 60U;
    pregame_selection_ = 0U;
    pregame_page_ = PregamePage::main;
    god_mode_ = false;
    armed_god_nukes_.clear();
    flow_ticks_ = 0U;
    frontend_frames_ = 0U;
    frontend_phase_ = FrontendPhase::none;
    flow_state_ = GameFlowState::pregame_menu;
    ++scene_revision_;
}

GameTickResult GameSimulation::tick_pregame_menu(
    const input::TickInput& input) {
    constexpr std::uint32_t spc_clocks_per_tick = 1'024'000U / 20U;
    constexpr std::uint8_t video_phases_per_tick = 3U;
    write_input(input);
    GameTickResult result;
    for (std::size_t phase = 0; phase < video_phases_per_tick; ++phase) {
        map_.set_apu_clock_offset(static_cast<std::uint32_t>(
            phase * spc_clocks_per_tick / video_phases_per_tick));
        service_audio_irq(result.sound_effect_commands);
    }
    ++flow_ticks_;
    if (frontend_phase_ == FrontendPhase::pregame_fade_to_intro) {
        if (map_.fade_direction() == 0 && map_.display_brightness() == 0U) {
            enter_intro();
        }
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }

    const auto previous_selection = pregame_selection_;
    const auto selection_count = pregame_page_ == PregamePage::main
        ? std::uint8_t{6U} : std::uint8_t{2U};
    if ((input.pressed & starfox::input::up) != 0U) {
        pregame_selection_ = static_cast<std::uint8_t>(
            (pregame_selection_ + selection_count - 1U) % selection_count);
    } else if ((input.pressed & starfox::input::down) != 0U) {
        pregame_selection_ = static_cast<std::uint8_t>(
            (pregame_selection_ + 1U) % selection_count);
    }
    if (pregame_selection_ != previous_selection) queue_sound_effect(0x11U);

    if (pregame_page_ == PregamePage::options) {
        const auto go_back = (input.pressed & starfox::input::b) != 0U
            || (pregame_selection_ == 1U
                && (input.pressed & (starfox::input::a
                    | starfox::input::select)) != 0U);
        if (go_back) {
            pregame_page_ = PregamePage::main;
            pregame_selection_ = 4U;
            queue_sound_effect(0x11U);
        } else if (pregame_selection_ == 0U
                   && (input.pressed & (starfox::input::left
                       | starfox::input::right | starfox::input::select
                       | starfox::input::a)) != 0U) {
            god_mode_ = !god_mode_;
            queue_sound_effect(0x11U);
        }
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }

    const auto change_timing = pregame_selection_ == 0U
        && (input.pressed & (starfox::input::left | starfox::input::right
            | starfox::input::select | starfox::input::a
            | starfox::input::b)) != 0U;
    if (change_timing) {
        timing_mode_ = timing_mode_ == TimingMode::unlocked_20_fps
            ? TimingMode::original_speed : TimingMode::unlocked_20_fps;
        queue_sound_effect(0x11U);
    }

    const auto change_presentation = pregame_selection_ == 1U
        && (input.pressed & (starfox::input::left | starfox::input::right
            | starfox::input::select | starfox::input::a
            | starfox::input::b)) != 0U;
    if (change_presentation) {
        const auto found = std::find(
            kPresentationRates.begin(), kPresentationRates.end(),
            presentation_fps_);
        auto index = found == kPresentationRates.end()
            ? std::size_t{2U}
            : static_cast<std::size_t>(
                std::distance(kPresentationRates.begin(), found));
        if ((input.pressed & starfox::input::left) != 0U) {
            index = (index + kPresentationRates.size() - 1U)
                % kPresentationRates.size();
        } else {
            index = (index + 1U) % kPresentationRates.size();
        }
        presentation_fps_ = kPresentationRates[index];
        queue_sound_effect(0x11U);
    }

    const auto change_display = pregame_selection_ == 2U
        && (input.pressed & (starfox::input::left | starfox::input::right
            | starfox::input::select | starfox::input::a
            | starfox::input::b)) != 0U;
    if (change_display) {
        const auto found = std::find(
            kDisplayModes.begin(), kDisplayModes.end(), display_mode_);
        auto index = found == kDisplayModes.end()
            ? std::size_t{}
            : static_cast<std::size_t>(
                std::distance(kDisplayModes.begin(), found));
        if ((input.pressed & starfox::input::left) != 0U) {
            index = (index + kDisplayModes.size() - 1U)
                % kDisplayModes.size();
        } else {
            index = (index + 1U) % kDisplayModes.size();
        }
        display_mode_ = kDisplayModes[index];
        queue_sound_effect(0x11U);
    }

    const auto open_options = pregame_selection_ == 4U
        && (input.pressed & (starfox::input::a | starfox::input::b)) != 0U;
    if (open_options) {
        pregame_page_ = PregamePage::options;
        pregame_selection_ = 0U;
        queue_sound_effect(0x11U);
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }

    const auto start_pressed = (input.pressed & starfox::input::start) != 0U;
    const auto confirm_start = pregame_selection_ == 5U
        && (input.pressed & (starfox::input::a | starfox::input::b)) != 0U;
    if (start_pressed || confirm_start) {
        queue_sound_effect(0x10U);
        map_.start_display_fade(-1);
        frontend_frames_ = 0U;
        frontend_phase_ = FrontendPhase::pregame_fade_to_intro;
    }
    result.audio_port_writes = map_.take_apu_port_writes();
    return result;
}

void GameSimulation::apply_god_mode_state() {
    if (!god_mode_
        || (flow_state_ != GameFlowState::gameplay
            && flow_state_ != GameFlowState::training)) {
        return;
    }

    // Star Fox EX's PSF3_NOCOLLISIONS flag is already honored by the retail
    // player strategies. Reassert it at the strategy boundary because map
    // transitions and player initialization legitimately clear PSHIPFLAGS3.
    map_.write_native_byte(player_ship_flags_3_, static_cast<std::uint8_t>(
        map_.read_native_byte(player_ship_flags_3_) | 0x08U));

    // A newly initialized ship normally starts with three Nova Bombs. Keep
    // that floor while God Mode is active; the exact pre-strategy value is
    // restored after firing below so pickups above three remain infinite too.
    if (map_.read_native_word(special_weapon_count_) < 3U) {
        map_.write_native_word(special_weapon_count_, 3U);
    }
}

void GameSimulation::detonate_god_nuke() {
    constexpr std::uint8_t friend_collision = 0x80U;
    constexpr std::uint8_t hit_flash = 0x02U;
    constexpr std::uint8_t collision_disabled = 0x01U;
    constexpr std::uint8_t nuked = 0x10U;
    constexpr std::uint8_t regular_nuke_damage = 10U;

    const std::array<ObjectHandle, 4> player_parts{
        player_,
        handle_from_native_pointer(map_.read_native_word(player_collision_box_)),
        handle_from_native_pointer(
            map_.read_native_word(player_left_wing_collision_box_)),
        handle_from_native_pointer(
            map_.read_native_word(player_right_wing_collision_box_)),
    };

    for (const auto handle : objects_.active_handles()) {
        if (std::find(player_parts.begin(), player_parts.end(), handle)
                != player_parts.end()) {
            continue;
        }
        auto& object = objects_.at(handle);
        if (object.shape == nuke_shape_
            || object.strategy_address == nuke_explosion_strategy_
            || (object.collision_flags & friend_collision) != 0U
            || std::bit_cast<std::int8_t>(object.health) < 0) {
            continue;
        }

        const auto protected_shape = std::find(
            god_nuke_protected_shapes_.begin(),
            god_nuke_protected_shapes_.end(), object.shape);
        if (protected_shape != god_nuke_protected_shapes_.end()) {
            // Star Fox EX excludes Andross outright and lets the Macbeth boss
            // components take normal bomb damage without autokilling them;
            // both exceptions prevent their scripted sequences softlocking.
            const auto protected_index = static_cast<std::size_t>(std::distance(
                god_nuke_protected_shapes_.begin(), protected_shape));
            if (protected_index >= 6U) continue;
            object.health = object.health > regular_nuke_damage
                ? static_cast<std::uint8_t>(object.health - regular_nuke_damage)
                : 0U;
        } else {
            object.strategy_flags[1] |= collision_disabled;
            object.health = 0U;
        }
        object.strategy_flags[0] |= hit_flash;
        object.type |= nuked;
    }
}

void GameSimulation::service_god_nuke(const input::TickInput& input,
    const std::vector<ObjectHandle>& nukes_before_strategies) {
    if (!god_mode_
        || (flow_state_ != GameFlowState::gameplay
            && flow_state_ != GameFlowState::training)) {
        armed_god_nukes_.clear();
        return;
    }

    const auto bomb_pressed = (input.pressed & starfox::input::a) != 0U;
    const auto god_nuke_pressed = bomb_pressed
        && (input.held & starfox::input::right_shoulder) != 0U;
    if (bomb_pressed) {
        for (const auto handle : objects_.active_handles()) {
            if (objects_.at(handle).shape != nuke_shape_
                || std::find(nukes_before_strategies.begin(),
                       nukes_before_strategies.end(), handle)
                    != nukes_before_strategies.end()) {
                continue;
            }
            // EX shortens God Mode's bomb cadence from the retail 50 logic
            // ticks to four, without changing the pace of the rest of play.
            map_.write_native_byte(special_weapon_delay_, 4U);
            if (god_nuke_pressed
                && std::find(armed_god_nukes_.begin(),
                       armed_god_nukes_.end(), handle)
                    == armed_god_nukes_.end()) {
                armed_god_nukes_.push_back(handle);
            }
        }
    }

    auto detonations = std::size_t{};
    armed_god_nukes_.erase(std::remove_if(
        armed_god_nukes_.begin(), armed_god_nukes_.end(),
        [this, &detonations](ObjectHandle handle) {
            if (!objects_.is_active(handle)) return true;
            const auto& object = objects_.at(handle);
            if (object.shape != null_shape_
                && object.strategy_address != nuke_explosion_strategy_) {
                return false;
            }
            ++detonations;
            return true;
        }), armed_god_nukes_.end());
    for (std::size_t detonation = 0; detonation < detonations; ++detonation) {
        detonate_god_nuke();
    }
}

std::array<std::uint16_t, 16> GameSimulation::palette_words() const noexcept {
    std::array<std::uint16_t, 16> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = map_.read_native_word(
            game_palette_ + static_cast<std::uint32_t>(index) * 2U);
    }
    return result;
}

std::uint32_t GameSimulation::rom_symbol(const std::string& name) const {
    for (const auto address : symbols_->find(name)) {
        if ((address & 0xffffU) >= 0x8000U && ((address >> 16U) & 0xffU) < 0x7eU) {
            return address;
        }
    }
    throw std::runtime_error{"missing game ROM symbol: " + name};
}

std::uint32_t GameSimulation::ram_symbol(const std::string& name) const {
    for (const auto address : symbols_->find(name)) {
        const auto bank = address >> 16U;
        if (bank == 0U || bank == 0x70U || bank == 0x7eU || bank == 0x7fU) {
            return address;
        }
    }
    throw std::runtime_error{"missing game RAM symbol: " + name};
}

MeterState GameSimulation::meter_state() const noexcept {
    return {
        map_.read_native_byte(meter_damage_),
        map_.read_native_byte(meter_boost_),
        map_.read_native_byte(meter_shield_up_) != 0U,
        map_.read_native_word(meters_enabled_) != 0U,
        map_.read_native_byte(boss_health_),
        map_.read_native_byte(boss_max_health_),
    };
}

CircleEffectState GameSimulation::circle_effect_state() const noexcept {
    return circle_effect_;
}

DialogueState GameSimulation::dialogue_state() const noexcept {
    const auto open_count = map_.read_native_byte(message_count_1_);
    const auto animation_count = map_.read_native_byte(message_count_2_);
    const auto active = open_count != 0U || animation_count != 0U;
    auto portrait_frame = std::uint8_t{};
    const auto pointer = map_.read_native_word(face_pointer_);
    const auto face_base = static_cast<std::uint16_t>(face_data_);
    if (pointer >= face_base) {
        portrait_frame = static_cast<std::uint8_t>((pointer - face_base) / 640U);
    }
    const auto friend_id = map_.read_native_byte(which_friend_);
    return {
        active,
        open_count != 0U && animation_count >= 5U,
        (friend_id & 0x80U) != 0U || (friend_id & 0x7fU) == 5U,
        portrait_frame,
        (messages_ & 0xff0000U) | map_.read_native_word(friends_message_),
    };
}

StageResultsState GameSimulation::stage_results_state() const noexcept {
    return {
        flow_state_ == GameFlowState::stage_results,
        stage_percentage_,
        displayed_stage_percentage_,
        stage_hit_score_,
        static_cast<std::uint16_t>(
            previous_total_percentage_ + displayed_stage_percentage_),
        {
            map_.read_native_byte(peppy_health_),
            map_.read_native_byte(falco_health_),
            map_.read_native_byte(slippy_health_),
        },
    };
}

BriefingState GameSimulation::briefing_state() const noexcept {
    return {
        flow_state_ == GameFlowState::planet_travel && briefing_started_,
        briefing_message_characters_,
        briefing_planet_characters_,
        briefing_message_address_,
        briefing_planet_address_,
    };
}

PlanetPresentationState GameSimulation::planet_presentation_state() const noexcept {
    PlanetPresentationState result;
    if (flow_state_ != GameFlowState::planet_travel) return result;
    const auto planet = map_.read_native_byte(current_planet_);
    if (planet < 17U) {
        const auto record = planet_positions_
            + static_cast<std::uint32_t>(planet) * 4U;
        const auto left = static_cast<std::int16_t>(rom_->read8(record + 2U) + 1U);
        const auto top = static_cast<std::int16_t>(rom_->read8(record + 3U) + 1U);
        result.isolate_left = left;
        result.isolate_top = top;
        result.isolate_right = static_cast<std::int16_t>(left + 29);
        result.isolate_bottom = static_cast<std::int16_t>(top + 29);
    }
    result.isolate_fade = frontend_phase_ == FrontendPhase::planet_isolate;
    result.isolate_amount = static_cast<std::uint8_t>(
        std::min<std::uint32_t>(31U, frontend_frames_));
    result.briefing_layers = frontend_phase_ == FrontendPhase::planet_zoom
        || frontend_phase_ == FrontendPhase::planet_briefing
        || frontend_phase_ == FrontendPhase::planet_fade_to_level;
    result.portrait_brightness = pepper_brightness_;
    return result;
}

void GameSimulation::calculate_meters() {
    map_.write_native_byte(meter_shield_up_, map_.read_native_byte(shield_up_));
    const auto collision_box = map_.read_native_word(player_collision_box_);
    const auto health = std::bit_cast<std::int8_t>(
        map_.read_native_byte(collision_box + 42U));
    map_.write_native_byte(meter_damage_,
        health < 0 ? 0U : static_cast<std::uint8_t>(health));

    auto boost = map_.read_native_byte(meter_boost_);
    if (map_.read_native_byte(boost_count_) != 0U) {
        const auto reduced = static_cast<std::int16_t>(boost) - 2;
        if (reduced < 0) {
            boost = 0U;
            map_.write_native_byte(boost_count_, 0U);
        } else {
            boost = static_cast<std::uint8_t>(reduced);
        }
    } else if (boost != 40U) {
        ++boost;
    }
    map_.write_native_byte(meter_boost_, boost);
}

std::uint16_t GameSimulation::native_pointer(ObjectHandle handle) noexcept {
    return handle == 0 ? 0U
        : static_cast<std::uint16_t>(0x0338U + (handle - 1U) * 56U);
}

ObjectHandle GameSimulation::handle_from_native_pointer(std::uint16_t pointer) const noexcept {
    if (pointer < 0x0338U) return 0;
    const auto displacement = static_cast<std::uint16_t>(pointer - 0x0338U);
    if (displacement % 56U != 0U) return 0;
    const auto handle = static_cast<ObjectHandle>(displacement / 56U + 1U);
    return objects_.is_active(handle) ? handle : 0;
}

void GameSimulation::refresh_player_reference() {
    const auto handle = handle_from_native_pointer(
        map_.read_native_word(internal_player_pointer_));
    if (handle == 0) return;
    player_ = handle;
    // INITGAME_L can recycle the same numeric alien slot after a restart.
    // Rebind even when the handle value is unchanged so MapVm never retains
    // the dead ship/dummy object's ownership state.
    map_.set_player(handle);
}

void GameSimulation::write_input(const input::TickInput& input) {
    // IRQ.ASM stores old/current high and low bytes interleaved rather than
    // as one contiguous 16-bit word: CONT0L, CONT0, CONTL0L, CONTL0.
    map_.write_native_byte(previous_controller_high_,
                           map_.read_native_byte(controller_high_));
    map_.write_native_byte(previous_controller_low_,
                           map_.read_native_byte(controller_low_));
    map_.write_native_byte(controller_high_,
                           static_cast<std::uint8_t>(input.held >> 8U));
    map_.write_native_byte(controller_low_,
                           static_cast<std::uint8_t>(input.held));
    map_.write_native_word(trigger_, input.pressed);
    map_.write_native_word(hardware_controller_, input.held);
}

void GameSimulation::service_audio_irq(std::vector<std::uint8_t>& commands) {
    // IRQ.ASM's STARTMUS runs once per 60 Hz video phase. Keep its two-step
    // port acknowledgements and 16-entry effect queue intact even though the
    // PC presentation loop is decoupled from the 20 Hz gameplay update.
    const auto upload_generation = map_.apu_upload_generation();
    if (upload_generation != observed_apu_upload_generation_) {
        observed_apu_upload_generation_ = upload_generation;
        background_music_start_pending_ = false;
        background_music_start_delay_phases_ = 0U;
        background_music_hold_phases_ = 0U;
    }
    const auto music = map_.read_native_byte(background_music_command_);
    const auto music_count = map_.read_native_byte(background_music_count_);
    if (music_count == 0U) {
        if (!background_music_start_pending_) {
            // A native SBOOTAPU resets BGMCNT at the end of an upload, but
            // the host executes that long CPU transfer atomically. Give the
            // newly started SPC driver the hardware time it would have had
            // before the first IRQ submits its track command.
            background_music_start_delay_phases_ =
                background_music_upload_delay_override_ != 0U
                ? background_music_upload_delay_override_ : 6U;
            background_music_upload_delay_override_ = 0U;
            background_music_start_pending_ = true;
        }
        if (background_music_start_delay_phases_ != 0U) {
            --background_music_start_delay_phases_;
        } else {
            map_.write_native_byte(0x002140U, music);
            map_.write_native_byte(background_music_count_, 1U);
            background_music_start_pending_ = false;
            // The CPU-side bus model cannot observe the emulated SPC
            // driver's asynchronous echo, so do not accept it immediately.
            background_music_hold_phases_ = 2U;
        }
    } else if (music_count == 1U) {
        if (background_music_hold_phases_ != 0U) {
            --background_music_hold_phases_;
        } else if (map_.read_native_byte(0x002140U) != music) {
            map_.write_native_byte(0x002140U, music);
        } else {
            map_.write_native_byte(0x002140U, 0U);
            map_.write_native_byte(background_music_count_, 2U);
        }
    }

    const auto pending = map_.read_native_byte(sound_pending_);
    if (pending != 0U) {
        if (map_.read_native_byte(0x002143U) != pending) return;
        map_.write_native_byte(sound_pending_, 0U);
        map_.write_native_byte(0x002143U, 0U);
    }

    const auto pause = map_.read_native_byte(pause_sound_);
    if (pause != 0U) {
        map_.write_native_byte(0x002143U, pause);
        map_.write_native_byte(sound_pending_, pause);
        map_.write_native_byte(sound_write_, 0U);
        map_.write_native_byte(sound_read_, 0U);
        map_.write_native_byte(pause_sound_, 0U);
        commands.push_back(pause);
        return;
    }

    const auto read = static_cast<std::uint8_t>(
        map_.read_native_byte(sound_read_) & 15U);
    const auto write = static_cast<std::uint8_t>(
        map_.read_native_byte(sound_write_) & 15U);
    if (read == write) return;
    const auto command = map_.read_native_byte(sound_buffer_ + read);
    map_.write_native_byte(0x002143U, command);
    map_.write_native_byte(sound_pending_, command);
    map_.write_native_byte(sound_read_, static_cast<std::uint8_t>((read + 1U) & 15U));
    commands.push_back(command);
}

void GameSimulation::queue_sound_effect(std::uint8_t command) {
    const auto read = static_cast<std::uint8_t>(
        map_.read_native_byte(sound_read_) & 15U);
    const auto write = static_cast<std::uint8_t>(
        map_.read_native_byte(sound_write_) & 15U);
    const auto next = static_cast<std::uint8_t>((write + 1U) & 15U);
    if (next == read) return;
    map_.write_native_byte(sound_buffer_ + write, command);
    map_.write_native_byte(sound_write_, next);
}

void GameSimulation::request_music(std::uint8_t command) {
    map_.write_native_byte(background_music_command_, command);
    map_.write_native_byte(background_music_count_, 0U);
    background_music_start_pending_ = false;
    background_music_start_delay_phases_ = 0U;
    background_music_hold_phases_ = 0U;
}

void GameSimulation::set_player_control(bool enabled) {
    auto flags = map_.read_native_byte(player_ship_flags_);
    if (enabled) flags &= static_cast<std::uint8_t>(~0xe0U);
    else flags |= 0x60U;
    map_.write_native_byte(player_ship_flags_, flags);
}

std::uint8_t GameSimulation::required_video_phases() const noexcept {
    if (timing_mode_ != TimingMode::original_speed) {
        return 3U;
    }
    // INTRO.ASM is a real Super FX scene, not a 65C816-only menu. Its text
    // trails, Arwings and boss vignette overrun the unlocked 20 Hz ceiling in
    // the same way as gameplay on the 10.7 MHz cartridge.
    if (flow_state_ == GameFlowState::intro) {
        const auto pressure = std::min<std::size_t>(
            3U, draw_order_.size() / 12U);
        return static_cast<std::uint8_t>(3U + pressure);
    }
    if (flow_state_ != GameFlowState::gameplay
        && flow_state_ != GameFlowState::training
        && flow_state_ != GameFlowState::game_over) {
        return 3U;
    }
    // NTSC capture of the 10.7 MHz cartridge launch falls between six and
    // seven video phases per completed source update while the tunnel and
    // four Arwings are submitted. The earlier constant seven came from PAL
    // Starwing footage and made this mode visibly too slow. A deterministic
    // 6,6,7,7,7 cadence averages 6.6 without disturbing the 60 Hz raster.
    if (flow_state_ == GameFlowState::gameplay
        && (map_.read_native_byte(player_ship_flags_) & 0x20U) != 0U) {
        return source_update_sequence_ % 5U < 2U ? 6U : 7U;
    }
    // Once control is active, approximate the cartridge's transfer pressure
    // from the submitted source draw list. This retains the 20 Hz ceiling and
    // introduces additional video phases only as a scene becomes crowded.
    const auto pressure = std::min<std::size_t>(3U, draw_order_.size() / 12U);
    return static_cast<std::uint8_t>(3U + pressure);
}

bool GameSimulation::logic_tick_ready() const noexcept {
    return video_phases_since_tick_ >= required_video_phases();
}

double GameSimulation::logic_interpolation_alpha(
    double video_phase_fraction) const noexcept {
    video_phase_fraction = std::clamp(video_phase_fraction, 0.0, 1.0);
    return std::min(1.0,
        (static_cast<double>(video_phases_since_tick_) + video_phase_fraction)
        / static_cast<double>(required_video_phases()));
}

void GameSimulation::complete_video_phases_for_tick() {
    const auto video_phases_per_tick = required_video_phases();
    while (video_phases_since_tick_ < video_phases_per_tick) {
        map_.tick_video_phase();
        ++video_phases_since_tick_;
    }
    current_tick_video_phases_ = video_phases_since_tick_;
    video_phases_since_tick_ = 0U;
}

void GameSimulation::start_map(const std::string& symbol) {
    paused_ = false;
    map_.start(rom_symbol(symbol), player_);
    map_.advance_distance(1);
    ++scene_revision_;
}

void GameSimulation::configure_route_for_map(const std::string& symbol) {
    // Normal route map names are LEVEL<route>_<stage>. During gameplay
    // WHICHROUTE is the displayed difficulty number; PLANETSEQ swaps routes
    // 0/1 while indexing STAGEPATHS and swaps them back before INITGAME_L.
    if (symbol.size() < 8U) return;
    std::string upper = symbol;
    for (auto& character : upper) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    if (!upper.starts_with("LEVEL") || upper[5] < '1' || upper[5] > '3'
        || upper[6] != '_' || upper[7] < '1' || upper[7] > '9') {
        return;
    }
    const auto route = static_cast<std::uint8_t>(upper[5] - '1');
    const auto stage = static_cast<std::uint16_t>(upper[7] - '1');
    map_.write_native_word(stage_, stage);
    map_.write_native_byte(current_level_, route);
    map_.write_native_byte(which_route_, route);
    if (route < 2U) map_.write_native_byte(which_route_, static_cast<std::uint8_t>(route ^ 1U));
    static_cast<void>(resolve_route_stage(stage));
    if (route < 2U) map_.write_native_byte(which_route_, route);
    route_display_order_ = false;
}

std::uint32_t GameSimulation::resolve_route_stage(std::uint16_t remaining_stage) {
    const auto route = map_.read_native_byte(which_route_);
    if (route >= 5U) {
        throw std::runtime_error{"planet route index is outside STAGEPATHS"};
    }
    auto cursor = stage_paths_ + rom_->read16(
        stage_paths_ + static_cast<std::uint32_t>(route) * 2U);

    for (std::size_t guard = 0; guard < 512U; ++guard) {
        const auto record = rom_->read8(cursor);
        if (record == 0U) {
            throw std::runtime_error{"planet route ended before the requested stage"};
        }
        if (record == 1U) {
            cursor = stage_paths_ + rom_->read16(cursor + 1U);
            continue;
        }
        if (record == 2U) {
            const auto slot = rom_->read16(cursor + 1U);
            if (slot >= 8U || (slot & 1U) != 0U) {
                throw std::runtime_error{"invalid planet route-choice slot"};
            }
            cursor = stage_paths_ + map_.read_native_word(routes_ + slot);
            continue;
        }
        if (record != 3U) {
            throw std::runtime_error{"unknown planet path record"};
        }

        const auto map_address =
            (static_cast<std::uint32_t>(rom_->read8(cursor + 6U)) << 16U)
            | 0x8000U | (rom_->read16(cursor + 4U) & 0x7fffU);
        map_.write_native_byte(current_planet_, rom_->read8(cursor + 3U));
        map_.write_native_byte(new_map_, static_cast<std::uint8_t>(map_address));
        map_.write_native_byte(new_map_ + 1U, static_cast<std::uint8_t>(map_address >> 8U));
        map_.write_native_byte(new_map_ + 2U, static_cast<std::uint8_t>(map_address >> 16U));
        map_.write_native_byte(pepper_message_, rom_->read8(cursor + 7U));
        map_.write_native_byte(current_level_, rom_->read8(cursor + 8U));
        if (remaining_stage == 0U) return map_address;

        cursor += 9U;
        for (std::size_t path_guard = 0; path_guard < 128U; ++path_guard) {
            if (rom_->read16(cursor) == 0xffffU) break;
            cursor += 4U;
            if (path_guard == 127U) {
                throw std::runtime_error{"unterminated planet path geometry"};
            }
        }
        cursor += 2U;
        --remaining_stage;
    }
    throw std::runtime_error{"planet route traversal exceeded its record limit"};
}

void GameSimulation::initialize_native_map(std::uint32_t address) {
    paused_ = false;
    circle_effect_ = {};
    // INITGAME3D_L normally follows the complete cartridge teardown, which
    // clears the colour-window program. Host scene changes call the native
    // initializer directly, so clear its retained smart-bomb state here as
    // well; otherwise leaving training during an active bomb resumes that
    // circle over the controller screen on the next source tick.
    map_.write_native_word(circle_animation_, 0U);
    map_.write_native_word(circle_object_, 0U);
    map_.write_native_word(circle_radius_, 0U);
    map_.write_native_byte(circle_affected_layers_, 0U);
    // INITGAME3D_L resets M_PARTICLERAND to $1234 for every map. The native
    // Super FX draw list is host-translated, so reset its host pool here too.
    particles_.reset();
    map_.write_native_word(ram_symbol("MAPPTR"),
        static_cast<std::uint16_t>(address & 0x7fffU));
    map_.write_native_byte(ram_symbol("MAPBANK"),
        static_cast<std::uint8_t>(address >> 16U));
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(initialize_game_, registers, 50'000'000, true);
    map_.restore_map_state_from_native();
    refresh_player_reference();
    draw_order_ = objects_.active_handles();
    ++scene_revision_;
}

void GameSimulation::enter_game_over() {
    paused_ = false;
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(
        game_over_initialize_, registers, 50'000'000, true);
    map_.restore_map_state_from_native();
    refresh_player_reference();

    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(
        game_over_background_, registers, 50'000'000, true);
    map_.write_native_word(level_finished_, 0U);
    draw_order_ = objects_.active_handles();
    flow_ticks_ = 0U;
    flow_state_ = GameFlowState::game_over;
}

void GameSimulation::update_continue_sprites() {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(clear_sprites_, registers, 2'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(fox_sprites_, registers, 2'000'000, true);
    map_.upload_oam(sprite_block_, 544U);
}

void GameSimulation::enter_continue_screen() {
    paused_ = false;
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_byte(foxy_option_, 0U);
    map_.write_native_byte(foxy_frame_, 0U);
    map_.write_native_byte(foxy_foot_, 0U);

    auto background_characters =
        assets::decrunch_reverse(*rom_, bg_fox_characters_).bytes;
    auto background_tilemap =
        assets::decrunch_reverse(*rom_, bg_fox_tilemap_).bytes;
    auto object_characters =
        assets::decrunch_reverse(*rom_, fox_object_characters_).bytes;
    const auto character_offset = static_cast<std::uint16_t>(
        (vchr_logical_background_ - vchr_physical_background_) / 16U);
    for (std::size_t index = 0; index + 1U < background_tilemap.size(); index += 2U) {
        const auto word = static_cast<std::uint16_t>(background_tilemap[index])
            | (static_cast<std::uint16_t>(background_tilemap[index + 1U]) << 8U);
        const auto adjusted = static_cast<std::uint16_t>(word + character_offset);
        background_tilemap[index] = static_cast<std::uint8_t>(adjusted);
        background_tilemap[index + 1U] = static_cast<std::uint8_t>(adjusted >> 8U);
    }
    background_characters.resize(6U * 1024U);
    background_tilemap.resize(8U * 1024U);
    object_characters.resize(4U * 1024U);
    map_.write_vram(static_cast<std::uint16_t>(vchr_logical_background_ * 2U),
        background_characters);
    map_.write_vram(static_cast<std::uint16_t>(vsc_base_2_ * 2U),
        background_tilemap);
    map_.write_vram(static_cast<std::uint16_t>(vobj_base_ * 2U),
        object_characters);
    std::array<std::uint16_t, 256> palette{};
    for (std::size_t index = 0; index < palette.size(); ++index) {
        palette[index] = map_.read_native_word(
            bg_fox_palette_ + static_cast<std::uint32_t>(index * 2U));
    }
    map_.write_cgram(0U, palette);

    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(set_charmap_fox_, registers, 5'000'000, true);
    map_.write_native_byte(0x002105U, 1U);
    map_.write_native_byte(0x00212cU, 0x13U);
    map_.set_display_brightness(15U);
    map_.write_native_word(ram_symbol("BG2XSCROLL"), 0U);
    map_.write_native_word(ram_symbol("BG2SCROLL"), 0U);
    map_.write_native_word(vanish_x_, 112U);
    map_.write_native_word(vanish_y_, 96U);
    for (std::uint32_t offset = 0; offset < 6U; offset += 2U) {
        map_.write_native_word(view_position_ + offset, 0U);
        map_.write_native_word(previous_view_position_ + offset, 0U);
        map_.write_native_word(view_rotation_ + offset, 0U);
    }

    objects_.reset();
    const auto demo = objects_.allocate_after();
    auto& object = objects_.at(demo);
    object.shape = static_cast<std::uint16_t>(fox_shape_);
    object.world_z = 350;
    object.rotation_y = 4U;
    player_ = demo;
    draw_order_ = {demo};
    update_continue_sprites();
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(continue_music_, registers, 20'000'000, true);
    flow_ticks_ = 0U;
    flow_state_ = GameFlowState::continue_choice;
    ++scene_revision_;
}

void GameSimulation::enter_title() {
    initialize_native_map(title_map_);
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(
        set_charmap_fox_, registers, 5'000'000, true);
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_word(level_finished_, 0U);
    flow_ticks_ = 0U;
    frontend_frames_ = 0U;
    intro_reveal_frames_ = 0U;
    frontend_phase_ = FrontendPhase::none;
    flow_state_ = GameFlowState::title;
}

void GameSimulation::enter_intro() {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(
        initialize_music_, registers, 20'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(
        intro_music_, registers, 20'000'000, true);
    initialize_native_map(intro_map_);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(
        set_charmap_fox_, registers, 5'000'000, true);
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_byte(exit_intro_, 0U);
    map_.write_native_byte(once_wipe_, 0U);
    map_.write_native_word(level_finished_, 0U);
    // Hide the constructor's close-up player pose while INTRO.ASM performs
    // its first few transfers. On hardware this work occurs under forced
    // blank before the star field/Nintendo raster becomes visible.
    map_.set_display_brightness(0U);
    intro_reveal_frames_ = 15U;
    flow_ticks_ = 0U;
    frontend_frames_ = 0U;
    frontend_phase_ = FrontendPhase::none;
    flow_state_ = GameFlowState::intro;
}

GameTickResult GameSimulation::tick_continue_screen(const input::TickInput& input) {
    constexpr std::uint32_t spc_clocks_per_tick = 1'024'000U / 20U;
    constexpr std::uint8_t video_phases_per_tick = 3U;
    write_input(input);
    GameTickResult result;
    for (std::size_t phase = 0; phase < video_phases_per_tick; ++phase) {
        map_.set_apu_clock_offset(static_cast<std::uint32_t>(
            phase * spc_clocks_per_tick / video_phases_per_tick));
        service_audio_irq(result.sound_effect_commands);
    }
    ++flow_ticks_;
    auto option = map_.read_native_byte(foxy_option_) != 0U
        ? std::uint8_t{1U} : std::uint8_t{};
    if ((input.pressed & starfox::input::select) != 0U) option ^= 1U;
    if ((input.pressed & starfox::input::up) != 0U) option = 0U;
    if ((input.pressed & starfox::input::down) != 0U) option = 1U;
    map_.write_native_byte(foxy_option_, option == 0U ? 0U : 0xffU);
    if (objects_.is_active(player_)) {
        auto& object = objects_.at(player_);
        if ((input.held & starfox::input::left) != 0U) ++object.rotation_y;
        if ((input.held & starfox::input::right) != 0U) --object.rotation_y;
        if ((input.held & starfox::input::up) != 0U) ++object.rotation_x;
        if ((input.held & starfox::input::down) != 0U) --object.rotation_x;
    }
    update_continue_sprites();
    if ((input.pressed & (starfox::input::a | starfox::input::b
            | starfox::input::start)) != 0U) {
        if (option == 0U) continue_current_stage();
        else enter_title();
    } else if (flow_ticks_ >= 1'200U) {
        enter_title();
    }
    result.audio_port_writes = map_.take_apu_port_writes();
    return result;
}

void GameSimulation::continue_current_stage() {
    const auto map_address = selected_route_stage(map_.read_native_word(stage_));
    enter_planet_map(false, map_address);
}

void GameSimulation::enter_credits() {
    paused_ = false;
    initialize_native_map(credits_map_);
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_word(level_finished_, 0U);
    flow_ticks_ = 0U;
    flow_state_ = GameFlowState::credits;
}

void GameSimulation::update_control_screen_sprites() {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    if (flow_state_ == GameFlowState::controls_choice) {
        map_.call_native_near_routine(
            controls_sprites_, registers, 2'000'000, true);
    } else {
        map_.write_native_word(sprite_position_, 0U);
        map_.call_native_routine(
            reset_sprites_, registers, 2'000'000, true);
    }
    map_.upload_oam(sprite_block_, 544U);
}

void GameSimulation::enter_controls(
    GameFlowState state, std::uint8_t selection) {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(
        controls_music_, registers, 20'000'000, true);
    // BGM OPS is uploaded over the 65C816/APU handshake before its first
    // track command can be acknowledged. The host copies it atomically, so
    // retain the measured setup interval explicitly. The controller raster is
    // held black for 90 presentations; start OPS as that hold ends instead of
    // letting it arrive over the preceding title-to-controls transition.
    background_music_upload_delay_override_ = 90U;
    initialize_native_map(controls_map_);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(
        set_charmap_fox_, registers, 5'000'000, true);
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_word(vanish_x_, 64U);
    map_.write_native_word(vanish_y_, 48U);
    map_.write_native_byte(controls_exit_, selection != 0U ? 1U : 0U);

    auto characters = assets::decrunch_reverse(*rom_, object_2_characters_).bytes;
    // vobj_base is the source VRAM word address $6800.
    std::array<std::uint8_t, 4U * 1024U> character_region{};
    if (characters.size() > character_region.size()) {
        throw std::runtime_error{"control-screen OBJ character archive is oversized"};
    }
    std::copy(characters.begin(), characters.end(), character_region.begin());
    map_.write_vram(0xd000U, character_region);
    std::array<std::uint16_t, 128> palette{};
    for (std::size_t index = 0; index < palette.size(); ++index) {
        palette[index] = map_.read_native_word(
            object_2_palette_ + static_cast<std::uint32_t>(index * 2U));
    }
    map_.write_cgram(128U, palette);

    flow_ticks_ = 0U;
    frontend_frames_ = 0U;
    // CONT_L uploads a new SPC program and CONTMAP advances the hidden ship
    // through its setup distance while the SNES display remains forced
    // black. Both operations execute atomically in the host, so preserve
    // their observed 1.5-second black interval explicitly; otherwise the
    // first controller-screen frames reveal the ship flying in from outside
    // its preview window.
    map_.set_display_brightness(0U);
    frontend_phase_ = FrontendPhase::controls_reveal_hold;
    flow_state_ = state;
    set_player_control(state == GameFlowState::controls_type);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(set_control_type_, registers);
    update_control_screen_sprites();
}

void GameSimulation::enter_training() {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(initialize_all_, registers, 5'000'000);
    initialize_native_map(training_map_);
    map_.write_native_byte(lives_, 1U);
    map_.write_native_word(level_finished_, 0U);
    flow_ticks_ = 0U;
    frontend_frames_ = 0U;
    frontend_phase_ = FrontendPhase::none;
    flow_state_ = GameFlowState::training;
}

void GameSimulation::start_initial_route() {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(initialize_all_, registers, 5'000'000);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(initialize_planets_, registers, 5'000'000, true);
    // PLANETSEQ begins with CONVERTROUTE: gameplay's internal routes 0/1
    // are swapped to the selector's NORMAL/EASY display order. Omitting this
    // made a fresh game show LEVEL 2 while still launching LEVEL1_1.
    auto route = map_.read_native_byte(which_route_);
    if (route < 2U) {
        map_.write_native_byte(
            which_route_, static_cast<std::uint8_t>(route ^ 1U));
    }
    route_display_order_ = true;
    enter_planet_map(true);
}

std::uint32_t GameSimulation::selected_route_stage(std::uint16_t stage) {
    if (route_display_order_) return resolve_route_stage(stage);
    auto route = map_.read_native_byte(which_route_);
    if (route < 2U) route ^= 1U;
    map_.write_native_byte(which_route_, route);
    const auto map_address = resolve_route_stage(stage);
    if (route < 2U) route ^= 1U;
    map_.write_native_byte(which_route_, route);
    return map_address;
}

void GameSimulation::redraw_planet_route(bool complete_route) {
    set_planet_route_lines(false, complete_route);
    set_planet_route_lines(true, complete_route);
    planet_route_blink_frames_ = 0U;
    planet_route_lines_visible_ = true;
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_near_routine(draw_route_name_, registers, 2'000'000, true);
    map_.upload_oam(sprite_block_, 544U);
}

void GameSimulation::set_planet_route_lines(
    bool visible, bool complete_route) {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    if (!visible) {
        map_.call_native_routine(
            undraw_planet_lines_, registers, 2'000'000, true);
        planet_route_lines_visible_ = false;
        return;
    }
    const auto saved_stage = map_.read_native_word(stage_);
    if (complete_route) map_.write_native_word(stage_, 10U);
    map_.call_native_routine(draw_planet_lines_, registers, 5'000'000, true);
    map_.write_native_word(stage_, saved_stage);
    if (complete_route) map_.write_native_word(current_planet_, 0xffffU);
    planet_route_lines_visible_ = true;
}

void GameSimulation::update_planet_ship_sprite() {
    constexpr std::uint32_t ship_sprite = 4U * 4U;
    const auto clear_ship = [this] {
        for (std::uint32_t byte = 0U; byte < 4U * 4U; ++byte) {
            map_.write_native_byte(sprite_block_ + 4U * 4U + byte, 0U);
        }
    };

    auto flash = map_.read_native_byte(flash_ship_);
    if ((flash & 1U) != 0U) {
        flash = static_cast<std::uint8_t>(flash + 64U);
        map_.write_native_byte(flash_ship_, flash);
        // IRQPLANETS rotates the accumulator and hides the four pieces when
        // the updated value's high bit becomes carry. This is the original
        // two-on/two-off selection flash, not a host-authored effect.
        if ((flash & 0x80U) != 0U) {
            clear_ship();
            return;
        }
    }

    auto position = map_.read_native_word(ship_position_);
    const auto target = map_.read_native_word(new_ship_position_);
    auto x = static_cast<std::uint8_t>(position);
    auto y = static_cast<std::uint8_t>(position >> 8U);
    const auto target_x = static_cast<std::uint8_t>(target);
    const auto target_y = static_cast<std::uint8_t>(target >> 8U);

    // This follows IRQPLANETS byte-for-byte: vertical segments move one
    // pixel per video frame, while its equality checks preserve the source
    // path's diagonal slopes rather than cutting directly between planets.
    if (y < target_y) {
        ++y;
        if (x != target_x) ++x;
    } else {
        const auto x_difference = static_cast<std::uint8_t>(x - target_x);
        if (x_difference != 0U) {
            if (x >= target_x) {
                const auto y_difference =
                    static_cast<std::uint8_t>(y - target_y);
                if (y_difference == 0U || y_difference == x_difference) --x;
            } else {
                const auto y_difference =
                    static_cast<std::uint8_t>(target_y - y);
                if (y_difference == 0U || y_difference == x_difference) ++x;
            }
        }
        if (y != target_y) --y;
    }
    position = static_cast<std::uint16_t>(x)
        | (static_cast<std::uint16_t>(y) << 8U);
    map_.write_native_word(ship_position_, position);

    const std::array<std::uint8_t, 4> xs{
        static_cast<std::uint8_t>(x + 8U),
        static_cast<std::uint8_t>(x + 16U),
        static_cast<std::uint8_t>(x + 8U),
        static_cast<std::uint8_t>(x + 16U),
    };
    const std::array<std::uint8_t, 4> ys{
        static_cast<std::uint8_t>(y + 8U),
        static_cast<std::uint8_t>(y + 8U),
        static_cast<std::uint8_t>(y + 16U),
        static_cast<std::uint8_t>(y + 16U),
    };
    const auto angle = map_.read_native_byte(ship_angle_);
    const auto first_tile = static_cast<std::uint8_t>(
        angle == 0U ? 9U : angle == 1U ? 5U : 13U);
    for (std::uint32_t piece = 0U; piece < 4U; ++piece) {
        const auto destination = sprite_block_ + ship_sprite + piece * 4U;
        map_.write_native_byte(destination, xs[piece]);
        map_.write_native_byte(destination + 1U, ys[piece]);
        map_.write_native_byte(destination + 2U,
            static_cast<std::uint8_t>(first_tile + piece));
        map_.write_native_byte(destination + 3U, 0x3eU);
    }
}

void GameSimulation::enter_planet_map(
    bool selecting_route, std::uint32_t pending_map) {
    paused_ = false;
    circle_effect_ = {};
    if (!route_display_order_) {
        auto route = map_.read_native_byte(which_route_);
        if (route < 2U) {
            map_.write_native_byte(
                which_route_, static_cast<std::uint8_t>(route ^ 1U));
        }
        route_display_order_ = true;
    }
    pending_map_ = pending_map;
    planet_travel_complete_ = false;
    planet_route_blink_frames_ = 0U;
    planet_route_lines_visible_ = false;
    briefing_started_ = false;
    briefing_message_address_ = 0U;
    briefing_planet_address_ = 0U;
    briefing_message_characters_ = 0U;
    briefing_message_character_count_ = 0U;
    briefing_planet_characters_ = 0U;
    briefing_planet_character_count_ = 0U;
    briefing_lead_frames_ = 0U;
    briefing_character_frames_ = 0U;
    briefing_hold_frames_ = 0U;
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_word(light_x_, 0U);
    map_.write_native_word(light_y_, 0U);
    map_.write_native_word(light_z_, 150U);
    map_.write_native_word(planet_light_x_, 0U);
    map_.write_native_word(planet_light_y_, 0U);
    map_.write_native_word(planet_light_z_, 150U);
    map_.write_native_word(planet_sprite_palette_, 6U);

    // PLANETSEQ starts a fresh sprite build at planetlines_spr and clears the
    // gameplay OAM image before drawing the map ship, route, labels and
    // planets. Reusing the last boss/result sprite block leaves its tiles and
    // high-table size bits scattered across the next map screen.
    for (std::uint32_t byte = 0U; byte < 544U; ++byte) {
        map_.write_native_byte(sprite_block_ + byte, 0U);
    }
    map_.write_native_word(ram_symbol("CURRENTSPRITE"),
        static_cast<std::uint16_t>(sprite_block_ + 8U * 4U));
    map_.write_native_byte(flash_ship_, 0U);
    map_.write_native_word(ship_angle_, 1U);

    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(setup_planets_, registers, 50'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(
        setup_planet_palette_, registers, 2'000'000, true);

    // PLANETSEQ copies MYCRAPCHARS immediately after SETUP_PLANETS_L. It is
    // 30 OBJ tiles containing both the four-piece map ship and the route
    // dots. SETUP_PLANETS_L clears VRAM, so omitting this outer-sequence DMA
    // left valid OAM records pointing at fully transparent character data.
    std::array<std::uint8_t, 4U * 8U * 30U> planet_objects{};
    for (std::size_t byte = 0U; byte < planet_objects.size(); ++byte) {
        planet_objects[byte] = rom_->read8(
            planet_object_characters_ + static_cast<std::uint32_t>(byte));
    }
    constexpr std::uint16_t first_planet_object_page = 0x4040U;
    constexpr std::uint16_t second_planet_object_page = 0xa040U;
    map_.write_vram(first_planet_object_page, planet_objects);
    map_.write_vram(second_planet_object_page, planet_objects);

    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(map_music_, registers, 20'000'000, true);
    map_.set_display_brightness(0U);

    flow_ticks_ = 0U;
    frontend_frames_ = 0U;
    frontend_phase_ = FrontendPhase::planet_fade_in;
    flow_state_ = selecting_route
        ? GameFlowState::planet_select : GameFlowState::planet_travel;
    redraw_planet_route(selecting_route);
    if (pending_map_ != 0U) {
        map_.write_native_byte(new_map_, static_cast<std::uint8_t>(pending_map_));
        map_.write_native_byte(new_map_ + 1U,
            static_cast<std::uint8_t>(pending_map_ >> 8U));
        map_.write_native_byte(new_map_ + 2U,
            static_cast<std::uint8_t>(pending_map_ >> 16U));
    }
    const auto selected_planet = map_.read_native_byte(current_planet_);
    const auto initial_planet = selecting_route || selected_planet >= 17U
        ? std::uint8_t{0U} : selected_planet;
    const auto start = rom_->read16(start_planet_positions_
        + static_cast<std::uint32_t>(initial_planet) * 2U);
    map_.write_native_word(ship_position_, start);
    map_.write_native_word(new_ship_position_, start);
    map_.write_native_word(route_x_, 0U);
    draw_order_.clear();
    planet_spin_remainders_.fill(0);
    ++scene_revision_;

    // Prime both source VRAM buffers so the first presented frame cannot
    // reveal the cleared alternate page. SETUP_PLANETS_L draws both pages at
    // the same initial angle; SPINPLANETS begins in the visible map loop.
    animate_planet_frame(false);
    animate_planet_frame(false);
    update_planet_ship_sprite();
    map_.upload_oam(sprite_block_, 544U);
}

void GameSimulation::animate_planet_frame(bool advance_rotation) {
    if (flow_state_ != GameFlowState::planet_select
        && flow_state_ != GameFlowState::planet_travel) return;
    if (frontend_phase_ == FrontendPhase::planet_zoom) {
        draw_selected_planet(true, true);
        return;
    }
    // Once START is accepted, PLANETSEQ waits, performs its filter fade and
    // scrolls the already-rendered selected bitmap. None of those three
    // loops calls SPINPLANETS; redrawing here changes the zoom's texture
    // phase by more than one hundred source steps.
    if (frontend_phase_ == FrontendPhase::planet_confirm_hold
        || frontend_phase_ == FrontendPhase::planet_isolate
        || frontend_phase_ == FrontendPhase::planet_centre) return;
    if (frontend_phase_ == FrontendPhase::planet_briefing
        || frontend_phase_ == FrontendPhase::planet_fade_to_level) return;

    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_near_routine(
        clear_planet_screen_, registers, 2'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(copy_planet_light_, registers, 2'000'000, true);
    if (advance_rotation) {
        advance_planet_rotation();
    }
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(
        draw_planet_sprites_, registers, 5'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(
        dma_planet_screen_, registers, 2'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(
        switch_planet_buffer_, registers, 2'000'000, true);
    if (flow_state_ == GameFlowState::planet_travel
        && frontend_phase_ == FrontendPhase::planet_route
        && !planet_travel_complete_) {
        registers = {};
        registers.status = 0x24U;
        map_.call_native_near_routine(
            move_ship_along_path_, registers, 5'000'000, true);
        planet_travel_complete_ = (registers.status & 0x01U) != 0U;
    }
    if (pending_map_ != 0U) {
        map_.write_native_byte(new_map_, static_cast<std::uint8_t>(pending_map_));
        map_.write_native_byte(new_map_ + 1U,
            static_cast<std::uint8_t>(pending_map_ >> 8U));
        map_.write_native_byte(new_map_ + 2U,
            static_cast<std::uint8_t>(pending_map_ >> 16U));
    }
    map_.upload_oam(sprite_block_, 544U);
}

void GameSimulation::advance_planet_rotation() {
    // SPINPLANETS applies these steps after an expensive six-planet Super FX
    // redraw. The cartridge completes that pass at roughly one sixth of the
    // 60 Hz presentation rate. Distribute the same step over six frames so
    // the PC result is smooth without running the planets six times too fast.
    constexpr std::array<std::int32_t, 6> source_speeds{
        6 * 256, -3 * 256, 4 * 256, 3 * 256, -5 * 256, -5 * 256,
    };
    for (std::size_t index = 0; index < source_speeds.size(); ++index) {
        auto& remainder = planet_spin_remainders_[index];
        remainder += source_speeds[index];
        const auto delta = remainder / 6;
        remainder -= delta * 6;
        const auto address = planet_rotation_table_
            + static_cast<std::uint32_t>(index * 2U);
        map_.write_native_word(address, static_cast<std::uint16_t>(
            map_.read_native_word(address) + delta));
    }
}

void GameSimulation::draw_selected_planet(
    bool centred, bool advance_rotation) {
    const auto planet = map_.read_native_byte(current_planet_);
    const auto sprite = rom_->read16(
        planet_sprites_ + static_cast<std::uint32_t>(planet) * 2U);
    const auto spherical = (sprite & 0x80U) != 0U;
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_near_routine(
        clear_planet_screen_, registers, 2'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(copy_planet_light_, registers, 2'000'000, true);
    if (advance_rotation) {
        advance_planet_rotation();
    }
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(
        centred ? draw_planet_in_centre_ : draw_selected_planet_,
        registers, 5'000'000, true);
    if (spherical) {
        // RetroCPU currently loses PLANETS.ASM's 8-bit BMI result here and
        // can launch MUSPRITE with a clobbered sprite word ($0221 for
        // Corneria). Keep the surrounding original routine for its exact
        // coordinates/rotation/light setup, then replace only that bad flat
        // draw with the translated MDRAWTSPHERE output.
        registers = {};
        registers.status = 0x24U;
        map_.call_native_near_routine(
            clear_planet_screen_, registers, 2'000'000, true);
        map_.draw_planet_sphere(static_cast<std::uint16_t>(sprite & 0x7fU));
    }
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(
        dma_planet_screen_, registers, 2'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(
        switch_planet_buffer_, registers, 2'000'000, true);
}

void GameSimulation::prepare_planet_briefing_graphics() {
    constexpr std::uint16_t background_characters = 40U * 1024U / 2U;
    constexpr std::uint16_t background_screen = 62U * 1024U / 2U;
    constexpr std::uint16_t pepper_screen_words = 16U * 24U * 32U / 2U;
    constexpr std::uint16_t fox_source = background_characters + 32U * 16U;
    constexpr std::uint16_t fox_destination = background_characters
        + pepper_screen_words + (0x40U + 2U) * 16U;

    // COPYFOX reads Fox from the outgoing Pepper bitmap. Preserve those
    // characters before MCLRPEPPERSCREEN clears the same VRAM range.
    std::array<std::uint8_t, 20U * 16U * 2U> fox{};
    const auto& vram = map_.ppu_state().vram;
    std::copy_n(vram.begin() + static_cast<std::size_t>(fox_source) * 2U,
        fox.size(), fox.begin());

    // MCLRPEPPERSCREEN and DMAPEPPERSCREEN install an empty 192x192 4-bpp
    // character bitmap before DOG.SCR is exposed. Leaving this range full of
    // the old planet bitmap made the portrait screen appear as random text
    // and map fragments.
    std::array<std::uint8_t, 16U * 24U * 32U> empty_pepper_screen{};
    map_.write_vram(static_cast<std::uint16_t>(
        (background_characters + 32U) * 2U), empty_pepper_screen);

    auto dog_characters = assets::decrunch_reverse(*rom_, dog_characters_).bytes;
    dog_characters.resize(4U * 1024U);
    map_.write_vram(static_cast<std::uint16_t>(
        (background_characters + pepper_screen_words + 32U) * 2U),
        dog_characters);

    auto dog_tilemap = assets::decrunch_reverse(*rom_, dog_tilemap_).bytes;
    dog_tilemap.resize(2U * 1024U);
    for (std::size_t index = 0; index + 1U < dog_tilemap.size(); index += 2U) {
        const auto word = static_cast<std::uint16_t>(dog_tilemap[index])
            | (static_cast<std::uint16_t>(dog_tilemap[index + 1U]) << 8U);
        const auto adjusted = static_cast<std::uint16_t>(word + 2U);
        dog_tilemap[index] = static_cast<std::uint8_t>(adjusted);
        dog_tilemap[index + 1U] = static_cast<std::uint8_t>(adjusted >> 8U);
    }
    map_.write_vram(static_cast<std::uint16_t>(background_screen * 2U), dog_tilemap);

    // COPYFOX copies the 20x16-tile Fox portrait already resident in the map
    // character sheet into DOG.PCR's reserved character range.
    map_.write_vram(static_cast<std::uint16_t>(fox_destination * 2U), fox);
    // PLANETS.ASM switches BG1 from the route-map tilemap to the centred
    // 128x128 square and offsets it down 40 lines. This is what places the
    // zoomed planet between Pepper and Fox instead of dropping the bitmap.
    map_.write_native_byte(0x002107U, 0x2cU);
    map_.set_bg1_scroll(0, -40);
    map_.write_native_byte(0x00212cU, 0x03U);
}

void GameSimulation::begin_planet_selection_sequence() {
    request_music(0xf1U);
    queue_sound_effect(0x10U);
    map_.write_native_byte(flash_ship_, 1U);
    flow_state_ = GameFlowState::planet_travel;
    frontend_phase_ = FrontendPhase::planet_confirm_hold;
    frontend_frames_ = 0U;
    pepper_brightness_ = 0U;
}

void GameSimulation::present_frame() {
    map_.tick_video_phase();
    if (video_phases_since_tick_ != 0xffU) ++video_phases_since_tick_;
    if (intro_reveal_frames_ != 0U && flow_state_ == GameFlowState::intro) {
        --intro_reveal_frames_;
        if (intro_reveal_frames_ == 0U) map_.start_display_fade(2);
        else map_.set_display_brightness(0U);
    }
    if (frontend_phase_ == FrontendPhase::controls_reveal_hold) {
        ++frontend_frames_;
        if (frontend_frames_ >= 90U) {
            frontend_frames_ = 0U;
            frontend_phase_ = FrontendPhase::none;
            map_.start_display_fade(1);
        } else {
            // CONTMAP reaches its fade-up immediately in host CPU time. Keep
            // suppressing it until the emulated upload/setup interval ends.
            map_.set_display_brightness(0U);
        }
    }
    animate_planet_frame();
    const auto map_sprites_active =
        (flow_state_ == GameFlowState::planet_select
            || flow_state_ == GameFlowState::planet_travel)
        && (frontend_phase_ == FrontendPhase::planet_fade_in
            || frontend_phase_ == FrontendPhase::planet_route
            || frontend_phase_ == FrontendPhase::planet_confirm_hold
            || frontend_phase_ == FrontendPhase::planet_isolate);
    if (flow_state_ == GameFlowState::planet_select
        && frontend_phase_ == FrontendPhase::planet_route) {
        // PLANETS.ASM samples GAMEFRAME & 2 while its six-planet drawing loop
        // runs. At the host's smooth 60 Hz presentation rate this is six
        // visible frames followed by six hidden frames.
        if (++planet_route_blink_frames_ >= 6U) {
            planet_route_blink_frames_ = 0U;
            set_planet_route_lines(!planet_route_lines_visible_, true);
        }
    }
    if (map_sprites_active) {
        update_planet_ship_sprite();
        map_.upload_oam(sprite_block_, 544U);
    }
    if (frontend_phase_ == FrontendPhase::planet_fade_in) {
        ++frontend_frames_;
        map_.set_display_brightness(static_cast<std::uint8_t>(
            std::min<std::uint32_t>(15U, frontend_frames_ * 2U - 1U)));
        if (frontend_frames_ >= 8U) {
            frontend_phase_ = FrontendPhase::planet_route;
            frontend_frames_ = 0U;
        }
    }
    if (flow_state_ == GameFlowState::planet_travel
        && frontend_phase_ != FrontendPhase::planet_route
        && frontend_phase_ != FrontendPhase::planet_fade_in) {
        ++frontend_frames_;
        if (frontend_phase_ == FrontendPhase::planet_confirm_hold
            && frontend_frames_ >= 83U) {
            frontend_phase_ = FrontendPhase::planet_isolate;
            frontend_frames_ = 0U;
        } else if (frontend_phase_ == FrontendPhase::planet_isolate) {
            if (frontend_frames_ >= 32U) {
                const std::array<std::uint16_t, 1> black{};
                map_.write_cgram(0U, black);
                map_.write_native_byte(0x00212cU, 0x01U);
                map_.set_bg1_scroll(0, 0);
                // The original redraws only the selected planet before the
                // next visible frame. Without this call, the route-map atlas
                // flashes once against black before the centring motion.
                draw_selected_planet(false, false);
                frontend_phase_ = FrontendPhase::planet_centre;
                frontend_frames_ = 0U;
            }
        } else if (frontend_phase_ == FrontendPhase::planet_centre) {
            const auto planet = map_.read_native_byte(current_planet_);
            const auto record = planet_positions_
                + static_cast<std::uint32_t>(planet) * 4U;
            const auto target_x = static_cast<std::int16_t>(
                static_cast<std::int32_t>(rom_->read8(record + 2U)) - 112);
            const auto target_y = static_cast<std::int16_t>(
                static_cast<std::int32_t>(rom_->read8(record + 3U)) - 88);
            const auto progress = std::min<std::uint32_t>(frontend_frames_, 32U);
            map_.set_bg1_scroll(
                static_cast<std::int16_t>(static_cast<std::int32_t>(target_x)
                    * static_cast<std::int32_t>(progress) / 32),
                static_cast<std::int16_t>(static_cast<std::int32_t>(target_y)
                    * static_cast<std::int32_t>(progress) / 32));
            if (frontend_frames_ >= 32U) {
                prepare_planet_briefing_graphics();
                map_.write_native_word(planet_radius_, 15U);
                const auto sprite = rom_->read16(
                    planet_sprites_ + static_cast<std::uint32_t>(planet) * 2U);
                planet_zoom_is_sphere_ = (sprite & 0x80U) != 0U;
                planet_zoom_remaining_ = 40U;
                pepper_brightness_ = 0U;
                request_music(planet_zoom_is_sphere_ ? 0x0bU : 0x0dU);
                frontend_phase_ = FrontendPhase::planet_zoom;
                frontend_frames_ = 0U;
            }
        } else if (frontend_phase_ == FrontendPhase::planet_zoom) {
            auto radius = map_.read_native_word(planet_radius_);
            bool consume_frame = true;
            if (planet_zoom_is_sphere_) {
                if (radius + 1U != 58U) ++radius;
            } else {
                if (radius + 1U != 126U) {
                    ++radius;
                    consume_frame = (radius & 1U) == 0U;
                }
            }
            map_.write_native_word(planet_radius_, radius);
            map_.write_native_word(planet_light_x_, static_cast<std::uint16_t>(
                map_.read_native_word(planet_light_x_) - 10U));
            map_.write_native_word(planet_light_z_, static_cast<std::uint16_t>(
                map_.read_native_word(planet_light_z_) + 10U));
            if ((planet_zoom_is_sphere_ && planet_zoom_remaining_ <= 5U)
                || (!planet_zoom_is_sphere_ && planet_zoom_remaining_ <= 20U)) {
                pepper_brightness_ = static_cast<std::uint8_t>(
                    std::min<std::uint32_t>(30U, pepper_brightness_ + 1U));
            }
            if (consume_frame && planet_zoom_remaining_ != 0U) {
                --planet_zoom_remaining_;
            }
            if (planet_zoom_remaining_ == 0U) {
                begin_planet_briefing();
                frontend_phase_ = FrontendPhase::planet_briefing;
                frontend_frames_ = 0U;
            }
        } else if (frontend_phase_ == FrontendPhase::planet_fade_to_level) {
            if (frontend_frames_ > 2U) {
                map_.set_display_brightness(static_cast<std::uint8_t>(15U
                    - std::min<std::uint32_t>(15U,
                        (frontend_frames_ - 2U) / 2U)));
            }
        }
    }
    if (flow_state_ == GameFlowState::planet_travel && briefing_started_
        && frontend_phase_ == FrontendPhase::planet_briefing) {
        pepper_brightness_ = static_cast<std::uint8_t>(
            std::min<std::uint32_t>(30U, pepper_brightness_ + 1U));
        const auto mutter = [this](std::uint32_t address,
                                   std::uint8_t visible) {
            if (address != 0U && visible != 0U
                && rom_->read8(address + visible)
                    != static_cast<std::uint8_t>('\'')) {
                queue_sound_effect(0x89U);
            }
        };
        // NTSC capture holds the completed portraits for about one second,
        // then both the planet name and Pepper's message grow at roughly one
        // character per three presentations. The inner count of 50 below
        // PEPPERSPEAKING is a same-scanline busy wait, not 50 video frames.
        if (briefing_lead_frames_ < 60U) {
            ++briefing_lead_frames_;
        } else if (briefing_planet_characters_
                   < briefing_planet_character_count_) {
            if (++briefing_character_frames_ >= 3U) {
                briefing_character_frames_ = 0U;
                ++briefing_planet_characters_;
                mutter(briefing_planet_address_, briefing_planet_characters_);
            }
        } else if (briefing_message_characters_
                   < briefing_message_character_count_) {
            if (++briefing_character_frames_ >= 3U) {
                briefing_character_frames_ = 0U;
                ++briefing_message_characters_;
                mutter(briefing_message_address_, briefing_message_characters_);
            }
        } else if (briefing_hold_frames_ != 0xffffU) {
            ++briefing_hold_frames_;
        }
    }
}

void GameSimulation::begin_planet_briefing() {
    const auto message_address = [this](std::uint8_t id) {
        if (id == 0U) return std::uint32_t{};
        const auto pointer = rom_->read16(messages_
            + static_cast<std::uint32_t>(id - 1U) * 2U);
        return (messages_ & 0xff0000U) | static_cast<std::uint16_t>(pointer + 2U);
    };
    briefing_message_address_ = message_address(
        map_.read_native_byte(pepper_message_));
    const auto planet = map_.read_native_byte(current_planet_);
    briefing_planet_address_ = planet < 17U
        ? message_address(rom_->read8(planet_names_ + planet)) : 0U;
    const auto text_length = [this](std::uint32_t address) {
        if (address == 0U) return std::uint16_t{};
        std::uint16_t length{};
        while (length < 255U && rom_->read8(address + 1U + length) != 0U) {
            ++length;
        }
        return length;
    };
    briefing_message_character_count_ = static_cast<std::uint8_t>(
        text_length(briefing_message_address_));
    briefing_planet_character_count_ = static_cast<std::uint8_t>(
        text_length(briefing_planet_address_));
    briefing_message_characters_ = 0U;
    briefing_planet_characters_ = 0U;
    briefing_lead_frames_ = 0U;
    briefing_character_frames_ = 0U;
    briefing_hold_frames_ = 0U;
    briefing_started_ = true;
}

void GameSimulation::launch_pending_stage() {
    if (pending_map_ == 0U) {
        pending_map_ = selected_route_stage(map_.read_native_word(stage_));
    }
    const auto map_address = pending_map_;
    pending_map_ = 0U;
    if (route_display_order_) {
        auto route = map_.read_native_byte(which_route_);
        if (route < 2U) {
            map_.write_native_byte(
                which_route_, static_cast<std::uint8_t>(route ^ 1U));
        }
        route_display_order_ = false;
    }
    initialize_native_map(map_address);
    flow_ticks_ = 0U;
    frontend_frames_ = 0U;
    frontend_phase_ = FrontendPhase::none;
    flow_state_ = GameFlowState::gameplay;
}

GameTickResult GameSimulation::tick_planet_map(const input::TickInput& input) {
    constexpr std::uint32_t spc_clocks_per_tick = 1'024'000U / 20U;
    constexpr std::uint8_t video_phases_per_tick = 3U;
    write_input(input);
    GameTickResult result;
    for (std::size_t phase = 0; phase < video_phases_per_tick; ++phase) {
        map_.set_apu_clock_offset(static_cast<std::uint32_t>(
            phase * spc_clocks_per_tick / video_phases_per_tick));
        service_audio_irq(result.sound_effect_commands);
    }
    ++flow_ticks_;
    if (flow_state_ == GameFlowState::planet_select
        && frontend_phase_ == FrontendPhase::planet_route) {
        auto route = map_.read_native_byte(which_route_);
        bool changed = false;
        if ((input.pressed & (starfox::input::left | starfox::input::down)) != 0U) {
            route = route == 0U ? 2U : static_cast<std::uint8_t>(route - 1U);
            changed = true;
        }
        if ((input.pressed & (starfox::input::right | starfox::input::up
                | starfox::input::select)) != 0U) {
            route = static_cast<std::uint8_t>((route + 1U) % 3U);
            changed = true;
        }
        if (changed) {
            map_.write_native_byte(which_route_, route);
            redraw_planet_route(true);
            queue_sound_effect(0x11U);
        }
        if ((input.pressed & (starfox::input::a | starfox::input::b
                | starfox::input::start)) != 0U) {
            map_.write_native_word(stage_, 0U);
            pending_map_ = selected_route_stage(0U);
            // Confirmation collapses the blinking full-route preview back to
            // stage zero before the ship starts its source flash animation.
            redraw_planet_route(false);
            map_.write_native_byte(
                new_map_, static_cast<std::uint8_t>(pending_map_));
            map_.write_native_byte(new_map_ + 1U,
                static_cast<std::uint8_t>(pending_map_ >> 8U));
            map_.write_native_byte(new_map_ + 2U,
                static_cast<std::uint8_t>(pending_map_ >> 16U));
            begin_planet_selection_sequence();
        }
    } else if (frontend_phase_ == FrontendPhase::planet_route) {
        if (planet_travel_complete_ || flow_ticks_ >= 240U) {
            begin_planet_selection_sequence();
        }
    } else if (frontend_phase_ == FrontendPhase::planet_briefing) {
        if (briefing_hold_frames_ >= 90U
            || (briefing_message_characters_ >= 10U
                && (input.pressed & (starfox::input::a | starfox::input::b
                    | starfox::input::start)) != 0U)) {
            queue_sound_effect(0x13U);
            frontend_phase_ = FrontendPhase::planet_fade_to_level;
            frontend_frames_ = 0U;
        }
    } else if (frontend_phase_ == FrontendPhase::planet_fade_to_level
               && frontend_frames_ >= 34U
               && map_.display_brightness() == 0U) {
        launch_pending_stage();
    }
    result.audio_port_writes = map_.take_apu_port_writes();
    return result;
}

void GameSimulation::service_level_exit() {
    const auto exit = map_.read_native_word(level_finished_);
    if (exit == 0U) return;
    if (flow_state_ == GameFlowState::training) {
        Wdc65816Registers registers;
        registers.status = 0x24U;
        map_.call_native_routine(initialize_all_, registers, 5'000'000);
        map_.write_native_byte(default_training_, 1U);
        enter_controls(GameFlowState::controls_choice,
            exit == 10U ? 0U : 1U);
        return;
    }
    if (flow_state_ == GameFlowState::credits) {
        if (exit == 8U) flow_state_ = GameFlowState::finished;
        return;
    }
    if (exit == 10U) {
        enter_game_over();
        return;
    }
    if (exit == 6U || exit == 9U) {
        enter_credits();
        return;
    }
    if (exit == 8U) {
        flow_state_ = GameFlowState::finished;
        return;
    }
    // MAIN.ASM increments STAGE before normal and special route exits.
    const auto next_stage = static_cast<std::uint16_t>(
        map_.read_native_word(stage_) + 1U);
    map_.write_native_word(stage_, next_stage);

    std::uint32_t route_change{};
    if (exit == 11U) route_change = route_change_black_hole_1_;
    else if (exit == 12U) route_change = route_change_black_hole_2_;
    else if (exit == 13U) route_change = route_change_black_hole_3_;
    else if (exit == 14U) route_change = route_change_1_;
    if (route_change != 0U) {
        Wdc65816Registers registers;
        registers.status = 0x24U;
        map_.call_native_routine(route_change, registers);
        const auto pointer = map_.read_native_word(percentage_pointer_);
        map_.write_native_byte(percentage_buffer_ + pointer, 101U);
        map_.write_native_word(percentage_pointer_,
            static_cast<std::uint16_t>(pointer + 1U));
        const auto next_map = selected_route_stage(next_stage);
        enter_planet_map(false, next_map);
        return;
    }

    auto percentage = std::uint16_t{};
    for (const auto health : {peppy_health_, falco_health_, slippy_health_}) {
        if (map_.read_native_byte(health) != 0U) percentage += 5U;
    }
    const auto special_total = map_.read_native_byte(special_object_total_);
    if (special_total != 0U) {
        percentage += static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(map_.read_native_byte(specials_dead_))
            * 100U / special_total);
    }
    stage_percentage_ = static_cast<std::uint8_t>(
        std::min<std::uint16_t>(percentage, 100U));
    displayed_stage_percentage_ = 0U;
    stage_hit_score_ = map_.read_native_word(player_score_);
    previous_total_percentage_ = 0U;
    const auto pointer = map_.read_native_word(percentage_pointer_);
    for (std::uint16_t index = 0; index < pointer; ++index) {
        const auto value = map_.read_native_byte(percentage_buffer_ + index);
        if (value != 101U) {
            previous_total_percentage_ = static_cast<std::uint16_t>(
                previous_total_percentage_ + value);
        }
    }
    map_.write_native_word(level_finished_, 0U);
    map_.write_native_word(meters_enabled_, 0U);
    flow_ticks_ = 0U;
    flow_state_ = GameFlowState::stage_results;
}

GameTickResult GameSimulation::tick_stage_results(const input::TickInput& input) {
    constexpr std::uint32_t spc_clocks_per_tick = 1'024'000U / 20U;
    constexpr std::uint8_t video_phases_per_tick = 3U;
    write_input(input);
    GameTickResult result;
    for (std::size_t phase = 0; phase < video_phases_per_tick; ++phase) {
        map_.set_apu_clock_offset(static_cast<std::uint32_t>(
            phase * spc_clocks_per_tick / video_phases_per_tick));
        service_audio_irq(result.sound_effect_commands);
    }
    ++flow_ticks_;
    if (displayed_stage_percentage_ < stage_percentage_) {
        displayed_stage_percentage_ = static_cast<std::uint8_t>(
            std::min<std::uint16_t>(stage_percentage_,
                static_cast<std::uint16_t>(displayed_stage_percentage_) + 3U));
    }
    const auto count_ticks = static_cast<std::uint32_t>(
        (static_cast<std::uint16_t>(stage_percentage_) + 2U) / 3U);
    if (displayed_stage_percentage_ == stage_percentage_
        && ((flow_ticks_ >= 20U
                && (input.pressed & (starfox::input::a | starfox::input::b
                    | starfox::input::start)) != 0U)
            || flow_ticks_ >= count_ticks + 60U)) {
        finish_stage_results();
    }
    result.audio_port_writes = map_.take_apu_port_writes();
    return result;
}

void GameSimulation::finish_stage_results() {
    const auto pointer = map_.read_native_word(percentage_pointer_);
    map_.write_native_byte(percentage_buffer_ + pointer, stage_percentage_);
    map_.write_native_word(percentage_pointer_,
        static_cast<std::uint16_t>(pointer + 1U));
    const auto next_map = selected_route_stage(map_.read_native_word(stage_));
    enter_planet_map(false, next_map);
}

void GameSimulation::service_transfer_request() {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    auto flags = map_.read_native_byte(background_flags_);
    if ((flags & 1U) != 0U) {
        // TRANS.ASM services this before background and info requests. The
        // original routine rebuilds the object lists and advances WORLD.ASM's
        // map interpreter from its saved checkpoint, so import those native
        // registers before returning to the host interpreter.
        map_.call_native_routine(restart_, registers, 20'000'000, true);
        map_.restore_map_state_from_native();
        refresh_player_reference();
        // These are also cleared by INITGAME_STRATS_L/PLAYERSTART_INIT_L in
        // the source. Mirror the postcondition here because the host resumes
        // between those native routines rather than through TRANS.ASM's
        // uninterrupted restart frame.
        map_.write_native_byte(game_flags_, static_cast<std::uint8_t>(
            map_.read_native_byte(game_flags_) & ~static_cast<std::uint8_t>(0x42U)));
        set_player_control(true);
        paused_ = false;
        flags = map_.read_native_byte(background_flags_);
    }
    if ((flags & 4U) != 0U) {
        registers = {};
        registers.status = 0x24U;
        map_.call_native_routine(
            do_background_request_, registers, 10'000'000, true);
        map_.complete_background_request();
    }
    flags = map_.read_native_byte(background_flags_);
    if ((flags & 8U) != 0U) {
        registers = {};
        registers.status = 0x24U;
        map_.call_native_routine(set_background_info_request_, registers, 1'000'000);
    }
    // transswap clears all three request bits together after servicing them.
    map_.write_native_byte(background_flags_, static_cast<std::uint8_t>(
        map_.read_native_byte(background_flags_) & ~static_cast<std::uint8_t>(13U)));
}

void GameSimulation::calculate_view() {
    const auto read_word = [this](std::uint32_t address) {
        return signed_word(map_.read_native_word(address));
    };
    const auto write_word = [this](std::uint32_t address, std::int16_t value) {
        map_.write_native_word(address, std::bit_cast<std::uint16_t>(value));
    };
    auto rotation_x = read_word(output_rotation_);
    if (map_.read_native_byte(no_x_rotation_) != 0U) {
        rotation_x = 0;
        write_word(output_rotation_, 0);
    }
    auto rotation_y = subtract16(
        read_word(output_rotation_ + 2U), read_word(player_turn_rotation_));
    auto rotation_z = subtract16(
        read_word(output_rotation_ + 4U), read_word(player_roll_));
    if (map_.read_native_byte(do_z_rotation_) == 0U) rotation_z = 0;

    if ((map_.read_native_byte(view_type_) & 2U) == 0U) {
        std::array<std::int16_t, 3> position{};
        for (std::size_t index = 0; index < 3U; ++index) {
            const auto shake = std::bit_cast<std::int8_t>(
                map_.read_native_byte(view_shake_ + static_cast<std::uint32_t>(index)));
            position[index] = add16(
                read_word(previous_view_position_ + static_cast<std::uint32_t>(index * 2U)),
                shake);
        }
        position[0] = add16(position[0], read_word(view_float_));
        position[1] = add16(position[1], read_word(view_float_ + 2U));
        position[2] = add16(position[2], read_word(previous_view_z_offset_));

        const auto pitch_matrix = rotation_matrix_q15(trigonometry_,
            wrap16(-static_cast<std::int32_t>(rotation_x)), 0, 0);
        auto offset = transform_q15(pitch_matrix,
            {0, 0, wrap16(-static_cast<std::int32_t>(read_word(output_distance_)))});
        const auto yaw_matrix = rotation_matrix_q15(trigonometry_, 0,
            wrap16(-static_cast<std::int32_t>(rotation_y)), 0);
        offset = transform_q15(yaw_matrix, offset);
        for (std::size_t index = 0; index < 3U; ++index) {
            write_word(view_position_ + static_cast<std::uint32_t>(index * 2U),
                add16(position[index], offset[index]));
        }
        write_word(view_rotation_, rotation_x);
        write_word(view_rotation_ + 2U, rotation_y);
        write_word(view_rotation_ + 4U, rotation_z);
    } else {
        rotation_x = read_word(view_rotation_);
        rotation_y = read_word(view_rotation_ + 2U);
        rotation_z = read_word(view_rotation_ + 4U);
    }

    for (std::size_t index = 0; index < 3U; ++index) {
        write_word(view_block_ + 12U + static_cast<std::uint32_t>(index * 2U),
            read_word(view_position_ + static_cast<std::uint32_t>(index * 2U)));
    }
    // GETVIEW_L always publishes VIEWBLK as VIEWPT after resolving either
    // a following camera or a fixed-position camera. Native strategies use
    // that synthetic object for distance gates (notably Corneria's gradual
    // ExitBase pullback), as well as positional sound. Leaving VIEWPT on the
    // player made those gates read a zero distance and collapse instantly.
    map_.write_native_word(view_point_, view_block_);
    if ((map_.read_native_byte(view_type_) & 1U) != 0U) {
        const auto target = map_.read_native_word(view_to_object_);
        Wdc65816Registers registers;
        registers.x = view_block_;
        registers.y = target;
        registers.status = 0x04U;
        map_.call_native_routine(x_angle_, registers);
        rotation_x = wrap16(-static_cast<std::int32_t>(signed_word(registers.a)));
        write_word(view_rotation_, rotation_x);
        write_word(output_rotation_, rotation_x);

        registers = {};
        registers.x = view_block_;
        registers.y = target;
        registers.status = 0x04U;
        map_.call_native_routine(y_angle_, registers);
        rotation_y = signed_word(registers.a);
        rotation_z = read_word(output_rotation_ + 4U);
        write_word(view_rotation_ + 2U, rotation_y);
        write_word(output_rotation_ + 2U, rotation_y);
        write_word(view_rotation_ + 4U, rotation_z);
    }

    const auto world = rotation_matrix_q15(
        trigonometry_, rotation_x, rotation_y, rotation_z);
    for (std::size_t index = 0; index < world.size(); ++index) {
        write_word(matrix_ + static_cast<std::uint32_t>(index * 2U), world[index]);
        write_word(world_matrix_ + static_cast<std::uint32_t>(index * 2U), world[index]);
    }

    // The tail of GETVIEW_L projects a point 500 world units along the
    // player's current aim and publishes its displacement from the Super FX
    // vanishing point. DO_CROSSHAIR consumes ARSEBANDX/Y later in the same
    // source frame. The host replaces the Super FX call above, so it must also
    // reproduce this output; otherwise the first-person reticle is frozen at
    // its initial centre even though the player is turning.
    if (objects_.is_active(player_)) {
        const auto& player = objects_.at(player_);
        const auto aim_matrix = rotation_matrix_q15(trigonometry_,
            static_cast<std::int16_t>(
                static_cast<std::uint16_t>(player.rotation_x) << 8U),
            static_cast<std::int16_t>(
                static_cast<std::uint16_t>(player.rotation_y) << 8U),
            0);
        const auto aim_offset = transform_q15(aim_matrix, {0, 0, 500});
        auto relative = transform_q15(world, {
            subtract16(add16(player.world_x, aim_offset[0]),
                signed_word(map_.read_native_word(view_position_))),
            subtract16(add16(player.world_y, aim_offset[1]),
                signed_word(map_.read_native_word(view_position_ + 2U))),
            subtract16(add16(player.world_z, aim_offset[2]),
                signed_word(map_.read_native_word(view_position_ + 4U))),
        });
        if (map_.read_native_byte(secondary_player_fly_mode_) == 3U) {
            relative[1] = add16(relative[1], 50);
        }
        const auto project_displacement = [](std::int16_t coordinate,
                                              std::int16_t depth) {
            if (depth == 0) depth = 1;
            const auto quotient = static_cast<std::int64_t>(coordinate) * 256
                / static_cast<std::int64_t>(depth);
            return wrap16(std::clamp<std::int64_t>(
                quotient, -16'383, 16'383));
        };
        map_.write_native_word(crosshair_x_, static_cast<std::uint16_t>(
            project_displacement(relative[0], relative[2])));
        map_.write_native_word(crosshair_y_, static_cast<std::uint16_t>(
            project_displacement(relative[1], relative[2])));
    }
}

std::size_t GameSimulation::update_view_flags_and_cull() {
    constexpr std::uint8_t view_flag_mask = 0x02U | 0x04U | 0x08U | 0x10U;
    constexpr std::uint8_t front_and_in_view = 0x08U | 0x10U;
    constexpr std::uint8_t left_of_view = 0x04U;
    constexpr std::uint8_t remove_behind = 0x08U;
    constexpr std::uint8_t first_frame = 0x04U;

    const std::array<std::int16_t, 3> camera{
        signed_word(map_.read_native_word(view_position_)),
        signed_word(map_.read_native_word(view_position_ + 2U)),
        signed_word(map_.read_native_word(view_position_ + 4U)),
    };
    MatrixQ15 world{};
    for (std::size_t index = 0; index < world.size(); ++index) {
        world[index] = signed_word(map_.read_native_word(
            world_matrix_ + static_cast<std::uint32_t>(index * 2U)));
    }

    struct DrawEntry {
        ObjectHandle handle{};
        std::int16_t sort_depth{};
    };
    std::vector<DrawEntry> ordered;
    std::vector<ObjectHandle> removals;
    for (const auto handle : objects_.active_handles()) {
        auto& object = objects_.at(handle);
        // showview jumps over invisible objects before touching their cached
        // player-relative flags or considering behind-view removal.
        if ((object.strategy_flags[3] & 0x08U) != 0U) continue;
        object.flags &= static_cast<std::uint8_t>(~view_flag_mask);
        const auto position = transform_q15(world, {
            subtract16(object.world_x, camera[0]),
            subtract16(object.world_y, camera[1]),
            subtract16(object.world_z, camera[2]),
        });
        // The enabled retail marioshowview/mallrotzsort path builds its
        // linked list once per source frame. Preserve its 16-bit additions,
        // 15,000 ground bias and stable equal-depth insertion here so 60 Hz
        // interpolation can never change membership or ordering.
        auto sort_depth = position[2];
        if (object.shape != 0U) {
            sort_depth = add16(sort_depth, rom_->read_i16(
                static_cast<std::uint32_t>(object.shape) + 5U));
        }
        if ((object.type & 0x01U) != 0U) {
            sort_depth = add16(sort_depth, 15'000);
        }
        const DrawEntry entry{handle, sort_depth};
        auto insertion = ordered.begin();
        while (insertion != ordered.end()) {
            // GSU CMP/BPL tests bit 15 of existing-current. It deliberately
            // has wrapping signed-word behavior rather than a C++ total-order
            // comparison; equal entries remain in source list order.
            const auto difference = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(insertion->sort_depth)
                - static_cast<std::uint16_t>(entry.sort_depth));
            if ((difference & 0x8000U) != 0U) break;
            ++insertion;
        }
        ordered.insert(insertion, entry);

        if (object.shape == 0U) continue;
        const auto z_max = rom_->read_i16(
            static_cast<std::uint32_t>(object.shape) + 14U);
        if (add16(position[2], z_max) >= 0) {
            object.flags |= front_and_in_view;
            if (position[0] < 0) object.flags |= left_of_view;
            continue;
        }
        if ((map_.read_native_byte(game_flags_) & 0x01U) != 0U
            || (object.collision_flags & first_frame) != 0U
            || (object.type & remove_behind) == 0U) {
            continue;
        }
        removals.push_back(handle);
    }

    draw_order_.clear();
    draw_order_.reserve(ordered.size());
    for (const auto& entry : ordered) draw_order_.push_back(entry.handle);

    std::size_t instructions = 0;
    for (const auto handle : removals) {
        if (!objects_.is_active(handle)) continue;
        instructions += map_.call_native_object_routine(
            remove_dead_, handle, 0x7eU, 0x24U, 1'000'000);
    }
    return instructions;
}

GameTickResult GameSimulation::tick(const input::TickInput& input) {
    if (flow_state_ == GameFlowState::finished) return {};
    complete_video_phases_for_tick();
    if (flow_state_ == GameFlowState::gameplay
        || flow_state_ == GameFlowState::training
        || flow_state_ == GameFlowState::game_over) {
        ++source_update_sequence_;
    }
    if (flow_state_ == GameFlowState::pregame_menu) {
        return tick_pregame_menu(input);
    }
    if (flow_state_ == GameFlowState::planet_select
        || flow_state_ == GameFlowState::planet_travel) {
        return tick_planet_map(input);
    }
    if (flow_state_ == GameFlowState::continue_choice) {
        return tick_continue_screen(input);
    }
    if (flow_state_ == GameFlowState::stage_results) {
        return tick_stage_results(input);
    }
    constexpr std::uint32_t spc_clocks_per_tick = 1'024'000U / 20U;
    const auto video_phases_per_tick = current_tick_video_phases_;
    if (paused_) {
        write_input(input);
        GameTickResult result;
        if ((input.pressed & starfox::input::start) != 0U) {
            paused_ = false;
            map_.write_native_byte(pause_sound_, 1U);
        }
        for (std::size_t phase = 0; phase < video_phases_per_tick; ++phase) {
            map_.set_apu_clock_offset(static_cast<std::uint32_t>(
                phase * spc_clocks_per_tick / video_phases_per_tick));
            service_audio_irq(result.sound_effect_commands);
        }
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }
    const auto pause_after_tick = flow_state_ == GameFlowState::gameplay
        && (input.pressed & starfox::input::start) != 0U
        && map_.read_native_byte(single_step_) == 0U
        && (map_.read_native_byte(player_ship_flags_) & 0x20U) == 0U
        && (map_.read_native_byte(boss_flags_) & 0x10U) == 0U
        && (map_.read_native_byte(player_strategy_flags_) & 0x20U) == 0U
        && map_.read_native_byte(doing_wipe_) == 0U
        && map_.read_native_byte(stay_black_) == 0xffU;
    if (pause_after_tick) map_.write_native_byte(pause_sound_, 2U);
    // build_drawlist copies hitflash into the just-submitted frame and clears
    // it from al_sflags. Presentation consumes object state after tick(), so
    // perform that clear at the following boundary: the flag remains visible
    // for exactly the three 60 Hz presentations belonging to one logic tick.
    for (const auto handle : objects_.active_handles()) {
        objects_.at(handle).strategy_flags[0] &= static_cast<std::uint8_t>(~0x02U);
    }
    // MDRAWLIS clears m_bossHP after every source frame. Strategies rebuild
    // it by summing the surviving boss components during this update.
    map_.write_native_word(boss_health_, 0U);
    map_.write_native_byte(previous_video_frame_count_,
        map_.read_native_byte(video_frame_counter_));
    map_.write_native_byte(video_frame_counter_, 0U);
    write_input(input);
    GameTickResult result;
    Wdc65816Registers registers;
    registers.status = 0x24U;
    // MAIN/CONT/ENDSEQ call SETBLACK_L once around every source transfer.
    // Besides drawing the black colour window it releases STAYBLACK, which
    // is also the native movement gate used after a death restart.
    result.prelude_instructions += map_.call_native_routine(
        set_black_, registers, 2'000'000, true);
    registers = {};
    registers.status = 0x24U;
    // TRANS.ASM advances the colour-math/window program before any
    // background work. Strategies only select a circle table; this routine
    // owns its exact radius, colour and lifetime timing.
    // The original routine also calls ROTPROJ_L when CIRCLEOBJ is non-zero.
    // Its logarithmic projection has an unbounded zero-coordinate edge case
    // which can be reached by the host Super FX bridge as a boss disappears.
    // Presentation already projects the same object below using the complete
    // host view matrix, so select TRANS.ASM's centre-screen branch while it
    // advances the animation and restore the tracked object while the circle
    // remains active. This avoids both the redundant projection and its
    // cartridge-side infinite loop without changing the circle program.
    const auto tracked_circle_object = map_.read_native_word(circle_object_);
    if (tracked_circle_object != 0U) {
        map_.write_native_word(circle_object_, 0U);
    }
    try {
        result.prelude_instructions += map_.call_native_routine(
            do_circle_explosion_, registers, 5'000'000, true);
    } catch (...) {
        if (tracked_circle_object != 0U) {
            map_.write_native_word(circle_object_, tracked_circle_object);
        }
        throw;
    }
    if (tracked_circle_object != 0U
        && map_.read_native_word(circle_animation_) != 0U) {
        map_.write_native_word(circle_object_, tracked_circle_object);
    }
    // Later Super FX calls reuse M_BIGX/Y as scratch registers, so capture
    // the circle centre at the same boundary where MCALC_CIRCLE consumes it.
    // These coordinates address the complete 256x224 SNES colour window;
    // only the Super FX character layer is inset to 224x192.
    auto circle_x = std::int16_t{128};
    auto circle_y = std::int16_t{112};
    const auto circle_handle = handle_from_native_pointer(
        map_.read_native_word(circle_object_));
    if (circle_handle != 0U) {
        const auto& object = objects_.at(circle_handle);
        const auto pitch = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(object.rotation_x) << 8U);
        const auto yaw = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(object.rotation_y) << 8U);
        const auto centre_offset = transform_q15(
            rotation_matrix_q15(trigonometry_, pitch, yaw, 0), {0, 0, -30});
        const std::array<std::int16_t, 3> relative{
            add16(add16(object.world_x, centre_offset[0]),
                -static_cast<std::int16_t>(map_.read_native_word(view_position_))),
            add16(add16(object.world_y, centre_offset[1]),
                -static_cast<std::int16_t>(map_.read_native_word(view_position_ + 2U))),
            add16(add16(object.world_z, centre_offset[2]),
                -static_cast<std::int16_t>(map_.read_native_word(view_position_ + 4U))),
        };
        MatrixQ15 view{};
        for (std::size_t index = 0; index < view.size(); ++index) {
            view[index] = static_cast<std::int16_t>(map_.read_native_word(
                world_matrix_ + static_cast<std::uint32_t>(index * 2U)));
        }
        const auto camera = transform_q15(view, relative);
        const auto project = [](std::int16_t coordinate, std::int16_t depth) {
            if (depth == 0) depth = 1;
            return wrap16(static_cast<std::int32_t>(coordinate) * 256 / depth);
        };
        circle_x = add16(add16(static_cast<std::int16_t>(
            map_.read_native_word(vanish_x_)), project(camera[0], camera[2])), 16);
        circle_y = add16(add16(static_cast<std::int16_t>(
            map_.read_native_word(vanish_y_)), project(camera[1], camera[2])), 16);
        // ROTPROJ_L's current Super FX bridge leaves M_BIGX/Y holding scratch
        // matrix products. Keep the cartridge-visible result coherent too.
        map_.write_native_word(circle_centre_x_, static_cast<std::uint16_t>(circle_x));
        map_.write_native_word(circle_centre_y_, static_cast<std::uint16_t>(circle_y));
    }
    circle_effect_ = {
        map_.read_native_word(circle_animation_) != 0U,
        circle_x,
        circle_y,
        map_.read_native_word(circle_radius_),
        map_.read_native_byte(circle_source_red_),
        map_.read_native_byte(circle_source_green_),
        map_.read_native_byte(circle_source_blue_),
        map_.read_native_byte(circle_affected_layers_),
    };
    registers = {};
    registers.status = 0x24U;
    result.prelude_instructions += map_.call_native_routine(
        calculate_background_scroll_, registers);
    registers = {};
    registers.status = 0x24U;
    result.prelude_instructions += map_.call_native_routine(
        calculate_background_vertical_offsets_, registers);
    map_.set_bg2_vertical_offsets_enabled(
        map_.read_native_byte(vertical_offsets_enabled_) != 0U);
    if (map_.ppu_state().bg2_vertical_offsets_enabled) {
        registers = {};
        registers.status = 0x24U;
        result.prelude_instructions += map_.call_native_routine(
            upload_background_vertical_offsets_, registers, 1'000'000);
    }
    const auto horizontal_offsets_enabled =
        map_.read_native_byte(horizontal_offsets_enabled_) != 0U;
    if (horizontal_offsets_enabled) {
        registers = {};
        registers.status = 0x24U;
        result.prelude_instructions += map_.call_native_routine(
            calculate_background_horizontal_offsets_, registers, 1'000'000);
        registers = {};
        registers.status = 0x24U;
        result.prelude_instructions += map_.call_native_routine(
            upload_background_horizontal_offsets_, registers, 1'000'000);
    }
    map_.capture_bg2_horizontal_offsets(
        map_.read_native_word(horizontal_offsets_buffer_),
        horizontal_offsets_enabled);
    result.prelude_instructions += strategies_.begin_tick();
    for (std::size_t phase = 0; phase < video_phases_per_tick; ++phase) {
        map_.set_apu_clock_offset(static_cast<std::uint32_t>(
            phase * spc_clocks_per_tick / video_phases_per_tick));
        map_.write_native_byte(video_frame_counter_, static_cast<std::uint8_t>(
            map_.read_native_byte(video_frame_counter_) + 1U));
        service_audio_irq(result.sound_effect_commands);
    }
    map_.set_apu_clock_offset(
        (video_phases_per_tick - 1U) * spc_clocks_per_tick
            / video_phases_per_tick + 1U);
    // This is TRANS.ASM's exact ordering: INIT_STRATS_L, UPDATE_OBJECTS_L,
    // then the active strategy list. WORLD.ASM therefore owns map distance,
    // bytecode dispatch, native call stacks and loop state during gameplay.
    registers = {};
    registers.status = 0x24U;
    result.prelude_instructions += map_.call_native_routine(
        update_objects_, registers, 10'000'000, true);
    map_.restore_map_state_from_native();
    refresh_player_reference();
    apply_god_mode_state();
    const auto god_mode_bombs_before = god_mode_
        ? map_.read_native_word(special_weapon_count_) : std::uint16_t{};
    std::vector<ObjectHandle> nukes_before_strategies;
    if (god_mode_
        && (input.pressed & starfox::input::a) != 0U) {
        for (const auto handle : objects_.active_handles()) {
            if (objects_.at(handle).shape == nuke_shape_) {
                nukes_before_strategies.push_back(handle);
            }
        }
    }
    result.strategies = strategies_.tick_all();
    refresh_player_reference();
    apply_god_mode_state();
    if (god_mode_) {
        map_.write_native_word(special_weapon_count_, std::max(
            god_mode_bombs_before,
            map_.read_native_word(special_weapon_count_)));
    }
    service_god_nuke(input, nukes_before_strategies);

    // TRANS.ASM snapshots these after strategies for release-edge controls
    // such as view toggling.
    map_.write_native_byte(last_controller_high_,
                           map_.read_native_byte(controller_high_));
    map_.write_native_byte(last_controller_low_,
                           map_.read_native_byte(controller_low_));

    // GETVIEW_L delegates its matrices and camera offset to Super FX. Model
    // that fixed-point path natively; running it against the temporary
    // coprocessor-complete stub would reuse stale m_wmat/m_big values.
    calculate_view();
    {
        const std::array<std::int16_t, 3> camera{
            signed_word(map_.read_native_word(view_position_)),
            signed_word(map_.read_native_word(view_position_ + 2U)),
            signed_word(map_.read_native_word(view_position_ + 4U)),
        };
        MatrixQ15 world{};
        for (std::size_t index = 0; index < world.size(); ++index) {
            world[index] = signed_word(map_.read_native_word(
                world_matrix_ + static_cast<std::uint32_t>(index * 2U)));
        }
        dust_.tick(camera, world, map_.dots_mode() < 0);
    }
    // showview also owns the per-object front/left/in-view flags and removal
    // of ATZREMOVE objects whose complete model has passed behind the camera.
    // Omitting this eventually exhausts all 70 alien slots on longer stages.
    result.prelude_instructions += update_view_flags_and_cull();
    // MDRAWLIS.MC creates, advances and ages the persistent GSU particle
    // pool once per submitted source frame, after the ordered object list.
    particles_.tick(objects_, map_.read_native_word(particles_enabled_) != 0U);
    // The remaining 65C816 presentation state is safe to execute directly:
    // positional/engine audio, palette transitions and the exact HUD/OAM
    // command builder.
    registers = {};
    registers.status = 0x24U;
    result.prelude_instructions += map_.call_native_routine(
        do_sounds_, registers, 5'000'000, true);
    for (const auto routine : {palette_goto_, fade_palette_, do_sprites_}) {
        registers = {};
        registers.status = 0x24U;
        result.prelude_instructions += map_.call_native_routine(
            routine, registers, 5'000'000, true);
    }
    if (flow_state_ == GameFlowState::gameplay
        || flow_state_ == GameFlowState::training) {
        // MAIN.ASM calls this immediately after TRANS_L. Its Super FX work is
        // host-rendered, but the original 65C816 routine still controls the
        // portrait animation, text lifetime and voice/effect commands.
        registers = {};
        registers.status = 0x24U;
        result.prelude_instructions += map_.call_native_routine(
            friends_messages_, registers, 5'000'000, true);
    }
    const auto current_palette = palette_words();
    map_.write_cgram(7U * 16U, current_palette);
    map_.upload_oam(ram_symbol("SPRITEBLK"), 544U);
    calculate_meters();

    // TRANS.ASM builds collisions from the post-strategy object positions,
    // then resolves them in RAM while the Super FX draws. Strategies consume
    // those flags on the following 20 Hz update.
    registers = {};
    registers.status = 0x24U;
    result.prelude_instructions += map_.call_native_routine(
        generate_collision_list_, registers, 5'000'000);
    registers = {};
    registers.status = 0x24U;
    result.prelude_instructions += map_.call_native_routine(
        resolve_collisions_, registers, 10'000'000);
    service_transfer_request();
    refresh_player_reference();
    service_level_exit();

    // Finish TRANS.ASM's frame accounting. FRAMERATE is deliberately the
    // just-completed NMI count and is consumed by framescalevecs next tick.
    map_.write_native_byte(strategy_frame_rate_,
        map_.read_native_byte(video_frame_counter_));
    auto frame_count = static_cast<std::uint16_t>(
        map_.read_native_byte(frame_count_)
        + map_.read_native_byte(previous_video_frame_count_));
    auto rendered_frames = static_cast<std::uint8_t>(
        map_.read_native_byte(rendered_frame_count_) + 1U);
    if (frame_count >= 60U) {
        map_.write_native_byte(measured_frame_rate_, rendered_frames);
        rendered_frames = 0U;
        frame_count = static_cast<std::uint16_t>(frame_count - 60U);
    }
    map_.write_native_byte(frame_count_, static_cast<std::uint8_t>(frame_count));
    map_.write_native_byte(rendered_frame_count_, rendered_frames);
    if (flow_state_ == GameFlowState::game_over) {
        ++flow_ticks_;
        // MAIN.ASM presents 50 transfers, then accepts START (or waits up to
        // 60 seconds) before opening FOXY_CONTINUE_L.
        if (flow_ticks_ >= 50U
            && ((input.pressed & starfox::input::start) != 0U
                || flow_ticks_ >= 1'250U)) {
            enter_continue_screen();
        }
    } else if (flow_state_ == GameFlowState::title) {
        ++flow_ticks_;
        if (frontend_phase_ == FrontendPhase::title_fade_to_controls
            && map_.fade_direction() == 0
            && map_.display_brightness() == 0U) {
            map_.write_native_byte(controls_exit_, 0U);
            map_.write_native_byte(default_training_, 0U);
            enter_controls(GameFlowState::controls_type);
        } else if (frontend_phase_ == FrontendPhase::title_fade_to_intro
                   && map_.fade_direction() == 0
                   && map_.display_brightness() == 0U) {
            enter_intro();
        }
        // TITLESEQ_L ignores START until GAMEFRAME reaches 40, then enters
        // CONT.ASM's controller/training selection screen.
        if (frontend_phase_ == FrontendPhase::none && flow_ticks_ >= 40U
            && (input.pressed & starfox::input::start) != 0U) {
            queue_sound_effect(0x10U);
            request_music(0xf1U);
            map_.start_display_fade(-3);
            frontend_phase_ = FrontendPhase::title_fade_to_controls;
        } else if (frontend_phase_ == FrontendPhase::none
                   && flow_ticks_ >= 880U) {
            request_music(0xf1U);
            map_.start_display_fade(-3);
            frontend_phase_ = FrontendPhase::title_fade_to_intro;
        }
    } else if (flow_state_ == GameFlowState::intro) {
        ++flow_ticks_;
        if (frontend_phase_ == FrontendPhase::intro_fade_to_title
            && map_.fade_direction() == 0
            && map_.display_brightness() == 0U) {
            enter_title();
        } else if (frontend_phase_ == FrontendPhase::none && flow_ticks_ >= 30U
            && (input.pressed != 0U || input.held != 0U
                || map_.read_native_byte(exit_intro_) != 0U)) {
            // INTRO_L explicitly starts its quick fade from brightness 11,
            // even when the user skips while the screen is fully bright.
            map_.set_display_brightness(11U);
            map_.start_display_fade(-2);
            frontend_phase_ = FrontendPhase::intro_fade_to_title;
        }
    } else if (flow_state_ == GameFlowState::controls_type) {
        ++flow_ticks_;
        if ((input.pressed & starfox::input::select) != 0U) {
            map_.write_native_byte(control_type_, static_cast<std::uint8_t>(
                (map_.read_native_byte(control_type_) + 1U) & 3U));
            queue_sound_effect(0x11U);
        }
        Wdc65816Registers registers;
        registers.status = 0x24U;
        map_.call_native_near_routine(set_control_type_, registers);
        if (flow_ticks_ >= 16U
            && (input.pressed & starfox::input::start) != 0U) {
            map_.write_native_byte(controls_exit_, 0U);
            flow_ticks_ = 0U;
            flow_state_ = GameFlowState::controls_choice;
            set_player_control(false);
            queue_sound_effect(0x10U);
        }
        update_control_screen_sprites();
    } else if (flow_state_ == GameFlowState::controls_choice) {
        if (frontend_phase_ == FrontendPhase::controls_fade_to_training
            && map_.fade_direction() == 0
            && map_.display_brightness() == 0U) {
            enter_training();
        } else if (frontend_phase_ == FrontendPhase::controls_fade_to_map
                   && map_.fade_direction() == 0
                   && map_.display_brightness() == 0U) {
            start_initial_route();
        }
        auto selection = map_.read_native_byte(controls_exit_) != 0U
            ? std::uint8_t{1U} : std::uint8_t{};
        const auto previous_selection = selection;
        if ((input.pressed & starfox::input::select) != 0U) selection ^= 1U;
        if ((input.pressed & starfox::input::up) != 0U) selection = 0U;
        if ((input.pressed & starfox::input::down) != 0U) selection = 1U;
        map_.write_native_byte(controls_exit_, selection);
        if (frontend_phase_ == FrontendPhase::none
            && selection != previous_selection) queue_sound_effect(0x11U);
        if (frontend_phase_ == FrontendPhase::none
            && (input.pressed & (starfox::input::x | starfox::input::y)) != 0U) {
            flow_ticks_ = 0U;
            flow_state_ = GameFlowState::controls_type;
            set_player_control(true);
            update_control_screen_sprites();
        } else if (frontend_phase_ == FrontendPhase::none
                   && (input.pressed & (starfox::input::a | starfox::input::b
                       | starfox::input::start)) != 0U) {
            queue_sound_effect(0x10U);
            request_music(0xf1U);
            map_.start_display_fade(-1);
            frontend_phase_ = selection != 0U
                ? FrontendPhase::controls_fade_to_map
                : FrontendPhase::controls_fade_to_training;
        } else {
            update_control_screen_sprites();
        }
    } else if (flow_state_ == GameFlowState::training) {
        ++flow_ticks_;
        if (frontend_phase_ == FrontendPhase::training_fade_to_controls
            && map_.fade_direction() == 0
            && map_.display_brightness() == 0U) {
            Wdc65816Registers registers;
            registers.status = 0x24U;
            map_.call_native_routine(initialize_all_, registers, 5'000'000);
            map_.write_native_byte(default_training_, 1U);
            enter_controls(GameFlowState::controls_choice, 1U);
        } else if (frontend_phase_ == FrontendPhase::none && flow_ticks_ >= 20U
            && (input.pressed & starfox::input::start) != 0U) {
            request_music(0xf1U);
            map_.start_display_fade(-1);
            frontend_phase_ = FrontendPhase::training_fade_to_controls;
        }
    }
    if (pause_after_tick && flow_state_ == GameFlowState::gameplay) paused_ = true;
    result.audio_port_writes = map_.take_apu_port_writes();
    return result;
}

} // namespace starfox::simulation
