#include "starfox/render/gpu_raster.hpp"
#include "starfox/localization/bitmap_font.hpp"
#if !defined(STARFOX_TEST_RASTER_CPU)
#include <SDL3/SDL.h>
#endif
#include <chrono>
#include <iostream>
#include <string_view>
int main(int argc,char** argv) {
    using namespace starfox::render;
    if(argc==2 && std::string_view(argv[1])=="--binning-only") {
        // Measure the actual source-command distribution before selecting a
        // GPU binning representation. No GPU submission or window creation.
        starfox::assets::Shape shape;
        shape.vertices={{-90,-60,0},{-80,60,25},{90,65,0},{80,-65,-20}};
        shape.colour_words={0x006c};shape.faces={{-1,0,{0,0,127},{0,1,2,3}}};
        for(unsigned scale:{1U,2U,4U}) {
            Framebuffer frame(400,224,scale);RasterCommands batch;
            batch.reset(frame.stored_width(),frame.stored_height());frame.record_to(&batch);
            RenderSettings settings;settings.render_scale=scale;SoftwareRenderer renderer(settings);
            for(unsigned object=0;object<32;++object) {
                RenderPose pose;pose.z=100+object*13;pose.x=int(object%8)*15-50;
                pose.vanish_x=200;pose.vanish_y=112;renderer.draw(shape,pose,frame,false);
            }
            frame.record_to(nullptr);std::vector<double> samples;
            for(unsigned repeat=0;repeat<11;++repeat) {
                const auto start=std::chrono::steady_clock::now();
                for(unsigned run=0;run<32;++run) batch.bin_rows();
                const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/32;
                if(repeat>=2) samples.push_back(us);
            }
            std::sort(samples.begin(),samples.end());
            const auto tiles=std::uint64_t((batch.width()+63)/64)*batch.height();
            std::cout<<"binning scale="<<scale<<" commands="<<batch.commands.size()
                <<" tiles="<<tiles<<" references="<<batch.indices.size()
                <<" csr_bytes="<<(batch.rows.size()+batch.indices.size())*4
                <<" dense_bitset_bytes="<<tiles*((batch.commands.size()+31)/32)*4
                <<" median_us="<<samples[samples.size()/2]<<'\n';
        }
        return 0;
    }
    if(argc==2 && std::string_view(argv[1])=="--geometry-only") {
        // Isolate transformation/projection cost; no faces, pixels or GPU
        // submission. This is not an in-game frame-rate benchmark.
        starfox::assets::Shape mesh;
        for(int i=0;i<256;++i) mesh.vertices.push_back({i%31-15,i%23-11,i%17-8});
        for(unsigned scale:{1U,2U,4U}) for(bool fractional:{false,true}) {
            RenderSettings config;config.render_scale=scale;SoftwareRenderer renderer(config);
            Framebuffer frame(224,192,scale);
            RenderPose pose;pose.z=500;pose.use_rotation_matrix=true;
            pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
            pose.subpixel_projection=fractional;
            std::vector<double> samples;
            for(unsigned sample=0;sample<11;++sample) {
                const auto start=std::chrono::steady_clock::now();
                for(unsigned i=0;i<2000;++i) {pose.x=int(i%41)-20;renderer.draw(mesh,pose,frame,false);}
                const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/2000;
                if(sample>=2) samples.push_back(elapsed);
            }
            std::sort(samples.begin(),samples.end());
            std::cout<<"geometry vertices=256 scale="<<scale<<" fractional="<<fractional<<" median_us="<<samples[samples.size()/2]<<'\n';
        }
        return 0;
    }
    for(unsigned scale:{1U,2U,4U}) {
        Framebuffer foreground(9,7,scale),layer(9,7,scale);
        foreground.begin_write_coverage();foreground.set(2,3,0);
        if(std::count(foreground.write_coverage().begin(),foreground.write_coverage().end(),1)!=int(scale*scale)) return 20;
        foreground.begin_write_coverage();layer.set(1,2,17);
        composite_transparent_layer(layer,foreground,{});
        if(std::count(foreground.write_coverage().begin(),foreground.write_coverage().end(),1)!=int(scale*scale)) return 21;
        foreground.begin_write_coverage();foreground.clear();
        if(std::count(foreground.write_coverage().begin(),foreground.write_coverage().end(),1)!=int(foreground.pixels().size())) return 22;
        foreground.begin_write_coverage();foreground.copy_pixels_from(layer);
        if(std::count(foreground.write_coverage().begin(),foreground.write_coverage().end(),1)!=int(foreground.pixels().size())) return 23;
        foreground.begin_write_coverage();foreground.end_write_coverage();foreground.clear();
        if(std::count(foreground.write_coverage().begin(),foreground.write_coverage().end(),1)!=0) return 24;
    }
#if !defined(STARFOX_TEST_RASTER_CPU)
    if(!SDL_Init(SDL_INIT_VIDEO)) return 1;
    struct SdlLifetime {~SdlLifetime(){SDL_Quit();}} sdl;
    GpuRaster gpu;
    GpuRaster resident;
    GpuRaster explicitly_binned;
    if(gpu.wait_for_completion() || resident.wait_for_completion()) return 34;
#endif
    unsigned glyph_cases=0;
    for(unsigned scale:{1U,2U,3U,4U}) for(unsigned height:{8U,12U})
    for(int origin:{-5,0,19,35}) for(unsigned width:{1U,7U,16U}) {
        Framebuffer expected(40,24,scale),recorded(40,24,scale),replayed(40,24,scale);
        expected.enable_layer_tags(true);recorded.enable_layer_tags(true);replayed.enable_layer_tags(true);
        std::array<std::uint8_t,24> rows;
        for(unsigned i=0;i<24;++i) rows[i]=std::uint8_t(i*73+19);
        // Independent old per-pixel loop: do not use glyph12 as its own oracle.
        for(unsigned y=0;y<height;++y) {
            const auto row=y*11/(height-1);
            const auto bits=unsigned(rows[row*2])|(unsigned(rows[row*2+1])<<8);
            for(unsigned x=0;x<width;++x) if(bits&(0x8000U>>x)) expected.set(origin+int(x),origin/2+int(y),173);
        }
        RasterCommands batch;batch.reset(recorded.stored_width(),recorded.stored_height());
        recorded.record_to(&batch);recorded.glyph12(origin,origin/2,width,rows,173,height);recorded.record_to(nullptr);
        if(batch.commands.size()>1 || batch.texels.size()!=24) throw std::runtime_error("Glyph was not compactly recorded");
        replay_raster_commands(batch,replayed,nullptr);
        if(replayed.pixels()!=expected.pixels() || replayed.layer_tags()!=expected.layer_tags()) throw std::runtime_error("Glyph replay mismatch");
#if !defined(STARFOX_TEST_RASTER_CPU)
        if(!gpu.render(batch,recorded,nullptr) || recorded.pixels()!=expected.pixels()
            || recorded.layer_tags()!=expected.layer_tags()) throw std::runtime_error("Glyph GPU mismatch: "+gpu.status());
#endif
        ++glyph_cases;
    }
    std::cout<<glyph_cases<<" packed font cases match independent pixel decoding.\n";
    unsigned bitmap_cases=0;
    for(unsigned scale:{1U,2U,3U,4U}) for(unsigned edge:{8U,12U}) for(int origin:{-5,0,19,35}) {
        Framebuffer expected(40,24,scale),recorded(40,24,scale),replayed(40,24,scale);
        expected.enable_layer_tags(true);recorded.enable_layer_tags(true);replayed.enable_layer_tags(true);
        RasterCommands batch;batch.reset(recorded.stored_width(),recorded.stored_height());recorded.record_to(&batch);
        for(const auto code:{U'?',U'\u00e9',U'\u3042',U'\u65e5'}) {
            const auto* glyph=starfox::localization::glyph(code);
            if(!glyph) continue;
            for(unsigned row=0;row<edge;++row) for(unsigned column=0;column<edge;++column)
                if(glyph->rows[row*8/edge]&(0x80U>>(column*8/edge))) expected.set(origin+int(column),origin/2+int(row),149);
            recorded.glyph8(origin,origin/2,glyph->rows,149,edge);
        }
        recorded.record_to(nullptr);replay_raster_commands(batch,replayed,nullptr);
        if(replayed.pixels()!=expected.pixels() || replayed.layer_tags()!=expected.layer_tags()) throw std::runtime_error("Bitmap font replay mismatch");
#if !defined(STARFOX_TEST_RASTER_CPU)
        if(!gpu.render(batch,recorded,nullptr) || recorded.pixels()!=expected.pixels()
            || recorded.layer_tags()!=expected.layer_tags()) throw std::runtime_error("Bitmap font GPU mismatch: "+gpu.status());
#endif
        ++bitmap_cases;
    }
    std::cout<<bitmap_cases<<" localized bitmap font cases match independent pixel decoding.\n";
    unsigned portrait_cases=0;
    for(unsigned scale:{1U,2U,3U,4U}) for(bool aspect:{false,true}) for(int origin:{-9,0,35}) {
        Framebuffer expected(64,56,scale),recorded(64,56,scale),replayed(64,56,scale);
        expected.enable_layer_tags(true);recorded.enable_layer_tags(true);replayed.enable_layer_tags(true);
        std::array<std::uint8_t,640> bytes;
        for(unsigned i=0;i<bytes.size();++i) bytes[i]=std::uint8_t(i*73+19);
        const auto edge=[&](int value){return (value*int(scale)*7+3)/6;};
        const int left=aspect?(origin+32)*int(scale)-edge(32):origin*int(scale);
        const auto tag=aspect?PixelLayer::three_d:PixelLayer::two_d;
        // Source forward expansion is independent of the GPU's inverse mapping.
        for(int sy=0;sy<40;++sy) for(int sx=0;sx<32;++sx) {
            const auto at=((sx/8)*5+sy/8)*32+(sy%8)*2;unsigned ink=0;
            for(unsigned plane=0;plane<4;++plane) ink|=((bytes[at+(plane/2)*16+plane%2]>>(7-sx%8))&1U)<<plane;
            for(int y=(origin+sy)*int(scale);y<(origin+sy+1)*int(scale);++y)
                for(int x=left+(aspect?edge(sx):sx*int(scale));x<left+(aspect?edge(sx+1):(sx+1)*int(scale));++x)
                    expected.set_stored(x,y,std::uint8_t(240+ink),tag);
        }
        RasterCommands batch;batch.reset(recorded.stored_width(),recorded.stored_height());
        RasterCommand c;c.left=c.u=left;c.right=(origin+32)*int(scale);
        c.top=c.v=origin*int(scale);c.bottom=(origin+40)*int(scale);
        c.du=scale;c.dv=aspect;c.textured=8;c.colour_base=240;c.tag=std::uint32_t(tag);
        c.texture_offset=batch.snapshot(bytes);batch.add(c);
        replay_raster_commands(batch,replayed,nullptr);
        if(replayed.pixels()!=expected.pixels() || replayed.layer_tags()!=expected.layer_tags()) throw std::runtime_error("Portrait replay mismatch");
#if !defined(STARFOX_TEST_RASTER_CPU)
        if(!gpu.render(batch,recorded,nullptr) || recorded.pixels()!=expected.pixels()
            || recorded.layer_tags()!=expected.layer_tags()) throw std::runtime_error("Portrait GPU mismatch: "+gpu.status());
#endif
        ++portrait_cases;
    }
    std::cout<<portrait_cases<<" packed portrait cases match independent forward expansion.\n";
    starfox::assets::Shape shape;
    shape.vertices={{-90,-60,0},{-80,60,25},{90,65,0},{80,-65,-20}};
    shape.colour_words={0x0009,0x006c,0x4000};
    shape.faces={{-1,1,{0,0,127},{0,1,2,3}},{-1,1,{0,0,127},{3,2,1,0}},
        {-1,0,{0,0,127},{0,2}}};
    starfox::assets::TextureImage texture;
    texture.descriptor=0x4000;texture.u_mask=texture.v_mask=7;
    texture.coordinates={{{0,0},{0,7},{7,7},{7,0}}};texture.texels.resize(64);
    for(unsigned i=0;i<64;++i) texture.texels[i]=std::uint8_t(i%7);
    shape.textures.push_back(texture);
    const auto faces=shape.faces;
#if !defined(STARFOX_TEST_RASTER_CPU)
    if(argc==2 && (std::string_view(argv[1])=="--compare-bins"
        || std::string_view(argv[1])=="--compare-resident-bins"
        || std::string_view(argv[1])=="--compare-mixed-bins")) {
        const bool mixed=std::string_view(argv[1])=="--compare-mixed-bins";
        const bool resident_only=mixed || std::string_view(argv[1])=="--compare-resident-bins";
        auto* environment=SDL_GetEnvironment();
        if(!SDL_SetEnvironmentVariable(environment,"STARFOX_TEST_GPU_BINS","1",true)) return 27;
        // Initialize both pipelines before alternation; the same device,
        // command data and readback are used for each paired measurement.
        for(unsigned workload=mixed?1U:0U;workload<=(mixed?2U:0U);++workload)
        for(unsigned scale:{1U,2U,4U}) for(bool metadata:{false,true}) {
            shape.faces=faces;
            if(workload) shape.faces[0].colour_id=shape.faces[1].colour_id=2;
            Framebuffer frame(400,224,scale),reference(400,224,scale);
            frame.enable_layer_tags(true);reference.enable_layer_tags(true);
            SurfaceBuffer surfaces(400*scale,224*scale),expected(400*scale,224*scale);
            RasterCommands batch;batch.reset(frame.stored_width(),frame.stored_height());
            frame.record_to(&batch);RenderSettings config;config.render_scale=scale;SoftwareRenderer renderer(config);
            for(unsigned object=0;object<32;++object) {
                RenderPose pose;pose.z=100+object*13;pose.x=int(object%8)*15-50;
                pose.vanish_x=200;pose.vanish_y=112;
                if(workload) {pose.texture_scroll_x=-3;pose.texture_scroll_y=260;}
                if(workload==2 && object%4!=0) {
                    pose.simple_scaled_sprite=true;pose.simple_sprite_colour=2;pose.simple_sprite_world_size=80;
                    if(object%3==0) pose.palette_override=203;
                }
                renderer.draw(shape,pose,frame,false,metadata?&surfaces:nullptr);
            }
            frame.record_to(nullptr);replay_raster_commands(batch,reference,metadata?&expected:nullptr);
            if(workload && (batch.texels.empty()
                || std::none_of(batch.commands.begin(),batch.commands.end(),[](const auto& c){return c.textured!=0;}))) return 36;
            if(workload==2 && std::none_of(batch.commands.begin(),batch.commands.end(),[](const auto& c){return c.textured==2;})) return 37;
            if(!SDL_SetEnvironmentVariable(environment,"STARFOX_TEST_GPU_BINS","1",true)
                || !gpu.render(batch,frame,metadata?&surfaces:nullptr)) return 28;
            std::vector<double> samples[2];
            for(unsigned repeat=0;repeat<13;++repeat) for(unsigned order=0;order<2;++order) {
                const auto mode=(repeat+order)%2;
                const bool configured=mode?SDL_SetEnvironmentVariable(environment,"STARFOX_TEST_GPU_BINS","1",true)
                    :SDL_UnsetEnvironmentVariable(environment,"STARFOX_TEST_GPU_BINS");
                if(!configured) return 29;
                const auto start=std::chrono::steady_clock::now();
                if(resident_only) {
                    if(!gpu.render_resident(gpu.resident_output().device,batch,metadata)
                        || !gpu.wait_for_completion()) return 30;
                } else if(!gpu.render(batch,frame,metadata?&surfaces:nullptr)) return 30;
                const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                if(resident_only) {
                    const auto completed=gpu.resident_output();
                    if(!gpu.wait_for_completion() || gpu.resident_output().generation!=completed.generation
                        || gpu.resident_output().pixels!=completed.pixels) return 35;
                }
                if(resident_only && !gpu.readback(frame,metadata?&surfaces:nullptr)) return 33;
                if(frame.pixels()!=reference.pixels() || frame.layer_tags()!=reference.layer_tags()) return 31;
                if(metadata) for(std::size_t i=0;i<expected.samples().size();++i) {
                    const auto& a=expected.samples()[i];const auto& b=surfaces.samples()[i];
                    if(a.valid!=b.valid || a.palette_index!=b.palette_index || a.depth!=b.depth
                        || a.normal_x!=b.normal_x || a.normal_y!=b.normal_y || a.normal_z!=b.normal_z) return 32;
                }
                if(repeat>=4) samples[mode].push_back(ms);
            }
            for(auto& sample:samples) std::sort(sample.begin(),sample.end());
            std::cout<<"paired bins workload="<<workload<<" resident="<<resident_only<<" scale="<<scale<<" surfaces="<<metadata<<" commands="<<batch.commands.size()
                <<" cpu_bins_ms="<<samples[0][4]<<" gpu_bins_ms="<<samples[1][4]<<'\n';
        }
        return 0;
    }
#endif
    {
        RasterCommands edges;edges.reset(8,2);RasterCommand edge;
        edge.left=0;edge.right=8;edge.top=0;edge.bottom=1;edge.even=7;edge.reserved1=1;edge.u=-1;edge.v=7;edges.add(edge);
        edge.top=1;edge.bottom=2;edge.u=2;edge.v=5;edges.add(edge);
        Framebuffer expected(8,2),actual(8,2);expected.set(7,0,7);expected.set(2,1,7);expected.set(5,1,7);
        replay_raster_commands(edges,actual,nullptr);
        if(actual.pixels()!=expected.pixels()) throw std::runtime_error("Edge-only replay coverage failed");
#if !defined(STARFOX_TEST_RASTER_CPU)
        if(!gpu.render(edges,actual,nullptr) || actual.pixels()!=expected.pixels()) throw std::runtime_error("Edge-only GPU coverage failed");
#endif
    }
    {
        RasterCommands masked;masked.reset(67,3);masked.texels.resize(40);
        RasterCommand c;c.right=67;c.bottom=3;c.even=0;c.odd=9;c.dither=1;
        c.tag=std::uint32_t(PixelLayer::three_d);c.reserved1=4;c.texture_offset=4;c.u_mask=12;c.v_mask=3;
        c.has_surface=1;c.surface={0,0,1,123};
        Framebuffer expected(67,3),actual(67,3);
        SurfaceBuffer mask_surfaces(67,3);
        const auto check_mask_surfaces=[&] {
            for(unsigned y=0;y<3;++y) for(unsigned x=0;x<67;++x) {
                const auto& s=mask_surfaces.samples()[y*67+x];
                const bool covered=x==0 || x==31 || x==32 || x==66;
                if(s.valid!=covered || (covered && (s.palette_index!=(((x^y)&1)?9:0)
                    || s.normal_x!=0 || s.normal_y!=0 || s.normal_z!=1 || s.depth!=123)))
                    throw std::runtime_error("Masked solid surface ownership mismatch");
            }
        };
        expected.enable_layer_tags(true);actual.enable_layer_tags(true);
        for(unsigned y=0;y<3;++y) for(unsigned x:{0U,31U,32U,66U}) {
            masked.texels[4+y*12+x/8]|=std::uint8_t(1U<<(x&7));
            expected.set_stored(x,y,((x^y)&1)?9:0,PixelLayer::three_d);
        }
        masked.add(c);
        replay_raster_commands(masked,actual,&mask_surfaces);check_mask_surfaces();
        if(actual.pixels()!=expected.pixels() || actual.layer_tags()!=expected.layer_tags())
            throw std::runtime_error("Masked solid replay lost gaps, zero writes or word boundaries");
#if !defined(STARFOX_TEST_RASTER_CPU)
        if(!gpu.render(masked,actual,&mask_surfaces) || actual.pixels()!=expected.pixels() || actual.layer_tags()!=expected.layer_tags())
            throw std::runtime_error("Masked solid GPU coverage mismatch");
        check_mask_surfaces();
#endif
        for(unsigned invalid=0;invalid<5;++invalid) {
            auto bad=c;
            if(invalid==0) bad.u_mask=0;
            if(invalid==1) bad.u_mask=2;
            if(invalid==2) bad.texture_offset=1;
            if(invalid==3) bad.v_mask=4;
            if(invalid==4) bad.texture_offset=0xfffffffcU;
            masked.commands={bad};expected.clear();
            replay_raster_commands(masked,actual,nullptr);
            if(actual.pixels()!=expected.pixels() || actual.layer_tags()!=expected.layer_tags())
                throw std::runtime_error("Invalid solid mask replay was not rejected");
#if !defined(STARFOX_TEST_RASTER_CPU)
            if(!gpu.render(masked,actual,nullptr) || actual.pixels()!=expected.pixels() || actual.layer_tags()!=expected.layer_tags())
                throw std::runtime_error("Invalid solid mask GPU was not rejected");
#endif
        }
    }
    for(unsigned width:{224U,257U}) for(unsigned scale:{1U,2U,4U}) for(unsigned mode=0;mode<14;++mode) for(bool textured:{false,true}) {
        shape.faces=faces;
        Framebuffer reference(width,193,scale),recorded(width,193,scale),replayed(width,193,scale);
        for(auto* frame:{&reference,&recorded,&replayed}) frame->enable_layer_tags(true);
        SurfaceBuffer expected(width*scale,193*scale),actual(width*scale,193*scale),software(width*scale,193*scale);
        RenderSettings settings;settings.render_scale=scale;SoftwareRenderer renderer(settings);
        RenderPose pose;pose.z=mode==9?40:280;pose.vanish_x=128;pose.texture_scroll_x=-3;pose.texture_scroll_y=260;
        pose.wireframe_mode=mode<3?mode:0;pose.wobble_mode=mode>=6?mode-6:0;
        pose.cel_mode=mode==4;pose.wave_mode=mode==5;pose.wave_offset=-17;
        pose.continuous_geometry=mode==8;shape.faces[0].colour_id=shape.faces[1].colour_id=textured?2:1;
        pose.use_rotation_matrix=mode==7 || mode==8;
        pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
        if(mode==10) shape.faces={{-1,2,{0,0,127},{0},true}};
        if(mode>=11) {
            pose.simple_scaled_sprite=true;pose.simple_sprite_colour=2;pose.simple_sprite_world_size=80;
            if(mode==12) pose.palette_override=203;
            if(mode==13) {pose.effect_clip_left=110;pose.effect_clip_right=140;}
        }
        RasterCommands batch;batch.reset(recorded.stored_width(),recorded.stored_height());recorded.record_to(&batch);
        renderer.draw(shape,pose,reference,false,&expected);
        renderer.draw(shape,pose,recorded,false,&actual);
        if(mode==7 && !textured) {
            std::vector<unsigned> row_writes(recorded.stored_height());
            for(const auto& command:batch.commands)
                for(int row=std::max(0,command.top);row<std::min(int(row_writes.size()),command.bottom);++row)
                    ++row_writes[std::size_t(row)];
            if(std::none_of(row_writes.begin(),row_writes.end(),[&](unsigned writes){return writes>shape.faces.size();}))
                throw std::runtime_error("Repeated-row wobble fixture no longer exercises multiple spans per face-row");
        }
        // A later non-surface write must not erase underlying metadata.
        reference.set(127,96,3);recorded.set(127,96,3);
        recorded.record_to(nullptr);
        replay_raster_commands(batch,replayed,&software);
        if(reference.pixels()!=replayed.pixels() || reference.layer_tags()!=replayed.layer_tags()) {
            std::cerr<<"CPU command parity failed scale="<<scale<<" mode="<<mode<<" textured="<<textured<<'\n';return 2;
        }
#if !defined(STARFOX_TEST_RASTER_CPU)
        if(!gpu.render(batch,recorded,&actual)) {std::cerr<<gpu.status()<<'\n';return 3;}
        if(reference.pixels()!=recorded.pixels() || reference.layer_tags()!=recorded.layer_tags()) {
            std::cerr<<"GPU command parity failed scale="<<scale<<" mode="<<mode<<" textured="<<textured<<'\n';return 4;
        }
        Framebuffer deferred(width,193,scale);deferred.enable_layer_tags(true);
        deferred.clear(233);
        SurfaceBuffer deferred_surfaces(width*scale,193*scale);
        const auto device=gpu.resident_output().device;
        if(!resident.render_resident(device,batch,true)) {std::cerr<<resident.status()<<'\n';return 7;}
        const auto first=resident.resident_output();
        if(!first.pixels || !first.surfaces || first.device!=device || first.width!=width*scale
            || first.height!=193*scale || deferred.pixels().front()!=233) return 8;
        // Replacing an unread CPU-binned frame with GPU bins is legal;
        // exported views are frame-scoped. The next case switches back.
        if(!resident.render_resident(device,batch,true,true)
            || resident.resident_output().generation<=first.generation
            || !resident.readback(deferred,&deferred_surfaces)
            || reference.pixels()!=deferred.pixels() || reference.layer_tags()!=deferred.layer_tags()) return 9;
#endif
        std::vector<const SurfaceBuffer*> comparisons{&software};
#if !defined(STARFOX_TEST_RASTER_CPU)
        comparisons.push_back(&actual);
        comparisons.push_back(&deferred_surfaces);
#endif
        for(std::size_t i=0;i<expected.samples().size();++i) for(const auto* samples:comparisons) {
            const auto& a=expected.samples()[i];const auto& b=samples->samples()[i];
            if(a.valid!=b.valid || a.palette_index!=b.palette_index || a.normal_x!=b.normal_x || a.normal_y!=b.normal_y
                || a.normal_z!=b.normal_z || a.depth!=b.depth) {std::cerr<<"Surface parity failed\n";return 5;}
        }
    }
    shape.faces=faces;
#if !defined(STARFOX_TEST_RASTER_CPU)
    std::cout<<gpu.status()<<": 168 native flat/texture/line/wave/wobble/clip/sprite cases exactly match pixels, tags and surfaces\n";
    // No-metadata frames must not expose the previous frame's surface buffer.
    // Exercise shrinking/growing allocations and repeated explicit captures.
    for(unsigned width:{65U,17U,257U}) {
        Framebuffer expected(width,19),captured(width,19);
        RasterCommands batch;batch.reset(width,19);expected.record_to(&batch);
        expected.set(3,4,71);expected.record_to(nullptr);
        replay_raster_commands(batch,expected,nullptr);
        // Start with CPU bins, lazily enable GPU bins on the same device, then
        // alternate without resetting allocations or exposing stale ownership.
        for(bool binning:{false,true,false,true}) {
            if(!explicitly_binned.render_resident(gpu.resident_output().device,batch,false,binning)
                || !explicitly_binned.readback(captured,nullptr) || captured.pixels()!=expected.pixels()) return 38;
        }
        if(!resident.render_resident(gpu.resident_output().device,batch,false)
            || resident.resident_output().surfaces
            || !resident.readback(captured,nullptr)
            || captured.pixels()!=expected.pixels()) return 11;
        captured.clear(255);
        if(!resident.readback(captured,nullptr) || captured.pixels()!=expected.pixels()) return 12;
    }
    {
        RasterCommands empty;empty.reset(65,19);
        Framebuffer captured(65,19);captured.enable_layer_tags(true);captured.clear(255);
        SurfaceBuffer surfaces(65,19);
        if(!resident.render_resident(gpu.resident_output().device,empty,true)
            || !resident.readback(captured,&surfaces)) return 25;
        if(std::any_of(captured.pixels().begin(),captured.pixels().end(),[](auto v){return v!=0;})
            || std::any_of(surfaces.samples().begin(),surfaces.samples().end(),[](const auto& v){return v.valid;})) return 26;
    }
    {
        // Queue enough changing unread frames to wrap the bounded submission
        // queue repeatedly. Grow/shrink buffers and alternate bin ownership.
        for(unsigned n=0;n<32;++n) {
            RasterCommands queued;queued.reset(65+(n%3)*64,19);
            RasterCommand c;c.left=2;c.top=3;c.right=6;c.bottom=7;c.even=n+1;
            c.has_surface=1;c.surface={0,0,1,float(n)};queued.add(c);
            if(!resident.render_resident(gpu.resident_output().device,queued,true,(n&1)!=0)) return 41;
        }
        Framebuffer captured(129,19);SurfaceBuffer surfaces(129,19);
        if(!resident.readback(captured,&surfaces)) return 42;
        for(unsigned y=0;y<19;++y) for(unsigned x=0;x<129;++x) {
            const auto i=y*129+x;const bool covered=x>=2 && x<6 && y>=3 && y<7;
            if(captured.pixels()[i]!=(covered?32:0) || surfaces.samples()[i].valid!=covered
                || (covered && surfaces.samples()[i].depth!=31)) return 43;
        }
        std::cout<<"32 changing unread submissions: bounded queue, bin switching, resized buffers and final capture passed\n";
    }
    resident.release_device();
    if(resident.resident_output().pixels || resident.wait_for_completion()) return 10;
    std::cout<<"Borrowed-device resident output, unread replacement, deferred capture and release: passed\n";
    for(unsigned scale:{1U,2U,4U}) {
        Framebuffer frame(400,224,scale);frame.enable_layer_tags(true);
        RenderSettings settings;settings.render_scale=scale;SoftwareRenderer renderer(settings);
        RasterCommands batch;double cpu=0,hardware=0;
        for(unsigned repeat=0;repeat<12;++repeat) for(unsigned order=0;order<2;++order) {
            const bool use_gpu=(repeat+order)%2;
            frame.clear();batch.reset(frame.stored_width(),frame.stored_height());
            frame.record_to(use_gpu?&batch:nullptr);
            const auto start=std::chrono::steady_clock::now();
            for(unsigned object=0;object<32;++object) {
                RenderPose pose;pose.z=100+object*13;pose.x=int(object%8)*15-50;
                pose.vanish_x=200;pose.vanish_y=112;renderer.draw(shape,pose,frame,false);
            }
            frame.record_to(nullptr);
            if(use_gpu && !gpu.render(batch,frame,nullptr)) return 6;
            const auto duration=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(repeat>=2) (use_gpu?hardware:cpu)+=duration;
        }
        std::cout<<"scale="<<scale<<" CPU_ms="<<cpu/10<<" GPU_ms="<<hardware/10<<'\n';
    }
#else
    std::cout<<"168 native raster command replay cases match source renderer exactly\n";
#endif
}
