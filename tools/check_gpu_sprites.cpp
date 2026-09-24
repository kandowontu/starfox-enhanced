#include "starfox/render/sprite_renderer.hpp"
#include "starfox/render/gpu_raster.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include <SDL3/SDL.h>
#include <iostream>
int main() {
    using namespace starfox::render;
    if(!SDL_Init(SDL_INIT_VIDEO)) return 1;
    struct Lifetime {~Lifetime(){SDL_Quit();}} lifetime;
    GpuRaster gpu;SpriteRenderer sprites;
    unsigned cases=0;
    for(unsigned scale:{1U,2U,4U}) for(unsigned select=0;select<256;select+=7) {
        starfox::simulation::SnesPpuState ppu{};ppu.main_screen=16;ppu.object_select=select;
        std::uint32_t random=0x5ae913;
        for(auto& byte:ppu.vram) {random=random*1664525U+1013904223U;byte=std::uint8_t(random>>24);}
        for(unsigned i=0;i<48;++i) {
            const int x=int(i%8)*54-25;
            const unsigned y[]{250,24,33,128,168,193};
            ppu.oam[i*4]=std::uint8_t(x);ppu.oam[i*4+1]=y[i/8];
            ppu.oam[i*4+2]=std::uint8_t(i*19+1);
            ppu.oam[i*4+3]=std::uint8_t((i%4)*64+((i/4)%4)*16+((i/3)%8)*2+(i&1));
            ppu.oam[512+i/4]|=std::uint8_t(((x<0?1:0)|((i&1)*2))<<((i%4)*2));
        }
        // Cleared records and hidden route dots must remain invisible even large.
        ppu.oam[512+60/4]=0xaa;ppu.oam[64*4]=ppu.oam[64*4+1]=0xf8;
        ppu.oam[512+64/4]=2;
        for(bool wide:{false,true}) {
            const unsigned width=wide?400:256;
            Framebuffer expected(width,224,scale),recorded(width,224,scale),replayed(width,224,scale),actual(width,224,scale);
            for(auto* f:{&expected,&recorded,&replayed,&actual}) f->enable_layer_tags(true);
            RasterCommands batch;batch.reset(recorded.stored_width(),recorded.stored_height());
            recorded.record_to(&batch);
            const auto draw=[&](Framebuffer& f) {
                for(unsigned priority=0;priority<4;++priority)
                    sprites.draw_objects(ppu,f,std::uint8_t(priority),wide?72:0,wide,wide);
            };
            draw(expected);draw(recorded);recorded.record_to(nullptr);
            // Prove recording owns its VRAM, not a reference to next tick's memory.
            ppu.vram.fill(0);
            replay_raster_commands(batch,replayed,nullptr);
            if(expected.pixels()!=replayed.pixels() || expected.layer_tags()!=replayed.layer_tags()) {
                std::cerr<<"Sprite CPU replay differs select="<<select<<" scale="<<scale<<" wide="<<wide;return 2;
            }
            if(!gpu.render(batch,actual,nullptr)) {std::cerr<<gpu.status();return 3;}
            if(expected.pixels()!=actual.pixels() || expected.layer_tags()!=actual.layer_tags()) {
                std::cerr<<"Sprite GPU differs select="<<select<<" scale="<<scale<<" wide="<<wide;return 4;
            }
            for(auto& byte:ppu.vram) {random=random*1664525U+1013904223U;byte=std::uint8_t(random>>24);}
            ++cases;
        }
    }
    std::cout<<cases<<" sprite bitplane GPU/CPU/replay cases pass (sizes, banks, flips, priority, clipping, wrapping, wide HUD and snapshot ownership)\n";
    unsigned layers=0;
    for(unsigned sourceScale:{1U,2U,4U}) for(unsigned scale:{1U,2U,4U})
    for(unsigned step=1;step<=16;++step) for(bool tagged:{false,true}) {
        Framebuffer source(23,17,sourceScale),expected(35,25,scale),recorded(35,25,scale),actual(35,25,scale),replay(35,25,scale);
        source.enable_layer_tags(tagged);
        for(auto* f:{&expected,&recorded,&actual,&replay}) f->enable_layer_tags(true);
        for(unsigned y=0;y<source.stored_height();++y) for(unsigned x=0;x<source.stored_width();++x)
            source.set_stored(x,y,(x+y)%7?std::uint8_t(1+(x*13+y)%254):0,PixelLayer((x+y)%5));
        LayerCompositeSettings s;s.offset_x=int(step%9)-4;s.offset_y=int(step%7)-3;
        s.mosaic=step==1?0:(step-1)*16+1;s.mosaic_layer_mask=1;s.mosaic_origin_x=7;s.mosaic_origin_y=-2;
        s.clip_left=2;s.clip_top=1;s.clip_right=30;s.clip_bottom=22;
        composite_transparent_layer(source,expected,s);
        RasterCommands batch;batch.reset(recorded.stored_width(),recorded.stored_height());recorded.record_to(&batch);
        composite_transparent_layer(source,recorded,s);recorded.record_to(nullptr);
        source.clear();replay_raster_commands(batch,replay,nullptr);
        if(expected.pixels()!=replay.pixels() || expected.layer_tags()!=replay.layer_tags()) {
            std::cerr<<"Layer replay mismatch "<<sourceScale<<'/'<<scale<<'/'<<step;return 5;
        }
        if(!gpu.render(batch,actual,nullptr) || expected.pixels()!=actual.pixels() || expected.layer_tags()!=actual.layer_tags()) {
            std::cerr<<"Layer GPU mismatch "<<sourceScale<<'/'<<scale<<'/'<<step<<' '<<gpu.status();return 6;
        }
        ++layers;
    }
    std::cout<<layers<<" indexed layer GPU/replay cases match clipping, mosaic, scale conversion and per-pixel tags\n";
}
