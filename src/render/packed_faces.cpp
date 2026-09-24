#include "starfox/render/packed_faces.hpp"
#include "starfox/render/face_material.hpp"
#include "starfox/render/source_shading.hpp"
#include <stdexcept>
#include <unordered_map>
namespace starfox::render {
std::vector<ContinuousTransformPose> pack_continuous_fragments(PackedProjection& vertices,
    PackedFaces& faces,const PackedBsp& graph,const RenderPose& pose,const RenderSettings& settings,bool colour_warp) {
    if(!vertices.continuous || !pose.explosion_progress || pose.collapse_to_axis_line
        || faces.polygons.size()!=graph.faces.size() || faces.materials.size()!=graph.faces.size())
        throw std::runtime_error("Invalid continuous fragment inputs");
    for(size_t f=0;f<graph.faces.size();++f) {
        const auto& polygon=faces.polygons[f];
        if(polygon[0]>faces.corners.size() || graph.faces[f].vertex_indices.size()>faces.corners.size()-polygon[0])
            throw std::runtime_error("Fragment corners exceed source storage");
    }
    auto normal_pose=pose;normal_pose.scale=1;normal_pose.x=normal_pose.y=normal_pose.z=0;
    const auto normal_projection=pack_projection(assets::Shape{},normal_pose,settings);
    const auto original=std::move(vertices.continuous_vertices);
    vertices.continuous_vertices.clear();vertices.continuous_vertices.reserve(faces.corners.size()+graph.faces.size());
    std::vector<ContinuousTransformPose> poses;
    poses.reserve(graph.faces.size()*6+4);
    vertices.visibility_faces={{{UINT32_MAX,UINT32_MAX,UINT32_MAX,1}}};
    vertices.visibility_faces.reserve(graph.faces.size()+1);
    for(size_t f=0;f<graph.faces.size();++f) {
        const auto& face=graph.faces[f];auto& polygon=faces.polygons[f];polygon[2]=0;
        const auto base=uint32_t(poses.size());
        for(auto p:vertices.continuous_poses) poses.push_back(p);
        for(unsigned kind=0;kind<2;++kind) poses[base+kind].vanish[2]=float(base+5);
        auto normal=normal_projection.continuous_poses[1],low=normal_projection.continuous_poses[3];
        normal.row0[3]=float(face.normal.x);normal.row1[3]=float(face.normal.y);normal.row2[3]=float(face.normal.z);
        normal.translation[3]=float(pose.explosion_progress);
        normal.vanish[3]=pose.use_rotation_matrix && !pose.subpixel_projection?1.f:0.f;
        if(normal.vanish[3]==1) for(unsigned c=0;c<3;++c) {
            normal.row0[c]=float(pose.rotation_matrix[c]);normal.row1[c]=float(pose.rotation_matrix[3+c]);normal.row2[c]=float(pose.rotation_matrix[6+c]);
        }
        poses.push_back(normal);poses.push_back(low);
        if(face.sprite && (colour_warp || faces.materials[f].textured) && face.vertex_indices.size()==1) {
            const auto source=face.vertex_indices[0];uint32_t centre=UINT32_MAX;
            if(source<original.size()) {
                auto vertex=original[source];vertex.pose+=uint32_t(graph.faces.size()*6);
                centre=uint32_t(vertices.continuous_vertices.size());vertices.continuous_vertices.push_back(vertex);
            }
            polygon[2]=uint32_t(vertices.visibility_faces.size());vertices.visibility_faces.push_back({centre,0,0,3});
        }
        for(size_t j=0;j<face.vertex_indices.size();++j) {
            const auto source=face.vertex_indices[j];auto& corner=faces.corners[polygon[0]+j];
            if(source>=original.size()) {polygon[1]=0;corner[0]=UINT32_MAX;continue;}
            auto vertex=original[source];vertex.pose+=base;
            corner[0]=uint32_t(vertices.continuous_vertices.size());vertices.continuous_vertices.push_back(vertex);
        }
    }
    for(auto p:vertices.continuous_poses) poses.push_back(p);
    return poses;
}
PackedWarpShading pack_warp_shading(const assets::Shape& shape,const RenderPose& pose,const RenderSettings& settings,bool axis) {
    PackedWarpShading result;
    auto& out=result.settings;
    const auto shading=source_shading(pose);
    out.depth_band=std::uint32_t(shading.depth_band);
    out.flags=(pose.palette_override?1U:0U)|(shape.has_diffuse_shade_tables?4U:0U)
        |(pose.has_depth_colour_tables?8U:0U)|(axis?16U:0U);
    out.override_colour=pose.palette_override.value_or(0);
    out.colour_base=settings.colour_index_base;
    out.scroll_x=pose.texture_scroll_x;out.scroll_y=pose.texture_scroll_y;
    for(unsigned i=0;i<3;++i) out.light[i]=shading.light[i];
    for(unsigned band=0;band<4;++band) {
        const auto& table=shape.diffuse_shade_tables[band];
        if(table.size()>62) throw std::runtime_error("GPU warp diffuse table exceeds 62 materials");
        out.shade_counts[band]=std::uint32_t(table.size());
        for(std::size_t material=0;material<table.size();++material)
            std::copy_n(table[material].data(),10,result.diffuse.data()+(band*62+material)*10);
    }
    return result;
}
PackedWarpTextures pack_warp_textures(const assets::Shape& shape) {
    PackedWarpTextures result;
    result.lookup.assign(65536,UINT32_MAX);
    for(const auto& texture:shape.textures) {
        if(result.lookup[texture.descriptor]!=UINT32_MAX) continue;
        const auto size=std::size_t(texture.u_mask+1U)*(texture.v_mask+1U);
        if(texture.texels.size()<size) throw std::runtime_error("GPU warp texture is shorter than its coordinate masks");
        if(size>16U*1024*1024-result.texels.size()) throw std::runtime_error("GPU warp texture budget exceeded");
        result.lookup[texture.descriptor]=std::uint32_t(result.textures.size());
        result.textures.push_back({std::uint32_t(result.texels.size()),texture.u_mask,texture.v_mask,std::uint32_t(result.coordinates.size())});
        for(const auto& uv:texture.coordinates) result.coordinates.push_back({uv.u,uv.v});
        result.texels.insert(result.texels.end(),texture.texels.begin(),texture.texels.begin()+size);
    }
    result.texels.resize((result.texels.size()+3U)&~std::size_t(3),0);
    return result;
}
static PackedFaces pack_faces_impl(const assets::Shape& shape,const PackedBsp& bsp,const RenderPose& pose,const RenderSettings& settings,bool warp_template) {
    const char* unsupported=(pose.colour_warp && !pose.force_colour && !warp_template)?"colour warp":
        pose.simple_scaled_sprite?"whole-object sprite":pose.collapse_to_axis_line?"axis collapse":
        nullptr;
    if(unsupported)
        throw std::runtime_error("GPU face packing does not support "+std::string(unsupported)
            +" for model "+shape.name+" (address "+std::to_string(shape.header.address)+")");
    PackedFaces result;const auto shading=source_shading(pose);
    std::unordered_map<const assets::TextureImage*,std::uint32_t> textures;
    for(const auto& face:bsp.faces) {
        auto primitive=face.sprite?PackedPrimitive::sprite:face.vertex_indices.size()==2?PackedPrimitive::line
            :face.vertex_indices.size()>=3?PackedPrimitive::polygon:PackedPrimitive::empty;
        if(face.vertex_indices.size()>32) throw std::runtime_error("GPU polygon corner limit exceeded");
        result.primitives.push_back(primitive);
        if(primitive==PackedPrimitive::line || primitive==PackedPrimitive::sprite) result.polygon_only=false;
        auto material=warp_template?FaceMaterial{}:face_material(shape,face,pose.colour_frame,shading.depth_band,shading.light,pose,std::nullopt,settings.colour_index_base);
        // Native two-point faces use the material colour through draw_line,
        // even when the descriptor also resolves to texture artwork. Match
        // shape_batch: do not upload or sample that artwork for lines.
        if(primitive==PackedPrimitive::line) material.texture=nullptr;
        RasterCommand command;
        command.even=std::uint8_t(settings.colour_index_base+material.colour.even);
        command.odd=std::uint8_t(settings.colour_index_base+material.colour.odd);
        command.dither=material.colour.dither;
        command.tag=std::uint32_t(pose.terrain_geometry?PixelLayer::terrain_geometry:pose.world_geometry?PixelLayer::world_geometry:PixelLayer::three_d);
        if(primitive==PackedPrimitive::polygon) command.reserved1=(pose.cel_mode?1U:0U)|(std::uint32_t(pose.wireframe_mode)<<1)|((pose.wobble_mode&2U)!=0?65536U:0U);
        if(primitive==PackedPrimitive::polygon && (pose.wobble_mode&1U)!=0) command.reserved1|=262144U;
        if(primitive==PackedPrimitive::polygon && pose.wave_mode && !pose.cel_mode
            && pose.wireframe_mode==0 && (pose.wobble_mode&2U)==0) {
            command.reserved1|=131072U;
            command.du=pose.wave_offset;command.dv=pose.animation_frame&15U;
        }
        if(material.texture) {
            const auto& texture=*material.texture;
            if(texture.texels.size()<std::size_t(texture.u_mask+1U)*(texture.v_mask+1U))
                throw std::runtime_error("GPU model texture is shorter than its coordinate masks");
            auto found=textures.find(&texture);
            if(found==textures.end()) {
                if(texture.texels.size()>16U*1024*1024 || result.texels.size()>16U*1024*1024-texture.texels.size())
                    throw std::runtime_error("GPU model texture budget exceeded");
                found=textures.emplace(&texture,std::uint32_t(result.texels.size())).first;
                result.texels.insert(result.texels.end(),texture.texels.begin(),texture.texels.end());
            }
            command.textured=1;command.texture_offset=found->second;command.u_mask=texture.u_mask;command.v_mask=texture.v_mask;
            command.colour_base=settings.colour_index_base;command.reserved0=std::uint32_t(pose.texture_scroll_x);command.reserved1=std::uint32_t(pose.texture_scroll_y);
            command.tag=std::uint32_t(primitive==PackedPrimitive::sprite?PixelLayer::two_d:primitive==PackedPrimitive::line?PixelLayer::three_d:PixelLayer::textured_geometry);
        }
        const auto first=std::uint32_t(result.corners.size());
        for(std::size_t corner=0;corner<face.vertex_indices.size();++corner) {
            assets::TextureCoordinate uv{};
            if(material.texture) uv=material.texture->coordinates[corner%material.texture->coordinates.size()];
            result.corners.push_back({face.vertex_indices[corner],uv.u,uv.v,0});
        }
        const auto visibility=!pose.explosion_progress && face.visibility_index>=0 && std::size_t(face.visibility_index)<shape.visibilities.size()
            ?std::uint32_t(face.visibility_index):std::uint32_t(shape.visibilities.size());
        const bool emitted=primitive!=PackedPrimitive::empty && (primitive!=PackedPrimitive::sprite || face.vertex_indices.size()==1);
        result.polygons.push_back({first,emitted?std::uint32_t(face.vertex_indices.size()):0U,
            visibility,command.textured|(primitive==PackedPrimitive::line?2U:0U)|(primitive==PackedPrimitive::sprite?4U:0U)
                |(settings.backface_culling && primitive==PackedPrimitive::polygon?8U:0U)});
        result.materials.push_back(command);
    }
    // Raster shaders fetch byte-addressed texels using aligned 32-bit loads.
    result.texels.resize((result.texels.size()+3U)&~std::size_t(3),0);
    return result;
}
PackedFaces pack_faces(const assets::Shape& shape,const PackedBsp& bsp,const RenderPose& pose,const RenderSettings& settings) {
    return pack_faces_impl(shape,bsp,pose,settings,false);
}
PackedFaces pack_warp_faces(const assets::Shape& shape,const PackedBsp& bsp,const RenderPose& pose,const RenderSettings& settings) {
    return pack_faces_impl(shape,bsp,pose,settings,true);
}
}
