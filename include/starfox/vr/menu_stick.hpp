#pragma once
#include "starfox/input/buttons.hpp"
#include <cmath>
namespace starfox::vr {
// One cardinal gesture per neutral-to-deflected movement. Gameplay retains
// diagonals; menus must not change both the selected row and its value.
class MenuStick {
    input::ButtonMask direction_{};
public:
    input::ButtonMask sample(float x,float y) noexcept {
        if(!std::isfinite(x) || !std::isfinite(y)) {reset();return 0;}
        const auto ax=std::abs(x),ay=std::abs(y);
        if(ax<=.25F && ay<=.25F) {reset();return 0;}
        if(direction_) return direction_;
        if(ax>.5F && ax>ay*1.15F) direction_=x>0?input::right:input::left;
        else if(ay>.5F && ay>ax*1.15F) direction_=y>0?input::up:input::down;
        return direction_;
    }
    void reset() noexcept {direction_=0;}
};
}
