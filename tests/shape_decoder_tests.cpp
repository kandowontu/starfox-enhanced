#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/software_renderer.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

std::size_t offset(std::uint32_t address) {
    return static_cast<std::size_t>((address >> 16U) & 0x7fU) * 0x8000U
        + static_cast<std::size_t>(address & 0x7fffU);
}

void put8(std::vector<std::uint8_t>& rom, std::uint32_t& address, std::uint8_t value) {
    rom[offset(address++)] = value;
}

void put16(std::vector<std::uint8_t>& rom, std::uint32_t& address, std::uint16_t value) {
    put8(rom, address, static_cast<std::uint8_t>(value & 0xffU));
    put8(rom, address, static_cast<std::uint8_t>(value >> 8U));
}

starfox::assets::RomImage make_rom() {
    std::vector<std::uint8_t> bytes(0x20000);
    auto header = std::uint32_t{0x008100};
    put16(bytes, header, 0x8000);
    put8(bytes, header, 0x01);
    put16(bytes, header, 0x8100);
    put16(bytes, header, 0);
    put8(bytes, header, 0);
    put16(bytes, header, 10);
    put16(bytes, header, 10);
    put16(bytes, header, 10);
    put16(bytes, header, 10);
    put16(bytes, header, 10);
    put16(bytes, header, 0x8200);
    put16(bytes, header, 0x8100);
    put16(bytes, header, 0x8100);
    put16(bytes, header, 0x8100);
    put16(bytes, header, 0x8100);

    auto points = std::uint32_t{0x018000};
    put8(bytes, points, 4);
    put8(bytes, points, 3);
    put8(bytes, points, static_cast<std::uint8_t>(-20));
    put8(bytes, points, static_cast<std::uint8_t>(-10));
    put8(bytes, points, 0);
    put8(bytes, points, 20);
    put8(bytes, points, static_cast<std::uint8_t>(-10));
    put8(bytes, points, 0);
    put8(bytes, points, 0);
    put8(bytes, points, 20);
    put8(bytes, points, 0);
    put8(bytes, points, 12);

    auto faces = std::uint32_t{0x018100};
    put8(bytes, faces, 48);
    put8(bytes, faces, 1);
    put8(bytes, faces, 0);
    put8(bytes, faces, 2);
    put8(bytes, faces, 1);
    put8(bytes, faces, 60); // BSPInit
    put8(bytes, faces, 40); // BSP node
    put8(bytes, faces, 0);
    put16(bytes, faces, 8); // node face list at $8111
    put8(bytes, faces, 4);  // alternate control at $810e
    put8(bytes, faces, 68); // fallthrough leaf
    put16(bytes, faces, 15);
    put8(bytes, faces, 68); // alternate leaf
    put16(bytes, faces, 14);

    put8(bytes, faces, 20); // node face list at $8111
    put8(bytes, faces, 3);
    put8(bytes, faces, 0);
    put8(bytes, faces, 14);
    put8(bytes, faces, 0);
    put8(bytes, faces, 0);
    put8(bytes, faces, 127);
    put8(bytes, faces, 0);
    put8(bytes, faces, 2);
    put8(bytes, faces, 1);
    put8(bytes, faces, 0xff);
    put8(bytes, faces, 20); // empty fallthrough leaf at $811c
    put8(bytes, faces, 0xff);
    put8(bytes, faces, 20); // empty alternate leaf at $811e
    put8(bytes, faces, 0xff);

    auto colours = std::uint32_t{0x038200};
    for (std::uint16_t index = 0; index < 15; ++index) {
        put16(bytes, colours, static_cast<std::uint16_t>(
            0x3f00U | (index << 4U) | index));
    }
    auto animated_colour = std::uint32_t{0x03821c};
    put16(bytes, animated_colour, 0x8300U); // COLANIM $038300
    auto colour_frames = std::uint32_t{0x038300};
    put8(bytes, colour_frames, 2);
    put16(bytes, colour_frames, 0x3f11U);
    put16(bytes, colour_frames, 0x3f22U);
    return starfox::assets::RomImage{std::move(bytes)};
}

