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

// Events from one presentation. Count complete action taps rather than ORing
// their edges together. Depth prevents overlapping bindings from manufacturing
// a second tap while the action is already down within this batch.
struct DigitalInputEvents {
    ButtonMask pressed{};
    ButtonMask released{};
    std::array<std::uint32_t, 16> complete_taps{};

    void record(ButtonMask buttons, bool down) noexcept {
        if (down) pressed = static_cast<ButtonMask>(pressed | buttons);
        else released = static_cast<ButtonMask>(released | buttons);
        for (unsigned bit = 0; bit < depth_.size(); ++bit) {
            if ((buttons & (1U << bit)) == 0U) continue;
            if (down) {
                if (depth_[bit] != std::numeric_limits<std::uint32_t>::max()) ++depth_[bit];
            } else if (depth_[bit] != 0U && --depth_[bit] == 0U) {
                if (complete_taps[bit] != std::numeric_limits<std::uint32_t>::max()) ++complete_taps[bit];
            }
        }
    }
private:
    std::array<std::uint32_t, 16> depth_{};
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

    void sample(ButtonMask held, const DigitalInputEvents& events) noexcept {
        const auto previously_held = last_sample_;
        sample(held, events.pressed, events.released);
        for (unsigned bit = 0; bit < events.complete_taps.size(); ++bit) {
            const auto button = 1U << bit;
            if ((previously_held & button) != 0U || events.complete_taps[bit] == 0U) continue;
            // The normal sample already retains one press. If the action
            // finishes down, that press belongs to the final held interval;
            // otherwise it belongs to the first complete tap.
            const auto extra = events.complete_taps[bit] - ((held & button) == 0U ? 1U : 0U);
            add(pressed_[bit], extra);
            add(released_[bit], extra);
        }
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

    static void add(std::uint32_t& count, std::uint32_t amount) noexcept {
        const auto available = std::numeric_limits<std::uint32_t>::max() - count;
        count += amount > available ? available : amount;
    }

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
