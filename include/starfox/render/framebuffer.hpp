#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <vector>

namespace starfox::render {

struct Rgba8;

// Identifies which pass produced a stored pixel. The distinction costs
// nothing to record: cartridge-authored 2D art always writes through the
// source raster at the current draw scale, while Super FX scan conversion
// drops that scale to 1 and writes stored pixels directly. A presentation
// filter that may only touch 2D art therefore reads these tags instead of
// guessing 2D ownership back out of the finished frame.
enum class PixelLayer : std::uint8_t {
    three_d = 0,
    two_d = 1,
    // Cartridge scenery: eligible for both 2D filtering and world effects,
    // unlike HUD/text pixels that also arrive through the source raster.
    background = 2,
    world_geometry = 3, // stars/dust/grid: world effects, but not a 2D-art filter
    textured_geometry = 4, // model texels: filterable artwork, still a 3D surface
};

// Pixels are stored at the render scale while every cartridge-authored pass
// keeps addressing the source raster: a draw scale of S expands one logical
// write into an SxS block, so 2D art stays pixel-exact. Scan conversion drops
// the scale to 1 and writes single pixels across the full stored extent.
class Framebuffer {
public:
    Framebuffer(std::uint32_t width, std::uint32_t height,
        std::uint32_t draw_scale = 1U)
        : stored_width_(width * std::max(1U, draw_scale)),
          stored_height_(height * std::max(1U, draw_scale)),
          draw_scale_(std::max(1U, draw_scale)),
          pixels_(static_cast<std::size_t>(stored_width_) * stored_height_) {}

