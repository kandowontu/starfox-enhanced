#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/input/input_latch.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/math.hpp"
#include "starfox/simulation/dust_system.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/simulation/particle_system.hpp"
#include "starfox/simulation/strategy_scheduler.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <vector>

namespace starfox::simulation {

struct GameTickResult {
    std::size_t prelude_instructions{};
    StrategyTickStats strategies{};
    std::vector<ApuPortWrite> audio_port_writes;
    std::vector<std::uint8_t> sound_effect_commands;
};

enum class GameFlowState {
    pregame_menu,
    title,
    intro,
    controls_type,
    controls_choice,
    training,
    planet_select,
    planet_travel,
    gameplay,
    stage_results,
    game_over,
    continue_choice,
    credits,
    finished,
};

enum class TimingMode {
    unlocked_20_fps,
    original_speed,
};

enum class DisplayMode {
    standard_4_3,
    widescreen_16_9,
    ultrawide_21_9,
    super_ultrawide_32_9,
};

enum class PregamePage {
    main,
    options,
};

struct MeterState {
    std::uint8_t damage{};
    std::uint8_t boost{};
    bool shield_up{};
    bool enabled{};
    std::uint8_t boss_health{};
    std::uint8_t boss_max_health{};
};

struct CircleEffectState {
    bool active{};
    std::int16_t centre_x{};
    std::int16_t centre_y{};
    std::uint16_t radius{};
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t affected_layers{};
};

struct DialogueState {
    bool active{};
    bool text_visible{};
    bool three_lines{};
    std::uint8_t portrait_frame{};
    std::uint32_t text_address{};
};

struct StageResultsState {
    bool active{};
    std::uint8_t percentage{};
    std::uint8_t displayed_percentage{};
    std::uint16_t hit_score{};
    std::uint16_t total_percentage{};
    std::array<std::uint8_t, 3> teammate_health{};
};

struct BriefingState {
    bool active{};
    std::uint8_t visible_message_characters{};
    std::uint8_t visible_planet_characters{};
    std::uint32_t message_address{};
    std::uint32_t planet_name_address{};
};

struct PlanetPresentationState {
    bool isolate_fade{};
    std::uint8_t isolate_amount{};
    std::int16_t isolate_left{};
    std::int16_t isolate_top{};
    std::int16_t isolate_right{};
    std::int16_t isolate_bottom{};
    bool briefing_layers{};
    std::uint8_t portrait_brightness{};
};

// A deterministic 20 Hz game-state shell around the original player/map/
// strategy data. Presentation deliberately lives outside this class.
class GameSimulation {
public:
    GameSimulation(
        const assets::RomImage& rom,
        const assets::SymbolMap& symbols,
        const std::string& initial_map = "LEVEL1_1");

    [[nodiscard]] GameTickResult tick(const input::TickInput& input);
    void present_frame();
    void start_map(const std::string& symbol);
    void synchronize_apu_output_ports(
        const std::array<std::uint8_t, 4>& ports) noexcept {
        map_.set_apu_output_ports(ports);
    }

