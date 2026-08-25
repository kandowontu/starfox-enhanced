#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/simulation/object_pool.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace starfox::simulation {

inline constexpr std::size_t kMaximumParticles = 300;

struct ParticleState {
    std::uint8_t life{};
    std::uint8_t flags{};
    std::uint8_t colour{};
    std::int8_t velocity_x{};
    std::int8_t velocity_y{};
    std::int8_t velocity_z{};
    std::int16_t x{};
    std::int16_t y{};
    std::int16_t z{};
    std::int16_t previous_x{};
    std::int16_t previous_y{};
    std::int16_t previous_z{};
    ObjectHandle owner{};
};

// Native equivalent of MPART.MC's persistent 300-entry Super FX pool.
class ParticleSystem {
public:
    ParticleSystem(
        const assets::RomImage& rom,
        const assets::SymbolMap& symbols);
    ParticleSystem(
        const assets::RomImage& rom,
        std::uint32_t fade_table,
        std::uint32_t circle_table) noexcept;

    void reset() noexcept;
    void tick(const ObjectPool& objects, bool enabled);

    [[nodiscard]] const std::array<ParticleState, kMaximumParticles>& particles()
        const noexcept { return particles_; }
    [[nodiscard]] std::size_t active_count() const noexcept;

private:
    void make_particles(
        ObjectHandle owner,
        std::uint8_t type,
        std::uint8_t life,
        std::uint8_t count);
    void show_particles(ObjectHandle owner);
    void update_particles();
    [[nodiscard]] std::uint16_t next_random() noexcept;
    [[nodiscard]] std::int8_t circle(std::size_t index) const;

    const assets::RomImage* rom_{};
    std::uint32_t fade_table_{};
    std::uint32_t circle_table_{};
    std::array<ParticleState, kMaximumParticles> particles_{};
    std::uint16_t random_{0x1234U};
};

} // namespace starfox::simulation
