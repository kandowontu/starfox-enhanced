#include "starfox/simulation/game_simulation.hpp"

#include "starfox/assets/decrunch.hpp"
#include "starfox/input/buttons.hpp"

#include <algorithm>
#include <cctype>
#include <bit>
#include <stdexcept>

namespace starfox::simulation {
namespace {

constexpr std::array<std::uint16_t, 8> kPresentationRates{
    20U, 30U, 60U, 90U, 120U, 240U, 360U, 480U};
constexpr std::array<DisplayMode, 5> kDisplayModes{
    DisplayMode::standard_4_3,
    DisplayMode::widescreen_16_9,
    DisplayMode::widescreen_16_10,
    DisplayMode::ultrawide_21_9,
    DisplayMode::super_ultrawide_32_9,
};
constexpr std::array<CrosshairColour, 8> kCrosshairColours{
    CrosshairColour::green,
    CrosshairColour::white,
    CrosshairColour::blue,
    CrosshairColour::red,
    CrosshairColour::yellow,
    CrosshairColour::cyan,
    CrosshairColour::magenta,
    CrosshairColour::orange,
};

std::int16_t signed_word(std::uint16_t value) noexcept {
    return std::bit_cast<std::int16_t>(value);
}

std::size_t native_object_capacity(const assets::SymbolMap& symbols) {
    const auto& values = symbols.find("NUMBER_AL");
    if (values.empty()) {
        throw std::runtime_error{"missing game constant: NUMBER_AL"};
    }
    return values.front();
}

ObjectMemoryLayout native_object_layout(const assets::SymbolMap& symbols) {
    // Star Fox EX replaces retail al_weapontype with al_weaponnum/openal and
    // inserts alx_frame before the draw-state fields.  These exported names
    // identify that concrete STRUCTS.INC layout without relying on a build
    // label or ROM checksum.
    return !symbols.find("AL_WEAPONNUM").empty()
        && !symbols.find("ALX_FRAME").empty()
        ? ObjectMemoryLayout::starfox_ex
        : ObjectMemoryLayout::original;
}

} // namespace

GameSimulation::GameSimulation(
    const assets::RomImage& rom,
    const assets::SymbolMap& symbols,
    const std::string& initial_map,
    std::span<const std::uint8_t> cartridge_ram,
    bool source_initialize_direct_map)
    : rom_(&rom),
      symbols_(&symbols),
      objects_(native_object_capacity(symbols), native_object_layout(symbols)),
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
      ppu_palette_(ram_symbol("PAL0PALETTE")),
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
      do_a_wipe_(ram_symbol("DOAWIPE")),
      stay_black_(ram_symbol("STAYBLACK")),
      background_music_count_(ram_symbol("BGMCNT")),
      background_music_command_(ram_symbol("BGM_MUSIC")),
      fade_to_red_(ram_symbol("FADETORED")),
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
      do_window_wipe_(rom_symbol("DO_WINDOW_WIPE_L")),
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
      actual_route_(0U),
      current_planet_(ram_symbol("CURRENTPLANET")),
      current_level_(ram_symbol("CURRENTLEVEL")),
      new_map_(ram_symbol("NEWMAP")),
      pepper_message_(ram_symbol("PEPPERMSG")),
      stage_paths_(rom_symbol("STAGEPATHS")),
      initialize_game_(rom_symbol("INITGAME_L")),
      initialize_all_(rom_symbol("INITIALISE_L")),
      initialize_all_2_(0U),
      first_download_(ram_symbol("FIRSTDNLD")),
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
      wipe_logic_(ram_symbol("M_WINWBGLOG")),
      wipe_left_buffer_(ram_symbol("M_WINBUF")),
      wipe_right_buffer_(ram_symbol("M_WINBUF2")),
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
    first_planet_campaign_ = {
        initialize_planets_, setup_planets_, setup_planet_palette_,
        copy_planet_light_, draw_planet_sprites_, draw_selected_planet_,
        draw_planet_in_centre_, clear_planet_screen_, dma_planet_screen_,
        switch_planet_buffer_, draw_route_name_, draw_planet_lines_,
        undraw_planet_lines_, move_ship_along_path_, start_planet_positions_,
        planet_sprites_, planet_positions_,
    };
    const auto find_optional_rom = [&symbols](const char* name) {
        for (const auto address : symbols.find(name)) {
            if ((address & 0xffffU) >= 0x8000U
                && ((address >> 16U) & 0xffU) < 0x7eU) return address;
        }
        return std::uint32_t{};
    };
    const auto find_optional_ram = [&symbols](const char* name) {
        for (const auto address : symbols.find(name)) {
            if ((address >> 16U) == 0U || (address >> 16U) == 0x7eU) {
                return address;
            }
        }
        return std::uint32_t{};
    };
    const auto find_optional_superfx = [&symbols](const char* name) {
        for (const auto address : symbols.find(name)) {
            if ((address >> 16U) == 0x70U) return address;
        }
        return std::uint32_t{};
    };
    end_game_sequence_ = find_optional_rom("END_GAME_SEQ");
    ending_transfer_ = find_optional_rom("TRANSFER_L");
    ending_end_transfer_ = find_optional_rom("ENDTRANS");
    credits_entry_ = find_optional_rom("CREDSTUFF");
    final_total_score_ = find_optional_rom("MAKETOTALSCORE2");
    credits_music_original_ = find_optional_rom("DO_BGM_STAFF");
    credits_music_ex_ = find_optional_rom("DO_BGM_SP0RCH");
    if (credits_entry_ == 0U && end_game_sequence_ != 0U) {
        // Retail MAIN.ASM does not export a CREDSTUFF label. Its equivalent
        // boundary is the instruction immediately following JSL ENDSEQ_L:
        // score and boss roll are complete there, while credits setup has not
        // started yet. Resolve the call site from assembled code so revision
        // layout differences do not require a hard-coded PC.
        const auto end_sequence = find_optional_rom("ENDSEQ_L");
        for (std::uint32_t offset = 0U;
             end_sequence != 0U && offset < 0x400U; ++offset) {
            const auto address = end_game_sequence_ + offset;
            if (rom_->read8(address) != 0x22U) continue;
            const auto target = static_cast<std::uint32_t>(
                rom_->read8(address + 1U))
                | (static_cast<std::uint32_t>(rom_->read8(address + 2U)) << 8U)
                | (static_cast<std::uint32_t>(rom_->read8(address + 3U)) << 16U);
            if (target == end_sequence) {
                credits_entry_ = address + 4U;
                break;
            }
        }
    }
    if (find_optional_rom("PLANETSEQ2_L") != 0U) {
        starfox_ex_cartridge_ = true;
        planet_count_ = 31U;
        initialize_all_2_ = find_optional_rom("INITIALISE2_L");
        map2_flag_ = find_optional_ram("MAP2");
        actual_route_ = find_optional_ram("ACTUALROUTE");
        ex_foxy_continue_ = find_optional_rom("FOXY_CONTINUE_L");
        const auto ex_title_intro = find_optional_rom("BG_TITLEI");
        const auto ex_background_lists = find_optional_rom("BGLISTS");
        if (ex_title_intro != 0U && ex_background_lists != 0U
            && (ex_title_intro & 0xff0000U)
                == (ex_background_lists & 0xff0000U)) {
            ex_title_intro_background_ = static_cast<std::uint16_t>(
                ex_title_intro - ex_background_lists);
        }
        ex_foxy_self_ = find_optional_rom("SELF");
        ex_randomize_background_ = find_optional_rom("RANDOMIZEBG");
        ex_restart_ = find_optional_rom("RESTART");
        ex_briefing_ = find_optional_rom("BRIEFING_L");
        ex_stop_counting_ = find_optional_ram("STOPCOUNTING");
        ex_menu_selected_ = find_optional_ram("MENUSELECTED");
        ex_credits_ = find_optional_ram("CREDITS");
        ex_page_number_ = find_optional_ram("PAGENUMBER");
        ex_foxy_pointer_ = find_optional_ram("FOXY_PTR");
        ex_foxy_shape_ = find_optional_ram("FOXY_SHAPE");
        ex_model_test_shape_ = find_optional_rom("A_WING");
        ex_bg2_vertical_offset_override_ =
            find_optional_ram("BG2VOFSOVERRIDE");
        ex_fade_palette_fx_pink_ = find_optional_ram("FADEPALTOFXPINK");
        ex_fade_palette_yamao_ = find_optional_ram("FADEPALTOYAMAO");
        ex_model_double_ = find_optional_superfx("M_BIGHEADMODE");
        ex_model_quadruple_ = find_optional_superfx("M_BIGGERHEADMODE");
        ex_nan_mode_ = find_optional_superfx("M_NANMODE");
        ex_more_dots_ = find_optional_superfx("M_MOREDOTS");
        ex_meter_boost_enabled_ =
            find_optional_superfx("M_DOBOOSTMETER");
        ex_meter_player_health_width_ =
            find_optional_superfx("M_PLAYERB_HP");
        ex_meter_player_health_max_ =
            find_optional_superfx("M_PLAYERB_HPACT");
        ex_meter_damage_two_ = find_optional_superfx("M_DAMAGETWO");
        ex_meter_player_one_dead_ =
            find_optional_superfx("M_PLAYERONEDEAD");
        ex_meter_player_two_activated_ =
            find_optional_superfx("M_PLAYERTWOACTIVATED");
        ex_meter_player_two_ = find_optional_superfx("M_PLAYERTWO");
        ex_meter_two_extra_bytes_ =
            find_optional_superfx("M_TWOEXTRABYTES");
        ex_shield_up_two_ = find_optional_ram("SHIELDUPTWO");
        ex_nan_colour_tables_ = {
            find_optional_rom("NAN_C"),
            find_optional_rom("FIREBODY_C"),
            find_optional_rom("BLUELAVABODY_C"),
            find_optional_rom("STEALTH_C"),
            find_optional_rom("TREVORTEX_C"),
        };
        ex_god_mode_ = find_optional_ram("GODMODE");
        ex_controller_2_high_ = find_optional_ram("CONT1");
        ex_controller_2_low_ = find_optional_ram("CONTL1");
        ex_previous_controller_2_high_ = find_optional_ram("CONT1L");
        ex_trigger_2_ = find_optional_ram("TRIG1");
        ex_hardware_controller_2_ = find_optional_ram("JOY2L");
        ex_last_controller_2_high_ = find_optional_ram("LASTCONT1");
        ex_last_controller_2_low_ = find_optional_ram("LASTCONTL1");
        ex_multitap_mode_ = find_optional_ram("MULTITAPMODE");
        ex_number_players_ = find_optional_ram("NUMPLAYERS");
        for (std::size_t index = 0; index < ex_multitap_controllers_.size();
             ++index) {
            const auto name = std::string{"CON"} + std::to_string(index + 1U);
            ex_multitap_controllers_[index] = find_optional_ram(name.c_str());
        }
        for (std::size_t index = 0;
             index < ex_last_multitap_controllers_.size(); ++index) {
            const auto name = std::string{"LASTCON"}
                + std::to_string(index + 3U);
            ex_last_multitap_controllers_[index] =
                find_optional_ram(name.c_str());
        }
        ex_mouse_mode_ = find_optional_ram("MOUSEMODE");
        ex_mouse_connected_ = find_optional_ram("MOUSE_CON1");
        ex_mouse_y_ = find_optional_ram("MOUSE_Y1");
        ex_mouse_x_ = find_optional_ram("MOUSE_X1");
        ex_mouse_buttons_ = find_optional_ram("MOUSE_SW1");
        ex_mouse_trigger_ = find_optional_ram("MOUSE_SWT1");
        ex_mouse_previous_buttons_ = find_optional_ram("MOUSE_SB1");
        ex_scope_mode_ = find_optional_ram("SCOPEMODE");
        ex_scope_no_latch_ = find_optional_ram("SCOPE_NO_LATCH_FLAG");
        ex_scope_held_ = find_optional_ram("SCOPE_HELD");
        ex_scope_new_ = find_optional_ram("SCOPE_NEW");
        ex_scope_previous_ = find_optional_ram("SCOPE_PREV");
        ex_scope_horizontal_ = find_optional_ram("SCOPE_H");
        ex_scope_vertical_ = find_optional_ram("SCOPE_V");
        ex_ntt_mode_ = find_optional_ram("NTTMODE");
        ex_ntt_read_ = find_optional_ram("JPREAD");
        ex_ntt_trigger_ = find_optional_ram("JPTRIG");
        ex_ntt_previous_ = find_optional_ram("JPPREV");
        ex_scored_ = find_optional_ram("SCORED");
        ex_ces_timer_ = find_optional_rom("CESTIMER_L");
        ex_no_hud_ = find_optional_ram("NOHUD");
        ex_dots_stars_ = find_optional_ram("DOTSSTARS");
        ex_dots_flag_ = find_optional_ram("DOTSFLAG");
        ex_no_sfx_ = find_optional_ram("NOSFX");
        ex_no_set_port_3_ = find_optional_ram("NOSETPORT3");
        ex_bgm_sfx_ = find_optional_ram("BGMSFX");
        ex_set_new_bgm_ = find_optional_ram("SETNEWBGM");
        ex_cursed_bgm_ = find_optional_ram("CURSEDBGM");
        ex_bgm_test_ = find_optional_ram("BGMTEST");
        ex_bgm_playlist_ = find_optional_rom("BGMPLAYLIST");
        ex_bgm_playlist_cursed_ = find_optional_rom("BGMPLAYLISTCURSED");
        ex_text_pointer_ = find_optional_ram("TEXTPT");
        ex_fps_counter_enabled_ = find_optional_ram("FPSCOUNTERON");
        ex_no_objects_ = find_optional_ram("NOOBJMODE");
        ex_no_background_mode_ = find_optional_ram("NOBGMODE");
        ex_fps_speed_ = find_optional_ram("FPSSPEED");
        ex_ntsc_pal_swap_ = find_optional_ram("NTSCPALSWAP");
        ex_dark_mode_ = find_optional_ram("DARKMODE");
        ex_palette_slow_counter_ = find_optional_ram("TEMPVAL5");
        ex_palette_slower_counter_ = find_optional_ram("TEMPVAL6");
        ex_palette_every_transfer_ = {
            find_optional_rom("PALFADETOYAMAO_L"),
            find_optional_rom("PALFADETOYAMAB_L"),
            find_optional_rom("PALFADETOCORN2_L"),
            find_optional_rom("PALFADERANDOM_L"),
            find_optional_rom("PALFADETOTITLE_L"),
        };
        ex_palette_every_fourth_transfer_ = {
            find_optional_rom("PALFADETOCORNNITE_L"),
            find_optional_rom("PALFADETONEWSPACE1_L"),
            find_optional_rom("PALFADETONEWSPACE2_L"),
            find_optional_rom("PALFADETONEWSPACE3_L"),
            find_optional_rom("PALFADETONEWSPACE4_L"),
            find_optional_rom("PALFADETONEWSPACE5_L"),
            find_optional_rom("PALFADETOBLACK_L"),
            find_optional_rom("PALFADETOOOTD_L"),
        };
        ex_palette_every_eleventh_transfer_ = {
            find_optional_rom("PALFADETOFXGREEN_L"),
            find_optional_rom("PALFADETOFXDES_L"),
            find_optional_rom("PALFADETOFXPINK_L"),
            find_optional_rom("PALFADETOFXBLUE_L"),
        };
        ex_fps_text_ = find_optional_rom("FPSTEXT");
        ex_print_point_ = find_optional_ram("PRINTPT");
        ex_open_text_ = find_optional_ram("OPEN_TEXT");
        ex_print_text_ = find_optional_rom("PRINTT_L");
        ex_print_decimal_ = find_optional_rom("PRINTBD_L");
        ex_do_bgm_reset_ = find_optional_rom("DO_BGM_RESET");
        ex_do_bgm_generic_ = find_optional_rom("DO_BGM_GENERIC_L");
        ex_strat_debug_ = find_optional_rom("STRATDEBUG_L");
        ex_freeze_strategies_ = find_optional_ram("FREEZESTRATS");
        ex_debug_flash_ = find_optional_ram("DEBUGFLASH");
        ex_debug_alien_ = find_optional_ram("DEBUGALIEN");
        ex_debug_backup_ = find_optional_ram("DEBUGBACKUP");
        ex_trigger_defaults_ = find_optional_rom("TRIGGERDEFAULTS");
        ex_load_data_ = find_optional_rom("LOADDATA");
        ex_load_index_ = find_optional_ram("MEMI");
        ex_end_level_sequence_ = find_optional_rom("END_LEVEL_SEQ");
        ex_transfer_ = find_optional_rom("TRANSFER_L");
        ex_doing_end_ = find_optional_ram("DOINGEND");
        ex_crosshair_on_ = find_optional_ram("CROSSHAIRON");
        ex_current_percentage_ = find_optional_ram("CLA1");
        ex_target_percentage_ = find_optional_ram("CLA2");
        ex_results_exit_ = find_optional_ram("CLB2");
        ex_machine_type_ = find_optional_ram("MACHINETYPE");
        ex_do_palette_stuff_ = find_optional_rom("DOPALSTUFF_L");
        friends_messages_2_ = find_optional_rom("FRIENDS_MESSAGES2_L");
        friends_message_2_ = find_optional_ram("FRIENDS_MSG2");
        message_count_1_2_ = find_optional_ram("MSG_COUNT12");
        message_count_2_2_ = find_optional_ram("MSG_COUNT22");
        which_friend_2_ = find_optional_ram("WHICHFRIEND2");
        face_data_2_ = find_optional_rom("FACEDATA2");
        messages_2_ = find_optional_rom("MESSAGES2");
        select_next_ship_ = find_optional_rom("SELECTNEXTSHIP_L");
        select_previous_ship_ = find_optional_rom("SELECTPREVSHIP_L");
        ex_set_ship_ = find_optional_rom("SETSHIP");
        current_ship_ = find_optional_ram("CURR_SHIP");
        next_ship_key_down_ = find_optional_ram("KEYRDOWN");
        previous_ship_key_down_ = find_optional_ram("KEYLDOWN");
        second_planet_campaign_ = {
            find_optional_rom("INITPLANETS_LL"),
            find_optional_rom("SETUP_PLANETS_LL"),
            find_optional_rom("SETUPPLANETPAL_LL"),
            find_optional_rom("COPYLIGHT2"),
            find_optional_rom("DRAWPLANETSPRITES2"),
            find_optional_rom("DRAWSELECTEDPLANET2"),
            find_optional_rom("DRAWPLANETINCENTRE2"),
            find_optional_rom("CLEARSCREEN2"),
            find_optional_rom("DMA256SCREEN2"),
            find_optional_rom("SWITCHBUFFER2_FAST2"),
            find_optional_rom("DRAWROUTENAME2"),
            find_optional_rom("DRAWPLANETLINES_LL"),
            find_optional_rom("UNDRAWPLANETLINES_LL"),
            find_optional_rom("MOVESHIPALONGPATH2"),
            find_optional_rom("STARTPLANETPOS22"),
            find_optional_rom("PLANETSPRS2"),
            find_optional_rom("PLANETPOS2"),
        };
        const std::array required_map2_symbols{
            second_planet_campaign_.initialize,
            second_planet_campaign_.setup,
            second_planet_campaign_.setup_palette,
            second_planet_campaign_.copy_light,
            second_planet_campaign_.draw_sprites,
            second_planet_campaign_.draw_selected,
            second_planet_campaign_.draw_centred,
            second_planet_campaign_.clear_screen,
            second_planet_campaign_.dma_screen,
            second_planet_campaign_.switch_buffer,
            second_planet_campaign_.draw_route_name,
            second_planet_campaign_.draw_lines,
            second_planet_campaign_.undraw_lines,
            second_planet_campaign_.move_ship,
            second_planet_campaign_.start_positions,
            second_planet_campaign_.sprites,
            second_planet_campaign_.positions,
        };
        if (initialize_all_2_ == 0U || map2_flag_ == 0U
            || actual_route_ == 0U
            || ex_foxy_continue_ == 0U || ex_foxy_self_ == 0U
            || ex_restart_ == 0U || ex_briefing_ == 0U
            || ex_randomize_background_ == 0U
            || ex_stop_counting_ == 0U || ex_menu_selected_ == 0U
            || ex_credits_ == 0U || ex_page_number_ == 0U
            || ex_foxy_pointer_ == 0U || ex_foxy_shape_ == 0U
            || ex_model_test_shape_ == 0U
            || ex_bg2_vertical_offset_override_ == 0U
            || ex_fade_palette_fx_pink_ == 0U
            || ex_fade_palette_yamao_ == 0U
            || ex_model_double_ == 0U || ex_model_quadruple_ == 0U
            || ex_nan_mode_ == 0U || ex_more_dots_ == 0U
            || ex_meter_boost_enabled_ == 0U
            || ex_meter_player_health_width_ == 0U
            || ex_meter_player_health_max_ == 0U
            || ex_meter_damage_two_ == 0U
            || ex_meter_player_one_dead_ == 0U
            || ex_meter_player_two_activated_ == 0U
            || ex_meter_player_two_ == 0U
            || ex_meter_two_extra_bytes_ == 0U
            || ex_shield_up_two_ == 0U
            || std::any_of(ex_nan_colour_tables_.begin(),
                ex_nan_colour_tables_.end(),
                [](std::uint32_t address) { return address == 0U; })
            || ex_god_mode_ == 0U || ex_scored_ == 0U
            || ex_controller_2_high_ == 0U || ex_controller_2_low_ == 0U
            || ex_previous_controller_2_high_ == 0U
            || ex_trigger_2_ == 0U || ex_hardware_controller_2_ == 0U
            || ex_last_controller_2_high_ == 0U
            || ex_last_controller_2_low_ == 0U
            || ex_multitap_mode_ == 0U || ex_number_players_ == 0U
            || std::any_of(ex_multitap_controllers_.begin(),
                ex_multitap_controllers_.end(),
                [](std::uint32_t address) { return address == 0U; })
            || std::any_of(ex_last_multitap_controllers_.begin(),
                ex_last_multitap_controllers_.end(),
                [](std::uint32_t address) { return address == 0U; })
            || ex_mouse_mode_ == 0U || ex_mouse_connected_ == 0U
            || ex_mouse_y_ == 0U || ex_mouse_x_ == 0U
            || ex_mouse_buttons_ == 0U || ex_mouse_trigger_ == 0U
            || ex_mouse_previous_buttons_ == 0U
            || ex_scope_mode_ == 0U || ex_scope_no_latch_ == 0U
            || ex_scope_held_ == 0U || ex_scope_new_ == 0U
            || ex_scope_previous_ == 0U || ex_scope_horizontal_ == 0U
            || ex_scope_vertical_ == 0U
            || ex_ntt_mode_ == 0U || ex_ntt_read_ == 0U
            || ex_ntt_trigger_ == 0U || ex_ntt_previous_ == 0U
            || ex_ces_timer_ == 0U || ex_no_hud_ == 0U
            || ex_dots_stars_ == 0U || ex_dots_flag_ == 0U
            || ex_no_sfx_ == 0U || ex_no_set_port_3_ == 0U
            || ex_bgm_sfx_ == 0U || ex_set_new_bgm_ == 0U
            || ex_cursed_bgm_ == 0U || ex_bgm_test_ == 0U
            || ex_bgm_playlist_ == 0U || ex_bgm_playlist_cursed_ == 0U
            || ex_text_pointer_ == 0U || ex_fps_counter_enabled_ == 0U
            || ex_no_objects_ == 0U
            || ex_no_background_mode_ == 0U
            || ex_fps_speed_ == 0U
            || ex_ntsc_pal_swap_ == 0U
            || ex_dark_mode_ == 0U
            || ex_palette_slow_counter_ == 0U
            || ex_palette_slower_counter_ == 0U
            || std::any_of(ex_palette_every_transfer_.begin(),
                ex_palette_every_transfer_.end(),
                [](std::uint32_t address) { return address == 0U; })
            || std::any_of(ex_palette_every_fourth_transfer_.begin(),
                ex_palette_every_fourth_transfer_.end(),
                [](std::uint32_t address) { return address == 0U; })
            || std::any_of(ex_palette_every_eleventh_transfer_.begin(),
                ex_palette_every_eleventh_transfer_.end(),
                [](std::uint32_t address) { return address == 0U; })
            || ex_fps_text_ == 0U || ex_print_point_ == 0U
            || ex_open_text_ == 0U || ex_print_text_ == 0U
            || ex_print_decimal_ == 0U || ex_do_bgm_reset_ == 0U
            || ex_do_bgm_generic_ == 0U
            || ex_strat_debug_ == 0U || ex_freeze_strategies_ == 0U
            || ex_debug_flash_ == 0U || ex_debug_alien_ == 0U
            || ex_debug_backup_ == 0U
            || ex_trigger_defaults_ == 0U
            || ex_load_data_ == 0U || ex_load_index_ == 0U
            || ex_end_level_sequence_ == 0U || ex_transfer_ == 0U
            || ex_doing_end_ == 0U || ex_crosshair_on_ == 0U
            || ex_current_percentage_ == 0U
            || ex_target_percentage_ == 0U || ex_results_exit_ == 0U
            || ex_machine_type_ == 0U
            || friends_messages_2_ == 0U || friends_message_2_ == 0U
            || message_count_1_2_ == 0U || message_count_2_2_ == 0U
            || which_friend_2_ == 0U || face_data_2_ == 0U
            || messages_2_ == 0U
            || select_next_ship_ == 0U || select_previous_ship_ == 0U
            || ex_set_ship_ == 0U || current_ship_ == 0U
            || next_ship_key_down_ == 0U
            || previous_ship_key_down_ == 0U
            || std::any_of(required_map2_symbols.begin(),
                required_map2_symbols.end(),
                [](std::uint32_t address) { return address == 0U; })) {
            throw std::runtime_error{"Star Fox EX map 2 symbols are incomplete"};
        }
        select_planet_campaign(true);
    }
    // BOOTNMI.ASM copies the original WRAM-resident IRQ, SuperFX launch and
    // collision routines before gameplay. Native background initializers
    // call RUNMARIO_L inside this block, so reproduce the boot copy rather
    // than substituting a host stub.
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("COPY_TO_0101_L"), registers, 5'000'000);

    if (starfox_ex_cartridge_) {
        initialize_ex_save_ram(cartridge_ram);
        // BOOTNMI.ASM seeds EX's 16-bit xorshift state to $e528, then XORs
        // in the otherwise-uninitialised startup word at $02f0. The native PC
        // host begins with deterministic zero-filled WRAM and enters below
        // that outer boot loop, so install the exact fallback seed here.
        // Leaving RAND at zero locks xorshift permanently: title showcase
        // models remain edge-on and every native random strategy receives 0.
        map_.write_native_word(ram_symbol("RAND"), 0xe528U);
        // The native runtime enters below EX's cold-boot emulator probe, so
        // MACHINETYPE would otherwise remain zero and INTRO.ASM would skip
        // its opening communication entirely. The cartridge's hardware
        // branch is the closest source-defined identity for a native port:
        // CPU/Super FX routines execute directly rather than through one of
        // the named SNES emulators.
        map_.write_native_byte(ex_machine_type_, 10U);
        map_.write_native_byte(ram_symbol("PLAYERB_HP"), 100U);
        map_.write_native_byte(ram_symbol("PLAYERW_HP"), 100U);
    } else if (!cartridge_ram.empty()) {
        throw std::invalid_argument{
            "cartridge RAM supplied for a retail Star Fox simulation"};
    }

    // Run the complete persistent game initialization. The compatibility
    // layer implements MDECRU.MC against source ROM bytes, so this also
    // installs the original packed background palettes in bank $7f.
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("INITIALISE_L"), registers, 5'000'000);

    if (starfox_ex_cartridge_) {
        // The EX controller/start flow grants four ships before handing the
        // demo player to normal strategies. Preserve that new-game state for
        // direct diagnostic maps and for the host-owned front-end alike.
        map_.write_native_byte(lives_, 4U);
        for (const auto* teammate_lives : {
                 "LIVESTWO", "LIVESTHREE", "LIVESFOUR", "LIVESFIVE"}) {
            const auto address = find_optional_ram(teammate_lives);
            if (address != 0U) map_.write_native_byte(address, 4U);
        }
    }

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
    constexpr std::uint16_t dark_depth_table_offset = 7U * 4U;
    const auto initial_depth_table_offset = starfox_ex_cartridge_
            && (map_.read_native_byte(ex_dark_mode_) & 1U) != 0U
        ? dark_depth_table_offset : normal_depth_table_offset;
    map_.write_native_word(ram_symbol("M_DEPTHTABLE"),
        static_cast<std::uint16_t>(
            rom_symbol("DEPTHTABLES") + initial_depth_table_offset));

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
        || initial_upper == "CONTMAP"
        || initial_upper == "CONTINUE"
        || initial_upper == "CREDITSMAP";
    if (!native_flow_entry) {
        if (source_initialize_direct_map) {
            // Development CLI entry is still a real level launch. Use the
            // cartridge initializer so the 3D viewport/background mode and
            // stage audio pipeline are established exactly as they are after
            // planet selection. The lower-level constructor mode remains
            // available to deterministic VM diagnostics.
            configure_route_for_map(initial_map);
            initialize_native_map(rom_symbol(initial_map));
        } else {
            start_map(initial_map);
            configure_route_for_map(initial_map);
        }
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
    } else if (initial_upper == "CONTINUE") {
        // Deterministic diagnostic entry used by the native renderer smoke
        // tests; ordinary play still reaches this through GAME OVER.
        enter_continue_screen();
    } else if (initial_upper == "CREDITSMAP") {
        enter_credits();
    }
}