    [[nodiscard]] ObjectHandle player() const noexcept { return player_; }
    [[nodiscard]] const ObjectPool& objects() const noexcept { return objects_; }
    [[nodiscard]] ObjectPool& objects() noexcept { return objects_; }
    [[nodiscard]] const MapVm& map() const noexcept { return map_; }
    [[nodiscard]] MapVm& map() noexcept { return map_; }
    [[nodiscard]] const ParticleSystem& particles() const noexcept { return particles_; }
    [[nodiscard]] const DustSystem& dust() const noexcept { return dust_; }
    [[nodiscard]] const std::vector<ObjectHandle>& draw_order() const noexcept {
        return draw_order_;
    }
    [[nodiscard]] std::array<std::uint16_t, 16> palette_words() const noexcept;
    [[nodiscard]] GameFlowState flow_state() const noexcept { return flow_state_; }
    [[nodiscard]] TimingMode timing_mode() const noexcept { return timing_mode_; }
    [[nodiscard]] DisplayMode display_mode() const noexcept { return display_mode_; }
    [[nodiscard]] std::uint16_t presentation_fps() const noexcept {
        return presentation_fps_;
    }
    [[nodiscard]] std::uint8_t pregame_selection() const noexcept {
        return pregame_selection_;
    }
    [[nodiscard]] PregamePage pregame_page() const noexcept {
        return pregame_page_;
    }
    [[nodiscard]] bool god_mode() const noexcept { return god_mode_; }
    void set_god_mode(bool enabled) noexcept { god_mode_ = enabled; }
    [[nodiscard]] bool logic_tick_ready() const noexcept;
    [[nodiscard]] double logic_interpolation_alpha(
        double video_phase_fraction = 0.0) const noexcept;
    [[nodiscard]] std::uint64_t scene_revision() const noexcept {
        return scene_revision_;
    }
    [[nodiscard]] bool paused() const noexcept { return paused_; }
    [[nodiscard]] MeterState meter_state() const noexcept;
    [[nodiscard]] CircleEffectState circle_effect_state() const noexcept;
    [[nodiscard]] DialogueState dialogue_state() const noexcept;
    [[nodiscard]] StageResultsState stage_results_state() const noexcept;
    [[nodiscard]] BriefingState briefing_state() const noexcept;
    [[nodiscard]] PlanetPresentationState planet_presentation_state() const noexcept;

private:
    enum class FrontendPhase {
        none,
        pregame_fade_to_intro,
        title_fade_to_controls,
        title_fade_to_intro,
        intro_fade_to_title,
        controls_reveal_hold,
        controls_fade_to_training,
        controls_fade_to_map,
        training_fade_to_controls,
        planet_fade_in,
        planet_route,
        planet_confirm_hold,
        planet_isolate,
        planet_centre,
        planet_zoom,
        planet_briefing,
        planet_fade_to_level,
    };

    [[nodiscard]] std::uint32_t rom_symbol(const std::string& name) const;
    [[nodiscard]] std::uint32_t ram_symbol(const std::string& name) const;
    [[nodiscard]] static std::uint16_t native_pointer(ObjectHandle handle) noexcept;
    [[nodiscard]] ObjectHandle handle_from_native_pointer(std::uint16_t pointer) const noexcept;
    void refresh_player_reference();
    void write_input(const input::TickInput& input);
    void service_transfer_request();
    void calculate_view();
    [[nodiscard]] std::size_t update_view_flags_and_cull();
    void calculate_meters();
    void service_audio_irq(std::vector<std::uint8_t>& commands);
    void configure_route_for_map(const std::string& symbol);
    [[nodiscard]] std::uint32_t resolve_route_stage(std::uint16_t stage);
    void service_level_exit();
    void enter_game_over();
    void enter_pregame_menu();
    void enter_continue_screen();
    void enter_title();
    void enter_intro();
    void update_continue_sprites();
    void enter_credits();
    void continue_current_stage();
    void start_initial_route();
    void enter_planet_map(bool selecting_route, std::uint32_t pending_map = 0U);
    void animate_planet_frame(bool advance_rotation = true);
    void advance_planet_rotation();
    void redraw_planet_route(bool complete_route);
    void set_planet_route_lines(bool visible, bool complete_route);
    void update_planet_ship_sprite();
    void launch_pending_stage();
    [[nodiscard]] std::uint32_t selected_route_stage(std::uint16_t stage);
    [[nodiscard]] GameTickResult tick_planet_map(const input::TickInput& input);
    [[nodiscard]] GameTickResult tick_pregame_menu(const input::TickInput& input);
    [[nodiscard]] GameTickResult tick_continue_screen(const input::TickInput& input);
    [[nodiscard]] GameTickResult tick_stage_results(const input::TickInput& input);
    void finish_stage_results();
    void begin_planet_briefing();
    void begin_planet_selection_sequence();
    void prepare_planet_briefing_graphics();
    void draw_selected_planet(bool centred, bool advance_rotation = true);
    void queue_sound_effect(std::uint8_t command);
    void request_music(std::uint8_t command);
    void set_player_control(bool enabled);
    [[nodiscard]] std::uint8_t required_video_phases() const noexcept;
    void complete_video_phases_for_tick();
    void enter_controls(GameFlowState state, std::uint8_t selection = 0U);
    void enter_training();
    void update_control_screen_sprites();
    void initialize_native_map(std::uint32_t address);
    void apply_god_mode_state();
    void service_god_nuke(const input::TickInput& input,
        const std::vector<ObjectHandle>& nukes_before_strategies);
    void detonate_god_nuke();

