#include "starfox/assets/rom.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/object_snapshot.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/strategy_scheduler.hpp"
#include "starfox/timing/fixed_step.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void check_reticle_identity() {
    // EX's BG1 guard columns are excluded by geometry, even when a palette
    // transition makes their formerly black tile tan.
    starfox::simulation::SnesPpuState ppu;
    ppu.background_mode = 1U; ppu.main_screen = 1U;
    ppu.bg1_character_base = 0x1000U;
    ppu.cgram[1] = 0x3b1fU;
    for (unsigned row=0;row<8;++row) ppu.vram[0x2000U+row*2U]=255U;
    for (unsigned width : {256U,400U,800U}) {
        starfox::render::Framebuffer frame{width,224U};
        const auto origin = int((width-256U)/2U);
        starfox::render::BackgroundRenderer{}.draw_bg1(ppu,frame,
            starfox::render::TilePriorityPass::all,origin,false,16U);
        for (int x=0;x<int(width);++x)
            require(frame.get(x,80) == (x>=origin+16 && x<origin+240 ? 1U:0U),
                "non-black EX bitmap guards leaked into presentation");
    }
    using namespace starfox::simulation;
    ObjectPool objects{2};
    const auto first = objects.allocate_after();
    const auto old_id = objects.generation(first);
    require(objects.remove(first), "reticle removal failed");
    const auto second = objects.allocate_after();
    require(first == second && objects.generation(second) != old_id,
        "same-shape recycled slot retained its presentation identity");
    const auto second_id = objects.generation(second);
    objects.restore_lists(objects.active_handles(), objects.free_handles());
    require(objects.generation(second) == second_id,
        "ordinary native object synchronization changed entity identity");
    objects.restore_lists({}, {1, 2});
    objects.restore_lists({second}, {static_cast<ObjectHandle>(3 - second)});
    require(objects.generation(second) != second_id,
        "native free/reallocate retained an old reticle identity");
    using namespace starfox::timing;
    const TransformSnapshot old_player{10, 20, 32760, 0, 0, 0};
    const TransformSnapshot player{40, 0, -32736, 0, 0, 0};
    const TransformSnapshot sight{50, 5, -32236, 0, 0, 0};
    const auto birth = relative_birth_snapshot(sight, old_player, player);
    for (unsigned step = 0; step <= 6; ++step) {
        const auto alpha = step / 6.0;
        const auto p = interpolate(old_player, player, alpha);
        const auto r = interpolate(birth, sight, alpha);
        require(std::abs(r.x - p.x - 10) < 1e-8 && std::abs(r.y - p.y - 5) < 1e-8
            && static_cast<std::uint16_t>(std::lround(r.z - p.z)) == 500,
            "new reticle jittered relative to interpolated player at coordinate wrap");
    }
}

void check_escape_explosions(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using namespace starfox::simulation;
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL1_1");
    auto& pool = game->objects();
    auto& map = game->map();
    // Exercise both allocations made by the escape camera: the first consumes
    // the final free slot, the second must use the cartridge's dummy object.
    const auto anchor = pool.allocate_after();
    const auto camera = pool.allocate_after();
    pool.at(camera).strategy_address = addr("VIEWOUTOFLB1_STRAT");
    pool.at(camera).health = 1;
    pool.at(camera).strategy_flags = {0, 0x11, 8, 0};
    map.write_native_word(addr("MAPVAR1"), static_cast<std::uint16_t>(addr("ALBLKS") + (anchor - 1U) * addr("AL_SIZE")));
    map.write_native_word(addr("VIEWTOOBJ"), static_cast<std::uint16_t>(addr("ALBLKS") + (game->player() - 1U) * addr("AL_SIZE")));
    map.write_native_byte(addr("GAMEFLAGS2"), 1);
    require(pool.remove(anchor), "escape anchor removal failed");
    while (pool.free_handles().size() > 1U) static_cast<void>(pool.allocate_after());
    static_cast<void>(map.call_native_object_routine(addr("VIEWOUTOFLB1_STRAT"), camera));
    require(pool.free_handles().empty(), "escape explosion strategy did not allocate its last slot");
    pool.restore_lists(pool.active_handles(), pool.free_handles());
    std::cout << "escape camera explosion lists remain valid\n";
    // A native routine that loses a list must fail without reclassifying live
    // objects as free. Do this after the valid escape has exhausted the pool.
    constexpr auto code_address = 0x7e6800U;
    const auto active_list = addr("ALLST");
    const std::array<std::uint8_t, 9> corrupt_code{
        0xc2, 0x20, 0xa9, 0x00, 0x00, 0x8d,
        static_cast<std::uint8_t>(active_list),
        static_cast<std::uint8_t>(active_list >> 8U), 0x6b};
    for (std::size_t i = 0; i < corrupt_code.size(); ++i)
        map.write_native_byte(code_address + i, corrupt_code[i]);
    const auto before = pool.active_handles();
    bool rejected = false;
    try {
        (void)map.call_native_object_routine(code_address, camera);
    } catch (const std::runtime_error& error) {
        rejected = std::string_view(error.what())
            == "native active/free lists do not cover the object pool";
    }
    require(rejected && pool.active_handles() == before && pool.free_handles().empty(),
        "missing native objects were silently reclaimed");
}

