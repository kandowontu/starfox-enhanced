#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/simulation/math.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

// Expected pixels come from executing the cartridge in a separately built
// Snes9x core, never from this renderer. See tools/reference/README.md.
int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::runtime_error("Expected ROM, symbols and reference CSV");
        auto rom = starfox::assets::RomImage::load(argv[1]);
        auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        starfox::assets::ShapeDecoder decoder(rom, symbols);
        const auto trig = starfox::simulation::TrigTables::load(rom, symbols);
        starfox::render::SoftwareRenderer renderer;
        std::ifstream input(argv[3]);
        if (!input) throw std::runtime_error("Cannot read reference CSV");
        std::string line;
        std::getline(input, line);
        const bool sprites = line.starts_with("shape,address,adjustment,x,y,z,");
        if (!sprites && !line.starts_with("shape,address,frame,yaw,depth,"))
            throw std::runtime_error("Unexpected reference CSV schema");
        unsigned cases = 0;
        starfox::assets::Shape shape;
        while (std::getline(input, line)) {
            if (line.empty()) continue;
            std::stringstream stream(line);
            std::vector<std::string> fields;
            std::string field;
            while (std::getline(stream, field, ',')) fields.push_back(field);
            if (fields.size() != (sprites ? 14 : 22)) throw std::runtime_error("Incomplete reference row");
            if (shape.name != fields[0]) shape = decoder.decode_by_name(symbols, fields[0]);
            if (shape.header.address != std::stoul(fields[1]))
                throw std::runtime_error("Reference cartridge/symbol mismatch: " + fields[0]);
            const auto hash_index = sprites ? 12U : 9U;
            const auto pixels_index = sprites ? 8U : 5U;
            if (fields[hash_index] != fields[hash_index + 1]
                || fields[hash_index - 2] != "0" || fields[hash_index - 1] != "0")
                throw std::runtime_error("Golden row must be independently verified");
            starfox::render::RenderPose pose;
            if (sprites) {
                pose.x = std::stod(fields[3]); pose.y = std::stod(fields[4]);
                pose.z = std::stod(fields[5]);
                pose.simple_scaled_sprite = true;
                pose.simple_sprite_world_size = decoder.simple_sprite_diameter(
                    shape.header, static_cast<int8_t>(std::stoi(fields[2])));
                pose.simple_sprite_colour = static_cast<uint8_t>(std::stoi(fields[6]));
                pose.colour_frame = static_cast<uint32_t>(std::stoul(fields[7]));
            } else {
                pose.z = std::stod(fields[4]);
                pose.animation_frame = static_cast<uint32_t>(std::stoul(fields[2]));
                pose.use_rotation_matrix = true;
                const auto yaw = static_cast<uint16_t>(std::stoul(fields[3]));
                const auto object = starfox::simulation::transpose_q15(
                    starfox::simulation::rotation_matrix_q15(trig, 0,
                        starfox::simulation::wrap16(-static_cast<int>(yaw)), 0));
                constexpr starfox::simulation::MatrixQ15 world{
                    32767, 0, 0, 0, 32767, 0, 0, 0, 32767};
                pose.rotation_matrix = starfox::simulation::compose_model_matrix_q15(
                    object, world, 0, yaw, 0);
                for (unsigned i = 0; i < 9; ++i) {
                    if (pose.rotation_matrix[i] != static_cast<int16_t>(std::stoi(fields[13 + i])))
                        throw std::runtime_error("Cartridge model matrix differs: " + fields[0]);
                }
                starfox::render::apply_source_depth_tables(rom, symbols.find("DEPTHTABLES").at(0),
                    static_cast<uint16_t>(std::stoul(fields[11])),
                    static_cast<uint16_t>(std::stoul(fields[12])), 0, pose);
            }
            starfox::render::Framebuffer frame(224, 192);
            renderer.draw(shape, pose, frame, true);
            uint64_t actual = 14695981039346656037ULL;
            unsigned pixels = 0;
            for (auto pixel : frame.pixels()) {
                pixel &= 15;
                pixels += pixel != 0;
                actual ^= pixel;
                actual *= 1099511628211ULL;
            }
            if (actual != std::stoull(fields[hash_index]) || pixels != std::stoul(fields[pixels_index])) {
                throw std::runtime_error("Cartridge pixels differ: " + fields[0]
                    + (sprites ? " sprite adjustment=" : " frame=") + fields[2]
                    + " row=" + std::to_string(cases + 2));
            }
            ++cases;
        }
        if (cases < 100) throw std::runtime_error("Reference suite is unexpectedly incomplete");
        std::cout << cases << " independent cartridge framebuffer hashes passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
