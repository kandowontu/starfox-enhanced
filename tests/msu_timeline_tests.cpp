#include "starfox/audio/msu_audio_timeline.hpp"
#include "starfox/audio/native_audio_clock.hpp"
#include "starfox/simulation/snes_timeline.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

using namespace starfox;
void require(bool value,const char* message) { if (!value) throw std::runtime_error{message}; }
std::vector<std::int16_t> credits(const audio::Msu1Audio::TrackLoader& loader,std::uint32_t quantum) {
    audio::Msu1Audio msu(loader);
    msu.set_enabled(true);
    audio::MsuAudioTimeline timeline(msu,32000U);
    for (const auto write : std::array{simulation::MsuRegisterWrite{0x2004U,49U,0U},
            simulation::MsuRegisterWrite{0x2005U,0U,0U},simulation::MsuRegisterWrite{0x2006U,255U,0U},
            simulation::MsuRegisterWrite{0x2007U,1U,0U}})
        static_cast<void>(timeline.access(320U,write.address,write.value));
    require(timeline.access(320U,0x2000U,{})==0x12U,"MSU did not report playing revision 2");
    for (std::uint64_t clock=320U;clock<51200U;) {
        clock=std::min<std::uint64_t>(clock+quantum,51200U);
        timeline.advance_to(clock);
    }
    require(msu.status()==2U && msu.use_native_music_tail(),"Credits EOF did not expose the continuing SPC tail");
    std::array<std::int16_t,3200> native;
    for (std::size_t i=0;i<native.size();++i) native[i]=static_cast<std::int16_t>(20000+i);
    const auto mixed=timeline.finish_packet(51200U,native);
    std::vector<std::int16_t> result(mixed.begin(),mixed.end());
    require(std::all_of(result.begin(),result.begin()+20,[](auto v) { return v==0; }),"MSU played before its command timestamp");
    require(std::any_of(result.begin()+20,result.begin()+660,[](auto v) { return v!=0; }),"Credits recording prefix was silent");
    require(std::equal(result.begin()+660,result.end(),native.begin()+660),"SPC tail began at the wrong sample");
    require(!std::equal(result.begin()+20,result.begin()+660,native.begin()+20),"SPC replaced the final MSU recording prefix");
    // Explicit stop cancels the hidden-jingle handoff at its own timestamp.
    static_cast<void>(timeline.access(51300U,0x2007U,0U));
    const auto stopped=timeline.finish_packet(102400U,native);
    require(std::equal(stopped.begin(),stopped.begin()+6,native.begin())
        && std::all_of(stopped.begin()+6,stopped.end(),[](auto v) { return v==0; }),
        "Explicit stop failed to cancel the native tail at its sample boundary");
    return result;
}
void controls(const audio::Msu1Audio::TrackLoader& loader) {
    audio::Msu1Audio msu(loader);
    msu.set_enabled(true);
    audio::MsuAudioTimeline timeline(msu,32000U);
    static_cast<void>(timeline.access(0U,0x2004U,2U));
    static_cast<void>(timeline.access(0U,0x2005U,0U));
    static_cast<void>(timeline.access(0U,0x2007U,3U));
    require(msu.status()==0x32U,"MSU repeat/play bits are incorrect");
    const std::array<std::int16_t,3200> native{};
    const auto first=timeline.finish_packet(51200U,native);
    // Track 2's normal loop point lies beyond this intentionally short
    // replacement. It must loop from zero instead of hanging at EOF.
    for (std::size_t i=640U;i<first.size();++i)
        require(first[i]==first[i%640U],"Short replacement recording did not loop safely");
    static_cast<void>(timeline.access(51840U,0x2006U,0U));
    static_cast<void>(timeline.access(52800U,0x2006U,255U));
    const auto volume=timeline.finish_packet(102400U,native);
    require(std::all_of(volume.begin()+40,volume.begin()+100,[](auto v) { return v==0; }),"Volume change missed its timestamp");
    require(std::any_of(volume.begin()+100,volume.end(),[](auto v) { return v!=0; }),"Volume restore did not resume audio");
    msu.set_paused(true);
    const auto paused=timeline.finish_packet(153600U,native);
    require(std::all_of(paused.begin(),paused.end(),[](auto v) { return v==0; }),"Host pause did not silence MSU output");
    msu.set_paused(false);
    static_cast<void>(timeline.access(153600U,0x2004U,99U));
    static_cast<void>(timeline.access(153600U,0x2005U,0U));
    require(msu.status() & 8U,"Missing track was not reported after selection");
    static_cast<void>(timeline.access(153600U,0x2007U,1U));
    require(!msu.playing(),"Missing track continued playing the previous recording");
    const auto missing=timeline.finish_packet(204800U,native);
    require(std::all_of(missing.begin(),missing.end(),[](auto v) { return v==0; }),"Missing track leaked old audio");
    msu.set_enabled(false);
    std::array<std::int16_t,3200> music;
    music.fill(1234);
    const auto disabled=timeline.finish_packet(256000U,music);
    require(std::equal(disabled.begin(),disabled.end(),music.begin()),"Disabled MSU did not retain native music");
}
void legacy_tail(const audio::Msu1Audio::TrackLoader& loader) {
    audio::Msu1Audio msu(loader);
    msu.set_enabled(true);
    const std::array writes{simulation::MsuRegisterWrite{0x2004U,49U,0U},
        simulation::MsuRegisterWrite{0x2005U,0U,0U},simulation::MsuRegisterWrite{0x2007U,1U,0U}};
    msu.process_register_writes(writes);
    const auto rendered=msu.render(1600U,32000U);
    const std::vector<std::int16_t> recording(rendered.begin(),rendered.begin()+640U);
    std::array<std::int16_t,3200> native;
    native.fill(20000);
    const auto selected=msu.select_music(native);
    require(std::equal(recording.begin(),recording.end(),selected.begin())
        && std::equal(selected.begin()+640U,selected.end(),native.begin()+640U),
        "Desktop music selection discarded the last MSU recording samples");
}
void bus(const audio::Msu1Audio::TrackLoader& loader) {
    assets::RomImage rom{std::vector<std::uint8_t>(0x8000U,0U)};
    simulation::Wdc65816 cpu(rom);
    audio::Spc700Audio spc;
    audio::Msu1Audio msu(loader);
    msu.set_enabled(true);
    audio::MsuAudioTimeline music(msu,32000U);
    audio::NativeAudioClock clock(spc,1U,4U,[&](auto end,auto native,auto) { static_cast<void>(music.finish_packet(end,native)); });
    const auto callback=[&](auto time,auto address,auto value) {
        clock.advance_to(time);
        return music.access(clock.spc_clock(),address,value);
    };
    bool rejected{};
    try { cpu.set_msu_bus_callback(callback); } catch (const std::logic_error&) { rejected=true; }
    require(rejected,"MSU binding accepted a missing native timeline");
    auto raster=std::make_shared<simulation::SnesCpuTimeline>();
    cpu.set_cpu_timeline(raster);
    std::vector<std::uint64_t> accesses;
    cpu.set_msu_bus_callback([&](auto time,auto address,auto value) {
        accesses.push_back(time);
        return callback(time,address,value);
    });
    const std::array<std::uint8_t,22> program{0xa9U,1U,0x8dU,4U,0x20U,0xa9U,0U,0x8dU,5U,0x20U,
        0xa9U,3U,0x8dU,7U,0x20U,0xadU,0U,0x20U,0x8dU,0U,0x12U,0x6bU};
    for (std::size_t i=0;i<program.size();++i) cpu.write8(0x1100U+static_cast<std::uint32_t>(i),program[i]);
    simulation::Wdc65816Registers registers;
    registers.status=0x24U;
    cpu.call_long(0x1100U,registers,100U);
    require(accesses==std::vector<std::uint64_t>{46U,92U,138U,164U}
        && cpu.read8(0x1200U)==0x32U,"Native MSU bus timing or returned status is incorrect");
    rejected=false;
    try { cpu.set_cpu_timeline({}); } catch (const std::logic_error&) { rejected=true; }
    require(rejected,"Native timeline detached underneath MSU playback");
    cpu.set_msu_bus_callback({}); cpu.set_cpu_timeline({});
}
int main(int argc,char** argv) try {
    if (argc!=2) throw std::invalid_argument{"Expected synthetic FLAC fixture"};
    std::ifstream file(argv[1],std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
    require(!bytes.empty(),"FLAC fixture is missing");
    const audio::Msu1Audio::TrackLoader loader=[&](std::uint16_t track) {
        return track<53U ? bytes : std::vector<std::uint8_t>{};
    };
    require(credits(loader,51200U)==credits(loader,97U),"MSU PCM selection depends on execution chunks");
    controls(loader); legacy_tail(loader); bus(loader);
    std::cout << "MSU timestamps, credits-tail boundary, repeat, volume, missing track, pause and native bus checks pass\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
