#include "starfox/simulation/game_simulation.hpp"
#include "starfox/input/buttons.hpp"
#include <iostream>
#include <memory>

int main(int argc,char** argv) try {
    if (argc != 4 && argc != 5) throw std::invalid_argument{"Expected ROM SYMBOLS MAP [--main]"};
    const bool main_loop=argc==5 && std::string_view{argv[4]}=="--main";
    if (argc==5 && !main_loop) throw std::invalid_argument{"Unknown execution mode"};
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    using namespace starfox;
    auto game=std::make_unique<simulation::GameSimulation>(rom,symbols,argv[3],std::span<const std::uint8_t>{},true);
    game->set_god_mode(true);
    const auto step=[&](const input::TickInput& controls) {
        if (main_loop) game->begin_native_gameplay_update(controls);
        else game->begin_native_transfer(controls);
        const std::array<input::ButtonMask,5> physical{controls.held,0U,0U,0U,0U};
        game->sample_native_controller_held(physical);
        unsigned slices{};
        while (!game->advance_native_transfer(32768U)) {
            game->sample_native_controller_held(physical);
            if (++slices>100000U) throw std::runtime_error{"Native input replay exceeded its budget"};
        }
    };
    unsigned warmup{};
    while (game->map().read_native_byte(symbols.find("PSHIPFLAGS").at(0)) & 0xe0U) {
        step({});
        if (++warmup>2000U) throw std::runtime_error{"Native input replay never reached player control"};
    }
    if (main_loop) {
        // The control flag clears before the launch wipe/pause gate settles.
        for (unsigned frame=0;frame<32U;++frame) step({});
        const auto pause=symbols.find("DOPAUSE").at(0);
        const auto print_pause=symbols.find("PRINTPAUSE").at(0);
        unsigned pause_reads{};
        game->map().set_native_instruction_boundary_callback([&](std::uint64_t) {
            const auto pc=game->map().native_program_address();
            if (pc>=pause && pc<print_pause && game->map().read_native_byte(pc)==0xadU
                    && game->map().read_native_byte(pc+1U)==0x19U
                    && game->map().read_native_byte(pc+2U)==0x42U) ++pause_reads;
        });
        step({input::start,input::start,0U});
        game->begin_native_gameplay_update({input::start,0U,0U});
        const auto wait_until=[&](const char* phase,const auto& ready) {
            unsigned slices{};
            while (!ready()) {
                if (game->advance_native_transfer(32768U))
                    throw std::runtime_error{std::string{"MAIN completed before pause phase: "}+phase};
                if (++slices>10000U) throw std::runtime_error{"Native pause input timed out"};
            }
        };
        wait_until("entry",[&] { return game->paused(); });
        game->sample_native_controller_held({});
        wait_until("released Start",[&] { return pause_reads>=2U; });
        const auto previous_reads=pause_reads;
        game->sample_native_controller_held({input::start,0U,0U,0U,0U});
        wait_until("pressed Start",[&] { return pause_reads>previous_reads; });
        game->sample_native_controller_held({});
        unsigned slices{};
        while (!game->advance_native_transfer(32768U))
            if (++slices>10000U) throw std::runtime_error{"Native MAIN did not resume after Start release"};
        if (game->paused()) throw std::runtime_error{"Native MAIN retained pause after returning from DOPAUSE"};
        game->map().set_native_instruction_boundary_callback({});
        step({});
        std::cout << "native MAIN pause accepted live release/press/release input\n";
    }
    std::cout << "player control after " << warmup << " transfers\n" << std::flush;
    for (const auto shoulder : {input::right_shoulder,input::left_shoulder}) {
        for (unsigned frame=0;frame<32U;++frame) step({});
        const input::TickInput tap{0U,shoulder,shoulder};
        bool rolled{};
        for (unsigned frame=0;frame<8U;++frame) {
            step(frame<2U ? tap : input::TickInput{});
            const auto velocity=game->map().read_native_byte(symbols.find("PLAYER_ROLLZVEL").at(0));
            if (frame==0U && velocity) throw std::runtime_error{"One tap incorrectly triggered a roll"};
            rolled |= velocity != 0U;
            const auto held=game->map().read_native_byte(symbols.find("CONTL0").at(0));
            if (frame>=2U && (held & shoulder)) throw std::runtime_error{"Released shoulder pulse remained held"};
        }
        if (!rolled) throw std::runtime_error{"Two complete shoulder taps were lost during native transfer IRQ polling"};
    }
    std::cout << "native transfer preserved complete shoulder taps\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