void check_score_digits(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    starfox::simulation::Wdc65816 cpu{rom, &symbols};
    const auto bitmap = addr("BITMAP1");
    const auto pixel = [&](unsigned x, unsigned y) {
        const auto base = 0x700000U | ((bitmap + ((x / 8U) * 24U + y / 8U) * 32U + (y & 7U)*2U) & 0xffffU);
        unsigned value = 0;
        for (unsigned p = 0; p < 4; ++p)
            value |= ((cpu.read8(base + (p / 2U)*16U + (p & 1U)) >> (7U-(x&7U))) & 1U) << p;
        return value;
    };
    for (unsigned percent = 0; percent <= 100; ++percent) {
        for (unsigned b = 0; b < 28U*24U*32U; ++b)
            cpu.write8(0x700000U | ((bitmap+b)&0xffffU), 0);
        cpu.write16(addr("M_X1"), 100);
        cpu.write16(addr("M_Y1"), 80);
        cpu.write16(addr("M_Z1"), percent);
        const auto entry = addr("MPRTPERC");
        cpu.write8(0x3034, entry >> 16U);
        cpu.write8(0x301e, entry);
        cpu.write8(0x301f, entry >> 8U);
        const auto digits = std::to_string(percent);
        const unsigned left = percent >= 100 ? 92U : percent <= 9 ? 108U : 100U;
        for (unsigned y = 0; y < 14; ++y) for (unsigned x = 88; x < 128; ++x) {
            bool ink = false;
            for (unsigned d = 0; d < digits.size(); ++d) {
                const auto dx = static_cast<int>(x) - static_cast<int>(left+d*8U);
                // MPRTNUM indexes the first ten font glyphs directly, primes
                // an empty scanline, then plots 15 columns of each source word.
                if (y >= 1 && y <= 12 && dx >= 0 && dx < 15)
                    ink |= (rom.read16(addr("FONT0FON") + (digits[d]-'0')*24U + (y-1U)*2U)
                        & (0x8000U >> dx)) != 0;
            }
            require(pixel(x,80+y) == (ink ? 14U : 0U), "score percentage digit pixels differ from native numeric glyphs");
        }
        require(cpu.read16(addr("M_X1")) == left + digits.size()*8U,
            "percentage cursor advance changed");
    }
    std::cout << "Score percentages 0-100 match native glyph pixels\n";
}

