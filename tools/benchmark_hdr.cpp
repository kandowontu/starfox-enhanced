#include "starfox/render/hdr_effect.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

int main() {
    using namespace starfox::render;
    for (const auto scale : {1U, 2U, 4U}) {
        Framebuffer frame{796U, 224U, scale};
        frame.enable_layer_tags(true);
        std::vector<std::uint8_t> rgba(frame.pixels().size()*4U);
        for (std::size_t i=0; i<rgba.size(); ++i) rgba[i]=static_cast<std::uint8_t>(i*73);
        const auto original=rgba;
        std::vector<double> samples;
        for (unsigned run=0; run<550; ++run) {
            rgba=original;
            const auto start=std::chrono::steady_clock::now();
            apply_hdr_effect(frame,rgba,3);
            const auto end=std::chrono::steady_clock::now();
            if (run>=50) samples.push_back(std::chrono::duration<double,std::micro>(end-start).count());
        }
        std::sort(samples.begin(),samples.end());
        std::uint64_t hash=14695981039346656037ULL;
        for (auto value:rgba) { hash^=value; hash*=1099511628211ULL; }
        std::cout << scale << "x median_us=" << samples[250] << " p95_us="
            << samples[475] << " hash=" << hash << '\n';
    }
}
