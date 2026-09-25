#include "starfox/render/backdrop_image.hpp"
#include <chrono>
#include <iostream>

int main() {
    using namespace starfox::render;
    BackdropImage image{2172,724,std::vector<std::uint32_t>(2172*724,0xff324567U)};
    BackdropUploadCache cache;cache.remember(image);
    constexpr unsigned frames=2000;
    const auto run=[&] {
        unsigned hits=0;
        const auto begin=std::chrono::steady_clock::now();
        for(unsigned i=0;i<frames;++i) {
            std::atomic_signal_fence(std::memory_order_seq_cst);
            hits+=cache.matches(image);
        }
        if(hits!=frames) throw std::runtime_error("Backdrop cache missed unchanged image");
        return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    };
    const auto mutable_ms=run();
    const auto mutable_bytes=cache.retained_pixels()*4;
    image.seal_for_upload();cache.remember(image);
    const auto sealed_ms=run();
    std::cout<<"Cache-only benchmark, "<<frames<<" checks of "<<image.pixels.size()*4<<" bytes\n"
        <<"Mutable content check: "<<mutable_ms<<" ms; retained "<<mutable_bytes<<" bytes\n"
        <<"Sealed identity check: "<<sealed_ms<<" ms; retained "<<cache.retained_pixels()*4<<" bytes\n"
        <<"Not an in-game FPS benchmark.\n";
}
