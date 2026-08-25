#include "starfox/simulation/particle_system.hpp"

#include "starfox/simulation/math.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace starfox::simulation {
namespace {

constexpr std::uint8_t kObjectAlive = 0x01U;
constexpr std::uint8_t kGravity = 0x02U;
constexpr std::uint8_t kFriction = 0x08U;
constexpr std::uint8_t kFadeOut = 0x10U;

std::uint32_t rom_symbol(
    const assets::SymbolMap& symbols, const std::string& name) {
    for (const auto address : symbols.find(name)) {
        if ((address & 0xffffU) >= 0x8000U
            && ((address >> 16U) & 0xffU) < 0x7eU) {
            return address;
        }
    }
    throw std::runtime_error{"missing particle ROM symbol: " + name};
}

std::uint8_t byte(std::int8_t value) noexcept {
    return std::bit_cast<std::uint8_t>(value);
}

std::int8_t signed_byte(std::uint8_t value) noexcept {
    return std::bit_cast<std::int8_t>(value);
}

std::int8_t div2(std::int8_t value) noexcept {
    if (value == -1) return 0;
    return static_cast<std::int8_t>(arithmetic_shift_right(value, 1));
}

} // namespace

ParticleSystem::ParticleSystem(
    const assets::RomImage& rom,
    const assets::SymbolMap& symbols)
    : ParticleSystem(rom,
          rom_symbol(symbols, "PARTFADETAB"),
          rom_symbol(symbols, "PARTICLE_CIRCLE")) {}

ParticleSystem::ParticleSystem(
    const assets::RomImage& rom,
    std::uint32_t fade_table,
    std::uint32_t circle_table) noexcept
    : rom_(&rom), fade_table_(fade_table), circle_table_(circle_table) {}

void ParticleSystem::reset() noexcept {
    particles_.fill({});
    random_ = 0x1234U;
}

std::size_t ParticleSystem::active_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        particles_.begin(), particles_.end(),
        [](const auto& particle) { return particle.life != 0U; }));
}

std::uint16_t ParticleSystem::next_random() noexcept {
    // MPART.MC: SWAP, ROR, ADD rand, ADC rand. SWAP preserves carry; the
    // preceding pool operations leave it clear at every call site used by
    // the shipped particle initializers.
    const auto swapped = static_cast<std::uint16_t>(
        (random_ << 8U) | (random_ >> 8U));
    const auto rotated = static_cast<std::uint16_t>(swapped >> 1U);
    const auto first = static_cast<std::uint32_t>(rotated) + random_;
    const auto carry = first > 0xffffU ? 1U : 0U;
    random_ = static_cast<std::uint16_t>(first + random_ + carry);
    return random_;
}

std::int8_t ParticleSystem::circle(std::size_t index) const {
    return std::bit_cast<std::int8_t>(rom_->read8(
        circle_table_ + static_cast<std::uint32_t>(index)));
}

