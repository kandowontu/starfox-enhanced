#pragma once
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/effect_types.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string_view>

namespace starfox::render {

// Apply presentation styles to world pixels, keeping the HUD and menus intact.
inline void apply_effect(Effect model_effect, const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba, std::vector<std::uint8_t>& scratch,
    std::uint8_t model_intensity = 100U, Effect world_effect = Effect::off,
    std::uint8_t world_intensity = 100U) {
    model_intensity = std::min<std::uint8_t>(model_intensity, 100U);
    world_intensity = std::min<std::uint8_t>(world_intensity, 100U);
    if (((model_effect == Effect::off || model_intensity == 0U)
            && (world_effect == Effect::off || world_intensity == 0U)) || !frame.layer_tags_enabled()
        || rgba.size() != frame.pixels().size() * 4U) return;
    scratch = rgba;
    const auto width = frame.stored_width();
    const auto height = frame.stored_height();
    const auto step = frame.draw_scale();
    const auto luma = [&](std::size_t i) {
        return (int(scratch[i * 4]) * 77 + int(scratch[i * 4 + 1]) * 150
            + int(scratch[i * 4 + 2]) * 29) / 256;
    };
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto i = std::size_t(y) * width + x;
            if (frame.layer_tags()[i] == std::uint8_t(PixelLayer::two_d)) continue;
            const auto is_world = [](std::uint8_t tag) {
                return tag == std::uint8_t(PixelLayer::background)
                    || tag == std::uint8_t(PixelLayer::world_geometry)
                    || tag == std::uint8_t(PixelLayer::terrain_geometry);
            };
            const auto background = is_world(frame.layer_tags()[i]);
            const auto effect = background ? world_effect : model_effect;
            const auto intensity = background ? world_intensity : model_intensity;
            // These materials are shaded by actual scene rays, never faked by
            // the colour-only fallback when ray tracing is unavailable/off.
            if (effect == Effect::off || effect==Effect::crosshatch || reflective_material(effect) || intensity == 0U) continue;
            if(spatial_manipulation(effect)) {
                const auto sample=[&](int sx,int sy,unsigned channel) {
                    sx=std::clamp(sx,0,int(width)-1);sy=std::clamp(sy,0,int(height)-1);
                    const auto n=std::size_t(sy)*width+sx;
                    if(frame.layer_tags()[n]==std::uint8_t(PixelLayer::two_d)
                        || is_world(frame.layer_tags()[n])!=background) return scratch[i*4+channel];
                    return scratch[n*4+channel];
                };
                std::array<unsigned,8> order{0,1,2,3,4,5,6,7};
                const auto block=int(x/(step*8)*step*8);
                if(effect==Effect::pixel_sort) {
                    const auto brightness=[&](unsigned n) {
                        return (77*sample(block+int(n*step),y,0)+150*sample(block+int(n*step),y,1)
                            +29*sample(block+int(n*step),y,2))/256;
                    };
                    for(unsigned a=1;a<8;++a) {
                        const auto key=order[a];
                        const auto light=brightness(key);
                        auto b=a;
                        while(b>0 && brightness(order[b-1])>light) {order[b]=order[b-1];--b;}
                        order[b]=key;
                    }
                }
                for(unsigned channel=0;channel<3;++channel) {
                    int sx=x,sy=y;
                    if(effect==Effect::kaleidoscope) {
                        sx=std::abs(int(x)*2-int(width)+1);sy=std::abs(int(y)*2-int(height)+1);
                    } else if(effect==Effect::prism_split) sx+=(int(channel)-1)*int(step)*6;
                    else if(effect==Effect::pixel_sort) sx=block+int(order[(x/step)%8]*step)+int(x%step);
                    else if(effect==Effect::shatter) {
                        const int size=int(step)*16,tx=int(x)/size,ty=int(y)/size;
                        const int lx=int(x)%size,ly=int(y)%size;
                        switch((tx*3+ty*5)%4) {
                        case 0: sx=tx*size+ly;sy=ty*size+size-1-lx;break;
                        case 1: sx=tx*size+size-1-lx;sy=ty*size+size-1-ly;break;
                        case 2: sx=tx*size+size-1-ly;sy=ty*size+lx;break;
                        default: break;
                        }
                    } else if(effect==Effect::melt) {
                        const unsigned column=x/(step*3);
                        const int drip=int(((column*13)^(column>>1))%32)*int(step);
                        sy-=drip*int(y)/std::max(1,int(height)-1);
                    } else if(effect==Effect::ripple_warp) {
                        const auto wave=[](int phase) {
                            const int triangle=16-std::abs(phase%64-32);
                            return triangle*(32-std::abs(triangle))/16;
                        };
                        sx+=wave(int(y/step))*int(step);
                        sy+=wave(int(x/step))*int(step)/4;
                    } else if(effect==Effect::barrel_warp) {
                        const int dx=int(x)-int(width)/2,dy=int(y)-int(height)/2;
                        const int nx=dx*128/std::max(1,int(width)),ny=dy*128/std::max(1,int(height));
                        const int lens=192+(nx*nx+ny*ny)/64;
                        sx=int(width)/2+dx*lens/256;sy=int(height)/2+dy*lens/256;
                    } else if(effect==Effect::venetian) {
                        const int size=int(step)*12;
                        sy=int(y)/size*size+(int(y)%size)/2+size/4;
                        sx+=(int(y)/size%2?1:-1)*int(step)*6;
                    } else if(effect==Effect::checker_fold) {
                        const int size=int(step)*24,tx=int(x)/size,ty=int(y)/size;
                        if((tx+ty)%2) sx=tx*size+size-1-int(x)%size;
                        else sy=ty*size+size-1-int(y)%size;
                    } else if(effect==Effect::twist) {
                        const int dx=int(x)-int(width)/2,dy=int(y)-int(height)/2;
                        const int turn=std::clamp(96-(std::abs(dx)+std::abs(dy))/int(step),0,96);
                        sx-=dy*turn/128;sy+=dx*turn/128;
                    } else if(effect==Effect::ring_ripple) {
                        const int dx=int(x)-int(width)/2,dy=int(y)-int(height)/2;
                        const int radius=std::max(std::abs(dx),std::abs(dy))+std::min(std::abs(dx),std::abs(dy))*3/8;
                        const int wave=16-std::abs((radius/int(step))%64-32);
                        sx+=dx*wave*int(step)/std::max(int(step),radius);
                        sy+=dy*wave*int(step)/std::max(int(step),radius);
                    } else if(effect==Effect::shard_split) {
                        const int size=int(step)*32;
                        const int side=(int(x)%size+int(y)%size<size)?1:-1;
                        sx+=side*int(step)*12;sy-=side*int(step)*8;
                    }
                    rgba[i*4+channel]=std::uint8_t((scratch[i*4+channel]*(100-intensity)
                        +sample(sx,sy,channel)*intensity+50)/100);
                }
                continue;
            }
            const auto light = luma(i);
            bool edge = false;
            if (effect == Effect::ink || effect == Effect::neon
                || effect == Effect::blueprint || effect == Effect::vaporwave || effect==Effect::chalk
                || effect==Effect::stained_glass || effect==Effect::hologram || effect==Effect::pencil
                || decorative_material(effect)) {
                const auto right = std::size_t(y) * width + std::min(x + step, width - 1);
                const auto below = std::size_t(std::min(y + step, height - 1)) * width + x;
                edge = std::abs(light - luma(right)) > 28 || std::abs(light - luma(below)) > 28;
            }
            if(effect==Effect::cel_drawn || effect==Effect::comic) {
                edge=false;
                for(const auto offset:{std::array<int,2>{-1,0},{1,0},{0,-1},{0,1}}) {
                    const auto nx=std::clamp(int(x)+offset[0]*int(step),0,int(width)-1);
                    const auto ny=std::clamp(int(y)+offset[1]*int(step),0,int(height)-1);
                    const auto n=std::size_t(ny)*width+nx;
                    if(frame.layer_tags()[n]==std::uint8_t(PixelLayer::two_d)) continue;
                    // Draw inside the brighter side of a boundary, avoiding
                    // doubled dark borders and outlines around HUD lettering.
                    edge |= light-luma(n)>40;
                }
            }
            // A bounded nine-tap bright pass in source-pixel units. Never
            // source light from HUD pixels, nor paint glow over the HUD.
            std::array<int, 3> glow{};
            std::array<int,3> wash{};
            if(effect==Effect::oil_paint || effect==Effect::mosaic) {
                std::array<int,4> counts{};
                std::array<std::array<int,3>,4> sums{};
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                    const int bx=effect==Effect::mosaic?int(x/(step*3)*(step*3)+step):int(x);
                    const int by=effect==Effect::mosaic?int(y/(step*3)*(step*3)+step):int(y);
                    const auto nx=std::clamp(bx+dx*int(step),0,int(width)-1);
                    const auto ny=std::clamp(by+dy*int(step),0,int(height)-1);
                    const auto n=std::size_t(ny)*width+nx;
                    if(frame.layer_tags()[n]==std::uint8_t(PixelLayer::two_d)
                        || is_world(frame.layer_tags()[n])!=background) continue;
                    const int bin=effect==Effect::mosaic?0:luma(n)/64;
                    ++counts[bin];for(unsigned c=0;c<3;++c) sums[bin][c]+=scratch[n*4+c];
                }
                int best=0;for(int b=1;b<4;++b) if(counts[b]>counts[best]) best=b;
                for(unsigned c=0;c<3;++c) wash[c]=counts[best]?sums[best][c]/counts[best]:scratch[i*4+c];
            }
            if(effect==Effect::watercolour) {
                int weight=0;
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                    const auto nx=std::clamp(int(x)+dx*int(step),0,int(width)-1);
                    const auto ny=std::clamp(int(y)+dy*int(step),0,int(height)-1);
                    const auto n=std::size_t(ny)*width+nx;
                    if(frame.layer_tags()[n]==std::uint8_t(PixelLayer::two_d)
                        || is_world(frame.layer_tags()[n])!=background || std::abs(light-luma(n))>32) continue;
                    const int w=dx==0 && dy==0?4:1;weight+=w;
                    for(unsigned c=0;c<3;++c) wash[c]+=scratch[n*4+c]*w;
                }
                for(auto& c:wash) c/=std::max(1,weight);
            }
            if (effect == Effect::bloom) {
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                    const auto nx = std::clamp(int(x) + dx * int(step), 0, int(width) - 1);
                    const auto ny = std::clamp(int(y) + dy * int(step), 0, int(height) - 1);
                    const auto n = std::size_t(ny) * width + nx;
                    if (frame.layer_tags()[n] == std::uint8_t(PixelLayer::two_d)
                        || is_world(frame.layer_tags()[n]) != background) continue;
                    const auto peak = std::max({scratch[n*4], scratch[n*4+1], scratch[n*4+2]});
                    if (peak <= 160) continue;
                    for (unsigned c = 0; c < 3; ++c) glow[c] += int(scratch[n*4+c]) * (peak - 160) / 95;
                }
            }
            for (unsigned c = 0; c < 3; ++c) {
                auto value = int(scratch[i * 4 + c]);
                switch (effect) {
                case Effect::ruby: case Effect::jade: case Effect::porcelain: {
                    const int shine=std::max(0,light-128),specular=shine*shine/64;
                    if(effect==Effect::ruby) value=std::array<int,3>{40,3,12}[c]+light*std::array<int,3>{180,12,42}[c]/255
                        +specular*std::array<int,3>{255,115,145}[c]/255+(edge?std::array<int,3>{50,15,22}[c]:0);
                    else if(effect==Effect::jade) value=std::array<int,3>{6,30,20}[c]+light*std::array<int,3>{65,145,90}[c]/255
                        +specular*std::array<int,3>{120,230,170}[c]/255+(edge?std::array<int,3>{20,35,25}[c]:0);
                    else value=25+light*std::array<int,3>{200,190,166}[c]/255+specular/3+(edge?18:0);
                    value=std::min(255,value);break;
                }
                // Palette-light-driven facets remain attached to the model,
                // rather than sliding a screen-space rainbow over it.
                case Effect::prism: case Effect::glass: case Effect::obsidian: case Effect::pearl: {
                    const int phase=(light*2+int(c)*128)%384;
                    const int spectrum=std::clamp(255-std::abs(phase-192)*4,0,255);
                    const int shine=std::max(0,light-128);
                    const int specular=shine*shine/64;
                    if(effect==Effect::prism) value=std::min(255,24+spectrum*3/4+specular/2+(edge?48:0));
                    else if(effect==Effect::glass) value=std::min(255,value/8+light/4+std::array<int,3>{30,65,85}[c]+specular/2+(edge?75:0));
                    else if(effect==Effect::obsidian) value=std::min(255,std::array<int,3>{9,7,18}[c]+light/16+specular*3/4+(edge?36:0));
                    else value=std::min(255,125+light/4+spectrum/5+specular/4+(edge?18:0));
                    break;
                }
                case Effect::cel_drawn: {
                    const int peak=std::max({int(scratch[i*4]),int(scratch[i*4+1]),int(scratch[i*4+2]),1});
                    const int band=std::min(255,((peak+25)/51)*51);
                    value=edge?value/4:(value*band+peak/2)/peak;break;
                }
                case Effect::ink: value = edge ? 16 : (light < 30 ? 24 : 235); break;
                case Effect::neon: value = edge ? (c == 0 ? 35 : 255) : value / 5; break;
                case Effect::monochrome: value = light; break;
                case Effect::dithered: {
                    constexpr std::array<int, 16> bayer{0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
                    const auto threshold = bayer[((y / step) % 4) * 4 + (x / step) % 4];
                    value = std::clamp((value * 4 + threshold * 16) / 256, 0, 4) * 255 / 4;
                    break;
                }
                case Effect::blueprint: {
                    constexpr std::array<int, 3> paper{8, 24, 58};
                    constexpr std::array<int, 3> line{130, 220, 255};
                    const auto grid = (x / step) % 16 == 0 || (y / step) % 16 == 0;
                    value = edge ? line[c] : paper[c] + (grid ? 12 : 0);
                    break;
                }
                case Effect::bloom: value = std::min(255, value + glow[c] / 12); break;
                case Effect::sepia: {
                    constexpr std::array<std::array<int, 3>, 3> weights{{
                        {101, 197, 48}, {89, 176, 43}, {70, 137, 34}}};
                    value = std::min(255, (scratch[i*4] * weights[c][0]
                        + scratch[i*4+1] * weights[c][1] + scratch[i*4+2] * weights[c][2]) / 256);
                    break;
                }
                case Effect::thermal: {
                    constexpr std::array<std::array<int, 3>, 5> palette{{
                        {8, 5, 40}, {65, 20, 150}, {220, 30, 70}, {255, 150, 15}, {255, 255, 210}}};
                    const auto band = std::min(light / 64, 3);
                    const auto fraction = light - band * 64;
                    value = (palette[band][c] * (64 - fraction)
                        + palette[band+1][c] * fraction) / 64;
                    break;
                }
                case Effect::night_vision:
                    value = c == 1 ? std::min(255, 24 + light * 6 / 5) : light / (c == 0 ? 7 : 4);
                    if ((y / step) % 2 != 0) value = value * 4 / 5;
                    break;
                case Effect::pastel: value = 96 + value * 5 / 8; break;
                case Effect::comic: {
                    const int peak=std::max({int(scratch[i*4]),int(scratch[i*4+1]),int(scratch[i*4+2]),1});
                    const int band=std::min(255,((peak+31)/64)*64);
                    value=edge?value/5:(value*band+peak/2)/peak;
                    break;
                }
                case Effect::vaporwave: {
                    constexpr std::array<int, 3> shadow{55, 8, 100}, highlight{70, 250, 245};
                    value = (shadow[c] * (255 - light) + highlight[c] * light) / 255;
                    if (edge) value = c == 1 ? 75 : 255;
                    break;
                }
                case Effect::posterized: value=std::min(255,((value+25)/51)*51);break;
                case Effect::ice: {
                    constexpr std::array<int,3> low{8,24,65},high{224,250,246};
                    value=(low[c]*(255-light)+high[c]*light)/255;break;
                }
                case Effect::film: {
                    constexpr std::array<int,3> tint{106,100,87},lift{9,4,6};
                    value=std::min(255,(value*value*(765-2*value)/65025)*tint[c]/100+lift[c]);break;
                }
                case Effect::negative: value=255-value;break;
                case Effect::solarized: value=value<128?value*2:(255-value)*2;break;
                case Effect::amber: value=light*std::array<int,3>{255,176,32}[c]/255;break;
                case Effect::emerald: value=light*std::array<int,3>{55,255,130}[c]/255;break;
                case Effect::cyanotype: {
                    constexpr std::array<int,3> low{8,22,70},high{224,246,240};
                    value=(low[c]*(255-light)+high[c]*light)/255;break;
                }
                case Effect::copper: {
                    constexpr std::array<int,3> low{30,10,8},high{255,190,120};
                    value=(low[c]*(255-light)+high[c]*light)/255;break;
                }
                case Effect::lavender: {
                    constexpr std::array<int,3> low{35,12,65},high{250,215,255};
                    value=(low[c]*(255-light)+high[c]*light)/255;break;
                }
                case Effect::cga: {
                    constexpr std::array<std::array<int,3>,4> colours{{{0,0,0},{0,170,170},{170,0,170},{255,255,255}}};
                    value=colours[std::min(3,light/64)][c];break;
                }
                case Effect::scanlines: if((y/step)%2) value=value*3/5;break;
                case Effect::watercolour: value=24+std::min(255,((wash[c]+15)/32)*32)*7/8;break;
                case Effect::chalk: value=edge?235:12+light/10;break;
                case Effect::emboss: {
                    const auto n=std::size_t(y)*width+(x>=step?x-step:0);
                    const int neighbour=frame.layer_tags()[n]==std::uint8_t(PixelLayer::two_d)?light:luma(n);
                    value=std::clamp(128+2*(light-neighbour),0,255);break;
                }
                case Effect::bleach_bypass: {
                    // Silver-retention look: luminance overlay, strong partial
                    // desaturation and crushed blacks, not a subtle colour tint.
                    const int overlay=light<128?2*value*light/255:255-2*(255-value)*(255-light)/255;
                    const int silver=(overlay+3*light)/4;
                    value=std::clamp((silver-112)*7/4+112,0,255);break;
                }
                case Effect::stained_glass: value=edge?8:std::clamp(((value*5/4-24+31)/64)*64+16,0,255);break;
                case Effect::risograph: {
                    const int red=std::max(0,int(scratch[i*4])-int(scratch[i*4+2]));
                    const int ink=(255-light)*3/4;
                    constexpr std::array<int,3> paper{250,237,208},blue{205,155,65},coral{0,110,120};
                    value=std::clamp(paper[c]-ink*blue[c]/255-red*coral[c]/255,0,255);break;
                }
                case Effect::hologram: {
                    const int energy=edge?255:24+light*3/4;
                    value=energy*std::array<int,3>{32,210,255}[c]/255;
                    if((y/step)%4==3) value=value*2/5;break;
                }
                case Effect::mosaic: value=((x/step)%3==0 || (y/step)%3==0)?wash[c]*2/3:std::min(255,wash[c]+10);break;
                case Effect::pencil: value=edge?32:std::clamp(246-(255-light)*(255-light)/510,0,255);break;
                case Effect::oil_paint: value=std::clamp((wash[c]-128)*6/5+128,0,255);break;
                case Effect::teal_orange: {
                    // Split-tone shadows/highlights while retaining source detail.
                    constexpr std::array<int,3> low{0,64,80},high{255,180,92};
                    const auto tone=(low[c]*(255-light)+high[c]*light)/255;
                    value=(value+tone*2)/3;break;
                }
                case Effect::handheld: {
                    constexpr std::array<std::array<int,3>,4> colours{{
                        {15,56,15},{48,98,48},{139,172,15},{155,188,15}}};
                    value=colours[std::min(3,light/64)][c];break;
                }
                default: break;
                }
                rgba[i * 4 + c] = static_cast<std::uint8_t>(
                    (int(scratch[i * 4 + c]) * (100 - intensity) + value * intensity + 50) / 100);
            }
        }
    }
}
}
