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
    // Channel offsets are separable: x depends only on the column, y only
    // on the row. Preserve the original expression and rounding order, but
    // calculate its clamped taps once instead of twice for every model pixel.
    struct Tap { unsigned first, second; double fraction; };
    const auto taps=[amount](unsigned position,unsigned extent) {
        std::array<Tap,2> result{};
        const auto delta=(2.0*(position+.5)/extent-1.0)*amount;
        for (unsigned channel=0;channel<2;++channel) {
            const auto sign=channel==0?1.0:-1.0;
            const auto sample=std::clamp(position+sign*delta,0.0,double(extent-1));
            const auto first=static_cast<unsigned>(sample);
            result[channel]={first,std::min(first+1,extent-1),sample-first};
        }
        return result;
    };
    std::vector<std::array<Tap,2>> columns(width);
    for (unsigned x=0;x<width;++x) columns[x]=taps(x,width);
    for (unsigned y=0;y<height;++y) {
        const auto rows=taps(y,height);
        for (unsigned x=0;x<width;++x) {
            if (!model(x,y)) continue;
            const auto pixel=(static_cast<std::size_t>(y)*width+x)*4U;
            for (const auto channel : {0U,2U}) {
                const auto& column=columns[x][channel/2];
                const auto& row=rows[channel/2];
                const auto x0=column.first, x1=column.second;
                const auto y0=row.first, y1=row.second;
                // Never pull HUD text or background art into model color channels.
                const auto sample=[&](unsigned px,unsigned py) {
                    return double(model(px,py)?scratch[(static_cast<std::size_t>(py)*width+px)*4U+channel]
                        :scratch[pixel+channel]);
                };
                const auto fx=column.fraction, fy=row.fraction;
                const auto top=sample(x0,y0)*(1-fx)+sample(x1,y0)*fx;
                const auto bottom=sample(x0,y1)*(1-fx)+sample(x1,y1)*fx;
                rgba[pixel+channel]=static_cast<std::uint8_t>(std::lround(top*(1-fy)+bottom*fy));
            }
        }
    }
}
} // namespace starfox::render
