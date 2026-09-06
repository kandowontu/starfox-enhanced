#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/snes_timeline.hpp"
#include <iostream>
#include <memory>

using namespace starfox::simulation;
void require(bool value,const char* message) { if (!value) throw std::runtime_error{message}; }
std::vector<std::uint8_t> object_bytes(const GameSimulation& game) {
    std::vector<std::uint8_t> result;
    for (const auto handle : game.objects().active_handles()) {
        result.push_back(static_cast<std::uint8_t>(handle));
        const auto size=game.objects().capacity()==kMaximumObjects ? 57U : 56U;
        for (unsigned offset=4;offset<size;++offset) result.push_back(game.objects().read_base_byte(handle,offset));
        for (unsigned offset=0;offset<56U;++offset) result.push_back(game.objects().read_path_byte(handle,0x80U+offset));
    }
    return result;
}
int main(int argc,char** argv) try {
    if (argc != 4) throw std::invalid_argument{"Expected ROM SYMBOLS MAP"};
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    auto small=std::make_unique<GameSimulation>(rom,symbols,argv[3],std::span<const std::uint8_t>{},true);
    auto large=std::make_unique<GameSimulation>(rom,symbols,argv[3],std::span<const std::uint8_t>{},true);
    const auto gameframe=symbols.find("GAMEFRAME").at(0);
    const auto run=[&](GameSimulation& game,std::uint64_t quantum) {
        const auto old_objects=object_bytes(game);
        const auto old_order=game.draw_order();
        const auto old_frame=game.map().read_native_word(gameframe);
        const auto old_vram=game.map().ppu_state().vram;
        const auto before=game.native_transfer_clock();
        game.begin_native_transfer({});
        bool rejected{};
        try { game.begin_native_transfer({}); } catch (const std::logic_error&) { rejected=true; }
        require(rejected,"native transfer accepted a second input/start while active");
        require(!game.advance_native_transfer(0U) && game.native_transfer_clock()==before,
            "zero native transfer budget advanced execution");
        unsigned yields{};
        while (!game.advance_native_transfer(quantum)) {
            require(game.native_transfer_active() && game.map().native_presentation_held(),
                "unfinished native transfer lost its presentation hold");
            if ((yields++ % 31U)==0U)
                require(object_bytes(game)==old_objects && game.draw_order()==old_order
                    && game.map().read_native_word(gameframe)==old_frame && game.map().ppu_state().vram==old_vram,
                    "partial source update leaked into GameSimulation presentation");
            require(yields<1000000U,"native transfer did not complete");
        }
        require(!game.native_transfer_active() && !game.map().native_presentation_held(),
            "completed native transfer did not publish and release its hold");
        rejected=false;
        try { static_cast<void>(game.tick({})); } catch (const std::logic_error&) { rejected=true; }
        require(rejected,"legacy host tick mixed with live native execution");
        return game.native_transfer_clock()-before;
    };
    for (unsigned frame=0;frame<9U;++frame) {
        // The final sub-instruction quantum lands on phase entries as
        // deadline yields, rather than only as explicit stop-address yields.
        const auto small_clocks=run(*small,frame==8U ? 2U : 4096U);
        const auto large_clocks=run(*large,100'000'000U);
        require(small_clocks==large_clocks && object_bytes(*small)==object_bytes(*large)
            && small->draw_order()==large->draw_order()
            && small->map().ppu_state().vram==large->map().ppu_state().vram
            && small->map().ppu_state().cgram==large->map().ppu_state().cgram,
            "GameSimulation native publication depends on scheduling quantum");
        for (const auto handle : small->draw_order())
            require(small->submitted_strategy_flags(handle)==large->submitted_strategy_flags(handle),
                "native submitted flags depend on scheduling quantum");
    }
    std::cout << "nine native GameSimulation transfers: stable in-flight publication and identical clocks/state across quanta\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
