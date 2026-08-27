#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
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

void write_bmp(const Framebuffer& framebuffer, const std::filesystem::path& path);
void write_bmp(
    const Framebuffer& framebuffer,
    const std::filesystem::path& path,
    std::span<const Rgba8> palette);

} // namespace starfox::render
