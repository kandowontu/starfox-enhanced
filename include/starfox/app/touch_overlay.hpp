#pragma once

#include "starfox/input/buttons.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace starfox::app {

struct TouchPoint {
    float x{};
    float y{};
};

struct TouchRect {
    float left{};
    float top{};
    float right{};
    float bottom{};

    [[nodiscard]] bool contains(float x, float y) const noexcept {
        return x >= left && x <= right && y >= top && y <= bottom;
    }
};

// Coordinates are window points, not cartridge pixels. Both the SDL overlay
// and normalized finger events use this layout, independent of render upscale
// and the game's letterboxed logical presentation.
struct TouchOverlayLayout {
    float width{};
    float height{};
    float unit{};
    TouchPoint dpad{};
    std::array<TouchPoint, 4> actions{}; // A, B, X, Y
    std::array<TouchRect, 2> shoulders{}; // L, R
    std::array<TouchRect, 2> system{}; // Select, Start

    [[nodiscard]] static TouchOverlayLayout make(
        float width, float height, TouchRect safe) noexcept {
        TouchOverlayLayout result;
        result.width = std::max(width, 1.0F);
        result.height = std::max(height, 1.0F);
        safe.left = std::clamp(safe.left, 0.0F, result.width);
        safe.right = std::clamp(safe.right, 0.0F, result.width);
        safe.top = std::clamp(safe.top, 0.0F, result.height);
        safe.bottom = std::clamp(safe.bottom, 0.0F, result.height);
        if (safe.right - safe.left < result.width * 0.55F
            || safe.bottom - safe.top < result.height * 0.55F) {
            safe = {0.0F, 0.0F, result.width, result.height};
        }
        const auto available_width = safe.right - safe.left;
        const auto available_height = safe.bottom - safe.top;
        result.unit = std::clamp(std::min(available_width * 0.045F,
            available_height * 0.072F), 20.0F, 35.0F);
        const auto margin = std::max(6.0F, result.unit * 0.28F);
        const auto row = safe.bottom - margin - result.unit * 3.2F;
        result.dpad = {safe.left + margin + result.unit * 3.0F, row};
        const TouchPoint action_centre{
            safe.right - margin - result.unit * 3.2F, row};
        const auto step = result.unit * 2.2F;
        result.actions = {{{action_centre.x + step, action_centre.y},
            {action_centre.x, action_centre.y + step},
            {action_centre.x, action_centre.y - step},
            {action_centre.x - step, action_centre.y}}};
        const auto shoulder_top = safe.top + margin;
        const auto shoulder_bottom = shoulder_top + result.unit * 1.4F;
        result.shoulders = {{{safe.left + margin, shoulder_top,
            safe.left + margin + result.unit * 2.7F, shoulder_bottom},
            {safe.right - margin - result.unit * 2.7F, shoulder_top,
                safe.right - margin, shoulder_bottom}}};
        const auto mid = (safe.left + safe.right) * 0.5F;
        const auto button_top = safe.bottom - margin - result.unit * 1.1F;
        const auto button_bottom = safe.bottom - margin;
        result.system = {{{mid - result.unit * 2.8F, button_top,
            mid - result.unit * 0.4F, button_bottom},
            {mid + result.unit * 0.4F, button_top,
                mid + result.unit * 2.8F, button_bottom}}};
        return result;
    }

    [[nodiscard]] input::ButtonMask hit_test(float normalized_x,
        float normalized_y) const noexcept {
        using namespace starfox::input;
        const auto x = normalized_x * width;
        const auto y = normalized_y * height;
        auto result = ButtonMask{};
        if (shoulders[0].contains(x, y)) result |= left_shoulder;
        if (shoulders[1].contains(x, y)) result |= right_shoulder;
        if (system[0].contains(x, y)) result |= select;
        if (system[1].contains(x, y)) result |= start;
        const auto dx = x - dpad.x;
        const auto dy = y - dpad.y;
        if (std::abs(dx) <= unit * 3.0F
            && std::abs(dy) <= unit * 3.0F) {
            if (dx < -unit * 0.45F) result |= left;
            if (dx > unit * 0.45F) result |= right;
            if (dy < -unit * 0.45F) result |= up;
            if (dy > unit * 0.45F) result |= down;
        }
        constexpr std::array<ButtonMask, 4> buttons{a, b, input::x, input::y};
        for (std::size_t i = 0; i < actions.size(); ++i) {
            const auto ax = x - actions[i].x;
            const auto ay = y - actions[i].y;
            if (ax * ax + ay * ay <= unit * unit * 1.32F)
                result |= buttons[i];
        }
        return result;
    }
};

} // namespace starfox::app
