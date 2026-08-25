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

} // namespace starfox::timing

