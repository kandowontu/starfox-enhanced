#pragma once
#include "starfox/vr/game_model_pose.hpp"
#include "starfox/vr/shape_batch.hpp"
#include "starfox/render/source_shading.hpp"
#include <algorithm>

namespace starfox::vr {
struct DrawPacket {
    // Empty/inactive source layers still participate in ordered scene updates.
    // Their default transform must remain a valid affine transform, not a zero
    // matrix rejected by the GPU submission validator.
    Matrix4 model{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    ShapeBatch geometry;
    render::SourceShading shading;
    bool preserve_native_colour{};
};
// Exact comparison of GPU-consumed data (not a collision-prone hash). Model
// matrices and source bookkeeping do not require replacement GPU buffers.
inline bool same_draw_geometry(std::span<const DrawPacket> a,std::span<const DrawPacket> b) {
    if(a.size()!=b.size()) return false;
    for(std::size_t i=0;i<a.size();++i) {
        if(a[i].preserve_native_colour!=b[i].preserve_native_colour) return false;
        const auto& x=a[i].geometry;const auto& y=b[i].geometry;
        if((x.shared_vertices && !x.vertices.empty()) || (y.shared_vertices && !y.vertices.empty())) return false;
        if((x.shared_line_vertices && !x.line_vertices.empty()) || (y.shared_line_vertices && !y.line_vertices.empty())) return false;
        const auto xv=x.vertex_view(),yv=y.vertex_view();
        const bool same_vertices=xv.size()==yv.size() && (xv.data()==yv.data() || std::equal(xv.begin(),xv.end(),yv.begin()));
        const auto xl=x.line_view(),yl=y.line_view();
        const bool same_lines=xl.size()==yl.size() && (xl.data()==yl.data() || std::equal(xl.begin(),xl.end(),yl.begin()));
        if(!x.deferred.empty() || !y.deferred.empty() || !same_vertices
            || !same_lines || !x.same_texels(y)) return false;
    }
    return true;
}
// The caller supplies the selected source LOD and complete source RenderPose.
// Build once, then record the same packet with each eye's camera. No CPU
// projection or eye-dependent colour/visibility is baked into the packet.
bool build_draw_packet(const assets::Shape&,const render::RenderPose&,
    std::span<const render::Rgba8> palette,uint8_t palette_base,uint32_t dither_scale,
    bool srgb_target,float units_per_metre,DrawPacket&,std::string& error);
}
