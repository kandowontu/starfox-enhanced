#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <vector>

namespace starfox::render {

struct Rgba8;

// Pixels are stored at the render scale while every cartridge-authored pass
// keeps addressing the source raster: a draw scale of S expands one logical
// write into an SxS block, so 2D art stays pixel-exact. Scan conversion drops
// the scale to 1 and writes single pixels across the full stored extent.
class Framebuffer {
public:
    Framebuffer(std::uint32_t width, std::uint32_t height,
        std::uint32_t draw_scale = 1U)
        : stored_width_(width * draw_scale),
          stored_height_(height * draw_scale),
          draw_scale_(draw_scale),
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

    void resize(std::uint32_t width, std::uint32_t height) {
        const auto stored_width = width * draw_scale_;
        const auto stored_height = height * draw_scale_;
        if (stored_width == stored_width_ && stored_height == stored_height_) return;
        stored_width_ = stored_width;
        stored_height_ = stored_height;
        pixels_.assign(
            static_cast<std::size_t>(stored_width_) * stored_height_, 0U);
    }

    void clear(std::uint8_t colour = 0) noexcept {
        std::fill(pixels_.begin(), pixels_.end(), colour);
    }

    void set(std::int32_t x, std::int32_t y, std::uint8_t colour) noexcept {
        if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(width())
            || y >= static_cast<std::int32_t>(height())) {
            return;
        }
        if (draw_scale_ == 1U) {
            pixels_[static_cast<std::size_t>(y) * stored_width_
                + static_cast<std::size_t>(x)] = colour;
            return;
        }
        const auto origin_x = static_cast<std::uint32_t>(x) * draw_scale_;
        const auto origin_y = static_cast<std::uint32_t>(y) * draw_scale_;
        for (std::uint32_t row = 0; row < draw_scale_; ++row) {
            const auto begin = pixels_.begin()
                + static_cast<std::ptrdiff_t>(
                    static_cast<std::size_t>(origin_y + row) * stored_width_
                    + origin_x);
            std::fill(begin, begin + draw_scale_, colour);
        }
    }

    [[nodiscard]] std::uint8_t get(std::uint32_t x, std::uint32_t y) const noexcept {
        return pixels_[
            static_cast<std::size_t>(y * draw_scale_) * stored_width_
            + x * draw_scale_];
    }

    void set_stored(std::uint32_t x, std::uint32_t y, std::uint8_t colour) noexcept {
        if (x >= stored_width_ || y >= stored_height_) return;
        pixels_[static_cast<std::size_t>(y) * stored_width_ + x] = colour;
    }

    [[nodiscard]] std::uint8_t get_stored(
        std::uint32_t x, std::uint32_t y) const noexcept {
        return pixels_[static_cast<std::size_t>(y) * stored_width_ + x];
    }

private:
    std::uint32_t stored_width_{};
    std::uint32_t stored_height_{};
    std::uint32_t draw_scale_{1U};
    std::vector<std::uint8_t> pixels_;
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
