#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace starfox::render {

struct PresentationFrame {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> rgba;
};

// A bounded ring of complete host presentations used by the frame debugger.
// Storing post-composite RGBA preserves HUD priority, colour math, fades, and
// other presentation-only effects when walking backward through frames.
class PresentationHistory {
public:
    explicit PresentationHistory(
        std::size_t memory_budget = 64U * 1024U * 1024U);

    void record(std::uint32_t width, std::uint32_t height,
        std::span<const std::uint8_t> rgba);
    [[nodiscard]] bool step_back() noexcept;
    [[nodiscard]] bool step_forward() noexcept;
    void to_live() noexcept;

    [[nodiscard]] bool at_live() const noexcept;
    [[nodiscard]] const PresentationFrame* current() const noexcept;
    [[nodiscard]] std::size_t frame_count() const noexcept { return count_; }
    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }

private:
    void reset_for_dimensions(
        std::uint32_t width, std::uint32_t height, std::size_t frame_bytes);

    std::size_t memory_budget_{};
    std::vector<PresentationFrame> frames_;
    std::size_t capacity_{};
    std::size_t oldest_{};
    std::size_t count_{};
    std::size_t cursor_{};
    std::uint32_t width_{};
    std::uint32_t height_{};
};

} // namespace starfox::render