void GameSimulation::initialize_ex_save_ram(
    std::span<const std::uint8_t> cartridge_ram) {
    if (!cartridge_ram.empty() && !map_.load_cartridge_ram(cartridge_ram)) {
        throw std::invalid_argument{
            "Star Fox EX cartridge RAM must be exactly 65536 bytes"};
    }

    constexpr std::uint32_t save_clear_first = 0x71effdU;
    constexpr std::uint32_t save_clear_last = 0x71ffffU;
    constexpr std::uint32_t magic_address = 0x71fffcU;
    constexpr std::array<std::uint8_t, 4> magic{'S', 'F', 'E', 'X'};
    const auto valid_magic = std::equal(magic.begin(), magic.end(),
        map_.cartridge_ram().begin() + 0xfffcU);

    Wdc65816Registers registers;
    registers.status = 0x24U;
    if (valid_magic) {
        // BOOTNMI enters LOADDATA with MEMI zero after its boot RAM clear.
        // Reset both bytes explicitly because a host runtime can reconstruct
        // the simulation without power-cycling the process.
        map_.write_native_word(ex_load_index_, 0U);
        map_.call_native_routine(
            ex_load_data_, registers, 5'000'000, true);
        return;
    }

    // This is BOOTNMI.ASM's exact first-boot/corrupt-save path. It clears
    // only $71:effd-$71:ffff, stamps the four-byte identifier, then lets the
    // cartridge's own routine initialize WRAM and serialize every default.
    for (auto address = save_clear_first; address <= save_clear_last; ++address) {
        map_.write_native_byte(address, 0U);
    }
    for (std::size_t index = 0; index < magic.size(); ++index) {
        map_.write_native_byte(
            magic_address + static_cast<std::uint32_t>(index), magic[index]);
    }
    map_.call_native_routine(
        ex_trigger_defaults_, registers, 5'000'000, true);
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
    timing_mode_ = TimingMode::original_speed;
    display_mode_ = DisplayMode::standard_4_3;
    presentation_fps_ = 60U;
    experience_ = Experience::original;
    pregame_selection_ = 0U;
    pregame_page_ = PregamePage::main;
    god_mode_ = false;
    show_fps_ = false;
    anti_aliasing_mode_ = AntiAliasingMode::off;
    enhanced_graphics_ = false;
    smooth_polys_ = false;
    rtx_lighting_ = false;
    vsync_ = false;
    crosshair_colour_ = CrosshairColour::green;
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
        ? std::uint8_t{12U} : std::uint8_t{5U};
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
            || (pregame_selection_ == 4U
                && (input.pressed & (starfox::input::a
                    | starfox::input::select)) != 0U);
        if (go_back) {
            pregame_page_ = PregamePage::main;
            pregame_selection_ = 10U;
            queue_sound_effect(0x11U);
        } else if (pregame_selection_ == 0U
                   && (input.pressed & (starfox::input::left
                       | starfox::input::right | starfox::input::select
                       | starfox::input::a)) != 0U) {
            god_mode_ = !god_mode_;
            queue_sound_effect(0x11U);
        } else if (pregame_selection_ == 1U
                   && (input.pressed & (starfox::input::left
                       | starfox::input::right | starfox::input::select
                       | starfox::input::a)) != 0U) {
            show_fps_ = !show_fps_;
            queue_sound_effect(0x11U);
        } else if (pregame_selection_ == 2U
                   && (input.pressed & (starfox::input::left
                       | starfox::input::right | starfox::input::select
                       | starfox::input::a)) != 0U) {
            const auto found = std::find(
                kCrosshairColours.begin(), kCrosshairColours.end(),
                crosshair_colour_);
            auto index = found == kCrosshairColours.end()
                ? std::size_t{}
                : static_cast<std::size_t>(
                    std::distance(kCrosshairColours.begin(), found));
            if ((input.pressed & starfox::input::left) != 0U) {
                index = (index + kCrosshairColours.size() - 1U)
                    % kCrosshairColours.size();
            } else {
                index = (index + 1U) % kCrosshairColours.size();
            }
            crosshair_colour_ = kCrosshairColours[index];
            queue_sound_effect(0x11U);
        }
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }

    const auto change_experience = pregame_selection_ == 0U
        && (input.pressed & (starfox::input::left | starfox::input::right
            | starfox::input::select | starfox::input::a
            | starfox::input::b)) != 0U;
    if (change_experience) {
        experience_ = experience_ == Experience::original
            ? Experience::starfox_ex : Experience::original;
        queue_sound_effect(0x11U);
    }

    const auto change_timing = pregame_selection_ == 1U
        && (input.pressed & (starfox::input::left | starfox::input::right
            | starfox::input::select | starfox::input::a
            | starfox::input::b)) != 0U;
    if (change_timing) {
        timing_mode_ = timing_mode_ == TimingMode::unlocked_20_fps
            ? TimingMode::original_speed : TimingMode::unlocked_20_fps;
        queue_sound_effect(0x11U);
    }

    const auto change_presentation = pregame_selection_ == 2U
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

    const auto change_display = pregame_selection_ == 3U
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

    const auto toggle_render_option =
        (input.pressed & (starfox::input::left | starfox::input::right
            | starfox::input::select | starfox::input::a
            | starfox::input::b)) != 0U;
    if (toggle_render_option && pregame_selection_ >= 4U
        && pregame_selection_ <= 8U) {
        switch (pregame_selection_) {
        case 4U: {
            auto mode = static_cast<std::uint8_t>(anti_aliasing_mode_);
            if ((input.pressed & starfox::input::left) != 0U) {
                mode = static_cast<std::uint8_t>((mode + 3U) % 4U);
            } else {
                mode = static_cast<std::uint8_t>((mode + 1U) % 4U);
            }
            anti_aliasing_mode_ = static_cast<AntiAliasingMode>(mode);
            break;
        }
        case 5U: enhanced_graphics_ = !enhanced_graphics_; break;
        case 6U: smooth_polys_ = !smooth_polys_; break;
        case 7U: rtx_lighting_ = !rtx_lighting_; break;
        case 8U: vsync_ = !vsync_; break;
        default: break;
        }
        queue_sound_effect(0x11U);
    }

    const auto open_options = pregame_selection_ == 10U
        && (input.pressed & (starfox::input::a | starfox::input::b)) != 0U;
    if (open_options) {
        pregame_page_ = PregamePage::options;
        pregame_selection_ = 0U;
        queue_sound_effect(0x11U);
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }

    const auto start_pressed = (input.pressed & starfox::input::start) != 0U;
    const auto confirm_start = pregame_selection_ == 11U
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

