#pragma once
#include <array>
#include <cmath>
#include <optional>
namespace starfox::render {
inline constexpr std::array face_planet_regions{
#define SF_FACE_PLANET_REGION(l,t,r,b) std::array<float,4>{l,t,r,b},
#include "starfox/render/ex_face_planet_regions.inc"
#undef SF_FACE_PLANET_REGION
};
// Follow each source body's affine centre without shearing its round surface.
inline std::optional<std::array<float,2>> face_planet_coordinates(float x,float y,
    const std::array<float,4>& transform) {
    const auto& t=transform;
    const float determinant=1-t[0]*t[1];
    if(std::abs(determinant)<.001f) return std::nullopt;
    for(const auto& r:face_planet_regions) {
        if(r[2]-r[0]!=r[3]-r[1]) continue; // Saucers remain original.
        const float cx=(r[0]+r[2])*.5f,cy=(r[1]+r[3])*.5f;
        const float sx=(cx-t[2]-t[0]*(cy-t[3]))/determinant;
        const float sy=cy-t[3]-t[1]*sx;
        const float dx=x-sx,dy=y-sy,radius=(r[2]-r[0])*.5f;
        if(dx*dx+dy*dy<=radius*radius) return std::array{cx+dx,cy+dy};
    }
    // Clear the old sheared sprite footprint after testing every replacement,
    // so its stretched edges cannot remain beside the new round body.
    const float source_x=x+t[0]*y+t[2],source_y=y+t[1]*x+t[3];
    for(const auto& r:face_planet_regions) {
        if(r[2]-r[0]==r[3]-r[1] && source_x>=r[0] && source_x<r[2]
            && source_y>=r[1] && source_y<r[3]) return std::array{0.f,0.f};
    }
    return std::nullopt;
}
}
