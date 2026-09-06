#include "starfox/audio/game_audio_timeline.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace starfox::audio {
GameAudioTimeline::GameAudioTimeline(Spc700Audio& sound,Msu1Audio& msu,PacketCallback packet)
    : packet_(std::move(packet)),music_(msu,sample_rate),
      clock_(sound,11'278'080U,236'250'000U,[this](auto end,auto music,auto effects) {
          music=music_.finish_packet(end,music);
          if (packet_) packet_(end,music,effects);
      }) {}

void GameAudioTimeline::advance_to(std::uint64_t master_clock) {
    clock_.advance_to(master_clock);
    music_.advance_to(clock_.spc_clock());
}
std::uint8_t GameAudioTimeline::access_apu(std::uint64_t master_clock,std::uint8_t port,
    std::optional<std::uint8_t> value) {
    const auto result=clock_.access(master_clock,port,value);
    music_.advance_to(clock_.spc_clock());
    return result;
}
std::uint8_t GameAudioTimeline::access_msu(std::uint64_t master_clock,std::uint16_t address,
    std::optional<std::uint8_t> value) {
    if (address<0x2000U || address>0x2007U) throw std::invalid_argument{"Invalid native MSU address"};
    clock_.advance_to(master_clock);
    return music_.access(clock_.spc_clock(),address,value);
}
void GameAudioTimeline::advance_host_tick(std::span<const simulation::ApuPortWrite> writes,
    std::span<const simulation::MsuRegisterWrite> msu_writes) {
    constexpr auto duration=sample_rate*32U/20U;
    const auto start=clock_.spc_clock();
    if (start>std::numeric_limits<std::uint64_t>::max()-duration)
        throw std::overflow_error{"Frontend sound clock overflow"};
    struct Event { std::uint32_t time; std::uint16_t address; std::uint8_t value; bool msu; };
    std::vector<Event> events;
    events.reserve(writes.size()+msu_writes.size());
    const auto convert=[](std::uint32_t offset) {
        return std::min(offset,Spc700Audio::clocks_per_frame)*801U/800U;
    };
    std::uint32_t previous{};
    for (const auto& write : writes) {
        if (write.port>3U) throw std::invalid_argument{"Invalid frontend APU port"};
        previous=std::max(previous,convert(write.clock_offset));
        events.push_back({previous,write.port,write.value,false});
    }
    previous=0U;
    for (const auto& write : msu_writes) {
        if (write.address<0x2000U || write.address>0x2007U)
            throw std::invalid_argument{"Invalid frontend MSU address"};
        previous=std::max(previous,convert(write.clock_offset));
        events.push_back({previous,write.address,write.value,true});
    }
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b) { return a.time<b.time; });
    for (const auto& event : events) {
        clock_.advance_spc_to(start+event.time);
        if (event.msu) static_cast<void>(music_.access(clock_.spc_clock(),event.address,event.value));
        else {
            static_cast<void>(clock_.access_spc(clock_.spc_clock(),static_cast<std::uint8_t>(event.address),event.value));
            music_.advance_to(clock_.spc_clock());
        }
    }
    clock_.advance_spc_to(start+duration);
    music_.advance_to(clock_.spc_clock());
}
} // namespace starfox::audio