void check_ex_reticle_stations(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    const auto entries = symbols.find("TEST_ISTRAT");
    if (entries.empty()) return;
    using namespace starfox;
    auto game = std::make_unique<simulation::GameSimulation>(rom, symbols, "LEVEL1_1",
        std::span<const std::uint8_t>{}, true);
    game->set_god_mode(true);
    const auto trig = simulation::TrigTables::load(rom, symbols);
    for (unsigned tick=0; tick<420; ++tick) static_cast<void>(game->tick({}));
    auto previous = render::capture_object_snapshots(game->objects(), trig);
    bool reproduced_particle_motion = false;
    unsigned tested = 0;
    for (unsigned tick=0; tick<30; ++tick) {
        static_cast<void>(game->tick({}));
        const auto current = render::capture_object_snapshots(game->objects(), trig);
        for (const auto& [handle,sight] : current) {
            if (sight.strategy_address != entries.front()) continue;
            const auto* prior = render::reticle_previous_snapshot(sight,current,previous,game->player());
            require(prior != nullptr, "EX sight station lost its previous pose");
            const auto& player = current.at(game->player()).transform;
            const auto& old_player = previous.at(game->player()).transform;
            const auto depth = simulation::wrap16(sight.transform.z-player.z);
            const auto old = previous.find(handle);
            if (old != previous.end() && old->second.generation == sight.generation
                && simulation::wrap16(old->second.transform.z-old_player.z) != depth)
                reproduced_particle_motion = true;
            for (unsigned fps : {60U,120U,240U,480U}) for (unsigned phase=0; phase<=fps/20U; ++phase) {
                const auto alpha = double(phase)/(fps/20U);
                const auto pose = timing::interpolate(prior->transform,sight.transform,alpha);
                const auto owner = timing::interpolate(old_player,player,alpha);
                require(static_cast<std::int16_t>(std::lround(pose.z-owner.z)) == depth,
                    "EX reticle moves between depth stations on fractional frames");
            }
            if (tested++ == 0) {
                const auto shape = assets::ShapeDecoder{rom,symbols}.decode(sight.shape);
                render::Framebuffer frame{224,192};
                render::RenderPose pose;
                pose.z=500;
                pose.palette_override=207U;
                render::RenderSettings settings;
                settings.colour_index_base = 112U; // the live game's BG1 row
                render::SoftwareRenderer{settings}.draw(shape,pose,frame,false);
                require(std::any_of(frame.pixels().begin(),frame.pixels().end(),[](auto p){return p==207U;}),
                    "EX reticle did not use its dedicated crosshair palette index");
                require(std::all_of(frame.pixels().begin(),frame.pixels().end(),[](auto p){return p==0U||p==207U;}),
                    "EX reticle retained an untinted material");
            }
        }
        previous=current;
    }
    require(tested > 20 && reproduced_particle_motion, "EX reticle motion regression did not exercise recycled native particles");
    // Hold DOWN (inverted flight controls) through the upper flight boundary.
    for (unsigned tick=0; tick<75; ++tick) static_cast<void>(game->tick({1024U,0,0}));
    const auto read = [&](const char* name) {
        return static_cast<std::int16_t>(game->map().read_native_word(symbols.find(name).at(0)));
    };
    const auto first_float = read("VIEWFLOATY");
    bool float_changed = false;
    previous = render::capture_object_snapshots(game->objects(), trig);
    auto old_float = read("VIEWFLOATY"), old_camera_y = read("VIEWPOSY");
    for (unsigned tick=0; tick<90; ++tick) {
        static_cast<void>(game->tick({1024U,0,0}));
        const auto current = render::capture_object_snapshots(game->objects(), trig);
        const auto camera_y = read("VIEWPOSY"), view_float = read("VIEWFLOATY");
        float_changed |= view_float != first_float;
        for (const auto& [handle,sight] : current) {
            if (sight.strategy_address != entries.front()) continue;
            const auto* prior = render::reticle_previous_snapshot(sight,current,previous,game->player());
            require(prior != nullptr,"boundary reticle lost station identity");
            const auto before = prior->transform.y + old_float - old_camera_y;
            const auto after = sight.transform.y + view_float - camera_y;
            require(before == after,"stabilized boundary sight moved between native ticks");
            for (unsigned phase=0;phase<=24;++phase) {
                const auto alpha = phase/24.0;
                const auto marker = timing::interpolate(prior->transform,sight.transform,alpha);
                const auto projected_y = marker.y + std::lerp(double(old_float),double(view_float),alpha)
                    - std::lerp(double(old_camera_y),double(camera_y),alpha);
                require(std::abs(projected_y-after)<1e-8,"boundary sight bobbed at fractional presentation phase");
            }
        }
        previous=current; old_float=view_float; old_camera_y=camera_y;
    }
    require(float_changed,"boundary regression did not exercise decorative camera float");
    std::cout << "EX sight stations remain stable at 60-480 FPS and accept HUD tint\n";
}

