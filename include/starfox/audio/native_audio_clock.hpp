#pragma once

#include "starfox/audio/spc700_audio.hpp"
#include <functional>
#include <optional>

namespace starfox::audio {

// Converts native master-clock timestamps to sound-processor clocks. The
// ratio is explicit: oscillator selection and output sample rate belong to
// the host. The caller services elapsed time at chunk boundaries as well as
// through port accesses, so audio continues even when the CPU is not polling.
class NativeAudioClock {
public:
    using PacketCallback=std::function<void(std::uint64_t,
        std::span<const std::int16_t>,std::span<const std::int16_t>)>;
    NativeAudioClock(Spc700Audio& audio,std::uint32_t spc_numerator,
        std::uint32_t master_denominator,PacketCallback packet={},std::uint64_t master_origin=0U);
    NativeAudioClock(const NativeAudioClock&)=delete;
    NativeAudioClock& operator=(const NativeAudioClock&)=delete;
    void advance_to(std::uint64_t master_clock);
    [[nodiscard]] std::uint8_t access(std::uint64_t master_clock,std::uint8_t port,
        std::optional<std::uint8_t> value);
    [[nodiscard]] std::uint64_t spc_clock() const noexcept { return spc_clock_; }
    [[nodiscard]] std::uint64_t master_clock() const noexcept { return master_clock_; }
private:
    Spc700Audio& audio_;
    PacketCallback packet_;
    std::uint32_t numerator_{},denominator_{};
    std::uint64_t master_clock_{},spc_clock_{},remainder_{};
    std::uint32_t frame_clock_{};
    bool advancing_{};
};

} // namespace starfox::audio
