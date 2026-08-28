#pragma once

#include "starfox/assets/shape.hpp"
#include "starfox/assets/rom.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/simulation/math.hpp"

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
    // Star Fox EX's M_WIREMODE selects the MDRAWP scanline path: 0 is the
    // retail fill, 1 is hlines23/mhlines3 wireframe, and 2 retains fills until
    // an asymmetric edge transition enters the source's missing-poly path.
    std::uint8_t wireframe_mode{};
    // EX NAN modes 6-9 select the deliberately broken/wavy/cel variants in
    // MDRAWP.MC. The wave phase combines M_SINEOFFSET with M_FRAMENUM.
    std::uint8_t wobble_mode{};
    bool wave_mode{};
    bool cel_mode{};
    std::int16_t wave_offset{};
    // EX's secret COLOR WARP option bypasses the model colour table in
    // MDO_COLOUR_NG and feeds its register PRNG word into the ordinary
    // material decoder.  M_PROJPNTS is the register seed at the start of a
    // normal face pass; retain it as data because EX moves that work buffer.
    bool colour_warp{};
    std::uint16_t projected_points_address{0x0b9fU};
    // afexp/al_count: MOBJ offsets each face along its signed normal as the
    // destroyed model breaks apart.
    std::uint8_t explosion_progress{};
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
    // Optional presentation clip for transient sprite/particle effects. A
    // non-empty interval keeps those effects inside the source camera while
    // allowing the surrounding 3D scene to use a wider host framebuffer.
    std::int32_t effect_clip_left{};
    std::int32_t effect_clip_right{};
    // Presentation-only fallback for a long tapered solid crossing the near
    // plane. It retains the model's material while drawing its centre axis.
    bool collapse_to_axis_line{};
};

void apply_source_depth_tables(
    const assets::RomImage& rom,
    std::uint32_t depth_table_address,
    std::uint16_t threshold_pointer,
    std::uint16_t colour_pointer,
    std::uint8_t object_depth_offset,
    RenderPose& pose);

struct RenderSettings {
    // MOBJ.MC projects with (coordinate * 256) / z before adding the
    // 112x96 vanishing point.
    double focal_length{256.0};
    // Retail shapes carry explicit Super FX visibility triples (and BSP
    // ordering). Applying a second vertex-winding test hides their selected
    // filled faces, so source rendering leaves generic culling disabled.
    bool backface_culling{false};
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

    // MHUD.MC's first-person direction indicators are a Super FX line pass,
    // separate from both the 3D object list and the SNES OAM reticle.
    void draw_cockpit_hud(
        const simulation::TrigTables& trigonometry,
        std::uint8_t rotation,
        std::uint8_t colour,
        std::uint8_t damage_flags,
        std::int32_t horizontal_origin,
        Framebuffer& target,
        std::uint8_t normal_colour_override = 0U) const;

private:
    RenderSettings settings_;
};

} // namespace starfox::render
