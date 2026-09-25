#include "starfox/render/software_reflections.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>

void require(bool value,const char* message) {
    if(!value) {std::cerr<<message<<'\n';std::exit(1);}
}
int main(int argc,char**) {
    using namespace starfox::render;
    using namespace starfox::render::shadows;
    SurfaceBuffer transient_surfaces(32,32);
    require(transient_surfaces.allocated_bytes()>=32U*32U*sizeof(SurfaceSample),
        "Surface buffer did not allocate its samples");
    transient_surfaces.resize(0,0);
    require(transient_surfaces.allocated_bytes()==0 && transient_surfaces.samples().empty(),
        "Disabled surface effects retained sample storage");
    transient_surfaces.resize(16,16);
    require(transient_surfaces.samples().size()==256,
        "Surface samples did not reallocate after enabling effects");
    Scene scene;
    // A mirror in front of the camera reflects a green model BEHIND it.
    // This must work without screen-space colour/depth or any GPU device.
    scene.add({{-100,-100,10},{100,-100,10},{100,100,10},1,1,true});
    scene.add({{-100,-100,10},{100,100,10},{-100,100,10},1,1,true});
    scene.add({{-100,-100,-10},{100,100,-10},{100,-100,-10},2,2,true});
    scene.add({{-100,-100,-10},{-100,100,-10},{100,100,-10},2,2,true});
    // Force BVH sorting; materials must remain attached to their geometry.
    for(unsigned i=0;i<16;++i) scene.add({{200.+i,0,0},{200.+i,1,0},{200.+i,0,1},3,3,true});
    scene.build();
    const auto hit=scene.nearest_hit({},{0,0,1},.1,100,true);
    require(hit && hit->distance==10 && scene.triangles()[hit->triangle].reflection_even==1,"BVH lost material association");
    Palette256 palette{};palette[1]={200,20,20,255};palette[2]={0,200,0,255};palette[3]={0,0,240,255};
    for(unsigned scale:{1U,2U,4U}) {
        Framebuffer frame(16*scale,16*scale);frame.set_draw_scale(scale);frame.enable_layer_tags(true);
        Framebuffer background(16*scale,16*scale);background.clear(3);
        SurfaceBuffer surfaces(16*scale,16*scale);
        frame.clear(1);
        for(unsigned y=0;y<surfaces.height();++y) for(unsigned x=0;x<surfaces.width();++x)
            surfaces.set(x,y,{0,0,-1,10},1);
        frame.layer_tags()[0]=static_cast<std::uint8_t>(PixelLayer::two_d); // HUD, even with matching palette.
        frame.layer_tags()[1]=static_cast<std::uint8_t>(PixelLayer::world_geometry); // stars/beams.
        frame.pixels()[2]=3; // A later overlay owns this pixel.
        frame.pixels()[3]=4; // Tiny second material inside a coarse cell.
        surfaces.set(3,0,{.1F,0,-.995F,10},4);
        std::vector<std::uint8_t> source;expand_rgba(frame,source,palette);
        SoftwareReflectionSettings settings{{16*scale,16*scale,16.*scale,8.*scale,8.*scale},0,0,0,100};
        auto off=source;apply_software_reflections(scene,surfaces,frame,&background,palette,off,settings);
        require(off==source,"OFF changed the frame");
        RowWorkers workers;workers.set_worker_count(3);
        for(unsigned quality=1;quality<=3;++quality) {
            settings.quality=quality;auto result=source;
            apply_software_reflections(scene,surfaces,frame,&background,palette,result,settings,&workers);
            for(unsigned i=0;i<3;++i) for(unsigned c=0;c<4;++c)
                require(result[i*4+c]==source[i*4+c],"reflection touched HUD/beam/overwritten pixel");
            for(std::size_t i=3;i<frame.pixels().size();++i)
                require(result[i*4]==0 && result[i*4+1]==200 && result[i*4+2]==0 && result[i*4+3]==255,"single-bounce offscreen model reflection missing");
            auto serial=source;apply_software_reflections(scene,surfaces,frame,&background,palette,serial,settings);
            require(serial==result,"parallel reflection differs from serial");
        }
        for(unsigned metal:{2U,3U}) {
            settings.metallic=metal;auto result=source;
            apply_software_reflections(scene,surfaces,frame,&background,palette,result,settings);
            require(result[4*4]==0 && result[4*4+1]==200*(metal==2?223:204)/255 && result[4*4+2]==0,
                "gold/copper must tint actual reflected geometry, independent of the red source material");
        }
        settings.metallic=0;
        settings.quality=3;settings.offset_y=4*scale;
        auto shifted=source;apply_software_reflections(scene,surfaces,frame,&background,palette,shifted,settings);
        require(std::equal(source.begin(),source.begin()+std::size_t(16*scale)*4*scale*4,shifted.begin()),"surface offset shaded outside model region");
        settings.offset_y=0;settings.camera.focal_length=0;
        auto invalid=source;apply_software_reflections(scene,surfaces,frame,&background,palette,invalid,settings);
        require(invalid==source,"invalid projection changed image");
        settings.camera.focal_length=16.*scale;
        Scene mirror;mirror.add({{-100,-100,10},{100,-100,10},{100,100,10},1,1,true});
        mirror.add({{-100,-100,10},{100,100,10},{-100,100,10},1,1,true});mirror.build();
        auto sky=source;apply_software_reflections(mirror,surfaces,frame,&background,palette,sky,settings);
        require(sky[3*4]==0 && sky[3*4+2]==240,"miss ray did not reflect background probe");
        background.enable_layer_tags(true);
        std::fill(background.layer_tags().begin(),background.layer_tags().end(),static_cast<std::uint8_t>(PixelLayer::two_d));
        auto hud=source;apply_software_reflections(mirror,surfaces,frame,&background,palette,hud,settings);
        require(hud[3*4]==0 && hud[3*4+1]==0 && hud[3*4+2]==0,"environment probe included UI text");
    }
    Scene excluded;excluded.add({{-100,-100,10},{100,-100,10},{0,100,10}});excluded.build();
    require(excluded.nearest({}, {0,0,1}) && !excluded.nearest_hit({}, {0,0,1},.1,100,true),"reflection exclusion changed shadow casters");
    std::cout<<"Software reflection geometry, quality, upscale, ownership, projection and worker checks passed\n";
    if(argc>1) {
        Framebuffer frame(640,448);frame.set_draw_scale(2);frame.enable_layer_tags(true);frame.clear(1);
        SurfaceBuffer surfaces(640,448);
        for(unsigned y=0;y<448;++y) for(unsigned x=0;x<640;++x) surfaces.set(x,y,{0,0,-1,10},1);
        std::vector<std::uint8_t> source;expand_rgba(frame,source,palette);
        RowWorkers workers;workers.set_worker_count(4);
        for(unsigned quality=1;quality<=3;++quality) {
            SoftwareReflectionSettings settings{{640,448,512,320,224},0,0,quality,60};
            double elapsed=0;auto pixels=source;
            for(unsigned i=0;i<25;++i) {
                pixels=source;
                const auto start=std::chrono::steady_clock::now();
                apply_software_reflections(scene,surfaces,frame,nullptr,palette,pixels,settings,&workers);
                if(i>=5) elapsed+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            }
            std::cout<<"CPU reflection benchmark quality="<<quality<<" ms="<<elapsed/20
                <<" (640x448, all pixels reflective, 20 triangles, 4 workers, pass only)\n";
        }
    }
}
