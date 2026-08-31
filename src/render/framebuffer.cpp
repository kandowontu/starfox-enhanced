#include "starfox/render/framebuffer.hpp"
#include "starfox/render/palette.hpp"

#include <array>
#include <fstream>
#include <stdexcept>

namespace starfox::render {
namespace {

std::int32_t mosaic_coordinate(
    std::int32_t coordinate, std::int32_t size) noexcept {
    auto remainder = coordinate % size;
    if (remainder < 0) remainder += size;
    return coordinate - remainder;
}

void write_u16(std::ofstream& output, std::uint16_t value) {
    output.put(static_cast<char>(value & 0xffU));
    output.put(static_cast<char>((value >> 8U) & 0xffU));
}

void write_u32(std::ofstream& output, std::uint32_t value) {
    write_u16(output, static_cast<std::uint16_t>(value & 0xffffU));
    write_u16(output, static_cast<std::uint16_t>((value >> 16U) & 0xffffU));
}

} // namespace

void composite_transparent_layer(const Framebuffer& source,
    Framebuffer& destination, const LayerCompositeSettings& settings) noexcept {
    const auto mosaic_enabled = settings.mosaic_layer_mask != 0U
        && (settings.mosaic & settings.mosaic_layer_mask) != 0U;
    const auto mosaic_size = static_cast<std::int32_t>(
        (settings.mosaic >> 4U) + 1U);
    if (!mosaic_enabled && source.draw_scale() == 1U
        && destination.draw_scale() == 1U) {
        // The normal gameplay layers are already aligned pixel-for-pixel.
        // Walk their contiguous storage directly instead of paying two
        // bounds checks and four coordinate multiplications for every one of
        // the hundreds of thousands of transparent pixels at wide outputs.
        auto source_left = std::max(0, -settings.offset_x);
        auto source_top = std::max(0, -settings.offset_y);
        auto source_right = std::min(
            static_cast<std::int32_t>(source.width()),
            static_cast<std::int32_t>(destination.width())
                - settings.offset_x);
        auto source_bottom = std::min(
            static_cast<std::int32_t>(source.height()),
            static_cast<std::int32_t>(destination.height())
                - settings.offset_y);
        if (settings.clip_left != std::numeric_limits<std::int32_t>::min()) {
            source_left = std::max(
                source_left, settings.clip_left - settings.offset_x);
        }
        if (settings.clip_top != std::numeric_limits<std::int32_t>::min()) {
            source_top = std::max(
                source_top, settings.clip_top - settings.offset_y);
        }
        if (settings.clip_right != std::numeric_limits<std::int32_t>::max()) {
            source_right = std::min(
                source_right, settings.clip_right - settings.offset_x);
        }
        if (settings.clip_bottom != std::numeric_limits<std::int32_t>::max()) {
            source_bottom = std::min(
                source_bottom, settings.clip_bottom - settings.offset_y);
        }
        if (source_left >= source_right || source_top >= source_bottom) return;
        const auto& source_pixels = source.pixels();
        auto& destination_pixels = destination.pixels();
        for (auto y = source_top; y < source_bottom; ++y) {
            auto source_index = static_cast<std::size_t>(y) * source.width()
                + static_cast<std::size_t>(source_left);
            auto destination_index = static_cast<std::size_t>(
                y + settings.offset_y) * destination.width()
                + static_cast<std::size_t>(source_left + settings.offset_x);
            for (auto x = source_left; x < source_right; ++x) {
                const auto colour = source_pixels[source_index++];
                if (colour != 0U) destination_pixels[destination_index] = colour;
                ++destination_index;
            }
        }
        return;
    }
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        for (std::uint32_t x = 0; x < source.width(); ++x) {
            const auto destination_x = static_cast<std::int32_t>(x)
                + settings.offset_x;
            const auto destination_y = static_cast<std::int32_t>(y)
                + settings.offset_y;
            if (destination_x < settings.clip_left
                || destination_x >= settings.clip_right
                || destination_y < settings.clip_top
                || destination_y >= settings.clip_bottom) {
                continue;
            }

            auto source_x = static_cast<std::int32_t>(x);
            auto source_y = static_cast<std::int32_t>(y);
            if (mosaic_enabled) {
                const auto logical_x = destination_x
                    - settings.mosaic_origin_x;
                const auto logical_y = destination_y
                    - settings.mosaic_origin_y;
                source_x = mosaic_coordinate(logical_x, mosaic_size)
                    + settings.mosaic_origin_x - settings.offset_x;
                source_y = mosaic_coordinate(logical_y, mosaic_size)
                    + settings.mosaic_origin_y - settings.offset_y;
                if (source_x < 0 || source_y < 0
                    || source_x >= static_cast<std::int32_t>(source.width())
                    || source_y >= static_cast<std::int32_t>(source.height())) {
                    continue;
                }
            }
            // Clipping and mosaic stay on the source raster grid, matching
            // the PPU. Only the transfer itself runs at the stored scale so a
            // higher-resolution 3D layer keeps its detail through the pass.
            const auto source_scale = source.draw_scale();
            const auto destination_scale = destination.draw_scale();
            if (source_scale == 1U && destination_scale == 1U) {
                const auto colour = source.get(
                    static_cast<std::uint32_t>(source_x),
                    static_cast<std::uint32_t>(source_y));
                if (colour != 0U) {
                    destination.set(destination_x, destination_y, colour);
                }
                continue;
            }
            if (destination_x < 0 || destination_y < 0) continue;
            const auto source_origin_x =
                static_cast<std::uint32_t>(source_x) * source_scale;
            const auto source_origin_y =
                static_cast<std::uint32_t>(source_y) * source_scale;
            const auto destination_origin_x =
                static_cast<std::uint32_t>(destination_x)
                * destination_scale;
            const auto destination_origin_y =
                static_cast<std::uint32_t>(destination_y)
                * destination_scale;
            for (std::uint32_t row = 0; row < destination_scale; ++row) {
                const auto source_row = std::min(source_scale - 1U,
                    ((row * 2U + 1U) * source_scale)
                        / (destination_scale * 2U));
                for (std::uint32_t column = 0;
                     column < destination_scale; ++column) {
                    const auto source_column = std::min(source_scale - 1U,
                        ((column * 2U + 1U) * source_scale)
                            / (destination_scale * 2U));
                    const auto colour = source.get_stored(
                        source_origin_x + source_column,
                        source_origin_y + source_row);
                    if (colour == 0U) continue;
                    destination.set_stored(destination_origin_x + column,
                        destination_origin_y + row, colour);
                }
            }
        }
    }
}

