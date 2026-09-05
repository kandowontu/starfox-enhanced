// Execute the cartridge's matrix and world-point routines independently of the
// port. Numeric output is suitable for tests without distributing the core.
#pragma once
#include "starfox/simulation/math.hpp"
#include <random>

inline int audit_math(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols, uint8_t* ram,
    unsigned (*gsu)(unsigned, unsigned),
    unsigned (*gsu_call)(unsigned, unsigned, unsigned),
    const std::string& mode, const char* output_path) {
    using namespace starfox;
    const bool points = mode == "points";
    if (!points && !gsu_call) throw std::runtime_error("Reference call bridge is not installed");
    const auto address = [&](const std::string& name) {
        const auto matches = symbols.find(name);
        if (matches.empty()) throw std::runtime_error("Missing symbol: " + name);
        return matches.front();
    };
    const auto word = [&](unsigned location) {
        return simulation::wrap16(ram[location & 65535]
            | (unsigned(ram[(location + 1) & 65535]) << 8));
    };
    const auto put = [&](unsigned location, unsigned value) {
        ram[location & 65535] = static_cast<uint8_t>(value);
        ram[(location + 1) & 65535] = static_cast<uint8_t>(value >> 8);
    };
    const std::vector<uint8_t> baseline(ram, ram + 0x10000);
    const auto trig = simulation::TrigTables::load(rom, symbols);
    assets::ShapeDecoder decoder(rom, symbols);
    const auto shape = decoder.decode_by_name(symbols, "SHIP_4");
    const auto stop = address("MPRTDEC") - 2;
    if (rom.read16(stop) != 0x0100) throw std::runtime_error("Missing native STOP");
    std::ofstream output(output_path);
    if (!output) throw std::runtime_error("Cannot create output CSV");
    output << "camera_x,camera_y,camera_z";
    if (!points) output << ",object_x,object_y,object_z,shadow";
    for (unsigned i = 0; i < 9; ++i) output << ",world" << i;
    if (points) output << ",x,y,z,result_x,result_y,result_z";
    else for (unsigned i = 0; i < 9; ++i) output << ",model" << i;
    output << '\n';

    std::vector<std::array<uint16_t, 3>> cameras{
        {0, 0, 0}, {0, 0x4000, 0}, {0, 0x8000, 0}, {0x4000, 0, 0},
        {0, 0, 0x4000}, {0xffff, 0xff01, 0x40ff}, {0x1234, 0x8765, 0x4321}};
    std::mt19937 random(0x65816);
    for (unsigned i = 0; i < 200; ++i)
        cameras.push_back({uint16_t(random()), uint16_t(random()), uint16_t(random())});
    const std::array<std::array<uint16_t, 3>, 9> objects{{
        {0, 0, 0}, {0, 0x8000, 0}, {0, 0x4000, 0}, {0x4000, 0, 0},
        {0, 0, 0x4000}, {0x1200, 0x8700, 0x4300}, {0xff00, 0x0100, 0},
        {0, 0xff00, 0x8000}, {0x8000, 0x8000, 0x8000}}};
    std::vector<std::array<int16_t, 3>> positions{
        {0, 0, 0}, {1, 1, 1}, {-1, -1, -1}, {0, 0, 1700},
        {100, -200, 600}, {-200, 100, 4500}, {32767, 32767, 32767},
        {-32768, -32768, -32768}, {32767, -32768, 32767},
        {32767, 0, 0}, {0, 32767, 0}, {0, 0, 32767},
        {-32768, 0, 0}, {0, -32768, 0}, {0, 0, -32768}};
    for (unsigned i = 0; i < 32; ++i)
        positions.push_back({simulation::wrap16(random()), simulation::wrap16(random()),
            simulation::wrap16(random())});
    unsigned world_bad = 0, result_bad = 0, total = 0;
    for (const auto camera : cameras) {
        std::copy(baseline.begin(), baseline.end(), ram);
        for (unsigned i = 0; i < 3; ++i) put(address("M_ROTX") + 2 * i, camera[i]);
        if (gsu(address("MCROTWMATZXY16"), 0) >= 10000000)
            throw std::runtime_error("World matrix call timed out");
        simulation::MatrixQ15 world{};
        for (unsigned i = 0; i < 9; ++i) world[i] = word(address("M_WMAT11") + 2 * i);
        const auto port_world = simulation::rotation_matrix_q15(trig,
            simulation::wrap16(camera[0]), simulation::wrap16(camera[1]),
            simulation::wrap16(camera[2]));
        world_bad += world != port_world;
        const auto restore_world = [&] {
            std::copy(baseline.begin(), baseline.end(), ram);
            for (unsigned i = 0; i < 9; ++i) put(address("M_WMAT11") + 2 * i, world[i]);
        };
        if (points) {
            for (const auto position : positions) {
                restore_world();
                // M_X1 is scratch-aliased elsewhere in SRAM; Y1/Z1 are not
                // adjacent to it in either cartridge's layout.
                put(address("M_X1"), position[0]);
                put(address("M_Y1"), position[1]);
                put(address("M_Z1"), position[2]);
                if (gsu(address("MWMATROTP16"), 0) >= 10000000)
                    throw std::runtime_error("World point call timed out");
                std::array<int16_t, 3> result{};
                for (unsigned i = 0; i < 3; ++i) result[i] = word(address("M_BIGX") + 2 * i);
                result_bad += result != simulation::transform_q15(port_world, position);
                ++total;
                output << camera[0] << ',' << camera[1] << ',' << camera[2];
                for (auto value : world) output << ',' << value;
                for (auto value : position) output << ',' << value;
                for (auto value : result) output << ',' << value;
                output << '\n';
            }
            continue;
        }
        for (const auto object : objects) for (unsigned shadow = 0; shadow < 2; ++shadow) {
            restore_world();
            for (unsigned i = 0; i < 3; ++i) put(address("M_ROTX") + 2 * i, object[i] >> 8);
            for (auto name : {"M_BIGX", "M_BIGY", "M_FRAMENUM", "M_COLFRAME", "M_DEPTHOFFSET"})
                put(address(name), 0);
            put(address("M_BIGZ"), 1700);
            put(address("M_SHAPEPTR"), address("SHIP_4"));
            put(address("M_XLEFT"), 0); put(address("M_XRIGHT"), 223);
            put(address("M_YTOP"), 0); put(address("M_YBOT"), 191);
            put(address("M_VANISHX"), 112); put(address("M_VANISHY"), 96);
            if (shadow) {
                put(address("M_SHIFT"), shape.header.shift);
                put(address("M_PNTPTR"), shape.header.points_address);
                put(address("M_FACEPTR"), shape.header.faces_address);
                put(address("M_SHAPEBANK"), shape.header.points_address >> 16);
                put(address("M_COLOURPTR"), shape.header.colour_pointer);
                put(address("M_OBJFLAGS"), 0);
                if (gsu_call(address("MSHOWSHADOW"), address("M_STACK") & 65535, stop & 65535) >= 10000000)
                    throw std::runtime_error("Shadow call timed out");
            } else if (gsu(address("MSHOWOBJ3"), 0) >= 10000000)
                throw std::runtime_error("Model call timed out");
            simulation::MatrixQ15 model{};
            for (unsigned i = 0; i < 9; ++i) model[i] = word(address("M_MAT11") + 2 * i);
            const auto rotation = simulation::transpose_q15(simulation::rotation_matrix_q15(trig,
                simulation::wrap16(-int(object[0])), simulation::wrap16(-int(object[1])),
                simulation::wrap16(-int(object[2]))));
            const auto port_model = simulation::compose_model_matrix_q15(rotation, port_world,
                object[0], object[1], object[2], shadow != 0);
            result_bad += model != port_model;
            ++total;
            output << camera[0] << ',' << camera[1] << ',' << camera[2];
            for (auto value : object) output << ',' << value;
            output << ',' << shadow;
            for (auto value : world) output << ',' << value;
            for (auto value : model) output << ',' << value;
            output << '\n';
        }
    }
    std::cout << cameras.size() << " worlds, " << world_bad << " different; "
        << total << (points ? " points, " : " models/shadows, ") << result_bad << " different\n";
    return world_bad || result_bad ? 1 : 0;
}
