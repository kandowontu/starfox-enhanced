#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
#include "starfox/render/face_planet_geometry.hpp"
#include "starfox/render/city_moon_geometry.hpp"
#include "starfox/render/cloud_limb_geometry.hpp"

namespace starfox::render {
// Optional enhancement artwork, independent of cartridge assets. Restricted
// BMP decoding keeps the embedded asset usable on every target without an
// OS image decoder. Returned pixels are little-endian RGBA words.
struct BackdropImage {
    static std::array<float,3> limb_colour(const std::array<float,3>& surface,
        const std::array<std::uint32_t,16>& ramp) {
        const float shade=1+14*(1-std::clamp(surface[0]/255.f,0.f,1.f));
        const unsigned a=unsigned(shade),b=std::min(a+1,15U);
        std::array<float,3> result{};
        for(unsigned c=0;c<3;++c) result[c]=std::lerp(float((ramp[a]>>(c*8))&255),
            float((ramp[b]>>(c*8))&255),shade-a);
        return result;
    }
    static std::array<float,3> moon_colour(std::array<float,3> surface,const std::array<float,2>& uv,
        const std::array<std::uint32_t,16>& ramp) {
        if(ramp[6]==1 || ramp[6]==2) {
            // Banded orange planets use eight authored warm shades, not a
            // grayscale disk multiplied by its pale brightest highlight.
            const bool blue=uv[0]>=.5f;
            const unsigned count=ramp[6]==1?7U:blue?1U:2U,base=ramp[6]==2 && blue?10U:7U;
            const float shade=float(count)*(1-std::clamp(surface[0]/255.f,0.f,1.f));
            const unsigned a=unsigned(shade),b=std::min(a+1,count);
            for(unsigned c=0;c<3;++c)
                surface[c]=std::lerp(float((ramp[base+a]>>(c*8))&255),float((ramp[base+b]>>(c*8))&255),shade-a);
            return surface;
        }
        const unsigned body=uv[0]<.5f?0:1;
        const auto bright=ramp[1+body],dark=ramp[3+body];
        const float x=(uv[0]-float(body)*.5f)*4-1;
        float light=std::clamp((x+.05f)/.4f,0.f,1.f);light=light*light*(3-2*light);
        for(unsigned c=0;c<3;++c) {
            const float b=float((bright>>(c*8))&255),d=float((dark>>(c*8))&255);
            surface[c]=ramp[5]?std::lerp(d,b,light)*(.8f+.2f*surface[c]/255.f):surface[c]*b/255.f;
        }
        return surface;
    }
    static std::array<float,3> moon_surface(std::array<float,3> colour,
        const std::array<float,2>& uv,const std::array<float,4>& fade) {
        if(fade[2]>=0 || uv[1]<2) return colour;
        float t=std::clamp((uv[1]-2)/std::max(.0001f,fade[1]),0.f,1.f);
        t=t*t*(3-2*t);
        for(auto& c:colour) c=(c+(255-c)*(.32f*(1-t)))*(1-.65f*t);
        return colour;
    }
    // A negative second radius reserves that unused moon slot for the first
    // moon's lower atmospheric fade, in normalized disk-height coordinates.
    static float moon_opacity(const std::array<float,2>& uv,const std::array<float,4>& fade) {
        if(fade[2]>=0 || uv[1]<2) return 1;
        const float t=std::clamp((uv[1]-2-fade[0])/std::max(.0001f,fade[1]-fade[0]),0.f,1.f);
        return 1-t*t*(3-2*t);
    }
    unsigned width{},height{};
    std::vector<std::uint32_t> pixels;
    // Nonzero only after all edits are finished. Callers must clear this key
    // before mutating pixels/dimensions, then seal again after editing.
    std::uint64_t immutable_upload_key{};
    void seal_for_upload() {
        static std::atomic<std::uint64_t> next{1};
        immutable_upload_key=next.fetch_add(1,std::memory_order_relaxed);
    }
    // Prepare a sky-only texture's clamped zenith once, before upload. A
    // varying top row extrudes stars/noise into vertical columns outside the
    // authored image. Blend a narrow edge into its mean sky radiance instead.
    // Interior pixels and the horizon remain exact; sampling has no extra cost.
    void prepare_zenith(unsigned rows=8) {
        if(!width || !height || pixels.size()!=std::size_t(width)*height || !rows) return;
        immutable_upload_key=0;
        rows=std::min(rows,height);
        std::array<std::uint64_t,3> sum{};
        for(unsigned x=0;x<width;++x) for(unsigned c=0;c<3;++c)
            sum[c]+=(pixels[x]>>(c*8))&255;
        std::array<float,3> mean{};
        for(unsigned c=0;c<3;++c) mean[c]=float(sum[c])/float(width);
        for(unsigned y=0;y<rows;++y) {
            float t=rows>1?float(y)/float(rows-1):0.f;t=t*t*(3-2*t);
            for(unsigned x=0;x<width;++x) {
                auto& pixel=pixels[std::size_t(y)*width+x];
                const auto original=pixel;pixel&=0xff000000U;
                for(unsigned c=0;c<3;++c)
                    pixel|=std::uint32_t(std::lerp(mean[c],float((original>>(c*8))&255),t)+.5f)<<(c*8);
            }
        }
    }
    static bool covers(float x,float y,float horizon,float slope,const std::array<float,4>& projection,
        const std::array<std::array<float,4>,2>& keep) {
        if(projection[3]==7) return true; // Orbital atlas includes the lower surface.
        if(projection[3]==6 || projection[3]==8) return y<horizon+slope*x;
        if(projection[3]==5) return face_planet_coordinates(x,y,keep[1]).has_value();
        if(projection[3]==9) return cloud_limb_coordinates(x,y,keep[1]).has_value();
        // Mode 3 is a single celestial sprite, not a repeating panorama.
        if(projection[3]==3 || projection[3]==4) {
            const auto& body=keep[0];
            if(body[2]<=0 || body[3]<=0) return false;
            if(projection[3]==4) {
                const auto& transform=keep[1];
                const float source_x=x+transform[0]*y+transform[2];
                y=y+transform[1]*x+transform[3];x=source_x;
            }
            const float dx=(x-body[0])/body[2],dy=(y-body[1])/body[3];
            return dx*dx+dy*dy<=1;
        }
        if(projection[3]==0 && y>=horizon+slope*x) return false;
        for(const auto& ellipse:keep) if(ellipse[2]>0 && ellipse[3]>0) {
            const float dx=(x-ellipse[0])/ellipse[2],dy=(y-ellipse[1])/ellipse[3];
            if(dx*dx+dy*dy<=1) return false;
        }
        return true;
    }
    // xyz: panorama horizontal/vertical scale and authored horizon position.
    // Orbital panoramas have visible surface below their horizon; landscapes
    // retain the original sky-only mapping by default.
    static std::array<float,2> coordinates(float x,float y,float horizon,float slope,float scroll,
        std::array<float,4> projection={1/512.f,1/160.f,1.f,0.f},
        const std::array<std::array<float,4>,2>& keep={}) {
        if(projection[3]==8) {
            if(const auto uv=city_moon_coordinates(x,y,keep[1])) return *uv;
        }
        if(projection[3]==5) {
            const auto source=face_planet_coordinates(x,y,keep[1]).value_or(std::array<float,2>{});
            return {source[0]/512.f,source[1]/512.f};
        }
        if(projection[3]==9) {
            const auto source=cloud_limb_coordinates(x,y,keep[1]).value_or(std::array<float,2>{});
            return {source[0]/512.f,source[1]/512.f};
        }
        if(projection[3]==6 || projection[3]==7) for(unsigned i=0;i<keep.size();++i) {
            const auto& body=keep[i];
            if(body[2]<=0 || body[3]<=0) continue;
            const float dx=(x-body[0])/body[2],dy=(y-body[1])/body[3];
            if(dx*dx+dy*dy<=1) return {(float(i)+(dx+1)*.5f)*.5f,2+(dy+1)*.5f};
        }
        // Mode 4 registers a unique body in cartridge atlas coordinates.
        // Offset-per-row/column scrolling is an affine shear, not necessarily
        // a camera rotation. Share the exact transform with its footprint.
        if(projection[3]==4) {
            const auto& t=keep[1];
            return {(x+t[0]*y+t[2]-keep[0][0]+scroll)*projection[0],
                projection[2]+(y+t[1]*x+t[3]-keep[0][1])*projection[1]};
        }
        const float inverse=1/std::sqrt(1+slope*slope),dy=y-horizon;
        const float v=projection[2]+inverse*(dy-slope*x)*projection[1];
        // In packed orbital atlases, v>=2 identifies a moon slot. Clamp the
        // panorama before tagging so a steep bank cannot select a moon tile.
        return {(inverse*(x+slope*dy)+scroll)*projection[0],projection[3]>=7?std::clamp(v,0.f,1.f):v};
    }
    // Repeat the panorama through a small overlap, not a hard image seam.
    // Vertical coordinates clamp to the authored zenith/horizon, never wrap
    // mountains above the camera or create a separate fallback-colour wedge.
    std::array<float,3> sample_region(float u,float v,unsigned height) const {
        if(!width || !height || height>this->height || pixels.size()!=std::size_t(width)*this->height || !std::isfinite(u) || !std::isfinite(v)) return {};
        const unsigned overlap=std::max(1u,width/32),period=std::max(1u,width-overlap);
        const float x=(u-std::floor(u))*float(period),y=std::clamp(v,0.f,1.f)*float(height-1);
        const auto bilinear=[&](float sx) {
            const auto x0=std::min(unsigned(sx),width-1),x1=std::min(x0+1,width-1);
            const auto y0=unsigned(y),y1=std::min(y0+1,height-1);
            const float fx=sx-std::floor(sx),fy=y-float(y0);
            std::array<float,3> c{};
            for(unsigned k=0;k<3;++k) {
                const auto at=[&](unsigned a,unsigned b){return float((pixels[std::size_t(b)*width+a]>>(k*8))&255u);};
                c[k]=std::lerp(std::lerp(at(x0,y0),at(x1,y0),fx),std::lerp(at(x0,y1),at(x1,y1),fx),fy);
            }
            return c;
        };
        auto c=bilinear(x);
        if(x<float(overlap)) {
            const auto tail=bilinear(float(period)+x);
            float blend=x/float(overlap);blend=blend*blend*(3-2*blend);
            for(unsigned k=0;k<3;++k) c[k]=std::lerp(tail[k],c[k],blend);
        }
        return c;
    }
    std::array<float,3> sample(float u,float v) const {return sample_region(u,v,height);}
    // Space panoramas repeat vertically through a short overlap. Landscapes
    // keep their existing clamped zenith/ground behavior.
    std::array<float,3> sample(float u,float v,bool repeat_vertical) const {
        if(!repeat_vertical || height<2 || !std::isfinite(v)) return sample(u,v);
        const unsigned overlap=std::max(1u,height/32),period=height-overlap;
        const float y=(v-std::floor(v))*float(period);
        auto c=sample(u,y/float(height-1));
        if(y<float(overlap)) {
            const auto tail=sample(u,(float(period)+y)/float(height-1));
            float t=y/float(overlap);t=t*t*(3-2*t);
            for(unsigned k=0;k<3;++k)c[k]=std::lerp(tail[k],c[k],t);
        }
        return c;
    }
    std::array<float,3> sample_projected(float u,float v,float mode) const {
        if(mode==6 || mode==7 || mode==8) {
            if(v<2) return sample_region(u,v,height/2);
            if(height<2) return {};
            return sample_projected(u,(float(height/2)+(v-2)*float(height/2-1))/float(height-1),3);
        }
        if(mode!=3 && mode!=4 && mode!=5 && mode!=9) return sample(u,v,mode==2);
        if(!width || !height || pixels.size()!=std::size_t(width)*height
            || !std::isfinite(u) || !std::isfinite(v)) return {};
        const float x=std::clamp(u,0.f,1.f)*float(width-1);
        const float y=std::clamp(v,0.f,1.f)*float(height-1);
        const unsigned x0=unsigned(x),y0=unsigned(y);
        std::array<float,3> result{};
        for(unsigned k=0;k<3;++k) {
            const auto at=[&](unsigned a,unsigned b){return float((pixels[std::size_t(b)*width+a]>>(k*8))&255u);};
            result[k]=std::lerp(std::lerp(at(x0,y0),at(std::min(x0+1,width-1),y0),x-x0),
                std::lerp(at(x0,std::min(y0+1,height-1)),at(std::min(x0+1,width-1),std::min(y0+1,height-1)),x-x0),y-y0);
        }
        return result;
    }
    static BackdropImage decode(std::span<const std::uint8_t> bytes) {
        const auto u16=[&](std::size_t p)->unsigned {
            if(p>bytes.size() || bytes.size()-p<2) throw std::runtime_error("Truncated backdrop BMP");
            return unsigned(bytes[p])|(unsigned(bytes[p+1])<<8);
        };
        const auto u32=[&](std::size_t p)->std::uint32_t {return u16(p)|(std::uint32_t(u16(p+2))<<16);};
        if(u16(0)!=0x4d42 || u32(14)<40 || u16(26)!=1 || u32(30)!=0)
            throw std::runtime_error("Unsupported backdrop BMP");
        const auto w=std::int32_t(u32(18)),h=std::int32_t(u32(22));
        const auto bits=u16(28),offset=u32(10);
        if(w<=0 || w>8192 || h==0 || h < -8192 || h>8192 || (bits!=24 && bits!=32))
            throw std::runtime_error("Invalid backdrop BMP dimensions");
        BackdropImage image;image.width=unsigned(w);image.height=unsigned(h<0?-h:h);
        const std::size_t stride=(std::size_t(w)*(bits/8)+3)&~std::size_t(3);
        if(offset<54 || offset>bytes.size() || image.height>(bytes.size()-offset)/stride)
            throw std::runtime_error("Truncated backdrop BMP pixels");
        image.pixels.resize(std::size_t(image.width)*image.height);
        for(unsigned y=0;y<image.height;++y) for(unsigned x=0;x<image.width;++x) {
            const auto at=offset+std::size_t(h<0?y:image.height-1-y)*stride+x*(bits/8);
            image.pixels[std::size_t(y)*image.width+x]=std::uint32_t(bytes[at+2])
                |(std::uint32_t(bytes[at+1])<<8)|(std::uint32_t(bytes[at])<<16)|0xff000000u;
        }
        return image;
    }
};
// Mutable callers retain exact content comparison. Sealed runtime assets
// need neither a full image comparison each frame nor a duplicate CPU copy.
class BackdropUploadCache {
    std::uint64_t key_{};
    std::vector<std::uint32_t> pixels_;
public:
    bool matches(const BackdropImage& image) const {
        return image.immutable_upload_key ? key_==image.immutable_upload_key
            : key_==0 && pixels_==image.pixels;
    }
    void remember(const BackdropImage& image) {
        key_=image.immutable_upload_key;
        if(key_) {std::vector<std::uint32_t>{}.swap(pixels_);}
        else pixels_=image.pixels;
    }
    std::size_t retained_pixels() const {return pixels_.size();}
};
}
