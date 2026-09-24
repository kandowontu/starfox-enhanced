#pragma once
#include <array>
#include <cmath>
#include <optional>
namespace starfox::render {
inline constexpr std::array cloud_limb_regions{
    std::array<float,4>{80,264,128,312}, std::array<float,4>{160,320,240,352}};
// Each object's center follows cartridge scrolling, without shearing its shape.
inline std::optional<std::array<float,2>> cloud_limb_coordinates(float x,float y,
    const std::array<float,4>& t) {
    const float determinant=1-t[0]*t[1];
    if(std::abs(determinant)<.001f) return std::nullopt;
    for(const auto& r:cloud_limb_regions) {
        const float cx=(r[0]+r[2])*.5f,cy=(r[1]+r[3])*.5f;
        float sx=(cx-t[2]-t[0]*(cy-t[3]))/determinant;
        const float sy=cy-t[3]-t[1]*sx;
        if(t[0]==0 && t[1]==0) {
            const float wrapped=sx-512*std::floor((sx+256)/512);
            if(wrapped>=-128 && wrapped<128) sx=wrapped;
        }
        const float px=cx+x-sx,py=cy+y-sy;
        if(px>=r[0] && px<r[2] && py>=r[1] && py<r[3]) return std::array{px,py};
    }
    if(t[0]==0 && t[1]==0) return std::nullopt;
    // Erase remnants of the original sheared rectangles, not the surrounding stars.
    const float px=x+t[0]*y+t[2],py=y+t[1]*x+t[3];
    for(const auto& r:cloud_limb_regions)
        if(px>=r[0] && px<r[2] && py>=r[1] && py<r[3]) return std::array{0.f,0.f};
    return std::nullopt;
}
}
