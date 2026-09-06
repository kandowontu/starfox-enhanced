#include "starfox/simulation/wdc65816.hpp"
#include <algorithm>
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

void check_timed_native_reads(bool resumable) {
    using namespace starfox;
    std::vector<std::uint8_t> bytes(0x8000);
    const std::vector<std::uint8_t> code{
        0xa5,0x00, 0x8d,0x00,0x10, 0xea,
        0xa5,0x00, 0x8d,0x01,0x10, 0xea,
        0xa5,0x00, 0x8d,0x02,0x10,
        0xa9,0x01, 0x8d,0x56,0x00, 0xea,
        0xa5,0x00, 0x8d,0x03,0x10,
        static_cast<std::uint8_t>(resumable ? 0x6b : 0x60)};
    std::copy(code.begin(), code.end(), bytes.begin());
    const assets::RomImage rom{std::move(bytes)};
    const auto symbols = assets::SymbolMap::parse(
        "BITMAP1 $004000\nVMAP1 $000050\nVMAP2 $000052\n"
        "TRANSBMP1 $000054\nNOIRQBIT3 $000056\nSPRITEBLK $001500\n");
    const auto run = [&](simulation::Wdc65816& cpu, bool service) {
        cpu.write8(0, 2);
        simulation::Wdc65816Registers registers;
        registers.status = 0x24;
        registers.x = 0x1234;
        registers.y = 0x5678;
        if (!resumable) cpu.call_near(0x008000, registers, 100, service);
        else {
            const std::array stops{0x008006U, 0x00800cU};
            auto result = cpu.begin_long_task(0x008000, registers, stops, 100, service);
            if (result.returned || result.stop_address != stops[0])
                throw std::runtime_error{"Timed task missed its first pause"};
            result = cpu.resume_task(registers, stops, 100, service);
            if (result.returned || result.stop_address != stops[1])
                throw std::runtime_error{"Timed task missed its second pause"};
            result = cpu.resume_task(registers, {}, 100, service);
            if (!result.returned) throw std::runtime_error{"Timed task did not return"};
        }
        if (registers.x != 0x1234 || registers.y != 0x5678 || registers.stack != 0x1ff)
            throw std::runtime_error{"Boundary callbacks damaged native registers or stack"};
        return registers.a;
    };
    simulation::Wdc65816 baseline{rom, &symbols}, observed{rom, &symbols};
    const auto expected_a = run(baseline, false);
    for (unsigned i = 0; i < 4; ++i)
        if (baseline.read8(0x1000 + i) != 2)
            throw std::runtime_error{"Unscheduled native control did not retain transfer state"};
    std::vector<std::uint64_t> boundaries;
    observed.set_instruction_boundary_callback([&](auto clocks) { boundaries.push_back(clocks); });
    if (run(observed, false) != expected_a
        || observed.executed_master_clocks() != baseline.executed_master_clocks()
        || boundaries.empty() || boundaries.front() != 0
        || boundaries.back() != observed.executed_master_clocks()
        || !std::is_sorted(boundaries.begin(), boundaries.end()))
        throw std::runtime_error{"Observation changed native timing or omitted its final boundary"};
    simulation::Wdc65816 automatic{rom, &symbols};
    automatic.set_instruction_boundary_callback([](std::uint64_t) {});
    run(automatic, true);
    for (unsigned i = 0; i < 4; ++i)
        if (automatic.read8(0x1000 + i) != 0)
            throw std::runtime_error{"Passive observation took ownership of synchronous DMA"};
    simulation::Wdc65816 scheduled{rom, &symbols};
    unsigned phase{}, gated{};
    scheduled.set_instruction_boundary_callback([&](std::uint64_t clocks) {
        // Slow-ROM LDA dp (24), STA abs (32), NOP (14) reach the
        // successive reads at 70 and 140 clocks. The gate opens at 244.
        if (phase == 0 && clocks >= 70) {
            if (!scheduled.advance_gameplay_bitmap_dma_phase()) throw std::runtime_error{"First DMA phase failed"};
            ++phase;
        }
        if (phase == 1 && clocks >= 140) {
            if (!scheduled.advance_gameplay_bitmap_dma_phase()) throw std::runtime_error{"Second DMA phase failed"};
            ++phase;
        }
        if (phase == 2 && clocks >= 210) {
            if (scheduled.advance_gameplay_bitmap_dma_phase()) ++phase;
            else ++gated;
        }
    }, true);
    run(scheduled, true);
    const std::array expected{2U, 4U, 6U, 0U};
    for (unsigned i = 0; i < expected.size(); ++i)
        if (scheduled.read8(0x1000 + i) != expected[i])
            throw std::runtime_error{"Native read did not observe its scheduled DMA phase"};
    if (phase != 3 || gated == 0
        || scheduled.executed_master_clocks() != baseline.executed_master_clocks())
        throw std::runtime_error{"Scheduling skipped the completion gate or changed instruction clocks"};
    scheduled.set_instruction_boundary_callback({});
    run(scheduled, true);
    if (scheduled.read8(0x1000) != 0)
        throw std::runtime_error{"Clearing the scheduler did not restore synchronous completion"};
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
    check_timed_native_reads(false);
    check_timed_native_reads(true);
    std::cout << "Gameplay DMA wrapping/gates and timed native reads across calls/resumes pass\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
