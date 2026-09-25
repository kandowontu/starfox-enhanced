#include "starfox/simulation/irq_palette.hpp"
#include <stdexcept>

static void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {
    using namespace starfox::simulation;
    {
        TitlePaletteCycle c;c.enabled=1;c.frame=2;
        for(unsigned i=0;i<9;++i){c.foreground[i]=100+i;c.background[i]=200+i;}
        std::array<std::uint8_t,3> ram{0,1,0};
        auto read=[&](auto a){return ram[a];};auto write=[&](auto a,auto v){ram[a]=v;};
        for(unsigned tick=0;tick<189;++tick) {
            const auto phase=tick%63;const auto expected=phase<16?phase/2:8;
            unsigned count=0;
            apply_ex_title_palette_cycle(c,false,read,write,[&](auto table,auto dest){
                require(table==(count?200U:100U)+expected,"Title palette cadence/table");
                require(dest==(count?0U:16U),"Title palette bank/order");++count;
            });
            require(count==2,"Title palette pair missing");
        }
        const auto saved=ram;
        apply_ex_title_palette_cycle(c,true,read,write,[](auto,auto){require(false,"Title ignored COLORTRIP");});
        require(ram==saved,"Suppressed title cycle advanced");
        ram[1]=0;
        apply_ex_title_palette_cycle(c,false,read,write,[](auto,auto){require(false,"Disabled title cycle uploaded");});
        ram=saved;c.background[8]=0;
        apply_ex_title_palette_cycle(c,false,read,write,[](auto,auto){require(false,"Partial title descriptor uploaded");});
        require(ram==saved,"Missing title table advanced state");
        c.background[8]=208;
        for(const auto frame:{62U,63U,127U,129U,130U,255U}) {
            ram[2]=std::uint8_t(frame);unsigned count=0;
            const unsigned expected=frame==62?8U:0U;
            apply_ex_title_palette_cycle(c,false,read,write,[&](auto table,auto){
                require(table==(count++?200U:100U)+expected,"Title byte CMP/BMI semantics");
            });
            const auto next=frame==63 || frame==127 || frame==129?1:std::uint8_t(frame+1);
            require(ram[2]==next,"Title reset/byte increment");
        }
    }
    {
        std::array<IrqPaletteCycle,3> cycles{};
        std::array<std::uint8_t,16> ram{};
        for(unsigned i=0;i<3;++i){
            cycles[i].enabled=1+i;cycles[i].frame=4+i;
            cycles[i].hold=i==1?3:2;cycles[i].destination=i==2?48:64;
            ram[1+i]=1;
            for(unsigned n=0;n<8;++n)cycles[i].tables[n]=100+i*8+n;
        }
        auto read=[&](auto a){return ram[a];};
        auto write=[&](auto a,auto v){ram[a]=v;};
        for(unsigned tick=0;tick<96;++tick){
            unsigned count=0;
            apply_ex_irq_palette_cycles(cycles,false,read,write,[&](auto table,auto dest){
                const auto hold=count==1?3U:2U;
                require(table==100+count*8+(tick/hold)%8,"EX cycle cadence/table changed");
                require(dest==(count==2?48U:64U),"EX cycle bank/order changed");
                ++count;
            });
            require(count==3,"EX cycle missing upload");
        }
        const auto saved=ram;
        apply_ex_irq_palette_cycles(cycles,true,read,write,[](auto,auto){require(false,"COLORTRIP did not suppress cycles");});
        require(ram==saved,"Suppressed cycle advanced RAM frame");
        ram[1]=ram[2]=ram[3]=0;
        apply_ex_irq_palette_cycles(cycles,false,read,write,[](auto,auto){require(false,"Disabled cycle uploaded");});
        require(ram[4]==saved[4] && ram[5]==saved[5] && ram[6]==saved[6],"Disabled cycle advanced");
        ram=saved;
        const auto checkpoint=ram;
        std::array<unsigned,60> sequence{};
        unsigned cursor=0;
        for(unsigned n=0;n<20;++n)
            apply_ex_irq_palette_cycles(cycles,false,read,write,
                [&](auto table,auto){sequence[cursor++]=table;});
        ram=checkpoint;cursor=0;
        for(unsigned n=0;n<20;++n)
            apply_ex_irq_palette_cycles(cycles,false,read,write,
                [&](auto table,auto){require(sequence[cursor++]==table,"Restored source counters changed cycle sequence");});
        const auto before_missing=ram;
        cycles[0].tables[3]=0;
        cycles[1].enabled=0;
        cycles[2].frame=0;
        apply_ex_irq_palette_cycles(cycles,false,read,write,
            [](auto,auto){require(false,"Incomplete cycle descriptors uploaded");});
        require(ram==before_missing,"Incomplete descriptors changed source counters");
    }
    std::array<std::uint8_t,4> random{};
    require(irq_random_byte(random)==254 && random==std::array<std::uint8_t,4>{254,255,254,254},
        "IRQRAND lost CLC/SBC borrow propagation");
    require(irq_random_byte(random)==2 && random==std::array<std::uint8_t,4>{2,254,255,0},
        "IRQRAND changed its second source sequence");
    require(irq_random_byte(random)==0 && random==std::array<std::uint8_t,4>{0,3,3,2},
        "IRQRAND changed its third source sequence");
    const auto unchanged=random;
    require(irq_palette_flashes(random,false,false,true)==0 && random==unchanged,
        "Disabled flashes consumed gameplay randomness");
    random={};
    require(irq_palette_flashes(random,true,true,true)==2 && random==std::array<std::uint8_t,4>{2,254,255,0},
        "Tunnel and sky tests ran in the wrong source order");
    unsigned tunnel_count=0,sky_count=0;
    for(unsigned sample=0;sample<256;++sample) {
        random={std::uint8_t(sample),17,29,41};const auto before=random;
        const auto selected=irq_palette_flashes(random,true,true,false);
        tunnel_count+=(selected&1)!=0;sky_count+=(selected&2)!=0;
        require(random==before,"EX/RNGMODE=2 modified RAND");
    }
    require(tunnel_count==51 && sky_count==5,"IRQ palette probability thresholds changed");
    random={4,23,127,255};auto restored=random;
    for(unsigned i=0;i<1000;++i)
        require(irq_palette_flashes(random,true,true,true)==irq_palette_flashes(restored,true,true,true)
            && random==restored,"Restoring source RAND did not restore flash sequence");
}
