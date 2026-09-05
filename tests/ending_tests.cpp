#include "starfox/assets/rom.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/simulation/game_simulation.hpp"

#include <array>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void check_ending_irq(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using namespace starfox::simulation;
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    Wdc65816 cpu{rom, &symbols};
    const std::array saved{"BG1HOFSBAK", "BG1VOFSBAK", "BG2HOFSBAK",
        "BG2VOFSBAK", "BG3HOFSBAK", "BG3VOFSBAK"};
    const std::array requested{"BG1HOFSREQ", "BG1VOFSREQ", "BG2HOFSREQ",
        "BG2VOFSREQ", "BG3HOFSREQ", "BG3VOFSREQ"};
    const std::array<std::int16_t, 6> initial{0, 0, 0, 256, 128, -17};
    const std::array<std::int16_t, 6> target{-48, 24, 0, 280, 32, 31};
    const std::array<std::int16_t, 6> after{-2, 1, 0, 257, 124, -15};
    for (unsigned i = 0; i < saved.size(); ++i) {
        cpu.write16(addr(saved[i]), initial[i]);
        cpu.write16(addr(requested[i]), target[i]);
    }
    cpu.tick_ending_video_phase();
    require(cpu.read16(addr(saved[3])) == 256, "SEQSCROLL ran outside ending IRQ mode");
    cpu.write8(0, 32);
    cpu.tick_ending_video_phase();
    const auto& ppu = cpu.ppu_state();
    const std::array observed{ppu.bg1_scroll_x, ppu.bg1_scroll_y, ppu.bg2_scroll_x,
        ppu.bg2_scroll_y, ppu.bg3_scroll_x, ppu.bg3_scroll_y};
    require(observed == initial, "SEQSCROLL must publish all six saved scroll registers first");
    for (unsigned i = 0; i < saved.size(); ++i) {
        require(cpu.read16(addr(saved[i])) == static_cast<std::uint16_t>(after[i]),
            "SEQSCROLL speed or signed RAMCHASE direction differs from IRQ.ASM");
    }
    for (unsigned phase = 1; phase < 25; ++phase) cpu.tick_ending_video_phase();
    const std::array settled{ppu.bg1_scroll_x, ppu.bg1_scroll_y, ppu.bg2_scroll_x,
        ppu.bg2_scroll_y, ppu.bg3_scroll_x, ppu.bg3_scroll_y};
    require(settled == target, "boss panels failed to settle after 24 source rasters");

    // Service ENDSEQBIT3 through a harmless RTS, preserving the normal CPU
    // service path rather than giving the test a separate text implementation.
    cpu.write8(0x7e0200, 0x60);
    cpu.write8(0x2115, 0x80);
    cpu.write8(addr("NOIRQBIT3"), 1);
    cpu.write8(addr("BGFLAGS"), 0x10);
    cpu.write16(addr("SEQ_TPTR"), 0);
    const std::array<std::uint8_t, 7> text{1, 0x21, 0, 'A', 'B', 0, 0};
    for (unsigned i = 0; i < text.size(); ++i) cpu.write8(addr("SEQ_BUFFER") + i, text[i]);
    cpu.write8(addr("ETESTTRANS") - 32 + 'A', 9);
    cpu.write8(addr("ETESTTRANS") - 32 + 'B', 10);
    const auto transfer = [&] {
        cpu.write8(0, 30);
        Wdc65816Registers registers;
        static_cast<void>(cpu.call_near(0x7e0200, registers, 100, true));
    };
    transfer();
    require(cpu.read16(addr("SEQ_TPTR")) == 4 && cpu.read16(addr("SEQ_VM")) == 0x7022,
        "SEQTEXT did not consume position + exactly one character");
    require(ppu.vram[0xe042] == 9 && ppu.vram[0xe043] == 0x21,
        "SEQTEXT wrote the wrong tile/palette/priority");
    cpu.tick_ending_video_phase();
    require(cpu.read16(addr("SEQ_TPTR")) == 4, "idle raster incorrectly typed another character");
    transfer();
    require(cpu.read16(addr("SEQ_TPTR")) == 5 && ppu.vram[0xe044] == 10,
        "second ENDSEQBIT3 did not type the second character");
    transfer();
    require((cpu.read8(addr("BGFLAGS")) & 0x10) == 0, "SEQTEXT did not stop at the terminator");
}

