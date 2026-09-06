#include "starfox/simulation/wdc65816.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

void check_phases(unsigned bitmap, unsigned page) {
    using namespace starfox;
    const assets::RomImage rom{std::vector<std::uint8_t>(0x8000)};
    std::ostringstream symbol_text;
    symbol_text << "BITMAP1 $" << std::hex << bitmap
        << "\nVMAP1 $000050\nVMAP2 $000052\nTRANSBMP1 $000054\n"
           "NOIRQBIT3 $000056\nSPRITEBLK $001500\n";
    const auto symbols = assets::SymbolMap::parse(symbol_text.str());
    simulation::Wdc65816 cpu{rom, &symbols};
    const std::vector<std::uint8_t> initial_vram(65536, 0xa5);
    cpu.write_vram(0, initial_vram);
    for (unsigned i = 0; i < 544; ++i) cpu.write8(0x7e3000 + i, 0xcc);
    cpu.upload_oam(0x7e3000, 544);
    for (unsigned i = 0; i < 328; ++i) cpu.write8(0x7e1500 + i, 1 + i % 251);
    for (unsigned i = 0; i < 21504; ++i)
        cpu.write8(0x700000U | ((bitmap + i) & 65535U), 1 + (i * 17 + 19) % 251);
    cpu.write16(0x50, page);
    cpu.write16(0x52, page ^ 0x4000);
    cpu.write8(0, 2);
    auto expected_vram = cpu.ppu_state().vram;
    const auto initial_oam = cpu.ppu_state().oam;
    for (unsigned phase = 0; phase < 2; ++phase) {
        for (unsigned i = phase * 10752; i < (phase + 1) * 10752; ++i)
            expected_vram[(page * 2 + i) & 65535U] = 1 + (i * 17 + 19) % 251;
        if (!cpu.advance_gameplay_bitmap_dma_phase()
            || cpu.read8(0) != 4 + phase * 2 || cpu.read8(0x54) != phase + 1
            || cpu.ppu_state().vram != expected_vram || cpu.ppu_state().oam != initial_oam
            || cpu.read16(0x50) != page || cpu.read16(0x52) != (page ^ 0x4000))
            throw std::runtime_error{"Bitmap DMA published incorrect partial state"};
    }
    if (cpu.advance_gameplay_bitmap_dma_phase() || cpu.read8(0) != 6
        || cpu.ppu_state().oam != initial_oam || cpu.read16(0x50) != page)
        throw std::runtime_error{"Bitmap DMA ignored the source completion gate"};
    cpu.write8(0x56, 1);
    if (!cpu.advance_gameplay_bitmap_dma_phase() || cpu.read8(0) != 0
        || cpu.read16(0x50) != (page ^ 0x4000) || cpu.read16(0x52) != page
        || cpu.ppu_state().vram != expected_vram)
        throw std::runtime_error{"Bitmap DMA completion changed bitmap or failed to swap pages"};
    for (unsigned i = 0; i < 544; ++i)
        if (cpu.ppu_state().oam[i] != (i < 328 ? 1 + i % 251 : 0xcc))
            throw std::runtime_error{"Bitmap DMA completion uploaded an incorrect OAM range"};
    if (cpu.advance_gameplay_bitmap_dma_phase())
        throw std::runtime_error{"Idle bitmap DMA advanced again"};
}

int main() try {
    using namespace starfox;
    const assets::RomImage rom{std::vector<std::uint8_t>(0x8000)};
    const auto symbols = assets::SymbolMap::parse(
        "BITMAP1 $004000\nVMAP1 $000050\nVMAP2 $000052\n"
        "TRANSBMP1 $000054\nNOIRQBIT3 $000056\nSPRITEBLK $001500\n");
    simulation::Wdc65816 cpu{rom, &symbols};
    for (unsigned i = 0; i < 328; ++i) cpu.write8(0x7e1500 + i, 1 + i % 251);
    cpu.write16(0x50, 0x4000);
    cpu.write16(0x52, 0x1000);
    cpu.write8(0x7e0200, 0x60); // harmless RTS services the pending transfer
    cpu.write8(0, 2);
    simulation::Wdc65816Registers registers;
    static_cast<void>(cpu.call_near(0x7e0200, registers, 100, true));
    for (unsigned i = 0; i < 328; ++i)
        if (cpu.ppu_state().oam[i] != 1 + i % 251)
            throw std::runtime_error{"Gameplay OAM DMA omitted source byte " + std::to_string(i)};
    for (unsigned bitmap : {0x4000U, 0xd800U})
        for (unsigned page : {0U, 0x4000U, 0x7ff0U}) check_phases(bitmap, page);
    std::cout << "Gameplay OAM length and six gated/wrapping bitmap DMA sequences pass\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
