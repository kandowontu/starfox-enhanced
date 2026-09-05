// Independent cartridge renderer comparison. See tools/reference/README.md.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <libretro.h>
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/software_renderer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include "math_audit.hpp"
#include "dust_audit.hpp"
#include "grid_audit.hpp"
#ifdef STARFOX_REFERENCE_ARES
#include "ares_gsu.hpp"
#endif

using namespace starfox;
namespace {
#ifdef STARFOX_REFERENCE_ARES
reference::AresGsu* ares_core{};
std::ofstream timing_output;
unsigned call_sequence{};
unsigned call_ares(unsigned address, unsigned stack, unsigned return_address) {
    const auto result = ares_core->run(address, stack, return_address);
    if (timing_output.is_open()) {
        timing_output << call_sequence++ << ',' << address << ',' << stack << ','
            << return_address << ',' << result.instructions << ',' << result.master_clocks << '\n';
    }
    return result.instructions;
}
unsigned run_ares(unsigned address, unsigned) { return call_ares(address, 0, 0); }
#endif
bool environment(unsigned command, void* data) {
    switch (command) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_GEOMETRY: return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(data) = "."; return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool*>(data) = true; return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(data) = false; return true;
    default: return false;
    }
}
void video(const void*, unsigned, unsigned, size_t) {}
void sample(int16_t, int16_t) {}
size_t batch(const int16_t*, size_t frames) { return frames; }
void poll() {}
int16_t input(unsigned, unsigned, unsigned, unsigned) { return 0; }
uint64_t hash(const render::Framebuffer& frame) {
    uint64_t result = 14695981039346656037ULL;
    for (auto pixel : frame.pixels()) {
        result ^= pixel;
        result *= 1099511628211ULL;
    }
    return result;
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 6
#ifdef STARFOX_REFERENCE_ARES
            && argc != 7
#endif
        ) {
            std::cerr << "Usage: render_audit CORE_DLL ROM SYMBOLS all|all-frames|matrices|points|dust|grid|[sprites:]NAME,NAME OUTPUT.csv\n";
#ifdef STARFOX_REFERENCE_ARES
            std::cerr << "Ares build also accepts an optional final TIMING.csv path.\n";
#endif
            return 2;
        }
        auto rom = assets::RomImage::load(argv[2]);
        auto symbols = assets::SymbolMap::load(argv[3]);
        assets::ShapeDecoder decoder(rom, symbols);
        const auto trig = simulation::TrigTables::load(rom, symbols);
        const auto address = [&](const std::string& name) {
            const auto matches = symbols.find(name);
            if (matches.empty()) throw std::runtime_error("Missing symbol: " + name);
            return matches.front();
        };
        std::ifstream file(argv[2], std::ios::binary);
        std::vector<char> bytes{std::istreambuf_iterator<char>(file), {}};
        const auto core = LoadLibraryW(std::filesystem::absolute(argv[1]).c_str());
        if (!core) throw std::runtime_error("Cannot load reference DLL");
