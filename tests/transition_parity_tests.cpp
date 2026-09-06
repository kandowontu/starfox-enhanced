#include "starfox/assets/rom.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/object_pool.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void source_tick(starfox::simulation::GameSimulation& game,
    const starfox::input::TickInput& input = {}) {
    game.present_frame();
    game.present_frame();
    game.present_frame();
    static_cast<void>(game.tick(input));
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: starfox_transition_parity_tests ROM SYMBOLS\n";
        return 2;
    }

    const auto rom = starfox::assets::RomImage::load(argv[1]);
    const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    const auto starfox_ex = !symbols.find("PLANETSEQ2_L").empty();
    starfox::simulation::ObjectPool objects{
        starfox_ex ? starfox::simulation::kMaximumObjects
                   : starfox::simulation::kOriginalMaximumObjects,
        starfox_ex ? starfox::simulation::ObjectMemoryLayout::starfox_ex
                   : starfox::simulation::ObjectMemoryLayout::original};
    starfox::simulation::MapDatabase database{rom, symbols};
    starfox::simulation::MapVm map{rom, database, objects, &symbols};
    const auto game_frame = symbols.find("GAMEFRAME").front();

    // IRQ.ASM's QFADEDOWN has one DEC and no fall-through second step.
    map.set_display_brightness(11U);
    map.start_display_fade(-2);
    map.tick_video_phase();
    require(map.display_brightness() == 10U,
        "quick fade-down did not decrement by one raster step");
    for (std::uint8_t brightness = 9U; brightness != 0U; --brightness) {
        map.tick_video_phase();
        require(map.display_brightness() == brightness,
            "quick fade-down skipped a native brightness value");
    }
    map.tick_video_phase();
    require(map.display_brightness() == 0U && map.fade_direction() == 0,
        "quick fade-down did not finish on its eleventh raster");

    // SFADEDOWN skips odd GAMEFRAME values. SETINIDISP is still invoked on
    // every raster, so the three presentations of an even source frame each
    // consume a brightness value.
    map.set_display_brightness(15U);
    map.write_native_byte(game_frame, 2U);
    map.start_display_fade(-3);
    map.tick_video_phase();
    map.tick_video_phase();
    map.tick_video_phase();
    require(map.display_brightness() == 12U,
        "slow fade did not advance on every raster of even GAMEFRAME");
    map.write_native_byte(game_frame, 3U);
    map.tick_video_phase();
    map.tick_video_phase();
    map.tick_video_phase();
    require(map.display_brightness() == 12U,
        "slow fade advanced during odd GAMEFRAME");
    map.write_native_byte(game_frame, 4U);
    map.tick_video_phase();
    require(map.display_brightness() == 11U,
        "slow fade did not resume on the next even GAMEFRAME");

    // QFADEUP performs two increments before falling into SETUP's third.
    map.set_display_brightness(0U);
    map.start_display_fade(2);
    map.tick_video_phase();
    require(map.display_brightness() == 3U,
        "quick fade-up did not advance three brightness steps");

    // ENDSEQ.ASM intentionally gives the EX logo/intro fifteen additional
    // source ticks before accepting a skip (45 versus retail's 30).
    auto game = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "BOOT");
    game->set_experience(starfox_ex
        ? starfox::simulation::Experience::starfox_ex
        : starfox::simulation::Experience::original);
    source_tick(*game, {starfox::input::start, starfox::input::start, 0U});
    for (std::size_t tick = 0;
         tick < 2'000U
            && game->flow_state()
                != starfox::simulation::GameFlowState::intro;
         ++tick) {
        source_tick(*game);
    }
    require(game->flow_state() == starfox::simulation::GameFlowState::intro,
        "pre-game fade did not reach the intro");
    const auto minimum_intro_ticks = starfox_ex ? 45U : 30U;
    for (std::uint32_t tick = 1U; tick < minimum_intro_ticks; ++tick) {
        const auto press_early = tick + 1U == minimum_intro_ticks;
        source_tick(*game, press_early
            ? starfox::input::TickInput{
                  starfox::input::start, starfox::input::start, 0U}
            : starfox::input::TickInput{});
    }
    require(game->flow_state() == starfox::simulation::GameFlowState::intro,
        "intro accepted a skip before its source threshold");
    source_tick(*game,
        {starfox::input::start, starfox::input::start, 0U});
    require(game->map().fade_direction() == -2
            && game->map().display_brightness() == 11U,
        "intro did not arm its 11-step quick fade at the source threshold");

    auto continue_game = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "CONTINUE");
    require(continue_game->map().display_brightness() == 0U,
        "Continue screen did not begin under forced black");
    continue_game->present_frame();
    require(continue_game->map().display_brightness() == 0U
            && continue_game->map().fade_direction() == 1,
        "Continue fade-in omitted its initial black raster");
    for (std::uint32_t frame = 1U; frame <= 15U; ++frame) {
        continue_game->present_frame();
        if (continue_game->map().display_brightness() != frame) {
            std::cerr << "Continue fade-in expected " << frame << " got "
                      << static_cast<unsigned>(
                             continue_game->map().display_brightness())
                      << '\n';
            require(false,
                "Continue fade-in skipped a manual brightness value");
        }
        if (frame % 3U == 0U) {
            const auto early_input = frame == 3U
                ? starfox::input::TickInput{
                      starfox::input::a, starfox::input::a, 0U}
                : starfox::input::TickInput{};
            static_cast<void>(continue_game->tick(early_input));
            require(continue_game->flow_state()
                    == starfox::simulation::GameFlowState::continue_choice,
                "Continue screen accepted input during its fade-in");
        }
    }
    static_cast<void>(continue_game->tick(
        {starfox::input::a, starfox::input::a, 0U}));
    require(continue_game->map().display_brightness() == 15U,
        "Continue choice cut away before its fade-out");
    continue_game->present_frame();
    require(continue_game->map().display_brightness() == 15U
            && continue_game->map().fade_direction() == -1,
        "Continue fade-out omitted its duplicate full-bright raster");
    for (std::uint32_t frame = 14U; frame != 0U; --frame) {
        continue_game->present_frame();
        require(continue_game->map().display_brightness() == frame,
            "Continue fade-out skipped a manual brightness value");
    }
    continue_game->present_frame();
    require(continue_game->map().display_brightness() == 0U,
        "Continue fade-out did not reach black");
    static_cast<void>(continue_game->tick({}));
    require(continue_game->flow_state()
            != starfox::simulation::GameFlowState::continue_choice,
        "Continue choice did not transition after completing its fade-out");
}
