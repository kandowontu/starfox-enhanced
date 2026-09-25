#include "starfox/render/packed_faces.hpp"
#include "starfox/render/ray_materials.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
#include <string_view>
using namespace starfox;
void require(bool value){if(!value) throw std::runtime_error("Packed faces assertion failed");}
int main(int argc,char** argv)try {
    {
        render::PackedFaces faces;
        faces.polygons={{0,3,0,0},{3,3,0,0}};
        faces.corners={{0,0,0,0},{1,1,0,0},{2,0,1,0},
            {3,0,0,0},{4,1,0,0},{5,0,1,0}};
        faces.materials.resize(2);faces.texels={0,4,8,12};
        faces.materials[0].even=21;faces.materials[0].odd=22;faces.materials[0].dither=1;
        auto& texture=faces.materials[1];texture.textured=1;texture.u_mask=texture.v_mask=1;
        texture.colour_base=32;texture.reserved0=std::uint32_t(-3);
        std::array<std::array<std::uint32_t,4>,2> topology{{{3,4,5,1},{0,1,2,0}}};
        render::RayMaterials materials;
        require(render::pack_ray_materials(faces,topology,materials));
        require(materials.triangles[0].face==1 && materials.triangles[0].uv[0]==-3
            && materials.triangles[0].colour_base==32 && materials.texels==faces.texels);
        require(materials.triangles[1].even==21 && materials.triangles[1].odd==22
            && materials.triangles[1].dither==1);
        texture.texture_offset=1;
        require(!render::pack_ray_materials(faces,topology,materials));
        require(materials.triangles.size()==2 && materials.triangles[0].offset==0);
        texture.texture_offset=0;topology[0][0]=0;
        require(!render::pack_ray_materials(faces,topology,materials));
    }
    assets::Shape shape;shape.visibilities.resize(1);
    assets::Face face;face.visibility_index=-1;face.vertex_indices={0,1,2,3};shape.faces={face,face,face};
    shape.faces[1].visibility_index=0;shape.faces[2].visibility_index=99;
    shape.colour_words={0x3f6c};shape.colour_materials={{0x3f6c,{}}};
    render::RenderSettings settings;settings.colour_index_base=128;render::RenderPose pose;
    auto bsp=render::pack_bsp(shape);auto packed=render::pack_faces(shape,bsp,pose,settings);
    require(packed.polygon_only && packed.polygons.size()==3 && packed.corners.size()==12);
    require(packed.polygons[0][2]==1 && packed.polygons[1][2]==0 && packed.polygons[2][2]==1);
    require(packed.materials[0].even==140 && packed.materials[0].odd==134 && packed.materials[0].dither);
    settings.backface_culling=true;
    const auto culled=render::pack_faces(shape,bsp,pose,settings);
    for(const auto& polygon:culled.polygons)require((polygon[3]&8U)!=0);
    settings.backface_culling=false;
    for(auto progress:{1U,31U,255U}) {
        pose.explosion_progress=std::uint8_t(progress);
        const auto fragments=render::pack_faces(shape,bsp,pose,settings);
        for(const auto& polygon:fragments.polygons) require(polygon[2]==shape.visibilities.size());
        require(fragments.corners==packed.corners);
        require(fragments.materials[0].even==packed.materials[0].even
            && fragments.materials[0].odd==packed.materials[0].odd);
    }
    pose.explosion_progress=0;
    require(render::pack_faces(shape,bsp,pose,settings).polygons==packed.polygons);
    pose.palette_override=203;packed=render::pack_faces(shape,bsp,pose,settings);
    require(packed.materials[0].even==203 && packed.materials[0].odd==203 && !packed.materials[0].dither);
    pose.palette_override.reset();pose.force_colour=true;pose.forced_colour=0x91;
    packed=render::pack_faces(shape,bsp,pose,settings);require(packed.materials[0].even==129 && packed.materials[0].odd==137);
    pose.colour_warp=true;
    packed=render::pack_faces(shape,bsp,pose,settings);
    require(packed.materials[0].even==129 && packed.materials[0].odd==137);
    pose.force_colour=false;
    bool rejected_warp=false;
    try {(void)render::pack_faces(shape,bsp,pose,settings);}
    catch(const std::runtime_error&) {rejected_warp=true;}
    require(rejected_warp); // Active warp still needs the native sequence path.
    pose.colour_warp=false;
    pose.force_colour=false;pose.texture_scroll_x=-3;pose.texture_scroll_y=7;
    shape.colour_words[0]=0x4000;shape.colour_materials[0].raw=0x4000;
    assets::TextureImage texture;texture.descriptor=0x4000;texture.u_mask=texture.v_mask=7;
    texture.texels.resize(64);for(unsigned i=0;i<64;++i) texture.texels[i]=std::uint8_t(i%16);
    texture.coordinates={{{1,2},{3,4},{5,6},{7,0}}};shape.textures.push_back(texture);
    const auto warp_textures=render::pack_warp_textures(shape);
    require(warp_textures.lookup.size()==65536 && warp_textures.lookup[0x4000]==0 && warp_textures.lookup[0x4001]==UINT32_MAX);
    require(warp_textures.textures[0]==std::array<std::uint32_t,4>{0,7,7,0});
    require(warp_textures.coordinates[2]==std::array<std::uint32_t,2>{5,6} && warp_textures.texels==texture.texels);
    auto duplicate=texture;duplicate.texels.clear();shape.textures.push_back(duplicate);
    require(render::pack_warp_textures(shape).texels==texture.texels);shape.textures.pop_back();
    auto corrupt=shape;corrupt.textures[0].texels.pop_back();bool bad_texture=false;
    try{(void)render::pack_warp_textures(corrupt);}catch(const std::runtime_error&){bad_texture=true;}require(bad_texture);
    packed=render::pack_faces(shape,bsp,pose,settings);
    require(packed.texels.size()==64 && packed.materials[1].texture_offset==0 && packed.materials[2].texture_offset==0);
    require(packed.materials[0].tag==std::uint32_t(render::PixelLayer::textured_geometry) && packed.materials[0].colour_base==128);
    require(packed.materials[0].reserved0==std::uint32_t(-3) && packed.materials[0].reserved1==7);
    require(packed.corners[2]==std::array<std::uint32_t,4>{2,5,6,0} && packed.polygons[0][3]==1);
    shape.faces[1].vertex_indices={0,1};shape.faces[2].sprite=true;shape.faces[2].vertex_indices={0};
    bsp=render::pack_bsp(shape);packed=render::pack_faces(shape,bsp,pose,settings);
    require(!packed.polygon_only && packed.primitives[1]==render::PackedPrimitive::line && packed.primitives[2]==render::PackedPrimitive::sprite);
    require(!packed.materials[1].textured && packed.materials[1].reserved0==0 && packed.materials[1].reserved1==0);
    require(packed.materials[2].textured==1); // Sprites still sample artwork.
    {
        auto lines=shape;lines.faces={shape.faces[1]};lines.textures[0].texels.clear();
        const auto line_data=render::pack_faces(lines,render::pack_bsp(lines),pose,settings);
        require(line_data.texels.empty() && !line_data.materials[0].textured);
        for(const auto& corner:line_data.corners) require(corner[1]==0 && corner[2]==0);
    }
    settings.backface_culling=true;
    const auto mixed_culling=render::pack_faces(shape,bsp,pose,settings);
    require((mixed_culling.polygons[0][3]&8U)!=0);
    require((mixed_culling.polygons[1][3]&8U)==0);
    require((mixed_culling.polygons[2][3]&8U)==0);
    settings.backface_culling=false;
    require(packed.polygons[1][1]==2 && (packed.polygons[1][3]&2)!=0 && packed.polygons[2][1]==1
        && packed.polygons[2][3]==5 && packed.materials[2].tag==std::uint32_t(render::PixelLayer::two_d));
    for(const auto mode:{1U,3U}) {
        pose.wobble_mode=mode;
        const auto bypass=render::pack_faces(shape,bsp,pose,settings);
        require(bypass.polygons==packed.polygons && bypass.corners==packed.corners);
        require(bypass.materials[0].textured==1 && bypass.materials[0].reserved1==7);
        require(bypass.materials[1].reserved1==packed.materials[1].reserved1 && bypass.materials[2].reserved1==7);
    }
    pose.wobble_mode=0;
    shape.faces[2].vertex_indices={0,1,2};bsp=render::pack_bsp(shape);
    packed=render::pack_faces(shape,bsp,pose,settings);require(packed.polygons[2][1]==0);
    pose.wobble_mode=2;packed=render::pack_faces(shape,bsp,pose,settings);
    require(packed.materials[0].textured==1 && packed.materials[0].reserved1==7);
    shape.colour_words[0]=0x3f6c;shape.colour_materials[0].raw=0x3f6c;
    packed=render::pack_faces(shape,bsp,pose,settings);
    require((packed.materials[0].reserved1&65536U)!=0 && packed.materials[1].reserved1==0);
    pose.wobble_mode=1;
    packed=render::pack_faces(shape,bsp,pose,settings);
    require((packed.materials[0].reserved1&262144U)!=0 && packed.materials[1].reserved1==0);
    pose.wobble_mode=0;
    pose.wave_mode=true;pose.wave_offset=65530;pose.animation_frame=19;
    packed=render::pack_faces(shape,bsp,pose,settings);
    require((packed.materials[0].reserved1&131072U)!=0);
    require(packed.materials[0].du==-6 && packed.materials[0].dv==3);
    require((packed.materials[1].reserved1&131072U)==0);
    {
        auto combined=pose;combined.wobble_mode=1;
        const auto data=render::pack_faces(shape,bsp,combined,settings);
        require((data.materials[0].reserved1&(131072U|262144U))==(131072U|262144U));
        require(data.materials[0].du==-6 && data.materials[0].dv==3);
    }
    for(unsigned override_mode=0;override_mode<3;++override_mode) {
        auto overridden=pose;
        overridden.cel_mode=override_mode==0;
        overridden.wireframe_mode=override_mode==1?1:0;
        overridden.wobble_mode=override_mode==2?2:0;
        const auto data=render::pack_faces(shape,bsp,overridden,settings);
        require((data.materials[0].reserved1&131072U)==0);
    }
    shape.colour_words[0]=0x4000;shape.colour_materials[0].raw=0x4000;
    const auto textured_wave=render::pack_faces(shape,bsp,pose,settings);
    require(textured_wave.materials[0].textured==1 && textured_wave.materials[0].reserved1==7);
    pose.colour_warp=true;
    const auto warp_template=render::pack_warp_faces(shape,bsp,pose,settings);
    require(warp_template.texels.empty() && !warp_template.materials[0].textured);
    require((warp_template.materials[0].reserved1&131072U)!=0);
    require(warp_template.materials[0].du==-6 && warp_template.materials[0].dv==3);
    require((warp_template.polygons[0][3]&1U)==0);
    for(const auto& corner:warp_template.corners) require(corner[1]==0 && corner[2]==0);
    auto invalid_source=shape;invalid_source.textures[0].texels.clear();
    require(render::pack_warp_faces(invalid_source,bsp,pose,settings).texels.empty());
    unsigned models=0,polygons=0,lines=0,sprites=0;
    const bool list_sprites=argc==4 && std::string_view(argv[3])=="--list-sprites";
    if(argc==3 || list_sprites) {
        const auto rom=assets::RomImage::load(argv[1]);const auto symbols=assets::SymbolMap::load(argv[2]);const assets::ShapeDecoder decoder(rom,symbols);
        if(const auto doors=symbols.find("HALF_D");!doors.empty()) {
            // SHAPES4's door keeps four fixed vertices before its ten-frame
            // jump table. A missing prefix or wrong relative jump makes the
            // final tunnel's open panel cover the wrong part of the view.
            const auto door=decoder.decode(doors.front(),"HALF_D");
            require(door.declared_frame_count==10 && door.frames.size()==10);
            for(const auto& frame:door.frames) {
                require(frame.vertices.size()==16);
                require(frame.vertices[0]==assets::Vec3i{30,30,-10});
                require(frame.vertices[3]==assets::Vec3i{30,-30,-10});
            }
            require(door.frames[0].vertices[4]==assets::Vec3i{20,30,-10});
            require(door.frames[9].vertices[4]==assets::Vec3i{-25,30,-10});
            require(door.frames[9].vertices[15]==assets::Vec3i{-30,-30,10});
        }
        std::set<std::uint32_t> seen;
        for(const auto& [name,addresses]:symbols.entries()) for(auto address:addresses)
            if(seen.insert(address).second && decoder.looks_like_shape_header(address)) {
                const auto model=decoder.decode(address,name);const auto graph=render::pack_bsp(model);
                const auto data=render::pack_faces(model,graph,{},{});++models;
                if(list_sprites) for(size_t face=0;face<data.primitives.size();++face)
                    if(data.primitives[face]==render::PackedPrimitive::sprite) {
                        const auto& material=data.materials[face];
                        std::cout<<"Sprite model "<<name<<" address "<<address<<" face "<<face
                            <<" corners "<<data.polygons[face][1]<<" textured "<<material.textured
                            <<" size "<<material.u_mask+1<<'x'<<material.v_mask+1<<'\n';
                    }
                render::RenderPose warp_pose;warp_pose.colour_warp=true;
                const auto templates=render::pack_warp_faces(model,graph,warp_pose,{});
                require(templates.polygons.size()==data.polygons.size() && templates.texels.empty());
                require(templates.primitives==data.primitives && templates.corners.size()==data.corners.size());
                for(std::size_t f=0;f<data.polygons.size();++f) {
                    auto expected=data.polygons[f];expected[3]&=~1U;
                    require(templates.polygons[f]==expected && !templates.materials[f].textured);
                }
                for(std::size_t c=0;c<data.corners.size();++c)
                    require(templates.corners[c]==std::array<std::uint32_t,4>{data.corners[c][0],0,0,0});
                const auto warp=render::pack_warp_textures(model);
                for(const auto& texture:model.textures) require(warp.lookup[texture.descriptor]<warp.textures.size());
                require(data.polygons.size()==graph.faces.size() && data.materials.size()==graph.faces.size());
                for(auto primitive:data.primitives) {polygons+=primitive==render::PackedPrimitive::polygon;lines+=primitive==render::PackedPrimitive::line;sprites+=primitive==render::PackedPrimitive::sprite;}
            }
    }else require(argc==1);
    std::cout<<"Face packaging passed: "<<models<<" models, "<<polygons<<" polygons, "<<lines<<" lines, "<<sprites<<" sprites\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
