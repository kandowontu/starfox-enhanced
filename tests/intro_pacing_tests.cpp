#include "starfox/simulation/game_simulation.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include <iostream>
#include <algorithm>
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    auto rom = starfox::assets::RomImage::load(argv[1]);
    auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    starfox::simulation::GameSimulation game{rom, symbols, "INTROMAP"};
    game.set_timing_mode(starfox::simulation::TimingMode::original_speed);
    starfox::audio::Spc700Audio audio;
    auto pending = game.map().take_apu_port_writes();
    (void)audio.prime_upload_sequence(pending);
    pending.clear();
    game.synchronize_apu_output_ports(audio.output_ports());
    unsigned phases=0, ticks=0, last_music_phase=0;
    const auto audible = [&] {
        const auto samples = audio.last_music_samples();
        return std::any_of(samples.begin(), samples.end(),
            [](auto s) { return s > 32 || s < -32; });
    };
    const auto exit = symbols.find("EXITINTRO").front();
    while (phases < 7200 && game.map().read_native_byte(exit)==0) {
        game.present_frame(); ++phases;
        if (game.logic_tick_ready()) {
            auto result=game.tick({}); ++ticks;
            pending.insert(pending.end(), result.audio_port_writes.begin(), result.audio_port_writes.end());
        }
        if (phases%3==0) {
            (void)audio.render_logic_tick(pending);pending.clear();
            if (audible()) last_music_phase=phases;
            game.synchronize_apu_output_ports(audio.output_ports());
        }
    }
    std::cout << "intro seconds=" << phases/60.0 << " updates=" << ticks << '\n';
    for(unsigned i=0;i<40;++i) {
        (void)audio.render_logic_tick({});
        if (audible()) last_music_phase=phases+(i+1)*3;
    }
    std::cout << "music audible until seconds=" << last_music_phase/60.0 << '\n';
    // This checks approximate choreography, not cycle accuracy. Neither the
    // old 25/30-second visuals nor slowing the audio to match can satisfy it.
    return phases >= 37*60 && phases <= 39*60
        && last_music_phase >= 37*60 && last_music_phase <= 39*60
        && std::abs(static_cast<int>(phases)-static_cast<int>(last_music_phase)) < 60
        ? 0 : 1;
}
