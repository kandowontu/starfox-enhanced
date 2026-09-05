#pragma once
#include <cstdint>
#include <type_traits>

namespace starfox::simulation {
template<class Word> struct DecimalResult {
    Word value{};
    bool carry{};
    bool overflow{};
};

// 65C816 decimal adjustment works a nibble at a time, including non-BCD
// input digits. V observes the top binary digit before its decimal adjustment;
// N and Z observe the final adjusted word. SBC takes carry as "no borrow".
template<class Word, bool Subtract>
[[nodiscard]] constexpr DecimalResult<Word> decimal_arithmetic(
    Word a, Word b, bool carry) noexcept {
    static_assert(std::is_same_v<Word, std::uint8_t>
        || std::is_same_v<Word, std::uint16_t>);
    unsigned result{};
    bool overflow{};
    constexpr unsigned bits = sizeof(Word) * 8U;
    for (unsigned shift = 0; shift < bits; shift += 4U) {
        const auto digit_a = (static_cast<unsigned>(a) >> shift) & 15U;
        const auto digit_b = (static_cast<unsigned>(b) >> shift) & 15U;
        int digit = static_cast<int>(digit_a + (Subtract ? 15U - digit_b : digit_b)
            + static_cast<unsigned>(carry));
        if (shift == bits - 4U) {
            const auto unadjusted = static_cast<unsigned>(digit) << shift;
            constexpr unsigned sign = 1U << (bits - 1U);
            overflow = ((static_cast<unsigned>(a) ^ unadjusted)
                & (Subtract ? (a ^ b) : ~(a ^ b)) & sign) != 0U;
        }
        if constexpr (Subtract) {
            if (digit <= 15) digit -= 6;
        } else {
            if (digit > 9) digit += 6;
        }
        carry = digit > 15;
        result |= (static_cast<unsigned>(digit) & 15U) << shift;
    }
    return {static_cast<Word>(result), carry, overflow};
}
}
