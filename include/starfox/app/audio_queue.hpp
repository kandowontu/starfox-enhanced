#pragma once

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>

#include <cstdint>
#include <limits>
#include <span>

namespace starfox::app {

// SDL reports queued bytes in the stream's INPUT format. This stream always
// accepts interleaved S16 stereo, even when the device resamples to 48 kHz.
// Limit only the playback FIFO: callers must still run every SPC/MSU update
// and deliver its output ports, including when old playback is discarded.
inline bool queue_realtime_audio(SDL_AudioStream* stream,
    std::span<const std::int16_t> samples, std::uint32_t max_source_frames) {
    constexpr auto bytes_per_frame = 2U * sizeof(std::int16_t);
    const auto limit = static_cast<std::uint64_t>(max_source_frames)
        * bytes_per_frame;
    if (samples.size() % 2U != 0U || samples.size_bytes() > limit
        || samples.size_bytes() > std::numeric_limits<int>::max()) {
        return SDL_SetError("Invalid realtime stereo audio packet size");
    }
    const auto queued = SDL_GetAudioStreamQueued(stream);
    if (queued < 0) return false;
    if (static_cast<std::uint64_t>(queued) + samples.size_bytes() > limit) {
        // A render stall/catch-up burst must not leave old sound effects
        // playing behind current input indefinitely. Rejoin current audio.
        if (!SDL_ClearAudioStream(stream)) return false;
    }
    return SDL_PutAudioStreamData(stream, samples.data(),
        static_cast<int>(samples.size_bytes()));
}

} // namespace starfox::app
