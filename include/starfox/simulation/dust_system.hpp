#pragma once

#include "starfox/simulation/math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace starfox::simulation {

inline constexpr std::size_t kMaximumDustPoints = 120U;

struct DustPoint {
    std::int16_t x{};
    std::int16_t y{};
    std::int16_t z{};
};

// Persistent native translation of MGDOTS.MC's star/dust point cloud. Point
// recycling remains on the source 20 Hz boundary; presentation only projects
// these fixed-point world positions.
class DustSystem {
public:
    DustSystem() noexcept { reset(); }

    void reset() noexcept;
    void tick(
        const std::array<std::int16_t, 3>& camera,
        const MatrixQ15& world_matrix,
        bool enabled) noexcept;

    [[nodiscard]] const std::array<DustPoint, kMaximumDustPoints>& points()
        const noexcept { return points_; }

private:
    [[nodiscard]] std::uint16_t next_random() noexcept;
    void recycle(
        DustPoint& point,
        const std::array<std::int16_t, 3>& camera,
        const MatrixQ15& world_matrix) noexcept;

    std::array<DustPoint, kMaximumDustPoints> points_{};
    std::uint16_t random_{0x19f8U};
    bool carry_{};
};

} // namespace starfox::simulation
