#pragma once
#include "starfox/render/framebuffer.hpp"
#include <array>
#include <cmath>

namespace starfox::render {
inline void apply_chromatic_aberration(const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba, std::vector<std::uint8_t>& scratch,
    std::uint8_t intensity) {
    if (intensity==0 || intensity>3 || !frame.layer_tags_enabled()
        || rgba.size()!=frame.pixels().size()*4U) return;
    const auto width=frame.stored_width(), height=frame.stored_height();
    if (width==0 || height==0) return;
    scratch=rgba;
    const auto model=[&](unsigned x,unsigned y) {
        const auto layer=frame.layer_stored(x,y);
        return layer==PixelLayer::three_d || layer==PixelLayer::textured_geometry;
    };
    const auto amount=std::array<double,4>{0,.6,1.2,2.4}[intensity]*frame.draw_scale();
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) {
        if (!model(x,y)) continue;
        const auto pixel=(static_cast<std::size_t>(y)*width+x)*4U;
        const auto dx=(2.0*(x+.5)/width-1.0)*amount;
        const auto dy=(2.0*(y+.5)/height-1.0)*amount;
        for (const auto channel : {0U,2U}) {
            const auto sign=channel==0?1.0:-1.0;
            const auto sx=std::clamp(x+sign*dx,0.0,double(width-1));
            const auto sy=std::clamp(y+sign*dy,0.0,double(height-1));
            const auto x0=static_cast<unsigned>(sx), y0=static_cast<unsigned>(sy);
            const auto x1=std::min(x0+1,width-1), y1=std::min(y0+1,height-1);
            // Never pull HUD text or background art into model color channels.
            const auto sample=[&](unsigned px,unsigned py) {
                return double(model(px,py)?scratch[(static_cast<std::size_t>(py)*width+px)*4U+channel]
                    :scratch[pixel+channel]);
            };
            const auto fx=sx-x0, fy=sy-y0;
            const auto top=sample(x0,y0)*(1-fx)+sample(x1,y0)*fx;
            const auto bottom=sample(x0,y1)*(1-fx)+sample(x1,y1)*fx;
            rgba[pixel+channel]=static_cast<std::uint8_t>(std::lround(top*(1-fy)+bottom*fy));
        }
    }
}
} // namespace starfox::render
