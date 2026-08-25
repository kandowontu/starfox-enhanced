#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape_decoder.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "usage: starfox_shape_coverage ROM SYMBOLS\n";
            return 2;
        }
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const starfox::assets::ShapeDecoder decoder{rom, symbols};

        std::set<std::uint32_t> seen_headers;
        std::vector<std::tuple<std::uint32_t, std::string, std::string>> failures;
        std::size_t decoded = 0;
        std::size_t static_shapes = 0;
        std::size_t animated_shapes = 0;
        std::size_t bsp_shapes = 0;
        for (const auto& [name, addresses] : symbols.entries()) {
            for (const auto address : addresses) {
                if (!seen_headers.insert(address).second || !decoder.looks_like_shape_header(address)) {
                    continue;
                }
                try {
                    const auto shape = decoder.decode(address, name);
                    ++decoded;
                    if (shape.frames.size() > 1) {
                        ++animated_shapes;
                    } else {
                        ++static_shapes;
                    }
                    bsp_shapes += !shape.bsp_nodes.empty();
                } catch (const std::exception& error) {
                    failures.emplace_back(address, name, error.what());
                }
            }
        }
        std::sort(failures.begin(), failures.end());

        std::map<std::string, std::size_t> failure_reasons;
        for (const auto& [address, name, reason] : failures) {
            static_cast<void>(address);
            static_cast<void>(name);
            ++failure_reasons[reason];
        }

        std::cout << "decoded headers: " << decoded << '\n'
                  << "static shapes:   " << static_shapes << '\n'
                  << "animated shapes: " << animated_shapes << '\n'
                  << "BSP shapes:      " << bsp_shapes << '\n'
                  << "unsupported:     " << failures.size() << '\n';
        for (const auto& [reason, count] : failure_reasons) {
            std::cout << "  " << std::setw(4) << count << "  " << reason << '\n';
        }
        if (!failures.empty()) {
            std::cout << "first unsupported headers:\n";
            const auto shown = std::min<std::size_t>(failures.size(), 20);
            for (std::size_t index = 0; index < shown; ++index) {
                const auto& [address, name, reason] = failures[index];
                std::cout << "  $" << std::hex << std::setw(6) << std::setfill('0') << address
                          << std::dec << std::setfill(' ') << "  " << name << "  " << reason << '\n';
            }
        }
        return failures.empty() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "shape coverage failed: " << error.what() << '\n';
        return 2;
    }
}
