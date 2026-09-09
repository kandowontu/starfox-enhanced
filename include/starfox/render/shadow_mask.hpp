#pragma once

#include "starfox/render/shadow_scene.hpp"
#include "starfox/render/row_workers.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace starfox::render::shadows {
struct ReceiverPlane { Vec3 point, normal; };
struct Camera {
    std::uint32_t width{}, height{};
    double focal_length{256}, center_x{112}, center_y{96};
};

// Geometry lives in camera space. Ground is supplied only in scenes with an
// actual ground receiver; space and menu backgrounds must not receive shadows.
// One entry per requested render pixel, independent of palette darkness.
inline void render_mask(const Scene& scene, Camera camera, Vec3 toward_light,
    std::optional<ReceiverPlane> ground, std::vector<std::uint8_t>& mask,
    RowWorkers* workers = nullptr) {
    mask.assign(static_cast<std::size_t>(camera.width)*camera.height, 0);
    const auto length=std::sqrt(dot(toward_light,toward_light));
    if (camera.focal_length<=0 || !std::isfinite(length) || length<=1e-10) return;
    toward_light=toward_light*(1.0/length);
    const auto reference=std::abs(toward_light.y)<.9?Vec3{0,1,0}:Vec3{1,0,0};
    auto tangent=cross(toward_light,reference);
    tangent=tangent*(1.0/std::sqrt(dot(tangent,tangent)));
    const auto bitangent=cross(toward_light,tangent);
    std::array<Vec3,8> light_samples;
    for (unsigned i=0;i<light_samples.size();++i) {
        // Fixed disk samples avoid temporal noise at high presentation FPS.
        // Angular spread makes penumbrae widen with caster/receiver distance.
        const auto radius=.015*std::sqrt((i+.5)/light_samples.size());
        const auto angle=i*2.399963229728653;
        auto direction=toward_light+tangent*(radius*std::cos(angle))
            +bitangent*(radius*std::sin(angle));
        light_samples[i]=direction*(1.0/std::sqrt(dot(direction,direction)));
    }
    const auto render_rows = [&](std::uint32_t first, std::uint32_t last) {
    for (std::uint32_t y=first;y<last;++y) {
        for (std::uint32_t x=0;x<camera.width;++x) {
            const Vec3 ray{(double(x)+.5-camera.center_x)/camera.focal_length,
                (double(y)+.5-camera.center_y)/camera.focal_length,1};
            auto distance=scene.nearest({},ray,1.0);
            if (ground) {
                const auto denominator=dot(ray,ground->normal);
                if (std::abs(denominator)>1e-10) {
                    const auto depth=dot(ground->point,ground->normal)/denominator;
                    if (depth>1 && depth<65536 && (!distance || depth<*distance)) distance=depth;
                }
            }
            if (!distance) continue;
            const auto receiver=ray * (*distance);
            const auto bias=std::max(.1,*distance*1e-5);
            unsigned blocked=0;
            for (const auto direction:light_samples)
                blocked+=scene.occluded(receiver,direction,bias)?1U:0U;
            mask[static_cast<std::size_t>(y)*camera.width+x]=
                static_cast<std::uint8_t>(160U*blocked/light_samples.size());
        }
    }
    };
    if (workers != nullptr) workers->parallel_rows(camera.height, render_rows);
    else render_rows(0, camera.height);
}
} // namespace starfox::render::shadows
