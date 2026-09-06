// Development-only comparison against unmodified pinned Ares counter code.
#include "starfox/simulation/snes_timeline.hpp"
#include <nall/platform.hpp>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace oracle {
using u32 = std::uint32_t;
class serializer;
struct Region {
    static inline bool ntsc = true;
    static bool NTSC() { return ntsc; }
    static bool PAL() { return !ntsc; }
};
struct {
    bool value{};
    bool interlace() const { return value; }
} ppu;
#include <ares/sfc/ppu/counter/counter.hpp>
#include <ares/sfc/ppu/counter/inline.hpp>
}

int main() try {
    using namespace starfox::simulation;
    std::uint64_t comparisons = 0;
    for (const auto region : {SnesRegion::ntsc, SnesRegion::pal}) {
        for (unsigned policy = 0; policy < 3U; ++policy) {
            oracle::Region::ntsc = region == SnesRegion::ntsc;
            oracle::PPUcounter source;
            source.reset();
            SnesRasterClock host{region};
            while (host.fields() < 8U) {
                // Policy 2 changes the live PPU setting on both sides of the
                // line-128 capture, including a late write that must wait.
                oracle::ppu.value = policy == 1U || (policy == 2U
                    && ((host.fields() & 1U) ? host.vertical() >= 129U : host.vertical() < 128U));
                source.tick();
                host.tick(oracle::ppu.value);
                if (host.field() != source.field() || host.interlace() != source.interlace()
                    || host.horizontal_period() != source.hperiod() || host.dot() != source.hdot())
                    throw std::runtime_error{"raster field/dot differs"};
                for (const auto delay : {0U, 2U, 6U, 10U}) {
                    if (host.horizontal(delay) != source.hcounter(delay)
                        || host.vertical(delay) != source.vcounter(delay))
                        throw std::runtime_error{"delayed beam differs"};
                    ++comparisons;
                }
            }
        }
    }
    std::cout << "48 NTSC/PAL fields, " << comparisons
        << " beam comparisons, zero differences against pinned Ares counter\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
