#pragma once

#include <cmath>
#include <optional>

namespace starfox::render::shadows {

struct Vec3 {
    double x{}, y{}, z{};
    Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
};
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
struct Triangle { Vec3 a, b, c; };
struct PreparedTriangle {
    Vec3 origin, edge1, edge2;
    double scale;
    explicit PreparedTriangle(const Triangle& triangle)
        : origin(triangle.a), edge1(triangle.b-triangle.a),
          edge2(triangle.c-triangle.a),
          scale(std::sqrt(dot(edge1,edge1)*dot(edge2,edge2))) {}
};
struct PreparedDirection {
    Vec3 p;
    double inverse{};
    bool valid{};
    PreparedDirection(Vec3 direction,const PreparedTriangle& triangle)
        : p(cross(direction,triangle.edge2)) {
        const auto determinant=dot(triangle.edge1,p);
        valid=!( !std::isfinite(determinant) || triangle.scale==0.0
            || std::abs(determinant)<=triangle.scale*1e-10);
        if (valid) inverse=1.0/determinant;
    }
};

// Double-sided: native models do not consistently use one winding convention.
// The caller supplies a normalized light direction so t is a world distance.
inline std::optional<double> intersect(Vec3 origin, Vec3 direction,
    const PreparedTriangle& triangle, const PreparedDirection& prepared,
    double minimum_distance, double maximum_distance) {
    const auto& edge1 = triangle.edge1;
    const auto& edge2 = triangle.edge2;
    if (!prepared.valid) return {};
    const auto& p=prepared.p;
    const auto inverse=prepared.inverse;
    const auto offset = origin - triangle.origin;
    const auto u = dot(offset, p) * inverse;
    if (u < 0.0 || u > 1.0) return {};
    const auto q = cross(offset, edge1);
    const auto v = dot(direction, q) * inverse;
    if (v < 0.0 || u + v > 1.0) return {};
    const auto distance = dot(edge2, q) * inverse;
    if (!std::isfinite(distance) || distance <= minimum_distance
        || distance > maximum_distance) return {};
    return distance;
}
inline std::optional<double> intersect(Vec3 origin, Vec3 direction,
    const PreparedTriangle& triangle, double minimum_distance, double maximum_distance) {
    return intersect(origin,direction,triangle,PreparedDirection{direction,triangle},
        minimum_distance,maximum_distance);
}
inline std::optional<double> intersect(Vec3 origin, Vec3 direction,
    const Triangle& triangle, double minimum_distance, double maximum_distance) {
    return intersect(origin,direction,PreparedTriangle{triangle},minimum_distance,maximum_distance);
}

// Intersection with an actual receiver plane, not a camera-facing decal.
inline std::optional<Vec3> project_to_plane(Vec3 point, Vec3 light_travel,
    Vec3 plane_point, Vec3 plane_normal) {
    const auto denominator = dot(light_travel, plane_normal);
    if (!std::isfinite(denominator) || std::abs(denominator) < 1e-10) return {};
    const auto distance = dot(plane_point - point, plane_normal) / denominator;
    if (!std::isfinite(distance) || distance < 0.0) return {};
    return point + light_travel * distance;
}

} // namespace starfox::render::shadows
