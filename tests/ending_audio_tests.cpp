#include "starfox/assets/rom.hpp"
#include "starfox/audio/msu1_audio.hpp"
#include "starfox/audio/msu1_pack.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/simulation/game_simulation.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
bool audible(std::span<const std::int16_t> samples) {
    return std::any_of(samples.begin(), samples.end(), [](auto s) { return s > 8 || s < -8; });
}
struct MusicHistory {
    unsigned first{}, last{}, quiet{}, longest_quiet{}, reprise{};
    void observe(bool sound, unsigned tick) {
        if (sound) {
            if (!first) first = tick;
            if (quiet > longest_quiet) longest_quiet = quiet;
            if (quiet >= 2000 && !reprise) reprise = tick;
            last = tick;
            quiet = 0;
        } else if (first) ++quiet;
    }
    void report(const char* stem) const {
        std::cout << stem << " first=" << first / 20.0 << " last=" << last / 20.0
            << " longest silence=" << longest_quiet / 20.0
            << " reprise=" << reprise / 20.0 << " seconds\n";
    }
};
}

int main(int argc, char** argv) {
    if (argc < 3 || argc > 5) return 2;
    try {
        using namespace starfox;
        const auto rom = assets::RomImage::load(argv[1]);
        const auto symbols = assets::SymbolMap::load(argv[2]);
        const bool ex = !symbols.find("PLANETSEQ2_L").empty();
        const bool special = argc == 5;
        const auto a = [&](const char* name) { return symbols.find(name).at(0); };
        auto game = std::make_unique<simulation::GameSimulation>(
            rom, symbols, "LEVEL1_6", std::span<const std::uint8_t>{}, true);
        game->set_god_mode(true);
        auto& map = game->map();
        audio::Spc700Audio spc;
        static_cast<void>(spc.prime_upload_sequence(map.take_apu_port_writes()));
        for (unsigned tick = 0; tick < 30; ++tick) static_cast<void>(spc.render_logic_tick({}));
        game->synchronize_apu_output_ports(spc.output_ports());
        audio::Msu1Pack pack{argc >= 4 ? argv[3] : ""};
        require(argc < 4 || ex || pack.available(), "MSU pack is unavailable");
        audio::Msu1Audio msu{[&](auto track) { return pack.load_track(track); }};
        msu.set_enabled(pack.available() && !ex);
        // Both output stems are measured using the same source register
        // stream. Desktop MSU selection replaces only the music stem.
        for (unsigned tick = 0; tick < 200; ++tick) {
            const auto result = game->tick({});
            static_cast<void>(spc.render_logic_tick(result.audio_port_writes));
            game->synchronize_apu_output_ports(spc.output_ports());
            static_cast<void>(map.take_msu_register_writes());
        }
        const std::array original_bosses{"BOSS11", "BOSS12", "BOSS13", "BOSS14", "BOSS15", "BOSSFINAL"};
        const std::array ex_bosses{"BOSS51", "BOSS52", "BOSS53", "BOSS54", "BOSS55", "BOSSFINAL5"};
        const auto& bosses = special ? ex_bosses : original_bosses;
        map.write_native_word(a("SPECPTR"), 6);
        for (unsigned i = 0; i < 6; ++i) {
            map.write_native_byte(a("SPECBUF") + i, 100);
            map.write_native_word(a("BOSS_SEQ") + 2 * i, a(bosses[i]) - a("ENDSEQBOSS"));
        }
        map.write_native_word(a("BOSS_PTR"), 12);
        if (special) map.write_native_byte(a("WHICHROUTE"), 4);
        game->start_map("FINALMAP_END");
        MusicHistory native, lossless;
        unsigned credits = 0, msu_handoff = 0;
        for (unsigned tick = 1; tick <= 27000; ++tick) {
            if (game->flow_state() == simulation::GameFlowState::gameplay) {
                map.write_native_byte(a("SPECIALOBJTOTAL"), 100);
                map.write_native_byte(a("SPECIALS_DEAD"), 100);
            }
            const auto result = game->tick({});
            static_cast<void>(spc.render_logic_tick(result.audio_port_writes));
            game->synchronize_apu_output_ports(spc.output_ports());
            msu.process_register_writes(map.take_msu_register_writes());
            const auto pcm = msu.render(audio::Spc700Audio::stereo_frames_per_logic_tick,
                audio::Spc700Audio::sample_rate);
            const auto pc = (static_cast<unsigned>(map.read_native_byte(a("MAPBANK"))) << 16)
                | 0x8000U | map.read_native_word(a("MAPPTR"));
            if (!credits && pc >= a("CREDITSMAP") && pc < a("CREDITSMAP") + 0x100U) {
                credits = tick;
                std::cout << "staff roll entered at " << tick / 20.0 << " seconds\n";
            }
            if (credits) {
                if (msu.use_native_music_tail() && !msu_handoff)
                    msu_handoff = tick - credits + 1;
                native.observe(audible(spc.last_music_samples()), tick - credits + 1);
                lossless.observe(audible(msu.use_native_music_tail()
                    ? spc.last_music_samples() : pcm), tick - credits + 1);
                if (tick - credits == 19000) break;
            }
        }
        require(credits, "full ending did not reach the staff roll");
        native.report(special ? "SPC EX orchestra" : "SPC staff roll");
        require(native.first && native.last, "SPC credits music is silent");
        if (!special) {
            require(native.longest_quiet >= 11000 && native.longest_quiet <= 13000,
                "SPC staff roll lost its approximately ten-minute silence");
            require(native.reprise && native.last - native.reprise >= 800,
                "SPC staff roll lost its delayed jingle");
        }
        if (msu.enabled()) {
            lossless.report("MSU staff roll");
            std::cout << "MSU native-tail handoff=" << msu_handoff / 20.0 << " seconds\n";
            require(msu.selected_track() == 49, "MSU staff roll selected the wrong track");
            require(lossless.first, "MSU staff roll is silent");
            require(lossless.reprise == native.reprise && lossless.last == native.last,
                "MSU output lost or mistimed the native delayed jingle");
            require(lossless.longest_quiet >= 11000,
                "MSU output replayed ordinary SPC credits music after the recording ended");
            msu.set_paused(true);
            require(!msu.use_native_music_tail(), "paused MSU playback exposed the native tail");
            msu.set_paused(false);
            require(msu.use_native_music_tail(), "unpaused MSU playback lost the native tail");
            const std::array stop{simulation::MsuRegisterWrite{0x2007U, 0U}};
            msu.process_register_writes(stop);
            require(!msu.use_native_music_tail(), "explicit stop retained the credits tail");
        }
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
