#pragma once

#include "starfox/input/buttons.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <optional>

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

enum class TouchGroup : std::uint8_t {
    dpad, actions, left_shoulder, right_shoulder, select, start, count
};

struct TouchGroupTransform {
    float x{}; // Fraction of the safe-area width from the default position.
    float y{}; // Fraction of the safe-area height from the default position.
    float scale{1.0F};
};

struct TouchLayoutConfig {
    std::array<TouchGroupTransform,static_cast<std::size_t>(TouchGroup::count)> groups{};

    [[nodiscard]] TouchGroupTransform& operator[](TouchGroup group) noexcept {
        return groups[static_cast<std::size_t>(group)];
    }
    [[nodiscard]] const TouchGroupTransform& operator[](TouchGroup group) const noexcept {
        return groups[static_cast<std::size_t>(group)];
    }
};

// Coordinates are window points, not cartridge pixels. Both the SDL overlay
// and normalized finger events use this layout, independent of render upscale
// and the game's letterboxed logical presentation.
struct TouchOverlayLayout {
    float width{};
    float height{};
    float unit{};
    float dpad_unit{};
    float action_unit{};
    TouchRect safe{};
    TouchPoint dpad{};
    std::array<TouchPoint, 4> actions{}; // A, B, X, Y
    std::array<TouchRect, 2> shoulders{}; // L, R
    std::array<TouchRect, 2> system{}; // Select, Start

    [[nodiscard]] static TouchOverlayLayout make(
        float width, float height, TouchRect safe,
        const TouchLayoutConfig& config = {}) noexcept {
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
        result.safe = safe;
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
        const auto transform_point=[&](TouchPoint centre,TouchGroup group,
                                       float radius_x,float radius_y) {
            const auto& edit=config[group];
            const auto x=centre.x+std::clamp(edit.x,-1.0F,1.0F)*available_width;
            const auto y=centre.y+std::clamp(edit.y,-1.0F,1.0F)*available_height;
            centre.x=safe.left+radius_x<=safe.right-radius_x
                ?std::clamp(x,safe.left+radius_x,safe.right-radius_x)
                :(safe.left+safe.right)*.5F;
            centre.y=safe.top+radius_y<=safe.bottom-radius_y
                ?std::clamp(y,safe.top+radius_y,safe.bottom-radius_y)
                :(safe.top+safe.bottom)*.5F;
            return centre;
        };
        const auto scale=[&](TouchGroup group) {
            const auto value=config[group].scale;
            const auto radius=group==TouchGroup::dpad?6.0F
                :group==TouchGroup::actions?6.4F:0.0F;
            const auto fit=radius>0.0F
                ?std::min(2.0F,std::min(available_width,available_height)
                    /(radius*result.unit)) :2.0F;
            return std::min(std::isfinite(value)
                ?std::clamp(value,0.55F,2.0F):1.0F,fit);
        };
        result.dpad_unit=result.unit*scale(TouchGroup::dpad);
        result.dpad=transform_point(result.dpad,TouchGroup::dpad,
            result.dpad_unit*3.0F,result.dpad_unit*3.0F);
        result.action_unit=result.unit*scale(TouchGroup::actions);
        const auto face_centre=transform_point(action_centre,TouchGroup::actions,
            result.action_unit*3.2F,result.action_unit*3.2F);
        const auto face_step=result.action_unit*2.2F;
        result.actions={{{face_centre.x+face_step,face_centre.y},
            {face_centre.x,face_centre.y+face_step},
            {face_centre.x,face_centre.y-face_step},
            {face_centre.x-face_step,face_centre.y}}};
        const auto transform_rect=[&](TouchRect rect,TouchGroup group) {
            const auto factor=scale(group);
            const auto half_x=(rect.right-rect.left)*factor*.5F;
            const auto half_y=(rect.bottom-rect.top)*factor*.5F;
            const auto centre=transform_point({(rect.left+rect.right)*.5F,
                (rect.top+rect.bottom)*.5F},group,half_x,half_y);
            return TouchRect{centre.x-half_x,centre.y-half_y,
                centre.x+half_x,centre.y+half_y};
        };
        result.shoulders[0]=transform_rect(result.shoulders[0],TouchGroup::left_shoulder);
        result.shoulders[1]=transform_rect(result.shoulders[1],TouchGroup::right_shoulder);
        result.system[0]=transform_rect(result.system[0],TouchGroup::select);
        result.system[1]=transform_rect(result.system[1],TouchGroup::start);
        return result;
    }

