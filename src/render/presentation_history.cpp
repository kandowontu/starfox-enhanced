#include "starfox/render/presentation_history.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace starfox::render {

PresentationHistory::PresentationHistory(std::size_t memory_budget)
    : memory_budget_(memory_budget) {
    if (memory_budget_ == 0U) {
        throw std::invalid_argument{"presentation history budget cannot be zero"};
    }
}

void PresentationHistory::reset_for_dimensions(
    std::uint32_t width, std::uint32_t height, std::size_t frame_bytes) {
    width_ = width;
    height_ = height;
    capacity_ = std::max<std::size_t>(2U, memory_budget_ / frame_bytes);
    frames_.clear();
    frames_.resize(capacity_);
    oldest_ = 0U;
    count_ = 0U;
    cursor_ = 0U;
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
    if (width != width_ || height != height_ || capacity_ == 0U) {
        reset_for_dimensions(width, height, frame_bytes);
    }

    // Recording after browsing an older image creates a new live edge. The
    // runtime normally walks forward through the retained images first, but
    // discarding the abandoned branch keeps this utility safe on its own.
    if (count_ != 0U && !at_live()) count_ = cursor_ + 1U;

    std::size_t slot{};
    if (count_ < capacity_) {
        slot = (oldest_ + count_) % capacity_;
        ++count_;
    } else {
        slot = oldest_;
        oldest_ = (oldest_ + 1U) % capacity_;
    }
    auto& frame = frames_[slot];
    frame.width = width;
    frame.height = height;
    frame.rgba.assign(rgba.begin(), rgba.end());
    cursor_ = count_ - 1U;
}

bool PresentationHistory::step_back() noexcept {
    if (count_ == 0U || cursor_ == 0U) return false;
    --cursor_;
    return true;
}

bool PresentationHistory::step_forward() noexcept {
    if (count_ == 0U || cursor_ + 1U >= count_) return false;
    ++cursor_;
    return true;
}

void PresentationHistory::to_live() noexcept {
    if (count_ != 0U) cursor_ = count_ - 1U;
}

bool PresentationHistory::at_live() const noexcept {
    return count_ == 0U || cursor_ + 1U == count_;
}

const PresentationFrame* PresentationHistory::current() const noexcept {
    if (count_ == 0U) return nullptr;
    return &frames_[(oldest_ + cursor_) % capacity_];
}

} // namespace starfox::render
