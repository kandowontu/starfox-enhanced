#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace starfox::render {
inline bool valid_raster_jitter(std::array<float,2> jitter) noexcept {
    return std::isfinite(jitter[0]) && std::isfinite(jitter[1])
        && std::abs(jitter[0])<=16 && std::abs(jitter[1])<=16;
}
// Pixel displacement, not normalized device coordinates. Quantizing to 1/256
// keeps phase identical in integer-backed 2D and floating-point 3D producers.
inline std::array<float,2> temporal_jitter(std::uint64_t frame) noexcept {
    const auto radical=[](unsigned n,unsigned base) {
        float result=0,weight=1;
        while(n) {weight/=base;result+=weight*(n%base);n/=base;}
        return std::round((result-.5f)*256.f)/256.f;
    };
    const auto sample=unsigned(frame%32)+1;
    return {radical(sample,2),radical(sample,3)};
}
}
