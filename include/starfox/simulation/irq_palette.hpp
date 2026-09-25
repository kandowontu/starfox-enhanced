#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace starfox::simulation {
struct IrqPaletteCycle {
    std::uint32_t enabled{},frame{};
    std::array<std::uint32_t,8> tables{};
    unsigned hold{},destination{};
};
struct TitlePaletteCycle {
    std::uint32_t enabled{},frame{};
    std::array<std::uint32_t,9> foreground{},background{};
};
template<class Find> TitlePaletteCycle ex_title_palette_cycle(Find find) {
    TitlePaletteCycle c;c.enabled=find("TITLFADE");c.frame=find("TITLFRAME");
    for(unsigned i=0;i<9;++i) {
        c.foreground[i]=find("TITL"+std::to_string(i+1));
        c.background[i]=find("TITL0"+std::to_string(i+1));
    }
    return c;
}
template<class ReadByte,class WriteByte,class Upload>
void apply_ex_title_palette_cycle(const TitlePaletteCycle& c,bool color_trip,
    ReadByte read,WriteByte write,Upload upload) {
    if(color_trip || !c.enabled || !c.frame || !read(c.enabled))return;
    for(unsigned i=0;i<9;++i)if(!c.foreground[i] || !c.background[i])return;
    auto frame=read(c.frame);unsigned stage=0;
    while(stage<8 && !(std::uint8_t(frame-(stage+1)*2)&128))++stage;
    // CHECKTITL holds its ninth pair through frame 62, then starts at pair 1.
    if(stage==8 && !(std::uint8_t(frame-63)&128)){stage=0;frame=0;}
    upload(c.foreground[stage],16U);
    upload(c.background[stage],0U);
    write(c.frame,std::uint8_t(frame+1));
}
template<class Find> auto ex_irq_palette_cycles(Find find) {
    std::array<IrqPaletteCycle,3> result{};
    unsigned i=0;
    for(const auto* name:{"WATER","SECTORK","SUN"}) {
        auto& c=result[i];const std::string prefix=name;
        c.enabled=find(prefix+"FADE");c.frame=find(prefix+"FRAME");
        c.hold=i==1?3:2;c.destination=i==2?48:64;
        for(unsigned n=0;n<8;++n)c.tables[n]=find(prefix+std::to_string(n+1));
        ++i;
    }
    return result;
}
// CHECKWATER, CHECKSECTORK, CHECKSUN execute in this order after palette DMA.
// Counters and table contents remain source RAM, including across save/load.
template<class ReadByte,class WriteByte,class Upload>
void apply_ex_irq_palette_cycles(const std::array<IrqPaletteCycle,3>& cycles,
    bool color_trip,ReadByte read,WriteByte write,Upload upload) {
    if(color_trip)return;
    for(const auto& c:cycles) {
        if(!c.enabled || !c.frame || !read(c.enabled))continue;
        bool valid=true;for(auto table:c.tables)valid&=table!=0;
        if(!valid)continue;
        auto frame=read(c.frame);unsigned stage=0;
        // Source uses CMP/BMI, including byte wrapping, rather than signed <.
        while(stage<8 && !(std::uint8_t(frame-(stage+1)*c.hold)&128))++stage;
        if(stage==8){stage=0;frame=0;}
        upload(c.tables[stage],c.destination);
        write(c.frame,std::uint8_t(frame+1));
    }
}
// IRQ.ASM IRQRAND: CLC followed by four byte SBCs, preserving each borrow.
// RAND lives in source RAM and is already part of the game's save state.
inline std::uint8_t irq_random_byte(std::array<std::uint8_t,4>& state) {
    unsigned accumulator=state[0];bool carry=false;
    for(unsigned index:{1U,2U,3U,0U}) {
        const int difference=int(accumulator)-int(state[index])-(carry?0:1);
        carry=difference>=0;
        accumulator=unsigned(difference)&255U;
        state[index]=std::uint8_t(accumulator);
    }
    return state[0];
}
// Source ordering matters: the tunnel test consumes its random byte first.
// EX and Original RNGMODE=2 read RAND without advancing it here.
inline unsigned irq_palette_flashes(std::array<std::uint8_t,4>& random,
    bool tunnel,bool sky,bool advance) {
    unsigned result=0;
    if(tunnel && (advance?irq_random_byte(random):random[0])<51U) result|=1U;
    if(sky && (advance?irq_random_byte(random):random[0])<5U) result|=2U;
    return result;
}
}