#define API(name) \
        const auto name = reinterpret_cast<decltype(&::name)>(GetProcAddress(core, #name)); \
        if (!name) throw std::runtime_error("Missing reference API: " #name)
        API(retro_init); API(retro_set_environment); API(retro_set_video_refresh);
        API(retro_set_audio_sample); API(retro_set_audio_sample_batch);
        API(retro_set_input_poll); API(retro_set_input_state); API(retro_load_game);
        API(retro_run); API(retro_get_memory_data); API(retro_get_memory_size);
        API(retro_unload_game); API(retro_deinit);
#undef API
#ifdef STARFOX_REFERENCE_ARES
        const auto gsu = &run_ares;
        const auto gsu_call = &call_ares;
        if (argc == 7) {
            timing_output.open(argv[6]);
            if (!timing_output) throw std::runtime_error("Cannot create timing CSV");
            timing_output << "call,address,stack,return_address,instructions,master_clocks\n";
        }
#else
        const auto gsu = reinterpret_cast<unsigned(*)(unsigned, unsigned)>(
            GetProcAddress(core, "retro_reference_gsu"));
        if (!gsu) throw std::runtime_error("Reference bridge is not installed");
        const auto gsu_call = reinterpret_cast<unsigned(*)(unsigned, unsigned, unsigned)>(
            GetProcAddress(core, "retro_reference_gsu_call"));
#endif
        retro_set_environment(environment);
        retro_set_video_refresh(video);
        retro_set_audio_sample(sample);
        retro_set_audio_sample_batch(batch);
        retro_set_input_poll(poll);
        retro_set_input_state(input);
        retro_init();
        retro_game_info game{argv[2], bytes.data(), bytes.size(), nullptr};
        if (!retro_load_game(&game)) throw std::runtime_error("Cannot load cartridge");
        for (unsigned frame = 0; frame < 600; ++frame) retro_run();
        auto* ram = static_cast<uint8_t*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
        if (!ram || retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) < 0x10000)
            throw std::runtime_error("Cartridge does not expose the expected GSU RAM");
#ifdef STARFOX_REFERENCE_ARES
        reference::AresGsu engine(rom.bytes(), std::span<uint8_t>(ram, 0x10000));
        ares_core = &engine;
#endif
        const std::vector<uint8_t> baseline(ram, ram + 0x10000);
        const auto word = [&](unsigned location) {
            location &= 65535;
            return unsigned(ram[location]) | (unsigned(ram[(location + 1) & 65535]) << 8);
        };
        const auto put = [&](unsigned location, unsigned value) {
            location &= 65535;
            ram[location] = static_cast<uint8_t>(value);
            ram[(location + 1) & 65535] = static_cast<uint8_t>(value >> 8);
        };
        const std::string mode = argv[4];
        if (mode == "matrices" || mode == "points" || mode == "dust" || mode == "grid") {
            const auto result = mode == "grid" ? audit_grid(rom, symbols, ram, gsu, gsu_call, argv[5])
                : mode == "dust" ? audit_dust(rom, symbols, ram, gsu, gsu_call, argv[5])
                : audit_math(rom, symbols, ram, gsu, gsu_call, mode, argv[5]);
            retro_unload_game();
            retro_deinit();
            FreeLibrary(core);
            return result;
        }
        const bool sprites = mode.starts_with("sprites:");
        if (sprites && !gsu_call) throw std::runtime_error("Reference call bridge is not installed");
        const bool census = mode == "all" || mode == "all-frames";
        std::vector<assets::Shape> shapes;
        if (census) {
            std::set<unsigned> seen;
            for (const auto& [name, addresses] : symbols.entries()) {
                for (const auto candidate : addresses) {
                    if (candidate < 0x8000 || candidate > 0xffff
                        || !seen.insert(candidate).second) continue;
                    try {
                        auto shape = decoder.decode(candidate, name);
                        if (!shape.header.compact && !shape.vertices.empty()
                            && !shape.faces.empty()) shapes.push_back(std::move(shape));
                    } catch (const std::exception&) {
                        // This is a symbol/format census, not an asset manifest.
                    }
                }
            }
        } else {
            std::stringstream names(sprites ? mode.substr(8) : mode);
            std::string name;
            while (std::getline(names, name, ','))
                shapes.push_back(decoder.decode_by_name(symbols, name));
        }
        std::ofstream output(argv[5]);
        if (!output) throw std::runtime_error("Cannot create output CSV");
        if (sprites) {
            output << "shape,address,adjustment,x,y,z,colour,colour_frame,native_pixels,"
                      "port_pixels,different_pixels,mask_difference,native_hash,port_hash";
        } else {
            output << "shape,address,frame,yaw,depth,native_pixels,port_pixels,different_pixels,"
                      "mask_difference,native_hash,port_hash,threshold_pointer,colour_pointer";
            for (unsigned i = 0; i < 9; ++i) output << ",matrix" << i;
        }
        output << '\n';
        const std::vector<unsigned> angles = mode == "all"
            ? std::vector<unsigned>{0x2000} : std::vector<unsigned>{0, 0x2000, 0x4000};
        const std::vector<unsigned> depths = mode == "all"
            ? std::vector<unsigned>{1700} : sprites
            ? std::vector<unsigned>{129, 256, 600, 1700, 4500}
            : std::vector<unsigned>{600, 1700, 4500};
        render::SoftwareRenderer renderer;
        unsigned total = 0, exact = 0;
        for (const auto& shape : shapes) {
            const auto frames = sprites ? size_t{5}
                : mode == "all" ? size_t{1} : shape.frames.size();
            for (size_t frame = 0; frame < frames; ++frame) {
                for (const auto yaw : angles) for (const auto depth : depths) {
                    std::copy(baseline.begin(), baseline.end(), ram);
                    std::fill(ram + 0x4000, ram + 0x10000, 0);
                    for (const auto* name : {"M_BIGX", "M_BIGY", "M_ROTX", "M_ROTZ",
                             "M_COLFRAME", "M_DEPTHOFFSET", "M_SPRXSCROLL", "M_SPRYSCROLL",
                             "M_WIREMODE", "M_WOBBLEMODE", "M_WAVEMODE", "M_CELMODE", "M_EXPCNT"}) {
                        if (!symbols.find(name).empty()) put(address(name), 0);
                    }
                    put(address("M_BIGZ"), depth);
                    put(address("M_ROTY"), yaw >> 8);
                    put(address("M_FRAMENUM"), static_cast<unsigned>(frame));
                    put(address("M_SHAPEPTR"), shape.header.address);
                    for (unsigned row = 1; row <= 3; ++row) {
                        for (unsigned column = 1; column <= 3; ++column)
                            put(address("M_WMAT" + std::to_string(row)
                                + std::to_string(column)), row == column ? 32767 : 0);
                    }
                    put(address("M_XLEFT"), 0); put(address("M_XRIGHT"), 223);
                    put(address("M_YTOP"), 0); put(address("M_YBOT"), 191);
                    put(address("M_VANISHX"), 112); put(address("M_VANISHY"), 96);
                    constexpr int adjustments[] = {-8, -1, 0, 1, 8};
                    const auto adjustment = sprites ? adjustments[frame] : 0;
                    const auto sprite_x = yaw == 0 ? -200 : yaw == 0x2000 ? 0 : 200;
                    unsigned instructions;
                    if (sprites) {
                        put(address("M_BIGX"), sprite_x); put(address("M_BIGY"), -10);
                        put(address("M_SHIFT"), shape.header.shift);
                        put(address("M_COLOURPTR"), shape.header.colour_pointer);
                        put(address("M_SHAPEBANK"), shape.header.points_address >> 16);
                        put(address("M_SPRA"), shape.header.size);
                        put(address("M_SPR0"), 0); put(address("M_SPRXSCALE"), adjustment);
                        // MSSPRITE returns to its caller without flushing the
                        // final pixel-cache tile. In-game MDO_3D_DISPLAY does
                        // that explicitly. Use the cartridge's own RPIX/STOP
                        // here, as the dust/grid fixtures do, rather than
                        // depending on a particular core's STOP behavior.
                        const auto stop = address("MSHOW") - 4;
                        if (rom.read16(stop) != 0x4c3d || rom.read16(stop + 2) != 0x0100)
                            throw std::runtime_error("Missing return pixel-cache flush/STOP");
                        instructions = gsu_call(address("MSSPRITE"),
                            address("M_STACK") & 65535, stop & 65535);
                    } else instructions = gsu(address("MSHOWOBJ3"), 0);
                    if (instructions >= 10000000) throw std::runtime_error("GSU timed out: " + shape.name);
                    render::Framebuffer native(224, 192), port(224, 192);
                    render::RenderPose pose;
                    pose.z = depth;
                    pose.animation_frame = sprites ? 0 : static_cast<uint32_t>(frame);
                    if (sprites) {
                        pose.x = sprite_x; pose.y = -10;
                        pose.simple_scaled_sprite = true;
                        pose.simple_sprite_world_size = decoder.simple_sprite_diameter(
                            shape.header, static_cast<int8_t>(adjustment));
                    }
                    pose.use_rotation_matrix = true;
                    simulation::MatrixQ15 native_matrix{};
                    for (unsigned i = 0; i < 9; ++i)
                        native_matrix[i] = static_cast<int16_t>(word(address("M_MAT11") + 2 * i));
                    constexpr simulation::MatrixQ15 world{
                        32767, 0, 0, 0, 32767, 0, 0, 0, 32767};
                    const auto object_matrix = simulation::transpose_q15(
                        simulation::rotation_matrix_q15(trig, 0,
                            simulation::wrap16(-static_cast<int>(yaw)), 0));
                    pose.rotation_matrix = simulation::compose_model_matrix_q15(
                        object_matrix, world, 0, yaw, 0);
                    const auto threshold = word(address("M_DEPTHTABLE"));
                    const auto colour = word(address("M_DEPTHSTAB"));
                    render::apply_source_depth_tables(rom, address("DEPTHTABLES"),
                        static_cast<uint16_t>(threshold), static_cast<uint16_t>(colour), 0, pose);
                    renderer.draw(shape, pose, port, true);
                    unsigned native_pixels = 0, port_pixels = 0, different = 0, mask = 0;
                    for (unsigned y = 0; y < 192; ++y) for (unsigned x = 0; x < 224; ++x) {
                        unsigned pixel = 0;
                        for (unsigned plane = 0; plane < 4; ++plane) {
                            const auto location = 0x4000 + ((x / 8) * 24 + y / 8) * 32
                                + (y % 8) * 2 + (plane / 2) * 16 + plane % 2;
                            pixel |= ((ram[location] >> (7 - x % 8)) & 1) << plane;
                        }
                        native.set(x, y, static_cast<uint8_t>(pixel));
                        const auto host_pixel = port.get(x, y) & 15;
                        port.set(x, y, static_cast<uint8_t>(host_pixel));
                        native_pixels += pixel != 0; port_pixels += host_pixel != 0;
                        different += pixel != unsigned(host_pixel);
                        mask += (pixel != 0) != (host_pixel != 0);
                    }
                    ++total;
                    exact += different == 0;
                    if (sprites) {
                        output << shape.name << ',' << shape.header.address << ',' << adjustment
                            << ',' << sprite_x << ",-10," << depth << ",0,0," << native_pixels << ','
                            << port_pixels << ',' << different << ',' << mask << ','
                            << hash(native) << ',' << hash(port);
                    } else {
                        output << shape.name << ',' << shape.header.address << ',' << frame << ','
                            << yaw << ',' << depth << ',' << native_pixels << ',' << port_pixels << ','
                            << different << ',' << mask << ',' << hash(native) << ',' << hash(port)
                            << ',' << threshold << ',' << colour;
                        for (const auto value : native_matrix) output << ',' << value;
                    }
                    output << '\n';
                }
            }
        }
        if (!output || total == 0) throw std::runtime_error("Empty or incomplete audit output");
        retro_unload_game();
        retro_deinit();
        FreeLibrary(core);
        std::cout << "Independent GSU: " << shapes.size() << " shapes, " << total
            << " cases, " << exact << " exact, " << total - exact << " different\n";
        // A census reports differences without hiding the other evidence.
        return !census && exact != total ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
