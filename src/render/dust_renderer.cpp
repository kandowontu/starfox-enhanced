#include "starfox/render/dust_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace starfox::render {
namespace {

constexpr std::int16_t kGridSize = 15;
constexpr std::int16_t kGridWidth = 256;
constexpr std::int16_t kGridHalfExtent = kGridWidth * kGridSize / 2;
constexpr std::int16_t kMaximumReciprocalDepth = 12 * 1'024;

std::int16_t camera_word(double value) noexcept {
    return simulation::wrap16(static_cast<std::int64_t>(std::trunc(value)));
}

std::int16_t grid_start(std::int16_t camera) noexcept {
    const auto phase = static_cast<std::uint16_t>(camera) & 0xffU;
    return simulation::wrap16(
        static_cast<std::int32_t>(phase ^ 0xffU) - kGridHalfExtent);
}

std::int16_t matrix_grid_step(std::int16_t value) noexcept {
    return simulation::wrap16(simulation::arithmetic_shift_right(value, 7));
}

std::int16_t grid_projection(std::int16_t coordinate, std::int16_t depth) noexcept {
    const auto even_depth = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(depth) & 0xfffeU);
    const auto reciprocal = static_cast<std::int16_t>(
        (32'767 * 256) / even_depth);
    return simulation::multiply_q15(coordinate, reciprocal);
}

double source_word_difference(double value, double origin) noexcept {
    auto difference = std::fmod(value - origin, 65'536.0);
    if (difference > 32'767.0) difference -= 65'536.0;
    else if (difference < -32'768.0) difference += 65'536.0;
    return difference;
}

std::uint32_t rom_symbol(
    const assets::SymbolMap& symbols, const char* name) {
    for (const auto address : symbols.find(name)) {
        if ((address & 0xffffU) >= 0x8000U
            && ((address >> 16U) & 0xffU) < 0x7eU) return address;
    }
    throw std::runtime_error{std::string{"missing dust ROM symbol: "} + name};
}

} // namespace

DustRenderer::DustRenderer(
    const assets::RomImage& rom,
    const assets::SymbolMap& symbols)
    : rom_(&rom), star_colours_(rom_symbol(symbols, "STAR_COLS")) {}

void DustRenderer::draw(
    const simulation::DustSystem& dust,
    const timing::RenderTransform& camera,
    const simulation::MatrixQ15& view_matrix,
    Framebuffer& target) const noexcept {
    constexpr auto q15 = 32'768.0;
    std::size_t index = 0;
    for (const auto& point : dust.points()) {
        const auto x = source_word_difference(point.x, camera.x);
        const auto y = source_word_difference(point.y, camera.y);
        const auto z = source_word_difference(point.z, camera.z);
        const auto camera_x = (x * view_matrix[0] + y * view_matrix[3]
            + z * view_matrix[6]) / q15;
        const auto camera_y = (x * view_matrix[1] + y * view_matrix[4]
            + z * view_matrix[7]) / q15;
        const auto camera_z = (x * view_matrix[2] + y * view_matrix[5]
            + z * view_matrix[8]) / q15;
        if (camera_z < 256.0) {
            ++index;
            continue;
        }
        const auto clipped_z = std::min(camera_z, 4'095.0);
        const auto screen_x = static_cast<std::int32_t>(target.width() / 2U)
            + static_cast<std::int32_t>(
                std::trunc(camera_x * 256.0 / clipped_z));
        const auto screen_y = static_cast<std::int32_t>(target.height() / 2U)
            + static_cast<std::int32_t>(
                std::trunc(camera_y * 256.0 / clipped_z));
        if (screen_x < 0 || screen_x >= static_cast<std::int32_t>(target.width())
            || screen_y < 0 || screen_y >= static_cast<std::int32_t>(target.height())) {
            ++index;
            continue;
        }
        const auto depth = static_cast<std::uint8_t>(
            std::clamp(static_cast<int>(clipped_z) >> 8, 0, 15));
        const auto remaining = simulation::kMaximumDustPoints - index;
        const auto colour = rom_->read8(star_colours_
            + static_cast<std::uint32_t>((remaining & 3U) * 16U + depth));
        target.set(screen_x, screen_y,
            static_cast<std::uint8_t>(7U * 16U + colour));
        if (camera_z < 1'024.0) {
            target.set(screen_x - 1, screen_y + 1,
                static_cast<std::uint8_t>(7U * 16U + colour));
        }
        ++index;
    }
}

void DustRenderer::draw_grid(
    const timing::RenderTransform& camera,
    const simulation::MatrixQ15& view_matrix,
    Framebuffer& target) const noexcept {
    const auto camera_x = camera_word(camera.x);
    const auto camera_y = camera_word(camera.y);
    const auto camera_z = camera_word(camera.z);
    auto row = simulation::transform_q15(view_matrix, {
        grid_start(camera_x),
        simulation::wrap16(-static_cast<std::int32_t>(camera_y)),
        grid_start(camera_z),
    });

    const std::array<std::int16_t, 3> x_step{
        matrix_grid_step(view_matrix[0]),
        matrix_grid_step(view_matrix[1]),
        matrix_grid_step(view_matrix[2]),
    };
    const std::array<std::int16_t, 3> z_step{
        matrix_grid_step(view_matrix[6]),
        matrix_grid_step(view_matrix[7]),
        matrix_grid_step(view_matrix[8]),
    };

    for (std::int16_t grid_z = 0; grid_z < kGridSize; ++grid_z) {
        auto point = row;
        for (std::int16_t grid_x = 0; grid_x < kGridSize; ++grid_x) {
            const auto original_z = point[2];
            if (original_z > 256) {
                const auto depth = std::min<std::int16_t>(
                    original_z, kMaximumReciprocalDepth - 1);
                const auto screen_x = simulation::add16(
                    grid_projection(point[0], depth),
                    static_cast<std::int16_t>(target.width() / 2U));
                const auto screen_y = simulation::add16(
                    grid_projection(point[1], depth),
                    static_cast<std::int16_t>(target.height() / 2U));
                if (static_cast<std::uint16_t>(screen_x) < target.width()
                    && static_cast<std::uint16_t>(screen_y) < target.height()) {
                    constexpr auto colour = static_cast<std::uint8_t>(
                        7U * 16U + 14U);
                    target.set(screen_x, screen_y, colour);
                    if (original_z < 512) {
                        target.set(screen_x - 1, screen_y + 1, colour);
                    }
                }
            }
            for (std::size_t axis = 0; axis < 3U; ++axis) {
                point[axis] = simulation::add16(point[axis], x_step[axis]);
            }
        }
        for (std::size_t axis = 0; axis < 3U; ++axis) {
            row[axis] = simulation::add16(row[axis], z_step[axis]);
        }
    }
}

} // namespace starfox::render
