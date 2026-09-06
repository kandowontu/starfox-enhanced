#include "starfox/render/software_renderer.hpp"

#include "starfox/simulation/math.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <functional>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace starfox::render {
namespace {

constexpr double kTau = 6.283185307179586476925286766559;

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

struct ScreenPoint {
    double x{};
    double y{};
    double z{};
    bool visible{};
};

struct FaceColour {
    std::uint8_t even{};
    std::uint8_t odd{};
    bool dither{};
};

struct FaceMaterial {
    FaceColour colour{};
    const assets::TextureImage* texture{};
};

struct TexturePoint {
    double u{};
    double v{};
};

struct RasterVertex {
    ScreenPoint point{};
    TexturePoint texture{};
};

std::int16_t rounded_word(double value) {
    return starfox::simulation::wrap16(
        static_cast<std::int64_t>(std::lround(value)));
}

std::int16_t midpoint_word(std::int16_t a, std::int16_t b) {
    const auto sum = starfox::simulation::add16(a, b);
    // Super FX DIV2 is ASR with its documented -1 -> 0 correction.
    if (sum == -1) return 0;
    return starfox::simulation::wrap16(
        starfox::simulation::arithmetic_shift_right(sum, 1));
}

Vec3 near_intersection(Vec3 a, Vec3 b, bool word_exact) {
    if (!word_exact) {
        const auto denominator = a.z - b.z;
        const auto amount = denominator == 0.0 ? 0.0 : a.z / denominator;
        return {
            a.x + (b.x - a.x) * amount,
            a.y + (b.y - a.y) * amount,
            0.0,
        };
    }

    auto ax = rounded_word(a.x);
    auto ay = rounded_word(a.y);
    auto az = rounded_word(a.z);
    auto bx = rounded_word(b.x);
    auto by = rounded_word(b.y);
    auto bz = rounded_word(b.z);
    if (az < 0 && bz >= 0) {
        std::swap(ax, bx);
        std::swap(ay, by);
        std::swap(az, bz);
    }

    // MCLIP.MC repeatedly bisects the camera-space edge with 16-bit ADD and
    // DIV2 instructions. Its final point is the first midpoint whose z is 0.
    for (auto iteration = 0; iteration < 32; ++iteration) {
        const auto middle_z = midpoint_word(az, bz);
        const auto middle_x = midpoint_word(ax, bx);
        const auto middle_y = midpoint_word(ay, by);
        if (middle_z == 0) {
            return {static_cast<double>(middle_x),
                static_cast<double>(middle_y), 0.0};
        }
        if (middle_z < 0) {
            if (middle_z == bz && middle_x == bx && middle_y == by) break;
            bx = middle_x;
            by = middle_y;
            bz = middle_z;
        } else {
            if (middle_z == az && middle_x == ax && middle_y == ay) break;
            ax = middle_x;
            ay = middle_y;
            az = middle_z;
        }
    }

    // Retain a safe fallback for malformed host-authored preview geometry.
    if (az == 0) return {static_cast<double>(ax), static_cast<double>(ay), 0.0};
    if (bz == 0) return {static_cast<double>(bx), static_cast<double>(by), 0.0};
    const auto denominator = static_cast<double>(az) - bz;
    const auto amount = denominator == 0.0 ? 0.0
        : static_cast<double>(az) / denominator;
    return {
        std::trunc(ax + (bx - ax) * amount),
        std::trunc(ay + (by - ay) * amount),
        0.0,
    };
}

bool clip_near_line(Vec3& a, Vec3& b, bool word_exact) {
    const auto a_behind = a.z < 0.0;
    const auto b_behind = b.z < 0.0;
    if (a_behind && b_behind) return false;
    if (a_behind) a = near_intersection(a, b, word_exact);
    if (b_behind) b = near_intersection(a, b, word_exact);
    return true;
}

std::vector<Vec3> clip_near_polygon(
    const std::vector<Vec3>& input, bool word_exact) {
    std::vector<Vec3> output;
    if (input.empty()) return output;
    output.reserve(input.size() + 2U);
    for (std::size_t index = 0; index < input.size(); ++index) {
        const auto& current = input[index];
        const auto& next = input[(index + 1U) % input.size()];
        const auto current_inside = current.z >= 0.0;
        const auto next_inside = next.z >= 0.0;
        if (current_inside) {
            output.push_back(current);
            if (!next_inside) {
                output.push_back(near_intersection(current, next, word_exact));
            }
        } else if (next_inside) {
            output.push_back(near_intersection(current, next, word_exact));
        }
    }
    return output;
}

ScreenPoint project_point(
    const Vec3& point,
    double focal_length,
    bool word_exact,
    double vanish_x,
    double vanish_y,
    bool subpixel = false) {
    if (word_exact && focal_length == 256.0) {
        const auto original_z = rounded_word(point.z);
        auto z = original_z;
        if (z < 0) z = starfox::simulation::wrap16(-static_cast<std::int32_t>(z));
        if (z == 0) z = 1;
        const auto magnitude = [](std::int16_t value) {
            return value < 0
                ? static_cast<std::uint16_t>(
                    -static_cast<std::uint16_t>(value))
                : static_cast<std::uint16_t>(value);
        };
        const auto x_word = rounded_word(point.x);
        const auto y_word = rounded_word(point.y);
        const auto x_magnitude = magnitude(x_word);
        const auto y_magnitude = magnitude(y_word);
        const auto z_magnitude = static_cast<std::uint16_t>(z);
        std::uint16_t projected_x{};
        std::uint16_t projected_y{};
        const auto dominant = std::max(x_magnitude, y_magnitude);
        if (dominant != 0U) {
            // MDO_PROJECT performs an unsigned (coordinate*256)/z. Once the
            // dominant quotient reaches 16384 it saturates that axis at
            // 16383 and derives the minor axis from the original ratio.
            const auto dominant_quotient =
                (static_cast<std::uint32_t>(dominant) << 8U) / z_magnitude;
            if (dominant_quotient >= 16'384U) {
                const auto minor = std::min(x_magnitude, y_magnitude);
                const auto minor_projected = static_cast<std::uint16_t>(
                    static_cast<std::uint32_t>(minor) * 16'383U / dominant);
                if (x_magnitude >= y_magnitude) {
                    projected_x = 16'383U;
                    projected_y = minor_projected;
                } else {
                    projected_y = 16'383U;
                    projected_x = minor_projected;
                }
            } else {
                projected_x = static_cast<std::uint16_t>(
                    (static_cast<std::uint32_t>(x_magnitude) << 8U) / z_magnitude);
                projected_y = static_cast<std::uint16_t>(
                    (static_cast<std::uint32_t>(y_magnitude) << 8U) / z_magnitude);
            }
        }
        const auto signed_projection = [original_z](
            std::uint16_t projected, std::int16_t coordinate) {
            const auto negative = (coordinate < 0) != (original_z < 0);
            return negative
                ? starfox::simulation::wrap16(-static_cast<std::int32_t>(projected))
                : static_cast<std::int16_t>(projected);
        };
        const auto projected_x_word = starfox::simulation::add16(
            signed_projection(projected_x, x_word),
            rounded_word(vanish_x));
        const auto projected_y_word = starfox::simulation::add16(
            signed_projection(projected_y, y_word),
            rounded_word(vanish_y));
        return {static_cast<double>(projected_x_word),
            static_cast<double>(projected_y_word), point.z, original_z >= 0};
    }
    const auto projection_z = point.z == 0.0 ? 1.0 : point.z;
    const auto projected_x = point.x * focal_length / projection_z;
    const auto projected_y = point.y * focal_length / projection_z;
    return {
        vanish_x + (subpixel ? projected_x : std::trunc(projected_x)),
        vanish_y + (subpixel ? projected_y : std::trunc(projected_y)),
        point.z,
        point.z >= 0.0,
    };
}

bool source_visibility(
    const assets::Visibility& visibility,
    const std::vector<ScreenPoint>& projected,
    const std::vector<Vec3>& transformed) {
    if (visibility.a >= projected.size() || visibility.b >= projected.size()
        || visibility.c >= projected.size()) {
        return false;
    }
    const auto& a = projected[visibility.a];
    const auto& b = projected[visibility.b];
    const auto& c = projected[visibility.c];
    // MSH_VIZIS performs all four projected differences in 16-bit Super FX
    // registers, then subtracts the two signed 16x16 products as a wrapping
    // 32-bit value. Using host doubles here gave a different facing result
    // whenever a near-plane projection crossed the signed screen-word seam;
    // the intro lasers would then expose a broad side face as a full-screen
    // wedge, especially while Original Speed held that source frame longer.
    const auto bx = starfox::simulation::subtract16(
        rounded_word(b.x), rounded_word(a.x));
    const auto by = starfox::simulation::subtract16(
        rounded_word(b.y), rounded_word(a.y));
    const auto cx = starfox::simulation::subtract16(
        rounded_word(c.x), rounded_word(a.x));
    const auto cy = starfox::simulation::subtract16(
        rounded_word(c.y), rounded_word(a.y));
    const auto cross = static_cast<std::int64_t>(bx) * cy
        - static_cast<std::int64_t>(by) * cx;
    const auto signed_area = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(cross));
    const auto odd_behind = (transformed[visibility.a].z < 0.0)
        != ((transformed[visibility.b].z < 0.0)
            != (transformed[visibility.c].z < 0.0));
    return (signed_area < 0) != odd_behind;
}

bool continuous_visibility(
    const assets::Visibility& visibility,
    const std::vector<Vec3>& transformed) {
    if (visibility.a >= transformed.size()
        || visibility.b >= transformed.size()
        || visibility.c >= transformed.size()) {
        return false;
    }
    const auto& a = transformed[visibility.a];
    const auto& b = transformed[visibility.b];
    const auto& c = transformed[visibility.c];
    // The signed projected area, after source_visibility's odd-behind
    // correction, has the same sign as this camera-space determinant. Use
    // that equivalent form for Render Upscale so a visibility triple close
    // to z=0 cannot wrap or quantize through a different facing for one host
    // presentation. At the exact tangent the projected source area is
    // undefined/zero; retaining the face prevents a one-frame hole between
    // the two otherwise-visible neighbouring poses.
    const auto determinant =
        a.x * (b.y * c.z - b.z * c.y)
        - a.y * (b.x * c.z - b.z * c.x)
        + a.z * (b.x * c.y - b.y * c.x);
    const auto coordinate_scale = std::max({
        std::abs(a.x), std::abs(a.y), std::abs(a.z),
        std::abs(b.x), std::abs(b.y), std::abs(b.z),
        std::abs(c.x), std::abs(c.y), std::abs(c.z), 1.0});
    const auto tangent_tolerance = coordinate_scale * coordinate_scale
        * coordinate_scale * 1.0e-12;
    return determinant <= tangent_tolerance;
}

RasterVertex interpolate_at(
    const RasterVertex& a, const RasterVertex& b, double amount) {
    return {
        {
            a.point.x + (b.point.x - a.point.x) * amount,
            a.point.y + (b.point.y - a.point.y) * amount,
            a.point.z + (b.point.z - a.point.z) * amount,
            true,
        },
        {
            a.texture.u + (b.texture.u - a.texture.u) * amount,
            a.texture.v + (b.texture.v - a.texture.v) * amount,
        },
    };
}

template <typename Inside, typename Intersect>
std::vector<RasterVertex> clip_screen_edge(
    const std::vector<RasterVertex>& input,
    Inside inside,
    Intersect intersect) {
    std::vector<RasterVertex> output;
    if (input.empty()) return output;
    output.reserve(input.size() + 2U);
    auto previous = input.back();
    auto previous_inside = inside(previous);
    for (const auto& current : input) {
        const auto current_inside = inside(current);
        if (current_inside != previous_inside) {
            output.push_back(intersect(previous, current));
        }
        if (current_inside) output.push_back(current);
        previous = current;
        previous_inside = current_inside;
    }
    return output;
}

// Scan conversion is the only stage that follows the render scale. Projection,
// visibility and clipping all stay on the source raster, so a scaled frame
// resolves exactly the same geometry with finer scan lines.
class StoredRasterScope {
public:
    StoredRasterScope(Framebuffer& target, std::uint32_t scale) noexcept
        : target_(target), previous_(target.draw_scale()) {
        if (scale > 1U) target_.set_draw_scale(1U);
    }
    ~StoredRasterScope() { target_.set_draw_scale(previous_); }
    StoredRasterScope(const StoredRasterScope&) = delete;
    StoredRasterScope& operator=(const StoredRasterScope&) = delete;

private:
    Framebuffer& target_;
    std::uint32_t previous_;
};

void scale_to_stored(ScreenPoint& point, std::uint32_t scale) noexcept {
    if (scale <= 1U) return;
    point.x *= static_cast<double>(scale);
    point.y *= static_cast<double>(scale);
}

std::vector<RasterVertex> clip_screen_polygon(
    std::vector<RasterVertex> polygon,
    const Framebuffer& target,
    bool source_exact) {
    // MOBJ.MC stores m_xright/m_ybot as the first coordinates outside the
    // viewport (224/192), and MCLIP.MC intersects edges with those values.
    // Keeping the intersection on that exclusive plane is important because
    // the source scan converter omits its terminal bottom scanline.
    const auto right = static_cast<double>(target.width());
    const auto bottom = static_cast<double>(target.height());
    if (source_exact) {
        const auto source_value_at = [](std::int16_t inside_value,
                                         std::int16_t outside_value,
                                         std::int16_t inside_axis,
                                         std::int16_t outside_axis,
                                         std::int16_t boundary) {
            const auto numerator = starfox::simulation::subtract16(
                boundary, inside_axis);
            const auto denominator = starfox::simulation::subtract16(
                outside_axis, inside_axis);
            const auto difference = starfox::simulation::subtract16(
                outside_value, inside_value);
            if (denominator == 0) return inside_value;
            const auto quotient = static_cast<std::int32_t>(numerator)
                * static_cast<std::int32_t>(difference) / denominator;
            return starfox::simulation::add16(inside_value,
                starfox::simulation::wrap16(quotient));
        };
        const auto clip_edge = [&source_value_at](
                                   const std::vector<RasterVertex>& input,
                                   bool vertical,
                                   std::int16_t boundary,
                                   bool keep_less) {
            std::vector<RasterVertex> output;
            if (input.empty()) return output;
            output.reserve(input.size() + 2U);
            const auto coordinate = [vertical](const RasterVertex& vertex) {
                return rounded_word(vertical
                    ? vertex.point.x : vertex.point.y);
            };
            const auto inside = [&coordinate, boundary, keep_less](
                                    const RasterVertex& vertex) {
                const auto value = coordinate(vertex);
                return keep_less ? value < boundary : value >= boundary;
            };
            auto previous = input.back();
            auto previous_inside = inside(previous);
            for (const auto& current : input) {
                const auto current_inside = inside(current);
                if (current_inside != previous_inside) {
                    // MCLIP.MC's *2 entry points swap an outside->inside
                    // edge, so every signed division is anchored at the
                    // endpoint that remains inside the clip window.
                    const auto& anchor = previous_inside ? previous : current;
                    const auto& outside = previous_inside ? current : previous;
                    const auto anchor_axis = coordinate(anchor);
                    const auto outside_axis = coordinate(outside);
                    auto intersection = anchor;
                    if (vertical) {
                        intersection.point.x = boundary;
                        intersection.point.y = source_value_at(
                            rounded_word(anchor.point.y),
                            rounded_word(outside.point.y),
                            anchor_axis, outside_axis, boundary);
                    } else {
                        intersection.point.x = source_value_at(
                            rounded_word(anchor.point.x),
                            rounded_word(outside.point.x),
                            anchor_axis, outside_axis, boundary);
                        intersection.point.y = boundary;
                    }
                    intersection.texture.u = source_value_at(
                        rounded_word(anchor.texture.u),
                        rounded_word(outside.texture.u),
                        anchor_axis, outside_axis, boundary);
                    intersection.texture.v = source_value_at(
                        rounded_word(anchor.texture.v),
                        rounded_word(outside.texture.v),
                        anchor_axis, outside_axis, boundary);
                    intersection.point.visible = true;
                    output.push_back(intersection);
                }
                if (current_inside) output.push_back(current);
                previous = current;
                previous_inside = current_inside;
            }
            return output;
        };
        polygon = clip_edge(polygon, true, 0, false);
        polygon = clip_edge(polygon, true,
            static_cast<std::int16_t>(target.width()), true);
        polygon = clip_edge(polygon, false, 0, false);
        polygon = clip_edge(polygon, false,
            static_cast<std::int16_t>(target.height()), true);
        return polygon;
    }
    polygon = clip_screen_edge(polygon,
        [](const RasterVertex& vertex) { return vertex.point.x >= 0.0; },
        [](const RasterVertex& a, const RasterVertex& b) {
            const auto denominator = b.point.x - a.point.x;
            return interpolate_at(a, b,
                denominator == 0.0 ? 0.0 : -a.point.x / denominator);
        });
    polygon = clip_screen_edge(polygon,
        [right](const RasterVertex& vertex) { return vertex.point.x < right; },
        [right](const RasterVertex& a, const RasterVertex& b) {
            const auto denominator = b.point.x - a.point.x;
            return interpolate_at(a, b, denominator == 0.0 ? 0.0
                : (right - a.point.x) / denominator);
        });
    polygon = clip_screen_edge(polygon,
        [](const RasterVertex& vertex) { return vertex.point.y >= 0.0; },
        [](const RasterVertex& a, const RasterVertex& b) {
            const auto denominator = b.point.y - a.point.y;
            return interpolate_at(a, b,
                denominator == 0.0 ? 0.0 : -a.point.y / denominator);
        });
    polygon = clip_screen_edge(polygon,
        [bottom](const RasterVertex& vertex) { return vertex.point.y < bottom; },
        [bottom](const RasterVertex& a, const RasterVertex& b) {
            const auto denominator = b.point.y - a.point.y;
            return interpolate_at(a, b, denominator == 0.0 ? 0.0
                : (bottom - a.point.y) / denominator);
        });
    return polygon;
}

bool clip_screen_line(
    ScreenPoint& a,
    ScreenPoint& b,
    const Framebuffer& target,
    bool source_exact) {
    if (source_exact) {
        const auto clip_edge = [&a, &b](
                                   bool vertical,
                                   std::int16_t boundary,
                                   bool keep_less) {
            const auto coordinate = [vertical](const ScreenPoint& point) {
                return rounded_word(vertical ? point.x : point.y);
            };
            const auto inside = [keep_less, boundary](std::int16_t value) {
                return keep_less ? value < boundary : value >= boundary;
            };
            const auto a_axis = coordinate(a);
            const auto b_axis = coordinate(b);
            const auto a_inside = inside(a_axis);
            const auto b_inside = inside(b_axis);
            if (!a_inside && !b_inside) return false;
            if (a_inside && b_inside) return true;
            auto& outside = a_inside ? b : a;
            const auto& anchor = a_inside ? a : b;
            const auto anchor_axis = coordinate(anchor);
            const auto outside_axis = coordinate(outside);
            const auto denominator = starfox::simulation::subtract16(
                outside_axis, anchor_axis);
            const auto interpolate = [anchor_axis, outside_axis, boundary,
                                      denominator](double inside_value,
                                                   double outside_value) {
                if (denominator == 0) return rounded_word(inside_value);
                const auto numerator = starfox::simulation::subtract16(
                    boundary, anchor_axis);
                const auto difference = starfox::simulation::subtract16(
                    rounded_word(outside_value), rounded_word(inside_value));
                const auto quotient = static_cast<std::int32_t>(numerator)
                    * static_cast<std::int32_t>(difference) / denominator;
                return starfox::simulation::add16(
                    rounded_word(inside_value),
                    starfox::simulation::wrap16(quotient));
            };
            if (vertical) {
                outside.x = boundary;
                outside.y = interpolate(anchor.y, outside.y);
            } else {
                outside.x = interpolate(anchor.x, outside.x);
                outside.y = boundary;
            }
            outside.visible = true;
            return true;
        };
        return clip_edge(true, 0, false)
            && clip_edge(true, static_cast<std::int16_t>(target.width()), true)
            && clip_edge(false, 0, false)
            && clip_edge(false, static_cast<std::int16_t>(target.height()), true);
    }
    auto first = RasterVertex{a, {}};
    auto second = RasterVertex{b, {}};
    auto enter = 0.0;
    auto leave = 1.0;
    const auto dx = second.point.x - first.point.x;
    const auto dy = second.point.y - first.point.y;
    const auto clip = [&enter, &leave](double direction, double distance) {
        if (direction == 0.0) return distance >= 0.0;
        const auto amount = distance / direction;
        if (direction < 0.0) {
            if (amount > leave) return false;
            enter = std::max(enter, amount);
        } else {
            if (amount < enter) return false;
            leave = std::min(leave, amount);
        }
        return true;
    };
    const auto right = static_cast<double>(target.width());
    const auto bottom = static_cast<double>(target.height());
    if (!clip(-dx, first.point.x)
        || !clip(dx, right - first.point.x)
        || !clip(-dy, first.point.y)
        || !clip(dy, bottom - first.point.y)
        || enter > leave) {
        return false;
    }
    const auto original_first = first;
    const auto original_second = second;
    first = interpolate_at(original_first, original_second, enter);
    second = interpolate_at(original_first, original_second, leave);
    a = first.point;
    b = second.point;
    return true;
}

Vec3 rotate(const assets::Vec3i& point, const RenderPose& pose, std::uint8_t shift) {
    const auto factor = pose.scale * static_cast<double>(std::uint32_t{1} << shift);
    Vec3 value{point.x * factor, point.y * factor, point.z * factor};
    if (pose.use_rotation_matrix) {
        if (pose.subpixel_projection) {
            constexpr auto q15 = 32'768.0;
            const auto transform = [&pose, &value](std::size_t column) {
                return value.x * pose.rotation_matrix[column] / q15
                    + value.y * pose.rotation_matrix[3U + column] / q15
                    + value.z * pose.rotation_matrix[6U + column] / q15;
            };
            value = {transform(0), transform(1), transform(2)};
            value.x += pose.x;
            value.y += pose.y;
            value.z += pose.z;
            return value;
        }
        const auto transform = [&pose, &value](std::size_t column) {
            const auto x = starfox::simulation::wrap16(
                static_cast<std::int64_t>(std::lround(value.x)));
            const auto y = starfox::simulation::wrap16(
                static_cast<std::int64_t>(std::lround(value.y)));
            const auto z = starfox::simulation::wrap16(
                static_cast<std::int64_t>(std::lround(value.z)));
            auto result = starfox::simulation::multiply_q15(
                x, pose.rotation_matrix[column]);
            result = starfox::simulation::add16(result,
                starfox::simulation::multiply_q15(
                    y, pose.rotation_matrix[3U + column]));
            result = starfox::simulation::add16(result,
                starfox::simulation::multiply_q15(
                    z, pose.rotation_matrix[6U + column]));
            return static_cast<double>(result);
        };
        value = {transform(0), transform(1), transform(2)};
        value.x = starfox::simulation::add16(rounded_word(value.x),
            rounded_word(pose.x));
        value.y = starfox::simulation::add16(rounded_word(value.y),
            rounded_word(pose.y));
        value.z = starfox::simulation::add16(rounded_word(value.z),
            rounded_word(pose.z));
        return value;
    }
    const auto pitch = pose.pitch * kTau / 65'536.0;
    const auto yaw = pose.yaw * kTau / 65'536.0;
    const auto roll = pose.roll * kTau / 65'536.0;

    const auto cx = std::cos(pitch);
    const auto sx = std::sin(pitch);
    const auto cy = std::cos(yaw);
    const auto sy = std::sin(yaw);
    const auto cz = std::cos(roll);
    const auto sz = std::sin(roll);

    value = {value.x, value.y * cx - value.z * sx, value.y * sx + value.z * cx};
    value = {value.x * cy + value.z * sy, value.y, -value.x * sy + value.z * cy};
    value = {value.x * cz - value.y * sz, value.x * sz + value.y * cz, value.z};
    value.x += pose.x;
    value.y += pose.y;
    value.z += pose.z;
    return value;
}

Vec3 explosion_offset(
    const assets::Face& face, const RenderPose& pose) {
    if (pose.explosion_progress == 0U) return {};
    auto direction_pose = pose;
    direction_pose.x = 0.0;
    direction_pose.y = 0.0;
    direction_pose.z = 0.0;
    direction_pose.scale = 1.0;
    // MOBJ's mexpfacesinit negates the source X/Z face-normal bytes, rotates
    // that vector through m_mat, then forces Y downward before multiplying by
    // al_count and dividing by four.
    auto direction = rotate(
        {-face.normal.x, face.normal.y, -face.normal.z}, direction_pose, 0U);
    direction.y = -std::abs(direction.y);
    const auto expand = [progress = pose.explosion_progress](double value) {
        const auto product = static_cast<std::int32_t>(std::lround(value))
            * static_cast<std::int32_t>(progress);
        return static_cast<double>(
            starfox::simulation::arithmetic_shift_right(product, 2U));
    };
    return {expand(direction.x), expand(direction.y), expand(direction.z)};
}

std::int32_t wide_edge_increment(
    std::int32_t difference, std::int32_t scanlines) noexcept {
    if (scanlines <= 0) return 0;
    const auto reciprocal = scanlines == 1
        ? 32'767 : 32'768 / scanlines;
    return starfox::simulation::arithmetic_shift_right(
        static_cast<std::int32_t>(difference * reciprocal), 7);
}

std::int16_t source_edge_increment(
    std::int32_t difference, std::int32_t scanlines) noexcept {
    return starfox::simulation::wrap16(
        wide_edge_increment(difference, scanlines));
}

std::int16_t source_advance_edge(
    std::int16_t position, std::int16_t increment) noexcept {
    auto advanced = starfox::simulation::add16(position, increment);
    // MDRAWP.MC's deliberate workaround catches only the narrow wrapped
    // interval immediately below x=0. Valid clipped x>=128 also has bit 15
    // set, but remains negative after adding $0800 and is left untouched.
    if (advanced < 0
        && starfox::simulation::add16(advanced, 0x0800) >= 0) {
        advanced = 0;
    }
    return advanced;
}

std::int32_t source_fixed_integer(std::int16_t value) noexcept {
    return static_cast<std::uint16_t>(value) >> 8U;
}

void fill_source_polygon(
    Framebuffer& target,
    const std::vector<RasterVertex>& polygon,
    FaceColour colour,
    std::uint8_t colour_index_base,
    const RenderPose& pose,
    bool winding_independent,
    SurfaceBuffer* surfaces,
    const SurfaceSample& surface) {
    if (polygon.size() < 3U) return;
    struct Point {
        std::int32_t x{};
        std::int32_t y{};
    };
    std::vector<Point> points;
    points.reserve(polygon.size());
    for (const auto& vertex : polygon) {
        points.push_back({
            std::clamp(static_cast<std::int32_t>(std::lround(vertex.point.x)),
                0, static_cast<std::int32_t>(target.width())),
            std::clamp(static_cast<std::int32_t>(std::lround(vertex.point.y)),
                0, static_cast<std::int32_t>(target.height())),
        });
    }

    auto minimum = std::size_t{};
    auto maximum_y = points.front().y;
    for (std::size_t index = 1; index < points.size(); ++index) {
        if (points[index].y < points[minimum].y) minimum = index;
        maximum_y = std::max(maximum_y, points[index].y);
    }
    auto y = points[minimum].y;
    if (y == maximum_y) return;

    // The source scan converter keeps X in an unsigned 8.8 word. That covers
    // the retail 0..224 clip window, but an exclusive X=256 boundary wraps to
    // zero, while the source's -8..0 overflow guard mistakes valid X=248..255
    // for a negative edge. Use an unwrapped host accumulator only when the
    // viewport is wider than the cartridge raster; the 224-pixel path below
    // retains the exact word arithmetic used by MDRAWP.MC.
    const auto extended_x = target.width() > 224U;
    const auto fixed_x = [extended_x](std::int32_t value) {
        return extended_x
            ? value << 8U
            : static_cast<std::int32_t>(starfox::simulation::wrap16(
                static_cast<std::int64_t>(value) << 8U));
    };
    const auto integer_x = [extended_x](std::int32_t value) {
        return extended_x
            ? starfox::simulation::arithmetic_shift_right(value, 8)
            : source_fixed_integer(static_cast<std::int16_t>(value));
    };
    const auto rounded_x = [extended_x, &integer_x](std::int32_t value) {
        return extended_x
            ? starfox::simulation::arithmetic_shift_right(value + 127, 8)
            : integer_x(starfox::simulation::add16(
                static_cast<std::int16_t>(value), 127));
    };
    const auto advance_x = [extended_x](
                               std::int32_t value, std::int32_t increment) {
        return extended_x
            ? value + increment
            : static_cast<std::int32_t>(source_advance_edge(
                static_cast<std::int16_t>(value),
                static_cast<std::int16_t>(increment)));
    };
    const auto edge_increment = [extended_x](
                                    std::int32_t difference,
                                    std::int32_t scanlines) {
        return extended_x
            ? wide_edge_increment(difference, scanlines)
            : static_cast<std::int32_t>(
                source_edge_increment(difference, scanlines));
    };

    struct Tracer {
        std::size_t vertex{};
        int direction{};
        std::int32_t x{};
        std::int32_t increment{};
        std::int32_t remaining{};
    };
    const auto initial_x = fixed_x(points[minimum].x);
    Tracer left{minimum, 1, initial_x, 0, 0};
    Tracer right{minimum, -1, initial_x, 0, 0};
    const auto begin_segment = [
                                   &points, &y, &fixed_x, &rounded_x,
                                   &edge_increment](Tracer& tracer) {
        const auto count = points.size();
        auto rounded = rounded_x(tracer.x);
        for (std::size_t guard = 0; guard < count; ++guard) {
            tracer.vertex = tracer.direction > 0
                ? (tracer.vertex + 1U) % count
                : (tracer.vertex + count - 1U) % count;
            const auto& endpoint = points[tracer.vertex];
            const auto scanlines = endpoint.y - y;
            if (scanlines < 0) return false;
            if (scanlines == 0) {
                rounded = endpoint.x;
                tracer.x = fixed_x(rounded);
                continue;
            }
            tracer.x = fixed_x(rounded);
            tracer.increment = edge_increment(
                endpoint.x - rounded, scanlines);
            tracer.remaining = scanlines;
            return true;
        }
        return false;
    };

    constexpr std::array<std::int8_t, 32> wave_sine{
        0, 1, 2, 3, 3, 3, 2, 1,
        0, -1, -2, -3, -3, -3, -2, -1,
        0, 1, 2, 3, 3, 3, 2, 1,
        0, -1, -2, -3, -3, -3, -2, -1,
    };
    const auto wave_y = [&pose, &wave_sine](
                            std::int32_t x, std::int32_t scanline) {
        auto phase = starfox::simulation::wrap16(
            static_cast<std::int32_t>(pose.wave_offset) + x);
        phase = starfox::simulation::wrap16(
            starfox::simulation::arithmetic_shift_right(phase, 1)
            + static_cast<std::int32_t>(pose.animation_frame & 15U) - 1);
        auto index = static_cast<std::int32_t>(phase);
        index %= static_cast<std::int32_t>(wave_sine.size());
        if (index < 0) index += static_cast<std::int32_t>(wave_sine.size());
        return scanline + wave_sine[static_cast<std::size_t>(index)];
    };
    auto mode2_edge_continuation = false;
    auto previous_wobble_left = std::int32_t{};
    auto has_previous_wobble_left = false;

    while (y < maximum_y) {
        const auto left_starts_segment = left.remaining == 0;
        const auto right_starts_segment = right.remaining == 0;
        if (left_starts_segment && !begin_segment(left)) return;
        if (right_starts_segment && !begin_segment(right)) return;
        auto x1 = integer_x(left.x);
        auto x2 = integer_x(right.x);
        // Fractional projection and near-plane clipping can reverse the two
        // active edges without changing whether the source visibility plane
        // selected this face. The cartridge raster silently assumes its
        // original winding; at upscale resolution that assumption discarded
        // the complete span, making the face flash off for a presentation.
        if (winding_independent && x2 < x1) std::swap(x1, x2);
        if (x2 >= x1) {
            const auto plot = [&](std::int32_t x, std::int32_t plot_y) {
                const auto palette_index = static_cast<std::uint8_t>(colour_index_base
                    + (colour.dither && ((x ^ plot_y) & 1) != 0
                        ? colour.odd : colour.even));
                target.set(x, plot_y, palette_index);
                if (surfaces != nullptr) {
                    surfaces->set(x, plot_y, surface, palette_index);
                }
            };
            // Wobble mode 2 selects hlines22: its span loop deliberately has
            // PLOT commented out. On continuing right-edge segments the
            // mhlines2 entry emits exactly one pixel at the previous left X.
            if ((pose.wobble_mode & 2U) != 0U) {
                if (!right_starts_segment && has_previous_wobble_left) {
                    plot(previous_wobble_left, y);
                }
            // EX hlines23 draws a complete chord whenever either polygon
            // tracer begins a new edge. Its mhlinesA continuation plots only
            // the two edge pixels on all other scanlines. Mode 2 normally
            // fills, but a left-only edge change enters that same mhlinesA
            // continuation until the right tracer begins its next segment.
            } else if ((pose.wireframe_mode == 1U
                    && !left_starts_segment && !right_starts_segment)
                || (pose.wireframe_mode == 2U
                    && mode2_edge_continuation
                    && !left_starts_segment && !right_starts_segment)) {
                plot(x1, y);
                if (x2 != x1) plot(x2, y);
            } else if (pose.cel_mode && pose.wireframe_mode == 0U) {
                // hlines2rr cancels PLOT's automatic X increment and skips
                // the two span endpoints, leaving the source cel outline.
                for (auto x = x1 + 1; x < x2; ++x) plot(x, y);
            } else if (pose.wave_mode && pose.wireframe_mode == 0U) {
                for (auto x = x1; x <= x2; ++x) plot(x, wave_y(x, y));
            } else {
                for (auto x = x1; x <= x2; ++x) plot(x, y);
            }
        }
        if ((pose.wobble_mode & 2U) != 0U) {
            previous_wobble_left = x1;
            has_previous_wobble_left = true;
        }
        if (pose.wireframe_mode == 2U) {
            if (right_starts_segment) mode2_edge_continuation = false;
            if (left_starts_segment && !right_starts_segment) {
                mode2_edge_continuation = true;
            }
        }
        left.x = advance_x(left.x, left.increment);
        right.x = advance_x(right.x, right.increment);
        --left.remaining;
        --right.remaining;
        if ((pose.wobble_mode & 1U) != 0U) {
            // The source repeats every step of a trapezoid on the same row;
            // when either tracer expires it advances Y once here and once in
            // the shared normal tail, producing NAN mode 6's two-row jump.
            if (left.remaining != 0 && right.remaining != 0) continue;
            ++y;
        }
        ++y;
    }
}

void fill_source_textured_polygon(
    Framebuffer& target,
    const std::vector<RasterVertex>& polygon,
    const assets::TextureImage& texture,
    std::int32_t texture_scroll_x,
    std::int32_t texture_scroll_y,
    std::uint8_t colour_index_base,
    bool winding_independent,
    SurfaceBuffer* surfaces,
    const SurfaceSample& surface) {
    if (polygon.size() < 3U) return;
    struct Point {
        std::int32_t x{};
        std::int32_t y{};
        std::int32_t u{};
        std::int32_t v{};
    };
    std::vector<Point> points;
    points.reserve(polygon.size());
    for (const auto& vertex : polygon) {
        points.push_back({
            std::clamp(static_cast<std::int32_t>(std::lround(vertex.point.x)),
                0, static_cast<std::int32_t>(target.width())),
            std::clamp(static_cast<std::int32_t>(std::lround(vertex.point.y)),
                0, static_cast<std::int32_t>(target.height())),
            static_cast<std::int32_t>(std::lround(vertex.texture.u)),
            static_cast<std::int32_t>(std::lround(vertex.texture.v)),
        });
    }

    auto minimum = std::size_t{};
    auto maximum_y = points.front().y;
    for (std::size_t index = 1; index < points.size(); ++index) {
        if (points[index].y < points[minimum].y) minimum = index;
        maximum_y = std::max(maximum_y, points[index].y);
    }
    auto y = points[minimum].y;
    if (y == maximum_y) return;
    const auto fixed = [](std::int32_t value) {
        return starfox::simulation::wrap16(
            static_cast<std::int64_t>(value) << 8U);
    };
    const auto extended_x = target.width() > 224U;
    const auto fixed_x = [extended_x](std::int32_t value) {
        return extended_x
            ? value << 8U
            : static_cast<std::int32_t>(starfox::simulation::wrap16(
                static_cast<std::int64_t>(value) << 8U));
    };
    const auto integer_x = [extended_x](std::int32_t value) {
        return extended_x
            ? starfox::simulation::arithmetic_shift_right(value, 8)
            : source_fixed_integer(static_cast<std::int16_t>(value));
    };
    const auto rounded_x = [extended_x, &integer_x](std::int32_t value) {
        return extended_x
            ? starfox::simulation::arithmetic_shift_right(value + 127, 8)
            : integer_x(starfox::simulation::add16(
                static_cast<std::int16_t>(value), 127));
    };
    const auto advance_x = [extended_x](
                               std::int32_t value, std::int32_t increment) {
        return extended_x
            ? value + increment
            : static_cast<std::int32_t>(source_advance_edge(
                static_cast<std::int16_t>(value),
                static_cast<std::int16_t>(increment)));
    };
    const auto edge_increment = [extended_x](
                                    std::int32_t difference,
                                    std::int32_t scanlines) {
        return extended_x
            ? wide_edge_increment(difference, scanlines)
            : static_cast<std::int32_t>(
                source_edge_increment(difference, scanlines));
    };

    struct Tracer {
        std::size_t vertex{};
        int direction{};
        std::int32_t x{};
        std::int16_t u{};
        std::int16_t v{};
        std::int32_t x_increment{};
        std::int16_t u_increment{};
        std::int16_t v_increment{};
        std::int32_t remaining{};
    };
    Tracer left{minimum, 1, fixed_x(points[minimum].x),
        fixed(points[minimum].u), fixed(points[minimum].v)};
    Tracer right{minimum, -1, fixed_x(points[minimum].x),
        fixed(points[minimum].u), fixed(points[minimum].v)};
    const auto begin_segment = [
                                   &points, &fixed, &fixed_x, &rounded_x,
                                   &edge_increment, &y](Tracer& tracer) {
        const auto count = points.size();
        auto start_x = rounded_x(tracer.x);
        auto start_u = source_fixed_integer(tracer.u);
        auto start_v = source_fixed_integer(tracer.v);
        for (std::size_t guard = 0; guard < count; ++guard) {
            tracer.vertex = tracer.direction > 0
                ? (tracer.vertex + 1U) % count
                : (tracer.vertex + count - 1U) % count;
            const auto& endpoint = points[tracer.vertex];
            const auto scanlines = endpoint.y - y;
            if (scanlines < 0) return false;
            if (scanlines == 0) {
                start_x = endpoint.x;
                start_u = endpoint.u;
                start_v = endpoint.v;
                tracer.x = fixed_x(start_x);
                tracer.u = fixed(start_u);
                tracer.v = fixed(start_v);
                continue;
            }
            tracer.x = fixed_x(start_x);
            tracer.u = fixed(start_u);
            tracer.v = fixed(start_v);
            tracer.x_increment = edge_increment(
                endpoint.x - start_x, scanlines);
            tracer.u_increment = source_edge_increment(
                endpoint.u - start_u, scanlines);
            tracer.v_increment = source_edge_increment(
                endpoint.v - start_v, scanlines);
            tracer.remaining = scanlines;
            return true;
        }
        return false;
    };

    const auto width = static_cast<std::size_t>(texture.u_mask) + 1U;
    while (y < maximum_y) {
        if (left.remaining == 0 && !begin_segment(left)) return;
        if (right.remaining == 0 && !begin_segment(right)) return;
        auto x1 = integer_x(left.x);
        auto x2 = integer_x(right.x);
        const auto next_left_u = source_advance_edge(
            left.u, left.u_increment);
        const auto next_left_v = source_advance_edge(
            left.v, left.v_increment);
        const auto next_right_u = source_advance_edge(
            right.u, right.u_increment);
        const auto next_right_v = source_advance_edge(
            right.v, right.v_increment);
        auto span_u = left.u;
        auto span_v = left.v;
        auto span_right_u = next_right_u;
        auto span_right_v = next_right_v;
        if (winding_independent && x2 < x1) {
            // Keep the corresponding affine texture endpoints attached to
            // whichever edge is now the left side of the stored raster.
            std::swap(x1, x2);
            span_u = right.u;
            span_v = right.v;
            span_right_u = next_left_u;
            span_right_v = next_left_v;
        }
        if (x2 >= x1) {
            const auto span = x2 - x1;
            const auto reciprocal = span == 0 ? std::int16_t{}
                : static_cast<std::int16_t>(
                    span == 1 ? 32'767 : 32'768 / span);
            const auto u_increment = starfox::simulation::multiply_q15(
                starfox::simulation::subtract16(span_right_u, span_u), reciprocal);
            const auto v_increment = starfox::simulation::multiply_q15(
                starfox::simulation::subtract16(span_right_v, span_v), reciprocal);
            auto u = span_u;
            auto v = span_v;
            for (auto x = x1; x <= x2; ++x) {
                const auto sample_u = (source_fixed_integer(u)
                    + texture_scroll_x) & texture.u_mask;
                const auto sample_v = (source_fixed_integer(v)
                    + texture_scroll_y) & texture.v_mask;
                const auto texel = texture.texels[
                    static_cast<std::size_t>(sample_v) * width
                    + static_cast<std::size_t>(sample_u)];
                if (texel != 0U) {
                    const auto palette_index = static_cast<std::uint8_t>(
                        colour_index_base + texel);
                    target.set(x, y, palette_index);
                    if (surfaces != nullptr) {
                        surfaces->set(x, y, surface, palette_index);
                    }
                }
                u = starfox::simulation::add16(u, u_increment);
                v = starfox::simulation::add16(v, v_increment);
            }
        }
        left.x = advance_x(left.x, left.x_increment);
        left.u = next_left_u;
        left.v = next_left_v;
        right.x = advance_x(right.x, right.x_increment);
        right.u = next_right_u;
        right.v = next_right_v;
        --left.remaining;
        --right.remaining;
        ++y;
    }
}

void draw_line(
    Framebuffer& target,
    ScreenPoint a,
    ScreenPoint b,
    FaceColour colour,
    std::uint8_t colour_index_base) {
    auto x0 = static_cast<int>(std::lround(a.x));
    auto y0 = static_cast<int>(std::lround(a.y));
    const auto x1 = static_cast<int>(std::lround(b.x));
    const auto y1 = static_cast<int>(std::lround(b.y));
    const auto dx = std::abs(x1 - x0);
    const auto sx = x0 < x1 ? 1 : -1;
    const auto dy = std::abs(y1 - y0);
    const auto sy = y0 < y1 ? 1 : -1;
    const auto plot = [&] {
        target.set(x0, y0, static_cast<std::uint8_t>(colour_index_base
            + (colour.dither && ((x0 ^ y0) & 1) != 0
                ? colour.odd : colour.even)));
    };
    // MDRAWC.MC mline uses a major-axis counter and a floor(major/2)
    // accumulator. Its strict-underflow tie rule differs from the common
    // symmetric Bresenham formulation by one pixel on half-slope lines.
    if (dx >= dy) {
        auto error = dx >> 1;
        for (auto count = dx + 1; count != 0; --count) {
            plot();
            error -= dy;
            if (error < 0) {
                error += dx;
                y0 += sy;
            }
            x0 += sx;
        }
    } else {
        auto error = dy >> 1;
        for (auto count = dy + 1; count != 0; --count) {
            plot();
            error -= dx;
            if (error < 0) {
                error += dy;
                x0 += sx;
            }
            y0 += sy;
        }
    }
}

void draw_textured_sprite(
    Framebuffer& target,
    const ScreenPoint& centre,
    const assets::TextureImage& texture,
    std::uint8_t colour_index_base) {
    if (centre.z < 32.0) return;
    const auto source_width = static_cast<std::int32_t>(texture.u_mask) + 1;
    const auto source_height = static_cast<std::int32_t>(texture.v_mask) + 1;
    const auto centre_x = static_cast<std::int32_t>(std::lround(centre.x));
    const auto centre_y = static_cast<std::int32_t>(std::lround(centre.y));
    const auto z = static_cast<std::int32_t>(
        static_cast<std::uint16_t>(rounded_word(centre.z)));

    // msh_s_sprite clears m_sprangle, derives source size from m_sprmask,
    // and passes 256 for 32px sheets or 128 for 64px sheets to mshowspr.
    // mshowspr turns that into an 8.8 source-coordinate increment.
    const auto input_scale = source_width == 64 ? 128 : 256;
    auto increment = static_cast<std::int32_t>(
        (static_cast<std::int64_t>(z) * input_scale) >> 8U);
    if (increment == 0) increment = 1;
    if (increment > 32'767) increment = 32'767;
    const auto half_extent = source_width * 128 / increment;
    const auto unclipped_left = centre_x - half_extent;
    const auto unclipped_top = centre_y - half_extent;
    const auto left = std::max(0, unclipped_left);
    const auto right = std::min(static_cast<std::int32_t>(target.width()) - 1,
        centre_x + half_extent);
    const auto top = std::max(0, unclipped_top);
    const auto bottom = std::min(static_cast<std::int32_t>(target.height()) - 1,
        centre_y + half_extent);
    if (left > right || top > bottom) return;

    // calcxpyp starts at (size/2)<<8 and advances by the effective depth for
    // each destination pixel. Preserve 16-bit register wrapping.
    auto source_y_fixed = starfox::simulation::wrap16(
        static_cast<std::int64_t>(source_width / 2) * 256
        + static_cast<std::int64_t>(top - centre_y) * increment);
    for (auto y = top; y <= bottom; ++y) {
        const auto source_y = static_cast<std::uint32_t>(
            static_cast<std::uint16_t>(source_y_fixed) >> 8U);
        auto source_x_fixed = starfox::simulation::wrap16(
            static_cast<std::int64_t>(source_width / 2) * 256
            + static_cast<std::int64_t>(left - centre_x) * increment);
        for (auto x = left; x <= right; ++x) {
            const auto source_x = static_cast<std::uint32_t>(
                static_cast<std::uint16_t>(source_x_fixed) >> 8U);
            if (source_x < static_cast<std::uint32_t>(source_width)
                && source_y < static_cast<std::uint32_t>(source_height)) {
                const auto texel = texture.texels[
                    static_cast<std::size_t>(source_y) * source_width + source_x];
                if (texel != 0U) {
                    target.set(x, y, static_cast<std::uint8_t>(
                        colour_index_base + texel));
                }
            }
            source_x_fixed = starfox::simulation::add16(
                source_x_fixed, static_cast<std::int16_t>(increment));
        }
        source_y_fixed = starfox::simulation::add16(
            source_y_fixed, static_cast<std::int16_t>(increment));
    }
}

const assets::TextureImage* texture_for_colour(
    const assets::Shape& shape,
    std::uint8_t colour,
    std::uint32_t colour_frame) {
    if (colour >= shape.colour_words.size()) return nullptr;
    auto descriptor = shape.colour_words[colour];
    if (colour < shape.colour_materials.size()) {
        const auto& material = shape.colour_materials[colour];
        if (!material.animation_frames.empty()) {
            descriptor = material.animation_frames[
                colour_frame % material.animation_frames.size()];
        }
    }
    const auto texture = std::find_if(
        shape.textures.begin(), shape.textures.end(), [descriptor](const auto& candidate) {
            return candidate.descriptor == descriptor;
        });
    return texture == shape.textures.end() ? nullptr : &*texture;
}

void draw_simple_scaled_sprite(
    Framebuffer& target,
    const assets::TextureImage& texture,
    const RenderPose& pose,
    double focal_length,
    std::uint8_t colour_index_base) {
    if (pose.z < 128.0 || pose.simple_sprite_world_size <= 0) return;
    auto dimension = static_cast<int>(std::trunc(
        static_cast<double>(pose.simple_sprite_world_size) * focal_length / pose.z));
    dimension = std::clamp(dimension, 0, 240);
    if (dimension == 0) return;
    const auto centre_x = static_cast<int>(std::lround(pose.vanish_x))
        + static_cast<int>(std::trunc(pose.x * focal_length / pose.z));
    const auto centre_y = static_cast<int>(std::lround(pose.vanish_y))
        + static_cast<int>(std::trunc(pose.y * focal_length / pose.z));
    const auto left = centre_x - dimension / 2;
    const auto top = centre_y - dimension / 2;
    const auto source_width = static_cast<int>(texture.u_mask) + 1;
    const auto source_height = static_cast<int>(texture.v_mask) + 1;
    auto first_x = 0;
    auto last_x = dimension;
    if (pose.effect_clip_right > pose.effect_clip_left) {
        first_x = std::max(first_x, pose.effect_clip_left - left);
        last_x = std::min(last_x, pose.effect_clip_right - left);
    }
    if (first_x >= last_x) return;
    for (auto y = 0; y < dimension; ++y) {
        const auto source_y = std::min(source_height - 1,
            static_cast<int>(static_cast<std::int64_t>(y) * source_height / dimension));
        for (auto x = first_x; x < last_x; ++x) {
            const auto source_x = std::min(source_width - 1,
                static_cast<int>(static_cast<std::int64_t>(x) * source_width / dimension));
            const auto texel = texture.texels[
                static_cast<std::size_t>(source_y) * source_width
                + static_cast<std::size_t>(source_x)];
            if (texel != 0U) {
                target.set(left + x, top + y,
                    static_cast<std::uint8_t>(colour_index_base + texel));
            }
        }
    }
}

FaceMaterial face_material(
    const assets::Shape& shape,
    const assets::Face& face,
    std::uint32_t colour_frame,
    std::size_t depth_band,
    const std::array<std::int8_t, 3>& light,
    const RenderPose& pose,
    std::optional<std::uint16_t> descriptor_override = std::nullopt) {
    if (pose.force_colour) {
        const auto even = static_cast<std::uint8_t>(pose.forced_colour & 0x0fU);
        const auto odd = static_cast<std::uint8_t>(pose.forced_colour >> 4U);
        return {{even, odd, even != odd}, nullptr};
    }
    if (!descriptor_override && face.colour_id >= shape.colour_words.size()) {
        const auto fallback = static_cast<std::uint8_t>(face.colour_id & 0x0fU);
        return {{fallback, fallback, false}, nullptr};
    }
    auto word = descriptor_override.value_or(shape.colour_words[face.colour_id]);
    if (!descriptor_override && face.colour_id < shape.colour_materials.size()) {
        const auto& material = shape.colour_materials[face.colour_id];
        if (!material.animation_frames.empty()) {
            word = material.animation_frames[
                colour_frame % material.animation_frames.size()];
        }
    }
    if ((word & 0xc000U) == 0x4000U) {
        const auto texture = std::find_if(
            shape.textures.begin(), shape.textures.end(), [word](const auto& candidate) {
                return candidate.descriptor == word;
            });
        return {{15, 15, false},
            texture == shape.textures.end() ? nullptr : &*texture};
    }
    // COLSMOOTH sets both descriptor flag bits. It is not an ordinary
    // two-nibble colour: the low nibble names the solid material and MOBJ's
    // smooth-shade path supplies coverage separately. Treating $C909 as $09
    // produced a black/green checkerboard on every render scale (most visible
    // at 10x). Retain the descriptor's actual material as a solid face rather
    // than inventing an alternating black nibble.
    if ((word & 0xc000U) == 0xc000U) {
        const auto colour = static_cast<std::uint8_t>(word & 0x0fU);
        return {{colour, colour, false}, nullptr};
    }
    const auto material = static_cast<std::uint8_t>(word >> 8U);
    auto byte = static_cast<std::uint8_t>(word);
    if (material < 62U && shape.has_diffuse_shade_tables
        && material < shape.diffuse_shade_tables[depth_band].size()) {
        const auto dot = face.normal.x * light[0]
            + face.normal.y * light[1] + face.normal.z * light[2];
        const auto intensity = std::clamp(dot >> 10, 6, 15);
        byte = shape.diffuse_shade_tables[depth_band][material][
            static_cast<std::size_t>(intensity - 6)];
    } else if (material == 62U && pose.has_depth_colour_tables) {
        byte = pose.depth_colour_tables[depth_band][byte & 0x1fU];
    }
    auto even = static_cast<std::uint8_t>(byte & 0x0fU);
    auto odd = static_cast<std::uint8_t>(byte >> 4U);
    return {{even, odd, even != odd}, nullptr};
}

} // namespace

SoftwareRenderer::SoftwareRenderer(RenderSettings settings) : settings_(settings) {}

void SoftwareRenderer::draw_cockpit_hud(
    const simulation::TrigTables& trigonometry,
    double rotation,
    std::uint8_t colour,
    std::uint8_t damage_flags,
    std::int32_t horizontal_origin,
    Framebuffer& target,
    std::uint8_t normal_colour_override) const {
    struct HudPoint {
        std::int32_t x{};
        std::int32_t y{};
    };
    rotation = std::fmod(rotation, 256.0);
    if (rotation < 0.0) rotation += 256.0;
    const auto angle = static_cast<std::uint8_t>(rotation);
    const auto next_angle = static_cast<std::uint8_t>(angle + 1U);
    const auto fraction = rotation - angle;
    const auto sine = std::lerp(static_cast<double>(trigonometry.sin8(angle)),
        static_cast<double>(trigonometry.sin8(next_angle)), fraction);
    const auto cosine = std::lerp(static_cast<double>(trigonometry.cos8(angle)),
        static_cast<double>(trigonometry.cos8(next_angle)), fraction);
    const auto scale = static_cast<std::int32_t>(target.draw_scale());
    const StoredRasterScope scale_guard{target, static_cast<std::uint32_t>(scale)};
    const auto rotate = [sine, cosine, scale](std::int32_t x, std::int32_t y) {
        // MROTPNTY loads a signed eight-bit table entry into the high byte of
        // a long product and performs one final ASR: (value * trig) / 512.
        return HudPoint{
            static_cast<std::int32_t>(std::floor((x * cosine + y * sine) * scale / 512.0)),
            static_cast<std::int32_t>(std::floor((y * cosine - x * sine) * scale / 512.0)),
        };
    };
    const auto source_left = horizontal_origin * scale;
    const auto source_right = (horizontal_origin + 224) * scale;
    const auto vanish_x = (horizontal_origin + 112) * scale;
    const auto vanish_y = 96 * scale;
    const auto paint_line = [&target, source_left, source_right,
                             vanish_x, vanish_y, scale](
                                HudPoint a, HudPoint b,
                                std::uint8_t pixel) {
        auto x0 = a.x + vanish_x;
        auto y0 = a.y + vanish_y;
        const auto x1 = b.x + vanish_x;
        const auto y1 = b.y + vanish_y;
        const auto dx = std::abs(x1 - x0);
        const auto sx = x0 < x1 ? 1 : -1;
        const auto dy = std::abs(y1 - y0);
        const auto sy = y0 < y1 ? 1 : -1;
        const auto plot = [&] {
            // Keep the source line weight while allowing sub-source-pixel
            // motion at Render Upscale > 1; never round roll back to 8 bits.
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    if (x0 + dx >= source_left && x0 + dx < source_right)
                        target.set(x0 + dx, y0 + dy, pixel);
                }
            }
        };
        if (dx >= dy) {
            auto error = dx >> 1;
            for (auto count = dx + 1; count != 0; --count) {
                plot();
                error -= dy;
                if (error < 0) {
                    error += dx;
                    y0 += sy;
                }
                x0 += sx;
            }
        } else {
            auto error = dy >> 1;
            for (auto count = dy + 1; count != 0; --count) {
                plot();
                error -= dx;
                if (error < 0) {
                    error += dy;
                    x0 += sx;
                }
                y0 += sy;
            }
        }
    };
    const auto normal = normal_colour_override != 0U
        ? normal_colour_override
        : static_cast<std::uint8_t>(
            settings_.colour_index_base + (colour & 0x0fU));
    const auto damaged = static_cast<std::uint8_t>(
        settings_.colour_index_base + 2U);
    const auto positive_colour = (damage_flags & 2U) != 0U ? damaged : normal;
    const auto negative_colour = (damage_flags & 1U) != 0U ? damaged : normal;
    const auto line = [&](std::int32_t x1, std::int32_t y1,
                          std::int32_t x2, std::int32_t y2) {
        const auto a = rotate(x1, y1);
        const auto b = rotate(x2, y2);
        paint_line(a, b, positive_colour);
        paint_line({-a.x, -a.y}, {-b.x, -b.y}, negative_colour);
    };

    // Exact MHUD.MC source geometry: the vertical pair plus the mirrored
    // three-line wing indicators.
    line(0, 200, 0, 265);
    line(250, 0, 315, -32);
    line(315, -32, 315, 32);
    line(250, 0, 315, 32);
}