    [[nodiscard]] TouchRect group_bounds(TouchGroup group) const noexcept {
        switch(group) {
        case TouchGroup::dpad:
            return {dpad.x-3*dpad_unit,dpad.y-3*dpad_unit,
                dpad.x+3*dpad_unit,dpad.y+3*dpad_unit};
        case TouchGroup::actions: {
            const auto centre=TouchPoint{(actions[0].x+actions[3].x)*.5F,
                (actions[1].y+actions[2].y)*.5F};
            return {centre.x-3.2F*action_unit,centre.y-3.2F*action_unit,
                centre.x+3.2F*action_unit,centre.y+3.2F*action_unit};
        }
        case TouchGroup::left_shoulder: return shoulders[0];
        case TouchGroup::right_shoulder: return shoulders[1];
        case TouchGroup::select: return system[0];
        case TouchGroup::start: return system[1];
        default: return {};
        }
    }

    [[nodiscard]] std::optional<TouchGroup> group_at(float x,float y) const noexcept {
        for(auto group:{TouchGroup::left_shoulder,TouchGroup::right_shoulder,
            TouchGroup::select,TouchGroup::start,TouchGroup::dpad,TouchGroup::actions})
            if(group_bounds(group).contains(x,y)) return group;
        return std::nullopt;
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
        if (std::abs(dx) <= dpad_unit * 3.0F
            && std::abs(dy) <= dpad_unit * 3.0F) {
            if (dx < -dpad_unit * 0.45F) result |= left;
            if (dx > dpad_unit * 0.45F) result |= right;
            if (dy < -dpad_unit * 0.45F) result |= up;
            if (dy > dpad_unit * 0.45F) result |= down;
        }
        constexpr std::array<ButtonMask, 4> buttons{a, b, input::x, input::y};
        for (std::size_t i = 0; i < actions.size(); ++i) {
            const auto ax = x - actions[i].x;
            const auto ay = y - actions[i].y;
            if (ax * ax + ay * ay <= action_unit * action_unit * 1.32F)
                result |= buttons[i];
        }
        return result;
    }
};

enum class TouchEditorButton : std::uint8_t { reset, cancel, apply };

[[nodiscard]] inline TouchRect touch_editor_button_rect(
    const TouchOverlayLayout& layout,TouchEditorButton button) noexcept {
    const auto width=std::clamp((layout.safe.right-layout.safe.left)*.15F,64.0F,100.0F);
    const auto gap=8.0F;
    const auto centre=(layout.safe.left+layout.safe.right)*.5F;
    const auto first=centre-(3*width+2*gap)*.5F;
    const auto left=first+static_cast<unsigned>(button)*(width+gap);
    const auto top=layout.safe.top+5.0F;
    return {left,top,left+width,top+std::max(32.0F,layout.unit*1.5F)};
}

// Pure window-point gesture state; the same transformed layout drives the
// rendered controls and hit testing. D-pad/face clusters each move and scale
// as a single group, never as four independently drifting buttons.
class TouchLayoutGesture {
public:
    void down(std::int64_t id,TouchPoint point,const TouchOverlayLayout& layout,
              const TouchLayoutConfig& config) noexcept {
        if(!primary_) {
            selected_=layout.group_at(point.x,point.y);
            if(!selected_) return;
            primary_=Finger{id,point};
            baseline_=config;
            first_start_=point;
        } else if(!secondary_ && primary_->id!=id) {
            secondary_=Finger{id,point};
            rebase(config);
        }
    }

