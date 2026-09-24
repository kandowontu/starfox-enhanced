#pragma once
#include "starfox/render/framebuffer.hpp"
#include <cmath>

namespace starfox::render {

enum class PersistenceMode : std::uint8_t { off, trails, long_exposure };

// Software reference for temporal manipulations. Time is presentation time,
// not a frame counter: a 240 Hz run must not fade four times as fast as 60 Hz.
// One instance per eye/output. The host owns the scene epoch and must change it
// on cartridge/scene/state-load transitions, even when dimensions match.
class FramePersistence {
public:
    void reset() noexcept {
        std::vector<float>{}.swap(history_);
        valid_=false;
    }
    [[nodiscard]] std::size_t allocated_bytes() const noexcept {
        return history_.capacity()*sizeof(float);
    }
    void apply(const Framebuffer& frame,std::vector<std::uint8_t>& rgba,
        PersistenceMode mode,bool models,bool world,double seconds,
        std::uint64_t scene_epoch,unsigned intensity=100) {
        intensity=std::min(intensity,100U);
        const auto count=frame.pixels().size();
        if(mode==PersistenceMode::off || (!models && !world) || !intensity
            || !frame.layer_tags_enabled() || rgba.size()!=count*4
            || !std::isfinite(seconds)) {reset();return;}
        const bool discontinuity=!valid_ || epoch_!=scene_epoch || mode_!=mode
            || models_!=models || world_!=world || width_!=frame.stored_width()
            || height_!=frame.stored_height() || scale_!=frame.draw_scale()
            || seconds<time_ || seconds-time_>1.0;
        const auto decay=discontinuity?0.0f:mode==PersistenceMode::long_exposure?1.0f:
            static_cast<float>(std::exp2(-(seconds-time_)/0.35));
        if(discontinuity) history_.assign(count*3,0.0f);
        epoch_=scene_epoch;mode_=mode;models_=models;world_=world;
        width_=frame.stored_width();height_=frame.stored_height();scale_=frame.draw_scale();
        time_=seconds;valid_=true;
        for(std::size_t i=0;i<count;++i) {
            const auto tag=static_cast<PixelLayer>(frame.layer_tags()[i]);
            const bool hud=tag==PixelLayer::two_d;
            const bool scenery=tag==PixelLayer::background || tag==PixelLayer::world_geometry || tag==PixelLayer::terrain_geometry;
            const bool selected=scenery?world:models;
            // Never capture HUD ink or allow stale trails to reappear after a
            // dialogue/menu covers a pixel. World-only history stays behind models.
            for(unsigned c=0;c<3;++c) {
                auto& memory=history_[i*3+c];
                if(hud || (!scenery && !models)) {memory=0;continue;}
                memory=std::max(selected?float(rgba[i*4+c]):0.0f,memory*decay);
                const auto value=std::max(float(rgba[i*4+c]),memory);
                rgba[i*4+c]=static_cast<std::uint8_t>(std::clamp(
                    std::lround(float(rgba[i*4+c])+(value-rgba[i*4+c])*float(intensity)/100.f),0L,255L));
            }
        }
    }
private:
    std::vector<float> history_;
    std::uint64_t epoch_{};
    std::uint32_t width_{},height_{},scale_{};
    double time_{};
    PersistenceMode mode_{PersistenceMode::off};
    bool models_{},world_{},valid_{};
};
}
