#pragma once

#include <array>
#include <cstdint>
#include <limits>

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
    void sample(ButtonMask held, ButtonMask event_pressed = 0,
        ButtonMask event_released = 0) noexcept {
        // Recover complete taps between presentations. Normal held-state
        // transitions still own edges when any binding holds the action;
        // this avoids a second device's tap retriggering an already-held key.
        const auto taps = static_cast<ButtonMask>(event_pressed & event_released
            & ~held & ~last_sample_);
        retain(pressed_, static_cast<ButtonMask>(taps | (held & ~last_sample_)));
        retain(released_, static_cast<ButtonMask>(taps | (last_sample_ & ~held)));
        held_ = held;
        last_sample_ = held;
    }

    [[nodiscard]] TickInput consume() noexcept {
        return {held_, take(pressed_), take(released_)};
    }

    void reset(ButtonMask held = 0) noexcept {
        held_ = held;
        last_sample_ = held;
        pressed_.fill(0);
        released_.fill(0);
    }

private:
    ButtonMask held_{};
    ButtonMask last_sample_{};
    // TickInput can represent one edge per button. Retain additional edges
    // for subsequent updates instead of merging repeated presses into a bit.
    // Held state always reflects the latest physical sample, never the queue.
    using EdgeCounts = std::array<std::uint32_t, 16>;
    EdgeCounts pressed_{};
    EdgeCounts released_{};

    static void retain(EdgeCounts& counts, ButtonMask edges) noexcept {
        for (unsigned bit = 0; bit < counts.size(); ++bit) {
            if ((edges & (1U << bit)) != 0U
                && counts[bit] != std::numeric_limits<std::uint32_t>::max()) ++counts[bit];
        }
    }

    static ButtonMask take(EdgeCounts& counts) noexcept {
        ButtonMask edges{};
        for (unsigned bit = 0; bit < counts.size(); ++bit) {
            if (counts[bit] == 0U) continue;
            --counts[bit];
            edges = static_cast<ButtonMask>(edges | (1U << bit));
        }
        return edges;
    }
};

} // namespace starfox::input
