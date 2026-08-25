#include "starfox/simulation/prng.hpp"

namespace starfox::simulation {
namespace {

std::uint8_t subtract_with_borrow(
    std::uint8_t left, std::uint8_t right, bool& carry) noexcept {
    const auto result = static_cast<int>(left) - static_cast<int>(right)
        - (carry ? 0 : 1);
    carry = result >= 0;
    return static_cast<std::uint8_t>(result);
}

} // namespace

std::uint8_t OriginalPrng::next() noexcept {
    auto accumulator = state_[0];
    auto carry = false;
    accumulator = subtract_with_borrow(accumulator, state_[1], carry);
    state_[1] = accumulator;
    accumulator = subtract_with_borrow(accumulator, state_[2], carry);
    state_[2] = accumulator;
    accumulator = subtract_with_borrow(accumulator, state_[3], carry);
    state_[3] = accumulator;
    accumulator = subtract_with_borrow(accumulator, state_[0], carry);
    state_[0] = accumulator;
    return accumulator;
}

} // namespace starfox::simulation