starfox::assets::RomImage make_animated_rom() {
    std::vector<std::uint8_t> bytes(0x20000);
    auto header = std::uint32_t{0x008100};
    put16(bytes, header, 0x8000);
    put8(bytes, header, 0x01);
    put16(bytes, header, 0x8100);
    put16(bytes, header, 0);
    put8(bytes, header, 0);
    for (int field = 0; field < 5; ++field) {
        put16(bytes, header, 10);
    }
    put16(bytes, header, 0x8200);
    for (int pointer = 0; pointer < 4; ++pointer) {
        put16(bytes, header, 0x8100);
    }

    auto points = std::uint32_t{0x018000};
    put8(bytes, points, 4); // one static prefix vertex
    put8(bytes, points, 1);
    put8(bytes, points, 0);
    put8(bytes, points, 0);
    put8(bytes, points, 0);
    put8(bytes, points, 28); // Frames 2
    put8(bytes, points, 2);
    put16(bytes, points, 3);  // entry $8007 -> frame $800b
    put16(bytes, points, 12); // entry $8009 -> frame $8016

    put8(bytes, points, 4); // frame 0
    put8(bytes, points, 2);
    put8(bytes, points, static_cast<std::uint8_t>(-20));
    put8(bytes, points, static_cast<std::uint8_t>(-10));
    put8(bytes, points, 0);
    put8(bytes, points, 20);
    put8(bytes, points, static_cast<std::uint8_t>(-10));
    put8(bytes, points, 0);
    put8(bytes, points, 32); // jump to shared EndPoints
    put16(bytes, points, 9);

    put8(bytes, points, 4); // frame 1
    put8(bytes, points, 2);
    put8(bytes, points, static_cast<std::uint8_t>(-10));
    put8(bytes, points, static_cast<std::uint8_t>(-20));
    put8(bytes, points, 0);
    put8(bytes, points, 10);
    put8(bytes, points, static_cast<std::uint8_t>(-20));
    put8(bytes, points, 0);
    put8(bytes, points, 12);

    auto faces = std::uint32_t{0x018100};
    put8(bytes, faces, 48);
    put8(bytes, faces, 1);
    put8(bytes, faces, 0);
    put8(bytes, faces, 1);
    put8(bytes, faces, 2);
    put8(bytes, faces, 20);
    put8(bytes, faces, 3);
    put8(bytes, faces, 0);
    put8(bytes, faces, 14);
    put8(bytes, faces, 0);
    put8(bytes, faces, 0);
    put8(bytes, faces, 127);
    put8(bytes, faces, 0);
    put8(bytes, faces, 2);
    put8(bytes, faces, 1);
    put8(bytes, faces, 0xfe);
    put8(bytes, faces, 0);

    auto colours = std::uint32_t{0x038200};
    for (std::uint16_t index = 0; index < 15; ++index) {
        put16(bytes, colours, static_cast<std::uint16_t>(
            0x3f00U | (index << 4U) | index));
    }
    return starfox::assets::RomImage{std::move(bytes)};
}

starfox::assets::RomImage make_empty_controller_rom() {
    std::vector<std::uint8_t> bytes(0x8000);
    // A source ShapeHdr whose point and face pointers are both zero denotes
    // a strategy-only controller rather than a visible model.
    auto header = std::uint32_t{0x008100};
    for (std::size_t byte = 0; byte < 28U; ++byte) put8(bytes, header, 0U);
    return starfox::assets::RomImage{std::move(bytes)};
}

} // namespace

