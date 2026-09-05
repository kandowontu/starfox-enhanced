#include "starfox/assets/shape_decoder.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/simulation/game_simulation.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

namespace {
using namespace starfox;
using namespace simulation;
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
std::uint32_t address(const assets::SymbolMap& symbols, const char* name) {
    require(!symbols.find(name).empty(), std::string{"missing symbol "} + name);
    return symbols.find(name).front();
}

void check_point_scaling(const assets::RomImage& rom, const assets::SymbolMap& symbols) {
    assets::ShapeDecoder decoder{rom, symbols};
    unsigned checked = 0, words = 0, bytes = 0;
    for (const auto* name : {"SHIP_4", "LUIGIHSBW", "LUIGIH", "BOXXIE", "POINTYANIM", "WOLF_1BOSS"}) {
        if (symbols.find(name).empty()) continue;
        const auto shape = decoder.decode_by_name(symbols, name);
        for (const unsigned multiplier : {1U, 2U, 4U}) {
            auto reference = shape;
            // MOBJ.MC: word commands use the coordinates directly; byte
            // commands multiply by M_SCALE before the rotation dot products.
            // Build equivalent world-sized geometry for an independent
            // framebuffer comparison, including each animation frame.
            for (auto& frame : reference.frames) {
                std::size_t index = 0;
                for (const auto& block : frame.point_blocks) {
                    const bool word = block.encoding == assets::PointEncoding::signed16
                        || block.encoding == assets::PointEncoding::mirrored_x_signed16;
                    const bool mirrored = block.encoding == assets::PointEncoding::mirrored_x_signed8
                        || block.encoding == assets::PointEncoding::mirrored_x_signed16;
                    const auto count = block.source_points.size() * (mirrored ? 2U : 1U);
                    const auto factor = word ? 1U : multiplier * (1U << shape.header.shift);
                    for (unsigned p = 0; p < count; ++p, ++index) {
                        frame.vertices[index].x *= factor;
                        frame.vertices[index].y *= factor;
                        frame.vertices[index].z *= factor;
                        word ? ++words : ++bytes;
                    }
                }
                require(index == frame.vertices.size(), "point blocks lost expanded vertices");
                frame.vertex_encodings.clear();
            }
            reference.header.shift = 0;
            for (const auto scale : {1U, 4U}) {
                render::RenderSettings settings;
                settings.render_scale = scale;
                render::SoftwareRenderer renderer{settings};
                for (unsigned frame = 0; frame < shape.frames.size(); ++frame) {
                    render::RenderPose pose;
                    pose.z = 1700;
                    pose.yaw = 4500;
                    pose.animation_frame = frame;
                    pose.scale = multiplier;
                    render::Framebuffer actual{224, 192, scale}, expected{224, 192, scale};
                    renderer.draw(shape, pose, actual, true);
                    pose.scale = 1;
                    renderer.draw(reference, pose, expected, true);
                    require(actual.pixels() == expected.pixels(),
                        std::string{name} + " differs from source point-command scaling");
                    require(std::any_of(actual.pixels().begin(), actual.pixels().end(),
                        [](auto p) { return p != 0; }), std::string{name} + " rendered no model");
                    ++checked;
                }
            }
        }
    }
    require(checked && bytes, "byte-coordinate rendering was not exercised");
    if (!symbols.find("PLANETSEQ2_L").empty()) require(words, "EX word models were not exercised");
    std::cout << "source point scaling: " << checked << " model/frame/scale comparisons\n";
}

void check_ex_shape_streams(const assets::RomImage& rom, const assets::SymbolMap& symbols) {
    if (symbols.find("PLANETSEQ2_L").empty()) return;
    assets::ShapeDecoder decoder{rom, symbols};
    for (const auto* name : {"PARA_1_S", "WALL1_S1", "HOU_L_S", "HOU_R_S",
             "SEA_0_S", "DRAIL_0S", "PILLAR3S", "PILLAR4S"}) {
        const auto shape = decoder.decode_by_name(symbols, name);
        require(shape.header.compact && shape.header.colour_pointer
            == static_cast<std::uint16_t>(address(symbols, "ID_0_C")),
            std::string{name} + " has the wrong standalone compact-header palette");
    }
    const auto human = decoder.decode_by_name(symbols, "HUMANA");
    bool long_branch = false;
    for (const auto& node : human.bsp_nodes) {
        const auto offset = rom.read8(node.address + 4);
        if (offset < 128) continue;
        long_branch = true;
        require(node.alternate_address == node.address + 4 + offset,
            "BSP branch was sign-extended instead of GETB/LOB zero-extended");
    }
    require(long_branch, "HUMANA did not exercise the large source BSP tree");
}

void check_planet_texels(const assets::RomImage& rom, const assets::SymbolMap& symbols) {
    const auto a = [&](const char* name) { return address(symbols, name); };
    unsigned checked = 0;
    for (const char* table : {"PLANETSPRS", "PLANETSPRS2"}) {
        if (symbols.find(table).empty()) continue;
        for (unsigned entry = 0; entry < 17; ++entry) {
            const auto sprite = rom.read8(a(table) + 2 * entry);
            if (sprite & 0x80) continue;
            const auto texture_entry = a("TEXTUREADDRTAB") + 3 * sprite;
            const auto source = rom.read16(texture_entry)
                | (static_cast<std::uint32_t>(rom.read8(texture_entry + 2)) << 16);
            for (const unsigned size : {32U, 64U}) {
                auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL1_1");
                auto& map = game->map();
                for (unsigned i = 0; i < 16384; ++i)
                    map.write_native_byte(0x700000 | ((a("BITMAP1") + i) & 0xffff), 0);
                map.write_native_word(a("MSPRITE"), sprite);
                map.write_native_word(a("M_XC"), size / 2);
                map.write_native_word(a("M_YC"), size / 2);
                map.write_native_word(a("MSPR_PAL"), 6);
                map.write_native_word(a("M_SPRSIZE"), 32);
                map.write_native_word(a("M_SPRXSCALE"), size);
                const auto routine = a(size == 32 ? "MDRAWSPRITE32" : "MUSPRITE");
                Wdc65816Registers registers;
                registers.status = 0x24;
                registers.a = routine >> 16;
                registers.x = routine & 0xffff;
                static_cast<void>(map.call_native_routine(a("RUNMARIO_L"), registers, 10000));
                for (unsigned y = 0; y < size; ++y) for (unsigned x = 0; x < size; ++x) {
                    const auto packed = rom.read8(source + y * 32 / size * 256 + x * 32 / size);
                    const auto texel = sprite & 32 ? packed >> 4 : packed & 15;
                    const unsigned expected = texel ? 0x60 | texel : 0;
                    const auto tile = (x / 8) * 16 + y / 8;
                    unsigned actual = 0;
                    for (unsigned plane = 0; plane < 8; ++plane) {
                        const auto b = map.read_native_byte(0x700000 | ((a("BITMAP1") + tile * 64
                            + (y % 8) * 2 + (plane / 2) * 16 + plane % 2) & 0xffff));
                        actual |= ((b >> (7 - x % 8)) & 1) << plane;
                    }
                    require(actual == expected, std::string{table} + " icon "
                        + std::to_string(entry) + " samples the wrong packed texel");
                }
                ++checked;
            }
        }
    }
    std::cout << "source planet art: " << checked << " complete flat/zoom icon comparisons\n";
}

void check_credits_input(const assets::RomImage& rom, const assets::SymbolMap& symbols) {
    const bool ex = !symbols.find("PLANETSEQ2_L").empty();
    for (unsigned scenario = 0; scenario < (ex ? 3U : 2U); ++scenario) {
        const bool held_early = scenario == 1;
        const bool australia = scenario == 2;
        auto game = std::make_unique<GameSimulation>(rom, symbols, "CREDITSMAP");
        unsigned returned = 0;
        for (unsigned tick = 0; tick < 6500; ++tick) {
            const bool press = held_early || tick >= 3500;
            const auto button = static_cast<input::ButtonMask>(press
                ? (australia ? input::y : input::start) : 0);
            static_cast<void>(game->tick({button, button, 0}));
            const auto flow = game->flow_state();
            if (flow != GameFlowState::credits && flow != GameFlowState::finished) {
                require(tick > 1000, "Start skipped the source credits input gate");
                require(australia ? flow == GameFlowState::gameplay
                    : ex ? flow == GameFlowState::ex_pregame_menu
                    : flow == GameFlowState::controls_type || flow == GameFlowState::title,
                    "credits returned to the wrong source screen");
                returned = tick;
                break;
            }
        }
        require(returned, "credits Start failed to return in bounded time");
        for (unsigned tick = 0; tick < 10; ++tick) static_cast<void>(game->tick({}));
        std::cout << "credits " << (australia ? "Australia Y" : held_early ? "held early" : "late Start")
            << " returned at " << returned << '\n';
    }
}

void check_wolf(const assets::RomImage& rom, const assets::SymbolMap& symbols) {
    if (symbols.find("STARWOLF_ISTRAT").empty()) return;
    const auto a = [&](const char* n) { return address(symbols, n); };
    const std::array strategies{a("STARWOLF_STRAT"), a("STARWOLFTEAM1_STRAT"),
        a("STARWOLFTEAM2_STRAT"), a("STARWOLFTEAM3_STRAT")};
    assets::ShapeDecoder decoder{rom, symbols};
    for (const bool original_pace : {false, true}) {
      for (const char* level : {"LEVEL5_5", "LEVEL6_6", "LEVEL7_5"}) {
        auto game = std::make_unique<GameSimulation>(rom, symbols, level);
        game->set_god_mode(true);
        game->set_timing_mode(original_pace ? TimingMode::original_speed : TimingMode::unlocked_20_fps);
        game->set_presentation_fps(original_pace ? 120 : 20);
        unsigned first = 0, sustained = 0;
        std::set<unsigned> rendered;
        for (unsigned tick = 1; tick < 5000; ++tick) {
            static_cast<void>(game->tick({}));
            std::set<unsigned> team;
            for (const auto handle : game->objects().active_handles()) {
                const auto& object = game->objects().at(handle);
                auto it = std::find(strategies.begin(), strategies.end(), object.strategy_address);
                if (it == strategies.end()) continue;
                const auto member = static_cast<unsigned>(it - strategies.begin());
                team.insert(member);
                // Decode and draw each live ship, so a valid spawn with an
                // invisible/unsupported model cannot satisfy the test.
                if (!rendered.contains(member)) {
                    auto shape = decoder.decode(object.shape, {}, object.colour_table);
                    render::Framebuffer image{224, 192};
                    render::RenderPose pose;
                    pose.z = 500;
                    pose.yaw = 5000;
                    render::SoftwareRenderer renderer;
                    renderer.draw(shape, pose, image, true);
                    require(std::any_of(image.pixels().begin(), image.pixels().end(),
                        [](auto p) { return p != 0; }), std::string{level} + " has an invisible Wolf ship");
                    rendered.insert(member);
                }
            }
            if (team.size() == 4 && !first) first = tick;
            if (first) {
                require(team.size() == 4, std::string{level} + " lost a living Wolf teammate");
                if (++sustained == 600) break;
            }
        }
        require(sustained == 600 && rendered.size() == 4,
            std::string{level} + " did not spawn/render/sustain all four Wolf ships");
        const auto boss_cursor = game->map().read_native_word(a("BOSS_PTR"));
        // Shoot each ship using ordinary fire and directional input. Enemy
        // health, projectile positions and collision state remain native.
        bool cleared = false;
        input::ButtonMask previous_buttons{};
        for (unsigned tick = 0; tick < 10000; ++tick) {
            input::ButtonMask buttons = input::y;
            for (const auto handle : game->objects().active_handles()) {
                const auto& enemy = game->objects().at(handle);
                if (std::find(strategies.begin(), strategies.end(), enemy.strategy_address)
                    == strategies.end()) continue;
                const auto& player = game->objects().at(game->player());
                const auto dx = enemy.world_x - player.world_x;
                const auto dy = enemy.world_y - player.world_y;
                if (dx > 12) buttons |= input::right;
                else if (dx < -12) buttons |= input::left;
                if (dy > 12) buttons |= input::up;
                else if (dy < -12) buttons |= input::down;
                break;
            }
            static_cast<void>(game->tick({buttons,
                static_cast<input::ButtonMask>(buttons & ~previous_buttons),
                static_cast<input::ButtonMask>(previous_buttons & ~buttons)}));
            previous_buttons = buttons;
            if (game->map().read_native_word(a("BOSS_PTR")) == boss_cursor + 2) {
                const auto boss = std::string{level} == "LEVEL5_5" ? "BOSS55"
                    : std::string{level} == "LEVEL6_6" ? "BOSS66" : "BOSS75";
                require(game->map().read_native_word(a("BOSS_SEQ") + boss_cursor)
                    == a(boss) - a("ENDSEQBOSS"), "Wolf recorded the wrong defeated boss");
                cleared = true;
                break;
            }
        }
        require(cleared, std::string{level} + " cannot complete through player laser combat");
        std::cout << level << (original_pace ? " Original pace" : " Unlocked pace")
            << " four-ship encounter, rendering, player laser kills and map continuation passed\n";
      }
    }
}

void check_route_end(const assets::RomImage& rom, const assets::SymbolMap& symbols) {
    if (symbols.find("PLANETSEQ2_L").empty()) return;
    for (const char* level : {"LEVEL1_6", "LEVEL2_6", "LEVEL3_7", "LEVEL4_5", "LEVEL5_5", "LEVEL6_6", "LEVEL7_5"}) {
        auto game = std::make_unique<GameSimulation>(rom, symbols, level);
        game->set_god_mode(true);
        auto& map = game->map();
        const auto a = [&](const char* n) { return address(symbols, n); };
        for (unsigned tick = 0; tick < 200; ++tick) static_cast<void>(game->tick({}));
        // Exercise the source shortcut with Pepper skip, as well as its
        // end-of-route handoff. The player-control fixture opens its normal
        // input gate without synthesizing LEVELFINISHED.
        map.write_native_byte(a("PEPPERSKIP"), 1);
        map.write_native_byte(a("STAYBLACK"), 255);
        map.write_native_byte(a("PSHIPFLAGS"), map.read_native_byte(a("PSHIPFLAGS")) & ~0x60U);
        constexpr auto buttons = input::select | input::left_shoulder | input::right_shoulder;
        static_cast<void>(game->tick({buttons, buttons, 0}));
        require(game->flow_state() == GameFlowState::stage_results,
            std::string{level} + " did not accept the source skip shortcut");
        bool returned = false;
        for (unsigned tick = 0; tick < 2500; ++tick) {
            static_cast<void>(game->tick({}));
            if (game->flow_state() == GameFlowState::planet_travel) { returned = true; break; }
        }
        require(returned, std::string{level} + " skip failed to return to the route map");
        const auto destination = map.read_native_word(a("NEWMAP"))
            | (static_cast<std::uint32_t>(map.read_native_byte(a("NEWMAP") + 2)) << 16) | 0x8000U;
        require(destination == a(level), std::string{level} + " route end lost its final source destination");
        std::cout << level << " L+R+Select at route terminator passed\n";
    }
}
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const auto rom = assets::RomImage::load(argv[1]);
        const auto symbols = assets::SymbolMap::load(argv[2]);
        check_point_scaling(rom, symbols);
        check_ex_shape_streams(rom, symbols);
        check_planet_texels(rom, symbols);
        check_credits_input(rom, symbols);
        check_wolf(rom, symbols);
        check_route_end(rom, symbols);
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
