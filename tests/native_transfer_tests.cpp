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
    if (argc != 4 && argc != 5) throw std::invalid_argument{"Expected ROM SYMBOLS MAP [--main]"};
    const bool main_loop=argc==5 && std::string_view{argv[4]}=="--main";
    if (argc==5 && !main_loop) throw std::invalid_argument{"Unknown execution mode"};
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    auto small=std::make_unique<GameSimulation>(rom,symbols,argv[3],std::span<const std::uint8_t>{},true);
    auto large=std::make_unique<GameSimulation>(rom,symbols,argv[3],std::span<const std::uint8_t>{},true);
    const auto messages=symbols.find("FRIENDS_MESSAGES_L").at(0);
    const auto second_messages=symbols.find("FRIENDS_MESSAGES2_L");
    const auto timer=symbols.find("CESTIMER_L");
    std::array<unsigned,3> main_calls{};
    if (main_loop) {
        if (!second_messages.empty()) {
            for (auto* game : {small.get(),large.get()})
                game->map().write_native_byte(symbols.find("SCORED").at(0),1U);
        }
        small->map().set_native_instruction_boundary_callback([&](std::uint64_t) {
            const auto pc=small->map().native_program_address();
            if (pc==messages) ++main_calls[0];
            if (!second_messages.empty() && pc==second_messages[0]) ++main_calls[1];
            if (!timer.empty() && pc==timer[0]) ++main_calls[2];
        });
    }
    const auto gameframe=symbols.find("GAMEFRAME").at(0);
    const auto run=[&](GameSimulation& game,std::uint64_t quantum) {
        const auto old_objects=object_bytes(game);
        const auto old_order=game.draw_order();
        const auto old_frame=game.map().read_native_word(gameframe);
        const auto old_vram=game.map().ppu_state().vram;
        const auto before=game.native_transfer_clock();
        const auto begin=[&] {
            if (main_loop) game.begin_native_gameplay_update({});
            else game.begin_native_transfer({});
        };
        begin();
        bool rejected{};
        try { begin(); } catch (const std::logic_error&) { rejected=true; }
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
    if (main_loop) {
        require(main_calls[0]==9U,"MAIN skipped or repeated its first communications channel");
        if (!second_messages.empty())
            require(main_calls[1]==9U && main_calls[2]==9U,"EX MAIN skipped or repeated its second channel or scored display");
        small->map().set_native_instruction_boundary_callback({});
        const auto stage=small->map().read_native_word(symbols.find("STAGE").at(0));
        const auto doing_end=small->map().read_native_byte(symbols.find("DOINGEND").at(0));
        small->map().write_native_word(symbols.find("LEVELFINISHED").at(0),10U);
        static_cast<void>(run(*small,4096U));
        require(small->native_gameplay_exit_pending(),"MAIN did not stop for a scene-exit handoff");
        require(small->map().read_native_word(symbols.find("STAGE").at(0))==stage
            && small->map().read_native_byte(symbols.find("DOINGEND").at(0))==doing_end,
            "MAIN entered scene-exit processing before the handoff");
        bool rejected{};
        try { small->begin_native_gameplay_update({}); } catch (const std::logic_error&) { rejected=true; }
        require(rejected,"MAIN resumed gameplay with a pending scene exit");
    }
    std::cout << (main_loop ? "native MAIN updates" : "native transfers")
        << ": stable in-flight publication and identical clocks/state across quanta\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
