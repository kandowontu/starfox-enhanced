#include "starfox/render/particle_renderer.hpp"

#include <algorithm>
#include <cmath>

namespace starfox::render {
namespace {

struct Point {
    int x{};
    int y{};
    bool visible{};
};

double interpolate_word(std::int16_t from, std::int16_t to, double alpha) noexcept {
    auto delta = static_cast<std::int32_t>(to) - from;
    if (delta > 32'767) delta -= 65'536;
    else if (delta < -32'768) delta += 65'536;
    return from + delta * alpha;
}

Point project(double x, double y, double z, const Framebuffer& target) {
    if (z < 256.0) return {};
    const auto screen_x = static_cast<int>(target.width() / 2U)
        + static_cast<int>(std::trunc(x * 256.0 / z));
    const auto screen_y = static_cast<int>(target.height() / 2U)
        + static_cast<int>(std::trunc(y * 256.0 / z));
    // MPART.MC clips against mrightclp-1 and mbotclp-1.
    return {screen_x, screen_y,
        screen_x >= 0 && screen_y >= 0
            && screen_x < static_cast<int>(target.width()) - 1
            && screen_y < static_cast<int>(target.height()) - 1};
}

void draw_line(Framebuffer& target, Point a, Point b, std::uint8_t colour) {
    auto x0 = a.x;
    auto y0 = a.y;
    const auto dx = std::abs(b.x - x0);
    const auto sx = x0 < b.x ? 1 : -1;
    const auto dy = -std::abs(b.y - y0);
    const auto sy = y0 < b.y ? 1 : -1;
    auto error = dx + dy;
    for (;;) {
        target.set(x0, y0, colour);
        if (x0 == b.x && y0 == b.y) break;
        const auto doubled = error * 2;
        if (doubled >= dy) {
            error += dy;
            x0 += sx;
        }
        if (doubled <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

} // namespace

void ParticleRenderer::draw_owner(
    const simulation::ParticleSystem& particles,
    simulation::ObjectHandle owner,
    const RenderPose& owner_pose,
    double interpolation_alpha,
    Framebuffer& target,
    std::uint8_t colour_index_base) const {
    const auto alpha = std::clamp(interpolation_alpha, 0.0, 1.0);
    for (const auto& particle : particles.particles()) {
        if (particle.life == 0U || particle.owner != owner) continue;
        const auto x = interpolate_word(
            particle.previous_x, particle.x, alpha);
        const auto y = interpolate_word(
            particle.previous_y, particle.y, alpha);
        const auto z = interpolate_word(
            particle.previous_z, particle.z, alpha);
        const auto point = project(
            owner_pose.x + x, owner_pose.y + y, owner_pose.z + z, target);
        if (!point.visible) continue;
        const auto colour = static_cast<std::uint8_t>(
            colour_index_base + particle.colour);
        if ((particle.flags & 0x04U) != 0U) {
            const auto previous = project(owner_pose.x + particle.previous_x,
                owner_pose.y + particle.previous_y,
                owner_pose.z + particle.previous_z, target);
            if (previous.visible) draw_line(target, previous, point, colour);
            continue;
        }
        // PLOT increments R1; the source's two PLOT pairs form a 2x2 dot.
        target.set(point.x, point.y, colour);
        target.set(point.x + 1, point.y, colour);
        target.set(point.x, point.y + 1, colour);
        target.set(point.x + 1, point.y + 1, colour);
    }
}

} // namespace starfox::render
