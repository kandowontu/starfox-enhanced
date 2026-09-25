#include "starfox/vr/application.hpp"
#include "starfox/vr/enhanced_landscape.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/vr/frame_wait.hpp"
#include "starfox/vr/scene_interpolation.hpp"
#include "starfox/vr/cartridge_save.hpp"
#include "starfox/vr/openxr_input.hpp"
#include "starfox/vr/vulkan_device.hpp"
#include "starfox/vr/vulkan_loader.hpp"
#include "starfox/vr/openxr_swapchains.hpp"
#include "starfox/vr/vulkan_eye_targets.hpp"
#include "starfox/vr/vulkan_stereo_draw.hpp"
#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include "starfox/vr/vulkan_scene_buffer.hpp"
#include "starfox/vr/vulkan_scene_textures.hpp"
#include "starfox/vr/shape_batch.hpp"
#include "starfox/vr/game_model_pose.hpp"
#include "starfox/vr/vulkan_draw_packets.hpp"
#include "starfox/vr/source_models.hpp"
#include "starfox/vr/packet_route.hpp"
#include "starfox/vr/pause_sandbox.hpp"
#include "starfox/vr/vulkan_source_scene.hpp"
#include "starfox/vr/vulkan_dxr_frame.hpp"
#include "starfox/vr/vulkan_mixed_ray_scene.hpp"
#include "starfox/vr/source_ray_policy.hpp"
#include "starfox/vr/source_shadow_environment.hpp"
#include "starfox/vr/vulkan_pipeline_cache.hpp"
#include "starfox/vr/source_sprites.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/assets/runtime_bundle.hpp"
#include "starfox/assets/embedded.hpp"
#include <fstream>
#include "starfox/vr/background_tiles.hpp"
#include "starfox/vr/game_frame_driver.hpp"
#include "starfox/vr/startup_menu.hpp"
#include "starfox/state/files.hpp"
#include "starfox/vr/pcm_output.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include "starfox/audio/stem_mixer.hpp"
#include "starfox/audio/msu1_pack.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/vr/vulkan_depth_targets.hpp"
#include <stdexcept>
#include <chrono>
#include <thread>
#include <array>
#include <iostream>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cmath>
#include <charconv>
namespace {
auto load_vr_backdrop(unsigned resource,std::string_view path) {
#if defined(STARFOX_VR_BUNDLE_ASSETS)
    (void)path;return starfox::assets::embedded_asset(int(resource));
#else
    (void)resource;
    std::ifstream stream(std::filesystem::path(path),std::ios::binary);
    if(!stream) throw std::runtime_error("Missing VR backdrop artwork");
    return std::vector<uint8_t>{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
#endif
}
struct LiveGame {
    starfox::assets::RomImage rom;
    starfox::assets::SymbolMap symbols;
    starfox::vr::CartridgeSave cartridge_save;
    starfox::simulation::GameSimulation game;
    starfox::audio::Spc700Audio audio;
    std::shared_ptr<starfox::audio::Msu1Pack> pack;
    starfox::audio::Msu1Audio msu_audio;
    std::vector<int16_t> mixed;
    starfox::vr::PcmOutput output;
    starfox::vr::SourceModels models;
    starfox::render::ScaledTextRenderer dialogue_layout;
    starfox::vr::EnhancedLandscape enhanced_landscape;
    std::unique_ptr<starfox::vr::GameSceneHistory> history;
    std::unique_ptr<starfox::vr::GameFrameDriver> driver;
    unsigned logic_ticks{};
    bool discard_audio{};
    LiveGame(const char* rom_path,const char* symbols_path,const char* msu_path,
             const starfox::vr::ApplicationHost& host,const char* level="LEVEL1_1",bool discard=false)
        :LiveGame(starfox::assets::RomImage::load(rom_path),starfox::assets::SymbolMap::load(symbols_path),
            msu_path,host,level,discard) {}
    // Bundle imports pass owned, validated assets directly. Their lifetime
    // covers all simulation/render references; no extracted files are needed.
    LiveGame(starfox::assets::RomImage image,starfox::assets::SymbolMap table,const char* msu_path,
             const starfox::vr::ApplicationHost& host,const char* level="LEVEL1_1",bool discard=false)
        :rom(std::move(image)),symbols(std::move(table)),
         cartridge_save(discard || symbols.find("PLANETSEQ2_L").empty()?std::filesystem::path{}:host.cartridge_save_path),
         game(rom,symbols,level,cartridge_save.initial(),true),models(rom,symbols,true,!discard),
         dialogue_layout(rom,symbols),enhanced_landscape(symbols),discard_audio(discard) {
        game.set_timing_mode(starfox::simulation::TimingMode::unlocked_20_fps);
        if(msu_path) {
            pack=std::make_shared<starfox::audio::Msu1Pack>(msu_path);
            if(!pack->available()) throw std::runtime_error("Invalid or unavailable MSU soundtrack pack");
            msu_audio=starfox::audio::Msu1Audio([source=pack](uint16_t track){return source->load_track(track);});
        }
        game.set_msu1_available(bool(pack));game.set_msu1_music(bool(pack));
        msu_audio.set_enabled(game.msu1_music());
        const bool native_intro=std::string_view(level)=="INTROMAP";
        const auto checkpoint=native_intro?0:symbols.find("MAPRESTART").at(0);
        unsigned warmup=0;
        while(!native_intro && !game.map().peek_ram_word(checkpoint).value() && warmup<3000) {
            if(host.stop_requested && host.stop_requested()) throw std::runtime_error("VR startup cancelled");
            const auto tick=game.tick({});
            starfox::audio::render_mixed_tick(audio,msu_audio,tick.audio_port_writes,game.map().take_msu_register_writes(),
                game.music_volume(),game.sfx_volume(),mixed);
            game.synchronize_apu_output_ports(audio.output_ports());++warmup;
        }
        if(warmup==3000) throw std::runtime_error("Live game checkpoint did not start");
        history=std::make_unique<starfox::vr::GameSceneHistory>(game,rom,symbols);
        cartridge_save.synchronize(game.ex_save_ram());
        driver=std::make_unique<starfox::vr::GameFrameDriver>(game,[this](auto apu,auto msu) {
            msu_audio.set_enabled(game.msu1_music());msu_audio.set_paused(game.paused());
            starfox::audio::render_mixed_tick(audio,msu_audio,apu,msu,game.music_volume(),game.sfx_volume(),mixed);
            if(!discard_audio && !output.push(mixed)) throw std::runtime_error(output.status());
            cartridge_save.synchronize(game.ex_save_ram());
            return audio.output_ports();
        },history.get());
        if(native_intro) std::cout<<"Source intro ready; no intro or audio ticks skipped; VR startup panel available\n";
        else std::cout<<"Live game checkpoint ready after "<<warmup<<" ticks; silent source preroll complete\n";
    }
};
}
int starfox::vr::run_application(int argc,char** argv,const ApplicationHost& host) {
    if(host.stop_requested && host.stop_requested()) return 0;
    bool graphics=false,loader_only=false,render_clear=false,render_triangle=false,render_model=false,render_game=false;
    const char* model_rom=nullptr;const char* model_symbols=nullptr;const char* model_name=nullptr;const char* msu_path=nullptr;
    const char* preflight_level="LEVEL1_1";unsigned preflight_frames=0;
    const char* bundle_path=nullptr;
    bool bundle_preflight_ex=false;
    bool verify_geometry_cache=false;
    bool background_audit=false;
    bool ray_audit=false;
    bool preflight_invulnerable=false;
    bool ray_tracing=false; // Opt-in while completing material coverage and headset validation.
    bool enhanced_sky=false; // Optional launch override; the menu preference defaults to off.
    for(int i=1;i<argc;++i) {
        const std::string_view arg=argv[i];
        if(arg=="--bundle" && i+1<argc) {bundle_path=argv[++i];render_game=true;graphics=true;preflight_level="INTROMAP";}
        else if(arg=="--graphics") graphics=true;
        else if(arg=="--render-clear") {graphics=true;render_clear=true;}
        else if(arg=="--render-triangle") {graphics=true;render_triangle=true;}
        else if(arg=="--render-model" && i+3<argc) {
            graphics=true;render_model=true;model_rom=argv[++i];model_symbols=argv[++i];model_name=argv[++i];
        }
        else if(arg=="--render-game" && i+2<argc) {
            graphics=true;render_game=true;model_rom=argv[++i];model_symbols=argv[++i];
        }
        else if(arg=="--intro" && i+2<argc) {
            graphics=true;render_game=true;model_rom=argv[++i];model_symbols=argv[++i];preflight_level="INTROMAP";
        }
        else if((arg=="--preflight" && i+4<argc) || ((arg=="--bundle-preflight" || arg=="--bundle-ex-preflight") && i+3<argc)) {
            render_game=true;
            if(arg!="--preflight") {bundle_path=argv[++i];bundle_preflight_ex=arg=="--bundle-ex-preflight";}
            else {model_rom=argv[++i];model_symbols=argv[++i];}
            preflight_level=argv[++i];
            const std::string_view frames=argv[++i];
            const auto parsed=std::from_chars(frames.data(),frames.data()+frames.size(),preflight_frames);
            if(parsed.ec!=std::errc{} || parsed.ptr!=frames.data()+frames.size() || !preflight_frames || preflight_frames>10000) {
                std::cerr<<"Preflight frames must be 1..10000\n";return 1;
            }
        }
        else if(arg=="--loader-only") loader_only=true;
        else if(arg=="--verify-geometry-cache") verify_geometry_cache=true;
        else if(arg=="--background-audit") background_audit=true;
        else if(arg=="--ray-tracing") ray_tracing=true;
        else if(arg=="--enhanced-sky") enhanced_sky=true;
        else if(arg=="--ray-audit") ray_audit=true;
        else if(arg=="--preflight-invulnerable") preflight_invulnerable=true;
        else if(arg=="--msu" && i+1<argc && !msu_path) msu_path=argv[++i];
        else {std::cerr<<"Usage: starfox_vr_runtime_check [--bundle BIN | --bundle-preflight BIN LEVEL FRAMES | --bundle-ex-preflight BIN LEVEL FRAMES | --graphics | --loader-only | --render-clear | --render-triangle | --render-model ROM SYMBOLS SHAPE | --render-game ROM SYMBOLS | --intro ROM SYMBOLS | --preflight ROM SYMBOLS LEVEL FRAMES] [--msu PACK]\n";return 1;}
    }
    if(preflight_invulnerable && !preflight_frames) {
        std::cerr<<"Invulnerable audit requires preflight\n";return 1;
    }
    if(background_audit && (!preflight_frames || verify_geometry_cache)) {
        std::cerr<<"Background audit requires preflight without geometry-cache verification\n";return 1;
    }
    if(ray_audit && (!preflight_frames || background_audit || verify_geometry_cache)) {
        std::cerr<<"Ray audit requires standalone preflight\n";return 1;
    }
    if(graphics && loader_only) {std::cerr<<"Choose one diagnostic mode\n";return 1;}
    if(preflight_frames && (graphics || loader_only)) {std::cerr<<"Preflight cannot be combined with a graphics/loader mode\n";return 1;}
    if(msu_path && !render_game) {std::cerr<<"--msu requires --render-game\n";return 1;}
    if(int(render_clear)+int(render_triangle)+int(render_model)+int(render_game)>1) {std::cerr<<"Choose one rendering mode\n";return 1;}
    std::unique_ptr<LiveGame> live;
    std::optional<starfox::assets::RuntimeBundlePayload> bundle;
    if(render_game) try {
        if(bundle_path) {
#if defined(STARFOX_VR_BUNDLE_ASSETS)
            if(model_rom) throw std::runtime_error("Choose bundle or loose development inputs, not both");
            std::ifstream input(bundle_path,std::ios::binary|std::ios::ate);
            if(!input) throw std::runtime_error("Cannot open Starfox-Assets.BIN");
            const auto size=input.tellg();
            if(size<=0 || size>64*1024*1024) throw std::runtime_error("Invalid asset bundle size");
            std::vector<uint8_t> bytes(static_cast<size_t>(size));input.seekg(0);
            if(!input.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))
                throw std::runtime_error("Cannot read complete asset bundle");
            bundle=starfox::assets::decode_runtime_bundle(bytes,
                starfox::assets::runtime_companion_manifest(starfox::assets::embedded_asset));
            live=std::make_unique<LiveGame>(starfox::assets::RomImage(bundle_preflight_ex?bundle->starfox_ex_rom:bundle->original_rom),
                starfox::assets::SymbolMap::parse(bundle_preflight_ex?bundle->starfox_ex_symbols:bundle->original_symbols),msu_path,host,preflight_level,preflight_frames!=0);
#else
            throw std::runtime_error("This development build lacks asset bundle validation resources");
#endif
        } else live=std::make_unique<LiveGame>(model_rom,model_symbols,msu_path,host,preflight_level,preflight_frames!=0);
    }
    catch(const std::exception& error) {
        if(host.stop_requested && host.stop_requested()) return 0;
        std::cerr<<error.what()<<'\n';return 1;
    }
    if(preflight_frames) try {
        if(preflight_invulnerable) {
            live->game.set_god_mode(true);
            std::cout<<"Preflight invulnerability enabled; no save data is written\n";
        }
        starfox::vr::SourceModels reference(live->rom,live->symbols,false);
        starfox::vr::SourceModels ray_models(live->rom,live->symbols,true,true);
        SourceRayPolicy ray_policy(live->symbols);
        std::array<uint64_t,5> ray_counts{};
        std::array<uint64_t,static_cast<size_t>(PacketRoute::count)> route_counts{};
        std::unordered_map<std::string,bool> ray_reported;
        std::size_t packets=0;unsigned ticks=0;
        std::array<unsigned,15> flow_samples{};
        std::array<std::size_t,5> effect_samples{};
        unsigned msu_playing_samples=0;
        std::uint16_t msu_last_track=0;
        unsigned msu_pcm_blocks=0;
        unsigned msu_pcm_peak=0;
        constexpr std::array effect_names{"colour-warp","wireframe","wobble","wave","cel"};
        constexpr std::array flow_names{"pregame_menu","title","ex_pregame_menu","intro",
            "controls_type","controls_choice","training","planet_select","planet_travel",
            "gameplay","stage_results","game_over","continue_choice","credits","finished"};
        static_assert(static_cast<unsigned>(starfox::simulation::GameFlowState::finished)+1==flow_names.size());
        std::optional<std::array<unsigned,6>> last_background;
        for(unsigned frame=0;frame<=preflight_frames;++frame) {
            if(host.stop_requested && host.stop_requested()) return 0;
            const auto advanced=live->driver->advance(XrTime(frame)*50'000'000,{},true);
            ticks+=advanced.logic_ticks;
            if(live->pack) {
                msu_playing_samples+=live->msu_audio.playing();
                msu_last_track=live->msu_audio.selected_track();
                if(advanced.audio_blocks && live->msu_audio.playing()) {
                    ++msu_pcm_blocks;
                    for(const auto sample:live->mixed)
                        msu_pcm_peak=std::max(msu_pcm_peak,
                            static_cast<unsigned>(std::abs(int(sample))));
                }
            }
            ++flow_samples.at(static_cast<unsigned>(live->history->current()->flow));
            if(ray_audit) {
                auto assembled=ray_models.assemble_world_interpolated(*live->history->previous(),*live->history->current(),.5,false,true);
                if(!assembled.pending.empty()) throw std::runtime_error("Ray audit model assembly incomplete");
                const auto report=[&](uint32_t key,const char* reason,uint32_t flags=0,uint32_t mode=0) {
                    uint32_t strategy=0,shape=0;
                    for(const auto& object:live->history->current()->objects) if(object.handle==(key&65535)) {
                        strategy=object.object.strategy_address;shape=object.presentation.shape;break;
                    }
                    // Pool slots are recycled. A later strategy/material or a
                    // different rejection in that slot is a new audit finding.
                    const auto signature=std::to_string(key)+":"+std::to_string(strategy)+":"+std::to_string(shape)
                        +":"+reason+":"+std::to_string(flags)+":"+std::to_string(mode);
                    if(ray_reported.emplace(signature,true).second) {
                        std::cout<<"ray-audit: tick="<<ticks<<" key="<<key<<" reason="<<reason<<" flags="<<flags<<" mode="<<mode;
                        for(const auto& object:live->history->current()->objects) if(object.handle==(key&65535))
                            std::cout<<" strategy="<<object.object.strategy_address<<" shape="<<object.presentation.shape;
                        std::cout<<'\n';
                    }
                };
                // Inventory before the shadow policy removes nonphysical visual
                // layers (dust/grid/indicators). Otherwise their CPU producers
                // disappear from the migration audit even though they still draw.
                for(size_t i=0;i<assembled.packets.size();++i) {
                    const auto key=assembled.handles[i];
                    if(is_source_shadow_pass(key)) continue;
                    const auto vertices=assembled.packets[i].geometry.vertex_view();
                    const auto lines=assembled.packets[i].geometry.line_view();
                    const auto route=packet_route(vertices,lines);
                    if(route==PacketRoute::empty) continue;
                    ++route_counts[static_cast<size_t>(route)];
                    const auto& first=vertices.empty()?lines.front():vertices.front();
                    report(key,packet_route_name(route),first.texture[3],first.visibility_enabled);
                }
                ray_policy.apply(assembled,*live->history->current());
                for(const auto& fallback:assembled.compute_fallbacks)
                    report(fallback.key,fallback.reason.c_str());
                for(const auto& model:assembled.compute_models) {
                    if(is_source_shadow_pass(model.key)) continue;
                    VulkanComputeRayScene::Plan plan;SourceRayCoverage materials;
                    const bool accepted=VulkanComputeRayScene::plan(std::span(&model,1),256,plan)
                        && VulkanComputeRayScene::coverage(std::span(&model,1),plan,materials);
                    ++ray_counts[accepted?0:1];
                    if(!accepted) report(model.key,model.axis?"axis":model.warp?"textured-warp":"material/topology");
                }
                for(size_t i=0;i<assembled.packets.size();++i) {
                    const auto key=assembled.handles[i];
                    if(is_source_shadow_pass(key)) continue;
                    const auto vertices=assembled.packets[i].geometry.vertex_view();
                    const auto lines=assembled.packets[i].geometry.line_view();
                    const auto route=packet_route(vertices,lines);
                    if(route==PacketRoute::empty) continue;
                    if(key==0x20000 || key==0x30000) continue;
                    const bool ordinary=route==PacketRoute::ordinary;
                    // Whole-object sprites intentionally bypass native face/BSP
                    // producers. Their corners are expanded by the GPU vertex
                    // shader; do not report them as unmigrated ordinary meshes.
                    const bool sprite=route==PacketRoute::sprite;
                    if(route==PacketRoute::particle || route==PacketRoute::text) continue;
                    ++ray_counts[sprite?4:ordinary?2:3];
                }
                continue;
            }
            if(background_audit) {
                const auto& snapshot=*live->history->current();
                const auto policy=snapshot.ppu->tunnel_scene?"tunnel":snapshot.background_landscape?"landscape":
                    snapshot.background_star_sphere?"stars":snapshot.background_space_horizon?"space-horizon":
                    snapshot.background_water_surround?"water":snapshot.background_unique_top_rows?"unique":"flat";
                const std::array<unsigned,6> state{snapshot.background_id,unsigned(snapshot.ppu->background_mode),
                    unsigned(snapshot.ppu->tunnel_scene),unsigned(snapshot.flow),
                    unsigned(snapshot.shadows_enabled),unsigned(std::uint8_t(snapshot.dots_mode))};
                if(!last_background || *last_background!=state) {
                    std::cout<<"background-audit: tick="<<ticks<<" id="<<state[0]<<" mode="<<state[1]
                        <<" tunnel="<<state[2]<<" flow="<<state[3]<<" shadows="<<state[4]
                        <<" dots="<<int(snapshot.dots_mode)<<" policy="<<policy<<'\n';
                    last_background=state;
                }
                continue;
            }
            for(const auto& object:live->history->current()->objects) {
                const auto& pose=object.source_pose;
                const std::array<bool,5> active{bool(pose.colour_warp),bool(pose.wireframe_mode),
                    bool(pose.wobble_mode),bool(pose.wave_mode),bool(pose.cel_mode)};
                for(std::size_t i=0;i<active.size();++i) effect_samples[i]+=active[i];
            }
            for(double alpha:{0.,0.5,1.}) {
                auto assembled=live->models.assemble_world_interpolated(*live->history->previous(),*live->history->current(),alpha,true);
                if(!assembled.pending.empty()) {
                    for(const auto& issue:assembled.pending) std::cerr<<"Preflight frame "<<frame<<" alpha "<<alpha<<" object "<<issue.handle<<": "<<issue.reason<<'\n';
                    return 9;
                }
                if(verify_geometry_cache) {
                    const auto expected=reference.assemble_world_interpolated(*live->history->previous(),*live->history->current(),alpha,true);
                    if(!expected.pending.empty() || expected.handles!=assembled.handles
                        || !starfox::vr::same_draw_geometry(expected.packets,assembled.packets))
                        throw std::runtime_error("Cached/uncached GPU data mismatch at frame "+std::to_string(frame));
                    for(std::size_t i=0;i<assembled.packets.size();++i)
                        if(assembled.packets[i].model!=expected.packets[i].model)
                            throw std::runtime_error("Cached/uncached model transform mismatch at frame "+std::to_string(frame));
                }
                packets+=assembled.packets.size();
            }
        }
        if(background_audit) std::cout<<"Background inventory: "<<ticks<<" ticks, level "<<preflight_level<<". No model/GPU verification.\n";
        else if(!ray_audit) std::cout<<"VR world preflight (models, dust, grid): "<<ticks<<" logic ticks, "<<packets<<" packets, 3 interpolation samples/frame, level "<<preflight_level<<". No GPU rendering or headset verification.\n";
        if(live->pack) std::cout<<"MSU preflight: selected track="<<msu_last_track
            <<" playing samples="<<msu_playing_samples<<'/'<<(preflight_frames+1)
            <<" PCM blocks="<<msu_pcm_blocks<<" peak="<<msu_pcm_peak
            <<" (audible device output not tested)\n";
        if(ray_audit) {
            for(size_t i=1;i<route_counts.size();++i)
                std::cout<<"Producer inventory: "<<packet_route_name(static_cast<PacketRoute>(i))<<"="<<route_counts[i]<<'\n';
            std::cout<<"Ray input audit: compute accepted="<<ray_counts[0]<<" rejected="<<ray_counts[1]
                <<" legacy ordinary="<<ray_counts[2]<<" procedural="<<ray_counts[3]
                <<" whole-object sprites="<<ray_counts[4]<<"; object/frame counts, no GPU verification.\n";
            return 0;
        }
        if(background_audit) return 0;
        if(verify_geometry_cache) std::cout<<"All cached model data and transforms match uncached assembly.\n";
        for(unsigned flow=0;flow<flow_samples.size();++flow) if(flow_samples[flow])
            std::cout<<"  "<<flow_names[flow]<<": "<<flow_samples[flow]<<" sampled states\n";
        // Zero coverage must be visible: a successful stage run cannot prove
        // support for an EX effect that never appeared during that fixture.
        for(std::size_t i=0;i<effect_samples.size();++i)
            std::cout<<"  Effect flag "<<effect_names[i]<<": "<<effect_samples[i]
                <<" object-tick samples (flags alone do not prove affected solid-face coverage)\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"VR preflight failed: "<<error.what()<<'\n';return 9;}
    starfox::vr::DrawPacket model_packet;
    auto& model_batch=model_packet.geometry;
    if(render_model) try {
        const auto rom=starfox::assets::RomImage::load(model_rom);
        const auto symbols=starfox::assets::SymbolMap::load(model_symbols);
        const starfox::assets::ShapeDecoder decoder(rom,symbols);
        const auto shape=decoder.decode_by_name(symbols,model_name);
        starfox::vr::ShapeMesh mesh;std::string error;
        if(!starfox::vr::decode_shape_mesh(shape,0,1,mesh,error)) throw std::runtime_error(error);
        float radius=0;
        for(const auto& vertex:mesh.vertices) for(float coordinate:vertex.position) radius=std::max(radius,std::abs(coordinate));
        if(radius<=0) throw std::runtime_error("Empty model bounds");
        const float scale=.9F/radius;
        starfox::render::RenderPose placement;placement.z=2/scale;
        starfox::render::Palette256 palette{};
        for(unsigned i=0;i<256;++i) palette[i]={uint8_t((i&15)*17),uint8_t((i&15)*17),uint8_t((i&15)*17),255};
        placement.use_source_lighting_state=true;placement.source_depth=0;
        if(!starfox::vr::build_draw_packet(shape,placement,palette,0,1,false,1/scale,model_packet,error)) throw std::runtime_error(error);
        if(!model_batch.deferred.empty()) throw std::runtime_error("Model contains unsupported primitives");
        if(model_batch.vertices.empty() && model_batch.line_vertices.empty()) throw std::runtime_error("Model has no drawable primitives");
        std::cout<<"Decoded "<<model_name<<": "<<model_batch.vertices.size()/3<<" triangles, "<<model_batch.line_vertices.size()/2<<" lines; diagnostic grayscale palette\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
    // Declaration order guarantees sessions/devices are destroyed before the
    // XR instance and dynamic Vulkan library on every early return.
    starfox::vr::VulkanLoader loader;
    if(graphics || loader_only) {
        if(!loader.initialize()) {std::cerr<<loader.status()<<'\n';return 3;}
        std::cout<<loader.status()<<'\n';
        if(loader_only) {
            auto get=loader.get_instance_proc_addr();
            if(!get(nullptr,"vkCreateInstance")) {std::cerr<<"Vulkan instance entry point missing\n";return 3;}
            std::cout<<"Vulkan loader check passed; no headset or graphics session tested.\n";return 0;
        }
    }
    starfox::vr::OpenXrRuntime runtime;
    if(!runtime.initialize(host.android)) {std::cerr<<runtime.status()<<'\n';return 2;}
    std::cout<<runtime.status()<<'\n';
    unsigned eye=0;
    for(const auto& view:runtime.views()) std::cout<<"Eye "<<eye++<<": "
        <<view.recommendedImageRectWidth<<'x'<<view.recommendedImageRectHeight
        <<", recommended samples "<<view.recommendedSwapchainSampleCount<<'\n';
    if(!graphics) {
        std::cout<<"Runtime discovery passed; use --graphics to check the Vulkan session and eye images.\n";return 0;
    }
    starfox::vr::VulkanDevice device;
    if(!device.initialize(runtime.instance(),runtime.system(),loader.get_instance_proc_addr())) {
        std::cerr<<device.status()<<'\n';return 4;
    }
    std::cout<<device.status()<<'\n';
    starfox::vr::OpenXrSession session;
    if(!session.initialize(runtime.instance(),runtime.system(),&device.binding())) {
        std::cerr<<session.status()<<'\n';return 5;
    }
    starfox::vr::OpenXrInput input;
    if(!input.initialize(runtime.instance(),session.handle())) {std::cerr<<input.status()<<'\n';return 5;}
    starfox::vr::OpenXrSwapchains swapchains;
    constexpr std::array<int64_t,4> formats{VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM};
    if(!swapchains.initialize(session.handle(),runtime.views(),formats)) {
        std::cerr<<swapchains.status()<<'\n';return 6;
    }
    std::array<std::vector<VkImage>,2> eye_images;
    std::array<VkExtent2D,2> eye_extents{};
    for(unsigned eye_index=0;eye_index<2;++eye_index) {
        const auto capacity=swapchains.image_count(eye_index);
        if(capacity==0 || capacity>4096) {std::cerr<<"Invalid eye image count\n";return 6;}
        std::vector<XrSwapchainImageVulkan2KHR> images(capacity,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
        uint32_t count{};
        if(!swapchains.enumerate_images(eye_index,capacity,&count,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())) || count==0 || count>capacity) {
            std::cerr<<"Eye image enumeration failed: "<<swapchains.status()<<'\n';return 6;
        }
        for(uint32_t i=0;i<count;++i) if(!images[i].image) {std::cerr<<"Null Vulkan eye image\n";return 6;}
        for(uint32_t i=0;i<count;++i) eye_images[eye_index].push_back(images[i].image);
        eye_extents[eye_index]={runtime.views()[eye_index].recommendedImageRectWidth,
            runtime.views()[eye_index].recommendedImageRectHeight};
        std::cout<<"Eye "<<eye_index<<": "<<count<<" Vulkan swapchain images, format "<<swapchains.format()<<'\n';
    }
    starfox::vr::VulkanDepthTargets depth;
    if(!depth.initialize(device.binding().instance,device.binding().physicalDevice,device.binding().device,
        loader.get_instance_proc_addr(),eye_extents)) {std::cerr<<depth.status()<<'\n';return 7;}
    starfox::vr::VulkanEyeTargets targets;
    const auto get_device=reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        loader.get_instance_proc_addr()(device.binding().instance,"vkGetDeviceProcAddr"));
    const std::array<std::span<const VkImage>,2> target_images{eye_images[0],eye_images[1]};
    if(!targets.initialize(device.binding().device,get_device,static_cast<VkFormat>(swapchains.format()),
        target_images,eye_extents,depth.format(),depth.views())) {std::cerr<<targets.status()<<'\n';return 7;}
    starfox::vr::VulkanPipelineCache shader_cache;
    starfox::vr::VulkanDrawPackets scene;
    starfox::vr::VulkanSourceScene compute_scene;
    bool compute_scene_active=false;
    std::array<std::unique_ptr<VulkanDxrFrame>,2> ray_frames;
    std::array<std::unique_ptr<VulkanMixedRayScene>,2> ray_scenes;
    std::array<std::optional<XrTime>,2> ray_times;
    std::array<uint64_t,2> ray_sizes{};
    VulkanSpanPipeline ray_pipeline;
    SourceModelPackets ray_inputs;
    StereoRayPlan ray_plan_cache;
    std::array<render::shadows::DxrShadows::Coverage,2> ray_coverage_views;
    bool ray_pipeline_ready=false;
    std::optional<render::shadows::ReceiverPlane> ray_ground;
    bool ray_environment_valid=false;
    render::shadows::Vec3 ray_light{-1,1,1};
    const auto ray_properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        loader.get_instance_proc_addr()(device.binding().instance,"vkGetPhysicalDeviceProperties2"));
    bool ray_supported=device.external_shadows_enabled() && device.adapter_luid().has_value() && ray_properties;
    if(ray_supported) {render::shadows::DxrShadows probe(*device.adapter_luid());ray_supported=probe.available();}
    starfox::vr::VulkanDrawPackets sprites;
    starfox::vr::VulkanDrawPackets backgrounds;
    starfox::vr::VulkanDrawPackets tunnel_surround;
    starfox::vr::VulkanDrawPackets surrounding_stars;
    starfox::vr::VulkanDrawPackets startup_panel;
    starfox::vr::VulkanDrawPackets sandbox_pointer;
    starfox::vr::PauseSandbox sandbox;
    for(auto* packets:{&scene,&sprites,&backgrounds,&tunnel_surround,&surrounding_stars,&startup_panel})
        packets->set_pipeline_cache(&shader_cache);
    starfox::vr::VulkanScenePipeline pipeline;
    if(render_triangle && !pipeline.initialize(device.binding().device,get_device,targets.render_pass(),true)) {
        std::cerr<<pipeline.status()<<'\n';return 7;
    }
    if(!render_clear && !render_triangle && !render_model && !render_game) {
        std::cout<<"Graphics session, eye images and render targets ready. No frames rendered; this is not playable VR.\n";
        return 0;
    }
    starfox::vr::VulkanSceneBuffer vertices;
    VkPhysicalDeviceMemoryProperties properties{};
    VkPhysicalDeviceProperties physical_properties{};
    if(render_triangle || render_model || render_game) {
        const auto get_memory=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
            loader.get_instance_proc_addr()(device.binding().instance,"vkGetPhysicalDeviceMemoryProperties"));
        if(!get_memory) {std::cerr<<"Missing physical memory query\n";return 8;}
        get_memory(device.binding().physicalDevice,&properties);
        const auto get_properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
            loader.get_instance_proc_addr()(device.binding().instance,"vkGetPhysicalDeviceProperties"));
        if(!get_properties) {std::cerr<<"Missing physical device limits query\n";return 8;}
        get_properties(device.binding().physicalDevice,&physical_properties);
        if(!shader_cache.initialize(device.binding().device,get_device,physical_properties,
            host.cartridge_save_path.empty()?std::filesystem::path{}:host.cartridge_save_path.parent_path()/"shader-cache"))
            std::cerr<<"Optional Vulkan pipeline cache unavailable; compiling normally\n";
        const std::array<starfox::vr::SceneVertex,3> triangle{{
            {{-.4F,-.3F,-2.F},{.1F,.8F,.2F,1.F}},
            {{.4F,-.3F,-2.F},{.1F,.8F,.2F,1.F}},
            {{0,.4F,-2.F},{.1F,.8F,.2F,1.F}}}};
        if(render_triangle && !vertices.initialize(device.binding().device,get_device,properties,triangle)) {
            std::cerr<<vertices.status()<<'\n';return 8;
        }
        if(render_model) {
            const bool srgb=swapchains.format()==VK_FORMAT_R8G8B8A8_SRGB || swapchains.format()==VK_FORMAT_B8G8R8A8_SRGB;
            const auto linear=[](float x) {return x<=.04045F?x/12.92F:std::pow((x+.055F)/1.055F,2.4F);};
            if(srgb) for(auto* batch:{&model_batch.vertices,&model_batch.line_vertices}) for(auto& vertex:*batch) {
                for(unsigned channel=0;channel<3;++channel) {vertex.color[channel]=linear(vertex.color[channel]);vertex.odd_color[channel]=linear(vertex.odd_color[channel]);}
                if(vertex.texture[3]&1) vertex.texture[3]|=2;
            }
            if(!scene.initialize(device.binding().device,get_device,properties,targets.render_pass(),
                std::span<const starfox::vr::DrawPacket>(&model_packet,1))) {std::cerr<<scene.status()<<'\n';return 8;}
        }
    }
    starfox::vr::VulkanEyeCommands commands;
    if(live && !live->output.open()) {std::cerr<<live->output.status()<<'\n';return 8;}
    if(!commands.initialize(device.binding().device,device.queue(),device.binding().queueFamilyIndex,get_device)) {
        std::cerr<<commands.status()<<'\n';return 8;
    }
    starfox::vr::VulkanStereoDraw draw(commands,targets);
    starfox::vr::StereoRenderer renderer(session,swapchains,render_game);
    const auto started=std::chrono::steady_clock::now();
    unsigned submitted=0;
    std::optional<XrTime> input_time;unsigned menu_presses=0;
    std::string game_error;
    std::vector<starfox::vr::DrawPacket> uploaded_packets;
    std::vector<uint32_t> uploaded_handles;
    std::vector<starfox::vr::Matrix4> model_updates;
    unsigned scene_uploads=0,model_only_updates=0;
    std::size_t gpu_uploads=0,gpu_reuses=0;
    std::optional<uint64_t> sprite_revision;
    std::optional<bool> uploaded_enhanced_sky;
    std::vector<starfox::vr::DrawPacket> uploaded_sprites;
    std::vector<starfox::vr::DrawPacket> uploaded_backgrounds;
    unsigned sprite_uploads=0,sprite_reuses=0;
    bool cancelled=false;
    starfox::vr::StartupMenu startup;
    startup.ray_tracing_available=ray_supported;startup.ray_tracing=ray_tracing;
    starfox::vr::VulkanScenePipeline circle_pipeline;
    starfox::vr::VulkanSceneBuffer circle_vertices;
    std::optional<starfox::vr::SceneBlend> circle_blend;
    starfox::vr::Matrix4 circle_model{};
    bool circle_active=false,circle_over_hud=false;
    starfox::vr::VulkanScenePipeline shutter_pipeline;
    starfox::vr::VulkanSceneBuffer shutter_vertices;
    bool shutter_ready=false,shutter_active=false;
    startup.open=live && live->game.flow_state()==starfox::simulation::GameFlowState::intro;
    const bool initial_extended=live && live->game.peek_meter_state().extended;
    startup.extended=initial_extended;
    if(live) {
        startup.language=live->game.language();startup.god_mode=live->game.god_mode();
        startup.default_laser=live->game.default_laser();
        startup.msu_available=live->game.msu1_available();startup.msu_music=live->game.msu1_music();
        startup.music_volume=live->game.music_volume();startup.sfx_volume=live->game.sfx_volume();
        startup.unlocked_pace=live->game.timing_mode()==starfox::simulation::TimingMode::unlocked_20_fps;
        startup.crosshair_colour=unsigned(live->game.crosshair_colour());
        startup.swap_face_buttons=live->game.swap_face_buttons();
        startup.infinite_bombs=live->game.infinite_bombs();startup.infinite_boost=live->game.infinite_boost();
        startup.infinite_lives=live->game.infinite_lives();
    }
    const auto input_directory=model_rom?std::filesystem::path(model_rom).parent_path():std::filesystem::path{};
    const auto alternate_rom=input_directory/(initial_extended?"Original.SFC":"SFES.SFC");
    const auto alternate_symbols=input_directory/(initial_extended?"Original-SYMBOLS.TXT":"SFES-SYMBOLS.TXT");
    startup.alternate_available=live && (bundle.has_value() || (std::filesystem::is_regular_file(alternate_rom)
        && std::filesystem::is_regular_file(alternate_symbols)));
    const auto collect_levels=[&](const auto& symbols,bool extended) {
        auto& choices=startup.level_choices[extended?1:0];choices={0};
        for(unsigned route=1;route<=(extended?7U:3U);++route)
            for(unsigned stage=1;stage<=9;++stage)
                if(!symbols.find("LEVEL"+std::to_string(route)+"_"+std::to_string(stage)).empty())
                    choices.push_back(route*10+stage);
    };
    if(live) collect_levels(live->symbols,initial_extended);
    if(startup.alternate_available)
        collect_levels(bundle?starfox::assets::SymbolMap::parse(bundle->starfox_ex_symbols)
            :starfox::assets::SymbolMap::load(alternate_symbols),!initial_extended);
    const auto preferences_path=host.cartridge_save_path.empty()?std::filesystem::path{}
        :host.cartridge_save_path.parent_path()/"vr-preferences.bin";
    if(startup.open && !preferences_path.empty()) try {
        if(std::filesystem::exists(preferences_path)) {
            if((std::filesystem::file_size(preferences_path)!=16 && std::filesystem::file_size(preferences_path)!=20)
                || !startup.restore_preferences(starfox::state::read_file(preferences_path)))
                std::cerr<<"Invalid VR preferences; using defaults\n";
        }
    } catch(const std::exception& error) {
        std::cerr<<"VR preferences could not load: "<<error.what()<<'\n';
    }
    // Explicit launch request takes precedence over a saved OFF preference.
    // Hardware availability still gates use; no flag keeps the saved/default value.
    if(ray_tracing) startup.ray_tracing=true;
    if(enhanced_sky) startup.enhanced_sky=true;
    if(live && live->pack)
        std::cerr<<"VR MSU-1 pack ready; startup music setting "
            <<(startup.msu_music?"ON":"OFF")<<'\n';
    auto saved_preferences=startup.preferences();
    bool startup_release=startup.open;
    std::unique_ptr<LiveGame> preview_game,parked_game;
    const LiveGame* last_render_game{};
    std::optional<unsigned> startup_revision;
    auto last_profile=std::chrono::steady_clock::now();
    FrameWait frame_wait;
    while((!host.frame_limit || submitted<host.frame_limit)
          && (host.time_limit.count()==0 || std::chrono::steady_clock::now()-started<host.time_limit)
          && !session.exit_requested()) {
        if(host.stop_requested && host.stop_requested()) {cancelled=true;break;}
        const auto result=renderer.step_async([&](unsigned eye,uint32_t image,const auto& camera,XrTime time) {
            if(!input_time || time!=*input_time) {
                // Both eyes share the preview; restore the real session only
                // at the next frame boundary, before processing menu input.
                if(parked_game) {preview_game=std::move(live);live=std::move(parked_game);}
                if(!input.poll(session.state()==XR_SESSION_STATE_FOCUSED)) return starfox::vr::StereoRenderer::EyeResult::failed;
                auto controls=input.controls();
                if(host.desktop_controls && session.state()==XR_SESSION_STATE_FOCUSED) {
                    const auto pad=host.desktop_controls();
                    if(std::hypot(pad.steer.x,pad.steer.y)>std::hypot(controls.steer.x,controls.steer.y))
                        controls.steer=pad.steer;
                    controls.fire|=pad.fire;controls.bomb|=pad.bomb;
                    controls.boost|=pad.boost;controls.brake|=pad.brake;
                    controls.menu|=pad.menu;controls.menu_pressed|=pad.menu_pressed;
                    controls.roll_left|=pad.roll_left;controls.roll_right|=pad.roll_right;
                    controls.select|=pad.select;controls.select_pressed|=pad.select_pressed;
                    controls.stick_left|=pad.stick_left;controls.stick_right|=pad.stick_right;
                    controls.reset_pressed|=pad.reset_pressed;
                }
                input_time=time;if(controls.menu_pressed) ++menu_presses;
                if(live) try {
                    const auto profile_start=std::chrono::steady_clock::now();
                    const bool focused=session.state()==XR_SESSION_STATE_FOCUSED;
                    if(focused && controls.reset_pressed) {
                        sandbox.cancel();
                        auto restarted=bundle?std::make_unique<LiveGame>(
                            starfox::assets::RomImage(initial_extended?bundle->starfox_ex_rom:bundle->original_rom),
                            starfox::assets::SymbolMap::parse(initial_extended?bundle->starfox_ex_symbols:bundle->original_symbols),msu_path,host,"INTROMAP")
                            :std::make_unique<LiveGame>(model_rom,model_symbols,msu_path,host,"INTROMAP");
                        live->output.close();
                        if(!restarted->output.open()) throw std::runtime_error(restarted->output.status());
                        live=std::move(restarted);preview_game.reset();
                        startup.extended=initial_extended;startup.runtime=false;startup.open=true;
                        startup.preview=false;startup.page=StartupMenu::Page::main;startup.selection=0;
                        startup.selected_level=0;++startup.revision;startup_release=true;
                        sprite_revision.reset();uploaded_backgrounds.clear();uploaded_sprites.clear();
                    }
                    if(focused && !startup.open && !startup_release && controls.menu && controls.select
                        && (controls.menu_pressed || controls.select_pressed)) {
                        startup.language=live->game.language();startup.god_mode=live->game.god_mode();
                        startup.default_laser=live->game.default_laser();
                        startup.msu_available=live->game.msu1_available();startup.msu_music=live->game.msu1_music();
                        startup.music_volume=live->game.music_volume();startup.sfx_volume=live->game.sfx_volume();
                        startup.unlocked_pace=live->game.timing_mode()==starfox::simulation::TimingMode::unlocked_20_fps;
                        startup.crosshair_colour=unsigned(live->game.crosshair_colour());
                        startup.swap_face_buttons=live->game.swap_face_buttons();
                        startup.infinite_bombs=live->game.infinite_bombs();startup.infinite_boost=live->game.infinite_boost();
                        startup.infinite_lives=live->game.infinite_lives();
                        startup.extended=live->game.peek_meter_state().extended;
                        startup.open_runtime();startup_release=true;
                    }
                    const bool was_menu=startup.open;
                    startup.sample(controls,focused);
                    if(was_menu && !startup.open && !preferences_path.empty()
                        && startup.preferences()!=saved_preferences) try {
                        const auto bytes=startup.preferences();
                        starfox::state::write_atomic(preferences_path,bytes);
                        saved_preferences=bytes;
                    } catch(const std::exception& error) {
                        std::cerr<<"VR preferences could not save: "<<error.what()<<'\n';
                    }
                    if(was_menu && !startup.open && !startup.runtime && startup.extended!=initial_extended) {
                        const auto rom_name=alternate_rom.string(),symbol_name=alternate_symbols.string();
                        auto selected=bundle?std::make_unique<LiveGame>(starfox::assets::RomImage(bundle->starfox_ex_rom),
                            starfox::assets::SymbolMap::parse(bundle->starfox_ex_symbols),msu_path,host,"INTROMAP")
                            :std::make_unique<LiveGame>(rom_name.c_str(),symbol_name.c_str(),msu_path,host,"INTROMAP");
                        if(selected->game.peek_meter_state().extended!=startup.extended)
                            throw std::runtime_error("Selected VR experience files contain the wrong cartridge");
                        live->output.close();
                        if(!selected->output.open()) throw std::runtime_error(selected->output.status());
                        live=std::move(selected);
                        sprite_revision.reset();uploaded_backgrounds.clear();uploaded_sprites.clear();
                    }
                    if(was_menu) {
                        live->game.set_language(uint8_t(startup.language));
                        live->game.set_god_mode(startup.god_mode);
                        live->game.set_music_volume(uint8_t(startup.music_volume));
                        live->game.set_msu1_music(startup.msu_music);
                        live->game.set_timing_mode(startup.unlocked_pace?starfox::simulation::TimingMode::unlocked_20_fps
                            :starfox::simulation::TimingMode::original_speed);
                        live->game.set_sfx_volume(uint8_t(startup.sfx_volume));
                        live->game.set_crosshair_colour(static_cast<starfox::simulation::CrosshairColour>(startup.crosshair_colour));
                        live->game.set_swap_face_buttons(startup.swap_face_buttons);
                        live->game.set_infinite_bombs(startup.infinite_bombs);
                        live->game.set_infinite_lives(startup.infinite_lives);
                        live->game.set_infinite_boost(startup.infinite_boost);
                        if(live->game.default_laser()!=startup.default_laser)
                            live->game.set_default_laser(uint8_t(startup.default_laser));
                    }
                    if(was_menu && !startup.open && startup.selected_level) {
                        sandbox.cancel();
                        live->game.set_selected_level(uint8_t(startup.selected_level));
                        if(!live->game.launch_selected_level()) throw std::runtime_error("VR selected level could not launch");
                        live->history->capture();live->driver->reset_for_scene_change();
                        live->output.close();
                        if(!live->output.open()) throw std::runtime_error(live->output.status());
                        startup.selected_level=0;
                    }
                    if(!startup.open && !controls.fire && !controls.menu
                        && !controls.select
                        && !controls.bomb && !controls.boost && !controls.brake)
                        startup_release=false;
                    const bool playing=focused && !startup.open && !startup_release;
                    if(!live->output.set_active(playing)) throw std::runtime_error(live->output.status());
                    // A new timestamp starts only after both previous eye
                    // submissions completed. Repeated eye/fence callbacks
                    // retain this exact simulation state and uploaded scene.
                    const bool was_paused=live->game.paused();
                    auto game_controls=playing?startup.gameplay_controls(controls):starfox::vr::VrControls{};
                    if(was_paused) {
                        game_controls.roll_left=game_controls.roll_right=false;
                        game_controls.fire=game_controls.bomb=game_controls.boost=game_controls.brake=false;
                        game_controls.steer={};game_controls.select=game_controls.select_pressed=false;
                    }
                    const auto advance=live->driver->advance(time,game_controls,playing);
                    if(!live->game.paused() && sandbox.active()) {
                        sandbox.commit(live->game.objects());live->history->capture();
                    }
                    live->logic_ticks+=advance.logic_ticks;
                    if(startup.open && startup.preview && !startup.runtime) {
                        if(!preview_game || preview_game->game.peek_meter_state().extended!=startup.extended) {
                            // Private, silent checkpoint: no save writes and no
                            // progression in the player's actual intro/session.
                            if(startup.extended!=live->game.peek_meter_state().extended) {
                                preview_game=bundle?std::make_unique<LiveGame>(starfox::assets::RomImage(bundle->starfox_ex_rom),
                                    starfox::assets::SymbolMap::parse(bundle->starfox_ex_symbols),nullptr,host,"LEVEL1_1",true)
                                    :std::make_unique<LiveGame>(alternate_rom.string().c_str(),alternate_symbols.string().c_str(),nullptr,host,"LEVEL1_1",true);
                            } else preview_game=std::make_unique<LiveGame>(live->rom,live->symbols,nullptr,host,"LEVEL1_1",true);
                        }
                        parked_game=std::move(live);live=std::move(preview_game);
                    }
                    if(last_render_game!=live.get()) {
                        sprite_revision.reset();uploaded_backgrounds.clear();uploaded_sprites.clear();
                        last_render_game=live.get();
                    }
                    const auto profile_logic=std::chrono::steady_clock::now();
                    const auto alpha=live->game.paused() || session.state()!=XR_SESSION_STATE_FOCUSED?1.:live->game.logic_interpolation_alpha(advance.raster_fraction);
                    const bool srgb=swapchains.format()==VK_FORMAT_R8G8B8A8_SRGB || swapchains.format()==VK_FORMAT_B8G8R8A8_SRGB;
                    if(startup.open && (!startup_revision || *startup_revision!=startup.revision)) {
                        std::array<uint16_t,256> palette{};palette[1]=0x7fff;palette[2]=0x03ff;
                        std::vector<starfox::vr::DrawPacket> rows;
                        const auto add=[&](std::string_view text,int y,uint8_t ink) {
                            auto row=starfox::vr::source_ui_text_packet(live->rom,live->symbols,text,16,y,240,ink,palette,15,srgb);
                            row.model=starfox::vr::source_layer_matrix(128,112,2.F).value();rows.push_back(std::move(row));
                        };
                        if(startup.language>=1 && startup.language<=4) {
                            const auto add_unicode=[&](std::u32string_view text,int y,uint8_t ink) {
                                auto row=starfox::vr::source_unicode_ui_text_packet(live->rom,live->symbols,text,16,y,ink,palette,15,srgb);
                                row.model=starfox::vr::source_layer_matrix(128,112,2.F).value();rows.push_back(std::move(row));
                            };
                            const auto labels=startup.localized_labels();
                            add_unicode(startup.translate(startup.title()),35,1);
                            const auto first=startup.first_visible_row();
                            for(unsigned i=first;i<labels.size() && i<first+6;++i) {
                                add_unicode((i==startup.selection?U"> ":U"  ")+labels[i],67+int(i-first)*18,i==startup.selection?2:1);
                            }
                            const auto help=startup.localized_help();add_unicode(help[0],183,1);add_unicode(help[1],201,1);
                        } else {
                            add(startup.title(),35,1);
                            const auto labels=startup.labels();
                            const auto first=startup.first_visible_row();
                            for(unsigned i=first;i<labels.size() && i<first+6;++i) add((i==startup.selection?"> ":"  ")+labels[i],67+int(i-first)*18,i==startup.selection?2:1);
                            add("STICK: MOVE   FIRE: SELECT",185,1);
                        }
                        if(!startup_panel.initialize(device.binding().device,get_device,properties,targets.render_pass(),rows,{},false))
                            throw std::runtime_error(startup_panel.status());
                        startup_revision=startup.revision;
                    }
                    // The startup/runtime panel replaces the scene. Do not
                    // compile or upload invisible game resources while it is open.
                    if(!startup.open || startup.preview) {
                    auto packets=live->models.assemble_world_interpolated(*live->history->previous(),*live->history->current(),alpha,srgb,true);
                    if(live->game.paused()) {
                        if(!sandbox.active()) {
                            starfox::vr::SourceModels picking(live->rom,live->symbols,false,false);
                            const auto bounds=picking.assemble(*live->history->current(),srgb);
                            sandbox.begin(bounds,*live->history->current());
                        }
                        auto hands=playing?input.aim_poses(session.space(),time):std::array<std::optional<XrPosef>,2>{};
                        for(auto& hand:hands) if(hand) *hand=renderer.anchored_pose(*hand);
                        auto pointer=sandbox.update(hands,{controls.roll_left,controls.roll_right});
                        sandbox.apply(packets);
                        if(!sandbox_pointer.initialize(device.binding().device,get_device,properties,targets.render_pass(),
                            std::span<const starfox::vr::DrawPacket>(&pointer,1),{},false))
                            throw std::runtime_error(sandbox_pointer.status());
                    }
                    const auto profile_models=std::chrono::steady_clock::now();
                    if(!packets.pending.empty()) throw std::runtime_error("Live model pass incomplete: "+packets.pending.front().reason);
                    // Stars form the distant environment, not depth-writing
                    // foreground objects. Planet art is composited after them.
                    const auto star=std::find(packets.handles.begin(),packets.handles.end(),0x20000U);
                    if(star==packets.handles.end()) throw std::runtime_error("Missing surrounding star pass");
                    const auto star_index=std::size_t(star-packets.handles.begin());
                    if(!surrounding_stars.initialize(device.binding().device,get_device,properties,targets.render_pass(),
                        std::span<const starfox::vr::DrawPacket>(&packets.packets[star_index],1),
                        std::span<const uint32_t>(&packets.handles[star_index],1),false))
                        throw std::runtime_error(surrounding_stars.status());
                    packets.packets.erase(packets.packets.begin()+star_index);
                    packets.handles.erase(star);
                    for(auto& compute:packets.compute_models) {
                        if(compute.packet_index==star_index) throw std::runtime_error("Star pass unexpectedly owns a compute model");
                        if(compute.packet_index>star_index) --compute.packet_index;
                    }
                    if(live->history->current()->flow==starfox::simulation::GameFlowState::gameplay
                        || live->history->current()->flow==starfox::simulation::GameFlowState::training) {
                        for(std::size_t i=0;i<packets.packets.size();++i)
                            if((packets.handles[i]&0xffffU)!=0 && packets.handles[i]<0x20000U)
                                packets.packets[i].model[14]-=.25F;
                    }
                    // Title models retain their authored placement. The former
                    // extra two-unit setback made them unnecessarily distant
                    // relative to the logo and PUSH START artwork in VR.
                    // The ordered source scene also owns legacy-only ray candidates.
                    // Do not disable rays just because this frame has no compute solids.
                    compute_scene_active=!packets.compute_models.empty()
                        || (startup.ray_tracing_enabled()
                            && std::any_of(packets.packets.begin(),packets.packets.end(),[](const auto& packet) {
                                return !packet.geometry.vertex_view().empty();
                            }));
                    if(compute_scene_active) {
                        if(!compute_scene.initialize(device.binding().device,get_device,properties,physical_properties.limits,
                            targets.render_pass(),packets,&shader_cache)) throw std::runtime_error(compute_scene.status());
                        if(startup.ray_tracing_enabled()) {
                            ray_inputs=packets;
                            ray_plan_cache.invalidate();
                            SourceRayPolicy(live->symbols).apply(ray_inputs,*live->history->current());
                            const auto now=live->history->current(),before=live->history->previous();
                            auto fraction=alpha;
                            if(before->flow!=now->flow || timing::camera_transform_is_discontinuous(before->camera,now->camera)) fraction=1.;
                            const auto view=simulation::interpolate_rotation_matrix_q15(before->view_matrix,now->view_matrix,fraction);
                            const auto camera_pose=timing::interpolate(before->camera,now->camera,fraction);
                            const bool gameplay=now->flow==simulation::GameFlowState::gameplay || now->flow==simulation::GameFlowState::training;
                            const auto environment=source_shadow_environment(view,camera_pose.y,now->shadow_height,now->shadows_enabled,gameplay?-.25:0);
                            ray_environment_valid=environment.has_value();
                            if(environment) {ray_ground=environment->ground;ray_light=environment->light;}
                            else ray_ground.reset();
                        }
                    } else if(scene_uploads && uploaded_handles==packets.handles && starfox::vr::same_draw_geometry(uploaded_packets,packets.packets)) {
                        model_updates.clear();model_updates.reserve(packets.packets.size());
                        for(const auto& packet:packets.packets)
                            if(!packet.geometry.vertex_view().empty() || !packet.geometry.line_view().empty()) model_updates.push_back(packet.model);
                        if(!scene.update_models(model_updates)) throw std::runtime_error("Native scene transform update failed");
                        ++model_only_updates;
                        gpu_reuses+=scene.size();
                    } else {
                        if(!scene.initialize(device.binding().device,get_device,properties,targets.render_pass(),packets.packets,packets.handles))
                            throw std::runtime_error(scene.status());
                        uploaded_packets=std::move(packets.packets);++scene_uploads;
                        uploaded_handles=std::move(packets.handles);
                        gpu_uploads+=scene.uploaded_packets();gpu_reuses+=scene.reused_packets();
                    }
                    const auto snapshot=live->history->current();
                    const auto profile_upload=std::chrono::steady_clock::now();
                    auto shutter=starfox::vr::source_shutter_packet(live->history->previous()->wipe,snapshot->wipe,alpha);
                    shutter_active=!shutter.geometry.vertices.empty();
                    if(shutter_active) {
                        if(!shutter_ready) {
                            if(!shutter_pipeline.initialize(device.binding().device,get_device,targets.render_pass(),false,
                                starfox::vr::SceneTopology::triangles,VK_NULL_HANDLE,starfox::vr::SceneBlend::opaque,&shader_cache))
                                throw std::runtime_error(shutter_pipeline.status());
                            shutter_ready=true;
                        }
                        if(!shutter_vertices.initialize(device.binding().device,get_device,properties,shutter.geometry.vertices))
                            throw std::runtime_error(shutter_vertices.status());
                    }
                    auto circle=starfox::vr::source_circle_packet(live->history->previous()->circle,
                        snapshot->circle,alpha,snapshot->display_brightness,srgb);
                    circle_active=!circle.geometry.vertices.empty();
                    if(circle_active) {
                        const auto flags=snapshot->circle.affected_layers;
                        circle_over_hud=(flags&0x10)!=0;
                        const auto blend=(flags&0x80)
                            ?((flags&0x40)?starfox::vr::SceneBlend::half_subtract:starfox::vr::SceneBlend::subtract)
                            :((flags&0x40)?starfox::vr::SceneBlend::half_add:starfox::vr::SceneBlend::add);
                        if(!circle_blend || *circle_blend!=blend) {
                            if(!circle_pipeline.initialize(device.binding().device,get_device,targets.render_pass(),false,
                                starfox::vr::SceneTopology::triangles,VK_NULL_HANDLE,blend,&shader_cache))
                                throw std::runtime_error(circle_pipeline.status());
                            circle_blend=blend;
                        }
                        if(!circle_vertices.initialize(device.binding().device,get_device,properties,circle.geometry.vertices))
                            throw std::runtime_error(circle_vertices.status());
                        circle_model=starfox::vr::source_layer_matrix(float(snapshot->source_vanishing_point[0])+16.F,
                            float(snapshot->source_vanishing_point[1])+16.F).value();
                    }
                    if(!sprite_revision || *sprite_revision!=snapshot->revision
                        || uploaded_enhanced_sky!=startup.enhanced_sky) {
                        if(!snapshot->ppu) throw std::runtime_error("Live sprite pass has no PPU snapshot");
                        auto packet=starfox::vr::source_sprite_packet(*snapshot->ppu,snapshot->display_brightness,{},srgb,&snapshot->meters);
                        // Initial diagnostic HUD plane in LOCAL space, matching
                        // the tested native-coordinate sprite capture. Headset
                        // comfort/layout needs physical validation, not a claim
                        // that this fixed plane is the finished VR HUD.
                        const bool fixed_menu=snapshot->flow==starfox::simulation::GameFlowState::ex_pregame_menu;
                        packet.model=starfox::vr::source_ui_layer_matrix(
                            float(snapshot->source_vanishing_point[0]),
                            float(snapshot->source_vanishing_point[1]),fixed_menu).value();
                        auto meters=starfox::vr::source_meter_packet(snapshot->meters,snapshot->ppu->cgram,snapshot->display_brightness,srgb);
                        // Meters use the inner 224x192 Super FX coordinates;
                        // OAM uses the surrounding 256x224 PPU coordinates.
                        // Omitting the OAM guard from the meter origin adds
                        // the required +16,+16 logical-pixel translation.
                        meters.model=starfox::vr::source_ui_layer_matrix(
                            float(snapshot->source_vanishing_point[0]),
                            float(snapshot->source_vanishing_point[1]),fixed_menu,true).value();
                        starfox::vr::BackgroundTileOptions background_options;
                        background_options.colour_subtract=snapshot->background_colour_subtract;
                        background_options.brightness=snapshot->display_brightness;
                        background_options.scroll_override=snapshot->background_scroll_override;
                        background_options.single_occurrence_top_rows=snapshot->background_unique_top_rows;
                        background_options.ex_twin_planets=snapshot->background_ex_twin_planets;
                        background_options.ex_face_planets=snapshot->background_ex_face_planets;
                        background_options.ex_ocean_island=snapshot->background_ex_ocean_island;
                        background_options.ex_volcanic_horizon=snapshot->background_ex_volcanic_horizon;
                        background_options.ex_city_planets=snapshot->background_ex_city_planets;
                        // Space black is the transparent sky between authored
                        // planet pixels; it must not mask the surrounding stars.
                        background_options.transparent_black=snapshot->background_unique_top_rows!=0;
                        // Mode-2 terrain is a world layer, not a HUD panel. Extend
                        // its logical coordinates without enlarging native pixels
                        // or changing sprite/text placement. The GPU decoder's
                        // wide-margin and single-occurrence policies still apply.
                        if(snapshot->ppu->background_mode==2) {
                            background_options.expanded_horizontal=true;
                            background_options.horizontal_bounds={-384,640};
                        }
                        std::vector<starfox::vr::DrawPacket> next_backgrounds;
                        std::vector<starfox::vr::DrawPacket> surround_packets;
                        if(snapshot->ppu->tunnel_scene) {
                            const auto border=[&](unsigned y,unsigned x) {
                                const auto ink=starfox::render::tunnel_border_index(*snapshot->ppu,y,x);
                                return starfox::vr::source_backdrop_colour(
                                    snapshot->ppu->cgram[ink],snapshot->display_brightness,srgb);
                            };
                            // The side walls retain their authored border ink;
                            // the upper/lower surround continues the actual
                            // ceiling/floor palette instead of making the
                            // entire VR periphery one dark window frame.
                            auto surround=starfox::vr::tunnel_surround_packet(
                                border(112,0),border(4,128),border(219,128));
                            surround.model=starfox::vr::source_layer_matrix(
                                float(snapshot->source_vanishing_point[0])+16.F,
                                float(snapshot->source_vanishing_point[1])+16.F).value();
                            surround_packets.push_back(std::move(surround));
                        }
                        if(!tunnel_surround.initialize(device.binding().device,get_device,properties,
                            targets.render_pass(),surround_packets,{},false))
                            throw std::runtime_error(tunnel_surround.status());
                        const bool intro_sky=!snapshot->ppu->tunnel_scene
                            && snapshot->flow==starfox::simulation::GameFlowState::intro
                            && !snapshot->meters.extended && snapshot->background_unique_top_rows==224;
                        if(intro_sky)
                            next_backgrounds.push_back(starfox::vr::intro_star_sphere_packet(
                                *snapshot->ppu,snapshot->display_brightness,srgb));
                        if(!snapshot->ppu->tunnel_scene && snapshot->background_unique_space)
                            next_backgrounds.push_back(starfox::vr::intro_star_sphere_packet(
                                *snapshot->ppu,snapshot->display_brightness,srgb,false,true));
                        if(snapshot->ppu->background_mode>=1 && snapshot->ppu->background_mode<=2) {
                            auto bg2=starfox::vr::background_tile_packet(*snapshot->ppu,
                                starfox::vr::BackgroundLayer::bg2,background_options,srgb);
                            if(intro_sky)
                                bg2=starfox::vr::intro_planet_packet(*snapshot->ppu,snapshot->display_brightness,srgb);
                            else if(!snapshot->ppu->tunnel_scene && snapshot->background_unique_space)
                                bg2=starfox::vr::unique_planet_packet(*snapshot->ppu,background_options,snapshot->background_planet_rect,srgb);
                            else if(!snapshot->ppu->tunnel_scene && snapshot->background_star_sphere)
                            {
                                bg2=starfox::vr::intro_star_sphere_packet(*snapshot->ppu,snapshot->display_brightness,srgb,true,false,snapshot->background_retain_sky_scroll);
                                if(snapshot->background_ex_face_planets && !bg2.geometry.texels.empty()) bg2.geometry.texels[7]|=256U;
                            }
                            else if(!snapshot->ppu->tunnel_scene && snapshot->background_space_horizon)
                                bg2=snapshot->background_orbital_planet
                                    ?starfox::vr::orbital_planet_sphere_packet(*snapshot->ppu,background_options,srgb,snapshot->background_orbital_thin,snapshot->background_orbital_entry,
                                        snapshot->flow==simulation::GameFlowState::gameplay && live->game.experience()==simulation::Experience::starfox_ex)
                                    :starfox::vr::space_horizon_sphere_packet(*snapshot->ppu,background_options,srgb);
                            else if(!snapshot->ppu->tunnel_scene && snapshot->background_landscape) {
                                bg2=starfox::vr::landscape_sphere_packet(*snapshot->ppu,background_options,
                                    112.F,srgb,snapshot->background_landscape_unique_half,snapshot->landscape_atlas_origin,
                                    snapshot->background_landscape_unique_right_half);
                                if(snapshot->landscape_grid_height<0)
                                    starfox::vr::place_landscape_ground(bg2,std::clamp(float(snapshot->landscape_grid_height)/256.F,-8.F,-.001F),true);
                            }
                            else if(snapshot->background_water_surround)
                                bg2=starfox::vr::water_surface_packet(*snapshot->ppu,background_options,
                                    std::clamp(std::abs(float(snapshot->camera.y-snapshot->shadow_height))/256.F,.01F,8.F),srgb);
                            else bg2.model=packet.model;
                            auto photograph=startup.enhanced_sky?live->enhanced_landscape.prepare(*snapshot,live->game,load_vr_backdrop,srgb)
                                :std::optional<starfox::vr::DrawPacket>{};
                            if(photograph) live->enhanced_landscape.retain_native_ground(bg2,*snapshot->ppu);
                            if(!photograph && snapshot->flow==starfox::simulation::GameFlowState::game_over)
                                next_backgrounds.push_back(starfox::vr::game_over_star_sphere_packet(*snapshot->ppu,
                                    snapshot->display_brightness,snapshot->background_colour_subtract,srgb));
                            if(photograph && snapshot->flow==starfox::simulation::GameFlowState::game_over) {
                                next_backgrounds.push_back(std::move(*photograph));photograph.reset();
                            }
                            next_backgrounds.push_back(std::move(bg2));
                            if(photograph) {
                                next_backgrounds.push_back(std::move(*photograph));
                                const auto& bodies=live->enhanced_landscape.bodies();
                                next_backgrounds.insert(next_backgrounds.end(),bodies.begin(),bodies.end());
                            }
                            if(!photograph && !snapshot->ppu->tunnel_scene && snapshot->background_ex_city_planets) {
                                auto planet_options=background_options;
                                planet_options.scroll_override=std::array<int16_t,2>{0,248};
                                next_backgrounds.push_back(starfox::vr::unique_planet_packet(
                                    *snapshot->ppu,planet_options,{384,208,56,48},srgb));
                            }
                            if(!photograph && !snapshot->ppu->tunnel_scene && snapshot->background_orbital_entry) {
                                auto planet_options=background_options;
                                planet_options.scroll_override=std::array<int16_t,2>{0,312};
                                next_backgrounds.push_back(starfox::vr::unique_planet_packet(
                                    *snapshot->ppu,planet_options,{336,320,56,64},srgb));
                            }
                            if(snapshot->ppu->background_mode==1) {
                                background_options.scroll_override.reset();
                                auto bg3=starfox::vr::background_tile_packet(*snapshot->ppu,
                                    starfox::vr::BackgroundLayer::bg3,background_options,srgb);
                                if(snapshot->background_water_surround)
                                    bg3=starfox::vr::water_surround_packet(*snapshot->ppu,background_options,srgb);
                                else bg3.model=packet.model;
                                if(snapshot->background_water_surround)
                                    next_backgrounds.insert(next_backgrounds.end()-1,std::move(bg3));
                                else next_backgrounds.push_back(std::move(bg3));
                            }
                        }
                        if(!sprite_revision || !starfox::vr::same_draw_geometry(uploaded_backgrounds,next_backgrounds)) {
                            if(!backgrounds.initialize(device.binding().device,get_device,properties,
                                targets.render_pass(),next_backgrounds,{},false))
                                throw std::runtime_error(backgrounds.status());
                            uploaded_backgrounds=std::move(next_backgrounds);
                        } else {
                            model_updates.clear();
                            for(const auto& background:next_backgrounds)
                                if(!background.geometry.vertex_view().empty() || !background.geometry.line_view().empty())
                                    model_updates.push_back(background.model);
                            if(!backgrounds.update_models(model_updates))
                                throw std::runtime_error("Native background transform update failed");
                        }
                        starfox::vr::DrawPacket bitmap;
                        const bool replace_dialogue=starfox::vr::replace_native_dialogue(*snapshot);
                        if(snapshot->native_ex_bitmap) {
                            starfox::vr::BackgroundTileOptions options;
                            options.brightness=snapshot->display_brightness;
                            options.guard_inset=16;options.transparent_black=true;
                            bitmap=starfox::vr::background_tile_packet(*snapshot->ppu,
                                starfox::vr::BackgroundLayer::bg1,options,srgb);
                            bitmap.model=packet.model;
                            // Match desktop: replace active EX communications,
                            // retaining the native pause and results bitmap.
                            if(replace_dialogue) bitmap.geometry={};
                        }
                        std::vector<starfox::vr::DrawPacket> next_sprites;
                        if(snapshot->flow==starfox::simulation::GameFlowState::title) {
                            next_sprites=starfox::vr::title_foreground_packets(*snapshot->ppu,
                                snapshot->display_brightness,!snapshot->ex_title_logo_screen,
                                snapshot->background_scroll_override,srgb);
                            for(auto& foreground:next_sprites) foreground.model=packet.model;
                        }
                        const auto ppu_transform=packet.model;
                        next_sprites.push_back(std::move(bitmap));
                        next_sprites.push_back(std::move(packet));
                        next_sprites.push_back(std::move(meters));
                        if(snapshot->ppu->background_mode==3) {
                            next_sprites=starfox::vr::source_mode3_packets(*snapshot->ppu,
                                snapshot->display_brightness,srgb,snapshot->background_scroll_override);
                            for(auto& layer:next_sprites) layer.model=ppu_transform;
                        }
                        if(replace_dialogue) {
                            live->dialogue_layout.set_language(uint8_t(startup.language));
                            auto dialogue_packets=starfox::vr::source_dialogue_packets(live->rom,live->symbols,
                                snapshot->dialogue,live->dialogue_layout,snapshot->ppu->cgram,snapshot->display_brightness,srgb);
                            for(auto& layer:dialogue_packets) {
                                layer.model=ppu_transform;next_sprites.push_back(std::move(layer));
                            }
                        }
                        if(snapshot->briefing.active) {
                            const auto append_text=[&](uint32_t address,int x,int y,int right,size_t count,uint8_t ink) {
                                auto text=starfox::vr::source_game_text_packet(live->rom,live->symbols,
                                    address,x,y,right,count,ink,snapshot->ppu->cgram,snapshot->display_brightness,srgb);
                                text.model=ppu_transform;
                                next_sprites.push_back(std::move(text));
                            };
                            const auto& brief=snapshot->briefing;
                            append_text(brief.message_address,30,173,218,brief.visible_message_characters,101);
                            append_text(brief.message_address,28,171,216,brief.visible_message_characters,109);
                            append_text(brief.planet_name_address,30,41,224,brief.visible_planet_characters,97);
                            append_text(brief.planet_name_address,28,39,224,brief.visible_planet_characters,100);
                        }
                        if(sprite_revision && starfox::vr::same_draw_geometry(uploaded_sprites,next_sprites)) {
                            model_updates.clear();
                            for(const auto& sprite:next_sprites)
                                if(!sprite.geometry.vertex_view().empty() || !sprite.geometry.line_view().empty())
                                    model_updates.push_back(sprite.model);
                            if(!sprites.update_models(model_updates))
                                throw std::runtime_error("Native sprite transform update failed");
                            ++sprite_reuses;
                        } else {
                            if(!sprites.initialize(device.binding().device,get_device,properties,targets.render_pass(),next_sprites,{},false))
                                throw std::runtime_error(sprites.status());
                            uploaded_sprites.assign(next_sprites.begin(),next_sprites.end());++sprite_uploads;
                        }
                        sprite_revision=snapshot->revision;
                        uploaded_enhanced_sky=startup.enhanced_sky;
                    }
                    if(snapshot->background_orbital_planet && !snapshot->ppu->tunnel_scene) {
                        const auto before=live->history->previous();
                        const bool continuous=before->background_orbital_planet && before->flow==snapshot->flow
                            && before->background_id==snapshot->background_id
                            && !timing::camera_transform_is_discontinuous(before->camera,snapshot->camera);
                        const auto scroll=[](const auto& state) {
                            return state.background_scroll_override?float((*state.background_scroll_override)[1]):float(state.ppu->bg2_scroll_y);
                        };
                        const auto motion=orbital_horizon_motion(scroll(continuous?*before:*snapshot),scroll(*snapshot),
                            continuous?alpha:1.,snapshot->background_orbital_thin,snapshot->background_orbital_entry,
                            snapshot->flow==simulation::GameFlowState::gameplay && live->game.experience()==simulation::Experience::starfox_ex);
                        const auto photographic_motion=orbital_horizon_motion(scroll(continuous?*before:*snapshot),scroll(*snapshot),
                            continuous?alpha:1.,snapshot->background_orbital_thin,snapshot->background_orbital_entry,false);
                        model_updates.clear();
                        for(std::size_t i=0;i<uploaded_backgrounds.size();++i) {
                            const auto& background=uploaded_backgrounds[i];
                            if(!background.geometry.vertex_view().empty() || !background.geometry.line_view().empty())
                                model_updates.push_back(startup.enhanced_sky && live->enhanced_landscape.orbital_body(background)
                                    ?photographic_motion:i==0?motion:background.model);
                        }
                        if(!backgrounds.update_models(model_updates)) throw std::runtime_error("Orbital horizon update failed");
                    }
                    if(snapshot->background_water_surround) {
                        const auto before=live->history->previous();
                        const bool continuous=before->background_water_surround && before->flow==snapshot->flow
                            && !starfox::timing::camera_transform_is_discontinuous(before->camera,snapshot->camera);
                        const auto height=[](const auto& state) {
                            return std::clamp(std::abs(float(state.camera.y-state.shadow_height))/256.F,.01F,8.F);
                        };
                        const auto motion=starfox::vr::water_height_motion(height(continuous?*before:*snapshot),
                            height(*snapshot),continuous?alpha:1.);
                        model_updates.clear();
                        for(const auto& background:uploaded_backgrounds) {
                            if(background.geometry.vertex_view().empty() && background.geometry.line_view().empty()) continue;
                            const auto payload=background.geometry.texel_view();
                            const bool receiver=payload.size()>15 && (payload[15]&32U)!=0;
                            model_updates.push_back(receiver?motion:background.model);
                        }
                        if(!backgrounds.update_models(model_updates)) throw std::runtime_error("Water height motion update failed");
                    }
                    if(snapshot->background_landscape) {
                        const auto before=live->history->previous();
                        const bool continuous=starfox::vr::same_landscape_mapping(*before,*snapshot)
                            && !starfox::timing::camera_transform_is_discontinuous(before->camera,snapshot->camera);
                        const auto motion=starfox::vr::landscape_camera_motion(*before,*snapshot,continuous?alpha:1.);
                        model_updates.clear();
                        for(std::size_t i=0;i<uploaded_backgrounds.size();++i) {
                            const auto& background=uploaded_backgrounds[i];
                            if(!background.geometry.vertex_view().empty() || !background.geometry.line_view().empty())
                                model_updates.push_back((i==0 || snapshot->background_ex_city_planets
                                    || (startup.enhanced_sky && live->enhanced_landscape.landscape_body(background))
                                    || (background.geometry.shared_texels && !background.geometry.vertex_view().empty()
                                        && (background.geometry.vertex_view().front().texture[3]&starfox::vr::backdrop_texture_flag)==starfox::vr::backdrop_texture_flag))
                                    ?motion:background.model);
                        }
                        if(!backgrounds.update_models(model_updates)) throw std::runtime_error("Landscape pitch update failed");
                    }
                    if(startup.enhanced_sky && live->enhanced_landscape.scrolling_pattern()) {
                        const auto motion=live->enhanced_landscape.pattern_motion(*live->history->previous(),*snapshot,alpha);
                        model_updates.clear();
                        for(const auto& background:uploaded_backgrounds) {
                            if(background.geometry.vertex_view().empty() && background.geometry.line_view().empty()) continue;
                            model_updates.push_back(live->enhanced_landscape.pattern_panorama(background)?motion:background.model);
                        }
                        if(!backgrounds.update_models(model_updates)) throw std::runtime_error("Photographic panorama interpolation failed");
                    }
                    if(startup.enhanced_sky && live->enhanced_landscape.moving_body()) {
                        model_updates.clear();
                        for(const auto& background:uploaded_backgrounds) {
                            const auto vertices=background.geometry.vertex_view();
                            if(vertices.empty() && background.geometry.line_view().empty()) continue;
                            const auto body=live->enhanced_landscape.tracked_body(background);
                            model_updates.push_back(body?live->enhanced_landscape.body_motion(*live->history->previous(),*snapshot,alpha,*body):background.model);
                        }
                        if(!backgrounds.update_models(model_updates)) throw std::runtime_error("Celestial motion update failed");
                    }
                    if(snapshot->flow==starfox::simulation::GameFlowState::intro && !snapshot->meters.extended
                        && snapshot->background_unique_top_rows==224) {
                        const auto before=live->history->previous();
                        const auto scroll=[](const auto& state) {
                            return state.background_scroll_override?(*state.background_scroll_override)[1]:state.background_vertical_scroll;
                        };
                        const bool continuous=before->flow==snapshot->flow && before->background_unique_top_rows==224;
                        const auto motion=starfox::vr::intro_planet_motion(scroll(continuous?*before:*snapshot),scroll(*snapshot),
                            continuous?alpha:1.,float(snapshot->source_vanishing_point[1])+16.F);
                        model_updates.clear();
                        for(std::size_t i=0;i<uploaded_backgrounds.size();++i) {
                            const auto& background=uploaded_backgrounds[i];
                            if(!background.geometry.vertex_view().empty() || !background.geometry.line_view().empty())
                                model_updates.push_back(i==1?motion:background.model);
                        }
                        if(!backgrounds.update_models(model_updates)) throw std::runtime_error("Intro planet motion update failed");
                    }
                    const auto profile_end=std::chrono::steady_clock::now();
                    if(profile_end-last_profile>=std::chrono::seconds(1)) {
                        const auto completion=draw.take_completion_timing();
                        const auto ms=[](auto a,auto b) {return std::chrono::duration<double,std::milli>(b-a).count();};
                        std::cout<<"VR CPU frame ms: logic="<<ms(profile_start,profile_logic)
                            <<" models="<<ms(profile_logic,profile_models)<<" upload="<<ms(profile_models,profile_upload)
                            <<" layers="<<ms(profile_upload,profile_end)<<" ticks="<<advance.logic_ticks
                            <<" flow="<<int(snapshot->flow)
                            <<" completed_eyes="<<completion.eyes
                            <<" submit_to_fence_avg_ms="<<(completion.eyes?completion.total_ms/completion.eyes:0.)
                            <<" submit_to_fence_max_ms="<<completion.maximum_ms
                            <<" command_submit_avg_ms="<<(completion.submissions?completion.submit_ms/completion.submissions:0.)
                            <<" pending_fence_polls="<<completion.pending_polls<<'\n';
                        last_profile=profile_end;
                    }
                    }
                } catch(const std::exception& error) {game_error=error.what();return starfox::vr::StereoRenderer::EyeResult::failed;}
            }
            bool ray_ready=false;
            if(startup.ray_tracing_enabled() && ray_environment_valid && render_game && compute_scene_active && (!startup.open || startup.preview) && eye<2) {
                if(!ray_times[eye] || *ray_times[eye]!=time) {
                    ray_times[eye]=time;
                    // Topology and source alpha materials are shared by both eyes.
                    // Cache only until the next source-packet update, including failures.
                    const bool ray_plan_valid=ray_plan_cache.prepare(ray_inputs,compute_scene,
                        physical_properties.limits.minStorageBufferOffsetAlignment);
                    const auto& plan=ray_plan_cache.plan();
                    const auto ray_camera=shadow_camera(camera,eye_extents[eye].width,eye_extents[eye].height);
                    const auto view=ShadowView::from_eye(camera);
                    if(ray_camera && view && ray_plan_valid) {
                        if(!ray_pipeline_ready) ray_pipeline_ready=ray_pipeline.initialize(device.binding().device,get_device,SourceComputeStage::ray_expand);
                        if(ray_pipeline_ready) {
                            if(ray_frames[eye] && ray_sizes[eye]!=plan.bytes
                                && ray_frames[eye]->state()==VulkanDxrFrame::State::idle) {
                                ray_scenes[eye].reset();
                                if(ray_frames[eye]->resize_geometry(uint32_t(plan.bytes/16))) ray_sizes[eye]=plan.bytes;
                            }
                            if(!ray_frames[eye] || ray_sizes[eye]!=plan.bytes || ray_frames[eye]->state()==VulkanDxrFrame::State::error) {
                                ray_scenes[eye].reset();ray_frames[eye]=std::make_unique<VulkanDxrFrame>();
                                if(!ray_frames[eye]->initialize(device.binding().device,get_device,device.binding().physicalDevice,
                                    ray_properties,*device.adapter_luid(),device.queue(),device.binding().queueFamilyIndex,uint32_t(plan.bytes/16))) ray_frames[eye].reset();
                                ray_sizes[eye]=plan.bytes;
                            }
                            if(ray_frames[eye]) {
                                if(ray_scenes[eye] && !ray_scenes[eye]->refresh(plan,camera)) ray_scenes[eye].reset();
                                if(!ray_scenes[eye]) {
                                    ray_scenes[eye]=std::make_unique<VulkanMixedRayScene>();
                                    if(!ray_scenes[eye]->initialize(device.binding().device,get_device,properties,physical_properties.limits,
                                        ray_pipeline,compute_scene,ray_frames[eye]->geometry(),plan,camera)) ray_scenes[eye].reset();
                                }
                                if(ray_scenes[eye]) {
                                    ray_coverage_views[eye]=ray_plan_cache.materials().view();
                                    ray_frames[eye]->begin(*ray_camera,view->direction(ray_light),ray_ground?std::optional(view->receiver(*ray_ground)):std::nullopt,
                                        [&](VkCommandBuffer command,VkExtent2D) {
                                            if(!compute_scene.record_compute(command) || !ray_scenes[eye]->record(command,false))
                                                throw std::runtime_error("Ray geometry recording failed");
                                        },&ray_coverage_views[eye]);
                                }
                            }
                        }
                    }
                }
                if(ray_frames[eye]) {
                    const auto state=ray_frames[eye]->poll();
                    if(state==VulkanDxrFrame::State::producing) return StereoRenderer::EyeResult::pending;
                    ray_ready=state==VulkanDxrFrame::State::ready
                        && ray_frames[eye]->prepare_draw(properties,targets.render_pass(),{0,0,0,1});
                }
            }
            const auto ray_wait=ray_ready?ray_frames[eye]->draw_wait():VulkanEyeCommands::TimelineWait{};
            VkClearColorValue clear{};
            clear.float32[eye==0?0:2]=0.1F;clear.float32[3]=1.0F;
            if(live) {
                const auto snapshot=live->history->current();
                const bool srgb=swapchains.format()==VK_FORMAT_R8G8B8A8_SRGB
                    || swapchains.format()==VK_FORMAT_B8G8R8A8_SRGB;
                const bool controls=snapshot->flow==starfox::simulation::GameFlowState::controls_type
                    || snapshot->flow==starfox::simulation::GameFlowState::controls_choice;
                const auto backdrop=(controls || snapshot->flow==starfox::simulation::GameFlowState::continue_choice)
                    ?starfox::vr::source_menu_background_colour(*snapshot->ppu,snapshot->display_brightness,srgb)
                    :snapshot->flow==starfox::simulation::GameFlowState::game_over || snapshot->ppu->tunnel_scene || snapshot->background_unique_top_rows!=0 || snapshot->background_star_sphere || snapshot->background_space_horizon
                    ?starfox::vr::source_background_border_colour(snapshot->ppu->cgram,snapshot->display_brightness,srgb)
                    :starfox::vr::source_backdrop_colour(snapshot->ppu->cgram[0],snapshot->display_brightness,srgb);
                std::copy(backdrop.begin(),backdrop.end(),clear.float32);
            }
            const auto eye_result=draw.draw(eye,image,camera,time,clear,[&](VkCommandBuffer command,VkExtent2D extent,const auto& eye_camera,XrTime) {
                if(startup.open && !startup.preview) {
                    if(!startup_panel.record(command,extent,eye_camera)) throw std::runtime_error("Startup menu recording failed");
                    return;
                }
                const bool controls_stars=live && (live->history->current()->flow==starfox::simulation::GameFlowState::controls_type
                    || live->history->current()->flow==starfox::simulation::GameFlowState::controls_choice);
                if(render_game && !controls_stars && !surrounding_stars.record(command,extent,eye_camera))
                    throw std::runtime_error("Surrounding starfield recording failed");
                auto world_camera=eye_camera;
                world_camera.effects={startup.world_effect,startup.world_intensity,0,0};
                auto model_camera=eye_camera;
                model_camera.effects={startup.model_effect,startup.model_intensity,0,0};
                if(render_game && !backgrounds.record(command,extent,world_camera))
                    throw std::runtime_error("Native background layer recording failed");
                // Mask only repeated background artwork. Gameplay models,
                // shadows and explosions remain free to occupy the full view.
                if(render_game && !tunnel_surround.record(command,extent,world_camera))
                    throw std::runtime_error("Tunnel background surround recording failed");
                if(render_game && controls_stars && !surrounding_stars.record(command,extent,eye_camera))
                    throw std::runtime_error("Controls starfield recording failed");
                if(render_triangle && !pipeline.record(command,extent,vertices.buffer(),vertices.count(),eye_camera))
                    throw std::runtime_error("Triangle recording failed");
                if((render_model || render_game) && !(render_game && compute_scene_active
                    ?compute_scene.record(command,extent,model_camera,ray_ready):scene.record(command,extent,model_camera)))
                    throw std::runtime_error("Model packet recording failed");
                if(ray_ready && !ray_frames[eye]->record_draw(command,extent)) throw std::runtime_error("Ray shadow drawing failed");
                const auto draw_circle=[&] {
                    if(render_game && circle_active && !circle_pipeline.record_model(command,extent,
                        circle_vertices.buffer(),circle_vertices.count(),eye_camera,circle_model))
                        throw std::runtime_error("Bomb circle recording failed");
                };
                if(!circle_over_hud) draw_circle();
                if(render_game && sandbox.active() && !sandbox_pointer.record(command,extent,eye_camera))
                    throw std::runtime_error("Sandbox pointer recording failed");
                if(render_game && !sprites.record(command,extent,eye_camera))
                    throw std::runtime_error("Native sprite layer recording failed");
                if(circle_over_hud) draw_circle();
                if(render_game && shutter_active) {
                    const starfox::vr::Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
                    if(!shutter_pipeline.record(command,extent,shutter_vertices.buffer(),shutter_vertices.count(),
                        starfox::vr::EyeCamera{identity,identity}))
                        throw std::runtime_error("Scramble shutter recording failed");
                }
                if(startup.open && startup.preview && !startup_panel.record(command,extent,eye_camera))
                    throw std::runtime_error("Preview menu recording failed");
            },[&](VkCommandBuffer command,VkExtent2D,const auto&,XrTime) {
                if(ray_ready && !ray_frames[eye]->record_acquire(command)) throw std::runtime_error("Ray shadow acquire failed");
                if(eye==0 && render_game && !compute_scene_active && (!startup.open || startup.preview) && !scene.record_compute(command))
                    throw std::runtime_error("Live grid compute recording failed");
                if(!ray_ready && eye==0 && render_game && compute_scene_active && (!startup.open || startup.preview) && !compute_scene.record_compute(command))
                    throw std::runtime_error("Live model compute recording failed");
            },[&](VkCommandBuffer command,VkExtent2D,const auto&,XrTime) {
                if(ray_ready && !ray_frames[eye]->record_release(command)) throw std::runtime_error("Ray shadow release failed");
            },ray_ready?&ray_wait:nullptr);
            if(eye_result==StereoRenderer::EyeResult::complete && eye<2 && ray_frames[eye]
                && ray_frames[eye]->state()==VulkanDxrFrame::State::ready) ray_frames[eye]->retire();
            return eye_result;
        },1.0F,0.05F);
        using Result=starfox::vr::StereoRenderer::Result;
        if(session.state()!=XR_SESSION_STATE_FOCUSED) {
            input.poll(false);
            if(live) static_cast<void>(live->driver->advance(input_time.value_or(0),{},false));
            if(live && !live->output.set_active(false)) {std::cerr<<live->output.status()<<'\n';return 8;}
        }
        if(result==Result::error) {
            std::cerr<<"Eye rendering failed: "<<commands.status()<<"; "<<session.status()<<"; "<<swapchains.status()<<"; "<<input.status()<<"; "<<game_error<<'\n';return 8;
        }
        if(result==Result::submitted) ++submitted;
        // A pending eye already performed a bounded, completion-aware fence
        // wait. Do not add another fixed millisecond after it.
        else if(!draw.pending()) frame_wait.pause();
    }
    if(!cancelled && host.frame_limit && submitted<host.frame_limit) {std::cerr<<"Eye rendering diagnostic incomplete: "<<submitted<<"/"<<host.frame_limit<<" stereo frames\n";return 8;}
    if(!cancelled && live && !live->logic_ticks) {std::cerr<<"Live game diagnostic never advanced a focused source tick\n";return 8;}
    std::cout<<"Submitted "<<submitted<<" fenced stereo "<<(render_game?"live game models":render_model?"cartridge model":render_triangle?"triangle":"clear")<<" frames. Experimental presentation; parity incomplete.\n";
    std::cout<<"Observed "<<menu_presses<<" menu press edges.\n";
    if(live) std::cout<<"Advanced "<<live->logic_ticks<<" source logic ticks with mixed PCM output (MSU "<<bool(live->pack)<<"); native sprite and meter layers enabled; backgrounds and HUD customization remain incomplete.\n";
    if(live) std::cout<<"Sprite layer uploads: "<<sprite_uploads<<"; unchanged source revisions reused: "<<sprite_reuses<<'\n';
    if(live) std::cout<<"Scene uploads: "<<scene_uploads<<"; transform-only reuse: "<<model_only_updates<<'\n';
    if(live) std::cout<<"Object GPU uploads: "<<gpu_uploads<<"; object resource reuses: "<<gpu_reuses<<'\n';
    return 0;
}
