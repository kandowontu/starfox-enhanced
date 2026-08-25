#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/timing/fixed_step.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

std::filesystem::path numbered_path(
    const std::filesystem::path& requested,
    std::uint32_t frame,
    std::uint32_t frame_count) {
    if (frame_count == 1) {
        return requested;
    }
    std::ostringstream name;
    name << requested.stem().string() << '_' << std::setfill('0') << std::setw(3) << frame
         << requested.extension().string();
    return requested.parent_path() / name.str();
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 5 || argc > 6) {
            std::cerr << "usage: starfox_shape_preview ROM SYMBOLS SHAPE OUTPUT.bmp [frames]\n";
            return 2;
        }

        const auto frame_count = argc == 6
            ? static_cast<std::uint32_t>(std::clamp(std::stoul(argv[5]), 1UL, 600UL))
            : 1U;
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const starfox::assets::ShapeDecoder decoder{rom, symbols};
        const auto shape = decoder.decode_by_name(symbols, argv[3]);

        starfox::render::Framebuffer framebuffer{224, 192};
        const starfox::render::SoftwareRenderer renderer;
        starfox::timing::TransformSnapshot previous{};
        starfox::timing::TransformSnapshot current{};
        current.yaw = 2'048;
        std::uint32_t simulation_tick = 0;

        for (std::uint32_t frame = 0; frame < frame_count; ++frame) {
            if (frame != 0 && frame % 3U == 0) {
                previous = current;
                current.yaw = static_cast<std::uint16_t>(current.yaw + 2'048U);
                ++simulation_tick;
            }
            const auto alpha = static_cast<double>(frame % 3U) / 3.0;
            const auto transform = starfox::timing::interpolate(previous, current, alpha);
            starfox::render::RenderPose pose;
            pose.yaw = transform.yaw;
            pose.pitch = -2'048.0;
            pose.y = 180.0;
            pose.z = 560.0;
            pose.animation_frame = simulation_tick;
            pose.colour_frame = simulation_tick;
            renderer.draw(shape, pose, framebuffer);
            starfox::render::write_bmp(
                framebuffer, numbered_path(argv[4], frame, frame_count));
        }

        std::cout << "shape:        " << shape.name << '\n'
                  << "header:       $" << std::hex << std::setw(6) << std::setfill('0')
                  << shape.header.address << std::dec << '\n'
                  << "vertices:     " << shape.vertices.size() << '\n'
                  << "faces:        " << shape.faces.size() << '\n'
                  << "visibilities: " << shape.visibilities.size() << '\n'
                  << "model frames: " << shape.frames.size() << '\n'
                  << "output frames:" << frame_count << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "shape preview failed: " << error.what() << '\n';
        return 1;
    }
}