void ParticleSystem::make_particles(
    ObjectHandle owner,
    std::uint8_t type,
    std::uint8_t life,
    std::uint8_t count) {
    auto remaining = count;
    for (std::size_t slot = 0;
         slot < particles_.size() && remaining != 0U; ++slot) {
        auto& particle = particles_[slot];
        if (particle.life != 0U) continue;
        particle = {};
        particle.owner = owner;
        particle.life = life;
        const auto alternating_colour = ((particles_.size() - slot) & 1U) != 0U
            ? std::uint8_t{14} : std::uint8_t{4};
        const auto random_component = [this](std::uint16_t mask, int bias) {
            return signed_byte(static_cast<std::uint8_t>(
                (next_random() & mask) - bias));
        };

        switch (type & 7U) {
        case 1: {
            particle.flags = 0U;
            do {
                particle.velocity_x = random_component(63U, 31);
                particle.velocity_y = random_component(63U, 31);
                particle.velocity_z = random_component(63U, 31);
            } while (std::abs(static_cast<int>(particle.velocity_x))
                    + std::abs(static_cast<int>(particle.velocity_y))
                    + std::abs(static_cast<int>(particle.velocity_z)) > 50);
            particle.colour = alternating_colour;
            break;
        }
        case 2:
        case 3: {
            particle.flags = type == 2U ? kGravity : 0U;
            particle.velocity_x = random_component(15U, 7);
            particle.velocity_y = signed_byte(static_cast<std::uint8_t>(
                (next_random() & 7U) + (type == 2U ? -40 : 50)));
            particle.velocity_z = random_component(15U, 7);
            auto colour = static_cast<std::uint8_t>(next_random() & 3U);
            if (colour == 0U) colour = 1U;
            particle.colour = static_cast<std::uint8_t>(colour + 1U);
            break;
        }
        case 4:
        case 6:
        case 7: {
            particle.life = static_cast<std::uint8_t>(
                life + (remaining & 15U) - 7U);
            particle.flags = kFadeOut;
            const auto circle_index = static_cast<std::size_t>(remaining - 1U) * 2U;
            auto vx = circle(circle_index);
            auto vy = circle(circle_index + 1U);
            if (type == 6U) {
                vx = div2(vx);
                vy = div2(vy);
            }
            particle.velocity_x = vx;
            particle.velocity_y = vy;
            particle.velocity_z = type == 6U
                ? random_component(31U, 15)
                : random_component(63U, 31);
            particle.x = particle.velocity_x;
            particle.y = particle.velocity_y;
            particle.z = particle.velocity_z;
            particle.previous_x = particle.x;
            particle.previous_y = particle.y;
            particle.previous_z = particle.z;
            particle.colour = alternating_colour;
            break;
        }
        case 5: {
            particle.flags = 0U;
            do {
                particle.velocity_x = random_component(31U, 15);
                particle.velocity_y = random_component(31U, 15);
                particle.velocity_z = random_component(31U, 15);
            } while (std::abs(static_cast<int>(particle.velocity_x))
                    + std::abs(static_cast<int>(particle.velocity_y))
                    + std::abs(static_cast<int>(particle.velocity_z)) > 25);
            particle.colour = alternating_colour;
            break;
        }
        default:
            particle.life = 0U;
            particle.owner = 0U;
            break;
        }
        --remaining;
    }
}

void ParticleSystem::show_particles(ObjectHandle owner) {
    for (auto& particle : particles_) {
        if (particle.life == 0U || particle.owner != owner) continue;
        particle.previous_x = particle.x;
        particle.previous_y = particle.y;
        particle.previous_z = particle.z;
        particle.x = add16(particle.x, particle.velocity_x);
        particle.y = add16(particle.y, particle.velocity_y);
        particle.z = add16(particle.z, particle.velocity_z);
        particle.flags |= kObjectAlive;
        if ((particle.flags & kGravity) != 0U) {
            particle.velocity_y = signed_byte(static_cast<std::uint8_t>(
                byte(particle.velocity_y) + 2U));
        }
        if ((particle.flags & kFriction) != 0U) {
            particle.velocity_x = signed_byte(static_cast<std::uint8_t>(
                byte(particle.velocity_x) - byte(div2(div2(particle.velocity_x)))));
            particle.velocity_y = signed_byte(static_cast<std::uint8_t>(
                byte(particle.velocity_y) - byte(div2(div2(particle.velocity_y)))));
            particle.velocity_z = signed_byte(static_cast<std::uint8_t>(
                byte(particle.velocity_z) - byte(div2(div2(particle.velocity_z)))));
        }
    }
}

void ParticleSystem::update_particles() {
    for (auto& particle : particles_) {
        if (particle.life == 0U) {
            particle.owner = 0U;
            continue;
        }
        --particle.life;
        if ((particle.flags & kFadeOut) != 0U && particle.life < 32U) {
            particle.colour = rom_->read8(fade_table_ + particle.life);
        }
        if ((particle.flags & kObjectAlive) == 0U) {
            particle.life = 0U;
            particle.owner = 0U;
        } else {
            particle.flags &= static_cast<std::uint8_t>(~kObjectAlive);
        }
    }
}

void ParticleSystem::tick(const ObjectPool& objects, bool enabled) {
    if (!enabled) return;
    for (const auto owner : objects.active_handles()) {
        const auto& object = objects.at(owner);
        if ((object.strategy_flags[3] & 0x08U) != 0U
            || (object.strategy_flags[0] & 0x10U) == 0U) {
            continue;
        }
        const auto type = static_cast<std::uint8_t>(object.scratch_bytes[2]) & 7U;
        if (type != 0U) {
            make_particles(owner, type,
                static_cast<std::uint8_t>(object.scratch_bytes[1]),
                static_cast<std::uint8_t>(object.scratch_bytes[0]));
        }
        show_particles(owner);
    }
    update_particles();
}

} // namespace starfox::simulation
