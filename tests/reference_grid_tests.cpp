#include "starfox/render/dust_renderer.hpp"
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        using namespace starfox;
        if (argc != 4) throw std::runtime_error("Expected ROM, symbols and grid reference CSV");
        const auto rom = assets::RomImage::load(argv[1]);
        const auto symbols = assets::SymbolMap::load(argv[2]);
        const auto trig = simulation::TrigTables::load(rom, symbols);
        std::ifstream input(argv[3]);
        if (!input) throw std::runtime_error("Cannot read grid reference CSV");
        std::string line;
        std::getline(input, line);
        if (!line.starts_with("lines,frame,x,y,z,pitch,yaw,roll,"))
            throw std::runtime_error("Unexpected grid reference schema");
        std::optional<render::DustRenderer> renderer;
        unsigned cases = 0;
        int previous_frame = -1, previous_mode = -1;
        while (std::getline(input, line)) {
            if (line.empty()) continue;
            std::stringstream stream(line);
            std::vector<std::string> fields;
            std::string field;
            while (std::getline(stream, field, ',')) fields.push_back(field);
            if (fields.size() != 11 || fields[8] != fields[9] || fields[10] != "0")
                throw std::runtime_error("Incomplete or unverified grid reference row");
            const int mode = std::stoi(fields[0]), frame = std::stoi(fields[1]);
            if (frame == 0) {
                if (mode != previous_mode + 1) throw std::runtime_error("Grid modes are incomplete");
                renderer.emplace(rom, symbols);
                previous_mode = mode;
            } else if (mode != previous_mode || frame != previous_frame + 1)
                throw std::runtime_error("Grid sequence is incomplete");
            previous_frame = frame;
            const timing::RenderTransform camera{
                std::stod(fields[2]), std::stod(fields[3]), std::stod(fields[4])};
            const auto world = simulation::rotation_matrix_q15(trig,
                simulation::wrap16(std::stoi(fields[5])), simulation::wrap16(std::stoi(fields[6])),
                simulation::wrap16(std::stoi(fields[7])));
            // Repeated presentations must start at this source frame's saved
            // line origin instead of advancing EX's M_PREVX/M_PREVY twice.
            for (unsigned presentation = 0; presentation < 2; ++presentation) {
                render::Framebuffer pixels(224, 192);
                if (mode) renderer->draw_grid_lines(camera, world, frame, pixels);
                else renderer->draw_grid(camera, world, pixels);
                uint64_t hash = 14695981039346656037ULL;
                for (const auto pixel : pixels.pixels()) {
                    hash ^= pixel & 15;
                    hash *= 1099511628211ULL;
                }
                if (hash != std::stoull(fields[8]))
                    throw std::runtime_error("Ground grid differs at row " + std::to_string(cases + 2));
            }
            ++cases;
        }
        const auto expected = symbols.find("MSHOWGRID2").empty() ? 192U : 384U;
        if (cases != expected || previous_frame != 191)
            throw std::runtime_error("Grid reference suite is incomplete");
        std::cout << cases << " ground-grid frames match independent cartridge pixels\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
