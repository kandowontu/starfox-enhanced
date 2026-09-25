#include "starfox/render/enhanced_terrain.hpp"
#include "starfox/render/environment_effects.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/packed_faces.hpp"
#include "starfox/render/packed_bsp.hpp"
#include <iostream>
#include <stdexcept>
static void require(bool yes,const char* message){if(!yes) throw std::runtime_error(message);}
int main() {
    using namespace starfox::render;
    {
        Framebuffer ground(4,4);ground.enable_layer_tags(true);
        std::fill(ground.layer_tags().begin(),ground.layer_tags().end(),
            std::uint8_t(PixelLayer::background));
        ground.layer_tags()[5]=std::uint8_t(PixelLayer::terrain_geometry);
        ground.layer_tags()[6]=std::uint8_t(PixelLayer::two_d);
        std::vector<std::uint8_t> image(4*4*4,100);
        for(unsigned i=0;i<16;++i) image[i*4+3]=255;
        EnvironmentEffects auto_ground;auto_ground.modes[0]=1;
        auto_ground.plane[1]=1;auto_ground.motion[0]=1;
        apply_environment(auto_ground,ground,image);
        require(image[0]==100 && image[4*4]!=100,
            "Auto ground failed to fill only the unclassified horizon band");
        require(image[5*4]==100 && image[6*4]==100,
            "Auto ground recoloured a world-space terrain mesh or 2D HUD");
    }
    {
        BackdropImage atlas{8,8,std::vector<std::uint32_t>(64,0xffffffff)};
        EnvironmentEffects e;e.backdrop=&atlas;e.modes[2]=1;e.plane[3]=1;e.motion[0]=48;
        e.backdrop_projection={1/512.f,1/160.f,1,8};
        e.classes[82]=7;e.backdrop_ramp[6]=2;
        e.backdrop_ramp[7]=0x786878;e.backdrop_ramp[10]=0xff6818;
        Framebuffer f(64,48);f.enable_layer_tags(true);
        f.pixels().assign(64*48,82); // The same ink is used for stars and a moon.
        for(unsigned i=0;i<f.layer_tags().size();++i) f.layer_tags()[i]=i%6;
        for(const auto& body:city_moons) {
            e.backdrop_keep[1]={body[0],body[1]-24,0,0};
            std::vector<std::uint8_t> rgba(64*48*4,173);
            apply_environment(e,f,rgba);
            for(unsigned y=0;y<48;++y) for(unsigned x=0;x<64;++x) {
                const auto i=y*64+x;
                const bool replaced=f.layer_tags()[i]==unsigned(PixelLayer::background)
                    && city_moon_coordinates(float(x)+.5f-32,float(y)+.5f,e.backdrop_keep[1]).has_value();
                require((rgba[i*4]!=173)==replaced,
                    "City moon replacement erased shared-ink stars or changed a protected layer");
            }
            e.modes[2]=0;rgba.assign(rgba.size(),173);apply_environment(e,f,rgba);
            require(std::all_of(rgba.begin(),rgba.end(),[](auto c){return c==173;}),
                "Disabled city moon enhancement still alters native rendering");
            e.modes[2]=1;
        }
    }
    require(shadowless_space_background("BG_1_2")
        && shadowless_space_background("BG_2_5")
        && shadowless_space_background("BG_6_7C")
        && shadowless_space_background("BG_COMET",true)
        && !shadowless_space_background("BG_TRAINING")
        && shadowless_space_background("BG_TRAINING",true)
        && !shadowless_space_background("BG_1_1C")
        && !shadowless_space_background("BG_6_7B"),
        "Space shadow exclusion misclassified a physical ground receiver");
    {
        Framebuffer frame(256,1);frame.enable_layer_tags(true);
        frame.layer_tags().assign(256,std::uint8_t(PixelLayer::background));
        std::vector<std::uint8_t> source(256*4,173);
        for(unsigned i=0;i<256;++i) frame.pixels()[i]=std::uint8_t(i);
        BackdropImage sky{2,2,std::vector<std::uint32_t>(4,0xff123456)};
        for(unsigned artwork:{6U,13U}) {
            EnvironmentEffects effect;effect.backdrop=&sky;effect.modes[2]=1;
            effect.motion[0]=224;effect.plane[3]=1;
            preserve_backdrop_celestial_ink(artwork,effect.classes);
            auto actual=source;apply_environment(effect,frame,actual);
            for(unsigned i=0;i<256;++i) {
                const bool celestial=artwork==6?ex_city_sky_detail(i):fortuna_moon_ink(i);
                require((actual[i*4]==source[i*4])==celestial,
                    "Enhanced backdrop erased moon/star ink or retained surrounding sky tiles");
            }
        }
        std::array<std::uint32_t,256> unrelated{};
        EnvironmentEffects game_over;game_over.backdrop=&sky;game_over.modes[2]=1;
        game_over.backdrop_projection={1/672.f,1/224.f,0,2};
        game_over_backdrop_classes(game_over.classes);
        auto actual=source;apply_environment(game_over,frame,actual);
        for(unsigned i=0;i<256;++i)
            require((actual[i*4]==source[i*4])==(i!=0 && i!=74),
                "Game Over sky erased Andross/star ink or failed to replace blank space");
        preserve_backdrop_celestial_ink(18,unrelated);
        require(unrelated==std::array<std::uint32_t,256>{},
            "Moon preservation leaked into a different city atlas");
    }
    {
        Framebuffer frame(2,1);frame.enable_layer_tags(true);
        frame.layer_tags().assign(2,std::uint8_t(PixelLayer::background));
        frame.pixels()[0]=1;frame.pixels()[1]=2;
        BackdropImage sky{2,2,std::vector<std::uint32_t>(4,0xff808080)};
        EnvironmentEffects e;e.backdrop=&sky;e.modes[2]=1;e.plane[3]=1;
        e.motion[0]=0;e.classes[1]=6;e.classes[2]=1;
        e.backdrop_palette[0]={0,0,0,.25f};e.backdrop_palette[1]={0,0,0,1};
        std::vector<std::uint8_t> pixels(8,173);apply_environment(e,frame,pixels);
        require(pixels[0]==32 && pixels[4]==173,
            "Sky below a fitted horizon adopted ground tint or overwrote ground");
    }
    {
        Framebuffer f(3,1);f.enable_layer_tags(true);
        f.layer_tags()={2,2,1};
        const std::vector<std::uint8_t> source{0,0,0,255,100,100,100,255,255,0,0,255};
        EnvironmentEffects scroll;scroll.scroll_fraction={.5f,0,0,0};
        auto pixels=source;apply_environment(scroll,f,pixels);
        require(pixels[0]==50 && pixels[4]==100 && pixels[8]==255 && pixels[9]==0,
            "Fractional BG scroll lost intermediate colour or moved/bleeded foreground text");
        scroll.scroll_fraction={0,0,0,0};pixels=source;apply_environment(scroll,f,pixels);
        require(pixels==source,"Inactive menu scroll changed source pixels");
    }
    {
        Framebuffer f(1,2);f.enable_layer_tags(true);
        f.layer_tags().assign(2,std::uint8_t(PixelLayer::background));
        f.pixels()={1,2}; // Ground above and sky below the estimated bank line.
        BackdropImage photo{2,2,std::vector<std::uint32_t>(4,0xff2080e0U)};
        EnvironmentEffects e;e.backdrop=&photo;e.modes[2]=1;
        e.classes[1]=1;e.classes[2]=6;e.motion[0]=1;e.plane[3]=1;
        std::vector<std::uint8_t> pixels{40,90,40,255,20,30,60,255};
        apply_environment(e,f,pixels);
        require(pixels[0]==40 && pixels[1]==90 && pixels[2]==40,
            "Photo sky painted over source ground at a banked seam");
        require(pixels[4]==224 && pixels[5]==128 && pixels[6]==32,
            "Source sky below the estimated bank line retained the old backdrop");
    }
    {
        std::array<std::uint16_t,16> fog{},clear{};
        for(unsigned i=1;i<16;++i) {
            fog[i]=std::uint16_t(23|(27<<5)|(31<<10));
            clear[i]=std::uint16_t((30-i)|((26-i)<<5)|((15-i)<<10));
        }
        clear[15]=0;
        const auto fog_ramp=titania_cloud_ramp(fog),clear_ramp=titania_cloud_ramp(clear);
        const auto bright=backdrop_ramp_colour({245,245,245},clear_ramp);
        require(bright==std::array<float,3>{239,206,115},"Titania cloud highlight lost its authored gold palette");
        require(backdrop_ramp_colour({140,140,140},clear_ramp)==std::array<float,3>{0,0,0},
            "Titania dark cloud shade failed to follow live palette");
        require(backdrop_ramp_colour({150,150,150},fog_ramp)==backdrop_ramp_colour({240,240,240},fog_ramp),
            "Titania's deliberately merged fog palette was not preserved");
        require(backdrop_ramp_colour({20,30,40},{})==std::array<float,3>{20,30,40},
            "Inactive cloud ramp changed another backdrop");
        const auto storm_ramp=authored_cloud_ramp(clear,0,220);
        require(backdrop_ramp_colour({220,220,220},storm_ramp)==bright
            && backdrop_ramp_colour({0,0,0},storm_ramp)==std::array<float,3>{0,0,0},
            "Storm photograph used pale-fog exposure calibration");
        require(!authored_cloud_ramp(clear,220,0)[0],"Invalid cloud exposure range accepted");
        auto thunder=clear;thunder.fill(0x7fff);
        require(!venom_lightning_visible(clear) && venom_cloud_ramp(clear)==storm_ramp,
            "Normal Venom cloud palette was changed by lightning handling");
        auto lightning=clear;
        lightning[11]=std::uint16_t(14|(31<<5)|(29<<10));
        const auto lightning_ramp=venom_cloud_ramp(lightning);
        require(venom_lightning_visible(lightning),"Authored cyan lightning not recognized");
        for(unsigned c=0;c<3;++c)
            require(((lightning_ramp[11]>>(c*8))&255)==
                (((lightning_ramp[10]>>(c*8))&255)+((lightning_ramp[12]>>(c*8))&255))/2,
                "Lightning cyan leaked into the photographic cloud shade ramp");
        require(!venom_lightning_visible({}) && !venom_cloud_ramp({})[0],
            "Missing Venom palette enabled lightning");
        require(backdrop_ramp_colour({120,120,120},authored_cloud_ramp(thunder,0,220))
            ==std::array<float,3>{255,255,255},"Live thunder palette did not recolor the clouds");
        for(auto name:{"BG_1_6A","BG_3_7A"}) {
            require(gameplay_landscape_backdrop(name,false)==9
                && gameplay_landscape_backdrop(name,true)==9,
                "Shared Venom cloud artwork assignment changed");
            require(gameplay_landscape_horizon(name,false)==360,"Venom escape cloud horizon moved with sampling origin");
        }
        require(gameplay_landscape_backdrop("BG_1_7B",false)==9
            && !gameplay_landscape_backdrop("BG_1_7B",true),
            "Unverified EX escape artwork changed");
        require(gameplay_landscape_backdrop("BG_2_3A",false)==12
            && gameplay_landscape_backdrop("BG_2_3A",true)==12,"EX Titania clouds were left native");
        require(gameplay_landscape_backdrop("BG_3_3A",false)==13
            && gameplay_landscape_backdrop("BG_3_3A",true)==13,"EX ocean clouds were left native");
        require(fortuna_moon_ink(97) && fortuna_moon_ink(109)
            && !fortuna_moon_ink(110) && !fortuna_moon_ink(96),
            "Fortuna moon protection preserved its rectangular sky fill");
        require(titania_ground_material(std::uint16_t(21|(25<<5)|(31<<10)))==4
            && titania_ground_material(std::uint16_t(11|(8<<5)|(6<<10)))==2,
            "Titania's far snow became water or weather-changed dirt stayed snow");
        for(unsigned blue=24;blue<=31;++blue)
            require(titania_ground_material(std::uint16_t(21|(25<<5)|(blue<<10)))==4,
                "EX Titania's live blue-white ground ramp split across Auto materials");
        const std::array<std::array<unsigned,3>,14> corneria_grass{{
            {3,11,7},{4,12,8},{5,13,9},{6,14,10},{7,15,11},{8,16,12},
            {9,17,13},{10,18,14},{11,19,15},{12,19,17},{13,19,19},
            {14,20,20},{15,21,21},{16,22,22}}};
        for(const auto& [r,g,b]:corneria_grass) {
            require(automatic_ground_material(std::uint16_t(r|(g<<5)|(b<<10)))==1,
                "Corneria's green-to-cyan grass ramp split into water");
        }
        require(automatic_ground_material(std::uint16_t(6|(14<<5)|(20<<10)))==5,
            "Blue-dominant water lost automatic selection");
    }
    {
        std::array<std::uint16_t,112> reference{},live{};
        std::array<std::uint8_t,256> regions{};
        reference[1]=reference[17]=std::uint16_t(20|(20<<5)|(20<<10));
        live=reference;regions[1]=2;regions[17]=1;
        const auto neutral=backdrop_palette_response(reference,live,regions);
        require(neutral[0]==std::array<float,4>{0,0,0,1} && neutral[1]==neutral[0],"neutral source palette changed photograph");
        live[1]=std::uint16_t(10|(5<<5)|(20<<10));
        const auto tint=backdrop_palette_response(reference,live,regions);
        require(std::abs(tint[0][0]+10.f/31)<1e-6f && std::abs(tint[0][1]+15.f/31)<1e-6f
            && tint[0][2]==0 && tint[0][3]==1 && tint[1]==neutral[1],
            "sky palette recoloring leaked onto planet surface");
        live[17]=0;
        const auto fade=backdrop_palette_response(reference,live,regions);
        require(fade[1]==std::array<float,4>{0,0,0,0},"palette fade did not black out surface");
        require(backdrop_palette_response(reference,live,{})==neutral,"unknown palette region invented a tint");
        reference[17]=31;
        const auto monochrome_fade=backdrop_palette_response(reference,live,regions);
        require(monochrome_fade[1]==std::array<float,4>{0,0,0,0},
            "black fade left photo channels absent from monochrome source palette visible");
        // Captured EX 5-1 palette: a small warm shift in a still-blue sky
        // must not turn photographic clouds pink through a 24/7 red gain.
        reference={};live={};regions={};
        const auto rgb=[](unsigned r,unsigned g,unsigned b) {
            return std::uint16_t(r|(g<<5)|(b<<10));
        };
        reference[8]=rgb(5,14,31);reference[9]=rgb(2,11,31);reference[10]=rgb(0,9,31);
        live[8]=rgb(11,16,25);live[9]=rgb(8,14,25);live[10]=rgb(6,12,25);
        regions[8]=regions[9]=regions[10]=2;
        const auto blue_transition=backdrop_palette_response(reference,live,regions);
        require(blue_transition[0][0]<.21f && blue_transition[0][2]==0 && blue_transition[0][3]>.8f,
            "blue sky fade amplified a weak red reference channel");
        auto grey_reference=reference;
        grey_reference[8]=rgb(19,22,20);grey_reference[9]=rgb(17,20,18);grey_reference[10]=rgb(15,18,16);
        calibrate_backdrop_palette(11,grey_reference);
        require(backdrop_palette_response(grey_reference,live,regions)==blue_transition,
            "6-1 reused blue artwork with a grey calibration, corrupting fade hues");
        auto unrelated=reference;calibrate_backdrop_palette(8,unrelated);
        require(unrelated==reference,"snow calibration altered another background");
        reference={};live={};regions={};regions[1]=2;
        reference[1]=rgb(0,20,30);
        for(unsigned step=0;step<=10;++step) {
            live[1]=rgb(0,2*step,3*step);
            const auto uniform=backdrop_palette_response(reference,live,regions);
            for(unsigned c=0;c<3;++c) require(std::abs(uniform[0][c])<1e-6f,
                "uniform fade altered hue");
            require(std::abs(uniform[0][3]-float(step)/10)<1e-6f,
                "uniform fade left a zero-reference channel bright");
        }
        live[1]=rgb(10,20,30);
        require(backdrop_palette_response(reference,live,regions)[0][0]>0,
            "palette transition ignored a newly introduced colour channel");
        // 7-1 keeps its blue start palette, then makes one orange transition.
        // Repeated presentation cannot create extra phases or depend on which
        // point in that transition was the first displayed frame.
        reference={};live={};regions={};regions[8]=2;
        reference[8]=live[8]=rgb(9,16,25);
        const auto start=backdrop_palette_response(reference,live,regions);
        require(start==neutral,"7-1 start palette was tinted");
        live[8]=rgb(26,15,2);
        const auto sunset=backdrop_palette_response(reference,live,regions);
        require(sunset[0][0]>.5f && sunset[0][2]<-.7f && sunset[1]==neutral[1],
            "7-1 orange endpoint lost its hue or contaminated the surface band");
        for(unsigned frame=0;frame<300;++frame)
            require(backdrop_palette_response(reference,live,regions)==sunset,
                "unchanged 7-1 palette generated another enhanced fade");
        for(unsigned c=0;c<3;++c) {
            const float original=float((reference[8]>>(c*5))&31)/31;
            const float target=float((live[8]>>(c*5))&31)/31;
            require(std::abs(original*sunset[0][3]+sunset[0][c]-target)<1e-6f,
                "7-1 palette transform failed to reproduce its orange endpoint");
        }
    }
    {
        Framebuffer surface(320,224);surface.enable_layer_tags(true);
        std::vector<std::uint8_t> source(surface.pixels().size()*4);
        for(std::size_t i=0;i<surface.pixels().size();++i) {
            surface.pixels()[i]=std::uint8_t(i%6+1);
            surface.layer_tags()[i]=std::uint8_t(i%11==0?PixelLayer::two_d:PixelLayer::background);
            for(unsigned c=0;c<4;++c) source[i*4+c]=std::uint8_t((i*17+c*43)%256);
        }
        RowWorkers workers;workers.set_worker_count(2);
        BackdropImage sky{8,4,std::vector<std::uint32_t>(32)};
        for(unsigned i=0;i<32;++i) sky.pixels[i]=0xff000000U|((i*731591U)&0xffffffU);
        {
            EnvironmentEffects lightning;lightning.backdrop=&sky;
            lightning.modes[2]=1;lightning.motion[0]=224;lightning.plane[3]=1;
            lightning.classes[2]=7;
            auto protected_pixels=source;
            apply_environment(lightning,surface,protected_pixels);
            require(protected_pixels!=source,"Lightning fixture did not replace surrounding sky");
            for(std::size_t i=0;i<surface.pixels().size();++i) if(surface.pixels()[i]==2)
                for(unsigned c=0;c<4;++c) require(protected_pixels[i*4+c]==source[i*4+c],
                    "Photographic cloud replacement erased authored lightning strokes");
        }
        for(bool photographic:{false,true}) for(unsigned material:{1U,6U,7U,8U}) {
            EnvironmentEffects e;e.modes={material,2,1,1};
            e.backdrop=photographic?&sky:nullptr;
            e.backdrop_palette={{{.1f,-.2f,0,.8f},{-.1f,.1f,-.3f,.7f}}};
            e.motion={100,12,34,2};e.plane={.2f,1,0,1};e.water_reflections=true;
            for(unsigned i=1;i<=6;++i)e.classes[i]=std::uint8_t(i);
            auto serial=source,parallel=source;
            apply_environment(e,surface,serial);
            apply_environment(e,surface,parallel,&workers);
            require(serial==parallel,"parallel environment shading/reflections changed pixels");
            require(serial!=source,"environment parity fixture did not exercise shading");
        }
    }
    require(ex_menu_landscape_origin(0)==320 && ex_menu_landscape_origin(7)==240
        && ex_menu_landscape_origin(36)==312 && !ex_menu_landscape_origin(25),
        "EX menu landscapes confused with orbital backgrounds");
    {
        starfox::simulation::SnesPpuState p{};p.background_mode=2;p.bg2_vertical_offsets_enabled=true;
        p.vram[0x5f5e]=248;p.vram[0x5f5f]=0x40;
        require(environment_center_scroll_y(p,232)==248,"Enhanced horizon ignores authored offset row");
        p.vram[0x5f5f]=0;
        require(environment_center_scroll_y(p,232)==232,"Invalid offset replaced source scroll");
        p.vram[0x5f5f]=0x40;p.background_mode=1;
        require(environment_center_scroll_y(p,200)==200,"Mode 1 consumed Mode 2 offset table");
    }
    for (unsigned choice : {0U, 3U, 4U, 5U})
        require(ex_menu_landscape_horizon(choice) == 432,
            "Mode 2 replacement horizon follows palette band instead of original limb");
    for (unsigned choice : {7U, 8U, 9U, 12U, 13U, 14U, 15U, 16U, 22U, 26U, 32U, 33U})
        require(ex_menu_landscape_horizon(choice) == 360,
            "Mode 1 replacement horizon follows palette band instead of original limb");
    require(!ex_menu_landscape_horizon(25) && !ex_menu_landscape_horizon(99),
        "Landscape limb leaked into orbital/blank menu backgrounds");
    for(auto scene:{"BG_6_2","BG_6_4"}) {
        require(gameplay_landscape_horizon(scene,true)
                ==352U
            && gameplay_landscape_origin(scene,true)==224,
            "Gameplay fortn/corn reused menu offsets and skipped near-horizon ground shades");
        require(!gameplay_landscape_horizon(scene,false) && !gameplay_landscape_origin(scene,false),
            "EX gameplay atlas correction leaked into Original");
    }
    require(ex_menu_landscape_backdrop(1)==15 && ex_menu_landscape_horizon(1)==432,
        "coastal preview lost its separate menu atlas horizon");
    require(ex_menu_full_sky_backdrop(30)==16 && ex_menu_full_sky_backdrop(2)==7
        && ex_menu_full_sky_backdrop(6)==19 && ex_menu_full_sky_backdrop(17)==20
        && ex_menu_full_sky_backdrop(29)==21 && !ex_menu_landscape_backdrop(30),
        "red cloud band acquired ground or leaked into a separate nebula scene");
    require(gameplay_landscape_backdrop("BG_3_5",true)==16
        && gameplay_landscape_backdrop("BG_3_5",false)==16
        && gameplay_landscape_horizon("BG_3_5",true)==136
        && gameplay_landscape_horizon("BG_3_5",false)==392,
        "red cloud gameplay lost its experience-specific atlas placement");
    {
        std::array<std::uint32_t,16> ramp{};ramp[0]=2;
        ramp[1]=0x332211;ramp[8]=0x665544;
        require(backdrop_ramp_colour({0,0,0},ramp)==std::array<float,3>{0,0,0},"Nebula lifted black space");
        require(backdrop_ramp_colour({200,200,200},ramp)==std::array<float,3>{200,200,200},"Nebula recolored neutral stars");
        require(backdrop_ramp_colour({180,0,0},ramp)==std::array<float,3>{17,34,51},"Warm nebula used wrong source bank");
        require(backdrop_ramp_colour({0,0,180},ramp)==std::array<float,3>{68,85,102},"Cool nebula used wrong source bank");
        ramp[1]=0;ramp[8]=0;
        require(backdrop_ramp_colour({180,0,0},ramp)==std::array<float,3>{0,0,0},"Nebula ignored live black palette");
    }
    require(ex_menu_landscape_horizon(10)==360 && ex_menu_landscape_origin(10)==248
        && ex_menu_landscape_backdrop(10)==17,
        "rocky brown-ground preview confused palette origin with ground horizon");
    require(ex_menu_landscape_backdrop(16)==1
        && ex_menu_landscape_backdrop(8)==10 && ex_menu_landscape_backdrop(4)==11
        && ex_menu_landscape_backdrop(5)==3
        && ex_menu_landscape_backdrop(7)==8 && ex_menu_landscape_backdrop(11)==18
        && ex_menu_landscape_backdrop(9)!=ex_menu_landscape_backdrop(11)
        && ex_menu_landscape_backdrop(13)==0 && ex_menu_landscape_backdrop(14)==12
        && ex_menu_landscape_backdrop(13)!=ex_menu_landscape_backdrop(14)
        && ex_menu_landscape_backdrop(9)==6
        && ex_menu_landscape_backdrop(12)==9 && ex_menu_landscape_backdrop(22)==9
        && ex_menu_landscape_backdrop(32)==13
        && ex_menu_landscape_backdrop(26)==12
        && ex_menu_landscape_backdrop(33)==14
        && ex_menu_landscape_backdrop(34)==32 && ex_menu_landscape_horizon(34)==344,
        "EX landscape artwork mapping changed");
    require(ex_city_sky_detail(82) && ex_city_sky_detail(84) && ex_city_sky_detail(86)
        && ex_city_sky_detail(88) && !ex_city_sky_detail(81) && !ex_city_sky_detail(85),
        "City celestial protection included sky fill or lost stars/moons");
    require(ex_menu_landscape_backdrop(36)==22
        && ex_menu_landscape_origin(36)==312 && ex_menu_landscape_horizon(36)==440
        && gameplay_landscape_backdrop("BG_COMET",true)==22
        && !gameplay_landscape_backdrop("BG_COMET",false),
        "Lava cavern replacement crossed the authored lava boundary or leaked into Original");
    for(unsigned choice:{2U,18U,21U,25U,35U,99U,255U})
        require(!ex_menu_landscape_backdrop(choice),"Non-landscape received landscape artwork");
    require(gameplay_landscape_backdrop("BG_3_1C",true)==1
        && gameplay_landscape_backdrop("BG_3_1C",false)==1
        && gameplay_landscape_backdrop("BG_7_1",true)==8
        && gameplay_landscape_backdrop("BG_7_2",true)==10
        && gameplay_landscape_backdrop("BG_6_2",true)==15
        && gameplay_landscape_backdrop("BG_5_1",true)==11
        && gameplay_landscape_backdrop("BG_6_1",true)==11
        && gameplay_landscape_backdrop("BG_5_5",true)==9
        && gameplay_landscape_backdrop("BG_7_5",true)==9
        && gameplay_landscape_backdrop("BG_7_3",true)==0
        && gameplay_landscape_backdrop("BG_7_4",true)==0
        && gameplay_landscape_backdrop("BG_5_4",true)==17
        && gameplay_landscape_backdrop("BG_6_4",true)==2
        && gameplay_landscape_backdrop("BG_6_5",true)==27
        && gameplay_landscape_backdrop("BG_6_6",true)==16
        && gameplay_landscape_backdrop("BG_2_2",true)==28
        && gameplay_landscape_backdrop("BG_2_5",true)==28
        && gameplay_landscape_backdrop("BG_3_6",true)==28
        && gameplay_landscape_backdrop("BG_3_6",false)==28
        && gameplay_landscape_backdrop("BG_2_2",false)==28
        && gameplay_landscape_backdrop("BG_2_5",false)==28
        && gameplay_landscape_backdrop("BG_1_5",false)==28
        && gameplay_landscape_backdrop("BG_6_7C",true)==28
        && gameplay_landscape_backdrop("BG_6_7B",true)==31
        && !gameplay_landscape_backdrop("BG_6_7C",false)
        && !gameplay_landscape_backdrop("BG_6_7B",false)
        && gameplay_landscape_horizon("BG_6_7C",true)==132
        && gameplay_landscape_horizon("BG_6_7B",true)==112
        && gameplay_landscape_origin("BG_6_7C",true)==0
        && gameplay_landscape_origin("BG_6_7B",true)==0,
        "EX gameplay landscape assignments changed");
    require(gameplay_landscape_horizon("BG_5_5",true)==360
        && gameplay_landscape_horizon("BG_7_5",true)==360
        && gameplay_landscape_horizon("BG_6_4",true)==352
        && gameplay_landscape_horizon("BG_2_2",true)==408
        && gameplay_landscape_horizon("BG_2_5",true)==200
        && gameplay_landscape_horizon("BG_3_6",true)==200
        && gameplay_landscape_horizon("BG_3_6",false)==200
        && gameplay_landscape_horizon("BG_2_5",false)==200
        && gameplay_landscape_horizon("BG_1_5",false)==200
        && !gameplay_landscape_horizon("BG_5_5",false),
        "Storm artwork overwrites the original ground bands");
    {
        starfox::simulation::SnesPpuState p{};
        p.background_mode=2;p.bg2_vertical_offsets_enabled=true;
        p.vram[0x5f5e]=184;p.vram[0x5f5f]=0x40;
        p.bg2_scroll_y=12;
        require(environment_landscape_scroll_y("BG_2_5",true,p,164)==12
            && environment_landscape_scroll_y("BG_3_6",true,p,164)==12
            && environment_landscape_scroll_y("BG_6_7B",true,p,164)==12
            && environment_landscape_scroll_y("BG_6_7C",true,p,164)==12
            && environment_landscape_scroll_y("BG_2_2",true,p,0)==184,
            "EX planet horizon followed moving-object offset words");
    }
    require(!gameplay_landscape_backdrop("BG_6_2",false)
        && !gameplay_landscape_backdrop("BG_TRAINING",true)
        && gameplay_landscape_backdrop("BG_TRAINING",false)==8
        && gameplay_landscape_backdrop("BG_1_1C",false)==8
        && gameplay_landscape_backdrop("BG_1_1C",true)==8
        && ex_menu_landscape_backdrop(13)==0
        && !gameplay_landscape_backdrop("BG_5_4",false)
        && !gameplay_landscape_backdrop("BG_6_5",false),
        "Gameplay artwork leaked across experience or unique background boundaries");
    {
        std::array<std::uint8_t,256> regions{};
        correct_ex_landscape_palette("BG_6_4",regions);
        require(regions[65]==2 && regions[73]==2 && regions[74]==0
            && regions[81]==1 && regions[95]==1,
            "EX storm palette included moon ink or lost terrain");
        regions.fill(0);correct_ex_landscape_palette("BG_6_5",regions);
        require(regions[58]==2 && regions[53]==0 && regions[57]==0,
            "EX firefield palette incorrectly treated flames as ground");
        regions.fill(0);correct_ex_landscape_palette("BG_6_6",regions);
        require(regions[49]==2 && regions[63]==2 && regions[65]==1
            && regions[79]==1,
            "EX red-storm palette faded sky and ground together");
    }
    for(unsigned mode:{1U,2U}) for(bool large:{false,true}) for(unsigned size=0;size<4;++size) {
        starfox::simulation::SnesPpuState p{};
        p.background_mode=mode;p.bg2_tile_size_16=large;p.bg2_screen_size=size;
        p.bg2_screen_base=0x7000;p.bg2_character_base=0x5000;
        const unsigned edge=large?16:8,width=(size&1)?64:32,height=(size&2)?64:32;
        for(unsigned tile:{0U,1U,16U,17U,32U,33U,48U,49U}) for(unsigned y=0;y<8;++y)
            p.vram[0xa000+tile*32+y*2+(tile>=32?1:0)]=255;
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const unsigned entry=((x/32)+(y/32)*(width/32))*1024+(y%32)*32+x%32;
            const unsigned tile=(y*edge>=128?32:0)|0xc000; // Both flips, including 16x16 subtiles.
            p.vram[0xe000+entry*2]=tile&255;p.vram[0xe000+entry*2+1]=tile>>8;
        }
        const auto regions=environment_palette_regions(p,0);
        require(regions[1]==2 && regions[2]==1 && regions[0]==0,
            "Menu landscape palette classification lost Mode 1/16x16 or page geometry");
        p.tunnel_scene=true;
        require(environment_palette_regions(p,0)==std::array<std::uint8_t,256>{},
            "Landscape enhancements contaminated a tunnel atlas");
    }
    for(const std::int16_t horizontal:{std::int16_t(-8192),std::int16_t(8192)}) {
        const std::array<std::int16_t,9> banked{31727,0,0,horizontal,31727,0,0,0,32767};
        const float slope=environment_horizon_slope(banked);
        require(std::abs(slope)>.25f && std::abs(slope)<.27f,"small camera bank flattened enhanced horizon");
        for(float x:{-400.f,0.f,400.f}) {
            const float y=112+slope*x;
            require(std::abs(float(horizontal)*x+31727*(y-112))<.5f,"enhanced horizon detached from ground normal");
            require(std::abs(BackdropImage::coordinates(x,y,112,slope,0)[1]-1)<.00001f,"photographic horizon detached during banking");
        }
    }
    EnvironmentClock clock;clock.restore(65535,1,65535);clock.advance(0,1);
    require(clock.ticks()==65536,"environment counter did not unwrap");
    require(clock.seconds(1)>clock.seconds(0),"environment time jumped backward on wrap");
    require(clock.seconds(.5)>clock.seconds(0) && clock.seconds(.5)<clock.seconds(1),"environment motion was not interpolated");
    require(clock.seconds(0,true)==clock.seconds(1),"paused environment did not settle");
    clock.advance(0,1);require(clock.seconds(0)==clock.seconds(1),"held source frame advanced environment");
    EnvironmentClock restored;restored.restore(0,1,clock.ticks());
    clock.advance(1,1);restored.advance(1,1);
    require(clock.seconds(.5)==restored.seconds(.5),"environment restore phase diverged");
    clock.advance(7,2);require(clock.ticks()==7 && clock.seconds(0)==clock.seconds(1),"environment scene reset interpolated across scenes");
    clock.advance(2,2);require(clock.ticks()==2,"cartridge reset became a giant time advance");
    for(unsigned motion=0;motion<3;++motion)
        require(backdrop_motion({.25f,.9f},motion,100.f)==std::array<float,2>{.25f,.9f},"cloud motion displaced mountains");
    EnvironmentEffects soil;soil.modes[0]=1;soil.motion[0]=100;
    {
        Framebuffer mirror(3,8);mirror.enable_layer_tags(true);
        std::fill(mirror.pixels().begin(),mirror.pixels().end(),1);
        std::fill(mirror.layer_tags().begin(),mirror.layer_tags().end(),unsigned(PixelLayer::background));
        std::vector<std::uint8_t> original(3*8*4,255);
        for(unsigned y=0;y<8;++y) for(unsigned x=0;x<3;++x) for(unsigned k=0;k<3;++k)
            original[(y*3+x)*4+k]=std::uint8_t(y*25);
        constexpr unsigned sample=(6*3+1)*4;
        original[sample]=original[sample+1]=original[sample+2]=0;
        EnvironmentEffects reflection;reflection.modes[0]=7;reflection.classes[1]=1;reflection.water_reflections=true;
        for(unsigned step=0;step<5;++step) {
            reflection.motion[0]=4.f+float(step)*.1f;
            auto pixels=original;apply_environment(reflection,mirror,pixels);
            require(std::abs(int(pixels[sample])-int(20+step*4))<=1,"mirror reflection snapped or used compressed water mapping");
            reflection.motion[3]=1234;
            auto later=original;apply_environment(reflection,mirror,later);
            require(later==pixels,"stationary mirror acquired animated water ripples");
        }
    }
    for(unsigned kind=1;kind<=5;++kind) {
        EnvironmentEffects automatic=soil,selected=soil;selected.modes[0]=kind+1;
        for(float x:{-190.f,0.f,190.f}) for(float y:{101.f,140.f,220.f})
            require(environment_colour({60,120,85},kind,x,y,automatic)==environment_colour({60,120,85},kind,x,y,selected),
                "Auto ground differs from selecting the detected material");
    }
    const auto dirt=environment_colour({180,90,25},2,0,180,soil);
    require(dirt[0]>dirt[1] && dirt[1]>dirt[2] && dirt[0]-dirt[2]<70,
        "dirt retained oversaturated native orange palette");
    const auto snow=environment_colour({173,206,255},4,0,180,soil);
    require(snow[0]<snow[1] && snow[1]<snow[2] && snow[0]<220 && snow[1]<235,
        "Bright EX snow clipped to a featureless white plane");
    require(environment_colour({0,0,0},2,0,180,soil)==std::array<float,3>{0,0,0},
        "soil treatment lifted a black fade");
    for(unsigned kind=1;kind<=4;++kind) for(unsigned detail=0;detail<3;++detail) {
        const auto s=EnhancedTerrain::make_shape(-3,511,kind,detail);
        const auto repeat=EnhancedTerrain::make_shape(-3,511,kind,detail);
        require(s.vertices==repeat.vertices,"terrain is not deterministic");
        const auto distant=EnhancedTerrain::make_shape(-3,511,kind,0);
        const auto floor_vertices = (EnhancedTerrain::subdivisions+1)*(EnhancedTerrain::subdivisions+1);
        require(std::equal(s.vertices.begin(),s.vertices.begin()+floor_vertices,distant.vertices.begin()),"foliage LOD changed terrain surface");
        require(!s.faces.empty() && s.vertices.size()<=256,"native terrain index budget");
        for(const auto& f:s.faces) {
            require(f.vertex_indices.size()==3,"terrain is not triangle geometry");
            for(auto i:f.vertex_indices) require(i<s.vertices.size(),"invalid triangle index");
            require(f.normal.x || f.normal.y || f.normal.z,"degenerate terrain triangle");
        }
        for(unsigned i=0;i<=16;++i) {
            const double v=double(i)/16;
            require(std::abs(EnhancedTerrain::height(-3,511,1,v,kind)-EnhancedTerrain::height(-2,511,0,v,kind))<1e-9,"terrain X seam");
            require(std::abs(EnhancedTerrain::height(-3,511,v,1,kind)-EnhancedTerrain::height(-3,512,v,0,kind))<1e-9,"terrain Z seam");
            require(std::abs(EnhancedTerrain::height(-3,511,v,.5,kind)-EnhancedTerrain::height(509,-1,v,.5,kind))<1e-9,"source word wrap seam");
        }
        require(s.vertices.size()==floor_vertices
            && s.faces.size()==2*EnhancedTerrain::subdivisions*EnhancedTerrain::subdivisions
            && s.vertices[EnhancedTerrain::subdivisions].x==512,
            "terrain retained tiny patches or exceeded patch budget");
    }
    require(EnhancedTerrain::foliage_detail(511) == 2
        && EnhancedTerrain::foliage_detail(512) == 1
        && EnhancedTerrain::foliage_detail(1024) == 0,
        "subpixel grass remained individually tessellated");
    for(unsigned kind:{1u,3u,4u}) {
        double low=100,high=-100;
        for(int z=0;z<128;z+=3) for(int x=0;x<128;x+=3) {
            const double h=EnhancedTerrain::height(x,z,.5,.5,kind);
            low=std::min(low,h);high=std::max(high,h);
        }
        require(low<-10 && high>-.6,"terrain needs raised regions and genuinely flat stretches");
    }
    EnhancedTerrain terrain;terrain.begin_frame();
    for(unsigned kind=1;kind<=4;++kind) {
        EnhancedTerrain::Batch batch;
        for(int n=0;n<16;++n) {
            const auto& patch=terrain.patch(125+n%4,-3+n/4,kind,0,{1,2,3,4});
            const auto first=batch.shape.vertices.size(),face_first=batch.shape.faces.size();
            require(batch.append(patch),"Terrain batch rejected an in-budget patch");
            for(unsigned i=0;i<patch.shape.vertices.size();++i) {
                auto expected=patch.shape.vertices[i];
                expected.x+=(patch.x-batch.x)*EnhancedTerrain::patch_size;
                expected.z+=(patch.z-batch.z)*EnhancedTerrain::patch_size;
                require(batch.shape.vertices[first+i]==expected,"Terrain batching changed world geometry");
            }
            for(unsigned i=0;i<patch.shape.faces.size();++i) {
                const auto& a=patch.shape.faces[i];const auto& b=batch.shape.faces[face_first+i];
                require(a.normal==b.normal && a.colour_id==b.colour_id && a.visibility_index==b.visibility_index,
                    "Terrain batching changed normals, color or visibility");
                for(unsigned j=0;j<a.vertex_indices.size();++j)
                    require(b.vertex_indices[j]==first+a.vertex_indices[j],"Terrain batching changed painter order or indices");
            }
        }
        require(batch.patches==16 && batch.shape.vertices.size()==256 && batch.shape.faces.size()==288,
            "Terrain batch did not reduce sixteen submissions to one");
        require(!batch.append(terrain.patch(0,0,kind,0,{1,2,3,4})) && batch.shape.vertices.size()==256,
            "Terrain batch overflowed byte indices or partially appended a rejected patch");
        EnhancedTerrain::Batch palette;
        require(palette.append(terrain.patch(0,0,kind,0,{1,2,3,4}))
            && !palette.append(terrain.patch(1,0,kind,0,{2,3,4,5})),"Terrain batch merged incompatible palettes");
    }
    for(unsigned kind=1;kind<=4;++kind) {
        const auto left=EnhancedTerrain::make_shape(7,-2,kind,0);
        const auto right=EnhancedTerrain::make_shape(8,-2,kind,2);
        for(unsigned row=0;row<=EnhancedTerrain::subdivisions;++row)
            require(left.vertices[row*(EnhancedTerrain::subdivisions+1)+EnhancedTerrain::subdivisions].y
                ==right.vertices[row*(EnhancedTerrain::subdivisions+1)].y,
                "large terrain patches have a vertical seam");
        const auto wrapped=EnhancedTerrain::make_shape(7+128,-2-128,kind,0);
        require(left.vertices==wrapped.vertices,"large terrain patch changed at coordinate wrap");
    }
    const auto& a=terrain.patch(0,0,1,2,{1,2,3,4});
    const auto* address=&a.shape;terrain.end_frame();
    terrain.begin_frame();require(&terrain.patch(0,0,1,2,{2,3,4,5}).shape==address,"unchanged tile was reallocated");terrain.end_frame();
    terrain.begin_frame();terrain.end_frame();require(terrain.cached_patches()==0,"unused geometry cache was retained");
    starfox::assets::Shape shape;shape.vertices={{-20,-20,0},{20,-20,0},{0,20,0}};shape.colour_words={0x11,0x11};
    starfox::assets::Face face;face.visibility_index=-1;face.colour_id=1;face.vertex_indices={2,1,0};shape.faces={face};
    RenderPose pose;pose.z=120;pose.vanish_x=pose.vanish_y=32;pose.terrain_geometry=true;pose.continuous_geometry=true;
    Framebuffer frame(64,64);frame.enable_layer_tags(true);SoftwareRenderer{}.draw(shape,pose,frame);
    unsigned pixels=0;
    for(unsigned i=0;i<frame.pixels().size();++i) if(frame.pixels()[i]) {++pixels;require(frame.layer_tags()[i]==5,"CPU terrain tagged as a model");}
    require(pixels>0,"no CPU terrain triangles");
    const auto packed=pack_faces(shape,pack_bsp(shape),pose,RenderSettings{});
    require(!packed.materials.empty() && packed.materials[0].tag==5,"GPU terrain tagged as a model");
    std::cout<<"Terrain geometry: hills/relief, deterministic seams/wrap, bounded cache, ordered batches and CPU/GPU world ownership passed\n";
}
