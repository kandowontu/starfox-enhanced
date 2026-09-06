#include "starfox/audio/spc700_audio.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>

using starfox::audio::Spc700Audio;
void require(bool value,const char* message) { if (!value) throw std::runtime_error{message}; }
bool same(std::span<const std::int16_t> a,std::span<const std::int16_t> b) {
    return std::equal(a.begin(),a.end(),b.begin(),b.end());
}
int main(int argc,char** argv) try {
    if (argc!=3) throw std::invalid_argument{"Expected ROM SYMBOLS"};
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    auto game=std::make_unique<starfox::simulation::GameSimulation>(rom,symbols,"LEVEL1_1",
        std::span<const std::uint8_t>{},true);
    const auto boot=game->map().take_apu_port_writes();
    Spc700Audio legacy,whole,split;
    for (auto* audio : {&legacy,&whole,&split}) {
        static_cast<void>(audio->prime_upload_sequence(boot));
        require(audio->driver_loaded(),"Boot driver did not load");
        for (unsigned frame=0;frame<30U;++frame) static_cast<void>(audio->render_logic_tick({}));
    }
    game->synchronize_apu_output_ports(legacy.output_ports());
    bool partial_ack{},stage_upload{};
    std::vector<starfox::simulation::ApuPortWrite> captured_upload;
    for (unsigned frame=0;frame<100U;++frame) {
        auto writes=game->tick({}).audio_port_writes;
        if (frame==10U) writes.push_back({3U,0x35U,40001U});
        if (frame==11U) writes.push_back({3U,0U,40001U});
        if (frame==12U) writes.push_back({3U,2U,40001U});
        if (frame==13U) writes.push_back({3U,1U,40001U});
        std::stable_sort(writes.begin(),writes.end(),[](const auto& a,const auto& b) {
            return a.clock_offset<b.clock_offset;
        });
        const auto upload=std::any_of(writes.begin(),writes.end(),[](const auto& write) {
            return write.port==0U && write.value==0xffU;
        });
        stage_upload|=upload;
        if (upload && captured_upload.empty()) {
            const auto restart=std::find_if(writes.begin(),writes.end(),[](const auto& write) {
                return write.port==0U && write.value==0xffU;
            });
            captured_upload.assign(restart,writes.end());
        }
        // Legacy uploads intentionally play a whole frame after decoding.
        // Align that changed-bank frame through the new timestamped path;
        // compare ordinary running-driver frames with the old implementation.
        if (upload) require(legacy.advance_frame(Spc700Audio::clocks_per_frame,writes),"Whole upload frame incomplete");
        else static_cast<void>(legacy.render_logic_tick(writes));
        require(whole.advance_frame(Spc700Audio::clocks_per_frame,writes),"Whole stream frame incomplete");
        const std::vector<std::int16_t> old_music(split.last_music_samples().begin(),split.last_music_samples().end());
        require(!split.advance_frame(0U),"Zero-clock frame completed");
        if (!frame) {
            bool rejected{};
            try { static_cast<void>(split.render_logic_tick({})); } catch (const std::logic_error&) { rejected=true; }
            require(rejected,"Legacy rendering accepted an incomplete stream frame");
            rejected=false;
            try { static_cast<void>(split.advance_frame(51201U)); } catch (const std::invalid_argument&) { rejected=true; }
            require(rejected,"Out-of-frame audio deadline accepted");
            const std::array bad{starfox::simulation::ApuPortWrite{3U,1U,2U}};
            rejected=false;
            try { static_cast<void>(split.advance_frame(1U,bad)); } catch (const std::invalid_argument&) { rejected=true; }
            require(rejected,"Future audio write accepted before its deadline");
        }
        std::size_t cursor{};
        for (const auto clock : {0U,1U,17U,1000U,5120U,34133U,34134U,40001U,51199U,51200U}) {
            const auto begin=cursor;
            while (cursor<writes.size() && writes[cursor].clock_offset<=clock) ++cursor;
            const auto completed=split.advance_frame(clock,std::span{writes}.subspan(begin,cursor-begin));
            require(completed==(clock==51200U),"Stream frame completed at the wrong time");
            if (!completed) {
                require(same(split.last_music_samples(),old_music),"Partial audio frame replaced published PCM");
                if (frame==10U && clock>40001U && split.output_ports()[3]==0x35U) partial_ack=true;
            }
        }
        require(cursor==writes.size(),"Audio writes exceeded the frame");
        require(same(whole.last_music_samples(),split.last_music_samples())
            && same(whole.last_effect_samples(),split.last_effect_samples())
            && whole.state()==split.state(),"Audio output/state depends on execution chunks");
        require(same(legacy.last_music_samples(),split.last_music_samples())
            && same(legacy.last_effect_samples(),split.last_effect_samples())
            && legacy.state()==split.state(),"Streaming running-driver audio differs from legacy timestamped rendering");
        require(split.last_music_samples().size()==3200U && split.last_effect_samples().size()==3200U,
            "Streaming changed the audio packet duration");
        game->synchronize_apu_output_ports(legacy.output_ports());
    }
    require(stage_upload,"Replay did not exercise a stage-bank upload");
    require(partial_ack,"Sound driver did not acknowledge the effect before the frame completed");
    // Distribute the real stage-bank protocol across frame boundaries and
    // many partial calls, rather than delivering every IPL byte at one time.
    std::size_t cursor{};
    std::uint64_t absolute_clock=17U;
    unsigned upload_frames{};
    while (cursor<captured_upload.size()) {
        std::vector<starfox::simulation::ApuPortWrite> writes;
        const auto frame_start=std::uint64_t(upload_frames)*Spc700Audio::clocks_per_frame;
        while (cursor<captured_upload.size() && absolute_clock<=frame_start+Spc700Audio::clocks_per_frame) {
            auto write=captured_upload[cursor++];
            write.clock_offset=static_cast<std::uint32_t>(absolute_clock-frame_start);
            writes.push_back(write);
            absolute_clock+=17U;
        }
        require(whole.advance_frame(51200U,writes),"Whole upload packet incomplete");
        std::size_t write_cursor{};
        for (std::uint32_t clock=0U;clock<51200U;) {
            clock=std::min(clock+997U,51200U);
            const auto begin=write_cursor;
            while (write_cursor<writes.size() && writes[write_cursor].clock_offset<=clock) ++write_cursor;
            static_cast<void>(split.advance_frame(clock,std::span{writes}.subspan(begin,write_cursor-begin)));
        }
        require(whole.driver_loaded()==split.driver_loaded() && whole.uploaded_bytes()==split.uploaded_bytes()
            && same(whole.last_music_samples(),split.last_music_samples())
            && same(whole.last_effect_samples(),split.last_effect_samples()) && whole.state()==split.state(),
            "Multi-packet upload depends on execution chunks");
        if (!whole.driver_loaded()) {
            // Restart is only 17 SPC clocks into the first packet, before
            // even one output sample is due. Every unfinished packet is silent.
            require(std::all_of(split.last_music_samples().begin(),split.last_music_samples().end(),
                [](auto sample) { return sample==0; }),"Unfinished upload leaked DSP look-ahead samples");
        }
        ++upload_frames;
    }
    require(upload_frames>1U && whole.driver_loaded(),"Stage protocol did not span packets and reload its driver");
    for (unsigned frame=0;frame<10U;++frame) {
        static_cast<void>(whole.advance_frame(51200U));
        static_cast<void>(split.advance_frame(12345U));
        static_cast<void>(split.advance_frame(51200U));
        require(same(whole.last_music_samples(),split.last_music_samples())
            && same(whole.last_effect_samples(),split.last_effect_samples()) && whole.state()==split.state(),
            "Reloaded driver differs after multi-packet upload");
    }
    // An execute packet on a frame boundary belongs to the same instant
    // whether delivered at the old frame's end or the new frame's start.
    auto boundary_upload=captured_upload;
    for (auto& write : boundary_upload) write.clock_offset=51200U;
    static_cast<void>(whole.advance_frame(51200U,boundary_upload));
    static_cast<void>(split.advance_frame(51200U));
    require(same(whole.last_music_samples(),split.last_music_samples())
        && same(whole.last_effect_samples(),split.last_effect_samples()),
        "End-of-frame upload altered audio preceding its timestamp");
    for (auto& write : boundary_upload) write.clock_offset=0U;
    static_cast<void>(split.advance_frame(0U,boundary_upload));
    static_cast<void>(whole.advance_frame(0U));
    require(whole.state()==split.state(),"Upload state depends on boundary ownership");
    static_cast<void>(whole.advance_frame(51200U));
    static_cast<void>(split.advance_frame(51200U));
    require(same(whole.last_music_samples(),split.last_music_samples())
        && same(whole.last_effect_samples(),split.last_effect_samples()),
        "First reloaded audio packet depends on boundary ownership");
    std::cout << "100 audio frames: identical PCM/state across chunks and live partial-frame acknowledgement\n";
    std::cout << upload_frames << " upload packets: silent until execution and stable resumed audio\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
