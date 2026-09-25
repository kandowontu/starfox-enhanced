#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/background_tiles.hpp"
#include <cmath>
#include <stdexcept>
namespace starfox::vr {
simulation::MatrixQ15 landscape_scene_view(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha) {
    return simulation::interpolate_rotation_matrix_q15(previous.view_matrix,current.view_matrix,alpha);
}
Matrix4 landscape_camera_motion(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid landscape camera fraction");
    alpha=std::clamp(alpha,0.,1.);
    if(previous.flow!=current.flow || !same_landscape_mapping(previous,current)
        || timing::camera_transform_is_discontinuous(previous.camera,current.camera)) alpha=1.;
    const auto view=landscape_scene_view(previous,current,alpha);
    Matrix4 motion{};motion[15]=1;
    constexpr int sign[]{1,-1,-1};
    for(unsigned column=0;column<3;++column) for(unsigned row=0;row<3;++row)
        motion[column*4+row]=float(view[column*3+row])*sign[column]*sign[row]/32768.F;
    const auto camera=timing::interpolate(previous.camera,current.camera,alpha);
    const float delta=float(camera.y-current.camera.y)/256.F;
    for(unsigned row=0;row<3;++row) motion[12+row]=motion[4+row]*delta;
    return motion;
}
std::vector<render::RenderPose> interpolate_scene_poses(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& current,double alpha,const SceneInterpolationRules& rules,bool shadows) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid scene interpolation fraction");
    alpha=std::clamp(alpha,0.,1.);
    if(previous.flow!=current.flow || timing::camera_transform_is_discontinuous(previous.camera,current.camera)) alpha=1;
    auto camera=timing::interpolate(previous.camera,current.camera,alpha);
    // Preserve source tracking and cinematic camera motion for every object.
    auto view=simulation::interpolate_rotation_matrix_q15(previous.view_matrix,current.view_matrix,alpha);
    const auto delta=[](double value,double origin) {
        auto d=std::fmod(value-origin,65536.);if(d>32767.) d-=65536.;else if(d< -32768.) d+=65536.;return d;
    };
    std::vector<render::RenderPose> poses;poses.reserve(current.objects.size());
    for(const auto& item:current.objects) {
        auto before=item.presentation,now=item.presentation;
        const auto old=previous.transforms.find(item.handle);
        if(old!=previous.transforms.end() && old->second.generation==now.generation
            && old->second.shape==now.shape && old->second.type==now.type && old->second.strategy_address==now.strategy_address)
            before=old->second;
        if(rules.crosshair && item.object.strategy_address==rules.crosshair) {
            const auto* station=render::reticle_previous_snapshot(now,current.transforms,previous.transforms,current.player);
            if(station) before=*station;
            else {
                before=now;
                const auto owner=current.transforms.find(current.player),old_owner=previous.transforms.find(current.player);
                if(owner!=current.transforms.end() && old_owner!=previous.transforms.end()) {
                    before.transform=timing::relative_birth_snapshot(now.transform,old_owner->second.transform,owner->second.transform);
                    before.rotation_matrix=old_owner->second.rotation_matrix;
                }
            }
        }
        if(rules.flash_player && item.object.strategy_address==rules.flash_player)
            render::anchor_player_overlay(before,now,previous.transforms,current.transforms,current.player);
        const auto object_alpha=rules.trail && item.object.strategy_address==rules.trail?1.:alpha;
        auto transform=timing::interpolate(before.transform,now.transform,object_alpha);
        if(rules.crosshair && item.object.strategy_address==rules.crosshair)
            transform.y+=std::lerp(double(previous.view_float_y),double(current.view_float_y),alpha);
        auto rotation=render::interpolate_object_rotation(before,now,object_alpha,rules.discrete_rotation_shape);
        auto pose=item.source_pose;
        pose.explosion_phase=render::interpolate_explosion_progress(
            old==previous.transforms.end()?nullptr:&old->second,now,object_alpha);
        if(shadows) {
            rotation[1]=rotation[4]=rotation[7]=0;
            auto source_rotation=item.presentation.rotation_matrix;
            source_rotation[1]=source_rotation[4]=source_rotation[7]=0;
            pose.source_lighting_matrix=simulation::multiply_matrix_q15(source_rotation,current.view_matrix);
            pose.simple_scaled_sprite=false;
            if(!(item.object.strategy_flags[0]&4U)) {
                transform.y=current.shadow_height;
                pose.force_colour=true;pose.forced_colour=9;
            }
        }
        const auto x=delta(transform.x,camera.x),y=delta(transform.y,camera.y),z=delta(transform.z,camera.z);
        pose.x=(x*view[0]+y*view[3]+z*view[6])/32768.;
        pose.y=(x*view[1]+y*view[4]+z*view[7])/32768.;
        pose.z=(x*view[2]+y*view[5]+z*view[8])/32768.;
        pose.rotation_matrix=alpha>0 && alpha<1?simulation::multiply_presentation_matrix_q15(rotation,view)
            :simulation::multiply_matrix_q15(rotation,view);
        pose.use_rotation_matrix=true;
        pose.continuous_geometry=object_alpha!=1 || !(rules.trail && item.object.strategy_address==rules.trail);
        pose.subpixel_projection=object_alpha>0 && object_alpha<1;
        poses.push_back(pose);
    }
    return poses;
}
}
