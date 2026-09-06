#pragma once

#include <cstdint>

namespace starfox::input {

using ButtonMask = std::uint16_t;

struct TickInput {
    ButtonMask held{};
    ButtonMask pressed{};
    ButtonMask released{};
};

// Presentation may poll a PC controller three times for every gameplay tick.
// This latch retains transitions until the 20 Hz simulation consumes them.
class InputLatch {
public:
    void sample(ButtonMask held) noexcept {
        pressed_ |= static_cast<ButtonMask>(held & ~last_sample_);
        released_ |= static_cast<ButtonMask>(last_sample_ & ~held);
        held_ = held;
        last_sample_ = held;
    }

    [[nodiscard]] TickInput consume() noexcept {
        const TickInput result{held_, pressed_, released_};
        pressed_ = 0;
        released_ = 0;
        return result;
    }

    void reset(ButtonMask held = 0) noexcept {
        held_ = held;
        last_sample_ = held;
        pressed_ = 0;
        released_ = 0;
    }

private:
    ButtonMask held_{};
    ButtonMask last_sample_{};
    ButtonMask pressed_{};
    ButtonMask released_{};
};

} // namespace starfox::input

