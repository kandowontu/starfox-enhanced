#pragma once
#include "starfox/render/packed_faces.hpp"
#include <bit>

namespace starfox::render {
// One record per ray triangle, in exactly the acceleration structure's order.
// Keep palette indices, not baked RGB: palette fades must affect reflections.
struct RayMaterial {
    std::array<float,6> uv{};
    std::uint32_t textured{},dither{};
    std::uint32_t even{},odd{},colour_base{},face{};
    std::uint32_t offset{},u_mask{},v_mask{},reserved{};
};
static_assert(sizeof(RayMaterial)==64);
struct RayMaterials {
    std::vector<RayMaterial> triangles;
    std::vector<std::uint8_t> texels;
};
// Input must be final shaded materials, not unresolved colour-warp templates.
// Topology includes offscreen/back-facing triangles and their original face ID.
// Reject unsupported emitters atomically; never silently reflect the wrong face.
inline bool pack_ray_materials(const PackedFaces& faces,
    std::span<const std::array<std::uint32_t,4>> topology,RayMaterials& output) {
    if(faces.materials.size()!=faces.polygons.size() || topology.size()>4'000'000
        || faces.texels.size()>16'000'000) return false;
    RayMaterials next;
    next.texels=faces.texels;next.triangles.reserve(topology.size());
    for(const auto& triangle:topology) {
        const auto face=triangle[3];
        if(face>=faces.materials.size()) return false;
        const auto& source=faces.materials[face];const auto& polygon=faces.polygons[face];
        if(source.textured>1 || source.even>255 || source.odd>255 || source.colour_base>255) return false;
        RayMaterial material;
        material.face=face;material.textured=source.textured;material.dither=source.dither;
        material.even=source.even;material.odd=source.odd;material.colour_base=source.colour_base;
        if(source.textured) {
            if(source.u_mask>4095 || source.v_mask>4095
                || (source.u_mask&(source.u_mask+1)) || (source.v_mask&(source.v_mask+1))
                || std::uint64_t(source.texture_offset)+std::uint64_t(source.u_mask+1)*(source.v_mask+1)>faces.texels.size()) return false;
            material.offset=source.texture_offset;material.u_mask=source.u_mask;material.v_mask=source.v_mask;
        }
        for(unsigned corner=0;corner<3;++corner) {
            const auto index=triangle[corner];
            if(index>=faces.corners.size() || index<polygon[0]
                || std::uint64_t(index)>=std::uint64_t(polygon[0])+polygon[1]) return false;
            if(source.textured) for(unsigned axis=0;axis<2;++axis) {
                const auto scroll=std::bit_cast<std::int32_t>(axis?source.reserved1:source.reserved0);
                const double uv=double(faces.corners[index][axis+1])+scroll;
                if(uv < -1e8 || uv > 1e8) return false;
                material.uv[corner*2+axis]=float(uv);
            }
        }
        next.triangles.push_back(material);
    }
    output=std::move(next);return true;
}
}
