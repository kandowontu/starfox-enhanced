#pragma once

#include "starfox/assets/rom.hpp"

#include <cstdint>
#include <vector>

namespace starfox::assets {

struct DecrunchResult {
    std::vector<std::uint8_t> bytes;
    // First byte consumed from the LoROM image, expressed as an SNES address.
    std::uint32_t compressed_begin{};
};

// Decodes the backwards control stream used by MDECRU.MC. end_address is the
// assembler label immediately following a .CCR/.PCR archive.
[[nodiscard]] DecrunchResult decrunch_reverse(
    const RomImage& rom, std::uint32_t end_address);

} // namespace starfox::assets
