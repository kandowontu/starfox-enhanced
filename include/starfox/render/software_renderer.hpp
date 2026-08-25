#pragma once

#include "starfox/assets/shape.hpp"
#include "starfox/assets/rom.hpp"
#include "starfox/render/framebuffer.hpp"

#include <cstdint>
#include <array>

namespace starfox::render {

struct RenderPose {
    double x{};
    double y{};
    double z{420.0};
    double pitch{};
    double yaw{};
    double roll{};
    double scale{1.0};
    double vanish_x{112.0};
    double vanish_y{96.0};
    std::uint32_t animation_frame{};
    std::uint32_t colour_frame{};
    std::int32_t texture_scroll_x{};
    std::int32_t texture_scroll_y{};
    std::array<std::int16_t, 9> rotation_matrix{};
    bool use_rotation_matrix{};
    std::array<std::int16_t, 3> depth_thresholds{2'560, 3'328, 3'840};
    std::array<std::array<std::uint8_t, 32>, 4> depth_colour_tables{};
    bool has_depth_colour_tables{};
    // MOBJ.MC replaces every material with colour $09 in CMODE 2 for an
    // ordinary projected shadow. The byte stores its alternating 4-bpp colours.
    bool force_colour{};
    std::uint8_t forced_colour{};
    bool simple_scaled_sprite{};
    std::uint8_t simple_sprite_colour{};
    std::int16_t simple_sprite_world_size{};
};

void apply_original_depth_tables(
    const assets::RomImage& rom,
    std::uint16_t threshold_pointer,
    std::uint16_t colour_pointer,
    std::uint8_t object_depth_offset,
    RenderPose& pose);

struct RenderSettings {
    // MOBJ.MC projects with (coordinate * 256) / z before adding the
    // 112x96 vanishing point.
    double focal_length{256.0};
    bool backface_culling{true};
    std::uint8_t background_colour{};
    std::uint8_t colour_index_base{};
};

class SoftwareRenderer {
public:
    explicit SoftwareRenderer(RenderSettings settings = {});

    void draw(
        const assets::Shape& shape,
        const RenderPose& pose,
        Framebuffer& target,
        bool clear_target = true) const;

private:
    RenderSettings settings_;
};

} // namespace starfox::render
