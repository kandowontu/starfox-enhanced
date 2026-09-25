#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/wdc65816.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) try {
    if(argc!=3) throw std::runtime_error("usage: starfox_comms_timing_check ROM SYMBOLS");
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    const auto address=[&](const char* name){return symbols.find(name).at(0);};
    unsigned mismatches=0;
    for(const bool requested:{false,true}) {
        starfox::simulation::GameSimulation game{rom,symbols,"LEVEL1_1"};
        auto& map=game.map();
        // These fixtures use the retail and EX source layouts respectively.
        const int meter_y=address("BITMAP1")==0x4000U ? 156 : 177;
        // Invoke the real message sender, then its source animator. Do not
        // synthesize opening/closing counters or host presentation snapshots.
        starfox::simulation::Wdc65816Registers send;send.a=36;send.status=0x24;
        map.call_native_routine(address("SEND_MESSAGE_L"),send,2'000'000,true);
        map.write_native_byte(address("FRIENDS_METER"),requested?0xa8:0);
        const auto pixel=[&](int x,int y) {
            const unsigned tile=unsigned(x/8)*24+unsigned(y/8);
            const auto base=std::uint16_t(address("BITMAP1")+tile*32+unsigned(y%8)*2);
            unsigned value=0;
            for(unsigned p=0;p<4;++p) {
                const auto offset=std::uint16_t(base+(p/2)*16+p%2);
                if(map.read_native_byte(0x700000U+offset)&(0x80U>>(x%8))) value|=1U<<p;
            }
            return value;
        };
        unsigned drawn=0,closed=0;
        for(unsigned frame=0;frame<300;++frame) {
            map.write_native_byte(address("M_CLRBITMAPS"),1);
            map.begin_superfx_bitmap_frame();
            starfox::simulation::Wdc65816Registers animate;animate.status=0x24;
            map.call_native_routine(address("FRIENDS_MESSAGES_L"),animate,5'000'000,true);
            // The native meter's four corner pixels are unambiguous: text is
            // above it and the portrait lies to its left. Decode source planar
            // RAM, not the host's own meter-visible flag, as the oracle.
            const bool native=pixel(82,meter_y)==14 && pixel(125,meter_y)==14
                && pixel(82,meter_y+11)==14 && pixel(125,meter_y+11)==14;
            const auto host=game.dialogue_state();
            if(frame==4 || frame==54) {
                const auto restored=game.restored_state(game.save_state());
                const auto saved=restored->dialogue_state();
                if(saved.text_visible!=host.text_visible || saved.meter_visible!=host.meter_visible)
                    throw std::runtime_error("Save/load changed the submitted communication phase");
            }
            drawn+=native;closed+=!host.active;
            if(native!=host.meter_visible) {
                ++mismatches;
                std::cout<<"requested="<<requested<<" frame="<<frame<<" native="<<native
                    <<" host="<<host.meter_visible<<" count="<<unsigned(map.read_native_byte(address("MSG_COUNT1")))
                    <<" animation="<<unsigned(map.read_native_byte(address("MSG_COUNT2")))
                    <<" portrait="<<unsigned(host.portrait_frame)<<'\n';
            }
        }
        if((requested && !drawn) || (!requested && drawn) || !closed)
            throw std::runtime_error("Source fixture did not cover requested meter and closed-message states");
        std::cout<<"requested="<<requested<<" source-meter-frames="<<drawn<<" closed-frames="<<closed<<'\n';
    }
    if(!symbols.find("FRIENDS_MESSAGES2_L").empty()) {
        starfox::simulation::GameSimulation game{rom,symbols,"LEVEL1_1"};
        auto& map=game.map();
        starfox::simulation::Wdc65816Registers send;send.a=1;send.status=0x24;
        map.call_native_routine(address("SEND_MESSAGEX2_L"),send,2'000'000,true);
        unsigned speaking_frames=0;
        for(unsigned frame=0;frame<300;++frame) {
            const bool speaking=map.read_native_byte(address("MSG_COUNT12"))!=0
                && map.read_native_byte(address("MSG_COUNT22"))>=5;
            starfox::simulation::Wdc65816Registers animate;animate.status=0x24;
            map.call_native_routine(address("FRIENDS_MESSAGES2_L"),animate,5'000'000,true);
            const auto host=game.dialogue_state();
            if(host.text_visible!=speaking || host.meter_visible)
                throw std::runtime_error("EX alternate channel opening/closing phase mismatch");
            speaking_frames+=speaking;
        }
        if(!speaking_frames) throw std::runtime_error("EX alternate fixture never spoke");
        std::cout<<"EX alternate speaking frames="<<speaking_frames<<'\n';
    }
    if(mismatches) throw std::runtime_error(std::to_string(mismatches)+" source/host meter timing mismatches");
    std::cout<<"Source bitmap and host comms meter timing agree\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