void check_tunnel(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using namespace starfox::simulation;
    auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL1_1");
    const auto address = [&](const char* name) { return symbols.find(name).at(0); };
    auto& map = game->map();
    const auto tables = address("CHEQUERED_TABLES");
    map.write_native_byte(address("INATUNNEL"), 1);
    map.write_native_byte(address("HDMAEN_GC"), 0x10);
    map.write_native_byte(address("BG2VOFSOVERRIDE"), 0);
    map.write_native_byte(0, 0);
    for (unsigned phase = 0; phase < 32; ++phase) {
        map.write_native_word(address("OLDVIEWPOSZ"), phase * 8U);
        map.tick_video_phase();
        const auto& ppu = map.ppu_state();
        require(ppu.bg2_scanline_scroll_enabled, "tunnel HDMA was not enabled");
        std::array<std::int16_t, 224> expected{};
        auto cursor = (tables & 0xff0000U) | map.read_native_word(tables + phase * 2U);
        unsigned line = 10;
        std::int16_t scroll{};
        for (unsigned run = 0; run < 224 && line < expected.size(); ++run) {
            const auto record = map.read_native_byte(cursor++);
            if ((record & 127) == 0) break;
            scroll = (record & 128) ? 280 : 24;
            for (unsigned count = record & 127; count && line < expected.size(); --count)
                expected[line++] = scroll;
        }
        std::fill(expected.begin() + line, expected.end(), scroll);
        require(expected == ppu.bg2_scanline_scroll_y,
            "tunnel scanline page selection differs from source table");
        require(std::find(expected.begin(), expected.end(), 280) != expected.end(),
            "source tunnel table did not select the second tilemap page");
    }
    map.write_native_byte(address("BG2VOFSOVERRIDE"), 1);
    map.tick_video_phase();
    require(!map.ppu_state().bg2_scanline_scroll_enabled,
        "credits/continue scroll override retained tunnel HDMA");
    map.write_native_byte(address("BG2VOFSOVERRIDE"), 0);
    map.write_native_byte(address("INATUNNEL"), 0);
    map.tick_video_phase();
    require(!map.ppu_state().bg2_scanline_scroll_enabled,
        "leaving tunnel retained its scanline offsets");
    map.write_native_byte(address("INATUNNEL"), 1);
    map.write_native_byte(0x2105, 3);
    map.tick_video_phase();
    require(!map.ppu_state().bg2_scanline_scroll_enabled,
        "gameplay tunnel scroll leaked into Mode 3 planet map");

    // Distinct solid tiles on each vertical page expose wrong global VOFS,
    // missing row selection, and unwanted repeats in the widescreen margins.
    auto ppu = std::make_unique<SnesPpuState>();
    ppu->bg2_screen_base = 0x1000;
    ppu->bg2_character_base = 0;
    ppu->bg2_screen_size = 2;
    ppu->bg2_scanline_scroll_enabled = true;
    for (unsigned row = 0; row < 224; ++row)
        ppu->bg2_scanline_scroll_y[row] = row % 2 ? 280 : 24;
    for (unsigned row = 0; row < 8; ++row) {
        ppu->vram[32 + row * 2] = 255; // tile 1, colour 1
        ppu->vram[64 + row * 2 + 1] = 255; // tile 2, colour 2
    }
    for (unsigned tile = 0; tile < 2048; ++tile)
        ppu->vram[0x2000 + tile * 2] = tile < 1024 ? 1 : 2;
    for (const unsigned width : {256U, 400U, 512U, 800U}) {
        starfox::render::Framebuffer frame{width, 224};
        starfox::render::BackgroundRenderer{}.draw_bg2(*ppu, 0, 91, frame,
            starfox::render::TilePriorityPass::all, (width - 256) / 2, true);
        const auto left = (width - 256U) / 2U;
        for (unsigned y = 16; y < 224; ++y)
            for (unsigned x = 0; x < width; ++x) {
                const auto expected_pixel = x >= left && x < left + 256U
                    ? (y % 2 ? 2U : 1U) : 0U;
                require(frame.get(x, y) == expected_pixel,
                    "tunnel must retain native pages with solid outer margins");
            }
    }
    std::cout << "32 source tunnel phases, override/exit, four viewport widths passed\n";
}

