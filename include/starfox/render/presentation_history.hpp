#pragma once

#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

namespace starfox::render {

struct PresentationFrame {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> rgba;
};

// A bounded history of complete host presentations used by the frame
// debugger. Reversible RGBA deltas preserve HUD priority, colour math, fades,
// and presentation-only effects while retaining substantially more frames
// than an equivalent ring of full images.
class PresentationHistory {
public:
    explicit PresentationHistory(
        std::size_t memory_budget = 128U * 1024U * 1024U,
        std::size_t maximum_frames = 3'600U);
    ~PresentationHistory();
    PresentationHistory(const PresentationHistory&) = delete;
    PresentationHistory& operator=(const PresentationHistory&) = delete;

    void record(std::uint32_t width, std::uint32_t height,
        std::span<const std::uint8_t> rgba);
    [[nodiscard]] bool step_back() noexcept;
    [[nodiscard]] bool step_forward() noexcept;
    void to_live() noexcept;

    [[nodiscard]] bool at_live() const noexcept;
    [[nodiscard]] const PresentationFrame* current() const noexcept;
    [[nodiscard]] std::size_t frame_count() const noexcept;
    [[nodiscard]] std::size_t cursor() const noexcept;

private:
    void reset_for_dimensions(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] static std::vector<std::uint8_t> encode_delta(
        std::span<const std::uint8_t> from,
        std::span<const std::uint8_t> to);
    static void apply_delta(std::span<std::uint8_t> rgba,
        std::span<const std::uint8_t> encoded) noexcept;
    void worker_loop() noexcept;
    void commit_frame(PresentationFrame frame);
    void flush_pending() const noexcept;

    std::size_t memory_budget_{};
    std::size_t maximum_frames_{};
    std::size_t encoded_bytes_{};
    std::deque<std::vector<std::uint8_t>> deltas_;
    PresentationFrame current_frame_;
    bool has_frame_{};
    std::size_t cursor_{};
    std::uint32_t width_{};
    std::uint32_t height_{};
    std::uint32_t input_width_{};
    std::uint32_t input_height_{};
    mutable std::mutex worker_mutex_;
    mutable std::condition_variable worker_cv_;
    std::deque<PresentationFrame> pending_frames_;
    std::size_t pending_bytes_{};
    bool worker_busy_{};
    bool worker_stop_{};
    std::thread worker_;
};

} // namespace starfox::render
