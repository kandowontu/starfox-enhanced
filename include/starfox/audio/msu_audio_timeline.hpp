#pragma once
#include "starfox/audio/msu1_audio.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include <array>
#include <optional>

namespace starfox::audio {
// Sample-granular MSU selection on the same SPC-clock timeline as native
// audio packets. Finish each packet before advancing beyond its end.
class MsuAudioTimeline {
public:
    MsuAudioTimeline(Msu1Audio& msu,std::uint32_t sample_rate);
    MsuAudioTimeline(const MsuAudioTimeline&)=delete;
    MsuAudioTimeline& operator=(const MsuAudioTimeline&)=delete;
    void advance_to(std::uint64_t spc_clock);
    [[nodiscard]] std::uint8_t access(std::uint64_t spc_clock,std::uint16_t address,
        std::optional<std::uint8_t> value);
    [[nodiscard]] std::span<const std::int16_t> finish_packet(std::uint64_t spc_clock,
        std::span<const std::int16_t> native_music);
private:
    Msu1Audio& msu_;
    std::uint32_t sample_rate_{};
    std::uint64_t packet_start_{},clock_{};
    std::size_t frames_{};
    std::array<std::int16_t,3200> recording_{},selected_{};
    std::array<bool,1600> native_{};
};
} // namespace starfox::audio