int main() {
    const auto parsed_symbols = starfox::assets::SymbolMap::parse(
        "EXAMPLE $123456\r\nignored report row\r\nexample $00abcd\r\n");
    require(parsed_symbols.find("example").size() == 2
            && parsed_symbols.find("EXAMPLE")[0] == 0x123456U
            && parsed_symbols.find("example")[1] == 0x00abcdU,
        "memory-backed symbol parsing diverged from file parsing");

    const auto rom = make_rom();
    const starfox::assets::ShapeDecoder decoder{rom};
    const auto shape = decoder.decode(0x008100, "triangle");
    require(shape.vertices.size() == 3, "point stream did not decode three vertices");
    require(shape.faces.size() == 1, "face stream did not decode one face");
    require(shape.bsp_nodes.size() == 1, "BSP node was not preserved");
    require(shape.bsp_leaves.size() == 2, "BSP leaves were not preserved");
    require(shape.faces[0].vertex_indices.size() == 3, "face vertex count is wrong");
    require(shape.colour_words.size() == 15, "colour table range is wrong");
    require(shape.colour_materials[14].animation_frames
                == std::vector<std::uint16_t>{0x3f11U, 0x3f22U},
            "COLANIM table was not preserved losslessly");

    const auto empty_rom = make_empty_controller_rom();
    const auto empty = starfox::assets::ShapeDecoder{empty_rom}.decode(
        0x008100, "controller");
    require(empty.vertices.empty() && empty.faces.empty(),
            "zero-geometry controller header was rejected as an invalid model");

    starfox::render::Framebuffer framebuffer{224, 192};
    starfox::render::SoftwareRenderer renderer{{180.0, false, 0}};
    renderer.draw(shape, {}, framebuffer);
    std::size_t coloured_pixels = 0;
    for (const auto pixel : framebuffer.pixels()) {
        coloured_pixels += pixel != 0;
    }
    require(coloured_pixels > 50, "decoded shape did not render a visible polygon");
    starfox::render::Framebuffer alternate_frame{224, 192};
    starfox::render::RenderPose alternate_pose;
    alternate_pose.colour_frame = 1;
    renderer.draw(shape, alternate_pose, alternate_frame);
    require(framebuffer.pixels() != alternate_frame.pixels(),
            "renderer did not select the requested colour-animation frame");
    auto exploding_shape = shape;
    exploding_shape.bsp_root_address = 0U;
    exploding_shape.faces[0].visibility_index = -1;
    exploding_shape.faces[0].normal = {12, -8, 0};
    starfox::render::RenderPose explosion_pose;
    explosion_pose.explosion_progress = 8U;
    starfox::render::Framebuffer explosion_frame{224, 192};
    renderer.draw(exploding_shape, explosion_pose, explosion_frame);
    starfox::render::Framebuffer intact_frame{224, 192};
    renderer.draw(exploding_shape, {}, intact_frame);
    require(explosion_frame.pixels() != intact_frame.pixels(),
            "afexp model did not separate its faces as al_count advanced");
    starfox::render::Framebuffer shadow_frame{224, 192};
    starfox::render::RenderPose shadow_pose;
    shadow_pose.force_colour = true;
    shadow_pose.forced_colour = 0x09U;
    renderer.draw(shape, shadow_pose, shadow_frame);
    require(std::find(shadow_frame.pixels().begin(), shadow_frame.pixels().end(), 9U)
                != shadow_frame.pixels().end(),
            "ordinary shadow did not use the source palette colour");
    require(std::none_of(shadow_frame.pixels().begin(), shadow_frame.pixels().end(),
                [](std::uint8_t pixel) { return pixel != 0U && pixel != 9U; }),
            "ordinary shadow retained a model material");

    auto clipped_shape = shape;
    clipped_shape.bsp_root_address = 0;
    clipped_shape.frames.clear();
    clipped_shape.vertices = {
        {-20, -10, -10},
        {20, -10, 10},
        {0, 20, 10},
    };
    clipped_shape.faces[0].visibility_index = -1;
    starfox::render::RenderPose near_pose;
    near_pose.z = 0.0;
    near_pose.force_colour = true;
    near_pose.forced_colour = 0x05U;
    starfox::render::Framebuffer clipped_frame{224, 192};
    renderer.draw(clipped_shape, near_pose, clipped_frame);
    require(std::any_of(clipped_frame.pixels().begin(), clipped_frame.pixels().end(),
                [](std::uint8_t pixel) { return pixel != 0U; }),
            "untextured polygon crossing z=0 was not clipped and drawn");

    auto clipped_line = clipped_shape;
    clipped_line.vertices[0] = {0, 0, -10};
    clipped_line.vertices[1] = {0, 0, 10};
    clipped_line.faces[0].vertex_indices = {0, 1};
    starfox::render::Framebuffer clipped_line_frame{224, 192};
    renderer.draw(clipped_line, near_pose, clipped_line_frame);
    require(std::any_of(clipped_line_frame.pixels().begin(),
                clipped_line_frame.pixels().end(),
                [](std::uint8_t pixel) { return pixel != 0U; }),
            "line crossing z=0 was not clipped and drawn");

    auto tapered_beam = clipped_shape;
    tapered_beam.header.shift = 7U;
    tapered_beam.vertices = {
        {0, 0, 0}, {-1, 0, -10}, {0, 0, 0},
        {1, 0, -10}, {0, -2, -10}, {0, 2, -10},
    };
    starfox::render::RenderPose beam_pose;
    beam_pose.x = -350.0;
    beam_pose.z = 900.0;
    beam_pose.force_colour = true;
    beam_pose.forced_colour = 0x05U;
    beam_pose.collapse_to_axis_line = true;
    starfox::render::Framebuffer beam_frame{224, 192};
    renderer.draw(tapered_beam, beam_pose, beam_frame);
    const auto beam_pixels = std::count_if(beam_frame.pixels().begin(),
        beam_frame.pixels().end(), [](std::uint8_t pixel) { return pixel != 0U; });
    require(beam_pixels > 8U && beam_pixels <= 224U,
            "near-camera tapered beam expanded beyond its centre axis");

    auto clipped_texture = clipped_shape;
    const auto texture_colour = clipped_texture.faces[0].colour_id;
    clipped_texture.colour_words[texture_colour] = 0x4000U;
    clipped_texture.colour_materials[texture_colour].animation_frames.clear();
    clipped_texture.textures.push_back({
        0x4000U,
        0,
        1,
        1,
        {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}},
        {1, 1, 1, 1},
    });
    starfox::render::Framebuffer clipped_texture_frame{224, 192};
    auto texture_near_pose = near_pose;
    texture_near_pose.force_colour = false;
    renderer.draw(clipped_texture, texture_near_pose, clipped_texture_frame);
    require(std::none_of(clipped_texture_frame.pixels().begin(),
                clipped_texture_frame.pixels().end(),
                [](std::uint8_t pixel) { return pixel != 0U; }),
            "texture map crossing z=0 was drawn instead of source-discarded");

    auto bottom_clipped_shape = shape;
    bottom_clipped_shape.bsp_root_address = 0;
    bottom_clipped_shape.frames.clear();
    bottom_clipped_shape.vertices = {
        {-20, 90, 0},
        {0, 110, 0},
        {20, 90, 0},
    };
    bottom_clipped_shape.faces[0].visibility_index = -1;
    bottom_clipped_shape.faces[0].vertex_indices = {0, 1, 2};
    starfox::render::RenderPose bottom_pose;
    bottom_pose.z = 256.0;
    bottom_pose.force_colour = true;
    bottom_pose.forced_colour = 0x55U;
    starfox::render::Framebuffer bottom_clipped_frame{224, 192};
    starfox::render::SoftwareRenderer source_projection{{256.0, false, 0}};
    source_projection.draw(
        bottom_clipped_shape, bottom_pose, bottom_clipped_frame);
    require(std::any_of(bottom_clipped_frame.pixels().begin() + 191U * 224U,
                bottom_clipped_frame.pixels().end(),
                [](std::uint8_t pixel) { return pixel == 5U; }),
            "exclusive source bottom clip dropped scanline 191");

    auto expanded_side_shape = shape;
    expanded_side_shape.bsp_root_address = 0;
    expanded_side_shape.frames.clear();
    expanded_side_shape.vertices = {
        {112, -20, 0},
        {140, 0, 0},
        {112, 20, 0},
    };
    expanded_side_shape.faces[0].visibility_index = -1;
    expanded_side_shape.faces[0].vertex_indices = {0, 2, 1};
    starfox::render::RenderPose expanded_side_pose;
    expanded_side_pose.z = 256.0;
    expanded_side_pose.vanish_x = 128.0;
    expanded_side_pose.vanish_y = 96.0;
    expanded_side_pose.force_colour = true;
    expanded_side_pose.forced_colour = 0x05U;
    expanded_side_pose.use_rotation_matrix = true;
    expanded_side_pose.rotation_matrix = {
        32'767, 0, 0,
        0, 32'767, 0,
        0, 0, 32'767,
    };
    starfox::render::Framebuffer expanded_side_frame{256, 192};
    source_projection.draw(
        expanded_side_shape, expanded_side_pose, expanded_side_frame);
    auto expanded_pixels = std::size_t{};
    auto leaked_pixels = std::size_t{};
    for (std::uint32_t y = 0; y < expanded_side_frame.height(); ++y) {
        for (std::uint32_t x = 0; x < expanded_side_frame.width(); ++x) {
            if (expanded_side_frame.get(x, y) != 5U) continue;
            ++expanded_pixels;
            leaked_pixels += x < 224U;
        }
    }
    require(expanded_pixels > 100U,
            "model crossing the expanded right column was not drawn");
    require(leaked_pixels == 0U,
            "expanded right-column edge wrapped into the original viewport");

    auto superwide_side_shape = expanded_side_shape;
    superwide_side_shape.vertices = {
        {360, -20, 0},
        {400, 0, 0},
        {360, 20, 0},
    };
    auto superwide_side_pose = expanded_side_pose;
    superwide_side_pose.vanish_x = 400.0;
    starfox::render::Framebuffer superwide_side_frame{800, 224};
    source_projection.draw(
        superwide_side_shape, superwide_side_pose, superwide_side_frame);
    auto superwide_pixels = std::size_t{};
    auto superwide_wrapped_pixels = std::size_t{};
    for (std::uint32_t y = 0; y < superwide_side_frame.height(); ++y) {
        for (std::uint32_t x = 0; x < superwide_side_frame.width(); ++x) {
            if (superwide_side_frame.get(x, y) != 5U) continue;
            ++superwide_pixels;
            superwide_wrapped_pixels += x < 700U;
        }
    }
    require(superwide_pixels > 100U,
            "model in the 32:9 outer column was not drawn");
    require(superwide_wrapped_pixels == 0U,
            "32:9 outer-column edge wrapped into the centred viewport");

    auto expanded_top_shape = shape;
    expanded_top_shape.bsp_root_address = 0;
    expanded_top_shape.frames.clear();
    expanded_top_shape.vertices = {
        {-20, -120, 0},
        {0, -100, 0},
        {20, -120, 0},
    };
    expanded_top_shape.faces[0].visibility_index = -1;
    expanded_top_shape.faces[0].vertex_indices = {0, 1, 2};
    auto expanded_top_pose = expanded_side_pose;
    expanded_top_pose.vanish_x = 200.0;
    expanded_top_pose.vanish_y = 112.0;
    starfox::render::Framebuffer expanded_top_frame{400, 224};
    source_projection.draw(
        expanded_top_shape, expanded_top_pose, expanded_top_frame);
    require(std::any_of(expanded_top_frame.pixels().begin(),
                expanded_top_frame.pixels().begin() + 16U * 400U,
                [](std::uint8_t pixel) { return pixel == 5U; }),
            "model crossing the two added top rows was still clipped");

    auto source_line_shape = shape;
    source_line_shape.bsp_root_address = 0;
    source_line_shape.frames.clear();
    source_line_shape.vertices = {{-12, 4, 0}, {-8, 6, 0}};
    source_line_shape.faces[0].visibility_index = -1;
    source_line_shape.faces[0].vertex_indices = {0, 1};
    starfox::render::Framebuffer source_line_frame{224, 192};
    source_projection.draw(source_line_shape, bottom_pose, source_line_frame);
    require(source_line_frame.get(100, 100) == 5U
                && source_line_frame.get(101, 100) == 5U
                && source_line_frame.get(102, 101) == 5U
                && source_line_frame.get(103, 101) == 5U
                && source_line_frame.get(104, 102) == 5U
                && source_line_frame.get(101, 101) == 0U,
            "source mline half-slope tie rule diverged");

    auto sprite_face_shape = shape;
    sprite_face_shape.bsp_root_address = 0;
    sprite_face_shape.frames.clear();
    sprite_face_shape.vertices = {{0, 0, 0}};
    sprite_face_shape.faces[0].visibility_index = -1;
    sprite_face_shape.faces[0].vertex_indices = {0};
    sprite_face_shape.faces[0].sprite = true;
    const auto sprite_colour = sprite_face_shape.faces[0].colour_id;
    sprite_face_shape.colour_words[sprite_colour] = 0x4000U;
    sprite_face_shape.colour_materials[sprite_colour].animation_frames.clear();
    sprite_face_shape.textures = {{
        0x4000U, 0U, 31U, 31U, {}, std::vector<std::uint8_t>(32U * 32U, 7U)}};
    starfox::render::RenderPose sprite_face_pose;
    sprite_face_pose.z = 256.0;
    starfox::render::Framebuffer sprite_face_frame{224, 192};
    source_projection.draw(
        sprite_face_shape, sprite_face_pose, sprite_face_frame);
    require(std::count(sprite_face_frame.pixels().begin(),
                sprite_face_frame.pixels().end(), 7U) == 32 * 32
                && sprite_face_frame.get(96, 80) == 7U
                && sprite_face_frame.get(127, 111) == 7U
                && sprite_face_frame.get(128, 112) == 0U,
            "source angle-zero 8.8 sprite-face scaler diverged");

    const auto animated_rom = make_animated_rom();
    const starfox::assets::ShapeDecoder animated_decoder{animated_rom};
    const auto animated = animated_decoder.decode(0x008100, "animated_triangle");
    require(animated.frames.size() == 2, "animated point table frame count is wrong");
    require(animated.declared_frame_count == 2, "declared frame count was not preserved");
    require(animated.frames[0].vertices.size() == 3,
            "static point prefix was not included in animation frame");
    require(animated.frames[0].vertices[1] != animated.frames[1].vertices[1],
            "animation frames decoded to the same point set");
    std::cout << "All shape decoder tests passed.\n";
    return 0;
}
