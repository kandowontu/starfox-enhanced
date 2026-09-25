#pragma once
#include "starfox/render/backdrop_image.hpp"
#include "starfox/render/cloud_limb_atlas.hpp"
#include <memory>

namespace starfox::vr {
// Bit 8192 is otherwise a modifier of grid bit 64. With texture bit 1 alone,
// it selects photographic artwork; it must never be mixed with source types.
inline constexpr uint32_t backdrop_texture_flag=8193;
inline constexpr size_t backdrop_texture_word_limit=8'000'000;
inline constexpr uint32_t backdrop_texture_magic=0x31545256; // VRT1

inline bool backdrop_texture_valid(std::span<const uint32_t> words,
                                   uint32_t width,uint32_t height) noexcept {
    if(words.size()<7 || words.size()>backdrop_texture_word_limit
        || !width || !height || width>4096 || height>4096
        || words[0]!=backdrop_texture_magic || words[1]<1 || words[1]>13
        || words[2]>1 || words[3]!=0) return false;
    const auto levels=words[1];
    size_t offset=4+3*levels;
    if(offset>words.size()) return false;
    for(unsigned level=0;level<levels;++level) {
        const auto record=4+3*level;
        const size_t pixels=size_t(width)*height;
        if(words[record]!=offset || words[record+1]!=width || words[record+2]!=height
            || pixels>words.size()-offset) return false;
        offset+=pixels;
        if(width==1 && height==1) return level+1==levels && offset==words.size();
        width=std::max(1U,width/2);height=std::max(1U,height/2);
    }
    return false;
}

// Immutable, premultiplied RGBA pyramid. Only rebuild when artwork changes;
// brightness, live palette response and camera state belong to draw parameters.
inline std::shared_ptr<const std::vector<uint32_t>> make_backdrop_texture(
    const render::BackdropImage& image,bool wrap_horizontal=true,bool stable_zenith=false,bool stable_nadir=false) {
    if(!image.width || !image.height || image.width>4096 || image.height>4096
        || image.pixels.size()!=size_t(image.width)*image.height)
        throw std::invalid_argument("Invalid VR backdrop image");
    unsigned levels=1,w=image.width,h=image.height;
    size_t pixels=size_t(w)*h;
    while(w!=1 || h!=1) {w=std::max(1U,w/2);h=std::max(1U,h/2);pixels+=size_t(w)*h;++levels;}
    const size_t header=4+3*levels;
    if(pixels+header>backdrop_texture_word_limit) throw std::invalid_argument("VR backdrop exceeds texture budget");
    auto words=std::make_shared<std::vector<uint32_t>>(header+pixels);
    auto& data=*words;data[0]=backdrop_texture_magic;data[1]=levels;data[2]=wrap_horizontal;
    data[4]=uint32_t(header);data[5]=image.width;data[6]=image.height;
    for(size_t i=0;i<image.pixels.size();++i) {
        const auto value=image.pixels[i],alpha=value>>24;
        uint32_t packed=alpha<<24;
        for(unsigned channel=0;channel<3;++channel)
            packed|=(((((value>>(channel*8))&255)*alpha)+127)/255)<<(channel*8);
        data[header+i]=packed;
    }
    for(unsigned level=1;level<levels;++level) {
        const auto previous=4+3*(level-1),record=previous+3;
        const auto old_width=data[previous+1],old_height=data[previous+2];
        const auto width=std::max(1U,old_width/2),height=std::max(1U,old_height/2);
        const auto offset=data[previous]+old_width*old_height;
        data[record]=offset;data[record+1]=width;data[record+2]=height;
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const unsigned x0=x*old_width/width,x1=(x+1)*old_width/width;
            const unsigned y0=y*old_height/height,y1=(y+1)*old_height/height;
            uint32_t sum[4]{};
            for(unsigned sy=y0;sy<y1;++sy) for(unsigned sx=x0;sx<x1;++sx) {
                const auto value=data[data[previous]+sy*old_width+sx];
                for(unsigned c=0;c<4;++c) sum[c]+=(value>>(c*8))&255;
            }
            const auto count=(x1-x0)*(y1-y0);uint32_t packed=0;
            for(unsigned c=0;c<4;++c) packed|=((sum[c]+count/2)/count)<<(c*8);
            data[offset+y*width+x]=packed;
        }
    }
    if(stable_zenith) {
        // Minification must not reintroduce longitude-dependent colours into
        // the clamped sky pole. Keep the same radiance at every mip level.
        const auto zenith=data[header];
        for(unsigned level=0;level<levels;++level) {
            const auto record=4+3*level;
            std::fill_n(data.begin()+data[record],data[record+1],zenith);
        }
    }
    if(stable_nadir) {
        const auto nadir=data[header+size_t(image.height-1)*image.width];
        for(unsigned level=0;level<levels;++level) {
            const auto record=4+3*level,w=data[record+1],h=data[record+2];
            if(h>1) std::fill_n(data.begin()+data[record]+size_t(h-1)*w,w,nadir);
        }
    }
    return words;
}
// All-sky patterns are not horizon bands. Bake repeated source detail into a
// true 2:1 sphere atlas, with overlapping boundaries and uniform pole radiance.
// This runs once per selected artwork, never while the palette fades.
inline std::shared_ptr<const std::vector<uint32_t>> make_pattern_sky_texture(
    const render::BackdropImage& source) {
    if(!source.width || !source.height || source.width>4096 || source.height>4096
        || source.pixels.size()!=size_t(source.width)*source.height)
        throw std::invalid_argument("Invalid pattern sky artwork");
    render::BackdropImage sphere;sphere.width=2048;sphere.height=1024;
    sphere.pixels.resize(size_t(sphere.width)*sphere.height);
    constexpr float overlap=1.F/32;
    const auto sample=[&](float u,float v) {
        const float x=u*(1-overlap),y=v*(1-overlap);
        const auto row=[&](float sy) {
            auto colour=source.sample_projected(x,sy,3);
            if(x<overlap) {
                const auto tail=source.sample_projected(x+1-overlap,sy,3);
                float t=x/overlap;t=t*t*(3-2*t);
                for(unsigned c=0;c<3;++c) colour[c]=std::lerp(tail[c],colour[c],t);
            }
            return colour;
        };
        auto colour=row(y);
        if(y<overlap) {
            const auto tail=row(y+1-overlap);float t=y/overlap;t=t*t*(3-2*t);
            for(unsigned c=0;c<3;++c) colour[c]=std::lerp(tail[c],colour[c],t);
        }
        return colour;
    };
    for(unsigned y=0;y<sphere.height;++y) for(unsigned x=0;x<sphere.width;++x) {
        const float u=(x+.5F)*4/sphere.width;
        const float v=(y+.5F)*4/sphere.width*source.width/source.height;
        const auto colour=sample(u-std::floor(u),v-std::floor(v));
        uint32_t pixel=0xff000000U;
        for(unsigned c=0;c<3;++c) pixel|=uint32_t(std::clamp(colour[c],0.F,255.F)+.5F)<<(8*c);
        sphere.pixels[size_t(y)*sphere.width+x]=pixel;
    }
    sphere.prepare_zenith();
    for(unsigned y=0;y<sphere.height/2;++y)
        std::swap_ranges(sphere.pixels.begin()+size_t(y)*sphere.width,
            sphere.pixels.begin()+size_t(y+1)*sphere.width,
            sphere.pixels.begin()+size_t(sphere.height-1-y)*sphere.width);
    sphere.prepare_zenith();
    for(unsigned y=0;y<sphere.height/2;++y)
        std::swap_ranges(sphere.pixels.begin()+size_t(y)*sphere.width,
            sphere.pixels.begin()+size_t(y+1)*sphere.width,
            sphere.pixels.begin()+size_t(sphere.height-1-y)*sphere.width);
    return make_backdrop_texture(sphere,true,true,true);
}
// A single immutable atlas keeps the orbital limb and its top-down surface in
// one descriptor. The disk projection samples the square lower section without
// longitude convergence; looking down never magnifies a thin horizon strip.
inline std::shared_ptr<const std::vector<uint32_t>> make_orbital_texture(
    const render::BackdropImage& panorama,const render::BackdropImage& surface) {
    for(const auto* image:{&panorama,&surface})
        if(!image->width || !image->height || image->width>4096 || image->height>4096
            || image->pixels.size()!=size_t(image->width)*image->height)
            throw std::invalid_argument("Invalid orbital artwork");
    render::BackdropImage atlas;atlas.width=1024;atlas.height=1536;
    atlas.pixels.resize(size_t(atlas.width)*atlas.height);
    for(unsigned y=0;y<atlas.height;++y) for(unsigned x=0;x<atlas.width;++x) {
        const float u=(x+.5F)/1024;
        std::array<float,3> colour;
        if(y<512) {
            constexpr float overlap=1.F/32;
            const float v=(y+.5F)/512,p=u*(1-overlap);
            colour=panorama.sample_projected(p,v,3);
            if(p<overlap) {
                const auto tail=panorama.sample_projected(p+1-overlap,v,3);
                float weight=p/overlap;weight=weight*weight*(3-2*weight);
                for(unsigned c=0;c<3;++c) colour[c]=std::lerp(tail[c],colour[c],weight);
            }
        } else colour=surface.sample_projected(u,(y-512+.5F)/1024,3);
        uint32_t pixel=0xff000000U;
        for(unsigned c=0;c<3;++c) pixel|=uint32_t(std::clamp(colour[c],0.F,255.F)+.5F)<<(8*c);
        atlas.pixels[size_t(y)*atlas.width+x]=pixel;
    }
    return make_backdrop_texture(atlas,true);
}
// Crop the two cloud-scene subjects independently. The native limb's ink mask
// distinguishes its opaque dark interior from transparent space; luminance
// alone would erase that interior when its palette becomes black.
inline std::shared_ptr<const std::vector<uint32_t>> make_cloud_body_texture(
    const render::BackdropImage& atlas,const render::CloudLimbAtlas& source,bool limb) {
    if(atlas.width!=2048 || atlas.height!=2048 || atlas.pixels.size()!=2048*2048)
        throw std::invalid_argument("Invalid cloud atlas");
    render::BackdropImage image;image.width=limb?320:192;image.height=limb?128:192;
    image.pixels.resize(size_t(image.width)*image.height);
    const auto mask=[&](int x,int y) {return x>=0 && y>=0 && source.limb_ink(unsigned(x),unsigned(y))?1.F:0.F;};
    for(unsigned y=0;y<image.height;++y) for(unsigned x=0;x<image.width;++x) {
        auto pixel=atlas.pixels[size_t(y+(limb?1280:1056))*2048+x+(limb?640:320)];
        float alpha;
        if(limb) {
            const float sx=(x+.5F)/4-.5F,sy=(y+.5F)/4-.5F;
            const int ax=int(std::floor(sx)),ay=int(std::floor(sy));
            alpha=std::lerp(std::lerp(mask(ax,ay),mask(ax+1,ay),sx-ax),
                std::lerp(mask(ax,ay+1),mask(ax+1,ay+1),sx-ax),sy-ay);
        } else {
            // Smoothly key the master image's black surround, preserving its
            // irregular wisps instead of clipping it to a planet-shaped disk.
            const float light=float(std::max({pixel&255,(pixel>>8)&255,(pixel>>16)&255}));
            alpha=std::clamp(light/12.F,0.F,1.F);
        }
        image.pixels[size_t(y)*image.width+x]=(pixel&0xffffffU)|(uint32_t(alpha*255+.5F)<<24);
    }
    return make_backdrop_texture(image,false);
}
inline std::shared_ptr<const std::vector<uint32_t>> make_celestial_texture(
    const render::BackdropImage& source,const std::array<float,4>& bounds) {
    if(!source.width || !source.height || source.pixels.size()!=size_t(source.width)*source.height)
        throw std::invalid_argument("Invalid celestial master");
    for(float value:bounds) if(!std::isfinite(value) || value<0 || value>1254)
        throw std::invalid_argument("Invalid celestial master bounds");
    if(bounds[2]<=bounds[0] || bounds[3]<=bounds[1]) throw std::invalid_argument("Empty celestial master bounds");
    render::BackdropImage image;image.width=image.height=512;image.pixels.resize(512*512);
    for(unsigned y=0;y<512;++y) for(unsigned x=0;x<512;++x) {
        const float u=(float(x)+.5F)/512,v=(float(y)+.5F)/512;
        const auto sample=source.sample_projected((bounds[0]+u*(bounds[2]-bounds[0]))/1253.F,
            (bounds[1]+v*(bounds[3]-bounds[1]))/1253.F,3);
        const float alpha=std::clamp((1-std::hypot(2*u-1,2*v-1))*256,0.F,1.F);
        uint32_t pixel=uint32_t(alpha*255+.5F)<<24;
        for(unsigned c=0;c<3;++c) pixel|=uint32_t(std::clamp(sample[c],0.F,255.F)+.5F)<<(c*8);
        image.pixels[y*512+x]=pixel;
    }
    return make_backdrop_texture(image,false);
}
// Register a unique cratered disk, not the black square around the master.
// Atmospheric fading is immutable; live moon colour remains draw-time data.
inline std::shared_ptr<const std::vector<uint32_t>> make_moon_texture(
    const render::BackdropImage& source,bool atmospheric,
    const std::array<float,4>& bounds={31,29,1216,1209}) {
    if(!source.width || !source.height || source.pixels.size()!=size_t(source.width)*source.height)
        throw std::invalid_argument("Invalid moon master");
    render::BackdropImage image;image.width=image.height=512;
    image.pixels.resize(size_t(image.width)*image.height);
    constexpr std::array<float,4> fade{.30F,31.F/56.F,-1,0};
    for(unsigned y=0;y<image.height;++y) for(unsigned x=0;x<image.width;++x) {
        const float u=(float(x)+.5F)/image.width,v=(float(y)+.5F)/image.height;
        const auto sample=source.sample_projected((bounds[0]+u*(bounds[2]-bounds[0]))/1253.F,
            (bounds[1]+v*(bounds[3]-bounds[1]))/1253.F,3);
        const float light=std::clamp((sample[0]*.2126F+sample[1]*.7152F+sample[2]*.0722F)/170.F,0.F,1.F);
        std::array<float,3> colour{255*light,255*light,255*light};
        const float radius=std::hypot(2*u-1,2*v-1);
        float alpha=std::clamp((1-radius)*image.width*.5F,0.F,1.F);
        if(atmospheric) {
            colour=render::BackdropImage::moon_surface(colour,{u*.5F,2+v},fade);
            alpha*=render::BackdropImage::moon_opacity({u*.5F,2+v},fade);
        }
        uint32_t pixel=uint32_t(alpha*255+.5F)<<24;
        for(unsigned c=0;c<3;++c) pixel|=uint32_t(std::clamp(colour[c],0.F,255.F)+.5F)<<(c*8);
        image.pixels[size_t(y)*image.width+x]=pixel;
    }
    return make_backdrop_texture(image,false);
}
// Bake the desktop sampler's short horizontal overlap once, before mipmapping.
// The zenith row is uniform so all longitudes meet cleanly at the sky pole.
// Use only for repeatable landscapes, never for a unique planet/moon atlas.
inline std::shared_ptr<const std::vector<uint32_t>> make_landscape_texture(
    const render::BackdropImage& image,bool full_sphere=false,bool uniform_sphere=false,
    unsigned overlap_divisor=32) {
    if(!image.width || !image.height || image.width>4096 || image.height>4096
        || image.pixels.size()!=size_t(image.width)*image.height || (uniform_sphere && !full_sphere)
        || overlap_divisor<4 || overlap_divisor>128)
        throw std::invalid_argument("Invalid VR landscape image");
    const unsigned overlap=std::max(1U,image.width/overlap_divisor);
    render::BackdropImage prepared;
    prepared.width=std::max(1U,image.width-overlap);prepared.height=image.height;
    prepared.pixels.resize(size_t(prepared.width)*prepared.height);
    for(unsigned y=0;y<image.height;++y) for(unsigned x=0;x<prepared.width;++x) {
        auto pixel=image.pixels[size_t(y)*image.width+x];
        if(x<overlap && image.width>1) {
            const auto tail=image.pixels[size_t(y)*image.width+prepared.width+x];
            float blend=float(x)/overlap;blend=blend*blend*(3-2*blend);
            pixel=0;
            for(unsigned c=0;c<4;++c) {
                const float a=float((tail>>(c*8))&255),b=float((image.pixels[size_t(y)*image.width+x]>>(c*8))&255);
                pixel|=uint32_t(std::lround(std::lerp(a,b,blend)))<<(c*8);
            }
        }
        prepared.pixels[size_t(y)*prepared.width+x]=pixel;
    }
    if(uniform_sphere) {
        // Star-only masters need a 2:1 sphere atlas to keep their points round.
        // Tile additional rows through a short overlap; never stretch the stars.
        render::BackdropImage sphere;sphere.width=prepared.width;
        sphere.height=std::max(1U,prepared.width/2);
        sphere.pixels.resize(size_t(sphere.width)*sphere.height);
        const unsigned overlap_y=std::max(1U,prepared.height/32),period=std::max(1U,prepared.height-overlap_y);
        const int offset=int(prepared.height/2)-int(sphere.height/2);
        for(unsigned y=0;y<sphere.height;++y) {
            const unsigned sy=unsigned(((int(y)+offset)%int(period)+int(period))%int(period));
            for(unsigned x=0;x<sphere.width;++x) {
                auto pixel=prepared.pixels[size_t(sy)*prepared.width+x];
                if(sy<overlap_y && prepared.height>1) {
                    const auto tail=prepared.pixels[size_t(period+sy)*prepared.width+x];
                    float blend=float(sy)/overlap_y;blend=blend*blend*(3-2*blend);pixel=0;
                    for(unsigned c=0;c<4;++c) pixel|=uint32_t(std::lround(std::lerp(float((tail>>(c*8))&255),
                        float((prepared.pixels[size_t(sy)*prepared.width+x]>>(c*8))&255),blend)))<<(c*8);
                }
                sphere.pixels[size_t(y)*sphere.width+x]=pixel;
            }
        }
        prepared=std::move(sphere);
    }
    prepared.prepare_zenith();
    if(full_sphere) {
        const auto flip=[&] {
            for(unsigned y=0;y<prepared.height/2;++y)
                std::swap_ranges(prepared.pixels.begin()+size_t(y)*prepared.width,
                    prepared.pixels.begin()+size_t(y+1)*prepared.width,
                    prepared.pixels.begin()+size_t(prepared.height-1-y)*prepared.width);
        };
        flip();prepared.prepare_zenith();flip();
    }
    return make_backdrop_texture(prepared,true,true,full_sphere);
}
}
