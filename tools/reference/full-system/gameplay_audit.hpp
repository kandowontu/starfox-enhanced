#pragma once

// Development-only continuous comparison: clone once, then advance the host
// independently with neutral input and the native transfer's raster count.
#include "starfox/simulation/game_simulation.hpp"
#include <fstream>
#include <memory>
#include <map>
#include <stdexcept>
#include <string>

class GameplayAudit {
    const starfox::assets::SymbolMap& symbols;
    std::unique_ptr<starfox::simulation::GameSimulation> game;
    std::string output_prefix;
    std::ofstream output, frames, seed;
    bool started = false;
    unsigned count = 0;
    unsigned required;
    bool seed_at_first_transfer;
    std::uint64_t comparisons = 0;
    std::uint64_t submitted_comparisons = 0;
    std::map<unsigned, std::uint8_t> submitted_flags;
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
        const std::string& map, const std::string& prefix, unsigned updates,
        bool first_transfer = false) : symbols(s),
        game(std::make_unique<starfox::simulation::GameSimulation>(rom,s,map,std::span<const std::uint8_t>{},true)),
        output_prefix(prefix),
        output(prefix+"-gameplay-differences.csv"), frames(prefix+"-gameplay.csv"),
        seed(prefix+"-gameplay-seed.csv"), required(updates), seed_at_first_transfer(first_transfer) {
        if (!output || !frames || !seed) throw std::runtime_error("Cannot create gameplay traces");
        output << "transfer,field,host,native\n";
        frames << "transfer,gameframe,raster_phases,host_raster_phases,objects,comparisons,submitted_flags,hitflashes,differences\n";
        seed << "mode,gameframe,map,player\n";
    }
    bool complete() const { return count == required; }
    void capture_submitted_flags() { try {
        if (!started || complete() || differences || !error.empty()) return;
        const auto object = unsigned(sfc::cpu.r.y.w);
        const auto list = unsigned(sfc::cpu.r.x.w);
        const auto offset = address("DL_SFLAGS") & 65535U;
        submitted_flags[object] = sfc::superfx.ram.read(list + offset);
    } catch (const std::exception& exception) { error = exception.what(); } }
    void finish() const {
        std::cout << "Gameplay: " << count << '/' << required << " updates, "
            << comparisons << " comparisons, " << differences << " differences\n";
        if (!error.empty()) throw std::runtime_error(error);
        if (differences) throw std::runtime_error("Gameplay state differs from native execution");
        if (!started || !complete()) throw std::runtime_error("Gameplay comparison did not reach its required update count");
        if (!submitted_comparisons) throw std::runtime_error("Gameplay comparison did not observe submitted draw flags");
    }
    void transfer() { try {
        if (!error.empty() || differences || complete()) return;
        if (!started) {
            if(!seed_at_first_transfer && native(address("GAMEFRAME")) != 0) return;
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
            const auto frame = native(address("GAMEFRAME"));
            seed << (seed_at_first_transfer ? "first-transfer" : "zero-frame") << ','
                << frame << ',' << native(address("MAPPTR")) << ',' << native(address("PLAYPT")) << '\n';
            if (!seed) throw std::runtime_error("Cannot write gameplay seed trace");
            std::cout << "Gameplay clone at GAMEFRAME=" << frame << " after the preceding display interrupt\n";
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
        const auto submitted_before = submitted_comparisons;
        unsigned hitflashes = 0;
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
            if (const auto submitted = submitted_flags.find(object); submitted != submitted_flags.end()) {
                const auto handle = static_cast<starfox::simulation::ObjectHandle>((object-base)/stride+1);
                const auto actual = game->submitted_strategy_flags(handle);
                ++comparisons;
                ++submitted_comparisons;
                hitflashes += (submitted->second & 2U) != 0U;
                if (actual != submitted->second) {
                    ++differences;
                    output << count << ",submitted_flags_" << object << ',' << unsigned(actual)
                        << ',' << unsigned(submitted->second) << '\n';
                }
            }
            object=native(object);
        }
        submitted_flags.clear();
        frames << count << ',' << native(address("GAMEFRAME")) << ',' << phases << ',' << host_phases << ',' << guard
            << ',' << comparisons-before << ',' << submitted_comparisons-submitted_before
            << ',' << hitflashes << ',' << differences << '\n';
        if (!output || !frames) throw std::runtime_error("Cannot write gameplay traces");
        if(differences) {
            // Preserve local diagnostic state at the first failing boundary.
            // These snapshots are evidence for investigation, not extra
            // compared fields or redistributed game assets.
            std::ofstream host(output_prefix + "-host-wram.bin", std::ios::binary);
            std::ofstream source(output_prefix + "-native-wram.bin", std::ios::binary);
            for (unsigned i = 0; i < 0x20000; ++i) {
                host.put(game->map().read_native_byte(0x7e0000U + i));
                source.put(sfc::cpu.wram[i]);
            }
            if (!host || !source) throw std::runtime_error("Cannot save failure-state snapshots");
            std::cout<<"Gameplay differences at transfer "<<count<<": "<<differences<<'\n';
        }
    } catch(const std::exception& e) {error=e.what();} }
};
