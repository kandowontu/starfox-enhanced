#pragma once
#include "starfox/render/framebuffer.hpp"
#include <array>
#include <cmath>

namespace starfox::render {
// SDR brightness/contrast styling only: does not request an HDR swapchain,
// change display metadata, or claim additional physical display headroom.
inline void apply_hdr_effect(const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba, std::uint8_t intensity) {
    if (intensity==0 || intensity>3 || !frame.layer_tags_enabled()
        || rgba.size()!=frame.pixels().size()*4U) return;
    std::array<std::uint8_t,256> curve{};
    const auto exposure=std::array<double,4>{1,1.15,1.35,1.65}[intensity];
    const auto contrast=std::array<double,4>{0,.2,.4,.6}[intensity];
    for (unsigned i=0;i<curve.size();++i) {
        const auto input=i/255.0;
        const auto lifted=input*exposure/(1+input*(exposure-1));
        const auto shaped=lifted*lifted*(3-2*lifted);
        curve[i]=static_cast<std::uint8_t>(std::lround(
            255*(lifted*(1-contrast)+shaped*contrast)));
    }
    const auto& tags=frame.layer_tags();
    for (std::size_t pixel=0;pixel<frame.pixels().size();++pixel) {
        if (tags[pixel]==static_cast<std::uint8_t>(PixelLayer::two_d)) continue;
        for (unsigned channel=0;channel<3;++channel)
            rgba[pixel*4+channel]=curve[rgba[pixel*4+channel]];
    }
}
} // namespace starfox::render
