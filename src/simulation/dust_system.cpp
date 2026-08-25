#include "starfox/simulation/dust_system.hpp"

#include <bit>

namespace starfox::simulation {

std::uint16_t DustSystem::next_random() noexcept {
    const auto swapped = static_cast<std::uint16_t>(
        (random_ << 8U) | (random_ >> 8U));
    const auto rotated = static_cast<std::uint16_t>(
        (carry_ ? 0x8000U : 0U) | (swapped >> 1U));
    carry_ = (swapped & 1U) != 0U;
    const auto first = static_cast<std::uint32_t>(rotated) + random_;
    carry_ = first > 0xffffU;
    const auto second = static_cast<std::uint32_t>(
        static_cast<std::uint16_t>(first)) + random_ + (carry_ ? 1U : 0U);
    carry_ = second > 0xffffU;
    random_ = static_cast<std::uint16_t>(second + 1U);
    return random_;
}

void DustSystem::reset() noexcept {
    random_ = 0x19f8U;
    carry_ = false;
    for (auto& point : points_) {
        point.x = std::bit_cast<std::int16_t>(next_random());
        point.y = std::bit_cast<std::int16_t>(next_random());
        point.z = std::bit_cast<std::int16_t>(next_random());
    }
    // MINITDUST stores the initial seed in m_rand before it fills the point
    // array; MSHOWDUST begins recycling from that saved seed.
    random_ = 0x19f8U;
    carry_ = false;
}

void DustSystem::recycle(
    DustPoint& point,
    const std::array<std::int16_t, 3>& camera,
    const MatrixQ15& world_matrix) noexcept {
    const std::array<std::int16_t, 3> local{
        wrap16(arithmetic_shift_right(
            std::bit_cast<std::int16_t>(next_random()), 5U)),
        wrap16(arithmetic_shift_right(
            std::bit_cast<std::int16_t>(next_random()), 5U)),
        static_cast<std::int16_t>((next_random() >> 5U) + 512U),
    };
    const auto world_offset = transform_q15(transpose_q15(world_matrix), local);
    point.x = add16(camera[0], world_offset[0]);
    point.y = add16(camera[1], world_offset[1]);
    point.z = add16(camera[2], world_offset[2]);
}

void DustSystem::tick(
    const std::array<std::int16_t, 3>& camera,
    const MatrixQ15& world_matrix,
    bool enabled) noexcept {
    if (!enabled) return;
    for (auto& point : points_) {
        const auto x = subtract16(point.x, camera[0]);
        const auto y = subtract16(point.y, camera[1]);
        const auto z = subtract16(point.z, camera[2]);
        if (x >= 2'048 || x < -2'048 || y >= 2'048 || y < -2'048
            || z >= 2'560 || z < -2'560) {
            recycle(point, camera, world_matrix);
        }
    }
}

} // namespace starfox::simulation
