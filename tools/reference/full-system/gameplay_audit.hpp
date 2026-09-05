#pragma once

// Development-only continuous comparison: clone once, then advance the host
// independently with neutral input and the native transfer's raster count.
#include "starfox/simulation/game_simulation.hpp"
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

class GameplayAudit {
    const starfox::assets::SymbolMap& symbols;
    std::unique_ptr<starfox::simulation::GameSimulation> game;
    std::ofstream output, frames;
    bool started = false;
    unsigned count = 0;
    unsigned required;
    std::uint64_t comparisons = 0;
    unsigned address(const char* name) const { return symbols.find(name).at(0); }
    unsigned native(unsigned p, unsigned size=2) const {
        unsigned v=0;
        for(unsigned i=0;i<size;++i) v |= unsigned(sfc::cpu.wram[(p+i)&0x1ffff]) << (8*i);
        return v;
    }
public:
    std::string error;
    unsigned differences = 0;
    GameplayAudit(const starfox::assets::RomImage& rom, const starfox::assets::SymbolMap& s,
        const std::string& map, const std::string& prefix, unsigned updates) : symbols(s),
        game(std::make_unique<starfox::simulation::GameSimulation>(rom,s,map,std::span<const std::uint8_t>{},true)),
        output(prefix+"-gameplay-differences.csv"), frames(prefix+"-gameplay.csv"), required(updates) {
        if (!output || !frames) throw std::runtime_error("Cannot create gameplay traces");
        output << "transfer,field,host,native\n";
        frames << "transfer,gameframe,raster_phases,host_raster_phases,objects,comparisons,differences\n";
    }
    bool complete() const { return count == required; }
    void finish() const {
        std::cout << "Gameplay: " << count << '/' << required << " updates, "
            << comparisons << " comparisons, " << differences << " differences\n";
        if (!error.empty()) throw std::runtime_error(error);
        if (differences) throw std::runtime_error("Gameplay state differs from native execution");
        if (!started || !complete()) throw std::runtime_error("Gameplay comparison did not reach its required update count");
    }
    void transfer() { try {
        if (!error.empty() || differences || complete()) return;
        if (!started) {
            if(native(address("GAMEFRAME")) != 0) return;
            if(native(address("PLAYPT")) != game->map().read_native_word(address("PLAYPT")))
                throw std::runtime_error("Player allocation differs at clone boundary");
            for(unsigned i=0;i<0x20000;++i) game->map().write_native_byte(0x7e0000+i,sfc::cpu.wram[i]);
            for(unsigned i=0;i<0x10000;++i) game->map().write_native_byte(0x700000+i,sfc::superfx.ram.read(i));
            if(sfc::superfx.ram.size()>0x10000)
                for(unsigned i=0;i<0x10000;++i) game->map().write_native_byte(0x710000+i,sfc::superfx.ram.read(0x10000+i));
            game->map().restore_objects_from_native();
            game->map().restore_map_state_from_native();
            game->map().restore_display_from_native();
            started = true;
            std::cout << "Gameplay clone at GAMEFRAME=0 after the preceding display interrupt\n";
            return;
        }
        // TRANSFER_L copies FRAMEC into FRAMER before waiting for IRQBIT3.
        // Compare after that wait; XINIDISP may still be pending at entry.
        const auto phases=native(address("FRAMER"),1);
        if (!phases) throw std::runtime_error("Native gameplay transfer has no raster phases");
        for(unsigned i=0;i<phases;++i) game->present_frame();
        // Make the host's existing three-raster minimum explicit in the
        // trace. This audit compares state, not an exact frame scheduler.
        auto host_phases = phases;
        while (!game->logic_tick_ready()) {
            game->present_frame();
            ++host_phases;
        }
        static_cast<void>(game->tick({}));
        ++count;
        const auto before = comparisons;
        auto compare=[&](const std::string& name, unsigned p, unsigned size=2) {
            unsigned host=game->map().read_native_byte(p);
            if(size==2) host |= unsigned(game->map().read_native_byte(p+1))<<8;
            auto expected=native(p,size);
            ++comparisons;
            if (host != expected) {
                output<<count<<','<<name<<','<<host<<','<<expected<<'\n';
                ++differences;
            }
        };
        for (auto name:{"FADE","FADEDIR","XINIDISP1","XINIDISP2","XINIDISP1A"})
            compare(name,address(name),1);
        for(auto name:{"GAMEFRAME","MAPPTR","MAPCNT","LASTPLAYZ","RAND","VIEWPOSX","VIEWPOSY","VIEWPOSZ",
            "PLAYPT","ALLST","ALFREELST","WMAT11W","WMAT12W","WMAT13W","WMAT21W","WMAT22W","WMAT23W","WMAT31W","WMAT32W","WMAT33W"})
            compare(name,address(name));
        auto object=native(address("ALLST"));
        const auto stride = address("AL_SIZE"), capacity = address("NUMBER_AL"), base = address("ALBLKS");
        const auto extended_size = stride == 57 ? 56U : 54U;
        unsigned guard=0;
        while(object) {
            if (++guard > capacity || object < base || (object-base) % stride
                || (object-base) / stride >= capacity)
                throw std::runtime_error("Invalid native active-object list");
            for(unsigned i=0;i<stride;++i) compare("object_"+std::to_string(object)+"_"+std::to_string(i),object+i,1);
            auto extended=address("XALBLKS") + object-base;
            for(unsigned i=0;i<extended_size;++i) compare("extended_"+std::to_string(object)+"_"+std::to_string(i),extended+i,1);
            object=native(object);
        }
        frames << count << ',' << native(address("GAMEFRAME")) << ',' << phases << ',' << host_phases << ',' << guard
            << ',' << comparisons-before << ',' << differences << '\n';
        if (!output || !frames) throw std::runtime_error("Cannot write gameplay traces");
        if(differences) std::cout<<"Gameplay differences at transfer "<<count<<": "<<differences<<'\n';
    } catch(const std::exception& e) {error=e.what();} }
};
