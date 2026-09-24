#include "starfox/vr/source_models.hpp"
#include <stdexcept>
#include <bit>
#include <cmath>
#include "starfox/render/grid_projection.hpp"
#include "starfox/render/grid_line_sample.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/vr/vulkan_connected_grid.hpp"
namespace starfox::vr {
SourceModelPackets SourceModels::assemble_world_interpolated(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& current,double alpha,bool srgb,bool surround_stars) {
    auto result=assemble_interpolated(previous,current,alpha,srgb,256,surround_stars);
    auto dust=assemble_dust_interpolated(previous,current,alpha,srgb,256,true);
    const bool controls=current.flow==simulation::GameFlowState::controls_type
        || current.flow==simulation::GameFlowState::controls_choice;
    if(surround_stars && !controls && dust.geometry.shared_vertices) {
        if(!surround_vertices_ || surround_vertices_->size()!=dust.geometry.shared_vertices->size()) {
            auto vertices=std::make_shared<std::vector<SceneVertex>>(*dust.geometry.shared_vertices);
            for(size_t i=0;i<vertices->size();++i) {
                auto& vertex=(*vertices)[i];vertex.texture[3]|=2048U;
                // Native recycling deliberately fills the forward camera cone.
                // VR uses stable world-space points in a periodic surrounding
                // volume; all 12 billboard vertices share their point position.
                uint32_t random=uint32_t(i/12)+0x9e3779b9U;
                for(unsigned axis=0;axis<3;++axis) {
                    random^=random>>16;random*=0x7feb352dU;
                    random^=random>>15;random*=0x846ca68bU;random^=random>>16;
                    vertex.position[axis]=float(int32_t(random&4095U)-2048);
                }
            }
            surround_vertices_=std::move(vertices);
        }
        dust.geometry.shared_vertices=surround_vertices_;
    }
    // Grid behavior belongs to the cartridge, independently of the VR star
    // surround. Preserve connected-line mode and the native camera transform;
    // do not substitute dots or the background's tilt-only transform in VR.
    auto grid=current.grid_lines?assemble_connected_grid_interpolated(previous,current,alpha,srgb)
        :assemble_grid_interpolated(previous,current,alpha,srgb,256,true,false);
    result.packets.insert(result.packets.begin(),std::move(dust));
    result.handles.insert(result.handles.begin(),0x20000U);
    result.packets.insert(result.packets.begin(),std::move(grid));
    result.handles.insert(result.handles.begin(),0x30000U);
    for(auto& model:result.compute_models) model.packet_index+=2; // Grid and dust prefixes.
    return result;
}
namespace {
bool text_packet(const assets::RomImage& rom,uint32_t font,uint32_t messages,
    const simulation::GameObject& object,const render::RenderPose& pose,
    std::span<const render::Rgba8> palette,bool srgb,float units,DrawPacket& packet,std::string& error) {
    if(pose.effect_clip_right>pose.effect_clip_left) {error="Scaled text eye-space mask pending";return false;}
    render::RenderPose placement;placement.x=pose.x;placement.y=pose.y;placement.z=pose.z;
    const auto model=game_model_matrix(placement,units);
    if(!model) {error="Invalid scaled text transform";return false;}
    packet.model=*model;
    if(object.colour_table<0x8000) return true;
    const int size=127+std::bit_cast<int8_t>(object.texture_scroll_x);
    std::vector<uint8_t> tokens;
    for(uint32_t i=0;i<256;++i) {
        const auto token=rom.read8((messages&0xff0000U)+object.colour_table+i);
        if(!token) break;tokens.push_back(token);
    }
    const auto index=uint8_t(112+object.extended[21]);
    if(index>=palette.size()) {error="Invalid scaled text palette";return false;}
    const auto colour=palette[index];
    const uint32_t ink=uint32_t(colour.r)|(uint32_t(colour.g)<<8)|(uint32_t(colour.b)<<16)|0xff000000U;
    std::array<uint32_t,42> offsets;offsets.fill(UINT32_MAX);
    for(size_t i=0;i<tokens.size();++i) {
        const auto token=tokens[i];if(token>41) continue;
        auto& offset=offsets[token];
        if(offset==UINT32_MAX) {
            offset=uint32_t(packet.geometry.texels.size());
            // Preserve source 1bpp rows; the fragment shader selects ink.
            // Nine words replace 256 expanded RGBA texels per unique glyph.
            packet.geometry.texels.push_back(ink);
            for(unsigned y=0;y<16;y+=2)
                packet.geometry.texels.push_back(uint32_t(rom.read16(font+uint32_t(token-1)*32+y*2))
                    |(uint32_t(rom.read16(font+uint32_t(token-1)*32+(y+1)*2))<<16));
        }
        const float left=float(i)-float(tokens.size())*.5F;
        const float corners[4][2]{{0,0},{1,0},{1,1},{0,1}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            SceneVertex v{};v.texture[0]=offset;v.texture[1]=v.texture[2]=15;v.texture[3]=134217728U|1028U|(srgb?2U:0U);
            v.group_a[0]=float(size);v.group_a[1]=float(pose.z);v.group_b[0]=left;
            v.billboard[0]=corners[corner][0];v.billboard[1]=.5F-corners[corner][1];
            v.uv[0]=corners[corner][0]*16;v.uv[1]=corners[corner][1]*16;
            packet.geometry.vertices.push_back(v);
        }
    }
    return true;
}
bool particle_packet(const GameSceneSnapshot& scene,simulation::ObjectHandle owner,
    const render::RenderPose& pose,std::span<const render::Rgba8> palette,bool srgb,float units,
    double alpha,DrawPacket& packet,std::string& error) {
    if(pose.effect_clip_right>pose.effect_clip_left) {error="Particle eye-space effect clipping pending";return false;}
    if(!std::isfinite(alpha)) {error="Invalid particle interpolation";return false;}
    alpha=std::clamp(alpha,0.,1.);
    render::RenderPose placement;placement.x=pose.x;placement.y=pose.y;placement.z=pose.z;
    const auto model=game_model_matrix(placement,units);
    if(!model) {error="Invalid particle owner transform";return false;}
    packet.model=*model;
    for(const auto& p:scene.particles) {
        if(!p.life || p.owner!=owner) continue;
        SceneVertex v{};
        v.group_a[0]=p.previous_x;v.group_a[1]=p.previous_y;v.group_a[2]=p.previous_z;
        v.group_b[0]=p.x;v.group_b[1]=p.y;v.group_b[2]=p.z;
        // Raw source endpoints; signed-word interpolation, depth rejection
        // and angular dot sizing happen once per GPU vertex, in both eyes.
        v.group_c[0]=float(alpha);v.group_c[1]=float(pose.z);
        render::FaceMaterial material{{p.colour,p.colour,false},nullptr};
        if(!apply_scene_material(v,material,palette,112,1,srgb)) {error="Invalid particle palette";return false;}
        v.texture[3]|=0x80000000U;
        if(p.flags&4U) {
            v.group_c[2]=2; // Trail: reject both ends if either depth is invalid.
            auto previous=v;previous.group_c[2]=1;
            packet.geometry.line_vertices.push_back(previous);packet.geometry.line_vertices.push_back(v);
        } else {
            // A source dot is two pixels wide at focal length 256. Keep its
            // centre in 3D and let each eye project an untextured billboard.
            const float corners[4][2]{{0,0},{1,0},{1,-1},{0,-1}};
            v.texture[3]|=4;
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                auto point=v;point.billboard[0]=corners[corner][0];point.billboard[1]=corners[corner][1];
                packet.geometry.vertices.push_back(point);
            }
        }
    }
    return true;
}
}
SourceModels::SourceModels(const assets::RomImage& rom,const assets::SymbolMap& symbols,bool cache_geometry,bool compute_solids,bool compute_shadows)
    :cache_geometry_(cache_geometry),compute_solids_(compute_solids),compute_shadows_(compute_shadows),rom_(&rom),decoder_(rom,symbols) {
    const auto symbol=[&](const char* name) {const auto& values=symbols.find(name);return values.empty()?uint32_t{}:values.front();};
    const auto rom_symbol=[&](const char* name) {
        for(auto address:symbols.find(name)) if((address&0xffffU)>=0x8000U && (address>>16U)<0x70U) return address;
        throw std::runtime_error(std::string("Missing scaled text symbol: ")+name);
    };
    scaled_font_=rom_symbol("MSCALECHARS");scaled_messages_=rom_symbol("MARIOMSGS");
    star_colours_=rom_symbol("STAR_COLS");
    interpolation_.trail=symbol("TRAIL_ISTRAT");if(interpolation_.trail) interpolation_.trail+=0x19U;
    interpolation_.flash_player=symbol("FLASHPLAYER_STRAT");
    interpolation_.discrete_rotation_shape=static_cast<uint16_t>(symbol("UP_DOOR"));
    intro_laser_shape_=static_cast<uint16_t>(symbol("RELFASTELASER"));
    intro_showcase_strategies_={symbol("ZACOINTRO_ISTRAT"),symbol("ZACO2INTRO_ISTRAT"),
        symbol("ZACOINTRO_STRAT"),symbol("ZACO2INTRO_STRAT")};
    if(symbol("M_NANMODE")) interpolation_.crosshair=symbol("TEST_ISTRAT");
    constexpr std::array names{"ID_1_C","RED_C","WHITE_C"};
    for(size_t i=0;i<names.size();++i) {
        for(auto address:symbols.find(names[i])) if((address&0xffffU)>=0x8000U && (address>>16U)<0x70U) {
            colours_[i]=static_cast<uint16_t>(address);break;
        }
        if(!colours_[i]) throw std::runtime_error(std::string("Missing model colour table: ")+names[i]);
    }
}
DrawPacket SourceModels::assemble_connected_grid(const GameSceneSnapshot& scene,bool srgb) const {
    DrawPacket packet;
    packet.model=source_layer_matrix(float(scene.source_vanishing_point[0])+16.F,
        float(scene.source_vanishing_point[1])+16.F).value();
    if(!scene.grid_lines || scene.dots_mode<=0 || scene.flow==simulation::GameFlowState::planet_select
        || scene.flow==simulation::GameFlowState::planet_travel || scene.flow==simulation::GameFlowState::continue_choice)
        return packet;
    auto words=scene.cgram;std::copy(scene.model_palette.begin(),scene.model_palette.end(),words.begin()+112);
    const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(words),scene.display_brightness);
    SceneVertex colour{};render::FaceMaterial material{{14,14,false},nullptr};
    if(!apply_scene_material(colour,material,palette,112,1,srgb)) throw std::runtime_error("Invalid connected grid palette");
    const auto rectangle=[&](int left,int top,int right,int bottom,uint32_t flags,uint32_t offset) {
        left=std::max(left,0);top=std::max(top,0);right=std::min(right,224);bottom=std::min(bottom,192);
        if(left>=right || top>=bottom) return;
        const int corners[4][2]{{left,top},{right,top},{right,bottom},{left,bottom}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
            auto vertex=colour;
            vertex.position[0]=float(corners[corner][0]+16);vertex.position[1]=float(corners[corner][1]+16);
            vertex.uv[0]=float(corners[corner][0]);vertex.uv[1]=float(corners[corner][1]);
            vertex.texture[0]=offset;vertex.texture[3]=flags;
            packet.geometry.vertices.push_back(vertex);
        }
    };
    const auto projected=render::project_source_grid(timing::interpolate(scene.camera,scene.camera,1.),scene.view_matrix,224,192);
    auto previous=scene.grid_line_start;
    for(size_t i=0;i<projected.count;++i) {
        const auto& point=projected.points[i];
        const int x=point.x-1,y=point.y,dx=x-previous[0];
        rectangle(x,y+2,x+1,y+3,0,0);
        const auto offset=uint32_t(packet.geometry.texels.size());
        packet.geometry.texels.insert(packet.geometry.texels.end(),{uint32_t(x),uint32_t(y),uint32_t(int32_t(previous[0])),uint32_t(int32_t(previous[1]))});
        rectangle(x-2-std::max(dx,0),std::min(y,int(previous[1])),x-1,std::max(y,int(previous[1]))+1,256,offset);
        previous={int16_t(x),int16_t(y)};
        if(point.depth<512) rectangle(x-1,y+1,x,y+2,0,0);
    }
    return packet;
}

