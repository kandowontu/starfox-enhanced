#include "starfox/render/presentation_history.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace starfox::render {

PresentationHistory::PresentationHistory(
    std::size_t memory_budget, std::size_t maximum_frames)
    : memory_budget_(memory_budget), maximum_frames_(maximum_frames) {
    if (memory_budget_ == 0U) {
        throw std::invalid_argument{"presentation history budget cannot be zero"};
    }
    if (maximum_frames_ < 2U) {
        throw std::invalid_argument{
            "presentation history must retain at least two frames"};
    }
    worker_ = std::thread{[this] { worker_loop(); }};
}

PresentationHistory::~PresentationHistory() {
    {
        const std::lock_guard lock{worker_mutex_};
        worker_stop_ = true;
    }
    worker_cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void PresentationHistory::reset_for_dimensions(
    std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height;
    encoded_bytes_ = 0U;
    deltas_.clear();
    current_frame_ = {};
    has_frame_ = false;
    cursor_ = 0U;
}

namespace {

constexpr std::uint64_t delta_zero = 0U;
constexpr std::uint64_t delta_repeat = 1U;
constexpr std::uint64_t delta_literal = 2U;

void append_varint(std::vector<std::uint8_t>& output, std::uint64_t value) {
    do {
        auto byte = static_cast<std::uint8_t>(value & 0x7fU);
        value >>= 7U;
        if (value != 0U) byte |= 0x80U;
        output.push_back(byte);
    } while (value != 0U);
}

std::uint64_t read_varint(
    std::span<const std::uint8_t> input, std::size_t& offset) noexcept {
    std::uint64_t value{};
    unsigned shift{};
    while (offset < input.size() && shift < 64U) {
        const auto byte = input[offset++];
        value |= static_cast<std::uint64_t>(byte & 0x7fU) << shift;
        if ((byte & 0x80U) == 0U) return value;
        shift += 7U;
    }
    return 0U;
}

std::uint32_t pixel_delta(
    std::span<const std::uint8_t> from,
    std::span<const std::uint8_t> to,
    std::size_t pixel) noexcept {
    const auto offset = pixel * 4U;
    std::uint32_t from_pixel{};
    std::uint32_t to_pixel{};
    std::memcpy(&from_pixel, from.data() + offset, sizeof(from_pixel));
    std::memcpy(&to_pixel, to.data() + offset, sizeof(to_pixel));
    return from_pixel ^ to_pixel;
}

} // namespace

std::vector<std::uint8_t> PresentationHistory::encode_delta(
    std::span<const std::uint8_t> from,
    std::span<const std::uint8_t> to) {
    std::vector<std::uint8_t> encoded;
    const auto pixel_count = from.size() / 4U;
    encoded.reserve(std::min<std::size_t>(from.size() / 8U, 256U * 1024U));
    const auto append_pixel = [&encoded](std::uint32_t value) {
        const auto old_size = encoded.size();
        encoded.resize(old_size + sizeof(value));
        std::memcpy(encoded.data() + old_size, &value, sizeof(value));
    };
    auto pixel = std::size_t{};
    while (pixel < pixel_count) {
        const auto value = pixel_delta(from, to, pixel);
        if (value == 0U) {
            const auto first = pixel++;
            while (pixel < pixel_count
                   && pixel_delta(from, to, pixel) == 0U) {
                ++pixel;
            }
            append_varint(encoded,
                (static_cast<std::uint64_t>(pixel - first) << 2U)
                    | delta_zero);
            continue;
        }

        auto repeated = std::size_t{1U};
        while (pixel + repeated < pixel_count
               && pixel_delta(from, to, pixel + repeated) == value) {
            ++repeated;
        }
        if (repeated >= 3U) {
            append_varint(encoded,
                (static_cast<std::uint64_t>(repeated) << 2U)
                    | delta_repeat);
            append_pixel(value);
            pixel += repeated;
            continue;
        }

        const auto first = pixel;
        pixel += std::min<std::size_t>(repeated, 2U);
        while (pixel < pixel_count) {
            const auto next = pixel_delta(from, to, pixel);
            if (next == 0U) break;
            auto next_repeated = std::size_t{1U};
            while (next_repeated < 3U
                   && pixel + next_repeated < pixel_count
                   && pixel_delta(from, to, pixel + next_repeated) == next) {
                ++next_repeated;
            }
            if (next_repeated >= 3U) break;
            pixel += next_repeated;
        }
        const auto count = pixel - first;
        append_varint(encoded,
            (static_cast<std::uint64_t>(count) << 2U) | delta_literal);
        const auto byte_first = first * 4U;
        const auto byte_end = pixel * 4U;
        for (auto byte = byte_first; byte < byte_end; ++byte) {
            encoded.push_back(
                static_cast<std::uint8_t>(from[byte] ^ to[byte]));
        }
    }
    return encoded;
}

void PresentationHistory::apply_delta(
    std::span<std::uint8_t> rgba,
    std::span<const std::uint8_t> encoded) noexcept {
    auto encoded_offset = std::size_t{};
    auto pixel = std::size_t{};
    const auto pixel_count = rgba.size() / 4U;
    while (encoded_offset < encoded.size() && pixel < pixel_count) {
        const auto token = read_varint(encoded, encoded_offset);
        const auto kind = token & 3U;
        const auto count = static_cast<std::size_t>(token >> 2U);
        if (count == 0U || count > pixel_count - pixel) return;
        if (kind == delta_zero) {
            pixel += count;
            continue;
        }
        if (kind == delta_repeat) {
            if (encoded.size() - encoded_offset < 4U) return;
            for (auto item = std::size_t{}; item < count; ++item) {
                const auto output = (pixel + item) * 4U;
                for (auto component = std::size_t{}; component < 4U;
                     ++component) {
                    rgba[output + component] ^= encoded[encoded_offset + component];
                }
            }
            encoded_offset += 4U;
            pixel += count;
            continue;
        }
        if (kind != delta_literal
            || count > (encoded.size() - encoded_offset) / 4U) {
            return;
        }
        const auto bytes = count * 4U;
        const auto output = pixel * 4U;
        for (auto byte = std::size_t{}; byte < bytes; ++byte) {
            rgba[output + byte] ^= encoded[encoded_offset + byte];
        }
        encoded_offset += bytes;
        pixel += count;
    }
}

void PresentationHistory::record(
    std::uint32_t width, std::uint32_t height,
    std::span<const std::uint8_t> rgba) {
    if (width == 0U || height == 0U
        || static_cast<std::size_t>(width)
            > std::numeric_limits<std::size_t>::max()
                / static_cast<std::size_t>(height) / 4U) {
        throw std::invalid_argument{"invalid presentation dimensions"};
    }
    const auto frame_bytes = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height) * 4U;
    if (rgba.size() != frame_bytes) {
        throw std::invalid_argument{"presentation RGBA size does not match dimensions"};
    }
    if (width != input_width_ || height != input_height_) {
        flush_pending();
        reset_for_dimensions(width, height);
        input_width_ = width;
        input_height_ = height;
    }
    PresentationFrame frame{width, height,
        std::vector<std::uint8_t>{rgba.begin(), rgba.end()}};
    {
        std::unique_lock lock{worker_mutex_};
        // A bounded raw queue decouples compression from presentation without
        // allowing an overloaded renderer to consume unbounded memory.
        constexpr std::size_t queue_budget = 64U * 1024U * 1024U;
        worker_cv_.wait(lock, [this, frame_bytes] {
            return worker_stop_ || pending_bytes_ + frame_bytes <= queue_budget;
        });
        if (worker_stop_) return;
        pending_bytes_ += frame_bytes;
        pending_frames_.push_back(std::move(frame));
    }
    worker_cv_.notify_one();
}

