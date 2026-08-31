#include "starfox/timing/fixed_step.hpp"
#include "starfox/input/input_latch.hpp"
#include "starfox/render/presentation_history.hpp"
#include "starfox/simulation/math.hpp"

#include <chrono>
#include <algorithm>
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
    constexpr std::array<std::uint32_t, 8> rates{
        20U, 30U, 60U, 90U, 120U, 240U, 360U, 480U};
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

    starfox::timing::RasterPhaseClock very_high_rate;
    const auto first_480 = very_high_rate.advance(480U);
    require(first_480.video_phases == 0U
                && std::abs(first_480.phase_fraction - 1.0 / 8.0) < 0.000001,
            "480 FPS did not expose fractional interpolation progress");
    for (std::uint32_t frame = 1; frame < 8U; ++frame) {
        static_cast<void>(very_high_rate.advance(480U));
    }
    const auto ninth_480 = very_high_rate.advance(480U);
    require(ninth_480.video_phases == 0U
                && std::abs(ninth_480.phase_fraction - 1.0 / 8.0) < 0.000001,
            "480 FPS cadence did not repeat after one raster phase");
}

void test_fast_forward_multipliers_preserve_exact_raster_pace() {
    constexpr std::array<std::uint32_t, 8> rates{
        20U, 30U, 60U, 90U, 120U, 240U, 360U, 480U};
    for (const auto multiplier : {2U, 3U, 5U}) {
        for (const auto rate : rates) {
            starfox::timing::RasterPhaseClock clock;
            std::uint32_t phases = 0U;
            double final_fraction = -1.0;
            for (std::uint32_t frame = 0; frame < rate; ++frame) {
                const auto batch = clock.advance(rate, multiplier);
                phases += batch.video_phases;
                final_fraction = batch.phase_fraction;
            }
            require(phases == starfox::timing::kPresentationHz * multiplier,
                    "fast-forward did not produce its exact raster rate");
            require(final_fraction == 0.0,
                    "fast-forward accumulated fractional drift");
        }
    }
    require(starfox::timing::playback_speed_multiplier(false, true, true) == 1U
                && starfox::timing::playback_speed_multiplier(true, false, true)
                    == 2U
                && starfox::timing::playback_speed_multiplier(true, true, false)
                    == 3U
                && starfox::timing::playback_speed_multiplier(true, true, true)
                    == 5U,
            "Tab modifier combination selected the wrong playback speed");
}

void test_frame_step_clock_synchronizes_fractional_phase() {
    starfox::timing::RasterPhaseClock clock;
    clock.synchronize(0.5);
    const auto batch = clock.advance(120U);
    require(batch.video_phases == 1U && batch.phase_fraction == 0.0,
            "frame-step clock did not continue from the displayed phase");

    clock.synchronize(0.99);
    const auto near_boundary = clock.advance(120U);
    require(near_boundary.video_phases == 1U,
            "frame-step synchronization wrapped a near-complete raster phase");
}

void test_frame_debug_never_steps_multiple_native_rasters() {
    require(starfox::timing::frame_debug_presentation_hz(20U) == 60U
            && starfox::timing::frame_debug_presentation_hz(30U) == 60U,
            "low output rate was allowed to skip native debugger frames");
    require(starfox::timing::frame_debug_presentation_hz(120U) == 120U,
            "high-rate debugger discarded interpolated presentation frames");

    starfox::timing::RasterPhaseClock clock;
    const auto low_rate_step = clock.advance(
        starfox::timing::frame_debug_presentation_hz(20U));
    require(low_rate_step.video_phases == 1U,
            "20 FPS debugger press advanced a complete 20 Hz logic interval");
}

void test_presentation_history_walks_without_changing_live_edge() {
    starfox::render::PresentationHistory history{24U};
    const std::array<std::uint8_t, 8> first{
        1U, 1U, 1U, 255U, 2U, 2U, 2U, 255U};
    const std::array<std::uint8_t, 8> second{
        3U, 3U, 3U, 255U, 4U, 4U, 4U, 255U};
    const std::array<std::uint8_t, 8> third{
        5U, 5U, 5U, 255U, 6U, 6U, 6U, 255U};
    history.record(2U, 1U, first);
    history.record(2U, 1U, second);
    history.record(2U, 1U, third);
    require(history.frame_count() == 3U && history.at_live(),
            "presentation history did not retain its live frame");
    require(history.step_back()
                && std::equal(history.current()->rgba.begin(),
                    history.current()->rgba.end(), second.begin(), second.end()),
            "F7 history step did not select the preceding presentation");
    require(history.step_forward()
                && std::equal(history.current()->rgba.begin(),
                    history.current()->rgba.end(), third.begin(), third.end())
                && history.at_live(),
            "F6 history step did not return to the live presentation");

    const std::array<std::uint8_t, 12> resized{};
    history.record(3U, 1U, resized);
    require(history.frame_count() == 1U && history.current()->width == 3U,
            "display-size change did not reset incompatible frame history");
}

