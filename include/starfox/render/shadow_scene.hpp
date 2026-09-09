#pragma once

#include "starfox/render/shadow_geometry.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace starfox::render::shadows {

// Rebuilt from current presentation geometry: no object-slot identity or stale
// transform cache survives a frame. Balanced median splits bound stack depth.
class Scene {
    struct Bounds {
        Vec3 low{INFINITY, INFINITY, INFINITY};
        Vec3 high{-INFINITY, -INFINITY, -INFINITY};
        void include(Vec3 p) {
            low = {std::min(low.x,p.x), std::min(low.y,p.y), std::min(low.z,p.z)};
            high = {std::max(high.x,p.x), std::max(high.y,p.y), std::max(high.z,p.z)};
        }
        bool hit(Vec3 origin, Vec3 direction, double near, double far) const {
            const std::array<double,3> o{origin.x,origin.y,origin.z};
            const std::array<double,3> d{direction.x,direction.y,direction.z};
            const std::array<double,3> a{low.x,low.y,low.z}, b{high.x,high.y,high.z};
            for (unsigned axis=0; axis<3; ++axis) {
                if (std::abs(d[axis]) < 1e-15) {
                    if (o[axis] < a[axis] || o[axis] > b[axis]) return false;
                } else {
                    auto t0=(a[axis]-o[axis])/d[axis], t1=(b[axis]-o[axis])/d[axis];
                    if (t0>t1) std::swap(t0,t1);
                    near=std::max(near,t0); far=std::min(far,t1);
                    if (near>far) return false;
                }
            }
            return true;
        }
    };
    struct Node { Bounds bounds; std::size_t begin{}, count{}, left{}, right{}; };
    std::vector<Triangle> triangles_;
    std::vector<Node> nodes_;
    bool ready_{};

    std::size_t build_node(std::size_t begin, std::size_t end) {
        const auto index=nodes_.size();
        nodes_.emplace_back();
        Bounds bounds;
        for (auto i=begin;i<end;++i) {
            bounds.include(triangles_[i].a); bounds.include(triangles_[i].b);
            bounds.include(triangles_[i].c);
        }
        nodes_[index].bounds=bounds;
        if (end-begin <= 4) {
            nodes_[index].begin=begin; nodes_[index].count=end-begin;
        } else {
            const auto extent=bounds.high-bounds.low;
            const auto axis=extent.x>extent.y ? (extent.x>extent.z?0:2) : (extent.y>extent.z?1:2);
            const auto middle=begin+(end-begin)/2;
            const auto center=[axis](const Triangle& t) {
                const auto c=t.a+t.b+t.c;
                return axis==0?c.x:axis==1?c.y:c.z;
            };
            std::nth_element(triangles_.begin()+begin,triangles_.begin()+middle,
                triangles_.begin()+end,[&](const auto& a,const auto& b){return center(a)<center(b);});
            const auto left=build_node(begin,middle), right=build_node(middle,end);
            nodes_[index].left=left; nodes_[index].right=right;
        }
        return index;
    }
public:
    [[nodiscard]] std::size_t triangle_count() const { return triangles_.size(); }
    void clear() { triangles_.clear(); nodes_.clear(); ready_=false; }
    void add(Triangle triangle) {
        const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
        if (!finite(triangle.a)||!finite(triangle.b)||!finite(triangle.c)) return;
        const auto normal=cross(triangle.b-triangle.a,triangle.c-triangle.a);
        if (dot(normal,normal)<=1e-20) return;
        triangles_.push_back(triangle); ready_=false;
    }
    void build() {
        nodes_.clear(); nodes_.reserve(triangles_.size()*2);
        if (!triangles_.empty()) build_node(0,triangles_.size());
        ready_=true;
    }
    bool occluded(Vec3 point, Vec3 toward_light, double bias=0.05,
        double maximum_distance=65536.0, std::size_t* triangle_tests=nullptr) const {
        if (triangle_tests) *triangle_tests=0;
        if (!ready_ || nodes_.empty()) return false;
        std::array<std::size_t,64> stack{};
        std::size_t size=1;
        while (size) {
            const auto& node=nodes_[stack[--size]];
            if (!node.bounds.hit(point,toward_light,bias,maximum_distance)) continue;
            if (node.count) {
                for (auto i=node.begin;i<node.begin+node.count;++i) {
                    if (triangle_tests) ++*triangle_tests;
                    if (intersect(point,toward_light,triangles_[i],bias,maximum_distance)) return true;
                }
            } else {
                stack[size++]=node.left; stack[size++]=node.right;
            }
        }
        return false;
    }

    // Exact camera-ray receiver depth. Unlike average face depth, this stays
    // on sloped surfaces and selects the closest of overlapping polygons.
    std::optional<double> nearest(Vec3 origin, Vec3 direction,
        double minimum_distance=0.05, double maximum_distance=65536.0) const {
        if (!ready_ || nodes_.empty()) return {};
        std::array<std::size_t,64> stack{};
        std::size_t size=1;
        std::optional<double> result;
        while (size) {
            const auto& node=nodes_[stack[--size]];
            if (!node.bounds.hit(origin,direction,minimum_distance,maximum_distance)) continue;
            if (node.count) {
                for (auto i=node.begin;i<node.begin+node.count;++i) {
                    const auto hit=intersect(origin,direction,triangles_[i],
                        minimum_distance,maximum_distance);
                    if (hit) { result=hit; maximum_distance=*hit; }
                }
            } else {
                stack[size++]=node.left; stack[size++]=node.right;
            }
        }
        return result;
    }
};
} // namespace starfox::render::shadows
