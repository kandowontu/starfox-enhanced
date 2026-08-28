#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace starfox::assets {

// Apply a BPS1 patch and validate its source, target, and patch CRC32 values.
// The returned image is the exact target byte stream described by the patch.
[[nodiscard]] std::vector<std::uint8_t> apply_bps_patch(
    std::span<const std::uint8_t> source,
    std::span<const std::uint8_t> patch);

[[nodiscard]] std::uint32_t crc32(
    std::span<const std::uint8_t> bytes) noexcept;

} // namespace starfox::assets
