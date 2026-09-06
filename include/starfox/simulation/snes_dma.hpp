#pragma once

#include "starfox/simulation/snes_timeline.hpp"
#include <array>
#include <memory>

namespace starfox::simulation {

// Shared general DMA / HDMA bus owner. Requests are raised by CPU stores or
// raster events and serviced at complete CPU cycles and DMA byte boundaries.
class SnesDma {
public:
    std::array<std::uint8_t, 0x80> registers{};

    void request(std::uint8_t mask) noexcept {
        enabled_ = mask;
        if (mask) pending_ = true;
    }
    void enable_hdma(std::uint8_t mask) noexcept { hdma_->enabled = mask; }
    [[nodiscard]] const std::shared_ptr<SnesHdmaState>& hdma_state() const noexcept { return hdma_; }
    [[nodiscard]] bool requested() const noexcept {
        return enabled_ != 0 || hdma_->enabled != 0 || hdma_->pending;
    }

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

    template<class Read, class Write>
    void edge(std::uint32_t cpu_clocks, SnesCpuTimeline& clock, Read&& read, Write&& write) {
        if (!cpu_clocks || (cpu_clocks & 1U))
            throw std::invalid_argument{"DMA requires a positive even CPU cycle"};
        Transfer<Read, Write>{*this, clock, read, write, cpu_clocks}.edge();
    }

private:
    std::uint16_t word(unsigned offset) const noexcept {
        return std::uint16_t(registers[offset] | (std::uint16_t(registers[offset + 1U]) << 8U));
    }
    void set_word(unsigned offset, std::uint16_t value) noexcept {
        registers[offset] = std::uint8_t(value);
        registers[offset + 1U] = std::uint8_t(value >> 8U);
    }
    template<class Read, class Write> struct Transfer {
        SnesDma& dma;
        SnesCpuTimeline& clock;
        Read& read;
        Write& write;
        std::uint32_t cpu_clocks;

        void step(std::uint32_t clocks) {
            dma.dma_clocks_ += clocks;
            clock.step(clocks, SnesClockWork::dma);
        }
        void align() {
            dma.dma_clocks_ = 0;
            step(8U - std::uint32_t(clock.raster().elapsed() & 7U));
        }
        void resync() {
            clock.step(cpu_clocks - std::uint32_t(dma.dma_clocks_ % cpu_clocks), SnesClockWork::dma);
            dma.active_ = false;
        }
        bool active(unsigned channel) const {
            return (dma.hdma_->enabled & ~dma.hdma_->completed & (1U << channel)) != 0;
        }
        void edge() {
            if (dma.active_) {
                if (dma.hdma_->pending) {
                    dma.hdma_->pending = false;
                    if (dma.hdma_->enabled) {
                        if (!dma.enabled_) align();
                        dma.hdma_->run ? hdma_run() : hdma_setup();
                        if (!dma.enabled_) resync();
                    }
                }
                if (dma.pending_) {
                    dma.pending_ = false;
                    if (dma.enabled_) {
                        align();
                        general();
                        resync();
                    }
                }
            }
            if (!dma.active_ && (dma.pending_ || dma.hdma_->pending)) dma.active_ = true;
        }
        std::uint8_t read_a(std::uint32_t address) {
            step(4U);
            const auto value = valid_a(address) ? std::uint8_t(read(address)) : std::uint8_t{};
            step(4U);
            return value;
        }
        void byte(unsigned channel, std::uint32_t address_a, unsigned index) {
            const auto base = channel * 16U;
            const auto parameters = dma.registers[base];
            unsigned offset{};
            switch (parameters & 7U) {
            case 1: case 5: offset = index & 1U; break;
            case 3: case 7: offset = (index >> 1U) & 1U; break;
            case 4: offset = index & 3U; break;
            default: break;
            }
            const auto address_b = 0x2100U | std::uint8_t(dma.registers[base + 1U] + offset);
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
        }
        void general() {
            step(8U);
            edge();
            for (unsigned channel = 0; channel < 8; ++channel) {
                const auto bit = 1U << channel;
                if (!(dma.enabled_ & bit)) continue;
                const auto base = channel * 16U;
                step(8U);
                edge();
                unsigned index{};
                do {
                    byte(channel, (std::uint32_t(dma.registers[base + 4U]) << 16U)
                        | dma.word(base + 2U), index++);
                    const auto parameters = dma.registers[base];
                    if (!(parameters & 8U)) dma.set_word(base + 2U, std::uint16_t(
                        dma.word(base + 2U) + ((parameters & 0x10U) ? -1 : 1)));
                    edge();
                    // HDMA can disable DMA here; the source then leaves DAS
                    // untouched instead of decrementing the interrupted byte.
                    if (!(dma.enabled_ & bit)) break;
                    dma.set_word(base + 5U, std::uint16_t(dma.word(base + 5U) - 1U));
                } while (dma.word(base + 5U));
                dma.enabled_ &= std::uint8_t(~bit);
            }
            clock.interrupts().inhibit();
        }
        void reload(unsigned channel) {
            const auto base = channel * 16U;
            auto data = read_a((std::uint32_t(dma.registers[base + 4U]) << 16U) | dma.word(base + 8U));
            if (dma.registers[base + 10U] & 0x7fU) return;
            dma.registers[base + 10U] = data;
            dma.set_word(base + 8U, std::uint16_t(dma.word(base + 8U) + 1U));
            const auto bit = std::uint8_t(1U << channel);
            if (data) { dma.hdma_->completed &= std::uint8_t(~bit); dma.hdma_->transfer |= bit; }
            else { dma.hdma_->completed |= bit; dma.hdma_->transfer &= std::uint8_t(~bit); }
            if (!(dma.registers[base] & 0x40U)) return;
            const auto indirect_byte = [&]() {
                const auto address = (std::uint32_t(dma.registers[base + 4U]) << 16U) | dma.word(base + 8U);
                dma.set_word(base + 8U, std::uint16_t(dma.word(base + 8U) + 1U));
                return read_a(address);
            };
            data = indirect_byte();
            dma.set_word(base + 5U, std::uint16_t(data) << 8U);
            bool finished = true;
            for (unsigned next = channel + 1U; next < 8U; ++next) if (active(next)) finished = false;
            if ((dma.hdma_->completed & bit) && finished) return;
            data = indirect_byte();
            dma.set_word(base + 5U, (std::uint16_t(data) << 8U) | (dma.word(base + 5U) >> 8U));
        }
        void hdma_setup() {
            step(8U);
            for (unsigned channel = 0; channel < 8U; ++channel) {
                const auto bit = std::uint8_t(1U << channel);
                dma.hdma_->transfer |= bit;
                if (!(dma.hdma_->enabled & bit)) continue;
                dma.enabled_ &= std::uint8_t(~bit);
                const auto base = channel * 16U;
                dma.set_word(base + 8U, dma.word(base + 2U));
                dma.registers[base + 10U] = 0;
                reload(channel);
            }
            clock.interrupts().inhibit();
        }
        void hdma_run() {
            step(8U);
            constexpr std::array lengths{1U,2U,2U,4U,4U,4U,2U,4U};
            for (unsigned channel = 0; channel < 8U; ++channel) {
                if (!active(channel)) continue;
                const auto bit = std::uint8_t(1U << channel);
                dma.enabled_ &= std::uint8_t(~bit);
                if (!(dma.hdma_->transfer & bit)) continue;
                const auto base = channel * 16U;
                const auto length = lengths[dma.registers[base] & 7U];
                for (unsigned index = 0; index < length; ++index) {
                    const bool indirect = (dma.registers[base] & 0x40U) != 0;
                    const auto offset = base + (indirect ? 5U : 8U);
                    const auto bank = dma.registers[base + (indirect ? 7U : 4U)];
                    const auto address = (std::uint32_t(bank) << 16U) | dma.word(offset);
                    dma.set_word(offset, std::uint16_t(dma.word(offset) + 1U));
                    byte(channel, address, index);
                }
            }
            for (unsigned channel = 0; channel < 8U; ++channel) {
                if (!active(channel)) continue;
                const auto base = channel * 16U;
                --dma.registers[base + 10U];
                const auto bit = std::uint8_t(1U << channel);
                if (dma.registers[base + 10U] & 0x80U) dma.hdma_->transfer |= bit;
                else dma.hdma_->transfer &= std::uint8_t(~bit);
                reload(channel);
            }
            clock.interrupts().inhibit();
        }
    };
    std::shared_ptr<SnesHdmaState> hdma_{std::make_shared<SnesHdmaState>()};
    std::uint64_t dma_clocks_{};
    std::uint8_t enabled_{};
    bool pending_{}, active_{};
};

} // namespace starfox::simulation
