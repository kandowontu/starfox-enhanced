#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace starfox::render {
struct TemporalCamera {
    std::array<double,3> position{};
    std::array<double,9> world_to_view{1,0,0,0,1,0,0,0,1};
};
struct TemporalGroundPlane {
    std::array<float,4> camera_plane{};
    std::int32_t world_height{}; // invalidate terrain correspondence when changed
};
inline std::optional<std::array<float,3>> temporal_unit_axis(
    const std::array<float,16>& matrix,unsigned row,float sign=1) {
    if(row>2 || !std::isfinite(sign)) return {};
    double length=0;
    for(unsigned i=0;i<3;++i) length+=double(matrix[row*4+i])*matrix[row*4+i];
    if(!std::isfinite(length) || length<1e-16) return {};
    const double factor=sign/std::sqrt(length);
    std::array<float,3> result{};
    for(unsigned i=0;i<3;++i) result[i]=float(matrix[row*4+i]*factor);
    return result;
}
// Current camera coordinates -> previous camera coordinates. Source world
// positions wrap every 65536 units; invert Q15-derived matrices exactly rather
// than assuming quantized rotations are perfectly orthonormal.
inline std::optional<std::array<float,16>> temporal_camera_mapping(
    const TemporalCamera& current,const TemporalCamera& previous) {
    const auto& m=current.world_to_view;
    for(const auto* camera:{&current,&previous}) {
        for(double v:camera->position) if(!std::isfinite(v)) return {};
        for(double v:camera->world_to_view) if(!std::isfinite(v)) return {};
    }
    const double det=m[0]*(m[4]*m[8]-m[5]*m[7])-m[1]*(m[3]*m[8]-m[5]*m[6])+m[2]*(m[3]*m[7]-m[4]*m[6]);
    if(!std::isfinite(det) || std::abs(det)<1e-8) return {};
    const std::array<double,9> inverse{
        (m[4]*m[8]-m[5]*m[7])/det,(m[2]*m[7]-m[1]*m[8])/det,(m[1]*m[5]-m[2]*m[4])/det,
        (m[5]*m[6]-m[3]*m[8])/det,(m[0]*m[8]-m[2]*m[6])/det,(m[2]*m[3]-m[0]*m[5])/det,
        (m[3]*m[7]-m[4]*m[6])/det,(m[1]*m[6]-m[0]*m[7])/det,(m[0]*m[4]-m[1]*m[3])/det};
    std::array<float,16> out{};out[15]=1;
    for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) {
        double value=0;for(unsigned k=0;k<3;++k) value+=inverse[r*3+k]*previous.world_to_view[k*3+c];
        out[r*4+c]=float(value);
    }
    for(unsigned c=0;c<3;++c) {
        double value=0;
        for(unsigned k=0;k<3;++k) {
            double delta=std::fmod(current.position[k]-previous.position[k]+32768.,65536.);
            if(delta<0) delta+=65536.;delta-=32768.;
            value+=delta*previous.world_to_view[k*3+c];
        }
        out[12+c]=float(value);
    }
    for(float v:out) if(!std::isfinite(v)) return {};
    return out;
}
// Row-vector/row-major, positive camera Z, downward camera Y. These matrices
// describe projection only; camera rigid motion must be supplied separately.
struct TemporalProjection {
    std::array<float,16> view_to_clip{},clip_to_view{};
    float vertical_fov{},aspect{};
};
inline std::optional<TemporalProjection> temporal_projection(
    std::uint32_t width,std::uint32_t height,float focal,float cx,float cy,
    float near_plane,float far_plane,float focal_y=0) {
    if(focal_y==0) focal_y=focal;
    if(!width || !height || !std::isfinite(focal) || focal<=0 ||
        !std::isfinite(focal_y) || focal_y<=0 ||
        !std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(near_plane) ||
        !std::isfinite(far_plane) || near_plane<=0 || far_plane<=near_plane) return {};
    TemporalProjection p;
    const double sx=2.*focal/width,sy=-2.*focal_y/height;
    const double ox=2.*cx/width-1,oy=1-2.*cy/height;
    const double a=double(far_plane)/(double(far_plane)-near_plane),b=-near_plane*a;
    p.view_to_clip={float(sx),0,0,0,0,float(sy),0,0,float(ox),float(oy),float(a),1,0,0,float(b),0};
    p.clip_to_view={float(1/sx),0,0,0,0,float(1/sy),0,0,0,0,0,float(1/b),
        float(-ox/sx),float(-oy/sy),1,float(-a/b)};
    p.vertical_fov=float(2*std::atan(double(height)/(2*focal_y)));
    p.aspect=float(double(width)*focal_y/(double(height)*focal));
    for(const auto* m:{&p.view_to_clip,&p.clip_to_view})
        for(float value:*m) if(!std::isfinite(value)) return {};
    return p;
}
inline std::array<float,16> temporal_matrix_product(
    const std::array<float,16>& a,const std::array<float,16>& b) {
    std::array<float,16> result{};
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) {
        double value=0;
        for(unsigned k=0;k<4;++k) value+=double(a[r*4+k])*b[k*4+c];
        result[r*4+c]=float(value);
    }
    return result;
}
} // namespace starfox::render
