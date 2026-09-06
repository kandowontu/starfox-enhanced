#pragma once

#include "starfox/simulation/snes_interrupts.hpp"
#include <cstdint>
#include <stdexcept>

namespace starfox::simulation {

enum class SnesRegion { ntsc, pal };

// S-CPU beam history, in master clocks. Interlace is sampled at line 128;
// callers supply the PPU setting rather than changing the field length early.
class SnesRasterClock {
public:
    explicit SnesRasterClock(SnesRegion region = SnesRegion::ntsc) noexcept
        : region_(region), vertical_period_(region == SnesRegion::ntsc ? 262U : 312U) {}

    // The CPU's smallest clock step. Returns true on a scanline boundary.
    bool tick(bool ppu_interlace) noexcept {
        elapsed_ += 2U;
        horizontal_ += 2U;
        if (horizontal_ != horizontal_period_) return false;
        previous_horizontal_period_ = horizontal_period_;
        horizontal_ = 0U;
        ++vertical_;
        if (vertical_ == 128U) {
            interlace_ = ppu_interlace;
            if (interlace_ && !field_) ++vertical_period_;
        }
        if (vertical_ == vertical_period_) {
            previous_vertical_period_ = vertical_period_;
            vertical_period_ = region_ == SnesRegion::ntsc ? 262U : 312U;
            vertical_ = 0U;
            field_ = !field_;
            ++fields_;
        }
        horizontal_period_ = 1364U;
        if (region_ == SnesRegion::ntsc && !interlace_ && field_ && vertical_ == 240U)
            horizontal_period_ = 1360U;
        if (region_ == SnesRegion::pal && interlace_ && field_ && vertical_ == 311U)
            horizontal_period_ = 1368U;
        return true;
    }

    [[nodiscard]] std::uint64_t elapsed() const noexcept { return elapsed_; }
    [[nodiscard]] std::uint64_t fields() const noexcept { return fields_; }
    [[nodiscard]] bool field() const noexcept { return field_; }
    [[nodiscard]] bool interlace() const noexcept { return interlace_; }
    [[nodiscard]] SnesRegion region() const noexcept { return region_; }
    [[nodiscard]] std::uint32_t horizontal_period() const noexcept { return horizontal_period_; }
    [[nodiscard]] std::uint32_t horizontal(std::uint32_t delay = 0U) const noexcept {
        return delay <= horizontal_ ? horizontal_ - delay
            : horizontal_ + previous_horizontal_period_ - delay;
    }
    [[nodiscard]] std::uint32_t vertical(std::uint32_t delay = 0U) const noexcept {
        return delay <= horizontal_ ? vertical_
            : vertical_ ? vertical_ - 1U : previous_vertical_period_ - 1U;
    }
    [[nodiscard]] std::uint32_t dot() const noexcept {
        if (horizontal_period_ == 1360U) return horizontal_ / 4U;
        return (horizontal_ - (horizontal_ > 1292U ? 2U : 0U)
            - (horizontal_ > 1310U ? 2U : 0U)) / 4U;
    }
    [[nodiscard]] InterruptBeam beam(std::uint16_t vblank_start = 225U) const noexcept {
        return {static_cast<std::uint16_t>(vertical(2U)),
            static_cast<std::uint16_t>(horizontal(6U)), static_cast<std::uint16_t>(vertical(6U)),
            static_cast<std::uint16_t>(horizontal(10U)), static_cast<std::uint16_t>(vertical(10U)),
            vblank_start};
    }

private:
    SnesRegion region_;
    std::uint64_t elapsed_{}, fields_{};
    std::uint32_t horizontal_{}, vertical_{};
    std::uint32_t horizontal_period_{1364U}, vertical_period_{};
    std::uint32_t previous_horizontal_period_{}, previous_vertical_period_{};
    bool interlace_{}, field_{};
};

enum class SnesClockWork { cpu, dma };

struct SnesClockTotals {
    std::uint64_t cpu{}, dma{}, refresh{};
};

// Shared bus-time raster/interrupt clock for the accurate scheduler. Each step
// is one CPU/DMA clock operation, not an aggregate instruction or frame. DMA
// arbitration and GSU overlap remain the caller's responsibility.
class SnesCpuTimeline {
public:
    explicit SnesCpuTimeline(SnesRegion region = SnesRegion::ntsc,
        std::uint8_t cpu_version = 2U)
        : raster_(region), cpu_version_(cpu_version), refresh_position_(cpu_version == 1U ? 530U : 538U) {
        if (cpu_version != 1U && cpu_version != 2U)
            throw std::invalid_argument{"Unsupported S-CPU timing version"};
    }

    void set_display(bool interlace, std::uint16_t vblank_start = 225U) noexcept {
        ppu_interlace_ = interlace;
        vblank_start_ = vblank_start;
    }
    void step(std::uint32_t clocks, SnesClockWork work = SnesClockWork::cpu) {
        if (clocks & 1U) throw std::invalid_argument{"S-CPU clock steps must be even"};
        interrupts_.clock_step();
        (work == SnesClockWork::cpu ? totals_.cpu : totals_.dma) += clocks;
        advance(clocks);
        // Refresh starts after the current bus operation. Its five 6+2-clock
        // phases also advance timer polling, while the CPU itself is stalled.
        if (!refreshed_ && raster_.horizontal() >= refresh_position_) {
            refreshed_ = true;
            totals_.refresh += 40U;
            for (unsigned phase = 0; phase < 5U; ++phase) {
                refresh_active_ = true;
                advance(6U);
                refresh_active_ = false;
                advance(2U);
            }
        }
    }

    [[nodiscard]] const SnesRasterClock& raster() const noexcept { return raster_; }
    [[nodiscard]] const SnesClockTotals& totals() const noexcept { return totals_; }
    [[nodiscard]] SnesInterrupts& interrupts() noexcept { return interrupts_; }
    [[nodiscard]] InterruptBeam beam() const noexcept { return raster_.beam(vblank_start_); }
    [[nodiscard]] std::uint32_t refresh_position() const noexcept { return refresh_position_; }
    [[nodiscard]] bool refresh_active() const noexcept { return refresh_active_; }
    [[nodiscard]] std::uint8_t cpu_version() const noexcept { return cpu_version_; }
    [[nodiscard]] std::uint16_t vblank_start() const noexcept { return vblank_start_; }

private:
    void advance(std::uint32_t clocks) noexcept {
        for (std::uint32_t remaining = clocks; remaining != 0U; remaining -= 2U) {
            if (raster_.tick(ppu_interlace_)) {
                refreshed_ = false;
                if (cpu_version_ == 2U)
                    refresh_position_ = 538U - static_cast<std::uint32_t>(raster_.elapsed() & 7U);
            }
            if (raster_.horizontal() & 2U) interrupts_.poll(beam());
        }
    }

    SnesRasterClock raster_;
    SnesInterrupts interrupts_;
    SnesClockTotals totals_;
    std::uint8_t cpu_version_;
    std::uint32_t refresh_position_;
    std::uint16_t vblank_start_{225U};
    bool ppu_interlace_{}, refreshed_{}, refresh_active_{};
};

} // namespace starfox::simulation
