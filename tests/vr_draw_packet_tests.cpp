#include "starfox/vr/draw_packet.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include "starfox/vr/enhanced_landscape.hpp"
#include "starfox/vr/packed_vram.hpp"
#include "starfox/vr/source_span_model.hpp"
#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/source_sprites.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/vr/game_model_pose.hpp"
#include "starfox/vr/pause_sandbox.hpp"
#include "starfox/render/grid_line_history.hpp"
#include "starfox/render/grid_line_sample.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <source_location>
#include <chrono>
using namespace starfox;
namespace {void require(bool value,const std::source_location& where=std::source_location::current()) {
    if(!value) throw std::runtime_error("Native draw packet assertion failed at line "+std::to_string(where.line()));
}}
int main() try {
    {
        const auto settled=vr::photographic_scroll_correction(12,-8,1);
        const auto neutral=vr::photographic_body_motion({128,112});require(settled==neutral);
        require(vr::photographic_scroll_correction(-508,508,0)==vr::photographic_scroll_correction(4,-4,0));
        float previous=1;
        for(unsigned phase=0;phase<=8;++phase) {
            const auto correction=vr::photographic_scroll_correction(12,0,phase/8.);
            require(correction[2]>=0 && correction[2]<=previous);previous=correction[2];
        }
        bool rejected=false;
        try{(void)vr::photographic_scroll_correction(0,0,std::numeric_limits<double>::quiet_NaN());}
        catch(const std::invalid_argument&){rejected=true;}require(rejected);
    }
    {
        render::BackdropImage limb,surface;
        limb.width=surface.width=limb.height=surface.height=2;
        limb.pixels.assign(4,0xff123456U);surface.pixels.assign(4,0xffabcdefU);
        limb.seal_for_upload();surface.seal_for_upload();
        const auto texture=vr::make_orbital_texture(limb,surface);
        require(vr::backdrop_texture_valid(*texture,1024,1536));
        const auto base=(*texture)[4];
        require((*texture)[base+256*1024+512]==0xff123456U);
        require((*texture)[base+1024*1024+512]==0xffabcdefU);
        require(texture->size()<2'100'000);
        const auto pattern=vr::make_pattern_sky_texture(limb);
        require(vr::backdrop_texture_valid(*pattern,2048,1024));
        require((*pattern)[(*pattern)[4]+512*2048+1024]==0xff123456U);
        surface.pixels.clear();bool rejected=false;
        try{(void)vr::make_orbital_texture(limb,surface);}
        catch(const std::invalid_argument&){rejected=true;}require(rejected);
    }
    {
        render::BackdropImage image;image.width=image.height=4;image.pixels.assign(16,0xffabcdefU);
        const auto texture=vr::make_backdrop_texture(image);
        vr::PhotographicLandscape options;options.orbital_surface=true;
        bool rejected=false;try{(void)vr::photographic_landscape_packet(texture,options);}
        catch(const std::invalid_argument&){rejected=true;}require(rejected);
        options.full_sphere=true;const auto globe=vr::photographic_landscape_packet(texture,options);
        unsigned top=0,bottom=0;
        for(const auto& v:globe.geometry.vertex_view()) {require(v.odd_color[3]==4);top+=v.position[1]>0;bottom+=v.position[1]<0;}
        require(top && bottom);
        options.latitude_uv=true;rejected=false;try{(void)vr::photographic_landscape_packet(texture,options);}
        catch(const std::invalid_argument&){rejected=true;}require(rejected);
    }
    {
        simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
        ppu.bg2_character_base=0;ppu.bg2_screen_base=0x1000;
        ppu.vram[0x2000]=1;ppu.vram[0x2001]=12; // Tile 1, bank 3.
        for(unsigned y=0;y<8;++y) {ppu.vram[32+y*2+1]=128;ppu.vram[48+y*2]=128;}
        vr::BackgroundTileOptions options;options.scroll_override=std::array<int16_t,2>{0,0};
        std::array<bool,256> keep{};keep[54]=true;keep[0]=true;
        for(bool large:{false,true}) for(bool flip:{false,true}) {
            ppu.bg2_tile_size_16=large;ppu.vram[0x2001]=uint8_t(12|(flip?64:0));
            const unsigned width=large?16:8;
            const auto body=vr::landscape_landmark_packet(ppu,options,{0,0,width,8},keep);
            require(body.geometry.vertices.size()==48 && body.geometry.texels[3]==0 && body.geometry.texels[4]==0);
            for(const auto& v:body.geometry.vertices) require(v.uv[0]>=(flip?width-1:0)
                && v.uv[0]<=(flip?width:1) && v.uv[1]>=0 && v.uv[1]<=8);
            ppu.cgram[54]=32767;
            const auto recoloured=vr::landscape_landmark_packet(ppu,options,{0,0,width,8},keep);
            require(body.geometry.vertices==recoloured.geometry.vertices);
        }
        keep.fill(false);
        require(vr::landscape_landmark_packet(ppu,options,{0,0,8,8},keep).geometry.vertices.empty());
    }
    {
        render::BackdropImage master;master.width=master.height=2;master.pixels.assign(4,0xffffffffU);master.seal_for_upload();
        simulation::SnesPpuState ppu;ppu.bg2_character_base=0;ppu.bg2_screen_base=0x1000;ppu.bg2_screen_size=3;
        // A black (ink 15) limb is opaque, but the empty first tile is space.
        for(unsigned i=32;i<64;++i) ppu.vram[i]=255;
        for(unsigned y=40;y<44;++y) for(unsigned x=20;x<30;++x) {
            const unsigned entry=(x/32+y/32*2)*1024+(y%32)*32+x%32;
            ppu.vram[0x2000+entry*2]=(x==20 && y==40)?0:1;
        }
        render::CloudLimbAtlas atlas;const auto& image=atlas.image(master,master,ppu);
        const auto key=image.immutable_upload_key;
        const auto texture=vr::make_cloud_body_texture(image,atlas,true);
        require(vr::backdrop_texture_valid(*texture,320,128) && (*texture)[2]==0);
        const auto pixel=[&](unsigned x,unsigned y){return (*texture)[(*texture)[4]+y*320+x];};
        require((pixel(8,8)>>24)==0 && (pixel(80,80)>>24)==255 && (pixel(80,80)&0xffffffU)==0);
        ppu.cgram[95]=32767;require(atlas.image(master,master,ppu).immutable_upload_key==key);
        vr::PhotographicBody options;options.diameter={80,32};options.palette[0]=5;options.palette[15]=32767;
        const auto body=vr::photographic_body_packet(texture,options);
        require(body.geometry.vertex_view()[0].visibility_a[0]==5);
    }
    {
        render::BackdropImage image;image.width=image.height=2;
        image.pixels.assign(4,0xff123456U);
        const auto texture=vr::make_celestial_texture(image,{31,29,1216,1209});
        require((*texture)[2]==0 && vr::backdrop_texture_valid(*texture,512,512));
        require(((*texture)[(*texture)[4]]>>24)==0
            && (*texture)[(*texture)[4]+256*512+256]==0xff123456U);
        vr::PhotographicBody options;options.center={376,80};options.palette_shift={-.1F,.2F,0};
        const auto placed=vr::photographic_body_packet(texture,options);
        options.center={128,112};const auto centered=vr::photographic_body_packet(texture,options);
        const auto transform=vr::photographic_body_motion({376,80});
        for(unsigned i=0;i<6;++i) for(unsigned row=0;row<3;++row) {
            float actual=transform[12+row];for(unsigned c=0;c<3;++c)
                actual+=transform[c*4+row]*centered.geometry.vertex_view()[i].position[c];
            require(std::abs(actual-placed.geometry.vertex_view()[i].position[row])<.00001F);
        }
        require(placed.geometry.vertex_view()[0].odd_color[0]==-.1F);
        bool rejected=false;
        try {(void)vr::make_celestial_texture(image,{30,20,20,10});}catch(const std::invalid_argument&) {rejected=true;}
        require(rejected);
    }
    {
        render::BackdropImage master;master.width=master.height=4;master.pixels.assign(16,0xffffffffU);
        const auto solid=vr::make_moon_texture(master,false),haze=vr::make_moon_texture(master,true);
        require((*solid)[2]==0 && vr::backdrop_texture_valid(*solid,512,512));
        const auto at=[](const auto& texture,unsigned x,unsigned y){return (*texture)[(*texture)[4]+y*512+x];};
        require((at(solid,0,0)>>24)==0 && (at(solid,256,400)>>24)==255);
        require((at(haze,256,80)>>24)==255 && (at(haze,256,400)>>24)==0);
        require((at(haze,256,80)&255)>(at(haze,256,200)&255));
        vr::PhotographicBody options;options.center={280,64};options.two_tone=true;options.bright=32767;
        const auto body=vr::photographic_body_packet(solid,options);
        const auto vertices=body.geometry.vertex_view();require(vertices.size()==6);
        const auto length=[](const auto& a,const auto& b) {
            double sum=0;for(unsigned c=0;c<3;++c) sum+=std::pow(a.position[c]-b.position[c],2);return std::sqrt(sum);
        };
        require(std::abs(length(vertices[0],vertices[1])-7)<.0001
            && std::abs(length(vertices[1],vertices[2])-7)<.0001);
        for(const auto& v:vertices) require(v.visibility_a[0]==3 && v.visibility_a[1]==32767 && v.odd_color[3]==0);
        options.diameter[0]=0;bool rejected=false;
        try {(void)vr::photographic_body_packet(solid,options);}catch(const std::invalid_argument&) {rejected=true;}
        require(rejected);
    }
    {
        simulation::SnesPpuState ppu;ppu.background_mode=1;ppu.main_screen=2;
        ppu.bg2_character_base=0;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=32767;
        // One white 4x4 ring encloses four opaque black pixels. Everything
        // outside it is black too, but must expose the surrounding starfield.
        ppu.vram[0x2000]=1;
        for(unsigned y=0;y<8;++y) {
            const uint8_t white=(y==2 || y==5)?0x3c:(y==3 || y==4)?0x24:0;
            ppu.vram[32+y*2]=white;ppu.vram[32+y*2+1]=uint8_t(~white);
            ppu.vram[y*2+1]=255;
        }
        const auto vertices=vr::game_over_foreground_vertices(ppu);
        require(vertices.size()==24);
        for(const auto& v:vertices) require(v.position[0]>=2 && v.position[0]<=6
            && v.position[1]>=2 && v.position[1]<=6 && v.texture[3]==8);
        const auto original=ppu;
        const auto linear=vr::game_over_foreground_vertices(ppu,true);
        require(linear.size()==vertices.size() && linear[0].texture[3]==10 && ppu==original);
    }
    {
        simulation::SnesPpuState ppu;ppu.background_mode=1;
        ppu.cgram[1]=31|(20<<5)|(10<<10);
        vr::BackgroundTileOptions options;options.colour_subtract=12;
        const auto background=vr::background_tile_payload(ppu,vr::BackgroundLayer::bg2,options);
        const auto foreground=vr::background_tile_payload(ppu,vr::BackgroundLayer::bg1,options);
        require(background[17]==0xff00429cU && foreground[17]==0xff52a5ffU);
        const auto stars=vr::game_over_star_sphere_packet(ppu,15,12);
        require(stars.geometry.shared_vertices && stars.geometry.texels[7]==512
            && stars.geometry.texels[17]==background[17]);
        options.colour_subtract=31;
        const auto black=vr::background_tile_payload(ppu,vr::BackgroundLayer::bg2,options);
        require(black[17]==0xff000000U && ppu.cgram[1]==(31|(20<<5)|(10<<10)));
        options.colour_subtract=32;bool rejected=false;
        try {(void)vr::background_tile_payload(ppu,vr::BackgroundLayer::bg2,options);}
        catch(const std::invalid_argument&) {rejected=true;}
        require(rejected);
    }
    {
        render::BackdropImage image;image.width=3;image.height=1;
        image.pixels={0xff0000ffU,0x00ff0000U,0xff0000ffU};
        const auto texture=vr::make_backdrop_texture(image);
        require(vr::backdrop_texture_valid(*texture,3,1));
        require((*texture)[1]==2 && (*texture)[2]==1);
        require((*texture)[(*texture)[4]+1]==0); // Transparent blue cannot bleed.
        require(texture->back()==0xaa0000aaU); // All three odd-width texels included.
        auto invalid=*texture;invalid[7]=UINT32_MAX;
        require(!vr::backdrop_texture_valid(invalid,3,1));
        invalid=*texture;invalid.pop_back();require(!vr::backdrop_texture_valid(invalid,3,1));
        invalid=*texture;invalid.push_back(0);require(!vr::backdrop_texture_valid(invalid,3,1));
        invalid=*texture;invalid[1]=14;require(!vr::backdrop_texture_valid(invalid,3,1));
        require(!vr::backdrop_texture_valid(*texture,4,1));
        require((*vr::make_backdrop_texture(image,false))[2]==0);
        const auto landscape=vr::photographic_landscape_packet(texture);
        require(landscape.geometry.shared_texels==texture && landscape.geometry.vertices.empty());
        unsigned horizon_vertices=0;float minimum_u=100,maximum_u=-100;
        for(const auto& vertex:landscape.geometry.vertex_view()) {
            require(vertex.position[1]>=0 && vertex.uv[1]>=0 && vertex.uv[1]<=1);
            require(vertex.texture[3]==vr::backdrop_texture_flag);
            minimum_u=std::min(minimum_u,vertex.uv[0]);maximum_u=std::max(maximum_u,vertex.uv[0]);
            if(vertex.position[1]==0) {require(std::abs(vertex.uv[1]-1.F)<.00001F);++horizon_vertices;}
        }
        require(horizon_vertices>0 && std::abs(maximum_u-minimum_u-6.F)<.00001F);
        auto options=vr::PhotographicLandscape{};options.vertical_scale=0;
        bool invalid_projection=false;
        try {(void)vr::photographic_landscape_packet(texture,options);}catch(const std::invalid_argument&) {invalid_projection=true;}
        require(invalid_projection);
        options=vr::PhotographicLandscape{};options.palette_shift={-.1F,.2F,0};
        options.horizontal_scale=2;options.repeats=12;
        options.cloud_palette[0]=1;options.cloud_palette[1]=32767;options.cloud_palette[15]=1234;
        const auto shifted=vr::photographic_landscape_packet(texture,options);
        for(const auto& vertex:shifted.geometry.vertex_view()) {
            require(vertex.odd_color[0]==-.1F && vertex.odd_color[1]==.2F);
            require(vertex.visibility_a[0]==1 && vertex.visibility_a[1]==32767 && vertex.group_c[0]==1234);
        }
        vr::EnhancedLandscape enhancement(assets::SymbolMap{});
        vr::DrawPacket native;
        auto native_vertices=std::make_shared<std::vector<vr::SceneVertex>>(9);
        for(unsigned i=0;i<9;++i) (*native_vertices)[i].position[1]=i<3?1.F:-1.F;
        (*native_vertices)[6].position[1]=0; // Preserve a horizon-touching ground face.
        native.geometry.shared_vertices=native_vertices;native.geometry.shared_texels=texture;
        auto unchanged_native=native;
        enhancement.retain_native_ground(native,simulation::SnesPpuState{});
        require(native.geometry.vertex_view().size()==6 && native.geometry.shared_texels==texture);
        enhancement.retain_native_ground(unchanged_native,simulation::SnesPpuState{});
        require(native.geometry.shared_vertices==unchanged_native.geometry.shared_vertices);
        require(native_vertices->size()==9); // Source snapshot remains immutable.
        options.full_sphere=true;options.horizon_v=.5F;
        const auto surround=vr::photographic_landscape_packet(texture,options);
        unsigned upper=0,lower=0;
        for(const auto& vertex:surround.geometry.vertex_view()) {
            upper+=vertex.position[1]>0;lower+=vertex.position[1]<0;
            require(vertex.uv[1]>=0 && vertex.uv[1]<=1);
        }
        require(upper>0 && upper==lower && surround.geometry.vertex_view().size()==64*32*6);
        options.latitude_uv=true;
        const auto stars=vr::photographic_landscape_packet(texture,options);
        unsigned interior=0;
        for(const auto& vertex:stars.geometry.vertex_view()) interior+=vertex.uv[1]>0 && vertex.uv[1]<1;
        require(interior>stars.geometry.vertex_view().size()*9/10); // Not squeezed into a horizon band.
        render::BackdropImage panorama;panorama.width=64;panorama.height=16;
        panorama.pixels.resize(64*16);
        for(unsigned y=0;y<16;++y) for(unsigned x=0;x<64;++x) panorama.pixels[y*64+x]=0xff000000U+x;
        const auto seamless=vr::make_landscape_texture(panorama);
        require(vr::backdrop_texture_valid(*seamless,62,16));
        const auto base=(*seamless)[4];
        for(unsigned x=1;x<62;++x) require((*seamless)[base+x]==(*seamless)[base]);
        for(unsigned level=0;level<(*seamless)[1];++level) {
            const auto record=4+3*level;
            for(unsigned x=0;x<(*seamless)[record+1];++x)
                require((*seamless)[(*seamless)[record]+x]==(*seamless)[base]);
        }
        require((*seamless)[base+15*62]==panorama.pixels[15*64+62]);
        require((*seamless)[base+15*62+20]==panorama.pixels[15*64+20]);
        for(unsigned x=0;x<64;++x) panorama.pixels[15*64+x]=0xff00ff00U;
        const auto spherical=vr::make_landscape_texture(panorama,true);
        const auto stars_texture=vr::make_landscape_texture(panorama,true,true);
        require((*stars_texture)[5]==62 && (*stars_texture)[6]==31
            && vr::backdrop_texture_valid(*stars_texture,62,31));
        for(unsigned level=0;level<(*spherical)[1];++level) {
            const auto record=4+3*level,w=(*spherical)[record+1],h=(*spherical)[record+2];
            if(h>1) for(unsigned x=0;x<w;++x)
                require((*spherical)[(*spherical)[record]+(h-1)*w+x]==0xff00ff00U);
        }
        image.width=4097;
        bool rejected=false;try {(void)vr::make_backdrop_texture(image);}catch(const std::invalid_argument&) {rejected=true;}
        require(rejected);
    }
    {
        simulation::ObjectPool pool;const auto key=pool.allocate_after();
        vr::SourceModelPackets packets;packets.handles={key};packets.packets.resize(1);
        auto& packet=packets.packets[0];packet.model=vr::source_layer_matrix(0,0,256).value();
        packet.model[5]=1;packet.model[14]=-2;
        for(const auto position:{std::array<float,3>{-.2F,-.2F,0},std::array<float,3>{.2F,.2F,0}}) {
            vr::SceneVertex v{};std::copy(position.begin(),position.end(),v.position);packet.geometry.vertices.push_back(v);
        }
        vr::GameSceneSnapshot snapshot;snapshot.view_matrix={32767,0,0,0,32767,0,0,0,32767};
        snapshot.transforms[key].generation=pool.generation(key);
        vr::PauseSandbox sandbox;sandbox.begin(packets,snapshot);
        XrPosef pose{};pose.orientation.w=1;
        std::array<std::optional<XrPosef>,2> hands{pose,std::nullopt};
        sandbox.update(hands,{true,false}); // Held while entering pause must not grab.
        hands[0]->position.x=1;sandbox.update(hands,{true,false});
        auto unchanged=packets;sandbox.apply(unchanged);require(unchanged.packets[0].model==packet.model);
        hands[0]=pose;sandbox.update(hands,{false,false});sandbox.update(hands,{true,false});
        hands[0]->position.x=.5F;sandbox.update(hands,{true,false});
        auto moved=packets;sandbox.apply(moved);require(std::abs(moved.packets[0].model[12]-.5F)<.001F);
        sandbox.update({},{});sandbox.commit(pool);
        require(pool.at(key).world_x==128 && !sandbox.active());
        sandbox.commit(pool);require(pool.at(key).world_x==128); // Commit only once.
        sandbox.begin(packets,snapshot);hands={pose,pose};
        sandbox.update(hands,{false,false});sandbox.update(hands,{true,true});
        hands[0]->position.x=.5F;hands[1]->position.x=-.5F;
        sandbox.update(hands,{true,true});moved=packets;sandbox.apply(moved);
        require(std::abs(moved.packets[0].model[12]-.5F)<.001F); // First hand owns the object.
        sandbox.update({},{});hands[0]->position.x=1;
        sandbox.update(hands,{true,true});moved=packets;sandbox.apply(moved);
        require(std::abs(moved.packets[0].model[12]-.5F)<.001F); // Tracking regain must re-arm.
        require(pool.remove(key));const auto recycled=pool.allocate_after();
        require(recycled==key);pool.at(recycled).world_x=42;
        sandbox.commit(pool);require(pool.at(recycled).world_x==42);
    }
    {
        std::array<uint8_t,65536> source{};
        std::array<uint32_t,16386> packed{};
        for(unsigned pattern=0;pattern<5;++pattern) {
            for(size_t i=0;i<source.size();++i)
                source[i]=pattern==0?0:pattern==1?255:pattern==2?uint8_t(i):pattern==3?uint8_t(1U<<(i%8)):uint8_t((i*i+37*i)^0x5aU);
            packed.fill(0xa5a5a5a5U);
            vr::pack_vram(source,std::span<uint32_t,16384>(packed.data()+1,16384));
            require(packed.front()==0xa5a5a5a5U && packed.back()==0xa5a5a5a5U);
            for(size_t i=0;i<source.size();++i)
                require(((packed[1+i/4]>>((i%4)*8))&255U)==source[i]);
        }
    }
    {
        const auto surround=vr::tunnel_surround_packet({.1F,.2F,.3F,1});
        require(surround.geometry.vertices.size()==54);
        require(surround.model==vr::source_layer_matrix(128,112,2).value());
        for(const auto& vertex:surround.geometry.vertices) {
            require(vertex.color[0]==.1F && vertex.color[1]==.2F && vertex.color[2]==.3F && vertex.color[3]==1);
            if(vertex.position[2]==0) require(vertex.position[0]<=0 || vertex.position[0]>=256
                || vertex.position[1]<=0 || vertex.position[1]>=224);
        }
        bool rejected=false;
        try {static_cast<void>(vr::tunnel_surround_packet({NAN,0,0,1}));}
        catch(const std::invalid_argument&) {rejected=true;}
        require(rejected);
        const auto layered=vr::tunnel_surround_packet(
            {.1F,.2F,.3F,1},{.7F,.6F,.5F,1},{.8F,.9F,1.F,1});
        require(layered.geometry.vertices.size()==54);
        require(layered.geometry.vertices[0].color[0]==.1F
            && layered.geometry.vertices[12].color[0]==.7F
            && layered.geometry.vertices[18].color[0]==.8F
            && layered.geometry.vertices[42].color[0]==.7F
            && layered.geometry.vertices[48].color[0]==.8F);
    }
    {
        assets::Shape shape;shape.vertices={{0,0,-20},{2,0,30},{-2,0,30},{0,0,5}};
        shape.faces={{-1,0,{0,0,127},{0,1,2}}};
        render::RenderPose pose;pose.collapse_to_axis_line=true;
        pose.vanish_x=128;pose.vanish_y=112;
        render::RenderSettings settings;settings.focal_length=256;
        vr::SourceAxisInputs inputs;std::string error;
        require(vr::prepare_source_axis_inputs(shape,pose,settings,4,true,inputs,error));
        require(inputs.indices==std::vector<uint32_t>({1,2,0}));
        require(inputs.settings.ranges==std::array<uint32_t,4>{0,2,2,1});
        require(inputs.settings.point_count==4 && inputs.settings.index_count==3
            && inputs.settings.fractional==1 && inputs.settings.has_residuals==1);
        require(inputs.settings.projection==std::array<float,4>{128,112,256,0});
        vr::SourceAxisArenaLayout arena;
        require(vr::layout_source_axis_inputs(inputs,1025,256,256,65536,65536,arena,error));
        require(arena[vr::SourceAxisRegion::indices].offset==1280);
        require(arena[vr::SourceAxisRegion::indices].size==12);
        require(arena[vr::SourceAxisRegion::endpoints].size==64);
        require(arena[vr::SourceAxisRegion::residuals].size==64);
        require(arena[vr::SourceAxisRegion::settings].size==48 && arena.bytes==2096);
        const auto arena_bytes=arena.bytes;
        std::vector<vr::SourceSpanInputWrite> writes;
        require(vr::source_axis_input_writes(inputs,arena,writes,error));
        require(writes.size()==2 && writes[0].offset==1280 && writes[0].bytes.size()==12
            && writes[1].offset==2048 && writes[1].bytes.size()==48);
        require(std::memcmp(writes[0].bytes.data(),inputs.indices.data(),12)==0);
        require(std::memcmp(writes[1].bytes.data(),&inputs.settings,48)==0);
        auto overlapping=arena;overlapping.regions[1].offset=1280;
        require(!vr::source_axis_input_writes(inputs,overlapping,writes,error));
        require(writes.size()==2 && writes[0].offset==1280 && writes[1].offset==2048);
        auto truncated_arena=arena;truncated_arena.bytes=2048;
        require(!vr::validate_source_axis_upload(inputs,truncated_arena,error));
        auto invalid=inputs;invalid.indices[0]=4;
        require(!vr::layout_source_axis_inputs(invalid,1025,256,256,65536,65536,arena,error));
        require(arena.bytes==arena_bytes);
        invalid=inputs;invalid.settings.ranges[1]=UINT32_MAX;
        require(!vr::layout_source_axis_inputs(invalid,0,256,256,65536,65536,arena,error));
        require(!vr::layout_source_axis_inputs(inputs,0,3,256,65536,65536,arena,error));
        require(!vr::layout_source_axis_inputs(inputs,UINT64_MAX,256,256,65536,65536,arena,error));
        require(!vr::layout_source_axis_inputs(inputs,0,256,256,63,65536,arena,error));
        require(!vr::layout_source_axis_inputs(inputs,0,256,256,65536,47,arena,error));
        require(arena.bytes==arena_bytes);
        const auto saved=inputs.indices;
        auto multi=shape;
        multi.faces.push_back({-1,1,{0,0,127},{1,2,3}});
        multi.colour_words={0x11,0x22};multi.bsp_root_address=0xdeadbeef;
        vr::SourceSpanModel axis_model;vr::SourceAxisInputs model_inputs;
        require(vr::prepare_source_axis_model(multi,pose,settings,256,192,axis_model,model_inputs,error));
        require(axis_model.bsp.faces.size()==1 && axis_model.bsp.output_capacity==1
            && axis_model.faces.primitives[0]==render::PackedPrimitive::line);
        require(axis_model.projection_settings[0]==4 && model_inputs.indices==saved);
        require(axis_model.faces.materials[0].even==1);
        auto warp_pose=pose;warp_pose.colour_warp=true;warp_pose.projected_points_address=0xfff8;
        warp_pose.explosion_progress=12;
        require(vr::prepare_source_axis_model(multi,warp_pose,settings,256,192,axis_model,model_inputs,error));
        vr::SourceWarpInputs axis_warp;
        require(vr::prepare_source_warp_inputs(multi,warp_pose,settings,axis_model.projection,axis_model.bsp,axis_warp,error,
            nullptr,axis_model.source_vertex_count));
        require(axis_warp.shading.settings.capacity==1 && axis_warp.shading.settings.face_count==1);
        require(axis_warp.shading.settings.seed==16 && (axis_warp.shading.settings.flags&16U));
        require(axis_warp.templates.primitives.size()==1 && axis_warp.templates.primitives[0]==render::PackedPrimitive::line);
        require(!vr::prepare_source_axis_inputs(shape,pose,settings,3,false,inputs,error));
        require(inputs.indices==saved && inputs.settings.has_residuals==1);
        auto flat=shape;for(auto& vertex:flat.vertices) vertex.z=0;
        require(vr::prepare_source_axis_inputs(flat,pose,settings,4,false,inputs,error));
        require(inputs.indices==std::vector<uint32_t>({0,1,2,3,0,1,2,3}));
        require(inputs.settings.ranges==std::array<uint32_t,4>{0,4,4,4});
        pose.collapse_to_axis_line=false;
        require(!vr::prepare_source_axis_inputs(shape,pose,settings,4,false,inputs,error));
    }
    {
        assets::Shape shape;shape.has_diffuse_shade_tables=true;
        render::RenderPose pose;pose.palette_override=203;pose.has_depth_colour_tables=true;
        pose.texture_scroll_x=-3;pose.texture_scroll_y=7;
        render::RenderSettings settings;settings.colour_index_base=128;
        for(unsigned band=0;band<4;++band) {
            for(unsigned material=0;material<shape.diffuse_shade_tables[band].size();++material)
                shape.diffuse_shade_tables[band][material].fill(uint8_t(band*62+material));
        }
        const auto packed=render::pack_warp_shading(shape,pose,settings,true);
        require(packed.settings.flags==29 && packed.settings.override_colour==203);
        require(packed.settings.colour_base==128 && packed.settings.scroll_x==-3 && packed.settings.scroll_y==7);
        for(unsigned band=0;band<4;++band) {
            require(packed.settings.shade_counts[band]==shape.diffuse_shade_tables[band].size());
            for(unsigned material=0;material<62;++material) for(unsigned shade=0;shade<10;++shade)
                require(packed.diffuse[(band*62+material)*10+shade]==(material<shape.diffuse_shade_tables[band].size()?uint8_t(band*62+material):0));
        }
        assets::Shape empty;
        const auto defaults=render::pack_warp_shading(empty,{},{});
        for(auto byte:defaults.diffuse) require(byte==0);
        require(defaults.settings.flags==0 && defaults.settings.capacity==0);
    }
    {
        assets::Shape shape;shape.vertices={{-4,-4,0},{4,-4,0},{0,4,0}};
        shape.faces={{-1,0,{0,0,127},{0,1,2}},{-1,0,{0,0,127},{0,1}}};
        render::RenderPose pose;pose.continuous_geometry=true;pose.z=256;
        pose.wobble_mode=1;pose.wave_mode=true;
        vr::SourceSpanModel model;std::string error;
        require(vr::prepare_source_span_model(shape,pose,{},256,192,model,error));
        require(model.bsp.faces.size()==2 && model.faces.polygons.size()==2);
        {
            auto broken_pose=pose;broken_pose.explosion_progress=7;
            auto projection=model.projection;auto fragments=model.faces;
            const auto transforms=render::pack_continuous_fragments(projection,fragments,model.bsp,broken_pose,{},false);
            require(transforms.size()==model.bsp.faces.size()*6+4);
            require(projection.continuous_vertices.size()==5 && projection.visibility_faces.size()==1);
            require(transforms[4].translation[3]==7 && transforms[10].translation[3]==7);
            broken_pose.explosion_phase=6.5;
            auto smooth_projection=model.projection;auto smooth_fragments=model.faces;
            const auto smooth=render::pack_continuous_fragments(smooth_projection,
                smooth_fragments,model.bsp,broken_pose,{},false);
            require(smooth[4].translation[3]==6.5F && smooth[10].translation[3]==6.5F);
            require(transforms[0].vanish[2]==5 && transforms[6].vanish[2]==11);
            require(fragments.corners[0][0]==0 && fragments.corners[3][0]==3);
            require(projection.continuous_vertices[3].pose==model.projection.continuous_vertices[0].pose+6);
            require(fragments.polygons[0][2]==0 && fragments.polygons[1][2]==0);
        }
        {
            auto warp_pose=pose;warp_pose.colour_warp=true;warp_pose.projected_points_address=65530;
            vr::SourceWarpInputs inputs;
            require(vr::prepare_source_warp_inputs(shape,warp_pose,{},model.projection,model.bsp,inputs,error));
            require(inputs.shading.settings.seed==uint16_t(65530+model.projection.continuous_vertices.size()*6));
            require(inputs.shading.settings.capacity==model.bsp.output_capacity);
            require(inputs.normals.size()==model.bsp.faces.size() && inputs.normals[0][2]==127);
            require(inputs.templates.polygons.size()==model.bsp.faces.size());
            require(inputs.textures.lookup.size()==65536);
            vr::SourceWarpArenaLayout warp_arena;
            require(vr::layout_source_warp_inputs(inputs,123,256,256,128*1024*1024,65536,warp_arena,error));
            uint64_t end=123;
            for(const auto& range:warp_arena.regions) {
                require(range.offset>=end && range.offset%256==0 && range.size>0);
                end=range.offset+range.size;
            }
            require(end==warp_arena.bytes);
            require(warp_arena[vr::SourceWarpRegion::corners].size==uint64_t(model.bsp.output_capacity)*512);
            require(warp_arena[vr::SourceWarpRegion::textures].size==4);
            const auto good_size=warp_arena.bytes;
            require(!vr::layout_source_warp_inputs(inputs,0,3,256,128*1024*1024,65536,warp_arena,error));
            require(!vr::layout_source_warp_inputs(inputs,0,256,256,128,65536,warp_arena,error));
            require(!vr::layout_source_warp_inputs(inputs,0,256,256,128*1024*1024,32,warp_arena,error));
            auto oversized=inputs;oversized.shading.settings.capacity=UINT32_MAX;
            require(!vr::layout_source_warp_inputs(oversized,0,256,256,UINT64_MAX,65536,warp_arena,error));
            require(warp_arena.bytes==good_size);
            inputs.shading.settings.scroll_x=-3;
            std::vector<vr::SourceSpanInputWrite> writes;
            require(vr::source_warp_input_writes(inputs,warp_arena,writes,error));
            require(writes.size()==7); // Empty texture/coordinate inputs omitted.
            for(const auto& write:writes) {
                for(const auto gpu:{vr::SourceWarpRegion::descriptors,vr::SourceWarpRegion::result,
                    vr::SourceWarpRegion::decoded,vr::SourceWarpRegion::polygons,
                    vr::SourceWarpRegion::corners,vr::SourceWarpRegion::materials})
                    require(write.offset!=warp_arena[gpu].offset);
                if(write.offset==warp_arena[vr::SourceWarpRegion::expand_settings].offset) {
                    std::array<uint32_t,8> words{};
                    require(write.bytes.size()==sizeof(words));
                    std::memcpy(words.data(),write.bytes.data(),sizeof(words));
                    require(words[0]==model.bsp.output_capacity && words[6]==uint32_t(-3));
                }
            }
            auto corrupt_layout=warp_arena;
            corrupt_layout.regions[1].offset=corrupt_layout.regions[0].offset;
            require(!vr::source_warp_input_writes(inputs,corrupt_layout,writes,error));
            require(writes.size()==7);
            const auto retained=inputs.templates.polygons;
            warp_pose.explosion_progress=1;
            require(!vr::prepare_source_warp_inputs(shape,warp_pose,{},model.projection,model.bsp,inputs,error));
            require(inputs.templates.polygons==retained);
            vr::SourceSpanModel fragments;
            require(vr::prepare_source_span_model(shape,warp_pose,{},256,192,fragments,error));
            require(fragments.fragmented && fragments.source_vertex_count==3
                && fragments.projection.continuous_vertices.size()==5 && fragments.faces.polygons[1][1]==2);
            require(vr::prepare_source_warp_inputs(shape,warp_pose,{},fragments.projection,fragments.bsp,inputs,error,
                &fragments.faces,fragments.source_vertex_count));
            require(inputs.shading.settings.seed==(0x80000000U|uint16_t(65530+3*6)));
            require(inputs.templates.corners==fragments.faces.corners);
        }
        require(model.faces.polygons[0][1]==3 && model.faces.polygons[1][1]==0);
        {
            auto ordinary_pose=pose;ordinary_pose.wobble_mode=0;ordinary_pose.wave_mode=false;
            vr::SourceSpanModel mixed;
            require(vr::prepare_source_span_model(shape,ordinary_pose,{},256,192,mixed,error,true));
            require(vr::source_span_has_lines(mixed) && mixed.faces.polygons[1][1]==2);
            require(vr::source_span_has_ordinary_faces(mixed));
            require(!mixed.faces.materials[1].textured);
        }
        vr::SourceSpanArenaLayout arena;
        require(vr::layout_source_span_model(model,256,256,128*1024*1024,65536,arena,error));
        uint64_t previous_end=0;
        for(const auto& range:arena.regions) {
            require(range.offset%256==0 && range.size>0 && range.offset>=previous_end);
            previous_end=range.offset+range.size;
        }
        require(previous_end==arena.bytes);
        require(arena[vr::SourceSpanRegion::commands].size==model.spans.count*192ULL*96);
        const auto good_bytes=arena.bytes;
        {
            auto occurrence=model;occurrence.graphics_palette_flags=4;
            vr::SourceSpanArenaLayout occurrence_arena;
            require(!vr::layout_source_span_model(occurrence,256,256,128*1024*1024,65536,occurrence_arena,error));
            occurrence.spans.ordered_mode=2;
            require(vr::layout_source_span_model(occurrence,256,256,128*1024*1024,65536,occurrence_arena,error));
            // The span consumer ignores source order in occurrence mode,
            // but the preceding BSP producer still writes every entry.
            require(occurrence.bsp.output_capacity>1);
            require(occurrence_arena[vr::SourceSpanRegion::order].size>=uint64_t(occurrence.bsp.output_capacity)*4);
            auto expanded=occurrence;expanded.warp_expanded=true;
            expanded.spans.count=expanded.bsp.output_capacity=uint32_t(expanded.bsp.faces.size())+3;
            expanded.spans.polygon_count=expanded.spans.count;
            expanded.bsp_settings[4]=expanded.tree[2]=expanded.spans.count;
            expanded.clip_settings[0]=expanded.spans.count;
            expanded.clip_settings[2]=expanded.spans.count*32U;
            require(vr::layout_source_span_model(expanded,256,256,128*1024*1024,65536,occurrence_arena,error));
            require(occurrence_arena[vr::SourceSpanRegion::clipped].size==uint64_t(expanded.spans.count)*129*16);
            require(occurrence_arena[vr::SourceSpanRegion::materials].size==expanded.bsp.faces.size()*96);
            std::vector<std::byte> expanded_image;
            require(vr::source_span_upload_image(expanded,occurrence_arena,expanded_image,error));
            expanded.warp_expanded=false;
            require(!vr::layout_source_span_model(expanded,256,256,128*1024*1024,65536,occurrence_arena,error));
            occurrence.graphics_palette_flags=8;
            require(!vr::layout_source_span_model(occurrence,256,256,128*1024*1024,65536,occurrence_arena,error));
        }
        require(!vr::layout_source_span_model(model,3,256,128*1024*1024,65536,arena,error));
        require(!vr::layout_source_span_model(model,256,256,16,65536,arena,error));
        require(!vr::layout_source_span_model(model,256,256,128*1024*1024,32,arena,error));
        require(arena.bytes==good_bytes);
        require(model.poses.size()==6 && model.poses[0].vanish[3]==2.f);
        require(model.projection_settings[2]==1 && model.clip_settings[7]==3);
        std::vector<std::byte> upload;
        model.graphics_palette_flags=3;
        for(uint32_t i=0;i<256;++i) model.graphics_palette[i]=0xff000000U|i|(255U-i)<<8;
        require(vr::source_span_upload_image(model,arena,upload,error));
        require(upload.size()==arena.bytes);
        std::array<uint32_t,4> graphics_header{};
        std::memcpy(graphics_header.data(),upload.data()+arena[vr::SourceSpanRegion::graphics_headers].offset,16);
        require(graphics_header[0]==arena[vr::SourceSpanRegion::commands].offset/4);
        require(graphics_header[1]==arena[vr::SourceSpanRegion::masks].offset/4);
        require(graphics_header[2]==192 && graphics_header[3]==0x80000000U);
        std::array<uint32_t,20> lookup{};
        std::memcpy(lookup.data(),upload.data()+arena[vr::SourceSpanRegion::graphics_lookup].offset,sizeof(lookup));
        require(lookup[19]==arena[vr::SourceSpanRegion::texture_bytes].offset/4);
        require(lookup[8]==arena[vr::SourceSpanRegion::graphics_polygons].offset/4);
        std::array<uint32_t,8> graphics_polygons{};
        std::memcpy(graphics_polygons.data(),upload.data()+arena[vr::SourceSpanRegion::graphics_polygons].offset,sizeof(graphics_polygons));
        require(graphics_polygons[1]==3 && graphics_polygons[5]==2);
        require(model.faces.polygons[1][1]==0); // Solid clipping still suppresses the line.
        {
            auto textured=model;
            textured.faces.texels={0,1,17,255,2,3,4,5};
            auto& material=textured.faces.materials[0];
            material.textured=1;material.texture_offset=4;material.u_mask=1;material.v_mask=1;
            vr::SourceSpanArenaLayout texture_arena;
            require(vr::layout_source_span_model(textured,256,256,128*1024*1024,65536,texture_arena,error));
            std::vector<std::byte> texture_upload;
            require(vr::source_span_upload_image(textured,texture_arena,texture_upload,error));
            require(std::memcmp(texture_upload.data()+texture_arena[vr::SourceSpanRegion::texture_bytes].offset,
                textured.faces.texels.data(),textured.faces.texels.size())==0);
            const auto saved=texture_upload;
            material.texture_offset=5;
            require(!vr::source_span_upload_image(textured,texture_arena,texture_upload,error) && texture_upload==saved);
            material.texture_offset=4;material.u_mask=UINT32_MAX;
            require(!vr::source_span_upload_image(textured,texture_arena,texture_upload,error));
            material.u_mask=1;textured.faces.texels.pop_back();
            require(!vr::source_span_upload_image(textured,texture_arena,texture_upload,error));
        }
        require(lookup[0]==arena[vr::SourceSpanRegion::order].offset/4);
        require(lookup[1]==arena[vr::SourceSpanRegion::results].offset/4);
        require(lookup[2]==arena[vr::SourceSpanRegion::points].offset/4);
        require(lookup[3]==arena[vr::SourceSpanRegion::graphics_headers].offset/4);
        require(lookup[4]==model.spans.count && lookup[5]==model.spans.polygon_count && lookup[6]==3);
        require(lookup[7]==256 && lookup[10]==model.faces.corners.size() && lookup[13]==192);
        require(lookup[14]==arena[vr::SourceSpanRegion::palette].offset/4 && lookup[15]==3);
        require(std::memcmp(upload.data()+arena[vr::SourceSpanRegion::palette].offset,
            model.graphics_palette.data(),sizeof(model.graphics_palette))==0);
        auto invalid_palette=model;invalid_palette.graphics_palette_flags=4;
        auto unchanged=upload;
        require(!vr::source_span_upload_image(invalid_palette,arena,upload,error) && upload==unchanged);
        const auto command_range=arena[vr::SourceSpanRegion::commands];
        require(std::all_of(upload.begin()+command_range.offset,upload.begin()+command_range.offset+command_range.size,
            [](std::byte b){return b==std::byte{};}));
        const auto old_upload=upload;
        require(vr::validate_source_span_upload(model,arena,error));
        auto ordinary=model;ordinary.graphics_unclipped=true;
        vr::SourceSpanArenaLayout ordinary_arena;
        require(vr::layout_source_span_model(ordinary,256,256,128*1024*1024,65536,ordinary_arena,error));
        require(ordinary_arena[vr::SourceSpanRegion::commands].size==4
            && ordinary_arena[vr::SourceSpanRegion::clipped].size==4
            && ordinary_arena[vr::SourceSpanRegion::masks].size==4 && ordinary_arena.bytes<arena.bytes);
        std::vector<vr::SourceSpanInputWrite> inputs;
        require(vr::source_span_input_writes(model,arena,inputs,error));
        size_t input_bytes=0;
        for(const auto& input:inputs) {
            input_bytes+=input.bytes.size();
            require(std::equal(input.bytes.begin(),input.bytes.end(),upload.begin()+input.offset));
            require(input.offset+input.bytes.size()<=command_range.offset || input.offset>=command_range.offset+command_range.size);
        }
        require(input_bytes<arena.bytes/2);
        std::vector<vr::SourceSpanInputWrite> recycle;
        require(vr::source_span_input_writes(model,arena,inputs,error,&recycle));
        std::vector<const std::byte*> retained;
        for(const auto& input:inputs) retained.push_back(input.bytes.data());
        require(vr::source_span_input_writes(model,arena,inputs,error,&recycle));
        require(vr::source_span_input_writes(model,arena,inputs,error,&recycle));
        require(inputs.size()==retained.size());
        for(size_t i=0;i<inputs.size();++i) require(inputs[i].bytes.data()==retained[i]);
        require(!vr::source_span_input_writes(model,arena,inputs,error,&inputs));
        auto misaligned=arena;misaligned.regions.back().offset+=1;misaligned.bytes+=1;
        require(!vr::validate_source_span_upload(model,misaligned,error));
        require(!vr::source_span_upload_image(model,misaligned,upload,error) && upload==old_upload);
        auto overlapping=arena;overlapping.regions[1].offset=overlapping.regions[0].offset;
        require(!vr::source_span_upload_image(model,overlapping,upload,error));
        require(upload==old_upload);
        for(unsigned field=0;field<8;++field) {
            auto invalid_settings=model;invalid_settings.clip_settings[field]^=1;
            require(!vr::source_span_upload_image(invalid_settings,arena,upload,error));
            require(upload==old_upload);
        }
        auto invalid_projection=model;invalid_projection.projection_settings[2]=3;
        require(!vr::source_span_upload_image(invalid_projection,arena,upload,error));
        auto invalid_tree=model;invalid_tree.tree[2]+=1;
        require(!vr::source_span_upload_image(invalid_tree,arena,upload,error));
        require(upload==old_upload);
        auto matrix_pose=pose;matrix_pose.use_rotation_matrix=true;
        vr::SourceSpanModel matrix_model;
        require(vr::prepare_source_span_model(shape,matrix_pose,{},256,192,matrix_model,error));
        require(matrix_model.poses.size()==4 && matrix_model.projection_settings[2]==0 && matrix_model.clip_settings[7]==0);
        render::RenderSettings culled;culled.backface_culling=true;
        require(vr::prepare_source_span_model(shape,matrix_pose,culled,256,192,matrix_model,error));
        require(matrix_model.projection_settings[2]==2 && matrix_model.clip_settings[7]==3);
        require(model.spans.count==2 && model.spans.mask_enabled==1 && model.spans.mask_stride==32);
        require(model.tree[2]==2 && model.faces.materials[0].reserved1==(262144U|131072U));
        require(!vr::prepare_source_span_model(shape,pose,{},0,192,model,error));
        require(model.spans.width==256 && model.faces.polygons[0][1]==3);
        pose.explosion_progress=1;
        vr::SourceSpanModel fragmented;
        require(vr::prepare_source_span_model(shape,pose,{},256,192,fragmented,error));
        require(fragmented.fragmented && fragmented.poses.size()==fragmented.bsp.faces.size()*6+4);
        vr::SourceSpanArenaLayout fragment_layout;
        require(vr::layout_source_span_model(fragmented,256,256,128*1024*1024,65536,fragment_layout,error));
        require(fragment_layout[vr::SourceSpanRegion::poses].size==fragmented.poses.size()*sizeof(render::ContinuousTransformPose));
        fragmented.fragmented=false;
        require(!vr::layout_source_span_model(fragmented,256,256,128*1024*1024,65536,fragment_layout,error));
        pose.explosion_progress=0;
        shape.bsp_root_address=100;
        shape.visibilities={{0,1,2}};
        shape.face_batches={{200,{shape.faces[0]}},{201,{shape.faces[1]}}};
        shape.bsp_nodes={{100,0,200,101,102}};
        shape.bsp_leaves={{101,200},{102,201}};
        require(vr::prepare_source_span_model(shape,pose,{},256,192,model,error));
        // One solid face occurs twice in the traversal; capacity counts both,
        // while all material/topology arrays retain the two unique face IDs.
        require(model.spans.count==3 && model.spans.polygon_count==2);
        require(model.faces.materials.size()==2 && model.tree[2]==3);
        require(model.faces.polygons[0][1]==3 && model.faces.polygons[1][1]==0);
    }
    {
        vr::GameSceneSnapshot before,after;
        before.background_landscape=after.background_landscape=true;
        before.background_id=after.background_id=27;
        require(vr::same_landscape_mapping(before,after));
        after.background_id=189;
        require(!vr::same_landscape_mapping(before,after));
        after=before;after.landscape_atlas_origin=272;
        require(!vr::same_landscape_mapping(before,after));
        after=before;after.meters.extended=true;
        require(!vr::same_landscape_mapping(before,after));
        after=before;after.background_landscape_unique_half=true;
        require(!vr::same_landscape_mapping(before,after));
        after=before;after.background_landscape=false;
        require(!vr::same_landscape_mapping(before,after));
    }
    {
        simulation::WindowWipeState off{},closed;
        closed.active=true;closed.logic=0xaa;
        closed.left.fill(16);closed.right.fill(239);
        auto opening=closed;
        for(unsigned y=80;y<112;++y) {opening.left[y]=15;opening.right[y]=16;}
        const auto full=vr::source_shutter_packet(off,closed,0);
        require(full.geometry.vertices.size()==12);
        for(const auto& vertex:full.geometry.vertices) {
            require(vertex.color[0]==0 && vertex.color[1]==0 && vertex.color[2]==0 && vertex.color[3]==1);
            require(vertex.position[0]>=-1 && vertex.position[0]<=1 && vertex.position[1]>=-1 && vertex.position[1]<=1);
        }
        for(unsigned phase=0;phase<12;++phase) {
            const auto mask=vr::source_shutter_packet(opening,off,phase/12.);
            require(mask.geometry.vertices.size()==12);
            const float expected=float(-1.+2.*std::lerp(80.,0.,phase/12.)/192.);
            require(std::abs(mask.geometry.vertices[2].position[1]-expected)<1e-6);
        }
        require(vr::source_shutter_packet(opening,off,1).geometry.vertices.empty());
        auto other=opening;other.logic=0xff;
        require(vr::source_shutter_packet(other,other,.5).geometry.vertices.empty());
    }
    {
        assets::Shape decal;
        decal.vertices={{-20,-20,0},{20,-20,0},{0,20,0}};
        decal.faces={{-1,0,{0,0,1},{0,1,2}},{-1,1,{0,0,1},{1,2,0}}};
        decal.colour_words={0x11,0x4000};
        assets::TextureImage texture;texture.descriptor=0x4000;texture.texels={1};
        decal.textures={texture};
        render::Palette256 palette{};palette[1]={255,255,255,255};
        render::RenderPose pose;pose.z=512;
        vr::DrawPacket packet;std::string error;
        require(vr::build_draw_packet(decal,pose,palette,0,1,false,256,packet,error));
        require(packet.geometry.vertices.size()==6);
        require(packet.geometry.vertices[0].texture[3]==0 && (packet.geometry.vertices[3].texture[3]&32768));
        pose.continuous_geometry=true;
        vr::SourceSpanModel resident;
        require(vr::prepare_source_span_model(decal,pose,{},224,192,resident,error,true));
        require(resident.graphics_unclipped && resident.faces.polygons[1][1]==3);
        require((resident.faces.polygons[1][3]&16U)!=0);
        auto effect=pose;effect.wave_mode=true;
        require(!vr::prepare_source_span_model(decal,effect,{},224,192,resident,error,true));
        require(vr::prepare_source_span_model(decal,effect,{},224,192,resident,error));
        require(!resident.graphics_unclipped && resident.faces.polygons[1][1]==0);
        require((resident.faces.polygons[1][3]&16U)!=0);
        // Similar, but genuinely separate surfaces are not decals.
        decal.vertices.push_back({0,20,1});decal.faces[1].vertex_indices={1,3,0};
        require(vr::build_draw_packet(decal,pose,palette,0,1,false,256,packet,error));
        require(!(packet.geometry.vertices[3].texture[3]&32768));
        require(vr::prepare_source_span_model(decal,pose,{},224,192,resident,error,true));
        require((resident.faces.polygons[1][3]&16U)==0);
        require(vr::prepare_source_span_model(decal,effect,{},224,192,resident,error));
        require((resident.faces.polygons[1][3]&16U)==0);
        {
            auto sprite=decal;
            sprite.faces={decal.faces[1]};sprite.faces[0].sprite=true;sprite.faces[0].vertex_indices={0};
            vr::SourceSpanModel sprite_model;
            require(vr::prepare_source_span_model(sprite,pose,{},224,192,sprite_model,error,true));
            require(sprite_model.faces.polygons[0][1]==1 && sprite_model.faces.materials[0].textured);
            require(vr::source_span_corner_capacity(sprite_model)==4 && !vr::source_span_has_lines(sprite_model));
            for(unsigned mode=0;mode<4;++mode) {
                auto effect_pose=pose;effect_pose.cel_mode=mode==0;effect_pose.wave_mode=mode==1;
                effect_pose.wobble_mode=mode==2?1:0;effect_pose.wireframe_mode=mode==3?1:0;
                vr::SourceSpanModel unaffected;
                require(vr::prepare_source_span_model(sprite,effect_pose,{},224,192,unaffected,error));
                require(unaffected.graphics_unclipped && unaffected.faces.polygons==sprite_model.faces.polygons
                    && unaffected.faces.corners==sprite_model.faces.corners && unaffected.faces.texels==sprite_model.faces.texels);
            }
            sprite.faces[0].vertex_indices={0,1};
            require(vr::prepare_source_span_model(sprite,pose,{},224,192,sprite_model,error,true));
            require(sprite_model.faces.polygons[0][1]==0); // Invalid centre count remains a no-op.
        }
        auto solid=decal;solid.faces.assign(64,decal.faces[0]);
        const auto begin=std::chrono::steady_clock::now();
        for(unsigned sample=0;sample<500;++sample)
            require(vr::prepare_source_span_model(solid,pose,{},32,32,resident,error,true));
        const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/500;
        require(resident.faces.texels.empty());
        for(const auto& polygon:resident.faces.polygons) require((polygon[3]&16U)==0);
        std::cout<<"Solid source preparation (64 faces): "<<elapsed<<" us/update (CPU preparation, not frame FPS)\n";
    }
    {
        const auto below=vr::intro_planet_motion(24,24,1);
        const auto centered=vr::intro_planet_motion(252,252,1);
        const auto middle=vr::intro_planet_motion(24,252,.5);
        require(below[9]>middle[9] && middle[9]>centered[9] && centered[9]==0);
        for(const auto& matrix:{below,middle,centered})
            require(std::abs(std::hypot(matrix[9]*64,matrix[10]*64)-64)<.001);
        simulation::CircleEffectState off{},circle{true,128,112,80,31,0,0,0x3f};
        const auto disk=vr::source_circle_packet(off,circle,.5);
        require(disk.geometry.vertices.size()==6);
        require(disk.geometry.vertices[0].position[0]==88 && disk.geometry.vertices[0].position[1]==72);
        require(disk.geometry.vertices[0].texture[3]==4096 && disk.geometry.vertices[0].color[0]==1);
        require(vr::source_circle_packet(circle,off,.5).geometry.vertices.empty());
        circle.affected_layers|=0x40;
        require(vr::source_circle_packet(circle,circle,1).geometry.vertices[0].color[3]==.5F);
    }
    {
        simulation::SnesPpuState water;water.background_mode=1;water.main_screen=4;
        water.bg3_scroll_x=29;water.bg3_scroll_y=24;
        water.main_screen|=2;
        const auto receiver=vr::water_surface_packet(water,{},.25F);
        require(receiver.geometry.vertices.size()==12);
        require((receiver.geometry.texels[15]&32U)!=0);
        require(receiver.geometry.texels[14]==0 && receiver.geometry.texels[11]==1);
        bool behind=false,in_front=false;
        for(const auto& vertex:receiver.geometry.vertices) {
            require(std::abs(vertex.position[1])==.25F);
            behind|=vertex.position[2]>0;in_front|=vertex.position[2]<0;
            require(vertex.uv[1]>=0 && vertex.uv[1]<=223);
            if(vertex.position[2]>0) require(vertex.uv[0]<0 || vertex.uv[0]>=256);
            if(vertex.position[2]<-1) require(std::abs(vertex.uv[0]-(128+vertex.position[0]*256/-vertex.position[2]))<.001F);
        }
        require(behind && in_front);
        for(unsigned step=0;step<=240;++step) {
            const double alpha=double(step)/240;
            const auto motion=vr::water_height_motion(.25F,.5F,alpha);
            require(std::abs(motion[5]*.5F-(.25+.25*alpha))<1e-6);
            require(motion[0]==1 && motion[10]==1 && motion[15]==1);
            require(motion[12]==0 && motion[13]==0 && motion[14]==0);
        }
        require(vr::water_height_motion(.25F,.5F,-1)[5]==.5F);
        require(vr::water_height_motion(.25F,.5F,2)[5]==1);
        const auto backdrop=vr::background_tile_packet(water,vr::BackgroundLayer::bg3,{});
        const auto surround=vr::water_surround_packet(water,{});
        require(surround.geometry.texels==backdrop.geometry.texels);
        require(surround.geometry.vertices.empty() && surround.geometry.shared_vertices
            && !surround.geometry.shared_vertices->empty());
        for(const auto& vertex:*surround.geometry.shared_vertices) {
            require(std::abs(std::hypot(vertex.position[0],vertex.position[1],vertex.position[2])-64)<.001F);
            require(vertex.uv[1]>=0 && vertex.uv[1]<=223);
        }
        simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
        ppu.bg2_scroll_x=123;ppu.bg2_scroll_y=321;
        const auto planet=vr::intro_planet_packet(ppu);
        require(planet.geometry.vertices.size()==6);
        require(planet.geometry.texels[3]==0 && planet.geometry.texels[4]==0);
        for(const auto& v:planet.geometry.vertices) {
            require(v.position[2]==-64 && std::abs(v.position[0])==4.5 && std::abs(v.position[1])==4.5);
            require(v.uv[0]>=88 && v.uv[0]<=160 && v.uv[1]>=328 && v.uv[1]<=400);
        }
        const auto sky=vr::landscape_sphere_packet(ppu,{});
        require((sky.geometry.texels[15]&0x20000000U)!=0);
        require((vr::landscape_sphere_packet(ppu,{},112,false,false,312).geometry.texels[15]&0x20000000U)!=0);
        require((vr::landscape_sphere_packet(ppu,{},112,false,false,320).geometry.texels[15]&0x40000000U)!=0);
        require((sky.geometry.texels[15]&0x40000000U)==0);
        require((vr::intro_star_sphere_packet(ppu).geometry.texels[15]&0x20000000U)==0);
        auto warped_sky_ppu=ppu;
        warped_sky_ppu.bg2_horizontal_offsets_enabled=true;
        warped_sky_ppu.bg2_horizontal_offsets.fill(131);
        const auto warped_sky=vr::intro_star_sphere_packet(warped_sky_ppu,15,false,true,false,true);
        require(warped_sky.geometry.texels[10]!=0
            && warped_sky.geometry.texels.size()>=272+16384+448
            && warped_sky.geometry.texels[272+16384]==131);
        require(vr::intro_star_sphere_packet(warped_sky_ppu,15,false,true).geometry.texels[10]==0);
        vr::BackgroundTileOptions twin_options;
        twin_options.expanded_horizontal=true;twin_options.ex_twin_planets=true;
        const auto twin_landscape=vr::landscape_sphere_packet(ppu,twin_options);
        require((twin_landscape.geometry.texels[15]&4U)!=0);
        require(twin_landscape.geometry.texels[3]==0 && twin_landscape.geometry.texels[4]==232);
        vr::BackgroundTileOptions face_options;
        face_options.expanded_horizontal=true;face_options.ex_face_planets=true;
        require((vr::background_tile_payload(ppu,vr::BackgroundLayer::bg2,face_options)[7]&256U)!=0);
        require((vr::background_tile_payload(ppu,vr::BackgroundLayer::bg1,face_options)[7]&256U)==0);
        face_options.expanded_horizontal=false;
        require((vr::background_tile_payload(ppu,vr::BackgroundLayer::bg2,face_options)[7]&256U)==0);
        vr::BackgroundTileOptions island_options;island_options.expanded_horizontal=true;
        island_options.ex_ocean_island=true;
        const auto island_landscape=vr::landscape_sphere_packet(ppu,island_options);
        require((island_landscape.geometry.texels[15]&0x1000000U)!=0);
        require(((island_landscape.geometry.texels[15]>>8)&65535U)==0);
        island_options.expanded_horizontal=false;
        require((vr::landscape_sphere_packet(ppu,island_options).geometry.texels[15]&0x1000000U)==0);
        vr::BackgroundTileOptions volcano_options;volcano_options.expanded_horizontal=true;
        volcano_options.ex_volcanic_horizon=true;
        const auto volcano=vr::landscape_sphere_packet(ppu,volcano_options,112,false,false,240);
        require((volcano.geometry.texels[15]&0x2000000U)!=0);
        require(((volcano.geometry.texels[15]>>8)&65535U)==0);
        require(std::any_of(volcano.geometry.shared_vertices->begin(),volcano.geometry.shared_vertices->end(),
            [](const auto& v){return v.uv[1]==-176.F;}));
        vr::BackgroundTileOptions city_options;city_options.expanded_horizontal=true;city_options.ex_city_planets=true;
        auto city_ppu=ppu;city_ppu.bg2_screen_size=1;
        const auto city=vr::landscape_sphere_packet(city_ppu,city_options,112,false,false,248,true);
        require((city.geometry.texels[15]&0x4000000U)!=0);
        require(((city.geometry.texels[15]>>8)&65535U)==0);
        require(city.geometry.shared_vertices==vr::landscape_sphere_packet(city_ppu,city_options,112,false,false,248,true).geometry.shared_vertices);
        require(city.geometry.shared_vertices!=vr::landscape_sphere_packet(city_ppu,city_options,112,true,false,248,true).geometry.shared_vertices);
        require(city.geometry.shared_vertices!=volcano.geometry.shared_vertices);
        require(std::any_of(city.geometry.shared_vertices->begin(),city.geometry.shared_vertices->end(),
            [](const auto& v){return v.uv[1]<-280.F;}));
        vr::BackgroundTileOptions orbital_options;orbital_options.single_occurrence_top_rows=168;
        orbital_options.transparent_black=true;
        const auto orbital=vr::space_horizon_sphere_packet(ppu,orbital_options);
        require(orbital.geometry.vertices.empty() && !orbital.geometry.shared_vertices->empty());
        require(orbital.geometry.texels[3]==123 && orbital.geometry.texels[4]==321);
        require(orbital.geometry.texels[15]==((168U<<8)|17U));
        auto unique_space_options=orbital_options;unique_space_options.single_occurrence_top_rows=512;
        const auto unique_space=vr::space_horizon_sphere_packet(ppu,unique_space_options);
        require(unique_space.geometry.texels[15]==((512U<<8)|3U));
        const auto quarter_stars=vr::intro_star_sphere_packet(ppu,15,false,false,true);
        require(quarter_stars.geometry.texels[15]==137U);
        const auto planet_patch=vr::unique_planet_packet(ppu,{},std::array<unsigned,4>{56,360,112,112});
        const auto patch_vertices=planet_patch.geometry.vertex_view();
        require(patch_vertices.size()==6);
        double width2=0,height2=0,orthogonal=0,center2=0;
        for(unsigned axis=0;axis<3;++axis) {
            const double u=patch_vertices[1].position[axis]-patch_vertices[0].position[axis];
            const double v=patch_vertices[2].position[axis]-patch_vertices[1].position[axis];
            const double center=(patch_vertices[0].position[axis]+patch_vertices[2].position[axis])*.5;
            width2+=u*u;height2+=v*v;orthogonal+=u*v;center2+=center*center;
        }
        require(std::abs(width2-196)<.01 && std::abs(height2-196)<.01);
        require(std::abs(orthogonal)<.01 && std::abs(center2-4096)<.01);
        auto scrolled_ppu=ppu;scrolled_ppu.bg2_scroll_x=-20;scrolled_ppu.bg2_scroll_y=80;
        vr::BackgroundTileOptions planet_scroll;planet_scroll.scroll_override=std::array<int16_t,2>{-20,80};
        const auto explicit_scroll=vr::unique_planet_packet(ppu,planet_scroll,{56,360,112,112});
        const auto register_scroll=vr::unique_planet_packet(scrolled_ppu,{}, {56,360,112,112});
        require(explicit_scroll.geometry.vertices==register_scroll.geometry.vertices);
        require(explicit_scroll.geometry.vertices!=planet_patch.geometry.vertices);
        bool above=false,below=false;
        for(const auto& v:*orbital.geometry.shared_vertices) {
            above|=v.uv[1]<0;below|=v.uv[1]>=224;
            require(std::abs(std::hypot(v.position[0],v.position[1],v.position[2])-64)<.001F);
        }
        require(above && below); // Unclamped UVs allow shader band rejection.
        const auto orbital_stars=vr::intro_star_sphere_packet(ppu);
        require(orbital_stars.geometry.texels[15]==9U); // Repeat only star-only upper 256 rows.
        require(orbital_stars.geometry.texels[10]==0 && orbital_stars.geometry.texels[12]==0);
        const auto repeating_horizon=vr::space_horizon_sphere_packet(ppu,{});
        require((repeating_horizon.geometry.texels[15]&16U)!=0);
        require((repeating_horizon.geometry.texels[15]>>8)==0); // No unique artwork in LSB horizon.
        const auto orbital_surface=vr::orbital_planet_sphere_packet(ppu,{});
        {
            vr::BackgroundTileOptions horizon_options;
            horizon_options.scroll_override=std::array<int16_t,2>{0,232};
            const auto menu=vr::orbital_planet_sphere_packet(ppu,horizon_options,false,false,true);
            require(menu.model[0]==1 && menu.model[1]==0);
            require(std::abs(menu.model[9]/menu.model[10]-80.F/512.F)<.00001F);
            const auto aligned=vr::orbital_planet_sphere_packet(ppu,horizon_options,false,false,true,true);
            require(aligned.model[0]==0 && aligned.model[1]==-1);
            require(std::abs(aligned.model[8]/aligned.model[10]-80.F/512.F)<.00001F);
            horizon_options.scroll_override=std::array<int16_t,2>{0,312};
            const auto centered=vr::orbital_planet_sphere_packet(ppu,horizon_options,false,false,true,true);
            require(std::abs(centered.model[8])<.00001F && centered.model[10]==1.F);
            for(double alpha:{0.,.25,.5,.75,1.}) {
                const auto motion=vr::orbital_horizon_motion(232,312,alpha,false,true,true);
                require(std::abs(motion[8]/motion[10]-float(80*(1-alpha)/512))<.00001F);
                require(motion[1]==-1 && motion[9]==0);
            }
            require(vr::orbital_horizon_motion(232,312,-1,false,true,true)==aligned.model);
            require(vr::orbital_horizon_motion(232,312,2,false,true,true)==centered.model);
            require(vr::orbital_horizon_motion(288,288,1,true,false,true)[1]==-1);
            require(vr::orbital_horizon_motion(288,288,1,true,false,false)[1]==0);
            require(vr::orbital_horizon_motion(288,288,1,false,false,true)[0]==1);
        }
        require(orbital_surface.geometry.texels[3]==0 && orbital_surface.geometry.texels[4]==0);
        require(orbital_surface.geometry.texels[15]==0x80000000U);
        for(const auto& vertex:orbital_surface.geometry.vertex_view()) {
            if(vertex.position[1]<0) require(vertex.uv[1]>=384 && vertex.uv[1]<=464.001F);
            else require(vertex.uv[1]>=0 && vertex.uv[1]<=384.001F);
            // First ring below the horizon must not stretch five source
            // pixels across the ~25-pixel native angular interval.
            if(vertex.position[1]<-6.F && vertex.position[1]>-7.F)
                require(vertex.uv[1]>420.F && vertex.uv[1]<423.F);
        }
        for(bool srgb:{false,true}) {
            const auto entry=vr::orbital_planet_sphere_packet(ppu,{},srgb,false,true);
            require(entry.geometry.texels[15]==0x88000000U);
            for(const auto& vertex:entry.geometry.vertex_view()) {
                if(vertex.position[1]<0) require(vertex.uv[1]>=424 && vertex.uv[1]<=464.001F);
            }
            const auto thin=vr::orbital_planet_sphere_packet(ppu,{},srgb,true);
            require(thin.geometry.texels[15]==0xc0000000U);
            for(const auto& vertex:thin.geometry.vertex_view()) {
                if(vertex.position[1]<-6.F && vertex.position[1]>-7.F)
                    require(vertex.uv[1]>414.F && vertex.uv[1]<415.F);
                if(vertex.position[1]<-55.F) {
                    require(std::abs(vertex.uv[0]-(128.F+vertex.position[0]*15.F/128.F))<.001F);
                    require(std::abs(vertex.uv[1]-(407.5F+vertex.position[2]*15.F/128.F))<.001F);
                }
                if(vertex.position[1]<0) require(vertex.uv[1]>=400 && vertex.uv[1]<=415.001F);
                else require(vertex.uv[1]>=0 && vertex.uv[1]<=400.001F);
                if(vertex.position[1]<-63.999F) {
                    require(std::abs(vertex.uv[0]-128.F)<.001F);
                    require(std::abs(vertex.uv[1]-407.5F)<.001F);
                }
            }
            require(thin.geometry.shared_vertices==vr::orbital_planet_sphere_packet(ppu,{},srgb,true).geometry.shared_vertices);
        }
        for(bool terrain:{false,true}) {
            const auto start=std::chrono::steady_clock::now();std::size_t count=0;
            for(unsigned i=0;i<128;++i) {
                const auto p=terrain?vr::landscape_sphere_packet(ppu,{}):vr::intro_star_sphere_packet(ppu);
                count+=p.geometry.vertex_view().size();
            }
            require(count==128*64*32*6);
            std::cout<<(terrain?"Landscape":"Star sphere")<<" preparation: "
                <<std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/128
                <<" us/update (CPU preparation, not frame FPS)\n";
        }
        const auto ex_stars=vr::intro_star_sphere_packet(ppu,15,false,true);
        for(const bool gpu_ground:{false,true}) {
            std::array<double,7> times{};
            for(auto& elapsed:times) {
                const auto start=std::chrono::steady_clock::now();
                for(unsigned i=0;i<64;++i) {
                    auto ground=sky;
                    vr::place_landscape_ground(ground,-.125F-float(i%16)*.03125F,gpu_ground);
                    require(ground.geometry.vertex_view().size()==sky.geometry.vertex_view().size());
                }
                elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/64;
            }
            std::sort(times.begin(),times.end());
            std::cout<<"Ground receiver "<<(gpu_ground?"GPU-payload":"CPU-deformed")
                <<" preparation median: "<<times[3]<<" us/update (includes packet copy; excludes GPU execution, not frame FPS)\n";
        }
        require(ex_stars.geometry.texels[3]==uint32_t(int32_t(ppu.bg2_scroll_x))
            && ex_stars.geometry.texels[4]==uint32_t(int32_t(ppu.bg2_scroll_y)));
        require(ex_stars.geometry.texels[15]==1 && ex_stars.geometry.vertex_view().size()==64*32*6);
        require(ex_stars.geometry.shared_vertices==vr::intro_star_sphere_packet(ppu).geometry.shared_vertices);
        require(sky.geometry.shared_vertices==vr::landscape_sphere_packet(ppu,{}).geometry.shared_vertices);
        require(sky.geometry.shared_vertices!=vr::landscape_sphere_packet(ppu,{},112,true).geometry.shared_vertices);
        require(sky.geometry.texels[3]==0 && sky.geometry.texels[4]==232);
        require(vr::landscape_sphere_packet(ppu,{},112,false,false,272).geometry.texels[4]==272);
        require(vr::landscape_sphere_packet(ppu,{},112,false,false,16).geometry.texels[4]==16);
        require(vr::landscape_scroll_motion(272,288,.5,0,0,272)==vr::landscape_scroll_motion(16,32,.5,0,0,16));
        require(sky.geometry.texels[10]==0 && sky.geometry.texels[12]==0);
        auto moon_ppu=ppu;moon_ppu.bg2_screen_size=1;
        const auto moon_sky=vr::landscape_sphere_packet(moon_ppu,{},112,false,true);
        require((moon_sky.geometry.texels[15]&64U)!=0);
        require(moon_sky.geometry.shared_vertices==sky.geometry.shared_vertices);
        const auto right_moon=vr::landscape_sphere_packet(moon_ppu,{},112,false,false,232,true);
        require((right_moon.geometry.texels[15]&192U)==192U);
        require(std::any_of(right_moon.geometry.shared_vertices->begin(),right_moon.geometry.shared_vertices->end(),
            [](const auto& v){return v.uv[1]==-32.F;}));
        auto large_moon=moon_ppu;large_moon.bg2_screen_size=0;large_moon.bg2_tile_size_16=true;
        require((vr::landscape_sphere_packet(large_moon,{},112,false,false,232,true).geometry.texels[15]&192U)==192U);
        large_moon.bg2_screen_size=1;
        require((vr::landscape_sphere_packet(large_moon,{},112,false,false,232,true).geometry.texels[15]&192U)==192U);
        const auto opening_sky=vr::landscape_sphere_packet(large_moon,{},112,false,true,240);
        require((opening_sky.geometry.texels[15]&192U)==64U);
        require(std::any_of(opening_sky.geometry.shared_vertices->begin(),opening_sky.geometry.shared_vertices->end(),
            [](const auto& v){return v.uv[1]==-64.F;}));
        moon_ppu.bg2_screen_size=0;
        bool rejected_moon_atlas=false;
        try {static_cast<void>(vr::landscape_sphere_packet(moon_ppu,{},112,false,true));}
        catch(const std::invalid_argument&) {rejected_moon_atlas=true;}
        require(rejected_moon_atlas);
        const auto pitch0=vr::landscape_pitch_motion(0,4096,0);
        const auto pitchHalf=vr::landscape_pitch_motion(0,4096,.5);
        const auto pitch1=vr::landscape_pitch_motion(0,4096,1);
        require(pitch0[9]<pitchHalf[9] && pitchHalf[9]<pitch1[9]);
        require(pitchHalf[0]==1 && pitchHalf[1]==0 && pitchHalf[2]==0);
        const auto wrapped=vr::landscape_pitch_motion(65535,1,.5);
        require(std::abs(wrapped[9])<.00001F);
        const auto level=vr::landscape_scroll_motion(232,232,1);
        const auto tilt=vr::landscape_scroll_motion(232,296,1);
        const auto tiltHalf=vr::landscape_scroll_motion(232,296,.5);
        require(level[9]==0 && tilt==level && tiltHalf==level);
        require(vr::landscape_scroll_motion(-300,500,.75)==level);
        const auto bank=vr::landscape_scroll_motion(232,232,.5,0,8192);
        require(bank[1]>.38F && bank[1]<.39F && bank[4]==-bank[1]);
        require(bank[2]==0 && bank[6]==0 && bank[8]==0); // No yaw.
        require(sky.geometry.vertex_view().size()==64*32*6);
        auto grounded=sky;
        auto gpu_ground=sky,gpu_ground_next=sky;
        vr::place_landscape_ground(gpu_ground,-52.F/256,true);
        vr::place_landscape_ground(gpu_ground_next,-80.F/256,true);
        require(gpu_ground.geometry.shared_vertices==gpu_ground_next.geometry.shared_vertices);
        require(gpu_ground.geometry.texels.size()==272+16384+1);
        require(std::bit_cast<float>(gpu_ground.geometry.texels.back())==-52.F/256);
        require(std::bit_cast<float>(gpu_ground_next.geometry.texels.back())==-80.F/256);
        for(size_t triangle=0;triangle<sky.geometry.vertex_view().size();triangle+=3) {
            const auto originals=sky.geometry.vertex_view();
            const bool floor=originals[triangle].position[1]<=0 && originals[triangle+1].position[1]<=0 && originals[triangle+2].position[1]<=0;
            for(size_t corner=0;corner<3;++corner) {
                auto expected=originals[triangle+corner];if(floor) expected.texture[3]|=268435456U;
                require(gpu_ground.geometry.vertex_view()[triangle+corner]==expected);
            }
        }
        vr::place_landscape_ground(grounded,-52.F/256);
        const auto original_vertices=sky.geometry.vertex_view(),ground_vertices=grounded.geometry.vertex_view();
        require(original_vertices.size()==ground_vertices.size());
        bool preserved_sky_horizon=false,flattened_floor_horizon=false;
        for(size_t triangle=0;triangle<ground_vertices.size();triangle+=3) {
            const bool sky_triangle=original_vertices[triangle].position[1]>0
                || original_vertices[triangle+1].position[1]>0 || original_vertices[triangle+2].position[1]>0;
            for(size_t corner=0;corner<3;++corner) {
                const size_t i=triangle+corner;
                if(sky_triangle) {
                    require(ground_vertices[i]==original_vertices[i]);
                    preserved_sky_horizon|=original_vertices[i].position[1]==0;
                } else {
                    require(ground_vertices[i].position[1]==-52.F/256);
                    flattened_floor_horizon|=original_vertices[i].position[1]==0;
                }
            }
        }
        require(preserved_sky_horizon && flattened_floor_horizon);
        for(const auto& v:sky.geometry.vertex_view()) {
            require(std::abs(std::hypot(v.position[0],v.position[1],v.position[2])-64)<.001F);
            require(v.uv[1]>=0 && v.uv[1]<=223);
        }
        ppu.main_screen=0;
        require(vr::intro_planet_packet(ppu).geometry.vertices.empty());
    }
    {
        simulation::SnesPpuState ppu;ppu.background_mode=3;ppu.main_screen=3;
        ppu.bg1_character_base=0x1000;ppu.bg2_character_base=0x2000;
        const auto layers=vr::source_mode3_packets(ppu,9,false,std::array<int16_t,2>{7,-3});
        require(layers.size()==8);
        for(unsigned pass=0;pass<2;++pass) {
            vr::BackgroundTileOptions options;options.priority=pass+1;options.brightness=9;
            options.scroll_override=std::array<int16_t,2>{7,-3};
            require(layers[pass*4].geometry.texels==vr::background_tile_payload(ppu,vr::BackgroundLayer::bg2,options));
            options.scroll_override.reset();
            require(layers[pass*4+2].geometry.texels==vr::background_tile_payload(ppu,vr::BackgroundLayer::bg1,options));
            require(layers[pass*4+1].geometry.vertices.empty() && layers[pass*4+3].geometry.vertices.empty());
        }
        ppu.main_screen=0;
        for(const auto& layer:vr::source_mode3_packets(ppu)) {
            require(layer.geometry.vertices.empty());
            require(vr::model_eye_camera(vr::EyeCamera{},layer.model).has_value());
        }
        ppu.background_mode=2;bool rejected=false;
        try {(void)vr::source_mode3_packets(ppu);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected);
    }
    {
        std::vector<uint8_t> bytes(32768);
        bytes[1]=3;bytes[0x400+'A'-32]=1;
        for(unsigned row=0;row<12;++row) {bytes[0x200+24+row*2]=0;bytes[0x200+25+row*2]=row&1?0xa0:0xe0;}
        bytes[0x800]=13;
        const std::string message="A AA A A";
        std::copy(message.begin(),message.end(),bytes.begin()+0x801);
        assets::RomImage rom(std::move(bytes));
        auto symbols=assets::SymbolMap::parse("FONT0WID $00008000\nFONT0FON $00008200\nFONT0TRN $00008400\nMSCALECHARS $00008200\nMARIOMSGS $00008800\nFACEDATA $00009000\n");
        render::ScaledTextRenderer source(rom,symbols);
        std::array<uint16_t,256> colours{};colours[109]=0x7fff;
        const auto portrait=vr::source_portrait_packet(rom,symbols,0,false,colours);
        simulation::DialogueState dialogue;
        vr::GameSceneSnapshot comms;
        comms.meters.extended=true;comms.flow=simulation::GameFlowState::gameplay;
        require(!vr::replace_native_dialogue(comms));
        comms.dialogue.active=true;require(vr::replace_native_dialogue(comms));
        comms.paused=true;require(!vr::replace_native_dialogue(comms));
        comms.paused=false;comms.flow=simulation::GameFlowState::stage_results;
        require(!vr::replace_native_dialogue(comms));
        comms.flow=simulation::GameFlowState::training;require(vr::replace_native_dialogue(comms));
        comms.flow=simulation::GameFlowState::intro;require(vr::replace_native_dialogue(comms));
        comms.paused=true;require(!vr::replace_native_dialogue(comms));
        comms.paused=false;comms.flow=simulation::GameFlowState::ex_pregame_menu;
        require(!vr::replace_native_dialogue(comms));
        dialogue.meter_visible=true;
        require(vr::source_dialogue_meter_packet(dialogue,colours).geometry.vertices.empty());
        dialogue.active=true;dialogue.meter_visible=false;
        require(vr::source_dialogue_meter_packet(dialogue,colours).geometry.vertices.empty());
        dialogue.meter_visible=true;
        require(vr::source_dialogue_meter_packet(dialogue,colours).geometry.vertices.size()==24);
        for(unsigned health:{1U,20U,40U,255U}) {
            dialogue.meter_health=uint8_t(health);
            const auto meter=vr::source_dialogue_meter_packet(dialogue,colours);
            require(meter.geometry.vertices.size()==30);
            require(meter.geometry.vertices[26].position[0]==84.F+std::min(health,41U));
            require(meter.geometry.vertices[26].position[1]==187.F);
        }
        require(portrait.geometry.vertices.size()==12);
        require(portrait.geometry.texels[1]==512); // Word-addressed screen base.
        require(portrait.geometry.texels[272+256]==0x1c051c00U);
        require(portrait.geometry.texels[272+272]==0x1c061c01U);
        require(portrait.geometry.vertices[0].texture[1]==112);
        require(portrait.geometry.vertices[0].texture[3]==32);
        require(portrait.geometry.vertices[6].texture[3]==8);
        require(std::abs(portrait.geometry.vertices[0].position[0]-(80.F-32.F*7.F/6.F))<.001F);
        require(portrait.geometry.vertices[2].position[0]==80.F);
        require(portrait.geometry.vertices[2].position[1]==192.F);
        for(size_t count:{0U,1U,4U,8U,256U}) for(int right:{30,35,44,216}) {
            render::Framebuffer expected(256,224),actual(256,224);
            source.draw_game_text(0x8800,28,171,expected,96,13,right,count);
            auto packet=vr::source_game_text_packet(rom,symbols,0x8800,28,171,right,count,109,colours);
            for(size_t v=0;v<packet.geometry.vertices.size();v+=6) {
                const auto& a=packet.geometry.vertices[v];const auto& b=packet.geometry.vertices[v+2];
                require(a.texture[3]==1024);
                for(int y=int(a.position[1]);y<int(b.position[1]);++y)
                    for(int x=int(a.position[0]);x<int(b.position[0]);++x) {
                        const auto row=unsigned(y-int(a.position[1]));
                        const auto bits=packet.geometry.texels[a.texture[0]+1+row/2]>>((row&1)*16);
                        if(bits&(0x8000U>>unsigned(x-int(a.position[0])))) actual.set(x,y,109);
                    }
            }
            for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x) require(actual.get(x,y)==expected.get(x,y));
        }
    }
    {
        simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
        vr::BackgroundTileOptions options;options.single_occurrence_top_rows=168;
        require(vr::background_tile_payload(ppu,vr::BackgroundLayer::bg2,options)[15]==168U*256U);
        require(vr::background_tile_payload(ppu,vr::BackgroundLayer::bg1,options)[15]==0);
        options.single_occurrence_top_rows=225;
        bool rejected=false;
        try {(void)vr::background_tile_packet(ppu,vr::BackgroundLayer::bg2,options);}
        catch(const std::invalid_argument&) {rejected=true;}
        require(rejected);
    }
    {
        simulation::SnesPpuState ppu;
        ppu.background_mode=1;ppu.main_screen=7;
        ppu.bg1_character_base=0x1000;ppu.bg2_character_base=0x2000;
        ppu.bg3_character_base=0x3000;
        ppu.bg1_scroll_x=11;ppu.bg3_scroll_x=33;
        const auto layers=vr::title_foreground_packets(ppu,9,true,
            std::array<int16_t,2>{21,22},true);
        require(layers.size()==3);
        for(unsigned i=0;i<3;++i) {
            const auto& data=layers[i].geometry.texels;
            require(data[13]==6 && data[15]==0); // No black transparency.
            require(data[7]==(i==1?0U:2U));
        }
        require(layers[0].geometry.texels[0]==0x2000);
        require(layers[1].geometry.texels[0]==0x1000);
        require(layers[2].geometry.texels[0]==0x3000);
        require(layers[0].geometry.texels[3]==21 && layers[0].geometry.texels[4]==22);
        require(layers[1].geometry.texels[3]==11 && layers[2].geometry.texels[3]==33);
        const auto logo=vr::title_foreground_packets(ppu,15,false);
        require(logo.size()==2 && logo[1].geometry.texels[0]==0x3000);
        ppu.main_screen=0;
        for(const auto& layer:vr::title_foreground_packets(ppu,15,true))
            require(layer.geometry.texels.empty());
        ppu.background_mode=2;
        require(vr::title_foreground_packets(ppu,15,true).empty());
    }
    {
        for(int dx=-16;dx<=16;++dx) for(int dy=-32;dy<=32;++dy) {
            const std::array<int16_t,2> current{32,48},previous{int16_t(32-dx),int16_t(48-dy)};
            int x=32,y=48,error=std::abs(dx),remaining=dx;
            unsigned step=0;
            do {
                const auto sampled=render::grid_line_sample(current,previous,step);
                require(sampled && *sampled==std::array<int32_t,2>{x-2,y});
                --x;error-=std::abs(dy);
                if(error<0) {y+=dy<0?1:-1;error+=std::abs(dx);}
                --remaining;++step;
            } while(remaining>=0);
            require(!render::grid_line_sample(current,previous,step));
        }
    }
    {
        render::GridLineHistory history;
        const auto first=history.begin(10);
        require(first.advances && first.start==std::array<int16_t,2>{0,0});
        history.finish(first,{23,-17});
        for(unsigned eye=0;eye<12;++eye) {
            const auto repeated=history.begin(10);
            require(!repeated.advances && repeated.start==first.start);
            history.finish(repeated,{99,99});
        }
        const auto next=history.begin(11);
        require(next.start==std::array<int16_t,2>{23,-17});
        history.finish(first,{88,88}); // A stale result cannot change this frame.
        history.finish(next,{-32768,32767});
        require(history.begin(12).start==std::array<int16_t,2>{-32768,32767});
        history.reset();
        require(history.begin(0).start==std::array<int16_t,2>{0,0});
    }
    {
        vr::DrawPacket owned;
        require(vr::model_eye_camera(vr::EyeCamera{},owned.model).has_value());
        require(owned.model==vr::Matrix4{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1});
        owned.geometry.vertices.resize(3);
        auto shared=owned;
        shared.geometry.shared_vertices=std::make_shared<const std::vector<vr::SceneVertex>>(owned.geometry.vertices);
        shared.geometry.vertices.clear();
        require(vr::same_draw_geometry(std::span(&owned,1),std::span(&shared,1)));
        shared.preserve_native_colour=true;
        require(!vr::same_draw_geometry(std::span(&owned,1),std::span(&shared,1)));
        shared.preserve_native_colour=false;
        owned.geometry.vertices[0].position[0]=1;
        require(!vr::same_draw_geometry(std::span(&owned,1),std::span(&shared,1)));
        auto ambiguous=shared;ambiguous.geometry.vertices.resize(3);
        require(!vr::same_draw_geometry(std::span(&ambiguous,1),std::span(&shared,1)));
        require(shared.geometry.vertex_view()[0].position[0]==0);
        owned={};owned.geometry.line_vertices.resize(2);
        shared=owned;
        shared.geometry.shared_line_vertices=std::make_shared<const std::vector<vr::SceneVertex>>(owned.geometry.line_vertices);
        shared.geometry.line_vertices.clear();
        require(vr::same_draw_geometry(std::span(&owned,1),std::span(&shared,1)));
        auto copy=shared;
        require(copy.geometry.line_view().data()==shared.geometry.line_view().data());
        owned.geometry.line_vertices[0].position[0]=1;
        require(!vr::same_draw_geometry(std::span(&owned,1),std::span(&shared,1)));
        ambiguous=shared;ambiguous.geometry.line_vertices.resize(2);
        require(!vr::same_draw_geometry(std::span(&ambiguous,1),std::span(&shared,1)));
        require(shared.geometry.line_view()[0].position[0]==0);
        owned={};owned.geometry.texels={12,34,56};
        shared=owned;
        shared.geometry.shared_texels=std::make_shared<const std::vector<uint32_t>>(owned.geometry.texels);
        shared.geometry.texels.clear();
        require(vr::same_draw_geometry(std::span(&owned,1),std::span(&shared,1)));
        copy=shared;
        require(copy.geometry.texel_view().data()==shared.geometry.texel_view().data());
        require(copy.geometry.same_texels(shared.geometry));
        owned.geometry.texels[1]=78;
        require(!vr::same_draw_geometry(std::span(&owned,1),std::span(&shared,1)));
        ambiguous=shared;ambiguous.geometry.texels.push_back(12);
        require(!vr::same_draw_geometry(std::span(&ambiguous,1),std::span(&shared,1)));
        require(!shared.geometry.same_texels(ambiguous.geometry));
        require(shared.geometry.texel_view()[1]==34);
        owned={};shared={};
        shared.geometry.shared_texels=std::make_shared<const std::vector<uint32_t>>();
        require(owned.geometry.same_texels(shared.geometry));
    }
    {
        for(float distance:{1.F,2.F,8.F}) for(float vx:{112.F,128.F,160.F})
            for(float vy:{96.F,112.F,144.F}) for(float x:{-256.F,0.F,256.F}) {
                const auto plane=vr::source_layer_matrix(vx,vy,distance).value();
                const float y=x*.25F,z=1024.F;
                const float source_x=vx+x*256.F/z,source_y=vy+y*256.F/z;
                const float world_x=plane[0]*source_x+plane[12];
                const float world_y=plane[5]*source_y+plane[13];
                require(std::abs(world_x/distance-x/z)<.000001F);
                require(std::abs(world_y/distance+y/z)<.000001F);
            }
        require(!vr::source_layer_matrix(128,112,0));
        require(!vr::source_layer_matrix(std::numeric_limits<float>::infinity(),112));
    }
    {
        for(unsigned value=0;value<32;++value) for(unsigned brightness=0;brightness<16;++brightness) {
            const uint16_t word=static_cast<uint16_t>(value|((31-value)<<5)|(((value*7)&31)<<10));
            std::array<uint16_t,16> source{};source[0]=word;
            const auto palette=render::decode_bgr555_palette(source);
            const auto faded=render::apply_snes_brightness(palette,static_cast<uint8_t>(brightness))[0];
            const auto actual=vr::source_backdrop_colour(word,brightness);
            require(std::abs(actual[0]*255-faded.r)<.001F && std::abs(actual[1]*255-faded.g)<.001F
                && std::abs(actual[2]*255-faded.b)<.001F && actual[3]==1);
            const auto linear=vr::source_backdrop_colour(word,brightness,true);
            for(unsigned channel=0;channel<3;++channel) {
                const float encoded=linear[channel]<=.0031308F?12.92F*linear[channel]:
                    1.055F*std::pow(linear[channel],1.F/2.4F)-.055F;
                require(std::abs(encoded-actual[channel])<.00001F);
            }
        }
        bool rejected=false;
        try {(void)vr::source_backdrop_colour(0,16);} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected);
    }
    {
        simulation::SnesPpuState ppu;ppu.background_mode=1;
        for(const auto layer:{vr::BackgroundLayer::bg1,vr::BackgroundLayer::bg2,vr::BackgroundLayer::bg3}) {
            ppu.main_screen=0;
            require(vr::background_tile_packet(ppu,layer).geometry.texels.empty());
            ppu.main_screen=7;const auto before=ppu;
            vr::BackgroundTileOptions options;options.brightness=9;options.priority=2;
            const auto packet=vr::background_tile_packet(ppu,layer,options,true);
            require(ppu==before && packet.geometry.vertices.size()==6);
            require(packet.geometry.texels==vr::background_tile_payload(ppu,layer,options));
            require(packet.geometry.vertices[2].position[0]==256 && packet.geometry.vertices[2].position[1]==224);
            require(packet.geometry.vertices[2].uv[0]==256 && packet.geometry.vertices[2].texture[3]==10);
            options.horizontal_bounds={-72,328};
            const auto wide=vr::background_tile_packet(ppu,layer,options);
            require(wide.geometry.vertices[0].position[0]==-72 && wide.geometry.vertices[0].uv[0]==-72);
            require(wide.geometry.vertices[2].position[0]==328 && wide.geometry.vertices[2].uv[0]==328);
            options.guard_inset=16;options.transparent_black=true;
            const auto bitmap=vr::background_tile_packet(ppu,layer,options);
            require(bitmap.geometry.vertices[0].position[0]==16 && bitmap.geometry.vertices[0].uv[0]==16);
            require(bitmap.geometry.vertices[2].position[0]==240 && bitmap.geometry.vertices[2].uv[0]==240);
            require(bitmap.geometry.texels[15]==1);
            options.guard_inset=128;
            require(vr::background_tile_packet(ppu,layer,options).geometry.texels.empty());
            options.guard_inset=129;bool rejected=false;
            try {(void)vr::background_tile_packet(ppu,layer,options);} catch(const std::invalid_argument&) {rejected=true;}
            require(rejected);
            options.guard_inset=0;
            options.horizontal_bounds={32,32};
            require(vr::background_tile_packet(ppu,layer,options).geometry.texels.empty());
            options.horizontal_bounds={-4096,4096};
            const auto maximum=vr::background_tile_packet(ppu,layer,options);
            require(maximum.geometry.vertices[0].uv[0]==-4096 && maximum.geometry.vertices[2].uv[0]==4096);
            for(const std::array<int32_t,2> bounds:{std::array<int32_t,2>{-4097,256},
                    std::array<int32_t,2>{0,4097},std::array<int32_t,2>{64,32}}) {
                options.horizontal_bounds=bounds;rejected=false;
                try {(void)vr::background_tile_packet(ppu,layer,options);} catch(const std::invalid_argument&) {rejected=true;}
                require(rejected);
            }
        }
    }
    {
        simulation::SnesPpuState ppu;ppu.main_screen=16;ppu.object_select=0;
        require(vr::source_sprite_packet(ppu).geometry.texels.empty());
        ppu.oam[0]=250;ppu.oam[1]=250;ppu.oam[2]=1;ppu.oam[3]=32;ppu.oam[512]=2;
        ppu.oam[4]=10;ppu.oam[5]=10;ppu.oam[6]=2;ppu.oam[7]=16;
        const auto before=ppu;
        const auto packet=vr::source_sprite_packet(ppu,11,{},true);
        require(ppu==before && packet.geometry.vertices.size()==12 && packet.geometry.texels.size()==272+16384);
        require(packet.geometry.texels[13]==4);
        require((packet.geometry.vertices[0].texture[1]>>8)==2);
        const auto& wrapped=packet.geometry.vertices[6];
        require((wrapped.texture[1]>>8)==1 && wrapped.texture[3]==18);
        require(wrapped.position[0]==250 && wrapped.position[1]==0 && wrapped.uv[0]==0 && wrapped.uv[1]==6);
        require(packet.geometry.vertices[8].position[0]==256 && packet.geometry.vertices[8].position[1]==10);
        require(vr::source_sprite_packet(ppu,15,2).geometry.vertices.size()==6);
        require(vr::source_sprite_packet(ppu,15,3).geometry.texels.empty());
        ppu.main_screen=0;require(vr::source_sprite_packet(ppu).geometry.vertices.empty());
        bool rejected=false;try {(void)vr::source_sprite_packet(ppu,16);} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected);
        ppu={};ppu.main_screen=16;ppu.object_select=0;
        ppu.oam[0]=200;ppu.oam[1]=8;ppu.oam[2]=0xf1;
        auto label=vr::source_sprite_packet(ppu);
        require(label.geometry.vertices[0].position[0]==200 && label.geometry.vertices[0].position[1]==9);
        simulation::MeterState meters;meters.enabled=true;meters.boss_max_health=80;
        label=vr::source_sprite_packet(ppu,15,{},false,&meters);
        require(label.geometry.vertices[0].position[0]==121 && label.geometry.vertices[0].position[1]==9);
        meters.boss_max_health=0xa0;
        require(vr::source_sprite_packet(ppu,15,{},false,&meters).geometry.vertices[0].position[0]==121);
        meters.enabled=false;
        require(vr::source_sprite_packet(ppu,15,{},false,&meters).geometry.vertices.empty());
        meters={};meters.enabled=true;meters.damage=20;meters.boost=10;
        const auto meter_packet=vr::source_meter_packet(meters,ppu.cgram,8);
        require(meter_packet.geometry.vertices.size()==60 && meter_packet.geometry.texels.size()==272);
        require(meter_packet.geometry.vertices[0].position[0]==8 && meter_packet.geometry.vertices[0].position[1]==178);
        require(meter_packet.geometry.vertices[0].texture[1]==125 && meter_packet.geometry.vertices[0].texture[3]==32);
        require(meter_packet.geometry.texels[13]==7);
        meters.enabled=false;require(vr::source_meter_packet(meters,ppu.cgram).geometry.texels.empty());
    }
    {
        // Compare clipped GPU rectangle coverage with the CPU meter layer,
        // including EX multiplayer/death, half-width bosses and moved HUDs.
        for(unsigned state=0;state<128;++state) {
            simulation::MeterState m;m.enabled=!(state&64);m.extended=state&1;
            m.boost_enabled=!(state&2);m.player_two_activated=state&4;
            m.second_player_view=state&8;m.player_one_dead=state&16;
            m.damage=25;m.damage_two=(state&32)?0:19;m.boost=40;
            m.boss_max_health=(state&32)?0xa0:80;m.boss_health=60;
            render::HudLayout layout;
            layout[render::HudElement::shield]={-12,38};
            layout[render::HudElement::boss_health]={7,-3};
            const unsigned width=(state&2)?400:256;
            render::Framebuffer cpu(width,224),gpu(width,224);
            render::SpriteRenderer{}.draw_meters(m,cpu,true,&layout);
            const auto packet=vr::source_meter_packet(m,{},15,width==400,width,true,&layout);
            const auto& vertices=packet.geometry.vertices;
            for(size_t q=0;q<vertices.size();q+=6) {
                const auto& a=vertices[q];const auto& b=vertices[q+2];
                require(a.position[0]>=0 && a.position[1]>=0 && b.position[0]<=width && b.position[1]<=224);
                for(int y=int(a.position[1]);y<int(b.position[1]);++y)
                    for(int x=int(a.position[0]);x<int(b.position[0]);++x)
                        gpu.set(x,y,static_cast<uint8_t>(a.texture[1]));
            }
            require(cpu.pixels()==gpu.pixels());
        }
    }
    {
        vr::GameSceneSnapshot old,now;
        old.view_matrix=now.view_matrix={32767,0,0,0,32767,0,0,0,32767};
        vr::GameSceneObject item;item.handle=2;item.presentation.generation=1;
        item.presentation.rotation_matrix=old.view_matrix;item.presentation.transform.x=100;
        item.source_pose.source_depth=999;item.source_pose.use_source_lighting_state=true;
        now.objects={item};now.transforms[2]=item.presentation;
        {
            auto before=now,after=now;
            before.background_landscape=after.background_landscape=true;
            before.background_vertical_scroll=after.background_vertical_scroll=232;
            before.landscape_grid_height=after.landscape_grid_height=20;
            before.camera.y=-100;after.camera.y=100;
            vr::SceneInterpolationRules rules;rules.fixed_landscape_height=true;
            // A ground-attached object and the ground receiver must undergo
            // the same height tracking and scripted pitch/yaw/roll transform.
            const std::array<simulation::MatrixQ15,3> cinematic_views{{
                {32767,0,0,0,0,32767,0,-32767,0},
                {0,0,-32767,0,32767,0,32767,0,0},
                {0,32767,0,-32767,0,0,0,0,32767}}};
            for(const auto& cinematic:cinematic_views) {
                auto turning=after;turning.view_matrix=cinematic;
                for(unsigned phase=0;phase<=12;++phase) {
                    const double alpha=double(phase)/12;
                    const auto pose=vr::interpolate_scene_poses(before,turning,alpha,rules)[0];
                    const auto motion=vr::landscape_camera_motion(before,turning,alpha);
                    const double point[]{100./256,turning.camera.y/256.,0};
                    const double expected[]{pose.x/256,-pose.y/256,-pose.z/256};
                    for(unsigned row=0;row<3;++row) {
                        double actual=motion[12+row];
                        for(unsigned column=0;column<3;++column) actual+=motion[column*4+row]*point[column];
                        require(std::abs(actual-expected[row])<.0001);
                    }
                }
            }
            for(unsigned phase=0;phase<=12;++phase) {
                const auto pose=vr::interpolate_scene_poses(before,after,double(phase)/12,rules)[0];
                require(std::abs(pose.y-((100.-200.*phase/12)*32767/32768))<.01);
                const auto motion=vr::landscape_camera_motion(before,after,double(phase)/12);
                const double receiver=motion[5]*(after.camera.y/256.)+motion[13];
                require(std::abs(receiver-((-100.+200.*phase/12)/256.*32767/32768))<.0001);
            }
            after.objects[0].presentation.transform.y=40;
            after.transforms[2]=after.objects[0].presentation;
            require(std::abs(vr::interpolate_scene_poses(before,after,1,rules)[0].y-(-60.*32767/32768))<.01);
            require(std::abs(vr::interpolate_scene_poses(before,after,1,{})[0].y-(-60.*32767/32768))<.01);
            // Scripted native pitch survives even with unchanged background scroll.
            after.view_matrix={32767,0,0,0,0,32767,0,-32767,0};
            require(std::abs(vr::interpolate_scene_poses(before,after,1,rules)[0].y)<.01);
            after.view_matrix=before.view_matrix;
            after.background_landscape=false;
            require(std::abs(vr::interpolate_scene_poses(before,after,1,rules)[0].y-(-60.*32767/32768))<.01);
        }
        auto previous=item.presentation;previous.transform.x=0;old.transforms[2]=previous;
        const auto halfway=vr::interpolate_scene_poses(old,now,.5,{});
        require(std::abs(halfway[0].x-50.*32767/32768)<.01 && halfway[0].source_depth==999);
        old.transforms[2].explosion_progress=4;
        now.objects[0].presentation.explosion_progress=5;
        now.transforms[2]=now.objects[0].presentation;
        now.objects[0].source_pose.explosion_progress=5;
        require(vr::interpolate_scene_poses(old,now,.5,{})[0].explosion_phase==4.5);
        require(!vr::interpolate_scene_poses(old,now,1,{})[0].explosion_phase);
        old.transforms[2].generation=2;
        require(!vr::interpolate_scene_poses(old,now,.5,{})[0].explosion_phase);
        require(std::abs(vr::interpolate_scene_poses(old,now,.5,{})[0].x-100.*32767/32768)<.01);
        now.player=1;old.transforms[1]=previous;now.transforms[1]=item.presentation;
        now.objects[0].object.strategy_address=123;now.objects[0].presentation.transform.x=900;
        for(unsigned phase=0;phase<=12;++phase) {
            const auto poses=vr::interpolate_scene_poses(old,now,double(phase)/12,{0,123,0,0});
            require(std::abs(poses[0].x-(100.*phase/12)*32767/32768)<.01 && poses[0].source_depth==999);
        }
        bool rejected=false;
        try {vr::interpolate_scene_poses(old,now,std::numeric_limits<double>::quiet_NaN(),{});} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected);
    }
    assets::Shape shape;shape.header.shift=3;
    shape.vertices={{1,-2,3},{4,5,-6},{7,8,9}};shape.word_coordinates=std::vector<bool>(3,false);shape.word_coordinates[1]=true;
    shape.faces={{-1,0,{3,4,5},{0,1,2}}};shape.colour_words={0x0011};
    render::Palette256 palette{};palette[1]={255,127,32,255};
    render::RenderPose pose;pose.scale=2;pose.x=256;pose.y=128;pose.z=512;
    vr::DrawPacket packet;std::string error;
    require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,packet,error));
    require(packet.geometry.vertices.size()==3 && packet.geometry.deferred.empty());
    {
        auto visible=shape;visible.header.shift=0;visible.vertices={{-50,-50,0},{50,-50,0},{0,50,0}};
        visible.faces[0].vertex_indices={2,1,0};
        visible.word_coordinates.assign(3,false);
        render::RenderPose p;p.z=512;
        render::Framebuffer baseline(256,224),with_noops(256,224);
        render::SoftwareRenderer renderer;renderer.draw(visible,p,baseline);
        require(std::any_of(baseline.pixels().begin(),baseline.pixels().end(),[](auto v){return v!=0;}));
        auto empty=visible.faces[0];empty.vertex_indices.clear();visible.faces.push_back(empty);
        auto point=empty;point.vertex_indices={0};visible.faces.push_back(point);
        auto sprite=point;sprite.sprite=true;visible.faces.push_back(sprite);
        renderer.draw(visible,p,with_noops);require(baseline.pixels()==with_noops.pixels());
        vr::DrawPacket tested;
        require(vr::build_draw_packet(visible,p,palette,0,1,false,256,tested,error));
        require(tested.geometry.vertices.size()==3 && tested.geometry.source_noops.size()==3 && tested.geometry.deferred.empty());
        require(tested.geometry.source_noops[0].reason==vr::SourceNoop::degenerate
            && tested.geometry.source_noops[2].reason==vr::SourceNoop::untextured_sprite);
        visible.faces.back().vertex_indices={99};
        require(!vr::build_draw_packet(visible,p,palette,0,1,false,256,tested,error));
    }
    {
        auto line=shape;line.header.shift=0;line.vertices={{-48,0,0},{48,0,0}};
        line.word_coordinates.assign(2,false);line.faces[0].vertex_indices={0,1};
        line.colour_words={0x4000};
        assets::TextureImage texture;texture.descriptor=0x4000;
        // Deliberately no texels: the source line must never sample them.
        line.textures={texture};
        auto colors=palette;colors[15]={255,255,255,255};
        render::RenderPose p;p.z=512;
        render::Framebuffer reference(256,224);render::SoftwareRenderer renderer;
        renderer.draw(line,p,reference);
        require(std::any_of(reference.pixels().begin(),reference.pixels().end(),[](auto v){return v==15;}));
        require(std::all_of(reference.pixels().begin(),reference.pixels().end(),[](auto v){return v==0 || v==15;}));
        vr::DrawPacket tested;
        require(vr::build_draw_packet(line,p,colors,0,1,false,256,tested,error));
        require(tested.geometry.vertices.empty() && tested.geometry.line_vertices.size()==2
            && tested.geometry.texels.empty());
        for(const auto& vertex:tested.geometry.line_vertices)
            require(vertex.texture[3]==0 && vertex.color[0]==1 && vertex.color[1]==1 && vertex.color[2]==1);
        // The same malformed texture remains an error for an actual sprite.
        line.faces[0].sprite=true;line.faces[0].vertex_indices={0};
        require(!vr::build_draw_packet(line,p,colors,0,1,false,256,tested,error));
    }
    {
        std::array<vr::DrawPacket,1> a{packet},b=a;
        b[0].model[12]+=1;require(vr::same_draw_geometry(a,b));
        b[0].geometry.vertices[0].uv[0]+=1;require(!vr::same_draw_geometry(a,b));b=a;
        b[0].geometry.vertices[0].group_enabled=1;require(!vr::same_draw_geometry(a,b));b=a;
        b[0].geometry.texels.push_back(123);require(!vr::same_draw_geometry(a,b));b=a;
        b[0].geometry.line_vertices.push_back({});require(!vr::same_draw_geometry(a,b));
        require(!vr::same_draw_geometry(a,{}));
    }
    require(packet.geometry.vertices[0].position[0]==16 && packet.geometry.vertices[1].position[0]==4);
    require(packet.model[12]==1 && packet.model[13]==-.5F && packet.model[14]==-2);
    require(packet.shading.depth_band==0 && packet.shading.light==std::array<int8_t,3>{73,73,73});
    for(const auto depth:{2559.,2560.,3327.,3328.,3839.,3840.}) {
        pose.z=depth;const auto expected=depth<2560?0U:depth<3328?1U:depth<3840?2U:3U;
        require(render::source_shading(pose).depth_band==expected);
    }
    pose.use_rotation_matrix=true;pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
    pose.source_lighting_matrix={-32768,0,0,0,32767,0,0,0,-32768};
    pose.use_source_lighting_state=true;pose.source_depth=2560;
    require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,packet,error));
    require(packet.shading.depth_band==1 && packet.shading.light==std::array<int8_t,3>{-74,73,-74});
    const auto stable=packet;
    pose.z=100;pose.rotation_matrix={0,32767,0,-32767,0,0,0,0,32767};
    require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,packet,error));
    require(packet.shading.depth_band==stable.shading.depth_band && packet.shading.light==stable.shading.light);
    const auto saved_model=packet.model;
    const auto rejected=[&] {require(!vr::build_draw_packet(shape,pose,palette,0,1,false,256,packet,error));require(!error.empty() && packet.model==saved_model);};
    pose.collapse_to_axis_line=true;
    require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,packet,error));
    require(packet.geometry.vertices.empty() && packet.geometry.line_vertices.size()==2);
    require(packet.geometry.line_vertices[0].position[2]==144 && packet.geometry.line_vertices[1].position[2]==-6);
    require(packet.geometry.line_vertices[0].visibility_enabled==0 && packet.geometry.line_vertices[0].group_enabled==0);
    {
        auto axis_shape=shape;
        axis_shape.vertices={{2,0,9},{6,4,9},{0,8,-6},{0,0,8}};
        axis_shape.word_coordinates={false,true,true,false};
        vr::DrawPacket axis;
        require(vr::build_draw_packet(axis_shape,pose,palette,0,1,false,256,axis,error));
        require(axis.geometry.line_vertices[0].position[0]==19 && axis.geometry.line_vertices[0].position[1]==2);
        require(axis.geometry.line_vertices[0].position[2]==76.5F && axis.geometry.line_vertices[1].position[2]==-6);
        axis_shape.frames.resize(1);axis_shape.frames[0].vertices=axis_shape.vertices;
        axis_shape.frames[0].word_coordinates=axis_shape.word_coordinates;
        axis_shape.frames[0].vertices[3].z=10;
        require(vr::build_draw_packet(axis_shape,pose,palette,0,1,false,256,axis,error));
        require(axis.geometry.line_vertices[0].position[2]==160);
        axis_shape.colour_words={0x4000};
        assets::TextureImage axis_texture;axis_texture.descriptor=0x4000;axis_shape.textures={axis_texture};
        palette[15]={255,255,255,255};
        require(vr::build_draw_packet(axis_shape,pose,palette,0,1,false,256,axis,error));
        require(axis.geometry.texels.empty() && axis.geometry.line_vertices[0].texture[3]==0
            && axis.geometry.line_vertices[0].color[0]==1);
    }
    {
        auto forced=pose;forced.force_colour=true;forced.forced_colour=0x13;
        vr::DrawPacket reference,warped;
        require(vr::build_draw_packet(shape,forced,palette,0,1,false,256,reference,error));
        forced.colour_warp=true;
        require(vr::build_draw_packet(shape,forced,palette,0,1,false,256,warped,error));
        require(vr::same_draw_geometry(std::span<const vr::DrawPacket>(&reference,1),
            std::span<const vr::DrawPacket>(&warped,1)));
        auto resident_pose=forced;resident_pose.continuous_geometry=true;
        resident_pose.collapse_to_axis_line=false;
        vr::SourceSpanModel resident_forced,resident_reference;
        require(vr::prepare_source_span_model(shape,resident_pose,{},224,192,resident_forced,error,true));
        resident_pose.colour_warp=false;
        require(vr::prepare_source_span_model(shape,resident_pose,{},224,192,resident_reference,error,true));
        require(resident_forced.faces.materials.size()==resident_reference.faces.materials.size());
        for(size_t face=0;face<resident_forced.faces.materials.size();++face) {
            const auto& a=resident_forced.faces.materials[face];
            const auto& b=resident_reference.faces.materials[face];
            require(a.even==b.even && a.odd==b.odd && a.dither==b.dither && a.textured==b.textured);
        }
        forced.force_colour=false;
        require(!vr::build_draw_packet(shape,forced,palette,0,1,false,256,warped,error));
        require(error=="Native colour-warp stage pending");
        for(const bool axis:{false,true}) {
            forced.collapse_to_axis_line=axis;
            // Zero is a present override, not the absence of an override.
            for(const unsigned index:{0U,15U}) {
                forced.palette_override=static_cast<uint8_t>(index);
                forced.colour_warp=false;
                require(vr::build_draw_packet(shape,forced,palette,0,1,false,256,reference,error));
                forced.colour_warp=true;
                require(vr::build_draw_packet(shape,forced,palette,0,1,false,256,warped,error));
                require(vr::same_draw_geometry(std::span<const vr::DrawPacket>(&reference,1),
                    std::span<const vr::DrawPacket>(&warped,1)));
            }
        }
    }
    pose.collapse_to_axis_line=false;
    pose.explosion_progress=1;
    {
        vr::DrawPacket exploded;
        require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,exploded,error));
        require(!exploded.geometry.vertices.empty() && exploded.model[0]==1 && exploded.model[12]==0);
        for(const auto& vertex:exploded.geometry.vertices) {
            require(vertex.visibility_enabled==2 && vertex.group_enabled==0);
            require(vertex.group_c[0]==1 && vertex.group_c[1]==256);
            require(vertex.group_a[2]==float(pose.z));
        }
        pose.explosion_phase=.5;
        require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,exploded,error));
        require(!exploded.geometry.vertices.empty()
            && exploded.geometry.vertices.front().group_c[0]==.5F);
        pose.explosion_phase.reset();
    }
    pose.explosion_progress=0;
    pose.effect_clip_left=10;pose.effect_clip_right=20;rejected();pose.effect_clip_right=0;
    pose.scale=std::numeric_limits<double>::infinity();rejected();pose.scale=1;
    pose.source_depth=std::numeric_limits<double>::quiet_NaN();rejected();pose.source_depth=0;
    shape.faces[0].sprite=true;shape.faces[0].vertex_indices={0};
    require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,packet,error));
    require(packet.geometry.vertices.empty() && packet.geometry.source_noops.size()==1);
    assets::TextureImage texture;texture.descriptor=0x4000;texture.u_mask=1;texture.v_mask=0;texture.texels={0,1};
    shape.colour_words={0x4000};shape.textures={texture};
    pose.simple_scaled_sprite=true;pose.simple_sprite_world_size=64;pose.z=512;
    require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,packet,error));
    require(packet.geometry.vertices.size()==6 && packet.geometry.texels.size()==257 && packet.geometry.texels[0]==0);
    require(packet.geometry.texels[256]==256);
    require(packet.geometry.vertices[0].billboard[0]==-1 && packet.geometry.vertices[2].uv[1]==1);
    require(packet.geometry.vertices[0].group_a[0]==64 && packet.geometry.vertices[0].group_a[1]==512);
    require(packet.model[0]==1.F/256 && packet.model[5]==-1.F/256); // Ignore source object rotation.
    pose.palette_override=1;
    require(vr::build_draw_packet(shape,pose,palette,0,1,true,256,packet,error));
    require(packet.geometry.vertices[0].texture[3]==(7U|134217728U|536870912U));
    {
        auto full=shape;
        full.textures[0].u_mask=255;full.textures[0].v_mask=1;
        full.textures[0].texels.resize(512);
        for(size_t i=0;i<512;++i) full.textures[0].texels[i]=static_cast<uint8_t>(i);
        vr::DrawPacket packed;
        require(vr::build_draw_packet(full,pose,palette,0,1,false,256,packed,error));
        require(packed.geometry.texels.size()==384);
        for(size_t i=0;i<512;++i)
            require(((packed.geometry.texels[256+i/4]>>((i&3)*8))&255U)==(i&255U));
        full.textures[0].v_mask=255;full.textures[0].texels.resize(65536);
        for(size_t i=0;i<65536;++i) full.textures[0].texels[i]=static_cast<uint8_t>(i);
        const auto started=std::chrono::steady_clock::now();
        for(unsigned repeat=0;repeat<128;++repeat)
            require(vr::build_draw_packet(full,pose,palette,0,1,false,256,packed,error));
        std::cout<<"256x256 sprite CPU preparation: "
            <<std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count()/128
            <<" us/update; "<<packed.geometry.texels.size()*4<<" payload bytes\n";
    }
    pose.z=127;
    {
        auto sample_pose=pose;sample_pose.z=512;sample_pose.palette_override.reset();
        render::Palette256 colours{};
        for(unsigned i=0;i<256;++i) colours[i]={uint8_t(i),uint8_t(i^85),uint8_t(255-i),255};
        vr::DrawPacket sample;
        for(unsigned base=0;base<256;++base) {
            require(vr::build_draw_packet(shape,sample_pose,colours,uint8_t(base),1,false,256,sample,error));
            require(sample.geometry.texels[0]==0);
            for(unsigned i=1;i<256;++i) {
                const auto c=colours[uint8_t(base+i)];
                require(sample.geometry.texels[i]==(uint32_t(c.r)|(uint32_t(c.g)<<8)|(uint32_t(c.b)<<16)|0xff000000U));
            }
        }
        const auto short_palette=std::span<const render::Rgba8>(colours.data(),2);
        require(vr::build_draw_packet(shape,sample_pose,short_palette,0,1,false,256,sample,error));
        const auto retained=sample.geometry.texels;
        require(!vr::build_draw_packet(shape,sample_pose,short_palette,1,1,false,256,sample,error));
        require(sample.geometry.texels==retained);
        sample_pose.palette_override=0;
        require(vr::build_draw_packet(shape,sample_pose,short_palette,255,1,false,256,sample,error));
        require(sample.geometry.texels[0]==0 && (sample.geometry.texels[1]>>24)==255);
    }
    require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,packet,error) && packet.geometry.vertices.empty());
    pose.z=128;pose.simple_sprite_world_size=1000;
    require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,packet,error));
    require(packet.geometry.vertices[0].billboard[0]==-1
        && packet.geometry.vertices[0].group_a[0]==1000 && packet.geometry.vertices[0].group_a[1]==128);
    std::cout<<"Native draw packets: scale, source lighting/depth, transforms and transactional rejection passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
