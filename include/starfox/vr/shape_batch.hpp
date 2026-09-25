#pragma once
#include "starfox/vr/shape_mesh.hpp"
#include "starfox/vr/scene_material.hpp"
#include "starfox/render/software_renderer.hpp"
#include <memory>
#include <algorithm>
namespace starfox::vr {
struct ShapeDrawRange {
    std::size_t source_face{};
    uint32_t first_vertex{},vertex_count{};
    int8_t visibility_index{};
};
enum class DeferredPrimitive {line,sprite,texture,empty};
struct DeferredFace {std::size_t source_face;DeferredPrimitive reason;int group_visibility{-1};};
enum class SourceNoop {degenerate,untextured_sprite,invalid_visibility};
struct SourceNoopFace {std::size_t source_face;SourceNoop reason;};
struct ShapeBatch {
    // Ordered solid-polygon boundaries for the EX span pipeline. Opt-in so
    // ordinary triangle rendering does not allocate or copy this payload.
    // One range per BSP face occurrence, never per fan triangle. With EX
    // span effects, retained solid faces are NOT also emitted as triangles;
    // the caller must submit these boundaries through the source-span path.
    std::vector<SceneVertex> polygon_vertices;
    std::vector<ShapeDrawRange> polygon_ranges;
    std::vector<SceneVertex> vertices;
    // Optional immutable geometry; never populate both representations.
    std::shared_ptr<const std::vector<SceneVertex>> shared_vertices;
    std::span<const SceneVertex> vertex_view() const noexcept {
        return shared_vertices?std::span<const SceneVertex>(*shared_vertices):std::span<const SceneVertex>(vertices);
    }
    std::vector<SceneVertex> line_vertices;
    std::shared_ptr<const std::vector<SceneVertex>> shared_line_vertices;
    std::span<const SceneVertex> line_view() const noexcept {
        return shared_line_vertices?std::span<const SceneVertex>(*shared_line_vertices):std::span<const SceneVertex>(line_vertices);
    }
    std::vector<uint32_t> texels; // Packed RGBA, transparent source index zero.
    // Immutable artwork can be retained across source frames without copying
    // its pixels. As with vertices, owned and shared storage are exclusive.
    std::shared_ptr<const std::vector<uint32_t>> shared_texels;
    std::span<const uint32_t> texel_view() const noexcept {
        return shared_texels?std::span<const uint32_t>(*shared_texels):std::span<const uint32_t>(texels);
    }
    bool same_texels(const ShapeBatch& other) const noexcept {
        if((shared_texels && !texels.empty()) || (other.shared_texels && !other.texels.empty())) return false;
        const auto a=texel_view(),b=other.texel_view();
        return a.size()==b.size() && (a.data()==b.data() || std::equal(a.begin(),a.end(),b.begin()));
    }
    std::vector<ShapeDrawRange> line_ranges;
    std::vector<ShapeDrawRange> ranges;
    std::vector<DeferredFace> deferred;
    std::vector<SourceNoopFace> source_noops; // Explicitly skipped by the source renderer.
    // Face and BSP group visibility triples are evaluated by the GPU.
    // This is membership, not the cartridge's view-dependent painter order.
};
bool build_shape_batch(const assets::Shape&,const ShapeMesh&,const render::RenderPose&,
    std::size_t depth_band,const std::array<int8_t,3>& light,
    std::span<const render::Rgba8> palette,uint8_t palette_base,uint32_t dither_scale,
    bool srgb_target,ShapeBatch& output,std::string& error,bool retain_polygon_boundaries=false);
}
