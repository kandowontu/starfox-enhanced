#include "starfox/simulation/game_simulation.hpp"

#include "starfox/assets/decrunch.hpp"
#include "starfox/input/buttons.hpp"

#include <cctype>
#include <bit>
#include <stdexcept>

namespace starfox::simulation {
namespace {

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
      update_objects_(rom_symbol("UPDATE_OBJECTS_L")),
      palette_goto_(rom_symbol("PALGOTO_L")),
      fade_palette_(rom_symbol("FADEPALTO_L")),
      do_sprites_(rom_symbol("DO_SPRITES_L")),
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
      spin_planets_(rom_symbol("SPINPLANETS")),
      draw_planet_sprites_(rom_symbol("DRAWPLANETSPRITES")),
      clear_planet_screen_(rom_symbol("CLEARSCREEN")),
      dma_planet_screen_(rom_symbol("DMA256SCREEN")),
      switch_planet_buffer_(rom_symbol("SWITCHBUFFER_FAST")),
      draw_route_name_(rom_symbol("DRAWROUTENAME")),
      draw_planet_lines_(rom_symbol("DRAWPLANETLINES_L")),
      undraw_planet_lines_(rom_symbol("UNDRAWPLANETLINES_L")),
      move_ship_along_path_(rom_symbol("MOVESHIPALONGPATH")),
      start_planet_positions_(rom_symbol("STARTPLANETPOS")),
      ship_position_(ram_symbol("SHIPXY")),
      new_ship_position_(ram_symbol("NEWSHIPXY")),
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
      view_block_(static_cast<std::uint16_t>(ram_symbol("VIEWBLK"))),
      x_angle_(rom_symbol("XANGLEXY_L")),
      y_angle_(rom_symbol("YANGLEXY_L")),
      player_collision_box_(ram_symbol("PCBOXOBJ_B")),
      shield_up_(ram_symbol("SHIELDUP")),
      boost_count_(ram_symbol("BOOSTCNT")),
      meter_damage_(ram_symbol("M_DAMAGE")),
      meter_boost_(ram_symbol("M_BOOSTANIM")),
      meter_shield_up_(ram_symbol("M_SHIELDUP")),
      meters_enabled_(ram_symbol("M_METERS")),
      boss_health_(ram_symbol("M_BOSSHP")),
      boss_max_health_(ram_symbol("M_BOSSMAXHP")),
      video_frame_counter_(ram_symbol("FRAMEC")),
      previous_video_frame_count_(ram_symbol("FRAMER")),
      strategy_frame_rate_(ram_symbol("FRAMERATE")),
      frame_count_(ram_symbol("FRAMECOUNT")),
      rendered_frame_count_(ram_symbol("FRAMES")),
      measured_frame_rate_(ram_symbol("FRAMESB")) {
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
    map_.call_native_routine(rom_symbol("DO_BGM_INIT"), registers, 20'000'000);

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
    start_map(initial_map);
    configure_route_for_map(initial_map);
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
    std::string initial_upper = initial_map;
    for (auto& character : initial_upper) {
        character = static_cast<char>(std::toupper(
            static_cast<unsigned char>(character)));
    }
    if (initial_upper == "TITLEMAP") {
        map_.write_native_word(meters_enabled_, 0U);
        flow_state_ = GameFlowState::title;
    } else if (initial_upper == "CONTMAP") {
        enter_controls(GameFlowState::controls_type);
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
    if (handle == 0 || handle == player_) return;
    player_ = handle;
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
    const auto music = map_.read_native_byte(background_music_command_);
    const auto music_count = map_.read_native_byte(background_music_count_);
    if (music_count == 0U) {
        map_.write_native_byte(0x002140U, music);
        map_.write_native_byte(background_music_count_, 1U);
    } else if (music_count == 1U) {
        if (map_.read_native_byte(0x002140U) != music) {
            map_.write_native_byte(0x002140U, music);
        } else {
            map_.write_native_byte(0x002140U, 0U);
        }
        map_.write_native_byte(background_music_count_, 2U);
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
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_word(level_finished_, 0U);
    flow_ticks_ = 0U;
    flow_state_ = GameFlowState::title;
}

void GameSimulation::enter_intro() {
    initialize_native_map(intro_map_);
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_byte(exit_intro_, 0U);
    map_.write_native_byte(once_wipe_, 0U);
    map_.write_native_word(level_finished_, 0U);
    flow_ticks_ = 0U;
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
    initialize_native_map(controls_map_);
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
    flow_state_ = state;
    Wdc65816Registers registers;
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
    flow_state_ = GameFlowState::training;
}

void GameSimulation::start_initial_route() {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(initialize_all_, registers, 5'000'000);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(initialize_planets_, registers, 5'000'000, true);
    enter_planet_map(true);
}

std::uint32_t GameSimulation::selected_route_stage(std::uint16_t stage) {
    auto route = map_.read_native_byte(which_route_);
    if (route < 2U) route ^= 1U;
    map_.write_native_byte(which_route_, route);
    const auto map_address = resolve_route_stage(stage);
    if (route < 2U) route ^= 1U;
    map_.write_native_byte(which_route_, route);
    return map_address;
}

void GameSimulation::redraw_planet_route(bool complete_route) {
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(undraw_planet_lines_, registers, 2'000'000, true);
    const auto saved_stage = map_.read_native_word(stage_);
    if (complete_route) map_.write_native_word(stage_, 10U);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(draw_planet_lines_, registers, 5'000'000, true);
    map_.write_native_word(stage_, saved_stage);
    if (complete_route) map_.write_native_word(current_planet_, 0xffffU);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(draw_route_name_, registers, 2'000'000, true);
    map_.upload_oam(sprite_block_, 544U);
}

void GameSimulation::enter_planet_map(
    bool selecting_route, std::uint32_t pending_map) {
    paused_ = false;
    pending_map_ = pending_map;
    planet_travel_complete_ = false;
    map_.write_native_word(meters_enabled_, 0U);
    map_.write_native_word(light_x_, 0U);
    map_.write_native_word(light_y_, 0U);
    map_.write_native_word(light_z_, 150U);
    map_.write_native_word(planet_light_x_, 0U);
    map_.write_native_word(planet_light_y_, 0U);
    map_.write_native_word(planet_light_z_, 150U);
    map_.write_native_word(planet_sprite_palette_, 6U);

    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_routine(setup_planets_, registers, 50'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_routine(
        setup_planet_palette_, registers, 2'000'000, true);
    // SETUP_PLANETS_L deliberately leaves INIDISP forced blank for the
    // original eight-frame fade. The native host presents the completed
    // setup atomically, so expose the same final brightness here.
    map_.set_display_brightness(15U);

    flow_ticks_ = 0U;
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
    if (!selecting_route) {
        const auto planet = map_.read_native_byte(current_planet_);
        const auto start = rom_->read16(start_planet_positions_
            + static_cast<std::uint32_t>(planet) * 2U);
        map_.write_native_word(ship_position_, start);
        map_.write_native_word(new_ship_position_, start);
        map_.write_native_word(route_x_, 0U);
    }
    draw_order_.clear();
    ++scene_revision_;

    // Prime both source VRAM buffers so the first presented frame cannot
    // reveal the cleared alternate page.
    animate_planet_frame();
    animate_planet_frame();
}

void GameSimulation::animate_planet_frame() {
    if (flow_state_ != GameFlowState::planet_select
        && flow_state_ != GameFlowState::planet_travel) return;
    Wdc65816Registers registers;
    registers.status = 0x24U;
    map_.call_native_near_routine(
        clear_planet_screen_, registers, 2'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(copy_planet_light_, registers, 2'000'000, true);
    registers = {};
    registers.status = 0x24U;
    map_.call_native_near_routine(spin_planets_, registers, 2'000'000, true);
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

void GameSimulation::present_frame() {
    animate_planet_frame();
}

void GameSimulation::launch_pending_stage() {
    if (pending_map_ == 0U) {
        pending_map_ = selected_route_stage(map_.read_native_word(stage_));
    }
    const auto map_address = pending_map_;
    pending_map_ = 0U;
    initialize_native_map(map_address);
    flow_ticks_ = 0U;
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
        map_.tick_video_phase();
        service_audio_irq(result.sound_effect_commands);
    }
    ++flow_ticks_;
    if (flow_state_ == GameFlowState::planet_select) {
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
        }
        if ((input.pressed & (starfox::input::a | starfox::input::b
                | starfox::input::start)) != 0U) {
            map_.write_native_word(stage_, 0U);
            pending_map_ = selected_route_stage(0U);
            launch_pending_stage();
        }
    } else if (planet_travel_complete_ || flow_ticks_ >= 120U
               || (flow_ticks_ >= 20U
                   && (input.pressed & (starfox::input::a | starfox::input::b
                       | starfox::input::start)) != 0U)) {
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
    }

    const auto next_map = selected_route_stage(next_stage);
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
    if (flow_state_ == GameFlowState::planet_select
        || flow_state_ == GameFlowState::planet_travel) {
        return tick_planet_map(input);
    }
    if (flow_state_ == GameFlowState::continue_choice) {
        return tick_continue_screen(input);
    }
    constexpr std::uint32_t spc_clocks_per_tick = 1'024'000U / 20U;
    constexpr std::uint8_t video_phases_per_tick = 3U;
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
        map_.tick_video_phase();
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
    result.strategies = strategies_.tick_all();
    refresh_player_reference();

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
        // TITLESEQ_L ignores START until GAMEFRAME reaches 40, then enters
        // CONT.ASM's controller/training selection screen.
        if (flow_ticks_ >= 40U
            && (input.pressed & starfox::input::start) != 0U) {
            map_.write_native_byte(controls_exit_, 0U);
            map_.write_native_byte(default_training_, 0U);
            enter_controls(GameFlowState::controls_type);
        } else if (flow_ticks_ >= 880U) {
            enter_intro();
        }
    } else if (flow_state_ == GameFlowState::intro) {
        ++flow_ticks_;
        if (flow_ticks_ >= 30U
            && (input.pressed != 0U || input.held != 0U
                || map_.read_native_byte(exit_intro_) != 0U)) {
            enter_title();
        }
    } else if (flow_state_ == GameFlowState::controls_type) {
        ++flow_ticks_;
        if ((input.pressed & starfox::input::select) != 0U) {
            map_.write_native_byte(control_type_, static_cast<std::uint8_t>(
                (map_.read_native_byte(control_type_) + 1U) & 3U));
        }
        Wdc65816Registers registers;
        registers.status = 0x24U;
        map_.call_native_near_routine(set_control_type_, registers);
        if (flow_ticks_ >= 16U
            && (input.pressed & starfox::input::start) != 0U) {
            map_.write_native_byte(controls_exit_, 0U);
            flow_ticks_ = 0U;
            flow_state_ = GameFlowState::controls_choice;
        }
        update_control_screen_sprites();
    } else if (flow_state_ == GameFlowState::controls_choice) {
        auto selection = map_.read_native_byte(controls_exit_) != 0U
            ? std::uint8_t{1U} : std::uint8_t{};
        if ((input.pressed & starfox::input::select) != 0U) selection ^= 1U;
        if ((input.pressed & starfox::input::up) != 0U) selection = 0U;
        if ((input.pressed & starfox::input::down) != 0U) selection = 1U;
        map_.write_native_byte(controls_exit_, selection);
        if ((input.pressed & (starfox::input::x | starfox::input::y)) != 0U) {
            flow_ticks_ = 0U;
            flow_state_ = GameFlowState::controls_type;
            update_control_screen_sprites();
        } else if ((input.pressed & (starfox::input::a | starfox::input::b
                       | starfox::input::start)) != 0U) {
            if (selection != 0U) start_initial_route();
            else enter_training();
        } else {
            update_control_screen_sprites();
        }
    } else if (flow_state_ == GameFlowState::training) {
        ++flow_ticks_;
        if (flow_ticks_ >= 20U
            && (input.pressed & starfox::input::start) != 0U) {
            Wdc65816Registers registers;
            registers.status = 0x24U;
            map_.call_native_routine(initialize_all_, registers, 5'000'000);
            map_.write_native_byte(default_training_, 1U);
            enter_controls(GameFlowState::controls_choice, 1U);
        }
    }
    if (pause_after_tick && flow_state_ == GameFlowState::gameplay) paused_ = true;
    result.audio_port_writes = map_.take_apu_port_writes();
    return result;
}

} // namespace starfox::simulation
