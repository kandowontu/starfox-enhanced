#include "starfox/render/gpu_model.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/packed_faces.hpp"
#include "starfox/render/face_material.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include <SDL3/SDL.h>
#include <iostream>
#include <iomanip>
#include <limits>
#include "starfox/render/enhanced_terrain.hpp"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <source_location>
std::string check_context;
void require(bool value,const std::source_location where=std::source_location::current()) {
    if(!value) throw std::runtime_error("GPU check line "+std::to_string(where.line())+" "+check_context+": "+SDL_GetError());
}
int main(int argc,char** argv)try {
    if(argc<3 || argc>7) throw std::runtime_error("usage: starfox_gpu_model_check ROM SYMBOLS [model-limit] [--live-ex61|--live-ex61-faces|--terrain|--terrain-batch|--terrain-merged|--scene|--mixed-scene|--mixed-batch|--submitted-batch|--recorded-batch|--matrix-scene|--polygons-only|--lines-only|--face-scan|--alternate|--alternate-scene|--alternate-layers|--alternate-faces] [model-name] [capture-directory]");
    const auto rom=starfox::assets::RomImage::load(argv[1]);const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    const starfox::assets::ShapeDecoder decoder(rom,symbols);
    const unsigned limit=argc>=4?unsigned(std::stoul(argv[3])):64;
    const bool terrain_merged=argc>=5 && std::string(argv[4])=="--terrain-merged";
    const bool terrain_batch=(terrain_merged && !SDL_getenv("STARFOX_TEST_TERRAIN_FACE")) || (argc>=5 && std::string(argv[4])=="--terrain-batch");
    const bool terrain=terrain_merged || terrain_batch || (argc>=5 && std::string(argv[4])=="--terrain");
    const bool live_wingman=argc>=5 && std::string(argv[4])=="--live-wingman";
    const bool live_ex61=argc>=5 && (std::string(argv[4])=="--live-ex61" || std::string(argv[4])=="--live-ex61-faces");
    std::vector<double> submission_us;
    const bool destruction_faces=argc>=5 && std::string(argv[4])=="--destruction-faces";
    const bool warp_axis=argc>=5 && std::string(argv[4])=="--warp-axis";
    const bool native_axis=argc>=5 && std::string(argv[4])=="--native-axis";
    const bool axis=native_axis || warp_axis || (argc>=5 && std::string(argv[4])=="--axis");
    const bool warp_alternate=argc>=5 && std::string(argv[4])=="--warp-alternate";
    const bool warp_destruction=argc>=5 && std::string(argv[4])=="--warp-destruction";
    const bool warp_wobble=argc>=5 && std::string(argv[4])=="--warp-wobble";
    const bool wobble_queued=argc>=5 && std::string(argv[4])=="--wobble-queued";
    const bool wobble_batch=wobble_queued || (argc>=5 && std::string(argv[4])=="--wobble-batch");
    const bool wobble_combinations=wobble_batch || warp_wobble || (argc>=5 && std::string(argv[4])=="--wobble-combinations");
    const bool colour_warp=warp_wobble || warp_destruction || warp_axis || warp_alternate || (argc>=5 && std::string(argv[4])=="--colour-warp");
    const bool wave_faces=argc>=5 && std::string(argv[4])=="--wave-faces";
    const bool wave_control=argc>=5 && std::string(argv[4])=="--wave-control";
    const bool wave_static=argc>=5 && std::string(argv[4])=="--wave-static";
    const bool wave_batch=argc>=5 && std::string(argv[4])=="--wave-batch";
    const bool wave=wave_batch || wave_static || wave_control || wave_faces || (argc>=5 && std::string(argv[4])=="--wave");
    const bool destruction=warp_destruction || destruction_faces || (argc>=5 && std::string(argv[4])=="--destruction");
    const bool polygons_only=argc>=5 && std::string(argv[4])=="--polygons-only";
    const bool wobble_bypass=wobble_combinations || (argc>=5 && std::string(argv[4])=="--wobble-bypass");
    const bool lines_only=argc>=5 && std::string(argv[4])=="--lines-only";
    const bool backface_faces=argc>=5 && std::string(argv[4])=="--backface-faces";
    const bool native_backface=argc>=5 && std::string(argv[4])=="--native-backface";
    const bool backface=native_backface || backface_faces || (argc>=5 && std::string(argv[4])=="--backface");
    const bool alternate_faces=argc>=5 && std::string(argv[4])=="--alternate-faces";
    const bool face_scan=wave_control || wave_faces || backface_faces || destruction_faces || alternate_faces || (argc>=5 && (std::string(argv[4])=="--face-scan" || std::string(argv[4])=="--live-ex61-faces"));
    const bool isolated_layers=alternate_faces || (argc>=5 && std::string(argv[4])=="--alternate-layers");
    const bool alternate_scene=isolated_layers || (argc>=5 && std::string(argv[4])=="--alternate-scene");
    const bool alternate=alternate_scene || (argc>=5 && std::string(argv[4])=="--alternate");
    const bool scene_mode=terrain_batch || wobble_batch || wave_batch || alternate_scene || (argc>=5 && !terrain && !live_wingman && !live_ex61 && !colour_warp && !wave && !axis && !destruction && !polygons_only && !lines_only && !face_scan && !alternate && !backface && !wobble_bypass);
    const bool matrix_only=scene_mode && std::string(argv[4])=="--matrix-scene";
    const bool recorded_batch=argc>=5 && std::string(argv[4])=="--recorded-batch";
    const bool billboard_batch=argc>=5 && std::string(argv[4])=="--billboard-batch";
    const bool queued_batch=wobble_queued || (argc>=5 && std::string(argv[4])=="--queued-batch");
    const bool submitted_batch=wobble_batch || queued_batch || recorded_batch || (argc>=5 && std::string(argv[4])=="--submitted-batch");
    const bool mixed_batch=terrain_batch || wave_batch || billboard_batch || submitted_batch || (argc>=5 && std::string(argv[4])=="--mixed-batch");
    const bool mixed_scene=mixed_batch || (argc>=5 && std::string(argv[4])=="--mixed-scene");
    require(!scene_mode || matrix_only || alternate_scene || mixed_scene || std::string(argv[4])=="--scene");
    require(SDL_Init(SDL_INIT_VIDEO));
    const auto device_properties=SDL_CreateProperties();require(device_properties!=0);
    SDL_SetBooleanProperty(device_properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(device_properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(device_properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN,true);
    SDL_SetBooleanProperty(device_properties,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
    SDL_SetBooleanProperty(device_properties,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,SDL_getenv("STARFOX_TEST_LOW_POWER_GPU")!=nullptr);
    auto* device=SDL_CreateGPUDeviceWithProperties(device_properties);SDL_DestroyProperties(device_properties);require(device);
    std::cout<<"GPU adapter: "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<std::endl;
    starfox::render::GpuModel gpu;
    starfox::render::GpuScene scene;
    starfox::render::GpuRaster legacy;
    if(mixed_scene) {
        starfox::render::RasterCommands pending;pending.reset(8,8);
        require(legacy.render_resident(device,pending,false));
        auto* rejected=SDL_AcquireGPUCommandBuffer(device);require(rejected);
        if(legacy.enqueue_commands(device,rejected,pending).pixels)
            throw std::runtime_error("Borrowed raster accepted pending submitted work");
        require(SDL_CancelGPUCommandBuffer(rejected));
        require(legacy.wait_for_completion());
        starfox::render::RasterCommands invalid;invalid.reset(0,8);
        rejected=SDL_AcquireGPUCommandBuffer(device);require(rejected);
        if(legacy.enqueue_commands(device,rejected,invalid).pixels)
            throw std::runtime_error("Borrowed raster accepted zero-width input");
        require(SDL_CancelGPUCommandBuffer(rejected));
        std::cout<<"Pending-work and invalid-dimension rejection preserve caller cancellation; subsequent mixed rendering tests recovery\n";
    }
    const unsigned width=(live_wingman || live_ex61)?400:224,height=(live_wingman || live_ex61)?224:192;
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,width*height*((terrain || live_wingman || wobble_bypass || backface || colour_warp || wave || axis || billboard_batch || destruction)?16U:4U)*20+4096,0};
    auto* download=SDL_CreateGPUTransferBuffer(device,&ti);require(download);
    if(mixed_batch) {
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
        const auto empty=scene.enqueue_batch(device,command,8,8,{});
        if(!empty.pixels) throw std::runtime_error(scene.status());
        if(empty.surfaces) throw std::runtime_error("Empty scene retained full-resolution surface metadata");
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(empty.pixels),0,64*4};
        SDL_GPUTransferBufferLocation to{download,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        const auto* pixels=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(device,download,false));require(pixels);
        for(unsigned i=0;i<64;++i) if(pixels[i]) throw std::runtime_error("Empty scene batch did not clear pixels/coverage");
        SDL_UnmapGPUTransferBuffer(device,download);
        starfox::render::RasterCommands invalid;invalid.reset(9,8);
        const std::array<starfox::render::GpuSceneDraw,1> draws{starfox::render::GpuRasterDraw{&invalid}};
        command=SDL_AcquireGPUCommandBuffer(device);require(command);
        if(scene.enqueue_batch(device,command,8,8,draws).pixels)
            throw std::runtime_error("Scene batch accepted inconsistent raster dimensions");
        require(SDL_CancelGPUCommandBuffer(command));
        std::cout<<"Empty batch clears; invalid layout rejects; following batches verify recovery\n";
    }
    std::set<std::uint32_t> seen;unsigned checked=0,skipped=0,images=0;std::uint64_t drawn=0;
    double maximum_normal_error=0,maximum_depth_error=0;
    auto model_entries=symbols.entries();
    // The live EX 6-1 frame at tick 1000 exposed a single half-pixel tie on
    // shape $009205. Keep the exact pose as an isolated coverage regression.
    if(live_ex61) {model_entries.clear();model_entries["LIVE_EX61"]={37381U};}
    for(const auto& [name,addresses]:model_entries) for(auto address:addresses) {
        if(argc>=6 && name!=argv[5]) continue;
        if(limit && checked>=limit) break;
        if(!seen.insert(address).second || !decoder.looks_like_shape_header(address)) continue;
        auto shape=decoder.decode(address,name);
        if(terrain) {
            shape=starfox::render::EnhancedTerrain::make_shape(int(checked)-3,7,1+checked%4,checked%3);
            shape.colour_words={0x11,0x22,0x33,0x44};
            if(terrain_merged) {
                starfox::render::EnhancedTerrain patches;
                starfox::render::EnhancedTerrain::Batch batch;
                for(int n=0;n<10;++n) {
                    if(!batch.append(patches.patch(n%4,n/4,1+checked%4,0,{1,2,3,4})))
                        throw std::runtime_error("Merged terrain fixture exceeds byte vertex indices");
                }
                shape=std::move(batch.shape);
                if(const auto* selected=SDL_getenv("STARFOX_TEST_TERRAIN_FACE")) {
                    const auto index=std::stoul(selected);
                    if(index>=shape.faces.size()) throw std::runtime_error("Terrain diagnostic face is out of range");
                    auto face=shape.faces[index];std::vector<starfox::assets::Vec3i> vertices;
                    for(auto& corner:face.vertex_indices) {vertices.push_back(shape.vertices[corner]);corner=std::uint8_t(vertices.size()-1);}
                    shape.vertices=std::move(vertices);shape.word_coordinates.assign(shape.vertices.size(),true);shape.faces={face};
                }
            }
        }
        unsigned billboard_colour=0;
        if(billboard_batch) {
            while(billboard_colour<256 && !starfox::render::texture_for_colour(shape,std::uint8_t(billboard_colour),0)) ++billboard_colour;
            if(billboard_colour==256) {++skipped;continue;}
        }
        if(polygons_only || lines_only) {
            const auto filter=[&](auto& faces){std::erase_if(faces,[&](const auto& face){return lines_only?!face.is_line():face.is_line();});};
            filter(shape.faces);for(auto& batch:shape.face_batches) filter(batch.faces);
        }
        const auto graph=starfox::render::pack_bsp(shape);
        const auto packed=starfox::render::pack_faces(shape,graph,{},{});
        if(face_scan && graph.faces.empty()) {++skipped;continue;}
        const auto complete_shape=shape;
        for(std::size_t face_index=0;face_index<(face_scan?graph.faces.size():1U);++face_index) {
        if(face_scan) {
            shape=complete_shape;shape.bsp_root_address=0;shape.face_batches.clear();
            shape.faces={graph.faces[face_index]};
            std::cout<<name<<" isolated face "<<face_index<<" shift "<<unsigned(shape.header.shift)<<std::endl;
            const auto& normal=shape.faces[0].normal;
            std::cout<<"normal: "<<normal.x<<","<<normal.y<<","<<normal.z<<"\n";
            for(auto index:shape.faces[0].vertex_indices) {const auto& v=shape.vertices[index];std::cout<<unsigned(index)<<": "<<v.x<<","<<v.y<<","<<v.z<<"\n";}
        }
        for(unsigned isolated_layer=0;isolated_layer<(isolated_layers?3U:1U);++isolated_layer)
        for(unsigned mode=0;mode<(wobble_combinations?10U:warp_alternate?5U:destruction?5U:alternate?4U:1U);++mode)
        for(unsigned view:{0U,1U,2U,3U,4U,5U,6U}) for(unsigned scale:{1U,2U,4U}) {
            if(view==6 && !billboard_batch) continue;
            if(const auto* requested=SDL_getenv("STARFOX_TEST_MODEL_VIEW");requested && view!=std::stoul(requested)) continue;
            if(const auto* requested=SDL_getenv("STARFOX_TEST_MODEL_SCALE");requested && scale!=std::stoul(requested)) continue;
            check_context=name+" mode "+std::to_string(mode)+" view "+std::to_string(view)+" scale "+std::to_string(scale);
            if(SDL_getenv("STARFOX_TEST_CONTINUOUS_FIRST") && view<3) continue;
            if(native_axis && (view>=3 || scale!=1))continue;
            if(native_backface && (view>=3 || scale!=1))continue;
            if(live_wingman && view!=0) continue;
            if(live_ex61 && (view!=0 || scale!=1)) continue;
            if(scale==4 && !terrain && !live_wingman && !wobble_bypass && !backface && !colour_warp && !wave && !axis && !billboard_batch && !destruction) continue;
            if(matrix_only && view>=4) continue;
            starfox::render::RenderSettings settings;settings.render_scale=scale;
            settings.backface_culling=backface;
            if(lines_only || axis) settings.wireframe_thickness=std::uint8_t(view%4+1);
            starfox::render::RenderPose pose;pose.use_rotation_matrix=true;
            if(wave) {pose.wave_mode=!wave_control;pose.wave_offset=std::uint16_t(view*13001U);pose.animation_frame=wave_static?0U:view*3U;}
            if(colour_warp) {pose.colour_warp=true;pose.projected_points_address=std::uint16_t(0xb9fU+view*521U);}
            pose.collapse_to_axis_line=axis;
            if(destruction) pose.explosion_progress=std::array<unsigned char,5>{1,2,31,127,255}[mode];
            if(alternate) {pose.cel_mode=mode==0;pose.wireframe_mode=std::uint8_t(mode<3?mode:0);pose.wobble_mode=mode==3?2:0;}
            if(wobble_bypass) pose.wobble_mode=view%2?3:1;
            if(wobble_combinations) {
                pose.wobble_mode=mode%2?3:1;
                pose.wireframe_mode=mode/2<3?mode/2:0;
                pose.cel_mode=mode/2==3;
                pose.wave_mode=mode/2==4;
                pose.wave_offset=std::uint16_t(view*13001U);pose.animation_frame=view*3U;
            }
            if(warp_alternate) {
                pose.cel_mode=mode==0;pose.wireframe_mode=std::uint8_t(mode<3?mode:0);
                pose.wobble_mode=mode==3?2:0;pose.wave_mode=mode==4;
                pose.wave_offset=std::uint16_t(view*13001U);pose.animation_frame=view*3U;
            }
            pose.rotation_matrix={-32768,0,0,0,-32768,0,0,0,-32768};pose.z=1024;
            if(view==1) {pose.rotation_matrix={0,0,-32768,0,-32768,0,32767,0,0};pose.z=256;}
            if(view==2) {pose.rotation_matrix={23170,0,23170,0,32767,0,-23170,0,23170};pose.z=32;}
            if(view==3) {pose.rotation_matrix={23170,0,23170,0,32767,0,-23170,0,23170};pose.z=256;pose.subpixel_projection=true;pose.continuous_geometry=true;}
            if(view>=4) {pose.use_rotation_matrix=false;pose.yaw=16384;pose.pitch=8192;pose.z=512;pose.continuous_geometry=true;}
            if(view==5){pose.x+=11;pose.y-=4;}
            if(terrain) {pose.terrain_geometry=true;pose.continuous_geometry=pose.subpixel_projection=true;}
            // Keep a stable fractional translation in the high-FPS path.
            if(scale==2){pose.x+=.25;pose.y-=.125;pose.continuous_geometry=true;}
            if(billboard_batch) {
                pose.simple_scaled_sprite=true;pose.simple_sprite_colour=std::uint8_t(billboard_colour);
                pose.simple_sprite_world_size=64;pose.colour_frame=view;
                if(view==0){pose.z=128;pose.simple_sprite_world_size=240;}
                if(view==1) pose.z=127.99999999;
                if(view==2){pose.z=512.00000001;pose.x=-14.5;pose.y=17.99999999;}
                if(view==3){pose.z=128;pose.simple_sprite_world_size=32767;pose.effect_clip_left=70;pose.effect_clip_right=140;}
                if(view==4){pose.z=256;pose.simple_sprite_world_size=1;pose.vanish_x=112.5;pose.vanish_y=-0.5;}
                if(view==5){pose.z=200.00000001;pose.simple_sprite_world_size=320;pose.x=-81.999999;pose.effect_clip_left=-5;pose.effect_clip_right=100;}
                if(view==6) {
                    // Near an integer projection boundary, the previous
                    // float-pair billboard shader could choose the adjacent
                    // row on integrated GPUs. Preserve source binary64 math.
                    pose.z=907.145;pose.y=std::nextafter(-32.0*pose.z/settings.focal_length,
                        std::numeric_limits<double>::infinity());
                    pose.vanish_x=112;pose.vanish_y=96;
                }
            }
            if(live_wingman) {
                pose={};pose.use_rotation_matrix=true;pose.continuous_geometry=true;pose.subpixel_projection=true;
                pose.x=740.28802491351939;pose.y=-531.96728517860129;pose.z=4199.0546895451434;
                pose.vanish_x=200;pose.vanish_y=112;
                pose.rotation_matrix={-26493,-13453,13813,-5660,27865,16282,-18432,10778,-24854};
                pose.animation_frame=111;pose.colour_frame=111;pose.palette_override=7;
            }
            if(live_ex61) {
                settings.colour_index_base=112;
                pose={};pose.use_rotation_matrix=true;pose.continuous_geometry=true;pose.subpixel_projection=true;
                pose.x=104.99359130859375;pose.y=-107.79342041015626;pose.z=1326.119055175782;
                pose.vanish_x=200;pose.vanish_y=112;
                pose.rotation_matrix={-32766,0,0,0,32764,0,0,0,-32766};
                pose.animation_frame=103;pose.colour_frame=103;
                pose.source_depth=1300.9205932617188;
                pose.use_source_lighting_state=true;pose.source_lighting_matrix=pose.rotation_matrix;
            }
            starfox::render::Framebuffer cpu(width,height,scale);starfox::render::SoftwareRenderer renderer(settings);
            cpu.enable_layer_tags(true);cpu.begin_write_coverage();
            starfox::render::SurfaceBuffer cpu_surfaces(width*scale,height*scale);
            auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
            starfox::render::GpuRasterOutput output{};
            starfox::render::GpuModelDiagnostics diagnostics;
            const bool trace_geometry=SDL_getenv("STARFOX_TEST_TRACE_GEOMETRY") || (argc>=6 &&
                ((view==5 && mode==0 && ((axis && scale==1) || (destruction_faces && scale==4)))
                 || (destruction_faces && view==1 && mode==3 && (scale==2 || scale==4))
                 || (destruction_faces && view<=1 && mode==0 && scale==4)
                 || (destruction_faces && view==4 && mode==0 && scale==2)
                 || (wave && view==5 && scale==2)
                 || (backface_faces && view>=3 && scale==1)
                 || (backface && view<=2 && scale==4)
                 || (backface_faces && view==0 && scale==2)));
            std::array<starfox::render::RasterCommands,5> batches;
            std::vector<starfox::render::GpuSceneDraw> draws;
            starfox::render::GpuSceneRecording recording;
            recording.reset(width*scale,height*scale);
            starfox::render::RasterCommands pending;pending.reset(width*scale,height*scale);
            for(unsigned layer=isolated_layers?isolated_layer:0;layer<(isolated_layers?isolated_layer+1:mixed_scene?5U:scene_mode?3U:1U);++layer) {
                if(isolated_layers) {cpu.clear();cpu.begin_write_coverage();cpu_surfaces.clear();}
                auto layer_pose=pose;const bool metadata=layer!=1;
                if(wave_batch) {
                    layer_pose.wave_mode=layer!=2;
                    layer_pose.wave_offset=starfox::simulation::wrap16(int(pose.wave_offset)+int(layer)*19);
                }
                if(layer==1){layer_pose.x-=7;layer_pose.y+=3;layer_pose.palette_override=0;}
                if(layer==2){layer_pose.x+=11;layer_pose.y-=4;}
                if(layer==3){layer_pose.x-=19;layer_pose.y-=7;layer_pose.palette_override=5;}
                if(layer==4){layer_pose.x+=23;layer_pose.y+=9;}
                starfox::render::RenderDiagnostics axis_trace;
                renderer.draw(shape,layer_pose,cpu,false,metadata?&cpu_surfaces:nullptr,nullptr,trace_geometry?&axis_trace:nullptr);
                if(trace_geometry && !axis) {
                    std::cout.precision(17);
                    for(const auto& polygon:axis_trace.polygons) {
                        std::cout<<"CPU polygon area "<<polygon.signed_area<<'\n';
                        for(unsigned i=0;i<polygon.camera.size();++i)std::cout<<"CPU polygon point "<<i<<": camera "<<polygon.camera[i][0]<<","<<polygon.camera[i][1]<<","<<polygon.camera[i][2]
                            <<" projected "<<polygon.projected[i][0]<<","<<polygon.projected[i][1]<<'\n';
                    }
                }
                if(trace_geometry && axis) {
                    std::cout.precision(17);
                    for(unsigned i=0;i<2;++i) std::cout<<"CPU axis "<<i<<": camera "<<axis_trace.camera[i][0]<<","<<axis_trace.camera[i][1]<<","<<axis_trace.camera[i][2]
                        <<" projected "<<axis_trace.projected[i][0]<<","<<axis_trace.projected[i][1]<<" clipped "<<axis_trace.clipped[i][0]<<","<<axis_trace.clipped[i][1]<<'\n';
                }
                starfox::render::GpuRasterOutput front;
                if(mixed_scene && (layer==1 || layer==3) && !(queued_batch && (view&1U))) {
                    starfox::render::Framebuffer recorded(width,height,scale);
                    starfox::render::SurfaceBuffer recorded_surfaces(width*scale,height*scale);
                    auto& batch=recorded_batch?pending:batches[layer];
                    if(!recorded_batch) batch.reset(width*scale,height*scale);
                    recorded.record_to(&batch);
                    renderer.draw(shape,layer_pose,recorded,false,metadata?&recorded_surfaces:nullptr);
                    if(recorded_batch) continue;
                    if(mixed_batch) {
                        draws.emplace_back(starfox::render::GpuRasterDraw{&batch,metadata,((view+layer/2)&1U)!=0});
                        continue;
                    }
                    front=legacy.enqueue_commands(device,command,batch,metadata,((view+layer/2)&1U)!=0);
                    if(!front.pixels) {SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(legacy.status());}
                } else if(mixed_batch) {
                    if(recorded_batch) {
                        recording.append_model(pending,{&shape,layer_pose,settings,metadata});
                        continue;
                    }
                    starfox::render::GpuModelDraw model_draw{&shape,layer_pose,settings,metadata};
                    model_draw.geometry_depth=terrain;
                    draws.emplace_back(model_draw);
                    continue;
                } else {
                    const auto started=std::chrono::steady_clock::now();
                    front=gpu.enqueue(device,command,shape,layer_pose,settings,width,height,metadata,nullptr,trace_geometry?&diagnostics:nullptr);
                    submission_us.push_back(std::chrono::duration<double,std::micro>(
                        std::chrono::steady_clock::now()-started).count());
                }
                if(!front.pixels){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(name+": "+gpu.status());}
                if(scene_mode) {
                    const auto back=output;output=scene.enqueue(command,front,layer && !isolated_layers?&back:nullptr);
                    if(!output.pixels){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(name+": "+scene.status());}
                } else output=front;
            }
            if(recorded_batch) {
                recording.finish(pending);
                draws.assign(recording.draws().begin(),recording.draws().end());
                starfox::render::Framebuffer replayed(width,height,scale);
                replayed.enable_layer_tags(true);replayed.begin_write_coverage();
                starfox::render::SurfaceBuffer replayed_surfaces(width*scale,height*scale);
                recording.replay(replayed,&replayed_surfaces);
                if(replayed.pixels()!=cpu.pixels() || replayed.layer_tags()!=cpu.layer_tags()
                    || !std::equal(replayed.write_coverage().begin(),replayed.write_coverage().end(),cpu.write_coverage().begin()))
                    throw std::runtime_error(name+": recorded CPU fallback differs from direct draw order: pixels="
                        +std::to_string(replayed.pixels()!=cpu.pixels())+" tags="+std::to_string(replayed.layer_tags()!=cpu.layer_tags())
                        +" view="+std::to_string(view)+" scale="+std::to_string(scale));
                const auto expected=cpu_surfaces.samples(),actual=replayed_surfaces.samples();
                for(std::size_t i=0;i<expected.size();++i) {
                    const auto& a=actual[i];const auto& b=expected[i];
                    if(a.valid!=b.valid || (a.valid && (a.palette_index!=b.palette_index
                        || a.normal_x!=b.normal_x || a.normal_y!=b.normal_y || a.normal_z!=b.normal_z || a.depth!=b.depth)))
                        throw std::runtime_error(name+": recorded CPU fallback changed surface ownership");
                }
            }
            if(mixed_batch) {
                if(checked==0 && view==0 && scale==1) {
                    auto rejected_draws=draws;
                    // Fail after four already encoded layers, then reuse the
                    // same batch resources for the CPU-checked complete scene.
                    std::get<starfox::render::GpuModelDraw>(rejected_draws.back()).pose.x=std::numeric_limits<double>::infinity();
                    std::get<starfox::render::GpuModelDraw>(rejected_draws.back()).pose.simple_scaled_sprite=false;
                    if(scene.enqueue_batch(device,command,width*scale,height*scale,rejected_draws).pixels)
                        throw std::runtime_error("Scene batch silently accepted unsupported geometry");
                    require(SDL_CancelGPUCommandBuffer(command));
                    command=SDL_AcquireGPUCommandBuffer(device);require(command);
                    std::cout<<"Partially encoded batch rejected and canceled; complete batch retries on reused resources\n";
                }
                if(submitted_batch) {
                    require(SDL_CancelGPUCommandBuffer(command));
                    if(checked==0 && view==0 && scale==1) {
                        auto rejected_draws=draws;
                        std::get<starfox::render::GpuModelDraw>(rejected_draws.back()).pose.x=std::numeric_limits<double>::infinity();
                        std::get<starfox::render::GpuModelDraw>(rejected_draws.back()).pose.simple_scaled_sprite=false;
                        if(scene.render_resident(device,width*scale,height*scale,rejected_draws)
                            || scene.resident_output().pixels)
                            throw std::runtime_error("Failed owned scene exposed a partial resident frame");
                    }
                    if(queued_batch) for(unsigned queued=0;queued<4;++queued) {
                        auto changing_draws=draws;
                        for(auto& draw:changing_draws) if(auto* model=std::get_if<starfox::render::GpuModelDraw>(&draw)) {
                            model->pose.x+=(queued&1)?37:-29;
                            model->pose.y+=int(queued)*11-15;
                            model->pose.palette_override=std::uint8_t(queued);
                        }
                        if(!scene.render_resident(device,width*scale,height*scale,changing_draws))
                            throw std::runtime_error(scene.status());
                    }
                    if(!scene.render_resident(device,width*scale,height*scale,draws))
                        throw std::runtime_error(scene.status());
                    output=scene.resident_output();
                    command=SDL_AcquireGPUCommandBuffer(device);require(command);
                    if(checked==0 && view==0 && scale==1) {
                        if(scene.enqueue_batch(device,command,width*scale,height*scale,draws).pixels)
                            throw std::runtime_error("Borrowed scene batch overwrote pending submission");
                        if(scene.resident_output().pixels!=output.pixels)
                            throw std::runtime_error("Rejected enqueue invalidated resident scene");
                        require(SDL_CancelGPUCommandBuffer(command));
                        require(scene.wait_for_completion());
                        command=SDL_AcquireGPUCommandBuffer(device);require(command);
                        std::cout<<"Pending scene submission protected; fence completion retains presentation buffers\n";
                    }
                } else output=scene.enqueue_batch(device,command,width*scale,height*scale,draws);
                if(!output.pixels){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(name+": "+scene.status());}
            }
            auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
            SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.pixels),0,width*height*scale*scale*4};
            SDL_GPUTransferBufferLocation to{download,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);
            from={static_cast<SDL_GPUBuffer*>(output.surfaces),0,width*height*scale*scale*16};
            to.offset=width*height*scale*scale*4;SDL_DownloadFromGPUBuffer(copy,&from,&to);
            const bool trace_valid=trace_geometry && diagnostics.continuous && diagnostics.point_count<=(4096-129*16)/32 && diagnostics.polygon_count==1;
            if(trace_valid) {
                from={static_cast<SDL_GPUBuffer*>(diagnostics.projected_points),0,diagnostics.point_count*32};
                to.offset=width*height*scale*scale*20;SDL_DownloadFromGPUBuffer(copy,&from,&to);
                from={static_cast<SDL_GPUBuffer*>(diagnostics.clipped_polygons),0,129*16};
                to.offset+=diagnostics.point_count*32;SDL_DownloadFromGPUBuffer(copy,&from,&to);
            }
            SDL_EndGPUCopyPass(copy);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
            const auto* pixels=static_cast<const Uint32*>(SDL_MapGPUTransferBuffer(device,download,false));require(pixels);
            const auto source=cpu.pixels();unsigned mismatch=0;
            if(trace_valid) {
                const auto* values=reinterpret_cast<const float*>(pixels+width*height*scale*scale*5);
                std::cout<<std::setprecision(17);
                for(unsigned i=0;i<diagnostics.point_count;++i) std::cout<<"GPU point "<<i<<": camera "<<values[i*8]<<","<<values[i*8+1]<<","<<values[i*8+2]<<" screen "<<values[i*8+4]<<","<<values[i*8+5]<<'\n';
                const auto* clipped=values+diagnostics.point_count*8;
                const auto count=std::bit_cast<unsigned>(clipped[0]);
                std::cout<<"GPU clip count/status: "<<count<<"/"<<std::bit_cast<unsigned>(clipped[1])<<'\n';
                if(count<=128) for(unsigned i=0;i<count;++i)
                    std::cout<<"GPU clipped "<<i<<": "<<clipped[4+i*4]<<","<<clipped[5+i*4]<<" UV "<<clipped[6+i*4]<<","<<clipped[7+i*4]<<'\n';
            }
            for(std::size_t i=0;i<source.size();++i){mismatch+=(pixels[i]&255U)!=source[i];drawn+=source[i]!=0;}
            const auto* surfaces=reinterpret_cast<const float*>(pixels+source.size());
            const auto expected_surfaces=cpu_surfaces.samples();
            if(submitted_batch) {
                starfox::render::Framebuffer downloaded(width,height,scale);
                downloaded.enable_layer_tags(true);
                downloaded.begin_write_coverage();
                starfox::render::SurfaceBuffer metadata(width*scale,height*scale);
                require(scene.readback(downloaded,&metadata));
                for(std::size_t i=0;i<source.size();++i) {
                    const auto& sample=metadata.samples()[i];
                    require(downloaded.pixels()[i]==std::uint8_t(pixels[i]));
                    require(downloaded.layer_tags()[i]==std::uint8_t(pixels[i]>>8));
                    require(bool(downloaded.write_coverage()[i])==bool(pixels[i]&(1U<<26)));
                    require(sample.valid==bool(pixels[i]&(1U<<24)));
                    if(sample.valid) {
                        require(sample.palette_index==std::uint8_t(pixels[i]>>16));
                        require(sample.normal_x==surfaces[i*4] && sample.normal_y==surfaces[i*4+1]
                            && sample.normal_z==surfaces[i*4+2] && sample.depth==surfaces[i*4+3]);
                    }
                }
            }
            const auto capture_comparison=[&] {
                if(argc!=7) return;
                const std::filesystem::path directory=argv[6];std::filesystem::create_directories(directory);
                starfox::render::Framebuffer comparison(width*scale*2,height*scale);
                for(unsigned y=0;y<height*scale;++y) for(unsigned x=0;x<width*scale;++x) {
                    const auto i=std::size_t(y)*width*scale+x;
                    comparison.set(x,y,source[i]);comparison.set(x+width*scale,y,std::uint8_t(pixels[i]));
                }
                const auto file="model-"+std::to_string(address)+"-face-"+std::to_string(face_index)+"-layer-"+std::to_string(isolated_layer)+"-mode-"+std::to_string(mode)+"-view-"+std::to_string(view)+"-scale-"+std::to_string(scale)+".bmp";
                starfox::render::write_bmp(comparison,directory/file);
                std::cout<<"Capture (CPU left, GPU right; diagnostic palette): "<<(directory/file).string()<<'\n';
            };
            bool coverage_mismatch=false;
            for(std::size_t i=0;i<source.size();++i)
                coverage_mismatch|=bool(pixels[i]&(1U<<26))!=bool(cpu.write_coverage()[i]);
            // Preserve the failing frame before validation throws, including
            // black writes that do not change the indexed pixel comparison.
            if(mismatch || coverage_mismatch) capture_comparison();
            for(std::size_t i=0;i<source.size();++i) {
                const auto& expected=expected_surfaces[i];
                if(bool(pixels[i]&(1U<<26))!=bool(cpu.write_coverage()[i])) throw std::runtime_error(name+" layer "+std::to_string(isolated_layer)+" mode "+std::to_string(mode)+" view "+std::to_string(view)+" scale "+std::to_string(scale)+": write coverage mismatch at "+std::to_string(i));
                if(((pixels[i]>>8)&255U)!=cpu.layer_tags()[i]) throw std::runtime_error(name+": pixel layer mismatch");
                if(!axis && !scene_mode && packed.polygon_only && bool(pixels[i]&(1U<<26))!=expected.valid) throw std::runtime_error(name+": pixel coverage mismatch");
                if(bool(pixels[i]&(1U<<24))!=expected.valid) throw std::runtime_error(name+" view "+std::to_string(view)+" scale "+std::to_string(scale)+": surface coverage mismatch at "+std::to_string(i)+" GPU "+std::to_string(pixels[i])+" CPU valid "+std::to_string(expected.valid));
                if(!expected.valid) continue;
                if(((pixels[i]>>16)&255U)!=expected.palette_index) throw std::runtime_error(name+" face "+std::to_string(face_index)+" view "+std::to_string(view)+" scale "+std::to_string(scale)+" mode "+std::to_string(mode)+": surface palette mismatch at "+std::to_string(i)+" CPU "+std::to_string(expected.palette_index)+" GPU "+std::to_string((pixels[i]>>16)&255U));
                const double normal_error=std::max({std::abs(double(surfaces[i*4])-expected.normal_x),std::abs(double(surfaces[i*4+1])-expected.normal_y),std::abs(double(surfaces[i*4+2])-expected.normal_z)});
                const double depth_error=std::abs(double(surfaces[i*4+3])-expected.depth);
                maximum_normal_error=std::max(maximum_normal_error,normal_error);maximum_depth_error=std::max(maximum_depth_error,depth_error);
                if(!std::isfinite(normal_error) || !std::isfinite(depth_error) || normal_error>1e-5 || depth_error>1e-4+std::abs(expected.depth)*1e-6) {
                    capture_comparison();
                    throw std::runtime_error(name+" view "+std::to_string(view)+" scale "+std::to_string(scale)+" mode "+std::to_string(mode)
                        +": surface normal/depth mismatch, normal error "+std::to_string(normal_error)+", depth error "+std::to_string(depth_error)
                        +", expected depth "+std::to_string(expected.depth)+", actual depth "+std::to_string(surfaces[i*4+3])
                        +", pixel "+std::to_string(i)+", image pixel mismatches "+std::to_string(mismatch));
                }
            }
            if(argc==7 && (((colour_warp || wobble_bypass) && view==0 && scale==2) || (wave && view==5 && scale==2)
                || (destruction?((view==1 && (scale==1 || scale==4)) || (view>=4 && mode==0)):(view==4 && scale==2)))) {
                capture_comparison();
            }
            SDL_UnmapGPUTransferBuffer(device,download);
            if(mismatch) throw std::runtime_error(name+" view "+std::to_string(view)+" scale "+std::to_string(scale)+": "+std::to_string(mismatch)+" pixel mismatches");
            ++images;
        }
        }
        ++checked;
        if(destruction && checked%128==0)
            std::cout<<"Destruction progress: "<<checked<<" models, "<<images<<" images checked\n"<<std::flush;
    }
    if(!checked) throw std::runtime_error("GPU model fixture selected no models");
    if(!drawn) throw std::runtime_error("GPU model comparisons produced no nonzero pixels; select a visible fixture ("+std::to_string(images)+" images checked)");
    gpu.release_device();scene.release_device();legacy.release_device();SDL_ReleaseGPUTransferBuffer(device,download);SDL_DestroyGPUDevice(device);SDL_Quit();
    std::cout<<checked<<(terrain?" generated terrain patches, ":" real models, ")<<images<<" GPU images match SoftwareRenderer; "<<drawn<<" nonzero pixels; "<<skipped<<" filtered/unsupported/empty models deferred\n";
    std::cout<<"Surface coverage/palettes exact; maximum normal error "<<maximum_normal_error<<", depth error "<<maximum_depth_error<<'\n';
    if(!submission_us.empty()) {
        std::sort(submission_us.begin(),submission_us.end());
        std::cout<<"Direct model CPU enqueue wall time: "<<submission_us.size()<<" samples, median "
            <<submission_us[submission_us.size()/2]<<" us, p95 "
            <<submission_us[(submission_us.size()-1)*95/100]
            <<" us (includes allocation/upload recording; excludes later GPU wait/readback; not game FPS)\n";
    }
    if(scene_mode) std::cout<<(mixed_scene?"Five-layer mixed":"Three-layer model")<<" resident composition: black writes, independent surfaces and ping-pong reuse passed\n";
    if(recorded_batch) std::cout<<"Recorded CPU fallback: pixels, layer tags, write coverage and surface samples match direct rendering exactly\n";
    if(queued_batch) std::cout<<"Four changing submissions without readback before each compared frame preserve final geometry and metadata\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
