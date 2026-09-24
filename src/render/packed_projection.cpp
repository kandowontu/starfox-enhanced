#include "starfox/render/packed_projection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
namespace starfox::render {
std::array<std::vector<std::uint32_t>,2> pack_axis_groups(
    const assets::Shape& shape,std::uint32_t animation_frame) {
    const auto& vertices=shape.frames.empty()?shape.vertices
        :shape.frames[animation_frame%shape.frames.size()].vertices;
    std::array<std::vector<std::uint32_t>,2> result;
    if(vertices.empty()) return result;
    if(vertices.size()>4U*1024*1024) throw std::runtime_error("GPU axis vertex limit exceeded");
    auto minimum=vertices.front().z,maximum=minimum;
    for(const auto& vertex:vertices) {minimum=std::min(minimum,vertex.z);maximum=std::max(maximum,vertex.z);}
    for(std::size_t i=0;i<vertices.size();++i) {
        if(vertices[i].z==maximum) result[0].push_back(std::uint32_t(i));
        if(vertices[i].z==minimum) result[1].push_back(std::uint32_t(i));
    }
    return result;
}
namespace {
float finite_float(double value) {
    if(!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max())
        throw std::runtime_error("Nonfinite GPU projection pose");
    return static_cast<float>(value);
}
std::int32_t rounded_word(double value) {
    if(!std::isfinite(value) || std::abs(value)>2147483647.0)
        throw std::runtime_error("GPU source prescale exceeds rounding range");
    return simulation::wrap16(static_cast<std::int64_t>(std::round(value)));
}
bool collinear(const assets::Vec3i& a,const assets::Vec3i& b,const assets::Vec3i& c) {
    // Decoded coordinates fit source words. Keep wider caller-supplied geometry
    // on the general path rather than overflowing an exact integer cross.
    for(const auto& p:{a,b,c}) for(auto v:{p.x,p.y,p.z}) if(v<-32768 || v>32767) return false;
    const std::int64_t bx=std::int64_t(b.x)-a.x,by=std::int64_t(b.y)-a.y,bz=std::int64_t(b.z)-a.z;
    const std::int64_t cx=std::int64_t(c.x)-a.x,cy=std::int64_t(c.y)-a.y,cz=std::int64_t(c.z)-a.z;
    return by*cz==bz*cy && bz*cx==bx*cz && bx*cy==by*cx;
}
}
std::optional<GpuProjection::MotionSurfaceSettings> pack_motion_surface(
    const PackedProjection& current,const PackedProjection& previous,
    std::uint32_t width,std::uint32_t height,std::uint32_t scale) {
    if(!current.continuous || !previous.continuous || !scale || scale>4 || !width || !height
        || current.continuous_vertices.empty()
        || current.continuous_vertices.size()!=previous.continuous_vertices.size()) return {};
    bool used[2]{};
    for(std::size_t i=0;i<current.continuous_vertices.size();++i) {
        const auto& a=current.continuous_vertices[i];const auto& b=previous.continuous_vertices[i];
        if(a.pose>1 || a.pose!=b.pose || a.x!=b.x || a.y!=b.y || a.z!=b.z) return {};
        used[a.pose]=true;
    }
    GpuProjection::MotionSurfaceSettings result;result.width=width;result.height=height;
    bool have=false;
    double mapping[3][4]{};
    for(unsigned kind=0;kind<2;++kind) if(used[kind]) {
        double matrices[2][3][4]{};
        for(unsigned which=0;which<2;++which) {
            const auto& packed=which?previous:current;
            const auto& p=packed.continuous_poses[kind];const auto& lo=packed.continuous_poses[kind+2];
            for(unsigned r=0;r<3;++r) {
                matrices[which][r][0]=double(p.row0[r])+lo.row0[r];
                matrices[which][r][1]=double(p.row1[r])+lo.row1[r];
                matrices[which][r][2]=double(p.row2[r])+lo.row2[r];
                matrices[which][r][3]=(double(p.translation[r])+lo.translation[r])+lo.vanish[r];
            }
        }
        const auto& a=matrices[0];const auto& b=matrices[1];
        const double det=a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1])
            -a[0][1]*(a[1][0]*a[2][2]-a[1][2]*a[2][0])+a[0][2]*(a[1][0]*a[2][1]-a[1][1]*a[2][0]);
        if(!std::isfinite(det) || std::abs(det)<1e-15) return {};
        double inverse[3][3]{};
        for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) {
            const unsigned r1=(c+1)%3,r2=(c+2)%3,c1=(r+1)%3,c2=(r+2)%3;
            inverse[r][c]=(a[r1][c1]*a[r2][c2]-a[r1][c2]*a[r2][c1])/det;
        }
        double candidate[3][4]{};
        for(unsigned r=0;r<3;++r) {
            for(unsigned c=0;c<3;++c) for(unsigned k=0;k<3;++k) candidate[r][c]+=b[r][k]*inverse[k][c];
            candidate[r][3]=b[r][3];
            for(unsigned k=0;k<3;++k) candidate[r][3]-=candidate[r][k]*a[k][3];
            for(unsigned c=0;c<4;++c) {
                if(!std::isfinite(candidate[r][c]) || !std::isfinite(float(candidate[r][c]))) return {};
                if(have && std::abs(candidate[r][c]-mapping[r][c])>1e-7*std::max(1.,std::abs(mapping[r][c]))) return {};
                mapping[r][c]=candidate[r][c];
            }
        }
        have=true;
    }
    unsigned row=0;
    for(auto* out:{result.previous_row0,result.previous_row1,result.previous_row2}) {
        for(unsigned c=0;c<4;++c) out[c]=float(mapping[row][c]);
        ++row;
    }
    for(unsigned which=0;which<2;++which) {
        const auto& p=(which?previous:current).continuous_poses[0];
        auto* projection=which?result.previous_projection:result.current_projection;
        projection[0]=projection[1]=p.translation[3]*scale;
        projection[2]=p.vanish[0]*scale;projection[3]=p.vanish[1]*scale;
    }
    return result;
}
PackedProjection pack_projection(const assets::Shape& shape,const RenderPose& pose,const RenderSettings& settings) {
    PackedProjection result;
    result.continuous=settings.render_scale>1 || pose.continuous_geometry;
    if(!result.continuous && (!pose.use_rotation_matrix || pose.subpixel_projection || settings.focal_length!=256.0))
        throw std::runtime_error("GPU projection combination requires another projection/visibility path");
    const auto& vertices=shape.frames.empty()?shape.vertices:shape.frames[pose.animation_frame%shape.frames.size()].vertices;
    const auto& words=shape.frames.empty()?shape.word_coordinates:shape.frames[pose.animation_frame%shape.frames.size()].word_coordinates;
    bool byte_coordinates=false;
    for(std::size_t i=0;i<vertices.size();++i) byte_coordinates=byte_coordinates || i>=words.size() || !words[i];
    if(byte_coordinates && shape.header.shift>=32) throw std::runtime_error("Invalid GPU shape scale shift");
    const double factor=byte_coordinates?pose.scale*static_cast<double>(std::uint32_t{1}<<shape.header.shift):1.0;
    static_cast<void>(finite_float(factor));
    if(vertices.size()>4U*1024*1024) throw std::runtime_error("GPU projection vertex limit exceeded");
    for(const auto& visibility:shape.visibilities) {
        std::uint32_t flags=0;
        if(result.continuous && visibility.a<vertices.size() && visibility.b<vertices.size() && visibility.c<vertices.size()) {
            const auto word=[&](std::size_t index){return index<words.size() && words[index];};
            if(word(visibility.a)==word(visibility.b) && word(visibility.a)==word(visibility.c)
                && collinear(vertices[visibility.a],vertices[visibility.b],vertices[visibility.c])) flags=2;
        }
        result.visibility_faces.push_back({visibility.a,visibility.b,visibility.c,flags});
    }
    result.visibility_faces.push_back({UINT32_MAX,UINT32_MAX,UINT32_MAX,1});
    if(!result.continuous) {
        auto& p=result.native_pose;
        for(unsigned i=0;i<3;++i) {
            p.row0[i]=pose.rotation_matrix[i];p.row1[i]=pose.rotation_matrix[3+i];p.row2[i]=pose.rotation_matrix[6+i];
        }
        p.translation[0]=rounded_word(pose.x);p.translation[1]=rounded_word(pose.y);p.translation[2]=rounded_word(pose.z);
        p.vanish[0]=rounded_word(pose.vanish_x);p.vanish[1]=rounded_word(pose.vanish_y);
        result.native_vertices.reserve(vertices.size());
        for(std::size_t i=0;i<vertices.size();++i) {
            const auto& v=vertices[i];const double scale=i<words.size() && words[i]?1.0:factor;
            result.native_vertices.push_back({rounded_word(v.x*scale),rounded_word(v.y*scale),rounded_word(v.z*scale),0});
        }
        return result;
    }
    // Build matrix coefficients once per pose. Euler order is X, Y, Z, just
    // as in rotate(); each basis vector becomes one input-coordinate row.
    double matrix[3][3]{};
    std::array<double,3> euler_cos{},euler_sin{};
    if(!pose.use_rotation_matrix) {
        const double angles[]{pose.pitch*2*std::numbers::pi/65536.0,
            pose.yaw*2*std::numbers::pi/65536.0,pose.roll*2*std::numbers::pi/65536.0};
        for(unsigned c=0;c<3;++c) {euler_cos[c]=std::cos(angles[c]);euler_sin[c]=std::sin(angles[c]);}
    }
    for(unsigned axis=0;axis<3;++axis) {
        if(pose.use_rotation_matrix) {
            for(unsigned c=0;c<3;++c) matrix[axis][c]=pose.rotation_matrix[axis*3+c]/32768.0;
        } else {
            double x=axis==0?1:0,y=axis==1?1:0,z=axis==2?1:0;
            const double y1=y*euler_cos[0]-z*euler_sin[0],z1=y*euler_sin[0]+z*euler_cos[0];
            const double x2=x*euler_cos[1]+z1*euler_sin[1],z2=-x*euler_sin[1]+z1*euler_cos[1];
            matrix[axis][0]=x2*euler_cos[2]-y1*euler_sin[2];
            matrix[axis][1]=x2*euler_sin[2]+y1*euler_cos[2];matrix[axis][2]=z2;
        }
    }
    for(unsigned kind=0;kind<2;++kind) {
        auto& p=result.continuous_poses[kind];const double scale=kind?1.0:factor;
        for(unsigned c=0;c<3;++c) {p.row0[c]=finite_float(matrix[0][c]*scale);p.row1[c]=finite_float(matrix[1][c]*scale);p.row2[c]=finite_float(matrix[2][c]*scale);}
        p.translation[0]=finite_float(pose.x);p.translation[1]=finite_float(pose.y);p.translation[2]=finite_float(pose.z);p.translation[3]=finite_float(settings.focal_length);
        p.vanish[0]=finite_float(pose.vanish_x);p.vanish[1]=finite_float(pose.vanish_y);
        auto& low=result.continuous_poses[kind+2];p.vanish[3]=1;
        for(unsigned c=0;c<3;++c) {
            low.row0[c]=finite_float(matrix[0][c]*scale-double(p.row0[c]));
            low.row1[c]=finite_float(matrix[1][c]*scale-double(p.row1[c]));
            low.row2[c]=finite_float(matrix[2][c]*scale-double(p.row2[c]));
        }
        const double translation[]{pose.x,pose.y,pose.z};
        for(unsigned c=0;c<3;++c) {
            const double tail=translation[c]-double(p.translation[c]);
            low.translation[c]=finite_float(tail);
            // A second tail keeps sub-ULP source positions when a translated
            // model meets a raster half-pixel. The low pose's vanish XYZ is
            // otherwise unused by ordinary matrix projection.
            low.vanish[c]=finite_float(tail-double(low.translation[c]));
        }
    }
    result.continuous_vertices.reserve(vertices.size());
    if(!pose.use_rotation_matrix) {
        auto& high=result.euler_operands[0];auto& low=result.euler_operands[1];
        for(unsigned c=0;c<3;++c) {
            const double cosine=euler_cos[c],sine=euler_sin[c];
            high.row0[c]=finite_float(cosine);low.row0[c]=finite_float(cosine-double(high.row0[c]));
            high.row1[c]=finite_float(sine);low.row1[c]=finite_float(sine-double(high.row1[c]));
            high.translation[c]=finite_float((cosine-double(high.row0[c]))-double(low.row0[c]));
            low.translation[c]=finite_float((sine-double(high.row1[c]))-double(low.row1[c]));
        }
        high.row2[0]=finite_float(factor);low.row2[0]=finite_float(factor-double(high.row2[0]));
        high.row2[1]=1.f; // Word coordinates bypass header/object scaling.
    }
    for(std::size_t i=0;i<vertices.size();++i) {
        const auto& v=vertices[i];result.continuous_vertices.push_back({finite_float(v.x),finite_float(v.y),finite_float(v.z),i<words.size() && words[i]?1U:0U});
    }
    return result;
}
}
