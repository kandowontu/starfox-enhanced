#include "starfox/render/dust_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>
#include <string>

namespace starfox::render {
namespace {

constexpr std::int16_t kGridWidth = 256;
// Both cartridges' CPU setup subtracts 15*256/2. EX's GSU draw routine
// subsequently visits 25 rows/columns; its origin remains this same value.
constexpr std::int16_t kGridHalfExtent = kGridWidth * 15 / 2;
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

void plot_source_pixel(Framebuffer& target, std::int32_t x, std::int32_t y,
    std::uint8_t colour) noexcept {
    if (target.width() == 224U && target.height() == 192U) {
        // PLOT reads byte coordinates without clipping its secondary pixels
        // or EX line pixels. The 192-row tile layout aliases Y>=192 into the
        // next column; preserve those writes in native-sized output.
        x &= 255;
        y &= 255;
        x += (y / 192) * 8;
        y %= 192;
    }
    target.set(x, y, colour);
}

std::uint16_t source_grid_size(const assets::RomImage& rom,
    const assets::SymbolMap& symbols) {
    const auto entry = symbols.find("MSHOWGRID").at(0);
    const auto size_address = symbols.find("M_GRIDZSIZE").at(0);
    // MSHOWGRID's IWT R0,count / SMS [M_GRIDZSIZE],R0 contains the assembled
    // loop count. EX's ROM uses 25 even though its CPU-origin constant is 15.
    for (unsigned offset = 0; offset < 64; ++offset) {
        const auto pc = entry + offset;
        if (rom.read8(pc) == 0xf0 && rom.read8(pc + 3) == 0x3e
            && rom.read8(pc + 4) == 0xa0
            && rom.read8(pc + 5) == ((size_address & 0x1ffU) >> 1U)) {
            const auto count = rom.read16(pc + 1);
            if (count != 0 && count <= 255) return count;
        }
    }
    throw std::runtime_error{"missing cartridge ground-grid loop count"};
}

std::uint16_t source_grid_two_pixel_depth(const assets::RomImage& rom,
    const assets::SymbolMap& symbols) {
    const auto entry = symbols.find("MGRDRAWDOT3").at(0) - 11;
    if (rom.read8(entry) != 0xf5 || rom.read8(entry + 3) != 0xb9
        || rom.read8(entry + 4) != 0x15 || rom.read8(entry + 5) != 0x65)
        throw std::runtime_error{"missing cartridge ground-grid dot threshold"};
    return rom.read16(entry + 1);
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
    : rom_(&rom), star_colours_(rom_symbol(symbols, "STAR_COLS")),
      snow_colours_(rom_symbol(symbols, "SNOW_COLS")),
      depth_table_(rom_symbol(symbols, "ZTAB")),
      grid_size_(source_grid_size(rom, symbols)),
      grid_two_pixel_depth_(source_grid_two_pixel_depth(rom, symbols)) {}

void DustRenderer::draw(
    const simulation::DustSystem& dust,
    std::size_t active_count,
    const timing::RenderTransform& camera,
    const simulation::MatrixQ15& view_matrix,
    Framebuffer& target, const DustRenderState& state) const noexcept {
    constexpr auto q15 = 32'768.0;
    active_count = std::min(active_count, dust.points().size());
    std::size_t index = 0;
    for (const auto& point : std::span{dust.points()}.first(active_count)) {
        const auto x = source_word_difference(point.x, camera.x);
        const auto y = source_word_difference(point.y, camera.y);
        const auto z = source_word_difference(point.z, camera.z);
        auto camera_x = (x * view_matrix[0] + y * view_matrix[3]
            + z * view_matrix[6]) / q15;
        auto camera_y = (x * view_matrix[1] + y * view_matrix[4]
            + z * view_matrix[7]) / q15;
        auto camera_z = (x * view_matrix[2] + y * view_matrix[5]
            + z * view_matrix[8]) / q15;
        if (!state.subpixel_projection) {
            const auto point = simulation::transform_q15(view_matrix,
                {camera_word(x), camera_word(y), camera_word(z)});
            camera_x = point[0]; camera_y = point[1]; camera_z = point[2];
        }
        if (camera_z < 256.0) {
            ++index;
            continue;
        }
        const auto clipped_z = std::min(camera_z, double(kMaximumReciprocalDepth - 1));
        const auto reciprocal = rom_->read_i16(depth_table_
            + (static_cast<std::uint16_t>(clipped_z) & 0xfffeU));
        const auto project = [&](double coordinate) {
            return state.subpixel_projection
                ? simulation::wrap16(static_cast<std::int32_t>(
                    std::trunc(coordinate * 256.0 / clipped_z)))
                : simulation::multiply_q15(camera_word(coordinate), reciprocal);
        };
        const auto screen_x = simulation::add16(project(camera_x), state.vanish_x);
        const auto screen_y = simulation::add16(project(camera_y), state.vanish_y);
        if (screen_x < 0 || screen_x >= static_cast<std::int32_t>(target.width())
            || screen_y < 0 || screen_y >= static_cast<std::int32_t>(target.height())) {
            ++index;
            continue;
        }
        const auto depth = camera_z < 4096.0 ? static_cast<unsigned>(camera_z) >> 8U : 0U;
        const auto remaining = active_count - index;
        const auto colour = state.planet_stars > 1 ? std::uint8_t{3}
            : state.planet_stars == 1 ? rom_->read8(snow_colours_ + depth)
            : rom_->read8(star_colours_
                + static_cast<std::uint32_t>((remaining & 3U) * 16U + depth));
        if ((colour & 15U) == 0U) { ++index; continue; }
        target.set(screen_x, screen_y,
            static_cast<std::uint8_t>(7U * 16U + colour));
        if (camera_z < 1'024.0) {
            // PLOT already advances X; the source DEC restores the first X.
            plot_source_pixel(target, screen_x, screen_y + 1,
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

    for (std::uint16_t grid_z = 0; grid_z < grid_size_; ++grid_z) {
        auto point = row;
        for (std::uint16_t grid_x = 0; grid_x < grid_size_; ++grid_x) {
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
                    if (static_cast<std::uint16_t>(original_z) < grid_two_pixel_depth_) {
                        plot_source_pixel(target, screen_x, screen_y + 1, colour);
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

void DustRenderer::draw_grid_lines(
    const timing::RenderTransform& camera,
    const simulation::MatrixQ15& view_matrix,
    std::uint64_t source_frame,
    Framebuffer& target) const noexcept {
    const auto new_source_frame = !grid_line_state_initialized_
        || grid_line_source_frame_ != source_frame;
    if (new_source_frame) {
        grid_line_state_initialized_ = true;
        grid_line_source_frame_ = source_frame;
        grid_line_frame_start_x_ = grid_line_previous_x_;
        grid_line_frame_start_y_ = grid_line_previous_y_;
    }
    auto previous_x = grid_line_frame_start_x_;
    auto previous_y = grid_line_frame_start_y_;
    constexpr auto colour = static_cast<std::uint8_t>(7U * 16U + 14U);
    const auto source_line = [&target](
                                 std::int16_t current_x,
                                 std::int16_t current_y,
                                 std::int16_t old_x,
                                 std::int16_t old_y) {
        // MSHOWGRID2 starts at the new point and walks left using DX as its
        // loop counter. PLOT advances X, so the pair of DECs before each PLOT
        // has a net one-pixel leftward step. Negative DX deliberately emits
        // only the first pixel at a projected row wrap.
        auto x = static_cast<std::int32_t>(current_x);
        auto y = static_cast<std::int32_t>(current_y);
        const auto dx = static_cast<std::int32_t>(current_x) - old_x;
        const auto absolute_dx = std::abs(dx);
        const auto absolute_dy = std::abs(
            static_cast<std::int32_t>(current_y) - old_y);
        const auto y_step = current_y < old_y ? 1 : -1;
        auto error = absolute_dx;
        auto remaining = dx;
        do {
            plot_source_pixel(target, x - 2, y, colour);
            --x;
            error -= absolute_dy;
            if (error < 0) {
                y += y_step;
                error += absolute_dx;
            }
            --remaining;
        } while (remaining >= 0);
        return std::array<std::int32_t, 2>{x, y};
    };

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

    for (std::uint16_t grid_z = 0; grid_z < grid_size_; ++grid_z) {
        auto point = row;
        for (std::uint16_t grid_x = 0; grid_x < grid_size_; ++grid_x) {
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
                    // The source first plots a marker at (x-1,y+2), restores
                    // (x-1,y), then connects back to M_PREVX/M_PREVY.
                    const auto adjusted_x = static_cast<std::int16_t>(
                        screen_x - 1);
                    plot_source_pixel(target, adjusted_x, screen_y + 2, colour);
                    const auto endpoint = source_line(adjusted_x, screen_y, previous_x, previous_y);
                    previous_x = adjusted_x;
                    previous_y = screen_y;
                    if (static_cast<std::uint16_t>(original_z) < grid_two_pixel_depth_) {
                        plot_source_pixel(target, endpoint[0] - 1, endpoint[1] + 1, colour);
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
    if (new_source_frame) {
        grid_line_previous_x_ = previous_x;
        grid_line_previous_y_ = previous_y;
    }
}

} // namespace starfox::render
