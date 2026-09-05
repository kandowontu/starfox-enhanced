#include "ares_gsu.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
template<class Exception, class F> void rejects(F&& action, const char* message) {
    bool caught = false;
    try { action(); } catch (const Exception&) { caught = true; }
    require(caught, message);
}
}

int main() { try {
    // Hand-counted microprograms test the adapter's pipeline, cold-cache and
    // clock accounting. They do not assert that a complete SNES frame costs
    // the sum of its isolated GSU launches.
    std::vector<std::uint8_t> rom(0x8000, 0x01), ram(0x10000);
    starfox::reference::AresGsu gsu(rom, ram);
    const auto check = [&](std::initializer_list<std::uint8_t> program,
                           unsigned instructions, unsigned slow, unsigned fast) {
        std::fill(rom.begin(), rom.end(), 0x01);
        std::copy(program.begin(), program.end(), rom.begin());
        for (const auto speed : {false, true}) {
            for (unsigned repeat = 0; repeat < 2; ++repeat) {
                const auto result = gsu.run(0x8000, 0, 0, 1000, speed);
                require(result.instructions == instructions, "Pipeline instruction count differs");
                require(result.master_clocks == (speed ? fast : slow), "Master-clock count differs");
            }
        }
    };
    // A cold run begins with one pipelined NOP. Noncache fetches cost 6/5
    // oscillator clocks. CACHE fills 16 bytes at 6/5 each, then hits cost 2/1.
    check({0x00}, 2, 12, 10);                         // STOP
    check({0x01, 0x00}, 3, 18, 15);                   // NOP; STOP
    check({0x02, 0x01, 0x00}, 4, 110, 91);            // CACHE; NOP; STOP
    check({0x02, 0x01, 0x01, 0x00}, 5, 112, 92);      // another cache hit
    // FMULT adds 7*2/7*1 clocks with CFGR.MS0 clear.
    check({0x9f, 0x00}, 3, 32, 22);
    check({0x02, 0x9f, 0x00}, 4, 124, 98);

    // IWT R0,$1234; IWT R1,$1000; STW (R1); NOP; STOP. Validate
    // write-buffer completion and caller-owned RAM, including a second run.
    const std::uint8_t store[]{0xf0, 0x34, 0x12, 0xf1, 0x00, 0x10, 0x31, 0x01, 0x00};
    std::copy(std::begin(store), std::end(store), rom.begin());
    static_cast<void>(gsu.run(0x8000));
    require(ram[0x1000] == 0x34 && ram[0x1001] == 0x12, "Buffered SRAM store differs");
    ram[0x4321] = 0xab;
    static_cast<void>(gsu.run(0x8000));
    require(ram[0x4321] == 0xab, "Cold GSU reset erased caller RAM");
    // Under CACHE, STOP may precede the final high-byte SRAM write. The
    // adapter must finish that pending transfer before returning to a CPU.
    const std::uint8_t pending[]{0x02, 0xf0, 0xcd, 0xab, 0xf1, 0x00, 0x10, 0x31, 0x00};
    std::copy(std::begin(pending), std::end(pending), rom.begin());
    static_cast<void>(gsu.run(0x8000));
    require(ram[0x1000] == 0xcd && ram[0x1001] == 0xab, "STOP lost the pending SRAM high byte");
    rejects<std::runtime_error>([&] { static_cast<void>(gsu.run(0x8000, 0, 0, 1)); },
        "Instruction budget did not stop execution");
    // A timed-out run must not poison the next isolated call.
    rom[0] = 0x00;
    require(gsu.run(0x8000).master_clocks == 12, "Reset after timeout differs");
    rejects<std::invalid_argument>([&] { static_cast<void>(gsu.run(0x600000)); },
        "Unsupported entry bank accepted");
    rejects<std::invalid_argument>([&] { static_cast<void>(gsu.run(0x8000, 0x10000)); },
        "Truncated stack accepted");
    rejects<std::invalid_argument>([&] {
        starfox::reference::AresGsu invalid{std::span{rom}.first(0x7000), ram};
    }, "Non-power-of-two ROM accepted");
    rejects<std::invalid_argument>([&] {
        starfox::reference::AresGsu invalid{rom, std::span{ram}.first(0x8000)};
    }, "Unsupported RAM accepted");
    std::cout << "Ares isolated GSU: pipeline/cache/multiply clocks, SRAM, bounds and reset checks passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
} }
