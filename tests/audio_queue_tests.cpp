#include "starfox/app/audio_queue.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

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
    constexpr std::uint32_t switch_limit = 3'200U; // exercise stricter legacy cap too
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
    // A render tick can be 16 ms late, then catch up on the following tick.
    // Reproduce the previous Switch underrun with 8 ms of headroom, and verify
    // the shared bounded policy survives the same jitter without losing PCM.
    const auto jitter_underruns = [&](unsigned startup_ms, unsigned limit_ms) {
        auto* jitter = SDL_CreateAudioStream(&source, &source);
        require(jitter != nullptr, "create jitter stream");
        std::vector<std::int16_t> prime(startup_ms * 32U * 2U, 100);
        require(SDL_PutAudioStreamData(jitter, prime.data(), static_cast<int>(prime.size()*2U)), "prime jitter stream");
        unsigned underruns = 0;
        for (unsigned tick = 0; tick < 100; ++tick) {
            packet.fill(100);
            require(starfox::app::queue_realtime_audio(jitter, packet, limit_ms * 32U), "queue jitter packet");
            std::vector<std::int16_t> consumed((tick % 2U ? 34U : 66U) * 32U * 2U);
            const auto bytes = static_cast<int>(consumed.size() * 2U);
            const auto read = SDL_GetAudioStreamData(jitter, consumed.data(), bytes);
            require(read >= 0, "consume jitter packet");
            if (read != bytes) ++underruns;
            require(std::all_of(consumed.begin(), consumed.begin()+read/2,
                [](auto sample) { return sample == 100; }), "jitter altered PCM");
            require(SDL_GetAudioStreamQueued(jitter) <= static_cast<int>(limit_ms*32U*4U), "jitter exceeded latency cap");
        }
        SDL_DestroyAudioStream(jitter);
        return underruns;
    };
    require(jitter_underruns(8, 100) > 0, "legacy Switch underrun was not reproduced");
    require(jitter_underruns(starfox::app::realtime_audio_startup_ms,
        starfox::app::realtime_audio_limit_ms) == 0, "bounded headroom did not prevent jitter underruns");
    SDL_Quit();
    std::cout << "Realtime audio queue tests passed.\n";
}
