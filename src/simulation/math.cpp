#include "starfox/simulation/math.hpp"

#include <algorithm>
#include <cmath>
#include <bit>
#include <limits>
#include <stdexcept>
#include <string>

namespace starfox::simulation {
namespace {

std::uint32_t unique_rom_symbol(
    const assets::SymbolMap& symbols, const std::string& name) {
    for (const auto address : symbols.find(name)) {
        if ((address & 0xffffU) >= 0x8000U && ((address >> 16U) & 0xffU) < 0x7eU) {
            return address;
        }
    }
    throw std::runtime_error{"missing ROM symbol: " + name};
}

} // namespace

std::int16_t wrap16(std::int64_t value) noexcept {
    const auto bits = static_cast<std::uint16_t>(static_cast<std::uint64_t>(value));
    return std::bit_cast<std::int16_t>(bits);
}

std::int16_t add16(std::int16_t left, std::int16_t right) noexcept {
    return wrap16(static_cast<std::int32_t>(left) + right);
}

std::int16_t subtract16(std::int16_t left, std::int16_t right) noexcept {
    return wrap16(static_cast<std::int32_t>(left) - right);
}

std::int32_t arithmetic_shift_right(std::int32_t value, unsigned bits) noexcept {
    if (bits == 0) {
        return value;
    }
    if (bits >= 31) {
        return value < 0 ? -1 : 0;
    }
    if (value >= 0) {
        return value >> bits;
    }
    const auto magnitude = -static_cast<std::int64_t>(value);
    const auto divisor = std::int64_t{1} << bits;
    return static_cast<std::int32_t>(-((magnitude + divisor - 1) / divisor));
}

std::int16_t multiply_q15(std::int16_t left, std::int16_t right) noexcept {
    return wrap16(arithmetic_shift_right(
        static_cast<std::int32_t>(left) * static_cast<std::int32_t>(right), 15));
}

std::int16_t multiply_q14(std::int16_t left, std::int16_t right) noexcept {
    return wrap16(arithmetic_shift_right(
        static_cast<std::int32_t>(left) * static_cast<std::int32_t>(right), 14));
}

TrigTables TrigTables::load(
    const assets::RomImage& rom, const assets::SymbolMap& symbols) {
    TrigTables result;
    const auto sine8_address = unique_rom_symbol(symbols, "SINTAB");
    const auto cosine8_address = unique_rom_symbol(symbols, "COSTAB");
    const auto sine16_address = unique_rom_symbol(symbols, "SINTAB16");
    for (std::uint32_t index = 0; index < 256; ++index) {
        result.sine8_[index] = static_cast<std::int8_t>(rom.read8(sine8_address + index));
        result.cosine8_[index] = static_cast<std::int8_t>(rom.read8(cosine8_address + index));
        result.sine_q15_[index] = rom.read_i16(sine16_address + index * 2U);
    }
    return result;
}

std::int8_t TrigTables::sin8(std::uint8_t angle) const noexcept {
    return sine8_[angle];
}

std::int8_t TrigTables::cos8(std::uint8_t angle) const noexcept {
    return cosine8_[angle];
}

std::int16_t TrigTables::sin_q15(std::uint16_t angle) const noexcept {
    const auto index = static_cast<std::uint8_t>(angle >> 8U);
    const auto fraction = static_cast<std::uint8_t>(angle);
    const auto current = sine_q15_[index];
    const auto next = sine_q15_[static_cast<std::uint8_t>(index + 1U)];
    const auto difference = static_cast<std::int32_t>(next) - current;
    return wrap16(static_cast<std::int32_t>(current)
        + arithmetic_shift_right(difference * fraction, 8));
}

std::int16_t TrigTables::cos_q15(std::uint16_t angle) const noexcept {
    return sin_q15(static_cast<std::uint16_t>(angle + 0x4000U));
}

MatrixQ15 rotation_matrix_q15(
    const TrigTables& trig, std::int16_t x, std::int16_t y, std::int16_t z) noexcept {
    const auto sx = trig.sin_q15(static_cast<std::uint16_t>(x));
    const auto cx = trig.cos_q15(static_cast<std::uint16_t>(x));
    const auto sy = trig.sin_q15(static_cast<std::uint16_t>(y));
    const auto cy = trig.cos_q15(static_cast<std::uint16_t>(y));
    const auto sz = trig.sin_q15(static_cast<std::uint16_t>(z));
    const auto cz = trig.cos_q15(static_cast<std::uint16_t>(z));
    const auto t1 = multiply_q15(cz, sy);
    const auto t2 = multiply_q15(cz, cy);
    const auto t3 = multiply_q15(sz, sy);
    const auto t4 = multiply_q15(sz, cy);
    return {
        add16(multiply_q15(t3, sx), t2),
        subtract16(multiply_q15(t1, sx), t4),
        multiply_q15(cx, sy),
        multiply_q15(cx, sz),
        multiply_q15(cx, cz),
        wrap16(-static_cast<std::int32_t>(sx)),
        subtract16(multiply_q15(t4, sx), t1),
        add16(multiply_q15(t2, sx), t3),
        multiply_q15(cx, cy),
    };
}

MatrixQ15 transpose_q15(const MatrixQ15& matrix) noexcept {
    return {matrix[0], matrix[3], matrix[6],
            matrix[1], matrix[4], matrix[7],
            matrix[2], matrix[5], matrix[8]};
}

MatrixQ15 multiply_matrix_q15(
    const MatrixQ15& left, const MatrixQ15& right) noexcept {
    MatrixQ15 result{};
    for (std::size_t row = 0; row < 3U; ++row) {
        for (std::size_t column = 0; column < 3U; ++column) {
            auto value = multiply_q15(left[row * 3U], right[column]);
            value = add16(value, multiply_q15(
                left[row * 3U + 1U], right[3U + column]));
            result[row * 3U + column] = add16(value, multiply_q15(
                left[row * 3U + 2U], right[6U + column]));
        }
    }
    return result;
}

MatrixQ15 interpolate_rotation_matrix_q15(
    const MatrixQ15& previous,
    const MatrixQ15& current,
    double alpha) noexcept {
    if (alpha <= 0.0) return previous;
    if (alpha >= 1.0) return current;
    alpha = std::clamp(alpha, 0.0, 1.0);

    struct Quaternion {
        double w{};
        double x{};
        double y{};
        double z{};
    };
    constexpr double q15 = 32'768.0;
    const auto normalized = [](Quaternion value) {
        const auto magnitude = std::sqrt(value.w * value.w
            + value.x * value.x + value.y * value.y + value.z * value.z);
        if (magnitude < 1.0e-12) return Quaternion{1.0, 0.0, 0.0, 0.0};
        value.w /= magnitude;
        value.x /= magnitude;
        value.y /= magnitude;
        value.z /= magnitude;
        return value;
    };
    const auto matrix_quaternion = [&normalized](const MatrixQ15& source) {
        std::array<double, 9> matrix{};
        for (std::size_t index = 0U; index < matrix.size(); ++index) {
            matrix[index] = static_cast<double>(source[index]) / q15;
        }
        Quaternion result{};
        const auto trace = matrix[0] + matrix[4] + matrix[8];
        if (trace > 0.0) {
            const auto scale = std::sqrt(trace + 1.0) * 2.0;
            result.w = 0.25 * scale;
            result.x = (matrix[7] - matrix[5]) / scale;
            result.y = (matrix[2] - matrix[6]) / scale;
            result.z = (matrix[3] - matrix[1]) / scale;
        } else if (matrix[0] > matrix[4] && matrix[0] > matrix[8]) {
            const auto scale = std::sqrt(std::max(
                0.0, 1.0 + matrix[0] - matrix[4] - matrix[8])) * 2.0;
            if (scale < 1.0e-12) return Quaternion{1.0, 0.0, 0.0, 0.0};
            result.w = (matrix[7] - matrix[5]) / scale;
            result.x = 0.25 * scale;
            result.y = (matrix[1] + matrix[3]) / scale;
            result.z = (matrix[2] + matrix[6]) / scale;
        } else if (matrix[4] > matrix[8]) {
            const auto scale = std::sqrt(std::max(
                0.0, 1.0 + matrix[4] - matrix[0] - matrix[8])) * 2.0;
            if (scale < 1.0e-12) return Quaternion{1.0, 0.0, 0.0, 0.0};
            result.w = (matrix[2] - matrix[6]) / scale;
            result.x = (matrix[1] + matrix[3]) / scale;
            result.y = 0.25 * scale;
            result.z = (matrix[5] + matrix[7]) / scale;
        } else {
            const auto scale = std::sqrt(std::max(
                0.0, 1.0 + matrix[8] - matrix[0] - matrix[4])) * 2.0;
            if (scale < 1.0e-12) return Quaternion{1.0, 0.0, 0.0, 0.0};
            result.w = (matrix[3] - matrix[1]) / scale;
            result.x = (matrix[2] + matrix[6]) / scale;
            result.y = (matrix[5] + matrix[7]) / scale;
            result.z = 0.25 * scale;
        }
        return normalized(result);
    };
    auto from = matrix_quaternion(previous);
    auto to = matrix_quaternion(current);
    auto cosine = from.w * to.w + from.x * to.x
        + from.y * to.y + from.z * to.z;
    // q and -q are the same orientation. Selecting the positive-dot
    // hemisphere guarantees the shortest presentation arc and prevents the
    // single-frame inversion that normalized matrix-column lerp can produce
    // near an antipodal source step.
    if (cosine < 0.0) {
        to.w = -to.w;
        to.x = -to.x;
        to.y = -to.y;
        to.z = -to.z;
        cosine = -cosine;
    }
    cosine = std::clamp(cosine, -1.0, 1.0);
    auto from_weight = 1.0 - alpha;
    auto to_weight = alpha;
    if (cosine < 0.9995) {
        const auto angle = std::acos(cosine);
        const auto sine = std::sin(angle);
        if (sine > 1.0e-12) {
            from_weight = std::sin((1.0 - alpha) * angle) / sine;
            to_weight = std::sin(alpha * angle) / sine;
        }
    }
    const auto blended = normalized(Quaternion{
        from.w * from_weight + to.w * to_weight,
        from.x * from_weight + to.x * to_weight,
        from.y * from_weight + to.y * to_weight,
        from.z * from_weight + to.z * to_weight,
    });

    const auto xx = blended.x * blended.x;
    const auto yy = blended.y * blended.y;
    const auto zz = blended.z * blended.z;
    const auto xy = blended.x * blended.y;
    const auto xz = blended.x * blended.z;
    const auto yz = blended.y * blended.z;
    const auto wx = blended.w * blended.x;
    const auto wy = blended.w * blended.y;
    const auto wz = blended.w * blended.z;
    const std::array<double, 9> matrix{
        1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz),
        2.0 * (xz + wy), 2.0 * (xy + wz),
        1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx),
        2.0 * (xz - wy), 2.0 * (yz + wx),
        1.0 - 2.0 * (xx + yy),
    };

    MatrixQ15 result{};
    for (std::size_t index = 0U; index < result.size(); ++index) {
        const auto value = std::lround(matrix[index] * q15);
        result[index] = static_cast<std::int16_t>(
            std::clamp<long>(value, -32'768L, 32'767L));
    }
    return result;
}

std::array<std::int16_t, 3> transform_q15(
    const MatrixQ15& matrix,
    const std::array<std::int16_t, 3>& value) noexcept {
    const auto component = [&matrix, &value](std::size_t column) {
        auto result = multiply_q15(value[0], matrix[column]);
        result = add16(result, multiply_q15(value[1], matrix[3U + column]));
        return add16(result, multiply_q15(value[2], matrix[6U + column]));
    };
    return {component(0), component(1), component(2)};
}

} // namespace starfox::simulation
