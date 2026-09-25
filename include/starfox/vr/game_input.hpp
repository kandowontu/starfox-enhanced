#pragma once
#include "starfox/vr/openxr_input.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/vr/menu_stick.hpp"
#include <cmath>
namespace starfox::vr {
// Produces the same SNES-button TickInput consumed by GameSimulation. Native
// control type/inversion remains the game's responsibility. Default type A:
// Y fire, A bomb, X boost, B brake, L/R roll. Retains quick taps between ticks.
class VrGameInput {
public:
    void sample(const VrControls& controls,bool menu_navigation=false) noexcept {
        input::ButtonMask buttons{};
        const auto add=[&](bool held,input::Button button) {if(held) buttons|=button;};
        add(controls.fire,input::y);add(controls.bomb,input::a);
        add(controls.boost,input::x);add(controls.brake,input::b);
        add(controls.roll_left,input::left_shoulder);add(controls.roll_right,input::right_shoulder);
        // Do not turn a held menu at startup/focus regain into a new Start.
        if(!controls.menu) menu_held_=false;
        if(controls.menu_pressed) menu_held_=true;
        add(menu_held_ && controls.menu,input::start);
        if(!controls.select) select_held_=false;
        if(controls.select_pressed) select_held_=true;
        add(select_held_ && controls.select,input::select);
        if(menu_navigation) buttons|=menu_stick_.sample(controls.steer.x,controls.steer.y);
        else {
            menu_stick_.reset();
            if(std::isfinite(controls.steer.x)) {
                add(controls.steer.x<-.35F,input::left);add(controls.steer.x>.35F,input::right);
            }
            if(std::isfinite(controls.steer.y)) {
                add(controls.steer.y<-.35F,input::down);add(controls.steer.y>.35F,input::up);
            }
        }
        latch_.sample(buttons);
    }
    input::TickInput consume() noexcept {return latch_.consume();}
    void reset() noexcept {latch_.reset();menu_stick_.reset();menu_held_=select_held_=false;}
private:
    input::InputLatch latch_;
    MenuStick menu_stick_;
    bool menu_held_{},select_held_{};
};
}
