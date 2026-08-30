#pragma once

#include "starfox/simulation/wdc65816.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace starfox::audio {

// Host-side MSU-1 audio device. The cartridge still owns track selection,
// looping, fades, and volume through the real $2004-$2007 register protocol;
// this class only decodes and presents the selected FLAC stream.
class Msu1Audio {
public:
    using TrackLoader =
        std::function<std::vector<std::uint8_t>(std::uint16_t)>;

    explicit Msu1Audio(TrackLoader loader = {});

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] bool playing() const noexcept { return playing_; }
    [[nodiscard]] std::uint16_t selected_track() const noexcept {
        return selected_track_;
    }
    void process_register_writes(
        std::span<const simulation::MsuRegisterWrite> writes);
    [[nodiscard]] std::span<const std::int16_t> render(
        std::size_t output_frames, std::uint32_t output_sample_rate);

private:
    bool load_selected_track();
    [[nodiscard]] static std::uint64_t loop_frame(
        std::uint16_t track) noexcept;

    TrackLoader loader_;
    std::vector<std::int16_t> decoded_;
    std::vector<std::int16_t> output_;
    std::uint32_t source_sample_rate_{};
    std::uint32_t source_channels_{};
    std::uint64_t source_frames_{};
    double source_cursor_{};
    double normalization_gain_{1.0};
    std::uint16_t selected_track_{};
    std::uint16_t loaded_track_{};
    std::uint8_t volume_{255U};
    bool enabled_{};
    bool playing_{};
    bool repeat_{};
};

} // namespace starfox::audio
