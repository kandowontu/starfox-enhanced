#pragma once

#include <chrono>
#include <cstdint>

namespace starfox::timing {

// The original NTSC game completes one gameplay/strategy update for every
// three display IRQ phases. Presentation may run at 60 Hz, but gameplay must
// remain at 20 Hz so map scripts, movement, damage, and audio cues retain their
// original pace.
inline constexpr std::uint32_t kSimulationHz = 20;
inline constexpr std::uint32_t kPresentationHz = 60;

// Tab accelerates the source clock by 100%; Ctrl and Shift raise that to the
// requested 200% and 400% boosts (2x, 3x and 5x total speed respectively).
[[nodiscard]] constexpr std::uint32_t playback_speed_multiplier(
    bool tab, bool control, bool shift) noexcept {
    if (!tab) return 1U;
    if (!control) return 2U;
    return shift ? 5U : 3U;
}

// Frame debugging must never collapse a keypress into a low-rate 20/30 FPS
// output interval (three/two cartridge rasters). Above the native raster rate,
// retain the selected interpolation cadence so every generated output frame
// remains individually inspectable.
[[nodiscard]] constexpr std::uint32_t frame_debug_presentation_hz(
    std::uint32_t selected_hz) noexcept {
    return selected_hz < kPresentationHz ? kPresentationHz : selected_hz;
}

struct RasterPhaseBatch {
    std::uint32_t video_phases{};
    double phase_fraction{};
};

// Converts an independently selected presentation rate into the cartridge's
// fixed 60 Hz raster clock. The supported presentation rates all divide a
// common 1440 Hz timebase, so this remains exact without wall-clock drift.
class RasterPhaseClock {
public:
    [[nodiscard]] RasterPhaseBatch advance(
        std::uint32_t presentation_hz,
        std::uint32_t speed_multiplier = 1U);
    void reset() noexcept;
    void synchronize(double phase_fraction) noexcept;

private:
    std::uint32_t subphase_units_{};
};

struct StepBatch {
    std::uint32_t simulation_steps{};
    double interpolation_alpha{};
    bool time_was_clamped{};
};

class FixedStepClock {
public:
    using duration = std::chrono::nanoseconds;

    explicit FixedStepClock(
        std::uint32_t simulation_hz = kSimulationHz,
        duration maximum_frame_time = std::chrono::milliseconds{250});

    [[nodiscard]] StepBatch advance(duration elapsed);
    void reset() noexcept;

    [[nodiscard]] std::uint32_t simulation_hz() const noexcept;
    [[nodiscard]] duration step_duration() const noexcept;

private:
    std::uint32_t simulation_hz_{};
    duration maximum_frame_time_{};
    std::uint64_t phase_units_{};
};

// Measures completed host presentations independently of the requested
// presentation rate. A short sample window makes missed-frame hot spots
// visible without allowing single-frame timing noise to make the readout
// illegible.
class LiveFpsCounter {
public:
    using clock = std::chrono::steady_clock;
    using duration = clock::duration;
    using time_point = clock::time_point;

    explicit LiveFpsCounter(
        duration sample_period = std::chrono::milliseconds{250});

    void reset(time_point now, std::uint32_t initial_fps = 0U) noexcept;
    void record_frame(time_point now) noexcept;
    [[nodiscard]] std::uint32_t fps() const noexcept { return fps_; }

private:
    duration sample_period_{};
    time_point sample_started_{};
    std::uint32_t sample_frames_{};
    std::uint32_t fps_{};
};

// A presentation-only transform. Fixed-point gameplay state should be copied
// into these snapshots after each 20 Hz tick. Interpolated values must never be
// written back into gameplay state.
struct TransformSnapshot {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
    std::uint16_t pitch{};
    std::uint16_t yaw{};
    std::uint16_t roll{};
};

struct RenderTransform {
    double x{};
    double y{};
    double z{};
    double pitch{};
    double yaw{};
    double roll{};
};

[[nodiscard]] RenderTransform interpolate(
    const TransformSnapshot& previous,
    const TransformSnapshot& current,
    double alpha) noexcept;

// Camera scripts occasionally replace the complete fixed-point view in one
// source update. Those are cuts, not unusually fast motion, and must not be
// blended across the extra host presentations.
[[nodiscard]] bool camera_transform_is_discontinuous(
    const TransformSnapshot& previous,
    const TransformSnapshot& current) noexcept;

} // namespace starfox::timing
