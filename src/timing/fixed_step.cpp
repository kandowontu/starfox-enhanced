#include "starfox/timing/fixed_step.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace starfox::timing {
namespace {

constexpr std::uint64_t kNanosecondsPerSecond = 1'000'000'000ULL;
constexpr double kAnglePeriod = 65'536.0;

double interpolate_angle(std::uint16_t from, std::uint16_t to, double alpha) noexcept {
    auto delta = static_cast<std::int32_t>(to) - static_cast<std::int32_t>(from);
    if (delta > 32'767) {
        delta -= 65'536;
    } else if (delta < -32'768) {
        delta += 65'536;
    }

    auto value = static_cast<double>(from) + static_cast<double>(delta) * alpha;
    if (value < 0.0) {
        value += kAnglePeriod;
    } else if (value >= kAnglePeriod) {
        value -= kAnglePeriod;
    }
    return value;
}

double interpolate_word(std::int32_t from, std::int32_t to, double alpha) noexcept {
    auto delta = static_cast<std::int64_t>(to) - from;
    if (delta > 32'767) {
        delta -= 65'536;
    } else if (delta < -32'768) {
        delta += 65'536;
    }
    return static_cast<double>(from) + static_cast<double>(delta) * alpha;
}

} // namespace

FixedStepClock::FixedStepClock(
    std::uint32_t simulation_hz,
    duration maximum_frame_time)
    : simulation_hz_(simulation_hz), maximum_frame_time_(maximum_frame_time) {
    if (simulation_hz_ == 0 || simulation_hz_ > kNanosecondsPerSecond) {
        throw std::invalid_argument{"simulation_hz must be in [1, 1,000,000,000]"};
    }
    if (maximum_frame_time_ <= duration::zero()) {
        throw std::invalid_argument{"maximum_frame_time must be positive"};
    }
}

StepBatch FixedStepClock::advance(duration elapsed) {
    if (elapsed < duration::zero()) {
        elapsed = duration::zero();
    }

    const auto clamped = elapsed > maximum_frame_time_;
    elapsed = std::min(elapsed, maximum_frame_time_);

    const auto elapsed_count = static_cast<std::uint64_t>(elapsed.count());
    if (elapsed_count > std::numeric_limits<std::uint64_t>::max() / simulation_hz_) {
        throw std::overflow_error{"elapsed time is too large"};
    }

    phase_units_ += elapsed_count * simulation_hz_;
    const auto steps = phase_units_ / kNanosecondsPerSecond;
    phase_units_ %= kNanosecondsPerSecond;

    return {
        static_cast<std::uint32_t>(steps),
        static_cast<double>(phase_units_) / static_cast<double>(kNanosecondsPerSecond),
        clamped,
    };
}

void FixedStepClock::reset() noexcept {
    phase_units_ = 0;
}

std::uint32_t FixedStepClock::simulation_hz() const noexcept {
    return simulation_hz_;
}

FixedStepClock::duration FixedStepClock::step_duration() const noexcept {
    return duration{static_cast<duration::rep>(kNanosecondsPerSecond / simulation_hz_)};
}

RenderTransform interpolate(
    const TransformSnapshot& previous,
    const TransformSnapshot& current,
    double alpha) noexcept {
    alpha = std::clamp(alpha, 0.0, 1.0);
    return {
        interpolate_word(previous.x, current.x, alpha),
        interpolate_word(previous.y, current.y, alpha),
        interpolate_word(previous.z, current.z, alpha),
        interpolate_angle(previous.pitch, current.pitch, alpha),
        interpolate_angle(previous.yaw, current.yaw, alpha),
        interpolate_angle(previous.roll, current.roll, alpha),
    };
}

} // namespace starfox::timing
