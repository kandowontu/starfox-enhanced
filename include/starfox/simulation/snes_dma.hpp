#pragma once

#include "starfox/simulation/snes_timeline.hpp"
#include <array>

namespace starfox::simulation {

// General DMA bus ownership. HDMA arbitration is a separate, unfinished part
// of the machine scheduler; this engine does not claim to model HDMA edges.
class SnesDma {
public:
    std::array<std::uint8_t, 0x80> registers{};

    void request(std::uint8_t mask) noexcept {
        enabled_ = mask;
        if (mask) pending_ = true;
    }
    [[nodiscard]] bool requested() const noexcept { return enabled_ != 0; }

    static bool valid_a(std::uint32_t address) noexcept {
        return (address & 0x40ff00U) != 0x2100U
            && (address & 0x40fe00U) != 0x4000U
            && (address & 0x40ffe0U) != 0x4200U
            && (address & 0x40ff80U) != 0x4300U;
    }
    static bool valid_b(std::uint32_t address_a, std::uint32_t address_b) noexcept {
        return address_b != 0x2180U
            || ((address_a & 0xfe0000U) != 0x7e0000U && (address_a & 0x40e000U) != 0U);
    }

    // Called before each complete CPU read/write/idle, not between the two
    // clock steps of a read. A request lets one CPU cycle finish before DMA.
    template<class Read, class Write>
    void edge(std::uint32_t cpu_clocks, SnesCpuTimeline& clock, Read&& read, Write&& write) {
        if (!cpu_clocks || (cpu_clocks & 1U))
            throw std::invalid_argument{"DMA requires a positive even CPU cycle"};
        if (active_ && pending_) {
            pending_ = false;
            if (enabled_) {
                std::uint64_t dma_clocks{};
                const auto step = [&](std::uint32_t clocks) {
                    dma_clocks += clocks;
                    clock.step(clocks, SnesClockWork::dma);
                };
                step(8U - static_cast<std::uint32_t>(clock.raster().elapsed() & 7U));
                step(8U); // global setup
                for (unsigned channel = 0; channel < 8; ++channel) {
                    if (!(enabled_ & (1U << channel))) continue;
                    const auto base = channel * 16U;
                    const auto parameters = registers[base];
                    const auto target = registers[base + 1U];
                    auto source = word(base + 2U);
                    auto remaining = word(base + 5U);
                    const auto bank = std::uint32_t(registers[base + 4U]) << 16U;
                    step(8U); // per-channel setup
                    unsigned index{};
                    do {
                        unsigned offset{};
                        switch (parameters & 7U) {
                        case 1: case 5: offset = index & 1U; break;
                        case 3: case 7: offset = (index >> 1U) & 1U; break;
                        case 4: offset = index & 3U; break;
                        default: break;
                        }
                        ++index;
                        const auto address_a = bank | source;
                        const auto address_b = 0x2100U | std::uint8_t(target + offset);
                        const bool b_allowed = valid_b(address_a, address_b);
                        const bool reverse = (parameters & 0x80U) != 0;
                        step(4U);
                        const auto value = reverse
                            ? (b_allowed ? std::uint8_t(read(address_b)) : std::uint8_t{})
                            : (valid_a(address_a) ? std::uint8_t(read(address_a)) : std::uint8_t{});
                        step(4U);
                        if (reverse) {
                            if (valid_a(address_a)) write(address_a, value);
                        } else if (b_allowed) write(address_b, value);
                        if (!(parameters & 8U)) source = std::uint16_t(
                            (parameters & 0x10U) ? source - 1U : source + 1U);
                        set_word(base + 2U, source);
                        --remaining; // zero length denotes 65,536 bytes
                        set_word(base + 5U, remaining);
                    } while (remaining);
                    enabled_ &= std::uint8_t(~(1U << channel));
                }
                clock.interrupts().inhibit();
                // CPU re-synchronization is charged as DMA ownership, but its
                // length uses DMA work excluding refresh and this final step.
                clock.step(cpu_clocks - std::uint32_t(dma_clocks % cpu_clocks), SnesClockWork::dma);
                active_ = false;
            }
        }
        if (!active_ && pending_) active_ = true;
    }

private:
    std::uint16_t word(unsigned offset) const noexcept {
        return std::uint16_t(registers[offset] | (std::uint16_t(registers[offset + 1U]) << 8U));
    }
    void set_word(unsigned offset, std::uint16_t value) noexcept {
        registers[offset] = std::uint8_t(value);
        registers[offset + 1U] = std::uint8_t(value >> 8U);
    }
    std::uint8_t enabled_{};
    bool pending_{}, active_{};
};

} // namespace starfox::simulation
