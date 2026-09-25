#pragma once
#include "starfox/vr/background_tiles.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include "starfox/vr/game_scene.hpp"
#include "starfox/render/enhanced_backdrop_library.hpp"
#include "starfox/render/environment_effects.hpp"
#include <tuple>
#include <numbers>

namespace starfox::vr {
// Render-thread owner. Call only when enabled; the native/default path performs
// no photographic loading, atlas classification or palette-reference reads.
class EnhancedLandscape {
    std::vector<std::pair<uint16_t,std::string_view>> names_;
    uint32_t palette_address_{},menu_choice_address_{};
    render::EnhancedBackdropLibrary images_;
    std::optional<unsigned> resident_;
    bool resident_full_sphere_{};
    bool resident_latitude_uv_{};
    std::shared_ptr<const std::vector<uint32_t>> texture_;
    std::optional<std::tuple<unsigned,unsigned,unsigned>> region_key_;
    std::array<uint8_t,256> regions_{};
    std::optional<PhotographicLandscape> options_;
    bool srgb_{};
    DrawPacket packet_;
    std::shared_ptr<const std::vector<SceneVertex>> ground_source_,ground_vertices_;
    std::unique_ptr<simulation::SnesPpuState> game_over_source_;
    std::shared_ptr<const std::vector<SceneVertex>> game_over_vertices_;
    bool game_over_srgb_{};
    std::array<std::shared_ptr<const std::vector<uint32_t>>,2> moon_textures_;
    std::vector<PhotographicBody> body_options_;
    std::vector<DrawPacket> bodies_;
    bool body_srgb_{};
    std::optional<render::UniqueSkyObject> unique_body_;
    std::optional<unsigned> unique_texture_artwork_;
    std::shared_ptr<const std::vector<uint32_t>> unique_texture_,body_texture_;
    bool face_mode_{};
    std::shared_ptr<const std::vector<uint32_t>> face_texture_,face_native_texture_;
    bool cloud_mode_{};
    render::CloudLimbAtlas cloud_atlas_;
    uint64_t cloud_key_{};
    std::array<std::shared_ptr<const std::vector<uint32_t>>,2> cloud_textures_;
    bool orbital_mode_{};
    std::optional<unsigned> orbital_artwork_;
    std::shared_ptr<const std::vector<uint32_t>> orbital_texture_,orbital_moon_texture_;
    std::optional<PhotographicLandscape> orbital_options_;
    DrawPacket orbital_packet_;
    bool orbital_srgb_{};
    bool pattern_mode_{},pattern_vertical_scroll_{};
    float pattern_scroll_scale_{1};
public:
    bool full_sphere() const {return options_ && options_->full_sphere;}
    const std::vector<DrawPacket>& bodies() const {return bodies_;}
    bool pattern_panorama(const DrawPacket& packet) const {
        return pattern_mode_ && packet.geometry.shared_vertices
            && packet.geometry.shared_vertices==packet_.geometry.shared_vertices;
    }
    bool scrolling_pattern() const {return pattern_mode_;}
    Matrix4 pattern_motion(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha) const {
        if(!pattern_mode_ || !options_ || !previous.ppu || !current.ppu)
            throw std::invalid_argument("Invalid photographic pattern motion state");
        if(previous.flow!=current.flow || previous.background_id!=current.background_id
            || previous.ppu->bg2_character_base!=current.ppu->bg2_character_base
            || previous.ppu->bg2_screen_base!=current.ppu->bg2_screen_base
            || timing::camera_transform_is_discontinuous(previous.camera,current.camera)) alpha=1;
        const auto scroll=[](const auto& state) {
            return render::environment_center_scroll_y(*state.ppu,
                state.flow==simulation::GameFlowState::ex_pregame_menu
                    ?unsigned(state.ppu->bg2_scroll_y):unsigned(state.background_vertical_scroll));
        };
        const float dx=std::remainder(float(current.ppu->bg2_scroll_x)-float(previous.ppu->bg2_scroll_x),512.F);
        return photographic_scroll_correction(dx*pattern_scroll_scale_,
            pattern_vertical_scroll_?float(scroll(current))-float(scroll(previous)):0.F,
            alpha,options_->horizontal_scale,options_->vertical_scale);
    }
    bool orbital_body(const DrawPacket& packet) const {
        return orbital_mode_ && packet.geometry.shared_vertices
            && std::any_of(bodies_.begin(),bodies_.end(),[&](const auto& body) {
                return body.geometry.shared_vertices==packet.geometry.shared_vertices;
            });
    }
    bool landscape_body(const DrawPacket& packet) const {
        if(moving_body() || !packet.geometry.shared_vertices) return false;
        return std::any_of(bodies_.begin(),bodies_.end(),[&](const auto& body) {
            return body.geometry.shared_vertices==packet.geometry.shared_vertices;
        });
    }
    bool moving_body() const {return unique_body_.has_value() || face_mode_ || cloud_mode_;}
    std::optional<size_t> tracked_body(const DrawPacket& packet) const {
        if(!moving_body() || !packet.geometry.shared_vertices) return {};
        for(size_t i=0;i<bodies_.size();++i)
            if(packet.geometry.shared_vertices==bodies_[i].geometry.shared_vertices) return i;
        return {};
    }
    Matrix4 body_motion(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha,size_t index=0) const {
        if(!moving_body() || !previous.ppu || !current.ppu || !std::isfinite(alpha)
            || index>=(face_mode_?render::face_planet_regions.size():cloud_mode_?2:1))
            throw std::invalid_argument("Invalid celestial motion state");
        auto body=unique_body_.value_or(render::UniqueSkyObject{});
        if(face_mode_) {
            const auto& r=render::face_planet_regions[index];body.x=(r[0]+r[2])*.5F;body.y=(r[1]+r[3])*.5F;
        }
        if(cloud_mode_) {
            const auto& r=render::cloud_limb_regions[index];body.x=(r[0]+r[2])*.5F;body.y=(r[1]+r[3])*.5F;
        }
        alpha=std::clamp(alpha,0.,1.);
        if(previous.flow!=current.flow || previous.background_id!=current.background_id
            || timing::camera_transform_is_discontinuous(previous.camera,current.camera)) alpha=1;
        if(current.flow==simulation::GameFlowState::ex_pregame_menu) {
            const auto center=[&](const auto& state) {
                const int scroll=((int(state.ppu->bg2_scroll_x)+256)&511)-256;
                float x=body.x-float(scroll)-128,wrapped=std::remainder(x,512.F);
                if(!face_mode_ && wrapped>=-128 && wrapped<128) x=wrapped;
                return std::array{x+128,body.y-float(unsigned(state.ppu->bg2_scroll_y)&511)};
            };
            const auto a=center(previous),b=center(current);
            return photographic_body_motion({float(a[0]+std::remainder(b[0]-a[0],512.F)*alpha),
                float(a[1]+std::remainder(b[1]-a[1],512.F)*alpha)});
        }
        const auto scroll=[](const auto& state) {
            const auto& p=*state.ppu;
            std::array<uint16_t,32> columns{};
            for(unsigned i=0;i<columns.size();++i) columns[i]=uint16_t(p.vram[0x5f40+i*2])
                |(uint16_t(p.vram[0x5f41+i*2])<<8);
            return render::celestial_scroll(p.bg2_horizontal_offsets,columns,
                float(p.bg2_scroll_x),float(p.bg2_scroll_y),p.bg2_horizontal_offsets_enabled,p.bg2_vertical_offsets_enabled);
        };
        const auto t=render::stabilize_celestial_body(render::interpolate_celestial_scroll(
            scroll(previous),scroll(current),float(alpha),face_mode_?256:body.x,face_mode_?256:body.y),body.x,body.y);
        return photographic_body_motion({body.x-t[2]+128,body.y-t[3]});
    }
    void retain_native_ground(DrawPacket& native,const simulation::SnesPpuState& ppu) {
        if(options_ && options_->preserve_game_over_front) {
            if(native.geometry.vertex_view().empty()) return;
            const auto key=[](const auto& p) {return std::tuple{p.background_mode,p.bg2_character_base,
                p.bg2_screen_base,p.bg2_screen_size,p.bg2_tile_size_16,p.bg2_scroll_x,p.bg2_scroll_y,p.mosaic};};
            if(!game_over_source_ || game_over_srgb_!=srgb_ || key(*game_over_source_)!=key(ppu)
                || game_over_source_->vram!=ppu.vram || game_over_source_->cgram!=ppu.cgram) {
                game_over_vertices_=std::make_shared<const std::vector<SceneVertex>>(
                    game_over_foreground_vertices(ppu,srgb_));
                game_over_source_=std::make_unique<simulation::SnesPpuState>(ppu);game_over_srgb_=srgb_;
            }
            native.geometry.vertices.clear();native.geometry.shared_vertices=game_over_vertices_;
            return;
        }
        if(full_sphere()) {native.geometry={};return;}
        const auto& source=native.geometry.shared_vertices;
        if(!source || source!=ground_source_) {
            auto vertices=std::make_shared<std::vector<SceneVertex>>();
            const auto all=native.geometry.vertex_view();
            for(size_t i=0;i+2<all.size();i+=3) {
                if(all[i].position[1]>=0 && all[i+1].position[1]>=0 && all[i+2].position[1]>=0) continue;
                vertices->insert(vertices->end(),all.begin()+i,all.begin()+i+3);
            }
            ground_source_=source;ground_vertices_=std::move(vertices);
        }
        native.geometry.vertices.clear();native.geometry.shared_vertices=ground_vertices_;
    }
    explicit EnhancedLandscape(const assets::SymbolMap& symbols) {
        const auto base=symbols.find("BGLISTS");
        constexpr std::array names{"BG_1_1C","BG_TRAINING","BG_2_3A","BG_1_6A","BG_3_7A",
            "BG_3_3A","BG_3_5","BG_3_1C","BG_7_1","BG_7_2","BG_7_3","BG_7_4",
            "BG_5_4","BG_5_1","BG_6_1","BG_6_5","BG_6_2","BG_6_4","BG_5_5",
            "BG_7_5","BG_6_6","BG_6_7B","BG_1_7B","BG_COMET","BG_1_2","BG_6_3","BG_5_3",
            "BG_3_4B","BG_3_4D","BG_3_2","BG_1_4","BG_1_14","BG_2_4","BG_3_7C"};
        if(!base.empty()) for(const auto name:names) {
            const auto values=symbols.find(name);
            if(!values.empty() && (values.front()>>16)==(base.front()>>16))
                names_.emplace_back(uint16_t(values.front()-base.front()),name);
        }
        const auto palette=symbols.find("VRAM3ADDR"),choice=symbols.find("PGBG");
        if(!palette.empty()) palette_address_=palette.front();
        if(!choice.empty()) menu_choice_address_=choice.front();
    }
    template<class Loader> std::optional<DrawPacket> prepare(const GameSceneSnapshot& scene,
        const simulation::GameSimulation& game,Loader&& loader,bool srgb=false) {
        unique_body_.reset();
        face_mode_=false;
        cloud_mode_=false;
        pattern_mode_=pattern_vertical_scroll_=false;
        pattern_scroll_scale_=1;
        orbital_mode_=scene.background_orbital_planet;
        // These scenes require separate, single-occurrence celestial masks or
        // lower-hemisphere projections. Never erase their subjects with a sky.
        if(!scene.ppu || scene.ppu->tunnel_scene) {orbital_mode_=false;return {};}
        const bool ex=scene.meters.extended;
        const bool menu=ex && scene.flow==simulation::GameFlowState::ex_pregame_menu;
        const bool game_over=scene.flow==simulation::GameFlowState::game_over;
        const unsigned choice=menu && menu_choice_address_?game.map().peek_ram_byte(menu_choice_address_).value_or(0):0;
        face_mode_=scene.background_ex_face_planets || (menu && choice==18);
        std::string_view name;
        for(const auto& [id,label]:names_) if(id==scene.background_id) {name=label;break;}
        // EX 1-4 switches to BG_1_14 in the live route; its authored atlas
        // contains the same single blue cloud and separate green limb as the
        // preview/BG_1_4. Keep both as individually tracked subjects.
        cloud_mode_=menu?choice==20:(name=="BG_1_4" || (ex && name=="BG_1_14"));
        const unsigned unique_choice=menu?choice:(name=="BG_3_4B" || name=="BG_3_4D")?28U:name=="BG_3_2"?27U:255U;
        if((menu || scene.background_unique_space)
            && (unique_choice==19 || unique_choice==27 || unique_choice==28 || unique_choice==31))
            unique_body_=render::ex_menu_sky_object(unique_choice);
        const bool fortuna=menu?choice==32:name=="BG_3_3A";
        const bool twins=ex && (menu?choice==10:name=="BG_5_4");
        const bool storm_moons=ex && (menu?choice==3:name=="BG_6_4");
        const bool city=ex && scene.background_ex_city_planets;
        const bool ocean_landmark=ex && (menu?choice==1:scene.background_ex_ocean_island);
        const bool sun_landmark=ex && (menu?choice==12:name=="BG_5_5" || name=="BG_7_5");
        const bool snow_moon=ex && (menu?choice==4:name=="BG_5_1" || name=="BG_6_1");
        const bool mesa_landmark=menu && choice==5;
        const bool landmark=ocean_landmark || sun_landmark || snow_moon || mesa_landmark
            || (ex && scene.background_ex_volcanic_horizon);
        if((scene.background_landscape_unique_half || scene.background_landscape_unique_right_half
            || scene.background_ex_twin_planets) && !fortuna && !twins && !storm_moons && !face_mode_ && !cloud_mode_ && !city && !landmark) return {};
        const bool nebula=ex && (menu?choice==2:name=="BG_5_3");
        const bool ember=ex && (menu?choice==29:name=="BG_2_4");
        const auto pattern=menu?render::ex_menu_full_sky_backdrop(choice)
            :name=="BG_1_2"?std::optional<unsigned>{20}
            :ex && name=="BG_6_3"?std::optional<unsigned>{19}
            :nebula?std::optional<unsigned>{7}
            :ember?std::optional<unsigned>{21}:std::nullopt;
        const bool star_surround=orbital_mode_ || game_over || unique_body_ || face_mode_ || cloud_mode_;
        const bool full_sky=star_surround || pattern.has_value();
        pattern_mode_=pattern.has_value();
        pattern_vertical_scroll_=pattern_mode_ && !nebula && !ember;
        if(!full_sky && !scene.background_landscape) return {};
        const auto artwork=star_surround?std::optional<unsigned>{34}:pattern?pattern
            :city?std::optional<unsigned>{6}:menu?render::ex_menu_landscape_backdrop(choice):render::gameplay_landscape_backdrop(name,ex);
        if(!artwork) return {};
        PhotographicLandscape options;
        if(full_sky) {
            options.full_sphere=true;options.vertical_scale=512.F/224.F;
            const unsigned origin=ember?312:menu?(choice==6?400:choice==30?124:352):(name=="BG_1_2"?352:384);
            // The menu's host camera scroll belongs to its models, not BG2.
            // Register scrolling keeps asteroid/nebula bands at native height.
            const auto scroll=render::environment_center_scroll_y(*scene.ppu,
                menu?unsigned(scene.ppu->bg2_scroll_y):unsigned(scene.background_vertical_scroll));
            options.horizon_v=(game_over || nebula)?.5F:.5F+(112.F-float(origin)+float(scroll))/224.F;
            options.horizontal_offset=.25F+float(unsigned(scene.ppu->bg2_scroll_x)&511)/512.F;
        }
        if(*artwork==22) {options.horizon_v=.89F;options.vertical_scale=512.F/200.F;}
        if(*artwork==16 && !full_sky) {options.horizon_v=.55F;options.vertical_scale=512.F/224.F;}
        if(storm_moons) {options.horizon_v=.55F;options.vertical_scale=512.F/224.F;}
        if(*artwork==27) {options.horizon_v=.55F;options.vertical_scale=512.F/200.F;}
        if(*artwork==31) options.horizon_v=.82F;
        if(*artwork==32) {options.horizon_v=.88235294F;options.vertical_scale=512.F/136.F;
            options.horizontal_scale=2;options.horizontal_offset=.5F;options.repeats=12;}
        const float brightness=float(std::min(unsigned(scene.display_brightness),15U))/15.F;
        std::array<float,4> response{0,0,0,1};
        // The native menu reloads preview palettes independently of VRAM3ADDR.
        // Matching desktop, previews use the authored exposure, not a stale
        // gameplay upload palette (which severely overexposes e.g. BG 26).
        if(ex && !menu && !game_over && !unique_body_ && !face_mode_ && !cloud_mode_ && !orbital_mode_ && palette_address_) {
            const unsigned origin=menu?render::ex_menu_landscape_origin(choice).value_or(scene.landscape_atlas_origin)
                :render::gameplay_landscape_origin(name,true).value_or(scene.landscape_atlas_origin);
            const auto key=std::tuple<unsigned,unsigned,unsigned>{scene.background_id,menu?choice+1:0,origin};
            if(region_key_!=key) {
                regions_=render::environment_palette_regions(*scene.ppu,origin);
                if(!menu) render::correct_ex_landscape_palette(name,regions_);
                region_key_=key;
            }
            auto regions=regions_;
            if(full_sky) regions.fill(0); // Asteroid atlases have no ground palette.
            if(*artwork==22) {regions.fill(0);for(unsigned ink=71;ink<79;++ink) regions[ink]=2;}
            const auto source=game.map().read_native_word(palette_address_)
                | (uint32_t(game.map().read_native_byte(palette_address_+2))<<16);
            std::array<uint16_t,112> reference{};
            for(unsigned i=0;i<reference.size();++i) reference[i]=game.map().read_native_word(source+i*2);
            render::calibrate_backdrop_palette(*artwork,reference);
            response=render::backdrop_palette_response(reference,scene.ppu->cgram,regions)[0];
        }
        for(unsigned c=0;c<3;++c) {options.response[c]=response[3]*brightness;options.palette_shift[c]=response[c]*brightness;}
        if(fortuna) for(unsigned c=0;c<3;++c) {options.response[c]*=.94F;options.palette_shift[c]*=.94F;}
        if(!ex && *artwork==12) {
            options.cloud_palette[0]=1;
            for(unsigned i=1;i<16;++i) options.cloud_palette[i]=scene.ppu->cgram[i]&32767;
        }
        if(nebula && !menu) {
            options.cloud_palette[0]=2;
            for(unsigned bank=0;bank<2;++bank) for(unsigned shade=0;shade<7;++shade)
                options.cloud_palette[1+bank*7+shade]=scene.ppu->cgram[(5+bank)*16+8+shade]&32767;
        }
        if(menu && choice==26) for(unsigned c=0;c<3;++c) options.response[c]*=.85F;
        if(nebula || ember) {
            options.latitude_uv=true;
            options.horizontal_scale=1.F/(2.F*std::numbers::pi_v<float>);options.repeats=1;
            pattern_scroll_scale_=.25F; // Four source tiles across the sphere atlas.
            options.horizontal_offset=.25F+float(unsigned(scene.ppu->bg2_scroll_x)&511)/(512.F*4);
        }
        if(orbital_mode_ || game_over || unique_body_ || face_mode_ || cloud_mode_) {
            options.latitude_uv=true;
            options.horizontal_scale=1.F/(2.F*std::numbers::pi_v<float>);options.repeats=1;
            options.preserve_game_over_front=game_over;
            if(game_over) for(unsigned c=0;c<3;++c) options.palette_shift[c]-=float(scene.background_colour_subtract)/31.F*brightness;
        }
        if(resident_!=artwork || resident_full_sphere_!=full_sky || resident_latitude_uv_!=options.latitude_uv) {
            const auto& image=images_.get(*artwork,std::forward<Loader>(loader));
            texture_=(nebula || ember)?make_pattern_sky_texture(image)
                :make_landscape_texture(image,full_sky,options.latitude_uv,
                    *artwork==0?8U:32U);resident_=artwork;
            resident_full_sphere_=full_sky;resident_latitude_uv_=options.latitude_uv;options_.reset();
        }
        if(!options_ || *options_!=options || srgb_!=srgb) {
            packet_=photographic_landscape_packet(texture_,options,srgb);options_=options;srgb_=srgb;
        }
        if(orbital_mode_) {
            const unsigned art=menu && choice==35?5:4;
            if(orbital_artwork_!=art) {
                orbital_texture_=make_orbital_texture(images_.get(art,loader),images_.get(art==5?36:35,loader));
                orbital_artwork_=art;orbital_options_.reset();
            }
            PhotographicLandscape surface;surface.full_sphere=true;surface.orbital_surface=true;
            std::array<float,4> fit{0,0,0,1};
            if(ex && !menu && palette_address_) {
                const auto address=game.map().read_native_word(palette_address_)
                    |(uint32_t(game.map().read_native_byte(palette_address_+2))<<16);
                std::array<uint16_t,112> reference{};
                for(unsigned i=0;i<reference.size();++i) reference[i]=game.map().read_native_word(address+i*2);
                render::calibrate_backdrop_palette(art,reference);
                const auto regions=render::environment_palette_regions(*scene.ppu,scene.background_orbital_entry?292:272);
                fit=render::backdrop_palette_response(reference,scene.ppu->cgram,regions)[0];
            }
            for(unsigned c=0;c<3;++c) {surface.response[c]=fit[3]*brightness;surface.palette_shift[c]=fit[c]*brightness;}
            if(orbital_options_!=surface || orbital_srgb_!=srgb) {
                orbital_packet_=photographic_landscape_packet(orbital_texture_,surface,srgb);
                orbital_options_=surface;orbital_srgb_=srgb;
            }
            const float scroll=scene.background_scroll_override?float((*scene.background_scroll_override)[1]):float(scene.ppu->bg2_scroll_y);
            // The photograph is already authored horizon-horizontal. The native
            // EX tile-layer quarter-turn must not rotate this replacement.
            const auto motion=orbital_horizon_motion(scroll,scroll,1.,scene.background_orbital_thin,scene.background_orbital_entry,false);
            const auto old=std::move(bodies_);bodies_={orbital_packet_};bodies_.front().model=motion;
            if(scene.background_orbital_entry) {
                if(!orbital_moon_texture_) orbital_moon_texture_=make_moon_texture(images_.get(25,loader),false,{53,55,1199,1191});
                PhotographicBody moon;moon.center={360,40};moon.diameter={48,48};moon.palette[0]=8;
                for(unsigned ink=0;ink<8;++ink) moon.palette[ink+1]=scene.ppu->cgram[17+ink]&32767;
                for(unsigned c=0;c<3;++c) moon.response[c]=brightness;
                auto body=photographic_body_packet(orbital_moon_texture_,moon,srgb);
                if(old.size()==2 && same_draw_geometry(std::span(&old[1],1),std::span(&body,1))) body.geometry=old[1].geometry;
                body.model=motion;bodies_.push_back(std::move(body));
            }
            body_options_.clear();body_texture_.reset();return packet_;
        }
        if(landmark) {
            std::array<bool,256> keep{};
            // Keep the tiny original luminous disk and mesa silhouette rather
            // than replacing them with a repeated photograph or a sky rectangle.
            if(snow_moon) for(unsigned ink:{3U,13U,14U,15U}) keep[ink]=true;
            else for(unsigned ink=mesa_landmark?108:sun_landmark?82:ocean_landmark?58:54;
                ink<=(mesa_landmark?110:sun_landmark?94:ocean_landmark?59:57);++ink) keep[ink]=true;
            BackgroundTileOptions source;source.brightness=scene.display_brightness;
            source.scroll_override=std::array<int16_t,2>{0,int16_t(scene.landscape_atlas_origin)};
            auto body=landscape_landmark_packet(*scene.ppu,source,
                snow_moon?std::array<unsigned,4>{420,menu?308U:228U,8,8}
                    :mesa_landmark?std::array<unsigned,4>{128,400,48,32}
                    :sun_landmark?std::array<unsigned,4>{284,328,16,16}
                    :ocean_landmark?std::array<unsigned,4>{256,menu?424U:344U,96,8}
                    :std::array<unsigned,4>{128,320,128,32},keep,srgb);
            const auto* old=bodies_.size()==1?&bodies_.front().geometry:nullptr;
            if(old && std::equal(old->vertex_view().begin(),old->vertex_view().end(),
                body.geometry.vertices.begin(),body.geometry.vertices.end())) {
                body.geometry.shared_vertices=old->shared_vertices;body.geometry.vertices.clear();
            } else {
                body.geometry.shared_vertices=std::make_shared<const std::vector<SceneVertex>>(std::move(body.geometry.vertices));
                body.geometry.vertices.clear();
            }
            if(old && old->same_texels(body.geometry)) {
                body.geometry.shared_texels=old->shared_texels;body.geometry.texels.clear();
            } else {
                body.geometry.shared_texels=std::make_shared<const std::vector<uint32_t>>(std::move(body.geometry.texels));
                body.geometry.texels.clear();
            }
            bodies_={std::move(body)};body_options_.clear();body_texture_.reset();return packet_;
        }
        if(cloud_mode_) {
            const auto& atlas=cloud_atlas_.image(images_.get(ex?26:30,loader),images_.get(23,loader),*scene.ppu);
            if(cloud_key_!=atlas.immutable_upload_key) {
                for(unsigned i=0;i<2;++i) cloud_textures_[i]=make_cloud_body_texture(atlas,cloud_atlas_,i==1);
                cloud_key_=atlas.immutable_upload_key;
            }
            const auto old=std::move(bodies_);bodies_.clear();
            for(unsigned i=0;i<2;++i) {
                const auto& r=render::cloud_limb_regions[i];PhotographicBody options;
                options.diameter={r[2]-r[0],r[3]-r[1]};
                for(unsigned c=0;c<3;++c) options.response[c]=brightness;
                if(i==1) {
                    options.palette[0]=5;
                    for(unsigned ink=1;ink<16;++ink) options.palette[ink]=scene.ppu->cgram[80+ink]&32767;
                }
                auto body=photographic_body_packet(cloud_textures_[i],options,srgb);
                if(i<old.size() && same_draw_geometry(std::span(&old[i],1),std::span(&body,1))) body.geometry=old[i].geometry;
                body.model=body_motion(scene,scene,1.,i);bodies_.push_back(std::move(body));
            }
            body_options_.clear();body_texture_.reset();return packet_;
        }
        if(face_mode_) {
            if(!face_texture_) face_texture_=make_celestial_texture(images_.get(33,loader),
                {.037F*1253,.034F*1253,.963F*1253,.947F*1253});
            const auto old=std::move(bodies_);bodies_.clear();
            const auto& p=*scene.ppu;
            for(size_t i=0;i<render::face_planet_regions.size();++i) {
                const auto& r=render::face_planet_regions[i];DrawPacket body;
                if(r[2]-r[0]==r[3]-r[1]) {
                    PhotographicBody options;options.diameter={r[2]-r[0],r[3]-r[1]};options.palette[0]=4;
                    for(unsigned c=0;c<3;++c) options.response[c]=brightness;
                    const unsigned edge=p.bg2_tile_size_16?16:8,pages=(p.bg2_screen_size&1)?2:1;
                    const unsigned x=unsigned((r[0]+r[2])*.5F)/edge,y=unsigned((r[1]+r[3])*.5F)/edge;
                    const unsigned entry=(x/32+y/32*pages)*1024+(y%32)*32+x%32;
                    const unsigned address=(p.bg2_screen_base*2+entry*2)&65535;
                    const unsigned tile=p.vram[address]|(unsigned(p.vram[(address+1)&65535])<<8);
                    for(unsigned ink=1;ink<15;++ink) options.palette[ink]=p.cgram[((tile>>10)&7)*16+ink]&32767;
                    body=photographic_body_packet(face_texture_,options,srgb);
                } else {
                    BackgroundTileOptions options;options.brightness=scene.display_brightness;
                    options.scroll_override=std::array<int16_t,2>{int16_t((r[0]+r[2])*.5F-128),int16_t((r[1]+r[3])*.5F-112)};
                    body=unique_planet_packet(p,options,{unsigned(r[0]),unsigned(r[1]),unsigned(r[2]-r[0]),unsigned(r[3]-r[1])},srgb);
                    if(!face_native_texture_ || *face_native_texture_!=body.geometry.texels)
                        face_native_texture_=std::make_shared<const std::vector<uint32_t>>(body.geometry.texels);
                    body.geometry.texels.clear();body.geometry.shared_texels=face_native_texture_;
                    body.geometry.shared_vertices=std::make_shared<const std::vector<SceneVertex>>(std::move(body.geometry.vertices));
                    body.geometry.vertices.clear();
                }
                if(i<old.size() && same_draw_geometry(std::span(&old[i],1),std::span(&body,1)))
                    body.geometry=old[i].geometry;
                body.model=body_motion(scene,scene,1.,i);bodies_.push_back(std::move(body));
            }
            // Other families cannot reuse this mixed set as their single-body cache.
            body_options_.clear();body_texture_.reset();return packet_;
        }
        std::vector<PhotographicBody> bodies;
        std::shared_ptr<const std::vector<uint32_t>> body_texture;
        if(unique_body_) {
            const auto& source=*unique_body_;
            if(unique_texture_artwork_!=source.artwork) {
                unique_texture_=make_celestial_texture(images_.get(source.artwork,loader),source.image_bounds);
                unique_texture_artwork_=source.artwork;
            }
            body_texture=unique_texture_;
            PhotographicBody body;body.diameter={source.width,source.height};
            std::array<float,4> fit{0,0,0,1};
            if(ex && !menu && palette_address_) {
                const auto address=game.map().read_native_word(palette_address_)
                    |(uint32_t(game.map().read_native_byte(palette_address_+2))<<16);
                std::array<uint16_t,112> reference{};
                for(unsigned i=0;i<reference.size();++i) reference[i]=game.map().read_native_word(address+i*2);
                render::calibrate_backdrop_palette(source.artwork,reference);
                std::array<uint8_t,256> regions{};
                for(unsigned i=1;i<15;++i) regions[source.palette_bank*16+i]=2;
                fit=render::backdrop_palette_response(reference,scene.ppu->cgram,regions)[0];
            }
            for(unsigned c=0;c<3;++c) {body.response[c]=fit[3]*brightness;body.palette_shift[c]=fit[c]*brightness;}
            bodies.push_back(body);
        } else if(city) {
            if(!moon_textures_[0]) moon_textures_[0]=make_moon_texture(images_.get(23,loader),false);
            body_texture=moon_textures_[0];
            for(const auto& source:render::city_moons) {
                PhotographicBody body;
                body.center={source[0],source[1]-scene.landscape_atlas_origin};
                body.diameter={source[2]*2,source[2]*2};
                const bool blue=source[3]!=0;body.palette[0]=blue?7:6;
                const std::array<unsigned,3> inks=blue?std::array<unsigned,3>{82,83,83}:std::array<unsigned,3>{88,87,86};
                for(unsigned i=0;i<inks.size();++i) body.palette[i+1]=scene.ppu->cgram[inks[i]]&32767;
                for(unsigned c=0;c<3;++c) body.response[c]=brightness;
                bodies.push_back(body);
            }
        } else if(fortuna || twins || storm_moons) {
            const unsigned atmospheric=fortuna?1:0;
            if(!moon_textures_[atmospheric])
                moon_textures_[atmospheric]=make_moon_texture(images_.get(23,loader),fortuna);
            body_texture=moon_textures_[atmospheric];
            if(storm_moons) {
                // Native BG_6_4 atlas centers/radii, shared with the desktop
                // presentation. Moon highlights also occur in the cloud band;
                // isolate the bodies rather than preserving that whole ink.
                uint16_t bright=0;unsigned light=0;
                for(unsigned ink=74;ink<=79;++ink) {
                    const auto colour=scene.ppu->cgram[ink]&32767;
                    const unsigned sum=(colour&31)+((colour>>5)&31)+((colour>>10)&31);
                    if(sum>light) {light=sum;bright=uint16_t(colour);}
                }
                for(unsigned i=0;i<2;++i) {
                    PhotographicBody body;
                    body.center={i?226.F:193.F,(i?271.F:240.F)+(menu?96.F:0.F)-scene.landscape_atlas_origin};
                    body.diameter={i?36.F:16.F,i?36.F:16.F};
                    for(unsigned c=0;c<3;++c) {
                        const unsigned value=(bright>>(5*c))&31;
                        body.response[c]=float((value<<3)|(value>>2))/255.F*brightness;
                    }
                    bodies.push_back(body);
                }
            } else if(fortuna) {
                PhotographicBody body;body.center={116,293.F-scene.landscape_atlas_origin};
                uint16_t bright=0;unsigned light=0;
                for(unsigned ink=97;ink<110;++ink) {
                    const auto colour=scene.ppu->cgram[ink]&32767;
                    const unsigned sum=(colour&31)+((colour>>5)&31)+((colour>>10)&31);
                    if(sum>light) {light=sum;bright=uint16_t(colour);}
                }
                for(unsigned c=0;c<3;++c) {
                    const unsigned value=(bright>>(5*c))&31;
                    body.response[c]=float((value<<3)|(value>>2))/255.F*brightness;
                }
                bodies.push_back(body);
            } else for(unsigned i=0;i<2;++i) {
                PhotographicBody body;
                body.center={i?296.F:272.F,(i?312.F:304.F)-scene.landscape_atlas_origin};
                body.diameter={i?14.F:32.F,i?14.F:32.F};body.two_tone=true;
                body.bright=scene.ppu->cgram[i?86:95]&32767;body.dark=scene.ppu->cgram[81]&32767;
                for(unsigned c=0;c<3;++c) body.response[c]=brightness;
                bodies.push_back(body);
            }
        }
        if(bodies!=body_options_ || body_srgb_!=srgb || body_texture_!=body_texture) {
            bodies_.clear();for(const auto& body:bodies)
                bodies_.push_back(photographic_body_packet(body_texture,body,srgb));
            body_options_=std::move(bodies);body_srgb_=srgb;body_texture_=std::move(body_texture);
        }
        if(unique_body_) bodies_.front().model=body_motion(scene,scene,1.);
        return packet_;
    }
};
}