void test_presentation_history_compresses_and_rewinds_many_frames() {
    // A moving low-colour scene changes a small number of RGBA pixels per
    // presentation. It should retain far more than the old full-frame ring
    // could fit in the same memory budget while remaining exactly reversible.
    starfox::render::PresentationHistory history{4'096U, 512U};
    std::array<std::uint8_t, 256> frame{};
    for (std::uint16_t index = 0U; index < 256U; ++index) {
        frame[index] = static_cast<std::uint8_t>(index);
        history.record(8U, 8U, frame);
    }
    require(history.frame_count() == 256U,
            "compressed debugger history did not retain a useful rewind span");
    for (std::uint16_t index = 255U; index != 0U; --index) {
        require(history.current()->rgba[index] == static_cast<std::uint8_t>(index),
                "rewind history current frame was corrupted");
        require(history.step_back(),
                "rewind history ended before the retained frame floor");
        require(history.current()->rgba[index] == 0U,
                "reverse delta did not restore the exact previous frame");
    }
    history.to_live();
    require(history.at_live() && history.current()->rgba[255U] == 255U,
            "rewind history could not restore its exact live edge");

    std::array<std::uint8_t, 256> uniform_a{};
    std::array<std::uint8_t, 256> uniform_b{};
    uniform_a.fill(0x31U);
    uniform_b.fill(0xc7U);
    starfox::render::PresentationHistory repeated_delta{64U, 8U};
    repeated_delta.record(8U, 8U, uniform_a);
    repeated_delta.record(8U, 8U, uniform_b);
    require(repeated_delta.step_back()
                && std::equal(repeated_delta.current()->rgba.begin(),
                    repeated_delta.current()->rgba.end(), uniform_a.begin()),
            "repeated-pixel reverse delta did not restore an exact frame");
    require(repeated_delta.step_forward()
                && std::equal(repeated_delta.current()->rgba.begin(),
                    repeated_delta.current()->rgba.end(), uniform_b.begin()),
            "repeated-pixel forward delta did not restore an exact frame");
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

void test_live_fps_counter_reports_actual_output_and_lag() {
    using namespace std::chrono_literals;
    using Counter = starfox::timing::LiveFpsCounter;
    Counter counter{250ms};
    const Counter::time_point start{};
    counter.reset(start, 60U);

    for (std::uint32_t frame = 1U; frame <= 15U; ++frame) {
        counter.record_frame(start + 250ms * frame / 15U);
    }
    require(counter.fps() == 60U,
            "live FPS counter did not report sixty completed presentations");

    for (std::uint32_t frame = 1U; frame <= 10U; ++frame) {
        counter.record_frame(start + 250ms + 250ms * frame / 10U);
    }
    require(counter.fps() == 40U,
            "live FPS counter hid a forty-FPS lag interval behind the target rate");
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

void test_rotation_matrix_interpolation_is_orthonormal() {
    constexpr starfox::simulation::MatrixQ15 identity{
        32'767, 0, 0, 0, 32'767, 0, 0, 0, 32'767};
    constexpr starfox::simulation::MatrixQ15 quarter_turn{
        0, -32'767, 0, 32'767, 0, 0, 0, 0, 32'767};
    const auto halfway = starfox::simulation::interpolate_rotation_matrix_q15(
        identity, quarter_turn, 0.5);
    const auto dot_columns = [&halfway](std::size_t left, std::size_t right) {
        std::int64_t result{};
        for (std::size_t row = 0; row < 3U; ++row) {
            result += static_cast<std::int64_t>(halfway[row * 3U + left])
                * halfway[row * 3U + right];
        }
        return result;
    };
    const auto unit = 32'768LL * 32'768LL;
    require(std::abs(dot_columns(0U, 0U) - unit) < 100'000LL
                && std::abs(dot_columns(1U, 1U) - unit) < 100'000LL
                && std::abs(dot_columns(2U, 2U) - unit) < 100'000LL
                && std::abs(dot_columns(0U, 1U)) < 100'000LL,
            "matrix interpolation introduced rotation scale or shear");
    require(halfway[0] > 23'000 && halfway[1] < -23'000
                && halfway[3] > 23'000 && halfway[4] > 23'000,
            "matrix interpolation did not follow the halfway rotation");
    require(starfox::simulation::interpolate_rotation_matrix_q15(
                identity, quarter_turn, 0.0) == identity
                && starfox::simulation::interpolate_rotation_matrix_q15(
                    identity, quarter_turn, 1.0) == quarter_turn,
            "matrix interpolation changed exact source-frame endpoints");
}

void test_camera_cuts_are_not_interpolated() {
    const starfox::timing::TransformSnapshot scramble_camera{
        0, -24, 10'625, 0, 0, 0};
    const starfox::timing::TransformSnapshot exit_base_camera{
        -400, -145, 1'000, 0, 0, 0};
    require(starfox::timing::camera_transform_is_discontinuous(
                scramble_camera, exit_base_camera),
            "scramble-to-ExitBase camera replacement was treated as motion");

    const starfox::timing::TransformSnapshot near_word_wrap{
        32'700, 0, 0, 0, 0, 0};
    const starfox::timing::TransformSnapshot after_word_wrap{
        -32'700, 0, 0, 0, 0, 0};
    require(!starfox::timing::camera_transform_is_discontinuous(
                near_word_wrap, after_word_wrap),
            "short camera motion across the source-word wrap became a cut");
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
    test_fast_forward_multipliers_preserve_exact_raster_pace();
    test_frame_step_clock_synchronizes_fractional_phase();
    test_frame_debug_never_steps_multiple_native_rasters();
    test_presentation_history_walks_without_changing_live_edge();
    test_presentation_history_compresses_and_rewinds_many_frames();
    test_missed_render_target_preserves_realtime_raster_pace();
    test_live_fps_counter_reports_actual_output_and_lag();
    test_stall_is_bounded();
    test_negative_time_is_ignored();
    test_interpolation_does_not_modify_snapshots();
    test_coordinate_interpolation_wraps_like_source_words();
    test_rotation_matrix_interpolation_is_orthonormal();
    test_camera_cuts_are_not_interpolated();
    test_invalid_frequency_is_rejected();
    test_input_edges_survive_between_ticks();
    std::cout << "All timing tests passed.\n";
    return 0;
}
