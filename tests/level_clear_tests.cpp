#include "starfox/simulation/game_simulation.hpp"
#include "starfox/timing/fixed_step.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace starfox::simulation;
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
struct ClearCase { const char* level; const char* clear; bool ex_only{}; };
constexpr ClearCase cases[] = {
    {"LEVEL1_2", "CL_WARP"}, {"LEVEL1_1", "CL_GROUND"},
    {"LEVEL1_3", "CL_SHIP1_3"}, {"LEVEL1_5", "CL_DIVE"},
    {"LEVEL2_2", "CL_EARTH"}, {"LEVEL2_3", "CL_BRIDGE"},
    {"LEVEL2_4", "CL_TURN"}, {"LEVEL3_2", "CL_CHASE"},
    {"LEVEL3_4", "CL_SHIP3_4"}, {"LEVEL3_5", "CL_UNDER"},
    {"LEVEL6_3", "CL_TURN2", true}, {"LEVEL_COMET", "CL_COMET", true},
};

struct ClearTimeline {
    unsigned tally{}, hidden{}, done{}, ticks{};
    std::vector<std::array<std::uint64_t, 4>> states;
    bool operator==(const ClearTimeline&) const = default;
};

ClearTimeline run_clear(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols, ClearCase test, bool ex, unsigned fps,
    TimingMode pace = TimingMode::original_speed, bool fast_end = false) {
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    if (ex && std::string_view{test.clear} == "CL_EARTH") test.level = "LEVEL1_1";
    auto game = std::make_unique<GameSimulation>(rom, symbols, test.level);
    game->set_god_mode(true);
    game->set_timing_mode(TimingMode::unlocked_20_fps);
    unsigned warmup = 0;
    do { static_cast<void>(game->tick({})); ++warmup; }
    while (warmup < 200 || (warmup < 2000
        && (game->map().read_native_byte(addr("PSHIPFLAGS")) & 0x20U) != 0U));
    if (ex) game->map().write_native_byte(addr("FASTEND"), fast_end ? 1 : 0);
    auto call = std::uint32_t{};
    const auto start = addr(test.level);
    auto end = std::min(start + 16'384U, (start & 0xff0000U) + 0xfffcU);
    for (const auto& [name, values] : symbols.entries()) {
        if (!name.starts_with("LEVEL")) continue;
        for (const auto value : values)
            if (value > start && value < end) end = value;
    }
    for (auto cursor = start; cursor + 3U < end; ++cursor) {
        if (rom.read8(cursor) == 40
            && (rom.read16(cursor + 1) | 0x8000U) == (addr(test.clear) & 0xffffU)
            && rom.read8(cursor + 3) == (addr(test.clear) >> 16)) { call = cursor; break; }
    }
    require(call != 0, std::string{test.level} + " missing authored " + test.clear + " call");
    // Keep the authored return continuation, not a synthesized exit flag.
    game->map().start(call, game->player());
    game->map().advance_distance(1);
    game->set_timing_mode(pace);
    game->set_presentation_fps(fps);
    starfox::timing::RasterPhaseClock clock;
    unsigned ticks{}, first_tally{}, hidden_at{}, finished_at{}, raster_count{};
    bool saw_tally = false, finished = false;
    ClearTimeline timeline;
    unsigned phases_this_tick{};
    std::map<std::uint32_t, unsigned> delayed_strategies;
    std::uint8_t previous_exit = 0xff;
    for (unsigned frame = 0; frame < fps * 120U && !finished; ++frame) {
        const auto batch = clock.advance(fps);
        for (unsigned phase = 0; phase < batch.video_phases; ++phase) {
            game->present_frame(); ++raster_count; ++phases_this_tick;
            if (!game->logic_tick_ready()) continue;
            if (phases_this_tick > 3 && game->objects().is_active(game->player()))
                delayed_strategies[game->objects().at(game->player()).strategy_address] += phases_this_tick - 3;
            phases_this_tick = 0;
            static_cast<void>(game->tick({})); ++ticks;
            const auto result = game->stage_results_state();
            const auto exit = game->map().read_native_byte(addr("CLB2"));
            if (result.active && exit != 0 && !(ex && exit == 2))
                require(!result.visible, std::string{test.clear} + " redisplays score/avatars after the hide signal");
            const auto& player = game->objects().at(game->player());
            const auto packed_position = std::uint64_t{static_cast<std::uint16_t>(player.world_x)}
                | (std::uint64_t{static_cast<std::uint16_t>(player.world_y)} << 16)
                | (std::uint64_t{static_cast<std::uint16_t>(player.world_z)} << 32);
            timeline.states.push_back({raster_count, game->map().cursor(), packed_position,
                std::uint64_t{player.strategy_address} | (std::uint64_t{exit} << 32)
                    | (std::uint64_t{static_cast<unsigned>(game->flow_state())} << 40)});
            if (result.active && !saw_tally) { saw_tally = true; first_tally = raster_count; }
            if (result.active && exit == (ex ? 3 : 2)) {
                if (hidden_at == 0) hidden_at = raster_count;
                require(!result.visible, std::string{test.clear} + " keeps score/avatars during warp");
            }
            if (result.active && exit != previous_exit) {
                std::cout << test.clear << " CLB2=" << unsigned(exit)
                    << " tick=" << ticks << " raster=" << raster_count << '\n' << std::flush;
            }
            previous_exit = exit;
            finished = saw_tally && !result.active;
            if (fast_end && !saw_tally) {
                finished = game->flow_state() != GameFlowState::gameplay
                    || (std::string_view{test.clear} == "CL_DIVE"
                        && game->map().background() == addr("BG_1_6A") - addr("BGLISTS"));
            }
            if (finished) { finished_at = raster_count; break; }
        }
    }
    std::cout << test.level << '/' << test.clear << " fps=" << fps
        << " pace=" << (pace == TimingMode::original_speed ? "original" : "unlocked")
        << " fast=" << fast_end
        << " tally=" << first_tally / 60.0 << "s hide=" << hidden_at / 60.0
        << "s done=" << finished_at / 60.0 << "s ticks=" << ticks
        << " flow=" << unsigned(game->flow_state()) << " warmup=" << warmup
        << " cursor=" << std::hex << game->map().cursor() << " strat="
        << game->objects().at(game->player()).strategy_address << std::dec
        << " wait=" << game->map().countdown() << '\n' << std::flush;
    if (!finished) {
        for (const auto* name : {"BG2XSCROLL", "GAMEFLAGS", "PSVAR_BYTE1", "PLAYERFLYMODE"})
            std::cerr << name << '=' << unsigned(game->map().read_native_byte(addr(name))) << ' ';
        std::cerr << "sbyte3=" << unsigned(game->objects().read_base_byte(game->player(), 0x24)) << '\n';
    }
    for (const auto& [strategy, delay] : delayed_strategies) {
        std::cout << " extra rasters " << std::hex << strategy << std::dec << '=' << delay;
        for (const auto& [name, values] : symbols.entries())
            if (std::find(values.begin(), values.end(), strategy) != values.end())
                std::cout << ' ' << name;
        std::cout << '\n';
    }
    require(saw_tally || fast_end, std::string{test.clear} + " never reached tally within 120 seconds");
    require(finished, std::string{test.clear} + " never finished its clear sequence");
    if (std::string_view{test.clear} == "CL_WARP" && !fast_end) {
        require(hidden_at > first_tally && hidden_at < finished_at,
            "warp clear did not hide the tally before scene completion");
        require(hidden_at - first_tally == 300U,
            "warp tally did not honor the source 100-update / 5-second hold");
    }
    timeline.tally = first_tally; timeline.hidden = hidden_at;
    timeline.done = finished_at; timeline.ticks = ticks;
    return timeline;
}

void check_ship_selection_scratch(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL1_1");
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    for (const auto* routine : {"SETSHIP", "SETSHIP2"}) {
        for (unsigned ship = 0; ship < 8; ++ship) {
            for (unsigned enhanced = 0; enhanced <= 1; ++enhanced) {
                game->map().write_native_byte(addr("CURR_SHIP"), ship);
                game->map().write_native_byte(addr("SHPMODE"), enhanced);
                game->objects().write_base_byte(game->player(), 0x24, 100);
                static_cast<void>(game->map().call_native_object_routine(addr(routine), game->player()));
                require(game->objects().read_base_byte(game->player(), 0x24) == 100,
                    std::string{routine} + " overwrote a caller's clear countdown");
                const auto expected = ship == 0 && enhanced
                    ? addr("P1SHPARWING") : rom.read16(addr("SHIPLISTSHAPE") + ship * 2U);
                require(game->objects().at(game->player()).shape == (expected & 0xffffU),
                    std::string{routine} + " changed source ship selection");
            }
        }
    }
}

unsigned check_colony_exit(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols, bool ex, unsigned fps) {
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL2_6");
    game->set_god_mode(true);
    game->set_timing_mode(TimingMode::unlocked_20_fps);
    for (unsigned tick = 0; tick < 200; ++tick) static_cast<void>(game->tick({}));
    const auto target = addr(ex ? "CL_COLON" : "MAP2_6A");
    std::uint32_t entry{};
    for (auto pc = addr("LEVEL2_6"); pc < addr("LEVEL2_6") + 128U; ++pc) {
        if (rom.read8(pc) == 40 && (rom.read16(pc + 1) | 0x8000U) == (target & 0xffffU)
            && rom.read8(pc + 3) == (target >> 16)) { entry = pc; break; }
    }
    require(entry != 0, "colony exit source continuation not found");
    // Retail includes CL_COLON inline, after MAP2_6A + compressed mapwait 4000.
    // EX instead calls that exact sequence as a subroutine.
    if (!ex) {
        require(rom.read8(entry + 5) == 250, "colony exit wait encoding changed");
        entry += 6;
    }
    game->map().start(entry, game->player());
    game->map().advance_distance(1);
    game->set_presentation_fps(fps);
    starfox::timing::RasterPhaseClock clock;
    unsigned rasters{};
    bool saw_colony = false;
    for (unsigned frame = 0; frame < fps * 120U; ++frame) {
        const auto batch = clock.advance(fps);
        for (unsigned phase = 0; phase < batch.video_phases; ++phase) {
            game->present_frame(); ++rasters;
            if (!game->logic_tick_ready()) continue;
            static_cast<void>(game->tick({}));
            require(game->flow_state() == GameFlowState::gameplay,
                "colony exit incorrectly inserted a score/map screen before the final tunnel");
            saw_colony |= game->map().background() == addr("BG_2_6B") - addr("BGLISTS");
            if (game->map().cursor() >= addr("FINAL_TUNNEL")
                && game->map().cursor() < addr("FINAL_TUNNEL") + 512U) {
                require(saw_colony, "colony exit skipped its winding corridor");
                std::cout << "CL_COLON fps=" << fps << " final tunnel at " << rasters / 60.0 << "s\n";
                return rasters;
            }
        }
    }
    throw std::runtime_error{"colony exit did not reach the final tunnel"};
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        const auto ex = !symbols.find("NOCROSSHAIRPLS").empty();
        if (ex) check_ship_selection_scratch(rom, symbols);
        require(check_colony_exit(rom, symbols, ex, 20) == check_colony_exit(rom, symbols, ex, 120),
            "colony corridor timing changed with presentation FPS");
        bool failed = false;
        for (const auto test : cases) {
            if (test.ex_only && !ex) continue;
            try {
                const auto baseline = run_clear(rom, symbols, test, ex, 20);
                for (const auto fps : {30U, 60U, 90U, 120U, 240U, 360U, 480U})
                    require(run_clear(rom, symbols, test, ex, fps) == baseline,
                        std::string{test.clear} + " changed timing/ship path with presentation FPS");
                const auto unlocked = run_clear(rom, symbols, test, ex, 20, TimingMode::unlocked_20_fps);
                require(run_clear(rom, symbols, test, ex, 120, TimingMode::unlocked_20_fps) == unlocked,
                    std::string{test.clear} + " changed unlocked timing at 120 FPS");
                if (ex) {
                    const auto fast = run_clear(rom, symbols, test, ex, 20, TimingMode::original_speed, true);
                    require(run_clear(rom, symbols, test, ex, 120, TimingMode::original_speed, true) == fast,
                        std::string{test.clear} + " changed fast-end timing at 120 FPS");
                }
            }
            catch (const std::exception& error) { failed = true; std::cerr << "FAILED: " << error.what() << '\n'; }
        }
        return failed ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