void run_ending(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols, bool original_pace,
    bool special_route) {
    using namespace starfox::simulation;
    const auto ex = !symbols.find("PLANETSEQ2_L").empty();
    const auto address = [&](const char* name) {
        require(!symbols.find(name).empty(), std::string{"missing symbol "} + name);
        return symbols.find(name).front();
    };
    auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL1_6");
    game->set_timing_mode(original_pace ? TimingMode::original_speed : TimingMode::unlocked_20_fps);
    game->set_presentation_fps(original_pace ? 120U : 20U);
    game->set_god_mode(true);
    auto& map = game->map();

    // Start at the real map continuation after Andross, NOT by injecting
    // LEVELFINISHED=6. Run the tunnel escape, radio, camera, tally and ending.
    // These scores and defeated bosses stand in for the previous stages.
    std::array<unsigned, 6> scores{100, 90, 80, 70, 60, 100};
    if (special_route) scores[2] = 101; // Excluded from total and average.
    map.write_native_word(address("SPECPTR"), scores.size());
    unsigned total = 100, stages = 1; // The final encounter is a 100% clear.
    for (unsigned i = 0; i < scores.size(); ++i) {
        map.write_native_byte(address("SPECBUF") + i, scores[i]);
        if (scores[i] != 101) { total += scores[i]; ++stages; }
    }
    const std::array retail_bosses{"BOSS11", "BOSS12", "BOSS13", "BOSS14", "BOSS15", "BOSSFINAL"};
    const std::array ex_bosses{"BOSS51", "BOSS52", "BOSS53", "BOSS54", "BOSS55", "BOSSFINAL5"};
    const auto& bosses = ex && special_route ? ex_bosses : retail_bosses;
    if (ex && special_route) map.write_native_byte(address("WHICHROUTE"), 4U);
    for (unsigned i = 0; i < bosses.size(); ++i) {
        map.write_native_word(address("BOSS_SEQ") + 2 * i,
            address(bosses[i]) - address("ENDSEQBOSS"));
    }
    map.write_native_word(address("BOSS_PTR"), 2 * bosses.size());
    game->start_map("FINALMAP_END");

    unsigned ending_start = 0, total_tick = 0, average_tick = 0;
    unsigned credits_tick = 0, finished_tick = 0;
    unsigned next_stage_card = 1, last_card_tick = 0;
    bool camera_orbit = false, camera_close = false, voice = false;
    bool average_value = false, final_total = false, final_average = false;
    bool boss_text_typed = false, boss_palette_checked = false;
    std::set<unsigned> seen_bosses, credits_text;
    std::array<unsigned, 6> boss_first_tick{}, boss_initial_count{};
    const auto credits_map = address("CREDITSMAP");
    const auto expected_average = static_cast<std::uint16_t>(address("MSG_00") + 5 * (total / stages));
    unsigned terminal_frames = 0;
    for (unsigned tick = 1; tick < 9000; ++tick) {
        if (game->flow_state() == GameFlowState::gameplay) {
            map.write_native_byte(address("SPECIALOBJTOTAL"), 100U);
            map.write_native_byte(address("SPECIALS_DEAD"), 100U);
        }
        const auto result = game->tick({});
        for (const auto command : result.sound_effect_commands) voice |= command == 0x0dU;
        const auto flow = game->flow_state();
        if (flow == GameFlowState::credits && ending_start == 0) {
            ending_start = tick;
            require(tick < 1200, "escape stalled waiting for world-coordinate wrap");
            require(map.read_native_word(address("SPECPTR")) == 7, "final tally omitted or recorded twice");
            require(map.read_native_byte(address("SPECBUF") + 6) == 100, "final stage tally is incorrect");
        }
        const auto bank = map.read_native_byte(address("MAPBANK"));
        const auto pc = 0x8000U | map.read_native_word(address("MAPPTR"));
        if (!credits_tick && bank == credits_map >> 16 && pc >= (credits_map & 0xffffU)
            && pc < (credits_map & 0xffffU) + 0x100U && seen_bosses.size() == bosses.size()) {
            credits_tick = tick;
            require(camera_orbit && camera_close && voice, "Pepper radio/camera sequence was skipped");
            require(total_tick && average_tick && average_value, "mission totals/average were not displayed");
        }
        if (ending_start && !credits_tick && seen_bosses.empty()) {
            const auto card = map.read_native_word(address("VRAM2ADDR"));
            if (card == next_stage_card && next_stage_card <= 7) {
                if (next_stage_card > 1) require(tick - last_card_tick == 30, "stage cards are not 30 source transfers apart");
                last_card_tick = tick;
                ++next_stage_card;
            }
        }
        for (const auto handle : game->objects().active_handles()) {
            const auto& obj = game->objects().at(handle);
            if (obj.strategy_address == address("VIEWOUTOFLB3_STRAT")) {
                require(map.read_native_word(address("VIEWTOOBJ")) != 0, "escape camera lost the ship target");
                camera_orbit |= obj.strategy_state == 4;
                camera_close |= obj.strategy_state == 6;
            }
            if ((obj.strategy_flags[0] & 0x40) == 0) continue;
            if (!credits_tick) {
                if (obj.colour_table == static_cast<std::uint16_t>(address("MSG_TOTAL")) && !total_tick) total_tick = tick;
                if (obj.colour_table == static_cast<std::uint16_t>(address("MSG_AVE")) && !average_tick) average_tick = tick;
                if (average_tick && obj.colour_table == expected_average) average_value = true;
            } else {
                credits_text.insert(obj.colour_table);
                final_total |= obj.colour_table == static_cast<std::uint16_t>(address("MSG_TOTAL"));
                final_average |= obj.colour_table == expected_average;
            }
        }
        if (ending_start && !credits_tick) {
            const auto cursor = map.read_native_word(address("BOSS_PTR"));
            const auto countdown = map.read_native_word(address("DEMOCNT"));
            if (game->boss_roll_active()) {
                boss_text_typed |= map.read_native_word(address("SEQ_TPTR")) > 20;
                if (cursor == 2 && countdown == 100) {
                    const auto& palette = map.ppu_state().cgram;
                    require(palette[0] == 0, "boss roll's black backdrop was overwritten");
                    const auto dossier_palette = address(ex && special_route ? "BGETEST0PAC" : "BGETESTPAC");
                    for (unsigned colour = 1; colour < 112; ++colour) {
                        require(palette[colour] == map.read_native_word(dossier_palette + 2 * colour),
                            "boss dossier palette replaced by preceding gameplay palette");
                    }
                    boss_palette_checked = true;
                }
            }
            for (unsigned i = 0; i < bosses.size(); ++i) {
                if (cursor == 2 * (i + 1) && countdown > 0 && countdown <= 300
                    && map.read_native_word(address("SEQ_HANDLER")) != 0
                    && map.read_native_word(address("BOSS_SEQ") + i * 2)
                        == address(bosses[i]) - address("ENDSEQBOSS"))
                {
                    if (seen_bosses.insert(i).second) {
                        boss_first_tick[i] = tick;
                        boss_initial_count[i] = countdown;
                        require(game->objects().active_count() > 3, "boss roll has no model objects");
                    }
                }
            }
        }
        if (flow == GameFlowState::finished) {
            if (!finished_tick) finished_tick = tick;
            // Allow FADEINTOTAL to settle; the original keeps TRANSFER alive.
            if (++terminal_frames == 120) break;
        }
        if (ex && credits_tick && tick - credits_tick > 3400) {
            static_cast<void>(game->tick({0, starfox::input::start, 0}));
            require(game->flow_state() == GameFlowState::ex_pregame_menu, "EX credits do not accept Start after THE END");
            finished_tick = tick;
            break;
        }
    }
    require(ending_start && credits_tick && finished_tick, "ending failed to reach every phase in bounded source time");
    require(next_stage_card == 8, "not all seven stage cards were presented");
    require(average_tick - total_tick == 50, "total/average timing differs from MAIN.ASM's two 25-transfer waits");
    require(seen_bosses.size() == bosses.size(), "defeated bosses missing from roll");
    require(boss_text_typed && boss_palette_checked, "boss presentation checks never exercised");
    for (unsigned i = 0; i + 1 < bosses.size(); ++i) {
        require(boss_first_tick[i + 1] - boss_first_tick[i] == boss_initial_count[i] + 38,
            std::string{"boss display/wipe timing mismatch: "} + bosses[i] + " got "
                + std::to_string(boss_first_tick[i + 1] - boss_first_tick[i])
                + " expected " + std::to_string(boss_initial_count[i] + 38));
    }
    require(credits_text.size() >= (ex ? 3U : 20U), "staff credits were skipped or empty");
    if (!ex) {
        require(final_total && final_average, "final total and average are absent");
        require(game->final_score_active(), "final score lost its active presentation state");
        const auto scroll = map.background_scroll_override();
        require(scroll && (*scroll)[0] == 0 && (*scroll)[1] == 0,
            "credits lost the source's fixed planet-horizon scroll override");
        const auto before = map.read_native_word(address("GAMEFRAME"));
        static_cast<void>(game->tick({}));
        require(map.read_native_word(address("GAMEFRAME")) != before, "final score animation froze");
    }
    std::cout << (ex ? "EX" : "Original") << (original_pace ? " original/120Hz" : " unlocked/20Hz")
        << (special_route ? " special-route" : " route-1") << ": escape=" << ending_start
        << " total=" << total_tick << " average=" << average_tick << " credits=" << credits_tick
        << " final=" << finished_tick << " bosses=" << seen_bosses.size()
        << " credit-strings=" << credits_text.size() << '\n';
}
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const auto rom = starfox::assets::RomImage::load(argv[1]);
        const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
        check_ending_irq(rom, symbols);
        run_ending(rom, symbols, false, false);
        run_ending(rom, symbols, true, true);
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
