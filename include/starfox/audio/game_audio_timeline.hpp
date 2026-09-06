#pragma once
#include "starfox/audio/native_audio_clock.hpp"
#include "starfox/audio/msu_audio_timeline.hpp"

namespace starfox::audio {

// One sound lifetime shared by native CPU scenes and host-owned frontends.
// The current cartridges use the NTSC CPU/APU oscillator profile.
class GameAudioTimeline {
public:
    static constexpr std::uint32_t sample_rate=32040U;
    using PacketCallback=NativeAudioClock::PacketCallback;
    GameAudioTimeline(Spc700Audio& sound,Msu1Audio& msu,PacketCallback packet);
    void rebase_master_clock(std::uint64_t origin) { clock_.rebase_master_clock(origin); }
    void advance_to(std::uint64_t master_clock);
    [[nodiscard]] std::uint8_t access_apu(std::uint64_t master_clock,std::uint8_t port,
        std::optional<std::uint8_t> value);
    [[nodiscard]] std::uint8_t access_msu(std::uint64_t master_clock,std::uint16_t address,
        std::optional<std::uint8_t> value);
    // Legacy frontend offsets are nominal 32 kHz SPC clocks over 50 ms.
    // Convert their timing to the continuing oscillator; keep partial packets.
    void advance_host_tick(std::span<const simulation::ApuPortWrite> writes,
        std::span<const simulation::MsuRegisterWrite> msu_writes);
    [[nodiscard]] std::uint64_t spc_clock() const noexcept { return clock_.spc_clock(); }
private:
    PacketCallback packet_;
    MsuAudioTimeline music_;
    NativeAudioClock clock_;
};
} // namespace starfox::audio
