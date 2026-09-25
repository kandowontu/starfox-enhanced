#include "starfox/render/backdrop_image.hpp"
#include "starfox/render/celestial_scroll.hpp"
#include "starfox/render/radial_backdrop.hpp"
#include "starfox/render/face_planet_atlas.hpp"
#include "starfox/render/moon_landscape_atlas.hpp"
#include "starfox/render/cloud_limb_atlas.hpp"
#include "starfox/render/enhanced_backdrop_library.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
int main(int argc,char** argv) {
    using starfox::render::BackdropImage;
    const auto require=[](bool b,const char* message){if(!b) throw std::runtime_error(message);};
    {
        using starfox::render::cloud_limb_coordinates;
        for(const auto& region:starfox::render::cloud_limb_regions) {
            const float cx=(region[0]+region[2])*.5f,cy=(region[1]+region[3])*.5f;
            const std::array<float,4> t{.2f,-.15f,280,250};
            const float sx=(cx-t[2]-t[0]*(cy-t[3]))/(1-t[0]*t[1]);
            const float sy=cy-t[3]-t[1]*sx;
            const auto center=cloud_limb_coordinates(sx,sy,t);
            const auto beside=cloud_limb_coordinates(sx+3,sy+2,t);
            require(center && beside && std::abs((*center)[0]-cx)<.001f
                && std::abs((*center)[1]-cy)<.001f && std::abs((*beside)[0]-cx-3)<.001f
                && std::abs((*beside)[1]-cy-2)<.001f,"Cloud/crescent centers drifted or surfaces sheared");
        }
        const std::array<float,4> wrapped{0,0,639,250};
        require(cloud_limb_coordinates(-23,38,wrapped).has_value()
            && !cloud_limb_coordinates(-535,38,wrapped)
            && !cloud_limb_coordinates(489,38,wrapped),"Cloud acquired repeated widescreen copies");
        require(!cloud_limb_coordinates(0,0,{1,1,0,0}),"Singular cloud projection escaped its guard");
        std::array<std::uint32_t,16> ramp{};
        ramp[1]=0x102030;ramp[15]=0x405060;
        require(BackdropImage::limb_colour({255,255,255},ramp)==std::array<float,3>{48,32,16}
            && BackdropImage::limb_colour({0,0,0},ramp)==std::array<float,3>{96,80,64},
            "Crescent ignored live bright/dark palette endpoints");
        BackdropImage cloud{2,2,std::vector<std::uint32_t>(4,0xffa04020)};
        BackdropImage surface{2,2,std::vector<std::uint32_t>(4,0xff808080)};
        cloud.seal_for_upload();surface.seal_for_upload();
        starfox::simulation::SnesPpuState ppu;
        ppu.bg2_screen_base=0x2000;ppu.bg2_screen_size=3;ppu.bg2_character_base=0x1000;
        for(unsigned ty=40;ty<44;++ty) for(unsigned tx=20;tx<30;++tx) {
            const unsigned at=0x4000+((ty/32*2)*1024+(ty%32)*32+tx)*2;
            ppu.vram[at]=1;ppu.vram[at+1]=5<<2;
        }
        for(unsigned y=0;y<8;++y) ppu.vram[0x2000+32+y*2]=255;
        starfox::render::CloudLimbAtlas atlas;
        const auto& first=atlas.image(cloud,surface,ppu);const auto key=first.immutable_upload_key;
        require(first.width==2048 && first.height==2048 && first.pixels[0]==0xff000000,
            "Cloud/crescent atlas lost its sparse black surround");
        require(first.sample_projected(104/512.f,288/512.f,9)==std::array<float,3>{32,64,160},
            "Cloud replacement entered crescent shade encoding");
        ppu.cgram[81]=0x7fff;
        require(atlas.image(cloud,surface,ppu).immutable_upload_key==key,
            "Palette-only crescent fade rebuilt a texture");
        ppu.vram[0x2000+32]=0;
        require(atlas.image(cloud,surface,ppu).immutable_upload_key!=key,
            "Changed crescent source tiles reused a stale atlas");
    }
    {
        using starfox::render::city_moon_coordinates;
        const std::array<float,4> scroll{299.25f,200.5f,0,0};
        for(const auto& body:starfox::render::city_moons) {
            const auto center=city_moon_coordinates(body[0]-scroll[0],body[1]-scroll[1],scroll);
            const auto edge=city_moon_coordinates(body[0]-scroll[0]+body[2]*.9f,body[1]-scroll[1],scroll);
            require(center && (*center)[0]==body[3]*.5f+.25f && (*center)[1]==2.5f,
                "City moon center or palette slot drifted from its native disk");
            require(edge && !city_moon_coordinates(body[0]-scroll[0]+512,body[1]-scroll[1],scroll)
                && !city_moon_coordinates(body[0]-scroll[0]-512,body[1]-scroll[1],scroll),
                "City moon radius was lost or a widescreen copy appeared");
        }
        std::array<std::uint32_t,16> ramp{};ramp[6]=2;
        ramp[7]=0x786878;ramp[8]=0x584858;ramp[9]=0x482848;
        ramp[10]=0xff6818;ramp[11]=0x804000;
        require(BackdropImage::moon_colour({255,255,255},{.25f,2.5f},ramp)==std::array<float,3>{120,104,120}
            && BackdropImage::moon_colour({0,0,0},{.25f,2.5f},ramp)==std::array<float,3>{72,40,72}
            && BackdropImage::moon_colour({255,255,255},{.75f,2.5f},ramp)==std::array<float,3>{24,104,255},
            "City moon palette families were merged");
    }
    {
        BackdropImage landscape{8,4,std::vector<std::uint32_t>(32,0xff604020)};
        BackdropImage moon{2,2,std::vector<std::uint32_t>(4,0xffaaaaaa)};
        landscape.seal_for_upload();moon.seal_for_upload();
        starfox::render::MoonLandscapeAtlas cache;
        const auto& atlas=cache.image(landscape,moon);
        const auto key=atlas.immutable_upload_key;
        require(cache.image(landscape,moon).immutable_upload_key==key,
            "Static moon landscape rebuilt its upload");
        for(float u:{-.7f,0.f,.2f,1.2f}) for(float v:{-.5f,0.f,.4f,1.f,1.5f})
            require(atlas.sample_projected(u,v,6)==landscape.sample(u,v),
                "Packed moons changed landscape sampling or its seam");
        const auto red=atlas.sample_projected(.2f,2.5f,6),blue=atlas.sample_projected(.8f,2.5f,6);
        require(red[0]>250 && red[2]>250 && blue[2]>250 && blue[0]>250,
            "Unique moon slots lost their neutral lighting master");
        const std::array<std::array<float,4>,2> keep{{{10,20,8,8},{50,40,18,18}}};
        const auto a=BackdropImage::coordinates(10,20,100,0,0,{1/512.f,1/224.f,.55f,6},keep);
        const auto b=BackdropImage::coordinates(522,20,100,0,0,{1/512.f,1/224.f,.55f,6},keep);
        require(a[1]==2.5f && b[1]<2,"Landscape moon repeated in widescreen margin");
        const std::array<float,4> orbital{1/512.f,1/224.f,.633f,7};
        require(BackdropImage::covers(10,20,100,0,orbital,keep)
            && BackdropImage::covers(500,600,100,.5f,orbital,keep),
            "Orbital moon atlas cut a hole or clipped its lower surface");
        require(BackdropImage::coordinates(10,20,100,0,0,orbital,keep)[1]==2.5f,
            "Orbital moon no longer selects its unique atlas slot");
        for(float bank:{-2.f,0.f,2.f}) for(float y:{-10000.f,10000.f}) {
            const auto uv=BackdropImage::coordinates(800,y,100,bank,0,orbital,keep);
            require(uv[1]>=0 && uv[1]<=1,"Offscreen orbital surface selected a moon tile");
            require(atlas.sample_projected(uv[0],uv[1],7)==landscape.sample(uv[0],uv[1]),
                "Packed moon altered orbital surface sampling");
        }
        const auto crop_key=cache.image(landscape,moon,{53,55,1199,1191}).immutable_upload_key;
        require(crop_key!=key && cache.image(landscape,moon,{53,55,1199,1191}).immutable_upload_key==crop_key,
            "Celestial crop change ignored or rebuilt on every frame");
        const std::array<float,4> fade{.4f,31.f/56.f,-1,0};
        const auto top=BackdropImage::moon_surface({120,120,120},{.25f,2},fade);
        const auto middle=BackdropImage::moon_surface({120,120,120},{.25f,2.25f},fade);
        const auto bottom=BackdropImage::moon_surface({120,120,120},{.25f,2.55f},fade);
        require(top[0]>120 && top[0]>middle[0] && middle[0]>bottom[0] && bottom[0]<60,
            "Fortuna moon lost its bright-top/dark-lower-edge gradient");
        require(BackdropImage::moon_surface({120,120,120},{.25f,2.55f},{})==std::array<float,3>{120,120,120},
            "Fortuna tonal gradient leaked into full moons");
        require(BackdropImage::moon_opacity({.25f,2.2f},fade)==1
            && BackdropImage::moon_opacity({.25f,2.6f},fade)==0
            && BackdropImage::moon_opacity({.25f,2.48f},fade)>0
            && BackdropImage::moon_opacity({.25f,2.48f},fade)<1,
            "Partial moon lost atmospheric fade or exposed lower hemisphere");
        std::array<std::uint32_t,16> phase{};
        phase[1]=0xffffff;phase[2]=0x80ffff;
        phase[3]=phase[4]=0x686868;phase[5]=1;
        require(BackdropImage::moon_colour({255,255,255},{.1f,2.5f},phase)==std::array<float,3>{104,104,104}
            && BackdropImage::moon_colour({255,255,255},{.4f,2.5f},phase)==std::array<float,3>{255,255,255}
            && BackdropImage::moon_colour({255,255,255},{.9f,2.5f},phase)==std::array<float,3>{255,255,128},
            "Twin planets lost their gray-left, white/yellow-right phases");
        phase[2]=0x40a060;
        require(BackdropImage::moon_colour({255,255,255},{.9f,2.5f},phase)==std::array<float,3>{96,160,64},
            "Twin planet phase ignored live palette color");
        phase[5]=0;
        require(BackdropImage::moon_colour({128,128,128},{.1f,2.5f},phase)==std::array<float,3>{128,128,128},
            "Phase lighting leaked into unphased moon");
        phase[6]=1;
        for(unsigned shade=0;shade<8;++shade) phase[7+shade]=(210-shade*20)|((100-shade*10)<<8)|((35-shade*5)<<16);
        require(BackdropImage::moon_colour({255,255,255},{.25f,2.5f},phase)==std::array<float,3>{210,100,35}
            && BackdropImage::moon_colour({0,0,0},{.25f,2.5f},phase)==std::array<float,3>{70,30,0},
            "Banded planet lost its independently colored light and dark shades");
        moon.seal_for_upload();
        require(cache.image(landscape,moon).immutable_upload_key!=key,
            "Changed moon artwork retained stale atlas");
    }
    {
        const std::array<float,4> t{.2f,-.1f,250,170};
        const float cx=344,cy=80;
        const float sx=(cx-t[2]-t[0]*(cy-t[3]))/(1-t[0]*t[1]);
        const float sy=cy-t[3]-t[1]*sx;
        const auto center=starfox::render::face_planet_coordinates(sx,sy,t);
        const auto right=starfox::render::face_planet_coordinates(sx+10,sy,t);
        require(center && right && std::abs((*center)[0]-cx)<.001f
            && std::abs((*right)[0]-cx-10)<.001f && std::abs((*right)[1]-cy)<.001f,
            "Face-planet center/shape changed with affine shear");
        require(!starfox::render::face_planet_coordinates(640,32,{0,0,0,0}),
            "Face planet duplicated outside the primary atlas");
        require(!starfox::render::face_planet_coordinates(64,172,{0,0,0,0}),
            "Face replacement overwrites original saucer");
        const auto cleared=starfox::render::face_planet_coordinates(304,40,{0,0,0,0});
        require(cleared && (*cleared)[0]==0 && (*cleared)[1]==0,
            "Original face footprint survives outside the round replacement");
        starfox::simulation::SnesPpuState ppu{};
        ppu.bg2_screen_size=3;
        for(unsigned i=1;i<15;++i) ppu.cgram[i]=std::uint16_t((15-i)*2);
        BackdropImage master{1,1,{0xffc0c0c0}};master.seal_for_upload();
        starfox::render::FacePlanetAtlas atlas;
        const auto& picture=atlas.image(master,ppu);const auto key=picture.immutable_upload_key;
        require(key && atlas.image(master,ppu).immutable_upload_key==key,
            "Unchanged face surface rebuilt its atlas");
        require(picture.sample_projected(128.f/512,32.f/512,5)[0]>0,
            "Face replacement missed its authored atlas location");
        ppu.cgram[1]^=31<<10;
        require(atlas.image(master,ppu).immutable_upload_key!=key,
            "Face atlas ignored its live palette");
    }
    {
        starfox::render::RadialBackdrop radial;
        std::array<std::uint16_t,256> palette{};
        for(unsigned i=1;i<=15;++i) {
            palette[48+i]=std::uint16_t((15-i)*2);
            palette[64+i]=std::uint16_t(((15-i)*2)<<10);
        }
        const auto& image=radial.image(palette);
        const auto key=image.immutable_upload_key;
        require(key && radial.image(palette).immutable_upload_key==key,
            "Unchanged radial palette rebuilt/uploaded its image");
        const auto left=image.sample_projected(0,.5f,2);
        const auto middle=image.sample_projected(.5f,.5f,2);
        require(left[0]>220 && left[2]==0 && middle[2]>220 && middle[0]==0,
            "Radial source centers or palette banks moved");
        const auto gap=image.sample_projected(.25f,.5f,2);
        require(gap==std::array<float,3>{0,0,0},"Black gap between radial disks disappeared");
        for(float x:{-.01f,0.f,.1f,.49f,.99f}) {
            const auto a=image.sample_projected(x,.5f,2),b=image.sample_projected(x+1,1.5f,2);
            for(unsigned c=0;c<3;++c) require(std::abs(a[c]-b[c])<.002f,
                "Radial repeat changed source period or introduced a seam");
        }
        palette[65]=31<<5;
        require(radial.image(palette).immutable_upload_key!=key,
            "Changed radial palette retained a stale GPU upload");
        require(radial.image(palette).sample_projected(.5f,.5f,2)[1]>250,
            "Live radial palette did not reach the replacement");
    }
    {
        using starfox::render::ex_menu_sky_object;
        const auto crater=ex_menu_sky_object(28),storm=ex_menu_sky_object(27),banded=ex_menu_sky_object(31);
        require(crater && storm && banded && crater->artwork!=storm->artwork
            && storm->artwork!=banded->artwork && crater->artwork!=banded->artwork,
            "Distinct native planets share replacement artwork");
        require(storm->x==112.5f && storm->y==415.5f && storm->width==95
            && banded->x==124 && banded->y==364.5f && banded->width==70,
            "Celestial atlas placement lost native size or offset");
        require(ex_menu_sky_object(19)->artwork==crater->artwork
            && ex_menu_sky_object(19)->y==280 && crater->y==152
            && !ex_menu_sky_object(18) && !ex_menu_sky_object(32),
            "Unrelated unique planets received a generic replacement");
        const auto cloud=ex_menu_sky_object(20);
        require(cloud && cloud->artwork==26 && cloud->palette_bank==0
            && cloud->x==104 && cloud->y==288 && cloud->width==48 && cloud->height==48,
            "The 1-4 blue cloud lost its dedicated non-planet artwork or placement");
        const auto original_cloud=starfox::render::original_cloud_sky_object();
        require(original_cloud.artwork==30 && original_cloud.x==cloud->x
            && original_cloud.y==cloud->y && original_cloud.width==cloud->width
            && original_cloud.height==cloud->height,
            "Original 1-4 cloud lost its warm asset or native placement");
        require(crater->palette_bank==5 && storm->palette_bank==1 && banded->palette_bank==6,
            "Distinct celestial palette banks were merged");
    }
    {
        using starfox::render::celestial_scroll;
        using starfox::render::interpolate_celestial_scroll;
        std::array<std::int16_t,224> rows{};
        std::array<std::uint16_t,32> columns{};
        for(unsigned i=0;i<rows.size();++i) rows[i]=std::int16_t((-110+int(i)/4)&8191);
        for(unsigned i=0;i<columns.size();++i) columns[i]=std::uint16_t(0x4000|((30-2*int(i+1))&8191));
        auto t=celestial_scroll(rows,columns,0,232,true,true);
        require(std::abs(t[0]-.25f)<.001f && std::abs(t[1]+.25f)<.001f,
            "Planet ignored affine row/column scrolling");
        require(std::abs(t[2]-17.63f)<.02f && std::abs(t[3]+2)<.001f,
            "Planet offset table origin or wrap is wrong");
        const auto fallback=celestial_scroll(rows,columns,7,30,false,false);
        require(fallback==std::array<float,4>{0,0,135,30},"Disabled scroll table affected planet");
        const auto halfway=interpolate_celestial_scroll({0,0,639,30},{0,0,128,30},.5f,384,152);
        require(halfway[2]==639.5f,"Planet interpolation took long path through scroll wrap");
        const auto stabilized=starfox::render::stabilize_celestial_body({.25f,-.25f,530,30},384,152);
        for(float a:{-.6f,0.f,.6f}) for(float b:{-.5f,0.f,.5f})
        for(float sx:{-200.f,0.f,200.f}) for(float sy:{-50.f,112.f,250.f}) {
            // Construct the atlas centre from a known screen centre; do not
            // duplicate the inverse formula under test in the expectation.
            const float cx=sx+a*sy+530,cy=sy+b*sx+30;
            const auto fixed=starfox::render::stabilize_celestial_body({a,b,530,30},cx,cy);
            require(fixed[0]==0 && fixed[1]==0 && std::abs(cx-fixed[2]-sx)<.0001f
                && std::abs(cy-fixed[3]-sy)<.0001f,"Planet centre changed during shear removal");
        }
        const auto singular=starfox::render::stabilize_celestial_body({1,1,530,30},384,152);
        require(singular==std::array<float,4>{0,0,530,30},"Degenerate celestial transform became nonfinite");
        const std::array<std::array<float,4>,2> stable_keep{{{384,152,56,56},stabilized}};
        const auto stable_left=BackdropImage::coordinates(-150,100,0,0,50,{.01f,.02f,.49f,4},stable_keep);
        const auto stable_right=BackdropImage::coordinates(-100,100,0,0,50,{.01f,.02f,.49f,4},stable_keep);
        require(std::abs(stable_right[0]-stable_left[0]-.5f)<.0001f
            && std::abs(stable_right[1]-stable_left[1])<.0001f,
            "Lateral movement still skews the replacement planet");
        const std::array<float,4> projection{.01f,.02f,.49f,4};
        const std::array<std::array<float,4>,2> keep{{{384,152,56,56},{.25f,-.25f,530,30}}};
        const float x=(-146-.25f*122)/1.0625f,y=122+.25f*x;
        require(BackdropImage::covers(x,y,0,0,projection,keep),"Affine planet center missing");
        const auto uv=BackdropImage::coordinates(x,y,0,0,50,projection,keep);
        require(std::abs(uv[0]-.5f)<.0001f && std::abs(uv[1]-.49f)<.0001f,
            "Planet image detached from affine footprint");
        require(!BackdropImage::covers(x+512,y,0,0,projection,keep),"Affine planet repeated in side margin");
    }
    {
        starfox::render::BackdropUploadCache cache;
        BackdropImage image{2,2,{1,2,3,4}};
        require(!cache.matches(image),"New mutable image matched empty upload cache");
        cache.remember(image);
        require(cache.matches(image),"Unchanged mutable image missed cache");
        image.pixels[0]=5;
        require(!cache.matches(image),"In-place mutable edit was missed");
        image.seal_for_upload();cache.remember(image);
        require(cache.matches(image) && cache.retained_pixels()==0,
            "Sealed image still retains/compares a CPU pixel copy");
        auto replacement=image;replacement.seal_for_upload();
        require(!cache.matches(replacement),"New sealed image reused prior asset identity");
        image.prepare_zenith(1);
        require(!image.immutable_upload_key && !cache.matches(image),"Zenith edit retained stale sealed identity");
        cache.remember(image);
        require(cache.matches(image) && cache.retained_pixels()==4,"Mutable cache did not resume content checks");
    }
    for(float slope:{-1.f,-.3f,0.f,.3f,1.f}) for(float x:{-300.f,0.f,300.f}) {
        const auto uv=BackdropImage::coordinates(x,48,112,slope,73);
        const float a=uv[0]*512-73,b=(uv[1]-1)*160;
        require(std::abs(a*a+b*b-(x*x+64*64))<.05f,"rolled backdrop is stretched/sheared");
        require(std::abs(BackdropImage::coordinates(x,112+slope*x,112,slope,73)[1]-1)<.0001f,"rolled horizon detached");
    }
    BackdropImage fixture;fixture.width=64;fixture.height=2;fixture.pixels.resize(128);
    {
        BackdropImage sky{32,16,std::vector<std::uint32_t>(512)};
        for(unsigned i=0;i<sky.pixels.size();++i) sky.pixels[i]=0xff000000U|(i*7919U&0xffffffU);
        const auto original=sky.pixels;
        auto disabled=sky;disabled.prepare_zenith(0);
        require(disabled.pixels==original,"Disabled zenith preparation changed artwork");
        sky.prepare_zenith();
        for(unsigned x=0;x<sky.width;++x)
            require(sky.pixels[x]==sky.pixels[0],"Zenith row still extrudes varying star columns");
        for(unsigned i=7*sky.width;i<sky.pixels.size();++i)
            require(sky.pixels[i]==original[i],"Zenith preparation changed interior/horizon artwork");
        for(float u:{-3.f,0.f,.23f,.8f,4.2f})
            require(sky.sample(u,-1)==sky.sample(0,-1),"Above-image sky depends on horizontal star location");
        BackdropImage empty;empty.prepare_zenith();
        BackdropImage small{1,1,{0xff123456U}};small.prepare_zenith();
        require(small.pixels[0]==0xff123456U,"Single-pixel zenith changed color");
    }
    const std::array<float,4> orbital{1/512.f,1/224.f,.633f,1.f};
    const std::array<float,4> landscape{1/512.f,1/160.f,1.f,0.f};
    const std::array<std::array<float,4>,2> keep{{{20,40,12,8},{-70,50,5,5}}};
    require(!BackdropImage::covers(20,40,112,0,orbital,keep),"unique planet overwritten");
    require(!BackdropImage::covers(-70,50,112,0,orbital,keep),"second unique planet overwritten");
    require(BackdropImage::covers(33,40,112,0,orbital,keep),"planet protection leaked outside ellipse");
    require(BackdropImage::covers(0,200,112,0,orbital,{}),"orbital surface clipped at horizon");
    require(!BackdropImage::covers(0,200,112,0,landscape,{}),"landscape overwrote ground");
    const std::array<float,4> celestial{1/24.f,1/16.f,.5f,3.f};
    require(BackdropImage::covers(20,40,40,0,celestial,keep)
        && !BackdropImage::covers(33,40,40,0,celestial,keep)
        && !BackdropImage::covers(-70,50,40,0,celestial,keep)
        && !BackdropImage::covers(20,40,40,0,celestial,{}),
        "Single celestial replacement repeated or escaped its authored footprint");
    BackdropImage body{2,2,{0xff000011,0xff000022,0xff000033,0xff000044}};
    require(body.sample_projected(-1,-1,3)[0]==17 && body.sample_projected(2,2,3)[0]==68
        && body.sample_projected(.5f,.5f,3)[0]==42.5f,
        "Celestial texture used panorama wrapping or overlap rescaling");
    for(float slope:{-1.f,-.3f,0.f,.3f,1.f}) for(float x:{-300.f,0.f,300.f}) {
        const float horizon=112+slope*x;
        const auto limb=BackdropImage::coordinates(x,horizon,112,slope,73,orbital);
        require(std::abs(limb[1]-orbital[2])<.0001f,"orbital limb detached from rolled horizon");
        const auto below=BackdropImage::coordinates(x,horizon+30,112,slope,73,orbital);
        const auto above=BackdropImage::coordinates(x,horizon-30,112,slope,73,orbital);
        require(below[1]>limb[1] && above[1]<limb[1],"orbital surface mapped above horizon");
        require(std::abs((below[1]-limb[1])-(limb[1]-above[1]))<.0001f,"orbital projection discontinuity");
    }
    for(unsigned i=0;i<64;++i) {fixture.pixels[i]=0xff000000u|i*4;fixture.pixels[i+64]=0xff00ff00u;}
    for(float u:{-5.2f,-.001f,0.f,.5f,.999f,3.1f}) {
        const auto a=fixture.sample(u,.3f),b=fixture.sample(u+1,.3f);
        for(unsigned k=0;k<3;++k) require(std::abs(a[k]-b[k])<.001f,"panorama wrap changed colour");
        require(fixture.sample(u,-10)==fixture.sample(u,0),"zenith must clamp");
        require(fixture.sample(u,10)==fixture.sample(u,1),"horizon must clamp");
    }
    require(std::abs(fixture.sample(-.000001f,0)[0]-fixture.sample(.000001f,0)[0])<.01f,"panorama seam");
    try {BackdropImage::decode({});throw std::logic_error("accepted missing asset");} catch(const std::runtime_error&) {}
    {
        BackdropImage space{2,64,std::vector<std::uint32_t>(128)};
        for(unsigned y=0;y<64;++y) for(unsigned x=0;x<2;++x)
            space.pixels[y*2+x]=0xff000000U|((y*3)<<16)|(y*2);
        for(float v:{-1.75f,-.25f,.25f,1.25f,2.25f}) {
            const auto a=space.sample(.2f,v,true),b=space.sample(.2f,v+1,true);
            for(unsigned k=0;k<3;++k) require(std::abs(a[k]-b[k])<.001f,"space vertical period");
        }
        require(space.sample(.2f,1.25f,true)!=space.sample(.2f,1.5f,true),"space edge was extruded");
        const auto a=space.sample(.2f,-.000001f,true),b=space.sample(.2f,.000001f,true);
        for(unsigned k=0;k<3;++k) require(std::abs(a[k]-b[k])<.01f,"space vertical seam");
        require(space.sample(.2f,2,false)==space.sample(.2f,1),"landscape clamp changed");
    }
    std::vector<std::uint8_t> bitmap(62);
    const auto word=[&](unsigned p,std::uint32_t v){for(unsigned k=0;k<4;++k) bitmap[p+k]=std::uint8_t(v>>(k*8));};
    bitmap[0]='B';bitmap[1]='M';word(10,54);word(14,40);word(18,1);word(22,2);bitmap[26]=1;bitmap[28]=24;
    bitmap[54]=255;bitmap[60]=255; // lower row blue, upper row red, four-byte strides
    auto decoded=BackdropImage::decode(bitmap);
    require(decoded.pixels==std::vector<std::uint32_t>{0xff0000ffu,0xffff0000u},"BMP row order or BGR channels");
    word(22,std::uint32_t(-2));decoded=BackdropImage::decode(bitmap);
    require(decoded.pixels==std::vector<std::uint32_t>{0xffff0000u,0xff0000ffu},"top-down BMP row order");
    {
        starfox::render::EnhancedBackdropLibrary library;
        unsigned calls=0;
        const auto loader=[&](unsigned resource,std::string_view path) {
            ++calls;
            require(resource>=200 && resource<200+starfox::render::enhanced_backdrop_assets.size(),"Backdrop resource ID changed");
            require(path==starfox::render::enhanced_backdrop_assets[resource-200].path,
                "Backdrop file and resource identities diverged");
            return std::span<const std::uint8_t>(bitmap);
        };
        const auto* first=&library.get(0,loader);const auto key=first->immutable_upload_key;
        require(key && first->pixels==decoded.pixels,"Shared loader changed ordinary artwork");
        require(&library.get(0,loader)==first && calls==1 && first->immutable_upload_key==key,
            "Repeated backdrop access reloaded or invalidated immutable artwork");
        auto prepared=decoded;prepared.prepare_zenith();
        for(unsigned index:{6U,16U,18U})
            require(library.get(index,loader).pixels==prepared.pixels,"Shared zenith preparation changed");
        require(library.index_of(first)==0 && !library.index_of(&decoded) && !library.index_of(nullptr),
            "Shared backdrop identity lookup misclassified an image");
        bool rejected=false;
        try {library.get(unsigned(starfox::render::enhanced_backdrop_assets.size()),loader);} catch(const std::out_of_range&) {rejected=true;}
        require(rejected && calls==4,"Invalid backdrop called its loader");
        rejected=false;
        try {library.get(1,[](unsigned,std::string_view){return std::span<const std::uint8_t>{};});}
        catch(const std::runtime_error&) {rejected=true;}
        require(rejected && library.get(1,loader).pixels==decoded.pixels && calls==5,
            "Failed backdrop decode poisoned subsequent retries");
    }
    if(argc>1) {
        std::ifstream stream(argv[1],std::ios::binary);
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>{stream},{}};
        const auto image=BackdropImage::decode(bytes);
        require(image.width>1000 && image.height>300,"backdrop lost source resolution");
        auto damaged=bytes;damaged.resize(bytes.size()/2);
        bool rejected=false;try {BackdropImage::decode(damaged);} catch(const std::runtime_error&) {rejected=true;}
        require(rejected,"truncated image accepted");
        std::cout<<"Decoded real backdrop "<<image.width<<'x'<<image.height<<"; ";
    }
    std::cout<<"seam overlap, negative scroll, pole clamps and malformed input passed\n";
}
