#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <vector>

namespace starfox::render {

struct Rgba8;

class Framebuffer {
public:
    Framebuffer(std::uint32_t width, std::uint32_t height)
        : width_(width), height_(height), pixels_(static_cast<std::size_t>(width) * height) {}

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] const std::vector<std::uint8_t>& pixels() const noexcept { return pixels_; }
    [[nodiscard]] std::vector<std::uint8_t>& pixels() noexcept { return pixels_; }

    void resize(std::uint32_t width, std::uint32_t height) {
        if (width == width_ && height == height_) return;
        width_ = width;
        height_ = height;
        pixels_.assign(static_cast<std::size_t>(width) * height, 0U);
    }

    void clear(std::uint8_t colour = 0) noexcept {
        std::fill(pixels_.begin(), pixels_.end(), colour);
    }

    void set(std::int32_t x, std::int32_t y, std::uint8_t colour) noexcept {
        if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(width_)
            || y >= static_cast<std::int32_t>(height_)) {
            return;
        }
        pixels_[static_cast<std::size_t>(y) * width_ + static_cast<std::size_t>(x)] = colour;
    }

    [[nodiscard]] std::uint8_t get(std::uint32_t x, std::uint32_t y) const noexcept {
        return pixels_[static_cast<std::size_t>(y) * width_ + x];
    }

private:
    std::uint32_t width_{};
    std::uint32_t height_{};
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
