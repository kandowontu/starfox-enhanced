#pragma once
#include "starfox/render/object_snapshot.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/grid_line_history.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include <memory>

namespace starfox::vr {
struct GameSceneObject {
    simulation::ObjectHandle handle{};
    simulation::GameObject object;
    render::ObjectPresentationSnapshot presentation;
    // Completed source-tick pose; presentation interpolation/scene-specific
    // overrides must not overwrite its source lighting/depth fields.
    render::RenderPose source_pose;
};

// Owned source-tick data, not references into the mutable object pool. Retain
// one shared snapshot for both eyes and for asynchronous GPU fence retries.
struct GameSceneSnapshot {
    uint64_t revision{};
    timing::TransformSnapshot camera;
    simulation::MatrixQ15 view_matrix{};
    int16_t view_float_y{};
    int16_t background_vertical_scroll{};
    int16_t shadow_height{};
    bool shadows_enabled{};
    simulation::GameFlowState flow{};
    simulation::ObjectHandle player{};
    render::ObjectSnapshotMap transforms;
    std::vector<GameSceneObject> objects; // Native draw-list order, never sorted.
    std::array<simulation::ParticleState,simulation::kMaximumParticles> particles{};
    std::array<uint16_t,256> cgram{};
    // Immutable tile/sprite/HDMA state from this same completed source tick.
    // Shared by both eyes; never reference the live VM's mutable VRAM.
    std::shared_ptr<const simulation::SnesPpuState> ppu;
    std::optional<std::array<int16_t,2>> background_scroll_override;
    uint8_t display_brightness{};
    uint8_t background_colour_subtract{};
    std::array<int16_t,2> source_vanishing_point{};
    simulation::MeterState meters;
    simulation::BriefingState briefing;
    simulation::DialogueState dialogue;
    bool paused{};
    simulation::CircleEffectState circle;
    simulation::WindowWipeState wipe;
    bool native_ex_bitmap{};
    bool ex_title_logo_screen{};
    uint16_t background_unique_top_rows{};
    bool background_ex_twin_planets{};
    bool background_ex_face_planets{};
    bool background_ex_ocean_island{};
    bool background_ex_volcanic_horizon{};
    bool background_ex_city_planets{};
    bool background_landscape{};
    uint16_t background_id{};
    bool background_landscape_unique_half{};
    bool background_landscape_unique_right_half{};
    uint16_t landscape_atlas_origin{232};
    bool background_water_surround{};
    bool background_space_horizon{};
    bool background_unique_space{};
    std::array<unsigned,4> background_planet_rect{};
    bool background_orbital_planet{};
    bool background_orbital_thin{};
    bool background_orbital_entry{};
    bool background_star_sphere{};
    bool background_retain_sky_scroll{};
    int16_t landscape_grid_height{}; // Fixed on outdoor entry; steering must not raise/lower the ground.
    std::array<simulation::DustPoint,simulation::kMaximumDustPoints> dust_points{};
    std::size_t dust_point_count{simulation::kNormalDustPoints};
    int8_t dots_mode{}; // Negative: stars; positive: ground grid; zero: neither.
    bool grid_lines{};
    // Canonical 224x192 Super FX viewport; the source compositor adds its guard.
    std::array<int16_t,2> grid_line_start{};
    std::array<uint16_t,16> model_palette{};
    uint8_t game_frame{},model_scale{};
    std::optional<uint16_t> colour_table_override;
};

inline bool replace_native_dialogue(const GameSceneSnapshot& scene) {
    return scene.dialogue.active && !scene.paused
        && (!scene.meters.extended || scene.flow==simulation::GameFlowState::gameplay
            || scene.flow==simulation::GameFlowState::training
            || scene.flow==simulation::GameFlowState::intro);
}
inline bool same_landscape_mapping(const GameSceneSnapshot& previous,const GameSceneSnapshot& current) {
    return previous.background_landscape && current.background_landscape
        && previous.background_id==current.background_id && previous.flow==current.flow
        && previous.meters.extended==current.meters.extended
        && previous.landscape_atlas_origin==current.landscape_atlas_origin
        && previous.background_landscape_unique_half==current.background_landscape_unique_half
        && previous.background_landscape_unique_right_half==current.background_landscape_unique_right_half;
}

class GameSceneHistory {
public:
    GameSceneHistory(const simulation::GameSimulation&,const assets::RomImage&,
                     const assets::SymbolMap&);
    // Capture once after a completed logic tick, including every catch-up
    // tick. Publication is transactional; retained older snapshots stay valid.
    void capture();
    // Pause/camera-clock rebases must not replay the previous pose on resume.
    void reset_interpolation() noexcept {previous_=current_;}
    [[nodiscard]] const simulation::GameSimulation& game() const {return game_;}
    [[nodiscard]] std::shared_ptr<const GameSceneSnapshot> current() const {return current_;}
    [[nodiscard]] std::shared_ptr<const GameSceneSnapshot> previous() const {return previous_;}
private:
    const simulation::GameSimulation& game_;
    const assets::RomImage& rom_;
    simulation::TrigTables trig_;
    std::array<uint32_t,12> addresses_{};
    std::array<uint32_t,2> tracking_strategies_{};
    std::array<uint32_t,11> model_addresses_{};
    uint32_t depth_tables_{};
    uint16_t ex_title_intro_background_{};
    uint16_t ex_menu_background_{};
    uint32_t ex_menu_background_choice_{};
    uint16_t ex_twin_planet_background_{};
    uint16_t ex_face_planet_background_{};
    uint16_t dimension_background_{};
    uint16_t blackhole_background_{};
    std::array<uint16_t,4> final_vortex_backgrounds_{};
    uint16_t comet_background_{};
    uint16_t ex_intro_star_background_{};
    uint16_t sector_y_star_background_{};
    uint16_t asteroid_star_background_{};
    std::array<uint16_t,8> unique_backgrounds_{};
    std::array<uint16_t,5> orbital_backgrounds_{};
    std::array<uint16_t,4> ex_star_backgrounds_{};
    std::array<uint16_t,25> landscape_backgrounds_{};
    uint16_t water_background_{};
    uint16_t colony_background_{};
    std::array<uint32_t,3> dust_addresses_{};
    std::shared_ptr<const GameSceneSnapshot> previous_,current_;
    render::GridLineHistory grid_line_history_;
};
}
