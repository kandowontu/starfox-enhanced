#pragma once

#include "starfox/render/framebuffer.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/simulation/particle_system.hpp"

#include <cstdint>

namespace starfox::render {

class ParticleRenderer {
public:
    void draw_owner(
        const simulation::ParticleSystem& particles,
        simulation::ObjectHandle owner,
        const RenderPose& owner_pose,
        double interpolation_alpha,
        Framebuffer& target,
        std::uint8_t colour_index_base = 7U * 16U) const;
};

} // namespace starfox::render
