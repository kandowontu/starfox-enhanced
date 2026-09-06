#pragma once

#include <cstdint>

namespace starfox::simulation {

// Shared event latch between the raster clock and the bus owner. The raster
// raises requests at bus-operation boundaries; transfers run at DMA edges.
struct SnesHdmaState {
    std::uint8_t enabled{}, completed{}, transfer{};
    bool pending{}, run{};

    void setup() noexcept {
        completed = transfer = 0;
        if (enabled) { pending = true; run = false; }
    }
    void scanline() noexcept {
        if (enabled & std::uint8_t(~completed)) { pending = true; run = true; }
    }
};

} // namespace starfox::simulation
