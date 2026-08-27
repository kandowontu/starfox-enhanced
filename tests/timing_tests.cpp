#include "starfox/timing/fixed_step.hpp"
#include "starfox/input/input_latch.hpp"

#include <chrono>
#include <array>
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

void test_selectable_presentation_rates_preserve_raster_pace() {
    constexpr std::array<std::uint32_t, 6> rates{
        20U, 30U, 60U, 120U, 240U, 360U};
    for (const auto rate : rates) {
        starfox::timing::RasterPhaseClock clock;
        std::uint32_t phases = 0U;
        double final_fraction = -1.0;
        for (std::uint32_t frame = 0; frame < rate; ++frame) {
            const auto batch = clock.advance(rate);
            phases += batch.video_phases;
            final_fraction = batch.phase_fraction;
        }
        require(phases == starfox::timing::kPresentationHz,
                "presentation FPS changed the 60 Hz raster pace");
        require(final_fraction == 0.0,
                "presentation cadence accumulated fractional drift");
    }

    starfox::timing::RasterPhaseClock high_rate;
    const auto first_360 = high_rate.advance(360U);
    require(first_360.video_phases == 0U
                && std::abs(first_360.phase_fraction - 1.0 / 6.0) < 0.000001,
            "360 FPS did not expose fractional interpolation progress");
    for (std::uint32_t frame = 1; frame < 6U; ++frame) {
        static_cast<void>(high_rate.advance(360U));
    }
    const auto seventh_360 = high_rate.advance(360U);
    require(seventh_360.video_phases == 0U
                && std::abs(seventh_360.phase_fraction - 1.0 / 6.0) < 0.000001,
            "360 FPS cadence did not repeat after one raster phase");
}

void test_fast_forward_exactly_doubles_raster_pace() {
    constexpr std::array<std::uint32_t, 6> rates{
        20U, 30U, 60U, 120U, 240U, 360U};
    for (const auto rate : rates) {
        starfox::timing::RasterPhaseClock clock;
        std::uint32_t phases = 0U;
        double final_fraction = -1.0;
        for (std::uint32_t frame = 0; frame < rate; ++frame) {
            const auto batch = clock.advance(rate, 2U);
            phases += batch.video_phases;
            final_fraction = batch.phase_fraction;
        }
        require(phases == starfox::timing::kPresentationHz * 2U,
                "2x fast-forward did not produce exactly 120 raster phases");
        require(final_fraction == 0.0,
                "2x fast-forward accumulated fractional drift");
    }
}

void test_missed_render_target_preserves_realtime_raster_pace() {
    using namespace std::chrono_literals;
    starfox::timing::FixedStepClock realtime_raster{
        starfox::timing::kPresentationHz};
    std::uint32_t phases = 0U;
    // A requested 360 FPS mode may only render 200 actual frames on a given
    // machine. One elapsed second must still service all sixty source rasters.
    for (std::uint32_t frame = 0; frame < 200U; ++frame) {
        phases += realtime_raster.advance(5ms).simulation_steps;
    }
    require(phases == starfox::timing::kPresentationHz,
            "a missed render target slowed the realtime raster/audio clock");
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
    test_selectable_presentation_rates_preserve_raster_pace();
    test_fast_forward_exactly_doubles_raster_pace();
    test_missed_render_target_preserves_realtime_raster_pace();
    test_stall_is_bounded();
    test_negative_time_is_ignored();
    test_interpolation_does_not_modify_snapshots();
    test_coordinate_interpolation_wraps_like_source_words();
    test_invalid_frequency_is_rejected();
    test_input_edges_survive_between_ticks();
    std::cout << "All timing tests passed.\n";
    return 0;
}
