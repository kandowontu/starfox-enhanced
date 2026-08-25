#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/particle_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/math.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 5 || argc > 6) {
        std::cerr << "Usage: starfox_stage_preview SF.SFC SYMBOLS.TXT MAP OUTPUT.bmp [ticks]\n";
        return 2;
    }
    try {
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const auto ticks = argc == 6 ? std::strtoul(argv[5], nullptr, 10) : 100UL;
        const std::string requested_scene = argv[3];
        const auto continue_scene = requested_scene == "CONTINUE";
        starfox::simulation::GameSimulation game{
            rom, symbols, continue_scene ? "LEVEL1_1" : requested_scene};
        if (continue_scene) {
            const auto finished = symbols.find("LEVELFINISHED").front();
            game.map().write_native_word(finished, 10U);
            static_cast<void>(game.tick({}));
            for (std::size_t tick = 1; tick < 50U; ++tick) {
                static_cast<void>(game.tick({}));
            }
            static_cast<void>(game.tick({0, starfox::input::start, 0}));
        } else {
            for (unsigned long tick = 0; tick < ticks; ++tick) {
                static_cast<void>(game.tick({}));
            }
        }

        starfox::render::Framebuffer framebuffer{224, 192};
        framebuffer.clear(0);
        starfox::render::RenderSettings render_settings;
        render_settings.colour_index_base = 7U * 16U;
        const starfox::render::SoftwareRenderer renderer{render_settings};
        const starfox::render::ParticleRenderer particle_renderer;
        const starfox::render::ScaledTextRenderer text_renderer{rom, symbols};
        const starfox::render::BackgroundRenderer background_renderer;
        const starfox::render::DustRenderer dust_renderer{rom, symbols};
        const starfox::render::SpriteRenderer sprite_renderer;
        const starfox::assets::ShapeDecoder decoder{rom, symbols};
        const auto trigonometry = starfox::simulation::TrigTables::load(rom, symbols);
        std::unordered_map<std::uint32_t, starfox::assets::Shape> cache;
        std::unordered_set<std::uint32_t> invalid;
        const auto ram_symbol = [&symbols](const char* name) {
            for (const auto address : symbols.find(name)) {
                if ((address >> 16U) == 0 || (address >> 16U) == 0x7eU) return address;
            }
            throw std::runtime_error{"missing camera symbol"};
        };
        const auto mario_symbol = [&symbols](const char* name) {
            for (const auto address : symbols.find(name)) {
                if ((address >> 16U) == 0x70U) return address;
            }
            throw std::runtime_error{"missing Super FX state symbol"};
        };
        const auto colour_symbol = [&symbols](const char* name) {
            for (const auto address : symbols.find(name)) {
                if ((address >> 16U) == 0x03U) {
                    return static_cast<std::uint16_t>(address);
                }
            }
            throw std::runtime_error{"missing colour symbol"};
        };
        const auto camera_x = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("VIEWPOSX")));
        const auto camera_y = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("VIEWPOSY")));
        const auto camera_z = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("VIEWPOSZ")));
        const auto pre_camera_x = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("PVIEWPOSX")));
        const auto pre_camera_y = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("PVIEWPOSY")));
        const auto pre_camera_z = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("PVIEWPOSZ")));
        const auto camera_pitch = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("VIEWROTXW")));
        const auto camera_yaw = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("VIEWROTYW")));
        const auto camera_roll = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("VIEWROTZW")));
        const auto vanish_x = static_cast<std::int16_t>(
            game.map().read_native_word(mario_symbol("M_VANISHX")));
        const auto vanish_y = static_cast<std::int16_t>(
            game.map().read_native_word(mario_symbol("M_VANISHY")));
        const auto view_matrix = starfox::simulation::rotation_matrix_q15(
            trigonometry, camera_pitch, camera_yaw, camera_roll);
        const auto game_frame = static_cast<std::uint8_t>(
            game.map().read_native_byte(ram_symbol("GAMEFRAME")) & 0x7fU);
        const auto background_x = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("BG2XSCROLL")));
        const auto background_y = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("BG2SCROLL")));
        const auto depth_colours = game.map().read_native_word(
            mario_symbol("M_DEPTHSTAB"));
        const auto depth_thresholds = game.map().read_native_word(
            mario_symbol("M_DEPTHTABLE"));
        const auto special_colour = colour_symbol("ID_1_C");
        const auto red_colour = colour_symbol("RED_C");
        const auto white_colour = colour_symbol("WHITE_C");
        std::cout << "ppu=(mode="
                  << static_cast<unsigned>(game.map().ppu_state().background_mode)
                  << ", objsel=$" << std::hex
                  << static_cast<unsigned>(game.map().ppu_state().object_select)
                  << ", tm=$" << static_cast<unsigned>(
                         game.map().ppu_state().main_screen)
                  << ", bg2sc=$" << game.map().ppu_state().bg2_screen_base
                  << ", bg2chr=$" << game.map().ppu_state().bg2_character_base
                  << std::dec << "), bg-scroll=(" << background_x << ','
                  << background_y << "), hofs=("
                  << game.map().ppu_state().bg2_horizontal_offsets_enabled << ','
                  << game.map().ppu_state().bg2_horizontal_offsets.front() << ','
                  << game.map().ppu_state().bg2_horizontal_offsets[112] << ','
                  << game.map().ppu_state().bg2_horizontal_offsets.back()
                  << "), dots=" << static_cast<int>(game.map().dots_mode()) << '\n';
        if (continue_scene) {
            const auto& debug_ppu = game.map().ppu_state();
            std::cout << "continue-assets=(chars="
                      << std::count_if(debug_ppu.vram.begin() + 0xb800U,
                             debug_ppu.vram.begin() + 0xd000U,
                             [](std::uint8_t value) { return value != 0U; })
                      << ", map="
                      << std::count_if(debug_ppu.vram.begin() + 0xe000U,
                             debug_ppu.vram.end(),
                             [](std::uint8_t value) { return value != 0U; })
                      << ", palette="
                      << std::count_if(debug_ppu.cgram.begin(), debug_ppu.cgram.end(),
                             [](std::uint16_t value) { return value != 0U; })
                      << ")\n";
        }
        if (std::getenv("STARFOX_DUMP_OAM") != nullptr) {
            const auto& oam = game.map().ppu_state().oam;
            for (std::size_t object = 0; object < 128U; ++object) {
                const auto offset = object * 4U;
                if (oam[offset] == 0U && oam[offset + 1U] == 0U
                    && oam[offset + 2U] == 0U && oam[offset + 3U] == 0U) continue;
                std::cout << "oam[" << object << "]="
                          << static_cast<unsigned>(oam[offset]) << ','
                          << static_cast<unsigned>(oam[offset + 1U]) << ",$"
                          << std::hex << static_cast<unsigned>(oam[offset + 2U])
                          << ",$" << static_cast<unsigned>(oam[offset + 3U])
                          << std::dec << '\n';
            }
        }
        const auto& ppu = game.map().ppu_state();
        const auto overlays_enabled = std::getenv("STARFOX_NO_OVERLAYS") == nullptr;
        const auto oam_enabled = overlays_enabled
            && std::getenv("STARFOX_NO_OAM") == nullptr;
        framebuffer.clear(0U);
        if (ppu.background_mode == 1U) {
            if (overlays_enabled) {
                background_renderer.draw_bg3(
                    ppu, framebuffer, starfox::render::TilePriorityPass::low);
            }
            if (oam_enabled) sprite_renderer.draw_objects(ppu, framebuffer, 0U);
            if (overlays_enabled && !ppu.bg3_high_priority) {
                background_renderer.draw_bg3(
                    ppu, framebuffer, starfox::render::TilePriorityPass::high);
            }
            if (oam_enabled) sprite_renderer.draw_objects(ppu, framebuffer, 1U);
            background_renderer.draw_bg2(ppu, background_x, background_y,
                framebuffer, starfox::render::TilePriorityPass::low);
            if (oam_enabled) sprite_renderer.draw_objects(ppu, framebuffer, 2U);
            background_renderer.draw_bg2(ppu, background_x, background_y,
                framebuffer, starfox::render::TilePriorityPass::high);
        } else if (ppu.background_mode == 2U) {
            background_renderer.draw_bg2(ppu, background_x, background_y,
                framebuffer, starfox::render::TilePriorityPass::low);
            if (oam_enabled) {
                sprite_renderer.draw_objects(ppu, framebuffer, 0U);
                sprite_renderer.draw_objects(ppu, framebuffer, 1U);
            }
            background_renderer.draw_bg2(ppu, background_x, background_y,
                framebuffer, starfox::render::TilePriorityPass::high);
            if (oam_enabled) sprite_renderer.draw_objects(ppu, framebuffer, 2U);
        } else if (ppu.background_mode == 3U) {
            background_renderer.draw_bg2(ppu, background_x, background_y,
                framebuffer, starfox::render::TilePriorityPass::low);
            if (oam_enabled) sprite_renderer.draw_objects(ppu, framebuffer, 0U);
            background_renderer.draw_bg1(
                ppu, framebuffer, starfox::render::TilePriorityPass::low);
            if (oam_enabled) sprite_renderer.draw_objects(ppu, framebuffer, 1U);
            background_renderer.draw_bg2(ppu, background_x, background_y,
                framebuffer, starfox::render::TilePriorityPass::high);
            if (oam_enabled) sprite_renderer.draw_objects(ppu, framebuffer, 2U);
            background_renderer.draw_bg1(
                ppu, framebuffer, starfox::render::TilePriorityPass::high);
        } else {
            background_renderer.draw_bg2(
                ppu, background_x, background_y, framebuffer);
            if (overlays_enabled) background_renderer.draw_bg3(ppu, framebuffer);
            if (oam_enabled) {
                for (std::uint8_t priority = 0; priority < 3U; ++priority) {
                    sprite_renderer.draw_objects(ppu, framebuffer, priority);
                }
            }
        }
        if (game.map().dots_mode() < 0) {
            const starfox::timing::RenderTransform camera{
                static_cast<double>(camera_x), static_cast<double>(camera_y),
                static_cast<double>(camera_z), static_cast<double>(camera_pitch),
                static_cast<double>(camera_yaw), static_cast<double>(camera_roll)};
            dust_renderer.draw(game.dust(), camera, view_matrix, framebuffer);
        } else if (game.map().dots_mode() > 0) {
            const starfox::timing::RenderTransform camera{
                static_cast<double>(camera_x), static_cast<double>(camera_y),
                static_cast<double>(camera_z), static_cast<double>(camera_pitch),
                static_cast<double>(camera_yaw), static_cast<double>(camera_roll)};
            dust_renderer.draw_grid(camera, view_matrix, framebuffer);
        }
        const auto display_frame = [game_frame](std::uint8_t object_frame) {
            return (object_frame & 0x80U) != 0U
                ? static_cast<std::uint32_t>(object_frame & 0x7fU)
                : static_cast<std::uint32_t>(game_frame);
        };
        const auto effective_colour_table = [special_colour, red_colour,
                                               white_colour](const auto& object) {
            const auto flags = object.strategy_flags[0];
            if ((flags & 0x40U) != 0U) return std::uint16_t{};
            if ((flags & 0x02U) != 0U && (flags & 0x20U) == 0U) {
                return static_cast<std::uint16_t>(
                    (flags & 0x01U) != 0U ? red_colour : white_colour);
            }
            return static_cast<std::uint16_t>(
                (flags & 0x01U) != 0U ? special_colour : object.colour_table);
        };
        std::cout << "camera=(" << camera_x << ',' << camera_y << ',' << camera_z
                  << "), pre-camera=(" << pre_camera_x << ',' << pre_camera_y << ','
                  << pre_camera_z << "), rotation=(" << camera_pitch << ','
                  << camera_yaw << ',' << camera_roll << "), vanish=("
                  << static_cast<std::int16_t>(game.map().read_native_word(0x700034U))
                  << ','
                  << static_cast<std::int16_t>(game.map().read_native_word(0x700036U))
                  << "), framerate="
                  << static_cast<unsigned>(game.map().read_native_byte(
                         ram_symbol("FRAMERATE")))
                  << ", depth=($" << std::hex
                  << game.map().read_native_word(ram_symbol("DEPTHTABPTR"))
                  << ",$" << game.map().read_native_word(0x70004eU)
                  << ",$" << game.map().read_native_word(0x700050U) << std::dec << ')'
                  << '\n';
        struct RenderItem {
            starfox::simulation::ObjectHandle handle{};
            std::array<std::int16_t, 3> position{};
        };
        std::vector<RenderItem> items;
        for (const auto handle : game.draw_order()) {
            if (!game.objects().is_active(handle)) continue;
            const auto& object = game.objects().at(handle);
            if ((object.strategy_flags[3] & 0x08U) != 0U || object.shape == 0U) continue;
            const auto colour_table = effective_colour_table(object);
            const auto base_shape_key = (static_cast<std::uint32_t>(object.shape) << 16U)
                | colour_table;
            if (invalid.contains(base_shape_key)) continue;
            auto base = cache.find(base_shape_key);
            if (base == cache.end()) {
                try {
                    base = cache.emplace(base_shape_key,
                        decoder.decode(object.shape, {}, colour_table)).first;
                } catch (const std::exception&) {
                    invalid.insert(base_shape_key);
                    continue;
                }
            }
            const auto position = starfox::simulation::transform_q15(view_matrix, {
                starfox::simulation::subtract16(object.world_x, camera_x),
                starfox::simulation::subtract16(object.world_y, camera_y),
                starfox::simulation::subtract16(object.world_z, camera_z)});
            items.push_back({handle, position});
        }
        std::size_t rendered = 0;
        std::size_t diagnostics = 0;
        const auto shadow_height = static_cast<std::int16_t>(
            game.map().read_native_word(ram_symbol("SHADOWHEIGHT")));
        const auto make_pose = [&](const RenderItem& item, bool shadow) {
            const auto& object = game.objects().at(item.handle);
            const auto true_colour_shadow =
                (object.strategy_flags[0] & 0x04U) != 0U;
            auto position = item.position;
            if (shadow && !true_colour_shadow) {
                position = starfox::simulation::transform_q15(view_matrix, {
                    starfox::simulation::subtract16(object.world_x, camera_x),
                    starfox::simulation::subtract16(shadow_height, camera_y),
                    starfox::simulation::subtract16(object.world_z, camera_z)});
            }
            starfox::render::RenderPose pose;
            pose.x = position[0];
            pose.y = position[1];
            pose.z = position[2];
            pose.pitch = static_cast<std::uint16_t>(object.rotation_x) << 8U;
            pose.yaw = static_cast<std::uint16_t>(object.rotation_y) << 8U;
            pose.roll = static_cast<std::uint16_t>(object.rotation_z) << 8U;
            pose.vanish_x = vanish_x;
            pose.vanish_y = vanish_y;
            auto object_matrix = starfox::simulation::transpose_q15(
                starfox::simulation::rotation_matrix_q15(
                    trigonometry,
                    starfox::simulation::wrap16(-static_cast<std::int32_t>(
                        static_cast<std::uint16_t>(object.rotation_x) << 8U)),
                    starfox::simulation::wrap16(-static_cast<std::int32_t>(
                        static_cast<std::uint16_t>(object.rotation_y) << 8U)),
                    starfox::simulation::wrap16(-static_cast<std::int32_t>(
                        static_cast<std::uint16_t>(object.rotation_z) << 8U))));
            if (shadow) {
                object_matrix[1] = 0;
                object_matrix[4] = 0;
                object_matrix[7] = 0;
                if (!true_colour_shadow) {
                    pose.force_colour = true;
                    pose.forced_colour = 0x09U;
                }
            }
            pose.rotation_matrix = starfox::simulation::multiply_matrix_q15(
                object_matrix, view_matrix);
            pose.use_rotation_matrix = true;
            pose.animation_frame = display_frame(object.animation_frame);
            pose.colour_frame = display_frame(object.colour_frame);
            pose.texture_scroll_x = object.texture_scroll_x;
            pose.texture_scroll_y = object.texture_scroll_y;
            starfox::render::apply_original_depth_tables(rom,
                depth_thresholds, depth_colours, object.extended[21], pose);
            return pose;
        };
        if ((game.map().read_native_byte(ram_symbol("PLAYERFLYMODE")) & 0x08U) != 0U) {
            for (const auto& item : items) {
                const auto& object = game.objects().at(item.handle);
                if ((object.strategy_flags[0] & 0x0cU) == 0U) continue;
                const auto colour_table = effective_colour_table(object);
                const auto base_shape_key =
                    (static_cast<std::uint32_t>(object.shape) << 16U)
                    | colour_table;
                const auto base = cache.find(base_shape_key);
                if (base == cache.end()) continue;
                const auto shadow_pointer = base->second.header.shadow_pointer;
                const auto shape_key =
                    (static_cast<std::uint32_t>(shadow_pointer) << 16U)
                    | colour_table;
                if (invalid.contains(shape_key)) continue;
                auto found = cache.find(shape_key);
                if (found == cache.end()) {
                    try {
                        found = cache.emplace(shape_key, decoder.decode_lod(
                            base->second.header, shadow_pointer,
                            colour_table)).first;
                    } catch (const std::exception&) {
                        invalid.insert(shape_key);
                        continue;
                    }
                }
                renderer.draw(found->second, make_pose(item, true), framebuffer, false);
                ++rendered;
            }
        }
        for (const auto& item : items) {
            const auto handle = item.handle;
            const auto& object = game.objects().at(handle);
            if ((object.strategy_flags[0] & 0x04U) != 0U) continue;
            const auto colour_table = effective_colour_table(object);
            const auto base_shape_key = (static_cast<std::uint32_t>(object.shape) << 16U)
                | colour_table;
            const auto base = cache.find(base_shape_key);
            if (base == cache.end()) continue;
            if ((object.strategy_flags[0] & 0x10U) != 0U) {
                particle_renderer.draw_owner(game.particles(), item.handle,
                    make_pose(item, false), 1.0, framebuffer);
                ++rendered;
                continue;
            }
            if ((object.strategy_flags[0] & 0x40U) != 0U) {
                text_renderer.draw(object.colour_table, object.extended[21],
                    std::bit_cast<std::int8_t>(object.texture_scroll_x),
                    make_pose(item, false), framebuffer);
                ++rendered;
                continue;
            }
            const auto relative_z = static_cast<double>(item.position[2]);
            const auto base_header = base->second.header;
            const auto selected_pointer = starfox::assets::ShapeDecoder::select_lod_pointer(
                base_header, relative_z);
            const auto shape_key = (static_cast<std::uint32_t>(selected_pointer) << 16U)
                | colour_table;
            auto found = cache.find(shape_key);
            if (found == cache.end()) {
                try {
                    found = cache.emplace(shape_key, decoder.decode_lod(
                        base_header, selected_pointer, colour_table)).first;
                } catch (const std::exception&) {
                    invalid.insert(shape_key);
                    continue;
                }
            }
            auto pose = make_pose(item, false);
            if ((object.strategy_flags[0] & 0x20U) != 0U) {
                auto size_adjustment = static_cast<std::int16_t>(
                    std::bit_cast<std::int8_t>(object.texture_scroll_x));
                for (std::uint8_t shift = 0; shift < base_header.shift; ++shift) {
                    size_adjustment = starfox::simulation::add16(
                        size_adjustment, size_adjustment);
                }
                auto diameter = starfox::simulation::add16(
                    base_header.size, size_adjustment);
                diameter = starfox::simulation::add16(diameter, diameter);
                if (diameter == 0) diameter = 1;
                pose.simple_scaled_sprite = true;
                pose.simple_sprite_colour = object.extended[21];
                pose.simple_sprite_world_size = diameter;
            }
            if (std::getenv("STARFOX_DUMP_OBJECTS") != nullptr || diagnostics++ < 12) {
                std::cout << "object=" << handle << " shape=$" << std::hex << object.shape
                          << std::dec << " pose=(" << pose.x << ',' << pose.y << ',' << pose.z
                          << ") shift=" << static_cast<unsigned>(found->second.header.shift)
                          << " vertices=" << found->second.vertices.size()
                          << " type=$" << std::hex
                          << static_cast<unsigned>(object.type) << " sflags=$"
                          << static_cast<unsigned>(object.strategy_flags[0]) << std::dec
                          << " depth=" << static_cast<unsigned>(object.extended[21])
                          << " tx=" << static_cast<int>(
                              std::bit_cast<std::int8_t>(object.texture_scroll_x))
                          << " size=" << found->second.header.size
                          << " textures=" << found->second.textures.size()
                          << " velocity=(" << object.velocity_x << ',' << object.velocity_y
                          << ',' << object.velocity_z << ") speed=" << object.velocity << '\n';
            }
            renderer.draw(found->second, pose, framebuffer, false);
            ++rendered;
        }
        if (overlays_enabled
            && std::getenv("STARFOX_NO_METERS") == nullptr) {
            sprite_renderer.draw_meters(game.meter_state(), framebuffer);
        }
        if (oam_enabled) {
            sprite_renderer.draw_objects(ppu, framebuffer, 3U);
        }
        if (overlays_enabled && ppu.background_mode == 1U
            && ppu.bg3_high_priority) {
            background_renderer.draw_bg3(
                ppu, framebuffer, starfox::render::TilePriorityPass::high);
        }
        const std::filesystem::path output{argv[4]};
        if (!output.parent_path().empty()) {
            std::filesystem::create_directories(output.parent_path());
        }
        const auto base_palette = starfox::render::decode_bgr555_palette(
            game.map().ppu_state().cgram);
        const auto palette = starfox::render::apply_snes_brightness(
            base_palette, game.map().display_brightness());
        starfox::render::write_bmp(framebuffer, output, palette);
        std::cout << "Rendered " << rendered << " live objects and "
                  << game.particles().active_count() << " particles to "
                  << output << '\n';
        return rendered == 0 ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Stage preview failed: " << error.what() << '\n';
        return 1;
    }
}