void check_cockpit_markers(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using starfox::timing::interpolate_cockpit_roll;
    for (const unsigned fps : {60U, 90U, 120U, 240U, 360U, 480U}) {
        for (unsigned frame = 0; frame <= fps / 20; ++frame) {
            const auto alpha = static_cast<double>(frame) / (fps / 20.0);
            require(std::abs(interpolate_cockpit_roll(0x80ff, 0x8001, alpha)
                - (255.0 + 2.0 * alpha)) < 1e-9,
                "cockpit markers did not use fractional shortest-arc roll");
        }
    }
    require(interpolate_cockpit_roll(0x00ff, 0x8040, 0.1) == 64.0,
        "entering cockpit interpolated a stale disabled HUD");
    const auto trig = starfox::simulation::TrigTables::load(rom, symbols);
    starfox::render::SoftwareRenderer renderer;
    starfox::render::Framebuffer a{400, 192, 4};
    starfox::render::Framebuffer b{400, 192, 4};
    renderer.draw_cockpit_hud(trig, 0.0, 15, 0, 88, a);
    renderer.draw_cockpit_hud(trig, 0.5, 15, 0, 88, b);
    require(a.pixels() != b.pixels(),
        "fractional cockpit rotation was rounded away at 4x");
    require(a.draw_scale() == 4 && b.draw_scale() == 4,
        "cockpit markers changed the following HUD passes' draw scale");
    a.clear(); b.clear();
    renderer.draw_cockpit_hud(trig, -0.5, 15, 0, 88, a);
    renderer.draw_cockpit_hud(trig, 255.5, 15, 0, 88, b);
    require(a.pixels() == b.pixels(), "cockpit roll wrap flickered");
    std::cout << "Cockpit markers retain fractional roll at 60-480 Hz\n";
}

void check_damage(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using namespace starfox::simulation;
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL1_2");
    for (unsigned tick = 0; tick < 200; ++tick) static_cast<void>(game->tick({}));
    auto& map = game->map();
    const auto pointer = map.read_native_word(addr("PCBOXOBJ_B"));
    const auto handle = static_cast<ObjectHandle>(
        (pointer - (addr("ALBLKS") & 0xffffU)) / addr("AL_SIZE") + 1U);
    require(game->objects().is_active(handle), "missing native body collision object");
    game->objects().at(handle).collision_object = 0;
    map.call_native_object_routine(addr("PCOLB_ISTRAT"), handle, 0x7e, 0x24, 1'000'000);
    require(map.read_native_byte(addr("SCREENFLASHCNT")) != 0,
        "body collision failed to arm source screen flash");
    auto shook = false;
    auto flashed = false;
    for (unsigned tick = 0; tick < 24; ++tick) {
        static_cast<void>(game->tick({}));
        shook |= map.read_native_byte(addr("VIEWSHAKEX")) != 0
            || map.read_native_byte(addr("VIEWSHAKEY")) != 0
            || map.read_native_byte(addr("VIEWSHAKEZ")) != 0;
        flashed |= game->colour_math_effect_state().active;
    }
    require(shook, "body damage did not execute the source camera shake");
    require(flashed, "body damage did not reach colour-math presentation");
    std::cout << "native body collision flash and camera shake passed\n";
}

void check_strategy_cadence(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using namespace starfox::simulation;
    const auto ex = !symbols.find("PLANETSEQ2_L").empty();
    ObjectPool objects{ex ? kMaximumObjects : kOriginalMaximumObjects,
        ex ? ObjectMemoryLayout::starfox_ex : ObjectMemoryLayout::original};
    MapVm map{rom, MapDatabase{rom, symbols}, objects, &symbols};
    const auto first = objects.allocate_after();
    const auto tail = objects.allocate_after(first);
    constexpr auto counter = 0x7e6830U;
    constexpr auto first_code = 0x7e6800U;
    constexpr auto tail_code = 0x7e6810U;
    // Harmless native strategy increments once; a following strategy removes
    // itself immediately, exercising the scheduler's removed-tail path.
    const std::array<std::uint8_t, 4> count_code{0xee, 0x30, 0x68, 0x6b};
    for (unsigned i = 0; i < count_code.size(); ++i) map.write_native_byte(first_code + i, count_code[i]);
    const auto remove = symbols.find("REMOVEDEADAL_L").at(0);
    const std::array<std::uint8_t, 5> remove_code{0x22,
        static_cast<std::uint8_t>(remove), static_cast<std::uint8_t>(remove >> 8),
        static_cast<std::uint8_t>(remove >> 16), 0x6b};
    for (unsigned i = 0; i < remove_code.size(); ++i) map.write_native_byte(tail_code + i, remove_code[i]);
    objects.at(first).strategy_address = first_code;
    objects.at(first).health = 1;
    objects.at(tail).strategy_address = tail_code;
    objects.at(tail).health = 1;
    NativeStrategyScheduler scheduler{symbols, objects, map};
    const auto stats = scheduler.tick_all();
    require(!objects.is_active(tail) && map.read_native_byte(counter) == 1
        && stats.objects_run == 2, "removed tail reran an already completed source strategy");
    std::cout << "native self-removal cannot double-tick player/boss logic\n";
}

