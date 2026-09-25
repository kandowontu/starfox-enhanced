#include "starfox/vr/draw_packet.hpp"
#include <cmath>
#include <bit>
#include <cstring>
#include <limits>

namespace starfox::vr {
bool build_draw_packet(const assets::Shape& shape,const render::RenderPose& pose,
    std::span<const render::Rgba8> palette,uint8_t base,uint32_t dither_scale,
    bool srgb,float units,DrawPacket& output,std::string& error) {
    if(!std::isfinite(pose.scale) || pose.scale<=0 || pose.scale>std::numeric_limits<float>::max()
        || (pose.use_source_lighting_state && !std::isfinite(pose.source_depth))) {
        error="Invalid source model scale or lighting depth";return false;
    }
    if(pose.effect_clip_right>pose.effect_clip_left) {
        error="Source effect clipping needs a native eye-space mask";return false;
    }
    const auto model=game_model_matrix(pose,units);
    if(!model) {error="Invalid native model transform";return false;}
    if(pose.simple_scaled_sprite) {
        // Whole-object source sprites bypass face shading, rotations and BSP.
        auto placement=pose;placement.use_rotation_matrix=false;placement.pitch=placement.yaw=placement.roll=0;
        DrawPacket next;next.model=*game_model_matrix(placement,units);next.shading=render::source_shading(pose);
        const auto* texture=render::texture_for_colour(shape,pose.simple_sprite_colour,pose.colour_frame);
        if(texture && pose.z>=128 && pose.simple_sprite_world_size>0) {
            const auto width=uint32_t(texture->u_mask)+1,height=uint32_t(texture->v_mask)+1;
            if(texture->texels.size()!=width*height) {error="Invalid whole-object sprite texture";return false;}
            {
                next.geometry.texels.resize(256);
                for(unsigned texel=1;texel<256;++texel) {
                    const auto index=pose.palette_override.value_or(static_cast<uint8_t>(base+texel));
                    if(index>=palette.size()) continue;
                    const auto c=palette[index];next.geometry.texels[texel]=uint32_t(c.r)|(uint32_t(c.g)<<8U)|(uint32_t(c.b)<<16U)|0xff000000U;
                }
                next.geometry.texels.resize(256+(texture->texels.size()+3)/4);
                if(palette.size()<256) {
                    for(const auto texel:texture->texels) {
                        if(texel && pose.palette_override.value_or(static_cast<uint8_t>(base+texel))>=palette.size()) {
                            error="Invalid whole-object sprite palette";return false;
                        }
                    }
                }
                if constexpr(std::endian::native==std::endian::little) {
                    std::memcpy(next.geometry.texels.data()+256,texture->texels.data(),texture->texels.size());
                } else {
                    for(size_t i=0;i<texture->texels.size();++i)
                        next.geometry.texels[256+i/4]|=uint32_t(texture->texels[i])<<((i&3)*8);
                }
                const float corners[4][2]{{-1,1},{1,1},{1,-1},{-1,-1}};
                for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                    SceneVertex v{};v.texture[1]=texture->u_mask;v.texture[2]=texture->v_mask;v.texture[3]=5U|134217728U|536870912U|(srgb?2U:0U);
                    // Retain corner signs and source dimensions. The vertex
                    // shader applies native truncation/capping per draw.
                    v.billboard[0]=corners[corner][0];v.billboard[1]=corners[corner][1];
                    v.group_a[0]=float(pose.simple_sprite_world_size);v.group_a[1]=float(pose.z);
                    v.uv[0]=corners[corner][0]>0?float(width):0;v.uv[1]=corners[corner][1]<0?float(height):0;
                    next.geometry.vertices.push_back(v);
                }
            }
        }
        output=std::move(next);error.clear();return true;
    }
    ShapeMesh mesh;
    if(!decode_shape_mesh(shape,pose.animation_frame,static_cast<float>(pose.scale),mesh,error)) return false;
    DrawPacket next;next.model=*model;next.shading=render::source_shading(pose);
    if(pose.collapse_to_axis_line && !mesh.vertices.empty() && !shape.faces.empty()) {
        if(pose.colour_warp && !pose.force_colour && !pose.palette_override) {error="Native colour-warp stage pending";return false;}
        // The source collapses to the centroids of the original vertex
        // frame's minimum/maximum Z groups, not the scaled mesh's extrema.
        // This distinction matters for mixed byte/word-coordinate models.
        // Averaging object-local positions commutes with the model transform;
        // projection, eye clipping and line rasterization remain on the GPU.
        const auto& points=shape.frames.empty()?shape.vertices:
            shape.frames[pose.animation_frame%shape.frames.size()].vertices;
        const auto bounds=std::minmax_element(points.begin(),points.end(),
            [](const auto& a,const auto& b){return a.z<b.z;});
        auto material=render::face_material(shape,shape.faces.front(),pose.colour_frame,
            next.shading.depth_band,next.shading.light,pose,std::nullopt,base);
        // The source's axis line uses the first face's colour even when that
        // face normally carries a texture. It does not sample texture pixels.
        material.texture=nullptr;
        SceneVertex prototype{};
        if(!apply_scene_material(prototype,material,palette,base,dither_scale,srgb)) {
            error="Invalid axis-line material";return false;
        }
        for(const auto z:{bounds.second->z,bounds.first->z}) {
            std::array<double,3> sum{};std::size_t count=0;
            for(std::size_t i=0;i<points.size();++i) if(points[i].z==z) {
                for(unsigned axis=0;axis<3;++axis) sum[axis]+=mesh.vertices[i].position[axis];
                ++count;
            }
            auto vertex=prototype;
            for(unsigned axis=0;axis<3;++axis) vertex.position[axis]=static_cast<float>(sum[axis]/count);
            next.geometry.line_vertices.push_back(vertex);
        }
        next.geometry.line_ranges.push_back({0,0,2,-1});
        output=std::move(next);error.clear();return true;
    }
    if(!build_shape_batch(shape,mesh,pose,next.shading.depth_band,next.shading.light,
        palette,base,dither_scale,srgb,next.geometry,error)) return false;
    if(pose.explosion_progress) {
        // The GPU needs the source (pre-headset) Y axis for downward fragments.
        // Supply coefficients, not CPU-transformed vertices/face offsets. Reuse
        // visibility attributes because source explosions disable those tests.
        const auto source_matrix=game_model_matrix(pose,1.F);
        if(!source_matrix) {error="Invalid explosion source transform";return false;}
        for(auto* vertices:{&next.geometry.vertices,&next.geometry.line_vertices}) for(auto& v:*vertices) {
            for(unsigned axis=0;axis<3;++axis) {
                v.visibility_a[axis]=(*source_matrix)[axis*4];
                v.visibility_b[axis]=-(*source_matrix)[axis*4+1];
                v.visibility_c[axis]=-(*source_matrix)[axis*4+2];
            }
            v.group_a[0]=float(pose.x);v.group_a[1]=float(pose.y);v.group_a[2]=float(pose.z);
            v.group_c[0]=float(pose.explosion_phase.value_or(
                double(pose.explosion_progress)));v.group_c[1]=units;
            v.group_c[2]=pose.use_rotation_matrix && !pose.subpixel_projection?1.F:0.F;
        }
        next.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    }
    // A renderable packet must not silently omit primitives.
    if(!next.geometry.deferred.empty()) {error="Source primitive still requires a native GPU implementation";return false;}
    output=std::move(next);error.clear();return true;
}
}
