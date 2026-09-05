#include "starfox/simulation/math.hpp"
#include "starfox/simulation/wdc65816.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

// Expected words are produced by the cartridge's GSU code in the independent
// reference core. See tools/reference/README.md for inputs and regeneration.
int main(int argc, char** argv) {
    try {
        using namespace starfox::simulation;
        if (argc != 5) throw std::runtime_error("Expected ROM, symbols, matrices CSV and points CSV");
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const auto trig = TrigTables::load(rom, symbols);
        Wdc65816 cpu(rom, &symbols);
        const auto address = [&](const char* name) { return symbols.find(name).at(0); };
        const auto launch = [&](const char* name) {
            const auto entry = address(name);
            cpu.write8(0x3034, static_cast<uint8_t>(entry >> 16));
            cpu.write16(0x301e, static_cast<uint16_t>(entry));
        };
        for (unsigned points = 0; points < 2; ++points) {
            std::ifstream input(argv[3 + points]);
            if (!input) throw std::runtime_error("Cannot read reference CSV");
            std::string line;
            std::getline(input, line);
            const std::string prefix = points ? "camera_x,camera_y,camera_z,world0,"
                : "camera_x,camera_y,camera_z,object_x,object_y,object_z,shadow,";
            if (!line.starts_with(prefix)) throw std::runtime_error("Unexpected reference CSV schema");
            unsigned count = 0;
            while (std::getline(input, line)) {
                if (line.empty()) continue;
                std::stringstream stream(line);
                std::vector<int> values;
                std::string field;
                while (std::getline(stream, field, ',')) values.push_back(std::stoi(field));
                if (values.size() != (points ? 18U : 25U))
                    throw std::runtime_error("Incomplete reference row");
                const auto world = rotation_matrix_q15(trig,
                    wrap16(values[0]), wrap16(values[1]), wrap16(values[2]));
                const auto world_index = points ? 3U : 7U;
                cpu.write16(address("M_ROTX"), static_cast<uint16_t>(values[0]));
                cpu.write16(address("M_ROTY"), static_cast<uint16_t>(values[1]));
                cpu.write16(address("M_ROTZ"), static_cast<uint16_t>(values[2]));
                launch("MCROTWMATZXY16");
                for (unsigned i = 0; i < 9; ++i) {
                    if (world[i] != values[world_index + i]
                        || wrap16(cpu.read16(address("M_WMAT11") + 2 * i)) != values[world_index + i])
                        throw std::runtime_error("Camera matrix differs at row " + std::to_string(count + 2));
                }
                if (points) {
                    const auto result = transform_q15(world,
                        {wrap16(values[12]), wrap16(values[13]), wrap16(values[14])});
                    cpu.write16(address("M_X1"), static_cast<uint16_t>(values[12]));
                    cpu.write16(address("M_Y1"), static_cast<uint16_t>(values[13]));
                    cpu.write16(address("M_Z1"), static_cast<uint16_t>(values[14]));
                    launch("MWMATROTP16");
                    for (unsigned i = 0; i < 3; ++i) {
                        if (result[i] != values[15 + i]
                            || wrap16(cpu.read16(address("M_BIGX") + 2 * i)) != values[15 + i])
                            throw std::runtime_error("World point differs at row " + std::to_string(count + 2));
                    }
                } else {
                    const auto object = transpose_q15(rotation_matrix_q15(trig,
                        wrap16(-values[3]), wrap16(-values[4]), wrap16(-values[5])));
                    const auto result = compose_model_matrix_q15(object, world,
                        values[3], values[4], values[5], values[6] != 0);
                    for (unsigned i = 0; i < 9; ++i) {
                        if (result[i] != values[16 + i])
                            throw std::runtime_error("Model/shadow matrix differs at row " + std::to_string(count + 2));
                    }
                }
                ++count;
            }
            if (count != (points ? 9729U : 3726U))
                throw std::runtime_error("Reference suite is incomplete");
            std::cout << count << (points ? " world points" : " model/shadow matrices")
                << " match independent cartridge words\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