std::uint8_t GameSimulation::model_scale_multiplier() const noexcept {
    if (!starfox_ex_cartridge_) return 1U;
    // MOBJ.MC checks M_BIGGERHEADMODE first and adds two to the shape's
    // coordinate shift; otherwise M_BIGHEADMODE adds one. Those are exact
    // 4x and 2x multipliers respectively in the host's shared projection.
    if (map_.read_native_byte(ex_model_quadruple_) != 0U) return 4U;
    if (map_.read_native_byte(ex_model_double_) != 0U) return 2U;
    return 1U;
}

std::optional<std::uint16_t>
GameSimulation::model_colour_table_override() const noexcept {
    if (!starfox_ex_cartridge_) return std::nullopt;
    const auto mode = map_.read_native_byte(ex_nan_mode_);
    if (mode == 0U || mode > ex_nan_colour_tables_.size()) {
        // Modes 6-9 alter scan conversion (wobble, wave and cel shading)
        // without replacing the object's source colour table.
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(ex_nan_colour_tables_[mode - 1U]);
}

std::size_t GameSimulation::dust_point_count() const noexcept {
    return starfox_ex_cartridge_
            && map_.read_native_word(ex_more_dots_) != 0U
        ? kMaximumDustPoints : kNormalDustPoints;
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
    const auto find_ram = [this](const std::string& candidate)
        -> std::optional<std::uint32_t> {
        for (const auto address : symbols_->find(candidate)) {
            const auto bank = address >> 16U;
            if (bank == 0U || bank == 0x70U || bank == 0x7eU
                || bank == 0x7fU) return address;
        }
        return std::nullopt;
    };
    if (const auto address = find_ram(name)) return *address;
    // Star Fox EX expanded the single-player bomb count and shield mirror for
    // its five-player state. These are the exact corresponding EX variables;
    // keeping the aliases here lets the shared native runtime execute either
    // symbol map without altering the assembled hack.
    if (name == "SPECWEPCNT") {
        if (const auto address = find_ram("SPECWEPCNTONE")) return *address;
    } else if (name == "M_SHIELDUP") {
        if (const auto address = find_ram("SHIELDUP")) return *address;
    }
    throw std::runtime_error{"missing game RAM symbol: " + name};
}

MeterState GameSimulation::meter_state() const noexcept {
    MeterState result{
        map_.read_native_byte(meter_damage_),
        map_.read_native_byte(meter_boost_),
        map_.read_native_byte(meter_shield_up_) != 0U,
        map_.read_native_word(meters_enabled_) != 0U,
        map_.read_native_byte(boss_health_),
        map_.read_native_byte(boss_max_health_),
    };
    if (!starfox_ex_cartridge_) return result;

    const auto player_two = map_.read_native_word(
        ex_meter_player_two_activated_);
    result.extended = true;
    result.boost_enabled =
        map_.read_native_byte(ex_meter_boost_enabled_) != 0U;
    result.player_two_activated = (player_two & 0xffU) != 0U;
    result.second_player_view =
        map_.read_native_byte(ex_meter_player_two_) != 0U;
    result.player_one_dead =
        map_.read_native_word(ex_meter_player_one_dead_) != 0U;
    result.damage_two = map_.read_native_byte(ex_meter_damage_two_);
    result.shield_up = (player_two & 0xff00U) != 0U;
    result.shield_up_two =
        map_.read_native_byte(ex_meter_two_extra_bytes_) != 0U;
    result.player_health_width =
        map_.read_native_byte(ex_meter_player_health_width_);
    result.player_health_max =
        map_.read_native_byte(ex_meter_player_health_max_);
    return result;
}

CircleEffectState GameSimulation::circle_effect_state() const noexcept {
    return circle_effect_;
}

WindowWipeState GameSimulation::window_wipe_state() const noexcept {
    WindowWipeState result;
    result.active = map_.read_native_byte(doing_wipe_) != 0U
        && map_.read_native_byte(do_a_wipe_) != 0U;
    // M_WINWBGLOG aliases M_BIGX in the source's shared Super FX scratch
    // block.  Later launches in this same submitted frame overwrite it, so
    // expose the value captured where DO_WINDOW_WIPE_L copies it into the
    // SNES window manager rather than rereading the aliased scratch word.
    result.logic = wipe_logic_snapshot_;
    for (std::size_t line = 0; line < result.left.size(); ++line) {
        const auto displacement = static_cast<std::uint32_t>(line * 2U);
        result.left[line] = map_.read_native_word(
            wipe_left_buffer_ + displacement);
        result.right[line] = map_.read_native_word(
            wipe_right_buffer_ + displacement);
    }
    return result;
}

DialogueState GameSimulation::dialogue_state() const noexcept {
    // MAIN.ASM advances channel 1 and then channel 2. MCOPYFACE2 therefore
    // overwrites MCOPYFACE whenever the EX channel is active, so expose that
    // same final compositor state rather than trying to show both at once.
    const auto alternate = starfox_ex_cartridge_
        && (map_.read_native_byte(message_count_1_2_) != 0U
            || map_.read_native_byte(message_count_2_2_) != 0U);
    const auto open_count = map_.read_native_byte(
        alternate ? message_count_1_2_ : message_count_1_);
    const auto animation_count = map_.read_native_byte(
        alternate ? message_count_2_2_ : message_count_2_);
    const auto active = open_count != 0U || animation_count != 0U;
    auto portrait_frame = std::uint8_t{};
    const auto pointer = map_.read_native_word(face_pointer_);
    const auto face_base = static_cast<std::uint16_t>(
        alternate ? face_data_2_ : face_data_);
    if (pointer >= face_base) {
        portrait_frame = static_cast<std::uint8_t>((pointer - face_base) / 640U);
    }
    const auto friend_id = map_.read_native_byte(
        alternate ? which_friend_2_ : which_friend_);
    return {
        active,
        open_count != 0U && animation_count >= 5U,
        (friend_id & 0x80U) != 0U || (friend_id & 0x7fU) == 5U,
        alternate,
        portrait_frame,
        ((alternate ? messages_2_ : messages_) & 0xff0000U)
            | map_.read_native_word(
            alternate ? friends_message_2_ : friends_message_),
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
    if (planet < planet_count_) {
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

    if (starfox_ex_cartridge_) {
        const auto player_two = static_cast<std::uint16_t>(
            map_.read_native_word(ex_meter_player_two_activated_)
            & 0x00ffU);
        map_.write_native_word(ex_meter_player_two_activated_,
            static_cast<std::uint16_t>(player_two
                | (map_.read_native_byte(shield_up_) != 0U ? 0x0100U : 0U)));
        map_.write_native_byte(ex_meter_two_extra_bytes_,
            map_.read_native_byte(ex_shield_up_two_));
    }
}

void GameSimulation::draw_ex_transfer_overlay(GameTickResult& result) {
    if (!starfox_ex_cartridge_
        || (flow_state_ != GameFlowState::gameplay
            && flow_state_ != GameFlowState::training)) return;

    // TRANS.ASM prints this after its FRAMES/FRAMECOUNT update and before the
    // bitmap transfer. The host replaces TRANS_L's geometry pass, so execute
    // the exact PRINTT_L/PRINTBD_L calls against the assembled EX text and
    // debug font instead of substituting a host FPS label.
    if (map_.read_native_byte(ex_fps_counter_enabled_) != 0U) {
        map_.write_native_word(ex_print_point_, 0U);
        map_.write_native_word(ex_text_pointer_,
            static_cast<std::uint16_t>(ex_open_text_));
        map_.write_native_byte(ex_text_pointer_ + 2U,
            static_cast<std::uint8_t>(ex_open_text_ >> 16U));
        for (std::uint32_t offset = 2U; offset < 256U; offset += 2U) {
            const auto characters = rom_->read16(ex_fps_text_ + offset);
            if (characters == 0U) break;
            map_.write_native_word(ex_open_text_, characters);
            Wdc65816Registers text_registers;
            text_registers.status = 0x24U;
            result.prelude_instructions += map_.call_native_routine(
                ex_print_text_, text_registers, 2'000'000U, true);
        }
        Wdc65816Registers decimal_registers;
        decimal_registers.a = map_.read_native_byte(measured_frame_rate_);
        decimal_registers.status = 0x24U;
        result.prelude_instructions += map_.call_native_routine(
            ex_print_decimal_, decimal_registers, 2'000'000U, true);
    }
    map_.submit_superfx_bitmap();
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
    if (starfox_ex_cartridge_) {
        const auto& second = secondary_inputs_.front();
        map_.write_native_byte(ex_previous_controller_2_high_,
            map_.read_native_byte(ex_controller_2_high_));
        map_.write_native_byte(ex_controller_2_high_,
            static_cast<std::uint8_t>(second.held >> 8U));
        map_.write_native_byte(ex_controller_2_low_,
            static_cast<std::uint8_t>(second.held));
        map_.write_native_word(ex_trigger_2_, second.pressed);
        map_.write_native_word(ex_hardware_controller_2_, second.held);

        std::array<input::ButtonMask, 5> held{
            input.held,
            secondary_inputs_[0].held,
            secondary_inputs_[1].held,
            secondary_inputs_[2].held,
            secondary_inputs_[3].held,
        };
        // EX's one-controller multitap mode deliberately mirrors player 1
        // into all five controller slots. Preserve that source convenience
        // while allowing distinct native PC devices in every other mode.
        if (map_.read_native_byte(ex_multitap_mode_) != 0U
            && map_.read_native_byte(ex_number_players_) == 1U) {
            held.fill(input.held);
        }
        for (std::size_t index = 0; index < held.size(); ++index) {
            map_.write_native_word(ex_multitap_controllers_[index],
                held[index]);
        }

        if (ex_mouse_control_enabled()) {
            const auto encode_axis = [](std::int16_t delta) {
                const auto signed_delta = static_cast<int>(delta);
                const auto magnitude = static_cast<std::uint8_t>(std::min<int>(
                    signed_delta < 0 ? -signed_delta : signed_delta, 0x7f));
                return static_cast<std::uint8_t>(
                    magnitude | (delta < 0 ? 0x80U : 0U));
            };
            const auto buttons = static_cast<std::uint8_t>(
                mouse_input_.buttons & 0x03U);
            const auto previous = map_.read_native_byte(
                ex_mouse_previous_buttons_);
            map_.write_native_byte(ex_mouse_connected_, 1U);
            map_.write_native_byte(ex_mouse_x_, encode_axis(mouse_input_.delta_x));
            map_.write_native_byte(ex_mouse_y_, encode_axis(mouse_input_.delta_y));
            map_.write_native_byte(ex_mouse_buttons_, buttons);
            // IRQ.ASM's mouse BIOS reports a trigger only on a switch-state
            // transition, and stores the new held value (releases therefore
            // produce trigger zero). Preserve that exact behavior so a fresh
            // both-button press reaches EX's boost/brake double-click logic.
            map_.write_native_byte(ex_mouse_trigger_,
                buttons != previous ? buttons : 0U);
            map_.write_native_byte(ex_mouse_previous_buttons_, buttons);
        }

        if (ex_scope_control_enabled()) {
            // The source Scope BIOS stores buttons in JOY2L's high byte and
            // the light-gun latch as 9-bit PPU H/V counters. PC mouse motion
            // is accumulated by the app, so write that physical packet at
            // the same boundary instead of allowing the absent SNES device
            // registers to erase it during calibration/gameplay.
            std::uint16_t held{};
            if ((mouse_input_.buttons & 0x01U) != 0U) held |= 0x8000U; // Fire
            if ((mouse_input_.buttons & 0x02U) != 0U) held |= 0x4000U; // Cursor
            if ((mouse_input_.buttons & 0x08U) != 0U) held |= 0x2000U; // Turbo
            if ((mouse_input_.buttons & 0x04U) != 0U) held |= 0x1000U; // Pause
            const auto previous = map_.read_native_word(ex_scope_previous_);
            map_.write_native_word(ex_scope_horizontal_, mouse_input_.scope_x);
            map_.write_native_word(ex_scope_vertical_, mouse_input_.scope_y);
            map_.write_native_word(ex_scope_held_, held);
            map_.write_native_word(ex_scope_new_,
                static_cast<std::uint16_t>((held ^ previous) & held));
            map_.write_native_word(ex_scope_previous_, held);
            map_.write_native_word(ex_scope_no_latch_, 0U);
        }

        if (ex_ntt_mode_ != 0U
            && map_.read_native_byte(ex_ntt_mode_) != 0U) {
            const auto previous = map_.read_native_word(ex_ntt_previous_);
            map_.write_native_word(ex_ntt_read_, ntt_input_);
            map_.write_native_word(ex_ntt_trigger_,
                static_cast<std::uint16_t>((ntt_input_ ^ previous)
                    & ntt_input_));
            map_.write_native_word(ex_ntt_previous_, ntt_input_);
        } else if (ex_ntt_read_ != 0U && ex_ntt_trigger_ != 0U) {
            // Both EX IRQ paths clear the serial read before checking whether
            // NTT mode is active. Do the same so disabling the option cannot
            // leave a special weapon/camera key latched in gameplay.
            map_.write_native_word(ex_ntt_read_, 0U);
            map_.write_native_word(ex_ntt_trigger_, 0U);
        }
    }
}

bool GameSimulation::ex_mouse_control_enabled() const noexcept {
    return starfox_ex_cartridge_ && ex_mouse_mode_ != 0U
        && map_.read_native_byte(ex_mouse_mode_) != 0U;
}

bool GameSimulation::ex_scope_control_enabled() const noexcept {
    return starfox_ex_cartridge_ && ex_scope_mode_ != 0U
        && map_.read_native_byte(ex_scope_mode_) != 0U;
}

void GameSimulation::set_secondary_inputs(
    std::span<const input::TickInput> controllers) noexcept {
    secondary_inputs_.fill({});
    const auto count = std::min(controllers.size(), secondary_inputs_.size());
    std::copy_n(controllers.begin(), count, secondary_inputs_.begin());
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
    const auto communication_active =
        map_.read_native_byte(message_count_1_) != 0U
        || map_.read_native_byte(message_count_2_) != 0U
        || (friends_message_2_ != 0U
            && (map_.read_native_byte(message_count_1_2_) != 0U
                || map_.read_native_byte(message_count_2_2_) != 0U));
    if (flow_state_ == GameFlowState::gameplay
        && communication_active && boss_music_before_death_.has_value()
        && (post_boss_dialogue_active_
            || map_.read_native_byte(boss_max_health_) == 0U)) {
        // SEND_MESSAGE gives post-boss chatter exactly 50 source updates.
        // At this point the boss has left the draw list and the cartridge is
        // back at its 20 Hz ceiling. Counting the lingering debris handles as
        // live Super FX pressure stretched those 50 updates well past the
        // original 2.5 seconds in ORIGINAL SPEED mode.
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
    const auto last_route_digit = starfox_ex_cartridge_ ? '7' : '3';
    if (!upper.starts_with("LEVEL") || upper[5] < '1'
        || upper[5] > last_route_digit
        || upper[6] != '_' || upper[7] < '1' || upper[7] > '9') {
        return;
    }
    const auto route = static_cast<std::uint8_t>(upper[5] - '1');
    const auto stage = static_cast<std::uint16_t>(upper[7] - '1');
    if (starfox_ex_cartridge_) select_planet_campaign(route >= 4U);
    map_.write_native_word(stage_, stage);
    map_.write_native_byte(current_level_, route);
    map_.write_native_byte(which_route_, route);
    if (actual_route_ != 0U) map_.write_native_byte(actual_route_, route);
    if (route < 2U) map_.write_native_byte(which_route_, static_cast<std::uint8_t>(route ^ 1U));
    static_cast<void>(resolve_route_stage(stage));
    if (route < 2U) map_.write_native_byte(which_route_, route);
    route_display_order_ = false;
}

std::uint32_t GameSimulation::resolve_route_stage(std::uint16_t remaining_stage) {
    const auto route = map_.read_native_byte(which_route_);
    // The visible selectors expose PLANETS routes 0-3 and PLANETS2 routes
    // 4-6. Their source-controlled special branches (Black Hole, OOTD,
    // credits and comet) continue through STAGEPATHS route 11. PLANETS3
    // begins at route 12 and is the unused test map, so stop at that exact
    // boundary rather than applying the retail cartridge's five-route cap.
    if (route >= 12U) {
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
    boss_music_before_death_.reset();
    post_boss_dialogue_active_ = false;
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
    // Communication counters and the submitted Super FX bitmap are owned by
    // the outgoing map. INITGAME does not consistently clear EX's second
    // channel, so an intro/training portrait could otherwise remain burned
    // into BG1 after the flow had moved to controls, title, or gameplay.
    clear_communications();
    map_.write_native_word(ram_symbol("MAPPTR"),
        static_cast<std::uint16_t>(address & 0x7fffU));
    map_.write_native_byte(ram_symbol("MAPBANK"),
        static_cast<std::uint8_t>(address >> 16U));
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(initialize_game_, registers, 50'000'000, true);
    map_.restore_map_state_from_native();
    // BRIEFING_L and the training/level handoffs normally arrive here with
    // SETGAMEPAL_L's palette-7 copy already installed by the outer cartridge
    // loop. EX's resumable RESTART task enters below that copy, however, and
    // INITIALISE_L has just cleared PAL0PALETTE. The host renderer still
    // rasterizes the correct models, but sixteen zero colours make the
    // controller, training and scramble ships completely black. Restore the
    // ordinary source palette only when that whole row is absent; valid
    // stage- and option-specific palettes remain authoritative.
    auto model_palette_is_empty = true;
    for (std::uint32_t colour = 0U; colour < 16U; ++colour) {
        model_palette_is_empty = model_palette_is_empty
            && (map_.read_native_word(ppu_palette_ + (7U * 16U + colour) * 2U)
                & 0x7fffU) == 0U;
    }
    if (model_palette_is_empty) {
        registers = {};
        registers.status = 0x24U;
        map_.call_native_routine(
            rom_symbol("SETGAMEPAL_L"), registers, 5'000'000, true);
    }
    refresh_player_reference();
    draw_order_ = objects_.active_handles();
    ++scene_revision_;
}

void GameSimulation::clear_communications() {
    map_.write_native_word(friends_message_, 0U);
    map_.write_native_byte(message_count_1_, 0U);
    map_.write_native_byte(message_count_2_, 0U);
    map_.write_native_byte(which_friend_, 0U);
    map_.write_native_word(face_pointer_, 0U);
    if (starfox_ex_cartridge_) {
        map_.write_native_word(friends_message_2_, 0U);
        map_.write_native_byte(message_count_1_2_, 0U);
        map_.write_native_byte(message_count_2_2_, 0U);
        map_.write_native_byte(which_friend_2_, 0U);
    }

    // M_CLRBITMAPS is deliberately optional during some front-end maps. Force
    // one transition clear, submit it through the normal FOXIRQ path, then
    // restore the cartridge's policy for subsequent source frames.
    const auto clear_bitmaps = ram_symbol("M_CLRBITMAPS");
    const auto previous = map_.read_native_word(clear_bitmaps);
    map_.write_native_word(clear_bitmaps, 1U);
    map_.begin_superfx_bitmap_frame();
    map_.submit_superfx_bitmap();
    map_.write_native_word(clear_bitmaps, previous);
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
    // The death sequence arrives here at brightness zero. MAIN presents the
    // GAME/OVER map immediately; the host does not execute its surrounding
    // raster fade loop, so reveal the initialized scene explicitly.
    map_.set_display_brightness(15U);
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
    // CONTINUE.ASM restores NUMLIVES both when the screen opens and when YES
    // is accepted. The shared host reconstruction must do the same for retail
    // and EX rather than carrying the zero that triggered game over.
    // LIVES includes the currently active Arwing. SPRITES.ASM displays
    // LIVES-1, so the source's NUMLIVES value of four is the visible x3 the
    // player receives after continuing.
    map_.write_native_byte(lives_, 4U);
    if (starfox_ex_cartridge_) {
        for (const auto* reserve : {
                 "LIVESTWO", "LIVESTHREE", "LIVESFOUR", "LIVESFIVE"}) {
            map_.write_native_byte(ram_symbol(reserve), 4U);
        }
    }

    if (starfox_ex_cartridge_ && ex_do_palette_stuff_ != 0U) {
        Wdc65816Registers palette_registers;
        palette_registers.status = 0x24U;
        map_.call_native_routine(
            ex_do_palette_stuff_, palette_registers, 5'000'000, true);
    }

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

    // BGFOXPAC supplies the 2D Continue artwork, then SETPAL 1 replaces the
    // dedicated 3D palette row used by the interactive Arwing. Without this
    // second source step the model inherited Fox/background hues (purple in
    // the retail pack) even after its projection was corrected.
    Wdc65816Registers model_palette_registers;
    model_palette_registers.a = 1U;
    model_palette_registers.status = 0x24U;
    map_.call_native_routine(rom_symbol("SETPAL_L"),
        model_palette_registers, 5'000'000, true);

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
    // The Continue Arwing itself is drawn by MSHOWOBJ3 below. Keep one
    // non-presented host slot alive, however: INITGAME_L uses the existing
    // player handle as its bridge while rebuilding EX's stage checkpoint.
    // Clearing the pool and leaving player_ at zero allowed the route fade
    // to finish but produced gameplay with no active player afterward.
    const auto player_seed = objects_.allocate_after();
    auto& seed = objects_.at(player_seed);
    seed.shape = static_cast<std::uint16_t>(fox_shape_);
    seed.world_z = 350;
    player_ = player_seed;
    draw_order_.clear();
    continue_rotation_velocity_x_ = 0U;
    continue_rotation_velocity_y_ = 4U;
    continue_rotation_velocity_z_ = 0U;
    continue_rotation_x_ = 0U;
    continue_rotation_y_ = 0U;
    continue_rotation_z_ = 0U;
    continue_model_z_ = 350;
    map_.write_native_word(ram_symbol("M_BIGZ"),
        static_cast<std::uint16_t>(continue_model_z_));
    map_.set_native_model_draw({true,
        static_cast<std::uint16_t>(fox_shape_), 0, 0, continue_model_z_,
        continue_rotation_x_, continue_rotation_y_, continue_rotation_z_,
        112, 96, 0, 0});
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

void GameSimulation::enter_ex_pregame_menu(bool model_test) {
    if (!starfox_ex_cartridge_) {
        throw std::logic_error{"EX pre-game menu requested for a retail cartridge"};
    }
    paused_ = false;
    draw_order_.clear();
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_word(level_finished_, 0U);
    map_.write_native_byte(ex_stop_counting_, model_test ? 10U : 11U);
    map_.write_native_byte(ex_menu_selected_, model_test ? 1U : 15U);
    map_.write_native_byte(ex_credits_, 1U);
    if (model_test) {
        // MAPS/TITLE.ASM installs these immediately after its completed
        // fade and before tail-jumping to FOXY_CONTINUE_L. Preserve the
        // cartridge's model-test entry state while the resumable source menu
        // task owns every subsequent control and draw.
        map_.write_native_byte(ex_page_number_, 3U);
        map_.write_native_word(ex_foxy_pointer_, 2U);
        map_.write_native_word(ex_foxy_shape_,
            static_cast<std::uint16_t>(ex_model_test_shape_));
    }
    write_input({});

    ex_menu_registers_ = {};
    ex_menu_registers_.status = 0x24U;
    constexpr std::size_t menu_instruction_limit = 100'000'000U;
    const std::array first_stop{ex_foxy_self_};
    auto task = map_.begin_native_task(ex_foxy_continue_, ex_menu_registers_,
        first_stop, menu_instruction_limit, true);
    if (task.returned || task.stop_address != ex_foxy_self_) {
        throw std::runtime_error{
            "Star Fox EX pre-game menu did not reach its source frame loop"};
    }

    // FOXY_CONTINUE reaches SELF after setup but before the first menu page
    // has been generated. Execute that first exact source iteration so the
    // initial presentation already contains page one and its cursor.
    // The hack's experimental MAX FPS modes intertwine transfer IRQ cadence,
    // strategy cadence and APU servicing. The native PC runtime deliberately
    // exposes render FPS separately, so keep EX on its stable/default 20 mode
    // and do not let a legacy SRAM value silently select 30/60 here.
    map_.write_native_byte(ex_fps_speed_, 0U);
    map_.write_native_byte(ex_ntsc_pal_swap_, 0U);
    const std::array frame_stops{ex_foxy_self_, ex_restart_};
    task = map_.resume_native_task(ex_menu_registers_, frame_stops,
        menu_instruction_limit, true);
    if (task.returned || task.stop_address != ex_foxy_self_) {
        throw std::runtime_error{
            "Star Fox EX pre-game menu did not draw its first source frame"};
    }
    god_mode_ = map_.read_native_byte(ex_god_mode_) != 0U;
    flow_ticks_ = 0U;
    frontend_frames_ = 0U;
    frontend_phase_ = FrontendPhase::none;
    flow_state_ = GameFlowState::ex_pregame_menu;
    ++scene_revision_;
}

GameTickResult GameSimulation::tick_ex_pregame_menu(
    const input::TickInput& input) {
    constexpr std::uint32_t spc_clocks_per_tick = 1'024'000U / 20U;
    constexpr std::size_t menu_instruction_limit = 100'000'000U;
    auto native_input = input;
    if (map_.read_native_byte(ex_stop_counting_) == 20U
        && map_.read_native_byte(ex_menu_selected_) <= 1U) {
        // CONTINUE.ASM's FPS submenu leaves MAX FPS visible for source-menu
        // fidelity, but its REGION and MAX FPS actions are disabled in this
        // port. The PC's RENDER FPS choice controls presentation only and is
        // available on the outer pre-game menu.
        constexpr auto fps_change_buttons = static_cast<starfox::input::ButtonMask>(
            starfox::input::left | starfox::input::right);
        native_input.held = static_cast<starfox::input::ButtonMask>(
            native_input.held & ~fps_change_buttons);
        native_input.pressed = static_cast<starfox::input::ButtonMask>(
            native_input.pressed & ~fps_change_buttons);
        native_input.released = static_cast<starfox::input::ButtonMask>(
            native_input.released & ~fps_change_buttons);
    }
    map_.write_native_byte(ex_fps_speed_, 0U);
    map_.write_native_byte(ex_ntsc_pal_swap_, 0U);
    write_input(native_input);
    if (map_.read_native_byte(ex_stop_counting_) == 10U
        || map_.read_native_byte(ex_stop_counting_) == 12U) {
        // CONTINUE.ASM's model viewer accidentally tests both shoulder bits
        // against CONT0L (the previous high controller byte) instead of
        // CONTL0 (the current low byte). That makes its documented zoom
        // controls inert on the cartridge as written. Mirror only L/R into
        // those two otherwise unrelated bit positions while this viewer owns
        // the frame, preserving every other source-menu input verbatim.
        constexpr auto shoulders = static_cast<starfox::input::ButtonMask>(
            starfox::input::left_shoulder
            | starfox::input::right_shoulder);
        auto model_controller_byte = map_.read_native_byte(
            previous_controller_high_);
        model_controller_byte = static_cast<std::uint8_t>(
            (model_controller_byte & ~static_cast<std::uint8_t>(shoulders))
            | static_cast<std::uint8_t>(native_input.held & shoulders));
        map_.write_native_byte(
            previous_controller_high_, model_controller_byte);
    }

    GameTickResult result;
    const std::array frame_stops{ex_foxy_self_, ex_restart_};
    const auto task = map_.resume_native_task(ex_menu_registers_, frame_stops,
        menu_instruction_limit, true);
    result.prelude_instructions += task.instructions;
    map_.write_native_byte(ex_fps_speed_, 0U);
    map_.write_native_byte(ex_ntsc_pal_swap_, 0U);
    if (task.returned) {
        throw std::runtime_error{
            "Star Fox EX pre-game menu returned without choosing START GAME"};
    }
    if (task.stop_address == ex_restart_) {
        // RESTART is EX's option-preservation boundary: it saves every menu
        // setting, clears WRAM, restores/mirrors those fields, and performs
        // INITIALISE_L. Pause at the source BRIEFING_L call, then hand the
        // controller screen to the existing PC presentation bridge.
        const std::array restart_stop{ex_briefing_};
        map_.write_native_byte(ex_fps_speed_, 0U);
        map_.write_native_byte(ex_ntsc_pal_swap_, 0U);
        const auto restart_task = map_.resume_native_task(ex_menu_registers_,
            restart_stop, menu_instruction_limit, true);
        result.prelude_instructions += restart_task.instructions;
        map_.write_native_byte(ex_fps_speed_, 0U);
        map_.write_native_byte(ex_ntsc_pal_swap_, 0U);
        if (restart_task.returned
            || restart_task.stop_address != ex_briefing_) {
            throw std::runtime_error{
                "Star Fox EX restart did not reach its controller briefing"};
        }
        god_mode_ = map_.read_native_byte(ex_god_mode_) != 0U;
        map_.write_native_byte(controls_exit_, 0U);
        map_.write_native_byte(default_training_, 0U);
        enter_controls(GameFlowState::controls_type);
    } else if (task.stop_address != ex_foxy_self_) {
        throw std::runtime_error{
            "Star Fox EX pre-game menu stopped outside its source frame loop"};
    } else {
        god_mode_ = map_.read_native_byte(ex_god_mode_) != 0U;
    }

    for (std::size_t phase = 0; phase < current_tick_video_phases_; ++phase) {
        map_.set_apu_clock_offset(static_cast<std::uint32_t>(
            phase * spc_clocks_per_tick / current_tick_video_phases_));
        service_audio_irq(result.sound_effect_commands);
    }
    ++flow_ticks_;
    result.audio_port_writes = map_.take_apu_port_writes();
    return result;
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
    // CONTINUE.ASM changes angular velocity with the D-pad, then advances
    // M_ROTX/Y/Z every source frame. Its Arwing is submitted through the
    // dedicated MSHOWOBJ3 projection, not through the gameplay draw list.
    if ((input.held & starfox::input::left) != 0U) {
        ++continue_rotation_velocity_y_;
    }
    if ((input.held & starfox::input::right) != 0U) {
        --continue_rotation_velocity_y_;
    }
    if ((input.held & starfox::input::up) != 0U) {
        ++continue_rotation_velocity_x_;
    }
    if ((input.held & starfox::input::down) != 0U) {
        --continue_rotation_velocity_x_;
    }
    if ((input.pressed & starfox::input::x) != 0U) {
        continue_rotation_velocity_x_ = 0U;
        continue_rotation_velocity_y_ = 0U;
        continue_rotation_velocity_z_ = 0U;
    }
    if ((input.held & starfox::input::left_shoulder) != 0U
        && continue_model_z_ >= 170) {
        continue_model_z_ = static_cast<std::int16_t>(continue_model_z_ - 20);
    }
    if ((input.held & starfox::input::right_shoulder) != 0U
        && continue_model_z_ <= 960) {
        continue_model_z_ = static_cast<std::int16_t>(continue_model_z_ + 20);
    }
    continue_rotation_x_ = static_cast<std::uint8_t>(
        continue_rotation_x_ + continue_rotation_velocity_x_);
    continue_rotation_y_ = static_cast<std::uint8_t>(
        continue_rotation_y_ + continue_rotation_velocity_y_);
    continue_rotation_z_ = static_cast<std::uint8_t>(
        continue_rotation_z_ + continue_rotation_velocity_z_);
    map_.write_native_word(ram_symbol("M_BIGZ"),
        static_cast<std::uint16_t>(continue_model_z_));
    map_.set_native_model_draw({true,
        static_cast<std::uint16_t>(fox_shape_), 0, 0, continue_model_z_,
        continue_rotation_x_, continue_rotation_y_, continue_rotation_z_,
        112, 96, 0, 0});
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
    map_.write_native_byte(lives_, 4U);
    if (starfox_ex_cartridge_) {
        for (const auto* reserve : {
                 "LIVESTWO", "LIVESTHREE", "LIVESFOUR", "LIVESFIVE"}) {
            map_.write_native_byte(ram_symbol(reserve), 4U);
        }
    }
    const auto map_address = selected_route_stage(map_.read_native_word(stage_));
    enter_planet_map(false, map_address);
}

void GameSimulation::enter_credits() {
    paused_ = false;
    ending_task_active_ = false;
    ending_final_score_ = false;
    pending_end_game_ = false;
    credits_complete_ = false;
    const auto credits_music = map_.read_native_byte(which_route_) >= 4U
        && credits_music_ex_ != 0U
        ? credits_music_ex_ : credits_music_original_;
    if (credits_music != 0U) {
        Wdc65816Registers music_registers;
        music_registers.status = 0x24U;
        map_.call_native_routine(
            credits_music, music_registers, 50'000'000U, true);
    }
    initialize_native_map(credits_map_);
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_word(level_finished_, 0U);
    flow_ticks_ = 0U;
    flow_state_ = GameFlowState::credits;
}

void GameSimulation::begin_end_game_sequence() {
    pending_end_game_ = false;
    paused_ = false;
    clear_communications();
    map_.write_native_word(meters_enabled_, 0U);
    flow_ticks_ = 0U;
    flow_state_ = GameFlowState::credits;
    credits_complete_ = false;
    ending_final_score_ = false;
    ++scene_revision_;

    if (end_game_sequence_ == 0U || ending_transfer_ == 0U
        || ending_end_transfer_ == 0U) {
        // Older symbol packs can omit private MAIN/ENDSEQ labels. Preserve a
        // playable ending rather than trapping the completed level.
        enter_credits();
        return;
    }

    ending_registers_ = {};
    ending_registers_.status = 0x24U;
    const std::array stops{ending_transfer_, ending_end_transfer_,
        credits_entry_ != 0U ? credits_entry_ : ending_transfer_};
    const auto task = map_.begin_native_near_task(end_game_sequence_,
        ending_registers_, stops, 50'000'000U, true);
    if ((credits_entry_ != 0U && task.stop_address == credits_entry_)
        || task.returned) {
        enter_credits();
        return;
    }
    ending_task_active_ = true;
    refresh_player_reference();
    draw_order_ = objects_.active_handles();
}

GameTickResult GameSimulation::tick_end_game_sequence(
    const input::TickInput& input) {
    constexpr std::uint32_t spc_clocks_per_tick = 1'024'000U / 20U;
    write_input(input);
    GameTickResult result;
    for (std::size_t phase = 0; phase < current_tick_video_phases_; ++phase) {
        map_.set_apu_clock_offset(static_cast<std::uint32_t>(
            phase * spc_clocks_per_tick / current_tick_video_phases_));
        service_audio_irq(result.sound_effect_commands);
    }

    const std::array stops{ending_transfer_, ending_end_transfer_,
        credits_entry_ != 0U ? credits_entry_ : ending_transfer_};
    const auto task = map_.resume_native_task(ending_registers_, stops,
        50'000'000U, true, true);
    result.prelude_instructions += task.instructions;
    const auto native_map =
        (static_cast<std::uint32_t>(map_.read_native_byte(ram_symbol("MAPBANK")))
            << 16U)
        | 0x8000U
        | (map_.read_native_word(ram_symbol("MAPPTR")) & 0x7fffU);
    const auto reached_credits = !ending_final_score_
        && ((credits_entry_ != 0U && task.stop_address == credits_entry_)
            || (credits_entry_ == 0U && native_map == credits_map_));
    if (reached_credits || (task.returned && !ending_final_score_)) {
        ending_task_active_ = false;
        enter_credits();
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }

    // END_GAME_SEQ and ENDSEQ_L build their object lists inside native
    // TRANSFER/ENDTRANS loops. Import those exact objects at each source-frame
    // yield, then rebuild only the host presentation metadata they replace.
    refresh_player_reference();
    calculate_view();
    draw_order_ = objects_.active_handles();
    auto palette = std::array<std::uint16_t, 8U * 16U>{};
    for (std::size_t index = 0; index < palette.size(); ++index) {
        palette[index] = map_.read_native_word(
            ppu_palette_ + static_cast<std::uint32_t>(index * 2U));
    }
    map_.write_cgram(0U, palette);
    map_.upload_oam(sprite_block_, 544U);
    if (task.returned && ending_final_score_) {
        ending_task_active_ = false;
        ending_final_score_ = false;
        flow_state_ = GameFlowState::finished;
    }
    result.audio_port_writes = map_.take_apu_port_writes();
    return result;
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

void GameSimulation::select_planet_campaign(bool second_map) {
    const auto& campaign = second_map
        ? second_planet_campaign_ : first_planet_campaign_;
    initialize_planets_ = campaign.initialize;
    setup_planets_ = campaign.setup;
    setup_planet_palette_ = campaign.setup_palette;
    copy_planet_light_ = campaign.copy_light;
    draw_planet_sprites_ = campaign.draw_sprites;
    draw_selected_planet_ = campaign.draw_selected;
    draw_planet_in_centre_ = campaign.draw_centred;
    clear_planet_screen_ = campaign.clear_screen;
    dma_planet_screen_ = campaign.dma_screen;
    switch_planet_buffer_ = campaign.switch_buffer;
    draw_route_name_ = campaign.draw_route_name;
    draw_planet_lines_ = campaign.draw_lines;
    undraw_planet_lines_ = campaign.undraw_lines;
    move_ship_along_path_ = campaign.move_ship;
    start_planet_positions_ = campaign.start_positions;
    planet_sprites_ = campaign.sprites;
    planet_positions_ = campaign.positions;
    second_planet_campaign_active_ = second_map;
    if (map2_flag_ != 0U) {
        map_.write_native_byte(map2_flag_, second_map ? 1U : 0U);
    }
}

void GameSimulation::start_initial_route() {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(initialize_all_, registers, 5'000'000);
    if (starfox_ex_cartridge_) {
        // EX's cartridge boot starts on PLANETS2 with routes 4-6. The
        // original host entered PLANETS unconditionally and therefore hid
        // the entire second campaign.
        select_planet_campaign(true);
        map_.write_native_byte(which_route_, 4U);
    }
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
    bool selecting_route, std::uint32_t pending_map,
    bool wait_for_arrival_confirmation) {
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
    planet_arrival_confirmation_required_ = wait_for_arrival_confirmation;
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
    // PLANETS starts the ship diagonally; PLANETS2 points it to the right.
    // This value selects a different four-tile Arwing frame in IRQPLANETS.
    map_.write_native_word(
        ship_angle_, second_planet_campaign_active_ ? 2U : 1U);

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
    const auto initial_planet = selecting_route || selected_planet >= planet_count_
        ? std::uint8_t{0U} : selected_planet;
    const auto start = rom_->read16(start_planet_positions_
        + static_cast<std::uint32_t>(initial_planet) * 2U);
    map_.write_native_word(ship_position_, start);
    map_.write_native_word(new_ship_position_, start);
    map_.write_native_word(route_x_, 0U);
    draw_order_.clear();
    planet_spin_remainders_.fill(0);
    planet_rotation_video_phases_ = 0U;
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
        draw_selected_planet(true, advance_rotation);
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
    const auto draw_all_during_route =
        flow_state_ == GameFlowState::planet_travel
        && (frontend_phase_ == FrontendPhase::planet_fade_in
            || frontend_phase_ == FrontendPhase::planet_route);
    const auto selected_planet = map_.read_native_word(current_planet_);
    if (draw_all_during_route) {
        // PLANETS.ASM's .shownext loop stores -1 in CURRENTPLANET before each
        // redraw, which makes DRAWPLANETSPRITES include all sixteen map cells.
        // Keep the actual route destination for MOVESHIPALONGPATH below; using
        // it during the draw omitted that planet and left an empty map box.
        map_.write_native_word(current_planet_, 0xffffU);
    }
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(
        draw_planet_sprites_, registers, 5'000'000, true);
    if (draw_all_during_route) {
        map_.write_native_word(current_planet_, selected_planet);
    }
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
    // redraw. Presentation calls this routine at a fixed 20 Hz below; split
    // each measured source step over two such updates to preserve the same
    // real-time angular speed as one step across six 60 Hz raster phases.
    constexpr std::array<std::int32_t, 6> source_speeds{
        6 * 256, -3 * 256, 4 * 256, 3 * 256, -5 * 256, -5 * 256,
    };
    for (std::size_t index = 0; index < source_speeds.size(); ++index) {
        auto& remainder = planet_spin_remainders_[index];
        remainder += source_speeds[index];
        const auto delta = remainder / 2;
        remainder -= delta * 2;
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
    planet_arrival_confirmation_required_ = false;
    flow_state_ = GameFlowState::planet_travel;
    frontend_phase_ = FrontendPhase::planet_confirm_hold;
    frontend_frames_ = 0U;
    pepper_brightness_ = 0U;
}

void GameSimulation::present_frame() {
    map_.tick_video_phase();
    if (video_phases_since_tick_ != 0xffU) ++video_phases_since_tick_;
    if (flow_state_ == GameFlowState::intro
        && frontend_phase_ == FrontendPhase::intro_final_hold) {
        // INTRO_L presents the transfer in which the Arwing crosses the
        // camera before its EXITINTRO check arms the quick fade. Starting
        // the host fade in the strategy tick hid that completed close pass.
        // Preserve its first 60 Hz raster, then begin the source's 11,-2
        // brightness sequence on the following presentation.
        if (++frontend_frames_ >= 2U) {
            frontend_frames_ = 0U;
            map_.set_display_brightness(11U);
            map_.start_display_fade(-2);
            frontend_phase_ = FrontendPhase::intro_fade_to_title;
        }
    }
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
    auto advance_planet_rotation = false;
    if (flow_state_ == GameFlowState::planet_select
        || flow_state_ == GameFlowState::planet_travel) {
        // Planet drawing is presentation work, but its rotation belongs to
        // the cartridge's 20 Hz pace. Keep redrawing the route/ship at the
        // 60 Hz raster rate while changing the six planet angles only once
        // every three phases. Since host FPS is converted to this fixed
        // raster first, 60, 120, 240, 360 and 480 FPS all see the same spin.
        if (++planet_rotation_video_phases_ >= 3U) {
            planet_rotation_video_phases_ = 0U;
            advance_planet_rotation = true;
        }
    } else {
        planet_rotation_video_phases_ = 0U;
    }
    animate_planet_frame(advance_planet_rotation);
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
    briefing_planet_address_ = planet < planet_count_
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
        const auto switch_to_first = starfox_ex_cartridge_
            && second_planet_campaign_active_
            && (input.pressed & starfox::input::right_shoulder) != 0U;
        const auto switch_to_second = starfox_ex_cartridge_
            && !second_planet_campaign_active_
            && (input.pressed & starfox::input::left_shoulder) != 0U;
        if (switch_to_first || switch_to_second) {
            map_.write_native_byte(first_download_, 1U);
            map_.write_native_byte(once_wipe_, 1U);
            map_.write_native_byte(which_route_, switch_to_second ? 4U : 0U);
            Wdc65816Registers registers;
            registers.status = 0x24U;
            map_.call_native_routine(
                switch_to_second ? initialize_all_ : initialize_all_2_,
                registers, 5'000'000, true);
            select_planet_campaign(switch_to_second);
            if (switch_to_first) {
                // PLANETSEQ_L begins with CONVERTROUTE. The freshly selected
                // route 0 is displayed as the cartridge's route 1 (NORMAL)
                // until the second conversion immediately before gameplay.
                map_.write_native_byte(which_route_, 1U);
            }
            route_display_order_ = true;
            enter_planet_map(true);
            result.audio_port_writes = map_.take_apu_port_writes();
            return result;
        }
        auto route = map_.read_native_byte(which_route_);
        const auto first_route = second_planet_campaign_active_
            ? std::uint8_t{4U} : std::uint8_t{0U};
        const auto route_count = second_planet_campaign_active_
            ? std::uint8_t{3U}
            : static_cast<std::uint8_t>(starfox_ex_cartridge_ ? 4U : 3U);
        bool changed = false;
        // PLANETS and PLANETS2 test LEFT|SELECT|UP as the previous-course
        // group. RIGHT and DOWN fall through to the next-course branch.
        if ((input.pressed & (starfox::input::left | starfox::input::up
                | starfox::input::select)) != 0U) {
            route = route == first_route
                ? static_cast<std::uint8_t>(first_route + route_count - 1U)
                : static_cast<std::uint8_t>(route - 1U);
            changed = true;
        }
        if ((input.pressed & (starfox::input::right | starfox::input::down)) != 0U) {
            route = static_cast<std::uint8_t>(first_route
                + (route + 1U - first_route) % route_count);
            changed = true;
        }
        if (changed) {
            map_.write_native_byte(which_route_, route);
            // Both PLANETS.ASM and PLANETS2.ASM preserve the player's real
            // course selection separately from WHICHROUTE's temporary
            // route-display conversion. EX consults ACTUALROUTE when its
            // shortened briefing path jumps directly into the first stage.
            if (actual_route_ != 0U) {
                map_.write_native_byte(actual_route_, route);
            }
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
        const auto arrived = planet_travel_complete_ || flow_ticks_ >= 240U;
        const auto confirmed = !planet_arrival_confirmation_required_
            || (input.pressed & starfox::input::a) != 0U;
        if (arrived && confirmed) {
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
        if (exit == 8U) {
            // CREDITS.ASM intentionally continues presenting its completed
            // THE END composition. Freezing GameSimulation here stopped its
            // paths, prevented EX's final score overlay, and ignored the
            // source's post-credits input loop.
            map_.write_native_word(level_finished_, 0U);
            if (starfox_ex_cartridge_) {
                credits_complete_ = true;
            } else if (final_total_score_ != 0U
                && ending_transfer_ != 0U) {
                // Retail MAIN.ASM follows CREDITSMAP with MAKETOTALSCORE2,
                // then holds that final composition forever. Run the native
                // routine as a resumable transfer task so its aggregate score
                // is actually shown instead of leaving the credits flyby up.
                ending_registers_ = {};
                ending_registers_.status = 0x24U;
                const std::array stops{ending_transfer_,
                    ending_end_transfer_ != 0U
                        ? ending_end_transfer_ : ending_transfer_,
                    ending_transfer_};
                const auto task = map_.begin_native_near_task(
                    final_total_score_, ending_registers_, stops,
                    50'000'000U, true);
                ending_final_score_ = true;
                if (task.returned) {
                    ending_final_score_ = false;
                    flow_state_ = GameFlowState::finished;
                } else {
                    ending_task_active_ = true;
                }
                refresh_player_reference();
                draw_order_ = objects_.active_handles();
            } else {
                flow_state_ = GameFlowState::finished;
            }
        }
        return;
    }
    if (exit == 10U) {
        enter_game_over();
        return;
    }
    if (exit == 9U) {
        enter_credits();
        return;
    }
    if (exit == 8U) {
        flow_state_ = GameFlowState::finished;
        return;
    }
    pending_end_game_ = exit == 6U;
    // MAIN.ASM increments STAGE before normal, special and final route exits.
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
        enter_planet_map(false, next_map, true);
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
    ex_results_recorded_ = false;
    previous_total_percentage_ = 0U;
    const auto pointer = map_.read_native_word(percentage_pointer_);
    for (std::uint16_t index = 0; index < pointer; ++index) {
        const auto value = map_.read_native_byte(percentage_buffer_ + index);
        if (value != 101U) {
            previous_total_percentage_ = static_cast<std::uint16_t>(
                previous_total_percentage_ + value);
        }
    }
    map_.write_native_word(meters_enabled_, 0U);
    flow_ticks_ = 0U;
    frontend_phase_ = FrontendPhase::none;
    flow_state_ = GameFlowState::stage_results;
    if (starfox_ex_cartridge_) {
        // MAIN.ASM enters END_LEVEL_SEQ as a local JSR after incrementing
        // STAGE and marking the end sequence active. Run that exact source
        // routine as a resumable near task, yielding before each TRANSFER_L
        // so one source iteration remains one native 20 Hz logic frame.
        map_.write_native_byte(ex_doing_end_, 1U);
        map_.write_native_byte(ex_crosshair_on_, 0U);
        ex_results_registers_ = {};
        ex_results_registers_.status = 0x24U;
        const std::array frame_stops{ex_transfer_};
        const auto task = map_.begin_native_near_task(
            ex_end_level_sequence_, ex_results_registers_, frame_stops,
            20'000'000U, true);
        ex_results_task_active_ = !task.returned;
        displayed_stage_percentage_ = static_cast<std::uint8_t>(
            std::min<std::uint16_t>(100U,
                map_.read_native_word(ex_current_percentage_)));
        stage_percentage_ = static_cast<std::uint8_t>(
            std::min<std::uint16_t>(100U,
                map_.read_native_word(ex_target_percentage_)));
        if (task.returned) {
            ex_results_recorded_ = true;
            map_.write_native_word(level_finished_, 0U);
            if (pending_end_game_) {
                begin_end_game_sequence();
            } else {
                map_.start_display_fade(-1);
                frontend_phase_ = FrontendPhase::stage_results_fade_to_map;
            }
        }
        return;
    }
    map_.write_native_word(level_finished_, 0U);
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
    if (pending_end_game_ && !starfox_ex_cartridge_) {
        // Retail's final END_LEVEL_SEQ only stores the last percentage and
        // immediately falls into END_GAME_SEQ; it has no ordinary per-stage
        // tally hold here.
        finish_stage_results();
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }
    if (frontend_phase_ == FrontendPhase::stage_results_fade_to_map) {
        if (map_.fade_direction() == 0 && map_.display_brightness() == 0U) {
            finish_stage_results();
        }
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }
    if (starfox_ex_cartridge_ && ex_results_task_active_) {
        const std::array frame_stops{ex_transfer_};
        const auto task = map_.resume_native_task(
            ex_results_registers_, frame_stops, 20'000'000U, true, true);
        refresh_player_reference();
        displayed_stage_percentage_ = static_cast<std::uint8_t>(
            std::min<std::uint16_t>(100U,
                map_.read_native_word(ex_current_percentage_)));
        stage_percentage_ = static_cast<std::uint8_t>(
            std::min<std::uint16_t>(100U,
                map_.read_native_word(ex_target_percentage_)));
        stage_hit_score_ = map_.read_native_word(player_score_);
        if (task.returned) {
            ex_results_task_active_ = false;
            ex_results_recorded_ = true;
            map_.write_native_word(level_finished_, 0U);
            if (pending_end_game_) {
                begin_end_game_sequence();
            } else {
                // END_LEVEL_SEQ returns to MAIN.ASM immediately before its
                // source FADEDOWN loop. Keep the completed native bitmap
                // visible while the host fade reaches true black.
                map_.start_display_fade(-1);
                frontend_phase_ = FrontendPhase::stage_results_fade_to_map;
            }
        }
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }
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
        // MAIN.ASM runs FADEDOWN after END_LEVEL_SEQ and only then jumps into
        // PLANETSEQ. Keep the completed tally on screen during that source
        // fade so the new Mode 3 tile/planet setup begins from true black.
        map_.start_display_fade(-1);
        frontend_phase_ = FrontendPhase::stage_results_fade_to_map;
    }
    result.audio_port_writes = map_.take_apu_port_writes();
    return result;
}

void GameSimulation::finish_stage_results() {
    if (!ex_results_recorded_) {
        const auto pointer = map_.read_native_word(percentage_pointer_);
        map_.write_native_byte(percentage_buffer_ + pointer, stage_percentage_);
        map_.write_native_word(percentage_pointer_,
            static_cast<std::uint16_t>(pointer + 1U));
    }
    if (starfox_ex_cartridge_) {
        map_.write_native_byte(ex_doing_end_, 0U);
    }
    ex_results_recorded_ = false;
    if (pending_end_game_) {
        begin_end_game_sequence();
        return;
    }
    const auto next_map = selected_route_stage(map_.read_native_word(stage_));
    enter_planet_map(false, next_map, true);
}

void GameSimulation::service_transfer_request() {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    auto flags = map_.read_native_byte(background_flags_);
    auto restarted = false;
    if ((flags & 1U) != 0U) {
        restarted = true;
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
        map_.write_native_byte(fade_to_red_, 0U);
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
    if (restarted && boss_music_before_death_) {
        // Player death replaces the active boss track with the short death
        // drumroll. The map interpreter resumes inside its boss wait and does
        // not execute SETBGM a second time, so restart the track that was
        // active before death after the background restart has finished.
        request_music(*boss_music_before_death_);
    }
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
    // Snapshot the live encounter track before SETBLACK/background setup or
    // either cartridge's strategies can clear the last boss-health marker.
    // EX does that earlier in its transfer than retail; taking this sample
    // near the draw-list build missed EX and left both death recovery music
    // and post-boss communication timing without an encounter boundary.
    if (flow_state_ == GameFlowState::gameplay
        && map_.read_native_byte(boss_max_health_) != 0U
        && (map_.read_native_byte(game_flags_) & 0x42U) == 0U) {
        const auto music = map_.read_native_byte(background_music_command_);
        if (music != 0U && music != 0x11U && music != 0xf1U) {
            boss_music_before_death_ = music;
        }
    }
    const auto communication_active =
        map_.read_native_byte(message_count_1_) != 0U
        || map_.read_native_byte(message_count_2_) != 0U
        || (friends_message_2_ != 0U
            && (map_.read_native_byte(message_count_1_2_) != 0U
                || map_.read_native_byte(message_count_2_2_) != 0U));
    if (!communication_active) {
        post_boss_dialogue_active_ = false;
    } else if (flow_state_ == GameFlowState::gameplay
        && boss_music_before_death_.has_value()
        && map_.read_native_byte(boss_max_health_) == 0U) {
        // Once the defeated-boss message begins, keep its source 20 Hz cadence
        // even if EX's following strategy pass briefly rebuilds M_BOSSMAXHP
        // from a lingering component. The communication queue, not that
        // transient draw-meter byte, owns the end of this timing interval.
        post_boss_dialogue_active_ = true;
    }
    complete_video_phases_for_tick();
    if (ending_task_active_) {
        return tick_end_game_sequence(input);
    }
    if (flow_state_ == GameFlowState::credits && credits_complete_
        && starfox_ex_cartridge_
        && (input.pressed & starfox::input::start) != 0U) {
        // EX's completed CREDITSMAP tail-jumps into FOXY_CONTINUE_L. A native
        // tail jump cannot return to the host's bounded map-code call, so make
        // that exact ownership handoff here once the source prompt accepts
        // START.
        queue_sound_effect(0xabU);
        Wdc65816Registers registers;
        registers.status = 0x24U;
        map_.call_native_routine(
            ex_randomize_background_, registers, 5'000'000U, true);
        map_.write_native_byte(ex_fade_palette_fx_pink_, 33U);
        map_.write_native_byte(ex_fade_palette_yamao_, 33U);
        enter_ex_pregame_menu(false);
        GameTickResult result;
        result.audio_port_writes = map_.take_apu_port_writes();
        return result;
    }
    if (starfox_ex_cartridge_) {
        // EX's source MAX FPS experiment is intentionally disabled. Keep the
        // cartridge on its stable/default 20-transfer path regardless of an
        // old save or a direct diagnostic write; host presentation FPS stays
        // independent and may still be selected from 20 through 480.
        map_.write_native_byte(ex_fps_speed_, 0U);
        map_.write_native_byte(ex_ntsc_pal_swap_, 0U);
    }
    if (flow_state_ == GameFlowState::gameplay
        || flow_state_ == GameFlowState::training
        || flow_state_ == GameFlowState::game_over) {
        ++source_update_sequence_;
    }
    if (flow_state_ == GameFlowState::pregame_menu) {
        return tick_pregame_menu(input);
    }
    if (flow_state_ == GameFlowState::ex_pregame_menu) {
        return tick_ex_pregame_menu(input);
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
    GameTickResult result;
    auto ex_pause_step = false;
    auto ex_pause_model_refresh = false;
    auto ex_pause_transfer = false;
    if (paused_) {
        write_input(input);
        if (starfox_ex_cartridge_) {
            // MAIN.ASM's DOPAUSE loop calls STRATDEBUG_L once per frozen
            // transfer.  This is EX's real interactive pause menu, not just
            // a diagnostic overlay: it edits the current weapon, double
            // shot, model, BGM, borders, crosshair, exploration, stepping
            // and fire-rate variables.  Keep the bitmap lifecycle at the
            // same source boundary while the host retains the last geometry
            // frame beneath it.
            map_.write_native_byte(ex_freeze_strategies_, 1U);
            map_.begin_superfx_bitmap_frame();
            Wdc65816Registers pause_registers;
            pause_registers.status = 0x24U;
            result.prelude_instructions += map_.call_native_routine(
                ex_strat_debug_, pause_registers, 20'000'000, true);
            // DEBUG.ASM either clears FREEZESTRATS for STEP BY STEP or sets
            // bit 2 after changing MODEL. Both continue through one special
            // TRANSFER_L pass; DOPAUSE restores the ordinary freeze at the
            // head of its next menu iteration.
            const auto freeze_state =
                map_.read_native_byte(ex_freeze_strategies_);
            const auto remain_paused =
                (input.pressed & starfox::input::start) == 0U;
            ex_pause_step = remain_paused && freeze_state == 0U;
            ex_pause_model_refresh = remain_paused
                && (freeze_state & 2U) != 0U;
            ex_pause_transfer = ex_pause_step || ex_pause_model_refresh;
            if (!ex_pause_transfer) map_.submit_superfx_bitmap();
        }
        if ((input.pressed & starfox::input::start) != 0U) {
            paused_ = false;
            map_.write_native_byte(pause_sound_, 1U);
            if (starfox_ex_cartridge_) {
                // DOPAUSE applies a newly selected BGM only after leaving
                // the menu and restores the debugger state it temporarily
                // forced for the expanded overlay.
                if (map_.read_native_byte(ex_menu_selected_) == 3U) {
                    map_.write_native_byte(ex_set_new_bgm_, 1U);
                }
                map_.write_native_byte(ex_freeze_strategies_, 0U);
                map_.write_native_byte(ex_debug_alien_,
                    map_.read_native_byte(ex_debug_backup_));
                map_.write_native_byte(ex_debug_flash_, 0U);
                map_.write_native_byte(controller_high_, 0U);
                map_.write_native_byte(controller_low_, 0U);
                map_.write_native_byte(previous_controller_high_, 0U);
                map_.write_native_byte(previous_controller_low_, 0U);
                // The following MAIN transfer clears the pause/debug bitmap
                // before drawing gameplay. The host does not launch the GSU
                // geometry pass, so perform that clear at the same handoff
                // to keep the menu from burning into subsequent frames.
                map_.begin_superfx_bitmap_frame();
                map_.submit_superfx_bitmap();
            }
        }
        if (!ex_pause_transfer) {
            for (std::size_t phase = 0; phase < video_phases_per_tick; ++phase) {
                map_.set_apu_clock_offset(static_cast<std::uint32_t>(
                    phase * spc_clocks_per_tick / video_phases_per_tick));
                service_audio_irq(result.sound_effect_commands);
            }
            result.audio_port_writes = map_.take_apu_port_writes();
            return result;
        }
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
    const auto ex_title_logo_active = starfox_ex_cartridge_
        && flow_state_ == GameFlowState::title
        && ex_title_intro_background_ != 0U
        && map_.background() == ex_title_intro_background_;
    const auto ex_title_model_test_requested = starfox_ex_cartridge_
        && flow_state_ == GameFlowState::title
        && !ex_title_logo_active
        && frontend_phase_ == FrontendPhase::none
        && flow_ticks_ >= 40U
        && (input.held & (starfox::input::left_shoulder
                | starfox::input::select))
            == (starfox::input::left_shoulder | starfox::input::select);
    auto native_input = input;
    if (flow_state_ == GameFlowState::title && !ex_title_logo_active) {
        // The PC front end owns the title-screen START transition. The EX
        // title map handles the same edge by entering TRANSFER_L and then
        // tail-jumping into its persistent special-menu loop. Following that
        // second main loop from this bounded UPDATE_OBJECTS_L call can never
        // return, so keep START out of the cartridge map while the host
        // performs the fade and changes front-end state below.
        constexpr auto start_mask = starfox::input::start;
        native_input.held = static_cast<input::ButtonMask>(
            native_input.held & ~start_mask);
        native_input.pressed = static_cast<input::ButtonMask>(
            native_input.pressed & ~start_mask);
        native_input.released = static_cast<input::ButtonMask>(
            native_input.released & ~start_mask);
        if (ex_title_model_test_requested) {
            // EX TITLE.ASM handles L+Select before START and tail-jumps into
            // FOXY_CONTINUE_L after its fade. UPDATE_OBJECTS_L is a bounded
            // host call, so withhold only the Select bit that would take the
            // persistent jump; the exact state handoff is reproduced below.
            constexpr auto select_mask = starfox::input::select;
            native_input.held = static_cast<input::ButtonMask>(
                native_input.held & ~select_mask);
            native_input.pressed = static_cast<input::ButtonMask>(
                native_input.pressed & ~select_mask);
            native_input.released = static_cast<input::ButtonMask>(
                native_input.released & ~select_mask);
        }
    }
    // The pause debugger and the stepped transfer consume the same IRQ input
    // sample. Re-latching here would turn the current buttons into their own
    // previous state and break source release/edge tests inside strategies.
    if (!ex_pause_transfer) write_input(native_input);
    if (!ex_pause_transfer && starfox_ex_cartridge_
        && (flow_state_ == GameFlowState::gameplay
            || flow_state_ == GameFlowState::training)) {
        // EX MAIN.ASM can replace the active SPC bank from its sound-test
        // option while gameplay is running.  Follow its exact reset-then-load
        // sequence through the assembled routines so the real upload bytes,
        // ports and driver state reach the PC SPC700 emulator.
        if (map_.read_native_byte(ex_bgm_sfx_) < 2U
            && map_.read_native_byte(ex_set_new_bgm_) != 0U) {
            const auto playlist = map_.read_native_byte(ex_cursed_bgm_) != 0U
                ? ex_bgm_playlist_cursed_ : ex_bgm_playlist_;
            const auto index = static_cast<std::uint32_t>(
                map_.read_native_word(ex_bgm_test_) & 0x00ffU);
            const auto routine = rom_->read16(playlist + index * 2U);
            map_.write_native_word(ex_text_pointer_, routine);
            map_.write_native_byte(ex_text_pointer_ + 2U,
                static_cast<std::uint8_t>(playlist >> 16U));
            Wdc65816Registers music_registers;
            music_registers.status = 0x24U;
            result.prelude_instructions += map_.call_native_routine(
                ex_do_bgm_reset_, music_registers, 50'000'000, true);
            music_registers = {};
            music_registers.status = 0x24U;
            result.prelude_instructions += map_.call_native_routine(
                ex_do_bgm_generic_, music_registers, 50'000'000, true);
            map_.write_native_byte(ex_set_new_bgm_, 0U);
        }
        // These are the EX-only option gates at the top of MAIN.ASM's
        // gameplay loop, immediately before SETBLACK/TRANSFER.  The source
        // deliberately latches HUD and SFX suppression until the next map
        // initialization and clears DOTSFLAG for every non-default particle
        // selection except option 2.
        if (map_.read_native_byte(ex_no_hud_) != 0U) {
            map_.write_native_word(meters_enabled_, 0U);
        }
        if ((map_.read_native_byte(ex_no_sfx_) & 1U) != 0U) {
            map_.write_native_byte(ex_no_set_port_3_, 1U);
        }
        const auto dots_stars = map_.read_native_byte(ex_dots_stars_);
        if (dots_stars != 0U && dots_stars != 2U) {
            map_.write_native_word(ex_dots_flag_, 0U);
        }
    }
    if (flow_state_ == GameFlowState::title
        || (!ex_pause_transfer && starfox_ex_cartridge_
            && (flow_state_ == GameFlowState::gameplay
                || flow_state_ == GameFlowState::training))) {
        // TRANS.ASM clears the two halves of BITMAP1 around its asynchronous
        // IRQ upload before the title/gameplay PRINTT_L calls populate the
        // next frame. Do that logical clear atomically for host presentation.
        map_.begin_superfx_bitmap_frame();
    }
    Wdc65816Registers registers;
    registers.status = 0x24U;
    // MAIN/CONT/ENDSEQ call SETBLACK_L once around every source transfer.
    // Besides drawing the black colour window it releases STAYBLACK, which
    // is also the native movement gate used after a death restart.
    if (!ex_pause_transfer) {
        result.prelude_instructions += map_.call_native_routine(
            set_black_, registers, 2'000'000, true);
    }
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
    if (!ex_pause_model_refresh) {
        result.prelude_instructions += strategies_.begin_tick();
    }
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
    if (ex_pause_model_refresh) {
        // FREEZESTRATS bit 2 branches to DOSTRATS2, whose complete active
        // body is a single JSL SETSHIP for PLAYPT. It deliberately skips
        // GAMEFRAME, INIT_STRATS, UPDATE_OBJECTS and every object strategy.
        registers = {};
        registers.status = 0x24U;
        result.prelude_instructions += map_.call_native_routine(
            ex_set_ship_, registers, 5'000'000, true);
        refresh_player_reference();
    } else {
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
        if (starfox_ex_cartridge_
            && map_.read_native_byte(ex_no_objects_) != 0U) {
            // EX TRANS.ASM keeps only PLAYPT and the body/left-wing/right-wing
            // collision objects alive while NOOBJMODE is enabled. Those four
            // objects still run their normal strategies; every other object is
            // removed without executing its strategy.
            const std::array<ObjectHandle, 4> protected_objects{
                player_,
                handle_from_native_pointer(
                    map_.read_native_word(player_collision_box_)),
                handle_from_native_pointer(
                    map_.read_native_word(player_left_wing_collision_box_)),
                handle_from_native_pointer(
                    map_.read_native_word(player_right_wing_collision_box_)),
            };
            result.strategies =
                strategies_.tick_all_no_objects(protected_objects);
        } else {
            result.strategies = strategies_.tick_all();
        }
        refresh_player_reference();
        apply_god_mode_state();
        if (god_mode_) {
            map_.write_native_word(special_weapon_count_, std::max(
                god_mode_bombs_before,
                map_.read_native_word(special_weapon_count_)));
        }
        service_god_nuke(input, nukes_before_strategies);
    }

    // TRANS.ASM snapshots these after strategies for release-edge controls
    // such as view toggling.
    map_.write_native_byte(last_controller_high_,
                           map_.read_native_byte(controller_high_));
    map_.write_native_byte(last_controller_low_,
                           map_.read_native_byte(controller_low_));
    if (starfox_ex_cartridge_) {
        map_.write_native_byte(ex_last_controller_2_high_,
            map_.read_native_byte(ex_controller_2_high_));
        map_.write_native_byte(ex_last_controller_2_low_,
            map_.read_native_byte(ex_controller_2_low_));
        for (std::size_t index = 0;
             index < ex_last_multitap_controllers_.size(); ++index) {
            map_.write_native_word(ex_last_multitap_controllers_[index],
                map_.read_native_word(ex_multitap_controllers_[index + 2U]));
        }
    }

    // GETVIEW_L delegates its matrices and camera offset to Super FX. Model
    // that fixed-point path natively; running it against the temporary
    // coprocessor-complete stub would reuse stale m_wmat/m_big values.
    if (!ex_pause_model_refresh) calculate_view();
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
        dust_.tick(camera, world, map_.dots_mode() < 0, dust_point_count());
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
    const auto call_transfer_routine = [&](std::uint32_t routine) {
        registers = {};
        registers.status = 0x24U;
        result.prelude_instructions += map_.call_native_routine(
            routine, registers, 5'000'000, true);
    };
    // EX TRANS.ASM's NOBGMODE branch skips every palette update but still
    // continues through SHOWVIEW/BUILD_DRAWLIST/DO_SPRITES. Its added level
    // palettes also have two deliberate cadence dividers: TEMPVAL5 runs its
    // group every fourth transfer and TEMPVAL6 every eleventh transfer.
    if (!starfox_ex_cartridge_
        || map_.read_native_byte(ex_no_background_mode_) == 0U) {
        call_transfer_routine(palette_goto_);
        call_transfer_routine(fade_palette_);
        if (starfox_ex_cartridge_) {
            for (const auto routine : ex_palette_every_transfer_) {
                call_transfer_routine(routine);
            }
            auto slow = map_.read_native_byte(ex_palette_slow_counter_);
            if ((static_cast<std::uint8_t>(slow - 3U) & 0x80U) == 0U) {
                map_.write_native_byte(ex_palette_slow_counter_, 0U);
                for (const auto routine : ex_palette_every_fourth_transfer_) {
                    call_transfer_routine(routine);
                }
            } else {
                map_.write_native_byte(ex_palette_slow_counter_,
                    static_cast<std::uint8_t>(slow + 1U));
            }
            auto slower = map_.read_native_byte(ex_palette_slower_counter_);
            if ((static_cast<std::uint8_t>(slower - 10U) & 0x80U) == 0U) {
                map_.write_native_byte(ex_palette_slower_counter_, 0U);
                for (const auto routine : ex_palette_every_eleventh_transfer_) {
                    call_transfer_routine(routine);
                }
            } else {
                map_.write_native_byte(ex_palette_slower_counter_,
                    static_cast<std::uint8_t>(slower + 1U));
            }
        }
    }
    call_transfer_routine(do_sprites_);
    // TRANS.ASM applies table-driven window wipes after the 3D display and
    // sprite list have been prepared. DO_CIRCLE_EXPLOSION_L above selects a
    // wipe frame; this routine rasterizes its per-scanline window bounds and
    // advances the source table for presentation.
    registers = {};
    registers.status = 0x24U;
    result.prelude_instructions += map_.call_native_routine(
        do_window_wipe_, registers, 5'000'000, true);
    if (map_.read_native_byte(do_a_wipe_) != 0U) {
        wipe_logic_snapshot_ = map_.read_native_byte(wipe_logic_);
    }
    if (!ex_pause_transfer && (flow_state_ == GameFlowState::gameplay
            || flow_state_ == GameFlowState::training
            || flow_state_ == GameFlowState::intro)) {
        // MAIN.ASM calls this immediately after TRANS_L in gameplay, while
        // ENDSEQ.ASM's INTRO_L makes the same two calls during EX's intro.
        // Their Super FX output is host-rendered, but the original 65C816
        // routines still control portrait animation, text lifetime and
        // voice/effect commands in both flows.
        const auto first_message_was_speaking =
            map_.read_native_byte(message_count_1_) != 0U;
        const auto second_message_was_speaking = friends_message_2_ != 0U
            && map_.read_native_byte(message_count_1_2_) != 0U;
        registers = {};
        registers.status = 0x24U;
        result.prelude_instructions += map_.call_native_routine(
            friends_messages_, registers, 5'000'000, true);
        if (friends_messages_2_ != 0U) {
            registers = {};
            registers.status = 0x24U;
            result.prelude_instructions += map_.call_native_routine(
                friends_messages_2_, registers, 5'000'000, true);
        }
        const auto communication_just_finished =
            (first_message_was_speaking
                && map_.read_native_byte(message_count_1_) == 0U)
            || (second_message_was_speaking
                && map_.read_native_byte(message_count_1_2_) == 0U);
        if (communication_just_finished
            && (!starfox_ex_cartridge_
                || map_.read_native_byte(ex_no_sfx_) == 0U)) {
            // Both cartridge routines issue effect $64 on the last speaking
            // frame to release the SPC driver's dialogue BGM attenuation.
            // A full 16-entry source ring silently overwrites that command;
            // this is much easier to hit in EX because two portrait channels
            // and its extra ambient effects share port 3. Preserve one copy
            // after the routines have run so a busy frame cannot leave the
            // music permanently ducked.
            auto restore_is_queued =
                map_.read_native_byte(sound_pending_) == 0x64U;
            auto read = static_cast<std::uint8_t>(
                map_.read_native_byte(sound_read_) & 15U);
            const auto write = static_cast<std::uint8_t>(
                map_.read_native_byte(sound_write_) & 15U);
            while (!restore_is_queued && read != write) {
                restore_is_queued =
                    map_.read_native_byte(sound_buffer_ + read) == 0x64U;
                read = static_cast<std::uint8_t>((read + 1U) & 15U);
            }
            if (!restore_is_queued) queue_sound_effect(0x64U);
        }
        // Star Fox EX MAIN.ASM invokes CESTIMER_L after both communications
        // channels on every scored-mode gameplay frame.  Despite its legacy
        // name, the shipped routine draws the live SCORE label/value into the
        // bitmap; skipping it makes EX's scored mode appear inert.
        if (ex_ces_timer_ != 0U
            && (map_.read_native_byte(ex_scored_) & 1U) != 0U) {
            registers = {};
            registers.status = 0x24U;
            result.prelude_instructions += map_.call_native_routine(
                ex_ces_timer_, registers, 5'000'000, true);
        }
    }
    // IRQBIT3 uploads PAL0PALETTE to all eight BG palette rows after the
    // source transfer routines have advanced their fades and colour cycles.
    // GAMEPALBUFF is only the stable 3D-palette source used while a new BG2
    // screen is installed; copying that row alone here discarded live EX
    // palette effects and could leave both backgrounds and models one state
    // behind the cartridge. Mirror the complete source IRQ DMA instead.
    auto current_background_palette = std::array<std::uint16_t, 8U * 16U>{};
    for (std::size_t index = 0; index < current_background_palette.size(); ++index) {
        current_background_palette[index] = map_.read_native_word(
            ppu_palette_ + static_cast<std::uint32_t>(index) * 2U);
    }
    map_.write_cgram(0U, current_background_palette);
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
    if (flow_state_ == GameFlowState::title) {
        // TITLE.ASM prints the current EX version through PRINTT_L into the
        // Super FX bitmap. The host replaces geometry rendering, but the
        // cartridge still owns these bitmap text pixels and its FOXIRQ
        // double-buffer swap.
        map_.submit_superfx_bitmap();
    }
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
    draw_ex_transfer_overlay(result);
    if (flow_state_ == GameFlowState::game_over) {
        ++flow_ticks_;
        // MAIN.ASM presents 50 transfers, then accepts START (or waits up to
        // 60 seconds) before opening FOXY_CONTINUE_L.
        if (flow_ticks_ >= 17U
            && ((input.pressed & starfox::input::start) != 0U
                || flow_ticks_ >= 1'250U)) {
            enter_continue_screen();
        }
    } else if (flow_state_ == GameFlowState::title) {
        ++flow_ticks_;
        if ((frontend_phase_ == FrontendPhase::title_fade_to_controls
                || frontend_phase_
                    == FrontendPhase::title_fade_to_ex_model_test)
            && map_.fade_direction() == 0
            && map_.display_brightness() == 0U) {
            if (starfox_ex_cartridge_) {
                // TITLE.ASM calls RANDOMIZEBG immediately after its completed
                // fade and immediately before tail-jumping to
                // FOXY_CONTINUE_L. START is masked from the persistent title
                // map above so its jump cannot strand a bounded host call;
                // preserve the skipped source-side random-background choice
                // at that same handoff boundary.
                const auto model_test = frontend_phase_
                    == FrontendPhase::title_fade_to_ex_model_test;
                if (model_test) {
                    map_.write_native_byte(
                        ex_bg2_vertical_offset_override_, 0U);
                    map_.write_native_word(ex_foxy_pointer_, 2U);
                    map_.write_native_word(ex_foxy_shape_,
                        static_cast<std::uint16_t>(ex_model_test_shape_));
                }
                Wdc65816Registers menu_registers;
                menu_registers.status = 0x24U;
                map_.call_native_routine(ex_randomize_background_,
                    menu_registers, 5'000'000, true);
                if (model_test) {
                    map_.write_native_byte(ex_fade_palette_fx_pink_, 33U);
                    map_.write_native_byte(ex_fade_palette_yamao_, 33U);
                }
                enter_ex_pregame_menu(model_test);
            } else {
                map_.write_native_byte(controls_exit_, 0U);
                map_.write_native_byte(default_training_, 0U);
                enter_controls(GameFlowState::controls_type);
            }
        } else if (frontend_phase_ == FrontendPhase::title_fade_to_intro
                   && map_.fade_direction() == 0
                   && map_.display_brightness() == 0U) {
            enter_intro();
        }
        // TITLESEQ_L ignores START until GAMEFRAME reaches 40, then enters
        // CONT.ASM's controller/training selection screen.
        if (frontend_phase_ == FrontendPhase::none
            && ex_title_model_test_requested) {
            // This is TITLE.ASM's documented L+Select shortcut. Its sound,
            // page/model selector values and fade occur before the direct
            // FOXY_CONTINUE_L jump; enter_ex_pregame_menu resumes from that
            // same source boundary once black is reached.
            map_.write_native_byte(ex_stop_counting_, 10U);
            map_.write_native_byte(ex_menu_selected_, 1U);
            map_.write_native_byte(ex_credits_, 1U);
            map_.write_native_byte(ex_page_number_, 3U);
            queue_sound_effect(0xabU);
            map_.start_display_fade(-3);
            frontend_phase_ = FrontendPhase::title_fade_to_ex_model_test;
        } else if (frontend_phase_ == FrontendPhase::none
            && !ex_title_logo_active && flow_ticks_ >= 40U
            && (input.pressed & starfox::input::start) != 0U) {
            queue_sound_effect(starfox_ex_cartridge_ ? 0xabU : 0x10U);
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
            && (input.pressed != 0U || input.held != 0U)) {
            // INTRO_L explicitly starts its quick fade from brightness 11,
            // even when the user skips while the screen is fully bright.
            map_.set_display_brightness(11U);
            map_.start_display_fade(-2);
            frontend_phase_ = FrontendPhase::intro_fade_to_title;
        } else if (frontend_phase_ == FrontendPhase::none && flow_ticks_ >= 30U
                   && map_.read_native_byte(exit_intro_) != 0U) {
            // The automatic ending is checked after the completed transfer;
            // defer only its fade so the near-camera ship frame is visible.
            frontend_frames_ = 0U;
            frontend_phase_ = FrontendPhase::intro_final_hold;
        }
    } else if (flow_state_ == GameFlowState::controls_type) {
        ++flow_ticks_;
        if (starfox_ex_cartridge_) {
            // CONT.ASM exposes EX's ship selector only on this interactive
            // model screen: hold X and tap R/L. Invoke the cartridge's own
            // selection routines so its complete ship table and every
            // dependent player variable remain authoritative. KEYRDOWN and
            // KEYLDOWN are words in the source and are released only when X
            // is released, exactly matching the original debounce behavior.
            const auto x_held = (input.held & starfox::input::x) != 0U;
            Wdc65816Registers ship_registers;
            ship_registers.status = 0x24U;
            if (x_held
                && (input.held & starfox::input::right_shoulder) != 0U
                && map_.read_native_word(next_ship_key_down_) == 0U) {
                map_.call_native_routine(
                    select_next_ship_, ship_registers, 5'000'000, true);
                map_.write_native_word(next_ship_key_down_, 1U);
            }
            if (!x_held) map_.write_native_word(next_ship_key_down_, 0U);

            ship_registers = {};
            ship_registers.status = 0x24U;
            if (x_held
                && (input.held & starfox::input::left_shoulder) != 0U
                && map_.read_native_word(previous_ship_key_down_) == 0U) {
                map_.call_native_routine(
                    select_previous_ship_, ship_registers, 5'000'000, true);
                map_.write_native_word(previous_ship_key_down_, 1U);
            }
            if (!x_held) map_.write_native_word(previous_ship_key_down_, 0U);
        }
        if (starfox_ex_cartridge_
            && (input.pressed & starfox::input::b) != 0U) {
            // EX's PSTRATS brake branch sets PSF2_BRAKING immediately before
            // testing it, making its following TRIGSE $33 unreachable. Keep
            // the interactive controller demo's promised brake feedback
            // without changing the cartridge's gameplay strategy timing.
            queue_sound_effect(0x33U);
        }
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
    if (pause_after_tick && flow_state_ == GameFlowState::gameplay) {
        paused_ = true;
        if (starfox_ex_cartridge_) {
            // DOPAUSE's quick-flip setup before its first frozen frame.
            map_.write_native_byte(ex_debug_flash_, 5U);
            map_.write_native_byte(ex_debug_alien_, 2U);
            map_.write_native_byte(ex_freeze_strategies_, 1U);
            map_.write_native_byte(ex_menu_selected_, 0U);
        }
    }
    if (ex_pause_transfer) {
        // One stepped/model-refresh DOPAUSE iteration has completed. Stay in
        // the menu and restore its freeze unless that transfer left gameplay.
        paused_ = flow_state_ == GameFlowState::gameplay;
        map_.write_native_byte(
            ex_freeze_strategies_, paused_ ? 1U : 0U);
    }
    result.audio_port_writes = map_.take_apu_port_writes();
    return result;
}

} // namespace starfox::simulation
