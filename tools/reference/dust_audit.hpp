#pragma once
#include "starfox/render/dust_renderer.hpp"
#include "starfox/simulation/dust_system.hpp"

inline int audit_dust(const starfox::assets::RomImage& rom,
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
    const auto hash_bytes = [](const auto& bytes) {
        uint64_t value = 14695981039346656037ULL;
        for (const auto byte : bytes) { value ^= byte; value *= 1099511628211ULL; }
        return value;
    };
    const auto flush = address("MSHOW") - 4;
    // Return through the cartridge's own RPIX/STOP: this commits the final
    // cached tile, which MSHOWDUST normally leaves for MDO_3D_DISPLAY to flush.
    if (rom.read16(flush) != 0x4c3d || rom.read16(flush + 2) != 0x0100)
        throw std::runtime_error("Missing native pixel-cache flush/STOP");
    const std::vector<uint8_t> baseline(ram, ram + 0x10000);
    std::ofstream output(output_path);
    if (!output) throw std::runtime_error("Cannot create output CSV");
    output << "frame,count,camera_x,camera_y,camera_z,pitch,yaw,roll,planet_stars,"
        "vanish_x,vanish_y,point_hash,frame_hash,point_differences,pixel_differences\n";
    const auto trig = simulation::TrigTables::load(rom, symbols);
    const bool ex = !symbols.find("M_MOREDOTS").empty();
    unsigned total = 0, point_bad = 0, pixel_bad = 0;
    for (unsigned variant = 0; variant < (ex ? 2U : 1U); ++variant) {
        std::copy(baseline.begin(), baseline.end(), ram);
        const auto count = variant ? 511U : 120U;
        if (ex) put(address("M_MOREDOTS"), variant);
        if (gsu(address("MINITDUST"), 0) >= 10000000)
            throw std::runtime_error("Dust initialization timed out");
        simulation::DustSystem dust;
        render::DustRenderer renderer(rom, symbols);
        for (int frame = -1; frame < 192; ++frame) {
            const int step = std::max(frame, 0);
            const std::array<int16_t, 3> camera{simulation::wrap16(step * 257),
                simulation::wrap16(step * -29), simulation::wrap16(step * 971)};
            const std::array<int16_t, 3> angles{simulation::wrap16(step * 151),
                simulation::wrap16(step * 331), simulation::wrap16(step * 119)};
            render::DustRenderState state;
            state.planet_stars = static_cast<uint8_t>((step / 32) % 3);
            state.vanish_x = static_cast<int16_t>(112 + (step % 7 - 3) * 17);
            state.vanish_y = static_cast<int16_t>(96 + (step % 5 - 2) * 31);
            render::Framebuffer native(224, 192), port(224, 192);
            if (frame >= 0) {
                std::fill(ram + 0x4000, ram + 0x10000, 0);
                for (unsigned i = 0; i < 3; ++i) put(address("M_ROTX") + 2 * i, angles[i]);
                if (gsu(address("MCROTWMATZXY16"), 0) >= 10000000)
                    throw std::runtime_error("Dust camera call timed out");
                put(address("M_VIEWPOSX"), camera[0]);
                put(address("M_VIEWPOSY"), camera[1]);
                put(address("M_VIEWPOSZ"), camera[2]);
                put(address("M_VANISHX"), state.vanish_x);
                put(address("M_VANISHY"), state.vanish_y);
                put(address("M_PLANETSTARS"), state.planet_stars);
                if (gsu_call(address("MSHOWDUST"), address("M_STACK") & 65535,
                        flush & 65535) >= 10000000) throw std::runtime_error("Dust draw timed out");
                const auto world = simulation::rotation_matrix_q15(trig, angles[0], angles[1], angles[2]);
                dust.tick(camera, world, true, count);
                renderer.draw(dust, count, {double(camera[0]), double(camera[1]), double(camera[2])},
                    world, port, state);
            }
            unsigned point_difference = 0, pixel_difference = 0;
            for (unsigned i = 0; i < count; ++i) {
                const auto point = dust.points()[i];
                point_difference += word(address("M_DUSTPNTS") + 6 * i) != point.x
                    || word(address("M_DUSTPNTS") + 6 * i + 2) != point.y
                    || word(address("M_DUSTPNTS") + 6 * i + 4) != point.z;
            }
            if (frame >= 0) {
                for (unsigned y = 0; y < 192; ++y) for (unsigned x = 0; x < 224; ++x) {
                    unsigned pixel = 0;
                    for (unsigned plane = 0; plane < 4; ++plane) {
                        const auto location = 0x4000 + ((x / 8) * 24 + y / 8) * 32
                            + (y % 8) * 2 + (plane / 2) * 16 + plane % 2;
                        pixel |= ((ram[location] >> (7 - x % 8)) & 1) << plane;
                    }
                    native.set(x, y, static_cast<uint8_t>(pixel));
                    pixel_difference += pixel != (port.get(x, y) & 15);
                }
            }
            const auto point_hash = hash_bytes(std::span<const uint8_t>(
                ram + (address("M_DUSTPNTS") & 65535), count * 6));
            output << frame << ',' << count;
            for (auto value : camera) output << ',' << value;
            for (auto value : angles) output << ',' << value;
            output << ',' << unsigned(state.planet_stars) << ',' << state.vanish_x << ',' << state.vanish_y
                << ',' << point_hash << ',' << (frame < 0 ? 0 : hash_bytes(native.pixels()))
                << ',' << point_difference << ',' << pixel_difference << '\n';
            ++total;
            point_bad += point_difference != 0;
            pixel_bad += pixel_difference != 0;
        }
    }
    if (!output) throw std::runtime_error("Incomplete dust output");
    std::cout << total << " dust states, " << point_bad << " point mismatches, "
        << pixel_bad << " framebuffer mismatches\n";
    return point_bad || pixel_bad ? 1 : 0;
}
