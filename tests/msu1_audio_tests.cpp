#include "starfox/audio/msu1_audio.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    require(argc == 2, "Expected synthetic FLAC fixture path");
    std::ifstream file(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    require(!bytes.empty(), "Missing fixture");
    starfox::audio::Msu1Audio audio([&](std::uint16_t track) {
        if (track == 50U) return std::vector<std::uint8_t>{};
        if (track == 51U) return std::vector<std::uint8_t>{1, 2, 3};
        return bytes;
    });
    audio.set_enabled(true);
    auto control = [&](std::uint8_t value, std::uint8_t track = 1) {
        const std::array writes{
            starfox::simulation::MsuRegisterWrite{0x2004, track, 0},
            starfox::simulation::MsuRegisterWrite{0x2005, 0, 0},
            starfox::simulation::MsuRegisterWrite{0x2007, value, 0}};
        audio.process_register_writes(writes);
    };
    auto silent = [](auto samples) {
        return std::all_of(samples.begin(), samples.end(), [](auto v) { return v == 0; });
    };
    require(silent(audio.render(10, 32000)), "Audio started before play command");
    control(1);
    require(!silent(audio.render(319, 32000)), "Play command did not start audio");
    require(audio.playing(), "One-shot ended early");
    audio.set_paused(true);
    require(silent(audio.render(100, 32000)) && audio.playing(), "Pause advanced playback");
    audio.set_paused(false);
    (void)audio.render(1, 32000);
    require(!audio.playing(), "One-shot remained active after its final sample");
    require(silent(audio.render(10, 32000)), "One-shot replayed after completion");
    control(3);
    require(!silent(audio.render(640, 32000)) && audio.playing(), "Loop ended at EOF");
    control(1, 38);
    require(audio.selected_track() == 38 && audio.playing(), "Death did not replace looping track");
    require(!silent(audio.render(320, 32000)) && !audio.playing(), "Death cue did not play once");
    control(0);
    require(!audio.playing() && silent(audio.render(10, 32000)), "Stop failed");
    for (const auto track : {50U, 51U}) {
        control(3);
        control(1, track);
        require(!audio.playing() && silent(audio.render(100, 32000)),
            "Failed replacement retained previous samples");
    }
    control(3, 2);
    require(!silent(audio.render(960, 32000)) && audio.playing(),
        "Short replacement did not loop from zero");
    control(1, 49);
    (void)audio.render(319, 32000);
    require(!audio.resume_native_music(), "Staff roll handed off before EOF");
    audio.set_paused(true);
    (void)audio.render(100, 32000);
    require(!audio.resume_native_music(), "Paused staff roll handed off");
    audio.set_paused(false);
    (void)audio.render(1, 32000);
    require(audio.resume_native_music(), "Staff EOF did not release native music");
    control(0, 49);
    require(!audio.resume_native_music(), "Explicit stop retained staff handoff");
    control(1, 38);
    (void)audio.render(640, 32000);
    require(!audio.resume_native_music(), "Death EOF incorrectly resumed native music");
}
