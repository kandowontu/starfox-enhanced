#pragma once
#include "starfox/render/backdrop_image.hpp"
#include "starfox/simulation/snes_ppu.hpp"
namespace starfox::render {
// Immutable color cloud + shade-encoded atmospheric limb. Live CGRAM is a
// uniform: palette fades never rebuild the texture or alter its ownership.
class CloudLimbAtlas {
    BackdropImage image_;
    std::uint64_t cloud_key_{},surface_key_{};
    std::array<std::uint8_t,40*34> source_{};
public:
    unsigned limb_ink(unsigned x,unsigned y) const {
        if(x>=80 || y>=32) return 0;
        const unsigned slot=((y/8)*10+x/8)*34;
        const unsigned tile=source_[slot]|(unsigned(source_[slot+1])<<8);
        const unsigned px=(tile&0x4000)?7-x%8:x%8,py=(tile&0x8000)?7-y%8:y%8;
        unsigned ink=0;
        for(unsigned bit=0;bit<4;++bit)
            ink|=((source_[slot+2+py*2+bit%2+(bit/2)*16]>>(7-px))&1)<<bit;
        return ink;
    }
    const BackdropImage& image(const BackdropImage& cloud,const BackdropImage& surface,
        const simulation::SnesPpuState& ppu) {
        decltype(source_) source{};
        const auto word=[&](unsigned a){return unsigned(ppu.vram[a&65535])|(unsigned(ppu.vram[(a+1)&65535])<<8);};
        // This authored scene uses 8x8 tiles, four map pages. Capture all tile
        // words and character bytes, including flips, to detect source changes.
        for(unsigned ty=0;ty<4;++ty) for(unsigned tx=0;tx<10;++tx) {
            const unsigned mx=20+tx,my=40+ty;
            const unsigned pages=(ppu.bg2_screen_size&1)?2:1;
            const unsigned entry=(mx/32+my/32*pages)*1024+(my%32)*32+mx%32;
            const unsigned tile=word(ppu.bg2_screen_base*2+entry*2);
            const unsigned slot=(ty*10+tx)*34;
            source[slot]=tile&255;source[slot+1]=tile>>8;
            for(unsigned i=0;i<32;++i)
                source[slot+2+i]=ppu.vram[(ppu.bg2_character_base*2+(tile&1023)*32+i)&65535];
        }
        if(image_.immutable_upload_key && source==source_ && cloud_key_==cloud.immutable_upload_key
            && surface_key_==surface.immutable_upload_key) return image_;
        source_=source;cloud_key_=cloud.immutable_upload_key;surface_key_=surface.immutable_upload_key;
        image_.immutable_upload_key=0;image_.width=image_.height=2048;
        image_.pixels.assign(2048*2048,0xff000000U);
        const auto put=[&](unsigned x,unsigned y,const std::array<float,3>& rgb) {
            std::uint32_t pixel=0xff000000U;
            for(unsigned c=0;c<3;++c) pixel|=std::uint32_t(std::clamp(rgb[c],0.f,255.f)+.5f)<<(c*8);
            image_.pixels[std::size_t(y)*2048+x]=pixel;
        };
        for(unsigned y=0;y<192;++y) for(unsigned x=0;x<192;++x) {
            const float u=(float(x)+.5f)/192,v=(float(y)+.5f)/192;
            put(320+x,1056+y,cloud.sample_projected((45+u*1175)/1253.f,(72+v*1115)/1253.f,3));
        }
        std::array<float,80*32> shades{};
        for(unsigned y=0;y<32;++y) for(unsigned x=0;x<80;++x) {
            const unsigned slot=((y/8)*10+x/8)*34;
            const unsigned tile=source[slot]|(unsigned(source[slot+1])<<8);
            const unsigned px=(tile&0x4000)?7-x%8:x%8,py=(tile&0x8000)?7-y%8:y%8;
            unsigned ink=0;
            for(unsigned bit=0;bit<4;++bit)
                ink|=((source[slot+2+py*2+bit%2+(bit/2)*16]>>(7-px))&1)<<bit;
            shades[y*80+x]=ink?float(15-ink)/14.f:0.f;
        }
        const auto shade=[&](int x,int y){return x>=0 && x<80 && y>=0 && y<32?shades[y*80+x]:0.f;};
        for(unsigned y=0;y<128;++y) for(unsigned x=0;x<320;++x) {
            const float sx=(float(x)+.5f)/4-.5f,sy=(float(y)+.5f)/4-.5f;
            const int ax=int(std::floor(sx)),ay=int(std::floor(sy));
            float value=std::lerp(std::lerp(shade(ax,ay),shade(ax+1,ay),sx-ax),
                std::lerp(shade(ax,ay+1),shade(ax+1,ay+1),sx-ax),sy-ay);
            const auto detail=surface.sample_projected((31+(sx+.5f)/80*1185)/1253.f,
                (29+(sy+.5f)/80*1180)/1253.f,3);
            // Retain the authored dark interior and narrow rim; texture relief
            // modulates existing illumination rather than inventing a full disk.
            value=std::clamp(value+value*(1-value)*(.15f*(detail[0]/255.f-.5f)),0.f,1.f)*255;
            put(640+x,1280+y,{value,value,value});
        }
        image_.seal_for_upload();return image_;
    }
};
}
