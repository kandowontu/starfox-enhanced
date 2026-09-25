#pragma once
#include "starfox/render/backdrop_image.hpp"

namespace starfox::render {
// EX menu 23/24 share BGLASTPCR: alternate 240px disks at (0,128)
// and (256,128), repeating every 512x256 atlas pixels. Only their two
// authored CGRAM ramps differ. Reconstruct those simple analytic shapes,
// not a generic landscape, preserving palette fades and the original phase.
class RadialBackdrop {
    std::array<std::uint16_t,32> palette_{};
    BackdropImage image_;
public:
    const BackdropImage& image(std::span<const std::uint16_t> palette) {
        if(palette.size()<80) throw std::runtime_error("Radial backdrop palette is incomplete");
        std::array<std::uint16_t,32> colours{};
        std::copy_n(palette.begin()+48,32,colours.begin());
        if(image_.immutable_upload_key && palette_==colours) return image_;
        palette_=colours;
        image_.immutable_upload_key=0;
        image_.width=1024;image_.height=512;
        image_.pixels.resize(std::size_t(image_.width)*image_.height);
        // Match BackdropImage's overlap-repeat period exactly. The extra
        // rows/columns are copies of the beginning, not squeezed atlas art.
        const float period_x=float(image_.width-image_.width/32);
        const float period_y=float(image_.height-image_.height/32);
        for(unsigned y=0;y<image_.height;++y) for(unsigned x=0;x<image_.width;++x) {
            const float sx=float(x)*512.f/period_x;
            const float sy=float(y)*256.f/period_y;
            const int disk=int(std::floor((sx+128.f)/256.f));
            const float dx=sx-float(disk)*256.f;
            const float dy=std::remainder(sy-128.f,256.f);
            const float radius=std::sqrt(dx*dx+dy*dy);
            // Source rings advance one shade per eight pixels, ending in
            // black at 120px. Interpolation removes only the band quantization.
            const float shade=std::clamp(radius/8.f,1.f,15.f);
            const unsigned low=unsigned(shade),high=std::min(low+1,15U);
            const unsigned bank=(disk&1)?16:0;
            std::uint32_t pixel=0xff000000U;
            for(unsigned c=0;c<3;++c) {
                const auto channel=[&](unsigned i) {
                    const unsigned v=(colours[bank+i]>>(c*5))&31;
                    return float((v<<3)|(v>>2));
                };
                pixel|=std::uint32_t(std::lerp(channel(low),channel(high),shade-low)+.5f)<<(c*8);
            }
            image_.pixels[std::size_t(y)*image_.width+x]=pixel;
        }
        image_.seal_for_upload();
        return image_;
    }
};
}
