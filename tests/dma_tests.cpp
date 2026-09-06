#include "starfox/simulation/wdc65816.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

using starfox::simulation::Wdc65816;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

int main() try {
    const starfox::assets::RomImage rom{std::vector<std::uint8_t>(0x8000)};
    // A1T is a 16-bit counter; A1B is a separate, unchanged register.
    // Distinct adjacent-bank bytes catch overflow even though only A1T is
    // written back by the old bridge. Fixed addressing wins over decrement.
    for (unsigned channel = 0; channel < 8; ++channel) {
        for (const unsigned control : {0U, 0x10U, 0x08U, 0x18U}) {
            for (const unsigned start : {0U, 0xfffeU, 0xffffU}) {
                Wdc65816 cpu{rom};
                cpu.write8(0x7dffff, 0x91);
                cpu.write8(0x7f0000, 0x92);
                cpu.write8(0x7f0001, 0x93);
                cpu.write8(0x7e0000, 0x21);
                cpu.write8(0x7e0001, 0x32);
                cpu.write8(0x7e0002, 0x43);
                cpu.write8(0x7efffd, 0x54);
                cpu.write8(0x7efffe, 0x65);
                cpu.write8(0x7effff, 0x76);
                std::array<std::uint8_t, 3> expected{};
                auto offset = static_cast<std::uint16_t>(start);
                for (auto& value : expected) {
                    value = cpu.read8(0x7e0000U | offset);
                    if (!(control & 8)) offset = static_cast<std::uint16_t>(
                        offset + ((control & 0x10) ? -1 : 1));
                }
                cpu.write8(0x2115, 0); // increment VRAM after low-byte writes
                cpu.write16(0x2116, 0);
                const auto base = 0x4300U + channel * 16U;
                cpu.write8(base, static_cast<std::uint8_t>(control));
                cpu.write8(base + 1, 0x18);
                cpu.write16(base + 2, static_cast<std::uint16_t>(start));
                cpu.write8(base + 4, 0x7e);
                cpu.write16(base + 5, 3);
                cpu.write8(0x420b, static_cast<std::uint8_t>(1U << channel));
                for (unsigned i = 0; i < expected.size(); ++i)
                    require(cpu.ppu_state().vram[i * 2] == expected[i],
                        "DMA crossed the A-bus bank or ignored fixed addressing");
                require(cpu.read16(base + 2) == offset && cpu.read8(base + 4) == 0x7e,
                    "DMA final A1T/A1B differ");
                require(cpu.read16(base + 5) == 0, "DMA count did not expire");
            }
        }
    }
    // BBAD plus the mode offset wraps at 8 bits, keeping every write in $21xx.
    // Observe the wrapped writes through real PPU register state.
    for (const unsigned mode : {1U, 3U, 4U, 5U, 7U}) {
        Wdc65816 cpu{rom};
        for (unsigned i = 0; i < 4; ++i) cpu.write8(0x7e1000 + i, 0x11 + i);
        cpu.write8(0x4300, static_cast<std::uint8_t>(mode));
        cpu.write8(0x4301, 0xff);
        cpu.write16(0x4302, 0x1000);
        cpu.write8(0x4304, 0x7e);
        cpu.write16(0x4305, 4);
        cpu.write8(0x420b, 1);
        require(cpu.read8(0x2100) == (mode == 4 ? 0x12 : 0x14),
            "DMA B-bus target did not wrap within $21xx");
        if (mode == 4)
            require(cpu.read8(0x2101) == 0x13 && cpu.read8(0x2102) == 0x14,
                "DMA mode 4 lost wrapped register writes");
    }
    std::cout << "96 DMA bank/fixed-address cases and 5 B-bus wrapping cases pass\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