    void move(std::int64_t id,TouchPoint point,const TouchOverlayLayout& layout,
              TouchLayoutConfig& config) noexcept {
        if(primary_ && primary_->id==id) primary_->point=point;
        else if(secondary_ && secondary_->id==id) secondary_->point=point;
        else return;
        if(!selected_ || !primary_) return;
        auto transform=baseline_[*selected_];
        const auto width=std::max(1.0F,layout.safe.right-layout.safe.left);
        const auto height=std::max(1.0F,layout.safe.bottom-layout.safe.top);
        if(secondary_) {
            const TouchPoint middle{(primary_->point.x+secondary_->point.x)*.5F,
                (primary_->point.y+secondary_->point.y)*.5F};
            const auto dx=primary_->point.x-secondary_->point.x;
            const auto dy=primary_->point.y-secondary_->point.y;
            const auto distance=std::sqrt(dx*dx+dy*dy);
            transform.x+=(middle.x-first_start_.x)/width;
            transform.y+=(middle.y-first_start_.y)/height;
            transform.scale*=distance/std::max(1.0F,first_distance_);
        } else {
            transform.x+=(primary_->point.x-first_start_.x)/width;
            transform.y+=(primary_->point.y-first_start_.y)/height;
        }
        transform.x=std::clamp(transform.x,-1.0F,1.0F);
        transform.y=std::clamp(transform.y,-1.0F,1.0F);
        transform.scale=std::clamp(transform.scale,0.55F,2.0F);
        // Keep the persisted offset in the same range as the visible group.
        // Otherwise dragging past a screen edge builds up invisible motion
        // that the next drag must undo before the controls move again.
        auto anchored=config;
        anchored[*selected_]={0.0F,0.0F,transform.scale};
        const auto base=TouchOverlayLayout::make(layout.width,layout.height,
            layout.safe,anchored).group_bounds(*selected_);
        transform.x=std::clamp(transform.x,
            std::max(-1.0F,(layout.safe.left-base.left)/width),
            std::min(1.0F,(layout.safe.right-base.right)/width));
        transform.y=std::clamp(transform.y,
            std::max(-1.0F,(layout.safe.top-base.top)/height),
            std::min(1.0F,(layout.safe.bottom-base.bottom)/height));
        config[*selected_]=transform;
    }

    void up(std::int64_t id,const TouchLayoutConfig& config) noexcept {
        if(primary_ && primary_->id==id) {
            primary_=secondary_;
            secondary_.reset();
        } else if(secondary_ && secondary_->id==id) secondary_.reset();
        if(primary_) rebase(config);
    }

    void clear() noexcept {primary_.reset();secondary_.reset();selected_.reset();}
    [[nodiscard]] std::optional<TouchGroup> selected() const noexcept {return selected_;}
    [[nodiscard]] bool dragging() const noexcept {return primary_.has_value();}

private:
    struct Finger {std::int64_t id;TouchPoint point;};
    void rebase(const TouchLayoutConfig& config) noexcept {
        baseline_=config;
        if(!primary_) return;
        first_start_=primary_->point;
        first_distance_=1.0F;
        if(secondary_) {
            first_start_={(primary_->point.x+secondary_->point.x)*.5F,
                (primary_->point.y+secondary_->point.y)*.5F};
            const auto dx=primary_->point.x-secondary_->point.x;
            const auto dy=primary_->point.y-secondary_->point.y;
            first_distance_=std::sqrt(dx*dx+dy*dy);
        }
    }
    std::optional<Finger> primary_,secondary_;
    std::optional<TouchGroup> selected_;
    TouchLayoutConfig baseline_{};
    TouchPoint first_start_{};
    float first_distance_{1.0F};
};

} // namespace starfox::app
