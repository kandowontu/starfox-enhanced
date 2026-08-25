#include "starfox/timing/fixed_step.hpp"
#include "starfox/input/input_latch.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void test_exact_60_to_20_cadence() {
    using namespace std::chrono_literals;
    starfox::timing::FixedStepClock clock;
    std::uint32_t total_steps = 0;

    for (std::uint32_t frame = 0; frame < 60; ++frame) {
        const auto elapsed = 16'666'666ns + (frame < 40 ? 1ns : 0ns);
        const auto batch = clock.advance(elapsed);
        require(batch.simulation_steps <= 1, "60 Hz presentation ran multiple 20 Hz steps");
        total_steps += batch.simulation_steps;
    }

    require(total_steps == 20, "one second must produce exactly 20 gameplay steps");
}

void test_stall_is_bounded() {
    using namespace std::chrono_literals;
    starfox::timing::FixedStepClock clock;
    const auto batch = clock.advance(2s);
    require(batch.time_was_clamped, "long frame was not reported as clamped");
    require(batch.simulation_steps == 5, "250 ms clamp must produce five 20 Hz steps");
}

void test_negative_time_is_ignored() {
    using namespace std::chrono_literals;
    starfox::timing::FixedStepClock clock;
    const auto batch = clock.advance(-10ms);
    require(batch.simulation_steps == 0, "negative elapsed time advanced gameplay");
    require(batch.interpolation_alpha == 0.0, "negative time altered interpolation");
}

void test_interpolation_does_not_modify_snapshots() {
    const starfox::timing::TransformSnapshot previous{0, 100, -100, 65'000, 0, 0};
    const starfox::timing::TransformSnapshot current{100, 300, 100, 500, 0, 0};
    const auto rendered = starfox::timing::interpolate(previous, current, 0.5);

    require(rendered.x == 50.0 && rendered.y == 200.0 && rendered.z == 0.0,
            "linear transform interpolation is incorrect");
    require(std::abs(rendered.pitch - 65'518.0) < 0.001,
            "16-bit angle interpolation did not take the shortest path");
    require(previous.x == 0 && current.x == 100,
            "render interpolation changed simulation snapshots");
}

void test_coordinate_interpolation_wraps_like_source_words() {
    const starfox::timing::TransformSnapshot previous{32'700, 0, 0, 0, 0, 0};
    const starfox::timing::TransformSnapshot current{-32'700, 0, 0, 0, 0, 0};
    const auto rendered = starfox::timing::interpolate(previous, current, 0.5);
    require(std::abs(rendered.x - 32'768.0) < 0.001,
            "16-bit world-coordinate interpolation crossed the long arc");
}

void test_invalid_frequency_is_rejected() {
    bool threw = false;
    try {
        [[maybe_unused]] starfox::timing::FixedStepClock clock{0};
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "zero simulation frequency was accepted");
}

void test_input_edges_survive_between_ticks() {
    constexpr starfox::input::ButtonMask fire = 1U << 7;
    starfox::input::InputLatch input;
    input.sample(fire);
    input.sample(0);

    const auto tick = input.consume();
    require(tick.held == 0, "released input was reported as held");
    require((tick.pressed & fire) != 0, "short input press was lost between ticks");
    require((tick.released & fire) != 0, "short input release was lost between ticks");

    const auto next_tick = input.consume();
    require(next_tick.pressed == 0 && next_tick.released == 0,
            "input edges were consumed more than once");
}

} // namespace

int main() {
    test_exact_60_to_20_cadence();
    test_stall_is_bounded();
    test_negative_time_is_ignored();
    test_interpolation_does_not_modify_snapshots();
    test_coordinate_interpolation_wraps_like_source_words();
    test_invalid_frequency_is_rejected();
    test_input_edges_survive_between_ticks();
    std::cout << "All timing tests passed.\n";
    return 0;
}
