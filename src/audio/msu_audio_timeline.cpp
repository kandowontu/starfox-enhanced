#include "starfox/audio/msu_audio_timeline.hpp"
#include <algorithm>
#include <stdexcept>

namespace starfox::audio {
MsuAudioTimeline::MsuAudioTimeline(Msu1Audio& msu,std::uint32_t sample_rate)
    : msu_(msu),sample_rate_(sample_rate) {
    if (!sample_rate) throw std::invalid_argument{"MSU timeline requires an output sample rate"};
}

void MsuAudioTimeline::advance_to(std::uint64_t spc_clock) {
    if (spc_clock<clock_ || spc_clock-packet_start_>Spc700Audio::clocks_per_frame)
        throw std::invalid_argument{"MSU timestamp is outside the pending audio packet"};
    const auto target=static_cast<std::size_t>((spc_clock-packet_start_)/32U);
    if (target>frames_) {
        const auto count=target-frames_;
        const auto recording=msu_.render(count,sample_rate_);
        std::copy(recording.begin(),recording.end(),recording_.begin()+frames_*2U);
        for (std::size_t i=0;i<count;++i)
            native_[frames_+i]=!msu_.enabled() || i>=msu_.native_tail_start_frame();
        frames_=target;
    }
    clock_=spc_clock;
}

std::uint8_t MsuAudioTimeline::access(std::uint64_t spc_clock,std::uint16_t address,
    std::optional<std::uint8_t> value) {
    if (address<0x2000U || address>0x2007U) throw std::invalid_argument{"Invalid MSU register"};
    advance_to(spc_clock);
    if (value) {
        const std::array write{simulation::MsuRegisterWrite{address,*value,0U}};
        msu_.process_register_writes(write);
        return 0U;
    }
    if (address==0x2000U) return msu_.status();
    constexpr std::array<std::uint8_t,6> signature{'S','-','M','S','U','1'};
    return address>=0x2002U ? signature[address-0x2002U] : 0U;
}

std::span<const std::int16_t> MsuAudioTimeline::finish_packet(std::uint64_t spc_clock,
    std::span<const std::int16_t> native_music) {
    if (spc_clock<packet_start_ || spc_clock-packet_start_!=Spc700Audio::clocks_per_frame
            || native_music.size()!=selected_.size())
        throw std::invalid_argument{"MSU packet boundary or native music length is invalid"};
    advance_to(spc_clock);
    for (std::size_t i=0;i<selected_.size();++i)
        selected_[i]=native_[i/2U] ? native_music[i] : recording_[i];
    packet_start_=spc_clock;
    frames_=0U;
    return selected_;
}
} // namespace starfox::audio