    const assets::RomImage* rom_{};
    const assets::SymbolMap* symbols_{};
    ObjectPool objects_;
    MapVm map_;
    NativeStrategyScheduler strategies_;
    TrigTables trigonometry_;
    ParticleSystem particles_;
    DustSystem dust_;
    ObjectHandle player_{};
    std::uint32_t internal_player_pointer_{};
    std::uint32_t controller_high_{};
    std::uint32_t controller_low_{};
    std::uint32_t previous_controller_high_{};
    std::uint32_t previous_controller_low_{};
    std::uint32_t last_controller_high_{};
    std::uint32_t last_controller_low_{};
    std::uint32_t trigger_{};
    std::uint32_t hardware_controller_{};
    std::uint32_t game_palette_{};
    std::uint32_t sound_read_{};
    std::uint32_t sound_write_{};
    std::uint32_t sound_buffer_{};
    std::uint32_t sound_pending_{};
    std::uint32_t pause_sound_{};
    std::uint32_t single_step_{};
    std::uint32_t player_ship_flags_{};
    std::uint32_t player_ship_flags_3_{};
    std::uint32_t special_weapon_count_{};
    std::uint32_t special_weapon_delay_{};
    std::uint32_t boss_flags_{};
    std::uint32_t player_strategy_flags_{};
    std::uint32_t doing_wipe_{};
    std::uint32_t stay_black_{};
    std::uint32_t background_music_count_{};
    std::uint32_t background_music_command_{};
    std::uint32_t background_flags_{};
    std::uint32_t calculate_background_scroll_{};
    std::uint32_t calculate_background_vertical_offsets_{};
    std::uint32_t upload_background_vertical_offsets_{};
    std::uint32_t vertical_offsets_enabled_{};
    std::uint32_t calculate_background_horizontal_offsets_{};
    std::uint32_t upload_background_horizontal_offsets_{};
    std::uint32_t horizontal_offsets_enabled_{};
    std::uint32_t horizontal_offsets_buffer_{};
    std::uint32_t do_sounds_{};
    std::uint32_t set_black_{};
    std::uint32_t update_objects_{};
    std::uint32_t palette_goto_{};
    std::uint32_t fade_palette_{};
    std::uint32_t do_sprites_{};
    std::uint32_t do_circle_explosion_{};
    std::uint32_t friends_messages_{};
    std::uint32_t generate_collision_list_{};
    std::uint32_t resolve_collisions_{};
    std::uint32_t restart_{};
    std::uint32_t remove_dead_{};
    std::uint32_t game_flags_{};
    std::uint32_t particles_enabled_{};
    std::uint32_t do_background_request_{};
    std::uint32_t set_background_info_request_{};
    std::uint32_t level_finished_{};
    std::uint32_t stage_{};
    std::uint32_t routes_{};
    std::uint32_t which_route_{};
    std::uint32_t current_planet_{};
    std::uint32_t current_level_{};
    std::uint32_t new_map_{};
    std::uint32_t pepper_message_{};
    std::uint32_t stage_paths_{};
    std::uint32_t initialize_game_{};
    std::uint32_t initialize_all_{};
    std::uint32_t controls_map_{};
    std::uint32_t training_map_{};
    std::uint32_t initialize_planets_{};
    std::uint32_t setup_planets_{};
    std::uint32_t setup_planet_palette_{};
    std::uint32_t copy_planet_light_{};
    std::uint32_t draw_planet_sprites_{};
    std::uint32_t draw_selected_planet_{};
    std::uint32_t draw_planet_in_centre_{};
    std::uint32_t clear_planet_screen_{};
    std::uint32_t dma_planet_screen_{};
    std::uint32_t switch_planet_buffer_{};
    std::uint32_t draw_route_name_{};
    std::uint32_t draw_planet_lines_{};
    std::uint32_t undraw_planet_lines_{};
    std::uint32_t move_ship_along_path_{};
    std::uint32_t start_planet_positions_{};
    std::uint32_t planet_object_characters_{};
    std::uint32_t ship_position_{};
    std::uint32_t new_ship_position_{};
    std::uint32_t flash_ship_{};
    std::uint32_t ship_angle_{};
    std::uint32_t route_x_{};
    std::uint32_t light_x_{};
    std::uint32_t light_y_{};
    std::uint32_t light_z_{};
    std::uint32_t planet_light_x_{};
    std::uint32_t planet_light_y_{};
    std::uint32_t planet_light_z_{};
    std::uint32_t planet_sprite_palette_{};
    std::uint32_t controls_sprites_{};
    std::uint32_t set_control_type_{};
    std::uint32_t reset_sprites_{};
    std::uint32_t controls_exit_{};
    std::uint32_t control_type_{};
    std::uint32_t default_training_{};
    std::uint32_t lives_{};
    std::uint32_t sprite_position_{};
    std::uint32_t sprite_block_{};
    std::uint32_t object_2_characters_{};
    std::uint32_t object_2_palette_{};
    std::uint32_t vanish_x_{};
    std::uint32_t vanish_y_{};
    std::uint32_t route_change_1_{};
    std::uint32_t route_change_black_hole_1_{};
    std::uint32_t route_change_black_hole_2_{};
    std::uint32_t route_change_black_hole_3_{};
    std::uint32_t game_over_initialize_{};
    std::uint32_t game_over_background_{};
    std::uint32_t title_map_{};
    std::uint32_t intro_map_{};
    std::uint32_t initialize_music_{};
    std::uint32_t intro_music_{};
    std::uint32_t controls_music_{};
    std::uint32_t title_music_{};
    std::uint32_t map_music_{};
    std::uint32_t exit_intro_{};
    std::uint32_t once_wipe_{};
    std::uint32_t set_charmap_fox_{};
    std::uint32_t clear_sprites_{};
    std::uint32_t fox_sprites_{};
    std::uint32_t continue_music_{};
    std::uint32_t foxy_option_{};
    std::uint32_t foxy_frame_{};
    std::uint32_t foxy_foot_{};
    std::uint32_t bg_fox_palette_{};
    std::uint32_t bg_fox_characters_{};
    std::uint32_t bg_fox_tilemap_{};
    std::uint32_t fox_object_characters_{};
    std::uint32_t fox_shape_{};
    std::uint16_t vchr_logical_background_{};
    std::uint16_t vchr_physical_background_{};
    std::uint16_t vsc_base_2_{};
    std::uint16_t vobj_base_{};
    std::uint32_t credits_map_{};
    std::uint32_t previous_view_position_{};
    std::uint32_t view_position_{};
    std::uint32_t view_shake_{};
    std::uint32_t view_float_{};
    std::uint32_t previous_view_z_offset_{};
    std::uint32_t view_type_{};
    std::uint32_t no_x_rotation_{};
    std::uint32_t output_rotation_{};
    std::uint32_t output_distance_{};
    std::uint32_t player_turn_rotation_{};
    std::uint32_t player_roll_{};
    std::uint32_t do_z_rotation_{};
    std::uint32_t view_rotation_{};
    std::uint32_t matrix_{};
    std::uint32_t world_matrix_{};
    std::uint32_t view_to_object_{};
    std::uint32_t view_point_{};
    std::uint16_t view_block_{};
    std::uint32_t secondary_player_fly_mode_{};
    std::uint32_t crosshair_x_{};
    std::uint32_t crosshair_y_{};
    std::uint32_t x_angle_{};
    std::uint32_t y_angle_{};
    std::uint32_t player_collision_box_{};
    std::uint32_t player_left_wing_collision_box_{};
    std::uint32_t player_right_wing_collision_box_{};
    std::uint32_t shield_up_{};
    std::uint32_t boost_count_{};
    std::uint32_t meter_damage_{};
    std::uint32_t meter_boost_{};
    std::uint32_t meter_shield_up_{};
    std::uint32_t meters_enabled_{};
    std::uint32_t boss_health_{};
    std::uint32_t boss_max_health_{};
    std::uint32_t circle_animation_{};
    std::uint32_t circle_object_{};
    std::uint32_t circle_radius_{};
    std::uint32_t circle_source_blue_{};
    std::uint32_t circle_source_green_{};
    std::uint32_t circle_source_red_{};
    std::uint32_t circle_affected_layers_{};
    std::uint32_t circle_centre_x_{};
    std::uint32_t circle_centre_y_{};
    std::uint32_t friends_message_{};
    std::uint32_t message_count_1_{};
    std::uint32_t message_count_2_{};
    std::uint32_t which_friend_{};
    std::uint32_t face_pointer_{};
    std::uint32_t face_data_{};
    std::uint32_t messages_{};
    std::uint32_t player_score_{};
    std::uint32_t special_object_total_{};
    std::uint32_t specials_dead_{};
    std::uint32_t peppy_health_{};
    std::uint32_t falco_health_{};
    std::uint32_t slippy_health_{};
    std::uint32_t percentage_buffer_{};
    std::uint32_t percentage_pointer_{};
    std::uint32_t planet_names_{};
    std::uint32_t dog_characters_{};
    std::uint32_t dog_tilemap_{};
    std::uint32_t planet_sprites_{};
    std::uint32_t planet_positions_{};
    std::uint32_t planet_radius_{};
    std::uint32_t planet_rotation_y_{};
    std::uint32_t planet_rotation_table_{};
    std::uint32_t video_frame_counter_{};
    std::uint32_t previous_video_frame_count_{};
    std::uint32_t strategy_frame_rate_{};
    std::uint32_t frame_count_{};
    std::uint32_t rendered_frame_count_{};
    std::uint32_t measured_frame_rate_{};
    std::uint16_t nuke_shape_{};
    std::uint16_t null_shape_{};
    std::uint32_t nuke_explosion_strategy_{};
    std::array<std::uint16_t, 8> god_nuke_protected_shapes_{};
    std::vector<ObjectHandle> draw_order_;
    std::vector<ObjectHandle> armed_god_nukes_;
    std::uint32_t flow_ticks_{};
    std::uint32_t frontend_frames_{};
    std::uint8_t intro_reveal_frames_{};
    std::uint8_t video_phases_since_tick_{};
    std::uint8_t current_tick_video_phases_{3U};
    std::uint32_t source_update_sequence_{};
    std::array<std::int32_t, 6> planet_spin_remainders_{};
    std::uint8_t planet_route_blink_frames_{};
    std::uint32_t pending_map_{};
    std::uint64_t scene_revision_{};
    GameFlowState flow_state_{GameFlowState::gameplay};
    FrontendPhase frontend_phase_{FrontendPhase::none};
    TimingMode timing_mode_{TimingMode::unlocked_20_fps};
    DisplayMode display_mode_{DisplayMode::standard_4_3};
    std::uint16_t presentation_fps_{60U};
    std::uint8_t pregame_selection_{};
    PregamePage pregame_page_{PregamePage::main};
    bool god_mode_{};
    bool planet_travel_complete_{};
    std::uint8_t stage_percentage_{};
    std::uint8_t displayed_stage_percentage_{};
    std::uint16_t stage_hit_score_{};
    std::uint16_t previous_total_percentage_{};
    std::uint32_t briefing_message_address_{};
    std::uint32_t briefing_planet_address_{};
    std::uint8_t briefing_message_characters_{};
    std::uint8_t briefing_message_character_count_{};
    std::uint8_t briefing_planet_characters_{};
    std::uint8_t briefing_planet_character_count_{};
    std::uint8_t briefing_lead_frames_{};
    std::uint8_t briefing_character_frames_{};
    std::uint16_t briefing_hold_frames_{};
    std::uint8_t planet_zoom_remaining_{};
    std::uint8_t pepper_brightness_{};
    bool planet_zoom_is_sphere_{};
    bool briefing_started_{};
    bool route_display_order_{};
    bool planet_route_lines_visible_{};
    CircleEffectState circle_effect_{};
    std::uint8_t background_music_hold_phases_{};
    std::uint8_t background_music_start_delay_phases_{};
    std::uint8_t background_music_upload_delay_override_{};
    bool background_music_start_pending_{};
    std::uint64_t observed_apu_upload_generation_{};
    bool paused_{};
};

} // namespace starfox::simulation
