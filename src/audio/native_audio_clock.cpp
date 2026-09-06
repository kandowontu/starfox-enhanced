#include "starfox/audio/native_audio_clock.hpp"
#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace starfox::audio {

NativeAudioClock::NativeAudioClock(Spc700Audio& audio,std::uint32_t spc_numerator,
    std::uint32_t master_denominator,PacketCallback packet,std::uint64_t master_origin)
    : audio_(audio),packet_(std::move(packet)),master_clock_(master_origin) {
    if (!spc_numerator || spc_numerator>master_denominator)
        throw std::invalid_argument{"Native audio requires a positive clock ratio no greater than one"};
    if (audio.frame_in_progress())
        throw std::logic_error{"Native audio clock requires an audio frame boundary"};
    const auto divisor=std::gcd(spc_numerator,master_denominator);
    numerator_=spc_numerator/divisor;
    denominator_=master_denominator/divisor;
}

void NativeAudioClock::advance_to(std::uint64_t master_clock) {
    if (advancing_) throw std::logic_error{"Native audio callbacks cannot reenter clock advancement"};
    if (master_clock<master_clock_) throw std::invalid_argument{"Native audio clock moved backward"};
    const auto delta=master_clock-master_clock_;
    // Splitting quotient/remainder avoids multiplying an ever-growing
    // absolute timestamp by the oscillator ratio. Both factors in the
    // fractional product fit in uint32_t, and numerator <= denominator.
    const auto fraction=(delta%denominator_)*numerator_+remainder_;
    auto clocks=(delta/denominator_)*numerator_+fraction/denominator_;
    if (clocks>std::numeric_limits<std::uint64_t>::max()-spc_clock_)
        throw std::overflow_error{"Native audio elapsed clock overflow"};
    struct Guard { bool& value; ~Guard() { value=false; } } guard{advancing_};
    advancing_=true;
    remainder_=fraction%denominator_;
    master_clock_=master_clock;
    while (clocks) {
        const auto count=static_cast<std::uint32_t>(std::min<std::uint64_t>(clocks,
            Spc700Audio::clocks_per_frame-frame_clock_));
        frame_clock_+=count;
        clocks-=count;
        spc_clock_+=count;
        if (audio_.advance_frame(frame_clock_)) {
            frame_clock_=0U;
            if (packet_) packet_(spc_clock_,audio_.last_music_samples(),audio_.last_effect_samples());
        }
    }
}

std::uint8_t NativeAudioClock::access(std::uint64_t master_clock,std::uint8_t port,
    std::optional<std::uint8_t> value) {
    if (port>3U) throw std::invalid_argument{"Invalid native APU port"};
    advance_to(master_clock);
    if (value) {
        const std::array write{simulation::ApuPortWrite{port,*value,frame_clock_}};
        static_cast<void>(audio_.advance_frame(frame_clock_,write));
        return 0U;
    }
    return audio_.output_ports()[port];
}

} // namespace starfox::audio
