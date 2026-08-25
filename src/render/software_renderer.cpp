#include "starfox/render/software_renderer.hpp"

#include "starfox/simulation/math.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <functional>
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
    double vanish_y) {
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
    return {
        vanish_x
            + std::trunc(point.x * focal_length / projection_z),
        vanish_y
            + std::trunc(point.y * focal_length / projection_z),
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
    const auto signed_area = (b.x - a.x) * (c.y - a.y)
        - (b.y - a.y) * (c.x - a.x);
    const auto odd_behind = (transformed[visibility.a].z < 0.0)
        != ((transformed[visibility.b].z < 0.0)
            != (transformed[visibility.c].z < 0.0));
    return (signed_area < 0.0) != odd_behind;
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

std::int16_t source_edge_increment(
    std::int32_t difference, std::int32_t scanlines) noexcept {
    if (scanlines <= 0) return 0;
    const auto reciprocal = scanlines == 1
        ? 32'767 : 32'768 / scanlines;
    return starfox::simulation::wrap16(
        starfox::simulation::arithmetic_shift_right(
            static_cast<std::int32_t>(difference * reciprocal), 7));
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
    std::uint8_t colour_index_base) {
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

    struct Tracer {
        std::size_t vertex{};
        int direction{};
        std::int16_t x{};
        std::int16_t increment{};
        std::int32_t remaining{};
    };
    const auto initial_x = starfox::simulation::wrap16(
        static_cast<std::int64_t>(points[minimum].x) << 8U);
    Tracer left{minimum, 1, initial_x, 0, 0};
    Tracer right{minimum, -1, initial_x, 0, 0};
    const auto begin_segment = [&points, &y](Tracer& tracer) {
        const auto count = points.size();
        auto rounded = source_fixed_integer(starfox::simulation::add16(
            tracer.x, 127));
        for (std::size_t guard = 0; guard < count; ++guard) {
            tracer.vertex = tracer.direction > 0
                ? (tracer.vertex + 1U) % count
                : (tracer.vertex + count - 1U) % count;
            const auto& endpoint = points[tracer.vertex];
            const auto scanlines = endpoint.y - y;
            if (scanlines < 0) return false;
            if (scanlines == 0) {
                rounded = endpoint.x;
                tracer.x = starfox::simulation::wrap16(
                    static_cast<std::int64_t>(rounded) << 8U);
                continue;
            }
            tracer.x = starfox::simulation::wrap16(
                static_cast<std::int64_t>(rounded) << 8U);
            tracer.increment = source_edge_increment(
                endpoint.x - rounded, scanlines);
            tracer.remaining = scanlines;
            return true;
        }
        return false;
    };

    while (y < maximum_y) {
        if (left.remaining == 0 && !begin_segment(left)) return;
        if (right.remaining == 0 && !begin_segment(right)) return;
        const auto x1 = source_fixed_integer(left.x);
        const auto x2 = source_fixed_integer(right.x);
        if (x2 >= x1) {
            for (auto x = x1; x <= x2; ++x) {
                target.set(x, y, static_cast<std::uint8_t>(colour_index_base
                    + (colour.dither && ((x ^ y) & 1) != 0
                        ? colour.odd : colour.even)));
            }
        }
        left.x = source_advance_edge(left.x, left.increment);
        right.x = source_advance_edge(right.x, right.increment);
        --left.remaining;
        --right.remaining;
        ++y;
    }
}

void fill_source_textured_polygon(
    Framebuffer& target,
    const std::vector<RasterVertex>& polygon,
    const assets::TextureImage& texture,
    std::int32_t texture_scroll_x,
    std::int32_t texture_scroll_y,
    std::uint8_t colour_index_base) {
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

    struct Tracer {
        std::size_t vertex{};
        int direction{};
        std::int16_t x{};
        std::int16_t u{};
        std::int16_t v{};
        std::int16_t x_increment{};
        std::int16_t u_increment{};
        std::int16_t v_increment{};
        std::int32_t remaining{};
    };
    Tracer left{minimum, 1, fixed(points[minimum].x),
        fixed(points[minimum].u), fixed(points[minimum].v)};
    Tracer right{minimum, -1, fixed(points[minimum].x),
        fixed(points[minimum].u), fixed(points[minimum].v)};
    const auto begin_segment = [&points, &fixed, &y](Tracer& tracer) {
        const auto count = points.size();
        auto start_x = source_fixed_integer(starfox::simulation::add16(
            tracer.x, 127));
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
                tracer.x = fixed(start_x);
                tracer.u = fixed(start_u);
                tracer.v = fixed(start_v);
                continue;
            }
            tracer.x = fixed(start_x);
            tracer.u = fixed(start_u);
            tracer.v = fixed(start_v);
            tracer.x_increment = source_edge_increment(
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
        const auto x1 = source_fixed_integer(left.x);
        const auto x2 = source_fixed_integer(right.x);
        const auto next_right_u = source_advance_edge(
            right.u, right.u_increment);
        const auto next_right_v = source_advance_edge(
            right.v, right.v_increment);
        if (x2 >= x1) {
            const auto span = x2 - x1;
            const auto reciprocal = span == 0 ? std::int16_t{}
                : static_cast<std::int16_t>(
                    span == 1 ? 32'767 : 32'768 / span);
            const auto u_increment = starfox::simulation::multiply_q15(
                starfox::simulation::subtract16(next_right_u, left.u), reciprocal);
            const auto v_increment = starfox::simulation::multiply_q15(
                starfox::simulation::subtract16(next_right_v, left.v), reciprocal);
            auto u = left.u;
            auto v = left.v;
            for (auto x = x1; x <= x2; ++x) {
                const auto sample_u = (source_fixed_integer(u)
                    + texture_scroll_x) & texture.u_mask;
                const auto sample_v = (source_fixed_integer(v)
                    + texture_scroll_y) & texture.v_mask;
                const auto texel = texture.texels[
                    static_cast<std::size_t>(sample_v) * width
                    + static_cast<std::size_t>(sample_u)];
                if (texel != 0U) {
                    target.set(x, y, static_cast<std::uint8_t>(
                        colour_index_base + texel));
                }
                u = starfox::simulation::add16(u, u_increment);
                v = starfox::simulation::add16(v, v_increment);
            }
        }
        left.x = source_advance_edge(left.x, left.x_increment);
        left.u = source_advance_edge(left.u, left.u_increment);
        left.v = source_advance_edge(left.v, left.v_increment);
        right.x = source_advance_edge(right.x, right.x_increment);
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
    for (auto y = 0; y < dimension; ++y) {
        const auto source_y = std::min(source_height - 1,
            static_cast<int>(static_cast<std::int64_t>(y) * source_height / dimension));
        for (auto x = 0; x < dimension; ++x) {
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
    const RenderPose& pose) {
    if (pose.force_colour) {
        const auto even = static_cast<std::uint8_t>(pose.forced_colour & 0x0fU);
        const auto odd = static_cast<std::uint8_t>(pose.forced_colour >> 4U);
        return {{even, odd, even != odd}, nullptr};
    }
    if (face.colour_id >= shape.colour_words.size()) {
        const auto fallback = static_cast<std::uint8_t>(face.colour_id & 0x0fU);
        return {{fallback, fallback, false}, nullptr};
    }
    auto word = shape.colour_words[face.colour_id];
    if (face.colour_id < shape.colour_materials.size()) {
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
    if (material != 63U && material != 62U) {
        // Diffuse materials use the high byte as a light-vector selector.
        // Until the original normal/light lookup is applied, preserve their
        // unlit base colour without inventing a second palette index.
        odd = even;
    }
    return {{even, odd, even != odd}, nullptr};
}

} // namespace

SoftwareRenderer::SoftwareRenderer(RenderSettings settings) : settings_(settings) {}

void apply_original_depth_tables(
    const assets::RomImage& rom,
    std::uint16_t threshold_pointer,
    std::uint16_t colour_pointer,
    std::uint8_t object_depth_offset,
    RenderPose& pose) {
    if (threshold_pointer < 0x8000U || colour_pointer < 0x8000U) {
        pose.has_depth_colour_tables = false;
        return;
    }
    if (object_depth_offset != 0U) {
        threshold_pointer = static_cast<std::uint16_t>(threshold_pointer
            + static_cast<std::uint16_t>(object_depth_offset - 1U) * 4U);
    }
    constexpr std::uint32_t data_bank = 0x030000U;
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
    bool clear_target) const {
    if (clear_target) {
        target.clear(settings_.background_colour);
    }
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
    const auto& vertices = shape.frames.empty()
        ? shape.vertices
        : shape.frames[pose.animation_frame % shape.frames.size()].vertices;
    auto depth_band = std::size_t{};
    while (depth_band < pose.depth_thresholds.size()
           && pose.z >= pose.depth_thresholds[depth_band]) {
        ++depth_band;
    }
    std::array<std::int8_t, 3> light{73, 73, 73};
    if (pose.use_rotation_matrix) {
        const auto transformed_light = starfox::simulation::transform_q15(
            pose.rotation_matrix, {18'917, 18'917, 18'917});
        for (std::size_t index = 0; index < light.size(); ++index) {
            light[index] = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
                std::bit_cast<std::uint16_t>(transformed_light[index]) >> 8U));
        }
    }
    transformed_vertices.reserve(vertices.size());
    projected.reserve(vertices.size());
    for (const auto& point : vertices) {
        const auto transformed = rotate(point, pose, shape.header.shift);
        transformed_vertices.push_back(transformed);
        projected.push_back(project_point(
            transformed, settings_.focal_length,
            pose.use_rotation_matrix, pose.vanish_x, pose.vanish_y));
    }

    std::vector<const assets::Face*> ordered_faces;
    if (shape.bsp_root_address == 0) {
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
                if (visibility.a < projected.size() && visibility.b < projected.size()
                    && visibility.c < projected.size()) {
                    plane_visible = source_visibility(
                        visibility, projected, transformed_vertices);
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
        if (face.visibility_index >= 0
            && static_cast<std::size_t>(face.visibility_index) < shape.visibilities.size()) {
            const auto& visibility = shape.visibilities[
                static_cast<std::size_t>(face.visibility_index)];
            if (visibility.a >= projected.size() || visibility.b >= projected.size()
                || visibility.c >= projected.size()) {
                continue;
            }
            if (!source_visibility(
                    visibility, projected, transformed_vertices)) {
                continue;
            }
        }

        const auto material = face_material(
            shape, face, pose.colour_frame, depth_band, light, pose);
        if (face.sprite) {
            if (material.texture != nullptr && face.vertex_indices.size() == 1U
                && face.vertex_indices[0] < projected.size()
                && projected[face.vertex_indices[0]].visible) {
                draw_textured_sprite(target, projected[face.vertex_indices[0]],
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
            const auto& point = transformed_vertices[index];
            any_behind = any_behind || point.z < 0.0;
            camera_polygon.push_back(point);
        }
        if (!valid || camera_polygon.size() < 2) {
            continue;
        }

        if (any_behind && material.texture != nullptr) {
            // MOBJ.MC explicitly declines to 3D-clip texture maps.
            continue;
        }

        if (camera_polygon.size() == 2U) {
            if (!clip_near_line(
                    camera_polygon[0], camera_polygon[1], pose.use_rotation_matrix)) {
                continue;
            }
        } else if (any_behind) {
            camera_polygon = clip_near_polygon(
                camera_polygon, pose.use_rotation_matrix);
            if (camera_polygon.size() < 3U) continue;
        }

        std::vector<ScreenPoint> polygon;
        polygon.reserve(camera_polygon.size());
        for (const auto& point : camera_polygon) {
            polygon.push_back(project_point(
                point, settings_.focal_length,
                pose.use_rotation_matrix, pose.vanish_x, pose.vanish_y));
        }

        if (polygon.size() == 2) {
            if (!clip_screen_line(
                    polygon[0], polygon[1], target, pose.use_rotation_matrix)) {
                continue;
            }
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
            std::move(raster_polygon), target, pose.use_rotation_matrix);
        if (raster_polygon.size() < 3U) continue;
        if (material.texture == nullptr) {
            fill_source_polygon(target, raster_polygon, material.colour,
                settings_.colour_index_base);
        } else {
            fill_source_textured_polygon(target, raster_polygon,
                *material.texture, pose.texture_scroll_x, pose.texture_scroll_y,
                settings_.colour_index_base);
        }
    }
}

} // namespace starfox::render