void apply_source_depth_tables(
    const assets::RomImage& rom,
    std::uint32_t depth_table_address,
    std::uint16_t threshold_pointer,
    std::uint16_t colour_pointer,
    std::uint8_t object_depth_offset,
    RenderPose& pose) {
    const auto data_bank = depth_table_address & 0xff0000U;
    if (data_bank >= 0x7e0000U
        || threshold_pointer < 0x8000U || colour_pointer < 0x8000U) {
        pose.has_depth_colour_tables = false;
        return;
    }
    if (object_depth_offset != 0U) {
        threshold_pointer = static_cast<std::uint16_t>(threshold_pointer
            + static_cast<std::uint16_t>(object_depth_offset - 1U) * 4U);
    }
    for (std::size_t index = 0; index < pose.depth_thresholds.size(); ++index) {
        const auto encoded = std::bit_cast<std::int8_t>(
            rom.read8(data_bank | static_cast<std::uint16_t>(
                threshold_pointer + static_cast<std::uint16_t>(index))));
        pose.depth_thresholds[index] = static_cast<std::int16_t>(
            -static_cast<std::int16_t>(encoded) * 256);
    }
    for (std::size_t depth = 0; depth < pose.depth_colour_tables.size(); ++depth) {
        for (std::size_t colour = 0;
             colour < pose.depth_colour_tables[depth].size(); ++colour) {
            pose.depth_colour_tables[depth][colour] = rom.read8(
                data_bank | static_cast<std::uint16_t>(colour_pointer
                    + static_cast<std::uint16_t>(depth * 32U + colour)));
        }
    }
    pose.has_depth_colour_tables = true;
}

