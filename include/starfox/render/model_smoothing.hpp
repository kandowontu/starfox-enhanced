#pragma once
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/row_workers.hpp"
#include <array>

namespace starfox::render {

// Smooth colour boundaries inside model coverage. Background/HUD neighbours
// never enter the kernel; outer silhouette antialiasing remains a separate option.
inline void smooth_models(std::uint8_t level, const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba, std::vector<std::uint8_t>& scratch,
    RowWorkers* workers = nullptr) {
    if (level == 0U || level > 3U || !frame.layer_tags_enabled()
        || rgba.size() != frame.pixels().size()*4U || rgba.empty()) return;
    scratch=rgba;
    const auto width=frame.stored_width(), height=frame.stored_height();
    const auto step=frame.draw_scale();
    const auto model=[&](std::size_t i) {
        const auto tag=frame.layer_tags()[i];
        return tag==std::uint8_t(PixelLayer::three_d)
            || tag==std::uint8_t(PixelLayer::textured_geometry);
    };
    const auto rows=[&](unsigned first,unsigned last) {
        for(unsigned y=first;y<last;++y) for(unsigned x=0;x<width;++x) {
            const auto i=std::size_t(y)*width+x;
            if(!model(i)) continue;
            const std::array neighbours{
                std::size_t(y)*width+(x>=step?x-step:0U),
                std::size_t(y)*width+std::min(x+step,width-1U),
                std::size_t(y>=step?y-step:0U)*width+x,
                std::size_t(std::min(y+step,height-1U))*width+x};
            std::array<unsigned,3> total{};
            unsigned count=0;
            for(auto n:neighbours) if(model(n)) {
                ++count;
                for(unsigned c=0;c<3;++c) total[c]+=scratch[n*4+c];
            }
            if(!count) continue;
            for(unsigned c=0;c<3;++c) {
                const auto average=(total[c]+count/2U)/count;
                rgba[i*4+c]=std::uint8_t((scratch[i*4+c]*(4U-level)+average*level+2U)/4U);
            }
        }
    };
    if(workers && frame.pixels().size()>=128U*1024U) workers->parallel_rows(height,rows);
    else rows(0,height);
}
}
