#pragma once
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/backdrop_image.hpp"
#include "starfox/render/celestial_scroll.hpp"
#include "starfox/render/radial_backdrop.hpp"
#include "starfox/render/face_planet_atlas.hpp"
#include "starfox/render/cloud_limb_atlas.hpp"
#include "starfox/render/moon_landscape_atlas.hpp"
#include "starfox/render/row_workers.hpp"
#include "starfox/simulation/snes_ppu.hpp"
#include <array>
#include <cmath>
#include <optional>
#include <string_view>

namespace starfox::render {
// These backgrounds depict open space or a distant planetary limb, not a
// physical floor. A ground-shadow receiver here would float below the ship.
inline constexpr std::array shadowless_space_background_names{
    "BG_1_2","BG_1_4","BG_1_14","BG_1_5","BG_2_2","BG_2_4",
    "BG_2_5","BG_3_2","BG_3_4B","BG_3_4D","BG_3_6","BG_3_7C",
    "BG_5_1I","BG_5_1E","BG_5_3","BG_6_3","BG_6_7A","BG_6_7C","BG_COMET","BG_TRAINING"};
inline constexpr bool shadowless_space_background(std::string_view name,bool ex=false) noexcept {
    // Original training is on Corneria; EX replaces it with a starfield.
    if(name=="BG_TRAINING") return ex;
    for(const auto background:shadowless_space_background_names)
        if(name==background) return true;
    return false;
}
// EX swaps menu graphics under BG_TITLE rather than installing a stage list.
// These origins describe the visible landscape strip in each native atlas.
inline std::optional<unsigned> ex_menu_landscape_origin(unsigned choice) noexcept {
    switch(choice) {
    case 0: case 1: case 3: case 4: case 5: return 320;
    case 7: case 8: case 12: case 22: case 26: case 34: return 240;
    case 9: case 10: case 13: case 14: case 15: case 16: case 32: case 33: return 248;
    case 11: return 312;
    case 20: return 288;
    case 36: return 312; // Lava begins at atlas row 440, not 456.
    default: return std::nullopt;
    }
}
// Actual sky/ground boundary, independent of the conservative palette sampling
// bands above. Measured from the original EX menu BG2 atlases/references.
// The 16px Mode 2 atlases start terrain at 432; these 8px landscapes at 360.
inline std::optional<unsigned> ex_menu_landscape_horizon(unsigned choice) noexcept {
    switch (choice) {
    case 0: case 1: case 3: case 4: case 5: return 432;
    case 36: return 440;
    case 34: return 344; // Venom highway clouds stop above the brown rim/stars.
    case 7: case 8: case 9: case 10: case 12: case 13: case 14: case 15: case 16: case 22: case 26: case 32: case 33: return 360;
    default: return std::nullopt;
    }
}
// Explicit landscape artwork assignments. Space, orbital and unique-object
// backgrounds have separate coverage rules and must not fall through here.
inline std::optional<unsigned> ex_menu_landscape_backdrop(unsigned choice) noexcept {
    switch(choice) {
    case 0: case 16: return 1; // Red dusk mountains.
    case 1: return 15; // Daylight coastal islands, matching BG_6_2 artwork.
    case 3: return 2; // Storm cloud bank.
    case 4: return 11; // Distant pale snowy ranges.
    case 5: return 3; // Rocky desert ranges, distinct from the smooth dunes.
    case 8: return 10; // Smooth orange dunes.
    case 7: return 8; // Cloud plains.
    case 12: case 22: return 9; // Golden storm banks; not Cygard's unique face.
    case 9: return 6; // Distant moonlit skyline.
    case 11: return 18; // Dense nearby city, plain night sky; no moon.
    case 10: return 17; // Low green rocky crags over separate brown ground.
    case 13: case 15: return 0; // Snow-covered ranges.
    case 14: return 12; // Cloud-only planet; never add mountain silhouettes.
    case 32: return 13; // Maritime clouds; retain the separate single moon.
    case 26: return 12; // Titania's pale cloud bank and snow palette.
    case 33: return 14; // Distant coastal islands under a night sky.
    case 36: return 22; // Open space over a brilliant comet; native surface below.
    case 34: return 32; // Spectral gold clouds; lower starfield stays native.
    default: return std::nullopt;
    }
}
inline std::optional<unsigned> gameplay_landscape_backdrop(std::string_view name,bool ex) noexcept {
    if(ex && name=="BG_COMET") return 22;
    if(name=="BG_1_1C" || (!ex && name=="BG_TRAINING")) return 0;
    // These authored cloud/ocean/storm lists appear in both experiences.
    // EX uses the same subjects, so do not silently leave its sky native.
    if(name=="BG_2_3A") return 12;
    if(name=="BG_3_3A") return 13;
    if(name=="BG_1_6A" || name=="BG_3_7A" || (!ex && name=="BG_1_7B")) return 9;
    if(name=="BG_3_5") return 16; // Red cloud band, not Macbeth's mountains.
    if(name=="BG_3_1C") return 1;
    if(ex && name=="BG_7_1") return 8;
    if(ex && name=="BG_7_2") return 10;
    if(ex && name=="BG_6_2") return 15; // Daytime coastal islands and cumulus.
    if(ex && (name=="BG_5_1" || name=="BG_6_1")) return 11;
    if(ex && (name=="BG_5_5" || name=="BG_7_5")) return 9;
    if(ex && (name=="BG_7_3" || name=="BG_7_4")) return 0;
    if(ex && name=="BG_5_4") return 17; // Rocky coast; native twin planets stay visible.
    if(ex && name=="BG_6_4") return 2;  // Night storm; retain the two native moons.
    if(ex && name=="BG_6_5") return 27; // Open ember sky behind native fire sprites.
    if(ex && name=="BG_6_6") return 16; // Red cloud front, not a repeated planet.
    if(ex && name=="BG_6_7B") return 31; // Final boss's single fire ribbon.
    if(name=="BG_2_2" || name=="BG_3_6" || name=="BG_2_5"
        || (ex && name=="BG_6_7C")
        || (!ex && name=="BG_1_5"))
        return 28; // Shared jade horizon; EX 2-2's brown planet stays native.
    return std::nullopt;
}
inline std::optional<unsigned> ex_menu_full_sky_backdrop(unsigned choice) noexcept {
    if(choice==6) return 19; // Central asteroid belt, not a ground horizon.
    if(choice==17) return 20; // Fine distant debris, separate from the large-rock belt.
    if(choice==29) return 21; // Sparse red nebula knots, not Sector K's two-color clouds.
    if(choice==2) return 7; // Sector K nebula.
    if(choice==30) return 16; // Red cloud band: sky above AND below, no ground.
    return std::nullopt;
}
inline std::optional<unsigned> gameplay_landscape_horizon(std::string_view name,bool ex) noexcept {
    if(name=="BG_3_5") return ex?136:392;
    if(ex && name=="BG_6_2") return 352;
    if(!ex && (name=="BG_1_6A" || name=="BG_3_7A" || name=="BG_1_7B")) return 360;
    // These palettes are sampled from origin 248, but their authored ground
    // starts at 360, not 248+128. Keep the artwork off the first ground bands.
    if(ex && (name=="BG_5_5" || name=="BG_7_5")) return 360;
    if(ex && (name=="BG_5_4" || name=="BG_6_4" || name=="BG_6_5"
        || name=="BG_6_6")) return (name=="BG_5_4" || name=="BG_6_4")?352:336;
    if(name=="BG_2_2") return 408;
    if(ex && name=="BG_6_7C") return 132;
    if(ex && name=="BG_6_7B") return 112;
    if(name=="BG_3_6" || name=="BG_2_5"
        || (!ex && name=="BG_1_5")) return 200;
    return std::nullopt;
}
inline std::optional<unsigned> gameplay_landscape_origin(std::string_view name,bool ex) noexcept {
    // Gameplay's fortn/corn atlases begin ground at 352, unlike menu copies
    // at 432. Sample their first ground shades too, not just rows 368 onward.
    if(ex && (name=="BG_6_2" || name=="BG_6_4")) return 224;
    if(ex && name=="BG_6_5") return 248;
    if(ex && (name=="BG_6_7B" || name=="BG_6_7C")) return 0;
    return std::nullopt;
}
// The cloud/fire atlases place sky and terrain in distinct palette banks,
// but distribute small highlights too sparsely for the generic tile-count
// classifier. Explicit ownership also lets their live palette fades tint the
// photograph without treating the fire as ground material.
inline void correct_ex_landscape_palette(std::string_view name,
    std::array<std::uint8_t,256>& regions) noexcept {
    if(name=="BG_6_4") {
        for(unsigned i=65;i<=73;++i) regions[i]=2;
        for(unsigned i=74;i<=79;++i) regions[i]=0; // Moon/rim highlights.
        for(unsigned i=81;i<=95;++i) regions[i]=1;
    } else if(name=="BG_6_5") {
        regions[58]=2;
        for(unsigned i=53;i<=57;++i) regions[i]=0; // Animated fire, never soil.
    } else if(name=="BG_6_6") {
        for(unsigned i=49;i<=63;++i) regions[i]=2;
        for(unsigned i=65;i<=79;++i) regions[i]=1;
    }
}
inline bool fortuna_moon_ink(unsigned index) noexcept {
    // Bank 6's shade 14 is the surrounding sky, not part of the moon.
    return index>=97 && index<110;
}
inline bool ex_city_sky_detail(unsigned index) noexcept {
    // Atlas bank 5: colored stars 82..84, moon shades 86..88.
    // Do not retain its black tile fill (81) around either object.
    return (index>=82 && index<=84) || (index>=86 && index<=88);
}
inline void game_over_backdrop_classes(std::array<std::uint32_t,256>& classes) noexcept {
    // BG_AND shares its star inks with Andross's hands. Preserve every
    // authored mark; replace only transparent backdrop and bank-4 blank ink.
    // The source character has no enclosed holes using blank ink 74.
    classes.fill(7);classes[0]=classes[74]=0;
}
inline void preserve_backdrop_celestial_ink(unsigned artwork,
    std::array<std::uint32_t,256>& classes) noexcept {
    // Apply by atlas/artwork in both gameplay and previews. Retain only the
    // authored celestial ink, never the rectangular surrounding sky fill.
    for(unsigned index=0;index<classes.size();++index)
        if((artwork==13 && fortuna_moon_ink(index))
            || (artwork==6 && ex_city_sky_detail(index))) classes[index]=7;
}
inline unsigned environment_center_scroll_y(const simulation::SnesPpuState& p,unsigned fallback) noexcept {
    if(p.background_mode==2 && p.bg2_vertical_offsets_enabled) {
        // Logical x=128 uses BG3 offset column 15, matching the BG2 renderer.
        const unsigned value=unsigned(p.vram[0x5f5e])|(unsigned(p.vram[0x5f5f])<<8);
        if(value&0x4000) return value&511;
    }
    return fallback&511;
}
inline unsigned environment_landscape_scroll_y(std::string_view name,bool ex,
    const simulation::SnesPpuState& p,unsigned fallback) noexcept {
    // These two EX starfield atlases use BG3's offset words for moving
    // objects, not for their level BG2 planet strip. The PPU's actual BG2
    // scroll is zero in the captured phases while the host camera value is
    // nonzero, so use the source layer register instead of either offset.
    if(name=="BG_3_6" || name=="BG_2_5"
        || (ex && (name=="BG_6_7B" || name=="BG_6_7C"))
        || (!ex && name=="BG_1_5"))
        return unsigned(p.bg2_scroll_y)&511;
    return environment_center_scroll_y(p,fallback);
}
// Ground's world-up vector in camera space. Convert Q15 components before
// division: integer division would flatten every bank smaller than 45 degrees.
inline float environment_horizon_slope(const std::array<std::int16_t,9>& view) noexcept {
    return view[4] ? -float(view[3])/float(view[4]) : 0.f;
}
// Independent from model materials: these decorate authored landscape fills.
// They never replace scene geometry or claim to ray trace reflected objects.
inline constexpr std::array<std::string_view,6> environment_labels{
    "ENHANCED GROUND","GROUND MATERIAL","GROUND MOTION","ENHANCED SKY","SKY STYLE","SKY MOTION"};
inline constexpr std::array<unsigned,6> environment_limits{2,8,4,2,4,3};
inline constexpr std::array<std::string_view,8> ground_material_names{"AUTO","GRASS","DIRT","SAND","SNOW","WATER","MIRROR","GOLD METAL"};
inline constexpr std::array<std::string_view,4> ground_motion_names{"OFF","WIND","RIPPLES","PULSE"};
inline constexpr std::array<std::string_view,4> sky_style_names{"CLOUDS","CIRRUS","AURORA","SUNSET"};
inline constexpr std::array<std::string_view,3> sky_motion_names{"OFF","DRIFT","SWIRL"};
inline bool valid_environment(const std::array<std::uint8_t,6>& values) {
    for(unsigned i=0;i<6;++i) if(values[i]>=environment_limits[i]) return false;
    return true;
}
// Presentation-only clock: unwrap the cartridge counter, then interpolate
// between completed source ticks. Never consume game RNG or wall-clock time.
class EnvironmentClock {
    std::uint64_t ticks_{},previous_{},scene_{};
    std::uint16_t frame_{};
public:
    void restore(std::uint16_t frame,std::uint64_t scene,std::uint64_t ticks) {
        frame_=frame;scene_=scene;ticks_=previous_=ticks;
    }
    void advance(std::uint16_t frame,std::uint64_t scene) {
        const auto delta=std::uint16_t(frame-frame_);
        if(scene!=scene_ || delta>8192) {restore(frame,scene,frame);return;}
        previous_=ticks_;ticks_+=delta;frame_=frame;
    }
    std::uint64_t ticks() const {return ticks_;}
    float seconds(double alpha,bool paused=false) const {
        return float(std::lerp(double(previous_),double(ticks_),paused?1.0:std::clamp(alpha,0.0,1.0))/60.0);
    }
};
struct EnvironmentEffects {
    // palette classifications: 0 unclassified, 1..5 ground material, 6 sky,
    // 7 authored sky detail protected from photographic replacement.
    std::array<std::uint32_t,256> classes{};
    std::array<std::uint32_t,4> modes{}; // ground material override+1, motion, sky style+1, motion
    std::array<float,4> motion{}; // logical horizon, world x/z, seconds
    std::array<float,4> plane{}; // horizon slope, geometry material, sky scroll, sky brightness
    std::array<float,4> backdrop_projection{1/512.f,1/160.f,1.f,0.f};
    // Screen-space ellipses protect unique authored planets from the repeating panorama.
    std::array<std::array<float,4>,2> backdrop_keep{};
    // RGB: signed normalized colour shift; W: uniform brightness multiplier.
    std::array<std::array<float,4>,2> backdrop_palette{{{0,0,0,1},{0,0,0,1}}};
    // Optional authored cloud shade ramp: [0] enables it, [1..15] bright to dark.
    std::array<std::uint32_t,16> backdrop_ramp{};
    const BackdropImage* backdrop{}; // immutable, borrowed throughout presentation
    bool water_reflections{}; // Reflective Surfaces controls even the fallback.
    bool ray_water{}; // Actual ray output is composited separately, never SSR.
    std::array<float,4> scroll_fraction{}; // presentation-only BG2 remainder, logical pixels
    bool active() const {return modes[0] || modes[2] || scroll_fraction[0] || scroll_fraction[1];}
};
inline std::array<std::uint32_t,16> authored_cloud_ramp(std::span<const std::uint16_t> palette,
    unsigned minimum=140,unsigned maximum=245) {
    std::array<std::uint32_t,16> ramp{};
    if(palette.size()<16 || minimum>=maximum || maximum>255) return ramp;
    ramp[0]=1|(minimum<<8)|(maximum<<16);
    for(unsigned i=1;i<16;++i) for(unsigned c=0;c<3;++c) {
        const unsigned v=(palette[i]>>(c*5))&31;
        ramp[i]|=((v<<3)|(v>>2))<<(c*8);
    }
    return ramp;
}
inline std::array<std::uint32_t,16> titania_cloud_ramp(std::span<const std::uint16_t> palette) {
    return authored_cloud_ramp(palette);
}
inline bool venom_lightning_visible(std::span<const std::uint16_t> palette) {
    if(palette.size()<16) return false;
    const auto colour=palette[11];
    return ((colour>>5)&31)>((colour&31)+8) && ((colour>>10)&31)>((colour&31)+8);
}
inline std::array<std::uint32_t,16> venom_cloud_ramp(std::span<const std::uint16_t> palette) {
    auto ramp=authored_cloud_ramp(palette,0,220);
    if(venom_lightning_visible(palette)) {
        // Shade 11 draws discrete lightning strokes, not cloud luminance.
        // Keep those source pixels separately; interpolate the surrounding
        // cloud shades instead of painting cyan contours across the photo.
        ramp[11]=0;
        for(unsigned c=0;c<3;++c)
            ramp[11]|=((((ramp[10]>>(c*8))&255)+((ramp[12]>>(c*8))&255))/2)<<(c*8);
    }
    return ramp;
}
inline std::array<float,3> backdrop_ramp_colour(std::array<float,3> colour,
    const std::array<std::uint32_t,16>& ramp) {
    if(!ramp[0]) return colour;
    if(ramp[0]==2) {
        const float peak=std::max({colour[0],colour[1],colour[2]});
        const float chroma=(peak-std::min({colour[0],colour[1],colour[2]}))/std::max(1.f,peak);
        const float cool=std::clamp(.5f+2.f*(colour[2]-colour[0])/std::max(1.f,peak),0.f,1.f);
        const float shade=7.f*(1.f-std::clamp(peak/180.f,0.f,1.f));
        const unsigned a=std::min(unsigned(shade),6U),b=a+1;
        for(unsigned c=0;c<3;++c) {
            const auto sample=[&](unsigned bank,unsigned n){return n>=7?0.f:float((ramp[1+bank*7+n]>>(c*8))&255);};
            const float warm=std::lerp(sample(0,a),sample(0,b),shade-a);
            const float cold=std::lerp(sample(1,a),sample(1,b),shade-a);
            colour[c]=std::lerp(colour[c],std::lerp(warm,cold,cool),std::clamp(chroma*4.f,0.f,1.f));
        }
        return colour;
    }
    // This cloud-only photograph has a fixed exposure range. Address authored
    // shades rather than fitting RGB: Titania deliberately merges shades 1..8
    // in fog, then separates them during its weather-change palette fade.
    const float light=colour[0]*.299f+colour[1]*.587f+colour[2]*.114f;
    const float maximum=(ramp[0]>>16)&255?float((ramp[0]>>16)&255):245.f;
    const float minimum=(ramp[0]>>16)&255?float((ramp[0]>>8)&255):140.f;
    const float shade=1.f+14.f*(1.f-std::clamp((light-minimum)/std::max(1.f,maximum-minimum),0.f,1.f));
    const unsigned a=unsigned(shade),b=std::min(a+1,15U);
    const float blend=shade-float(a);
    for(unsigned c=0;c<3;++c)
        colour[c]=std::lerp(float((ramp[a]>>(c*8))&255),float((ramp[b]>>(c*8))&255),blend);
    return colour;
}
// Use the scene's upload palette and live CGRAM, never the first presented
// frame. Separate sky/surface banks avoid tinting a planet with sky flashes.
inline void calibrate_backdrop_palette(unsigned artwork,std::span<std::uint16_t> reference) {
    // Distant-snow artwork is authored against 5-1's blue daytime sky.
    // 6-1 reuses that atlas/artwork but uploads a grey starting palette. Using
    // its grey upload as the photographic baseline made orange phases purple.
    // Share the artwork's reference, not a stage's current starting weather.
    if(artwork==11 && reference.size()>10) {
        reference[8]=std::uint16_t(5|(14<<5)|(31<<10));
        reference[9]=std::uint16_t(2|(11<<5)|(31<<10));
        reference[10]=std::uint16_t((9<<5)|(31<<10));
    }
}
inline std::array<std::array<float,4>,2> backdrop_palette_response(
    std::span<const std::uint16_t> reference,std::span<const std::uint16_t> current,
    const std::array<std::uint8_t,256>& regions) {
    std::array<std::array<float,4>,2> response{{{0,0,0,1},{0,0,0,1}}};
    for(unsigned band=0;band<2;++band) {
        std::array<unsigned,3> before{},after{};unsigned count=0;
        for(unsigned i=0;i<std::min(reference.size(),current.size());++i) {
            if(i>=regions.size() || i%16==0 || regions[i]!=(band?1:2)) continue;
            ++count;
            for(unsigned c=0;c<3;++c) {
                before[c]+=(reference[i]>>(c*5))&31;
                after[c]+=(current[i]>>(c*5))&31;
            }
        }
        if(count) {
            // Separate brightness from chroma. Dividing each channel by its
            // own reference made the blue 5-1 sky's tiny red component amplify
            // a modest sunset transition several-fold (and ignored channels
            // initially zero). Apply a signed colour shift after uniform
            // brightness instead: source mean maps exactly to live mean,
            // while photographic detail and uniform fades are preserved.
            const unsigned peak=*std::max_element(before.begin(),before.end());
            if(peak) {
                const float brightness=float(*std::max_element(after.begin(),after.end()))/peak;
                response[band][3]=brightness;
                for(unsigned c=0;c<3;++c)
                    response[band][c]=(float(after[c])-float(before[c])*brightness)/(31.f*count);
            }
        }
    }
    return response;
}
// Classification is performed only for explicitly registered landscape atlases.
// Palette entries shared between sky and terrain are intentionally left alone.
inline std::array<std::uint8_t,256> environment_palette_regions(const simulation::SnesPpuState& p,unsigned origin) {
    std::array<std::uint8_t,256> regions{};
    if((p.background_mode!=1 && p.background_mode!=2) || p.tunnel_scene) return regions;
    const unsigned edge=p.bg2_tile_size_16?16:8;
    const unsigned width=((p.bg2_screen_size&1)?64:32)*edge;
    const unsigned height=((p.bg2_screen_size&2)?64:32)*edge;
    const unsigned pages_wide=(p.bg2_screen_size&1)?2:1;
    const auto word=[&](unsigned a){return unsigned(p.vram[a&65535])|(unsigned(p.vram[(a+1)&65535])<<8);};
    for(unsigned y=origin;y<height;++y) {
        const unsigned region=y<origin+48?2:y>=origin+128?1:0;if(!region) continue;
        std::array<unsigned,256> counts{};
        for(unsigned x=0;x<width;x+=4) {
            const unsigned tx=x/edge,ty=y/edge;
            const unsigned entry=(tx/32+(ty/32)*pages_wide)*1024+(ty%32)*32+tx%32;
            const unsigned tile=word(p.bg2_screen_base*2+entry*2);
            const unsigned px=(tile&0x4000)?edge-1-(x%edge):x%edge,py=(tile&0x8000)?edge-1-(y%edge):y%edge;
            const unsigned character=((tile&1023)+(px/8)+(py/8)*16)&1023;
            const unsigned a=p.bg2_character_base*2+character*32+(py%8)*2;
            const unsigned bits=word(a),high=word(a+16),mask=128>>(px%8);
            const unsigned ink=((bits&mask)?1:0)+((bits&(mask<<8))?2:0)+((high&mask)?4:0)+((high&(mask<<8))?8:0);
            if(ink) ++counts[((tile>>10)&7)*16+ink];
        }
        for(unsigned i=1;i<128;++i) if(counts[i]>=(region==1?width/64:(width*95+399)/400)) regions[i]|=region;
    }
    return regions;
}
inline unsigned automatic_ground_material(std::uint16_t c) {
    const unsigned r=c&31,g=(c>>5)&31,b=(c>>10)&31;
    if(std::min({r,g,b})>17 && std::max({r,g,b})-std::min({r,g,b})<9) return 4;
    // Corneria's brighter grass shades approach cyan: green and blue become
    // equal near the top of the ramp. Keep the whole green-led ramp as grass;
    // water must have blue strictly above green.
    if(g>r && g>=b) return 1;
    if(b>r+3 && b>g) return 5;
    if(r>g && g>b && r>20) return 3;
    return 2;
}
inline unsigned titania_ground_material(std::uint16_t c) {
    // The icy-blue far bands are snow, not water. After the weather changer
    // the same indices become red/brown dirt; classify from the live palette.
    return (c&31)>((c>>10)&31)?2U:4U;
}
inline float environment_noise(float x,float y) {
    const int ix=int(std::floor(x)),iy=int(std::floor(y));
    float fx=x-float(ix),fy=y-float(iy);fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
    const auto hash=[](int a,int b) {
        std::uint32_t n=std::uint32_t(a)*1597334677u ^ std::uint32_t(b)*3812015801u;
        n^=n>>16;n*=2246822519u;n^=n>>13;return float(n&65535u)/65535.f;
    };
    const float a=hash(ix,iy),b=hash(ix+1,iy),c=hash(ix,iy+1),d=hash(ix+1,iy+1);
    return (a+(b-a)*fx)*(1-fy)+(c+(d-c)*fx)*fy;
}
inline std::array<float,2> backdrop_motion(std::array<float,2> uv,unsigned motion,float time) {
    const float sky=std::clamp((.72f-uv[1])/.20f,0.f,1.f);
    if(motion==1) uv[0]+=time*.006f*sky;
    if(motion==2) {const float u=uv[0];uv[0]+=std::sin(uv[1]*6+time*.15f)*.012f*sky;uv[1]+=std::cos(u*9+time*.1f)*.008f*sky;}
    return uv;
}
inline std::array<float,3> backdrop_style(std::array<float,3> c,std::array<float,2> uv,unsigned style,float time) {
    const float sky=std::clamp((.72f-uv[1])/.20f,0.f,1.f);
    if(style==2) {
        const float wisps=std::pow(std::max(0.f,std::sin(uv[1]*95+environment_noise(uv[0]*12,uv[1]*4)*7)),12.f)*sky*.13f;
        for(auto& v:c) v+= (255-v)*wisps;
    }
    if(style==3) {
        const float ribbon=std::pow(std::max(0.f,std::sin(uv[0]*21+std::sin(uv[1]*9+time*.08f)*2)),8.f)*sky;
        c={c[0]*.20f+ribbon*30,c[1]*.28f+ribbon*145,c[2]*.40f+ribbon*90};
    }
    if(style==4) {const float light=c[0]*.3f+c[1]*.59f+c[2]*.11f;c={light*1.12f+12*sky,light*.68f,light*.50f+10*sky};}
    for(auto& v:c) v=std::clamp(v,0.f,255.f);
    return c;
}
inline std::array<float,3> environment_colour(std::array<float,3> c,unsigned kind,
    float x,float y,const EnvironmentEffects& e) {
    const auto& m=e.modes;auto v=e.motion;v[0]+=e.plane[0]*x;
    const float t=v[3];float detail=0;std::array<float,3> tint{1,1,1};
    if(kind<=5 && m[0]) {
        if(m[0]>1) kind=m[0]-1;
        const float distance=std::max(4.f,y-v[0]);
        float u=x*24.f/distance+v[1]*.015625f,z=2048.f/distance+v[2]*.015625f;
        if(m[1]==1) u+=std::sin(z*.17f+t)*.6f;
        if(m[1]==2) z+=std::sin(u*.2f+t*1.5f)*.8f;
        const float a=environment_noise(u*.35f,z*.35f)*2-1;
        const float b=std::sin(u*2.7f+z*1.3f)*std::sin(z*3.1f-u*.9f);
        // Fine detail fades at the horizon instead of aliasing into shimmer.
        const float near=std::clamp(distance/100.f,0.f,1.f);
        if(kind<=4) {
            const float grain=environment_noise(u*2.3f,z*2.3f)*2-1;
            if(kind==1) {detail=a*.08f+grain*.045f*near;tint={.91f,1.06f,.86f};}
            if(kind==2) {detail=a*.065f+grain*.04f*near;tint={1.f,1.f,1.f};}
            if(kind==3) {
                const float patches=std::clamp((environment_noise(u*.065f+17,z*.065f-9)-.38f)*3,0.f,1.f);
                const float warp=environment_noise(u*.18f,z*.18f)*9;
                detail=a*.04f+std::sin(z*.48f+warp)*.045f*patches*near+grain*.012f*near;
                tint={1.12f,1.04f,.89f};
            }
            if(kind==4) {detail=a*.04f+grain*.015f*near;tint={1.03f,1.06f,1.12f};}
        }
        if(kind==5) {
            const float wave=std::sin(z*.42f+std::sin(u*.09f)*2-t*.8f);
            const float glint=std::pow(std::max(0.f,std::sin(u*.035f+wave*.23f)),16.f);
            detail=wave*.10f+b*.025f*near+glint*.30f;tint={.86f,1.06f,1.18f};
        }
        { // Auto and an explicit selection share the same material response.
            const float light=(c[0]*.3f+c[1]*.59f+c[2]*.11f);
            if(kind==1) c={light*.70f,light*1.20f,light*.56f};
            if(kind==2) c={light*1.25f,light*.95f,light*.67f};
            if(kind==3) c={light*1.30f,light*1.15f,light*.75f};
            // Pale cartridge snow is already bright. The old >1.6x lift
            // clipped every channel to white and erased both relief and the
            // icy blue tint in EX's Auto ground.
            if(kind==4) c={light*.88f,light*.93f,light*.98f};
            if(kind==5) c={light*.46f,light*.95f,light*1.50f};
            if(kind==6) c={light*1.15f,light*1.18f,light*1.22f};
            if(kind==7) c={light*1.55f,light*1.15f,light*.45f};
        }
        if(kind==2) {
            // Native palettes can offer only bright rust/orange in the chosen
            // bank. Keep their luminance/fades but bring soil toward muted loam.
            const float light=c[0]*.3f+c[1]*.59f+c[2]*.11f;
            c={c[0]*.2f+light*.864f,c[1]*.2f+light*.768f,c[2]*.2f+light*.64f};
        }
        if(m[1]==3) detail+=std::sin(t*1.5f+z*.1f)*.08f;
        const float fade=std::clamp(distance/32.f,0.f,1.f);
        detail*=fade;
        for(unsigned i=0;i<3;++i) tint[i]=1+(tint[i]-1)*fade;
    } else if(kind==6 && m[2]) {
        float u=x*.018f,w=(y-e.plane[0]*x)*.03f;
        if(m[3]==1) u+=t*.04f;
        if(m[3]==2) {u+=std::sin(w+t*.15f)*.5f;w+=std::cos(u+t*.1f)*.3f;}
        const float cloud=environment_noise(u,w)*.65f+environment_noise(u*2.7f,w*2.7f)*.35f;
        if(m[2]==1) detail=std::max(0.f,cloud-.35f)*.65f;
        if(m[2]==2) detail=std::max(0.f,std::sin(w*2+std::sin(u)*2))*.08f;
        if(m[2]==3) {detail=std::pow(std::max(0.f,std::sin(u*1.7f+std::sin(w*.4f)*2)),6.f)*.20f;tint={.85f,1.12f,1.14f};}
        if(m[2]==4) {detail=cloud*.035f;tint={1.23f,.92f,.85f};}
    } else return c;
    // Multiplicative radiance preserves fades, black and authored brightness.
    for(unsigned i=0;i<3;++i) c[i]=std::clamp(c[i]*(tint[i]+detail),0.f,255.f);
    return c;
}
inline void apply_environment(const EnvironmentEffects& e,const Framebuffer& frame,std::vector<std::uint8_t>& rgba,RowWorkers* workers=nullptr) {
    if(!e.active() || frame.layer_tags().size()!=frame.pixels().size() || rgba.size()!=frame.pixels().size()*4) return;
    const auto width=frame.stored_width();const float scale=float(frame.draw_scale());
    if(e.scroll_fraction[0] || e.scroll_fraction[1]) {
        const auto source=rgba;
        const auto height=frame.stored_height();
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto i=std::size_t(y)*width+x;
            if(frame.layer_tags()[i]!=unsigned(PixelLayer::background)) continue;
            const float sx=std::clamp(float(x)+e.scroll_fraction[0]*scale,0.f,float(width-1));
            const float sy=std::clamp(float(y)+e.scroll_fraction[1]*scale,0.f,float(height-1));
            const unsigned ax=unsigned(sx),ay=unsigned(sy);
            for(unsigned c=0;c<3;++c) {
                float value=0;
                for(unsigned by=0;by<2;++by) for(unsigned bx=0;bx<2;++bx) {
                    const auto n=std::size_t(std::min(ay+by,height-1))*width+std::min(ax+bx,width-1);
                    const auto safe=frame.layer_tags()[n]==unsigned(PixelLayer::background)?n:i;
                    value+=source[safe*4+c]*(bx?sx-ax:1-(sx-ax))*(by?sy-ay:1-(sy-ay));
                }
                rgba[i*4+c]=std::uint8_t(value+.5f);
            }
        }
    }
    const bool water=e.water_reflections && !e.ray_water && (e.modes[0]>=6 || (e.modes[0]==1 && std::find(e.classes.begin(),e.classes.end(),5)!=e.classes.end()));
    const auto reflection=water?rgba:std::vector<std::uint8_t>{};
    auto terrain_shading=e;terrain_shading.modes[0]=1;
    const auto rows = [&](unsigned first, unsigned last) {
    for(std::size_t i=std::size_t(first)*width;i<std::size_t(last)*width;++i) {
        const unsigned source_kind=e.classes[frame.pixels()[i]];
        if(e.backdrop && e.modes[2] && (source_kind!=7 || e.backdrop_projection[3]==8)
            && frame.layer_tags()[i]==unsigned(PixelLayer::background)) {
            const float x=(float(i%width)+.5f)/scale-float(width)/scale*.5f;
            const float y=(float(i/width)+.5f)/scale;
            // BG2's per-column offsets can move the real sky/ground boundary
            // away from a straight horizon when banking. The indexed source
            // pixel decides ownership there; the line is only a fallback for
            // transparent/unclassified margin pixels.
            const bool city_moon=e.backdrop_projection[3]==8
                && city_moon_coordinates(x,y,e.backdrop_keep[1]).has_value();
            const bool landscape=e.backdrop_projection[3]==0 || e.backdrop_projection[3]==6 || e.backdrop_projection[3]==8;
            const bool sky_owned=city_moon || (source_kind!=7 && (!landscape
                || source_kind==6 || (source_kind==0
                    && y<e.motion[0]+e.plane[0]*x)));
            if(sky_owned && ((e.backdrop_projection[3]==6 && source_kind==6) || BackdropImage::covers(x,y,e.motion[0],e.plane[0],
                    e.backdrop_projection[3]==0 && source_kind==6
                        ?std::array<float,4>{e.backdrop_projection[0],e.backdrop_projection[1],
                            e.backdrop_projection[2],1.f}:e.backdrop_projection,
                    e.backdrop_keep))) {
                const auto uv=backdrop_motion(BackdropImage::coordinates(x,y,e.motion[0],e.plane[0],e.plane[2],e.backdrop_projection,e.backdrop_keep),e.modes[3],e.motion[3]);
                auto sky=backdrop_ramp_colour(backdrop_style(e.backdrop->sample_projected(uv[0],uv[1],e.backdrop_projection[3]),uv,e.modes[2],e.modes[3]?e.motion[3]:0.f),e.backdrop_ramp);
                const bool cloud_limb=e.backdrop_projection[3]==9 && uv[1]>=320/512.f;
                if(cloud_limb) sky=BackdropImage::limb_colour(sky,e.backdrop_ramp);
                const bool moon_atlas=e.backdrop_projection[3]>=6 && e.backdrop_projection[3]<=8;
                if(moon_atlas && uv[1]>=2) {
                    if(e.backdrop_projection[3]!=8) sky=BackdropImage::moon_surface(sky,uv,e.backdrop_keep[1]);
                    sky=BackdropImage::moon_colour(sky,uv,e.backdrop_ramp);
                }
                // Mode 0 only replaces sky. Source ownership can extend that
                // sky beyond the fitted horizon when banking; it must not
                // acquire the ground's palette response there.
                const auto response=cloud_limb || (moon_atlas && uv[1]>=2)
                    ?std::array<float,4>{0,0,0,1}:e.backdrop_palette[!landscape
                    && y>=e.motion[0]+e.plane[0]*x?1:0];
                const float opacity=moon_atlas && e.backdrop_projection[3]!=8?BackdropImage::moon_opacity(uv,e.backdrop_keep[1]):1;
                std::array<float,3> behind{};
                if(opacity<1) {
                    const auto background_uv=backdrop_motion(BackdropImage::coordinates(x,y,e.motion[0],e.plane[0],e.plane[2],e.backdrop_projection,{}),e.modes[3],e.motion[3]);
                    behind=backdrop_style(e.backdrop->sample_projected(background_uv[0],background_uv[1],6),background_uv,e.modes[2],e.modes[3]?e.motion[3]:0.f);
                    for(unsigned k=0;k<3;++k) behind[k]=behind[k]*e.backdrop_palette[0][3]+255.f*e.backdrop_palette[0][k];
                }
                for(unsigned k=0;k<3;++k) {
                    float colour=sky[k]*response[3]+255.f*response[k];
                    rgba[i*4+k]=std::uint8_t(std::clamp(std::lerp(behind[k],colour,opacity)*e.plane[3],0.f,255.f)+.5f);
                }
                continue;
            }
        }
        const bool terrain=frame.layer_tags()[i]==unsigned(PixelLayer::terrain_geometry);
        if(!terrain && frame.layer_tags()[i]!=unsigned(PixelLayer::background)) continue;
        const unsigned kind=terrain?unsigned(e.plane[1]):e.classes[frame.pixels()[i]];if(!kind) continue;
        const auto& shading=terrain?terrain_shading:e;
        auto base=std::array{float(rgba[i*4]),float(rgba[i*4+1]),float(rgba[i*4+2])};
        if(terrain) {
            // Geometry already has its selected material colour.
            if(kind==4) for(auto& channel:base) channel*=.84f; // leave room for snow highlights
        }
        const auto c=environment_colour(base,kind,
            (float(i%width)+.5f)/scale-float(width)/scale*.5f,(float(i/width)+.5f)/scale,shading);
        for(unsigned k=0;k<3;++k) rgba[i*4+k]=std::uint8_t(c[k]+.5f);
        const float y=(float(i/width)+.5f)/scale;
        const float x=(float(i%width)+.5f)/scale-float(width)/scale*.5f;
        const float d=y-e.motion[0]-e.plane[0]*x;
        if(water && kind<=5 && (e.modes[0]>=6 || kind==5) && d>0) {
            const float offset=(e.modes[0]>=7?2.f:1.55f)*d/(1+e.plane[0]*e.plane[0]);
            const float sx=std::clamp(float(i%width)+offset*e.plane[0]*scale+(e.modes[0]>=7?0:std::sin(y*.12f+e.motion[3])*(2*scale)),0.f,float(width-1));
            const float sy=std::clamp((y-offset)*scale-.5f,0.f,float(frame.stored_height()-1));
            const unsigned x0=unsigned(sx),y0=unsigned(sy);const float fx=sx-x0,fy=sy-y0;
            std::array<float,3> reflected{};float weight=0;
            for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) {
                const auto sample=std::size_t(std::min(y0+dy,frame.stored_height()-1))*width+std::min(x0+dx,width-1);
                if(frame.layer_tags()[sample]==unsigned(PixelLayer::two_d)) continue;
                const float w=(dx?fx:1-fx)*(dy?fy:1-fy);weight+=w;
                for(unsigned k=0;k<3;++k) reflected[k]+=float(reflection[sample*4+k])*w;
            }
            if(weight>0) {
                const float amount=e.modes[0]>=7?.8f:.15f+.20f*std::clamp(1-d/200.f,0.f,1.f);
                constexpr float gold[3]{1,.875f,.58f};
                for(unsigned k=0;k<3;++k) rgba[i*4+k]=std::uint8_t(c[k]*(1-amount)+reflected[k]/weight*(e.modes[0]==8?gold[k]:1)*amount+.5f);
            }
        }
    }
    };
    // Reuse the presentation pool; no per-frame thread creation. Reflection
    // reads the immutable pre-pass snapshot, so row partitions cannot race.
    if(workers && frame.pixels().size()>=32768)
        workers->parallel_rows(frame.stored_height(), rows);
    else rows(0,frame.stored_height());
}
}
