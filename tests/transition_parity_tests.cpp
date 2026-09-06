#include "starfox/assets/rom.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/object_pool.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void source_tick(starfox::simulation::GameSimulation& game,
    const starfox::input::TickInput& input = {}) {
    game.present_frame();
    game.present_frame();
    game.present_frame();
    static_cast<void>(game.tick(input));
}

void check_gameplay_bitmap_dma(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols) {
    using namespace starfox::simulation;
    // Run the unmodified first IRQBIT3 DMA block, after its beam wait and
    // before the next BG2 DMA. This keeps the source's OAM length authoritative.
    Wdc65816 reference{rom, &symbols}, host{rom, &symbols};
    Wdc65816Registers copy_registers;
    copy_registers.status = 0x24;
    reference.call_long(symbols.find("COPY_TO_0101_L").at(0), copy_registers, 5'000'000);
    const auto irq = symbols.find("IRQBIT3").at(0);
    const auto sprite = symbols.find("SPRITEBLK").at(0);
    std::uint32_t begin{};
    for (unsigned offset = 0; offset < 256; ++offset) {
        const auto pc = irq + offset;
        if (reference.read8(pc) == 0xa9 && reference.read8(pc + 1) == 4
            && reference.read8(pc + 2) == 0x8d && reference.read16(pc + 3) == 0x4301
            && reference.read8(pc + 5) == 0xa2 && reference.read16(pc + 6) == 0
            && reference.read8(pc + 8) == 0x8e && reference.read16(pc + 9) == 0x2102) {
            require(begin == 0, "ambiguous source gameplay OAM DMA");
            begin = pc;
        }
    }
    require(begin != 0 && reference.read8(begin + 22) == 0xa2
        && reference.read16(begin + 23) == 328 && reference.read8(begin + 35) == 0x8d
        && reference.read16(begin + 36) == 0x420b, "unexpected source gameplay OAM DMA block");
    for (auto* cpu : {&reference, &host}) {
        for (unsigned i = 0; i < 544; ++i) cpu->write8(0x7f8000 + i, 0xa7);
        cpu->upload_oam(0x7f8000, 544);
        for (unsigned i = 0; i < 328; ++i) cpu->write8(sprite + i, 1 + i % 251);
    }
    for (auto* cpu : {&reference, &host}) {
        cpu->write16(symbols.find("VMAP1").at(0), 0x4000);
        cpu->write16(symbols.find("VMAP2").at(0), 0x1000);
        cpu->write8(0, 2);
        const auto bitmap = symbols.find("BITMAP1").at(0);
        for (unsigned i = 0; i < 21504; ++i)
            cpu->write8(0x700000U | ((bitmap + i) & 65535U), 1 + (i * 17 + 19) % 251);
    }
    const std::array music_stop{symbols.find("STARTMUS").at(0)};
    for (const auto* phase : {"IRQBIT1", "IRQBIT2"}) {
        Wdc65816Registers phase_registers;
        phase_registers.status = 0x24;
        const auto result = reference.begin_long_task(symbols.find(phase).at(0),
            phase_registers, music_stop, 1000);
        require(!result.returned && result.stop_address == music_stop[0],
            "source bitmap IRQ did not reach its post-DMA audio boundary");
        require(host.advance_gameplay_bitmap_dma_phase(), "host bitmap DMA did not advance");
        require(host.ppu_state().vram == reference.ppu_state().vram
            && host.read8(0) == reference.read8(0)
            && host.read8(symbols.find("TRANSBMP1").at(0))
                == reference.read8(symbols.find("TRANSBMP1").at(0)),
            "partial bitmap transfer differs from the source DMA");
    }
    Wdc65816Registers registers;
    registers.status = 0x24;
    const std::array stops{begin + 38};
    const auto task = reference.begin_long_task(begin, registers, stops, 100);
    require(!task.returned && task.stop_address == stops[0], "source OAM DMA did not reach its boundary");
    host.write8(0, 6);
    host.write8(symbols.find("NOIRQBIT3").at(0), 1);
    require(host.advance_gameplay_bitmap_dma_phase(), "host OAM phase did not complete");
    require(host.ppu_state().oam == reference.ppu_state().oam,
        "gameplay bitmap completion differs from the source OAM DMA");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: starfox_transition_parity_tests ROM SYMBOLS\n";
        return 2;
    }

    const auto rom = starfox::assets::RomImage::load(argv[1]);
    const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    check_gameplay_bitmap_dma(rom, symbols);
    const auto starfox_ex = !symbols.find("PLANETSEQ2_L").empty();
    starfox::simulation::ObjectPool objects{
        starfox_ex ? starfox::simulation::kMaximumObjects
                   : starfox::simulation::kOriginalMaximumObjects,
        starfox_ex ? starfox::simulation::ObjectMemoryLayout::starfox_ex
                   : starfox::simulation::ObjectMemoryLayout::original};
    starfox::simulation::MapDatabase database{rom, symbols};
    starfox::simulation::MapVm map{rom, database, objects, &symbols};
    const auto game_frame = symbols.find("GAMEFRAME").front();

    // Compare every valid FADE counter against the assembled SETINIDISP.
    // Use independent display values to catch stores that the source skips.
    starfox::simulation::Wdc65816 reference{rom, &symbols};
    starfox::simulation::Wdc65816Registers registers;
    registers.status = 0x24U;
    reference.call_long(symbols.find("COPY_TO_0101_L").front(), registers, 5'000'000U);
    registers = {};
    registers.status = 0x24U;
    map.call_native_routine(symbols.find("COPY_TO_0101_L").front(), registers, 5'000'000U);
    // Run the unmodified IRQ flash block through its own branches and DMA
    // writes. Compare the complete CGRAM image and all four RNG bytes.
    const auto find_flag_load = [&](const char* name) {
        const auto flag = symbols.find(name).front();
        const auto irq = symbols.find("IRQBIT3").front();
        std::uint32_t found = 0;
        for (unsigned i = 0; i < 2048; ++i) {
            const auto pc = irq + i;
            if (reference.read8(pc) == 0xad && reference.read16(pc + 1) == flag
                && reference.read8(pc + 3) == 0xf0) {
                require(found == 0, "ambiguous native IRQ flash block");
                found = pc;
            }
        }
        require(found != 0, "missing native IRQ flash block");
        return found;
    };
    const auto flash_begin = find_flag_load("FLASHTUNNELON");
    const auto background_begin = find_flag_load("FLASHBG");
    const auto flash_end = background_begin + 5U
        + static_cast<std::int8_t>(reference.read8(background_begin + 4));
    const std::array flash_stops{flash_end};
    unsigned flash_cases = 0, tunnel_flashes = 0, background_flashes = 0;
    for (unsigned enabled = 0; enabled < 4; ++enabled) {
        for (unsigned low = 0; low < 256; ++low) {
            const auto write = [&](std::uint32_t address, std::uint8_t value) {
                reference.write8(address, value);
                map.write_native_byte(address, value);
            };
            const auto seed = (0x9e3779b9U * (low + 1U)) ^ 0x12345678U;
            for (unsigned i = 0; i < 4; ++i)
                write(symbols.find("RAND").front() + i, static_cast<std::uint8_t>(seed >> (i * 8U)));
            write(symbols.find("FLASHTUNNELON").front(), enabled & 1U);
            write(symbols.find("FLASHBG").front(), enabled & 2U);
            // Distinct fixtures establish both DMA range and source table;
            // they do not depend on incidental copied palette contents.
            for (const auto [name, bytes] : {std::pair{"REDTUNNEL", 64U},
                     std::pair{"THUNDERCOL", 32U}}) {
                const auto palette = symbols.find(name).front();
                for (unsigned i = 0; i < bytes; ++i) write(palette + i, static_cast<std::uint8_t>(i * 37U));
            }
            std::array<std::uint16_t, 256> palette{};
            palette.fill(0x1234U);
            reference.write_cgram(0, palette);
            map.write_cgram(0, palette);
            registers = {};
            registers.status = 0x24U;
            const auto task = reference.begin_long_task(flash_begin, registers, flash_stops);
            require(!task.returned && task.stop_address == flash_end,
                "native IRQ flash block did not reach its exit");
            static_cast<void>(map.apply_irq_palette_flashes());
            for (unsigned i = 0; i < 4; ++i) {
                const auto address = symbols.find("RAND").front() + i;
                require(map.read_native_byte(address) == reference.read8(address),
                    "IRQ flash RNG consumption differs from the source");
            }
            if (map.ppu_state().cgram != reference.ppu_state().cgram) {
                for (unsigned i = 0; i < palette.size(); ++i) {
                    if (map.ppu_state().cgram[i] != reference.ppu_state().cgram[i])
                        std::cerr << "flash flags=" << enabled << " seed=" << seed
                            << " colour=" << i << " host=" << map.ppu_state().cgram[i]
                            << " native=" << reference.ppu_state().cgram[i] << '\n';
                }
                require(false, "IRQ flash palettes differ from native DMA output");
            }
            tunnel_flashes += reference.ppu_state().cgram[0] != palette[0];
            background_flashes += reference.ppu_state().cgram[80] != palette[80];
            ++flash_cases;
        }
    }
    require(tunnel_flashes != 0 && background_flashes != 0,
        "IRQ cases did not exercise both visible flash palettes");
    std::cout << "IRQ flashes: " << flash_cases << " native RNG/palette cases, "
        << tunnel_flashes << " tunnel and " << background_flashes << " background flashes\n";
    const auto fade = symbols.find("FADE").front();
    const auto fade_direction = symbols.find("FADEDIR").front();
    for (const auto frame : {2U, 3U}) {
        for (const auto direction : {-4, -3, -2, -1, 0, 1, 2, 3}) {
            for (unsigned value = 0U; value <= 15U; ++value) {
                for (const auto display : {0x80U, 7U, 15U}) {
                    const auto write = [&](std::uint32_t address, std::uint8_t byte) {
                        reference.write8(address, byte);
                        map.write_native_byte(address, byte);
                    };
                    write(game_frame, frame);
                    write(fade, value);
                    write(fade_direction, static_cast<std::uint8_t>(direction));
                    for (const auto name : {"XINIDISP1", "XINIDISP2", "XINIDISP1A"}) {
                        write(symbols.find(name).front(), display);
                    }
                    registers = {};
                    registers.status = 0x34U;
                    reference.call_near(symbols.find("SETINIDISP").front(), registers);
                    map.tick_display_transfer();
                    for (const auto name : {"FADE", "FADEDIR", "XINIDISP1", "XINIDISP2", "XINIDISP1A"}) {
                        const auto address = symbols.find(name).front();
                        if (map.read_native_byte(address) != reference.read8(address)) {
                            std::cerr << name << " differs: direction=" << direction
                                << " fade=" << value << " display=" << display
                                << " GAMEFRAME=" << frame << '\n';
                            require(false, "display transfer differs from native SETINIDISP");
                        }
                    }
                }
            }
        }
    }

    // IRQ.ASM's QFADEDOWN branches into SETDOWN for its second DEC.
    map.set_display_brightness(11U);
    map.start_display_fade(-2);
    map.tick_video_phase();
    require(map.display_brightness() == 9U,
        "quick fade-down did not decrement twice");
    for (const auto brightness : {7U, 5U, 3U, 1U}) {
        map.tick_video_phase();
        require(map.display_brightness() == brightness,
            "quick fade-down skipped a native brightness value");
    }
    map.tick_video_phase();
    require(map.display_brightness() == 0U && map.fade_direction() == 0,
        "quick fade-down did not finish on its sixth transfer");

    // SFADEDOWN skips odd GAMEFRAME values, irrespective of how the caller
    // schedules completed display transfers.
    map.set_display_brightness(15U);
    map.write_native_byte(game_frame, 2U);
    map.start_display_fade(-3);
    map.tick_video_phase();
    map.tick_video_phase();
    map.tick_video_phase();
    require(map.display_brightness() == 12U,
        "slow fade did not advance on every raster of even GAMEFRAME");
    map.write_native_byte(game_frame, 3U);
    map.tick_video_phase();
    map.tick_video_phase();
    map.tick_video_phase();
    require(map.display_brightness() == 12U,
        "slow fade advanced during odd GAMEFRAME");
    map.write_native_byte(game_frame, 4U);
    map.tick_video_phase();
    require(map.display_brightness() == 11U,
        "slow fade did not resume on the next even GAMEFRAME");

    // QFADEUP performs two increments before falling into SETUP's third.
    map.set_display_brightness(0U);
    map.start_display_fade(2);
    map.tick_video_phase();
    require(map.display_brightness() == 3U,
        "quick fade-up did not advance three brightness steps");

    // ENDSEQ.ASM intentionally gives the EX logo/intro fifteen additional
    // source ticks before accepting a skip (45 versus retail's 30).
    auto game = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "BOOT");
    game->set_experience(starfox_ex
        ? starfox::simulation::Experience::starfox_ex
        : starfox::simulation::Experience::original);
    source_tick(*game, {starfox::input::start, starfox::input::start, 0U});
    for (std::size_t tick = 0;
         tick < 2'000U
            && game->flow_state()
                != starfox::simulation::GameFlowState::intro;
         ++tick) {
        source_tick(*game);
    }
    require(game->flow_state() == starfox::simulation::GameFlowState::intro,
        "pre-game fade did not reach the intro");
    const auto minimum_intro_ticks = starfox_ex ? 45U : 30U;
    for (std::uint32_t tick = 1U; tick < minimum_intro_ticks; ++tick) {
        const auto press_early = tick + 1U == minimum_intro_ticks;
        source_tick(*game, press_early
            ? starfox::input::TickInput{
                  starfox::input::start, starfox::input::start, 0U}
            : starfox::input::TickInput{});
    }
    require(game->flow_state() == starfox::simulation::GameFlowState::intro,
        "intro accepted a skip before its source threshold");
    source_tick(*game,
        {starfox::input::start, starfox::input::start, 0U});
    require(game->map().fade_direction() == -2
            && game->map().display_brightness() == 11U,
        "intro did not arm its quick fade at brightness 11 at the source threshold");

    auto continue_game = std::make_unique<starfox::simulation::GameSimulation>(
        rom, symbols, "CONTINUE");
    require(continue_game->map().display_brightness() == 0U,
        "Continue screen did not begin under forced black");
    continue_game->present_frame();
    require(continue_game->map().display_brightness() == 0U
            && continue_game->map().fade_direction() == 1,
        "Continue fade-in omitted its initial black raster");
    for (std::uint32_t frame = 1U; frame <= 15U; ++frame) {
        continue_game->present_frame();
        if (continue_game->map().display_brightness() != frame) {
            std::cerr << "Continue fade-in expected " << frame << " got "
                      << static_cast<unsigned>(
                             continue_game->map().display_brightness())
                      << '\n';
            require(false,
                "Continue fade-in skipped a manual brightness value");
        }
        if (frame % 3U == 0U) {
            const auto early_input = frame == 3U
                ? starfox::input::TickInput{
                      starfox::input::a, starfox::input::a, 0U}
                : starfox::input::TickInput{};
            static_cast<void>(continue_game->tick(early_input));
            require(continue_game->flow_state()
                    == starfox::simulation::GameFlowState::continue_choice,
                "Continue screen accepted input during its fade-in");
        }
    }
    static_cast<void>(continue_game->tick(
        {starfox::input::a, starfox::input::a, 0U}));
    require(continue_game->map().display_brightness() == 15U,
        "Continue choice cut away before its fade-out");
    continue_game->present_frame();
    require(continue_game->map().display_brightness() == 15U
            && continue_game->map().fade_direction() == -1,
        "Continue fade-out omitted its duplicate full-bright raster");
    for (std::uint32_t frame = 14U; frame != 0U; --frame) {
        continue_game->present_frame();
        require(continue_game->map().display_brightness() == frame,
            "Continue fade-out skipped a manual brightness value");
    }
    continue_game->present_frame();
    require(continue_game->map().display_brightness() == 0U,
        "Continue fade-out did not reach black");
    static_cast<void>(continue_game->tick({}));
    require(continue_game->flow_state()
            != starfox::simulation::GameFlowState::continue_choice,
        "Continue choice did not transition after completing its fade-out");
}
