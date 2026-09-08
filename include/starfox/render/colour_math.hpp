#pragma once

#include "starfox/render/framebuffer.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include <array>

namespace starfox::render {

// Fixed-colour windows leave OBJ untouched unless CGADSUB explicitly includes
// it. In particular, revival's blackfade must not erase the STAGE sprites.
inline void apply_colour_math(const simulation::ColourMathEffectState& effect,
    const Framebuffer& frame, std::vector<std::uint8_t>& rgba) {
    if (!effect.active || effect.affected_layers == 0U
        || rgba.size() != frame.pixels().size()*4U) return;
    const auto expand = [](std::uint8_t v) { return int((v&31U)*8U + ((v&31U)>>2U)); };
    const std::array fixed{expand(effect.red),expand(effect.green),expand(effect.blue)};
    for (std::size_t i=0; i<frame.pixels().size(); ++i) {
        if (frame.pixels()[i]>=128U && (effect.affected_layers&0x10U)==0U) continue;
        for (unsigned c=0; c<3; ++c) {
            int value=rgba[i*4+c];
            value=effect.subtract ? value-fixed[c] : value+fixed[c];
            if (effect.half) value/=2;
            rgba[i*4+c]=std::uint8_t(std::clamp(value,0,255));
        }
    }
}

} // namespace starfox::render
