#pragma once
#include "starfox/render/dust_renderer.hpp"

inline int audit_grid(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols, uint8_t* ram,
    unsigned (*gsu)(unsigned, unsigned),
    unsigned (*gsu_call)(unsigned, unsigned, unsigned), const char* output_path) {
    using namespace starfox;
    if (!gsu_call) throw std::runtime_error("Reference call bridge is not installed");
    const auto address = [&](const char* name) { return symbols.find(name).at(0); };
    const auto put = [&](unsigned location, unsigned value) {
        ram[location & 65535] = static_cast<uint8_t>(value);
        ram[(location + 1) & 65535] = static_cast<uint8_t>(value >> 8);
    };
    const auto word = [&](unsigned location) {
        return simulation::wrap16(ram[location & 65535]
            | (unsigned(ram[(location + 1) & 65535]) << 8));
    };
    const auto frame_hash = [](const render::Framebuffer& frame) {
        uint64_t value = 14695981039346656037ULL;
        for (const auto pixel : frame.pixels()) {
            value ^= pixel & 15;
            value *= 1099511628211ULL;
        }
        return value;
    };
    const auto flush = address("MSHOW") - 4;
    if (rom.read16(flush) != 0x4c3d || rom.read16(flush + 2) != 0x0100)
        throw std::runtime_error("Missing native pixel-cache flush/STOP");
    // Verify the CPU-origin constant independently of the GSU's loop count.
    const auto setup = address("MSHOWGRID_L");
    if (rom.read8(setup + 16) != 0xe9 || rom.read16(setup + 17) != 1920)
        throw std::runtime_error("Unexpected CPU ground-grid origin");
    const bool ex = !symbols.find("MSHOWGRID2").empty();
    const std::vector<uint8_t> baseline(ram, ram + 0x10000);
    const auto trig = simulation::TrigTables::load(rom, symbols);
    std::ofstream output(output_path);
    if (!output) throw std::runtime_error("Cannot create grid output CSV");
    output << "lines,frame,x,y,z,pitch,yaw,roll,native_hash,port_hash,different_pixels\n";
    unsigned cases = 0, bad = 0;
    for (unsigned lines = 0; lines < (ex ? 2U : 1U); ++lines) {
        std::copy(baseline.begin(), baseline.end(), ram);
        if (ex) { put(address("M_PREVX"), 0); put(address("M_PREVY"), 0); }
        render::DustRenderer renderer(rom, symbols);
        for (unsigned frame = 0; frame < 192; ++frame) {
            const std::array<int16_t, 3> camera{
                simulation::wrap16(frame * 257), simulation::wrap16(-128 - int(frame % 11) * 53),
                simulation::wrap16(frame * 971)};
            const std::array<int16_t, 3> angles = frame < 3 ? std::array<int16_t, 3>{0, 0, 0}
                : std::array<int16_t, 3>{simulation::wrap16((int(frame % 17) - 8) * 193),
                    simulation::wrap16(frame * 331), simulation::wrap16((int(frame % 23) - 11) * 129)};
            for (unsigned i = 0; i < 3; ++i) put(address("M_ROTX") + 2 * i, angles[i]);
            if (gsu(address("MCROTWMATZXY16"), 0) >= 10000000)
                throw std::runtime_error("Grid camera call timed out");
            const auto start = [](int16_t camera) {
                return simulation::wrap16(((uint16_t(camera) & 255) ^ 255) - 1920);
            };
            put(address("M_X1"), start(camera[0]));
            put(address("M_Y1"), -int(camera[1]));
            put(address("M_Z1"), start(camera[2]));
            if (gsu(address("MWMATROTP16"), 0) >= 10000000)
                throw std::runtime_error("Grid origin call timed out");
            put(address("M_X1"), word(address("M_BIGX")));
            put(address("M_Y1"), word(address("M_BIGY")));
            put(address("M_Z1"), word(address("M_BIGZ")));
            for (unsigned i = 0; i < 3; ++i) {
                constexpr const char* x_names[]{"M_PXX", "M_PXY", "M_PXZ"};
                constexpr const char* z_names[]{"M_PZX", "M_PZY", "M_PZZ"};
                put(address(x_names[i]), simulation::arithmetic_shift_right(
                    word(address("M_WMAT11") + 2 * i), 7));
                put(address(z_names[i]), simulation::arithmetic_shift_right(
                    word(address("M_WMAT11") + 12 + 2 * i), 7));
            }
            put(address("M_GRIDX"), uint16_t(camera[0]) >> 8);
            put(address("M_GRIDZ"), uint16_t(camera[2]) >> 8);
            std::fill(ram + 0x4000, ram + 0x10000, 0);
            if (gsu_call(address(lines ? "MSHOWGRID2" : "MSHOWGRID"), address("M_STACK") & 65535,
                    flush & 65535) >= 10000000) throw std::runtime_error("Grid draw timed out");
            const auto world = simulation::rotation_matrix_q15(trig, angles[0], angles[1], angles[2]);
            const timing::RenderTransform view{double(camera[0]), double(camera[1]), double(camera[2])};
            render::Framebuffer native(224, 192), port(224, 192);
            if (lines) renderer.draw_grid_lines(view, world, frame, port);
            else renderer.draw_grid(view, world, port);
            unsigned different = 0;
            for (unsigned y = 0; y < 192; ++y) for (unsigned x = 0; x < 224; ++x) {
                unsigned pixel = 0;
                for (unsigned plane = 0; plane < 4; ++plane) {
                    const auto location = 0x4000 + ((x / 8) * 24 + y / 8) * 32
                        + (y % 8) * 2 + (plane / 2) * 16 + plane % 2;
                    pixel |= ((ram[location] >> (7 - x % 8)) & 1) << plane;
                }
                native.set(x, y, static_cast<uint8_t>(pixel));
                different += pixel != (port.get(x, y) & 15);
            }
            output << lines << ',' << frame;
            for (auto value : camera) output << ',' << value;
            for (auto value : angles) output << ',' << value;
            output << ',' << frame_hash(native) << ',' << frame_hash(port) << ',' << different << '\n';
            ++cases;
            bad += different != 0;
        }
    }
    if (!output) throw std::runtime_error("Incomplete grid output");
    std::cout << cases << " grid frames, " << bad << " different\n";
    return bad ? 1 : 0;
}
