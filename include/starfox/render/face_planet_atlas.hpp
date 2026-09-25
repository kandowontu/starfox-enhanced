#pragma once
#include "starfox/render/backdrop_image.hpp"
#include "starfox/simulation/snes_ppu.hpp"
namespace starfox::render {
class FacePlanetAtlas {
    std::array<std::array<std::uint16_t,16>,16> ramps_{};
    std::uint64_t master_key_{};
    BackdropImage image_;
public:
    const BackdropImage& image(const BackdropImage& master,const simulation::SnesPpuState& ppu) {
        decltype(ramps_) ramps{};
        const unsigned edge=ppu.bg2_tile_size_16?16:8;
        const unsigned pages=(ppu.bg2_screen_size&1)?2:1;
        for(unsigned i=0;i<face_planet_regions.size();++i) {
            const auto& r=face_planet_regions[i];
            if(r[2]-r[0]!=r[3]-r[1]) continue;
            const unsigned tx=unsigned((r[0]+r[2])*.5f)/edge,ty=unsigned((r[1]+r[3])*.5f)/edge;
            const unsigned entry=(tx/32+ty/32*pages)*1024+(ty%32)*32+tx%32;
            const unsigned address=(ppu.bg2_screen_base*2+entry*2)&65535;
            const unsigned tile=ppu.vram[address]|(unsigned(ppu.vram[(address+1)&65535])<<8);
            const unsigned bank=((tile>>10)&7)*16;
            // Inks 1..14 are this body's bright-to-dark ramp, zero is the
            // scene backdrop and 15 is not part of its surface.
            for(unsigned ink=1;ink<15;++ink) ramps[i][ink]=ppu.cgram[bank+ink];
        }
        if(image_.immutable_upload_key && ramps_==ramps && master_key_==master.immutable_upload_key)
            return image_;
        ramps_=ramps;master_key_=master.immutable_upload_key;
        image_.immutable_upload_key=0;
        image_.width=image_.height=2048;
        image_.pixels.assign(2048*2048,0xff000000U);
        for(unsigned i=0;i<face_planet_regions.size();++i) {
            const auto& r=face_planet_regions[i];
            if(r[2]-r[0]!=r[3]-r[1]) continue;
            const unsigned size=unsigned(r[2]-r[0])*4;
            for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x) {
                // Register the generated disk, excluding its black margin.
                const float u=std::lerp(.037f,.963f,(float(x)+.5f)/float(size));
                const float v=std::lerp(.034f,.947f,(float(y)+.5f)/float(size));
                const auto sample=master.sample_projected(u,v,3);
                const float shade=1+13*(1-std::clamp(sample[0]/240.f,0.f,1.f));
                const unsigned a=unsigned(shade),b=std::min(a+1,14U);
                std::uint32_t pixel=0xff000000U;
                for(unsigned c=0;c<3;++c) {
                    const auto channel=[&](unsigned n) {
                        const unsigned value=(ramps[i][n]>>(c*5))&31;
                        return float((value<<3)|(value>>2));
                    };
                    pixel|=std::uint32_t(std::lerp(channel(a),channel(b),shade-a)+.5f)<<(c*8);
                }
                image_.pixels[(unsigned(r[1])*4+y)*2048+unsigned(r[0])*4+x]=pixel;
            }
        }
        image_.seal_for_upload();return image_;
    }
};
}
