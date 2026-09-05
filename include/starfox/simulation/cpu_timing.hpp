#pragma once
#include <cstdint>

namespace starfox::simulation {

// SNES CPU bus access length in master clocks, excluding DMA, WRAM refresh
// and coprocessor waits. FastROM affects the high-bank cartridge regions.
[[nodiscard]] constexpr std::uint8_t cpu_access_master_clocks(
    std::uint32_t address, bool fast_rom) noexcept {
    const auto bank = (address >> 16U) & 0xffU;
    const auto offset = address & 0xffffU;
    if ((bank & 0x40U) != 0U || offset >= 0x8000U)
        return bank >= 0x80U && fast_rom ? 6U : 8U;
    if (offset < 0x2000U || offset >= 0x6000U) return 8U;
    if (offset >= 0x4000U && offset < 0x4200U) return 12U;
    return 6U;
}

} // namespace starfox::simulation
