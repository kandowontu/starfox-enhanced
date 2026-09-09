// Standalone CPU microbenchmark; build with palette.cpp and row_workers.cpp.
#include "starfox/render/palette.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

int main() {
    using namespace starfox::render;
    Palette256 palette{};
    for (std::size_t i = 0; i < palette.size(); ++i)
        palette[i] = {static_cast<std::uint8_t>(i),
            static_cast<std::uint8_t>(i * 37),
            static_cast<std::uint8_t>(i * 91), static_cast<std::uint8_t>(i * 13)};
    RowWorkers workers;
    for (auto width : {256U, 796U}) {
        for (auto scale : {1U, 2U, 4U}) {
            Framebuffer frame(width, 224U, scale);
            for (std::size_t i = 0; i < frame.pixels().size(); ++i)
                frame.pixels()[i] = static_cast<std::uint8_t>(i * 73 + i / 97);
            for (auto count : {1U, 16U, 255U, 256U}) {
                const std::span<const Rgba8> colours{palette.data(), count};
                std::vector<std::uint8_t> expected, output;
                expand_rgba(frame, expected, colours);
                expand_rgba(frame, output, colours, workers);
                if (expected != output) return 1;
            }
            for (bool threaded : {false, true}) {
                std::vector<std::uint8_t> output;
                std::vector<double> samples;
                for (int iteration = 0; iteration < 1100; ++iteration) {
                    const auto start = std::chrono::steady_clock::now();
                    if (threaded) expand_rgba(frame, output, palette, workers);
                    else expand_rgba(frame, output, palette);
                    const auto end = std::chrono::steady_clock::now();
                    if (iteration >= 100) samples.push_back(
                        std::chrono::duration<double, std::micro>(end-start).count());
                }
                std::sort(samples.begin(), samples.end());
                std::cout << width << "x224 @" << scale << " "
                    << (threaded ? "pool" : "serial") << " median_us="
                    << samples[500] << " p95_us=" << samples[950] << '\n';
            }
        }
    }
}