void check_black_hole_music(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using namespace starfox::simulation;
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL1_2",
        std::span<const std::uint8_t>{}, true);
    starfox::audio::Spc700Audio audio;
    static_cast<void>(audio.prime_upload_sequence(game->map().take_apu_port_writes()));
    for (unsigned tick = 0; tick < 30; ++tick) static_cast<void>(audio.render_logic_tick({}));
    game->synchronize_apu_output_ports(audio.output_ports());
    for (unsigned tick = 0; tick < 30; ++tick) {
        const auto result = game->tick({});
        static_cast<void>(audio.render_logic_tick(result.audio_port_writes));
        game->synchronize_apu_output_ports(audio.output_ports());
    }
    Wdc65816Registers registers;
    game->map().call_native_routine(addr("ROUTECHANGE2_L"), registers);
    game->map().write_native_word(addr("LEVELFINISHED"), 15);
    auto selected_track = false;
    auto audible = false;
    for (unsigned tick = 0; tick < 120; ++tick) {
        const auto result = game->tick({});
        for (const auto& write : result.audio_port_writes)
            selected_track |= write.port == 0 && write.value == 0x0f;
        static_cast<void>(audio.render_logic_tick(result.audio_port_writes));
        game->synchronize_apu_output_ports(audio.output_ports());
        if (tick > 30 && selected_track)
            audible |= std::any_of(audio.last_music_samples().begin(),
                audio.last_music_samples().end(), [](auto sample) { return sample != 0; });
    }
    require(selected_track && audible,
        "black-hole map failed to select/play source SPC track $0f without MSU");
    require(game->flow_state() == GameFlowState::planet_travel,
        "black-hole map auto-entered a level without confirmation");
    std::cout << "source black-hole route selection and non-MSU music PCM passed\n";
}

void check_map_cadence(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using namespace starfox::simulation;
    constexpr std::array<int, 6> speed{6, -3, 4, 3, -5, -5};
    const auto rotations = symbols.find("ROTY1").at(0);
    for (const auto pace : {TimingMode::original_speed, TimingMode::unlocked_20_fps}) {
        for (const unsigned fps : {20U, 30U, 60U, 90U, 120U, 240U, 360U, 480U}) {
            auto game = std::make_unique<GameSimulation>(rom, symbols, "PLANETSELECT");
            game->set_timing_mode(pace);
            game->set_presentation_fps(fps);
            std::array<std::uint16_t, 6> before{};
            for (unsigned i = 0; i < 6; ++i)
                before[i] = game->map().read_native_word(rotations + i * 2);
            starfox::timing::RasterPhaseClock clock;
            for (unsigned frame = 0; frame < fps; ++frame) {
                const auto batch = clock.advance(fps);
                for (unsigned raster = 0; raster < batch.video_phases; ++raster) {
                    game->present_frame();
                    if (game->logic_tick_ready()) static_cast<void>(game->tick({}));
                }
            }
            for (unsigned i = 0; i < 6; ++i)
                require(game->map().read_native_word(rotations + i * 2)
                    == static_cast<std::uint16_t>(before[i] + speed[i] * 256 * 10),
                    "map planet angle depends on presentation FPS or gameplay pace");
        }
    }
    std::cout << "six map planets: eight FPS choices, both paces, exact one-second angles passed\n";
}