void PresentationHistory::commit_frame(PresentationFrame frame) {
    if (!has_frame_) {
        current_frame_ = std::move(frame);
        has_frame_ = true;
        return;
    }

    // Recording from an older cursor creates a new live branch. Retain the
    // displayed frame as its base and discard only the unseen future.
    while (deltas_.size() > cursor_) {
        encoded_bytes_ -= deltas_.back().size();
        deltas_.pop_back();
    }

    auto delta = encode_delta(current_frame_.rgba, frame.rgba);
    encoded_bytes_ += delta.size();
    deltas_.push_back(std::move(delta));
    ++cursor_;
    current_frame_ = std::move(frame);

    // Every delta is reversible, so the oldest full frame is unnecessary.
    // Evict its transition and the next frame becomes the new rewind floor.
    while (deltas_.size() > 1U
        && (encoded_bytes_ > memory_budget_
            || deltas_.size() + 1U > maximum_frames_)) {
        encoded_bytes_ -= deltas_.front().size();
        deltas_.pop_front();
        --cursor_;
    }
}

void PresentationHistory::worker_loop() noexcept {
    for (;;) {
        auto frame = PresentationFrame{};
        {
            std::unique_lock lock{worker_mutex_};
            worker_cv_.wait(lock, [this] {
                return worker_stop_ || !pending_frames_.empty();
            });
            if (worker_stop_ && pending_frames_.empty()) return;
            frame = std::move(pending_frames_.front());
            pending_bytes_ -= frame.rgba.size();
            pending_frames_.pop_front();
            worker_busy_ = true;
        }
        worker_cv_.notify_all();
        try {
            commit_frame(std::move(frame));
        } catch (...) {
            // Frame history is a diagnostic aid. A transient allocation
            // failure must never terminate the game or its worker thread.
        }
        {
            const std::lock_guard lock{worker_mutex_};
            worker_busy_ = false;
        }
        worker_cv_.notify_all();
    }
}

void PresentationHistory::flush_pending() const noexcept {
    std::unique_lock lock{worker_mutex_};
    worker_cv_.wait(lock, [this] {
        return pending_frames_.empty() && !worker_busy_;
    });
}

bool PresentationHistory::step_back() noexcept {
    flush_pending();
    if (!has_frame_ || cursor_ == 0U) return false;
    apply_delta(current_frame_.rgba, deltas_[cursor_ - 1U]);
    --cursor_;
    return true;
}

bool PresentationHistory::step_forward() noexcept {
    flush_pending();
    if (!has_frame_ || cursor_ >= deltas_.size()) return false;
    apply_delta(current_frame_.rgba, deltas_[cursor_]);
    ++cursor_;
    return true;
}

void PresentationHistory::to_live() noexcept {
    flush_pending();
    while (step_forward()) {}
}

bool PresentationHistory::at_live() const noexcept {
    flush_pending();
    return !has_frame_ || cursor_ == deltas_.size();
}

const PresentationFrame* PresentationHistory::current() const noexcept {
    flush_pending();
    return has_frame_ ? &current_frame_ : nullptr;
}

std::size_t PresentationHistory::frame_count() const noexcept {
    flush_pending();
    return has_frame_ ? deltas_.size() + 1U : 0U;
}

std::size_t PresentationHistory::cursor() const noexcept {
    flush_pending();
    return cursor_;
}

} // namespace starfox::render
