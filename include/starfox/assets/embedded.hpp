#pragma once

#include <cstdint>
#include <span>

namespace starfox::assets {

// Generated non-Windows builds expose the same numbered payloads used by the
// Windows resource table. The retail ROM is never included: only source-built
// BPS deltas, symbol maps, and regional canonicalization deltas are embedded.
[[nodiscard]] std::span<const std::uint8_t> embedded_asset(int identifier);

} // namespace starfox::assets