void check_venom_handoff(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using namespace starfox::simulation;
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL1_5");
    game->set_god_mode(true);
    for (unsigned tick = 0; tick < 200; ++tick) static_cast<void>(game->tick({}));
    // Enter the authored CL_DIVE call, including its dialogue, tally signal,
    // fade and later mapend__not. MAIN re-reads the exit AFTER the tally;
    // testing only an injected 7 misses this late change from ordinary clear.
    auto exit_record = std::uint32_t{};
    for (auto cursor = addr("LEVEL1_5"); cursor < addr("LEVEL1_5") + 1024; ++cursor) {
        if (rom.read8(cursor) == 40
            && (rom.read16(cursor + 1) | 0x8000U) == (addr("CL_DIVE") & 0xffffU)
            && rom.read8(cursor + 3) == (addr("CL_DIVE") >> 16)) {
            exit_record = cursor;
            break;
        }
    }
    require(exit_record != 0, "source Venom-space dive call not found");
    const auto stage = game->map().read_native_word(addr("STAGE"));
    game->map().start(exit_record, game->player());
    game->map().advance_distance(1);
    bool saw_results = false;
    bool launched = false;
    for (unsigned tick = 0; tick < 2000 && !launched; ++tick) {
        for (unsigned raster = 0; raster < 3; ++raster) game->present_frame();
        static_cast<void>(game->tick({}));
        require(game->flow_state() != GameFlowState::planet_select
            && game->flow_state() != GameFlowState::planet_travel,
            "Venom-space handoff incorrectly entered map/briefing");
        saw_results |= game->flow_state() == GameFlowState::stage_results;
        launched = saw_results && game->flow_state() == GameFlowState::gameplay;
    }
    if (!launched) std::cerr << "Venom flow=" << static_cast<int>(game->flow_state())
        << " exit=" << game->map().read_native_word(addr("LEVELFINISHED"))
        << " stage=" << game->map().read_native_word(addr("STAGE")) << '\n';
    require(launched, "Venom-space tally never launched its base stage");
    require(game->map().read_native_word(addr("STAGE")) == stage + 1,
        "Venom-space handoff skipped or repeated a route stage");
    const auto map_pointer = game->map().read_native_word(addr("NEWMAP"))
        | (static_cast<std::uint32_t>(game->map().read_native_byte(addr("NEWMAP") + 2)) << 16);
    require(map_pointer == addr("LEVEL1_6"), "Venom-space handoff launched wrong map");
    std::cout << "native Venom-space exit -> tally -> Venom base, without map/briefing passed\n";
}

void check_map_sprite_restore(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using namespace starfox::simulation;
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    const auto map_cell = [&](const char* stage, unsigned exit) {
        auto game = std::make_unique<GameSimulation>(rom, symbols,
            exit == 0 ? "PLANETSELECT" : "LEVEL1_2");
        if (exit != 0) {
        game->set_god_mode(true);
        for (unsigned tick = 0; tick < 200; ++tick) static_cast<void>(game->tick({}));
        Wdc65816Registers registers;
        game->map().call_native_routine(addr("ROUTECHANGE2_L"), registers);
        game->map().write_native_word(addr("LEVELFINISHED"), 15);
        static_cast<void>(game->tick({}));
        if (exit != 15) {
            // Preserve the actual route/campaign by entering through the map.
            // A direct EX special-map launch uses its default second campaign.
            for (unsigned tick = 0; tick < 1200
                && game->flow_state() == GameFlowState::planet_travel; ++tick) {
                for (unsigned raster = 0; raster < 3; ++raster) game->present_frame();
                const auto button = tick % 60 == 59 ? starfox::input::a : 0;
                static_cast<void>(game->tick({static_cast<starfox::input::ButtonMask>(button),
                    static_cast<starfox::input::ButtonMask>(button), 0}));
            }
            require(game->flow_state() == GameFlowState::gameplay,
                "black-hole entry through map did not launch");
            for (unsigned tick = 0; tick < 100; ++tick) static_cast<void>(game->tick({}));
        }
        if (exit != 15) {
            game->map().write_native_word(addr("LEVELFINISHED"), exit);
            static_cast<void>(game->tick({}));
        }
        require(game->flow_state() == GameFlowState::planet_travel,
            "special route exit failed to restore planet map");
        }
        for (unsigned raster = 0; raster < 8; ++raster) game->present_frame();
        starfox::render::Framebuffer frame{256, 224};
        starfox::render::BackgroundRenderer{}.draw_bg1(game->map().ppu_state(), frame);
        if (const auto* captures = std::getenv("STARFOX_HITLIST_CAPTURE_DIR")) {
            std::filesystem::create_directories(captures);
            starfox::render::write_bmp(frame,
                std::filesystem::path{captures} / (std::string{stage} + "-" + std::to_string(exit) + ".bmp"),
                starfox::render::decode_bgr555_palette(game->map().ppu_state().cgram));
        }
        // PLANETPOS entry 9 is Sector Y (SPACE4), not the black-hole entry 10.
        const auto x = rom.read8(addr("PLANETPOS") + 9 * 4 + 2);
        const auto y = rom.read8(addr("PLANETPOS") + 9 * 4 + 3);
        std::array<std::uint16_t, 32 * 32> cell{};
        for (unsigned row = 0; row < 32; ++row)
            for (unsigned column = 0; column < 32; ++column)
                cell[row * 32 + column] = game->map().ppu_state().cgram[
                    frame.get(x + column, y + row)];
        return cell;
    };
    const auto normal = map_cell("LEVEL1_2", 15);
    // EX's direct selector starts a different campaign than this route fixture.
    if (symbols.find("PLANETSEQ2_L").empty()) {
        require(normal == map_cell("PLANETSELECT", 0),
            "Sector Y pixels/palette changed when black hole was activated");
    }
    require(std::any_of(normal.begin(), normal.end(), [](auto colour) { return colour != 0; }),
        "Sector Y reference cell is empty");
    for (const auto exit : {11U, 12U, 13U})
        require(normal == map_cell("LEVEL_BLACKHOLE", exit),
            "Sector Y pixels/palette changed after leaving black hole");
    std::cout << "Sector Y map pixels/palette preserved across all three black-hole exits\n";
}