DrawPacket SourceModels::assemble_connected_grid_binned(const GameSceneSnapshot& scene,bool srgb) const {
    return connected_grid_pose(scene,timing::interpolate(scene.camera,scene.camera,1.),scene.view_matrix,srgb);
}
DrawPacket SourceModels::assemble_connected_grid_gpu(const GameSceneSnapshot& scene,bool srgb) const {
    return connected_grid_pose(scene,timing::interpolate(scene.camera,scene.camera,1.),scene.view_matrix,srgb,true);
}
DrawPacket SourceModels::assemble_connected_grid_interpolated(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& scene,double alpha,bool srgb) const {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid connected grid interpolation fraction");
    alpha=std::clamp(alpha,0.,1.);
    if(previous.flow!=scene.flow || previous.dots_mode!=scene.dots_mode || previous.grid_lines!=scene.grid_lines
        || timing::camera_transform_is_discontinuous(previous.camera,scene.camera)) alpha=1.;
    // Current source frame owns the start endpoint for all presentations/eyes.
    return connected_grid_pose(scene,timing::interpolate(previous.camera,scene.camera,alpha),
        simulation::interpolate_rotation_matrix_q15(previous.view_matrix,scene.view_matrix,alpha),srgb,true);
}
DrawPacket SourceModels::connected_grid_pose(const GameSceneSnapshot& scene,const timing::RenderTransform& camera,
    const simulation::MatrixQ15& matrix,bool srgb,bool gpu) const {
    DrawPacket packet;
    packet.model=source_layer_matrix(float(scene.source_vanishing_point[0])+16.F,
        float(scene.source_vanishing_point[1])+16.F).value();
    if(!scene.grid_lines || scene.dots_mode<=0 || scene.flow==simulation::GameFlowState::planet_select
        || scene.flow==simulation::GameFlowState::planet_travel || scene.flow==simulation::GameFlowState::continue_choice)
        return packet;
    auto words=scene.cgram;std::copy(scene.model_palette.begin(),scene.model_palette.end(),words.begin()+112);
    const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(words),scene.display_brightness);
    SceneVertex colour{};render::FaceMaterial material{{14,14,false},nullptr};
    if(!apply_scene_material(colour,material,palette,112,1,srgb)) throw std::runtime_error("Invalid connected grid palette");
    if(gpu) {
        for(double value:{camera.x,camera.y,camera.z})
            packet.geometry.texels.push_back(uint32_t(int32_t(simulation::wrap16(int64_t(std::trunc(value))))));
        for(auto value:matrix) packet.geometry.texels.push_back(uint32_t(int32_t(value)));
        for(auto value:scene.grid_line_start) packet.geometry.texels.push_back(uint32_t(int32_t(value)));
        const unsigned corners[6][2]{{0,0},{224,0},{224,192},{0,0},{224,192},{0,192}};
        for(const auto& corner:corners) {
            auto v=colour;v.position[0]=float(corner[0]+16);v.position[1]=float(corner[1]+16);
            v.uv[0]=float(corner[0]);v.uv[1]=float(corner[1]);v.texture[3]=gpu_connected_grid_flag;
            packet.geometry.vertices.push_back(v);
        }
        return packet;
    }
    std::array<std::vector<uint32_t>,192> rows;
    std::vector<uint32_t> payload(384);
    const auto marker=[&](int x,int y) {
        if(x<0 || x>=224 || y<0 || y>=192) return;
        const auto record=uint32_t(payload.size());
        payload.insert(payload.end(),{1U,uint32_t(x),uint32_t(y),0,0});
        rows[size_t(y)].push_back(record);
    };
    const auto projected=render::project_source_grid(camera,matrix,224,192);
    auto previous=scene.grid_line_start;
    for(size_t i=0;i<projected.count;++i) {
        const auto& point=projected.points[i];
        const std::array<int16_t,2> current{int16_t(point.x-1),point.y};
        const int x=current[0],y=current[1],dx=x-previous[0];
        marker(x,y+2);
        // Only endpoint samples are evaluated on CPU, to bound the monotone
        // line after horizontal clipping. Pixel coverage remains in the shader.
        const int first=std::max(0,x-2-223),last=std::min(std::max(dx,0),x-2);
        if(first<=last) {
            const auto a=render::grid_line_sample(current,previous,uint32_t(first)).value();
            const auto b=render::grid_line_sample(current,previous,uint32_t(last)).value();
            const int top=std::max(0,std::min(a[1],b[1])),bottom=std::min(191,std::max(a[1],b[1]));
            if(top<=bottom) {
                const auto record=uint32_t(payload.size());
                payload.insert(payload.end(),{0U,uint32_t(x),uint32_t(y),uint32_t(int32_t(previous[0])),uint32_t(int32_t(previous[1]))});
                for(int row=top;row<=bottom;++row) rows[size_t(row)].push_back(record);
            }
        }
        previous=current;
        if(point.depth<512) marker(x-1,y+1);
    }
    for(size_t y=0;y<rows.size();++y) {
        payload[y*2]=uint32_t(payload.size());payload[y*2+1]=uint32_t(rows[y].size());
        payload.insert(payload.end(),rows[y].begin(),rows[y].end());
    }
    packet.geometry={};packet.geometry.texels=std::move(payload);
    const unsigned corners[6][2]{{0,0},{224,0},{224,192},{0,0},{224,192},{0,192}};
    for(const auto& corner:corners) {
        auto v=colour;v.position[0]=float(corner[0]+16);v.position[1]=float(corner[1]+16);
        v.uv[0]=float(corner[0]);v.uv[1]=float(corner[1]);
        v.texture[0]=v.texture[1]=v.texture[2]=0;v.texture[3]=512;
        packet.geometry.vertices.push_back(v);
    }
    return packet;
}

