#include "starfox/render/framebuffer.hpp"
#include "starfox/render/palette.hpp"

#include <array>
#include <fstream>
#include <stdexcept>

namespace starfox::render {
namespace {

void write_u16(std::ofstream& output, std::uint16_t value) {
    output.put(static_cast<char>(value & 0xffU));
    output.put(static_cast<char>((value >> 8U) & 0xffU));
}

void write_u32(std::ofstream& output, std::uint32_t value) {
    write_u16(output, static_cast<std::uint16_t>(value & 0xffffU));
    write_u16(output, static_cast<std::uint16_t>((value >> 16U) & 0xffffU));
}

} // namespace

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
