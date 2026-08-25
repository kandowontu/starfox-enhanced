#pragma once

#include "starfox/input/input_latch.hpp"

namespace starfox::input {

// Native input uses the original SNES joypad bit layout so strategy code can
// consume it without a translation layer.
enum Button : ButtonMask {
    b = 1U << 15U,
    y = 1U << 14U,
    select = 1U << 13U,
    start = 1U << 12U,
    up = 1U << 11U,
    down = 1U << 10U,
    left = 1U << 9U,
    right = 1U << 8U,
    a = 1U << 7U,
    x = 1U << 6U,
    left_shoulder = 1U << 5U,
    right_shoulder = 1U << 4U,
};

} // namespace starfox::input