void SoftwareRenderer::draw(
    const assets::Shape& shape,
    const RenderPose& pose,
    Framebuffer& target,
    bool clear_target,
    SurfaceBuffer* surfaces) const {
    if (clear_target) {
        target.clear(settings_.background_colour);
        if (surfaces != nullptr) surfaces->clear();
    }
    const auto word_exact = pose.use_rotation_matrix
        && !pose.subpixel_projection;
    // Interpolated presentation frames already keep fractional transformed
    // vertices. At a completed 20 Hz source frame the source-exact path
    // quantizes them again; that single-frame snap is hidden by the native
    // raster but becomes visible as flashing polygon edges when supersampled.
    // Use one continuous fractional copy for high-resolution scan conversion,
    // visibility and BSP ordering. Mixing its edges with the rounded source
    // copy made whole faces alternate between present and absent on each
    // completed 20 Hz frame.
    const auto continuous_upscaled_geometry = settings_.render_scale > 1U
        && !pose.subpixel_projection;
    auto raster_pose = pose;
    if (settings_.render_scale > 1U) {
        raster_pose.subpixel_projection = true;
    }
    const auto raster_word_exact = raster_pose.use_rotation_matrix
        && !raster_pose.subpixel_projection;
    if (pose.simple_scaled_sprite) {
        const auto* texture = texture_for_colour(
            shape, pose.simple_sprite_colour, pose.colour_frame);
        if (texture != nullptr) {
            draw_simple_scaled_sprite(target, *texture, pose,
                settings_.focal_length, settings_.colour_index_base);
        }
        return;
    }
    std::vector<Vec3> transformed_vertices;
    std::vector<ScreenPoint> projected;
    std::vector<Vec3> upscaled_vertices;
    std::vector<ScreenPoint> upscaled_projected;
    const auto& vertices = shape.frames.empty()
        ? shape.vertices
        : shape.frames[pose.animation_frame % shape.frames.size()].vertices;
    const auto shading_depth = pose.use_source_lighting_state
        ? pose.source_depth : pose.z;
    auto depth_band = std::size_t{};
    while (depth_band < pose.depth_thresholds.size()
           && shading_depth >= pose.depth_thresholds[depth_band]) {
        ++depth_band;
    }
    std::array<std::int8_t, 3> light{73, 73, 73};
    if (pose.use_rotation_matrix) {
        // Point rotation consumes the columns of m_mat, but initlight's three
        // MDOTPROD16MQ calls explicitly consume its rows. Transpose before
        // using the shared column-vector helper so the source light follows
        // object orientation instead of being rotated by the inverse basis.
        const auto& lighting_matrix = pose.use_source_lighting_state
            ? pose.source_lighting_matrix : pose.rotation_matrix;
        const auto transformed_light = starfox::simulation::transform_q15(
            starfox::simulation::transpose_q15(lighting_matrix),
            {18'917, 18'917, 18'917});
        for (std::size_t index = 0; index < light.size(); ++index) {
            light[index] = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
                std::bit_cast<std::uint16_t>(transformed_light[index]) >> 8U));
        }
    }
    transformed_vertices.reserve(vertices.size());
    projected.reserve(vertices.size());
    if (continuous_upscaled_geometry) {
        upscaled_vertices.reserve(vertices.size());
        upscaled_projected.reserve(vertices.size());
    }
    for (const auto& point : vertices) {
        const auto transformed = rotate(point, pose, shape.header.shift);
        transformed_vertices.push_back(transformed);
        if (continuous_upscaled_geometry) {
            const auto upscaled = rotate(
                point, raster_pose, shape.header.shift);
            upscaled_vertices.push_back(upscaled);
            upscaled_projected.push_back(project_point(
                upscaled, settings_.focal_length,
                raster_word_exact, raster_pose.vanish_x,
                raster_pose.vanish_y, raster_pose.subpixel_projection));
        }
        projected.push_back(project_point(
            transformed, settings_.focal_length,
            word_exact, pose.vanish_x, pose.vanish_y,
            pose.subpixel_projection));
    }
    const auto& raster_vertices = continuous_upscaled_geometry
        ? upscaled_vertices : transformed_vertices;
    const auto& visibility_vertices = raster_vertices;
    const auto& visibility_projected = continuous_upscaled_geometry
        ? upscaled_projected : projected;
    const auto face_visible = [&](const assets::Visibility& visibility) {
        return settings_.render_scale > 1U
            ? continuous_visibility(visibility, visibility_vertices)
            : source_visibility(
                visibility, visibility_projected, visibility_vertices);
    };

    // MOBJ.MC enters the face pass with r8 holding the end of M_PROJPNTS,
    // then COLOR WARP's mrand macro uses that register directly instead of
    // reading the colour table.  Its 16-bit SWAP/ROR/ADD/ADC/INC sequence is
    // the same one used by MGDOTS.MC.  Animated descriptors loop back through
    // .getwordagain; with warp enabled that consumes another random word.
    auto colour_warp_state = static_cast<std::uint16_t>(
        pose.projected_points_address
        + static_cast<std::uint16_t>(vertices.size() * 6U));
    auto colour_warp_carry = false;
    const auto next_colour_warp_word = [&]()
            -> std::optional<std::uint16_t> {
        if (!pose.colour_warp || pose.force_colour) return std::nullopt;
        auto word = std::uint16_t{};
        for (auto animation_hops = 0; animation_hops < 32; ++animation_hops) {
            const auto swapped = static_cast<std::uint16_t>(
                (colour_warp_state << 8U) | (colour_warp_state >> 8U));
            const auto rotated = static_cast<std::uint16_t>(
                (colour_warp_carry ? 0x8000U : 0U) | (swapped >> 1U));
            colour_warp_carry = (swapped & 1U) != 0U;
            const auto first = static_cast<std::uint32_t>(rotated)
                + colour_warp_state;
            colour_warp_carry = first > 0xffffU;
            const auto second = static_cast<std::uint32_t>(
                static_cast<std::uint16_t>(first)) + colour_warp_state
                + (colour_warp_carry ? 1U : 0U);
            colour_warp_carry = second > 0xffffU;
            colour_warp_state = static_cast<std::uint16_t>(second + 1U);
            word = colour_warp_state;
            if ((word & 0xc000U) != 0x8000U) return word;
        }
        // Corrupt/random data can theoretically select animated material
        // words forever. The hardware has no guard, but bounding a malformed
        // host frame keeps the option from hanging the process.
        return static_cast<std::uint16_t>(word & ~0x8000U);
    };

    if (pose.collapse_to_axis_line && !vertices.empty() && !shape.faces.empty()) {
        const auto [minimum, maximum] = std::minmax_element(
            vertices.begin(), vertices.end(), [](const auto& left, const auto& right) {
                return left.z < right.z;
            });
        Vec3 near_axis{};
        Vec3 far_axis{};
        std::size_t near_count{};
        std::size_t far_count{};
        for (std::size_t index = 0; index < vertices.size(); ++index) {
            if (vertices[index].z == maximum->z) {
                near_axis.x += raster_vertices[index].x;
                near_axis.y += raster_vertices[index].y;
                near_axis.z += raster_vertices[index].z;
                ++near_count;
            }
            if (vertices[index].z == minimum->z) {
                far_axis.x += raster_vertices[index].x;
                far_axis.y += raster_vertices[index].y;
                far_axis.z += raster_vertices[index].z;
                ++far_count;
            }
        }
        if (near_count != 0U && far_count != 0U) {
            near_axis.x /= static_cast<double>(near_count);
            near_axis.y /= static_cast<double>(near_count);
            near_axis.z /= static_cast<double>(near_count);
            far_axis.x /= static_cast<double>(far_count);
            far_axis.y /= static_cast<double>(far_count);
            far_axis.z /= static_cast<double>(far_count);
            if (clip_near_line(near_axis, far_axis, raster_word_exact)) {
                auto near_screen = project_point(near_axis, settings_.focal_length,
                    raster_word_exact, raster_pose.vanish_x,
                    raster_pose.vanish_y, raster_pose.subpixel_projection);
                auto far_screen = project_point(far_axis, settings_.focal_length,
                    raster_word_exact, raster_pose.vanish_x,
                    raster_pose.vanish_y, raster_pose.subpixel_projection);
                if (clip_screen_line(near_screen, far_screen, target,
                        raster_word_exact)) {
                    const auto material = face_material(shape, shape.faces.front(),
                        pose.colour_frame, depth_band, light, pose,
                        next_colour_warp_word());
                    const StoredRasterScope raster{
                        target, settings_.render_scale};
                    scale_to_stored(near_screen, settings_.render_scale);
                    scale_to_stored(far_screen, settings_.render_scale);
                    draw_line(target, near_screen, far_screen, material.colour,
                        settings_.colour_index_base);
                }
            }
        }
        return;
    }

    std::vector<const assets::Face*> ordered_faces;
    if (shape.bsp_root_address == 0 || pose.explosion_progress != 0U) {
        ordered_faces.reserve(shape.faces.size());
        for (const auto& face : shape.faces) {
            ordered_faces.push_back(&face);
        }
    } else {
        std::unordered_map<std::uint32_t, const assets::BspNode*> nodes;
        std::unordered_map<std::uint32_t, const assets::BspLeaf*> leaves;
        std::unordered_map<std::uint32_t, const assets::FaceBatch*> batches;
        for (const auto& node : shape.bsp_nodes) {
            nodes.emplace(node.address, &node);
        }
        for (const auto& leaf : shape.bsp_leaves) {
            leaves.emplace(leaf.address, &leaf);
        }
        for (const auto& batch : shape.face_batches) {
            batches.emplace(batch.address, &batch);
        }

        const auto append_batch = [&ordered_faces, &batches](std::uint32_t address) {
            const auto batch = batches.find(address);
            if (batch == batches.end()) {
                return;
            }
            for (const auto& face : batch->second->faces) {
                ordered_faces.push_back(&face);
            }
        };
        std::unordered_set<std::uint32_t> active;
        std::function<void(std::uint32_t)> traverse = [&](std::uint32_t address) {
            if (address == 0 || !active.insert(address).second) {
                return;
            }
            const auto leaf = leaves.find(address);
            if (leaf != leaves.end()) {
                append_batch(leaf->second->face_batch_address);
                active.erase(address);
                return;
            }
            const auto node = nodes.find(address);
            if (node == nodes.end()) {
                active.erase(address);
                return;
            }

            bool plane_visible = false;
            if (node->second->visibility_index < shape.visibilities.size()) {
                const auto& visibility = shape.visibilities[node->second->visibility_index];
                if (visibility.a < visibility_projected.size()
                    && visibility.b < visibility_projected.size()
                    && visibility.c < visibility_projected.size()) {
                    plane_visible = face_visible(visibility);
                }
            }

            if (plane_visible) {
                traverse(node->second->fallthrough_address);
                append_batch(node->second->face_batch_address);
                traverse(node->second->alternate_address);
            } else {
                traverse(node->second->alternate_address);
                traverse(node->second->fallthrough_address);
            }
            active.erase(address);
        };
        traverse(shape.bsp_root_address);
    }

    for (const auto* face_pointer : ordered_faces) {
        const auto& face = *face_pointer;
        if (pose.explosion_progress == 0U && face.visibility_index >= 0
            && static_cast<std::size_t>(face.visibility_index) < shape.visibilities.size()) {
            const auto& visibility = shape.visibilities[
                static_cast<std::size_t>(face.visibility_index)];
            if (visibility.a >= visibility_projected.size()
                || visibility.b >= visibility_projected.size()
                || visibility.c >= visibility_projected.size()) {
                continue;
            }
            if (!face_visible(visibility)) {
                continue;
            }
        }

        const auto material = face_material(
            shape, face, pose.colour_frame, depth_band, light, pose,
            next_colour_warp_word());
        const auto face_offset = explosion_offset(face, pose);
        if (face.sprite) {
            if (material.texture != nullptr && face.vertex_indices.size() == 1U
                && face.vertex_indices[0] < visibility_projected.size()
                && visibility_projected[face.vertex_indices[0]].visible) {
                auto centre = raster_vertices[face.vertex_indices[0]];
                centre.x += face_offset.x;
                centre.y += face_offset.y;
                centre.z += face_offset.z;
                draw_textured_sprite(target, project_point(centre,
                    settings_.focal_length, raster_word_exact,
                    raster_pose.vanish_x, raster_pose.vanish_y,
                    raster_pose.subpixel_projection),
                    *material.texture, settings_.colour_index_base);
            }
            continue;
        }
        std::vector<Vec3> camera_polygon;
        camera_polygon.reserve(face.vertex_indices.size());
        bool valid = true;
        bool any_behind = false;
        for (const auto index : face.vertex_indices) {
            if (index >= transformed_vertices.size()) {
                valid = false;
                break;
            }
            auto point = raster_vertices[index];
            point.x += face_offset.x;
            point.y += face_offset.y;
            point.z += face_offset.z;
            any_behind = any_behind || point.z < 0.0;
            camera_polygon.push_back(point);
        }
        if (!valid || camera_polygon.size() < 2) {
            continue;
        }

        auto surface = SurfaceSample{};
        if (surfaces != nullptr) {
            auto normal_pose = pose;
            normal_pose.x = 0.0;
            normal_pose.y = 0.0;
            normal_pose.z = 0.0;
            normal_pose.scale = 1.0;
            const auto transformed_normal = rotate(face.normal, normal_pose, 0U);
            const auto normal_length = std::sqrt(
                transformed_normal.x * transformed_normal.x
                + transformed_normal.y * transformed_normal.y
                + transformed_normal.z * transformed_normal.z);
            if (normal_length > 0.0001) {
                surface.normal_x = static_cast<float>(
                    transformed_normal.x / normal_length);
                surface.normal_y = static_cast<float>(
                    transformed_normal.y / normal_length);
                surface.normal_z = static_cast<float>(
                    transformed_normal.z / normal_length);
            }
            surface.depth = static_cast<float>(std::accumulate(
                camera_polygon.begin(), camera_polygon.end(), 0.0,
                [](double total, const Vec3& point) { return total + point.z; })
                / static_cast<double>(camera_polygon.size()));
        }

        if (any_behind && material.texture != nullptr) {
            // MOBJ.MC explicitly declines to 3D-clip texture maps.
            continue;
        }

        if (camera_polygon.size() == 2U) {
            if (!clip_near_line(
                    camera_polygon[0], camera_polygon[1],
                    raster_word_exact)) {
                continue;
            }
        } else if (any_behind) {
            camera_polygon = clip_near_polygon(
                camera_polygon, raster_word_exact);
            if (camera_polygon.size() < 3U) continue;
        }

        std::vector<ScreenPoint> polygon;
        polygon.reserve(camera_polygon.size());
        for (const auto& point : camera_polygon) {
            polygon.push_back(project_point(
                point, settings_.focal_length,
                raster_word_exact, raster_pose.vanish_x,
                raster_pose.vanish_y, raster_pose.subpixel_projection));
        }

        if (polygon.size() == 2) {
            if (!clip_screen_line(
                    polygon[0], polygon[1], target,
                    raster_word_exact)) {
                continue;
            }
            const StoredRasterScope raster{target, settings_.render_scale};
            scale_to_stored(polygon[0], settings_.render_scale);
            scale_to_stored(polygon[1], settings_.render_scale);
            draw_line(target, polygon[0], polygon[1], material.colour,
                settings_.colour_index_base);
            continue;
        }

        double signed_area = 0.0;
        for (std::size_t index = 0; index < polygon.size(); ++index) {
            const auto& a = polygon[index];
            const auto& b = polygon[(index + 1U) % polygon.size()];
            signed_area += a.x * b.y - b.x * a.y;
        }
        if (settings_.backface_culling && !face.sprite && signed_area >= 0.0) {
            continue;
        }
        std::vector<RasterVertex> raster_polygon;
        raster_polygon.reserve(polygon.size());
        for (std::size_t index = 0; index < polygon.size(); ++index) {
            auto texture = TexturePoint{};
            if (material.texture != nullptr) {
                const auto& coordinate = material.texture->coordinates[
                    index % material.texture->coordinates.size()];
                texture = {static_cast<double>(coordinate.u),
                    static_cast<double>(coordinate.v)};
            }
            raster_polygon.push_back({polygon[index], texture});
        }
        raster_polygon = clip_screen_polygon(
            std::move(raster_polygon), target, raster_word_exact);
        if (raster_polygon.size() < 3U) continue;
        const StoredRasterScope raster{target, settings_.render_scale};
        for (auto& vertex : raster_polygon) {
            scale_to_stored(vertex.point, settings_.render_scale);
        }
        if (material.texture == nullptr) {
            fill_source_polygon(target, raster_polygon, material.colour,
                settings_.colour_index_base, pose,
                settings_.render_scale > 1U, surfaces, surface);
        } else {
            fill_source_textured_polygon(target, raster_polygon,
                *material.texture, pose.texture_scroll_x, pose.texture_scroll_y,
                settings_.colour_index_base, settings_.render_scale > 1U,
                surfaces, surface);
        }
    }
}

} // namespace starfox::render
