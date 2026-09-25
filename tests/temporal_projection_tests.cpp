#include "starfox/render/temporal_projection.hpp"
#include "starfox/render/temporal_jitter.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace starfox::render;
void require(bool v) {if(!v) throw std::runtime_error("Temporal projection assertion failed");}
std::array<double,4> transform(std::array<double,4> v,const std::array<float,16>& m) {
    std::array<double,4> out{};
    for(unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r) out[c]+=v[r]*m[r*4+c];
    return out;
}
int main() try {
    for(unsigned i=0;i<64;++i) {
        const auto j=temporal_jitter(i);require(valid_raster_jitter(j));
        require(j==temporal_jitter(i+32));
        for(const auto value:j) require(value>=-.5f && value<=.5f && std::round(value*256)==value*256);
        require(j!=temporal_jitter(i+1));
    }
    require(!valid_raster_jitter({std::numeric_limits<float>::quiet_NaN(),0}));
    require(!valid_raster_jitter({0,17}));
    unsigned samples=0;
    // SDK ratios round each axis independently. Preserve the same physical
    // field of view and clip coordinates at the reduced raster dimensions.
    for(const auto size:{std::array<unsigned,2>{533,299},{464,260},{400,224}}) {
        const auto full=temporal_projection(800,448,512,400,224,.1f,100000.f);
        const auto reduced=temporal_projection(size[0],size[1],512.f*size[0]/800,
            size[0]*.5f,size[1]*.5f,.1f,100000.f,512.f*size[1]/448);
        require(bool(full) && bool(reduced));
        for(unsigned i=0;i<16;++i)
            require(std::abs(full->view_to_clip[i]-reduced->view_to_clip[i])<1e-6);
        require(std::abs(full->vertical_fov-reduced->vertical_fov)<1e-6);
        require(std::abs(full->aspect-reduced->aspect)<1e-6);
    }
    for(unsigned w:{224u,800u,1600u}) for(unsigned h:{192u,448u,900u})
    for(float focal:{128.f,512.f}) for(float cx:{70.25f,400.f}) {
        auto p=temporal_projection(w,h,focal,cx,93.75f,.1f,100000.f);require(bool(p));
        auto previous=temporal_projection(w,h,focal*.9f,cx+12,100.f,.1f,100000.f);require(bool(previous));
        const auto mapping=temporal_matrix_product(p->clip_to_view,previous->view_to_clip);
        const auto inverse_mapping=temporal_matrix_product(previous->clip_to_view,p->view_to_clip);
        for(double z:{.1,1.,100.,10000.}) {
            const std::array<double,4> point{z*.2,-z*.3,z,1};
            const auto clip=transform(point,p->view_to_clip);
            const auto old=transform(point,previous->view_to_clip);
            const auto mapped=transform(clip,mapping);
            const auto back=transform(mapped,inverse_mapping);
            require(std::abs((clip[0]/clip[3]+1)*w*.5-(cx+focal*.2))<.001);
            require(std::abs((1-clip[1]/clip[3])*h*.5-(93.75-focal*.3))<.001);
            for(unsigned i=0;i<4;++i) {
                require(std::abs(mapped[i]-old[i])<.003*std::max(1.,z));
                require(std::abs(back[i]-clip[i])<.003*std::max(1.,z));
            }
            for(unsigned i=0;i<3;++i) {
                require(std::abs(mapped[i]/mapped[3]-old[i]/old[3])<1e-5);
                require(std::abs(back[i]/back[3]-clip[i]/clip[3])<1e-5);
            }
            ++samples;
        }
    }
    const float nan=std::numeric_limits<float>::quiet_NaN();
    TemporalCamera old_camera,current_camera;
    old_camera.position={32760,5,30};current_camera.position={-32760,8,34};
    current_camera.world_to_view={0,0,-1,0,1,0,1,0,0};
    auto camera_back=temporal_camera_mapping(current_camera,old_camera);
    auto camera_forward=temporal_camera_mapping(old_camera,current_camera);
    require(bool(camera_back) && bool(camera_forward));
    // World point relative to current is (10,20,100); relative to previous
    // includes the wrapped camera displacement (+16,+3,+4).
    const auto old_point=transform({100,20,-10,1},*camera_back);
    require(std::abs(old_point[0]-26)<1e-5 && std::abs(old_point[1]-23)<1e-5 && std::abs(old_point[2]-104)<1e-5);
    const auto current_point=transform(old_point,*camera_forward);
    require(std::abs(current_point[0]-100)<1e-5 && std::abs(current_point[1]-20)<1e-5 && std::abs(current_point[2]+10)<1e-5);
    const auto current_projection=temporal_projection(800,448,512,400,224,.1f,100000.f);
    const auto old_projection=temporal_projection(800,448,480,390,220,.1f,100000.f);
    const auto clip_mapping=temporal_matrix_product(temporal_matrix_product(current_projection->clip_to_view,*camera_back),old_projection->view_to_clip);
    const auto mapped_clip=transform(transform({100,20,-10,1},current_projection->view_to_clip),clip_mapping);
    const auto expected_clip=transform(old_point,old_projection->view_to_clip);
    for(unsigned i=0;i<3;++i) require(std::abs(mapped_clip[i]/mapped_clip[3]-expected_clip[i]/expected_clip[3])<1e-5);
    current_camera.world_to_view.fill(0);require(!temporal_camera_mapping(current_camera,old_camera));
    current_camera=old_camera;current_camera.position[0]=nan;
    require(!temporal_camera_mapping(current_camera,old_camera));
    std::array<float,16> quantized_axes{};quantized_axes[0]=.9999695f;
    quantized_axes[5]=.9999695f;quantized_axes[10]=.9999695f;
    require(temporal_unit_axis(quantized_axes,0)->at(0)==1);
    require(temporal_unit_axis(quantized_axes,1,-1)->at(1)==-1);
    require(temporal_unit_axis(quantized_axes,2)->at(2)==1);
    require(!temporal_unit_axis(quantized_axes,3));
    quantized_axes[0]=nan;require(!temporal_unit_axis(quantized_axes,0));
    quantized_axes.fill(0);require(!temporal_unit_axis(quantized_axes,0));
    require(!temporal_projection(0,100,256,0,0,.1f,100));
    require(!temporal_projection(100,100,0,0,0,.1f,100));
    require(!temporal_projection(100,100,256,nan,0,.1f,100));
    require(!temporal_projection(100,100,256,0,0,100,100));
    std::cout<<samples<<" temporal projection/reprojection samples passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
