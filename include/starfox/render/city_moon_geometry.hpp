#pragma once
#include <array>
#include <optional>
namespace starfox::render {
inline constexpr std::array city_moons{
#define SF_CITY_MOON(x,y,r,p) std::array<float,4>{x,y,r,p},
#include "starfox/render/ex_city_moons.inc"
#undef SF_CITY_MOON
};
// EX menu 9 has six unique disks, with translation-only source scrolling.
// Return packed moon-atlas coordinates, never wrap the object positions.
inline std::optional<std::array<float,2>> city_moon_coordinates(float x,float y,
    const std::array<float,4>& source_scroll) {
    for(const auto& body:city_moons) {
        const float dx=(x+source_scroll[0]-body[0])/body[2];
        const float dy=(y+source_scroll[1]-body[1])/body[2];
        if(dx*dx+dy*dy<=1) return std::array{body[3]*.5f+(dx+1)*.25f,2+(dy+1)*.5f};
    }
    return std::nullopt;
}
}