void check_map_icon_texels(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    starfox::simulation::Wdc65816 cpu{rom, &symbols};
    const auto launch = [&](const char* name) {
        const auto entry = addr(name);
        cpu.write8(0x3034, static_cast<std::uint8_t>(entry >> 16U));
        cpu.write8(0x301e, static_cast<std::uint8_t>(entry));
        cpu.write8(0x301f, static_cast<std::uint8_t>(entry >> 8U));
    };
    // Draw each visit's small map icon and enlarged selection icon. Compare
    // against packed ROM texels rather than another potentially wrong map.
    for (const auto index : {9U, 10U, 14U}) {
        const auto sprite = rom.read8(addr("PLANETSPRS") + index * 2U) & 0x7fU;
        const auto pointer = addr("TEXTUREADDRTAB") + sprite * 3U;
        const auto texture = rom.read8(pointer) | (rom.read8(pointer + 1U) << 8U)
            | (rom.read8(pointer + 2U) << 16U);
        for (const auto size : {32U, 64U}) {
            for (unsigned byte = 0; byte < 16384U; ++byte)
                cpu.write8(0x700000U + addr("BITMAP1") + byte, 0);
            cpu.write16(addr("MSPRITE"), sprite);
            cpu.write16(addr("M_XC"), 64);
            cpu.write16(addr("M_YC"), 64);
            cpu.write16(addr("MSPR_PAL"), 6);
            cpu.write16(addr("M_SPRSIZE"), 32);
            cpu.write16(addr("M_SPRXSCALE"), size);
            launch(size == 32U ? "MDRAWSPRITE32" : "MUSPRITE");
            for (unsigned y = 0; y < size; ++y) for (unsigned x = 0; x < size; ++x) {
                const auto expected_texel = (rom.read8(texture + (y * 32U / size) * 256U
                    + x * 32U / size) >> ((sprite & 32U) ? 4U : 0U)) & 15U;
                const auto px = 64U - size / 2U + x, py = 64U - size / 2U + y;
                const auto tile = (px / 8U) * 16U + py / 8U;
                const auto base = 0x700000U + addr("BITMAP1") + tile * 64U + (py & 7U) * 2U;
                unsigned pixel{};
                for (unsigned plane = 0; plane < 8U; ++plane)
                    pixel |= ((cpu.read8(base + (plane / 2U) * 16U + (plane & 1U))
                        >> (7U - (px & 7U))) & 1U) << plane;
                require(pixel == (expected_texel ? 0x60U | expected_texel : 0U),
                    "Sector Y/Black Hole/Out of This Dimension icon used the wrong packed texels");
            }
        }
    }
    std::cout << "map and zoom icons use correct packed texture banks\n";
}
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        check_reticle_identity();
        for (const unsigned fps : {20U, 30U, 60U, 90U, 120U, 240U, 360U, 480U}) {
            for (unsigned phase = 0; phase <= fps; ++phase) {
                const auto alpha = static_cast<double>(phase) / fps;
                const auto up = starfox::timing::interpolate_wrapped_scroll(511, 0, alpha, 511);
                const auto down = starfox::timing::interpolate_wrapped_scroll(0, 511, alpha, 511);
                require((up == 511 || up == 0) && (down == 0 || down == 511),
                    "BG2 wrap interpolated across the entire background");
            }
        }
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        check_score_digits(rom, symbols);
        check_ex_reticle_stations(rom, symbols);
        check_map_icon_texels(rom, symbols);
        check_escape_explosions(rom, symbols);
        check_cockpit_markers(rom, symbols);
        check_tunnel(rom, symbols);
        check_damage(rom, symbols);
        check_strategy_cadence(rom, symbols);
        check_black_hole_music(rom, symbols);
        check_map_cadence(rom, symbols);
        check_venom_handoff(rom, symbols);
        check_map_sprite_restore(rom, symbols);
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
