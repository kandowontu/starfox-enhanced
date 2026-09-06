#include "starfox/audio/native_audio_clock.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/snes_timeline.hpp"
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>

using namespace starfox;
void require(bool value,const char* message) { if (!value) throw std::runtime_error{message}; }
void bus_test() {
    assets::RomImage rom{std::vector<std::uint8_t>(0x8000U,0U)};
    simulation::Wdc65816 cpu{rom};
    bool rejected{};
    const auto dummy=[](std::uint64_t,std::uint8_t,std::optional<std::uint8_t>) { return std::uint8_t{}; };
    try { cpu.set_apu_bus_callback(dummy); } catch (const std::logic_error&) { rejected=true; }
    require(rejected,"APU binding accepted a missing CPU timeline");
    auto timeline=std::make_shared<simulation::SnesCpuTimeline>();
    cpu.set_cpu_timeline(timeline);
    std::vector<std::tuple<std::uint64_t,std::uint8_t,std::optional<std::uint8_t>>> events;
    bool reentry_rejected{};
    cpu.set_apu_bus_callback([&](auto time,auto port,auto value) {
        events.emplace_back(time,port,value);
        if (events.size()==1U) {
            try { cpu.set_apu_bus_callback({}); } catch (const std::logic_error&) { reentry_rejected=true; }
        }
        return std::uint8_t{0x82U};
    });
    const std::array<std::uint8_t,12> code{0xa9U,0x35U,0x8dU,0x43U,0x21U,
        0xadU,0x43U,0x21U,0x8dU,0U,0x12U,0x6bU};
    for (std::size_t i=0;i<code.size();++i) cpu.write8(0x1100U+static_cast<std::uint32_t>(i),code[i]);
    simulation::Wdc65816Registers registers;
    registers.status=0x24U;
    cpu.call_long(0x1100U,registers,100U);
    require(events.size()==2U && std::get<0>(events[0])==46U && std::get<0>(events[1])==72U,
        "APU access did not occur at the native write/read bus sample clocks");
    require(std::get<1>(events[0])==3U && std::get<2>(events[0])==0x35U
        && !std::get<2>(events[1]) && cpu.read8(0x1200U)==0x82U,
        "Native CPU did not exchange real APU port values");
    require(reentry_rejected,"APU callback was replaced during CPU execution");
    cpu.write8(0x2140U,0xffU);
    require(cpu.read8(0x2140U)==0xaaU,"Live APU binding replaced the IPL boot acknowledgement");
    cpu.write8(0x2140U,0xccU);
    require(cpu.read8(0x2140U)==0xccU,"Live APU binding lost IPL token echo");
    cpu.write8(0x2141U,0U); cpu.write8(0x2142U,0U); cpu.write8(0x2143U,0U);
    require(cpu.read8(0x2143U)==0x82U,"Finished IPL upload did not return to the live driver");
    rejected=false;
    try { cpu.set_cpu_timeline({}); } catch (const std::logic_error&) { rejected=true; }
    require(rejected,"CPU timeline detached underneath the APU binding");
    cpu.set_apu_bus_callback({});
    cpu.set_cpu_timeline({});
}

void ratio_test() {
    audio::Spc700Audio a,b;
    unsigned packets_a{},packets_b{};
    audio::NativeAudioClock whole(a,3U,7U,[&](auto,auto,auto) { ++packets_a; },100U);
    audio::NativeAudioClock split(b,3U,7U,[&](auto,auto,auto) { ++packets_b; },100U);
    constexpr std::uint64_t end=3'000'107U;
    whole.advance_to(end);
    for (std::uint64_t clock=100U;clock<end;) {
        clock=std::min(clock+4093U,end);
        split.advance_to(clock);
    }
    require(whole.spc_clock()==(end-100U)*3U/7U && whole.spc_clock()==split.spc_clock()
        && packets_a==packets_b && packets_a==whole.spc_clock()/51200U,
        "Native audio conversion accumulated scheduling-dependent rounding drift");
    bool rejected{};
    try { split.advance_to(end-1U); } catch (const std::invalid_argument&) { rejected=true; }
    require(rejected && split.master_clock()==end,"Backward clock changed native audio state");
    rejected=false;
    try { audio::NativeAudioClock duplicate(b,3U,7U); } catch (const std::logic_error&) { rejected=true; }
    require(rejected,"A new clock accepted an already-partial audio frame");
    audio::Spc700Audio near_limit_audio;
    audio::NativeAudioClock near_limit(near_limit_audio,3U,7U,{},std::numeric_limits<std::uint64_t>::max()-10U);
    near_limit.advance_to(std::numeric_limits<std::uint64_t>::max());
    require(near_limit.spc_clock()==4U,"Large master-clock origin overflowed fractional conversion");
}

