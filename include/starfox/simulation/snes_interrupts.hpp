#pragma once
#include <cstdint>

namespace starfox::simulation {

// The scheduler supplies counter history in master clocks, including short
// scanlines/interlace. Keeping beam generation outside the interrupt unit
// avoids introducing a second, potentially inconsistent video clock.
struct InterruptBeam {
    std::uint16_t vertical_2{};
    std::uint16_t horizontal_6{}, vertical_6{};
    std::uint16_t horizontal_10{}, vertical_10{};
    std::uint16_t vblank_start{225};
};

struct InterruptRequest {
    bool nmi{}, irq{}, wake{};
    bool operator==(const InterruptRequest&) const = default;
};

// S-CPU timer/NMI latches, separate from architectural 65C816 interrupt entry.
// poll() is called every four master clocks; timer writes additionally evaluate
// the IRQ comparator at the write instant. sample() belongs at the CPU's
// last-cycle polling point, not at an arbitrary host frame boundary.
class SnesInterrupts {
public:
    void poll(const InterruptBeam& beam) noexcept {
        if (nmi_hold_ && (control_ & 0x80U)) nmi_pending_ = true;
        nmi_hold_ = false;
        const bool blank = beam.vertical_2 >= beam.vblank_start;
        if (blank != in_vblank_) {
            in_vblank_ = blank;
            nmi_latch_ = blank;
            nmi_hold_ = blank;
        }
        compare_irq(beam);
    }

    void write_control(std::uint8_t value) noexcept {
        if (!(value & 0x30U)) irq_latch_ = irq_pending_ = false;
        else if ((value & 0x30U) == 0x20U && irq_latch_) irq_pending_ = true;
        if ((value & 0x80U) && !(control_ & 0x80U) && nmi_latch_)
            nmi_pending_ = true;
        control_ = value;
        locked_ = true;
    }

    // Index 0..3 corresponds to HTIMEL, HTIMEH, VTIMEL, VTIMEH.
    void write_timer(unsigned index, std::uint8_t value,
        const InterruptBeam& beam) noexcept {
        if (index > 3U) return;
        auto& timer = index < 2U ? horizontal_ : vertical_;
        if (index & 1U) timer = (timer & 0xffU) | ((value & 1U) << 8U);
        else timer = (timer & 0x100U) | value;
        compare_irq(beam);
    }

    std::uint8_t read_irq(std::uint8_t open_bus) noexcept {
        const auto result = static_cast<std::uint8_t>(
            (open_bus & 0x7fU) | (irq_latch_ ? 0x80U : 0U));
        if (!irq_hold_) irq_latch_ = irq_pending_ = false;
        return result;
    }

    std::uint8_t read_nmi(std::uint8_t open_bus, std::uint8_t version = 2) noexcept {
        const auto result = static_cast<std::uint8_t>((open_bus & 0x70U)
            | (version & 0x0fU) | (nmi_latch_ ? 0x80U : 0U));
        if (!nmi_hold_) nmi_latch_ = false;
        return result;
    }

    // Writes to NMITIMEN and completion of DMA inhibit instruction polling
    // until the next CPU clock step. Neither acknowledgement clears an
    // external IRQ source (for example a coprocessor).
    void inhibit() noexcept { locked_ = true; }
    void clock_step() noexcept { locked_ = false; }
    InterruptRequest sample(bool masked, bool external_irq = false, bool external_nmi = false) noexcept {
        if (locked_) return {};
        const bool irq = irq_pending_ || external_irq;
        const InterruptRequest result{nmi_pending_ || external_nmi, irq && !masked,
            nmi_pending_ || external_nmi || irq};
        nmi_pending_ = irq_pending_ = false;
        return result;
    }

private:
    void compare_irq(const InterruptBeam& beam) noexcept {
        irq_hold_ = false;
        const bool enabled = (control_ & 0x30U) != 0;
        if (enabled && irq_latch_) irq_pending_ = true;
        const bool match = enabled
            && (!(control_ & 0x10U) || beam.horizontal_10 == (horizontal_ + 1U) * 4U)
            && (!(control_ & 0x20U) || beam.vertical_10 == vertical_)
            && (beam.horizontal_6 != 0 || beam.vertical_6 != 0);
        if (match && !comparator_) irq_latch_ = irq_hold_ = true;
        comparator_ = match;
    }

    std::uint16_t horizontal_{0x1ff}, vertical_{0x1ff};
    std::uint8_t control_{};
    bool locked_{};
    bool in_vblank_{}, nmi_latch_{}, nmi_hold_{}, nmi_pending_{};
    bool comparator_{}, irq_latch_{}, irq_hold_{}, irq_pending_{};
};

} // namespace starfox::simulation
