#include "starfox/timing/fixed_step.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
    using namespace std::chrono_literals;
    using starfox::timing::FixedStepClock;

    FixedStepClock clock;
    std::uint32_t total_steps = 0;

    // One exact second split across 60 presentation frames. The extra 40 ns
    // are distributed so this demonstration does not accumulate truncation.
    for (std::uint32_t frame = 0; frame < 60; ++frame) {
        const auto elapsed = 16'666'666ns + (frame < 40 ? 1ns : 0ns);
        const auto batch = clock.advance(elapsed);
        total_steps += batch.simulation_steps;
    }

    std::cout << "presentation frames: 60\n"
              << "simulation steps:    " << total_steps << "\n"
              << "expected game pace:  " << starfox::timing::kSimulationHz << " Hz\n";

    return total_steps == starfox::timing::kSimulationHz ? 0 : 1;
}

