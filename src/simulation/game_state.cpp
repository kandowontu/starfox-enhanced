#include "starfox/simulation/game_simulation.hpp"
#include "starfox/render/environment_effects.hpp"
#include "starfox/assets/bps.hpp"
#include "starfox/state/archive.hpp"
#include "starfox/state/container.hpp"
#include <cmath>

namespace starfox::simulation {

template<class Archive, class Self>
void GameSimulation::transfer_host_state(Archive& a, Self& s) {
    a(s.player_, s.second_planet_campaign_active_,
        s.continue_rotation_velocity_x_, s.continue_rotation_velocity_y_,
        s.continue_rotation_velocity_z_, s.continue_rotation_x_,
        s.continue_rotation_y_, s.continue_rotation_z_, s.continue_model_z_,
        s.pace_debt_, s.draw_order_, s.armed_god_nukes_);
    for (auto* r : {&s.ex_menu_registers_, &s.ex_results_registers_, &s.ending_registers_})
        a(r->a, r->x, r->y, r->direct, r->stack, r->data_bank, r->status);
    a(s.flow_ticks_, s.frontend_frames_, s.intro_reveal_frames_,
        s.video_phases_since_tick_, s.current_tick_video_phases_,
        s.planet_rotation_video_phases_, s.source_update_sequence_,
        s.planet_spin_remainders_, s.planet_route_blink_frames_, s.pending_map_,
        s.active_route_stage_, s.scene_revision_, s.flow_state_, s.frontend_phase_,
        s.timing_mode_, s.display_mode_, s.presentation_fps_, s.pregame_selection_,
        s.pregame_page_, s.experience_, s.god_mode_, s.show_fps_,
        s.anti_aliasing_mode_, s.enhanced_graphics_, s.smooth_polys_, s.rtx_lighting_,
        s.two_d_filter_, s.effect_, s.effect_intensity_, s.world_effect_, s.bloom_,
        s.bloom_2d_, s.model_smoothing_, s.language_, s.enhanced_shadows_,
        s.ray_tracing_, s.infinite_bombs_, s.infinite_boost_, s.default_laser_,
        s.selected_level_, s.default_laser_pending_, s.chromatic_aberration_,
        s.hdr_effect_, s.world_effect_intensity_, s.menu_preview_,
        s.preview_requested_, s.preview_start_requested_, s.vsync_, s.renderer_mode_,
        s.msu1_music_, s.msu1_available_, s.rumble_, s.music_volume_, s.sfx_volume_,
        s.on_screen_controls_, s.swap_face_buttons_, s.pregame_horizontal_blocked_,
        s.pregame_confirmation_blocked_, s.crosshair_colour_, s.render_scale_,
        s.planet_travel_complete_, s.planet_arrival_confirmation_required_,
        s.planet_route_destination_, s.stage_percentage_, s.displayed_stage_percentage_,
        s.stage_hit_score_, s.previous_total_percentage_, s.briefing_message_address_,
        s.briefing_planet_address_, s.briefing_message_characters_,
        s.briefing_message_character_count_, s.briefing_planet_characters_,
        s.briefing_planet_character_count_, s.briefing_lead_frames_,
        s.briefing_character_frames_, s.briefing_hold_frames_, s.briefing_voice_frames_,
        s.planet_zoom_remaining_, s.pepper_brightness_, s.planet_zoom_is_sphere_,
        s.briefing_started_, s.route_display_order_, s.planet_route_lines_visible_);
    auto& c = s.circle_effect_;
    a(c.active, c.centre_x, c.centre_y, c.radius, c.red, c.green, c.blue, c.affected_layers);
    auto& m = s.colour_math_effect_;
    a(m.active, m.subtract, m.half, m.affected_layers, m.red, m.green, m.blue);
    a(s.wipe_logic_snapshot_);
    for (auto& input : s.secondary_inputs_) a(input.held, input.pressed, input.released);
    a(s.mouse_input_.delta_x, s.mouse_input_.delta_y, s.mouse_input_.buttons,
        s.mouse_input_.scope_x, s.mouse_input_.scope_y, s.ntt_input_,
        s.background_music_hold_phases_, s.background_music_start_delay_phases_,
        s.background_music_upload_delay_override_, s.background_music_start_pending_,
        s.death_music_cut_latched_, s.deferred_msu_track_, s.deferred_msu_frames_,
        s.deferred_msu_repeat_, s.boss_music_before_death_, s.post_boss_dialogue_active_,
        s.observed_apu_upload_generation_, s.ex_results_task_active_, s.ex_results_recorded_,
        s.pending_end_game_, s.pending_direct_stage_start_, s.pending_level_exit_,
        s.special_exit_white_frames_, s.ending_task_active_, s.ending_final_score_,
        s.credits_complete_, s.paused_);
    // Symbol addresses, static tables, strategy entry lists and callbacks are
    // rebound from the same cartridge by the constructor, never read from disk.
}

std::vector<std::uint8_t> GameSimulation::save_state() const {
    state::Writer a;
    transfer_host_state(a, *this);
    a(objects_.save_state(), map_.save_state(), particles_.save_state(), dust_.save_state());
    a(infinite_lives_,host_god_mode_override_);
    a(manipulation_,manipulation_intensity_);
    a(material_);
    a(environment_);
    a(planet_select_cheat_,planet_cheat_active_);
    return state::pack(0x47414d01U, assets::crc32(rom_->bytes()), a.bytes());
}

std::unique_ptr<GameSimulation> GameSimulation::restored_state(
    std::span<const std::uint8_t> bytes) const {
    state::Reader a{state::unpack(bytes, 0x47414d01U, assets::crc32(rom_->bytes()))};
    auto result = std::make_unique<GameSimulation>(*rom_, *symbols_, "LEVEL1_1");
    transfer_host_state(a, *result);
    std::vector<std::uint8_t> objects, map, particles, dust;
    a(objects, map, particles, dust);
    const bool legacy_cheats_menu=a.empty();
    if(!a.empty()) a(result->infinite_lives_); // Older archives default OFF.
    if(!a.empty()) a(result->host_god_mode_override_);
    if(!a.empty()) a(result->manipulation_,result->manipulation_intensity_);
    else if(render::manipulation(static_cast<render::Effect>(result->effect_))) {
        result->manipulation_=result->effect_;result->effect_=0;
        result->manipulation_intensity_=result->effect_intensity_;
    }
    if(!a.empty()) a(result->material_);
    else if(render::material(static_cast<render::Effect>(result->effect_))) {
        result->material_=result->effect_;result->effect_=0;
    }
    if(!a.empty()) a(result->environment_);
    if(!a.empty()) a(result->planet_select_cheat_,result->planet_cheat_active_);
    a.finish();
    // Before Infinite Lives was added, row five was Back. Preserve the
    // action selected by older archives rather than enabling a new cheat.
    if(legacy_cheats_menu && result->pregame_page_==PregamePage::cheats
        && result->pregame_selection_==5U) result->pregame_selection_=6U;
    result->effect_=render::canonical_effect(result->effect_);
    result->world_effect_=render::canonical_effect(result->world_effect_);
    auto valid_enum = [](auto value, auto last) {
        return static_cast<unsigned>(value) <= static_cast<unsigned>(last);
    };
    if (!valid_enum(result->flow_state_, GameFlowState::finished)
        || !valid_enum(result->frontend_phase_, FrontendPhase::planet_fade_to_level)
        || !valid_enum(result->pregame_page_, PregamePage::cheats)
        || !valid_enum(result->experience_, Experience::starfox_ex)
        || !valid_enum(result->timing_mode_, TimingMode::original_speed)
        || !valid_enum(result->display_mode_, DisplayMode::super_ultrawide_32_9)
        || !valid_enum(result->renderer_mode_, RendererMode::software)
        || !valid_enum(result->render_scale_, RenderScale::scale_4x)
        || !valid_enum(result->crosshair_colour_, CrosshairColour::orange)
        || !valid_enum(result->two_d_filter_, TwoDFilterMode::scalefx)
        || !valid_enum(result->anti_aliasing_mode_, AntiAliasingMode::heavy)
        || !std::isfinite(result->pace_debt_) || std::abs(result->pace_debt_) > 2.0
        || result->pending_map_ > 0xffffffU
        || result->default_laser_ > 2U || result->music_volume_ > 100U
        || result->sfx_volume_ > 100U || result->effect_intensity_ > 100U
        || result->world_effect_intensity_ > 100U || result->language_ >= 6U
        || !render::valid_manipulation(result->manipulation_) || result->manipulation_intensity_>100
        || !render::valid_material(result->material_)
        || !render::valid_environment(result->environment_)
        || !render::selectable_effect(result->effect_, false)
        || !render::selectable_effect(result->world_effect_, true)
        || result->rtx_lighting_ > 3U || result->bloom_ > 3U
        || result->bloom_2d_ > 3U || result->model_smoothing_ > 3U
        || result->chromatic_aberration_ > 3U || result->hdr_effect_ > 3U)
        throw std::runtime_error{"Invalid game host state"};
    // Older archives may have stopped on the removed Enhanced Shadows row.
    if(result->pregame_page_==PregamePage::three_d && result->pregame_selection_==26U)
        result->pregame_selection_=29U;
    // Campaign selection rebinds ROM entry points and writes one RAM flag.
    // Do it before restoring the native image, to preserve every saved byte.
    result->select_planet_campaign(result->second_planet_campaign_active_);
    result->objects_.load_state(objects);
    result->map_.load_state(map);
    result->particles_.load_state(particles);
    result->dust_.load_state(dust);
    const auto capacity = result->objects_.capacity();
    if (result->player_ > capacity
        || std::any_of(result->draw_order_.begin(), result->draw_order_.end(),
            [capacity](auto handle) { return handle > capacity; })
        || std::any_of(result->armed_god_nukes_.begin(), result->armed_god_nukes_.end(),
            [capacity](auto handle) { return handle > capacity; }))
        throw std::runtime_error{"Invalid saved object handle"};
    result->shape_face_counts_ = shape_face_counts_;
    // Output-device selection belongs to the current session, not the saved
    // cartridge timeline. Preserve it without invalidating existing archives.
    result->stereo_output_ = stereo_output_;
    result->dlss_mode_ = dlss_mode_;
    result->fsr1_mode_ = fsr1_mode_;
    result->fsr1_menu_ = fsr1_menu_;
    result->reflective_surfaces_ = reflective_surfaces_;
    result->neural_filter_available_ = neural_filter_available_;
    result->neural_filter_requested_ = neural_filter_requested_;
    return result;
}

void GameSimulation::swap_state(GameSimulation& other) noexcept {
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY != WINAPI_FAMILY_APP)
    static_assert(std::is_nothrow_move_constructible_v<GameSimulation>);
    static_assert(std::is_nothrow_move_assignable_v<GameSimulation>);
#endif
    if (this == &other) return;
    std::swap(*this, other);
    for (auto* game : {this, &other}) {
        game->map_.objects_ = &game->objects_;
        game->strategies_.objects_ = &game->objects_;
        game->strategies_.native_state_ = &game->map_;
    }
}
} // namespace starfox::simulation