DrawPacket SourceModels::assemble_grid(const GameSceneSnapshot& scene,bool srgb,float units) const {
    return grid_pose(scene,timing::interpolate(scene.camera,scene.camera,1.),scene.view_matrix,srgb,units);
}
DrawPacket SourceModels::assemble_grid_gpu(const GameSceneSnapshot& scene,bool srgb,float units) const {
    return grid_pose(scene,timing::interpolate(scene.camera,scene.camera,1.),scene.view_matrix,srgb,units,true);
}
DrawPacket SourceModels::assemble_grid_interpolated(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& scene,double alpha,bool srgb,float units,bool gpu,bool fixed_landscape_height) const {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid grid interpolation fraction");
    alpha=std::clamp(alpha,0.,1.);
    if(previous.flow!=scene.flow || previous.dots_mode!=scene.dots_mode || previous.grid_lines!=scene.grid_lines
        || timing::camera_transform_is_discontinuous(previous.camera,scene.camera)) alpha=1.;
    auto camera=timing::interpolate(previous.camera,scene.camera,alpha);
    auto view=simulation::interpolate_rotation_matrix_q15(previous.view_matrix,scene.view_matrix,alpha);
    if(fixed_landscape_height && scene.background_landscape) {
        view=landscape_scene_view(previous,scene,alpha);
    }
    return grid_pose(scene,camera,view,srgb,units,gpu);
}
DrawPacket SourceModels::grid_pose(const GameSceneSnapshot& scene,const timing::RenderTransform& camera,
    const simulation::MatrixQ15& m,bool srgb,float units,bool gpu) const {
    DrawPacket packet;
    render::RenderPose origin;origin.x=origin.y=origin.z=0;
    const auto model=game_model_matrix(origin,units);
    if(!model) throw std::invalid_argument("Invalid grid scene units");
    packet.model=*model;
    if(scene.dots_mode<=0 || scene.flow==simulation::GameFlowState::planet_select
        || scene.flow==simulation::GameFlowState::planet_travel
        || scene.flow==simulation::GameFlowState::continue_choice) return packet;
    if(scene.grid_lines && !gpu) throw std::runtime_error("Connected grids require their dedicated source or GPU world pass");
    const auto start=[](int16_t camera) {
        return simulation::wrap16(int32_t((uint16_t(camera)&255U)^255U)-1920);
    };
    // Match the source grid's word quantization and wrapped lattice phase;
    // interpolate camera/view, never connect recycled lattice indices.
    const auto word=[](double value) {return simulation::wrap16(int64_t(std::trunc(value)));};
    const std::array<int16_t,3> source_start{start(word(camera.x)),
        simulation::wrap16(-int64_t(word(camera.y))),start(word(camera.z))};
    auto words=scene.cgram;std::copy(scene.model_palette.begin(),scene.model_palette.end(),words.begin()+112);
    const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(words),scene.display_brightness);
    if(gpu) {
        for(auto value:source_start) packet.geometry.texels.push_back(uint32_t(int32_t(value)));
        for(auto value:m) packet.geometry.texels.push_back(uint32_t(int32_t(value)));
        SceneVertex base{};
        render::FaceMaterial material{{14,14,false},nullptr};
        if(!apply_scene_material(base,material,palette,112,1,srgb)) throw std::runtime_error("Invalid grid palette");
        base.texture[3]=64|4;
        if(grid_vertices_ && grid_material_==base) {
            packet.geometry.shared_vertices=grid_vertices_;
            return packet;
        }
        packet.geometry.vertices.reserve(225*12);
        for(unsigned z=0;z<15;++z) for(unsigned x=0;x<15;++x) for(unsigned dot=0;dot<2;++dot) {
            constexpr float corners[4][2]{{0,0},{1,0},{1,-1},{0,-1}};
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                auto vertex=base;vertex.position[0]=float(x);vertex.position[2]=float(z);
                vertex.texture[1]=dot;
                vertex.billboard[0]=corners[corner][0]-float(dot);
                vertex.billboard[1]=corners[corner][1]-float(dot);
                packet.geometry.vertices.push_back(vertex);
            }
        }
        grid_vertices_=std::make_shared<const std::vector<SceneVertex>>(std::move(packet.geometry.vertices));
        grid_material_=base;packet.geometry.shared_vertices=grid_vertices_;
        return packet;
    }
    auto row=simulation::transform_q15(m,source_start);
    for(unsigned z=0;z<15;++z) {
        auto point=row;
        for(unsigned x=0;x<15;++x) {
            if(point[2]>256) {
                SceneVertex v{};v.position[0]=point[0];v.position[1]=point[1];
                v.position[2]=std::min<int>(point[2],12287);
                render::FaceMaterial material{{14,14,false},nullptr};
                if(!apply_scene_material(v,material,palette,112,1,srgb)) throw std::runtime_error("Invalid grid palette");
                v.texture[3]=4;const float size=v.position[2]/256.F;
                for(unsigned dot=0;dot<(point[2]<512?2U:1U);++dot) {
                    constexpr float corners[4][2]{{0,0},{1,0},{1,-1},{0,-1}};
                    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                        auto vertex=v;vertex.billboard[0]=(corners[corner][0]-float(dot))*size;
                        vertex.billboard[1]=(corners[corner][1]-float(dot))*size;
                        packet.geometry.vertices.push_back(vertex);
                    }
                }
            }
            for(unsigned axis=0;axis<3;++axis)
                point[axis]=simulation::add16(point[axis],simulation::wrap16(simulation::arithmetic_shift_right(m[axis],7)));
        }
        for(unsigned axis=0;axis<3;++axis)
            row[axis]=simulation::add16(row[axis],simulation::wrap16(simulation::arithmetic_shift_right(m[6+axis],7)));
    }
    return packet;
}
DrawPacket SourceModels::assemble_dust_gpu(const GameSceneSnapshot& scene,bool srgb,float units) const {
    return dust_pose(scene,timing::interpolate(scene.camera,scene.camera,1.),scene.view_matrix,srgb,units,true);
}
DrawPacket SourceModels::assemble_dust(const GameSceneSnapshot& scene,bool srgb,float units) const {
    return dust_pose(scene,timing::interpolate(scene.camera,scene.camera,1.),scene.view_matrix,srgb,units);
}
DrawPacket SourceModels::assemble_dust_interpolated(const GameSceneSnapshot& previous,
    const GameSceneSnapshot& scene,double alpha,bool srgb,float units,bool gpu) const {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid dust interpolation fraction");
    alpha=std::clamp(alpha,0.,1.);
    if(previous.flow!=scene.flow || previous.dots_mode!=scene.dots_mode
        || timing::camera_transform_is_discontinuous(previous.camera,scene.camera)) alpha=1.;
    // Recycled points retain their current source identity; interpolate only
    // camera motion, never draw a streak between unrelated recycled positions.
    return dust_pose(scene,timing::interpolate(previous.camera,scene.camera,alpha),
        simulation::interpolate_rotation_matrix_q15(previous.view_matrix,scene.view_matrix,alpha),srgb,units,gpu);
}
DrawPacket SourceModels::dust_pose(const GameSceneSnapshot& scene,const timing::RenderTransform& camera,
    const simulation::MatrixQ15& m,bool srgb,float units,bool gpu) const {
    DrawPacket packet;
    render::RenderPose origin;origin.x=origin.y=origin.z=0;
    const auto model=game_model_matrix(origin,units);
    if(!model || scene.dust_point_count>scene.dust_points.size())
        throw std::invalid_argument("Invalid dust scene units/count");
    packet.model=*model;
    if(scene.dots_mode>=0 || scene.flow==simulation::GameFlowState::planet_select
        || scene.flow==simulation::GameFlowState::planet_travel
        || scene.flow==simulation::GameFlowState::continue_choice) return packet;
    auto words=scene.cgram;std::copy(scene.model_palette.begin(),scene.model_palette.end(),words.begin()+112);
    const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(words),scene.display_brightness);
    if(gpu) {
        const bool controls=scene.flow==simulation::GameFlowState::controls_type
            || scene.flow==simulation::GameFlowState::controls_choice;
        if(controls) packet.model=source_layer_matrix(float(scene.source_vanishing_point[0])+16.F,
            float(scene.source_vanishing_point[1])+16.F).value();
        for(double value:{camera.x,camera.y,camera.z}) packet.geometry.texels.push_back(std::bit_cast<uint32_t>(float(value)));
        for(auto value:m) packet.geometry.texels.push_back(uint32_t(int32_t(value)));
        for(unsigned index=0;index<64;++index) {
            const auto shade=rom_->read8(star_colours_+index);
            SceneVertex colour{};render::FaceMaterial material{{shade,shade,false},nullptr};
            if(!apply_scene_material(colour,material,palette,112,1,srgb)) throw std::runtime_error("Invalid GPU dust colour");
            for(float value:colour.color) packet.geometry.texels.push_back(std::bit_cast<uint32_t>(value));
        }
        if(dust_vertices_ && !dust_vertices_->empty()
            && bool(dust_vertices_->front().texture[3]&16384U)==controls && dust_source_points_.size()==scene.dust_point_count
            && (controls || std::equal(dust_source_points_.begin(),dust_source_points_.end(),scene.dust_points.begin()))) {
            packet.geometry.shared_vertices=dust_vertices_;
            return packet;
        }
        packet.geometry.vertices.reserve(scene.dust_point_count*12);
        for(size_t index=0;index<scene.dust_point_count;++index) for(unsigned dot=0;dot<2;++dot) {
            const auto& point=scene.dust_points[index];
            constexpr float corners[4][2]{{0,0},{1,0},{1,-1},{0,-1}};
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                SceneVertex v{};v.position[0]=point.x;v.position[1]=point.y;v.position[2]=point.z;
                if(controls) {
                    uint32_t seed=uint32_t(index)+0x9e3779b9U;
                    const auto random=[&] {seed^=seed>>16;seed*=0x7feb352dU;seed^=seed>>15;seed*=0x846ca68bU;seed^=seed>>16;return seed;};
                    v.position[0]=float(int(random()%97)-48);v.position[1]=float(int(random()%73)-36);
                    v.position[2]=float(random()&4095U);
                }
                v.texture[3]=128|4|(controls?16384U:0U);v.texture[1]=uint32_t((scene.dust_point_count-index)&3);v.texture[2]=dot;
                v.billboard[0]=corners[corner][0]-float(dot);v.billboard[1]=corners[corner][1]-float(dot);
                packet.geometry.vertices.push_back(v);
            }
        }
        auto source_points=std::vector<simulation::DustPoint>(scene.dust_points.begin(),scene.dust_points.begin()+scene.dust_point_count);
        auto vertices=std::make_shared<const std::vector<SceneVertex>>(std::move(packet.geometry.vertices));
        dust_source_points_=std::move(source_points);dust_vertices_=std::move(vertices);
        packet.geometry.shared_vertices=dust_vertices_;
        return packet;
    }
    const auto delta=[](double value,double origin) {
        auto d=std::fmod(value-origin,65536.);
        if(d>32767.) d-=65536.;else if(d< -32768.) d+=65536.;
        return d;
    };
    for(size_t index=0;index<scene.dust_point_count;++index) {
        const auto& point=scene.dust_points[index];
        const double x=delta(point.x,camera.x);
        const double y=delta(point.y,camera.y);
        const double z=delta(point.z,camera.z);
        const double depth=(x*m[2]+y*m[5]+z*m[8])/32768.;
        if(depth<256) continue;
        const double clipped=std::min(depth,4095.);
        SceneVertex vertex{};
        vertex.position[0]=float((x*m[0]+y*m[3]+z*m[6])/32768.);
        vertex.position[1]=float((x*m[1]+y*m[4]+z*m[7])/32768.);
        vertex.position[2]=float(clipped);
        const auto shade=rom_->read8(star_colours_+uint32_t(((scene.dust_point_count-index)&3)*16+(int(clipped)>>8)));
        render::FaceMaterial material{{shade,shade,false},nullptr};
        if(!apply_scene_material(vertex,material,palette,112,1,srgb))
            throw std::runtime_error("Invalid source dust colour");
        vertex.texture[3]=4;
        const float size=float(clipped/256.);
        for(unsigned dot=0;dot<(depth<1024?2U:1U);++dot) {
            constexpr float corners[4][2]{{0,0},{1,0},{1,-1},{0,-1}};
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                auto v=vertex;v.billboard[0]=(corners[corner][0]-float(dot))*size;
                v.billboard[1]=(corners[corner][1]-float(dot))*size;
                packet.geometry.vertices.push_back(v);
            }
        }
    }
    return packet;
}
SourceModelPackets SourceModels::assemble(const GameSceneSnapshot& scene,bool srgb,float units) {
    const auto shadows=scene.shadows_enabled?interpolate_scene_poses(scene,scene,1.,interpolation_,true):std::vector<render::RenderPose>{};
    return assemble_poses(scene,{},shadows,srgb,units,1.);
}
SourceModelPackets SourceModels::assemble_interpolated(const GameSceneSnapshot& previous,const GameSceneSnapshot& scene,double alpha,bool srgb,float units,bool fixed_landscape_height) {
    if(!std::isfinite(alpha)) throw std::invalid_argument("Invalid scene interpolation fraction");
    if(previous.flow!=scene.flow || timing::camera_transform_is_discontinuous(previous.camera,scene.camera)) alpha=1.;
    auto rules=interpolation_;rules.fixed_landscape_height=fixed_landscape_height;
    auto poses=interpolate_scene_poses(previous,scene,alpha,rules);
    if(fixed_landscape_height && !scene.meters.extended && scene.flow==simulation::GameFlowState::title) {
        // Original's ship belongs behind its logo, independently of EX's
        // deliberately close model showcase. Preserve the viewing direction.
        for(auto& pose:poses) {pose.x*=2.;pose.y*=2.;pose.z*=2.;}
    }
    if(fixed_landscape_height && scene.meters.extended && scene.flow==simulation::GameFlowState::intro) {
        for(size_t i=0;i<poses.size();++i) {
            const auto strategy=scene.objects[i].object.strategy_address;
            if(strategy && std::find(intro_showcase_strategies_.begin(),intro_showcase_strategies_.end(),strategy)
                !=intro_showcase_strategies_.end()) {
                // Apply before either GPU generation or fallback assembly.
                // Scale the centre, not the geometry; preserve its viewing ray
                // and source lighting depth while enlarging the model.
                poses[i].x*=.375;poses[i].y*=.375;poses[i].z*=.375;
            }
        }
    }
    const auto shadows=scene.shadows_enabled?interpolate_scene_poses(previous,scene,alpha,rules,true):std::vector<render::RenderPose>{};
    return assemble_poses(scene,poses,shadows,srgb,units,alpha);
}
SourceModelPackets SourceModels::assemble_poses(const GameSceneSnapshot& scene,std::span<const render::RenderPose> poses,std::span<const render::RenderPose> shadows,bool srgb,float units,double alpha) {
    SourceModelPackets result;
    ++cache_epoch_;
    auto words=scene.cgram;
    std::copy(scene.model_palette.begin(),scene.model_palette.end(),words.begin()+112);
    // Models, dust and grid share the source display fade with the 2D layers.
    // Otherwise the next screen's ship/stars remain visible during forced black.
    const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(words),scene.display_brightness);
    std::optional<std::array<uint32_t,256>> graphics_palette;
    for(unsigned pass=0;pass<2;++pass) for(size_t i=0;i<scene.objects.size();++i) {
        const bool shadow=pass==0;
        const auto& item=scene.objects[i];
        const auto& object=item.object;
        if(interpolation_.crosshair && object.strategy_address==interpolation_.crosshair
            && scene.flow!=simulation::GameFlowState::gameplay && scene.flow!=simulation::GameFlowState::training) continue;
        const auto flags=object.strategy_flags[0];
        if(shadow && (!scene.shadows_enabled || !(flags&0x0cU))) continue;
        if(!shadow && (flags&4U)) continue;
        const auto defer=[&](std::string reason) {result.pending.push_back({item.handle,std::move(reason)});};
        if(flags&0x40U) {
            if(shadow) continue;
            DrawPacket packet;std::string error;
            try {
                if(!text_packet(*rom_,scaled_font_,scaled_messages_,object,poses.empty()?item.source_pose:poses[i],palette,srgb,units,packet,error)) {defer(error);continue;}
                result.handles.push_back(item.handle);result.packets.push_back(std::move(packet));
            } catch(const std::exception& error) {defer(error.what());}
            continue;
        }
        if(!shadow && (flags&0x10U)) {
            DrawPacket packet;std::string error;
            if(!particle_packet(scene,item.handle,poses.empty()?item.source_pose:poses[i],palette,srgb,units,alpha,packet,error)) {defer(error);continue;}
            result.handles.push_back(item.handle);result.packets.push_back(std::move(packet));continue;
        }
        if(!object.shape) continue;
        const bool crosshair=!shadow && interpolation_.crosshair
            && object.strategy_address==interpolation_.crosshair;
        uint16_t colour=object.colour_table;
        if(scene.colour_table_override && !crosshair) colour=*scene.colour_table_override;
        else if((flags&2U) && !(flags&0x20U)) colour=colours_[(flags&1U)?1:2];
        else if(flags&1U) colour=colours_[0];
        try {
            const uint64_t base_key=(uint64_t(object.shape)<<32U)|colour;
            auto base=shapes_.find(base_key);
            if(base==shapes_.end()) base=shapes_.emplace(base_key,decoder_.decode(object.shape,{},colour)).first;
            const auto header=base->second.header;
            const auto selected=shadow?header.shadow_pointer:assets::ShapeDecoder::select_lod_pointer(header,item.source_pose.source_depth);
            const uint64_t lod_key=base_key|(uint64_t(selected)<<16U);
            auto lod=shapes_.find(lod_key);
            if(lod==shapes_.end()) lod=shapes_.emplace(lod_key,decoder_.decode_lod(header,selected,colour)).first;
            DrawPacket packet;std::string error;
            auto pose=shadow?shadows[i]:poses.empty()?item.source_pose:poses[i];
            // EX's aiming marks use their dedicated HUD palette entry, not
            // the active model colour table or the scene's warp animation.
            if(crosshair) {
                pose.palette_override=128U+4U*16U+15U;
                pose.colour_warp=false;
                pose.wireframe_mode=0;pose.wobble_mode=0;
                pose.wave_mode=false;pose.cel_mode=false;pose.wave_offset=0;
            }
            // Match the desktop intro's near-camera beam presentation rule.
            pose.collapse_to_axis_line=scene.flow==simulation::GameFlowState::intro
                && intro_laser_shape_ && object.shape==intro_laser_shape_ && pose.z<1024.;
            if(pose.simple_scaled_sprite) {
                auto adjustment=static_cast<int16_t>(std::bit_cast<int8_t>(object.texture_scroll_x));
                for(unsigned shift=0;shift<header.shift;++shift) adjustment=simulation::add16(adjustment,adjustment);
                auto diameter=simulation::add16(header.size,adjustment);
                diameter=simulation::add16(diameter,diameter);
                pose.simple_sprite_world_size=diameter?diameter:1;
                pose.simple_sprite_colour=object.extended[21];
            }
            const uint32_t pass_key=uint32_t(item.handle)|(shadow?source_shadow_pass:0U);
            if(compute_solids_ && (!shadow || compute_shadows_) && !pose.simple_scaled_sprite
                && !lod->second.faces.empty()
                && pose.effect_clip_right<=pose.effect_clip_left) {
                SourceSpanModel prepared;
                auto continuous_pose=pose;continuous_pose.continuous_geometry=true;
                render::RenderSettings settings;settings.colour_index_base=112;
                const bool warp_requested=pose.colour_warp && !pose.force_colour;
                bool ordinary=!warp_requested && !pose.wireframe_mode && !pose.wobble_mode && !pose.wave_mode && !pose.cel_mode;
                auto base_pose=continuous_pose;base_pose.colour_warp=warp_requested;
                std::shared_ptr<SourceAxisInputs> axis;
                bool ready;
                if(pose.collapse_to_axis_line) {
                    axis=std::make_shared<SourceAxisInputs>();
                    ready=prepare_source_axis_model(lod->second,continuous_pose,settings,224,192,prepared,*axis,error);
                } else ready=prepare_source_span_model(lod->second,base_pose,settings,224,192,prepared,error,ordinary);
                std::shared_ptr<SourceWarpInputs> warp;
                if(ready && warp_requested) {
                    warp=std::make_shared<SourceWarpInputs>();
                    ready=prepare_source_warp_inputs(lod->second,continuous_pose,settings,prepared.projection,prepared.bsp,*warp,error,
                        prepared.fragmented?&prepared.faces:nullptr,prepared.source_vertex_count);
                    if(ready) ready=std::all_of(warp->templates.primitives.begin(),warp->templates.primitives.end(),
                        [](auto primitive){return primitive==render::PackedPrimitive::polygon
                            || primitive==render::PackedPrimitive::line || primitive==render::PackedPrimitive::sprite;});
                    if(ready) {
                        prepared.faces=warp->templates;prepared.warp_expanded=true;
                        prepared.graphics_unclipped=false;
                        prepared.spans.ordered_mode=2;prepared.spans.polygon_count=prepared.spans.count;
                        prepared.clip_settings[0]=prepared.spans.count;prepared.clip_settings[2]=prepared.spans.count*32U;
                    }
                }
                if(ready) ordinary=prepared.graphics_unclipped;
                if(ready
                    && !prepared.faces.primitives.empty()
                    && std::all_of(prepared.faces.primitives.begin(),prepared.faces.primitives.end(),
                        [](auto primitive){return primitive==render::PackedPrimitive::polygon
                            || primitive==render::PackedPrimitive::line
                            || primitive==render::PackedPrimitive::sprite;})) {
                    prepared.graphics_palette_flags=(srgb?3U:1U)|(warp?4U:0U);
                    prepared.graphics_unclipped=ordinary;
                    if(!graphics_palette) {
                        graphics_palette.emplace();
                        for(size_t colour_index=0;colour_index<256;++colour_index) {
                            const auto c=palette[colour_index];
                            (*graphics_palette)[colour_index]=uint32_t(c.r)|(uint32_t(c.g)<<8)
                                |(uint32_t(c.b)<<16)|(uint32_t(c.a)<<24);
                        }
                    }
                    prepared.graphics_palette=*graphics_palette;
                    result.compute_models.push_back({pass_key,result.packets.size(),units,std::move(prepared),std::move(warp),std::move(axis)});
                    packet.preserve_native_colour=crosshair || shadow;
                    result.handles.push_back(pass_key);result.packets.push_back(std::move(packet));
                    continue;
                }
                // Mixed/unsupported models keep their existing full renderer;
                // never drop texture, sprite or line faces to migrate solids.
                result.compute_fallbacks.push_back({pass_key,error.empty()
                    ?"empty or unsupported compute primitive set":error});
            }
            // Ordinary meshes depend on authored animation/material state, not
            // the interpolated transform. Preserve exact input equality (no hash
            // collisions); rebuild special source effects through their full path.
            const bool cacheable=cache_geometry_ && !pose.simple_scaled_sprite && !pose.explosion_progress
                && !pose.collapse_to_axis_line && !pose.colour_warp && !pose.wireframe_mode
                && !pose.wobble_mode && !pose.wave_mode && !pose.cel_mode
                && pose.effect_clip_right<=pose.effect_clip_left && std::isfinite(pose.scale)
                && pose.scale>0 && (!pose.use_source_lighting_state || std::isfinite(pose.source_depth));
            GeometryKey key;
            bool reused=false;
            if(cacheable) {
                const auto model=game_model_matrix(pose,units);
                if(!model) {defer("Invalid native model transform");continue;}
                packet.model=*model;packet.shading=render::source_shading(pose);
                key.shape=&lod->second;key.scale=pose.scale;
                key.animation=pose.animation_frame%std::max(std::size_t(1),lod->second.frames.size());
                const bool colour_animated=std::any_of(lod->second.colour_materials.begin(),lod->second.colour_materials.end(),
                    [](const auto& m){return !m.animation_frames.empty();});
                key.colour_frame=colour_animated?pose.colour_frame:0;
                key.scroll_x=pose.texture_scroll_x;key.scroll_y=pose.texture_scroll_y;
                key.depth=packet.shading.depth_band;key.light=packet.shading.light;
                key.depth_tables=pose.has_depth_colour_tables;
                if(key.depth_tables) key.depth_colours=pose.depth_colour_tables[std::min(key.depth,std::size_t(3))];
                key.palette=words;key.palette_override=pose.palette_override;
                key.brightness=scene.display_brightness;
                key.forced_colour=pose.forced_colour;key.force_colour=pose.force_colour;key.srgb=srgb;
                const auto cached=geometry_cache_.find(pass_key);
                if(cached!=geometry_cache_.end() && cached->second.key==key) {
                    packet.geometry=cached->second.geometry;cached->second.used=cache_epoch_;reused=true;
                }
            }
            if(!reused && !build_draw_packet(lod->second,pose,palette,112,1,srgb,units,packet,error)) {
                defer("shape "+std::to_string(object.shape)+" LOD "+std::to_string(selected)+": "+error);continue;
            }
            if(!reused && interpolation_.crosshair && object.strategy_address==interpolation_.crosshair) {
                // Reticle stations belong to the game's camera plane, not
                // each eye's head-rolled billboard basis. Preserve the quad
                // offsets in model space before the normal eye transform.
                for(auto& vertex:packet.geometry.vertices) if(vertex.texture[3]&4U) {
                    if(vertex.texture[3]&134217728U) {
                        // Apply orientation after GPU source-size truncation.
                        vertex.group_b[2]=1;
                        continue;
                    }
                    vertex.position[0]+=vertex.billboard[0];
                    vertex.position[1]+=vertex.billboard[1];
                    vertex.billboard[0]=vertex.billboard[1]=0;
                    vertex.texture[3]&=~4U;
                }
            }
            if(cacheable && !reused) {
                if(const auto old=geometry_cache_.find(pass_key);old!=geometry_cache_.end()) {
                    cached_vertices_-=old->second.geometry.vertex_view().size()+old->second.geometry.line_view().size();
                    cached_texels_-=old->second.geometry.texels.size();geometry_cache_.erase(old);
                }
                const auto vertex_count=packet.geometry.vertex_view().size()+packet.geometry.line_view().size();
                // Bound retained data (~14 MB plus bookkeeping), regardless of
                // model count. Exceeding this changes performance, never output.
                if(vertex_count+cached_vertices_<=65536 && packet.geometry.texels.size()+cached_texels_<=262144) {
                    if(!packet.geometry.vertices.empty()) packet.geometry.shared_vertices=
                        std::make_shared<const std::vector<SceneVertex>>(std::move(packet.geometry.vertices));
                    if(!packet.geometry.line_vertices.empty()) packet.geometry.shared_line_vertices=
                        std::make_shared<const std::vector<SceneVertex>>(std::move(packet.geometry.line_vertices));
                    geometry_cache_.insert_or_assign(pass_key,CachedGeometry{std::move(key),packet.geometry,cache_epoch_});
                    cached_vertices_+=vertex_count;cached_texels_+=packet.geometry.texels.size();
                }
            }
            packet.preserve_native_colour=crosshair || shadow;
            result.handles.push_back(pass_key);result.packets.push_back(std::move(packet));
        } catch(const std::exception& error) {defer(error.what());}
    }
    std::erase_if(geometry_cache_,[&](const auto& item) {
        if(item.second.used==cache_epoch_) return false;
        cached_vertices_-=item.second.geometry.vertex_view().size()+item.second.geometry.line_view().size();
        cached_texels_-=item.second.geometry.texels.size();return true;
    });
    return result;
}
}