struct Result {
    std::uint64_t master{},spc{},pcm_hash{14695981039346656037ULL};
    unsigned packets{},reads{},writes{};
    std::uint16_t gameframe{},map_pointer{};
    audio::Spc700Audio::State state;
    friend bool operator==(const Result&,const Result&)=default;
};
Result run(const assets::RomImage& rom,const assets::SymbolMap& symbols,const char* map,std::uint64_t quantum) {
    auto game=std::make_unique<simulation::GameSimulation>(rom,symbols,map,
        std::span<const std::uint8_t>{},true);
    audio::Spc700Audio sound;
    static_cast<void>(sound.prime_upload_sequence(game->map().take_apu_port_writes()));
    for (unsigned i=0;i<30U;++i) static_cast<void>(sound.render_logic_tick({}));
    game->synchronize_apu_output_ports(sound.output_ports());
    Result result;
    // Pinned ares NTSC CPU oscillator is 236250000/11 Hz; its SMP
    // executes 32040*32 clocks/second. Keep the ratio explicit in the test.
    audio::NativeAudioClock clock(sound,11'278'080U,236'250'000U,[&](auto,auto music,auto effects) {
        ++result.packets;
        for (const auto samples : {music,effects}) for (const auto sample : samples) {
            result.pcm_hash^=static_cast<std::uint16_t>(sample);
            result.pcm_hash*=1099511628211ULL;
        }
    });
    for (unsigned frame=0;frame<12U;++frame) {
        game->begin_native_gameplay_update({});
        if (!frame) game->map().set_apu_bus_callback([&](auto time,auto port,auto value) {
            if (value) ++result.writes; else ++result.reads;
            return clock.access(time,port,value);
        });
        unsigned slices{};
        while (true) {
            const auto completed=game->advance_native_transfer(quantum);
            clock.advance_to(game->native_transfer_clock());
            if (completed) break;
            if (++slices>100000U) throw std::runtime_error{"Native audio gameplay exceeded its execution budget"};
        }
        require(!game->native_gameplay_exit_pending(),"Native audio replay unexpectedly left gameplay");
    }
    game->map().set_apu_bus_callback({});
    result.master=game->native_transfer_clock(); result.spc=clock.spc_clock();
    result.state=sound.state();
    result.gameframe=game->map().read_native_word(symbols.find("GAMEFRAME").at(0));
    result.map_pointer=game->map().read_native_word(symbols.find("MAPPTR").at(0));
    require(result.reads && result.writes && result.packets && sound.driver_loaded()
        && game->map().apu_upload_generation()>=2U,"Native audio did not exercise live traffic and a stage upload");
    return result;
}

int main(int argc,char** argv) try {
    bus_test(); ratio_test();
    if (argc!=4) throw std::invalid_argument{"Expected ROM SYMBOLS MAP"};
    const auto rom=assets::RomImage::load(argv[1]);
    const auto symbols=assets::SymbolMap::load(argv[2]);
    const auto small=run(rom,symbols,argv[3],4096U);
    const auto large=run(rom,symbols,argv[3],100'000'000U);
    require(small==large,"Native CPU/audio clocks, traffic, PCM or state depend on execution quantum");
    std::cout << "12 MAIN updates: " << small.master << " master clocks, " << small.spc
        << " SPC clocks, " << small.reads << " reads, " << small.writes << " writes, "
        << small.packets << " audio packets; identical across execution quanta\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
