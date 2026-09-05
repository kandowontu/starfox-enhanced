#include "starfox/simulation/dust_system.hpp"

#include <bit>
#include <span>

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
    // MSHOWDUST's pointer SUB #6 sets carry before the first MRAND. Each
    // coordinate then executes six ASRs (the last has TO rx/ry), and its
    // carry feeds the following MRAND. These are part of the random stream.
    carry_ = true;
    const auto random_x = next_random();
    carry_ = (random_x & 0x20U) != 0U;
    const auto random_y = next_random();
    carry_ = (random_y & 0x20U) != 0U;
    const auto random_z = next_random();
    const std::array<std::int16_t, 3> local{
        wrap16(arithmetic_shift_right(std::bit_cast<std::int16_t>(random_x), 6U)),
        wrap16(arithmetic_shift_right(std::bit_cast<std::int16_t>(random_y), 6U)),
        static_cast<std::int16_t>((random_z >> 5U) + 512U),
    };
    const auto world_offset = transform_q15(transpose_q15(world_matrix), local);
    point.x = add16(camera[0], world_offset[0]);
    point.y = add16(camera[1], world_offset[1]);
    point.z = add16(camera[2], world_offset[2]);
}

void DustSystem::tick(
    const std::array<std::int16_t, 3>& camera,
    const MatrixQ15& world_matrix,
    bool enabled,
    std::size_t active_count) noexcept {
    if (!enabled) return;
    active_count = std::min(active_count, points_.size());
    for (auto& point : std::span{points_}.first(active_count)) {
        for (;;) {
            const auto x = subtract16(point.x, camera[0]);
            const auto y = subtract16(point.y, camera[1]);
            const auto z = subtract16(point.z, camera[2]);
            if (x < 2'048 && x >= -2'048 && y < 2'048 && y >= -2'048
                && z < 2'560 && z >= -2'560
                && transform_q15(world_matrix, {x, y, z})[2] >= 0) break;
            // The cartridge retries this same point after recycling, including
            // points still outside the world box or behind a rotated camera.
            recycle(point, camera, world_matrix);
        }
    }
}

} // namespace starfox::simulation
