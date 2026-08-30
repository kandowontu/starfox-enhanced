#pragma once

#include "starfox/simulation/wdc65816.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <span>
#include <vector>

namespace starfox::audio {

// Runs the cartridge's uploaded SPC700 driver and BRR sample data through
// the cycle-accurate SNES_SPC/S-DSP core. The cartridge upload protocol is
// decoded into its post-IPL state so Nintendo's separate 64-byte IPL ROM is
// neither required nor embedded in the PC port.
class Spc700Audio {
public:
    struct State {
        std::uint16_t program_counter{};
        std::uint8_t accumulator{};
        std::uint8_t x{};
        std::uint8_t y{};
        std::uint8_t status{};
        std::uint8_t stack{};
        std::uint8_t dsp_flags{};
        std::uint8_t dsp_key_on{};
        std::int8_t main_volume_left{};
        std::int8_t main_volume_right{};
        std::array<std::uint8_t, 4> output_ports{};
    };

    static constexpr std::uint32_t sample_rate = 32'000;
    static constexpr std::size_t stereo_frames_per_logic_tick = 1'600;

    Spc700Audio();
    ~Spc700Audio();
    Spc700Audio(Spc700Audio&&) noexcept;
    Spc700Audio& operator=(Spc700Audio&&) noexcept;
    Spc700Audio(const Spc700Audio&) = delete;
    Spc700Audio& operator=(const Spc700Audio&) = delete;

    // Applies CPU-to-APU writes from one 20 Hz gameplay update and returns
    // exactly 50 ms of native 32 kHz interleaved stereo output.
    [[nodiscard]] std::vector<std::int16_t> render_logic_tick(
        std::span<const simulation::ApuPortWrite> writes);

    // Music commands (CPU ports 0-2) and effect commands (port 3) run on
    // independent SPC700 instances in the native port. Exposing the most
    // recent stems makes it possible to verify that an effect changed only
    // the effect mix and never consumed or muted a music voice.
    [[nodiscard]] std::span<const std::int16_t> last_music_samples() const
        noexcept { return last_music_samples_; }
    [[nodiscard]] std::span<const std::int16_t> last_effect_samples() const
        noexcept { return last_effect_samples_; }

    // Services all complete IPL uploads captured during synchronous cartridge
    // construction. If one captured stream contains multiple banks, each
    // replacement is separated by the driver's $ff restart command and must
    // receive its own 50 ms SPC frame so the preceding driver can initialize
    // the ARAM workspace before the next bank overlays it.
    [[nodiscard]] std::size_t prime_upload_sequence(
        std::span<const simulation::ApuPortWrite> writes);

    [[nodiscard]] bool driver_loaded() const noexcept;
    [[nodiscard]] std::size_t uploaded_bytes() const noexcept;
    [[nodiscard]] std::array<std::uint8_t, 4> output_ports() const noexcept;
    [[nodiscard]] State state() const;

private:
    struct Impl;
    std::unique_ptr<Impl> music_impl_;
    std::unique_ptr<Impl> effects_impl_;
    std::vector<std::int16_t> last_music_samples_;
    std::vector<std::int16_t> last_effect_samples_;
};

} // namespace starfox::audio
