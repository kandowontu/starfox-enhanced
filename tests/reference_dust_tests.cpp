#include "starfox/render/dust_renderer.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

// Native point bytes and framebuffer hashes come from MINITDUST/MSHOWDUST
// in the independent reference core, including snow/pollen and EX More Dots.
int main(int argc, char** argv) {
    try {
        using namespace starfox;
        if (argc != 4) throw std::runtime_error("Expected ROM, symbols and dust reference CSV");
        const auto rom = assets::RomImage::load(argv[1]);
        const auto symbols = assets::SymbolMap::load(argv[2]);
        const auto trig = simulation::TrigTables::load(rom, symbols);
        render::DustRenderer renderer(rom, symbols);
        simulation::DustSystem dust;
        std::ifstream input(argv[3]);
        if (!input) throw std::runtime_error("Cannot read dust reference CSV");
        std::string line;
        std::getline(input, line);
        if (!line.starts_with("frame,count,camera_x,camera_y,camera_z,pitch,yaw,roll,"))
            throw std::runtime_error("Unexpected dust reference schema");
        unsigned cases = 0, previous_count = 0;
        int previous_frame = -2;
        while (std::getline(input, line)) {
            if (line.empty()) continue;
            std::stringstream stream(line);
            std::vector<std::string> fields;
            std::string field;
            while (std::getline(stream, field, ',')) fields.push_back(field);
            if (fields.size() != 15 || fields[13] != "0" || fields[14] != "0")
                throw std::runtime_error("Incomplete or unverified dust reference row");
            const auto frame = std::stoi(fields[0]);
            const auto count = static_cast<unsigned>(std::stoul(fields[1]));
            if (frame == -1) {
                if (count != 120 && count != 511) throw std::runtime_error("Unexpected dust count");
                dust.reset();
                previous_count = count;
            } else if (count != previous_count || frame != previous_frame + 1)
                throw std::runtime_error("Dust sequence is incomplete");
            previous_frame = frame;
            const std::array<int16_t, 3> camera{
                simulation::wrap16(std::stoi(fields[2])), simulation::wrap16(std::stoi(fields[3])),
                simulation::wrap16(std::stoi(fields[4]))};
            const auto world = simulation::rotation_matrix_q15(trig,
                simulation::wrap16(std::stoi(fields[5])), simulation::wrap16(std::stoi(fields[6])),
                simulation::wrap16(std::stoi(fields[7])));
            if (frame >= 0) dust.tick(camera, world, true, count);
            uint64_t point_hash = 14695981039346656037ULL;
            for (unsigned i = 0; i < count; ++i) {
                const auto point = dust.points()[i];
                for (auto value : {point.x, point.y, point.z}) {
                    for (unsigned shift : {0U, 8U}) {
                        point_hash ^= (static_cast<uint16_t>(value) >> shift) & 255U;
                        point_hash *= 1099511628211ULL;
                    }
                }
            }
            if (point_hash != std::stoull(fields[11]))
                throw std::runtime_error("Dust point state differs at row " + std::to_string(cases + 2));
            if (frame >= 0) {
                render::Framebuffer pixels(224, 192);
                render::DustRenderState state;
                state.planet_stars = static_cast<uint8_t>(std::stoul(fields[8]));
                state.vanish_x = simulation::wrap16(std::stoi(fields[9]));
                state.vanish_y = simulation::wrap16(std::stoi(fields[10]));
                renderer.draw(dust, count, {double(camera[0]), double(camera[1]), double(camera[2])},
                    world, pixels, state);
                uint64_t frame_hash = 14695981039346656037ULL;
                for (const auto pixel : pixels.pixels()) {
                    frame_hash ^= pixel & 15;
                    frame_hash *= 1099511628211ULL;
                }
                if (frame_hash != std::stoull(fields[12]))
                    throw std::runtime_error("Dust pixels differ at row " + std::to_string(cases + 2));
            }
            ++cases;
        }
        const auto expected = symbols.find("M_MOREDOTS").empty() ? 193U : 386U;
        if (cases != expected || previous_frame != 191)
            throw std::runtime_error("Dust reference suite is incomplete");
        std::cout << cases << " dust states match independent native points/pixels\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
