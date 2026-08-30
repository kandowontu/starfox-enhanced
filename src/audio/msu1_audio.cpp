#include "starfox/audio/msu1_audio.hpp"

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace starfox::audio {
namespace {

constexpr std::array<std::uint64_t, 53> kLoopFrames{
    0, 0, 36750, 22609, 545357, 821, 4044, 0, 0, 0, 11, 159385,
    0, 88151, 399104, 90471, 468709, 142668, 323173, 0, 0, 1352027,
    681400, 80669, 528583, 1349300, 563668, 4003, 520328, 1520932,
    279722, 1087476, 644834, 3561, 468947, 130546, 136251, 158123, 0,
    0, 0, 142525, 0, 0, 0, 691760, 0, 348515, 714448, 16608967,
    0, 0, 0};

std::int16_t clamp_sample(double value) noexcept {
    return static_cast<std::int16_t>(std::clamp(value,
        static_cast<double>(std::numeric_limits<std::int16_t>::min()),
        static_cast<double>(std::numeric_limits<std::int16_t>::max())));
}

} // namespace

Msu1Audio::Msu1Audio(TrackLoader loader) : loader_(std::move(loader)) {}

void Msu1Audio::set_enabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) playing_ = false;
}

void Msu1Audio::process_register_writes(
    std::span<const simulation::MsuRegisterWrite> writes) {
    for (const auto& write : writes) {
        switch (write.address) {
        case 0x2004U:
            selected_track_ = static_cast<std::uint16_t>(
                (selected_track_ & 0xff00U) | write.value);
            break;
        case 0x2005U:
            selected_track_ = static_cast<std::uint16_t>(
                (selected_track_ & 0x00ffU)
                | (static_cast<std::uint16_t>(write.value) << 8U));
            break;
        case 0x2006U:
            volume_ = write.value;
            break;
        case 0x2007U:
            if ((write.value & 0x01U) == 0U) {
                playing_ = false;
                source_cursor_ = 0.0;
                break;
            }
            repeat_ = (write.value & 0x02U) != 0U;
            if (enabled_ && load_selected_track()) {
                // Bit 2 requests resume. Ordinary play always restarts.
                if ((write.value & 0x04U) == 0U) source_cursor_ = 0.0;
                playing_ = true;
            }
            break;
        default:
            break;
        }
    }
}

bool Msu1Audio::load_selected_track() {
    if (loaded_track_ == selected_track_ && !decoded_.empty()) return true;
    if (!loader_ || selected_track_ == 0U) return false;
    const auto bytes = loader_(selected_track_);
    if (bytes.empty()) return false;
    unsigned channels{};
    unsigned sample_rate{};
    drflac_uint64 frames{};
    auto* samples = drflac_open_memory_and_read_pcm_frames_s16(
        bytes.data(), bytes.size(), &channels, &sample_rate, &frames, nullptr);
    if (samples == nullptr || channels == 0U || sample_rate == 0U
        || frames == 0U) {
        if (samples != nullptr) drflac_free(samples, nullptr);
        return false;
    }
    decoded_.assign(samples, samples
        + static_cast<std::size_t>(frames) * channels);
    drflac_free(samples, nullptr);
    source_sample_rate_ = sample_rate;
    source_channels_ = channels;
    source_frames_ = frames;
    loaded_track_ = selected_track_;
    source_cursor_ = 0.0;

    // The source pack's tracks.json asks MSUPCM++ for -18 dB normalization,
    // with quieter -25 dB targets for Training and Continue. Apply the same
    // targets after lossless decode while protecting against clipping.
    long double energy{};
    auto peak = 1.0;
    for (const auto sample : decoded_) {
        const auto value = static_cast<double>(sample) / 32768.0;
        energy += value * value;
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    const auto rms = std::sqrt(static_cast<double>(
        energy / static_cast<long double>(decoded_.size())));
    const auto target_db = selected_track_ == 4U || selected_track_ == 41U
        ? -25.0 : -18.0;
    const auto target = std::pow(10.0, target_db / 20.0);
    normalization_gain_ = rms > 0.0 ? target / rms : 1.0;
    normalization_gain_ = std::min(normalization_gain_, 32767.0 / peak);
    return true;
}

std::span<const std::int16_t> Msu1Audio::render(
    std::size_t output_frames, std::uint32_t output_sample_rate) {
    output_.assign(output_frames * 2U, 0);
    if (!enabled_ || !playing_ || decoded_.empty()
        || output_sample_rate == 0U) return output_;
    const auto step = static_cast<double>(source_sample_rate_)
        / static_cast<double>(output_sample_rate);
    const auto loop = std::min(loop_frame(selected_track_), source_frames_);
    const auto volume = static_cast<double>(volume_) / 255.0;
    for (std::size_t frame = 0; frame < output_frames; ++frame) {
        while (source_cursor_ >= static_cast<double>(source_frames_)) {
            if (!repeat_) {
                playing_ = false;
                return output_;
            }
            source_cursor_ = static_cast<double>(loop)
                + (source_cursor_ - static_cast<double>(source_frames_));
        }
        const auto first = static_cast<std::uint64_t>(source_cursor_);
        const auto second = std::min(first + 1U, source_frames_ - 1U);
        const auto fraction = source_cursor_ - static_cast<double>(first);
        for (std::size_t channel = 0; channel < 2U; ++channel) {
            const auto source_channel = std::min<std::size_t>(
                channel, source_channels_ - 1U);
            const auto a = decoded_[static_cast<std::size_t>(first)
                * source_channels_ + source_channel];
            const auto b = decoded_[static_cast<std::size_t>(second)
                * source_channels_ + source_channel];
            output_[frame * 2U + channel] = clamp_sample(
                (static_cast<double>(a)
                    + (static_cast<double>(b) - a) * fraction)
                * normalization_gain_ * volume);
        }
        source_cursor_ += step;
    }
    return output_;
}

std::uint64_t Msu1Audio::loop_frame(std::uint16_t track) noexcept {
    return track < kLoopFrames.size() ? kLoopFrames[track] : 0U;
}

} // namespace starfox::audio
