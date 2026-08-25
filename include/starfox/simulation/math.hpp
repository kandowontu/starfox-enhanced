#pragma once

#include "starfox/assets/rom.hpp"

#include <array>
#include <cstdint>

namespace starfox::simulation {

[[nodiscard]] std::int16_t wrap16(std::int64_t value) noexcept;
[[nodiscard]] std::int16_t add16(std::int16_t left, std::int16_t right) noexcept;
[[nodiscard]] std::int16_t subtract16(std::int16_t left, std::int16_t right) noexcept;
[[nodiscard]] std::int32_t arithmetic_shift_right(std::int32_t value, unsigned bits) noexcept;
[[nodiscard]] std::int16_t multiply_q15(std::int16_t left, std::int16_t right) noexcept;
[[nodiscard]] std::int16_t multiply_q14(std::int16_t left, std::int16_t right) noexcept;

class TrigTables {
public:
    [[nodiscard]] static TrigTables load(
        const assets::RomImage& rom, const assets::SymbolMap& symbols);

    [[nodiscard]] std::int8_t sin8(std::uint8_t angle) const noexcept;
    [[nodiscard]] std::int8_t cos8(std::uint8_t angle) const noexcept;
    [[nodiscard]] std::int16_t sin_q15(std::uint16_t angle) const noexcept;
    [[nodiscard]] std::int16_t cos_q15(std::uint16_t angle) const noexcept;

private:
    std::array<std::int8_t, 256> sine8_{};
    std::array<std::int8_t, 256> cosine8_{};
    std::array<std::int16_t, 256> sine_q15_{};
};

using MatrixQ15 = std::array<std::int16_t, 9>;

[[nodiscard]] MatrixQ15 rotation_matrix_q15(
    const TrigTables& trig,
    std::int16_t x,
    std::int16_t y,
    std::int16_t z) noexcept;
[[nodiscard]] MatrixQ15 transpose_q15(const MatrixQ15& matrix) noexcept;
[[nodiscard]] MatrixQ15 multiply_matrix_q15(
    const MatrixQ15& left, const MatrixQ15& right) noexcept;
[[nodiscard]] std::array<std::int16_t, 3> transform_q15(
    const MatrixQ15& matrix,
    const std::array<std::int16_t, 3>& value) noexcept;

} // namespace starfox::simulation
