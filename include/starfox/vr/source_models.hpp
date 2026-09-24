#pragma once
#include "starfox/vr/game_scene.hpp"
#include "starfox/vr/draw_packet.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/source_span_model.hpp"
#include <unordered_map>
#include <memory>

namespace starfox::vr {
inline constexpr uint32_t source_shadow_pass=0x10000U;
inline constexpr bool is_source_shadow_pass(uint32_t key) noexcept {
    return (key&0xffff0000U)==source_shadow_pass;
}
struct SourceModelIssue {simulation::ObjectHandle handle{};std::string reason;};
struct SourceComputeFallback {uint32_t key{};std::string reason;};
struct SourceComputeModel {
    uint32_t key{};
    size_t packet_index{}; // Placeholder in packets/handles, preserving pass order.
    float units{};
    SourceSpanModel model;
    std::shared_ptr<const SourceWarpInputs> warp;
    std::shared_ptr<const SourceAxisInputs> axis;
};
struct SourceModelPackets {
    std::vector<DrawPacket> packets;
    // Low 16 bits identify the source object; upper bits identify extra passes.
    std::vector<uint32_t> handles;
    std::vector<SourceModelIssue> pending;
    // Rendered by the legacy geometry path, not missing from the scene.
    // Keep the producer rejection reason for migration coverage audits.
    std::vector<SourceComputeFallback> compute_fallbacks;
    std::vector<SourceComputeModel> compute_models;
};
// Completed-tick model assembly only. Pending entries must be handled before
// presenting a complete game scene; they must never be silently discarded.
// This does not enumerate or emit background, overlay or extra shadow passes.
class SourceModels {
public:
    SourceModels(const assets::RomImage&,const assets::SymbolMap&,bool cache_geometry=true,bool compute_solids=false,bool compute_shadows=true);
    DrawPacket assemble_dust(const GameSceneSnapshot&,bool srgb_target=false,float units_per_metre=256) const;
    DrawPacket assemble_dust_gpu(const GameSceneSnapshot&,bool srgb_target=false,float units_per_metre=256) const;
    DrawPacket assemble_dust_interpolated(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,
        double alpha,bool srgb_target=false,float units_per_metre=256,bool gpu=false) const;
    DrawPacket assemble_grid(const GameSceneSnapshot&,bool srgb_target=false,float units_per_metre=256) const;
    DrawPacket assemble_grid_gpu(const GameSceneSnapshot&,bool srgb_target=false,float units_per_metre=256) const;
    DrawPacket assemble_connected_grid(const GameSceneSnapshot&,bool srgb_target=false) const;
    DrawPacket assemble_connected_grid_binned(const GameSceneSnapshot&,bool srgb_target=false) const;
    DrawPacket assemble_connected_grid_gpu(const GameSceneSnapshot&,bool srgb_target=false) const;
    DrawPacket assemble_connected_grid_interpolated(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,
        double alpha,bool srgb_target=false) const;
    DrawPacket assemble_grid_interpolated(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,
        double alpha,bool srgb_target=false,float units_per_metre=256,bool gpu=false,bool fixed_landscape_height=false) const;
    SourceModelPackets assemble(const GameSceneSnapshot&,bool srgb_target=false,float units_per_metre=256);
    SourceModelPackets assemble_interpolated(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,
        double alpha,bool srgb_target=false,float units_per_metre=256,bool fixed_landscape_height=false);
    // Same ordered world passes used by the live app and headless preflight:
    // grid, dust, then native model/shadow order. Does not include PPU overlays.
    SourceModelPackets assemble_world_interpolated(const GameSceneSnapshot& previous,
        const GameSceneSnapshot& current,double alpha,bool srgb_target=false,bool surround_stars=false);
private:
    struct GeometryKey {
        const assets::Shape* shape{};
        double scale{};
        uint32_t animation{},colour_frame{};
        int32_t scroll_x{},scroll_y{};
        std::size_t depth{};
        std::array<int8_t,3> light{};
        std::array<uint8_t,32> depth_colours{};
        std::array<uint16_t,256> palette{};
        std::optional<uint8_t> palette_override;
        uint8_t forced_colour{},brightness{};
        bool force_colour{},depth_tables{},srgb{};
        friend bool operator==(const GeometryKey&,const GeometryKey&)=default;
    };
    struct CachedGeometry {GeometryKey key;ShapeBatch geometry;uint64_t used{};};
    std::unordered_map<uint32_t,CachedGeometry> geometry_cache_;
    std::size_t cached_vertices_{},cached_texels_{};
    uint64_t cache_epoch_{};
    bool cache_geometry_{};
    bool compute_solids_{};
    bool compute_shadows_{true};
    DrawPacket connected_grid_pose(const GameSceneSnapshot&,const timing::RenderTransform&,
        const simulation::MatrixQ15&,bool,bool gpu=false) const;
    DrawPacket grid_pose(const GameSceneSnapshot&,const timing::RenderTransform&,
        const simulation::MatrixQ15&,bool,float,bool gpu=false) const;
    DrawPacket dust_pose(const GameSceneSnapshot&,const timing::RenderTransform&,
        const simulation::MatrixQ15&,bool,float,bool gpu=false) const;
    SourceModelPackets assemble_poses(const GameSceneSnapshot&,std::span<const render::RenderPose>,std::span<const render::RenderPose>,bool,float,double);
    SceneInterpolationRules interpolation_;
    const assets::RomImage* rom_{};
    uint32_t scaled_font_{},scaled_messages_{},star_colours_{};
    uint16_t intro_laser_shape_{};
    std::array<uint32_t,4> intro_showcase_strategies_{};
    assets::ShapeDecoder decoder_;
    std::array<uint16_t,3> colours_{};
    std::unordered_map<uint64_t,assets::Shape> shapes_;
    mutable SceneVertex grid_material_{};
    mutable std::shared_ptr<const std::vector<SceneVertex>> grid_vertices_;
    mutable std::vector<simulation::DustPoint> dust_source_points_;
    mutable std::shared_ptr<const std::vector<SceneVertex>> dust_vertices_;
    std::shared_ptr<const std::vector<SceneVertex>> surround_vertices_;
};
}
