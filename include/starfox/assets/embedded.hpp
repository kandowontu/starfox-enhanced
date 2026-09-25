#pragma once

#include <cstdint>
#include <span>

namespace starfox::assets {

// Generated non-Windows builds expose the same numbered payloads used by the
// Windows resource table. The retail ROM is never included: only source-built
// BPS deltas, symbol maps, regional canonicalization deltas and authored
// enhanced-backdrop artwork are embedded.
[[nodiscard]] std::span<const std::uint8_t> embedded_asset(int identifier);

} // namespace starfox::assets
