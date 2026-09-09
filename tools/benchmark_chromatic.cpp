#include "starfox/render/chromatic_aberration.hpp"
#include <chrono>
#include <iostream>

int main() {
    using namespace starfox::render;
    for (unsigned scale : {1U,2U,4U}) {
        Framebuffer frame{796U,224U,scale};
        frame.enable_layer_tags(true);
        for (std::size_t i=0; i<frame.pixels().size(); ++i)
            frame.layer_tags()[i]=static_cast<std::uint8_t>((i/13+i/797)%5);
        std::vector<std::uint8_t> original(frame.pixels().size()*4), rgba, scratch;
        for (std::size_t i=0;i<original.size();++i)
            original[i]=static_cast<std::uint8_t>(i*73+i/17);
        for (std::uint8_t level : {1,2,3}) {
            std::vector<double> times;
            for (unsigned run=0;run<70;++run) {
                rgba=original;
                const auto start=std::chrono::steady_clock::now();
                apply_chromatic_aberration(frame,rgba,scratch,level);
                const auto stop=std::chrono::steady_clock::now();
                if (run>=10) times.push_back(std::chrono::duration<double,std::micro>(stop-start).count());
            }
            std::sort(times.begin(),times.end());
            std::uint64_t hash=14695981039346656037ULL;
            for (auto b:rgba) { hash^=b; hash*=1099511628211ULL; }
            std::cout << "scale=" << scale << " level=" << unsigned(level)
                << " median_us=" << times[30] << " p95_us=" << times[57]
                << " hash=" << hash << '\n';
        }
    }
}
