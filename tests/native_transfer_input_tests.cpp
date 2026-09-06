#include "starfox/simulation/game_simulation.hpp"
#include "starfox/input/buttons.hpp"
#include <iostream>
#include <memory>

int main(int argc,char** argv) try {
    if (argc != 4) throw std::invalid_argument{"Expected ROM SYMBOLS MAP"};
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    using namespace starfox;
    auto game=std::make_unique<simulation::GameSimulation>(rom,symbols,argv[3],std::span<const std::uint8_t>{},true);
    game->set_god_mode(true);
    const auto step=[&](const input::TickInput& controls) {
        game->begin_native_transfer(controls);
        unsigned slices{};
        while (!game->advance_native_transfer(32768U))
            if (++slices>100000U) throw std::runtime_error{"Native input replay exceeded its budget"};
    };
    unsigned warmup{};
    while (game->map().read_native_byte(symbols.find("PSHIPFLAGS").at(0)) & 0xe0U) {
        step({});
        if (++warmup>2000U) throw std::runtime_error{"Native input replay never reached player control"};
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