void write_bmp(const Framebuffer& framebuffer, const std::filesystem::path& path) {
    write_bmp(framebuffer, path, preview_palette());
}

void write_bmp(
    const Framebuffer& framebuffer,
    const std::filesystem::path& path,
    std::span<const Rgba8> palette) {
    if (palette.empty()) {
        throw std::invalid_argument{"bitmap palette is empty"};
    }
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output{path, std::ios::binary};
    if (!output) {
        throw std::runtime_error{"unable to create bitmap: " + path.string()};
    }

    const auto row_bytes = ((framebuffer.width() * 3U) + 3U) & ~3U;
    const auto pixel_bytes = row_bytes * framebuffer.height();
    output.write("BM", 2);
    write_u32(output, 54U + pixel_bytes);
    write_u16(output, 0);
    write_u16(output, 0);
    write_u32(output, 54);
    write_u32(output, 40);
    write_u32(output, framebuffer.width());
    write_u32(output, framebuffer.height());
    write_u16(output, 1);
    write_u16(output, 24);
    write_u32(output, 0);
    write_u32(output, pixel_bytes);
    write_u32(output, 2'835);
    write_u32(output, 2'835);
    write_u32(output, 0);
    write_u32(output, 0);

    const std::array<char, 3> padding{};
    for (std::uint32_t y = framebuffer.height(); y-- > 0;) {
        for (std::uint32_t x = 0; x < framebuffer.width(); ++x) {
            const auto pixel = framebuffer.get(x, y);
            const auto colour = palette[std::min<std::size_t>(
                pixel, palette.size() - 1U)];
            output.put(static_cast<char>(colour.b));
            output.put(static_cast<char>(colour.g));
            output.put(static_cast<char>(colour.r));
        }
        output.write(padding.data(), static_cast<std::streamsize>(row_bytes - framebuffer.width() * 3U));
    }
}

} // namespace starfox::render
