#pragma once

#include "starfox/assets/shape.hpp"
#include "starfox/assets/rom.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/simulation/math.hpp"

#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>
#include <vector>

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
    // Geometry may be interpolated between source frames for high-FPS
    // presentation, but MOBJ chooses its depth band and light vector only at
    // the completed source draw. Keeping that state separate prevents a
    // presentation frame from dimming a face before the original routine did.
    double source_depth{};
    std::array<std::int16_t, 9> source_lighting_matrix{};
    bool use_source_lighting_state{};
    // Generated presentation frames may retain fractional transformed
    // coordinates. Completed source frames leave this disabled and continue
    // through the bit-exact Super FX word path.
    bool subpixel_projection{};
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
    // Supersampling factor for polygon projection and scan conversion. The
    // fractional geometry shared by rasterization, visibility and BSP order
    // stays scale-independent and stable across source-frame boundaries.
    std::uint32_t render_scale{1U};
};

// Presentation metadata for a host-rendered Super FX surface. The indexed
// framebuffer remains source-accurate; this parallel buffer gives optional PC
// rendering effects the actual polygon normal and depth instead of asking a
// screen-space filter to guess them from palette brightness.
struct SurfaceSample {
    float normal_x{};
    float normal_y{};
    float normal_z{1.0F};
    float depth{};
    std::uint8_t palette_index{};
    bool valid{};
};

class SurfaceBuffer {
public:
    SurfaceBuffer(std::uint32_t width, std::uint32_t height)
        : width_(width), height_(height), samples_(
              static_cast<std::size_t>(width) * height) {}

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

    void resize(std::uint32_t width, std::uint32_t height) {
        if (width == width_ && height == height_) return;
        width_ = width;
        height_ = height;
        samples_.assign(static_cast<std::size_t>(width) * height, {});
        reset_bounds();
    }

    void clear() noexcept {
        if (!empty()) {
            for (auto y = minimum_y_; y < maximum_y_; ++y) {
                const auto first = samples_.begin()
                    + static_cast<std::ptrdiff_t>(
                        static_cast<std::size_t>(y) * width_ + minimum_x_);
                std::fill(first, first + static_cast<std::ptrdiff_t>(
                    maximum_x_ - minimum_x_), SurfaceSample{});
            }
        }
        reset_bounds();
    }

    void set(std::int32_t x, std::int32_t y, SurfaceSample sample,
        std::uint8_t palette_index) noexcept {
        if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(width_)
            || y >= static_cast<std::int32_t>(height_)) {
            return;
        }
        sample.palette_index = palette_index;
        sample.valid = true;
        samples_[static_cast<std::size_t>(y) * width_
            + static_cast<std::size_t>(x)] = sample;
        minimum_x_ = std::min(minimum_x_, static_cast<std::uint32_t>(x));
        minimum_y_ = std::min(minimum_y_, static_cast<std::uint32_t>(y));
        maximum_x_ = std::max(maximum_x_, static_cast<std::uint32_t>(x) + 1U);
        maximum_y_ = std::max(maximum_y_, static_cast<std::uint32_t>(y) + 1U);
    }

    [[nodiscard]] const SurfaceSample& get(
        std::uint32_t x, std::uint32_t y) const noexcept {
        return samples_[static_cast<std::size_t>(y) * width_ + x];
    }

    [[nodiscard]] bool empty() const noexcept {
        return minimum_x_ >= maximum_x_ || minimum_y_ >= maximum_y_;
    }
    [[nodiscard]] std::uint32_t minimum_x() const noexcept { return minimum_x_; }
    [[nodiscard]] std::uint32_t minimum_y() const noexcept { return minimum_y_; }
    [[nodiscard]] std::uint32_t maximum_x() const noexcept { return maximum_x_; }
    [[nodiscard]] std::uint32_t maximum_y() const noexcept { return maximum_y_; }

private:
    void reset_bounds() noexcept {
        minimum_x_ = width_;
        minimum_y_ = height_;
        maximum_x_ = 0U;
        maximum_y_ = 0U;
    }

    std::uint32_t width_{};
    std::uint32_t height_{};
    std::vector<SurfaceSample> samples_;
    std::uint32_t minimum_x_{width_};
    std::uint32_t minimum_y_{height_};
    std::uint32_t maximum_x_{};
    std::uint32_t maximum_y_{};
};

class SoftwareRenderer {
public:
    explicit SoftwareRenderer(RenderSettings settings = {});

    void draw(
        const assets::Shape& shape,
        const RenderPose& pose,
        Framebuffer& target,
        bool clear_target = true,
        SurfaceBuffer* surfaces = nullptr) const;

    // MHUD.MC's first-person direction indicators are a Super FX line pass,
    // separate from both the 3D object list and the SNES OAM reticle.
    void draw_cockpit_hud(
        const simulation::TrigTables& trigonometry,
        double rotation,
        std::uint8_t colour,
        std::uint8_t damage_flags,
        std::int32_t horizontal_origin,
        Framebuffer& target,
        std::uint8_t normal_colour_override = 0U) const;

private:
    RenderSettings settings_;
};

} // namespace starfox::render
