#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace starfox::assets {

struct RuntimeBundlePayload {
    std::vector<std::uint8_t> original_rom;
    std::string original_symbols;
    std::vector<std::uint8_t> starfox_ex_rom;
    std::string starfox_ex_symbols;
};

// Starfox-Assets.BIN is generated locally from the user's retail v1.2 ROM.
// The manifest binds it to the precise embedded patches/symbol tables in the
// executable, so stale companions cannot silently mix incompatible code/data.
[[nodiscard]] std::vector<std::uint8_t> encode_runtime_bundle(
    const RuntimeBundlePayload& payload,
    std::uint32_t manifest);

[[nodiscard]] RuntimeBundlePayload decode_runtime_bundle(
    std::span<const std::uint8_t> bytes,
    std::uint32_t expected_manifest);

} // namespace starfox::assets
