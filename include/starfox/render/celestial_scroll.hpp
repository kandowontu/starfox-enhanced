#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <optional>

namespace starfox::render {
struct UniqueSkyObject {
    unsigned artwork;
    float x,y,width,height;
    // Measured disk bounds in the 1254-square generated master, exclusive
    // right/bottom. Register the disk, not the surrounding black margin.
    std::array<float,4> image_bounds;
    unsigned palette_bank;
};
inline std::optional<UniqueSkyObject> ex_menu_sky_object(unsigned choice) {
    switch(choice) {
    // Preview 19 scrolls its atlas down by 200. Keep the enhanced body
    // visible there, without moving the approach/departure planet in 28.
    case 19: return UniqueSkyObject{23,384,280,110,110,{31,29,1217,1210},5};
    case 28: return UniqueSkyObject{23,384,152,110,110,{31,29,1217,1210},5};
    case 20: return UniqueSkyObject{26,104,288,48,48,{45,72,1220,1187},0}; // Blue gas cloud, not a planet.
    case 27: return UniqueSkyObject{24,112.5f,415.5f,95,95,{44,44,1210,1204},1};
    case 31: return UniqueSkyObject{25,124,364.5f,70,71,{53,55,1200,1192},6};
    default: return std::nullopt;
    }
}
inline UniqueSkyObject original_cloud_sky_object() {
    // Original 1-4 shares the EX cloud atlas geometry, but its inner glow is
    // warm amber instead of EX's icy blue.
    return {30,104,288,48,48,{45,72,1220,1187},0};
}
// Smooth the cartridge's quantized affine scroll tables. Coordinates are
// centered screen X and top-origin Y; output is atlas X/Y. No per-pixel map
// or extra GPU upload is needed. Fit completed source frames before temporal
// interpolation, rather than fitting already rounded presentation offsets.
inline std::array<float,4> celestial_scroll(
    std::span<const std::int16_t> rows, std::span<const std::uint16_t> columns,
    float base_x,float base_y,bool horizontal,bool vertical) {
    const auto fit=[](auto values,bool valid_bits,float step,float first,float fallback) {
        double sx=0,sy=0,sxx=0,sxy=0,n=0;
        float previous=0;
        for(std::size_t i=0;i<values.size();++i) {
            const unsigned value=unsigned(values[i]);
            if(valid_bits && !(value&0x4000)) continue;
            float y=float(value&511);
            y=n?previous+std::remainder(y-previous,512.f):std::remainder(y,512.f);
            previous=y;
            const double x=first+step*float(i);
            sx+=x;sy+=y;sxx+=x*x;sxy+=x*y;++n;
        }
        if(!n) return std::array<float,2>{0,std::remainder(fallback,512.f)};
        const double d=n*sxx-sx*sx;
        const double slope=d!=0?(n*sxy-sx*sy)/d:0;
        return std::array<float,2>{float(slope),float((sy-slope*sx)/n)};
    };
    const auto x=fit(horizontal?rows:rows.first(0),false,1,0,base_x);
    const auto y=fit(vertical?columns:columns.first(0),true,8,8,base_y);
    return {x[0],y[0],128+x[1],y[1]+128*y[0]};
}
inline std::array<float,4> interpolate_celestial_scroll(
    const std::array<float,4>& previous,const std::array<float,4>& current,float alpha,
    float body_x,float body_y) {
    std::array<float,4> t{};
    for(unsigned i=0;i<4;++i)
        t[i]=previous[i]+(i<2?current[i]-previous[i]:std::remainder(current[i]-previous[i],512.f))*alpha;
    // Select one atlas occurrence nearest the native viewport. Keep atlas
    // coordinates continuous through 511 -> 0, with no repeated planets.
    t[2]+=512*std::round((body_x-t[2]-t[0]*112)/512);
    t[3]+=512*std::round((body_y-t[3]-112)/512);
    return t;
}
// The cartridge's per-row/column offsets can shear a tile-map planet as the
// ship moves. Follow that map's interpolated centre, but sample the enhanced
// circular artwork without its affine deformation.
inline std::array<float,4> stabilize_celestial_body(
    std::array<float,4> transform,float body_x,float body_y) {
    // Invert source = [1 a; b 1] * screen + translation first.
    // Substituting atlas coordinates for screen coordinates moves the centre
    // when either translation or shear changes (especially on a bank).
    const float determinant=1-transform[0]*transform[1];
    if(std::abs(determinant)>=.001f) {
        const float dx=body_x-transform[2],dy=body_y-transform[3];
        const float screen_x=(dx-transform[0]*dy)/determinant;
        const float screen_y=dy-transform[1]*screen_x;
        transform[2]=body_x-screen_x;
        transform[3]=body_y-screen_y;
    }
    transform[0]=0;
    transform[1]=0;
    return transform;
}
}
