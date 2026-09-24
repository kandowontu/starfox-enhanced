#include "starfox/vr/vulkan_loader.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include "starfox/vr/enhanced_landscape.hpp"
#include <iterator>
#include <numbers>
#include "check_model_ray_expansion.hpp"
#include "starfox/vr/vulkan_compute_ray_scene.hpp"
#include "starfox/vr/vulkan_legacy_ray_geometry.hpp"
#include "starfox/vr/vulkan_mixed_ray_scene.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/vulkan_pipeline_cache.hpp"
#include "starfox/vr/vulkan_scene_buffer.hpp"
#include "starfox/vr/vulkan_span_pipeline.hpp"
#include "starfox/vr/vulkan_source_storage.hpp"
#include "starfox/vr/vulkan_source_bindings.hpp"
#include "starfox/vr/vulkan_source_model.hpp"
#include "starfox/vr/source_span_layout.hpp"
#include "starfox/render/packed_projection.hpp"
#include "starfox/vr/vulkan_scene_textures.hpp"
#include "starfox/vr/vulkan_connected_grid.hpp"
#include "starfox/vr/vulkan_eye_commands.hpp"
#include "starfox/vr/vulkan_depth_targets.hpp"
#include "starfox/vr/scene_material.hpp"
#include "starfox/vr/shape_batch.hpp"
#include "starfox/vr/game_model_pose.hpp"
#include "starfox/vr/draw_packet.hpp"
#include "starfox/vr/vulkan_draw_packets.hpp"
#include "starfox/vr/source_models.hpp"
#include "starfox/vr/vulkan_source_scene.hpp"
#include "starfox/vr/background_tiles.hpp"
#include "starfox/vr/source_sprites.hpp"
#include "starfox/vr/startup_menu.hpp"
#include "starfox/render/grid_line_sample.hpp"
#include "starfox/render/grid_projection.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include <chrono>
#include <cstring>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <limits>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace starfox::vr;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void check(VkResult result,const char* message) {require(result==VK_SUCCESS,message);}
struct Cleanup {
    std::vector<std::function<void()>> actions;
    ~Cleanup() {for(auto it=actions.rbegin();it!=actions.rend();++it) (*it)();}
};
template<class T> T function(PFN_vkVoidFunction fn) {require(fn!=nullptr,"Missing Vulkan entry point");return reinterpret_cast<T>(fn);}
void bitmap(const std::filesystem::path& path,const std::vector<unsigned char>& rgba,uint32_t width,uint32_t height) {
    std::array<unsigned char,54> header{};
    const auto word=[&](unsigned offset,uint32_t value,unsigned bytes=4) {
        for(unsigned i=0;i<bytes;++i) header[offset+i]=static_cast<unsigned char>(value>>(8*i));
    };
    const auto stride=(width*3+3)&~3U;
    header[0]='B';header[1]='M';word(2,54+stride*height);word(10,54);word(14,40);
    word(18,width);word(22,height);word(26,1,2);word(28,24,2);word(34,stride*height);
    std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(header.data()),header.size());
    std::vector<unsigned char> row(stride);
    for(uint32_t y=height;y>0;--y) {
        for(uint32_t x=0;x<width;++x) for(unsigned c=0;c<3;++c) row[x*3+c]=rgba[((y-1)*width+x)*4+2-c];
        output.write(reinterpret_cast<const char*>(row.data()),row.size());
    }
    require(bool(output),"Write GPU proof bitmap");
}
}
int main(int argc,char** argv) try {
    const bool photographic_sky=argc==4 && std::string_view(argv[2])=="--photographic-landscape";
    std::string photographic_file;
    if(photographic_sky) {photographic_file=argv[3];argc=2;}
    const bool pipeline_cache_fixture=argc==3 && std::string_view(argv[2])=="--cache";
    if(pipeline_cache_fixture) argc=2;
    const bool sbs_fixture=argc==3 && std::string_view(argv[2])=="--sbs";
    const bool tunnel_fixture=argc==3 && std::string_view(argv[2])=="--tunnel-surround";
    if(tunnel_fixture) argc=2;
    const bool gameplane_sprite=argc==3 && (std::string_view(argv[2])=="--sprite-gameplane-gpu"
        || std::string_view(argv[2])=="--sprite-gameplane-reference");
    const bool sprite_gpu=argc==3 && (std::string_view(argv[2])=="--sprite-gpu"
        || std::string_view(argv[2])=="--sprite-gameplane-gpu");
    const bool sprite_reference=argc==3 && (std::string_view(argv[2])=="--sprite-reference"
        || std::string_view(argv[2])=="--sprite-gameplane-reference");
    const bool shadow_gpu=argc==3 && std::string_view(argv[2])=="--shadow-mask-gpu";
    const bool shadow_reference=argc==3 && std::string_view(argv[2])=="--shadow-mask-reference";
    const bool sprite_sizes=sprite_gpu || sprite_reference || shadow_gpu || shadow_reference;
    if(sprite_sizes) argc=2;
    const bool axis_warp=argc==3 && std::string_view(argv[2])=="--axis-gpu-warp";
    const bool axis_near=argc==3 && (std::string_view(argv[2])=="--axis-gpu-near"
        || std::string_view(argv[2])=="--axis-reference-near");
    const bool axis_matrix=argc==3 && (std::string_view(argv[2])=="--axis-gpu-matrix"
        || std::string_view(argv[2])=="--axis-reference-matrix");
    const bool axis_rotated=argc==3 && (std::string_view(argv[2])=="--axis-gpu-rotated"
        || std::string_view(argv[2])=="--axis-reference-rotated");
    const bool axis_gpu=argc==3 && (std::string_view(argv[2])=="--axis-gpu"
        || std::string_view(argv[2])=="--axis-gpu-rotated" || std::string_view(argv[2])=="--axis-gpu-matrix"
        || std::string_view(argv[2])=="--axis-gpu-near" || axis_warp);
    if((axis_rotated || axis_matrix || axis_near) && !axis_gpu) argc=2;
    require(argc==1 || argc==2 || argc==4 || argc==5 || sbs_fixture || axis_gpu,"Usage: starfox_vr_scene_check [capture-directory [--axis-gpu | --sbs | rom symbols [shape | --live | --live-sprites | --text | --particles | --shadows | --shadows-off | --tiles | --tiles-empty | --tile-suite | --source-bg2 | --source-bg3 | --source-bg23 | --source-oam]]]");
    if(argc>=2) std::filesystem::create_directories(argv[1]);
    ShapeBatch cartridge;
    std::vector<DrawPacket> live_packets;
    std::optional<DrawPacket> connected_expected;
    std::optional<std::array<unsigned char,3>> terminal_ground_colour;
    bool menu_blackout_capture=false;
    SourceModelPackets live_compute_packets;
    const bool resident_eyes=argc==5 && std::string_view(argv[4]).starts_with("--resident-eyes-");
    const bool resident_model=resident_eyes || (argc==5 && std::string_view(argv[4]).starts_with("--resident-model="));
    const bool compute_stage=argc==5 && std::string_view(argv[4]).starts_with("--live-compute-stage=");
    const bool explosion_gpu=argc==5 && (std::string_view(argv[4])=="--explosion-gpu" || std::string_view(argv[4])=="--explosion-continuous-gpu");
    const bool live_compute=explosion_gpu || compute_stage || (argc==5 && std::string_view(argv[4])=="--live-compute");
    std::vector<DrawPacket> live_sprites;
    std::vector<DrawPacket> live_backgrounds;
    std::vector<DrawPacket> live_tunnel_surround;
    DrawPacket live_shutter;
    std::array<float,4> live_backdrop{};
    std::vector<DrawPacket> tile_cases;
    std::vector<uint32_t> tile_expected;
    const bool meter_fixture=argc==5 && std::string_view(argv[4])=="--meter-fixture-ex";
    const bool source_meters=meter_fixture || (argc==5 && std::string_view(argv[4])=="--source-meters");
    const bool planet_screen=argc==5 && std::string_view(argv[4])=="--planet-screen";
    const bool source_bitmap=argc==5 && std::string_view(argv[4])=="--source-ex-bitmap";
    const bool source_oam=source_bitmap || source_meters || (argc==5 && std::string_view(argv[4])=="--source-oam");
    const bool omit_dust=argc==5 && std::string_view(argv[4])=="--live-space-without-dust";
    const bool omit_bitmap=argc==5 && (std::string_view(argv[4])=="--live-space-without-bitmap"
        || std::string_view(argv[4]).find("@no-bitmap")!=std::string_view::npos);
    const bool omit_background=argc==5 && std::string_view(argv[4])=="--live-space-without-background";
    const bool cpu_space_dust=argc==5 && std::string_view(argv[4])=="--live-space-cpu-dust";
    const bool space_scene=cpu_space_dust || omit_background || (omit_bitmap && std::string_view(argv[4])=="--live-space-without-bitmap") || omit_dust || (argc==5 && std::string_view(argv[4])=="--live-space");
    const bool omit_connected_shadows=argc==5 && std::string_view(argv[4])=="--live-connected-grid-without-shadows";
    const bool live_connected=omit_connected_shadows || (argc==5 && std::string_view(argv[4])=="--live-connected-grid");
    const bool intro_background=argc==5 && std::string_view(argv[4]).starts_with("--live-intro-background");
    const bool intro_scene=intro_background || (argc==5 && std::string_view(argv[4])=="--live-intro");
    const bool landscape_view=argc==5 && std::string_view(argv[4]).starts_with("--landscape-");
    const bool training_scene=argc==5 && std::string_view(argv[4])=="--landscape-training";
    const bool controls_from_title=argc==5 && std::string_view(argv[4])=="--live-controls-from-title";
    const bool controls_scene=controls_from_title || (argc==5 && std::string_view(argv[4])=="--live-controls");
    const bool cache_fixture=argc==5 && std::string_view(argv[4])=="--live-cache";
    const bool explicit_stage=compute_stage || (argc==5 && std::string_view(argv[4]).starts_with("--live-stage="));
    const bool full_layers=explicit_stage || cache_fixture || controls_scene || landscape_view || intro_scene || live_connected || space_scene || (argc==5 && std::string_view(argv[4])=="--live-composite");
    const bool combined_live=full_layers || (argc==5 && std::string_view(argv[4])=="--live-sprites");
    const bool source_layers=argc==5 && std::string_view(argv[4])=="--source-bg23";
    const bool wide_space_bg2=argc==5 && std::string_view(argv[4])=="--source-space-bg2-wide";
    const bool source_space_bg2=wide_space_bg2 || (argc==5 && std::string_view(argv[4])=="--source-space-bg2");
    const bool source_bg2=source_space_bg2 || source_layers || (argc==5 && std::string_view(argv[4])=="--source-bg2");
    const bool source_bg3=source_bg2 || (argc==5 && std::string_view(argv[4])=="--source-bg3");
    const bool grid_line_suite=argc==5 && std::string_view(argv[4])=="--grid-line-suite";
    const bool tile_suite=grid_line_suite || (argc==5 && std::string_view(argv[4])=="--tile-suite");
    const bool particle_reference=argc==5 && std::string_view(argv[4])=="--particles-reference";
    const bool particles=particle_reference || (argc==5 && std::string_view(argv[4])=="--particles");
    const std::string_view grid_mode=argc==5?std::string_view(argv[4]):std::string_view{};
    const bool rotated_grid=grid_mode=="--grid-rotated" || grid_mode=="--grid-gpu-rotated" ||
        grid_mode=="--connected-grid-rotated" || grid_mode=="--connected-grid-reference-rotated" || grid_mode=="--connected-grid-texture-rotated" || grid_mode=="--connected-grid-common-rotated" || grid_mode=="--connected-grid-binned-rotated" || grid_mode=="--connected-grid-gpu-rotated";
    const bool wrapped_grid=grid_mode=="--grid-wrapped" || grid_mode=="--grid-gpu-wrapped";
    const bool surround_grid=grid_mode=="--grid-surround" || grid_mode=="--grid-surround-lines";
    const bool gpu_grid=surround_grid || grid_mode=="--grid-gpu" || grid_mode=="--grid-gpu-rotated" || grid_mode=="--grid-gpu-wrapped";
    const bool connected_texture=grid_mode=="--connected-grid-texture" || grid_mode=="--connected-grid-texture-rotated";
    const bool connected_reference=connected_texture || grid_mode=="--connected-grid-reference" || grid_mode=="--connected-grid-reference-rotated";
    const bool connected_common=grid_mode=="--connected-grid-common-rotated";
    const bool connected_binned=grid_mode=="--connected-grid-binned" || grid_mode=="--connected-grid-binned-rotated";
    const bool connected_compute=grid_mode=="--connected-grid-gpu" || grid_mode=="--connected-grid-gpu-rotated";
    const bool connected_grid=connected_compute || connected_reference || connected_common || connected_binned || grid_mode=="--connected-grid" || grid_mode=="--connected-grid-rotated";
    const bool grid=connected_grid || gpu_grid || rotated_grid || wrapped_grid || grid_mode=="--grid";
    const bool wrapped_dust=grid_mode=="--dust-wrapped" || grid_mode=="--dust-gpu-wrapped";
    const bool moving_dust=wrapped_dust || grid_mode=="--dust-motion" || grid_mode=="--dust-gpu-motion";
    const bool maximum_dust=grid_mode=="--dust-511" || grid_mode=="--dust-gpu-511";
    const bool coloured_dust=maximum_dust || moving_dust || grid_mode=="--dust-colours" || grid_mode=="--dust-gpu-colours";
    const bool surround_dust=grid_mode.starts_with("--dust-surround-");
    const bool controls_dust=grid_mode=="--dust-controls";
    const bool gpu_dust=controls_dust || surround_dust || grid_mode=="--dust-gpu" || grid_mode=="--dust-gpu-colours" || grid_mode=="--dust-gpu-motion" || grid_mode=="--dust-gpu-wrapped" || grid_mode=="--dust-gpu-511";
    const bool dust=grid || gpu_dust || coloured_dust || grid_mode=="--dust";
    const bool shadows_off=argc==5 && std::string_view(argv[4])=="--shadows-off";
    const bool shadows=shadows_off || (argc==5 && std::string_view(argv[4])=="--shadows");
    const bool startup_menu=grid_mode.starts_with("--startup-");
    const bool font_reference=grid_mode=="--font-reference";
    const bool full_font=font_reference || grid_mode=="--font";
    const bool briefing_reference=grid_mode=="--briefing-text-reference";
    const bool briefing_text=briefing_reference || grid_mode=="--briefing-text";
    const bool scaled_text_reference=grid_mode=="--text-reference" || grid_mode=="--text-reference-close" || grid_mode=="--text-reference-hidden" || grid_mode=="--text-reference-zero";
    const bool scaled_text_close=grid_mode=="--text-close" || grid_mode=="--text-reference-close";
    const bool scaled_text_zero=grid_mode=="--text-zero" || grid_mode=="--text-reference-zero";
    const bool scaled_text_hidden=scaled_text_zero || grid_mode=="--text-hidden" || grid_mode=="--text-reference-hidden";
    const bool projected_text=briefing_text || full_font || scaled_text_reference || scaled_text_close || scaled_text_hidden || grid_mode=="--text";
    const bool tiles_empty=argc==5 && std::string_view(argv[4])=="--tiles-empty";
    const bool tilted_tiles=grid_mode=="--tiles-tilted" || grid_mode=="--tiles-tilted-alternate";
    const bool tiles=tilted_tiles || tiles_empty || (argc==5 && std::string_view(argv[4])=="--tiles");
    const bool explosion_continuous=argc==5 && (std::string_view(argv[4])=="--explosion-continuous" || std::string_view(argv[4])=="--explosion-continuous-reference" || std::string_view(argv[4])=="--explosion-continuous-gpu");
    const bool explosion_gpu_reference=argc==5 && std::string_view(argv[4])=="--explosion-gpu-reference";
    // Resident VR always uses continuous vertices. With subpixel projection
    // disabled, destruction directions still use source Q15 arithmetic. Keep
    // that hybrid reference separate from the fully word-quantized legacy one.
    const bool explosion_reference=explosion_gpu_reference || (argc==5 && (std::string_view(argv[4])=="--explosion-reference" || std::string_view(argv[4])=="--explosion-continuous-reference"));
    const bool explosion=explosion_gpu || explosion_continuous || explosion_reference || (argc==5 && std::string_view(argv[4])=="--explosion");
    const bool live=live_compute || startup_menu || planet_screen || explosion || dust || combined_live || source_oam || source_bg3 || tile_suite || tiles || projected_text || particles || shadows || (argc==5 && std::string_view(argv[4])=="--live");
    Matrix4 cartridge_model{};
    float cartridge_scale=1;
    const bool eye_material_fixture=resident_eyes || grid_mode.starts_with("--eyes-");
    const char* cartridge_name=eye_material_fixture?((grid_mode.starts_with("--eyes-mario") || grid_mode.starts_with("--resident-eyes-mario"))?"MARIOH":"LUIGIH"):
        resident_model?argv[4]+17:argc==5?argv[4]:"MYSHIP_4";
    if(explosion) {
        starfox::assets::Shape shape;
        shape.vertices={{-72,-48,0},{72,-48,0},{0,72,0}};
        starfox::assets::Face face;face.vertex_indices={0,1,2};face.normal={17,-23,31};face.colour_id=1;
        shape.faces.push_back(face);
        starfox::render::Palette256 palette;palette.fill({0,255,0,255});
        starfox::render::RenderPose pose;pose.z=512;pose.x=-9;pose.y=7;
        pose.use_rotation_matrix=true;pose.rotation_matrix={23170,23170,0,-23170,23170,0,0,0,32767};
        pose.explosion_progress=9;
        pose.subpixel_projection=explosion_continuous;
        DrawPacket packet;std::string error;
        require(build_draw_packet(shape,pose,palette,0,1,false,256,packet,error),error.c_str());
        if(explosion_reference) {
            const auto rotate=[&](const starfox::assets::Vec3i& p,bool continuous) {
                std::array<double,3> result{};
                for(unsigned axis=0;axis<3;++axis) {
                    if(continuous) {
                        result[axis]=(double(p.x)*pose.rotation_matrix[axis]+double(p.y)*pose.rotation_matrix[3+axis]+double(p.z)*pose.rotation_matrix[6+axis])/32768.;
                        continue;
                    }
                    auto value=starfox::simulation::multiply_q15(p.x,pose.rotation_matrix[axis]);
                    value=starfox::simulation::add16(value,starfox::simulation::multiply_q15(p.y,pose.rotation_matrix[3+axis]));
                    result[axis]=starfox::simulation::add16(value,starfox::simulation::multiply_q15(p.z,pose.rotation_matrix[6+axis]));
                }
                return result;
            };
            auto direction=rotate({-17,-23,-31},explosion_continuous);direction[1]=-std::abs(direction[1]);
            const int16_t translation[]{-9,7,512};
            for(auto& vertex:packet.geometry.vertices) {
                const bool continuous_vertices=explosion_continuous || explosion_gpu_reference;
                const auto source=rotate({int16_t(vertex.position[0]),int16_t(vertex.position[1]),int16_t(vertex.position[2])},continuous_vertices);
                for(unsigned axis=0;axis<3;++axis) {
                    const auto offset=starfox::simulation::arithmetic_shift_right(int32_t(std::lround(direction[axis]))*9,2);
                    const auto placed=continuous_vertices?source[axis]+translation[axis]:double(starfox::simulation::add16(int16_t(source[axis]),translation[axis]));
                    vertex.position[axis]=float(placed+offset)*(axis? -1.F:1.F)/256.F;
                }
                vertex.visibility_enabled=0;
            }
        }
        if(explosion_gpu) {
            pose.continuous_geometry=true;
            SourceSpanModel model;
            require(prepare_source_span_model(shape,pose,{},256,192,model,error,true),error.c_str());
            model.graphics_palette_flags=1;model.graphics_palette.fill(0xff00ff00U);
            DrawPacket placeholder;placeholder.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            live_compute_packets.packets={placeholder};live_compute_packets.handles={1};
            live_compute_packets.compute_models={{1,0,256,std::move(model)}};
        } else live_packets.push_back(std::move(packet));
    } else if(planet_screen) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        starfox::simulation::GameSimulation game(rom,symbols,"PLANETSELECT");
        starfox::audio::Spc700Audio audio;
        for(unsigned tick=0;tick<120;++tick) {
            const auto result=game.tick({});(void)audio.render_logic_tick(result.audio_port_writes);
            game.synchronize_apu_output_ports(audio.output_ports());
        }
        const auto& ppu=game.map().ppu_state();
        require(ppu.background_mode==3,"Planet screen did not enter Mode 3");
        live_packets=source_mode3_packets(ppu,game.map().display_brightness());
        for(auto& packet:live_packets) packet.model=source_layer_matrix(128,112).value();
    } else if(source_oam) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        starfox::simulation::GameSimulation game(rom,symbols,"LEVEL1_1",{},true);
        game.set_timing_mode(starfox::simulation::TimingMode::unlocked_20_fps);
        starfox::audio::Spc700Audio audio;
        const auto advance=[&] {const auto tick=game.tick({});(void)audio.render_logic_tick(tick.audio_port_writes);game.synchronize_apu_output_ports(audio.output_ports());};
        const auto checkpoint=symbols.find("MAPRESTART").at(0);unsigned warmup=0;
        while(!game.map().peek_ram_word(checkpoint).value() && warmup<3000) {advance();++warmup;}
        require(warmup<3000,"Source OAM checkpoint did not start");
        for(unsigned i=0;i<40;++i) advance();
        if(source_meters && !meter_fixture) {
            unsigned wait=0;
            while(starfox::render::meter_rectangles(game.peek_meter_state(),256).count==0 && wait<600) {advance();++wait;}
            std::cout<<"Meter initialization wait: "<<wait<<" source ticks\n";
        }
        GameSceneHistory history(game,rom,symbols);const auto snapshot=history.current();
        const auto& ppu=*snapshot->ppu;
        auto meters=snapshot->meters;
        if(meter_fixture) {
            // Explicit synthetic EX state, not a cartridge initialization claim.
            meters={};meters.enabled=true;meters.extended=true;meters.player_two_activated=true;
            meters.damage=30;meters.damage_two=17;meters.boost=23;
            meters.boss_max_health=0xa0;meters.boss_health=130;
            std::cout<<"Synthetic EX multiplayer/half-width boss meter fixture\n";
        }
        if(source_meters) {
            const auto& m=snapshot->meters;
            require(m==game.meter_state(),"Snapshot meter state differs from native getter");
            std::cout<<"Source meters: enabled="<<m.enabled<<" extended="<<m.extended
                <<" boost="<<m.boost_enabled<<" dead="<<m.player_one_dead
                <<" second_view="<<m.second_player_view<<" width="<<unsigned(m.player_health_width)
                <<" health="<<unsigned(m.damage)<<" boss_max="<<unsigned(m.boss_max_health)<<'\n';
        }
        auto packet=source_meters?source_meter_packet(meters,ppu.cgram,snapshot->display_brightness)
            :source_sprite_packet(ppu,snapshot->display_brightness);
        if(source_bitmap) {
            require(snapshot->native_ex_bitmap,"Source snapshot is not an EX bitmap scene");
            BackgroundTileOptions options;options.brightness=snapshot->display_brightness;
            options.guard_inset=16;options.transparent_black=true;
            packet=background_tile_packet(ppu,BackgroundLayer::bg1,options);
        }
        require(!packet.geometry.vertices.empty(),source_meters?"Source snapshot has no visible meters":"Source OAM snapshot has no sprites");
        packet.model={.00625F,0,0,0,0,-.00625F,0,0,0,0,1,0,-.8F,.7F,-2,1};
        live_packets.push_back(packet);
        starfox::render::Framebuffer expected(256,224);starfox::render::SpriteRenderer decoder;
        if(source_meters) decoder.draw_meters(meters,expected);
        else decoder.draw_objects(ppu,expected);
        if(source_bitmap) {
            expected.clear();
            starfox::render::BackgroundRenderer{}.draw_bg1(ppu,expected,
                starfox::render::TilePriorityPass::all,0,false,16);
            for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x) {
                const auto index=expected.get(x,y);
                if((ppu.cgram[index]&0x7fff)==0) expected.set(x,y,0);
            }
        }
        const auto palette=starfox::render::apply_snes_brightness(starfox::render::decode_bgr555_palette(ppu.cgram),snapshot->display_brightness);
        unsigned coloured=0;
        for(unsigned y=3;y<224;y+=8) for(unsigned x=3;x<256;x+=16) {
            const auto index=expected.get(x,y);const auto c=palette[index];
            const uint32_t rgb=index?(uint32_t(c.r)|(uint32_t(c.g)<<8)|(uint32_t(c.b)<<16)):0;
            tile_expected.push_back(rgb);coloured+=rgb!=0;
            DrawPacket sample;sample.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            sample.geometry.texels=packet.geometry.texels;
            const auto& vertices=packet.geometry.vertices;
            for(size_t q=0;q<vertices.size();q+=6) {
                const auto& first=vertices[q];const auto& opposite=vertices[q+2];
                if(float(x)<first.position[0] || float(y)<first.position[1]
                    || float(x)>=opposite.position[0] || float(y)>=opposite.position[1]) continue;
                const float corners[4][2]{{-.6F,.6F},{.6F,.6F},{.6F,-.6F},{-.6F,-.6F}};
                for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                    auto v=first;v.position[0]=corners[corner][0];v.position[1]=corners[corner][1];v.position[2]=-2;
                    v.uv[0]=first.uv[0]+float(x)-first.position[0];v.uv[1]=first.uv[1]+float(y)-first.position[1];
                    sample.geometry.vertices.push_back(v);
                }
            }
            tile_cases.push_back(std::move(sample));
        }
        require(coloured>0,"Source OAM grid has no coloured samples");
        std::cout<<"Source OAM: "<<packet.geometry.vertices.size()/6<<" quads, "<<coloured<<" coloured samples\n";
    } else if(source_bg3) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        starfox::simulation::GameSimulation game(rom,symbols,source_space_bg2?"LEVEL1_2":"TITLEMAP",{},source_space_bg2);
        if(source_space_bg2) game.set_timing_mode(starfox::simulation::TimingMode::unlocked_20_fps);
        starfox::audio::Spc700Audio audio;
        for(unsigned i=0;i<(source_space_bg2?58U:600U);++i) {
            const auto tick=game.tick({});static_cast<void>(audio.render_logic_tick(tick.audio_port_writes));
            game.synchronize_apu_output_ports(audio.output_ports());
        }
        GameSceneHistory history(game,rom,symbols);
        const auto snapshot=history.current();const auto& ppu=*snapshot->ppu;
        std::cout<<(source_space_bg2?"Space PPU: mode=":"Title PPU: mode=")<<unsigned(ppu.background_mode)<<" main="<<unsigned(ppu.main_screen)
            <<" BG2="<<ppu.bg2_character_base<<'/'<<ppu.bg2_screen_base
            <<" BG3="<<ppu.bg3_character_base<<'/'<<ppu.bg3_screen_base
            <<" palette_nonzero="<<std::count_if(ppu.cgram.begin(),ppu.cgram.end(),[](auto x){return x!=0;})
            <<" vram_nonzero="<<std::count_if(ppu.vram.begin(),ppu.vram.end(),[](auto x){return x!=0;})<<'\n';
        require((ppu.main_screen&(source_bg2?2:4))!=0,"Source background is disabled");
        BackgroundTileOptions source_options;source_options.brightness=snapshot->display_brightness;
        if(source_space_bg2) source_options.scroll_override=snapshot->background_scroll_override;
        const unsigned source_width=wide_space_bg2?400:256;
        const int source_origin=wide_space_bg2?72:0;
        if(wide_space_bg2) {source_options.expanded_horizontal=true;source_options.horizontal_bounds={-72,328};}
        auto packet=background_tile_packet(ppu,source_bg2?BackgroundLayer::bg2:BackgroundLayer::bg3,source_options);
        packet.model={.00625F,0,0,0,0,-.00625F,0,0,0,0,1,0,-.8F,.7F,-2,1};
        auto& data=packet.geometry.texels;
        const auto faded_palette=starfox::render::apply_snes_brightness(
            starfox::render::decode_bgr555_palette(ppu.cgram),snapshot->display_brightness);
        if(source_layers) {
            const auto upper=background_tile_payload(ppu,BackgroundLayer::bg3,source_options);
            const auto start=uint32_t(data.size());data.insert(data.end(),upper.begin(),upper.end());
            const auto lower_vertices=packet.geometry.vertices;
            for(auto v:lower_vertices) {v.texture[0]=start;packet.geometry.vertices.push_back(v);}
        }
        live_packets.push_back(packet);
        starfox::render::Framebuffer expected(source_width,224);starfox::render::BackgroundRenderer decoder;
        if(source_bg2) decoder.draw_bg2(ppu,
            source_options.scroll_override?(*source_options.scroll_override)[0]:ppu.bg2_scroll_x,
            source_options.scroll_override?(*source_options.scroll_override)[1]:ppu.bg2_scroll_y,expected,
            starfox::render::TilePriorityPass::all,source_origin);
        else decoder.draw_bg3(ppu,expected);
        if(source_layers) decoder.draw_bg3(ppu,expected);
        unsigned opaque_pixels=0,colour_pixels=0;
        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<source_width;++x) {
            const auto index=expected.get(x,y);opaque_pixels+=index!=0;
            colour_pixels+=index!=0 && (data[16+index]&0xffffffU)!=0;
        }
        std::cout<<"CPU layer pixels: indexed="<<opaque_pixels<<" coloured="<<colour_pixels
            <<" scroll="<<int32_t(data[3])<<','<<int32_t(data[4])<<'\n';
        unsigned coloured=0;
        std::vector<std::array<unsigned,2>> sample_points;
        for(unsigned y=7;y<224;y+=16) for(unsigned x=7;x<source_width;x+=16) sample_points.push_back({x,y});
        if(source_space_bg2) {
            unsigned visible=0,added=0;const unsigned stride=std::max(1U,colour_pixels/128U);
            for(unsigned y=0;y<224;++y) for(unsigned x=0;x<source_width;++x) {
                const auto index=expected.get(x,y);
                if(index && (data[16+index]&0xffffffU)!=0 && visible++%stride==0 && added<128) {
                    sample_points.push_back({x,y});++added;
                }
            }
        }
        for(const auto& coordinate:sample_points) {
            const auto x=coordinate[0],y=coordinate[1];
            const auto index=expected.get(x,y);
            const auto colour=faded_palette[index];
            const auto rgb=index?(uint32_t(colour.r)|(uint32_t(colour.g)<<8)|(uint32_t(colour.b)<<16)):0U;
            tile_expected.push_back(rgb);coloured+=rgb!=0;
            auto sample=packet;
            for(auto& v:sample.geometry.vertices) {v.uv[0]=float(int(x)-source_origin);v.uv[1]=float(y);}
            tile_cases.push_back(std::move(sample));
        }
        require(coloured>0,"Source background sample grid contains no visible pixels");
        std::cout<<(source_space_bg2?"Captured space BG":"Captured title BG")<<(source_bg2?2:3)<<": "<<coloured<<" coloured samples from immutable PPU, native palette\n";
    } else if(grid_line_suite) {
        for(int dx:{-3,0,1,7}) for(int dy:{-9,0,3,9}) for(unsigned variant=0;variant<4;++variant) {
            const int step=variant==0?0:variant==3?std::max(dx,0)+1:std::max(dx,0);
            const auto expected=starfox::render::grid_line_sample({32,48},{int16_t(32-dx),int16_t(48-dy)},unsigned(step));
            int y=expected?(*expected)[1]:48;
            if(variant==2) ++y;
            DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            packet.geometry.texels={32,48,uint32_t(32-dx),uint32_t(48-dy)};
            const float corners[4][2]{{-.6F,.6F},{.6F,.6F},{.6F,-.6F},{-.6F,-.6F}};
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                SceneVertex v{};v.position[0]=corners[corner][0];v.position[1]=corners[corner][1];v.position[2]=-2;
                v.color[1]=v.color[3]=1;v.texture[3]=256;v.uv[0]=float(30-step);v.uv[1]=float(y);
                packet.geometry.vertices.push_back(v);
            }
            tile_cases.push_back(std::move(packet));
            tile_expected.push_back(expected && variant!=2?0x00ff00U:0);
        }
        live_packets.push_back(tile_cases.front());
    } else if(tile_suite) {
        for(unsigned bpp:{2U,4U,8U}) for(unsigned large:{0U,1U}) for(unsigned flip=0;flip<4;++flip)
        for(unsigned screens=0;screens<4;++screens) for(unsigned priority=0;priority<3;++priority)
        for(unsigned offset_mode=0;offset_mode<(bpp==4?3U:1U);++offset_mode) {
            starfox::simulation::SnesPpuState ppu;
            for(size_t i=0;i<ppu.vram.size();++i) ppu.vram[i]=uint8_t((i*73U+(i>>3)*19U)^0xa5U);
            for(unsigned entry=0;entry<4096;++entry) {
                const uint16_t tile=((entry*17+3)&511)|(((entry>>1)&7)<<10)|((entry&1)<<13)|(flip<<14);
                ppu.vram[0xc000+entry*2]=uint8_t(tile);ppu.vram[0xc001+entry*2]=uint8_t(tile>>8);
            }
            const int offsets[]{-1,257,511,-513};
            const int x=offsets[(screens+flip)%4],y=offsets[(priority+large)%4];
            ppu.background_mode=bpp==8?3:1;ppu.main_screen=7;
            if(offset_mode) {
                ppu.background_mode=2;ppu.bg2_vertical_offsets_enabled=true;
                for(unsigned i=0;i<32;++i) {
                    const unsigned rise=offset_mode==2?i*37/5:i*37;
                    const uint16_t value=uint16_t(((8190+rise)&8191)|((i+large)%3?0x4000:0));
                    ppu.vram[0x5f40+i*2]=uint8_t(value);
                    ppu.vram[0x5f41+i*2]=uint8_t(value>>8);
                }
            }
            ppu.bg1_character_base=ppu.bg3_character_base=0x1000;
            ppu.bg1_screen_base=ppu.bg3_screen_base=0x6000;
            ppu.bg1_screen_size=ppu.bg3_screen_size=uint8_t(screens);
            ppu.bg1_tile_size_16=ppu.bg3_tile_size_16=large!=0;
            ppu.bg1_scroll_x=ppu.bg3_scroll_x=int16_t(x);ppu.bg1_scroll_y=ppu.bg3_scroll_y=int16_t(y);
            const unsigned scanlines=bpp==4?1+screens%3:0;
            ppu.bg2_character_base=0x1000;ppu.bg2_screen_base=0x6000;
            ppu.bg2_screen_size=uint8_t(screens);ppu.bg2_tile_size_16=large!=0;
            ppu.bg2_horizontal_offsets_enabled=(scanlines&1)!=0;ppu.bg2_scanline_scroll_enabled=(scanlines&2)!=0;
            for(int row=0;row<224;++row) {
                ppu.bg2_horizontal_offsets[row]=int16_t(row*29-300);
                ppu.bg2_scanline_scroll_y[row]=int16_t(511-row*17);
            }
            const unsigned mosaic=priority==0?0:priority==1?4:16;
            ppu.mosaic=mosaic?uint8_t(((mosaic-1)<<4)|(bpp==2?4:bpp==4?2:1)):0;
            starfox::render::Framebuffer expected(offset_mode==2?384:18,10);starfox::render::BackgroundRenderer decoder;
            const auto pass=static_cast<starfox::render::TilePriorityPass>(priority);
            const int origins[]{0,20,-250,100};
            const int origin=offset_mode?origins[flip]:((flip&1)?20:0);
            if(bpp==2) decoder.draw_bg3(ppu,expected,pass,origin);
            else if(bpp==4) decoder.draw_bg2(ppu,x,y,expected,pass,origin);
            else decoder.draw_bg1(ppu,expected,pass,origin);
            const unsigned brightness=(large*48+flip*12+screens*3+priority)%16;
            tile_expected.push_back(uint32_t(expected.get(17,9)*brightness/15)*0x010101U);
            DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            auto& data=packet.geometry.texels;data.resize(272+16384+(scanlines?448:0));
            data[0]=0x1000;data[1]=0x6000;data[2]=screens;data[3]=uint32_t(x);data[4]=uint32_t(y);
            data[5]=bpp;data[7]=priority;data[8]=large;data[9]=mosaic;
            data[10]=scanlines;
            data[11]=ppu.bg2_scanline_scroll_enabled && bpp==4 && !offset_mode?1:0;
            data[12]=offset_mode;
            data[13]=15-brightness;
            if(scanlines) for(unsigned row=0;row<224;++row) {
                data[272+16384+row]=uint32_t(int32_t(ppu.bg2_horizontal_offsets[row]));
                data[272+16384+224+row]=uint32_t(int32_t(ppu.bg2_scanline_scroll_y[row]));
            }
            for(unsigned i=0;i<256;++i) data[16+i]=0xff000000U|i|(i<<8)|(i<<16);
            for(unsigned i=0;i<65536;++i) data[272+i/4]|=uint32_t(ppu.vram[i])<<((i%4)*8);
            BackgroundTileOptions encode_options;
            encode_options.priority=priority;
            encode_options.brightness=brightness;
            encode_options.expanded_horizontal=bpp==4 && offset_mode!=1;
            encode_options.scroll_override=std::array<int16_t,2>{int16_t(x),int16_t(y)};
            const auto encoded=background_tile_payload(ppu,bpp==2?BackgroundLayer::bg3:
                bpp==4?BackgroundLayer::bg2:BackgroundLayer::bg1,encode_options);
            require(encoded.size()==data.size()
                && std::equal(encoded.begin(),encoded.begin()+16,data.begin())
                && std::equal(encoded.begin()+272,encoded.end(),data.begin()+272),
                "Source background encoder differs from independent diagnostic payload");
            const float corners[4][2]{{-.6F,.6F},{.6F,.6F},{.6F,-.6F},{-.6F,-.6F}};
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                SceneVertex v{};v.position[0]=corners[corner][0];v.position[1]=corners[corner][1];v.position[2]=-2;v.texture[3]=8;
                v.uv[0]=float(17-origin);v.uv[1]=9;
                packet.geometry.vertices.push_back(v);
            }
            tile_cases.push_back(std::move(packet));
        }
        // EX unique planets: authored occurrence, repeated planet ink, and
        // cloud ink sharing the small planet's region. Include wrapped scroll.
        for(int scroll:{0,8191}) for(unsigned ink:{1U,9U}) for(int x:{288,800}) {
            starfox::simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
            ppu.bg2_character_base=0x1000;ppu.bg2_screen_base=0x6000;ppu.bg2_screen_size=3;
            ppu.bg2_scroll_x=int16_t(scroll);
            for(unsigned i=0;i<4096;++i) ppu.vram[0xc000+i*2+1]=0x14;
            for(unsigned row=0;row<8;++row) {
                ppu.vram[0x2000+row*2]=255;
                if(ink==9) ppu.vram[0x2000+17+row*2]=255;
            }
            ppu.cgram[81]=0x03e0;ppu.cgram[89]=0x001f;ppu.cgram[88]=0x7c00;
            BackgroundTileOptions options;options.expanded_horizontal=true;options.ex_twin_planets=true;
            auto packet=background_tile_packet(ppu,BackgroundLayer::bg2,options);
            packet.geometry.vertices.clear();
            const float corners[4][2]{{-.6F,.6F},{.6F,.6F},{.6F,-.6F},{-.6F,-.6F}};
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                SceneVertex v{};v.position[0]=corners[corner][0];v.position[1]=corners[corner][1];v.position[2]=-2;
                v.texture[3]=8;v.uv[0]=float(x+2);v.uv[1]=306;
                packet.geometry.vertices.push_back(v);
            }
            tile_cases.push_back(std::move(packet));
            tile_expected.push_back(ink==9?0x0000ffU:x==800?0xff0000U:0x00ff00U);
        }
        // Compare the actual title packet builder against the source compositor,
        // with a model-coloured plane underneath. All layers are depth-disabled,
        // as in the application's sprite/foreground pass, so painter order matters.
        for(unsigned enabled=0;enabled<8;++enabled) for(bool include_bg1:{false,true})
        for(unsigned priorities=0;priorities<4;++priorities) for(bool black:{false,true}) {
            starfox::simulation::SnesPpuState ppu;ppu.background_mode=1;ppu.main_screen=enabled;
            ppu.bg1_screen_base=0x4000;ppu.bg2_screen_base=0x4800;ppu.bg3_screen_base=0x5000;
            ppu.bg1_character_base=0;ppu.bg2_character_base=0x1000;ppu.bg3_character_base=0x2000;
            ppu.cgram[4]=0x03e0;ppu.cgram[17]=black?0:0x001f;
            ppu.cgram[33]=black?0:0x7c00;ppu.cgram[13]=black?0:0x7fff;
            const auto word=[&](unsigned address,unsigned value) {
                ppu.vram[address*2]=uint8_t(value);ppu.vram[address*2+1]=uint8_t(value>>8);
            };
            word(ppu.bg1_screen_base,1U<<10);
            word(ppu.bg2_screen_base,(2U<<10)|((priorities&1)?8192U:0U));
            word(ppu.bg3_screen_base,(3U<<10)|((priorities&2)?8192U:0U));
            ppu.vram[0]=255;ppu.vram[0x2000]=255;ppu.vram[0x4000]=255;
            starfox::render::Framebuffer expected(3,1);expected.clear(4);
            starfox::render::BackgroundRenderer decoder;
            decoder.draw_title_foreground(ppu,0,0,expected,0,include_bg1);
            const auto colour=starfox::render::decode_bgr555_palette(ppu.cgram)[expected.get(1,0)];
            tile_expected.push_back(uint32_t(colour.r)|(uint32_t(colour.g)<<8)|(uint32_t(colour.b)<<16));
            DrawPacket packet=tile_cases.front();packet.geometry.texels.assign(272,0);
            packet.geometry.texels[20]=0xff00ff00U;
            for(auto& vertex:packet.geometry.vertices) {
                vertex.texture[0]=0;vertex.texture[1]=4;vertex.texture[3]=32;
            }
            const auto quad=packet.geometry.vertices;
            for(const auto& layer:title_foreground_packets(ppu,15,include_bg1)) {
                if(layer.geometry.texels.empty()) continue;
                const auto offset=uint32_t(packet.geometry.texels.size());
                packet.geometry.texels.insert(packet.geometry.texels.end(),
                    layer.geometry.texels.begin(),layer.geometry.texels.end());
                for(auto vertex:quad) {
                    vertex.texture[0]=offset;vertex.texture[1]=0;vertex.texture[3]=8;
                    vertex.uv[0]=1;vertex.uv[1]=0;
                    packet.geometry.vertices.push_back(vertex);
                }
            }
            tile_cases.push_back(std::move(packet));
        }
        for(unsigned rows:{0U,168U}) for(int logical_x:{-1,0,255,256})
        for(unsigned y:{0U,167U,168U}) for(int scroll:{-1,0,1}) {
            starfox::simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
            ppu.bg2_screen_base=0x6000;ppu.bg2_character_base=0;
            ppu.cgram.fill(0x7fff);ppu.cgram[1]=0x03e0;ppu.cgram[7]=0;
            for(unsigned row=0;row<8;++row) ppu.vram[row*2]=255;
            BackgroundTileOptions options;options.single_occurrence_top_rows=rows;
            options.scroll_override=std::array<int16_t,2>{int16_t(scroll),0};
            starfox::render::Framebuffer expected(258,169);
            starfox::render::BackgroundRenderer decoder;
            decoder.draw_bg2(ppu,scroll,0,expected,starfox::render::TilePriorityPass::all,
                1,true,true,false,rows);
            const auto colour=starfox::render::decode_bgr555_palette(ppu.cgram)[expected.get(logical_x+1,y)];
            tile_expected.push_back(uint32_t(colour.r)|(uint32_t(colour.g)<<8)|(uint32_t(colour.b)<<16));
            auto packet=tile_cases.front();packet.geometry.texels.assign(272,0);
            packet.geometry.texels[18]=0xffff00ffU; // Magenta reveals accidental discard.
            for(auto& vertex:packet.geometry.vertices) {vertex.texture[0]=0;vertex.texture[1]=2;vertex.texture[3]=32;}
            const auto quad=packet.geometry.vertices;
            const auto overlay=background_tile_payload(ppu,BackgroundLayer::bg2,options);
            packet.geometry.texels.insert(packet.geometry.texels.end(),overlay.begin(),overlay.end());
            for(auto vertex:quad) {
                vertex.texture[0]=272;vertex.texture[1]=0;vertex.texture[3]=8;
                vertex.uv[0]=float(logical_x);vertex.uv[1]=float(y);
                packet.geometry.vertices.push_back(vertex);
            }
            tile_cases.push_back(std::move(packet));
        }
        for(bool wrap:{false,true}) for(unsigned large:{0U,1U}) for(unsigned wide:{0U,1U})
        for(int scroll:{-513,-1,0,255,512}) {
            starfox::simulation::SnesPpuState ppu;ppu.background_mode=1;ppu.main_screen=2;
            ppu.bg2_screen_base=0x6000;ppu.bg2_character_base=0;
            ppu.bg2_screen_size=wide;ppu.bg2_tile_size_16=large;
            ppu.cgram[1]=0x03e0;
            for(unsigned row=0;row<8;++row) ppu.vram[row*2]=255;
            BackgroundTileOptions options;options.wrap_horizontal=wrap;
            options.scroll_override=std::array<int16_t,2>{int16_t(scroll),0};
            starfox::render::Framebuffer expected(18,10);
            starfox::render::BackgroundRenderer decoder;
            decoder.draw_bg2(ppu,scroll,0,expected,starfox::render::TilePriorityPass::all,20,true,wrap);
            auto packet=tile_cases.front();packet.geometry.texels=background_tile_payload(ppu,BackgroundLayer::bg2,options);
            for(auto& vertex:packet.geometry.vertices) {vertex.uv[0]=-3;vertex.uv[1]=3;}
            tile_expected.push_back(expected.get(17,3)?0x00ff00U:0U);
            tile_cases.push_back(std::move(packet));
        }
        for(bool transparent:{false,true}) for(bool black:{false,true}) for(unsigned brightness:{0U,15U}) {
            starfox::simulation::SnesPpuState ppu;ppu.background_mode=1;ppu.main_screen=1;
            ppu.bg1_screen_base=0x6000;ppu.bg1_character_base=0;
            ppu.cgram[1]=black?0:0x7fff;ppu.cgram[2]=0x03e0;
            // Tile zero, row zero, plane zero gives nonzero palette index 1.
            ppu.vram[0]=255;
            BackgroundTileOptions options;options.transparent_black=transparent;options.brightness=brightness;
            const auto overlay=background_tile_payload(ppu,BackgroundLayer::bg1,options);
            DrawPacket packet=tile_cases.front();packet.geometry.texels.assign(272,0);
            packet.geometry.texels[18]=0xff00ff00U;
            const auto start=uint32_t(packet.geometry.texels.size());
            packet.geometry.texels.insert(packet.geometry.texels.end(),overlay.begin(),overlay.end());
            for(auto& v:packet.geometry.vertices) {v.texture[0]=0;v.texture[1]=2;v.texture[3]=32;v.position[2]=-2.1F;}
            const auto behind=packet.geometry.vertices;
            for(auto v:behind) {v.texture[0]=start;v.texture[1]=0;v.texture[3]=8;v.position[2]=-2;v.uv[0]=0;v.uv[1]=0;packet.geometry.vertices.push_back(v);}
            tile_expected.push_back(transparent && black?0x00ff00U:black || brightness==0?0U:0xffffffU);
            tile_cases.push_back(std::move(packet));
        }
        for(unsigned brightness=0;brightness<16;++brightness) {
            starfox::simulation::SnesPpuState ppu;
            ppu.background_mode=1;ppu.main_screen=2;ppu.tunnel_scene=true;
            ppu.cgram.fill(0x7fff);ppu.cgram[7]=uint16_t(1|(2<<5)|(3<<10));
            BackgroundTileOptions options;options.expanded_horizontal=true;options.brightness=brightness;
            options.priority=brightness%3;
            DrawPacket packet=tile_cases.front();
            packet.geometry.texels=background_tile_payload(ppu,BackgroundLayer::bg2,options);
            const int coordinates[]{-1,0,255,256};const int x=coordinates[brightness%4];
            for(auto& v:packet.geometry.vertices) {v.uv[0]=float(x);v.uv[1]=9;}
            starfox::render::Framebuffer expected(18,10);starfox::render::BackgroundRenderer decoder;
            decoder.draw_bg2(ppu,0,0,expected,static_cast<starfox::render::TilePriorityPass>(options.priority),17-x);
            const auto palette=starfox::render::apply_snes_brightness(starfox::render::decode_bgr555_palette(ppu.cgram),uint8_t(brightness));
            const auto index=expected.get(17,9);const auto c=palette[index];
            tile_expected.push_back(index?(uint32_t(c.r)|(uint32_t(c.g)<<8)|(uint32_t(c.b)<<16)):0);
            tile_cases.push_back(std::move(packet));
        }
        for(unsigned size_index=0;size_index<4;++size_index) for(unsigned flip=0;flip<4;++flip)
        for(unsigned bank=0;bank<2;++bank) for(unsigned gap=0;gap<4;++gap) {
            starfox::simulation::SnesPpuState ppu;ppu.background_mode=1;ppu.main_screen=16;
            ppu.object_select=uint8_t(7+(gap<<3)+((size_index>1?size_index-1:0)<<5));
            ppu.oam[2]=255;ppu.oam[3]=uint8_t((flip<<6)|14|bank);
            ppu.oam[512]=size_index?2:0;
            for(unsigned i=0;i<65536;++i) ppu.vram[i]=uint8_t((i*73U+(i>>3)*19U)^0xa5U);
            for(unsigned i=0;i<256;++i) ppu.cgram[i]=uint16_t((i*139)&0x7fff);
            const unsigned size=8U<<size_index,x=size-3,y=size-2;
            const unsigned brightness=(size_index*4+flip)%16;
            DrawPacket packet=source_sprite_packet(ppu,brightness);
            require(packet.geometry.vertices.size()==6 && packet.geometry.texels.size()==272+16384,
                "Source OAM did not assemble one sprite with one shared payload");
            for(unsigned i=0;i<6;++i) {
                auto& v=packet.geometry.vertices[i];
                std::copy_n(tile_cases.front().geometry.vertices[i].position,3,v.position);
                v.uv[0]=float(x);v.uv[1]=float(y);
            }
            starfox::render::Framebuffer expected(64,64);starfox::render::SpriteRenderer decoder;
            decoder.draw_objects(ppu,expected);
            const auto palette=starfox::render::apply_snes_brightness(starfox::render::decode_bgr555_palette(ppu.cgram),uint8_t(brightness));
            const auto index=expected.get(x,y);const auto c=palette[index];
            tile_expected.push_back(index?(uint32_t(c.r)|(uint32_t(c.g)<<8)|(uint32_t(c.b)<<16)):0);
            tile_cases.push_back(std::move(packet));
        }
        live_packets.push_back(tile_cases.front());
    } else if(tiles) {
        DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        packet.geometry.texels.resize(272+16384);
        packet.geometry.texels[1]=1024;packet.geometry.texels[5]=4;
        packet.geometry.texels[17]=0xff00ff00;
        packet.geometry.texels[272]=tiles_empty?0:128;
        const float corners[4][2]{{-.6F,.6F},{.6F,.6F},{.6F,-.6F},{-.6F,-.6F}};
        const std::array<unsigned,6> indices=grid_mode=="--tiles-tilted-alternate"
            ?std::array<unsigned,6>{0,1,3,1,2,3}:std::array<unsigned,6>{0,1,2,0,2,3};
        for(unsigned corner:indices) {
            SceneVertex v{};v.position[0]=corners[corner][0];v.position[1]=corners[corner][1];v.position[2]=-2;
            if(tilted_tiles) {
                v.position[2]+=.9F*v.position[0];
                v.uv[0]=(v.position[0]+.6F)*32.F;
                v.uv[1]=(.6F-v.position[1])*32.F;
            }
            v.texture[3]=8;packet.geometry.vertices.push_back(v);
        }
        live_packets.push_back(std::move(packet));
    } else if(startup_menu) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        StartupMenu menu;menu.language=unsigned(std::stoul(std::string(grid_mode.substr(10))));
        require(menu.language<=5,"Startup capture expects language 0..5");
        menu.alternate_available=true;menu.selection=1;
        const bool sky_options=grid_mode.ends_with(":two-d");
        if(sky_options) {menu.page=StartupMenu::Page::two_d;menu.selection=3;menu.enhanced_sky=true;}
        std::array<uint16_t,256> palette{};palette[1]=0x7fff;palette[2]=0x03ff;
        const auto add=[&](std::u32string_view text,int y,uint8_t ink) {
            auto packet=menu.language==0 || menu.language==5
                ?source_ui_text_packet(rom,symbols,std::string(text.begin(),text.end()),16,y,240,ink,palette)
                :source_unicode_ui_text_packet(rom,symbols,text,16,y,ink,palette);
            packet.model=source_layer_matrix(128,112,2.F).value();live_packets.push_back(std::move(packet));
        };
        add(menu.translate(sky_options?menu.title():"STAR FOX VR"),35,1);
        const auto labels=menu.localized_labels();
        for(unsigned i=0;i<(sky_options?labels.size():4);++i)
            add((i==menu.selection?U"> ":U"  ")+labels[i],67+int(i)*(sky_options?18:26),i==menu.selection?2:1);
        if(menu.language==0 || menu.language==5) add(U"STICK: MOVE   FIRE: SELECT",185,1);
        else {const auto help=menu.localized_help();add(help[0],183,1);add(help[1],201,1);}
    } else if(briefing_text) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        starfox::render::ScaledTextRenderer oracle(rom,symbols);
        const auto address=symbols.find("TEAMTXT").at(0);
        std::array<uint16_t,256> palette{};palette[109]=0x03e0;
        starfox::render::Framebuffer cpu(256,256);
        int y=30;
        for(size_t count:{1U,3U,7U,256U}) {
            auto packet=source_game_text_packet(rom,symbols,address,28,y,216,count,109,palette);
            packet.model=source_layer_matrix(128,112).value();
            oracle.draw_game_text(address,28,y,cpu,96,13,216,count);
            if(!briefing_reference) live_packets.push_back(std::move(packet));
            y+=40;
        }
        if(briefing_reference) {
            DrawPacket packet;packet.model=source_layer_matrix(128,112).value();
            for(unsigned y=0;y<256;++y) for(unsigned x=0;x<256;++x)
                packet.geometry.texels.push_back(cpu.get(x,y)?0xff00ff00U:0);
            const float corners[4][2]{{0,0},{256,0},{256,224},{0,224}};
            for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                SceneVertex v{};v.position[0]=v.uv[0]=corners[corner][0];
                v.position[1]=v.uv[1]=corners[corner][1];
                v.texture[1]=v.texture[2]=255;v.texture[3]=1;
                packet.geometry.vertices.push_back(v);
            }
            live_packets.push_back(std::move(packet));
        }
    } else if(projected_text) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        if(full_font) {
            const auto font=symbols.find("MSCALECHARS").at(0);
            DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            for(unsigned glyph=0;glyph<41;++glyph) {
                const auto offset=uint32_t(packet.geometry.texels.size());
                if(font_reference) {
                    for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x)
                        packet.geometry.texels.push_back(rom.read16(font+glyph*32+y*2)&(0x8000U>>x)?0xff00ff00U:0);
                } else {
                    packet.geometry.texels.push_back(0xff00ff00U);
                    for(unsigned y=0;y<16;y+=2)
                        packet.geometry.texels.push_back(uint32_t(rom.read16(font+glyph*32+y*2))
                            |(uint32_t(rom.read16(font+glyph*32+(y+1)*2))<<16));
                }
                const float corners[4][2]{{0,0},{1,0},{1,1},{0,1}};
                for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                    SceneVertex v{};v.position[0]=-1.4F+float(glyph%8)*.4F;
                    v.position[1]=1.F-float(glyph/8)*.4F;v.position[2]=-2;
                    v.billboard[0]=(corners[corner][0]-.5F)*.38F;
                    v.billboard[1]=(.5F-corners[corner][1])*.38F;
                    v.uv[0]=corners[corner][0]*16;v.uv[1]=corners[corner][1]*16;
                    v.texture[0]=offset;v.texture[1]=v.texture[2]=15;v.texture[3]=font_reference?5:1028;
                    packet.geometry.vertices.push_back(v);
                }
            }
            live_packets.push_back(std::move(packet));
        } else {
        SourceModels assembler(rom,symbols);GameSceneSnapshot scene;
        scene.display_brightness=15;
        scene.model_palette[3]=0x03e0;scene.objects.resize(1);
        auto& owner=scene.objects.front();owner.handle=1;owner.object.strategy_flags[0]=0x40;
        owner.object.colour_table=static_cast<uint16_t>(symbols.find("MSG_NINTENDO").at(0));
        owner.object.extended[21]=3;owner.source_pose.z=1024;
        if(scaled_text_close) {owner.source_pose.z=128;owner.object.texture_scroll_x=127;}
        if(scaled_text_hidden && !scaled_text_zero) owner.source_pose.z=127;
        if(scaled_text_zero) owner.object.texture_scroll_x=128;
        const auto assembled=assembler.assemble(scene);
        require(assembled.pending.empty() && assembled.packets.size()==1,"Projected text GPU assembly failed");
        live_packets=assembled.packets;
        if(scaled_text_reference && scaled_text_hidden) live_packets.clear();
        if(scaled_text_reference) for(auto& packet:live_packets) for(auto& v:packet.geometry.vertices) {
            // Previous CPU sizing formula; packed glyph decoding stays common.
            const double size=scaled_text_close?254.:127.;
            const double dimension=std::trunc(size*256./owner.source_pose.z);
            const float side=float(dimension*owner.source_pose.z/256.);
            v.billboard[0]=v.group_b[0]*side+v.billboard[0]*side;v.billboard[1]*=side;
            v.texture[3]&=~134217728U;
        }
        }
    } else if(shadows) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        SourceModels assembler(rom,symbols);GameSceneSnapshot scene;
        scene.display_brightness=15;
        scene.shadows_enabled=!shadows_off;scene.shadow_height=128;
        scene.view_matrix={32767,0,0,0,32767,0,0,0,32767};scene.model_palette[9]=0x03e0;
        scene.objects.resize(1);auto& owner=scene.objects.front();owner.handle=1;
        owner.object.shape=static_cast<uint16_t>(symbols.find("MYSHIP_4").at(0));
        owner.object.strategy_flags[0]=9;owner.source_pose.z=512;owner.source_pose.source_depth=512;
        owner.presentation.transform={0,0,512,0,0,0};
        owner.presentation.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
        scene.transforms[1]=owner.presentation;
        const auto assembled=assembler.assemble(scene);
        require(assembled.pending.empty() && assembled.packets.size()==(shadows_off?1U:2U)
            && assembled.handles[0]==(shadows_off?1U:0x10001U),
            "Source shadow GPU fixture assembly failed");
        // Isolate the source shadow so ordinary model pixels cannot pass the check.
        for(size_t i=0;i<assembled.packets.size();++i)
            if(assembled.handles[i]&0x10000U) live_packets.push_back(assembled.packets[i]);
    } else if(dust) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        SourceModels assembler(rom,symbols);GameSceneSnapshot scene;
        scene.display_brightness=15;
        scene.dots_mode=-1;scene.dust_point_count=5;
        scene.flow=starfox::simulation::GameFlowState::gameplay;
        if(controls_dust) {
            scene.flow=starfox::simulation::GameFlowState::controls_type;
            scene.source_vanishing_point={64,48};scene.dust_point_count=120;
        }
        scene.view_matrix={32767,0,0,0,32767,0,0,0,32767};
        scene.model_palette.fill(0x03e0);scene.cgram.fill(0x03e0);
        scene.dust_points[0]={-128,-64,512};scene.dust_points[1]={128,64,768};
        scene.dust_points[2]={512,-256,2048};scene.dust_points[3]={-1024,512,8192};
        scene.dust_points[4]={0,0,128};
        if(surround_dust) scene.dust_point_count=511;
        if(coloured_dust) {
            scene.dust_point_count=maximum_dust?511:64;
            for(unsigned i=0;i<16;++i) scene.model_palette[i]=uint16_t(i|((31-i)<<5)|(((i*7)&31)<<10));
            scene.view_matrix={23170,23170,0,-23170,23170,0,0,0,32767};
            for(unsigned i=0;i<scene.dust_point_count;++i) {
                const int depth=256+int(i%16)*256+128;
                scene.dust_points[i]={int16_t((int(i%8)-4)*depth/8),int16_t((int(i/8)-4)*depth/8),int16_t(depth)};
                if(maximum_dust) scene.dust_points[i]={int16_t((int((i*37)%31)-15)*depth/32),
                    int16_t((int((i*17)%29)-14)*depth/32),int16_t(depth)};
            }
        }
        if(grid) {scene.dots_mode=1;scene.camera.y=-64;}
        if(rotated_grid) scene.view_matrix={23170,23170,0,-23170,23170,0,0,0,32767};
        if(wrapped_grid) {
            scene.camera.x=32767;scene.camera.z=-32768;scene.camera.y=-257;
            scene.view_matrix={28378,0,-16384,0,32767,0,16384,0,28378};
        }
        if(gpu_grid || gpu_dust) {
            std::size_t prepared_vertices=0;
            for(bool gpu:{false,true}) {
                const auto started=std::chrono::steady_clock::now();
                for(unsigned frame=0;frame<128;++frame) {
                    auto moving=scene;moving.camera.x=static_cast<int16_t>(frame);
                    const auto prepared=grid?(gpu?assembler.assemble_grid_gpu(moving):assembler.assemble_grid(moving)):
                        (gpu?assembler.assemble_dust_gpu(moving):assembler.assemble_dust(moving));
                    prepared_vertices+=prepared.geometry.vertex_view().size();
                }
                const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count()/128;
                std::cout<<(gpu?"GPU":"CPU")<<(grid?" grid":" dust")<<" packet preparation: "<<elapsed<<" us/update (not frame FPS)\n";
            }
            require(prepared_vertices>0,"Grid preparation benchmark was empty");
        }
        auto packet=grid?(gpu_grid?assembler.assemble_grid_gpu(scene):assembler.assemble_grid(scene)):
            gpu_dust?assembler.assemble_dust_gpu(scene):assembler.assemble_dust(scene);
        if(controls_dust) {
            for(const auto& vertex:packet.geometry.vertex_view())
                require((vertex.texture[3]&16384U) && std::abs(vertex.position[0])<=48
                    && std::abs(vertex.position[1])<=36,"Controls star emission outside preview");
            auto changed=scene;changed.dust_points[0].x+=100;
            require(assembler.assemble_dust_gpu(changed).geometry.shared_vertices==packet.geometry.shared_vertices,
                "Controls emitter followed unrelated native recycling");
        }
        if(surround_grid) {
            {
                auto ground=scene;ground.background_landscape=true;ground.landscape_grid_height=-128;
                ground.background_vertical_scroll=232;
                const auto reference=assembler.assemble_world_interpolated(ground,ground,1.,false,true).packets.at(0);
                for(bool lines:{false,true}) for(int y:{-500,-128,0,500}) for(double alpha:{0.,.25,.5,.75,1.}) {
                    auto moved=ground;moved.camera.y=int16_t(y);moved.grid_lines=lines;
                    const auto p=assembler.assemble_world_interpolated(ground,moved,alpha,false,true).packets.at(0);
                    require(p.model==reference.model && p.geometry.texels==reference.geometry.texels,
                        "Vertical steering detached the outdoor grid from its fixed ground");
                }
                auto banked=ground;banked.camera.roll=8192;banked.background_vertical_scroll=296;
                const auto tilted=assembler.assemble_world_interpolated(ground,banked,.5,false,true).packets.at(0);
                const auto tilt=landscape_scroll_motion(232,296,.5,0,8192);
                const auto expected=model_eye_camera(EyeCamera{tilt,{}},reference.model)->view;
                require(tilted.model==expected,"Grid tilt diverged from background tilt");
                std::cout<<"Outdoor grid: 40 vertical steering/line/interpolation cases preserve height; shared tilt passed\n";
            }
            scene.grid_lines=grid_mode=="--grid-surround-lines";
            packet=assembler.assemble_world_interpolated(scene,scene,1.,false,true).packets.at(0);
            require(packet.geometry.vertex_view().size()==39*15*12,"Rear grid extent missing");
            for(const auto& v:packet.geometry.vertex_view())
                require(v.position[0]>=0 && v.position[0]<=14 && v.position[2]>=-24 && v.position[2]<=14,
                    "Rear grid changed native sides or forward range");
            if(scene.grid_lines) require(packet.geometry.line_vertices.size()==39*14*2,"Rear grid rows missing");
            // Turn the submitted scene around to exercise previously culled rear points.
            packet.model[0]*=-1;packet.model[10]*=-1;
        }
        if(surround_dust) {
            const auto world=assembler.assemble_world_interpolated(scene,scene,1.,false,true);
            packet=world.packets.at(1);
            require(packet.geometry.shared_vertices && (packet.geometry.shared_vertices->front().texture[3]&2048U),
                "Surround star flag missing");
            const auto cached=assembler.assemble_world_interpolated(scene,scene,1.,false,true);
            require(cached.packets.at(1).geometry.shared_vertices==packet.geometry.shared_vertices,
                "Unchanged surrounding stars rebuilt their vertices");
            auto recycled=scene;
            for(auto& point:recycled.dust_points) point={900,800,700};
            const auto stable=assembler.assemble_world_interpolated(recycled,recycled,1.,false,true);
            require(stable.packets.at(1).geometry.shared_vertices==packet.geometry.shared_vertices,
                "Native star recycling changed the VR volume");
            starfox::render::RenderPose rotation;
            if(grid_mode=="--dust-surround-rear") rotation.yaw=32768;
            else if(grid_mode=="--dust-surround-left") rotation.yaw=16384;
            else if(grid_mode=="--dust-surround-right") rotation.yaw=-16384;
            else if(grid_mode=="--dust-surround-up") rotation.pitch=16384;
            else if(grid_mode=="--dust-surround-down") rotation.pitch=-16384;
            else require(grid_mode=="--dust-surround-front","Unknown surrounding star direction");
            packet.model=game_model_matrix(rotation).value();
        }
        if(connected_grid) {
            scene.grid_lines=true;scene.source_vanishing_point={112,96};
            packet=connected_compute?assembler.assemble_connected_grid_gpu(scene):connected_binned?assembler.assemble_connected_grid_binned(scene):assembler.assemble_connected_grid(scene);
            if(connected_compute) require(packet.geometry.texels.size()==14 && packet.geometry.vertex_view().size()==6,
                "Compute grid must upload raw source words, not projected points or row lists");
            if(connected_compute) connected_expected=assembler.assemble_connected_grid_binned(scene);
            if(connected_binned) {
                size_t words=0,row_candidates=0,max_row_candidates=0;
                const auto started=std::chrono::steady_clock::now();
                for(unsigned frame=0;frame<256;++frame) {
                    auto moving=scene;moving.camera.x=int16_t(frame);
                    const auto prepared=assembler.assemble_connected_grid_binned(moving);
                    require(prepared.geometry.vertex_view().size()==6,"Connected-grid benchmark lost its source quad");
                    words+=prepared.geometry.texels.size();
                    for(unsigned row=0;row<192;++row) {
                        const auto count=size_t(prepared.geometry.texels[row*2+1]);
                        row_candidates+=count;max_row_candidates=std::max(max_row_candidates,count);
                    }
                }
                const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count()/256;
                std::cout<<"Connected-grid preparation: "<<elapsed<<" us/update, "<<words*4/256
                    <<" payload bytes/update, "<<double(row_candidates)/(256*192)<<" candidates/row, max "
                    <<max_row_candidates<<" (CPU preparation only; not GPU time or frame FPS)\n";
            }
            if(connected_common) {
                // Isolate interpolation across individual line bounds from
                // the line sampler itself. Diagnostic only: excessive overdraw.
                const unsigned corners[6][2]{{0,0},{224,0},{224,192},{0,0},{224,192},{0,192}};
                for(size_t i=0;i<packet.geometry.vertices.size();++i) {
                    auto& v=packet.geometry.vertices[i];
                    if(v.texture[3]!=256U) continue;
                    v.position[0]=float(corners[i%6][0]+16);v.position[1]=float(corners[i%6][1]+16);
                    v.uv[0]=float(corners[i%6][0]);v.uv[1]=float(corners[i%6][1]);
                }
            }
            if(connected_reference) {
                starfox::render::Framebuffer reference{224,192};
                starfox::render::DustRenderer raster(rom,symbols);
                raster.draw_grid_lines(starfox::timing::interpolate(scene.camera,scene.camera,1.),scene.view_matrix,0,reference);
                packet.geometry={};
                if(connected_texture) {
                    // Independent native framebuffer reference sampled as one
                    // image, rather than separately rasterized pixel quads.
                    packet.geometry.texels.resize(256*256);
                    for(unsigned y=0;y<192;++y) for(unsigned x=0;x<224;++x)
                        if(reference.get(x,y)) packet.geometry.texels[y*256+x]=0xff00ff00U;
                    const unsigned corners[4][2]{{0,0},{224,0},{224,192},{0,192}};
                    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                        SceneVertex v{};v.position[0]=float(corners[corner][0]+16);v.position[1]=float(corners[corner][1]+16);
                        v.uv[0]=float(corners[corner][0]);v.uv[1]=float(corners[corner][1]);
                        v.texture[1]=v.texture[2]=255;v.texture[3]=1;
                        packet.geometry.vertices.push_back(v);
                    }
                } else
                for(unsigned y=0;y<192;++y) for(unsigned x=0;x<224;++x) if(reference.get(x,y)) {
                    const float corners[4][2]{{0,0},{1,0},{1,1},{0,1}};
                    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                        SceneVertex v{};v.position[0]=float(x+16)+corners[corner][0];v.position[1]=float(y+16)+corners[corner][1];
                        v.color[1]=v.color[3]=1;packet.geometry.vertices.push_back(v);
                    }
                }
            }
        }
        if(moving_dust) {
            if(wrapped_dust) {
                scene.camera.x=32760;
                for(auto& point:scene.dust_points) point.x=starfox::simulation::wrap16(int32_t(point.x)+32760);
            }
            const auto previous=scene;scene.camera.x=wrapped_dust?32764:13;scene.camera.y=-7;scene.camera.z=3;
            packet=assembler.assemble_dust_interpolated(previous,scene,.375,false,256,gpu_dust);
        }
        require(connected_grid || ((controls_dust || surround_grid || surround_dust || coloured_dust || rotated_grid || wrapped_grid || packet.geometry.vertex_view().size()==(gpu_grid?2700U:grid?720U:gpu_dust?60U:36U))
            && packet.geometry.texels.size()==(gpu_grid?12U:gpu_dust?268U:0U)),"Dust/grid billboard fixture mismatch");
        live_packets.push_back(std::move(packet));
    } else if(particles) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        SourceModels assembler(rom,symbols);GameSceneSnapshot scene;
        scene.display_brightness=15;
        scene.objects.resize(1);auto& owner=scene.objects.front();owner.handle=1;
        owner.object.strategy_flags[0]=0x10;owner.source_pose.z=512;
        scene.model_palette[1]=0x03e0;scene.model_palette[2]=0x001f;scene.model_palette[3]=0x7c00;
        for(unsigned i=0;i<4;++i) {
            auto& dot=scene.particles[i];dot.owner=1;dot.life=10;dot.colour=1;
            dot.previous_x=int16_t(-96+int(i)*64);dot.x=dot.previous_x+16;dot.y=dot.previous_y=-64;
        }
        auto& trail=scene.particles[4];trail.owner=1;trail.life=10;trail.colour=2;trail.flags=4;
        trail.previous_x=-96;trail.x=96;trail.previous_y=trail.y=64;
        scene.particles[5]=trail;scene.particles[5].owner=2;scene.particles[5].colour=3;
        scene.particles[6]=trail;scene.particles[6].life=0;scene.particles[6].colour=3;
        scene.particles[7]=scene.particles[0];scene.particles[7].colour=3;
        scene.particles[7].previous_x=32760;scene.particles[7].x=-32760; // Must not interpolate through screen centre.
        scene.particles[8]=scene.particles[0];scene.particles[8].colour=3;
        scene.particles[8].previous_z=scene.particles[8].z=-257; // Source near-plane rejection.
        scene.particles[9]=trail;scene.particles[9].colour=3;scene.particles[9].previous_z=-300;
        scene.particles[10]=scene.particles[0];scene.particles[10].previous_z=-300;scene.particles[10].z=0;
        constexpr double particle_alpha=.5;
        const auto assembled=assembler.assemble(scene);
        require(assembled.pending.empty() && assembled.packets.size()==1,"Particle GPU fixture assembly failed");
        live_packets=assembled.packets;
        for(auto& packet:live_packets) {
            for(auto& vertex:packet.geometry.vertices) vertex.group_c[0]=float(particle_alpha);
            for(auto& vertex:packet.geometry.line_vertices) vertex.group_c[0]=float(particle_alpha);
        }
        if(particle_reference) {
            // Independent implementation of the previous CPU producer, used
            // only as a pixel reference for the raw-endpoint GPU path.
            auto& geometry=live_packets.front().geometry;
            geometry.vertices.clear();geometry.line_vertices.clear();
            const auto interpolate=[particle_alpha](int16_t previous,int16_t current) {
                int delta=int(current)-previous;
                if(delta>32767) delta-=65536;else if(delta<-32768) delta+=65536;
                return float(previous+delta*particle_alpha);
            };
            for(const auto& p:scene.particles) {
                if(!p.life || p.owner!=1) continue;
                SceneVertex vertex{};
                vertex.position[0]=interpolate(p.previous_x,p.x);
                vertex.position[1]=interpolate(p.previous_y,p.y);
                vertex.position[2]=interpolate(p.previous_z,p.z);
                const double depth=512.+vertex.position[2];
                if(depth<256) continue;
                vertex.color[0]=p.colour==2?1.F:0.F;vertex.color[1]=p.colour==1?1.F:0.F;
                vertex.color[2]=p.colour==3?1.F:0.F;vertex.color[3]=1;
                std::copy_n(vertex.color,4,vertex.odd_color);
                if(p.flags&4U) {
                    if(512.+p.previous_z<256) continue;
                    auto previous=vertex;previous.position[0]=p.previous_x;previous.position[1]=p.previous_y;previous.position[2]=p.previous_z;
                    geometry.line_vertices.push_back(previous);geometry.line_vertices.push_back(vertex);
                } else {
                    constexpr float corners[4][2]{{0,0},{1,0},{1,-1},{0,-1}};
                    vertex.texture[3]=4;
                    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                        auto point=vertex;point.billboard[0]=corners[corner][0]*float(depth/128.);
                        point.billboard[1]=corners[corner][1]*float(depth/128.);geometry.vertices.push_back(point);
                    }
                }
            }
        }
    } else if(live) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        const auto stage_argument=explicit_stage?std::string_view(argv[4]).substr(compute_stage?21:13):std::string_view{};
        // --live-stage=LEVEL2_3:1200@rear selects elapsed stage ticks before
        // applying the view direction; omitted timing retains entry fixtures.
        const auto stage_end=stage_argument.find_first_of(":@");
        const std::string selected_stage(stage_argument.substr(0,stage_end));
        std::optional<unsigned> explicit_ticks;
        bool titania_water=false;
        bool scramble_shutter=false;
        bool ex_menu_capture=false;
        unsigned ex_menu_delay=0;
        std::optional<unsigned> ex_menu_choice;
        std::optional<unsigned> ex_menu_page;
        if(explicit_stage && stage_end!=std::string_view::npos && stage_argument[stage_end]==':') {
            const auto token=stage_argument.substr(stage_end+1,stage_argument.find('@',stage_end)-stage_end-1);
            if(token=="menu" || token.starts_with("menu-")) {
                require(selected_stage=="TITLEMAP","Menu fixture requires TITLEMAP");
                if(token.starts_with("menu-page-")) {
                    const auto page=token.substr(10);
                    require(page=="1" || page=="2" || page=="3","Native menu page must be 1..3");
                    ex_menu_page=unsigned(page.front()-'0');
                } else if(token.starts_with("menu-choice-")) {
                    const auto choice=token.substr(12);
                    require(!choice.empty() && choice.find_first_not_of("0123456789")==std::string_view::npos,
                        "Invalid native menu background choice");
                    const auto value=std::stoul(std::string(choice));
                    require(value<=36 || value==99,"Native menu background choice must be 0..36 or 99");
                    ex_menu_choice=unsigned(value);
                } else if(token!="menu") {
                    const auto delay=token.substr(5);
                    require(!delay.empty() && delay.find_first_not_of("0123456789")==std::string_view::npos,
                        "Invalid menu entry delay");
                    const auto value=std::stoul(std::string(delay));
                    require(value<=2000,"Menu entry delay exceeds 2000 ticks");
                    ex_menu_delay=unsigned(value);
                }
                ex_menu_capture=true;explicit_ticks=40;
            } else if(token=="water") {
                require(selected_stage=="LEVEL2_3","Water fixture requires LEVEL2_3");
                titania_water=true;explicit_ticks=200;
            } else if(token=="scramble") {
                scramble_shutter=true;explicit_ticks=10;
            } else {
            require(!token.empty() && token.find_first_not_of("0123456789")==std::string_view::npos,
                "Invalid stage capture tick count");
            const auto ticks=std::stoul(std::string(token));
            require(ticks<=10000,"Stage capture tick count exceeds 10000");
            explicit_ticks=unsigned(ticks);
            }
        }
        const char* stage=explicit_stage?selected_stage.c_str():controls_from_title?"TITLEMAP":controls_scene?"CONTMAP":intro_scene?"INTROMAP":training_scene?"TRAININGMAP":space_scene?"LEVEL1_2":"LEVEL1_1";
        require(*stage!='\0' && (std::string_view(stage)=="CONTINUE"
            || std::string_view(stage)=="GAMEOVER" || !symbols.find(stage).empty()),
            "Live capture stage is missing from cartridge symbols");
        starfox::simulation::GameSimulation game(rom,symbols,stage,{},true);
        game.set_experience(game.peek_meter_state().extended?starfox::simulation::Experience::starfox_ex
            :starfox::simulation::Experience::original);
        std::cout<<"Capture cartridge: "<<(game.peek_meter_state().extended?"EX":"ORIGINAL")<<'\n';
        if(explicit_ticks) game.set_god_mode(true); // Bounded environment audit, not a death-flow fixture.
        if(explicit_stage && stage_argument.find("@scored-overlay")!=std::string_view::npos) {
            const auto& scored=symbols.find("SCORED");
            require(game.peek_meter_state().extended && !scored.empty(),"Scored overlay fixture requires EX SCORED");
            game.map().write_native_byte(scored.front(),1);
        }
        game.set_timing_mode(starfox::simulation::TimingMode::unlocked_20_fps);
        starfox::audio::Spc700Audio audio;
        starfox::input::InputLatch capture_input;
        const auto advance=[&] {
            const auto tick=game.tick(capture_input.consume());static_cast<void>(audio.render_logic_tick(tick.audio_port_writes));
            game.synchronize_apu_output_ports(audio.output_ports());
        };
        if(ex_menu_capture) {
            require(game.peek_meter_state().extended,"Native menu fixture requires EX");
            unsigned steps=0;
            for(;steps<3000 && game.flow_state()!=starfox::simulation::GameFlowState::ex_pregame_menu;++steps) {
                capture_input.sample(steps>=ex_menu_delay && steps%6==5?starfox::input::start:0);
                advance();
            }
            require(steps<3000,"EX title did not reach native pregame menu");
            capture_input.sample(0);
            if(ex_menu_page) {
                for(unsigned settle=0;settle<40;++settle) advance();
                const auto page_address=symbols.find("PAGENUMBER").at(0);
                for(unsigned change=0;change<3 && game.map().read_native_byte(page_address)!=*ex_menu_page;++change) {
                    capture_input.sample(starfox::input::right_shoulder);advance();
                    capture_input.sample(0);
                    for(unsigned settle=0;settle<20;++settle) advance();
                }
                require(game.map().read_native_byte(page_address)==*ex_menu_page,
                    "Native shoulder input did not reach requested menu page");
            }
            if(ex_menu_choice) {
                // Seed only the menu cursor/page. The cartridge's own Right
                // handler cycles PGBG and reloads graphics/palettes normally.
                for(unsigned settle=0;settle<40;++settle) advance();
                game.map().write_native_byte(symbols.find("STOPCOUNTING").at(0),9);
                game.map().write_native_byte(symbols.find("PAGENUMBER").at(0),2);
                game.map().write_native_byte(symbols.find("MENUSELECTED").at(0),15);
                const auto choice_address=symbols.find("PGBG").at(0);
                unsigned changes=0;
                while(game.map().peek_ram_byte(choice_address).value()!=*ex_menu_choice && changes<38) {
                    capture_input.sample(starfox::input::right);advance();
                    capture_input.sample(0);
                    for(unsigned settle=0;settle<8;++settle) advance();
                    ++changes;
                }
                require(game.map().peek_ram_byte(choice_address).value()==*ex_menu_choice,
                    "Native menu background cycling did not reach requested choice");
            }
            std::cout<<"EX pregame menu reached after "<<steps<<" ticks; PGBG="
                <<unsigned(game.map().read_native_byte(symbols.find("PGBG").at(0)))<<'\n';
        }
        if(controls_from_title) {
            using Flow=starfox::simulation::GameFlowState;
            unsigned steps=0;
            for(;steps<3000 && game.flow_state()!=Flow::controls_type;++steps) {
                starfox::input::ButtonMask buttons=0;
                if(steps%6==5) {
                    if(game.flow_state()==Flow::title) buttons=starfox::input::start;
                    else if(game.flow_state()==Flow::ex_pregame_menu) {
                        const auto selected=game.map().peek_ram_byte(symbols.find("MENUSELECTED").at(0)).value();
                        buttons=selected==15?starfox::input::start:starfox::input::down;
                    }
                }
                capture_input.sample(buttons);advance();
            }
            require(game.flow_state()==Flow::controls_type,"Title/menu route did not reach controls");
            capture_input.sample(0);
            std::cout<<"Controls reached through title/menu after "<<steps<<" ticks\n";
        }
        const auto checkpoint=symbols.find("MAPRESTART").at(0);
        unsigned warmup=0;
        while(!ex_menu_capture && !controls_scene && !intro_scene && !training_scene
            && stage_argument.find("@from-entry")==std::string_view::npos
            && !game.map().peek_ram_word(checkpoint).value() && warmup<3000) {advance();++warmup;}
        require(warmup<3000,"Live scene checkpoint did not start");
        if(const auto at=stage_argument.find("@continue-map=");at!=std::string_view::npos) {
            const auto tail=stage_argument.substr(at+14);
            const auto label=std::string(tail.substr(0,tail.find('@')));
            const auto entry=symbols.find(label);
            require(!entry.empty(),"Continuation map symbol not found");
            // Start an internal authored route only after a real level has
            // initialized the player, palette and display state. Directly
            // booting a FINAL_* fragment skips the level's setup contract.
            game.map().start(entry.front(),game.player());
            std::cout<<"Authored map continuation: "<<selected_stage<<" -> "<<label
                <<" after "<<warmup<<" bootstrap ticks\n";
        }
        if(scramble_shutter) game.map().write_native_word(symbols.find("CIRCLEANIM").at(0),
            static_cast<uint16_t>(symbols.find("MSCRAMWIPE_CIRCLE").at(0)));
        if(titania_water) {
            const auto entry=symbols.find("LEVEL2_3").at(0);
            const auto background=static_cast<uint16_t>(symbols.find("BG_2_3B").at(0)-symbols.find("BGLISTS").at(0));
            auto limit=(entry&0xff0000U)+0xfffeU;
            for(const auto& [name,values]:symbols.entries()) if(name.starts_with("LEVEL"))
                for(const auto value:values) if(value>entry && value<limit) limit=value;
            uint32_t command=0;
            for(auto pc=entry;pc+2U<limit;++pc)
                if(rom.read8(pc)==16U && rom.read16(pc+1U)==background) {command=pc;break;}
            require(command!=0,"Titania authored water command not found");
            game.map().start(command,game.player());
            game.map().advance_distance(1);game.map().advance_distance(30000);
        }
        unsigned scene_ticks=intro_scene || training_scene?400U:40U;
        if(explicit_ticks) scene_ticks=*explicit_ticks;
        if(intro_background && grid_mode.find(':')!=std::string_view::npos)
            scene_ticks=unsigned(std::stoul(std::string(grid_mode.substr(grid_mode.find(':')+1))));
        for(unsigned i=0;i<scene_ticks;++i) {
            if(explicit_stage) {
                const auto held=stage_argument.find("@weather")!=std::string_view::npos
                    ?(i>=2600 && i<2620?starfox::input::right:i==2620?starfox::input::down:0)
                    :stage_argument.find("@steer-up")!=std::string_view::npos?starfox::input::up
                    :stage_argument.find("@steer-down")!=std::string_view::npos?starfox::input::down
                    :stage_argument.find("@steer-left")!=std::string_view::npos?starfox::input::left
                    :stage_argument.find("@steer-right")!=std::string_view::npos?starfox::input::right:0;
                capture_input.sample(static_cast<starfox::input::ButtonMask>(held));
            }
            advance();
            if(explicit_stage && stage_argument.find("@first-showcase")!=std::string_view::npos) {
                bool found=false;
                for(const auto handle:game.objects().active_handles()) {
                    const auto strategy=game.objects().at(handle).strategy_address;
                    for(const auto* name:{"ZACOINTRO_ISTRAT","ZACO2INTRO_ISTRAT","ZACOINTRO_STRAT","ZACO2INTRO_STRAT"}) {
                        const auto& choices=symbols.find(name);
                        found|=strategy!=0 && std::find(choices.begin(),choices.end(),strategy)!=choices.end();
                    }
                }
                if(found) {scene_ticks=i+1;break;}
                require(i+1<scene_ticks,"No EX model showcase reached within capture tick limit");
            }
            if(explicit_stage && stage_argument.find("@first-dialogue")!=std::string_view::npos) {
                const auto dialogue=game.dialogue_state();
                if(dialogue.active && dialogue.text_visible) {scene_ticks=i+1;break;}
                require(i+1<scene_ticks,"No active visible dialogue reached within capture tick limit");
            }
            if(intro_scene && i%40==39) {
                const auto& p=game.map().ppu_state();
                std::cout<<"Intro motion tick "<<i+1<<" scroll "<<p.bg2_scroll_y
                    <<" effective "<<game.map().peek_ram_word(symbols.find("BG2SCROLL").at(0)).value()
                    <<" scanline "<<p.bg2_scanline_scroll_y[112]
                    <<" pitch "<<game.map().peek_ram_word(symbols.find("VIEWROTXW").at(0)).value()
                    <<" vertical "<<p.bg2_vertical_offsets_enabled<<"\n";
                std::cout<<"  background "<<game.map().background()<<" mode "<<unsigned(p.background_mode)<<'\n';
            }
        }
        if(live_connected) {
            game.map().write_native_word(symbols.find("M_MOREDOTS").at(0),1);
            game.map().write_native_word(symbols.find("M_GRIDLINES").at(0),1);
        }
        if(explicit_stage && stage_argument.find("@native-pause")!=std::string_view::npos) {
            capture_input.sample(starfox::input::start);advance();
            capture_input.sample(0);
            for(unsigned settle=0;settle<8;++settle) advance();
            require(game.paused(),"Native paused scene fixture did not pause");
            std::cout<<"Native paused background="<<game.map().background()<<" scroll="
                <<game.map().ppu_state().bg2_scroll_y<<'\n';
        }
        if(explicit_stage && stage_argument.find("@expose")!=std::string_view::npos) {
            std::cout<<"Diagnostic exposure override: native brightness="<<unsigned(game.map().display_brightness())<<" -> 15\n";
            game.map().set_display_brightness(15);
        }
        GameSceneHistory history(game,rom,symbols);
        const auto bg_lists=symbols.find("BGLISTS");
        for(const auto label:{"BG_3_7C","BG_6_6C","BG_6_6D","BG_6_6E"}) {
            const auto vortex=symbols.find(label);
            if(!vortex.empty() && !bg_lists.empty() && history.current()->ppu->background_mode==2
                && history.current()->background_id==uint16_t(vortex.front()-bg_lists.front())) {
                require(!history.current()->ppu->tunnel_scene && history.current()->background_star_sphere,
                    "Final vortex room retained its corridor mask");
                std::cout<<"Full-surround final room: "<<label<<", native brightness "
                    <<unsigned(game.map().display_brightness())<<'\n';
            }
        }
        if(controls_scene) {
            const auto& controls=*history.current();
            std::cout<<"Controls model palette (working/CGRAM):";
            for(size_t i=0;i<16;++i) std::cout<<' '<<controls.model_palette[i]<<'/'<<controls.cgram[112+i];
            std::cout<<'\n';
        }
        SourceModels assembler(rom,symbols,true,live_compute,
            stage_argument.find("@cpu-shadows")==std::string_view::npos);
        if(scramble_shutter) {
            live_shutter=source_shutter_packet(history.current()->wipe,history.current()->wipe,1);
            require(!live_shutter.geometry.vertices.empty(),"Live source scramble shutter not reached");
        }
        if(cache_fixture) {
            SourceModels cached(rom,symbols),reference(rom,symbols,false);
            const auto first=cached.assemble(*history.current());
            const auto repeated=cached.assemble(*history.current());
            std::size_t shared_line_count=0;
            require(first.handles==repeated.handles,"Repeated cache membership changed");
            for(std::size_t i=0;i<first.packets.size();++i) {
                const auto& lines=first.packets[i].geometry.shared_line_vertices;
                if(lines && !lines->empty()) {
                    require(lines==repeated.packets[i].geometry.shared_line_vertices,"Cached lines copied instead of shared");
                    shared_line_count+=lines->size();
                }
            }
            std::cout<<"Immutable cached line vertices reused: "<<shared_line_count<<'\n';
            const auto compare=[&](const GameSceneSnapshot& state,unsigned sample) {
                const auto a=cached.assemble(state),b=reference.assemble(state);
                require(a.handles==b.handles && a.pending.size()==b.pending.size(),"Model cache changed packet membership");
                for(std::size_t i=0;i<a.pending.size();++i)
                    require(a.pending[i].handle==b.pending[i].handle && a.pending[i].reason==b.pending[i].reason,
                        "Model cache suppressed a rejected effect");
                if(!same_draw_geometry(a.packets,b.packets)) throw std::runtime_error("Model cache geometry mismatch at sample "+std::to_string(sample));
                for(std::size_t i=0;i<a.packets.size();++i)
                    require(a.packets[i].model==b.packets[i].model && a.packets[i].shading.light==b.packets[i].shading.light
                        && a.packets[i].shading.depth_band==b.packets[i].shading.depth_band,"Model cache changed transforms/shading");
            };
            for(unsigned sample=0;sample<64;++sample) {
                auto state=*history.current();
                for(auto& item:state.objects) {
                    auto& pose=item.source_pose;
                    switch(sample%16) {
                    case 0:pose.x+=sample;pose.y-=sample;break;
                    case 1:pose.colour_frame+=sample;break;
                    case 2:pose.animation_frame+=sample;break;
                    case 3:pose.texture_scroll_x+=sample;pose.texture_scroll_y-=sample;break;
                    case 4:pose.force_colour=true;pose.forced_colour=0x13;break;
                    case 5:pose.palette_override=143;break;
                    case 6:state.model_palette[2]^=0x7fff;break;
                    case 7:pose.depth_thresholds={0,1,2};break;
                    case 8:pose.has_depth_colour_tables=true;for(auto& row:pose.depth_colour_tables) row.fill(uint8_t(sample));break;
                    case 9:pose.use_source_lighting_state=true;pose.source_lighting_matrix.fill(0);break;
                    case 10:pose.scale*=.5;break;
                    case 11:pose.simple_scaled_sprite=true;pose.simple_sprite_world_size=64;break;
                    case 12:pose.explosion_progress=7;break;
                    case 13:pose.effect_clip_left=0;pose.effect_clip_right=10;break;
                    case 14:pose.cel_mode=true;break;
                    case 15:pose.colour_warp=true;break;
                    }
                }
                compare(state,sample);compare(*history.current(),sample);
            }
            std::cout<<"Model cache: 128 mutated/restored geometry, palette, animation, effect and pose comparisons passed\n";
            for(bool enabled:{false,true}) {
                SourceModels models(rom,symbols,enabled);
                (void)models.assemble_interpolated(*history.current(),*history.current(),1.);
                const auto start=std::chrono::steady_clock::now();std::size_t packets=0;
                for(unsigned i=0;i<128;++i) packets+=models.assemble_interpolated(*history.current(),*history.current(),double(i%60)/60).packets.size();
                require(packets>0,"Model cache benchmark was empty");
                std::cout<<(enabled?"Cached":"Uncached")<<" model preparation: "
                    <<std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/128
                    <<" us/frame (CPU preparation, not headset FPS)\n";
            }
        }
        if(ex_menu_capture) {
            std::cout<<"Menu page="<<unsigned(game.map().read_native_byte(symbols.find("PAGENUMBER").at(0)))
                <<" UI origin="<<history.current()->source_vanishing_point[0]<<','
                <<history.current()->source_vanishing_point[1]<<'\n';
            const auto choice=game.map().peek_ram_byte(symbols.find("PGBG").at(0)).value();
            menu_blackout_capture=choice==99;
            if(choice==18) require(history.current()->background_star_sphere
                && history.current()->background_retain_sky_scroll
                && !history.current()->background_landscape,
                "EX Dimension menu BG 18 did not select the scrolling surround");
            if(choice==4 || choice==5 || choice==11 || choice==14 || choice==16 || choice==32 || choice==33) {
                const auto& menu=*history.current();
                require(menu.background_landscape
                    && menu.landscape_atlas_origin==((choice==4 || choice==5)?320:choice==11?312:248)
                    && menu.background_landscape_unique_half==(choice==4 || choice==5 || choice==32),
                    "Native EX menu choice selected the wrong 360 landscape");
            }
            if(choice==0 || choice==1 || choice==3 || choice==7 || choice==8
                || choice==10 || choice==12 || choice==13 || choice==15) {
                const auto& menu=*history.current();
                require(menu.background_landscape
                    && menu.landscape_atlas_origin==(choice<=3?320:(choice==7 || choice==8 || choice==12)?240:248)
                    && menu.background_landscape_unique_half==(choice==1 || choice==3)
                    && menu.background_landscape_unique_right_half==(choice==10 || choice==12),
                    "Native EX menu landscape atlas policy mismatch");
            }
            if(choice==2 || choice==6 || choice==17 || choice==24) require(history.current()->background_star_sphere
                && !history.current()->background_landscape,"EX abstract menu pattern is not spherical");
            if(choice==9) require(history.current()->background_landscape
                && history.current()->landscape_atlas_origin==248
                && history.current()->background_landscape_unique_right_half
                && history.current()->background_ex_city_planets,
                "EX city menu did not separate its unique planets from the surround");
            if(choice==23 || choice==29 || choice==30 || choice==99)
                require(history.current()->background_star_sphere,"EX menu panorama remained planar");
            if(choice==19 || choice==27 || choice==28 || choice==31)
                require(history.current()->background_unique_space
                    && history.current()->background_unique_top_rows==512
                    && history.current()->background_planet_rect[2]>0,
                    "EX menu single planet was not separated from repeating stars");
            if(choice==21 || choice==25 || choice==35)
                require(history.current()->background_space_horizon && history.current()->background_orbital_planet
                    && history.current()->background_orbital_entry==(choice==25),
                    "EX menu orbital surface does not fill the lower hemisphere");
            if(choice==20 || choice==22 || choice==26 || choice==34 || choice==36)
                require(history.current()->background_landscape
                    && history.current()->landscape_atlas_origin==(choice==20?288:choice==36?328:240),
                    "EX menu horizon origin mismatch");
        }
        if(training_scene || (explicit_stage && selected_stage=="TRAININGMAP")) {
            const auto& training=*history.current();
            require(training.meters.extended
                ?training.background_star_sphere && !training.background_landscape
                :training.background_landscape,"Training selected the wrong cartridge environment");
        }
        if(explicit_stage) std::cout<<"Stage capture "<<selected_stage<<" ticks "<<scene_ticks
            <<" background "<<game.map().background()<<" mode "<<unsigned(history.current()->ppu->background_mode)<<'\n';
        if(explicit_stage && scene_ticks==400 && stage_argument.find("@from-entry")!=std::string_view::npos
            && game.experience()==starfox::simulation::Experience::original) {
            if(selected_stage=="LEVEL2_3" || selected_stage=="LEVEL3_3")
                require(history.current()->dots_mode==-1,
                    "Titania/Fortuna entry must preserve native airborne particles, not forced ground dots");
            if(selected_stage=="LEVEL3_5")
                require(history.current()->dots_mode==1,"Macbeth entry lost native ground dots");
        }
        if(titania_water) require(history.current()->ppu->background_mode==1,"Titania water fixture did not reach Mode 1");
        if(titania_water)
            require(history.current()->background_water_surround,"Titania water surround not selected");
        if(explicit_stage && !explicit_ticks && selected_stage=="LEVEL2_3"
            && game.experience()==starfox::simulation::Experience::original)
            require(history.current()->background_landscape,"Titania outdoor entry did not select surrounding landscape");
        if(explicit_stage && !explicit_ticks && (selected_stage=="LEVEL2_4" || selected_stage=="LEVEL1_2")
            && game.experience()==starfox::simulation::Experience::original)
            require(history.current()->background_star_sphere,"Stars-only stage entry did not select surrounding starfield");
        if(explicit_stage && !explicit_ticks && selected_stage=="LEVEL2_2"
            && game.experience()==starfox::simulation::Experience::original)
            require(history.current()->background_space_horizon,"Sector X did not select the orbital horizon mapping");
        if(explicit_stage && selected_stage=="LEVEL2_2" && scene_ticks==400
            && game.experience()==starfox::simulation::Experience::original)
            require(history.current()->background_orbital_planet && history.current()->background_orbital_entry,
                "Sector X surface must use its authored 424-row planet band");
        if(explicit_stage && selected_stage=="LEVEL1_4"
            && game.experience()==starfox::simulation::Experience::original
            && history.current()->ppu->background_mode==2) {
            require(history.current()->background_landscape
                && history.current()->background_landscape_unique_half
                && history.current()->landscape_atlas_origin==232,
                "Original 1-4 must wrap only its planet-free atlas half");
        }
        if(intro_scene) std::cout<<"Intro flow "<<unsigned(history.current()->flow)<<" background "<<game.map().background()<<" unique "<<history.current()->background_unique_top_rows<<'\n';
        if(explicit_stage && scene_ticks==2200
            && stage_argument.find("@from-entry")!=std::string_view::npos
            && game.experience()==starfox::simulation::Experience::starfox_ex
            && (selected_stage=="LEVEL5_1" || selected_stage=="LEVEL6_1" || selected_stage=="LEVEL7_1"))
            require(history.current()->background_orbital_planet && history.current()->background_orbital_thin
                && !history.current()->background_orbital_entry && !history.current()->background_landscape,
                "EX boss horizon must select the thin planet hemisphere, not a landscape or flat panel");
        if(explicit_stage && scene_ticks==60 && game.experience()==starfox::simulation::Experience::starfox_ex) {
            if(selected_stage=="LEVEL1_2" || selected_stage=="LEVEL2_4"
                || selected_stage=="LEVEL5_3" || selected_stage=="LEVEL6_3")
                require(history.current()->background_star_sphere,"Verified EX star atlas must use surrounding stars");
            const bool scramble_entry=stage_argument.find("@from-entry")!=std::string_view::npos
                && (selected_stage=="LEVEL5_1" || selected_stage=="LEVEL6_1" || selected_stage=="LEVEL7_1");
            if(scramble_entry)
                require(history.current()->background_orbital_planet && history.current()->background_orbital_entry,
                    "EX scramble entry must retain its bottom planet hemisphere");
            if(!scramble_entry && (selected_stage=="LEVEL7_1" || selected_stage=="LEVEL7_2"
                || selected_stage=="LEVEL7_3" || selected_stage=="LEVEL7_4"))
                require(history.current()->background_landscape,"Verified EX landscape must surround the viewer");
            if(selected_stage=="LEVEL5_4")
                require(history.current()->background_landscape && history.current()->background_ex_twin_planets,
                    "EX twin-planet surround must preserve unique planet ink");
        }
        if(intro_background || explicit_stage) {
            std::cout<<"Background scroll: register "<<history.current()->ppu->bg2_scroll_y
                <<", source vertical "<<history.current()->background_vertical_scroll<<'\n';
            auto ppu=*history.current()->ppu;
            std::cout<<"BG1 state base "<<ppu.bg1_character_base<<" map "<<ppu.bg1_screen_base
                <<" scroll "<<ppu.bg1_scroll_x<<','<<ppu.bg1_scroll_y<<'\n';
            for(const auto name:{"M_CLRBITMAPS","VMAP1","VMAP2"}) {
                const auto& address=symbols.find(name);
                if(!address.empty()) std::cout<<name<<' '<<game.map().read_native_word(address.front())<<'\n';
            }
            const auto& bitmap_address=symbols.find("BITMAP1");
            if(!bitmap_address.empty()) {
                unsigned ram_nonzero=0,vram_nonzero=0,mismatches=0;
                const auto base=0x700000U|(bitmap_address.front()&65535U);
                for(unsigned i=0;i<21504;++i) {
                    const auto value=game.map().read_native_byte(base+i);
                    const auto uploaded=ppu.vram[(unsigned(ppu.bg1_character_base)*2+i)&65535U];
                    ram_nonzero+=value!=0;vram_nonzero+=uploaded!=0;mismatches+=value!=uploaded;
                }
                std::cout<<"Bitmap data RAM nonzero "<<ram_nonzero<<" VRAM nonzero "<<vram_nonzero<<" mismatch "<<mismatches<<'\n';
                if(explicit_stage && stage_argument.find("@scored-overlay")!=std::string_view::npos)
                    require(ram_nonzero>0 && mismatches==0,"Native scored overlay was lost or not transferred");
            }
            ppu.bg2_horizontal_offsets_enabled=false;ppu.bg2_scanline_scroll_enabled=false;
            ppu.bg2_vertical_offsets_enabled=false;
            const unsigned atlas_width=((ppu.bg2_screen_size&1)?64U:32U)*(ppu.bg2_tile_size_16?16U:8U);
            starfox::render::Framebuffer decoded(atlas_width,512);
            starfox::render::BackgroundRenderer decoder;
            decoder.draw_bg2(ppu,0,0,decoded,starfox::render::TilePriorityPass::all,0,true);
            const auto palette=starfox::render::decode_bgr555_palette(ppu.cgram);
            if(stage_argument.find("@landmark-audit")!=std::string_view::npos) {
                unsigned audit_x=0,audit_y=320,audit_width=atlas_width,audit_height=40;
                if(const auto at=stage_argument.find("@ink-rect=");at!=std::string_view::npos) {
                    auto text=std::string(stage_argument.substr(at+10));
                    text.resize(text.find('@')==std::string::npos?text.size():text.find('@'));
                    std::replace(text.begin(),text.end(),',',' ');
                    std::istringstream fields(text);
                    require(bool(fields>>audit_x>>audit_y>>audit_width>>audit_height)
                        && audit_width>0 && audit_height>0 && audit_x<atlas_width && audit_y<512
                        && audit_width<=atlas_width-audit_x && audit_height<=512-audit_y,
                        "Invalid atlas ink audit rectangle");
                }
                std::array<unsigned,256> counts{},left{},right{};left.fill(atlas_width);
                for(unsigned y=audit_y;y<audit_y+audit_height;++y)
                    for(unsigned x=audit_x;x<audit_x+audit_width;++x) {
                    const unsigned ink=decoded.get(x,y);++counts[ink];
                    left[ink]=std::min(left[ink],x);right[ink]=std::max(right[ink],x);
                }
                for(unsigned ink=0;ink<256;++ink) if(counts[ink]) {
                    const auto c=palette[ink];
                    std::cout<<"Landmark ink "<<ink<<" count "<<counts[ink]<<" x "<<left[ink]<<' '<<right[ink]
                        <<" rgb "<<unsigned(c.r)<<' '<<unsigned(c.g)<<' '<<unsigned(c.b)<<'\n';
                }
            }
            std::vector<unsigned char> pixels(atlas_width*512*4);
            for(unsigned y=0;y<512;++y) for(unsigned x=0;x<atlas_width;++x) {
                const auto colour=palette[decoded.get(x,y)];const auto i=(y*atlas_width+x)*4;
                pixels[i]=colour.r;pixels[i+1]=colour.g;pixels[i+2]=colour.b;pixels[i+3]=255;
            }
            std::filesystem::create_directories(argv[1]);
            bitmap(std::filesystem::path(argv[1])/"authored-bg2.bmp",pixels,atlas_width,512);
            if(stage_argument.find("@terminal-ground-check")!=std::string_view::npos) {
                require(history.current()->background_landscape,"Terminal ground check needs a landscape");
                const auto index=decoded.get(0,511);
                for(unsigned x=1;x<atlas_width;++x)
                    require(decoded.get(x,511)==index,"Terminal ground fixture is not a solid row");
                const auto colour=starfox::render::apply_snes_brightness(palette,history.current()->display_brightness)[index];
                terminal_ground_colour=std::array<unsigned char,3>{colour.r,colour.g,colour.b};
            }
            if(stage_argument.find("@atlas-check")!=std::string_view::npos) {
                BackgroundTileOptions atlas_options;
                atlas_options.scroll_override=std::array<int16_t,2>{0,0};
                auto sample=background_tile_packet(ppu,BackgroundLayer::bg2,atlas_options);
                sample.model={.00625F,0,0,0,0,-.00625F,0,0,0,0,1,0,-.8F,.7F,-2,1};
                // Exercise every lower-atlas row and vary columns within
                // 16x16 tiles; the old title-only sampling missed this region.
                for(unsigned y=256;y<512;++y) {
                    const unsigned x=(y*37U)%atlas_width;
                    const auto index=decoded.get(x,y);const auto colour=palette[index];
                    tile_expected.push_back(index?(uint32_t(colour.r)|(uint32_t(colour.g)<<8)|(uint32_t(colour.b)<<16)):0U);
                    for(auto& vertex:sample.geometry.vertices) {
                        vertex.uv[0]=float(x);vertex.uv[1]=float(y);
                    }
                    tile_cases.push_back(sample);
                }
            }
            starfox::render::Framebuffer sprite_reference(256,224);
            starfox::render::SpriteRenderer{}.draw_objects(ppu,sprite_reference,{},0,false,false,nullptr,false,&history.current()->meters);
            std::vector<unsigned char> sprite_pixels(256*224*4);
            for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x) {
                const auto colour=palette[sprite_reference.get(x,y)];const auto index=(y*256+x)*4;
                sprite_pixels[index]=colour.r;sprite_pixels[index+1]=colour.g;
                sprite_pixels[index+2]=colour.b;sprite_pixels[index+3]=255;
            }
            bitmap(std::filesystem::path(argv[1])/"source-sprites-reference.bmp",sprite_pixels,256,224);
            starfox::render::Framebuffer bitmap_reference(256,224);
            decoder.draw_bg1(ppu,bitmap_reference,starfox::render::TilePriorityPass::all,0,false,16,true);
            for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x) {
                const auto colour=palette[bitmap_reference.get(x,y)];const auto index=(y*256+x)*4;
                sprite_pixels[index]=colour.r;sprite_pixels[index+1]=colour.g;sprite_pixels[index+2]=colour.b;
            }
            bitmap(std::filesystem::path(argv[1])/"source-bg1-reference.bmp",sprite_pixels,256,224);
        }
        if(titania_water) {
            const auto& ppu=*history.current()->ppu;
            const auto palette=starfox::render::decode_bgr555_palette(ppu.cgram);
            starfox::render::BackgroundRenderer decoder;
            for(unsigned layer=0;layer<3;++layer) {
                auto source=ppu;
                const unsigned width=layer==0?512:256,height=layer==0?512:224;
                starfox::render::Framebuffer decoded(width,height);
                if(layer==0) {
                    source.bg2_horizontal_offsets_enabled=false;
                    source.bg2_scanline_scroll_enabled=false;
                    source.bg2_vertical_offsets_enabled=false;
                    decoder.draw_bg2(source,0,0,decoded);
                } else if(layer==1) decoder.draw_bg2(source,source.bg2_scroll_x,source.bg2_scroll_y,decoded);
                else decoder.draw_bg3(source,decoded);
                std::vector<unsigned char> pixels(width*height*4);
                for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                    const auto colour=palette[decoded.get(x,y)];const auto i=(y*width+x)*4;
                    pixels[i]=colour.r;pixels[i+1]=colour.g;pixels[i+2]=colour.b;pixels[i+3]=255;
                }
                const char* name=layer==0?"water-bg2-atlas.bmp":layer==1?"water-bg2-native.bmp":"water-bg3-native.bmp";
                bitmap(std::filesystem::path(argv[1])/name,pixels,width,height);
            }
        }
        if(space_scene) {
            unsigned wait=0;
            while(assembler.assemble_dust(*history.current()).geometry.vertices.size()<180 && wait<600) {
                advance();history.capture();++wait;
            }
            std::cout<<"Space starfield warmup: "<<wait<<" additional source ticks\n";
            warmup+=wait;
        }
        auto model_snapshot=*history.current();
        if(omit_connected_shadows) model_snapshot.shadows_enabled=false; // Diagnostic ablation only.
        const bool vr_world=explicit_stage && stage_argument.find("@vr-world")!=std::string_view::npos;
        auto assembled=vr_world?assembler.assemble_world_interpolated(model_snapshot,model_snapshot,1.,false,true)
            :assembler.assemble(model_snapshot);
        if(full_layers && !vr_world) {
            auto dust_packet=assembler.assemble_dust_interpolated(*history.previous(),*history.current(),1.,false,256,!cpu_space_dust);
            std::cout<<"Source dust mode "<<int(history.current()->dots_mode)<<", active points "
                <<history.current()->dust_point_count<<", submitted quads "<<dust_packet.geometry.vertex_view().size()/6<<'\n';
            if(space_scene) require(history.current()->dots_mode<0 && !dust_packet.geometry.vertex_view().empty(),
                "Space fixture did not produce source star geometry");
            if(omit_dust) dust_packet.geometry={}; // Diagnostic ablation, never a gameplay setting.
            assembled.packets.insert(assembled.packets.begin(),std::move(dust_packet));
            assembled.handles.insert(assembled.handles.begin(),0x20000U);
            auto grid_packet=history.current()->grid_lines
                ?assembler.assemble_connected_grid_interpolated(*history.previous(),*history.current(),1.)
                :assembler.assemble_grid_gpu(*history.current());
            if(live_connected) require(grid_packet.geometry.vertex_view().size()==6
                && grid_packet.geometry.vertex_view()[0].texture[3]==(512U|4194304U),
                "Combined scene did not select compute connected grid");
            assembled.packets.insert(assembled.packets.begin(),std::move(grid_packet));
            assembled.handles.insert(assembled.handles.begin(),0x30000U);
            for(auto& compute:assembled.compute_models) compute.packet_index+=2;
        }
        for(const auto& pending:assembled.pending) std::cerr<<"Pending object "<<pending.handle<<": "<<pending.reason<<'\n';
        require(assembled.pending.empty(),"Live scene has unsupported normal-model draws");
        if(live_compute) {
            require(!assembled.compute_models.empty(),"Live scene produced no compute models");
            std::cout<<"Live compute scene: "<<assembled.compute_models.size()<<" models, "<<assembled.packets.size()<<" ordered packets\n";
            size_t sprite_faces=0,line_faces=0;
            for(const auto& model:assembled.compute_models) for(auto primitive:model.model.faces.primitives) {
                sprite_faces+=primitive==starfox::render::PackedPrimitive::sprite;
                line_faces+=primitive==starfox::render::PackedPrimitive::line;
            }
            std::cout<<"Resident primitive coverage: "<<line_faces<<" lines, "<<sprite_faces<<" sprites\n";
            live_compute_packets=assembled;
        }
        live_packets=std::move(assembled.packets);
        if(intro_background) live_packets.clear();
        if(full_layers) {
            const auto snapshot=history.current();
            if(const auto ship=snapshot->transforms.find(snapshot->player);ship!=snapshot->transforms.end())
                std::cout<<"Tracking player y "<<ship->second.transform.y<<" float "<<snapshot->view_float_y
                    <<" pitch "<<snapshot->camera.pitch<<" strategy "<<ship->second.strategy_address<<'\n';
            std::cout<<"Source camera "<<snapshot->camera.x<<','<<snapshot->camera.y<<','<<snapshot->camera.z
                <<" rotation "<<snapshot->camera.pitch<<','<<snapshot->camera.yaw<<','<<snapshot->camera.roll
                <<" shadow plane "<<snapshot->shadow_height<<'\n';
            for(size_t i=0;i<live_packets.size();++i) {
                const auto& packet=live_packets[i];
                if((assembled.handles[i]&0xffff0000U)!=0x10000U) continue;
                std::cout<<"Shadow handle "<<(assembled.handles[i]&0xffffU)<<" world translation "
                    <<packet.model[12]<<','<<packet.model[13]<<','<<packet.model[14]
                    <<" triangles "<<packet.geometry.vertices.size()/3<<'\n';
            }
        }
        if(combined_live) {
            const auto snapshot=history.current();
            auto sprites=source_sprite_packet(*snapshot->ppu,snapshot->display_brightness,{},false,full_layers?&snapshot->meters:nullptr);
            if(intro_background) sprites.geometry={};
            require(ex_menu_capture || intro_scene || controls_scene || stage_argument.find("@from-entry")!=std::string_view::npos
                || !sprites.geometry.vertices.empty(),"Combined live scene has no sprite artwork");
            sprites.model={.00625F,0,0,0,0,-.00625F,0,0,0,0,1,0,-.8F,.7F,-2,1};
            if(full_layers) {
                sprites.model=source_ui_layer_matrix(float(snapshot->source_vanishing_point[0]),
                    float(snapshot->source_vanishing_point[1]),ex_menu_capture).value();
                if(ex_menu_page) require(sprites.model==source_layer_matrix(128,112).value(),
                    "Native menu page moved its screen-space text plane");
                const auto& ppu=*snapshot->ppu;
                if(ppu.tunnel_scene) {
                    auto surround=tunnel_surround_packet(source_backdrop_colour(ppu.cgram[starfox::render::tunnel_wall_index(ppu)],snapshot->display_brightness));
                    surround.model=sprites.model;live_tunnel_surround.push_back(std::move(surround));
                }
                live_backdrop=(controls_scene || snapshot->flow==starfox::simulation::GameFlowState::continue_choice)?source_menu_background_colour(ppu,snapshot->display_brightness):snapshot->flow==starfox::simulation::GameFlowState::game_over || ppu.tunnel_scene || snapshot->background_unique_top_rows!=0 || snapshot->background_star_sphere || snapshot->background_space_horizon
                    ?source_background_border_colour(ppu.cgram,snapshot->display_brightness)
                    :source_backdrop_colour(ppu.cgram[0],snapshot->display_brightness);
                BackgroundTileOptions options;options.brightness=snapshot->display_brightness;
                options.colour_subtract=snapshot->background_colour_subtract;
                options.scroll_override=snapshot->background_scroll_override;
                options.single_occurrence_top_rows=snapshot->background_unique_top_rows;
                options.ex_twin_planets=snapshot->background_ex_twin_planets;
                options.ex_face_planets=snapshot->background_ex_face_planets;
                options.ex_ocean_island=snapshot->background_ex_ocean_island;
                options.ex_volcanic_horizon=snapshot->background_ex_volcanic_horizon;
                options.ex_city_planets=snapshot->background_ex_city_planets;
                if(ppu.background_mode==2) {
                    options.expanded_horizontal=true;options.horizontal_bounds={-384,640};
                }
                auto bg2=background_tile_packet(ppu,BackgroundLayer::bg2,options);
                if(stage_argument.find("@bg2-audit")!=std::string_view::npos) {
                    starfox::render::Framebuffer reference(400,224);
                    const int sx=options.scroll_override?(*options.scroll_override)[0]:ppu.bg2_scroll_x;
                    const int sy=options.scroll_override?(*options.scroll_override)[1]:ppu.bg2_scroll_y;
                    if(ppu.tunnel_scene) {
                        // Desktop stretches one cross-section across its wide
                        // frame. VR intentionally keeps a native center window
                        // and solid surround: compare its sampling, not that
                        // different desktop presentation transform.
                        starfox::render::Framebuffer center(256,224);
                        starfox::render::BackgroundRenderer{}.draw_bg2(ppu,sx,sy,center);
                        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<400;++x)
                            reference.set(x,y,x>=72 && x<328?center.get(x-72,y)
                                :starfox::render::tunnel_wall_index(ppu));
                    } else starfox::render::BackgroundRenderer{}.draw_bg2(ppu,sx,sy,reference,
                        starfox::render::TilePriorityPass::all,72,true);
                    const auto colours=starfox::render::apply_snes_brightness(
                        starfox::render::decode_bgr555_palette(ppu.cgram),snapshot->display_brightness);
                    std::vector<unsigned char> pixels(400*224*4);
                    for(unsigned y=0;y<224;++y) for(unsigned x=0;x<400;++x) {
                        const auto colour=colours[reference.get(x,y)];const auto at=(y*400+x)*4;
                        pixels[at]=colour.r;pixels[at+1]=colour.g;pixels[at+2]=colour.b;pixels[at+3]=255;
                    }
                    bitmap(std::filesystem::path(argv[1])/"source-bg2-reference.bmp",pixels,400,224);
                    std::cout<<"BG2 audit: effective scroll "<<sx<<','<<sy
                        <<" horizontal offsets "<<ppu.bg2_horizontal_offsets_enabled
                        <<" vertical offsets "<<ppu.bg2_vertical_offsets_enabled
                        <<" scanline scroll "<<ppu.bg2_scanline_scroll_enabled
                        <<" first scanline y "<<ppu.bg2_scanline_scroll_y[0]<<'\n';
                    for(unsigned y:{0U,32U,64U,111U,160U,223U})
                        for(unsigned x:{0U,64U,72U,80U,136U,200U,264U,320U,327U,335U,399U}) {
                            auto sample=bg2;sample.model={.00625F,0,0,0,0,-.00625F,0,0,0,0,1,0,-.8F,.7F,-2,1};
                            for(auto& vertex:sample.geometry.vertices) {vertex.uv[0]=float(int(x)-72);vertex.uv[1]=float(y);}
                            const auto ink=reference.get(x,y);const auto colour=colours[ink];
                            tile_expected.push_back(ink?(uint32_t(colour.r)|(uint32_t(colour.g)<<8)|(uint32_t(colour.b)<<16)):0U);
                            tile_cases.push_back(std::move(sample));
                        }
                }
                if(snapshot->flow==starfox::simulation::GameFlowState::intro && !snapshot->meters.extended
                    && snapshot->background_unique_top_rows==224) {
                    live_backgrounds.push_back(intro_star_sphere_packet(ppu,snapshot->display_brightness));
                    bg2=intro_planet_packet(ppu,snapshot->display_brightness);
                    bg2.model=intro_planet_motion(snapshot->background_vertical_scroll,snapshot->background_vertical_scroll,1.,
                        float(snapshot->source_vanishing_point[1])+16.F);
                } else if(snapshot->background_star_sphere) {
                    bg2=intro_star_sphere_packet(ppu,snapshot->display_brightness,false,true,false,snapshot->background_retain_sky_scroll);
                    if(snapshot->background_ex_face_planets && !bg2.geometry.texels.empty()) bg2.geometry.texels[7]|=256U;
                } else if(snapshot->background_space_horizon) {
                    if(snapshot->background_unique_space)
                        live_backgrounds.push_back(intro_star_sphere_packet(ppu,snapshot->display_brightness,false,false,true));
                    bg2=snapshot->background_orbital_planet?orbital_planet_sphere_packet(ppu,options,false,snapshot->background_orbital_thin,snapshot->background_orbital_entry,
                        snapshot->flow==starfox::simulation::GameFlowState::gameplay && game.experience()==starfox::simulation::Experience::starfox_ex)
                        :space_horizon_sphere_packet(ppu,options);
                    if(snapshot->background_orbital_planet) {
                        const bool corrected=snapshot->flow==starfox::simulation::GameFlowState::gameplay
                            && game.experience()==starfox::simulation::Experience::starfox_ex
                            && (snapshot->background_orbital_entry || snapshot->background_orbital_thin);
                        require(bg2.model[0]==(corrected?0.F:1.F)
                            && bg2.model[1]==(corrected?-1.F:0.F),"Orbital correction escaped EX gameplay scope");
                    }
                    if(snapshot->background_unique_space) bg2=unique_planet_packet(ppu,options,snapshot->background_planet_rect);
                } else if(snapshot->background_landscape) {
                    bg2=landscape_sphere_packet(ppu,options,112.F,false,snapshot->background_landscape_unique_half,snapshot->landscape_atlas_origin,
                        snapshot->background_landscape_unique_right_half);
                    if(snapshot->landscape_grid_height<0)
                        // Match the headset's GPU-deformed receiver by default.
                        // CPU deformation is an explicit comparison mode, not
                        // evidence that the deployed GPU path renders correctly.
                        place_landscape_ground(bg2,std::clamp(float(snapshot->landscape_grid_height)/256.F,-8.F,-.001F),stage_argument.find("@cpu-ground")==std::string_view::npos);
                    bg2.model=landscape_camera_motion(*snapshot,*snapshot,1.);
                }
                else if(snapshot->background_water_surround) {
                    bg2=water_surface_packet(ppu,options,std::clamp(std::abs(float(snapshot->camera.y-snapshot->shadow_height))/256.F,.01F,8.F));
                    if(stage_argument.ends_with("@height-equivalent")) {
                        // Same world plane, represented by half-height geometry
                        // and a 2x model scale. Texture projection must agree.
                        for(auto& vertex:bg2.geometry.vertices) vertex.position[1]*=.5F;
                        bg2.model[5]=2.F;
                    }
                }
                else bg2.model=sprites.model;
                std::optional<DrawPacket> photograph;
                std::vector<DrawPacket> photographic_bodies;
                if(stage_argument.find("@enhanced-sky")!=std::string_view::npos) {
                    EnhancedLandscape enhancement(symbols);
                    const auto loader=[](unsigned,std::string_view relative) {
                        const auto path=std::filesystem::path(__FILE__).parent_path().parent_path()/relative;
                        std::ifstream stream(path,std::ios::binary);require(bool(stream),"Missing live sky audit artwork");
                        return std::vector<uint8_t>{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
                    };
                    photograph=enhancement.prepare(*snapshot,game,loader);
                    require(photograph.has_value(),"Requested enhanced sky family has not migrated");
                    if(stage_argument.find("@weather")!=std::string_view::npos) {
                        const auto& palette=snapshot->ppu->cgram;
                        std::cout<<"Weather endpoint inks 1/14/25: "<<palette[1]<<','<<palette[14]<<','<<palette[25]<<'\n';
                        const bool fog=palette[14]==(21|(25<<5)|(30<<10)) && palette[25]==(21|(25<<5)|(31<<10));
                        const bool clear=palette[1]==(29|(25<<5)|(15<<10)) && palette[14]==4
                            && palette[25]==(11|(8<<5)|(6<<10));
                        require(scene_ticks<=2600?fog:clear,"Natural VR weather replay missed authored palette endpoint");
                    }
                    if(ex_menu_capture) for(const auto& vertex:photograph->geometry.vertex_view())
                        for(unsigned c=0;c<3;++c) require(vertex.odd_color[c]==0
                            && vertex.color[c]==float(snapshot->display_brightness)/15.F*(ex_menu_choice==26?.85F:ex_menu_choice==32?.94F:1.F),
                            "Menu photograph inherited a stale gameplay palette");
                    const auto reused=enhancement.prepare(*snapshot,game,loader);
                    require(reused->geometry.shared_texels==photograph->geometry.shared_texels
                        && reused->geometry.shared_vertices==photograph->geometry.shared_vertices,"Unchanged live sky was rebuilt");
                    if(enhancement.scrolling_pattern()) {
                        require(enhancement.pattern_panorama(*reused),"Pattern interpolation lost its uploaded packet");
                        auto previous=*snapshot;
                        auto ppu=std::make_shared<starfox::simulation::SnesPpuState>(*snapshot->ppu);
                        ppu->bg2_scroll_x=uint16_t((unsigned(ppu->bg2_scroll_x)+508)&511);previous.ppu=ppu;
                        const auto start=enhancement.pattern_motion(previous,*snapshot,0);
                        const auto middle=enhancement.pattern_motion(previous,*snapshot,.5);
                        const auto end=enhancement.pattern_motion(previous,*snapshot,1);
                        require(start!=middle && middle!=end && end==photographic_body_motion({128,112}),
                            "Pattern scroll snapped instead of interpolating");
                        previous.flow=starfox::simulation::GameFlowState::game_over;
                        require(enhancement.pattern_motion(previous,*snapshot,0)==end,
                            "Pattern interpolated across a scene transition");
                        std::cout<<"Photographic panorama: wrapped scroll interpolates without texture uploads\n";
                    }
                    photographic_bodies=enhancement.bodies();
                    const auto storm_id=symbols.find("BG_6_4"),storm_base=symbols.find("BGLISTS");
                    if((ex_menu_capture && ex_menu_choice==3)
                        || (snapshot->meters.extended && !storm_id.empty() && !storm_base.empty()
                            && snapshot->background_id==uint16_t(storm_id.front()-storm_base.front()))) {
                        require(photographic_bodies.size()==2,"Storm moons missing or repeated");
                        require(photographic_bodies[0].geometry.shared_texels==photographic_bodies[1].geometry.shared_texels,
                            "Storm moons uploaded duplicate crater images");
                        auto tinted=*snapshot;
                        auto palette=std::make_shared<starfox::simulation::SnesPpuState>(*snapshot->ppu);
                        for(unsigned ink=74;ink<=79;++ink) palette->cgram[ink]=31;
                        tinted.ppu=palette;
                        (void)enhancement.prepare(tinted,game,loader);
                        for(size_t i=0;i<2;++i) {
                            const auto& body=enhancement.bodies()[i];
                            require(body.geometry.shared_texels==photographic_bodies[i].geometry.shared_texels,
                                "Storm palette change reuploaded crater image");
                            for(const auto& vertex:body.geometry.vertex_view())
                                require(vertex.color[1]==0 && vertex.color[2]==0,
                                    "Storm moon ignored its live palette");
                        }
                        (void)enhancement.prepare(*snapshot,game,loader);
                        photographic_bodies=enhancement.bodies();
                        std::cout<<"Storm moons: two shared images, native centers, live palette\n";
                    }
                    if(ex_menu_capture && (ex_menu_choice==1 || ex_menu_choice==4 || ex_menu_choice==5
                        || ex_menu_choice==10 || ex_menu_choice==12)) {
                        require(photographic_bodies.size()==(ex_menu_choice==10?2:1),
                            "Enhanced preview lost its unique landmark");
                        for(const auto& body:photographic_bodies)
                            require(!body.geometry.vertex_view().empty(),"Preview landmark mask selected no pixels");
                    }
                    if(!ex_menu_capture && snapshot->meters.extended && !storm_base.empty())
                        for(const auto label:{"BG_5_1","BG_6_1","BG_5_5","BG_7_5"}) {
                            const auto id=symbols.find(label);
                            if(!id.empty() && snapshot->background_id==uint16_t(id.front()-storm_base.front())) {
                                require(photographic_bodies.size()==1
                                    && !photographic_bodies.front().geometry.vertex_view().empty(),
                                    "Enhanced gameplay lost its luminous landmark");
                                if(std::string_view(label)=="BG_5_1" || std::string_view(label)=="BG_6_1")
                                    require(snapshot->landscape_atlas_origin+112==352,
                                        "Snow receiver includes native mountain rows");
                            }
                        }
                    if(snapshot->background_orbital_planet) {
                        require(photographic_bodies.size()==(snapshot->background_orbital_entry?2:1),"Orbital surface/moon missing or repeated");
                        require(photographic_bodies.front().geometry.vertex_view().front().odd_color[3]==4,
                            "Orbital surface is using the clamped landscape projection");
                    }
                    if(snapshot->background_ex_city_planets) {
                        require(photographic_bodies.size()==starfox::render::city_moons.size(),"City moons are missing or repeated");
                        for(const auto& body:photographic_bodies)
                            require(body.geometry.shared_texels==photographic_bodies.front().geometry.shared_texels,
                                "City moons allocated duplicate images");
                    }
                    (void)enhancement.prepare(*snapshot,game,loader);
                    for(size_t i=0;i<photographic_bodies.size();++i)
                        require(photographic_bodies[i].geometry.shared_vertices==enhancement.bodies()[i].geometry.shared_vertices
                            && photographic_bodies[i].geometry.shared_texels==enhancement.bodies()[i].geometry.shared_texels,
                            "Unchanged photographic moon was rebuilt");
                    auto faded=*snapshot;faded.display_brightness=0;
                    const auto dark=enhancement.prepare(faded,game,loader);
                    require(dark && dark->geometry.shared_texels==photograph->geometry.shared_texels,
                        "Display fade reloaded photographic pixels");
                    for(const auto& vertex:dark->geometry.vertex_view()) for(unsigned c=0;c<3;++c)
                        require(vertex.color[c]==0 && vertex.odd_color[c]==0,"Photographic sky ignored display blackout");
                    for(size_t i=0;i<photographic_bodies.size();++i) {
                        const auto vertices=photographic_bodies[i].geometry.vertex_view();
                        if(!vertices.empty() && (vertices.front().texture[3]&backdrop_texture_flag)==backdrop_texture_flag)
                            require(photographic_bodies[i].geometry.shared_texels==enhancement.bodies()[i].geometry.shared_texels,
                                "Moon fade reuploaded the master");
                        else require(enhancement.bodies()[i].geometry.texel_view()[13]==15,"Native saucer ignored display blackout");
                        for(const auto& vertex:enhancement.bodies()[i].geometry.vertex_view()) for(unsigned c=0;c<3;++c)
                            require(vertex.color[c]==0,"Photographic moon ignored display blackout");
                    }
                    auto excluded=*snapshot;auto tunnel=std::make_shared<starfox::simulation::SnesPpuState>(*snapshot->ppu);
                    tunnel->tunnel_scene=true;excluded.ppu=tunnel;
                    require(!enhancement.prepare(excluded,game,loader),"Photographic replacement erased tunnel scene");
                    (void)enhancement.prepare(*snapshot,game,loader);
                    if(enhancement.moving_body()) {
                        const bool faces=snapshot->background_ex_face_planets || (ex_menu_capture && ex_menu_choice==18);
                        const auto cloud_id=symbols.find("BG_1_4"),cloud_route_id=symbols.find("BG_1_14"),
                            background_base=symbols.find("BGLISTS");
                        const bool clouds=(ex_menu_capture && ex_menu_choice==20)
                            || (!ex_menu_capture && !background_base.empty()
                                && ((!cloud_id.empty() && snapshot->background_id==uint16_t(cloud_id.front()-background_base.front()))
                                    || (game.experience()==starfox::simulation::Experience::starfox_ex && !cloud_route_id.empty()
                                        && snapshot->background_id==uint16_t(cloud_route_id.front()-background_base.front()))));
                        require(photographic_bodies.size()==(faces?starfox::render::face_planet_regions.size():clouds?2:1),"Unique planet is missing or repeated");
                        const auto previous=history.previous();
                        for(size_t body=0;body<photographic_bodies.size();++body) for(unsigned phase=0;phase<=8;++phase) {
                            const auto motion=enhancement.body_motion(*previous,*snapshot,double(phase)/8,body);
                            for(unsigned column=0;column<3;++column) {
                                float norm=0;for(unsigned row=0;row<3;++row) norm+=motion[column*4+row]*motion[column*4+row];
                                require(std::abs(norm-1)<.00001F,"Unique body motion changes scale");
                            }
                        }
                        auto a=*snapshot,b=*snapshot;
                        auto p=std::make_shared<starfox::simulation::SnesPpuState>(*snapshot->ppu),q=std::make_shared<starfox::simulation::SnesPpuState>(*snapshot->ppu);
                        p->bg2_horizontal_offsets_enabled=q->bg2_horizontal_offsets_enabled=false;
                        p->bg2_vertical_offsets_enabled=q->bg2_vertical_offsets_enabled=false;
                        p->bg2_scroll_x=96;q->bg2_scroll_x=112;p->bg2_scroll_y=q->bg2_scroll_y=100;
                        a.ppu=p;b.ppu=q;
                        const auto motion_vertices=enhancement.bodies().front().geometry.shared_vertices;
                        const auto motion_texels=enhancement.bodies().front().geometry.shared_texels;
                        const auto yaw=[](const Matrix4& m) {return std::atan2(-m[8],m[10]);};
                        const auto start=yaw(enhancement.body_motion(a,b,0)),end=yaw(enhancement.body_motion(a,b,1));
                        const auto middle=yaw(enhancement.body_motion(a,b,.5));
                        require(std::abs(start-end)>.001F && middle>std::min(start,end) && middle<std::max(start,end),
                            "Celestial scroll did not interpolate between source frames");
                        require(enhancement.bodies().front().geometry.shared_vertices==motion_vertices
                            && enhancement.bodies().front().geometry.shared_texels==motion_texels,
                            "Celestial interpolation replaced geometry or pixels");
                    }
                    enhancement.retain_native_ground(bg2,*snapshot->ppu);
                    if(!enhancement.full_sphere()) {
                        photograph->model=landscape_camera_motion(*snapshot,*snapshot,1.);
                        for(auto& body:photographic_bodies) body.model=photograph->model;
                        require(photograph->model==bg2.model,"Enhanced sky and ground use different motion matrices");
                    } else if(snapshot->flow!=starfox::simulation::GameFlowState::game_over)
                        require(bg2.geometry.vertex_view().empty(),"Full sky retained duplicate native background");
                    std::cout<<"Enhanced live backdrop: "<<snapshot->background_id<<" cached image/geometry; "
                        <<(snapshot->flow==starfox::simulation::GameFlowState::game_over?"native foreground retained"
                            :enhancement.full_sphere()?"native background replaced":"native ground retained")<<'\n';
                    std::cout<<"Photographic unique bodies: "<<photographic_bodies.size()<<'\n';
                }
                if(!photograph && snapshot->flow==starfox::simulation::GameFlowState::game_over)
                    live_backgrounds.push_back(game_over_star_sphere_packet(*snapshot->ppu,snapshot->display_brightness,
                        snapshot->background_colour_subtract));
                if(photograph && snapshot->flow==starfox::simulation::GameFlowState::game_over) {
                    live_backgrounds.push_back(std::move(*photograph));photograph.reset();
                }
                live_backgrounds.push_back(std::move(bg2));
                if(photograph) {
                    live_backgrounds.push_back(std::move(*photograph));
                    live_backgrounds.insert(live_backgrounds.end(),photographic_bodies.begin(),photographic_bodies.end());
                }
                if(!photograph && !ppu.tunnel_scene && snapshot->background_orbital_entry) {
                    auto planet_options=options;
                    planet_options.scroll_override=std::array<int16_t,2>{0,312};
                    live_backgrounds.push_back(unique_planet_packet(ppu,planet_options,{336,320,56,64}));
                }
                if(!photograph && !ppu.tunnel_scene && snapshot->background_ex_city_planets) {
                    auto planet_options=options;
                    planet_options.scroll_override=std::array<int16_t,2>{0,248};
                    auto planet=unique_planet_packet(ppu,planet_options,{384,208,56,48});
                    planet.model=landscape_camera_motion(*snapshot,*snapshot,1.);
                    live_backgrounds.push_back(std::move(planet));
                }
                options.scroll_override.reset();
                options.expanded_horizontal=false;options.horizontal_bounds={0,256};
                options.single_occurrence_top_rows=0;
                if(ppu.background_mode==1) {
                    auto bg3=background_tile_packet(ppu,BackgroundLayer::bg3,options);
                    if(snapshot->background_water_surround) bg3=water_surround_packet(ppu,options);
                    else bg3.model=sprites.model;
                    if(snapshot->background_water_surround)
                        live_backgrounds.insert(live_backgrounds.end()-1,std::move(bg3));
                    else live_backgrounds.push_back(std::move(bg3));
                }
                if(snapshot->native_ex_bitmap && !omit_bitmap && !replace_native_dialogue(*snapshot)) {
                    options.guard_inset=16;options.transparent_black=true;
                    auto bitmap=background_tile_packet(ppu,BackgroundLayer::bg1,options);
                    bitmap.model=sprites.model;live_sprites.push_back(std::move(bitmap));
                }
            }
            live_sprites.push_back(std::move(sprites));
            if(full_layers) {
                auto meters=source_meter_packet(snapshot->meters,snapshot->ppu->cgram,snapshot->display_brightness);
                meters.model=source_ui_layer_matrix(float(snapshot->source_vanishing_point[0]),
                    float(snapshot->source_vanishing_point[1]),ex_menu_capture,true).value();
                live_sprites.push_back(std::move(meters));
                if(replace_native_dialogue(*snapshot)) {
                    starfox::render::ScaledTextRenderer layout(rom,symbols);
                    auto dialogue=source_dialogue_packets(rom,symbols,snapshot->dialogue,layout,
                        snapshot->ppu->cgram,snapshot->display_brightness);
                    unsigned visible=0;
                    for(auto& layer:dialogue) {
                        visible+=!layer.geometry.vertex_view().empty();
                        layer.model=source_layer_matrix(float(snapshot->source_vanishing_point[0])+16.F,
                            float(snapshot->source_vanishing_point[1])+16.F).value();
                        live_sprites.push_back(std::move(layer));
                    }
                    std::cout<<"Live comms: "<<visible<<" visible layers, text="<<snapshot->dialogue.text_visible
                        <<" meter="<<snapshot->dialogue.meter_visible<<" alternate="<<snapshot->dialogue.alternate_portraits<<'\n';
                    if(selected_stage=="INTROMAP" && snapshot->meters.extended
                        && stage_argument.find("@first-dialogue")!=std::string_view::npos)
                        require(visible>=3 && snapshot->dialogue.alternate_portraits && !snapshot->dialogue.meter_visible,
                            "EX intro communication lost portrait/text or inherited a teammate meter");
                }
            }
        }
        require(intro_background || !live_packets.empty(),"Live scene has no model packets");
        std::cout<<"Live "<<stage<<" after "<<warmup+scene_ticks<<" ticks: "<<live_packets.size()<<" model packets, source palettes\n";
    } else if(argc>=4) {
        const auto rom=starfox::assets::RomImage::load(argv[2]);
        const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
        const starfox::assets::ShapeDecoder decoder(rom,symbols);
        const auto ship=decoder.decode_by_name(symbols,cartridge_name);
        ShapeMesh mesh;std::string error;
        require(decode_shape_mesh(ship,0,1,mesh,error),error.c_str());
        float radius=0;
        for(const auto& vertex:mesh.vertices) for(float coordinate:vertex.position) radius=std::max(radius,std::abs(coordinate));
        for(const auto& face:ship.faces) if(face.sprite) {
            const auto material=starfox::render::face_material(ship,face,0,0,{0,0,0},{},std::nullopt,0);
            if(material.texture) {
                const auto width=uint32_t(material.texture->u_mask)+1;
                radius=std::max(radius,float(width)*(width==64?1.F:.5F));
            }
        }
        require(radius>0,"Empty cartridge bounds");cartridge_scale=.9F/radius;
        // Neutral diagnostic palette, not a claim of in-game palette parity.
        starfox::render::Palette256 palette{};
        for(unsigned i=0;i<16;++i) palette[i]={uint8_t(i*17),uint8_t(i*17),uint8_t(i*17),255};
        starfox::render::RenderPose pose;
        pose.z=2/cartridge_scale;pose.pitch=.55*65536/(2*std::acos(-1.));
        if(eye_material_fixture) {pose.pitch=0;pose.yaw=32768;}
        pose.use_source_lighting_state=true;pose.source_depth=0; // Neutral diagnostic shading.
        DrawPacket packet;
        require(build_draw_packet(ship,pose,palette,0,1,false,1/cartridge_scale,packet,error),error.c_str());
        if(resident_model) {
            auto gpu_pose=pose;gpu_pose.continuous_geometry=true;
            SourceSpanModel gpu_model;
            require(prepare_source_span_model(ship,gpu_pose,{},224,192,gpu_model,error,true),error.c_str());
            gpu_model.graphics_palette_flags=1;
            for(size_t i=0;i<palette.size();++i) {
                const auto c=palette[i];gpu_model.graphics_palette[i]=uint32_t(c.r)|(uint32_t(c.g)<<8)
                    |(uint32_t(c.b)<<16)|(uint32_t(c.a)<<24);
            }
            live_compute_packets.packets.emplace_back();live_compute_packets.handles.push_back(1);
            live_compute_packets.compute_models.push_back({1,0,1/cartridge_scale,std::move(gpu_model)});
        }
        for(std::size_t f=0;f<ship.faces.size();++f) {
            const auto& face=ship.faces[f];
            const auto material=starfox::render::face_material(ship,face,0,0,{0,0,0},pose,std::nullopt,0);
            if(!eye_material_fixture && !material.texture && !face.sprite) continue;
            std::cout<<"Face "<<f<<" colour "<<unsigned(face.colour_id)<<" sprite "<<face.sprite
                <<" texture "<<bool(material.texture)<<" vertices";
            for(auto i:face.vertex_indices) {const auto& p=mesh.vertices.at(i).position;std::cout<<" "<<unsigned(i)<<":"<<p[0]<<","<<p[1]<<","<<p[2];}
            std::cout<<'\n';
            if(eye_material_fixture && material.texture) {
                const auto& t=*material.texture;
                std::vector<unsigned char> pixels(t.texels.size()*4);
                for(std::size_t i=0;i<t.texels.size();++i) {
                    const auto c=palette[t.texels[i]];
                    pixels[i*4]=c.r;pixels[i*4+1]=c.g;pixels[i*4+2]=c.b;pixels[i*4+3]=255;
                }
                bitmap(std::filesystem::path(argv[1])/("eye-texture-"+std::to_string(f)+".bmp"),pixels,t.u_mask+1,t.v_mask+1);
            }
        }
        if(grid_mode.ends_with("-textures")) {
            std::erase_if(packet.geometry.vertices,[](const auto& v){return !(v.texture[3]&1);});
            packet.geometry.ranges.clear();
        }
        cartridge=std::move(packet.geometry);cartridge_model=packet.model;
        require(cartridge.deferred.empty(),"Cartridge fixture requires unsupported primitives");
    }
    VulkanLoader loader;require(loader.initialize(),loader.status().c_str());
    Cleanup cleanup;
    auto get=loader.get_instance_proc_addr();
    auto create_instance=function<PFN_vkCreateInstance>(get(nullptr,"vkCreateInstance"));
    VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    VkInstance instance{};check(create_instance(&instance_info,nullptr,&instance),"Create Vulkan instance");
#define INSTANCE(name) auto name=function<PFN_##name>(get(instance,#name))
    INSTANCE(vkDestroyInstance);
    cleanup.actions.push_back([=]{vkDestroyInstance(instance,nullptr);});
    INSTANCE(vkEnumeratePhysicalDevices);INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);
    INSTANCE(vkGetPhysicalDeviceMemoryProperties);INSTANCE(vkGetPhysicalDeviceProperties);
    INSTANCE(vkGetPhysicalDeviceFormatProperties);
    INSTANCE(vkCreateDevice);INSTANCE(vkGetDeviceProcAddr);
    uint32_t count{};check(vkEnumeratePhysicalDevices(instance,&count,nullptr),"Count physical devices");
    require(count>0,"No Vulkan device");std::vector<VkPhysicalDevice> devices(count);
    check(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"Enumerate physical devices");
    VkPhysicalDevice physical{};uint32_t family{};
    for(auto candidate:devices) {
        uint32_t families{};vkGetPhysicalDeviceQueueFamilyProperties(candidate,&families,nullptr);
        std::vector<VkQueueFamilyProperties> queues(families);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate,&families,queues.data());
        for(uint32_t i=0;i<families;++i) if(queues[i].queueCount && (queues[i].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT)) {
            physical=candidate;family=i;break;
        }
        if(physical) break;
    }
    require(physical!=VK_NULL_HANDLE,"No graphics queue");
    VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(physical,&properties);
    std::cout<<"GPU: "<<properties.deviceName<<'\n';
    VkPhysicalDeviceMemoryProperties memory_properties{};vkGetPhysicalDeviceMemoryProperties(physical,&memory_properties);
    const float priority=1;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex=family;queue_info.queueCount=1;queue_info.pQueuePriorities=&priority;
    VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};device_info.queueCreateInfoCount=1;device_info.pQueueCreateInfos=&queue_info;
    VkDevice device{};check(vkCreateDevice(physical,&device_info,nullptr,&device),"Create Vulkan device");
#define DEVICE(name) auto name=function<PFN_##name>(vkGetDeviceProcAddr(device,#name))
    DEVICE(vkDestroyDevice);DEVICE(vkDeviceWaitIdle);
    cleanup.actions.push_back([=]{vkDeviceWaitIdle(device);vkDestroyDevice(device,nullptr);});
    VulkanPipelineCache graphics_cache;
    VulkanPipelineCache* graphics_cache_ptr=nullptr;
    if(pipeline_cache_fixture) {
        require(graphics_cache.initialize(device,vkGetDeviceProcAddr,properties,std::filesystem::path(argv[1])/"pipeline-cache"),"Graphics cache unavailable");
        graphics_cache_ptr=&graphics_cache;
    }
    DEVICE(vkGetDeviceQueue);VkQueue queue{};vkGetDeviceQueue(device,family,0,&queue);
    {
        VulkanSpanPipeline spans;
        require(!spans.initialize(VK_NULL_HANDLE,vkGetDeviceProcAddr),"Null span device accepted");
        require(spans.initialize(device,vkGetDeviceProcAddr),spans.status().c_str());
        for(unsigned set=0;set<3;++set) require(spans.descriptor_layout(set)!=VK_NULL_HANDLE,"Missing span descriptor layout");
        require(spans.descriptor_layout(3)==VK_NULL_HANDLE,"Invalid span set accepted");
        require(!spans.record(VK_NULL_HANDLE,{},1),"Invalid span dispatch accepted");
        spans.close();spans.close();
        require(spans.initialize(device,vkGetDeviceProcAddr),spans.status().c_str());
        std::cout<<"Shared source span compute pipeline creation/reinitialization passed\n";
        require(spans.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::continuous_clip),spans.status().c_str());
        for(unsigned set=0;set<3;++set) require(spans.descriptor_layout(set)!=VK_NULL_HANDLE,"Missing clip descriptor layout");
        require(!spans.initialize(device,vkGetDeviceProcAddr,static_cast<SourceComputeStage>(99)),"Invalid source compute stage accepted");
        require(spans.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::continuous_clip),spans.status().c_str());
        std::cout<<"Shared continuous clipping compute pipeline creation/recovery passed\n";
        require(spans.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::colour_warp),spans.status().c_str());
        for(unsigned set=0;set<3;++set) require(spans.descriptor_layout(set)!=VK_NULL_HANDLE,"Missing warp descriptor layout");
        require(spans.stage()==SourceComputeStage::colour_warp,"Wrong warp stage retained");
        std::cout<<"Shared ordered colour-warp Vulkan pipeline creation passed\n";
        require(spans.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::warp_material),spans.status().c_str());
        for(unsigned set=0;set<3;++set) require(spans.descriptor_layout(set)!=VK_NULL_HANDLE,"Missing warp material descriptor layout");
        require(spans.stage()==SourceComputeStage::warp_material,"Wrong material stage retained");
        std::cout<<"Shared warp material Vulkan pipeline creation passed\n";
        require(spans.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::warp_expand),spans.status().c_str());
        for(unsigned set=0;set<3;++set) require(spans.descriptor_layout(set)!=VK_NULL_HANDLE,"Missing warp expansion descriptor layout");
        std::cout<<"Shared warp expansion Vulkan pipeline creation passed\n";
    }
    DEVICE(vkCreateImage);DEVICE(vkDestroyImage);DEVICE(vkGetImageMemoryRequirements);
    DEVICE(vkCreateImageView);DEVICE(vkDestroyImageView);
    DEVICE(vkAllocateMemory);DEVICE(vkFreeMemory);DEVICE(vkBindImageMemory);
    DEVICE(vkCreateBuffer);DEVICE(vkDestroyBuffer);DEVICE(vkGetBufferMemoryRequirements);DEVICE(vkBindBufferMemory);
    DEVICE(vkMapMemory);DEVICE(vkUnmapMemory);DEVICE(vkInvalidateMappedMemoryRanges);
    DEVICE(vkCreateCommandPool);DEVICE(vkDestroyCommandPool);DEVICE(vkAllocateCommandBuffers);
    DEVICE(vkResetCommandPool);DEVICE(vkBeginCommandBuffer);DEVICE(vkEndCommandBuffer);
    DEVICE(vkCmdPipelineBarrier);DEVICE(vkCmdCopyImageToBuffer);DEVICE(vkQueueSubmit);DEVICE(vkQueueWaitIdle);
    const auto allocate=[&](const VkMemoryRequirements& required,VkMemoryPropertyFlags flags) {
        uint32_t type=memory_properties.memoryTypeCount;
        for(uint32_t i=0;i<memory_properties.memoryTypeCount;++i)
            if((required.memoryTypeBits&(1U<<i)) && (memory_properties.memoryTypes[i].propertyFlags&flags)==flags) {type=i;break;}
        require(type<memory_properties.memoryTypeCount,"No compatible Vulkan memory");
        VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};info.allocationSize=required.size;info.memoryTypeIndex=type;
        VkDeviceMemory memory{};check(vkAllocateMemory(device,&info,nullptr,&memory),"Allocate memory");return memory;
    };
    // Menu layout captures need enough eye resolution to inspect localized
    // glyphs. Keep the pixel-exact synthetic suite at its original resolution.
    const uint32_t width=startup_menu?1536U:256U,height=width;
    VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType=VK_IMAGE_TYPE_2D;image_info.format=VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent={width,height,1};image_info.mipLevels=image_info.arrayLayers=1;
    image_info.samples=VK_SAMPLE_COUNT_1_BIT;image_info.tiling=VK_IMAGE_TILING_OPTIMAL;
    image_info.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImage image{};check(vkCreateImage(device,&image_info,nullptr,&image),"Create color image");
    VkMemoryRequirements required{};vkGetImageMemoryRequirements(device,image,&required);
    auto image_memory=std::make_shared<VkDeviceMemory>();
    cleanup.actions.push_back([=]{vkDestroyImage(device,image,nullptr);if(*image_memory) vkFreeMemory(device,*image_memory,nullptr);});
    *image_memory=allocate(required,0);check(vkBindImageMemory(device,image,*image_memory,0),"Bind color memory");
    VkBufferCreateInfo read_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    read_info.size=width*height*4;read_info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer readback{};check(vkCreateBuffer(device,&read_info,nullptr,&readback),"Create readback buffer");
    auto read_memory=std::make_shared<VkDeviceMemory>();
    cleanup.actions.push_back([=]{vkDestroyBuffer(device,readback,nullptr);if(*read_memory) vkFreeMemory(device,*read_memory,nullptr);});
    vkGetBufferMemoryRequirements(device,readback,&required);*read_memory=allocate(required,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    check(vkBindBufferMemory(device,readback,*read_memory,0),"Bind readback memory");
    VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool_info.queueFamilyIndex=family;
    VkCommandPool pool{};check(vkCreateCommandPool(device,&pool_info,nullptr,&pool),"Create copy pool");
    cleanup.actions.push_back([=]{vkDestroyCommandPool(device,pool,nullptr);});
    VkCommandBufferAllocateInfo buffer_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    buffer_info.commandPool=pool;buffer_info.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;buffer_info.commandBufferCount=1;
    VkCommandBuffer copy{};check(vkAllocateCommandBuffers(device,&buffer_info,&copy),"Allocate copy commands");
    VkBuffer produced_span_storage{};
    uint64_t produced_span_bytes=12288;
    uint32_t produced_span_lookup{};
    VkBuffer wave_span_storage{};uint64_t wave_span_bytes{};uint32_t wave_span_lookup{};
    VkBuffer tilted_span_storage{};uint64_t tilted_span_bytes{};uint32_t tilted_span_lookup{};
    std::shared_ptr<VulkanSourceModel> solid_cover_model,wave_cover_model,tilted_cover_model;
    std::shared_ptr<VulkanSourceModel> occurrence_cover_model;
    std::shared_ptr<VulkanSourceModel> textured_occurrence_model;
    std::shared_ptr<VulkanSourceStorage> warp_draw_storage;
    std::shared_ptr<VulkanSourceModel> warp_draw_model;
    SourceModelPackets warp_scene_input;
    std::shared_ptr<VulkanSourceScene> warp_scene;
    uint32_t warp_draw_lookup{};
    std::array<std::shared_ptr<VulkanSourceScene>,3> combined_source_scenes;
    std::shared_ptr<VulkanSourceScene> shadow_filtered_scene;
    std::array<SourceModelPackets,3> combined_source_inputs;
    {
        VulkanSpanPipeline axis;
        require(axis.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::axis),axis.status().c_str());
        VulkanSourceStorage storage;
        require(storage.initialize(device,vkGetDeviceProcAddr,memory_properties,1536),storage.status().c_str());
        VulkanSourceStorage owned_storage;
        require(owned_storage.initialize(device,vkGetDeviceProcAddr,memory_properties,1536),owned_storage.status().c_str());
        VulkanAxisBindings owned_bindings;
        DEVICE(vkCreateDescriptorPool);DEVICE(vkDestroyDescriptorPool);
        DEVICE(vkAllocateDescriptorSets);DEVICE(vkUpdateDescriptorSets);
        std::array<VkDescriptorPoolSize,2> counts{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,5},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1}}};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pi.maxSets=3;pi.poolSizeCount=2;pi.pPoolSizes=counts.data();
        VkDescriptorPool descriptors{};check(vkCreateDescriptorPool(device,&pi,nullptr,&descriptors),"Create axis descriptors");
        cleanup.actions.push_back([=]{vkDestroyDescriptorPool(device,descriptors,nullptr);});
        std::array<VkDescriptorSetLayout,3> layouts{axis.descriptor_layout(0),axis.descriptor_layout(1),axis.descriptor_layout(2)};
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool=descriptors;ai.descriptorSetCount=3;ai.pSetLayouts=layouts.data();
        std::array<VkDescriptorSet,3> sets{};check(vkAllocateDescriptorSets(device,&ai,sets.data()),"Allocate axis descriptors");
        std::array<VkDescriptorBufferInfo,6> buffers{};
        std::array<VkWriteDescriptorSet,6> writes{};
        constexpr std::array<VkDeviceSize,6> sizes{128,16,128,64,64,48};
        for(unsigned i=0;i<6;++i) {
            buffers[i]={storage.buffer(),i*256U,sizes[i]};
            auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet=sets[i<3?0:i<5?1:2];w.dstBinding=i<3?i:i<5?i-3:0;
            w.descriptorCount=1;w.descriptorType=i==5?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;w.pBufferInfo=&buffers[i];
        }
        vkUpdateDescriptorSets(device,6,writes.data(),0,nullptr);
        for(unsigned fixture=0;fixture<3;++fixture) {
            std::array<uint32_t,384> data{};
            const auto put=[&](size_t index,float value){data[index]=std::bit_cast<uint32_t>(value);};
            // Authored extrema groups average transformed vertices, not local
            // coordinates. Asymmetric depth catches averaging projected XY.
            const float points[4][3]{{-40,20,256},{-20,40,768},{40,-20,256},{20,-40,768}};
            for(unsigned i=0;i<4;++i) {
                for(unsigned c=0;c<3;++c) put(i*8+c,points[i][c]);
                put(i*8+3,1);data[64+i]=i;
            }
            starfox::assets::Shape authored;
            authored.vertices={{0,0,20},{1,0,20},{0,0,-20},{1,0,-20}};
            authored.faces={{-1,0,{0,0,127},{0,1,2}}};
            starfox::render::RenderPose axis_pose;axis_pose.collapse_to_axis_line=true;
            axis_pose.vanish_x=128;axis_pose.vanish_y=112;
            starfox::render::RenderSettings axis_settings;axis_settings.focal_length=256;
            SourceAxisInputs axis_inputs;std::string axis_error;
            require(prepare_source_axis_inputs(authored,axis_pose,axis_settings,4,false,axis_inputs,axis_error),axis_error.c_str());
            std::copy(axis_inputs.indices.begin(),axis_inputs.indices.end(),data.begin()+64);
            std::memcpy(data.data()+320,&axis_inputs.settings,sizeof(axis_inputs.settings));
            if(fixture==1) data[64]=99; // invalid member rejects only group zero
            if(fixture==2) data[325]=0; // empty group must not retain prior output
            if(fixture==0) {
                SourceSpanArenaLayout source;source.bytes=384;
                source.regions[size_t(SourceSpanRegion::points)]={0,128};
                source.regions[size_t(SourceSpanRegion::residuals)]={256,128};
                SourceAxisArenaLayout arena;
                require(layout_source_axis_inputs(axis_inputs,384,256,256,65536,65536,arena,axis_error),axis_error.c_str());
                auto truncated=source;truncated.regions[size_t(SourceSpanRegion::points)].size=32;
                require(!owned_bindings.initialize(device,vkGetDeviceProcAddr,owned_storage.buffer(),1536,truncated,arena,axis_inputs,axis),"Axis accepted truncated projection");
                require(owned_bindings.initialize(device,vkGetDeviceProcAddr,owned_storage.buffer(),1536,source,arena,axis_inputs,axis),owned_bindings.status().c_str());
            }
            auto owned_data=data;
            std::copy_n(data.begin()+64,4,owned_data.begin()+128);
            std::fill_n(owned_data.begin()+64,32,0);
            require(owned_storage.upload(0,std::as_bytes(std::span(owned_data))),owned_storage.status().c_str());
            if(fixture==0) {
                SourceAxisArenaLayout arena;
                require(layout_source_axis_inputs(axis_inputs,384,256,256,65536,65536,arena,axis_error),axis_error.c_str());
                std::vector<SourceSpanInputWrite> input_writes;
                require(source_axis_input_writes(axis_inputs,arena,input_writes,axis_error),axis_error.c_str());
                for(const auto& write:input_writes)
                    require(owned_storage.upload(write.offset,write.bytes),owned_storage.status().c_str());
            }
            require(storage.upload(0,std::as_bytes(std::span(data))),storage.status().c_str());
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            check(vkBeginCommandBuffer(copy,&begin),"Begin axis dispatch");
            require(!axis.record(copy,sets,1) && !axis.record(copy,sets,3),"Axis accepted invalid endpoint count");
            require(axis.record(copy,sets,2),"Record axis reduction");
            require(owned_bindings.record(copy),"Record owned axis bindings");
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
            check(vkEndCommandBuffer(copy),"End axis dispatch");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
            check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit axis");check(vkQueueWaitIdle(queue),"Wait axis");
            require(storage.readback(0,std::as_writable_bytes(std::span(data))),storage.status().c_str());
            require(owned_storage.readback(0,std::as_writable_bytes(std::span(owned_data))),owned_storage.status().c_str());
            require(std::equal(data.begin()+192,data.begin()+208,owned_data.begin()+192),"Owned axis endpoints differ from direct binding");
            const auto get=[&](size_t index){return std::bit_cast<float>(data[index]);};
            if(fixture==0) require(get(192)==-30 && get(193)==30 && get(194)==512
                && get(196)==113 && get(197)==127,"Axis first mean/projection mismatch");
            else require(get(195)==-1,"Invalid axis group retained an endpoint");
            require(get(200)==30 && get(201)==-30 && get(202)==512
                && get(204)==143 && get(205)==97,"Axis second mean/projection mismatch");
        }
        for(bool combined:{false,true}) {
            starfox::assets::Shape authored;authored.vertices={{0,0,20},{2,0,20},{0,2,-20},{2,2,-20}};
            authored.faces={{-1,0,{0,0,127},{0,1,2}}};
            starfox::render::RenderPose pose;pose.continuous_geometry=true;pose.z=512;
            SourceSpanModel model;std::string error;
            require(prepare_source_span_model(authored,pose,{},256,192,model,error,true),error.c_str());
            pose.collapse_to_axis_line=true;
            SourceAxisInputs input;
            require(prepare_source_axis_inputs(authored,pose,{},4,false,input,error),error.c_str());
            SourceWarpInputs warp_input;
            std::array<VulkanSpanPipeline,3> warp_producers;
            std::array<const VulkanSpanPipeline*,3> warp_pipelines{};
            if(combined) {
                pose.colour_warp=true;pose.projected_points_address=uint16_t(0-24);
                require(prepare_source_axis_model(authored,pose,{},256,192,model,input,error),error.c_str());
                require(prepare_source_warp_inputs(authored,pose,{},model.projection,model.bsp,warp_input,error),error.c_str());
                model.warp_expanded=true;
                model.graphics_unclipped=false;model.graphics_palette_flags|=4U;
                model.faces=warp_input.templates;model.spans.ordered_mode=2;
                model.spans.polygon_count=model.spans.count;
                model.clip_settings[0]=model.spans.count;model.clip_settings[2]=model.spans.count*32U;
                for(unsigned i=0;i<3;++i) {
                    require(warp_producers[i].initialize(device,vkGetDeviceProcAddr,static_cast<SourceComputeStage>(i+5)),warp_producers[i].status().c_str());
                    warp_pipelines[i]=&warp_producers[i];
                }
            }
            std::array<VulkanSpanPipeline,5> producers;
            std::array<const VulkanSpanPipeline*,5> pipelines{};
            for(unsigned i=combined?0:2;i<5;++i) {
                require(producers[i].initialize(device,vkGetDeviceProcAddr,static_cast<SourceComputeStage>(i)),producers[i].status().c_str());
                pipelines[i]=&producers[i];
            }
            VulkanSourceModel resident;
            const bool initialized=resident.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,model,pipelines,
                combined?&warp_input:nullptr,combined?&warp_pipelines:nullptr,&input,&axis);
            require(initialized,resident.status().c_str());
            require(resident.update(model,combined?&warp_input:nullptr,&input),resident.status().c_str());
            require(resident.update(model,combined?&warp_input:nullptr,&input) && resident.updated_input_bytes()==0,"Unchanged axis model uploaded inputs");
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            check(vkBeginCommandBuffer(copy,&begin),"Begin resident axis");
            require(resident.record(copy),resident.status().c_str());
            check(vkEndCommandBuffer(copy),"End resident axis");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
            check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit resident axis");check(vkQueueWaitIdle(queue),"Wait resident axis");
            resident.compute_completed();
            if(combined) {
                std::array<uint32_t,4> decoded{};
                require(resident.readback_warp(SourceWarpRegion::decoded,std::as_writable_bytes(std::span(decoded))),"Read combined axis material");
                const auto base=warp_input.shading.settings.colour_base;
                require(decoded[0]==base+1 && decoded[1]==base && decoded[2]==1 && decoded[3]==0xffffffffU,
                    "Combined axis warp did not decode the first random line material");
            }
            std::array<float,16> endpoints{};
            require(resident.readback_axis(SourceAxisRegion::endpoints,std::as_writable_bytes(std::span(endpoints))),"Read resident endpoints");
            require(endpoints[0]==1 && endpoints[1]==0 && endpoints[2]==532
                && endpoints[8]==1 && endpoints[9]==2 && endpoints[10]==492,"Resident axis transformed endpoints differ");
            SceneVertex material{};material.color[1]=material.color[3]=1;
            std::vector<SceneVertex> endpoint_templates;
            require(resident.cover_geometry(256,material,endpoint_templates),resident.status().c_str());
            require(endpoint_templates.size()==2 && endpoint_templates[0].texture[1]==0
                && endpoint_templates[1].texture[1]==1 && endpoint_templates[0].texture[3]==67108864U
                && endpoint_templates[0].texture[0]==resident.axis_layout()[SourceAxisRegion::endpoints].offset/4,
                "Axis draw templates do not address resident endpoints");
        }
        std::cout<<"Native Vulkan axis reduction/projection, owned model and invalid-group tests passed\n";
    }
    {
        VulkanSpanPipeline warp;
        require(warp.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::colour_warp),warp.status().c_str());
        VulkanSourceStorage storage;
        require(storage.initialize(device,vkGetDeviceProcAddr,memory_properties,291840),storage.status().c_str());
        std::vector<uint32_t> data(291840/4);
        std::fill(data.begin()+832,data.begin()+66368,UINT32_MAX);data[832+29351]=5;
        // Repeated face zero must consume a different random word each time.
        data[0]=0;data[1]=1;data[2]=0;data[64]=3;
        data[128+2]=0;data[128+4+2]=1;
        data[192]=1;data[193]=0;
        data[384]=4;data[385]=2;data[386]=2;data[387]=1234;
        require(storage.upload(0,std::as_bytes(std::span(data))),storage.status().c_str());
        DEVICE(vkCreateDescriptorPool);DEVICE(vkDestroyDescriptorPool);
        DEVICE(vkAllocateDescriptorSets);DEVICE(vkUpdateDescriptorSets);
        std::array<VkDescriptorPoolSize,2> counts{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,37},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,5}}};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pi.maxSets=15;pi.poolSizeCount=2;pi.pPoolSizes=counts.data();
        VkDescriptorPool descriptors{};check(vkCreateDescriptorPool(device,&pi,nullptr,&descriptors),"Create warp descriptors");
        cleanup.actions.push_back([=]{vkDestroyDescriptorPool(device,descriptors,nullptr);});
        std::array<VkDescriptorSetLayout,3> layouts{warp.descriptor_layout(0),warp.descriptor_layout(1),warp.descriptor_layout(2)};
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool=descriptors;ai.descriptorSetCount=3;ai.pSetLayouts=layouts.data();
        std::array<VkDescriptorSet,3> sets{};check(vkAllocateDescriptorSets(device,&ai,sets.data()),"Allocate warp descriptors");
        std::array<VkDescriptorBufferInfo,7> buffers{};
        std::array<VkWriteDescriptorSet,7> writes{};
        constexpr std::array<VkDeviceSize,7> sizes{16,8,32,8,16,8,16};
        for(unsigned i=0;i<7;++i) {
            buffers[i]={storage.buffer(),i*256U,sizes[i]};
            auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet=sets[i<4?0:i<6?1:2];w.dstBinding=i<4?i:i<6?i-4:0;
            w.descriptorCount=1;w.descriptorType=i==6?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;w.pBufferInfo=&buffers[i];
        }
        vkUpdateDescriptorSets(device,7,writes.data(),0,nullptr);
        VulkanSpanPipeline material;
        require(material.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::warp_material),material.status().c_str());
        std::array<VkDescriptorSetLayout,3> material_layouts{material.descriptor_layout(0),material.descriptor_layout(1),material.descriptor_layout(2)};
        ai.pSetLayouts=material_layouts.data();
        std::array<VkDescriptorSet,3> material_sets{};
        check(vkAllocateDescriptorSets(device,&ai,material_sets.data()),"Allocate warp material descriptors");
        std::array<VkDescriptorBufferInfo,8> material_buffers{};
        std::array<VkWriteDescriptorSet,8> material_writes{};
        constexpr std::array<VkDeviceSize,8> material_offsets{1024,0,1792,2048,2304,3328,2816,3072};
        constexpr std::array<VkDeviceSize,8> material_sizes{16,16,32,4,4,262144,64,64};
        for(unsigned i=0;i<8;++i) {
            material_buffers[i]={storage.buffer(),material_offsets[i],material_sizes[i]};
            auto& w=material_writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet=material_sets[i<6?0:i==6?1:2];w.dstBinding=i<6?i:0;
            w.descriptorCount=1;w.descriptorType=i==7?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            w.pBufferInfo=&material_buffers[i];
        }
        vkUpdateDescriptorSets(device,8,material_writes.data(),0,nullptr);
        data[768]=4;data[769]=2; // occurrence count and authored face count
        VulkanSpanPipeline expand;
        require(expand.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::warp_expand),expand.status().c_str());
        std::array<VkDescriptorSetLayout,3> expand_layouts{expand.descriptor_layout(0),expand.descriptor_layout(1),expand.descriptor_layout(2)};
        ai.pSetLayouts=expand_layouts.data();std::array<VkDescriptorSet,3> expand_sets{};
        check(vkAllocateDescriptorSets(device,&ai,expand_sets.data()),"Allocate warp expansion descriptors");
        constexpr std::array<VkDeviceSize,12> expand_offsets{0,1280,512,265472,265728,2816,265984,266240,266496,266752,268800,269312};
        constexpr std::array<VkDeviceSize,12> expand_sizes{16,8,32,48,192,64,96,32,64,2048,384,32};
        std::array<VkDescriptorBufferInfo,12> expand_buffers{};std::array<VkWriteDescriptorSet,12> expand_writes{};
        for(unsigned i=0;i<12;++i) {
            expand_buffers[i]={storage.buffer(),expand_offsets[i],expand_sizes[i]};
            auto& w=expand_writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet=expand_sets[i<8?0:i<11?1:2];w.dstBinding=i<8?i:i<11?i-8:0;
            w.descriptorCount=1;w.descriptorType=i==11?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            w.pBufferInfo=&expand_buffers[i];
        }
        vkUpdateDescriptorSets(device,12,expand_writes.data(),0,nullptr);
        data[129]=data[133]=3;
        for(unsigned corner=0;corner<3;++corner) data[265472/4+corner*4]=corner;
        data[265984/4+20+1]=1;data[265984/4+20+2]=1;
        data[266240/4+2]=1;data[266240/4+4]=1;data[266240/4+5]=1;
        const std::array<uint32_t,8> expansion_settings{4,2,3,6,4,0,0,0};
        std::copy(expansion_settings.begin(),expansion_settings.end(),data.begin()+269312/4);
        VulkanSpanPipeline occurrence_clip,occurrence_spans;
        require(occurrence_clip.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::continuous_clip),occurrence_clip.status().c_str());
        require(occurrence_spans.initialize(device,vkGetDeviceProcAddr),occurrence_spans.status().c_str());
        const auto bind_occurrences=[&](const VulkanSpanPipeline& pipeline,
            std::span<const VkDeviceSize> offsets,std::span<const VkDeviceSize> ranges,unsigned inputs,unsigned outputs) {
            std::array<VkDescriptorSetLayout,3> ls{pipeline.descriptor_layout(0),pipeline.descriptor_layout(1),pipeline.descriptor_layout(2)};
            auto allocation=ai;allocation.pSetLayouts=ls.data();
            std::array<VkDescriptorSet,3> result{};
            check(vkAllocateDescriptorSets(device,&allocation,result.data()),"Allocate occurrence consumers");
            std::vector<VkDescriptorBufferInfo> bs(offsets.size());std::vector<VkWriteDescriptorSet> ws(offsets.size());
            for(unsigned i=0;i<offsets.size();++i) {
                bs[i]={storage.buffer(),offsets[i],ranges[i]};
                auto& w=ws[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.dstSet=result[i<inputs?0:i<inputs+outputs?1:2];
                w.dstBinding=i<inputs?i:i<inputs+outputs?i-inputs:0;
                w.descriptorCount=1;w.descriptorType=i==inputs+outputs?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                w.pBufferInfo=&bs[i];
            }
            vkUpdateDescriptorSets(device,uint32_t(ws.size()),ws.data(),0,nullptr);return result;
        };
        const std::array<VkDeviceSize,8> oc_offsets{269568,266752,266496,768,269824,270080,270336,278784};
        const std::array<VkDeviceSize,8> oc_sizes{96,2048,64,8,64,4,8256,32};
        const auto oc_sets=bind_occurrences(occurrence_clip,oc_offsets,oc_sizes,6,1);
        const std::array<VkDeviceSize,7> os_offsets{270336,268800,0,1280,279040,291328,291584};
        const std::array<VkDeviceSize,7> os_sizes{8256,384,16,8,12288,4,64};
        const auto os_sets=bind_occurrences(occurrence_spans,os_offsets,os_sizes,4,2);
        const std::array<std::array<float,2>,3> positions{{{4,4},{4,12},{12,12}}};
        for(unsigned i=0;i<3;++i) {
            const std::array<float,8> p{positions[i][0]-16,positions[i][1]-16,256,1,positions[i][0],positions[i][1],256,1};
            std::memcpy(data.data()+269568/4+i*8,p.data(),sizeof(p));
        }
        for(unsigned i=0;i<4;++i) {
            const std::array<float,4> p{16,16,256,1};
            std::memcpy(data.data()+269824/4+i*4,p.data(),sizeof(p));
        }
        const std::array<uint32_t,8> oc_settings{4,3,128,2,32,32,4,0};
        std::memcpy(data.data()+278784/4,oc_settings.data(),sizeof(oc_settings));
        SourceSpanSettings os_settings;os_settings.count=os_settings.polygon_count=4;
        os_settings.width=os_settings.height=32;os_settings.fractional=1;os_settings.ordered_mode=2;
        require(source_span_buffer_sizes(os_settings).has_value(),"Invalid occurrence span settings");
        std::memcpy(data.data()+291584/4,&os_settings,sizeof(os_settings));
        // Re-run valid geometry after failed and empty submissions on the
        // same arena: live effects must recover without resource recreation.
        for(unsigned fixture=0;fixture<6;++fixture) {
        data[0]=0;data[1]=1;data[2]=0;data[64]=3;data[65]=0;
        data[387]=1234U|(fixture==1?0x80000000U:0U);
        if(fixture==2) data[65]=1; // traversal failure
        if(fixture==3) data[1]=2; // out-of-range ordered face
        if(fixture==4) data[64]=0; // empty reuse clears previous output
        std::fill(data.begin()+256,data.begin()+260,0x12345678U);
        require(storage.upload(0,std::as_bytes(std::span(data))),storage.status().c_str());
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(copy,&begin),"Begin warp dispatch");
        require(!warp.record(copy,sets,2),"Serial warp accepted multiple workgroups");
        require(warp.record(copy,sets,1),"Record ordered warp");
        VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
        require(material.record(copy,material_sets,4),"Record warp material decode");
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
        require(expand.record(copy,expand_sets,4),"Record warp geometry expansion");
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
        require(occurrence_clip.record(copy,oc_sets,4),"Clip expanded warp occurrences");
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
        require(occurrence_spans.record(copy,os_sets,4),"Generate expanded warp spans");
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        check(vkEndCommandBuffer(copy),"End warp dispatch");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
        check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit warp");check(vkQueueWaitIdle(queue),"Wait warp");
        require(storage.readback(0,std::as_writable_bytes(std::span(data))),storage.status().c_str());
        if(fixture<2 || fixture==5) {
            require(data[320]==3 && data[321]==0,"Warp traversal result mismatch");
            // Source byte-swap/rotate/add sequence, seed 1234: 29351,14600,63021.
            require(data[256]==29351 && data[257]==(fixture==1?14600U:UINT32_MAX)
                && data[258]==(fixture==1?63021U:14600U) && data[259]==UINT32_MAX,
                "Warp exact descriptor sequence mismatch");
            require(data[704]==15 && data[705]==15 && data[706]==0 && data[707]==5,"First warp texture material mismatch");
            const unsigned last=712;
            require(data[last]==(fixture==1?13U:8U) && data[last+1]==(fixture==1?13U:0U)
                && data[last+2]==(fixture==1?0U:1U) && data[last+3]==UINT32_MAX,"Repeated face material mismatch");
            if(fixture!=1) require(data[711]==0xfffffffeU,"Hidden warp material emitted");
            const unsigned polygon=266496/4,corners=266752/4,commands=268800/4;
            require(data[polygon]==0 && data[polygon+1]==3 && (data[polygon+3]&1U)==1,"Warp texture polygon expansion mismatch");
            require(data[polygon+8]==64 && data[polygon+9]==3 && (data[polygon+11]&1U)==0,"Repeated face did not get independent polygon");
            require(data[corners+4]==1 && data[corners+5]==1 && data[corners+6]==0,"Warp texture UV expansion mismatch");
            require(data[corners+256+8]==2 && data[corners+256+9]==0,"Repeated solid corners mismatch");
            require(data[commands+4]==15 && data[commands+21]==1 && data[commands+48+4]==(fixture==1?13U:8U),"Warp expanded materials mismatch");
            for(unsigned slot:{0U,2U}) {
                unsigned emitted=0;
                for(unsigned row=0;row<32;++row) {
                    const auto at=279040/4+(slot*32+row)*24;
                    if(int32_t(data[at+2])<=int32_t(data[at])) continue;
                    ++emitted;
                    require(data[at+4]==(slot==0?15U:fixture==1?13U:8U),"Warp span used another occurrence material");
                    require(data[at+21]==(slot==0?1U:0U),"Warp span lost occurrence texture mode");
                }
                require(emitted>0,"Expanded warp occurrence emitted no spans");
            }
        } else {
            require(data[320]==0 && data[321]==(fixture==4?0U:1U),"Warp invalid/empty traversal mismatch");
            for(unsigned i=256;i<260;++i) require(data[i]==UINT32_MAX,"Warp retained stale output");
            for(unsigned i=0;i<4;++i) require(data[704+i*4+3]==0xfffffffeU,"Stale decoded warp material retained");
            for(unsigned i=0;i<16;++i) require(data[266496/4+i]==0,"Stale expanded warp polygon retained");
            for(unsigned i=0;i<12288/4;++i) require(data[279040/4+i]==0,"Failed warp retained stale span commands");
        }
        check(vkResetCommandPool(device,pool,0),"Reset warp commands");
        }
        std::cout<<"Ordered Vulkan warp through clipping/spans: exact PRNG, repeated materials/textures, invalid/empty recovery passed\n";
    }
    {
        VulkanSpanPipeline spans;
        require(spans.initialize(device,vkGetDeviceProcAddr),spans.status().c_str());
        DEVICE(vkCreateDescriptorPool);DEVICE(vkDestroyDescriptorPool);
        DEVICE(vkAllocateDescriptorSets);DEVICE(vkUpdateDescriptorSets);
        auto arena=std::make_shared<VulkanSourceStorage>();
        require(!arena->initialize(device,vkGetDeviceProcAddr,memory_properties,3),"Unaligned arena accepted");
        require(arena->initialize(device,vkGetDeviceProcAddr,memory_properties,12288),arena->status().c_str());
        VkBuffer storage=arena->buffer();
        produced_span_storage=storage;
        cleanup.actions.push_back([arena]{arena->close();});
        const std::array<std::byte,4> invalid_upload{};
        require(!arena->upload(12286,invalid_upload),"Overrunning arena upload accepted");
        require(!arena->upload(UINT64_MAX,invalid_upload),"Wrapping arena upload accepted");
        // Offsets satisfy Vulkan's guaranteed storage/uniform alignment limits.
        const std::array<VkDeviceSize,7> offsets{0,2304,2560,2816,3072,6144,6400};
        const std::array<VkDeviceSize,7> sizes{2064,96,4,8,3072,128,64};
        const std::array<VkDescriptorPoolSize,2> pool_sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,26},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,5}}};
        VkDescriptorPoolCreateInfo descriptor_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        descriptor_info.maxSets=15;descriptor_info.poolSizeCount=2;descriptor_info.pPoolSizes=pool_sizes.data();
        VkDescriptorPool descriptors{};check(vkCreateDescriptorPool(device,&descriptor_info,nullptr,&descriptors),"Create span descriptors");
        cleanup.actions.push_back([=]{vkDestroyDescriptorPool(device,descriptors,nullptr);});
        const std::array<VkDescriptorSetLayout,3> layouts{spans.descriptor_layout(0),spans.descriptor_layout(1),spans.descriptor_layout(2)};
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool=descriptors;allocation.descriptorSetCount=3;allocation.pSetLayouts=layouts.data();
        std::array<VkDescriptorSet,3> sets{};
        check(vkAllocateDescriptorSets(device,&allocation,sets.data()),"Allocate span descriptors");
        std::array<VkDescriptorBufferInfo,7> buffers{};
        std::array<VkWriteDescriptorSet,7> writes{};
        for(unsigned i=0;i<7;++i) {
            buffers[i]={storage,offsets[i],sizes[i]};
            auto& write=writes[i];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet=sets[i<4?0:i<6?1:2];write.dstBinding=i<4?i:i<6?i-4:0;
            write.descriptorCount=1;write.descriptorType=i==6?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo=&buffers[i];
        }
        vkUpdateDescriptorSets(device,7,writes.data(),0,nullptr);
        VulkanSpanPipeline clip;
        require(clip.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::continuous_clip),clip.status().c_str());
        const std::array<VkDescriptorSetLayout,3> clip_layouts{clip.descriptor_layout(0),clip.descriptor_layout(1),clip.descriptor_layout(2)};
        allocation.pSetLayouts=clip_layouts.data();
        std::array<VkDescriptorSet,3> clip_sets{};
        check(vkAllocateDescriptorSets(device,&allocation,clip_sets.data()),"Allocate clip descriptors");
        const std::array<VkDeviceSize,8> clip_offsets{6656,6912,7168,7424,7680,7936,0,8192};
        const std::array<VkDeviceSize,8> clip_sizes{128,64,16,4,16,128,2064,32};
        std::array<VkDescriptorBufferInfo,8> clip_buffers{};
        std::array<VkWriteDescriptorSet,8> clip_writes{};
        for(unsigned i=0;i<8;++i) {
            clip_buffers[i]={storage,clip_offsets[i],clip_sizes[i]};
            auto& write=clip_writes[i];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet=clip_sets[i<6?0:i==6?1:2];write.dstBinding=i<6?i:0;
            write.descriptorCount=1;write.descriptorType=i==7?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo=&clip_buffers[i];
        }
        vkUpdateDescriptorSets(device,8,clip_writes.data(),0,nullptr);
        VulkanSpanPipeline project;
        require(project.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::continuous_projection),project.status().c_str());
        const std::array<VkDescriptorSetLayout,3> project_layouts{project.descriptor_layout(0),project.descriptor_layout(1),project.descriptor_layout(2)};
        allocation.pSetLayouts=project_layouts.data();
        std::array<VkDescriptorSet,3> project_sets{};
        check(vkAllocateDescriptorSets(device,&allocation,project_sets.data()),"Allocate projection descriptors");
        const std::array<VkDeviceSize,5> project_offsets{8448,8704,6656,7936,9984};
        const std::array<VkDeviceSize,5> project_sizes{64,320,128,128,16};
        std::array<VkDescriptorBufferInfo,5> project_buffers{};
        std::array<VkWriteDescriptorSet,5> project_writes{};
        for(unsigned i=0;i<5;++i) {
            project_buffers[i]={storage,project_offsets[i],project_sizes[i]};
            auto& write=project_writes[i];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet=project_sets[i<2?0:i<4?1:2];write.dstBinding=i<2?i:i<4?i-2:0;
            write.descriptorCount=1;write.descriptorType=i==4?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo=&project_buffers[i];
        }
        vkUpdateDescriptorSets(device,5,project_writes.data(),0,nullptr);
        VulkanSpanPipeline visibility;
        require(visibility.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::continuous_visibility),visibility.status().c_str());
        const std::array<VkDescriptorSetLayout,3> visibility_layouts{visibility.descriptor_layout(0),visibility.descriptor_layout(1),visibility.descriptor_layout(2)};
        allocation.pSetLayouts=visibility_layouts.data();
        std::array<VkDescriptorSet,3> visibility_sets{};
        check(vkAllocateDescriptorSets(device,&allocation,visibility_sets.data()),"Allocate visibility descriptors");
        const std::array<VkDeviceSize,4> visibility_offsets{6656,10240,7424,10496};
        const std::array<VkDeviceSize,4> visibility_sizes{128,16,4,16};
        std::array<VkDescriptorBufferInfo,4> visibility_buffers{};
        std::array<VkWriteDescriptorSet,4> visibility_writes{};
        for(unsigned i=0;i<4;++i) {
            visibility_buffers[i]={storage,visibility_offsets[i],visibility_sizes[i]};
            auto& write=visibility_writes[i];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet=visibility_sets[i<2?0:i==2?1:2];write.dstBinding=i<2?i:0;
            write.descriptorCount=1;write.descriptorType=i==3?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo=&visibility_buffers[i];
        }
        vkUpdateDescriptorSets(device,4,visibility_writes.data(),0,nullptr);
        VulkanSpanPipeline bsp;
        require(bsp.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::bsp),bsp.status().c_str());
        const std::array<VkDescriptorSetLayout,3> bsp_layouts{bsp.descriptor_layout(0),bsp.descriptor_layout(1),bsp.descriptor_layout(2)};
        allocation.pSetLayouts=bsp_layouts.data();
        std::array<VkDescriptorSet,3> bsp_sets{};
        check(vkAllocateDescriptorSets(device,&allocation,bsp_sets.data()),"Allocate BSP descriptors");
        const std::array<VkDeviceSize,7> bsp_offsets{10752,7424,11008,11264,2560,2816,11520};
        const std::array<VkDeviceSize,7> bsp_sizes{32,4,4,16,4,8,32};
        std::array<VkDescriptorBufferInfo,7> bsp_buffers{};
        std::array<VkWriteDescriptorSet,7> bsp_writes{};
        for(unsigned i=0;i<7;++i) {
            bsp_buffers[i]={storage,bsp_offsets[i],bsp_sizes[i]};
            auto& write=bsp_writes[i];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet=bsp_sets[i<4?0:i<6?1:2];write.dstBinding=i<4?i:i<6?i-4:0;
            write.descriptorCount=1;write.descriptorType=i==6?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo=&bsp_buffers[i];
        }
        vkUpdateDescriptorSets(device,7,bsp_writes.data(),0,nullptr);
        SourceSpanArenaLayout source_arena;source_arena.bytes=12288;
        source_arena.regions[static_cast<size_t>(SourceSpanRegion::graphics_headers)]={9216,16};
        source_arena.regions[static_cast<size_t>(SourceSpanRegion::graphics_lookup)]={9472,64};
        source_arena.regions[static_cast<size_t>(SourceSpanRegion::palette)]={9728,4}; // Disabled legacy fixture.
        source_arena.regions[static_cast<size_t>(SourceSpanRegion::texture_bytes)]={9984,4};
        source_arena.regions[static_cast<size_t>(SourceSpanRegion::graphics_polygons)]={10240,4};
        using Region=SourceSpanRegion;
        const auto regions=[&](std::initializer_list<Region> names,const auto& region_offsets,const auto& region_sizes) {
            unsigned i=0;for(auto name:names) {
                source_arena.regions[static_cast<size_t>(name)]={region_offsets[i],region_sizes[i]};++i;
            }
        };
        regions({Region::clipped,Region::materials,Region::order,Region::results,Region::commands,Region::masks,Region::span_settings},offsets,sizes);
        regions({Region::points,Region::corners,Region::polygons,Region::visibility,Region::projection_parameters,Region::residuals,Region::clipped,Region::clip_settings},clip_offsets,clip_sizes);
        regions({Region::vertices,Region::poses,Region::points,Region::residuals,Region::projection_settings},project_offsets,project_sizes);
        regions({Region::points,Region::triples,Region::visibility,Region::visibility_settings},visibility_offsets,visibility_sizes);
        regions({Region::nodes,Region::visibility,Region::face_ids,Region::trees,Region::order,Region::results,Region::bsp_settings},bsp_offsets,bsp_sizes);
        VulkanSourceBindings source_bindings;
        const std::array<const VulkanSpanPipeline*,5> source_pipelines{&spans,&clip,&project,&visibility,&bsp};
        auto invalid_arena=source_arena;invalid_arena.regions[0].offset=UINT64_MAX;
        require(!source_bindings.initialize(device,vkGetDeviceProcAddr,storage,12288,invalid_arena,source_pipelines),"Invalid source binding range accepted");
        auto invalid_pipelines=source_pipelines;invalid_pipelines[0]=&clip;
        require(!source_bindings.initialize(device,vkGetDeviceProcAddr,storage,12288,source_arena,invalid_pipelines),"Incorrect source pipeline accepted");
        require(source_bindings.initialize(device,vkGetDeviceProcAddr,storage,12288,source_arena,source_pipelines),source_bindings.status().c_str());
        sets=source_bindings.sets(SourceComputeStage::spans);
        clip_sets=source_bindings.sets(SourceComputeStage::continuous_clip);
        project_sets=source_bindings.sets(SourceComputeStage::continuous_projection);
        visibility_sets=source_bindings.sets(SourceComputeStage::continuous_visibility);
        bsp_sets=source_bindings.sets(SourceComputeStage::bsp);
        for(unsigned scenario:{7U,9U,0U,1U,2U,3U,4U,5U,6U,8U,11U,10U}) for(unsigned mode:{1U,2U,3U,4U,5U,0U}) {
            const bool chained=scenario!=0;
            const uint32_t expected_left=scenario==2?0:4;
            std::array<uint32_t,12288/4> host_words{};
            auto* words=host_words.data();
            words[9216/4]=3072/4;words[9216/4+1]=6144/4;words[9216/4+2]=32;
            words[0]=4;
            const std::array<std::array<uint32_t,2>,4> points{{{4,4},{4,12},{12,12},{12,4}}};
            for(unsigned i=0;i<4;++i) {words[4+i*4]=points[i][0];words[5+i*4]=points[i][1];}
            words[2304/4+4]=words[2304/4+5]=7; // even/odd material
            constexpr std::array<uint32_t,6> effects{0,1,2,262144,65536,262144|131072};
            words[2304/4+23]=effects[mode];
            const bool repeated=mode==3 || mode==5;
            SourceSpanSettings settings;
            settings.count=settings.polygon_count=1;settings.width=settings.height=32;
            settings.mask_enabled=repeated;settings.mask_stride=4;
            if(scenario>=8) {
                settings.ordered_mode=1;
                words[10752/4]=scenario==9?1:0;
                words[10752/4+1]=words[10752/4+2]=UINT32_MAX;words[10752/4+4]=1;
                words[11264/4+2]=1;words[11264/4+3]=16;
                const std::array<uint32_t,8> bsp_settings{1,1,1,1,1,0,0,0};
                std::memcpy(words+11520/4,bsp_settings.data(),sizeof(bsp_settings));
                if(scenario>=10) {
                    settings.ordered_mode=2;
                    words[source_arena[Region::order].offset/4]=UINT32_MAX;
                    words[source_arena[Region::results].offset/4]=1;
                    words[source_arena[Region::results].offset/4+1]=scenario==11?1:0;
                }
            }
            if(chained) {
                words[0]=0; // Only the clip dispatch may produce the polygon.
                settings.fractional=1;
                auto* point_data=reinterpret_cast<float*>(words+6656/4);
                for(unsigned i=0;i<4;++i) {
                    const float x=scenario==2 && points[i][0]==4?-4.F:float(points[i][0]);
                    point_data[i*8]=x-16;point_data[i*8+1]=float(points[i][1])-16;
                    point_data[i*8+2]=256;
                    point_data[i*8+4]=x;point_data[i*8+5]=float(points[i][1]);
                    words[6912/4+i*4]=i;
                }
                words[7168/4+1]=4;words[7424/4]=1;
                const std::array<float,4> projection{16,16,256,1};
                std::memcpy(words+7680/4,projection.data(),sizeof(projection));
                const std::array<uint32_t,8> clip_settings{1,4,4,1,32,32,1,0};
                std::memcpy(words+8192/4,clip_settings.data(),sizeof(clip_settings));
                if(scenario>=3) {
                    std::memset(point_data,0,128); // projection shader owns this output
                    starfox::assets::Shape source_shape;
                    for(const auto& point:points) source_shape.vertices.push_back({int(point[0])-16,int(point[1])-16,0});
                    starfox::render::RenderPose pose;
                    pose.continuous_geometry=true;pose.use_rotation_matrix=true;
                    pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
                    pose.z=256;pose.vanish_x=pose.vanish_y=16;
                    if(scenario==4) {
                        source_shape.header.shift=3;source_shape.word_coordinates.assign(4,true);pose.scale=2;
                    }
                    if(scenario==5) {
                        source_shape.header.shift=1;source_shape.word_coordinates={true,true,false,false};
                        for(unsigned i=2;i<4;++i) {source_shape.vertices[i].x/=2;source_shape.vertices[i].y/=2;}
                    }
                    const auto packed=starfox::render::pack_projection(source_shape,pose,{});
                    require(packed.continuous && packed.continuous_vertices.size()==4,"Invalid packed projection fixture");
                    require(packed.continuous_vertices.size()*sizeof(packed.continuous_vertices[0])==64,"Projection vertex ABI changed");
                    require(sizeof(packed.continuous_poses)==320,"Projection pose ABI changed");
                    std::memcpy(words+8448/4,packed.continuous_vertices.data(),64);
                    std::memcpy(words+8704/4,packed.continuous_poses.data(),320);
                    words[9984/4]=4;words[9984/4+1]=4;
                    if(scenario>=6) {
                        words[7424/4]=0;
                        words[10240/4+1]=scenario==7?2:1;words[10240/4+2]=scenario==7?1:2;
                        words[10496/4]=1;words[10496/4+1]=4;
                    }
                }
            }
            const auto required_sizes=source_span_buffer_sizes(settings);
            require(required_sizes.has_value(),"Invalid source span fixture layout");
            for(unsigned i=0;i<7;++i) require((*required_sizes)[i]<=sizes[i],"Span fixture binding too short");
            std::memcpy(words+6400/4,&settings,sizeof(settings));
            require(arena->upload(0,std::as_bytes(std::span(host_words))),arena->status().c_str());
            check(vkResetCommandPool(device,pool,0),"Reset span commands");
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            check(vkBeginCommandBuffer(copy,&begin),"Begin span commands");
            require(!spans.record(copy,sets,0),"Zero span dispatch accepted");
            require(!spans.record(copy,sets,65535U*32U+1U),"Oversized span dispatch accepted");
            for(unsigned missing=0;missing<3;++missing) {
                auto incomplete=sets;incomplete[missing]=VK_NULL_HANDLE;
                require(!spans.record(copy,incomplete,1),"Missing span descriptor accepted");
            }
            if(chained) {
                if(scenario>=3) {
                    require(project.record(copy,project_sets,4),"Record model projection dispatch");
                    VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
                }
                if(scenario>=6) {
                    require(visibility.record(copy,visibility_sets,1),"Record source visibility dispatch");
                    VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
                }
                if(scenario>=8 && scenario<10) {
                    require(bsp.record(copy,bsp_sets,1),"Record source BSP dispatch");
                    VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
                }
                require(clip.record(copy,clip_sets,1),"Record continuous clip dispatch");
                VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&dependency,0,nullptr,0,nullptr);
            }
            require(spans.record(copy,sets,1),"Record source span dispatch");
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT|VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
            check(vkEndCommandBuffer(copy),"End span commands");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
            check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit span dispatch");
            check(vkQueueWaitIdle(queue),"Wait span dispatch");
            require(arena->readback(0,std::as_writable_bytes(std::span(host_words))),arena->status().c_str());
            const auto* result=words+3072/4;
            bool correct=true;
            for(unsigned y=0;y<32;++y) {
                const auto* row=result+y*24;
                if(scenario==7 || scenario==9 || scenario==11) {
                    for(unsigned word=0;word<24;++word) correct&=row[word]==0;
                    if(repeated) correct&=words[6144/4+y]==0;
                    continue;
                }
                if(repeated) {
                    const bool covered=y==4 || y==6;
                    const auto mask=words[6144/4+y];
                    correct&=mask==(covered?(0x2000U-(1U<<expected_left)):0U);
                    if(covered) {
                        correct&=row[0]==expected_left && row[2]==13 && row[1]==y && row[3]==y+1;
                        correct&=row[23]==(mode==5?6U:4U) && row[9]==4 && row[10]==32;
                    } else for(unsigned word=0;word<24;++word) correct&=row[word]==0;
                    continue;
                }
                if(y<4 || y>=12) {for(unsigned word=0;word<24;++word) correct&=row[word]==0;continue;}
                if(mode==4) {
                    correct&=row[0]==(y==4?0U:expected_left) && row[2]==(y==4?0U:expected_left+1);
                    correct&=row[1]==y && row[3]==y+1 && row[23]==0;
                    continue;
                }
                correct&=row[0]==expected_left+(mode==1?1U:0U) && row[2]==(mode==1?12U:13U);
                correct&=row[1]==y && row[3]==y+1 && row[4]==7 && row[5]==7;
                correct&=row[23]==(mode==2 && y>4?1U:0U);
            }
            require(correct,("Native Vulkan span readback mismatch, scenario "+std::to_string(scenario)+" mode "+std::to_string(mode)).c_str());
        }
        std::cout<<"Native Vulkan projection/visibility/BSP/clip/span chain: twelve scenarios x six modes exact, including occurrence indexing/recovery\n";
        for(bool matrix:{true,false}) for(unsigned effect=0;effect<8;++effect) {
            starfox::assets::Shape shape;
            shape.vertices={{-12,-12,0},{-12,-4,0},{-4,-4,0},{-4,-12,0}};
            if(effect==7) shape.vertices={{-9,-9,-64},{-9,-3,-64},{-5,-5,64},{-5,-15,64}};
            shape.faces={{-1,0,{0,0,127},{0,1,2,3}}};
            starfox::render::RenderPose pose;pose.continuous_geometry=true;
            pose.use_rotation_matrix=matrix;pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
            pose.z=256;pose.vanish_x=pose.vanish_y=16;
            if(effect==1) pose.cel_mode=true;
            if(effect==2) pose.wireframe_mode=1;
            if(effect==3 || effect==5) pose.wobble_mode=1;
            if(effect==4) pose.wobble_mode=2;
            if(effect>=5) {pose.wave_mode=true;pose.wave_offset=-6;pose.animation_frame=3;}
            SourceSpanModel model;SourceSpanArenaLayout layout;std::string model_error;
            require(prepare_source_span_model(shape,pose,{},32,32,model,model_error),model_error.c_str());
            if(effect>=6) {
                // Exercise palette lookup from GPU-generated commands, not
                // prototype vertex colours. Distinct non-endpoint channels
                // also expose missing/double sRGB conversion.
                model.graphics_palette_flags=effect==7?3U:1U;
                model.graphics_palette[17]=0xff4080c0U;
                model.graphics_palette[231]=0xffc04080U;
                model.faces.materials[0].even=17;
                model.faces.materials[0].odd=231;
                model.faces.materials[0].dither=1;
            }
            auto model_owner=std::make_shared<VulkanSourceModel>();
            if(matrix && (effect==0 || effect==6 || effect==7)) {
                SourceModelPackets source;
                source.packets.emplace_back();source.handles.push_back(7);
                // Enable a green palette for the solid coverage control.
                auto input=model;
                if(effect==0) {
                    input.graphics_palette_flags=1;input.graphics_palette.fill(0xff00ff00U);
                    // Exercise resident projection/BSP/corner fetch and indexed
                    // fragment sampling together, not merely a fragment fixture.
                    input.graphics_unclipped=true;
                    input.faces.materials[0].textured=1;
                    input.faces.materials[0].u_mask=1;
                    input.faces.materials[0].v_mask=1;
                    input.faces.texels={1,2,3,0};
                    input.graphics_palette[2]=0xff00ffffU;
                    input.graphics_palette[3]=0xffffff00U;
                    // Source screen order is left-top, left-bottom,
                    // right-bottom, right-top. Interpolate both UV axes.
                    for(size_t corner=0;corner<4;++corner) {
                        input.faces.corners[corner][1]=corner>=2?2U:0U;
                        input.faces.corners[corner][2]=(corner==0 || corner==3)?0U:2U;
                    }
                }
                if(effect==6) {
                    auto mixed_shape=shape;
                    mixed_shape.vertices.push_back({-2,-12,0});mixed_shape.vertices.push_back({-2,-4,0});
                    mixed_shape.faces.push_back({-1,0,{0,0,127},{4,5}});
                    mixed_shape.vertices.push_back({4,-8,0});
                    mixed_shape.faces.push_back({-1,1,{0,0,127},{6}});
                    mixed_shape.faces.back().sprite=true;
                    mixed_shape.vertices.insert(mixed_shape.vertices.end(),{{8,-12,0},{8,-10,0},{12,-10,0},{12,-12,0}});
                    mixed_shape.faces.push_back({-1,1,{0,0,127},{7,8,9,10}});
                    auto overlay=mixed_shape.faces.front();overlay.colour_id=1;
                    mixed_shape.faces.push_back(overlay);
                    mixed_shape.colour_words.resize(2);mixed_shape.colour_words[1]=0x4000;
                    starfox::assets::TextureImage mixed_texture;
                    mixed_texture.descriptor=0x4000;mixed_texture.u_mask=1;mixed_texture.v_mask=0;
                    mixed_texture.texels={1,1};mixed_shape.textures={mixed_texture};
                    SourceSpanModel mixed_model;
                    require(prepare_source_span_model(mixed_shape,pose,{},32,32,mixed_model,model_error),model_error.c_str());
                    mixed_model.graphics_palette=input.graphics_palette;mixed_model.graphics_palette_flags=input.graphics_palette_flags;
                    mixed_model.graphics_palette[0]=0xffff0000U;
                    mixed_model.faces.materials[0]=input.faces.materials[0];
                    mixed_model.faces.materials[1].even=mixed_model.faces.materials[1].odd=0;
                    mixed_model.faces.materials[1].dither=0;
                    mixed_model.graphics_palette[1]=0xffff00ffU;
                    input=std::move(mixed_model);
                }
                source.compute_models.push_back({7,0,256,std::move(input)});
                if(effect==0) {
                    // Empty packet, ordinary nearer strip, then an equal-depth
                    // blue compute model. Green must win equal depth even when
                    // the request vector is deliberately in reverse order.
                    source.packets.resize(4);source.handles={7,8,9,10};
                    auto duplicate=source.compute_models.front();duplicate.key=10;duplicate.packet_index=3;
                    duplicate.model.graphics_palette.fill(0xffff0000U);
                    source.compute_models.insert(source.compute_models.begin(),std::move(duplicate));
                    for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                        constexpr float xy[4][2]{{-11,12},{-10,12},{-10,4},{-11,4}};
                        SceneVertex vertex{};vertex.position[0]=xy[corner][0]/256;
                        vertex.position[1]=xy[corner][1]/256;vertex.position[2]=-.5F;
                        vertex.color[0]=vertex.color[3]=1;
                        source.packets[2].geometry.vertices.push_back(vertex);
                    }
                    for(auto& packet:source.packets) packet.model[12]=1.F/256;
                    // A resident line outside the textured quad verifies that
                    // line templates fetch both GPU endpoints and do not emit
                    // an accidental triangle from the same ordered slot.
                    auto line_shape=shape;
                    line_shape.vertices={{-2,-12,0},{-2,-4,0}};
                    line_shape.faces={{-1,0,{0,0,127},{0,1}}};
                    SourceSpanModel line_model;
                    require(prepare_source_span_model(line_shape,pose,{},32,32,line_model,model_error,true),model_error.c_str());
                    line_model.graphics_palette_flags=1;line_model.graphics_palette.fill(0xffff0000U);
                    source.packets.emplace_back();source.packets.back().model[12]=1.F/256;
                    source.handles.push_back(11);
                    source.compute_models.push_back({11,4,256,std::move(line_model)});
                    auto sprite_shape=shape;sprite_shape.vertices={{4,-8,0}};
                    sprite_shape.faces={{-1,0,{0,0,127},{0}}};sprite_shape.faces[0].sprite=true;
                    sprite_shape.colour_words={0x4000};
                    starfox::assets::TextureImage sprite_texture;
                    sprite_texture.descriptor=0x4000;sprite_texture.u_mask=1;sprite_texture.v_mask=0;
                    sprite_texture.texels={1,1};sprite_shape.textures={sprite_texture};
                    SourceSpanModel sprite_model;
                    require(prepare_source_span_model(sprite_shape,pose,{},32,32,sprite_model,model_error,true),model_error.c_str());
                    sprite_model.graphics_palette_flags=1;sprite_model.graphics_palette.fill(0xffff00ffU);
                    source.packets.emplace_back();source.packets.back().model[12]=1.F/256;
                    source.handles.push_back(12);
                    source.compute_models.push_back({12,5,256,std::move(sprite_model)});
                }
                combined_source_inputs[effect==0?0:effect==6?1:2]=std::move(source);
            }
            auto& gpu_model=*model_owner;
            if(effect==0 && matrix) {
                // Exercise occurrence-indexed resident cover drawing with
                // the same expected pixels as the authored-face reference.
                model.spans.ordered_mode=2;model.graphics_palette_flags|=4U;
            }
            require(!gpu_model.record(VK_NULL_HANDLE),"Uninitialized source model recorded");
            require(gpu_model.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,model,source_pipelines),gpu_model.status().c_str());
            std::array<VkDescriptorBufferInfo,3> ray_inputs{};
            require(gpu_model.ray_source_ranges(ray_inputs),"Resident ray inputs unavailable");
            require(ray_inputs[0].buffer==gpu_model.buffer()
                && ray_inputs[0].offset==gpu_model.layout()[SourceSpanRegion::points].offset
                && ray_inputs[1].offset==gpu_model.layout()[SourceSpanRegion::residuals].offset
                && ray_inputs[2].offset==gpu_model.layout()[SourceSpanRegion::corners].offset,
                "Ray inputs did not borrow the resident model arena");
            require(gpu_model.update(model) && gpu_model.compute_needed(),"Initial compute unexpectedly reusable");
            const auto initial_upload_bytes=gpu_model.updated_input_bytes();
            check(vkResetCommandPool(device,pool,0),"Reset model source commands");
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            check(vkBeginCommandBuffer(copy,&begin),"Begin model source commands");
            require(gpu_model.record(copy),"Record uploaded model chain");
            require(gpu_model.compute_needed(),"Recording incorrectly acknowledged GPU completion");
            check(vkEndCommandBuffer(copy),"End model source commands");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
            check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit uploaded model");
            check(vkQueueWaitIdle(queue),"Wait uploaded model");
            gpu_model.compute_completed();
            check(vkResetCommandPool(device,pool,0),"Reset resident ray commands");
            require(check_model_ray_expansion(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                copy,queue,gpu_model,model),"Resident model ray expansion mismatch");
            require(!gpu_model.compute_needed(),"Completed compute output not reusable");
            require(gpu_model.update(model) && !gpu_model.compute_needed(),"Unchanged inputs invalidated completed compute");
            std::array<uint32_t,2> result{};
            require(gpu_model.readback(Region::results,std::as_writable_bytes(std::span(result))),gpu_model.status().c_str());
            require(result[0]==1 && result[1]==0,"Uploaded model BSP failed");
            std::vector<uint32_t> rows(32*24);
            require(gpu_model.readback(Region::commands,std::as_writable_bytes(std::span(rows))),gpu_model.status().c_str());
            unsigned emitted=0;
            for(unsigned y=0;y<32;++y) {
                const auto* row=rows.data()+y*24;
                if(int32_t(row[2])<=int32_t(row[0])) continue;
                ++emitted;require(row[1]==y && row[3]==y+1,"Uploaded model row indexing failed");
                if(effect==0) require(y>=4 && y<12 && row[0]==4 && row[2]==13,"Uploaded model solid coverage mismatch");
            }
            require(emitted>0,"Uploaded model produced no coverage");
            if(effect>=6) {
                const auto previous_buffer=gpu_model.buffer();
                require(gpu_model.update(model),gpu_model.status().c_str());
                require(gpu_model.update(model),gpu_model.status().c_str());
                require(gpu_model.updated_input_bytes()==0,"Identical source model uploaded unchanged inputs");
                std::array<uint32_t,256> palette{};
                for(uint32_t i=0;i<256;++i) palette[i]=0xff000000U|i|(255U-i)<<8;
                require(gpu_model.update_palette(palette),gpu_model.status().c_str());
                require(!gpu_model.compute_needed(),"Palette-only update invalidated reusable geometry");
                std::array<uint32_t,256> observed{};
                require(gpu_model.readback(Region::palette,std::as_writable_bytes(std::span(observed))),gpu_model.status().c_str());
                require(observed==palette && gpu_model.buffer()==previous_buffer,"Palette-only update changed allocation or lost colours");
                std::vector<uint32_t> retained_rows(rows.size());
                require(gpu_model.readback(Region::commands,std::as_writable_bytes(std::span(retained_rows))),gpu_model.status().c_str());
                require(retained_rows==rows,"Palette-only update modified generated geometry spans");
                require(gpu_model.update(model),gpu_model.status().c_str());
                require(gpu_model.readback(Region::palette,std::as_writable_bytes(std::span(observed))),gpu_model.status().c_str());
                require(observed==model.graphics_palette,"Delta input cache failed to restore directly changed palette");
            }
            if(effect==0) require(emitted==8,"Uploaded model solid row count mismatch");
            if(effect==0 && matrix) {
                // Keep the generated model's actual arena/header alive for
                // both-eye graphics. No command readback is re-uploaded.
                produced_span_storage=gpu_model.buffer();
                solid_cover_model=model_owner;
                produced_span_bytes=gpu_model.layout().bytes;
                produced_span_lookup=static_cast<uint32_t>(gpu_model.layout()[Region::graphics_lookup].offset/4);
                cleanup.actions.push_back([model_owner]{model_owner->close();});
            }
            if(effect==0 && !matrix) {
                const auto retained_buffer=gpu_model.buffer();
                pose.x=4;
                SourceSpanModel moved;
                require(prepare_source_span_model(shape,pose,{},32,32,moved,model_error),model_error.c_str());
                require(gpu_model.update(moved),gpu_model.status().c_str());
                const auto full_input_bytes=initial_upload_bytes;
                require(gpu_model.compute_needed(),"Pose change failed to invalidate completed compute");
                require(full_input_bytes>0,"Initial input snapshot uploaded no bytes");
                require(gpu_model.update(moved),gpu_model.status().c_str());
                require(gpu_model.updated_input_bytes()==0,"Repeated moved model rewrote static inputs");
                auto shifted=moved;shifted.poses[0]=model.poses[0];
                require(gpu_model.update(shifted),gpu_model.status().c_str());
                require(gpu_model.updated_input_bytes()>0 && gpu_model.updated_input_bytes()<full_input_bytes,
                    "Pose-only update failed to reduce uploaded input bytes");
                std::cout<<"Source input upload bytes: full="<<full_input_bytes<<" repeated=0 pose="
                    <<gpu_model.updated_input_bytes()<<'\n';
                require(gpu_model.update(moved),gpu_model.status().c_str());
                std::vector<uint32_t> before_recompute(rows.size());
                require(gpu_model.readback(Region::commands,std::as_writable_bytes(std::span(before_recompute))),gpu_model.status().c_str());
                require(before_recompute==rows,"Input-only update overwrote GPU span output");
                auto invalid=moved;invalid.spans.polygon_count+=1;
                require(!gpu_model.update(invalid),"Incompatible source model update accepted");
                require(gpu_model.buffer()==retained_buffer,"Source model update reallocated storage");
                check(vkResetCommandPool(device,pool,0),"Reset updated model commands");
                check(vkBeginCommandBuffer(copy,&begin),"Begin updated model commands");
                require(gpu_model.record(copy),"Record updated model");
                check(vkEndCommandBuffer(copy),"End updated model commands");
                check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit updated model");
                check(vkQueueWaitIdle(queue),"Wait updated model");
                require(gpu_model.readback(Region::commands,std::as_writable_bytes(std::span(rows))),gpu_model.status().c_str());
                unsigned moved_rows=0;
                for(unsigned y=0;y<32;++y) {
                    const auto* row=rows.data()+y*24;
                    if(int32_t(row[2])<=int32_t(row[0])) continue;
                    ++moved_rows;require(y>=4 && y<12 && row[0]==8 && row[2]==17,"Retained source model update coverage mismatch");
                }
                require(moved_rows==8,"Retained source model update lost rows");
            }
            if(effect==6 && matrix) {
                wave_cover_model=model_owner;
                wave_span_storage=gpu_model.buffer();wave_span_bytes=gpu_model.layout().bytes;
                wave_span_lookup=static_cast<uint32_t>(gpu_model.layout()[Region::graphics_lookup].offset/4);
                cleanup.actions.push_back([model_owner]{model_owner->close();});
            }
            if(effect==7 && matrix) {
                tilted_cover_model=model_owner;
                tilted_span_storage=gpu_model.buffer();tilted_span_bytes=gpu_model.layout().bytes;
                tilted_span_lookup=static_cast<uint32_t>(gpu_model.layout()[Region::graphics_lookup].offset/4);
                cleanup.actions.push_back([model_owner]{model_owner->close();});
            }
        }
        {
            // Exercise the real resident arena and descriptor bindings with
            // more than one occurrence: BSP must retain its full order output
            // even though mode 2's span consumer doesn't read that buffer.
            starfox::assets::Shape shape;
            shape.vertices={{-12,-12,0},{-12,-4,0},{-4,-4,0},{-4,-12,0},
                {2,-12,0},{2,-4,0},{10,-4,0},{10,-12,0}};
            shape.faces={{-1,0,{0,0,127},{0,1,2,3}},{-1,0,{0,0,127},{4,5,6,7}}};
            starfox::render::RenderPose pose;pose.continuous_geometry=true;
            pose.z=256;pose.vanish_x=pose.vanish_y=16;
            SourceSpanModel model;std::string error;
            require(prepare_source_span_model(shape,pose,{},32,32,model,error),error.c_str());
            model.spans.ordered_mode=2;model.graphics_palette_flags=5;
            model.graphics_palette[17]=0xff00ff00U;
            model.graphics_palette[231]=0xffff0000U;
            model.faces.materials[0].even=model.faces.materials[0].odd=17;
            model.faces.materials[1].even=model.faces.materials[1].odd=231;
            occurrence_cover_model=std::make_shared<VulkanSourceModel>();
            auto& resident=*occurrence_cover_model;
            cleanup.actions.push_back([owner=occurrence_cover_model]{owner->close();});
            require(resident.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,model,source_pipelines),resident.status().c_str());
            require(model.spans.count==2,"Two-face occurrence capacity changed");
            require(resident.layout()[Region::order].size>=8,"Resident BSP output was undersized");
            check(vkResetCommandPool(device,pool,0),"Reset occurrence commands");
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            check(vkBeginCommandBuffer(copy,&begin),"Begin occurrence commands");
            require(resident.record(copy),"Two-face occurrence recording failed");
            check(vkEndCommandBuffer(copy),"End occurrence commands");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
            check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit occurrence commands");
            check(vkQueueWaitIdle(queue),"Wait occurrence commands");
            std::array<uint32_t,2> order{},result{};
            require(resident.readback(Region::order,std::as_writable_bytes(std::span(order))),resident.status().c_str());
            require(resident.readback(Region::results,std::as_writable_bytes(std::span(result))),resident.status().c_str());
            require(result[0]==2 && result[1]==0 && order[0]!=order[1] && order[0]<2 && order[1]<2,
                "Resident BSP did not write both occurrences");
            std::vector<uint32_t> rows(2*32*24);
            require(resident.readback(Region::commands,std::as_writable_bytes(std::span(rows))),resident.status().c_str());
            for(unsigned slot=0;slot<2;++slot) {
                unsigned emitted=0;
                for(unsigned y=0;y<32;++y) {
                    const auto* row=rows.data()+(slot*32+y)*24;
                    if(int32_t(row[2])<=int32_t(row[0])) continue;
                    ++emitted;
                    require(row[4]==(slot?231U:17U) && row[5]==row[4],"Occurrence material crossed slots");
                    require(row[0]==(slot?18U:4U),"Occurrence geometry crossed slots");
                }
                require(emitted==8,"Occurrence lost span rows");
            }
            std::cout<<"Resident two-face occurrence arena: full BSP output and independent material spans passed\n";
            for(unsigned orientation:{0U,1U,2U}) {
                const bool matrix=orientation!=0;
                auto fragments=shape;
                fragments.faces[0].normal={32,64,96};fragments.faces[1].normal={-64,-32,-96};
                auto fragment_pose=pose;fragment_pose.use_rotation_matrix=matrix;
                fragment_pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
                if(orientation==2) fragment_pose.rotation_matrix={0,0,-32768,0,32767,0,32767,0,0};
                std::vector<std::array<float,8>> baseline;
                for(unsigned progress:{0U,1U,4U,31U}) {
                    fragment_pose.explosion_progress=uint16_t(progress);
                    SourceSpanModel fragment_model;
                    require(prepare_source_span_model(fragments,fragment_pose,{},32,32,fragment_model,error),error.c_str());
                    VulkanSourceModel fragment_gpu;
                    require(fragment_gpu.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                        fragment_model,source_pipelines),fragment_gpu.status().c_str());
                    check(vkResetCommandPool(device,pool,0),"Reset fragment position commands");
                    check(vkBeginCommandBuffer(copy,&begin),"Begin fragment position commands");
                    require(fragment_gpu.record(copy),"Record fragment positions");
                    check(vkEndCommandBuffer(copy),"End fragment position commands");
                    check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit fragment positions");
                    check(vkQueueWaitIdle(queue),"Wait fragment positions");
                    std::vector<std::array<float,8>> points(fragment_model.projection.continuous_vertices.size());
                    require(fragment_gpu.readback(Region::points,std::as_writable_bytes(std::span(points))),fragment_gpu.status().c_str());
                    if(!progress) {baseline=points;continue;}
                    require(points.size()==baseline.size(),"Fragment fixture changed corner count");
                    for(size_t vertex=0;vertex<points.size();++vertex) {
                        const auto n=fragments.faces[vertex/4].normal;
                        const std::array<int,3> direction{-int(n.x),int(n.y),-int(n.z)};
                        for(unsigned axis=0;axis<3;++axis) {
                            int rotated=direction[axis];
                            if(matrix) {
                                rotated=0;
                                for(unsigned component=0;component<3;++component)
                                    rotated+=starfox::simulation::arithmetic_shift_right(direction[component]*fragment_pose.rotation_matrix[component*3+axis],15);
                            }
                            if(axis==1) rotated=-std::abs(rotated);
                            const auto offset=starfox::simulation::arithmetic_shift_right(rotated*int(progress),2);
                            require(std::abs((points[vertex][axis]-baseline[vertex][axis])-float(offset))<.001F,
                                "GPU fragment displacement differs from source normal expansion");
                        }
                    }
                }
            }
            // The second occurrence switches to a texture without changing
            // the first solid occurrence's material or source geometry.
            model.faces.materials[1].textured=1;
            model.faces.polygons[1][3]|=1U;
            model.faces.materials[1].u_mask=model.faces.materials[1].v_mask=1;
            model.faces.materials[1].reserved0=1; // One texel of horizontal scroll.
            model.faces.texels={231,17,17,231};
            const std::array<std::array<uint32_t,2>,4> texture_uv{{{0,0},{0,2},{2,2},{2,0}}};
            for(unsigned corner=0;corner<4;++corner) {
                auto& packed=model.faces.corners[model.faces.polygons[1][0]+corner];
                packed[1]=texture_uv[corner][0];packed[2]=texture_uv[corner][1];
            }
            textured_occurrence_model=std::make_shared<VulkanSourceModel>();
            require(textured_occurrence_model->initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,model,source_pipelines),textured_occurrence_model->status().c_str());
            cleanup.actions.push_back([owner=textured_occurrence_model]{owner->close();});
            check(vkResetCommandPool(device,pool,0),"Reset textured occurrence commands");
            check(vkBeginCommandBuffer(copy,&begin),"Begin textured occurrence commands");
            require(textured_occurrence_model->record(copy),"Record textured occurrence");
            check(vkEndCommandBuffer(copy),"End textured occurrence commands");
            check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit textured occurrence");
            check(vkQueueWaitIdle(queue),"Wait textured occurrence");
            {
                auto warp_pose=pose;warp_pose.colour_warp=true;
                SourceWarpInputs inputs;
                require(prepare_source_warp_inputs(shape,warp_pose,{},model.projection,model.bsp,inputs,error),error.c_str());
                inputs.shading.settings.seed=1234;
                auto source=model;source.faces=inputs.templates;
                source.spans.ordered_mode=2;source.graphics_palette_flags=5;
                source.graphics_palette[15]=0xff00ff00U;
                source.graphics_palette[8]=source.graphics_palette[0]=0xffff0000U;
                source.warp_expanded=true;
                source.spans.polygon_count=source.spans.count;
                source.clip_settings[0]=source.spans.count;
                source.clip_settings[2]=source.spans.count*32U;
                DrawPacket warp_placeholder;
                warp_placeholder.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
                warp_scene_input.packets={warp_placeholder};warp_scene_input.handles={77};
                warp_scene_input.compute_models={{77,0,256,source,std::make_shared<SourceWarpInputs>(inputs)}};
                SourceSpanArenaLayout prefix;SourceWarpArenaLayout suffix;
                require(layout_source_span_model(source,properties.limits.minStorageBufferOffsetAlignment,
                    properties.limits.minUniformBufferOffsetAlignment,properties.limits.maxStorageBufferRange,
                    properties.limits.maxUniformBufferRange,prefix,error),error.c_str());
                require(layout_source_warp_inputs(inputs,prefix.bytes,properties.limits.minStorageBufferOffsetAlignment,
                    properties.limits.minUniformBufferOffsetAlignment,properties.limits.maxStorageBufferRange,
                    properties.limits.maxUniformBufferRange,suffix,error),error.c_str());
                std::vector<std::byte> image;
                require(source_span_upload_image(source,prefix,image,error),error.c_str());
                image.resize(suffix.bytes);
                std::vector<SourceSpanInputWrite> writes;
                require(source_warp_input_writes(inputs,suffix,writes,error),error.c_str());
                for(const auto& write:writes) std::memcpy(image.data()+write.offset,write.bytes.data(),write.bytes.size());
                SourceSpanInputWrite graphics_write;
                require(source_warp_graphics_write(source,prefix,inputs,suffix,graphics_write,error),error.c_str());
                const auto lookup_started=std::chrono::steady_clock::now();
                for(unsigned repeat=0;repeat<256;++repeat)
                    require(source_warp_graphics_write(source,prefix,inputs,suffix,graphics_write,error),error.c_str());
                std::cout<<"Warp graphics metadata preparation: "
                    <<std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-lookup_started).count()/256
                    <<" us/update (CPU preparation, not frame FPS)\n";
                std::array<uint32_t,20> warp_lookup{};
                std::memcpy(warp_lookup.data(),graphics_write.bytes.data(),sizeof(warp_lookup));
                require(warp_lookup[1]==suffix[SourceWarpRegion::result].offset/4
                    && warp_lookup[8]==suffix[SourceWarpRegion::polygons].offset/4
                    && warp_lookup[9]==suffix[SourceWarpRegion::corners].offset/4
                    && warp_lookup[10]==source.spans.count*32U
                    && warp_lookup[16]==suffix[SourceWarpRegion::materials].offset/4
                    && warp_lookup[0]==prefix[SourceSpanRegion::order].offset/4
                    && warp_lookup[11]==prefix[SourceSpanRegion::projection_parameters].offset/4,
                    "Warp graphics lookup mixed source and occurrence regions");
                const auto retained_graphics=graphics_write.bytes;
                auto bad_graphics_source=source;bad_graphics_source.warp_expanded=false;
                require(!source_warp_graphics_write(bad_graphics_source,prefix,inputs,suffix,graphics_write,error)
                    && graphics_write.bytes==retained_graphics,"Warp graphics rejection changed valid metadata");
                std::memcpy(image.data()+graphics_write.offset,graphics_write.bytes.data(),graphics_write.bytes.size());
                warp_draw_storage=std::make_shared<VulkanSourceStorage>();
                cleanup.actions.push_back([owner=warp_draw_storage]{owner->close();});
                auto& arena=*warp_draw_storage;
                warp_draw_lookup=uint32_t(prefix[SourceSpanRegion::graphics_lookup].offset/4);
                require(arena.initialize(device,vkGetDeviceProcAddr,memory_properties,suffix.bytes),arena.status().c_str());
                require(arena.upload(0,image),arena.status().c_str());
                VulkanSourceBindings ordinary;
                require(ordinary.initialize(device,vkGetDeviceProcAddr,arena.buffer(),arena.size(),prefix,source_pipelines,true),ordinary.status().c_str());
                VulkanSourceBindings consumers;
                require(consumers.initialize(device,vkGetDeviceProcAddr,arena.buffer(),arena.size(),prefix,source_pipelines,false,&suffix),consumers.status().c_str());
                std::array<VulkanSpanPipeline,3> stages;
                std::array<const VulkanSpanPipeline*,3> borrowed{};
                for(unsigned stage=0;stage<3;++stage) {
                    require(stages[stage].initialize(device,vkGetDeviceProcAddr,static_cast<SourceComputeStage>(5+stage)),stages[stage].status().c_str());
                    borrowed[stage]=&stages[stage];
                }
                VulkanWarpBindings bindings;
                require(!bindings.record(VK_NULL_HANDLE),"Uninitialized warp bindings recorded");
                for(const auto region:{SourceSpanRegion::order,SourceSpanRegion::results,
                    SourceSpanRegion::polygons,SourceSpanRegion::visibility,
                    SourceSpanRegion::corners,SourceSpanRegion::materials}) {
                    auto truncated=prefix;
                    truncated.regions[static_cast<size_t>(region)].size-=4;
                    require(!bindings.initialize(device,vkGetDeviceProcAddr,arena.buffer(),arena.size(),truncated,suffix,inputs,borrowed),
                        "Warp bindings accepted truncated producer input");
                    require(!bindings.record(copy),"Rejected warp bindings remained recordable");
                }
                require(bindings.initialize(device,vkGetDeviceProcAddr,arena.buffer(),arena.size(),prefix,suffix,inputs,borrowed),bindings.status().c_str());
                warp_draw_model=std::make_shared<VulkanSourceModel>();
                cleanup.actions.push_back([owner=warp_draw_model]{owner->close();});
                require(warp_draw_model->initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                    source,source_pipelines,&inputs,&borrowed),warp_draw_model->status().c_str());
                require(warp_draw_model->update(source,&inputs),warp_draw_model->status().c_str());
                require(warp_draw_model->update(source,&inputs) && warp_draw_model->updated_input_bytes()==0,
                    "Unchanged warp model reuploaded inputs");
                check(vkResetCommandPool(device,pool,0),"Reset bound warp commands");
                check(vkBeginCommandBuffer(copy,&begin),"Begin bound warp commands");
                for(unsigned stage=2;stage<5;++stage) {
                    const uint32_t count=stage==2?source.projection_settings[0]:stage==3?source.visibility_settings[0]:1U;
                    require(source_pipelines[stage]->record(copy,ordinary.sets(static_cast<SourceComputeStage>(stage)),count),"Record warp producer");
                    VkMemoryBarrier dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0,1,&dependency,0,nullptr,0,nullptr);
                }
                require(bindings.record(copy),"Record bound warp chain");
                require(source_pipelines[1]->record(copy,consumers.sets(SourceComputeStage::continuous_clip),source.spans.polygon_count),"Clip bound warp output");
                VkMemoryBarrier warp_dependency{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                warp_dependency.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
                warp_dependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,1,&warp_dependency,0,nullptr,0,nullptr);
                require(source_pipelines[0]->record(copy,consumers.sets(SourceComputeStage::spans),source.spans.count),"Rasterize bound warp output");
                require(warp_draw_model->record(copy),"Record owned warp model chain");
                check(vkEndCommandBuffer(copy),"End bound warp commands");
                check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit bound warp chain");
                check(vkQueueWaitIdle(queue),"Wait bound warp chain");
                std::array<uint32_t,2> sequence{};
                require(arena.readback(suffix[SourceWarpRegion::descriptors].offset,std::as_writable_bytes(std::span(sequence))),arena.status().c_str());
                require(sequence==std::array<uint32_t,2>{29351,14600},"Bound warp PRNG sequence mismatch");
                std::array<starfox::render::RasterCommand,2> materials;
                require(arena.readback(suffix[SourceWarpRegion::materials].offset,std::as_writable_bytes(std::span(materials))),arena.status().c_str());
                require(materials[0].even==15 && materials[0].odd==15 && materials[1].even==8 && materials[1].odd==0,
                    "Bound warp expanded materials mismatch");
                std::vector<uint32_t> warp_rows(2*32*24);
                require(arena.readback(prefix[SourceSpanRegion::commands].offset,std::as_writable_bytes(std::span(warp_rows))),arena.status().c_str());
                std::vector<uint32_t> owned_rows(warp_rows.size());
                require(warp_draw_model->readback(SourceSpanRegion::commands,std::as_writable_bytes(std::span(owned_rows))),warp_draw_model->status().c_str());
                require(owned_rows==warp_rows,"Owned warp model differs from explicit pipeline chain");
                warp_draw_model->compute_completed();
                std::array<VkDescriptorBufferInfo,3> rejected_warp_ranges{};
                require(!warp_draw_model->ray_source_ranges(rejected_warp_ranges),"Warp candidates require explicit opt-in");
                check(vkResetCommandPool(device,pool,0),"Reset warp ray expansion commands");
                require(check_model_ray_expansion(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                    copy,queue,*warp_draw_model,source,true),"Warp source candidate ray expansion mismatch");
                require(warp_draw_model->update(source,&inputs) && !warp_draw_model->compute_needed(),
                    "Unchanged warp model invalidated completed output");
                const auto warp_buffer=warp_draw_model->buffer();
                auto changed_warp=inputs;changed_warp.shading.settings.seed++;
                require(warp_draw_model->update(source,&changed_warp)
                    && warp_draw_model->updated_input_bytes()==16 && warp_draw_model->compute_needed()
                    && warp_draw_model->buffer()==warp_buffer,"Warp seed update did not reuse arena with one uniform write");
                auto invalid_warp=changed_warp;invalid_warp.shading.settings.capacity++;
                require(!warp_draw_model->update(source,&invalid_warp),"Warp update accepted incompatible capacity");
                require(warp_draw_model->update(source,&inputs),warp_draw_model->status().c_str());
                check(vkResetCommandPool(device,pool,0),"Reset updated warp commands");
                check(vkBeginCommandBuffer(copy,&begin),"Begin updated warp commands");
                require(warp_draw_model->record(copy),"Record updated warp model");
                check(vkEndCommandBuffer(copy),"End updated warp commands");
                check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit updated warp model");
                check(vkQueueWaitIdle(queue),"Wait updated warp model");
                require(warp_draw_model->readback(SourceSpanRegion::commands,std::as_writable_bytes(std::span(owned_rows)))
                    && owned_rows==warp_rows,"Restored warp inputs changed generated output");
                for(unsigned slot=0;slot<2;++slot) {
                    unsigned emitted=0;
                    for(unsigned y=0;y<32;++y) {
                        const auto* row=warp_rows.data()+(slot*32+y)*24;
                        if(int32_t(row[2])<=int32_t(row[0])) continue;
                        ++emitted;
                        require(row[4]==materials[slot].even && row[5]==materials[slot].odd,
                            "Bound warp consumer used authored material instead of occurrence material");
                    }
                    require(emitted==8,"Bound warp consumer lost occurrence geometry");
                }
                std::cout<<"Resident warp bindings: source producers through PRNG/material/expansion/clip/spans passed\n";
            }
        }
        std::cout<<"Generated model uploads: Q15/Euler x eight cases dispatched with device-aligned descriptors\n";
        if(argc==4) {
            const auto rom=starfox::assets::RomImage::load(argv[2]);
            const auto symbols=starfox::assets::SymbolMap::load(argv[3]);
            const starfox::assets::ShapeDecoder decoder(rom,symbols);
            SourceModels assembled_models(rom,symbols,true,true);
            unsigned assembled_compute_count=0,assembled_warp_count=0;
            for(const char* name:{"ROBOT_0","BOSS_H_2","MY_DEMO"}) {
                GameSceneSnapshot snapshot;snapshot.flow=starfox::simulation::GameFlowState::gameplay;
                for(unsigned i=0;i<256;++i) snapshot.cgram[i]=uint16_t(i);
                snapshot.model_palette.fill(0x03e0);
                GameSceneObject object;object.handle=1;
                object.object.shape=uint16_t(symbols.find(name).front());
                object.source_pose.z=512;object.source_pose.source_depth=512;
                snapshot.objects.push_back(object);
                const auto assembled=assembled_models.assemble(snapshot,true);
                require(assembled.pending.empty(),"Source assembly reported an unsupported fixture");
                for(const auto& compute:assembled.compute_models) {
                    ++assembled_compute_count;
                    require(compute.packet_index<assembled.packets.size() && assembled.handles[compute.packet_index]==compute.key,
                        "Compute model lost ordered placeholder");
                    require(compute.model.graphics_palette_flags==3 && compute.model.graphics_palette[112]==0xff00ff00U,
                        "Compute model lost live palette/sRGB settings");
                    require(assembled.packets[compute.packet_index].geometry.vertex_view().empty(),"Compute model also emitted CPU geometry");
                }
                snapshot.objects[0].source_pose.explosion_progress=1;
                const auto destroyed=assembled_models.assemble(snapshot,true);
                require(destroyed.pending.empty() && !destroyed.compute_models.empty(),"Cartridge destruction did not reach GPU assembly");
                for(const auto& compute:destroyed.compute_models) {
                    require(compute.model.fragmented && !compute.warp,"Cartridge destruction lost per-face GPU transforms");
                    require(destroyed.packets[compute.packet_index].geometry.vertex_view().empty(),"Destruction duplicated CPU geometry");
                }
                snapshot.objects[0].source_pose.explosion_progress=0;
                snapshot.objects[0].source_pose.colour_warp=true;
                const auto warped=assembled_models.assemble(snapshot,true);
                snapshot.objects[0].source_pose.explosion_progress=1;
                const auto warp_destroyed=assembled_models.assemble(snapshot,true);
                require(warp_destroyed.pending.empty() && !warp_destroyed.compute_models.empty(),"Combined warp destruction did not reach GPU assembly");
                for(const auto& compute:warp_destroyed.compute_models)
                    require(compute.model.fragmented && compute.warp
                        && (compute.warp->shading.settings.seed&0x80000000U),"Combined warp destruction lost source sequence mode");
                snapshot.objects[0].source_pose.explosion_progress=0;
                if(!warped.pending.empty()) std::cout<<"Warp assembly "<<name<<": "<<warped.pending.front().reason<<"\n";
                require(warped.pending.empty(),
                    "Unexpected cartridge warp assembly failure");
                for(const auto* variant:{&warped,&warp_destroyed}) for(const auto& compute:variant->compute_models) if(compute.warp) {
                    ++assembled_warp_count;
                    require(compute.model.warp_expanded && (compute.model.graphics_palette_flags&4U)
                        && compute.model.spans.polygon_count==compute.model.spans.count,
                        "Cartridge warp lost occurrence consumer mode");
                    SourceSpanArenaLayout checked;
                    std::string error;
                    require(layout_source_span_model(compute.model,256,256,128*1024*1024,65536,checked,error),error.c_str());
                    require(warped.packets[compute.packet_index].geometry.vertex_view().empty(),"Warp duplicated CPU geometry");
                    std::array<VulkanSpanPipeline,3> warp_stages;
                    std::array<const VulkanSpanPipeline*,3> warp_pipelines{};
                    for(unsigned stage=0;stage<3;++stage) {
                        require(warp_stages[stage].initialize(device,vkGetDeviceProcAddr,static_cast<SourceComputeStage>(5+stage)),warp_stages[stage].status().c_str());
                        warp_pipelines[stage]=&warp_stages[stage];
                    }
                    VulkanSourceModel resident_warp;
                    require(resident_warp.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                        compute.model,source_pipelines,compute.warp.get(),&warp_pipelines),resident_warp.status().c_str());
                    check(vkResetCommandPool(device,pool,0),"Reset cartridge warp commands");
                    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                    check(vkBeginCommandBuffer(copy,&begin),"Begin cartridge warp commands");
                    require(resident_warp.record(copy),"Record cartridge warp");
                    check(vkEndCommandBuffer(copy),"End cartridge warp commands");
                    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
                    check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit cartridge warp");
                    check(vkQueueWaitIdle(queue),"Wait cartridge warp");
                    std::array<uint32_t,2> traversal{};
                    require(resident_warp.readback(SourceSpanRegion::results,std::as_writable_bytes(std::span(traversal)))
                        && traversal[0]>0 && traversal[1]==0,"Cartridge warp traversal failed");
                }
            }
            require(assembled_compute_count>0,"No cartridge models reached compute assembly");
            require(assembled_warp_count>0,"No cartridge warp models reached compute assembly");
            std::cout<<"Cartridge warp assembly: "<<assembled_warp_count<<" models routed to resident GPU requests\n";
            std::cout<<"Ordered source assembly: "<<assembled_compute_count<<" compute models with live palette\n";
            for(const char* name:{"ROBOT_0","BOSS_H_2","MY_DEMO"}) for(unsigned effect=0;effect<8;++effect) {
                const auto shape=decoder.decode_by_name(symbols,name);
                starfox::render::RenderPose pose;pose.continuous_geometry=true;
                pose.z=512;pose.vanish_x=128;pose.vanish_y=96;pose.yaw=16;
                if(effect==1) pose.cel_mode=true;
                if(effect==2) pose.wireframe_mode=1;
                if(effect==3 || effect==5) pose.wobble_mode=1;
                if(effect==4) pose.wobble_mode=2;
                if(effect==5) {pose.wave_mode=true;pose.wave_offset=-6;pose.animation_frame=3;}
                if(effect>=6) pose.explosion_progress=1;
                if(effect==7) {pose.use_rotation_matrix=true;pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};}
                SourceSpanModel model;std::string error;
                require(prepare_source_span_model(shape,pose,{},256,192,model,error),error.c_str());
                VulkanSourceModel gpu_model;
                require(gpu_model.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,model,source_pipelines),gpu_model.status().c_str());
                check(vkResetCommandPool(device,pool,0),"Reset cartridge source commands");
                VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                check(vkBeginCommandBuffer(copy,&begin),"Begin cartridge source commands");
                require(gpu_model.record(copy),"Record cartridge source model");
                check(vkEndCommandBuffer(copy),"End cartridge source commands");
                VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
                check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit cartridge source model");
                check(vkQueueWaitIdle(queue),"Wait cartridge source model");
                std::array<uint32_t,2> result{};
                require(gpu_model.readback(Region::results,std::as_writable_bytes(std::span(result))),gpu_model.status().c_str());
                require(result[0]>0 && result[0]<=model.spans.count && result[1]==0,"Cartridge source BSP failure");
                std::vector<uint32_t> clipped(model.spans.polygon_count*129*4);
                require(gpu_model.readback(Region::clipped,std::as_writable_bytes(std::span(clipped))),gpu_model.status().c_str());
                for(unsigned face=0;face<model.spans.polygon_count;++face)
                    require(clipped[face*129*4+1]==0,"Cartridge source clipping status failure");
                std::vector<uint32_t> rows(model.spans.count*192*24);
                require(gpu_model.readback(Region::commands,std::as_writable_bytes(std::span(rows))),gpu_model.status().c_str());
                size_t emitted=0;
                for(size_t row=0;row<rows.size()/24;++row) {
                    const auto* command=rows.data()+row*24;
                    if(int32_t(command[2])<=int32_t(command[0])) continue;
                    ++emitted;require(command[1]==row%192 && command[3]==row%192+1,"Cartridge span row mismatch");
                }
                require(emitted>0,(std::string("Empty cartridge span model: ")+name+" effect "+std::to_string(effect)).c_str());
                std::cout<<"Cartridge GPU spans "<<name<<" effect "<<effect<<": "<<result[0]<<" ordered faces, "<<emitted<<" nonempty rows\n";
            }
        }
    }
    const std::array<VkImage,1> image_list{image};
    const std::array<std::span<const VkImage>,2> eyes{image_list,image_list};
    const std::array<VkExtent2D,2> extents{{{width,height},{width,height}}};
    VulkanDepthTargets depth;
    require(depth.initialize(instance,physical,device,get,extents),depth.status().c_str());
    require(depth.views()[0]!=depth.views()[1],"Eyes unexpectedly share a depth allocation");
    VulkanEyeTargets targets;require(targets.initialize(device,vkGetDeviceProcAddr,image_info.format,eyes,extents,
        depth.format(),depth.views()),targets.status().c_str());
    VulkanSceneTextures texture_fixture,cartridge_textures;
    warp_scene=std::make_shared<VulkanSourceScene>();
    cleanup.actions.push_back([owner=warp_scene]{owner->close();});
    for(unsigned iteration=0;iteration<4;++iteration) {
        auto frame=warp_scene_input;
        if(iteration==1) {
            auto resized=std::make_shared<SourceWarpInputs>(*frame.compute_models[0].warp);
            resized->textures.coordinates.push_back({0,0});
            resized->shading.settings.coordinate_count=uint32_t(resized->textures.coordinates.size());
            frame.compute_models[0].warp=std::move(resized);
        }
        require(warp_scene->initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
            targets.render_pass(),frame),warp_scene->status().c_str());
        if(iteration==3) require(warp_scene->reused_resources(),"Warp scene failed resource reuse");
        else if(iteration) require(!warp_scene->reused_resources(),"Warp scene reused incompatible coordinate storage");
        check(vkResetCommandPool(device,pool,0),"Reset warp scene commands");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(copy,&begin),"Begin warp scene commands");
        require(warp_scene->record_compute(copy),"Record scene-owned warp compute");
        check(vkEndCommandBuffer(copy),"End warp scene commands");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
        check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit warp scene commands");
        check(vkQueueWaitIdle(queue),"Wait warp scene commands");
    }
    {
        VulkanSourceScene transition;
        for(unsigned index:{0U,1U,0U,2U,0U}) {
            const auto& source=combined_source_inputs[index];
            require(transition.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                targets.render_pass(),source),transition.status().c_str());
            check(vkResetCommandPool(device,pool,0),"Reset lazy compute transition commands");
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            check(vkBeginCommandBuffer(copy,&begin),"Begin lazy compute transition");
            require(transition.record_compute(copy),"Record lazy compute transition");
            check(vkEndCommandBuffer(copy),"End lazy compute transition");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
            check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit lazy compute transition");
            check(vkQueueWaitIdle(queue),"Wait lazy compute transition");
        }
        std::cout<<"Lazy compute pipelines: ordinary/effect/ordinary transitions dispatched successfully\n";
    }
    for(size_t index=0;index<combined_source_inputs.size();++index) {
        const auto& source=combined_source_inputs[index];
        auto combined=std::make_shared<VulkanSourceScene>();
        require(combined->initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,targets.render_pass(),source),combined->status().c_str());
        require(!combined->reused_resources(),"Initial source upload reported reuse");
        VulkanSourceScene::RaySource first_ray;
        const auto ray_key=source.compute_models[0].key;
        require(combined->ray_source(ray_key,first_ray),"Resident scene ray source missing");
        require(first_ray.units==source.compute_models[0].units && first_ray.ranges[0].buffer,
            "Resident scene ray metadata mismatch");
        require(combined->initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,targets.render_pass(),source),combined->status().c_str());
        require(combined->reused_resources(),"Compatible source scene recreated GPU resources");
        VulkanSourceScene::RaySource updated_ray;
        require(combined->ray_source(ray_key,updated_ray) && updated_ray.generation!=first_ray.generation
            && updated_ray.ranges[0].buffer==first_ray.ranges[0].buffer,"Ray source generation/reuse mismatch");
        require(!combined->ray_source(UINT32_MAX,updated_ray),"Missing ray model accepted");
        if(index==0) {
            const auto retained_start=std::chrono::steady_clock::now();
            for(unsigned iteration=0;iteration<10;++iteration)
                require(combined->initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,targets.render_pass(),source),combined->status().c_str());
            const double retained_us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-retained_start).count()/10;
            const auto rebuild_start=std::chrono::steady_clock::now();
            for(unsigned iteration=0;iteration<3;++iteration) {
                VulkanSourceScene rebuilt;
                require(rebuilt.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,targets.render_pass(),source),rebuilt.status().c_str());
            }
            const double rebuild_us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-rebuild_start).count()/3;
            std::cout<<"Combined source scene setup: retained "<<retained_us<<" us, rebuild "<<rebuild_us<<" us (CPU setup, not frame FPS)\n";
        }
        auto invalid=source;invalid.compute_models[0].packet_index=SIZE_MAX;
        require(!combined->initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,targets.render_pass(),invalid),"Invalid compute placeholder accepted");
        combined_source_scenes[index]=combined;
        if(index==0) {
            auto shadow_source=source;
            for(auto& key:shadow_source.handles) if(key==9 || key==11) key|=source_shadow_pass;
            for(auto& request:shadow_source.compute_models) if(request.key==11) request.key|=source_shadow_pass;
            shadow_filtered_scene=std::make_shared<VulkanSourceScene>();
            require(shadow_filtered_scene->initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                targets.render_pass(),shadow_source),shadow_filtered_scene->status().c_str());
            cleanup.actions.push_back([owner=shadow_filtered_scene]{owner->close();});
        }
        cleanup.actions.push_back([combined]{combined->close();});
    }
    {
        auto source=combined_source_inputs[0];
        auto second=source.compute_models[0];second.key=0x7ffe1234;second.packet_index=source.packets.size();
        source.packets.push_back(source.packets[source.compute_models[0].packet_index]);
        source.handles.push_back(second.key);source.compute_models.push_back(std::move(second));
        VulkanSourceScene scene;
        require(scene.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,targets.render_pass(),source),scene.status().c_str());
        VulkanComputeRayScene::Plan plan;
        require(VulkanComputeRayScene::plan(source.compute_models,256,plan) && plan.parts.size()>=2,"Multi-model ray plan failed");
        auto colour_only_models=source.compute_models;
        auto colour_only_warp=std::make_shared<SourceWarpInputs>();
        colour_only_models.front().warp=colour_only_warp;
        VulkanComputeRayScene::Plan colour_only_plan;
        SourceRayCoverage colour_only_materials;
        require(VulkanComputeRayScene::plan(colour_only_models,256,colour_only_plan)
            && colour_only_plan.parts.front().warp_candidates
            && VulkanComputeRayScene::coverage(colour_only_models,colour_only_plan,colour_only_materials),"Colour-only warp coverage rejected");
        colour_only_warp->textures.texels={0,1};
        require(!VulkanComputeRayScene::plan(colour_only_models,256,colour_only_plan)
            && !VulkanComputeRayScene::coverage(colour_only_models,plan,colour_only_materials),"Dynamic textured warp accepted as opaque");
        SourceRayCoverage ray_materials;
        require(VulkanComputeRayScene::coverage(source.compute_models,plan,ray_materials)
            && ray_materials.triangles.size()==plan.bytes/48,"Multi-model material layout failed");
        size_t coverage_end=0;
        for(const auto& part:plan.parts) {
            for(size_t i=coverage_end;i<part.offset/48;++i)
                require(ray_materials.triangles[i].flags==0,"Ray padding material corrupted");
            coverage_end=part.offset/48+part.topology.size();
        }
        auto invalid_coverage_plan=plan;invalid_coverage_plan.parts.back().offset=1;
        const auto coverage_count=ray_materials.triangles.size();
        require(!VulkanComputeRayScene::coverage(source.compute_models,invalid_coverage_plan,ray_materials)
            && ray_materials.triangles.size()==coverage_count,"Invalid material plan modified output");
        VulkanSourceStorage output;
        require(output.initialize(device,vkGetDeviceProcAddr,memory_properties,plan.bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT),output.status().c_str());
        VulkanSpanPipeline ray_pipeline;
        require(ray_pipeline.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::ray_expand),ray_pipeline.status().c_str());
        VulkanComputeRayScene rays;
        const Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        require(rays.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,ray_pipeline,scene,
            {output.buffer(),0,output.size()},plan,EyeCamera{identity,identity}),"Multi-model ray initialization failed");
        std::vector<float> results(plan.bytes/4,99);
        require(output.upload(0,std::as_bytes(std::span(results))),"Seed ray padding");
        check(vkResetCommandPool(device,pool,0),"Reset ray scene commands");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(copy,&begin),"Begin ray scene commands");
        require(scene.record_compute(copy) && rays.record(copy),"Record multi-model ray scene");
        VkMemoryBarrier visible{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        visible.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;visible.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
            0,1,&visible,0,nullptr,0,nullptr);
        check(vkEndCommandBuffer(copy),"End ray scene commands");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
        check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit ray scene");check(vkQueueWaitIdle(queue),"Wait ray scene");
        require(output.readback(0,std::as_writable_bytes(std::span(results))),"Read ray scene");
        size_t previous=0;
        for(const auto& part:plan.parts) {
            for(size_t i=previous;i<part.offset/4;++i) require(results[i]==0,"Stale geometry in ray padding");
            const auto end=part.offset/4+part.topology.size()*12;
            for(size_t i=part.offset/4;i<end;++i) require(std::isfinite(results[i]) && (i%4!=3 || results[i]==1),"Invalid ray model output");
            previous=end;
        }
        require(rays.vertex_count()==plan.bytes/16,"Ray scene vertex count mismatch");
        require(plan.parts.front().topology.size()==plan.parts.back().topology.size(),"Duplicate ray fixture topology mismatch");
        for(size_t i=0;i<plan.parts.front().topology.size()*12;++i)
            require(results[plan.parts.front().offset/4+i]==results[plan.parts.back().offset/4+i],
                "Identical model rays differ across aligned output slots");
        require(scene.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,targets.render_pass(),source),scene.status().c_str());
        check(vkResetCommandPool(device,pool,0),"Reset stale ray commands");check(vkBeginCommandBuffer(copy,&begin),"Begin stale ray commands");
        require(!rays.record(copy),"Stale source generation was consumed");
        auto wrong_plan=plan;wrong_plan.parts[0].topology[0][0]=UINT32_MAX;
        require(!rays.refresh(wrong_plan,EyeCamera{identity,identity}) && !rays.record(copy),"Changed ray topology reused descriptors");
        auto shifted_eye=EyeCamera{identity,identity};shifted_eye.view[12]=.25F;
        require(rays.refresh(plan,shifted_eye) && rays.refresh(plan,shifted_eye),"Compatible ray refresh failed");
        require(scene.record_compute(copy) && rays.record(copy),"Record refreshed ray scene");
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
            0,1,&visible,0,nullptr,0,nullptr);
        check(vkEndCommandBuffer(copy),"End refreshed ray commands");
        check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit refreshed ray scene");check(vkQueueWaitIdle(queue),"Wait refreshed rays");
        const auto original_results=results;
        require(output.readback(0,std::as_writable_bytes(std::span(results))),"Read refreshed rays");
        std::vector<bool> populated(results.size(),false);
        for(const auto& part:plan.parts) for(size_t i=part.offset/4;i<part.offset/4+part.topology.size()*12;++i) {
            populated[i]=true;
            require(std::abs(results[i]-(original_results[i]+(i%4==0?64.F:0.F)))<.001F,"Refreshed ray placement mismatch");
        }
        for(size_t i=0;i<results.size();++i) if(!populated[i]) require(results[i]==0,"Refreshed ray padding was not cleared");
        rays.close();
        std::cout<<"Scene-owned multi-model ray expansion, padding clear and stale-generation rejection passed\n";
    }
    std::array<uint32_t,320> test_texels{0xff00ff00U,0U,0xff808080U};
    // Indexed-texture fixture shares the arena with the original RGBA cases.
    // Material at word 8, packed bytes at 32, palette at 64.
    test_texels[8+9]=1;test_texels[8+11]=112;
    test_texels[32]=1; // byte indices {1,0,0,0}: opaque then transparent.
    test_texels[64+113]=0xff00ff00U;
    test_texels[40+11]=112;test_texels[40+8]=4;
    test_texels[33]=2;test_texels[64+114]=0xff808080U;
    require(texture_fixture.initialize(device,vkGetDeviceProcAddr,memory_properties,test_texels),texture_fixture.status().c_str());
    if(!cartridge.texels.empty()) require(cartridge_textures.initialize(device,vkGetDeviceProcAddr,memory_properties,cartridge.texels),cartridge_textures.status().c_str());
    VulkanScenePipeline pipeline;require(pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),true,SceneTopology::triangles,VK_NULL_HANDLE,SceneBlend::opaque,graphics_cache_ptr),pipeline.status().c_str());
    VulkanScenePipeline line_pipeline;
    VulkanSceneBuffer cartridge_lines;
    if(!cartridge.line_vertices.empty()) {
        require(line_pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),true,SceneTopology::lines,cartridge_textures.layout()),line_pipeline.status().c_str());
        require(cartridge_lines.initialize(device,vkGetDeviceProcAddr,memory_properties,cartridge.line_vertices),cartridge_lines.status().c_str());
    }
    // The farther red triangle is deliberately submitted last. Painter order
    // alone would hide green; depth testing must preserve the near triangle.
    const std::array<SceneVertex,6> triangle{{
        {{-.4F,-.3F,-2},{0,1,0,1}},{{.4F,-.3F,-2},{0,1,0,1}},{{0,.4F,-2},{0,1,0,1}},
        {{-.9F,-.675F,-3},{1,0,0,1}},{{.9F,-.675F,-3},{1,0,0,1}},{{0,.9F,-3},{1,0,0,1}}}};
    VulkanSceneBuffer vertices;require(vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,triangle),vertices.status().c_str());
    VulkanDrawPackets packet_scene;
    VulkanSourceScene live_compute_scene;
    if(live_compute || resident_model) require(live_compute_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,
        properties.limits,targets.render_pass(),live_compute_packets),live_compute_scene.status().c_str());
    VulkanDrawPackets sprite_scene;
    VulkanScenePipeline live_shutter_pipeline;
    VulkanSceneBuffer live_shutter_vertices;
    if(!live_shutter.geometry.vertices.empty()) {
        require(live_shutter_pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),false),live_shutter_pipeline.status().c_str());
        require(live_shutter_vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,live_shutter.geometry.vertices),live_shutter_vertices.status().c_str());
    }
    VulkanDrawPackets background_scene;
    VulkanDrawPackets tunnel_surround_scene;
    if(!live_tunnel_surround.empty())
        require(tunnel_surround_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),live_tunnel_surround,{},false),"Initialize tunnel surround");
    if(full_layers)
        require(background_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),live_backgrounds,{},false),background_scene.status().c_str());
    if(combined_live)
        require(sprite_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),live_sprites,{},false),sprite_scene.status().c_str());
    std::array<DrawPacket,2> packets;
    for(unsigned i=0;i<2;++i) {
        packets[i].model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        packets[i].geometry.vertices.assign(triangle.begin()+i*3,triangle.begin()+i*3+3);
    }
    packets[0].model[12]=.125F;
    for(auto& vertex:packets[0].geometry.vertices) vertex.position[0]-=.125F;
    packets[1].model[13]=.125F;packets[1].geometry.texels={0xff0000ffU};
    for(auto& vertex:packets[1].geometry.vertices) {vertex.position[1]-=.125F;vertex.texture[3]=1;}
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets),packet_scene.status().c_str());
    require(packet_scene.size()==2,"Multi-object native upload lost a packet");
    VulkanDrawPackets::RaySource legacy_ray;
    require(!packet_scene.ray_source(17,legacy_ray),"Unkeyed ray lookup accepted");
    const std::array<uint32_t,2> keys{17,29};
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,keys),packet_scene.status().c_str());
    require(packet_scene.ray_source(17,legacy_ray) && legacy_ray.count==3
        && legacy_ray.vertices.range==3*sizeof(SceneVertex) && legacy_ray.model==packets[0].model,"Legacy resident ray source mismatch");
    const auto legacy_ray_buffer=legacy_ray.vertices.buffer;
    {
        VulkanDrawPackets eligibility;
        auto changed=packets;
        for(auto& vertex:changed[0].geometry.vertices) vertex.texture[3]=4;
        require(eligibility.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,keys),eligibility.status().c_str());
        VulkanDrawPackets::RaySource candidate;
        require(eligibility.ray_source(17,candidate),"Plain ray eligibility missing");
        require(eligibility.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),changed,keys),eligibility.status().c_str());
        require(eligibility.ray_source(17,candidate),"Billboard ray eligibility missing");
        require(eligibility.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,keys),eligibility.status().c_str());
        require(eligibility.ray_source(17,candidate),"Restored plain eligibility remained stale");
    }
    {
        auto mixed_input=combined_source_inputs[0];
        mixed_input.packets.push_back(packets[0]);mixed_input.handles.push_back(0x7ffe5678);
        VulkanSourceScene mixed;
        require(mixed.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,targets.render_pass(),mixed_input),mixed.status().c_str());
        VulkanSourceScene::LegacyRaySource scene_legacy;
        VulkanSourceScene::RaySource scene_compute;
        require(mixed.legacy_ray_source(0x7ffe5678,scene_legacy)
            && mixed.ray_source(mixed_input.compute_models[0].key,scene_compute)
            && scene_legacy.generation==scene_compute.generation,"Mixed ray scene inputs mismatch");
        require(!mixed.ray_source(0x7ffe5678,scene_compute)
            && !mixed.legacy_ray_source(mixed_input.compute_models[0].key,scene_legacy),"Mixed ray paths overlapped");
        require(mixed.legacy_ray_source(0x7ffe5678,scene_legacy),"Mixed legacy source lost");
        VulkanSpanPipeline expansion;
        require(expansion.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::ray_expand),expansion.status().c_str());
        VulkanSourceStorage output;
        require(output.initialize(device,vkGetDeviceProcAddr,memory_properties,legacy_ray.count*16),output.status().c_str());
        VulkanLegacyRayGeometry ray;
        const Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        const EyeCamera eye{identity,identity};
        require(ray.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,expansion,scene_legacy.geometry,
            {output.buffer(),0,output.size()},eye),"Legacy retained ray initialization failed");
        auto shifted=scene_legacy.geometry;shifted.model[12]+=.125F;
        require(ray.update(shifted,eye) && ray.update(shifted,eye),"Legacy retained transform update failed");
        check(vkResetCommandPool(device,pool,0),"Reset legacy ray commands");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(copy,&begin),"Begin legacy ray commands");
        require(ray.record(copy),"Record legacy rays");
        VkMemoryBarrier visible{VK_STRUCTURE_TYPE_MEMORY_BARRIER};visible.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;visible.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&visible,0,nullptr,0,nullptr);
        check(vkEndCommandBuffer(copy),"End legacy ray commands");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
        check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit legacy rays");check(vkQueueWaitIdle(queue),"Wait legacy rays");
        std::array<std::array<float,4>,3> result;
        require(output.readback(0,std::as_writable_bytes(std::span(result))),"Read legacy rays");
        for(unsigned i=0;i<3;++i) {
            const auto& vertex=packets[0].geometry.vertices[i];
            for(unsigned row=0;row<3;++row) {
                float world=shifted.model[12+row];
                for(unsigned col=0;col<3;++col) world+=shifted.model[col*4+row]*vertex.position[col];
                require(std::abs(result[i][row]-world*(row?-256.F:256.F))<.001F,"Legacy ray placement mismatch");
            }
            require(result[i][3]==1,"Legacy ray vertex missing");
        }
        VulkanMixedRayScene::Plan mixed_plan;
        require(VulkanMixedRayScene::plan(mixed_input,mixed,256,mixed_plan)
            && mixed_plan.compute.bytes && mixed_plan.legacy.size()==2,"Mixed ray plan missing a path");
        SourceRayCoverage mixed_materials;
        {
            StereoRayPlan cache;
            require(cache.prepare(mixed_input,mixed,256),"Stereo ray cache initial preparation failed");
            const auto* storage=cache.materials().triangles.data();
            require(cache.prepare(mixed_input,mixed,256) && storage==cache.materials().triangles.data(),
                "Second eye rebuilt shared material storage");
            auto invalid=mixed_input;invalid.handles.clear();cache.invalidate();
            require(!cache.prepare(invalid,mixed,256) && cache.materials().triangles.empty(),
                "Invalid frame retained prior materials");
            require(!cache.prepare(mixed_input,mixed,256),"Failure not retained for second eye");
            cache.invalidate();
            require(cache.prepare(mixed_input,mixed,256),"Stereo cache failed to recover after invalidation");
        }
        {
            SourceModelPackets legacy_only;
            legacy_only.packets.push_back(packets[0]);legacy_only.handles.push_back(0x5678);
            VulkanSourceScene legacy_scene;
            require(legacy_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                targets.render_pass(),legacy_only),"Legacy-only source scene rejected");
            VulkanMixedRayScene::Plan legacy_plan;SourceRayCoverage legacy_materials;
            require(VulkanMixedRayScene::plan(legacy_only,legacy_scene,256,legacy_plan)
                && legacy_plan.compute.bytes==0 && legacy_plan.legacy.size()==1 && legacy_plan.bytes>0
                && VulkanMixedRayScene::coverage(legacy_only,legacy_plan,legacy_materials),
                "Legacy-only ray path requires a compute model");
        }
        require(VulkanMixedRayScene::coverage(mixed_input,mixed_plan,mixed_materials)
            && mixed_materials.triangles.size()==mixed_plan.bytes/48,"Mixed material assembly failed");
        auto textured_input=mixed_input;
        const auto texture_key=mixed_plan.legacy.back().key;
        size_t texture_packet=0;
        while(textured_input.handles[texture_packet]!=texture_key) ++texture_packet;
        auto& texture_batch=textured_input.packets[texture_packet].geometry;
        auto source_vertices=texture_batch.vertex_view();
        texture_batch.vertices=std::vector<SceneVertex>(source_vertices.begin(),source_vertices.end());
        texture_batch.shared_vertices.reset();texture_batch.texels={0xffffffff,0};
        for(auto& v:texture_batch.vertices) {v.texture[0]=0;v.texture[1]=1;v.texture[2]=0;v.texture[3]=1;v.uv[0]=.5F;v.uv[1]=0;}
        require(VulkanMixedRayScene::coverage(textured_input,mixed_plan,mixed_materials),"Legacy textured material rejected");
        const auto& ray_texture=mixed_materials.triangles[mixed_plan.legacy.back().offset/48];
        require(ray_texture.flags==1 && ray_texture.u_mask==1 && ray_texture.uv[0]==.5F
            && mixed_materials.texels[ray_texture.offset]==0xffffffff
            && mixed_materials.texels[ray_texture.offset+1]==0,"Legacy texture relocation incorrect");
        texture_batch.vertices[1].texture[0]=1;
        require(!VulkanMixedRayScene::coverage(textured_input,mixed_plan,mixed_materials),"Inconsistent triangle material accepted");
        texture_batch.texels.assign(257,0);texture_batch.texels[7]=0xff123456;texture_batch.texels[256]=7;
        for(auto& v:texture_batch.vertices) {v.texture[0]=0;v.texture[3]=0x28000005U;}
        require(VulkanMixedRayScene::coverage(textured_input,mixed_plan,mixed_materials),"Indexed sprite material rejected");
        const auto indexed_material=mixed_materials.triangles[mixed_plan.legacy.back().offset/48];
        require(mixed_materials.texels[indexed_material.offset]==0xff123456
            && mixed_materials.texels[indexed_material.offset+1]==0,"Indexed sprite palette/alpha mismatch");
        for(size_t i=0;i<texture_batch.vertices.size()/3;++i)
            require(mixed_materials.triangles[mixed_plan.legacy.back().offset/48+i].offset==indexed_material.offset,
                "Indexed texture decoded again for a shared face");
        require(mixed_materials.texels.size()==indexed_material.offset+2,"Duplicate indexed texture storage");
        texture_batch.texels.pop_back();
        require(!VulkanMixedRayScene::coverage(textured_input,mixed_plan,mixed_materials),"Truncated indexed sprite accepted");
        auto shadow_input=mixed_input;
        auto shadow_model=shadow_input.compute_models.front();shadow_model.key=source_shadow_pass|shadow_model.key;
        shadow_input.compute_models.push_back(std::move(shadow_model));
        shadow_input.packets.push_back(packets[0]);shadow_input.handles.push_back(source_shadow_pass|1234);
        VulkanMixedRayScene::Plan no_duplicate_shadows;
        require(VulkanMixedRayScene::plan(shadow_input,mixed,256,no_duplicate_shadows)
            && no_duplicate_shadows.bytes==mixed_plan.bytes
            && no_duplicate_shadows.compute.parts.size()==mixed_plan.compute.parts.size()
            && no_duplicate_shadows.legacy.size()==mixed_plan.legacy.size(),"Native shadow visuals entered ray geometry");
        VulkanSourceStorage mixed_output;
        require(mixed_output.initialize(device,vkGetDeviceProcAddr,memory_properties,mixed_plan.bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT),mixed_output.status().c_str());
        VulkanMixedRayScene mixed_rays;
        require(mixed_rays.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,expansion,mixed,
            {mixed_output.buffer(),0,mixed_output.size()},mixed_plan,eye),"Mixed ray assembly failed");
        check(vkResetCommandPool(device,pool,0),"Reset mixed ray commands");
        check(vkBeginCommandBuffer(copy,&begin),"Begin mixed ray commands");
        require(mixed.record_compute(copy) && mixed_rays.record(copy),"Record mixed ray paths");
        visible.srcAccessMask|=VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
            0,1,&visible,0,nullptr,0,nullptr);
        check(vkEndCommandBuffer(copy),"End mixed ray commands");
        check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit mixed rays");check(vkQueueWaitIdle(queue),"Wait mixed rays");
        std::vector<float> mixed_result(mixed_plan.bytes/4);
        require(mixed_output.readback(0,std::as_writable_bytes(std::span(mixed_result))),"Read mixed ray paths");
        const auto first=mixed_plan.legacy.back().offset/4;
        for(unsigned i=0;i<3;++i) for(unsigned axis=0;axis<4;++axis)
            require(std::abs(mixed_result[first+i*4+axis]-(result[i][axis]-(axis==0?32.F:0.F)))<.001F,"Mixed legacy output mismatch");
        size_t gap_start=mixed_plan.compute.bytes/4;
        for(const auto& part:mixed_plan.legacy) {
            for(size_t i=gap_start;i<part.offset/4;++i) require(mixed_result[i]==0,"Mixed alignment gap not cleared");
            gap_start=part.offset/4+part.count*4;
            for(size_t i=part.offset/4;i<gap_start;++i)
                require(std::isfinite(mixed_result[i]) && (i%4!=3 || mixed_result[i]==1),"Mixed legacy model missing");
        }
        for(const auto& part:mixed_plan.compute.parts) for(size_t i=part.offset/4;i<part.offset/4+part.topology.size()*12;++i)
            require(std::isfinite(mixed_result[i]) && (i%4!=3 || mixed_result[i]==1),"Mixed compute model missing");
        require(mixed_rays.vertex_count()==mixed_plan.bytes/16,"Mixed AS vertex count mismatch");
        check(vkResetCommandPool(device,pool,0),"Reset mixed ray refresh");
        check(vkBeginCommandBuffer(copy,&begin),"Begin mixed ray refresh");
        auto invalid_mixed_plan=mixed_plan;invalid_mixed_plan.legacy[0].offset+=48;
        require(!mixed_rays.refresh(invalid_mixed_plan,eye) && !mixed_rays.record(copy),"Changed mixed layout was recorded");
        auto moved_eye=eye;moved_eye.view[12]=.25F;
        require(mixed_rays.refresh(mixed_plan,moved_eye) && mixed_rays.refresh(mixed_plan,moved_eye),"Retained mixed refresh failed");
        require(mixed.record_compute(copy) && mixed_rays.record(copy),"Record refreshed mixed rays");
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
            0,1,&visible,0,nullptr,0,nullptr);
        check(vkEndCommandBuffer(copy),"End mixed ray refresh");
        check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit mixed ray refresh");check(vkQueueWaitIdle(queue),"Wait mixed ray refresh");
        const auto previous_mixed=mixed_result;
        require(mixed_output.readback(0,std::as_writable_bytes(std::span(mixed_result))),"Read mixed ray refresh");
        for(size_t vertex=0;vertex<mixed_result.size();vertex+=4) for(unsigned axis=0;axis<4;++axis) {
            const float delta=axis==0 && previous_mixed[vertex+3]==1?64.F:0.F;
            require(std::abs(mixed_result[vertex+axis]-(previous_mixed[vertex+axis]+delta))<.001F,"Mixed retained camera transform mismatch");
        }
        mixed_rays.close();
        ray.close();
        const auto generation=scene_legacy.generation;
        require(mixed.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,targets.render_pass(),combined_source_inputs[0]),mixed.status().c_str());
        require(!mixed.legacy_ray_source(0x7ffe5678,scene_legacy)
            && mixed.ray_source(mixed_input.compute_models[0].key,scene_compute)
            && scene_compute.generation!=generation,"Removed legacy ray input survived scene update");
    }
    const std::array<DrawPacket,2> reordered{packets[1],packets[0]};
    const std::array<uint32_t,2> reordered_keys{29,17};
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),reordered,reordered_keys),packet_scene.status().c_str());
    require(packet_scene.reused_packets()==2 && packet_scene.uploaded_packets()==0,"Reordered objects re-uploaded unchanged geometry");
    require(packet_scene.ray_source(17,legacy_ray) && legacy_ray.vertices.buffer==legacy_ray_buffer,"Reordered ray source changed buffers");
    require(!packet_scene.ray_source(UINT32_MAX,legacy_ray),"Missing legacy ray key accepted");
    auto inserted=std::vector<DrawPacket>{packets[0],DrawPacket{},packets[1],packets[0]};
    // Match the live Original bitmap placeholder: default-constructed, empty,
    // with no caller-supplied transform. This must not abort the entire frame.
    const std::array<uint32_t,4> inserted_keys{8,9,29,17};
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),inserted,inserted_keys),packet_scene.status().c_str());
    require(packet_scene.size()==3 && packet_scene.reused_packets()==2 && packet_scene.uploaded_packets()==1,"Insertion invalidated unchanged object uploads");
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),inserted),packet_scene.status().c_str());
    require(packet_scene.size()==3,"Unkeyed empty bitmap placeholder lost drawable layers");
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),inserted,inserted_keys),packet_scene.status().c_str());
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,keys),packet_scene.status().c_str());
    require(packet_scene.reused_packets()==2 && packet_scene.uploaded_packets()==0,"Removal invalidated surviving object uploads");
    auto recycled=packets;recycled[0].geometry.vertices[0].uv[0]+=1;
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),recycled,keys),packet_scene.status().c_str());
    require(packet_scene.reused_packets()==1 && packet_scene.uploaded_packets()==1,"Recycled key reused stale geometry");
    const std::array<uint32_t,2> duplicates{17,17};
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,duplicates)
        && packet_scene.size()==2,"Duplicate keys replaced working scene");
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,std::span<const uint32_t>(keys.data(),1))
        && packet_scene.size()==2,"Partial keys replaced working scene");
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets),packet_scene.status().c_str());
    std::cout<<"Keyed uploads: reorder/remove reuse, insertion, empty packets and recycled/invalid keys passed\n";
    const std::array<uint32_t,2> pass_keys{17,0x10011};
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,pass_keys),packet_scene.status().c_str());
    const std::array<uint32_t,2> reversed_pass_keys{0x10011,17};
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),reordered,reversed_pass_keys),packet_scene.status().c_str());
    require(packet_scene.reused_packets()==2 && packet_scene.uploaded_packets()==0,
        "Same-object separate passes collided or lost upload reuse");
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,keys),packet_scene.status().c_str());
    std::cout<<"32-bit pass keys: same source handle, separate pass identities preserve geometry reuse\n";
    std::array<Matrix4,2> models{packets[0].model,packets[1].model};
    auto shared_packets=packets;
    for(auto& packet:shared_packets) {
        packet.geometry.shared_vertices=std::make_shared<const std::vector<SceneVertex>>(packet.geometry.vertices);
        packet.geometry.vertices.clear();
    }
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),shared_packets,keys),packet_scene.status().c_str());
    require(packet_scene.uploaded_packets()==0 && packet_scene.reused_packets()==2,
        "Shared/owned vertex representation change re-uploaded identical geometry");
    auto ambiguous=shared_packets;ambiguous[0].geometry.vertices=packets[0].geometry.vertices;
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),ambiguous,keys)
        && packet_scene.size()==2,"Ambiguous shared/owned vertices replaced live scene");
    auto payload_changed=packets;payload_changed[1].geometry.texels[0]=0xff00ff00U;
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),payload_changed,keys),packet_scene.status().c_str());
    require(packet_scene.uploaded_vertex_buffers()==0 && packet_scene.uploaded_texture_buffers()==1,
        "Payload-only change re-uploaded immutable vertices");
    auto vertex_changed=payload_changed;vertex_changed[1].geometry.vertices[0].position[0]+=.1F;
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),vertex_changed,keys),packet_scene.status().c_str());
    require(packet_scene.uploaded_vertex_buffers()==1 && packet_scene.uploaded_texture_buffers()==0,
        "Vertex-only change re-uploaded unchanged payload");
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,keys),packet_scene.status().c_str());
    require(packet_scene.uploaded_vertex_buffers()==1 && packet_scene.uploaded_texture_buffers()==1,
        "Restoring changed vertex/payload failed to replace both");
    std::cout<<"Independent vertex/payload uploads: payload-only 0/1, vertex-only 1/0, restore 1/1 passed\n";
    {
        VulkanDrawPackets immutable_scene;
        std::array<DrawPacket,1> immutable{packets[1]};
        auto& mesh=immutable[0].geometry;
        mesh.shared_texels=std::make_shared<const std::vector<uint32_t>>(std::move(mesh.texels));
        mesh.texels.clear();
        require(immutable_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),immutable),immutable_scene.status().c_str());
        require(immutable_scene.uploaded_texture_buffers()==1,"Initial immutable payload was not uploaded");
        auto moved=immutable;moved[0].model[12]+=.1F;
        require(moved[0].geometry.texel_view().data()==mesh.texel_view().data(),"Packet copy duplicated immutable pixels");
        require(immutable_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),moved),immutable_scene.status().c_str());
        require(immutable_scene.reused_packets()==1 && immutable_scene.uploaded_texture_buffers()==0,"Transform change uploaded immutable pixels");
        moved[0].geometry.vertices[0].position[0]+=.1F;
        require(immutable_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),moved),immutable_scene.status().c_str());
        require(immutable_scene.uploaded_vertex_buffers()==1 && immutable_scene.uploaded_texture_buffers()==0,"Vertex change uploaded immutable pixels");
        auto changed=*mesh.shared_texels;changed[0]^=0x00ffffffU;
        moved[0].geometry.shared_texels=std::make_shared<const std::vector<uint32_t>>(std::move(changed));
        require(immutable_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),moved),immutable_scene.status().c_str());
        require(immutable_scene.uploaded_vertex_buffers()==0 && immutable_scene.uploaded_texture_buffers()==1,"Replaced immutable payload was not uploaded independently");
        auto invalid=moved;invalid[0].geometry.texels.push_back(0);
        require(!immutable_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid)
            && immutable_scene.size()==1,"Ambiguous shared/owned texels replaced live scene");
        invalid=moved;invalid[0].geometry.shared_texels=std::make_shared<const std::vector<uint32_t>>();
        require(!immutable_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid)
            && immutable_scene.size()==1,"Truncated shared texels replaced live scene");
        std::cout<<"Immutable texture payloads: retained identity, independent uploads and rejection passed\n";
    }
    {
        starfox::simulation::SnesPpuState ground_ppu{};ground_ppu.main_screen=2;ground_ppu.background_mode=2;
        const auto sky=landscape_sphere_packet(ground_ppu,{},112.F);
        VulkanDrawPackets ground_scene;
        for(const float height:{-.001F,-.125F,-.25F,-1.F,-8.F}) {
            auto ground=sky;place_landscape_ground(ground,height,true);
            const std::array<DrawPacket,1> draw{ground};
            require(ground_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),draw),ground_scene.status().c_str());
            require(ground_scene.uploaded_vertex_buffers()==unsigned(height==-.001F)
                && ground_scene.uploaded_texture_buffers()==1,"Ground height change re-uploaded immutable vertices");
            require(ground_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),draw)
                && ground_scene.uploaded_vertex_buffers()==0 && ground_scene.uploaded_texture_buffers()==0,
                "Unchanged ground height uploaded buffers");
        }
        ground_scene.close();
        std::cout<<"GPU ground upload reuse: five heights, payload-only changes and zero-upload repeats passed\n";
    }
    auto invalid_models=models;invalid_models[1][15]=0;
    require(!packet_scene.update_models(invalid_models),"Invalid transform-only update accepted");
    require(!packet_scene.update_models(std::span<const Matrix4>(models.data(),1)),"Partial transform-only update accepted");
    auto invalid_packets=packets;invalid_packets[1].geometry.vertices[0].texture[0]=1;
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid_packets)
        && packet_scene.size()==2,"Invalid texture replaced the live GPU scene");
    auto row_fixture=packets;
    {
        auto raw=packets;auto& mesh=raw[0].geometry;
        mesh.vertices.resize(6,mesh.vertices.front());mesh.texels.assign(14,0);
        for(auto& v:mesh.vertices) {v.texture[0]=v.texture[1]=v.texture[2]=0;v.texture[3]=gpu_connected_grid_flag;}
        require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),raw),
            "Valid raw compute grid rejected");
        std::vector<uint32_t> unwritten(connected_grid_output_words);
        require(!packet_scene.readback_connected_grid(0,unwritten),"Unprepared compute grid exposed undefined storage");
        require(packet_scene.allocated_grid_outputs()==1 && packet_scene.reused_grid_outputs()==0,
            "First grid arena allocation not recorded");
        mesh.texels[3]=mesh.texels[7]=mesh.texels[11]=32767;
        const starfox::simulation::MatrixQ15 matrix{32767,0,0,0,32767,0,0,0,32767};
        const std::array<int16_t,3> cameras[]{{0,-600,0},{31,-600,31},{255,-600,255},
            {256,-600,256},{-1,-600,-1},{-32768,-600,32767},{32767,32767,-32768},{0,-32768,0}};
        std::vector<uint32_t> current(connected_grid_output_words);
        for(const auto& position:cameras) {
            for(unsigned axis=0;axis<3;++axis) mesh.texels[axis]=uint32_t(int32_t(position[axis]));
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),raw),
                "Changed raw grid update failed");
            require(packet_scene.allocated_grid_outputs()==0 && packet_scene.reused_grid_outputs()==1
                && packet_scene.uploaded_vertex_buffers()==0,"Grid camera update reallocated output or vertex storage");
            require(!packet_scene.readback_connected_grid(0,unwritten),"Updated source exposed the preceding frame's output");
            check(vkResetCommandPool(device,pool,0),"Reset grid producer");
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            check(vkBeginCommandBuffer(copy,&begin),"Begin grid producer");
            require(packet_scene.record_compute(copy),"Record retained grid output");
            check(vkEndCommandBuffer(copy),"End grid producer");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
            check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit retained grid");
            check(vkQueueWaitIdle(queue),"Wait retained grid");
            require(packet_scene.readback_connected_grid(0,current),"Read retained grid output");
            starfox::timing::RenderTransform camera;camera.x=position[0];camera.y=position[1];camera.z=position[2];
            const auto expected=starfox::render::project_source_grid(camera,matrix,224,192);
            size_t visible=0;
            for(unsigned i=0;i<225;++i) {
                const auto offset=connected_grid_output_words-225*4+i*4;
                if(!current[offset+3]) continue;
                require(visible<expected.count,"GPU grid exposed a rejected point");
                const auto& point=expected.points[visible++];
                require(int32_t(current[offset])==point.x && int32_t(current[offset+1])==point.y
                    && int32_t(current[offset+2])==point.depth,"Retained grid projection differs after camera update");
            }
            require(visible==expected.count,"Retained grid lost projected points");
            auto invalid=raw;invalid[0].geometry.texels.pop_back();
            require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid)
                && packet_scene.readback_connected_grid(0,unwritten) && unwritten==current,
                "Failed grid replacement corrupted the completed prior output");
        }
        require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),raw)
            && packet_scene.reused_grid_outputs()==1 && packet_scene.allocated_grid_outputs()==0
            && packet_scene.uploaded_texture_buffers()==0,"Unchanged grid uploaded inputs or reallocated arena");
        std::cout<<"Connected-grid arena: eight camera/wrap updates match CPU, zero output/vertex reallocations; failed updates preserve output\n";
        {
            auto pipeline=std::make_shared<VulkanConnectedGridPipeline>();
            require(pipeline->initialize(device,vkGetDeviceProcAddr,VK_NULL_HANDLE),pipeline->status().c_str());
            const auto benchmark=[&](bool reuse) {
                std::unique_ptr<VulkanConnectedGrid> previous;
                auto words=mesh.texels;
                const auto started=std::chrono::steady_clock::now();
                for(unsigned frame=0;frame<128;++frame) {
                    words[0]=frame;
                    auto next=std::make_unique<VulkanConnectedGrid>();
                    require(next->initialize(device,vkGetDeviceProcAddr,memory_properties,words,pipeline,reuse?previous.get():nullptr),next->status().c_str());
                    require(next->reused_output()==(reuse && frame!=0),"Grid benchmark did not exercise intended allocation path");
                    if(reuse && previous) require(next->buffer()==previous->buffer(),"Grid output handle changed during reuse");
                    previous=std::move(next);
                }
                return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count()/128;
            };
            const auto fresh=benchmark(false),retained=benchmark(true);
            std::cout<<"Connected-grid setup: fresh arena "<<fresh<<" us, retained arena "<<retained
                <<" us/update; "<<connected_grid_output_words*4<<" output bytes retained (CPU/Vulkan setup only, not GPU time or FPS)\n";
        }
        require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets),packet_scene.status().c_str());
        for(unsigned failure=0;failure<8;++failure) {
            auto invalid=raw;auto& bad=invalid[0].geometry;
            switch(failure) {
            case 0: bad.texels.pop_back();break;
            case 1: bad.texels.push_back(0);break;
            case 2: bad.texels[0]=32768;break;
            case 3: bad.texels[1]=uint32_t(-32769);break;
            case 4: bad.vertices[0].texture[3]=0;break;
            case 5: bad.vertices[0].uv[0]=std::numeric_limits<float>::infinity();break;
            case 6: bad.vertices[0].texture[1]=1;break;
            case 7: bad.line_vertices={bad.vertices[0],bad.vertices[1]};break;
            }
            require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid)
                && packet_scene.size()==2,"Malformed compute grid replaced working scene");
        }
        std::cout<<"Raw compute-grid validation: eight malformed packets rejected, prior scene preserved\n";
    }
    auto& row_mesh=row_fixture[0].geometry;
    row_mesh.texels.assign(390,0);
    for(unsigned row=0;row<192;++row) row_mesh.texels[row*2]=389;
    row_mesh.texels[1]=1;row_mesh.texels[384]=1;row_mesh.texels[389]=384;
    for(auto& v:row_mesh.vertices) {v.texture[0]=0;v.texture[3]=512;}
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),row_fixture),
        "Valid connected-grid row fixture rejected");
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets),packet_scene.status().c_str());
    for(unsigned failure=0;failure<11;++failure) {
        auto invalid=row_fixture;auto& mesh=invalid[0].geometry;
        switch(failure) {
        case 0: mesh.texels.resize(383);break;
        case 1: mesh.vertices[0].texture[3]|=256;break;
        case 2: mesh.vertices[0].texture[0]=1;break;
        case 3: mesh.texels[0]=0;break;
        case 4: mesh.texels[0]=391;break;
        case 5: mesh.texels[1]=676;break;
        case 6: mesh.texels[389]=386;break;
        case 7: mesh.texels[384]=2;break;
        case 8: mesh.texels[385]=8192;break;
        case 9: mesh.texels[385]=uint32_t(-8193);break;
        case 10: mesh.texels[389]=0;break;
        }
        require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid)
            && packet_scene.size()==2,"Invalid connected-grid rows replaced working scene");
        require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets)
            && packet_scene.reused_packets()==2 && packet_scene.uploaded_packets()==0,
            "Connected-grid validation failure mutated retained uploads");
    }
    std::cout<<"Connected-grid rows: 11 malformed payloads rejected, retained uploads unchanged\n";
    for(unsigned failure=0;failure<9;++failure) {
        auto invalid=packets;auto& mesh=invalid[0].geometry;
        mesh.texels.assign(9,0xffffffffU);
        for(auto& v:mesh.vertices) {v.texture[0]=0;v.texture[1]=v.texture[2]=15;v.texture[3]=1028;}
        switch(failure) {
        case 0: mesh.texels.resize(8);break;
        case 1: mesh.vertices[0].texture[0]=1;break;
        case 2: mesh.vertices[0].texture[0]=UINT32_MAX;break;
        case 3: mesh.vertices[0].texture[1]=16;break;
        case 4: mesh.vertices[0].texture[2]=14;break;
        case 5: mesh.vertices[0].texture[3]|=512;break;
        case 6: mesh.vertices[0].texture[3]|=134217728U;mesh.vertices[0].group_a[1]=std::numeric_limits<float>::infinity();break;
        case 7: mesh.vertices[0].texture[3]=134217728U|1024U;break;
        case 8: mesh.vertices[0].texture[3]|=134217728U;mesh.vertices[0].group_b[0]=std::numeric_limits<float>::quiet_NaN();break;
        }
        require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid)
            && packet_scene.size()==2,"Invalid packed glyph replaced working scene");
        require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets)
            && packet_scene.reused_packets()==2 && packet_scene.uploaded_packets()==0,
            "Packed glyph rejection mutated retained uploads");
    }
    std::cout<<"Packed glyphs: nine malformed payloads rejected transactionally\n";
    for(unsigned bad=0;bad<6;++bad) {
        auto invalid=packets;auto& mesh=invalid[0].geometry;
        mesh.vertices[0].texture[0]=0;mesh.vertices[0].texture[3]=8;
        mesh.texels.assign(272+16384,0);mesh.texels[5]=4;
        if(bad<4) {
            constexpr uint32_t priorities[]{3,259,1024,UINT32_MAX};
            mesh.texels[7]=priorities[bad];
        } else mesh.texels[14]=bad==4?257U:UINT32_MAX;
        require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid)
            && packet_scene.size()==2,"Invalid background controls replaced live scene");
        require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets)
            && packet_scene.reused_packets()==2 && packet_scene.uploaded_packets()==0,
            "Background control rejection mutated retained uploads");
    }
    std::cout<<"Background controls: six invalid priority/wall values rejected transactionally\n";
    for(unsigned bad=0;bad<5;++bad) {
        auto invalid=packets;
        auto& mesh=invalid[0].geometry;
        mesh.vertices[0].texture[0]=0;mesh.vertices[0].texture[3]=8U|268435456U;
        mesh.texels.assign(272+16384+1,0);mesh.texels[5]=4;
        mesh.texels.back()=std::bit_cast<uint32_t>(-.25F);
        if(bad==0) mesh.texels.pop_back();
        if(bad==1) mesh.texels.back()=std::bit_cast<uint32_t>(std::numeric_limits<float>::quiet_NaN());
        if(bad==2) mesh.texels.back()=std::bit_cast<uint32_t>(0.F);
        if(bad==3) mesh.texels.back()=std::bit_cast<uint32_t>(-9.F);
        if(bad==4) mesh.vertices[0].texture[3]=32U|268435456U;
        require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid)
            && packet_scene.size()==2,"Invalid GPU ground payload replaced live scene");
        require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets)
            && packet_scene.reused_packets()==2 && packet_scene.uploaded_packets()==0,
            "GPU ground rejection mutated retained uploads");
    }
    std::cout<<"GPU ground: five malformed payloads rejected transactionally\n";
    invalid_packets=packets;invalid_packets[0].geometry.vertices[0].texture[3]=8;
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid_packets)
        && packet_scene.size()==2,"Truncated tile payload replaced the live GPU scene");
    invalid_packets[0].geometry.texels.resize(272+16384);
    invalid_packets[0].geometry.texels[5]=3;
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid_packets)
        && packet_scene.size()==2,"Invalid tile bit depth replaced the live GPU scene");
    invalid_packets[0].geometry.texels.assign(272+16384,0);
    invalid_packets[0].geometry.texels[5]=4;
    invalid_packets[0].geometry.texels[15]=64;
    // 256 pixels cannot contain two 256-pixel landscape halves. The old
    // 1024-pixel rejection is obsolete: EX's 16-pixel atlas is supported.
    invalid_packets[0].geometry.texels[2]=0;
    invalid_packets[0].geometry.texels[8]=0;
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid_packets)
        && packet_scene.size()==2,"Invalid unique-landscape atlas replaced live GPU scene");
    invalid_packets[0].geometry.texels[2]=invalid_packets[0].geometry.texels[8]=invalid_packets[0].geometry.texels[15]=0;
    invalid_packets[0].geometry.texels[10]=1;
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid_packets)
        && packet_scene.size()==2,"Truncated scanline payload replaced the live GPU scene");
    invalid_packets[0].geometry.texels[10]=0;
    invalid_packets[0].geometry.texels[12]=3;
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid_packets)
        && packet_scene.size()==2,"Invalid offset mode replaced the live GPU scene");
    invalid_packets=packets;invalid_packets[0].model[15]=0;
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid_packets)
        && packet_scene.size()==2,"Invalid model replaced the live GPU scene");
    for(unsigned invalid=0;invalid<4;++invalid) {
        invalid_packets=packets;
        auto& mesh=invalid_packets[0].geometry;mesh.texels.assign(12,0);
        for(auto& vertex:mesh.vertices) {vertex.texture[3]=68;vertex.position[0]=vertex.position[2]=0;}
        if(invalid==0) mesh.texels.pop_back();
        if(invalid==1) mesh.texels[7]=32768;
        if(invalid==2) mesh.vertices[0].position[0]=.5F;
        if(invalid==3) mesh.vertices[0].texture[3]|=8;
        require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid_packets)
            && packet_scene.size()==2,"Invalid grid payload replaced the live GPU scene");
    }
    for(unsigned invalid=0;invalid<8;++invalid) {
        invalid_packets=packets;
        auto& mesh=invalid_packets[0].geometry;mesh.texels.assign(268,0);
        for(auto& vertex:mesh.vertices) {vertex.texture[3]=132;vertex.texture[1]=vertex.texture[2]=0;vertex.position[0]=vertex.position[1]=vertex.position[2]=0;}
        if(invalid==0) mesh.texels.pop_back();
        if(invalid==1) mesh.texels[0]=0x7fc00000U; // NaN camera
        if(invalid==2) mesh.texels[12]=0x7f800000U; // Infinite colour
        if(invalid==3) mesh.texels[3]=32768;
        if(invalid==4) mesh.vertices[0].texture[1]=4;
        if(invalid==5) mesh.vertices[0].texture[2]=2;
        if(invalid==6) mesh.vertices[0].texture[3]|=64;
        if(invalid==7) mesh.vertices[0].position[0]=.5F;
        require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),invalid_packets)
            && packet_scene.size()==2,"Invalid dust payload replaced the live GPU scene");
    }
    const auto upload_started=std::chrono::steady_clock::now();
    for(unsigned i=0;i<8;++i) {
        packet_scene.close();
        require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets),packet_scene.status().c_str());
    }
    const auto upload_us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-upload_started).count()/8;
    const auto partial_started=std::chrono::steady_clock::now();
    auto partial=packets;
    for(unsigned i=0;i<8;++i) {
        partial[0].geometry.vertices[0].uv[0]=1.F+float(i&1U);
        require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),partial),packet_scene.status().c_str());
        require(packet_scene.reused_packets()==1 && packet_scene.uploaded_packets()==1,"Changing one object re-uploaded its unchanged neighbour");
    }
    const auto partial_us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-partial_started).count()/8;
    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets),packet_scene.status().c_str());
    auto failed_partial=packets;failed_partial[0].geometry.vertices[0].uv[0]=3;
    failed_partial[1].geometry.vertices[0].position[0]=std::numeric_limits<float>::quiet_NaN();
    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),failed_partial)
        && packet_scene.size()==2,"Partially uploaded failed scene replaced working resources");
    const auto update_started=std::chrono::steady_clock::now();
    for(unsigned i=0;i<10000;++i) {
        auto updated=models;updated[0][12]+=float(i&1U)*.2F;
        require(packet_scene.update_models(updated),"Benchmark transform-only update");
    }
    const auto update_us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-update_started).count()/10000;
    require(packet_scene.update_models(models),"Restore benchmark model transforms");
    std::cout<<"Two-packet submission microbenchmark: rebuild "<<upload_us<<" us, one-object refresh "<<partial_us<<" us, transform-only "<<update_us<<" us per update (not frame FPS)\n";
    VulkanEyeCommands commands;require(commands.initialize(device,queue,family,vkGetDeviceProcAddr),commands.status().c_str());
    {
        VulkanSourceStorage producer_output;
        const auto fill_producer=reinterpret_cast<PFN_vkCmdFillBuffer>(vkGetDeviceProcAddr(device,"vkCmdFillBuffer"));
        require(fill_producer!=nullptr,"Producer fill entry missing");
        require(producer_output.initialize(device,vkGetDeviceProcAddr,memory_properties,256,VK_BUFFER_USAGE_TRANSFER_DST_BIT),"Producer storage failed");
        require(!commands.submit_work({},{}),"Empty producer callback accepted");
        for(uint32_t frame=1;frame<=3;++frame) {
            require(commands.submit_work({64,1},[&](VkCommandBuffer command,VkExtent2D extent) {
                require(extent.width==64 && extent.height==1,"Producer extent changed");
                fill_producer(command,producer_output.buffer(),0,256,frame);
                VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
                vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
            }),"Producer submission failed");
            require(!commands.submit_work({},[](auto,auto){}),"Pending producer allocation reused");
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            for(;;) {
                const auto complete=commands.poll();
                if(complete==VulkanEyeCommands::Completion::complete) break;
                require(complete!=VulkanEyeCommands::Completion::error && std::chrono::steady_clock::now()<deadline,"Producer completion failed");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::array<uint32_t,64> actual{};
            require(producer_output.readback(0,std::as_writable_bytes(std::span(actual)))
                && std::all_of(actual.begin(),actual.end(),[&](auto value){return value==frame;}),"Producer GPU output mismatch");
        }
        std::cout<<"Standalone producer submissions: GPU output, busy rejection and fence reuse passed\n";
    }
    std::array<double,2> centroid{};
    std::array<uint64_t,2> billboard_coverage{};
    const unsigned visibility_start=argc>=4?14U:12U;
    const unsigned circle_start=visibility_start+38+unsigned(tile_cases.size())*2;
    const unsigned decal_start=circle_start+8;
    const unsigned shutter_start=decal_start+4;
    const unsigned span_draw_start=shutter_start+8;
    std::array<VulkanSpanPipeline,5> axis_producers;
    std::array<VulkanSpanPipeline,3> axis_warp_producers;
    VulkanSpanPipeline axis_reduction;
    VulkanSourceModel axis_graphics_model;
    VulkanSourceScene axis_graphics_scene;
    const unsigned photo_start=span_draw_start+40;
    VulkanDrawPackets photo_scene;
    std::array<unsigned,3> photo_zenith{};
    std::shared_ptr<const std::vector<uint32_t>> weather_texture;
    std::array<float,3> weather_expected{};
    for(unsigned sample=photographic_sky?photo_start+14:startup_menu?12U:0U;sample<(sbs_fixture?2U:startup_menu?14U:photo_start+(photographic_sky?20:68));++sample) {
        const unsigned eye=sample%2;
        const bool cartridge_sample=argc>=4 && sample>=12 && sample<14;
        const int visibility_case=sample>=visibility_start && sample<visibility_start+12?int((sample-visibility_start)/2):-1;
        const int line_case=sample>=visibility_start+12 && sample<visibility_start+16?int((sample-visibility_start-12)/2):-1;
        const int texture_case=sample>=visibility_start+16 && sample<visibility_start+26?int((sample-visibility_start-16)/2):-1;
        const int billboard_case=sample>=visibility_start+26 && sample<visibility_start+34?int((sample-visibility_start-26)/2):-1;
        const int axis_case=sample>=visibility_start+34 && sample<visibility_start+38?int((sample-visibility_start-34)/2):-1;
        const int tile_case=sample>=visibility_start+38 && sample<circle_start?int((sample-visibility_start-38)/2):-1;
        const int circle_case=sample>=circle_start && sample<decal_start?int((sample-circle_start)/2):-1;
        const int decal_case=sample>=decal_start && sample<shutter_start?int((sample-decal_start)/2):-1;
        const int shutter_case=sample>=shutter_start && sample<span_draw_start?int((sample-shutter_start)/2):-1;
        const int span_draw_case=sample>=span_draw_start && sample<photo_start?int((sample-span_draw_start)/2):-1;
        const int photo_case=sample>=photo_start?int((sample-photo_start)/2):-1;
        if(photo_case>=0 && eye==0) {
            starfox::render::BackdropImage image;
            image.width=2;image.height=1;image.pixels={0xff0000ffU,0xffff0000U};
            if(photo_case>=3 && photo_case<=5) {
                image.width=image.height=64;image.pixels.resize(4096);
                for(unsigned y=0;y<64;++y) for(unsigned x=0;x<64;++x)
                    image.pixels[y*64+x]=((x+y)&1)?0xffffffffU:0xff000000U;
            }
            if(photo_case==6) image.pixels[1]=0x00ff0000U;
            if(photo_case>=7 && photo_case<10) {
                if(photographic_sky) {
                    std::ifstream stream(photographic_file,std::ios::binary);
                    require(bool(stream),"Cannot read photographic landscape fixture");
                    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
                    image=starfox::render::BackdropImage::decode(bytes);
                    image.prepare_zenith();
                } else {
                    image.width=256;image.height=128;image.pixels.resize(256*128);
                    for(unsigned y=0;y<128;++y) for(unsigned x=0;x<256;++x)
                        image.pixels[y*256+x]=0xff804020U+(y<<8);
                }
            }
            if(photo_case>=10) image.pixels={0xffc3c3c3U,0xffc3c3c3U};
            if(photo_case>=12 && photo_case<15) {
                const uint32_t colour=photo_case==12?0xff0c2878U:photo_case==13?0xff78280cU:0xffc8c8c8U;
                image.pixels={colour,colour};
            }
            DrawPacket photo;
            photo.geometry.shared_texels=make_backdrop_texture(image,photo_case!=2);
            const float corners[6][2]{{-.4F,.4F},{.4F,.4F},{.4F,-.4F},{-.4F,.4F},{.4F,-.4F},{-.4F,-.4F}};
            for(const auto& corner:corners) {
                SceneVertex vertex{};vertex.position[0]=corner[0];vertex.position[1]=corner[1];vertex.position[2]=-2;
                vertex.texture[1]=image.width-1;vertex.texture[2]=image.height-1;
                vertex.texture[3]=backdrop_texture_flag|(photo_case==5?2U:0U);
                vertex.color[0]=vertex.color[1]=vertex.color[2]=photo_case==4?.5F:1.F;vertex.color[3]=1;
                if(photo_case==4) {vertex.odd_color[0]=-32.F/255.F;vertex.odd_color[2]=32.F/255.F;}
                vertex.uv[0]=photo_case==1 || photo_case==2?-.25F:.5F;vertex.uv[1]=.5F;
                if(photo_case>=3 && photo_case<=5) {vertex.uv[0]=corner[0]*64;vertex.uv[1]=corner[1]*64;}
                photo.geometry.vertices.push_back(vertex);
            }
            if(photo_case>=7 && photo_case<10) photo=photographic_landscape_packet(make_landscape_texture(image));
            if(photo_case>=10 && photo_case<12) {
                if(photo_case==10) weather_texture=photo.geometry.shared_texels;
                else photo.geometry.shared_texels=weather_texture;
                std::array<uint16_t,16> palette{};
                for(unsigned i=1;i<16;++i) palette[i]=uint16_t(photo_case==10?(i<9?0x7fff:0x4210)
                    :((i*2)&31)|(((31-i)&31)<<5)|((i&31)<<10));
                weather_expected=starfox::render::backdrop_ramp_colour({195,195,195},starfox::render::titania_cloud_ramp(palette));
                for(auto& v:photo.geometry.vertices) {
                    float* ramp[]{v.visibility_a,v.visibility_b,v.visibility_c,v.group_a,v.group_b,v.group_c};
                    for(unsigned i=0;i<16;++i) ramp[i/3][i%3]=float(i?palette[i]:1);
                }
            }
            if(photo_case>=12 && photo_case<15) {
                std::array<uint16_t,16> palette{};
                std::array<uint32_t,16> ramp{};ramp[0]=2;
                for(unsigned i=1;i<15;++i) {
                    palette[i]=uint16_t(((i*2)&31)|(((31-i)&31)<<5)|((i&31)<<10));
                    for(unsigned c=0;c<3;++c) {
                        const auto v=(palette[i]>>(c*5))&31;
                        ramp[i]|=((v<<3)|(v>>2))<<(c*8);
                    }
                }
                const auto pixel=image.pixels[0];
                weather_expected=starfox::render::backdrop_ramp_colour(
                    {float(pixel&255),float((pixel>>8)&255),float((pixel>>16)&255)},ramp);
                for(auto& v:photo.geometry.vertices) {
                    float* fields[]{v.visibility_a,v.visibility_b,v.visibility_c,v.group_a,v.group_b,v.group_c};
                    for(unsigned i=0;i<16;++i) fields[i/3][i%3]=float(i?palette[i]:2);
                }
            }
            if(photo_case>=19) {
                const bool limb=photo_case>=22;
                const unsigned light=(photo_case-19)%3==0?50:(photo_case-19)%3==1?120:230;
                const uint32_t pixel=0xff000000U|light|(light<<8)|(light<<16);
                image.pixels={pixel,pixel};photo.geometry.shared_texels=make_backdrop_texture(image);
                const bool city=photo_case>=25,blue=photo_case>=28;
                const bool orbital=photo_case>=31;
                std::array<uint16_t,16> palette{};palette[0]=orbital?8:city?(blue?7:6):limb?5:4;
                for(unsigned i=1;i<16;++i) palette[i]=uint16_t(((31-i*2)&31)|((i*2&31)<<5)|((i&31)<<10));
                const unsigned shades=orbital?7:city?(blue?1:2):limb?14:13;
                const float shade=1+shades*(1-float(light)/(limb?255:240));
                const unsigned a=unsigned(shade),b=std::min(a+1,shades+1);
                for(unsigned c=0;c<3;++c) {
                    const auto channel=[&](unsigned i) {const auto v=(palette[i]>>(c*5))&31;return float((v<<3)|(v>>2));};
                    weather_expected[c]=std::lerp(channel(a),channel(b),shade-a);
                }
                for(auto& v:photo.geometry.vertices) {
                    float* fields[]{v.visibility_a,v.visibility_b,v.visibility_c,v.group_a,v.group_b,v.group_c};
                    for(unsigned i=0;i<16;++i) fields[i/3][i%3]=palette[i];
                }
            }
            if(photo_case>=15 && photo_case<18) {
                std::array<uint32_t,16> ramp{};ramp[5]=1;
                const float u=photo_case==15?.1F:photo_case==16?.5F:.9F;
                for(auto& v:photo.geometry.vertices) {
                    v.uv[0]=u;v.visibility_a[0]=3;
                    v.visibility_a[1]=uint16_t(27|(20<<5)|(31<<10));
                    v.visibility_a[2]=uint16_t(1|(2<<5)|(8<<10));
                }
                // Oracle uses exact expanded BGR555, not approximate RGB constants.
                const auto expand=[](unsigned value) {return (value<<3)|(value>>2);};
                ramp[1]=expand(27)|(expand(20)<<8)|(expand(31)<<16);
                ramp[3]=expand(1)|(expand(2)<<8)|(expand(8)<<16);
                weather_expected=starfox::render::BackdropImage::moon_colour({195,195,195},{u*.5F,2.5F},ramp);
            }
            if(photo_case==0) {
                VulkanDrawPackets shared;std::array repeated{photo,photo,photo};
                require(shared.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),repeated),shared.status().c_str());
                require(shared.uploaded_texture_buffers()==1,"Shared photograph uploaded once per body");
                repeated[1].geometry.vertices[0].color[0]=.5F;
                require(shared.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),repeated),shared.status().c_str());
                require(shared.uploaded_texture_buffers()==0,"Palette change reuploaded a shared photograph");
            }
            if(photo_case==18) {
                auto under=photo;
                image.pixels={0xffff0000U,0xffff0000U};under.geometry.shared_texels=make_backdrop_texture(image);
                image.pixels={0x800000ffU,0x800000ffU};photo.geometry.shared_texels=make_backdrop_texture(image);
                const std::array layers{under,photo};weather_expected={128,0,127};
                require(photo_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),layers,{},false),photo_scene.status().c_str());
            } else require(photo_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),std::span(&photo,1)),photo_scene.status().c_str());
            if(photo_case==11) require(photo_scene.uploaded_texture_buffers()==0,"Weather reuploaded photographic pixels");
            auto invalid=photo;invalid.geometry.texels.push_back(0);
            require(!photo_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),std::span(&invalid,1)),"Mixed photo storage accepted");
            if(photo_case==11) {
                invalid=photo;invalid.geometry.vertices[0].visibility_b[0]=32768;
                require(!photo_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),std::span(&invalid,1)),"Invalid cloud palette accepted");
            }
            invalid=photo;
            invalid.geometry.vertices.assign(photo.geometry.vertex_view().begin(),photo.geometry.vertex_view().end());
            invalid.geometry.shared_vertices.reset();invalid.geometry.vertices[0].texture[1]=UINT32_MAX;
            require(!photo_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),std::span(&invalid,1)),"Overflowed photo extent accepted");
        }
        const bool depth_enabled=sample<2 || sample>=4;
        if(sample==0) {
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,keys),packet_scene.status().c_str());
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),reordered,reordered_keys),packet_scene.status().c_str());
            require(packet_scene.reused_packets()==2,"Left-eye keyed reorder missed reuse");
        }
        if(sample==1) {
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,keys),packet_scene.status().c_str());
            require(packet_scene.reused_packets()==2,"Right-eye keyed reorder missed reuse");
        }
        if(sample==2) require(pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),false,SceneTopology::triangles,VK_NULL_HANDLE,SceneBlend::opaque,graphics_cache_ptr),pipeline.status().c_str());
        if(sample==2 || sample==4) {
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),packets,keys,sample==4),packet_scene.status().c_str());
            require(packet_scene.reused_packets()==2,"Changing layer depth policy re-uploaded geometry");
        }
        if(sample==4) require(pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),true,SceneTopology::triangles,VK_NULL_HANDLE,SceneBlend::opaque,graphics_cache_ptr),pipeline.status().c_str());
        if(sample==12 && cartridge_sample && cartridge_textures.layout())
            require(pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),true,SceneTopology::triangles,cartridge_textures.layout()),pipeline.status().c_str());
        if(sample==visibility_start)
            require(pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),true,SceneTopology::triangles,VK_NULL_HANDLE,SceneBlend::opaque,graphics_cache_ptr),pipeline.status().c_str());
        if(sample==visibility_start+12)
            require(pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),true,SceneTopology::lines,VK_NULL_HANDLE,SceneBlend::opaque,graphics_cache_ptr),pipeline.status().c_str());
        if(sample==visibility_start+16)
            require(pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),true,SceneTopology::triangles,texture_fixture.layout(),SceneBlend::opaque,graphics_cache_ptr),pipeline.status().c_str());
        Matrix4 model{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        // Turn the model edge-on, compensating the centre translation. An
        // ordinary model-space quad would disappear; a billboard must not.
        if(billboard_case==1) {model[0]=model[10]=0;model[2]=-1;model[8]=1;model[12]=2;model[14]=-2;}
        if(billboard_case==2) model[14]=4;
        if(sample==4 || sample==5) model[12]=.2F;
        if(sample==4) {
            // A source vanishing-point change must move reused layers even
            // though their vertex/texture payload is identical. At two metres,
            // moving the source centre left 25.6 pixels moves its plane +0.2m.
            const auto previous_plane=source_layer_matrix(128.F,112.F).value();
            const auto current_plane=source_layer_matrix(102.4F,112.F).value();
            auto translated=models;
            for(auto& transform:translated) transform[12]+=current_plane[12]-previous_plane[12];
            require(packet_scene.update_models(translated),"Update existing packet model matrices");
        }
        if(sample==6 || sample==7) model[0]=model[5]=.5F;
        if(cartridge_sample) {
            model=cartridge_model;
        }
        const unsigned dither_scale=sample>=10?2:1;
        if(sample==8 || sample==10) {
            auto dithered=triangle;
            starfox::render::Palette256 palette{};palette[1]={0,255,0,255};palette[2]={0,0,255,255};
            const starfox::render::FaceMaterial material{{1,2,true},nullptr};
            for(unsigned i=0;i<3;++i) require(apply_scene_material(dithered[i],material,palette,0,dither_scale,false),"Apply dither material");
            require(vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,dithered),vertices.status().c_str());
        }
        if(sample==12 && argc>=4 && !live) require(vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,cartridge.vertices),vertices.status().c_str());
        if(sample==12 && cartridge_sample) {
            DrawPacket packet;packet.model=cartridge_model;packet.geometry=cartridge;
            const auto upload=live?std::span<const DrawPacket>(live_packets):std::span<const DrawPacket>(&packet,1);
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),upload,{},!(planet_screen || source_layers || source_oam)),packet_scene.status().c_str());
        }
        if(visibility_case>=0 && sample%2==0) {
            auto tested=triangle;
            const int winding=visibility_case%3;
            for(unsigned i=0;i<3;++i) {
                auto& vertex=tested[i];vertex.visibility_enabled=1;
                std::copy_n(triangle[0].position,3,vertex.visibility_a);
                std::copy_n(triangle[winding==2?0:winding==1?2:1].position,3,vertex.visibility_b);
                std::copy_n(triangle[winding==2?0:winding==1?1:2].position,3,vertex.visibility_c);
                if(visibility_case>=3) {
                    vertex.group_enabled=1;
                    std::copy_n(vertex.visibility_a,3,vertex.group_a);
                    std::copy_n(vertex.visibility_b,3,vertex.group_b);
                    std::copy_n(vertex.visibility_c,3,vertex.group_c);
                    // Individual face passes; its group may still hide it.
                    std::copy_n(triangle[1].position,3,vertex.visibility_b);
                    std::copy_n(triangle[2].position,3,vertex.visibility_c);
                }
            }
            require(vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,tested),vertices.status().c_str());
        }
        if(line_case>=0 && sample%2==0) {
            std::array<SceneVertex,2> line{triangle[0],triangle[1]};
            if(line_case==1) for(auto& vertex:line) {
                vertex.group_enabled=1;
                std::copy_n(triangle[0].position,3,vertex.group_a);
                std::copy_n(triangle[2].position,3,vertex.group_b);
                std::copy_n(triangle[1].position,3,vertex.group_c);
            }
            require(vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,line),vertices.status().c_str());
            DrawPacket packet;packet.model=model;packet.geometry.line_vertices.assign(line.begin(),line.end());
            packet.geometry.shared_line_vertices=std::make_shared<const std::vector<SceneVertex>>(std::move(packet.geometry.line_vertices));
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),std::span<const DrawPacket>(&packet,1)),packet_scene.status().c_str());
        }
        if(texture_case>=0 && sample%2==0) {
            auto textured=triangle;
            const float coordinates[]{0,1,-1,2,0};
            for(unsigned i=0;i<3;++i) {
                textured[i].texture[1]=1;textured[i].texture[3]=1;
                textured[i].uv[0]=coordinates[texture_case];
                if(texture_case==4) {textured[i].texture[0]=2;textured[i].texture[1]=0;textured[i].texture[3]=3;}
                if(texture_case>=2) {
                    textured[i].texture[0]=texture_case==4?40:8;
                    textured[i].texture[1]=32;textured[i].texture[2]=64;
                    textured[i].texture[3]=8388608U|(texture_case==4?2U:0U);
                }
            }
            require(vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,textured),vertices.status().c_str());
        }
        if(billboard_case>=0 && sample%2==0) {
            std::array<SceneVertex,6> sprite{};
            const float corners[6][2]{{-.4F,.4F},{.4F,.4F},{.4F,-.4F},{-.4F,.4F},{.4F,-.4F},{-.4F,-.4F}};
            for(unsigned i=0;i<6;++i) {
                sprite[i].position[2]=-2;sprite[i].texture[3]=5;
                std::copy_n(corners[i],2,sprite[i].billboard);
                if(billboard_case==3) sprite[i].uv[1]=corners[i][1]<0?2.F:0.F;
            }
            require(vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,sprite),vertices.status().c_str());
        }
        if(sprite_sizes && billboard_case>=0) {
            starfox::assets::Shape shape;shape.colour_words={0x4000};
            starfox::assets::TextureImage texture;texture.descriptor=0x4000;texture.texels={1};
            texture.u_mask=3;texture.v_mask=1;texture.texels={0,1,2,3,3,2,1,0};
            shape.textures={texture};
            starfox::render::RenderPose pose;pose.simple_scaled_sprite=true;
            pose.simple_sprite_world_size=billboard_case==1?1000:billboard_case==2?1:64;
            pose.z=std::array<double,4>{512,128,32768,512.001}[billboard_case];
            starfox::render::Palette256 palette{};palette[1]={0,255,0,255};
            palette[2]={255,0,0,255};palette[3]={0,0,255,255};
            if(billboard_case==1) pose.palette_override=1;
            DrawPacket packet;std::string error;
            require(build_draw_packet(shape,pose,palette,0,1,false,256,packet,error),error.c_str());
            if(shadow_gpu || shadow_reference) {
                const uint8_t coverage[]{0,255,128,64,32,192,255,0};
                packet.geometry.texels.assign(shadow_gpu?2:8,0);
                for(unsigned i=0;i<8;++i) {
                    if(shadow_gpu) packet.geometry.texels[i/4]|=uint32_t(coverage[i])<<((i&3)*8);
                    else packet.geometry.texels[i]=coverage[i]?(uint32_t(coverage[i])<<24)|0x0000ff00U:0;
                }
                for(auto& vertex:packet.geometry.vertices) {
                    vertex.texture[3]&=~536870912U;
                    if(shadow_gpu) vertex.texture[3]|=1073741824U;
                    vertex.color[0]=vertex.color[2]=0;vertex.color[1]=vertex.color[3]=1;
                    // Three active pixels with a nonzero padding byte in row 0.
                    // Reference keeps the four-wide RGBA allocation but uses
                    // only the first three columns; packed mode declares width 3.
                    if(billboard_case==0) {
                        vertex.uv[0]*=.75F;
                        if(shadow_gpu) vertex.texture[1]=2;
                    }
                }
            }
            if(sprite_reference) {
                // Independent previous CPU texture expansion, alongside sizing.
                std::vector<uint32_t> expanded;
                for(const auto index:texture.texels) {
                    if(!index) {expanded.push_back(0);continue;}
                    const auto c=palette[pose.palette_override.value_or(index)];
                    expanded.push_back(uint32_t(c.r)|(uint32_t(c.g)<<8)|(uint32_t(c.b)<<16)|0xff000000U);
                }
                packet.geometry.texels=std::move(expanded);
                for(auto& vertex:packet.geometry.vertices) vertex.texture[3]&=~536870912U;
                // Independent pre-migration double-precision CPU sizing.
                const auto dimension=std::clamp(std::trunc(pose.simple_sprite_world_size*256./pose.z),0.,240.);
                const float half=float(dimension*pose.z/512.);
                for(auto& vertex:packet.geometry.vertices) {
                    vertex.texture[3]&=~134217728U;
                    vertex.billboard[0]*=half;vertex.billboard[1]*=half;
                }
            }
            if(gameplane_sprite) for(auto& vertex:packet.geometry.vertices) {
                if(sprite_gpu) vertex.group_b[2]=1;
                else {
                    vertex.position[0]+=vertex.billboard[0];
                    vertex.position[1]+=vertex.billboard[1];
                    vertex.billboard[0]=vertex.billboard[1]=0;
                    vertex.texture[3]&=~4U;
                }
            }
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),std::span<const DrawPacket>(&packet,1)),packet_scene.status().c_str());
            if(sprite_gpu && billboard_case==0) {
                for(unsigned invalid=0;invalid<4;++invalid) {
                    auto bad=packet;
                    if(invalid==0) bad.geometry.texels.resize(255);
                    if(invalid==1) bad.geometry.texels.pop_back();
                    if(invalid==2) for(auto& vertex:bad.geometry.vertices) vertex.texture[0]=0xffffffffU;
                    if(invalid==3) for(auto& vertex:bad.geometry.vertices) vertex.texture[1]=256;
                    require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,
                        targets.render_pass(),std::span<const DrawPacket>(&bad,1)),"Invalid indexed sprite accepted");
                    require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,
                        targets.render_pass(),std::span<const DrawPacket>(&packet,1)),"Valid indexed sprite lost after rejection");
                    require(packet_scene.uploaded_packets()==0,"Rejected indexed sprite replaced retained resources");
                }
            }
        }
        if(tunnel_fixture && billboard_case>=0) {
            auto surround=tunnel_surround_packet({.2F,.3F,.4F,1});
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),
                std::span<const DrawPacket>(&surround,1),{},false),packet_scene.status().c_str());
        }
        XrView view{XR_TYPE_VIEW};view.pose.orientation.w=1;view.pose.position.x=eye?.032F:-.032F;
        if(photo_case>=8 && photo_case<10) {
            const float half_yaw=photo_case==8?std::numbers::pi_v<float>*.25F:std::numbers::pi_v<float>*.5F;
            view.pose.orientation.y=std::sin(half_yaw);view.pose.orientation.w=std::cos(half_yaw);
        }
        if(gameplane_sprite && billboard_case>=0) {
            const float angle=std::array<float,4>{.17F,.39F,-.23F,-.51F}[billboard_case];
            view.pose.orientation={0,0,std::sin(angle),std::cos(angle)};
        }
        if(tunnel_fixture && billboard_case>=0) {
            const float half=std::sqrt(.5F);
            if(billboard_case==1) view.pose.orientation={0,half,0,half};
            if(billboard_case==2) view.pose.orientation={0,-half,0,half};
            if(billboard_case==3) view.pose.orientation={0,1,0,0};
        }
        if(cartridge_sample && (landscape_view || ((explicit_stage || intro_background) && grid_mode.find('@')!=std::string_view::npos))) {
            const auto direction=std::string_view(argv[4]);
            const float half=std::sqrt(.5F);
            if(direction=="--landscape-rear" || direction.ends_with("@rear")) view.pose.orientation={0,1,0,0};
            else if(direction=="--landscape-left" || direction.ends_with("@left")) view.pose.orientation={0,half,0,half};
            else if(direction=="--landscape-right" || direction.ends_with("@right")) view.pose.orientation={0,-half,0,half};
            else if(direction=="--landscape-up" || direction.ends_with("@up")) view.pose.orientation={half,0,0,half};
            else if(direction=="--landscape-down" || direction.ends_with("@down")) view.pose.orientation={-half,0,0,half};
            else if(direction.ends_with("@island-focus")) {
                const float yaw=(296.F-128.F)/256.F;
                view.pose.orientation={0,-std::sin(yaw*.5F),0,std::cos(yaw*.5F)};
            }
            else if(direction.ends_with("@planet-focus") || direction.ends_with("@moon-focus")) {
                require(!live_backgrounds.empty(),"Planet-focus capture needs a background");
                const auto moon=std::find_if(live_backgrounds.rbegin(),live_backgrounds.rend(),[](const auto& packet) {
                    const auto v=packet.geometry.vertex_view();return v.size()==6 && (v.front().texture[3]&backdrop_texture_flag)==backdrop_texture_flag;
                });
                const bool focus_moon=direction.ends_with("@moon-focus");
                require(!focus_moon || moon!=live_backgrounds.rend(),"Moon-focus capture needs a photographic body");
                const auto& body=focus_moon?*moon:live_backgrounds.back();
                const auto vertices=body.geometry.vertex_view();
                require(vertices.size()==6,"Planet-focus capture needs one six-vertex planet patch");
                float center[3]{};
                for(unsigned row=0;row<3;++row) {
                    center[row]=body.model[12+row];
                    for(unsigned col=0;col<3;++col) center[row]+=body.model[col*4+row]
                        *(vertices[0].position[col]+vertices[2].position[col])*.5F;
                }
                const auto [x,y,z]=std::array{center[0],center[1],center[2]};
                const float yaw=std::atan2(x,-z)*.5F,pitch=std::atan2(y,std::hypot(x,z))*.5F;
                view.pose.orientation={std::sin(pitch)*std::cos(yaw),-std::cos(pitch)*std::sin(yaw),
                    std::sin(pitch)*std::sin(yaw),std::cos(pitch)*std::cos(yaw)};
            }
            else require(direction=="--landscape-front" || direction.ends_with("@front") || direction.ends_with("@from-entry") || training_scene
                || direction.ends_with("@height-equivalent"),"Unknown landscape direction");
        }
        view.fov={-.7F,.7F,.7F,-.7F};
        if(cartridge_sample && explicit_stage && (grid_mode.ends_with("@island-focus") || grid_mode.ends_with("@moon-focus")))
            view.fov={-.12F,.12F,.12F,-.12F};
        auto camera=eye_camera(view,1,.05F);require(camera.has_value(),"Invalid eye camera");
        if(cartridge_sample && explicit_stage && grid_mode.find("@effect-sepia")!=std::string_view::npos)
            camera->effects={8,100,0,0};
        if(cartridge_sample && explicit_stage) {
            for(const auto style:{1U,4U,8U,9U,10U,11U,13U,14U,15U,16U}) {
                const auto token="@style-"+std::to_string(style)+"@";
                if(grid_mode.find(token)!=std::string_view::npos) camera->effects={style,100,0,0};
            }
            if(grid_mode.find("@intensity-0@")!=std::string_view::npos) camera->effects[1]=0;
            if(grid_mode.find("@intensity-50@")!=std::string_view::npos) camera->effects[1]=50;
        }
        if(sbs_fixture) {
            const auto pair=sbs_eye_cameras(1.4F,1,.064F,4,.05F);
            require(pair.has_value(),"Invalid SBS cameras");
            camera=(*pair)[eye];
        }
        if(axis_case>=0 && sample%2==0) {
            starfox::assets::Shape axis_shape;
            axis_shape.vertices={{-120,-20,-1},{-80,20,-1},{80,-20,1},{120,20,1}};
            axis_shape.faces={{-1,0,{0,0,1},{0,1,2,3}}};axis_shape.colour_words={0x11};
            starfox::render::RenderPose axis_pose;axis_pose.z=axis_case? -512:512;axis_pose.collapse_to_axis_line=true;
            if(axis_near) {
                axis_shape.vertices={{-20,-4,-128},{-12,4,-128},{12,-4,128},{20,4,128}};
                axis_pose.z=axis_case?-512:32;axis_pose.continuous_geometry=true;
            }
            if(axis_rotated) {
                axis_pose.continuous_geometry=true;axis_pose.subpixel_projection=true;
                axis_pose.pitch=17.25;axis_pose.yaw=34.5;axis_pose.roll=26.75;
            }
            if(axis_matrix) {
                axis_pose.continuous_geometry=true;axis_pose.use_rotation_matrix=true;
                axis_pose.rotation_matrix={23170,23170,0,-23170,23170,0,0,0,32767};
            }
            starfox::render::Palette256 axis_palette{};axis_palette[1]={0,255,0,255};
            DrawPacket axis_packet;std::string axis_error;
            require(build_draw_packet(axis_shape,axis_pose,axis_palette,0,1,false,256,axis_packet,axis_error),axis_error.c_str());
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),std::span<const DrawPacket>(&axis_packet,1)),packet_scene.status().c_str());
            if(axis_gpu) {
                axis_graphics_model.close();
                std::array<const VulkanSpanPipeline*,5> producers{};
                for(unsigned i=axis_warp?0:2;i<5;++i) {
                    if(!axis_producers[i].descriptor_layout(0))
                        require(axis_producers[i].initialize(device,vkGetDeviceProcAddr,static_cast<SourceComputeStage>(i)),axis_producers[i].status().c_str());
                    producers[i]=&axis_producers[i];
                }
                if(!axis_reduction.descriptor_layout(0))
                    require(axis_reduction.initialize(device,vkGetDeviceProcAddr,SourceComputeStage::axis),axis_reduction.status().c_str());
                SourceSpanModel source;SourceAxisInputs input;
                axis_shape.faces.push_back({-1,1,{0,0,1},{0,1,2,3}});
                axis_shape.colour_words.push_back(0x22);axis_shape.bsp_root_address=0xdeadbeef;
                require(prepare_source_axis_model(axis_shape,axis_pose,{},256,192,source,input,axis_error),axis_error.c_str());
                source.graphics_palette_flags=1;source.graphics_palette[1]=0xff00ff00U;
                source.graphics_palette[2]=0xff0000ffU;
                std::shared_ptr<SourceWarpInputs> warp;
                std::array<const VulkanSpanPipeline*,3> warp_pipelines{};
                if(axis_warp) {
                    axis_pose.colour_warp=true;axis_pose.projected_points_address=16864;
                    warp=std::make_shared<SourceWarpInputs>();
                    require(prepare_source_warp_inputs(axis_shape,axis_pose,{},source.projection,source.bsp,*warp,axis_error),axis_error.c_str());
                    source.warp_expanded=true;source.graphics_unclipped=false;source.graphics_palette_flags|=4U;
                    source.faces=warp->templates;source.spans.ordered_mode=2;source.spans.polygon_count=source.spans.count;
                    source.clip_settings[0]=source.spans.count;source.clip_settings[2]=source.spans.count*32U;
                    for(unsigned i=0;i<3;++i) {
                        if(!axis_warp_producers[i].descriptor_layout(0))
                            require(axis_warp_producers[i].initialize(device,vkGetDeviceProcAddr,static_cast<SourceComputeStage>(i+5)),axis_warp_producers[i].status().c_str());
                        warp_pipelines[i]=&axis_warp_producers[i];
                    }
                }
                require(axis_graphics_model.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                    source,producers,warp.get(),warp?&warp_pipelines:nullptr,&input,&axis_reduction),axis_graphics_model.status().c_str());
                SceneVertex material{};material.color[0]=material.color[3]=1; // Must be replaced by the source green palette.
                require(axis_graphics_model.prepare_graphics(targets.render_pass(),256,material),axis_graphics_model.status().c_str());
                SourceModelPackets frame;frame.packets.emplace_back();frame.handles.push_back(123);
                frame.compute_models.push_back({123,0,256,source,warp,std::make_shared<SourceAxisInputs>(input)});
                require(axis_graphics_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                    targets.render_pass(),frame),axis_graphics_scene.status().c_str());
                VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                check(vkBeginCommandBuffer(copy,&begin),"Begin axis graphics producer");
                require(axis_graphics_model.record(copy),axis_graphics_model.status().c_str());
                require(axis_graphics_scene.record_compute(copy),axis_graphics_scene.status().c_str());
                check(vkEndCommandBuffer(copy),"End axis graphics producer");
                VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
                check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit axis graphics producer");check(vkQueueWaitIdle(queue),"Wait axis graphics producer");
                axis_graphics_model.compute_completed();
                require(axis_graphics_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,properties.limits,
                    targets.render_pass(),frame) && axis_graphics_scene.reused_resources(),"Axis scene did not retain resources");
            }
        }
        VkClearColorValue clear{};
        if(tunnel_fixture && billboard_case>=0) {clear.float32[1]=1;clear.float32[3]=1;}
        if(span_draw_case>=0) {
            const bool resident=span_draw_case==2 || span_draw_case==3 || span_draw_case>=10;
            std::vector<uint32_t> payload(4+32*24+32);
            payload[0]=4;payload[1]=4+32*24;payload[2]=32;
            if(span_draw_case>=4) payload[3]=0x80000000U|(3U<<16)|65530U;
            for(unsigned y=4;y<12;++y) {
                const auto at=4+y*24;payload[at]=4;payload[at+1]=y;payload[at+2]=12;payload[at+3]=y+1;
                if(span_draw_case>=4) payload[at+23]=2;
                if(span_draw_case==5) {
                    payload[at+23]|=4;payload[at+9]=4;payload[at+10]=32;
                    payload[payload[1]+y]=0x5550;
                }
            }
            if(resident) {
                const auto active_storage=span_draw_case==19?warp_draw_storage->buffer():span_draw_case==18?textured_occurrence_model->buffer():span_draw_case==17?occurrence_cover_model->buffer():span_draw_case==16?tilted_span_storage:span_draw_case==15?wave_span_storage:produced_span_storage;
                const auto active_bytes=span_draw_case==19?warp_draw_storage->size():span_draw_case==18?textured_occurrence_model->layout().bytes:span_draw_case==17?occurrence_cover_model->layout().bytes:span_draw_case==16?tilted_span_bytes:span_draw_case==15?wave_span_bytes:produced_span_bytes;
                require(!texture_fixture.initialize_external(device,vkGetDeviceProcAddr,produced_span_storage,3),"Misaligned external storage accepted");
                require(texture_fixture.initialize_external(device,vkGetDeviceProcAddr,active_storage,active_bytes),texture_fixture.status().c_str());
                texture_fixture.close(); // must not free producer storage
                require(texture_fixture.initialize_external(device,vkGetDeviceProcAddr,active_storage,active_bytes),texture_fixture.status().c_str());
            } else require(texture_fixture.initialize(device,vkGetDeviceProcAddr,memory_properties,payload),texture_fixture.status().c_str());
            std::vector<SceneVertex> geometry;
            const auto quad=[&](float low,float high,float z,bool span) {
                for(unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                    const float corners[4][2]{{0,0},{1,0},{1,1},{0,1}};
                    const float depth=span && span_draw_case>=8?(corners[corner][0]==0?.375F:.625F):z;
                    SceneVertex vertex{};vertex.position[0]=low+(high-low)*corners[corner][0];
                    vertex.position[1]=low+(high-low)*corners[corner][1];vertex.position[2]=depth;
                    if(span_draw_case>=6) {vertex.position[0]*=depth;vertex.position[1]*=depth;}
                    vertex.color[span?1:0]=1;vertex.color[3]=1;
                    if(span) {vertex.texture[0]=resident?produced_span_lookup:0;vertex.texture[1]=resident?0:32;vertex.texture[3]=resident?131072:65536;vertex.uv[0]=corners[corner][0]*32;vertex.uv[1]=corners[corner][1]*32;}
                    if(span && span_draw_case>=10) vertex.texture[1]=span_draw_case==10?1U:UINT32_MAX;
                    if(span && span_draw_case>=12) {
                        vertex.texture[1]=0;vertex.texture[2]=span_draw_case==13?32U:corner;
                        vertex.texture[3]=131072U|262144U;vertex.billboard[1]=256;
                        if(span_draw_case>=14) vertex.texture[3]=131072U|524288U;
                        if(span_draw_case==15) vertex.texture[0]=wave_span_lookup;
                        if(span_draw_case==16) vertex.texture[0]=tilted_span_lookup;
                    }
                    if(span && span_draw_case>=8) {vertex.uv[0]*=depth;vertex.uv[1]*=depth;vertex.billboard[0]=depth;}
                    geometry.push_back(vertex);
                }
            };
            if(span_draw_case<12 && span_draw_case%2==0) quad(-.625F,-.5F,.25F,false);
            quad(-1,1,.5F,true);
            if(span_draw_case<12 && span_draw_case%2==1) quad(-.625F,-.5F,.25F,false);
            if(span_draw_case==19) {
                SceneVertex material{};material.color[1]=material.color[3]=1;
                require(warp_draw_model->prepare_graphics(targets.render_pass(),256,material),warp_draw_model->status().c_str());
                geometry.clear();
                for(uint32_t slot=0;slot<2;++slot) for(uint32_t corner:{0U,1U,2U,0U,2U,3U}) {
                    SceneVertex vertex{};vertex.color[1]=vertex.color[3]=1;
                    vertex.texture[0]=warp_draw_lookup;vertex.texture[1]=slot;vertex.texture[2]=corner;
                    vertex.texture[3]=131072U|524288U;vertex.billboard[1]=256;
                    geometry.push_back(vertex);
                }
            } else if(span_draw_case>=14) {
                const auto& model=span_draw_case==18?textured_occurrence_model:span_draw_case==17?occurrence_cover_model:span_draw_case==16?tilted_cover_model:span_draw_case==15?wave_cover_model:solid_cover_model;
                SceneVertex material{};material.color[1]=material.color[3]=1;
                require(model && model->cover_geometry(256,material,geometry),"Build runtime source cover templates");
                require(geometry.size()==(span_draw_case>=17?12U:6U) && geometry[0].texture[1]==0 && geometry[5].texture[2]==3,"Source cover slot/corner mismatch");
                const auto retained=geometry;
                require(!model->cover_geometry(0,material,geometry) && geometry==retained,"Invalid source cover replaced geometry");
                require(model->prepare_graphics(targets.render_pass(),256,material),model->status().c_str());
            }
            require(vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,geometry),vertices.status().c_str());
            require(pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),true,SceneTopology::triangles,texture_fixture.layout(),SceneBlend::opaque,graphics_cache_ptr),pipeline.status().c_str());
            if(span_draw_case<=1) {
                DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
                packet.geometry.vertices=geometry;packet.geometry.texels=payload;
                require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),std::span<const DrawPacket>(&packet,1)),packet_scene.status().c_str());
                packet.geometry.texels[0]=UINT32_MAX;
                require(!packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),std::span<const DrawPacket>(&packet,1)),"Corrupt span offset accepted");
            }
        }
        bool shutter_drawable=false;
        if(shutter_case>=0) {
            clear.float32[0]=clear.float32[1]=clear.float32[2]=clear.float32[3]=1;
            starfox::simulation::WindowWipeState closed,off;
            closed.active=true;closed.logic=0xaa;closed.left.fill(16);closed.right.fill(239);
            auto partial=closed;
            for(unsigned y=80;y<112;++y) {partial.left[y]=15;partial.right[y]=16;}
            auto shutter=shutter_case==0?source_shutter_packet(off,closed,0):
                shutter_case==1?source_shutter_packet(partial,partial,1):
                source_shutter_packet(partial,off,shutter_case==2?.5:1.);
            shutter_drawable=!shutter.geometry.vertices.empty();
            if(shutter_drawable) require(vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,
                shutter.geometry.vertices),vertices.status().c_str());
            require(pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),false,SceneTopology::triangles,VK_NULL_HANDLE,SceneBlend::opaque,graphics_cache_ptr),pipeline.status().c_str());
        }
        if(decal_case>=0 && eye==0) {
            DrawPacket packet;packet.model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            packet.geometry.texels={0xff00ff00U};
            for(unsigned i=0;i<3;++i) {
                auto v=triangle[i];v.color[0]=1;v.color[1]=v.color[2]=0;
                v.position[2]=-2;packet.geometry.vertices.push_back(v);
            }
            for(unsigned i:{1U,2U,0U}) {
                auto v=triangle[i];v.position[2]=-2;v.texture[3]=32769;
                packet.geometry.vertices.push_back(v);
            }
            if(decal_case==1) for(unsigned i=0;i<3;++i) {
                auto v=triangle[i];v.position[2]=-1.99F;v.color[0]=1;v.color[1]=v.color[2]=0;
                packet.geometry.vertices.push_back(v);
            }
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),
                std::span<const DrawPacket>(&packet,1)),packet_scene.status().c_str());
        }
        if(circle_case>=0) {
            clear.float32[0]=clear.float32[1]=clear.float32[2]=.5F;clear.float32[3]=1;
            starfox::simulation::CircleEffectState state{true,128,112,80,8,0,0,
                uint8_t(0x3f|((circle_case&1)?0x80:0)|((circle_case&2)?0x40:0))};
            auto disk=source_circle_packet(state,state,1);
            require(vertices.initialize(device,vkGetDeviceProcAddr,memory_properties,disk.geometry.vertices),vertices.status().c_str());
            const SceneBlend modes[]{SceneBlend::add,SceneBlend::subtract,SceneBlend::half_add,SceneBlend::half_subtract};
            require(pipeline.initialize(device,vkGetDeviceProcAddr,targets.render_pass(),false,
                SceneTopology::triangles,VK_NULL_HANDLE,modes[circle_case],graphics_cache_ptr),pipeline.status().c_str());
        }
        if(cartridge_sample && full_layers) std::copy(live_backdrop.begin(),live_backdrop.end(),clear.float32);
        if(tile_case>=0 && eye==0)
            require(packet_scene.initialize(device,vkGetDeviceProcAddr,memory_properties,targets.render_pass(),std::span<const DrawPacket>(&tile_cases[tile_case],1),{},!(source_layers || source_oam || tile_suite)),packet_scene.status().c_str());
        require(commands.submit(targets,eye,0,clear,[&](VkCommandBuffer cmd,VkExtent2D extent) {
            if(span_draw_case>=0) {
                const Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
                EyeCamera span_camera{identity,identity};
                if(span_draw_case>=6) {
                    span_camera.view[12]=eye==0?1.F/64:-1.F/64;
                    span_camera.projection[11]=1;span_camera.projection[14]=-.1F;span_camera.projection[15]=0;
                }
                if(span_draw_case>=12) {
                    span_camera={identity,identity};
                    span_camera.view[0]=16;span_camera.view[5]=-16;span_camera.view[10]=-.5F;
                    if(span_draw_case==16) {
                        span_camera.view[10]=-1;span_camera.view[12]=eye==0?1.F/64:-1.F/64;
                        span_camera.projection[11]=1;span_camera.projection[14]=-.1F;span_camera.projection[15]=0;
                    }
                }
                if(span_draw_case<=1) require(packet_scene.record(cmd,extent,span_camera),"Record retained source-span packet after rejection");
                else if(span_draw_case==19)
                    require(warp_scene->record(cmd,extent,span_camera),"Draw scene-owned resident warp outputs");
                else if(span_draw_case>=17) {
                    const auto& model=span_draw_case==18?textured_occurrence_model:occurrence_cover_model;
                    require(model->record_graphics(cmd,extent,span_camera),"Record occurrence graphics");
                } else if(span_draw_case>=14) {
                    const auto& source=combined_source_scenes[span_draw_case-14];
                    if(span_draw_case==14 && eye==1)
                        require(shadow_filtered_scene->record(cmd,extent,span_camera,true),"Record replacement-ready shadow suppression");
                    else require(source && source->record(cmd,extent,span_camera),"Record combined ordered source scene");
                } else if(span_draw_case==4) {
                    require(!pipeline.record_range(cmd,extent,vertices.buffer(),vertices.count(),UINT32_MAX,3,span_camera,texture_fixture.descriptor()),"Overflowed vertex range accepted");
                    require(!pipeline.record_range(cmd,extent,vertices.buffer(),vertices.count(),1,3,span_camera,texture_fixture.descriptor()),"Unaligned triangle range accepted");
                    require(!pipeline.record_range(cmd,extent,vertices.buffer(),vertices.count(),0,UINT32_MAX,span_camera,texture_fixture.descriptor()),"Oversized vertex count accepted");
                    for(uint32_t first=0;first<vertices.count();first+=3)
                        require(pipeline.record_range(cmd,extent,vertices.buffer(),vertices.count(),first,3,span_camera,texture_fixture.descriptor()),"Record ordered triangle range");
                } else require(pipeline.record(cmd,extent,vertices.buffer(),vertices.count(),span_camera,texture_fixture.descriptor()),"Record depth-tested source spans");
                return;
            }
            if(shutter_case>=0) {
                const Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
                if(shutter_drawable) require(pipeline.record(cmd,extent,vertices.buffer(),vertices.count(),
                    EyeCamera{identity,identity}),"Record head-locked shutter");
                return;
            }
            if(circle_case>=0) {
                require(pipeline.record_model(cmd,extent,vertices.buffer(),vertices.count(),*camera,
                    source_layer_matrix(128,112,2).value()),"Record analytic circle blend");
                return;
            }
            if(cartridge_sample && full_layers && !omit_background)
                require(background_scene.record(cmd,extent,*camera),"Record combined background layers");
            if(cartridge_sample && !live_tunnel_surround.empty())
                require(tunnel_surround_scene.record(cmd,extent,*camera),"Record tunnel background surround");
            if(axis_case>=0 && axis_gpu) {
                require(axis_graphics_scene.record(cmd,extent,*camera),"Draw scene-owned GPU axis endpoints");
                return;
            }
            if((sprite_sizes || tunnel_fixture) && billboard_case>=0) {
                require(packet_scene.record(cmd,extent,*camera),"Draw whole-object sprite sizing fixture");
                return;
            }
            if(cartridge_sample && explicit_stage
                && std::string_view(argv[4]).find("@background-only")!=std::string_view::npos)
                return; // Diagnostic isolation, never a gameplay rendering option.
            if(photo_case>=0) {
                require(photo_scene.record(cmd,extent,*camera),"Record filtered photographic artwork");return;
            }
            if(sample<6 || cartridge_sample || line_case>=0 || axis_case>=0 || tile_case>=0 || decal_case>=0) {
                require(!packet_scene.record_range(cmd,extent,*camera,SIZE_MAX,1),"Overflowed draw start accepted");
                require(!packet_scene.record_range(cmd,extent,*camera,0,SIZE_MAX),"Overflowed draw count accepted");
                require(packet_scene.record_range(cmd,extent,*camera,packet_scene.size(),0),"Empty tail draw range rejected");
                const bool omit_models=cartridge_sample && explicit_stage
                    && grid_mode.find("@no-models")!=std::string_view::npos;
                if(omit_models) {} // Diagnostic ablation; retain backgrounds, enclosure and sprites.
                else if(cartridge_sample && (live_compute || resident_model)) require(live_compute_scene.record(cmd,extent,*camera),"Record live compute scene");
                else if(eye==0) require(packet_scene.record(cmd,extent,*camera),"Record native draw packets");
                else for(size_t item=0;item<packet_scene.size();++item)
                    require(packet_scene.record_range(cmd,extent,*camera,item,1),"Record ordered native draw range");
                if(cartridge_sample && combined_live
                    && !(explicit_stage && grid_mode.find("@no-sprites")!=std::string_view::npos))
                    require(sprite_scene.record(cmd,extent,*camera),"Record combined sprite layer");
                if(cartridge_sample && live_shutter_vertices.count()) {
                    const Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
                    require(live_shutter_pipeline.record(cmd,extent,live_shutter_vertices.buffer(),live_shutter_vertices.count(),
                        EyeCamera{identity,identity}),"Record live source scramble shutter");
                }
                return;
            }
            require(pipeline.record_model(cmd,extent,vertices.buffer(),vertices.count(),*camera,model,
                texture_case>=0 || billboard_case>=0?texture_fixture.descriptor():cartridge_sample?cartridge_textures.descriptor():VK_NULL_HANDLE),"Record model triangle");
            if(cartridge_sample && cartridge_lines.count())
                require(line_pipeline.record_model(cmd,extent,cartridge_lines.buffer(),cartridge_lines.count(),*camera,model,cartridge_textures.descriptor()),"Record model lines");
        },[&](VkCommandBuffer command,VkExtent2D) {
            if(packet_scene.size()) require(packet_scene.record_compute(command),"Packet pre-render compute failed");
            if(cartridge_sample && (live_compute || resident_model) && eye==0)
                require(live_compute_scene.record_compute(command),"Live scene pre-render compute failed");
            if(span_draw_case>=14 && span_draw_case<17 && eye==0)
                require(combined_source_scenes[span_draw_case-14]->record_compute(command),"Fenced pre-render compute failed");
            if(span_draw_case==14 && eye==1)
                require(shadow_filtered_scene->record_compute(command),"Filtered scene pre-render compute failed");
        }),commands.status().c_str());
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        for(;;) {
            const auto complete=commands.poll();
            if(complete==VulkanEyeCommands::Completion::complete) break;
            require(complete!=VulkanEyeCommands::Completion::error,"GPU submission failed");
            require(std::chrono::steady_clock::now()<deadline,"GPU submission timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if(connected_expected && cartridge_sample) {
            std::vector<uint32_t> actual(connected_grid_output_words);
            require(packet_scene.readback_connected_grid(0,actual),"Read completed connected-grid compute rows");
            const auto& expected=connected_expected->geometry.texels;
            for(size_t row=0;row<192;++row) {
                const auto count=actual.at(row*2+1);
                require(count==expected.at(row*2+1),"GPU connected-grid row count differs from CPU");
                for(size_t entry=0;entry<count;++entry) {
                    auto a=actual.at(actual.at(row*2)+entry),b=expected.at(expected.at(row*2)+entry);
                    for(unsigned word=0;word<5;++word)
                        require(actual.at(a+word)==expected.at(b+word),"GPU connected-grid primitive differs from CPU");
                }
            }
            std::cout<<"Connected-grid GPU rows match independent CPU projection/binning, eye "<<eye<<'\n';
        }
        check(vkResetCommandPool(device,pool,0),"Reset copy pool");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};check(vkBeginCommandBuffer(copy,&begin),"Begin copy");
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        barrier.image=image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={width,height,1};
        vkCmdCopyImageToBuffer(copy,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&region);
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(copy,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
        check(vkEndCommandBuffer(copy),"End copy");VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&copy;
        check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit copy");check(vkQueueWaitIdle(queue),"Wait for copy");
        void* mapped{};check(vkMapMemory(device,*read_memory,0,VK_WHOLE_SIZE,0,&mapped),"Map readback");
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=*read_memory;range.size=VK_WHOLE_SIZE;
        const auto invalidate=vkInvalidateMappedMemoryRanges(device,1,&range);
        if(invalidate!=VK_SUCCESS) {vkUnmapMemory(device,*read_memory);check(invalidate,"Invalidate readback");}
        const auto* pixels=static_cast<const unsigned char*>(mapped);
        if(span_draw_case>=0) {
            bool correct=true;
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                if(span_draw_case>=17) {
                    const auto* pixel=pixels+(y*width+x)*4;
                    // Solid span mode includes the source right-edge column:
                    // command intervals are [4,13) and [18,27), at 8x here.
                    bool green=x>=32 && x<104 && y>=32 && y<96;
                    bool blue=x>=144 && x<(span_draw_case==18?208U:216U) && y>=32 && y<96;
                    if(span_draw_case==18 && blue) {
                        const auto u=((x-144)/32+1)&1U,v=((y-32)/32)&1U;
                        green=u!=v;blue=!green;
                    }
                    correct&=pixel[0]==0 && pixel[1]==(green?255:0) && pixel[2]==(blue?255:0);
                    continue;
                }
                if(span_draw_case>=12) {
                    int source_y=int(y/8);
                    int source_x=int(x/8);
                    if(span_draw_case==14) source_x=int(std::floor((double(x)-8)/8));
                    if(span_draw_case==16) {
                        const double divisor=256+224*(32767./32768.);
                        const double a=-512/divisor,b=256/divisor,eye_x=eye==0?1./64:-1./64;
                        const double screen=(double(x)+.5)/128-1;
                        source_x=int(std::floor(((screen-eye_x*b)/(1+eye_x*a)+1)*16));
                    }
                    if(span_draw_case>=15) {
                        constexpr std::array<int,16> wave{0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1};
                        source_y-=wave[static_cast<unsigned>(((source_x-6)>>1)+2)&15U];
                    }
                    // Case 14 now uses ordinary hardware triangles: unlike
                    // inclusive source spans, their right boundary is x=12.
                    const bool green=span_draw_case!=13 && source_x>=4 && source_x<(span_draw_case>=15?13:12) && source_y>=4 && source_y<12;
                    const auto at=(y*width+x)*4;
                    // This fixture uses identity projection: positive eye-Y
                    // billboard offsets increase framebuffer Y. Only the
                    // first texture row survives the non-square art clip.
                    if(span_draw_case==15 && x>=32 && x<96 && y>=32 && y<96) {
                        correct&=pixels[at]==255 && pixels[at+1]==0 && pixels[at+2]==255;
                    } else if(span_draw_case==15 && x>=192 && x<224 && y>=32 && y<48) {
                        correct&=pixels[at]==255 && pixels[at+1]==0 && pixels[at+2]==255;
                    } else if(span_draw_case==15 && x>=152 && x<168 && y>=64 && y<72) {
                        correct&=pixels[at]==255 && pixels[at+1]==0 && pixels[at+2]==255;
                    } else if(span_draw_case==15 && x==111 && y>=32 && y<96) {
                        correct&=pixels[at]==0 && pixels[at+1]==0 && pixels[at+2]==255;
                    } else if(span_draw_case==14 && x>=160 && x<176 && y>=64 && y<72) {
                        correct&=pixels[at]==255 && pixels[at+1]==0 && pixels[at+2]==255;
                    } else if(span_draw_case==14 && eye==0 && x==119 && y>=32 && y<96) {
                        correct&=pixels[at]==0 && pixels[at+1]==0 && pixels[at+2]==255;
                    } else if(span_draw_case==14 && eye==0 && x>=48 && x<56 && y>=32 && y<96) {
                        correct&=pixels[at]==255 && pixels[at+1]==0 && pixels[at+2]==0;
                    } else if(span_draw_case==14 && green && source_y>=8) {
                        const bool transparent=source_x>=8;
                        correct&=pixels[at]==0 && pixels[at+1]==(transparent?0:255) && pixels[at+2]==(transparent?0:255);
                    } else if(span_draw_case==14 && green && source_x>=8) {
                        correct&=pixels[at]==255 && pixels[at+1]==255 && pixels[at+2]==0;
                    } else if(span_draw_case>=15 && green) {
                        const bool odd=((source_x^(int(y)/8))&1)!=0;
                        const std::array<unsigned,3> rgb=odd?std::array<unsigned,3>{128,64,192}:
                            std::array<unsigned,3>{192,128,64};
                        for(unsigned channel=0;channel<3;++channel) {
                            double expected=rgb[channel];
                            if(span_draw_case==16) {
                                const double encoded=expected/255.;
                                expected=255*(encoded<=.04045?encoded/12.92:std::pow((encoded+.055)/1.055,2.4));
                            }
                            correct&=std::abs(int(pixels[at+channel])-int(std::lround(expected)))<=1;
                        }
                    } else correct&=pixels[at]==0 && pixels[at+1]==(green?255:0) && pixels[at+2]==0;
                    continue;
                }
                const int shift=span_draw_case>=6?(eye==0?4:-4):0;
                const int source_x=int(x)-shift;
                int logical_x=int(std::floor(source_x/8.));
                if(span_draw_case>=8) {
                    const double a=(1/.625-1/.375)/2,b=(1/.625+1/.375)/2;
                    const double eye_x=eye==0?1./64:-1./64;
                    const double screen=(double(x)+.5)/128-1;
                    logical_x=int(std::floor(((screen-eye_x*b)/(1+eye_x*a)+1)*16));
                }
                const bool red=int(x)>=48+shift*2 && int(x)<64+shift*2 && y>=48 && y<64;
                const bool resident=span_draw_case==2 || span_draw_case==3 || span_draw_case>=10;
                int row=int(y/8);
                if(span_draw_case>=4) {
                    constexpr std::array<int,16> wave{0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1};
                    const int phase=((logical_x-6)>>1)+2;
                    row-=wave[static_cast<unsigned>(phase)&15U];
                }
                const bool mask=span_draw_case!=5 || (logical_x&1)==0;
                const bool green=span_draw_case<10 && !red && logical_x>=4 && logical_x<(resident?13:12) && row>=4 && row<12 && mask;
                const auto at=(y*width+x)*4;
                correct&=pixels[at]==(red?255:0) && pixels[at+1]==(green?255:0) && pixels[at+2]==0;
            }
            std::vector<unsigned char> capture(pixels,pixels+width*height*4);
            vkUnmapMemory(device,*read_memory);
            if(argc>=2) bitmap(std::filesystem::path(argv[1])/("span-depth-"+std::to_string(span_draw_case)+"-eye-"+std::to_string(eye)+".bmp"),capture,width,height);
            require(correct,"Source span face coverage/depth mismatch");
            std::cout<<"Span face "<<span_draw_case<<" eye "<<eye<<": exact coverage and order-independent nearer occlusion passed\n";
            continue;
        }
        if(shutter_case>=0) {
            const double tops[]{96,80,40,0},bottoms[]{96,112,152,192};
            bool correct=true;
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                const double row=(double(y)+.5)*192./height;
                const unsigned expected=row>=tops[shutter_case] && row<bottoms[shutter_case]?255:0;
                for(unsigned c=0;c<3;++c) correct&=pixels[(y*width+x)*4+c]==expected;
            }
            std::vector<unsigned char> capture(pixels,pixels+width*height*4);
            vkUnmapMemory(device,*read_memory);
            require(correct,"VR shutter coverage differs from analytic opening bounds");
            if(argc>=2) bitmap(std::filesystem::path(argv[1])/("shutter-"+std::to_string(shutter_case)+"-eye-"+std::to_string(eye)+".bmp"),capture,width,height);
            std::cout<<"Shutter "<<shutter_case<<" eye "<<eye<<": exact full-viewport GPU mask passed\n";
            continue;
        }
        if(circle_case>=0) {
            const unsigned center=(height/2*width+width/2)*4;
            const int expected[]{194,62,97,31};
            const bool correct=std::abs(int(pixels[center])-expected[circle_case])<=1
                && std::abs(int(pixels[center+1])-((circle_case&2)?64:128))<=1
                && std::abs(int(pixels[0])-128)<=1;
            std::vector<unsigned char> disk_capture(pixels,pixels+width*height*4);
            vkUnmapMemory(device,*read_memory);
            require(correct,"Analytic circle color blend or exterior coverage mismatch");
            if(argc>=2) bitmap(std::filesystem::path(argv[1])/("circle-"+std::to_string(circle_case)+"-eye-"+std::to_string(eye)+".bmp"),disk_capture,width,height);
            std::cout<<"Circle blend "<<circle_case<<" eye "<<eye<<": center color and untouched exterior passed\n";
            continue;
        }
        if(photo_case>=0) {
            constexpr unsigned expected[7][3]{{128,0,128},{0,0,255},{255,0,0},{128,128,128},{32,64,96},{55,55,55},{128,0,0}};
            if(photo_case==7 && eye==0) for(unsigned c=0;c<3;++c) photo_zenith[c]=pixels[c];
            unsigned count=0;
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                const auto offset=(y*width+x)*4;
                if(!pixels[offset] && !pixels[offset+1] && !pixels[offset+2]) continue;
                ++count;
                if(photo_case<7) for(unsigned c=0;c<3;++c)
                    require(std::abs(int(pixels[offset+c])-int(expected[photo_case][c]))<=1,"Filtered artwork pixel mismatch");
                if(photo_case>=10) for(unsigned c=0;c<3;++c)
                    require(std::abs(float(pixels[offset+c])-weather_expected[c])<=1.1F,"Weather GPU/CPU shade mapping mismatch");
                if(photo_case>=7 && photo_case<10) require(y<=height/2,"Photographic sky covered the ground");
                if(photo_case>=7 && photo_case<10 && y<height/8) for(unsigned c=0;c<3;++c)
                    require(std::abs(int(pixels[offset+c])-int(photo_zenith[c]))<=1,"Photographic zenith changes with eye/longitude/LOD");
            }
            require(count>100,"Photographic test produced no visible surface");
            if(photo_case>=7 && photo_case<10) require(count>=width*(height/2-1),"Photographic sky has coverage holes");
            if(photo_case>=10) require(count>100,"Weather palette fixture is blank");
            if(photo_case>=7 && photo_case<10 && argc>=2) {
                const std::vector<unsigned char> capture(pixels,pixels+width*height*4);
                bitmap(std::filesystem::path(argv[1])/("photo-landscape-"+std::to_string(photo_case-7)+"-eye-"+std::to_string(eye)+".bmp"),capture,width,height);
            }
            vkUnmapMemory(device,*read_memory);
            std::cout<<"Filtered artwork "<<photo_case<<" eye "<<eye<<": bilinear/mip/palette/alpha sample passed\n";
            continue;
        }
        uint64_t sum=0,green=0,red=0,blue=0,wrong_pattern=0,lit=0,linear_gray=0;
        uint64_t wrong_tile=0;
        for(uint32_t y=0;y<height;++y) for(uint32_t x=0;x<width;++x) {
            const auto offset=(y*width+x)*4;
            if(pixels[offset]>=54 && pixels[offset]<=56 && pixels[offset+1]==pixels[offset] && pixels[offset+2]==pixels[offset]) ++linear_gray;
            if(pixels[offset] || pixels[offset+1] || pixels[offset+2]) ++lit;
            if(tile_case>=0 && (pixels[offset] || pixels[offset+1] || pixels[offset+2]))
                for(unsigned c=0;c<3;++c) if(pixels[offset+c]!=((tile_expected[tile_case]>>(c*8))&255)) ++wrong_tile;
            if(pixels[offset+1]>200 && pixels[offset]<20 && pixels[offset+2]<20) {sum+=x;++green;}
            if(pixels[offset]>200 && pixels[offset+1]<20 && pixels[offset+2]<20) ++red;
            const bool is_green=pixels[offset+1]>200 && pixels[offset]<20 && pixels[offset+2]<20;
            const bool is_blue=pixels[offset+2]>200 && pixels[offset]<20 && pixels[offset+1]<20;
            if(is_blue) ++blue;
            if(sample>=8 && (is_green || is_blue) && is_blue!=bool(((x/dither_scale)^(y/dither_scale))&1)) ++wrong_pattern;
        }
        std::vector<unsigned char> capture;
        if(argc>=2) capture.assign(pixels,pixels+width*height*4);
        vkUnmapMemory(device,*read_memory);
        if(decal_case>=0) {
            require(decal_case==0?(green>1500 && red==0):(red>1500 && green==0),
                "Coplanar texture decal or nearer-surface occlusion failed");
            std::cout<<"Decal "<<decal_case<<" eye "<<eye<<": backing/nearer-occluder depth passed\n";
            continue;
        }
        if(tile_case>=0) {
            if(wrong_tile || (tile_expected[tile_case]?lit<100:lit!=0))
                throw std::runtime_error("GPU tile parity case "+std::to_string(tile_case)+" eye "+std::to_string(eye));
            if(eye==1 && std::size_t(tile_case)+1==tile_cases.size()) std::cout<<tile_cases.size()<<" GPU tile cases match CPU decoder in both eyes\n";
            continue;
        }
        if(argc>=2 && billboard_case>=0) bitmap(std::filesystem::path(argv[1])/
            ("billboard-"+std::to_string(billboard_case)+(eye?"-right.bmp":"-left.bmp")),capture,width,height);
        else if(argc>=2 && texture_case>=0) bitmap(std::filesystem::path(argv[1])/
            ("texture-"+std::to_string(texture_case)+(eye?"-right.bmp":"-left.bmp")),capture,width,height);
        else if(argc>=2 && line_case>=0) bitmap(std::filesystem::path(argv[1])/
            (std::string(line_case==0?"line-visible-":"line-hidden-")+(eye?"right.bmp":"left.bmp")),capture,width,height);
        else if(argc>=2 && cartridge_sample && live) bitmap(std::filesystem::path(argv[1])/
            (std::string("live-scene-")+(eye?"right.bmp":"left.bmp")),capture,width,height);
        else if(argc>=2) bitmap(std::filesystem::path(argv[1])/
            (std::string(visibility_case==0?"visible-":visibility_case==1?"hidden-":visibility_case==2?"tangent-":visibility_case==3?"group-visible-":visibility_case==4?"group-hidden-":visibility_case==5?"group-tangent-":cartridge_sample?"arwing-geometry-":sample>=10?"dither2-":sample>=8?"dither1-":sample>=6?"scaled-":sample>=4?"translated-":depth_enabled?"":"no-depth-")+(eye?"right.bmp":"left.bmp")),capture,width,height);
        if(visibility_case>=0) {
            require(visibility_case%3==1?green==0:green==1636,"GPU source visibility selected wrong faces");
            std::cout<<"Eye "<<eye<<" visibility case "<<visibility_case<<": "<<green<<" green pixels\n";continue;
        }
        if(line_case>=0) {
            require(line_case==1?green==0:(green>=59 && green<=62),"GPU line coverage/group visibility incorrect");
            std::cout<<"Eye "<<eye<<" line case "<<line_case<<": "<<green<<" green pixels\n";continue;
        }
        if(texture_case>=0) {
            const bool transparent=texture_case==1 || texture_case==2;
            require(texture_case==4?(linear_gray==1636 && red==1987):transparent?(green==0 && red==3623):(green==1636 && red==1987),"Texture wrapping/transparency/color-space mismatch");
            std::cout<<"Eye "<<eye<<" texture case "<<texture_case<<": "<<green<<" green pixels, "<<red<<" red pixels\n";continue;
        }
        if(axis_case>=0) {
            if(axis_case==0) require(axis_near?green>0:green>20 && green<100,"Collapsed model axis coverage invalid");
            else require(lit==0,"Behind-eye collapsed model was not clipped");
            if(argc>=2) bitmap(std::filesystem::path(argv[1])/("axis-"+std::to_string(axis_case)+(eye?"-right.bmp":"-left.bmp")),capture,width,height);
            std::cout<<"Eye "<<eye<<" collapsed axis "<<axis_case<<": "<<green<<" green pixels\n";continue;
        }
        if(billboard_case>=0) {
            if(tunnel_fixture) {
                require(billboard_case==0?(green>0 && green<width*height):green==0,
                    "Tunnel opening/surround coverage invalid");
                for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
                    const auto* pixel=capture.data()+(y*width+x)*4;
                    if(pixel[0]==0 && pixel[1]==255 && pixel[2]==0) continue;
                    require(std::abs(int(pixel[0])-51)<=1 && std::abs(int(pixel[1])-77)<=1
                        && std::abs(int(pixel[2])-102)<=1,"Tunnel surround colour mismatch");
                }
                std::cout<<"Tunnel direction "<<billboard_case<<" eye "<<eye<<": "<<green<<" opening pixels\n";
                continue;
            }
            if(sprite_sizes) {
                require(billboard_case==2?lit==0:green>0,"Whole-object sprite visibility/size invalid");
                std::cout<<"Whole-object sprite case "<<billboard_case<<" eye "<<eye<<": "<<green<<" green pixels\n";
                continue;
            }
            if(billboard_case==0) {require(green>3000 && green<4000,"Billboard coverage invalid");billboard_coverage[eye]=green;}
            if(billboard_case==1) require(green==billboard_coverage[eye],"Model rotation changed eye-facing billboard coverage");
            if(billboard_case==2) require(lit==0,"Behind-eye billboard was not clipped");
            if(billboard_case==3) require(green==billboard_coverage[eye]/2,"Non-square sprite texels repeated instead of clipping");
            std::cout<<"Eye "<<eye<<" billboard case "<<billboard_case<<": "<<green<<" green pixels\n";continue;
        }
        if(cartridge_sample) {
            if(live) {
                if(tiles) require(tiles_empty?lit==0:(green>100 && green==lit),"GPU tile decoding/transparency mismatch");
                if(projected_text) require(scaled_text_hidden?lit==0:(green>100 && green==lit),"Projected text GPU glyph/colour coverage mismatch");
                if(shadows) require(shadows_off?lit==0:(green>10 && green==lit),"Source shadow GPU coverage/colour mismatch");
                if(particles) {
                    std::cout<<"Eye "<<eye<<" particles: "<<green<<" green dot pixels, "<<red<<" red trail pixels, "<<blue<<" blue pixels\n";
                    require(green>=4 && red>10 && blue==0,"Particle GPU dots/trails/filtering mismatch");
                }
                if(dust) {
                    std::cout<<"Eye "<<eye<<" dust: "<<green<<" green, "<<lit<<" lit pixels\n";
                    // At this diagnostic's wide FOV one source pixel is
                    // subpixel-sized; do not require every star to hit a sample.
                    require(coloured_dust?lit>0:(green>0 && green==lit),"Dust GPU billboard colour/coverage mismatch");
                }
                const auto direction=argc==5?std::string_view(argv[4]):std::string_view{};
                if(direction.starts_with("--live-stage=TITLEMAP:menu@")
                    && (direction.ends_with("@rear") || direction.ends_with("@left") || direction.ends_with("@right"))) {
                    // The native initial menu atlas has blue sky and green
                    // ground. A lit-pixel-only test accepts its old brown clear.
                    for(unsigned x=0;x<width;++x) {
                        const auto top=size_t(x)*4;
                        const auto bottom=(size_t(height-1)*width+x)*4;
                        require(pixels[top+2]>pixels[top] && pixels[top+2]>pixels[top+1],
                            "EX menu surround lost its blue sky");
                        require(pixels[bottom+1]>pixels[bottom] && pixels[bottom+1]>pixels[bottom+2],
                            "EX menu surround lost its green ground");
                    }
                }
                if(direction.starts_with("--live-stage=CONTINUE:")
                    && (direction.ends_with("@rear") || direction.ends_with("@left") || direction.ends_with("@right"))) {
                    require(lit==width*height,"Continue surround has an uncovered pixel");
                    for(size_t pixel=1;pixel<size_t(width)*height;++pixel)
                        for(unsigned channel=0;channel<3;++channel)
                            require(pixels[pixel*4+channel]==pixels[channel],
                                "Continue foreground repeated into the surrounding background");
                }
                const bool black_tunnel_rear=!live_tunnel_surround.empty()
                    && (direction.ends_with("@rear") || direction.ends_with("@left") || direction.ends_with("@right"))
                    && live_backdrop[0]==0.f && live_backdrop[1]==0.f && live_backdrop[2]==0.f;
                if(black_tunnel_rear && grid_mode.find("@no-models")!=std::string_view::npos)
                    require(lit==0,"Tunnel background artwork repeated outside the center opening");
                const bool blackout_rear=menu_blackout_capture && (direction.ends_with("@rear")
                    || direction.ends_with("@left") || direction.ends_with("@right"));
                if(blackout_rear) require(lit==0,"Native blackout menu surround must be entirely black");
                require(tile_suite || tiles_empty || shadows_off || scaled_text_hidden || black_tunnel_rear || blackout_rear || lit>0,"Live game models produced a blank GPU image");
                std::cout<<"Eye "<<eye<<" live scene: "<<live_packets.size()<<" model packets, "<<lit<<" lit pixels\n";
                if(terminal_ground_colour) {
                    for(unsigned y=height-16;y<height;++y) for(unsigned x=width/4;x<width*3/4;++x)
                        for(unsigned channel=0;channel<3;++channel)
                            require(pixels[(y*width+x)*4+channel]==(*terminal_ground_colour)[channel],
                                "Surround ground stretched a transition row instead of the terminal floor");
                    std::cout<<"Eye "<<eye<<" terminal ground: native solid floor, no transition-row stripes\n";
                }
                continue;
            }
            require(lit>100 && lit<width*height/2,"Cartridge model coverage invalid");
            std::cout<<"Eye "<<eye<<" cartridge "<<cartridge_name<<": "<<cartridge.vertices.size()/3<<" triangles, "<<cartridge.line_vertices.size()/2<<" lines, "<<lit<<" nonblack pixels (diagnostic palette)\n";continue;
        }
        if(!depth_enabled) {
            require(green==0 && red>3000,"Depth-disabled control did not expose painter-order occlusion");
            std::cout<<"Eye "<<eye<<" depth-disabled control: "<<green<<" green, "<<red<<" red pixels\n";
            continue;
        }
        if(sample>=8) {
            require(green>750 && blue>750 && green+blue==1636 && wrong_pattern==0,"GPU dither differs from source XOR pattern");
            std::cout<<"Eye "<<eye<<" dither scale "<<dither_scale<<": "<<green<<" green + "<<blue<<" blue, exact XOR pattern\n";continue;
        }
        if(sample>=6) {
            require(green>380 && green<440,"GPU model scaling did not quarter triangle coverage");
            std::cout<<"Eye "<<eye<<" half-scale model: "<<green<<" green pixels\n";continue;
        }
        if(sample>=4) {
            require(green>1500 && green<1800,"GPU model translation changed triangle area");
            const double displacement=double(sum)/green-centroid[eye];
            require(displacement>14.9 && displacement<15.5,"GPU model translation has wrong direction or magnitude");
            std::cout<<"Eye "<<eye<<" translated model: "<<displacement<<" pixels right\n";continue;
        }
        require(green>500 && green<5000,"Triangle pixel coverage invalid");
        require(red>500,"Far triangle missing; depth coverage not exercised");
        centroid[eye]=double(sum)/green;
        std::cout<<"Eye "<<eye<<": "<<green<<" green pixels, "<<red<<" red pixels, centroid "<<centroid[eye]<<'\n';
    }
    if(photographic_sky) {
        std::cout<<"Photographic landscape front/side/rear captured in both eyes; synthetic triangle suite not selected.\n";
    } else if(startup_menu) {
        std::cout<<"Localized startup layout captured in both eyes; synthetic triangle suite not selected.\n";
    } else {
        if(sbs_fixture) {
            const double expected=double(width)*.5/std::tan(.7)*.064*(1./2-1./4);
            require(std::abs(centroid[0]-centroid[1]-expected)<.5,"SBS GPU disparity differs from off-axis projection");
        } else require(centroid[0]-centroid[1]>3 && centroid[0]-centroid[1]<6,"Stereo disparity missing or reversed");
        std::cout<<"Native Vulkan stereo/depth triangle readback passed; no headset used.\n";
    }
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