    [[nodiscard]] std::uint32_t width() const noexcept {
        return stored_width_ / draw_scale_;
    }
    [[nodiscard]] std::uint32_t height() const noexcept {
        return stored_height_ / draw_scale_;
    }
    [[nodiscard]] std::uint32_t stored_width() const noexcept {
        return stored_width_;
    }
    [[nodiscard]] std::uint32_t stored_height() const noexcept {
        return stored_height_;
    }
    [[nodiscard]] std::uint32_t draw_scale() const noexcept {
        return draw_scale_;
    }
    // Repartitions the same storage between source-raster and stored extents.
    void set_draw_scale(std::uint32_t draw_scale) noexcept {
        draw_scale_ = draw_scale == 0U ? 1U : draw_scale;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& pixels() const noexcept { return pixels_; }
    [[nodiscard]] std::vector<std::uint8_t>& pixels() noexcept { return pixels_; }

    // Layer tags are opt-in so a frame presented without a 2D filter pays
    // neither the allocation nor the per-write store.
    void enable_layer_tags(bool enabled) {
        if (enabled == layer_tags_enabled_) return;
        layer_tags_enabled_ = enabled;
        if (enabled) {
            tags_.assign(pixels_.size(),
                static_cast<std::uint8_t>(PixelLayer::three_d));
        } else {
            tags_.clear();
            tags_.shrink_to_fit();
        }
    }
    [[nodiscard]] bool layer_tags_enabled() const noexcept {
        return layer_tags_enabled_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& layer_tags() const noexcept {
        return tags_;
    }
    // Mirrors the mutable pixels() accessor: a bulk transfer that writes the
    // pixel storage directly must move the tags in the same loop, or the
    // destination keeps whichever layer happened to own those cells before.
    [[nodiscard]] std::vector<std::uint8_t>& layer_tags() noexcept {
        return tags_;
    }
    [[nodiscard]] PixelLayer layer_stored(
        std::uint32_t x, std::uint32_t y) const noexcept {
        return static_cast<PixelLayer>(
            tags_[static_cast<std::size_t>(y) * stored_width_ + x]);
    }
    // Overrides the draw-scale-derived tag for world-space passes that still
    // address the source raster (dust, particles). See ScopedLayer.
    void set_layer_override(PixelLayer layer) noexcept {
        layer_override_ = static_cast<std::int8_t>(layer);
    }
    void clear_layer_override() noexcept { layer_override_ = -1; }
    [[nodiscard]] std::int8_t layer_override() const noexcept {
        return layer_override_;
    }

    void resize(std::uint32_t width, std::uint32_t height) {
        const auto stored_width = width * draw_scale_;
        const auto stored_height = height * draw_scale_;
        if (stored_width == stored_width_ && stored_height == stored_height_) return;
        stored_width_ = stored_width;
        stored_height_ = stored_height;
        pixels_.assign(
            static_cast<std::size_t>(stored_width_) * stored_height_, 0U);
        if (layer_tags_enabled_) {
            tags_.assign(pixels_.size(),
                static_cast<std::uint8_t>(PixelLayer::three_d));
        }
    }

    void clear(std::uint8_t colour = 0) noexcept {
        std::fill(pixels_.begin(), pixels_.end(), colour);
        if (layer_tags_enabled_) {
            std::fill(tags_.begin(), tags_.end(),
                static_cast<std::uint8_t>(PixelLayer::three_d));
        }
    }

    void set(std::int32_t x, std::int32_t y, std::uint8_t colour) noexcept {
        if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(width())
            || y >= static_cast<std::int32_t>(height())) {
            return;
        }
        if (draw_scale_ == 1U) {
            pixels_[static_cast<std::size_t>(y) * stored_width_
                + static_cast<std::size_t>(x)] = colour;
            if (layer_tags_enabled_) {
                tags_[static_cast<std::size_t>(y) * stored_width_
                    + static_cast<std::size_t>(x)] = write_tag(
                        PixelLayer::two_d);
            }
            return;
        }
        const auto origin_x = static_cast<std::uint32_t>(x) * draw_scale_;
        const auto origin_y = static_cast<std::uint32_t>(y) * draw_scale_;
        const auto tag = write_tag(PixelLayer::two_d);
        for (std::uint32_t row = 0; row < draw_scale_; ++row) {
            const auto offset = static_cast<std::ptrdiff_t>(
                static_cast<std::size_t>(origin_y + row) * stored_width_
                + origin_x);
            const auto begin = pixels_.begin() + offset;
            std::fill(begin, begin + draw_scale_, colour);
            if (layer_tags_enabled_) {
                const auto tag_begin = tags_.begin() + offset;
                std::fill(tag_begin, tag_begin + draw_scale_, tag);
            }
        }
    }

    [[nodiscard]] std::uint8_t get(std::uint32_t x, std::uint32_t y) const noexcept {
        return pixels_[
            static_cast<std::size_t>(y * draw_scale_) * stored_width_
            + x * draw_scale_];
    }

    // Stored writes come from layer compositing, where the tag belongs to the
    // source layer rather than to this call: the Super FX world, the cartridge
    // HUD and the EX overlay all arrive through here. Callers that know the
    // source layer pass it; the default suits geometry.
    void set_stored(std::uint32_t x, std::uint32_t y, std::uint8_t colour,
        PixelLayer layer = PixelLayer::three_d) noexcept {
        if (x >= stored_width_ || y >= stored_height_) return;
        pixels_[static_cast<std::size_t>(y) * stored_width_ + x] = colour;
        if (layer_tags_enabled_) {
            tags_[static_cast<std::size_t>(y) * stored_width_ + x] =
                write_tag(layer);
        }
    }

    // Wholesale copy used by the cartridge and Mode 2 background caches, which
    // save and restore a finished raster. Layer tags travel with the pixels so
    // a restored frame filters exactly like the frame it was captured from.
    void copy_pixels_from(const Framebuffer& source) {
        pixels_ = source.pixels_;
        if (!layer_tags_enabled_) return;
        if (source.layer_tags_enabled_ && source.tags_.size() == pixels_.size()) {
            tags_ = source.tags_;
        } else {
            tags_.assign(pixels_.size(),
                static_cast<std::uint8_t>(PixelLayer::three_d));
        }
    }

    [[nodiscard]] std::uint8_t get_stored(
        std::uint32_t x, std::uint32_t y) const noexcept {
        return pixels_[static_cast<std::size_t>(y) * stored_width_ + x];
    }

private:
    [[nodiscard]] std::uint8_t write_tag(PixelLayer derived) const noexcept {
        return layer_override_ < 0
            ? static_cast<std::uint8_t>(derived)
            : static_cast<std::uint8_t>(layer_override_);
    }

    std::uint32_t stored_width_{};
    std::uint32_t stored_height_{};
    std::uint32_t draw_scale_{1U};
    std::vector<std::uint8_t> pixels_;
    std::vector<std::uint8_t> tags_;
    bool layer_tags_enabled_{false};
    std::int8_t layer_override_{-1};
};

// Reclassifies every write made during its lifetime. World-space effects that
// still address the source raster (dust points, particles) are geometry rather
// than cartridge art, so they opt out of 2D filtering and keep the crisp
// block-replicated look they have today.
class ScopedLayer {
public:
    ScopedLayer(Framebuffer& target, PixelLayer layer) noexcept
        : target_(target), previous_(target.layer_override()) {
        target_.set_layer_override(layer);
    }
    ~ScopedLayer() {
        if (previous_ < 0) {
            target_.clear_layer_override();
        } else {
            target_.set_layer_override(static_cast<PixelLayer>(previous_));
        }
    }
    ScopedLayer(const ScopedLayer&) = delete;
    ScopedLayer& operator=(const ScopedLayer&) = delete;

private:
    Framebuffer& target_;
    std::int8_t previous_{-1};
};

struct LayerCompositeSettings {
    std::int32_t offset_x{};
    std::int32_t offset_y{};
    std::int32_t clip_left{std::numeric_limits<std::int32_t>::min()};
    std::int32_t clip_top{std::numeric_limits<std::int32_t>::min()};
    std::int32_t clip_right{std::numeric_limits<std::int32_t>::max()};
    std::int32_t clip_bottom{std::numeric_limits<std::int32_t>::max()};
    std::uint8_t mosaic{};
    std::uint8_t mosaic_layer_mask{};
    std::int32_t mosaic_origin_x{};
    std::int32_t mosaic_origin_y{};
};

void composite_transparent_layer(const Framebuffer& source,
    Framebuffer& destination, const LayerCompositeSettings& settings) noexcept;

void write_bmp(const Framebuffer& framebuffer, const std::filesystem::path& path);
void write_bmp(
    const Framebuffer& framebuffer,
    const std::filesystem::path& path,
    std::span<const Rgba8> palette);

} // namespace starfox::render
