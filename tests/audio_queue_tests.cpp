#include "starfox/app/audio_queue.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << ": " << SDL_GetError() << '\n';
        std::exit(1);
    }
}
} // namespace

int main() {
    require(SDL_Init(SDL_INIT_AUDIO), "audio initialization");
    constexpr SDL_AudioSpec source{SDL_AUDIO_S16, 2, 32'000};
    constexpr SDL_AudioSpec switch_device{SDL_AUDIO_S16, 2, 48'000};
    constexpr std::uint32_t switch_limit = 3'200U; // 100 ms of input
    constexpr int limit_bytes = switch_limit * 4U;
    auto* stream = SDL_CreateAudioStream(&source, &source);
    require(stream != nullptr, "create test stream");
    std::array<std::int16_t, 3'200> packet{}; // one 50 ms source tick
    std::array<std::int16_t, 3'200> output{};
    packet.fill(100);

    // Exactly two normal packets fit. A third catch-up packet evicts the
    // backlog and retains the most recent sound, rather than dropping it.
    for (int tick = 0; tick < 3; ++tick) {
        packet.fill(static_cast<std::int16_t>(100 + tick));
        require(starfox::app::queue_realtime_audio(stream, packet, switch_limit),
            "enqueue catch-up tick");
        require(SDL_GetAudioStreamQueued(stream)
                == static_cast<int>((tick == 1 ? 2U : 1U) * sizeof(packet)),
            "queue limit or normal headroom");
    }
    require(SDL_GetAudioStreamData(stream, output.data(), sizeof(output))
            == sizeof(output), "drain most recent tick");
    require(std::all_of(output.begin(), output.end(),
        [](auto value) { return value == 102; }), "stale audio survived reset");

    // Continuous real-time playback must preserve every packet unmodified.
    for (int tick = 0; tick < 100; ++tick) {
        packet.fill(static_cast<std::int16_t>(tick + 1));
        require(starfox::app::queue_realtime_audio(stream, packet, switch_limit),
            "enqueue regular tick");
        require(SDL_GetAudioStreamData(stream, output.data(), sizeof(output))
            == sizeof(output) && output == packet, "normal audio was lost");
    }
    SDL_DestroyAudioStream(stream);

    // Queue accounting must remain in 32 kHz INPUT units with a 48 kHz
    // Switch device. A blocked/slow consumer cannot grow the FIFO unbounded.
    stream = SDL_CreateAudioStream(&source, &switch_device);
    require(stream != nullptr, "create resampling stream");
    for (int tick = 0; tick < 1'000; ++tick) {
        require(starfox::app::queue_realtime_audio(stream, packet, switch_limit),
            "enqueue resampled backlog");
        const auto queued = SDL_GetAudioStreamQueued(stream);
        require(queued >= 0 && queued <= limit_bytes, "unbounded audio latency");
    }
    require(!starfox::app::queue_realtime_audio(stream,
        std::span<const std::int16_t>{packet}.first(3U), switch_limit),
        "accepted incomplete stereo frame");
    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    std::cout << "Realtime audio queue tests passed.\n";
}
