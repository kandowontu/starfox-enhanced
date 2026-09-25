#include "starfox/render/gpu_effects.hpp"
#include "starfox/render/effects.hpp"
#include "starfox/render/hdr_effect.hpp"
#include "starfox/render/chromatic_aberration.hpp"
#include "starfox/render/model_smoothing.hpp"
#include "starfox/render/bloom.hpp"
#include "starfox/render/frame_persistence.hpp"
#include "starfox/render/pixel_filter.hpp"
#if defined(STARFOX_TEST_PORTABLE_GPU)
#include "starfox/render/sdl_gpu_effects.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#else
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#endif
#include <iostream>
#include <chrono>
#include <cstring>
int main() {
    using namespace starfox::render;
#if defined(STARFOX_TEST_PORTABLE_GPU)
    if(!SDL_Init(SDL_INIT_VIDEO)) return 1;
    struct Device {
        SDL_GPUDevice* value{};
        SDL_GPUDevice* Get() {return value;}
        ~Device(){if(value) SDL_DestroyGPUDevice(value);SDL_Quit();}
    } device{SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_MSL
        |SDL_GPU_SHADERFORMAT_DXIL,true,nullptr)};
    if(!device.Get()) {std::cerr<<SDL_GetError()<<'\n';return 1;}
    SdlGpuEffects gpu;
#else
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,device.GetAddressOf(),nullptr,nullptr);
    if(FAILED(hr)) return 1;
    GpuEffects gpu;
#endif
    Framebuffer frame(257,129,2); frame.enable_layer_tags(true);
    // All environment variants: both packed/native paths share the shader;
    // this reference also checks that model, HUD and dust tags remain intact.
    for(float slope : {-.25f,0.f,.25f})
    for(unsigned reflections=0;reflections<2;++reflections)
    for(unsigned ground=0;ground<=8;++ground) for(unsigned sky=0;sky<=4;++sky) {
        Framebuffer f(37,29,2);f.enable_layer_tags(true);
        std::vector<std::uint8_t> cpu(f.pixels().size()*4,130);
        GpuEffectSettings settings;auto& e=settings.environment;
        e.modes={ground,ground%4,sky,sky%3};e.motion={12,48,-192,2.75};
        if(!ground && !sky) e.scroll_fraction={slope,.375f,0,0};
        e.water_reflections=reflections!=0;e.plane[0]=slope;e.plane[1]=float(1+ground%4);
        for(unsigned i=0;i<256;++i)e.classes[i]=i%7;
        for(unsigned i=0;i<f.pixels().size();++i) {
            f.pixels()[i]=i%256;f.layer_tags()[i]=(i/7)%6;
            // Nonuniform source colours exercise fractional reflected taps
            // and HUD exclusion; a flat fixture cannot detect wrong samples.
            cpu[i*4]=std::uint8_t((i%f.stored_width())*3);
            cpu[i*4+1]=std::uint8_t((i/f.stored_width())*4);
            cpu[i*4+2]=std::uint8_t((i*17)%256);cpu[i*4+3]=255;
        }
        auto hardware=cpu;apply_environment(e,f,cpu);
        if(!gpu.apply(device.Get(),f,hardware,settings)) {std::cerr<<gpu.status();return 40;}
        for(unsigned i=0;i<cpu.size();++i) if(std::abs(int(cpu[i])-int(hardware[i]))>1) {
            std::cerr<<"Environment mismatch "<<ground<<','<<sky<<" at "<<i<<'\n';return 41;
        }
    }
    std::cout<<"Environment materials/motions: CPU/GPU match; protected layers unchanged\n";
#if defined(STARFOX_TEST_PORTABLE_GPU)
    {
        BackdropImage sky;sky.width=64;sky.height=16;sky.pixels.resize(1024);
        for(unsigned y=0;y<16;++y) for(unsigned x=0;x<64;++x)
            sky.pixels[y*64+x]=0xff000000u|(x*4)|((y*16)<<8)|(((x+y)*3)<<16);
        for(unsigned style=1;style<=4;++style) for(unsigned motion=0;motion<3;++motion)
        for(float roll:{-.3f,0.f,.3f}) for(float scroll:{-512.1f,0.f,511.9f}) for(float brightness:{0.f,.5f,1.f}) for(unsigned layout:{0U,1U,2U,3U,4U,5U,6U,7U,8U,9U}) {
            const bool orbital=layout!=0;
            Framebuffer f(37,29,2);f.enable_layer_tags(true);
            for(unsigned i=0;i<f.pixels().size();++i) f.layer_tags()[i]=(i/7)%6;
            std::vector<std::uint8_t> cpu(f.pixels().size()*4,120),hardware=cpu;
            GpuEffectSettings settings;auto& e=settings.environment;
            e.modes[2]=style;e.modes[3]=motion;e.motion={20,0,0,5.25f};e.plane={roll,0,scroll,brightness};e.backdrop=&sky;
            e.backdrop_palette={{{.15f,-.2f,.1f,.8f},{-.1f,.2f,-.25f,.65f}}};
            // Authored sky can cross below the fitted horizon when banking.
            // Exercise its palette ownership as well as unclassified margins.
            e.classes[1]=6;e.classes[2]=1;
            for(unsigned i=0;i<f.pixels().size();++i) f.pixels()[i]=i%3;
            if(motion==1 || motion==2) {
                e.backdrop_ramp[0]=motion==1?2U:orbital?1U|(220U<<16):1U|(140U<<8)|(245U<<16);
                for(unsigned shade=1;shade<16;++shade)
                    e.backdrop_ramp[shade]=(255-shade*12)|((220-shade*12)<<8)|((150-shade*10)<<16);
            }
            if(orbital) {
                e.backdrop_projection={1/512.f,1/224.f,.633f,1.f};
                e.backdrop_keep={{{-6,15,4,5},{8,9,3,3}}};
            }
            if(layout==2) e.backdrop_projection={1/512.f,1/16.f,.8f,2.f};
            if(layout==3) {
                e.backdrop_projection={1/24.f,1/16.f,.5f,3.f};
                e.backdrop_keep[0]={0,14,12,8};
            }
            if(layout==4) {
                e.backdrop_projection={1/24.f,1/16.f,.5f,4.f};
                e.backdrop_keep={{{384,152,12,8},{roll,-roll,384.f+scroll*.01f,138.f}}};
                e.plane[2]=12;
            }
            if(layout==5) {
                e.backdrop_projection={1/512.f,1/512.f,0,5};
                e.backdrop_keep[1]={roll,-roll,128.f+scroll*.01f,18.f};
            }
            if(layout==6 || layout==7) {
                e.backdrop_projection={1/512.f,layout==7?1/4.f:1/224.f,.55f,float(layout)};
                e.backdrop_keep={{{-6,8,4,4},{8,14,5,5}}};
                  e.backdrop_ramp.fill(0);e.backdrop_ramp[1]=0xff8050;e.backdrop_ramp[2]=0x80a0ff;
                  if(motion==1) {e.backdrop_ramp[3]=0x606060;e.backdrop_ramp[4]=0x404040;e.backdrop_ramp[5]=1;}
                if(motion==2) e.backdrop_keep[1]={.4f,31.f/56.f,-1,0};
                if(layout==7 && motion==1) {
                    e.backdrop_ramp[6]=1;
                    for(unsigned shade=0;shade<8;++shade)
                        e.backdrop_ramp[7+shade]=(210-shade*20)|((100-shade*10)<<8)|((35-shade*5)<<16);
                }
            }
            if(layout==8) {
                e.backdrop_projection={1/512.f,1/160.f,1,8};e.classes[2]=7;
                const auto& body=city_moons[(style+motion)%city_moons.size()];
                e.backdrop_keep={{{},{body[0]+scroll*.001f,body[1]-14,0,0}}};
                e.backdrop_ramp.fill(0);e.backdrop_ramp[6]=2;
                for(unsigned shade=7;shade<=11;++shade)
                    e.backdrop_ramp[shade]=(230-shade*10)|((180-shade*10)<<8)|((shade*20)<<16);
            }
            if(layout==9) {
                e.backdrop_projection={1/512.f,1/512.f,0,9};
                const auto& body=cloud_limb_regions[style%2];
                e.backdrop_keep[1]={roll,-roll,(body[0]+body[2])*.5f+scroll*.001f,
                    (body[1]+body[3])*.5f-14};
                e.backdrop_ramp.fill(0);
                for(unsigned ink=1;ink<16;++ink)
                    e.backdrop_ramp[ink]=(255-ink*15)|((255-ink*12)<<8)|((255-ink*15)<<16);
            }
            apply_environment(e,f,cpu);
            if(!gpu.apply(device.Get(),f,hardware,settings)) {std::cerr<<gpu.status();return 42;}
            for(unsigned i=0;i<cpu.size();++i) if(std::abs(int(cpu[i])-int(hardware[i]))>1) {
                std::cerr<<"Backdrop CPU/GPU mismatch at "<<i<<" layout="<<layout
                    <<" style="<<style<<" motion="<<motion<<" roll="<<roll<<" scroll="<<scroll
                    <<" brightness="<<brightness<<" CPU="<<unsigned(cpu[i])<<" GPU="<<unsigned(hardware[i])<<'\n';return 43;
            }
        }
        std::cout<<"Enhanced backdrop: GPU/CPU scroll, roll, fades and layer protection passed\n";
        // Check semantics independently of parity: two equally wrong paths
        // must not pass by animating Aurora while Sky Motion is OFF.
        for(bool photographic:{false,true}) for(unsigned style=1;style<=4;++style) {
            Framebuffer f(64,48);f.enable_layer_tags(true);
            std::fill(f.layer_tags().begin(),f.layer_tags().end(),2);
            GpuEffectSettings settings;auto& e=settings.environment;
            e.classes.fill(6);e.modes[2]=style;e.motion[0]=160;e.plane[3]=1;
            e.backdrop=photographic?&sky:nullptr;
            const std::vector<std::uint8_t> input(f.pixels().size()*4,120);
            auto first=input,second=input,hardware_first=input,hardware_second=input;
            apply_environment(e,f,first);
            if(!gpu.apply(device.Get(),f,hardware_first,settings)) return 84;
            e.motion[3]=93.5f;
            apply_environment(e,f,second);
            if(!gpu.apply(device.Get(),f,hardware_second,settings)) return 85;
            if(first!=second || hardware_first!=hardware_second) {
                std::cerr<<"Sky Motion OFF animated style "<<style<<" photographic="<<photographic;return 86;
            }
            e.modes[3]=1;second=input;
            apply_environment(e,f,second);
            if(first==second) {std::cerr<<"Sky drift did not animate style "<<style;return 87;}
        }
        std::cout<<"Sky Motion OFF freezes every procedural/photographic style; drift remains active\n";
    }
    {
        SdlGpuEffects cached;
        Framebuffer f(16,16);f.enable_layer_tags(true);
        std::fill(f.layer_tags().begin(),f.layer_tags().end(),2);
        const std::vector<std::uint8_t> input(f.pixels().size()*4,120);
        auto pixels=input;
        if(!cached.apply(device.Get(),f,pixels,{})) return 80;
        const auto baseline=cached.last_staging_upload_bytes();
        BackdropImage sky;sky.width=64;sky.height=16;sky.pixels.assign(1024,0xff553311);
        GpuEffectSettings settings;auto& e=settings.environment;
        e.modes[2]=1;e.motion[0]=20;e.plane[3]=1;e.backdrop=&sky;
        for(unsigned step=0;step<13;++step) {
            bool upload=step==0 || step==2 || step==3 || step==6 || step==8 || step==9 || step==11;
            if(step==2) sky.pixels[0]^=0x00ffffff; // Same object, changed contents.
            if(step==3) {sky.width=128;sky.pixels.resize(2048,0xff779955);}
            if(step==4) {sky.width=64;sky.height=32;} // Shape only: reuse bytes.
            if(step==5) {e.modes[2]=0;sky.pixels[3]^=0x00ffffff;}
            if(step==6) e.modes[2]=1; // Catch edits made while disabled.
            if(step==7) {e.plane[0]=.3f;e.plane[2]=37;e.plane[3]=.5f;}
            if(step==8) cached.release_device();
            if(step==9) sky.seal_for_upload();
            if(step==11) {sky.immutable_upload_key=0;sky.pixels[0]^=0x00ffffff;sky.seal_for_upload();}
            pixels=input;auto reference=input;apply_environment(e,f,reference);
            if(!cached.apply(device.Get(),f,pixels,settings)) {std::cerr<<cached.status();return 81;}
            const auto expected=baseline+(upload?((sky.pixels.size()*4+255)&~std::size_t(255)):0);
            if(cached.last_staging_upload_bytes()!=expected) {
                std::cerr<<"Backdrop upload cache mismatch at "<<step;return 82;
            }
            for(unsigned i=0;i<pixels.size();++i) if(std::abs(int(pixels[i])-int(reference[i]))>1) {
                std::cerr<<"Stale cached backdrop at "<<step<<':'<<i;return 83;
            }
        }
        std::cout<<"Backdrop residency: unchanged frames skip image upload; edits, resize, toggle and device reset pass\n";
    }
    {
        Framebuffer f(8,1);f.enable_layer_tags(true);
        std::fill(f.pixels().begin(),f.pixels().end(),1);
        std::fill(f.layer_tags().begin(),f.layer_tags().end(),2);
        f.layer_tags()[1]=1;f.layer_tags()[2]=0;f.pixels()[3]=2;
        const std::uint32_t traced[8]={0xfe332211,0xfe332211,0xfe332211,0xfe332211,
            0xff332211,0,0xfe332211,0xfe332211};
        SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizeof(traced),0};
        auto* buffer=SDL_CreateGPUBuffer(device.Get(),&bi);
        SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,sizeof(traced),0};
        auto* transfer=SDL_CreateGPUTransferBuffer(device.Get(),&ti);
        if(!buffer || !transfer) return 42;
        auto* mapped=SDL_MapGPUTransferBuffer(device.Get(),transfer,false);if(!mapped) return 43;
        std::memcpy(mapped,traced,sizeof(traced));SDL_UnmapGPUTransferBuffer(device.Get(),transfer);
        auto* command=SDL_AcquireGPUCommandBuffer(device.Get());auto* pass=SDL_BeginGPUCopyPass(command);
        SDL_GPUTransferBufferLocation source{transfer,0};SDL_GPUBufferRegion target{buffer,0,sizeof(traced)};
        SDL_UploadToGPUBuffer(pass,&source,&target,false);SDL_EndGPUCopyPass(pass);
        if(!SDL_SubmitGPUCommandBuffer(command)) return 44;
        for(unsigned material:{6U,7U,8U}) {
            GpuEffectSettings settings;auto& e=settings.environment;
            e.modes={material,0,0,0};e.motion={-10,0,0,0};e.classes[1]=5;e.ray_water=true;
            settings.resident_reflection={device.Get(),buffer,8,1,32};
            std::vector<std::uint8_t> pixels(32,100),reference=pixels;
            apply_environment(e,f,reference);
            if(!gpu.apply(device.Get(),f,pixels,settings)) {std::cerr<<gpu.status();return 45;}
            for(unsigned i=0;i<8;++i) for(unsigned c=0;c<3;++c) {
                const bool selected=i==0 || i>=6;
                const int expected=selected?int((traced[i]>>(c*8))&255):reference[i*4+c];
                if(std::abs(int(pixels[i*4+c])-expected)>1) {std::cerr<<"Ray ground composition leaked/missed "<<material<<':'<<i;return 46;}
            }
        }
        SDL_ReleaseGPUTransferBuffer(device.Get(),transfer);SDL_ReleaseGPUBuffer(device.Get(),buffer);
        std::cout<<"Ray ground composition: water/mirror/gold, HUD/model/sky protection and model-ray marker rejection passed\n";
    }
#endif
    std::vector<std::uint8_t> source(frame.pixels().size()*4),scratch;
    for(std::size_t i=0;i<frame.pixels().size();++i) {
        frame.layer_tags()[i]=(i/17)%5;
        source[i*4]=(i*13)%256; source[i*4+1]=(i*19)%256;
        source[i*4+2]=(i*7)%256; source[i*4+3]=255;
    }
#if defined(STARFOX_TEST_PORTABLE_GPU)
    for(unsigned mode:{1U,2U}) for(unsigned fps:{60U,120U,240U}) {
        FramePersistence reference;
        Framebuffer trail(8,4);trail.enable_layer_tags(true);
        for(unsigned tick=0;tick<=fps;++tick) {
            std::fill(trail.layer_tags().begin(),trail.layer_tags().end(),std::uint8_t(PixelLayer::background));
            std::vector<std::uint8_t> cpu(128,0);
            for(unsigned i=3;i<cpu.size();i+=4)cpu[i]=255;
            const auto moving=(tick/(fps/8))%8;
            trail.layer_tags()[moving]=std::uint8_t(PixelLayer::three_d);
            cpu[moving*4]=240;cpu[moving*4+1]=60;
            trail.layer_tags()[7]=std::uint8_t(PixelLayer::two_d);
            cpu[28]=17;
            auto hardware=cpu;
            // An unrelated pass between frames must not erase or advance the
            // final style pass's history (legacy multi-pass presentation).
            GpuEffectSettings intermediate;intermediate.preserve_persistence=true;
            Framebuffer overlay(4,2);overlay.enable_layer_tags(true);
            std::vector<std::uint8_t> interim(32,0);
            if(!gpu.apply(device.Get(),overlay,interim,intermediate)) return 33;
            GpuEffectSettings settings;settings.persistence_mode=mode;settings.persistence_models=true;
            settings.presentation_seconds=double(tick)/fps;settings.scene_epoch=mode*1000+fps+(tick>=fps/2?10000:0);
            reference.apply(trail,cpu,static_cast<PersistenceMode>(mode),true,false,
                settings.presentation_seconds,settings.scene_epoch);
            if(!gpu.apply(device.Get(),trail,hardware,settings)) {std::cerr<<gpu.status();return 30;}
            for(std::size_t i=0;i<cpu.size();++i)if(std::abs(int(cpu[i])-int(hardware[i]))>1) {
                std::cerr<<"persistence mismatch mode="<<mode<<" fps="<<fps<<" tick="<<tick<<" byte="<<i<<'\n';return 31;
            }
        }
    }
    // Interleaved outputs must never inherit another eye's bright objects.
    Framebuffer eye_frame(8,4);eye_frame.enable_layer_tags(true);
    std::array<FramePersistence,3> eyes;
    for(unsigned tick=0;tick<4;++tick) for(unsigned eye=0;eye<3;++eye) {
        std::fill(eye_frame.layer_tags().begin(),eye_frame.layer_tags().end(),std::uint8_t(PixelLayer::background));
        std::vector<std::uint8_t> cpu(128,0);
        for(unsigned i=3;i<cpu.size();i+=4)cpu[i]=255;
        if(!tick) {eye_frame.layer_tags()[eye]=std::uint8_t(PixelLayer::three_d);cpu[eye*4+eye]=240;}
        auto hardware=cpu;
        GpuEffectSettings settings;settings.persistence_mode=2;settings.persistence_models=true;
        settings.persistence_slot=eye;settings.scene_epoch=90000;settings.presentation_seconds=double(tick)/60;
        eyes[eye].apply(eye_frame,cpu,PersistenceMode::long_exposure,true,false,settings.presentation_seconds,90000);
        if(!gpu.apply(device.Get(),eye_frame,hardware,settings) || cpu!=hardware) {
            std::cerr<<"persistence eye history contamination\n";return 32;
        }
    }
    std::cout<<"Temporal persistence: CPU/GPU trails and long exposure agree at 60/120/240 Hz, including HUD, scene reset and eye isolation\n";
#endif
    for(unsigned effect=0;effect<effect_count;++effect) for(unsigned intensity:{0U,37U,100U}) {
        auto cpu=source,hardware=source;
        GpuEffectSettings settings;
        settings.model_effect=effect; settings.world_effect=(effect+3)%effect_count;
        settings.model_intensity=intensity; settings.world_intensity=100-intensity;
        apply_effect(Effect(settings.model_effect),frame,cpu,scratch,settings.model_intensity,
            Effect(settings.world_effect),settings.world_intensity);
        if(!gpu.apply(device.Get(),frame,hardware,settings)) {std::cerr<<gpu.status();return 2;}
        if(cpu!=hardware) {
            for(std::size_t i=0;i<cpu.size();++i) if(cpu[i]!=hardware[i]) {
                std::cerr<<"style mismatch effect="<<effect<<" intensity="<<intensity<<" byte="<<i
                    <<" cpu="<<int(cpu[i])<<" gpu="<<int(hardware[i])<<'\n'; break;
            }
            return 3;
        }
    }
    for(unsigned level=1;level<=3;++level) {
        auto cpu=source,hardware=source;
        GpuEffectSettings settings; settings.hdr=level; settings.chromatic=level;settings.smoothing=level;
        apply_hdr_effect(frame,cpu,level);apply_chromatic_aberration(frame,cpu,scratch,level);
        smooth_models(level,frame,cpu,scratch);
        if(!gpu.apply(device.Get(),frame,hardware,settings)) return 4;
        unsigned maximum=0; std::size_t different=0;
        for(std::size_t i=0;i<cpu.size();++i) {maximum=std::max(maximum,unsigned(std::abs(int(cpu[i])-int(hardware[i]))));different+=cpu[i]!=hardware[i];}
        std::cout<<"level="<<level<<" differing_bytes="<<different<<" max_delta="<<maximum<<'\n';
        if(maximum>1) return 5;
    }
    for(unsigned mode=1;mode<effect_count;++mode) {
        if(!spatial_manipulation(static_cast<Effect>(mode))) continue;
        auto cpu=source,hardware=source;
        std::vector<std::uint8_t> scratch;
        GpuEffectSettings settings;settings.model_effect=1;settings.material=unsigned(Effect::pearl);settings.manipulation=mode;settings.manipulation_intensity=70;
        apply_effect(Effect::pearl,frame,cpu,scratch);
        apply_effect(Effect::cel_drawn,frame,cpu,scratch);
        apply_effect(static_cast<Effect>(mode),frame,cpu,scratch,70);
        if(!gpu.apply(device.Get(),frame,hardware,settings)) return 51;
        for(std::size_t i=0;i<cpu.size();++i) if(std::abs(int(cpu[i])-int(hardware[i]))>1) {
            std::cerr<<"combined manipulation mismatch "<<mode<<" byte "<<i<<'\n';return 52;
        }
    }
    std::cout<<"Independent Cel + manipulation passes match CPU\n";
    BloomPass bloom;
    for(unsigned model=0;model<=3;++model) for(unsigned world=0;world<=3;++world) {
        auto cpu=source,hardware=source;
        GpuEffectSettings settings;settings.bloom_model=model;settings.bloom_world=world;
        bloom.apply(model,world,frame,cpu);
        if(!gpu.apply(device.Get(),frame,hardware,settings)) return 6;
        unsigned maximum=0;std::size_t different=0;
        for(std::size_t i=0;i<cpu.size();++i) {maximum=std::max(maximum,unsigned(std::abs(int(cpu[i])-int(hardware[i]))));different+=cpu[i]!=hardware[i];}
        std::cout<<"bloom="<<model<<'/'<<world<<" differing_bytes="<<different<<" max_delta="<<maximum<<'\n';
        if(maximum>1) return 7;
    }
    std::cout<<gpu.status()<<": styles, tone/chromatic/smoothing and bloom checked\n";
    for(int offset:{-7,0,9}) {
        const unsigned sw=frame.stored_width()-3,sh=frame.stored_height()-5;
        std::vector<std::uint8_t> mask(std::size_t(sw)*sh);
        for(std::size_t i=0;i<mask.size();++i) mask[i]=std::uint8_t(i*17);
        auto cpu=source,hardware=source;
        for(unsigned y=0;y<frame.stored_height();++y) for(unsigned x=0;x<frame.stored_width();++x) {
            const int sy=int(y)-offset;const auto i=std::size_t(y)*frame.stored_width()+x;
            const auto tag=frame.layer_tags()[i];
            if(x>=sw || sy<0 || sy>=int(sh) || (tag!=0 && tag!=2 && tag!=4)) continue;
            for(unsigned c=0;c<3;++c) cpu[i*4+c]=unsigned(cpu[i*4+c])*(255-mask[std::size_t(sy)*sw+x])/255;
        }
        GpuEffectSettings settings;settings.shadow_mask=mask;settings.shadow_width=sw;
        settings.shadow_height=sh;settings.shadow_offset_y=offset;
        if(!gpu.apply(device.Get(),frame,hardware,settings) || cpu!=hardware) return 10;
    }
    std::cout<<"GPU shadow composition: exact for negative/zero/positive offsets\n";
    for(unsigned level=1;level<=3;++level) {
        auto separate=source,batched=source;
        GpuEffectSettings style;style.smoothing=level;style.model_effect=level;
        style.world_effect=level+3;
        if(!gpu.apply(device.Get(),frame,separate,style)) return 11;
        const auto base=separate;
        GpuEffectSettings glow;glow.bloom_model=level;glow.bloom_world=4-level;
        if(!gpu.apply(device.Get(),frame,separate,glow)) return 12;
        const auto bloomed=separate;
        GpuEffectSettings aa;aa.anti_aliasing=level;
        if(!gpu.apply(device.Get(),frame,separate,aa)) return 13;
        std::vector<std::uint8_t> snapshotBase,snapshotGlow;
        style.bloom_model=glow.bloom_model;style.bloom_world=glow.bloom_world;
        style.anti_aliasing=level;style.bloom_base=&snapshotBase;style.bloom_glow=&snapshotGlow;
        if(!gpu.apply(device.Get(),frame,batched,style) || separate!=batched
            || base!=snapshotBase || bloomed!=snapshotGlow) return 14;
        double timings[2]{};
        for(unsigned run=0;run<8;++run) for(unsigned order=0;order<2;++order) {
            const unsigned mode=(run+order)%2;auto pixels=source;
            const auto start=std::chrono::steady_clock::now();
            if(mode) {
                if(!gpu.apply(device.Get(),frame,pixels,style)) return 15;
            } else {
                auto onlyStyle=style;onlyStyle.bloom_model=onlyStyle.bloom_world=onlyStyle.anti_aliasing=0;
                onlyStyle.bloom_base=onlyStyle.bloom_glow=nullptr;
                if(!gpu.apply(device.Get(),frame,pixels,onlyStyle)) return 15;
                snapshotBase=pixels;
                if(!gpu.apply(device.Get(),frame,pixels,glow)) return 15;
                snapshotGlow=pixels;
                if(!gpu.apply(device.Get(),frame,pixels,aa)) return 15;
            }
            timings[mode]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        }
        std::cout<<"level="<<level<<" separate_ms="<<timings[0]/8<<" batched_ms="<<timings[1]/8<<'\n';
    }
    std::cout<<"Batched styles/bloom/AA and both bloom snapshots: exact\n";
#if defined(STARFOX_TEST_PORTABLE_GPU)
    {
        SDL_GPUTextureCreateInfo info{};
        info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;info.width=frame.stored_width();info.height=frame.stored_height();
        info.layer_count_or_depth=info.num_levels=1;
        auto* target=SDL_CreateGPUTexture(device.Get(),&info);
        if(!target) return 40;
        // Alternate captured and uncaptured frames, including replacement of
        // an unread frame. Capture must always return the latest composition.
        for(unsigned level=0;level<4;++level) {
            GpuEffectSettings settings;settings.hdr=level;settings.bloom_model=level;
            auto expected=source,direct=source;
            if(!gpu.apply(device.Get(),frame,expected,settings)) return 41;
            // Start direct presentation with no staging allocation. A prior
            // CPU-readback pass must not be required for the first capture.
            gpu.release_device();
            settings.presentation_texture=target;
            for(unsigned repeat=0;repeat<2;++repeat)
                if(!gpu.apply(device.Get(),frame,direct,settings) || direct!=source) return 42;
            if(!gpu.readback(direct) || direct!=expected) return 43;
            direct.clear();
            if(!gpu.readback(direct) || direct!=expected) return 44;
        }
        SDL_ReleaseGPUTexture(device.Get(),target);
        std::cout<<"Portable deferred capture: exact, including unread replacement and repeated reads\n";
    }
#endif
    {
#if !defined(STARFOX_TEST_PORTABLE_GPU)
        D3D11_TEXTURE2D_DESC desc{};desc.Width=frame.stored_width();desc.Height=frame.stored_height();
        desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> target,staging;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,target.GetAddressOf()))) return 16;
        desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,staging.GetAddressOf()))) return 16;
        auto expected=source,direct=source;GpuEffectSettings settings;
        settings.model_effect=3;settings.world_effect=5;settings.smoothing=2;settings.anti_aliasing=2;
        if(!gpu.apply(device.Get(),frame,expected,settings)) return 17;
        settings.presentation_texture=target.Get();
        if(!gpu.apply(device.Get(),frame,direct,settings) || direct!=source) return 18;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(context.GetAddressOf());
        context->CopyResource(staging.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))) return 19;
        bool equal=true;
        for(unsigned y=0;y<desc.Height;++y) for(unsigned x=0;x<desc.Width*4;++x)
            equal &= static_cast<const std::uint8_t*>(mapped.pData)[std::size_t(y)*mapped.RowPitch+x]
                ==expected[std::size_t(y)*desc.Width*4+x];
        context->Unmap(staging.Get(),0);
        if(!equal || !gpu.readback(direct) || direct!=expected) return 20;
        std::cout<<"Direct GPU presentation and deferred capture: exact\n";
        Microsoft::WRL::ComPtr<ID3D11Texture2D> glowTarget;
        target->GetDesc(&desc);
        if(FAILED(device->CreateTexture2D(&desc,nullptr,glowTarget.GetAddressOf()))) return 21;
        for(unsigned level=1;level<=3;++level) {
            std::vector<std::uint8_t> base,glowPixels;
            settings.presentation_texture=nullptr;settings.presentation_glow_texture=nullptr;
            settings.bloom_model=level;settings.bloom_world=4-level;settings.anti_aliasing=level;
            settings.bloom_base=&base;settings.bloom_glow=&glowPixels;
            expected=source;
            if(!gpu.apply(device.Get(),frame,expected,settings)) return 22;
            split_bloom_layer(base,glowPixels,expected);
            direct=source;settings.presentation_texture=target.Get();settings.presentation_glow_texture=glowTarget.Get();
            if(!gpu.apply(device.Get(),frame,direct,settings) || direct!=source) return 23;
            for(unsigned layer=0;layer<2;++layer) {
                context->CopyResource(staging.Get(),layer?glowTarget.Get():target.Get());
                if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))) return 24;
                const auto& reference=layer?glowPixels:base;equal=true;
                for(unsigned y=0;y<desc.Height;++y) for(unsigned x=0;x<desc.Width*4;++x)
                    equal &= static_cast<const std::uint8_t*>(mapped.pData)[std::size_t(y)*mapped.RowPitch+x]
                        ==reference[std::size_t(y)*desc.Width*4+x];
                context->Unmap(staging.Get(),0);
                if(!equal) return 25;
            }
            if(!gpu.readback(direct) || direct!=expected) return 26;
        }
        std::cout<<"Direct GPU bloom split, base/glow textures and capture: exact\n";
        Microsoft::WRL::ComPtr<ID3D11Texture2D> modelTarget;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,modelTarget.GetAddressOf()))) return 27;
        SurfaceBuffer surfaces(desc.Width-4,desc.Height-3);
        for(unsigned y=0;y<surfaces.height();++y) for(unsigned x=0;x<surfaces.width();++x)
            if((x/7+y/9)%3) surfaces.set(x,y,SurfaceSample{},(x+y)%11?0:1);
        for(unsigned level=0;level<=3;++level) {
            std::vector<std::uint8_t> base,glowPixels;
            settings.presentation_texture=settings.presentation_glow_texture=settings.presentation_model_texture=nullptr;
            settings.bloom_model=level;settings.bloom_world=level;settings.bloom_base=&base;settings.bloom_glow=&glowPixels;
            expected=source;
            if(!gpu.apply(device.Get(),frame,expected,settings)) return 28;
            if(level) split_bloom_layer(base,glowPixels,expected);else base=expected;
            auto modelBase=base;std::vector<std::uint8_t> modelPixels(base.size(),0);
            settings.surfaces=&surfaces;settings.surface_x=int(level)-2;settings.surface_y=3-int(level)*2;
            const auto owns=[&](int x,int y) {
                const int sx=x-settings.surface_x,sy=y-settings.surface_y;
                return x>=0 && y>=0 && x<int(desc.Width) && y<int(desc.Height) && sx>=0 && sy>=0
                    && sx<int(surfaces.width()) && sy<int(surfaces.height()) && surfaces.get(sx,sy).valid
                    && surfaces.get(sx,sy).palette_index==frame.get_stored(x,y);
            };
            const int offsets[8][2]={{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}};
            for(unsigned y=0;y<desc.Height;++y) for(unsigned x=0;x<desc.Width;++x) if(owns(x,y)) {
                const auto i=(std::size_t(y)*desc.Width+x)*4;
                std::copy_n(base.data()+i,4,modelPixels.data()+i);modelPixels[i+3]=255;
                for(const auto& d:offsets) {
                    int nx=int(x)+d[0],ny=int(y)+d[1];
                    if(nx<0 || ny<0 || nx>=int(desc.Width) || ny>=int(desc.Height)) continue;
                    if(!owns(nx,ny)) {std::copy_n(base.data()+(std::size_t(ny)*desc.Width+nx)*4,4,modelBase.data()+i);break;}
                }
            }
            settings.presentation_texture=target.Get();settings.presentation_glow_texture=level?glowTarget.Get():nullptr;
            settings.presentation_model_texture=modelTarget.Get();direct=source;
            if(!gpu.apply(device.Get(),frame,direct,settings)) return 29;
            for(unsigned layer=0;layer<2;++layer) {
                context->CopyResource(staging.Get(),layer?modelTarget.Get():target.Get());
                if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))) return 30;
                const auto& reference=layer?modelPixels:modelBase;equal=true;
                for(unsigned y=0;y<desc.Height;++y) for(unsigned x=0;x<desc.Width*4;++x)
                    equal &= static_cast<const std::uint8_t*>(mapped.pData)[std::size_t(y)*mapped.RowPitch+x]
                        ==reference[std::size_t(y)*desc.Width*4+x];
                context->Unmap(staging.Get(),0);if(!equal) return 31;
            }
            if(!gpu.readback(direct) || direct!=expected) return 32;
        }
        std::cout<<"GPU 1440p model separation, offsets, occlusion and bloom combinations: exact\n";
#endif
    }
    std::array<Rgba8,256> palette{};
    for(unsigned i=0;i<256;++i) palette[i]={std::uint8_t(i*13),std::uint8_t(i*7),std::uint8_t(i*19),255};
    PixelFilterScratch filterScratch;RowWorkers workers;workers.set_worker_count(4);
    // AA runs after the 2D filter in both native and portable GPU pipelines.
    // CRT scanlines and other authored art must remain byte-exact while model,
    // world and terrain geometry still receive AA.
    {
        Framebuffer layered(20,16);layered.enable_layer_tags(true);
        std::vector<std::uint8_t> source(layered.pixels().size()*4,255);
        for(unsigned y=0;y<16;++y) for(unsigned x=0;x<20;++x) {
            const auto i=std::size_t(y)*20+x;
            layered.pixels()[i]=std::uint8_t((x*11+y*17)%256);
            layered.layer_tags()[i]=std::uint8_t(y<4?PixelLayer::background:
                y<8?PixelLayer::two_d:y<12?PixelLayer::textured_geometry:
                x<7?PixelLayer::three_d:x<14?PixelLayer::world_geometry:
                PixelLayer::terrain_geometry);
            source[i*4]=source[i*4+1]=source[i*4+2]=
                std::uint8_t((y&1)?240:16);
        }
        GpuEffectSettings filter_only;filter_only.filter=unsigned(TwoDFilter::crt);
        auto filtered=source;
        if(!gpu.apply(device.Get(),layered,filtered,filter_only)) return 90;
        bool geometry_changed=false;
        for(unsigned aa=1;aa<=3;++aa) {
            auto combined=source;
            auto settings=filter_only;settings.anti_aliasing=aa;
            if(!gpu.apply(device.Get(),layered,combined,settings)) return 91;
            for(std::size_t i=0;i<layered.pixels().size();++i) {
                const auto layer=PixelLayer(layered.layer_tags()[i]);
                if(!anti_aliasing_eligible(layer)) {
                    if(std::memcmp(filtered.data()+i*4,combined.data()+i*4,4)!=0) {
                        std::cerr<<"AA changed 2D/CRT art at "<<i<<'\n';return 92;
                    }
                } else if(std::memcmp(filtered.data()+i*4,combined.data()+i*4,3)!=0)
                    geometry_changed=true;
            }
        }
        if(!geometry_changed) {std::cerr<<"AA did not affect geometry\n";return 93;}
        std::cout<<"CRT/AA overlap: art unchanged, 3D geometry smoothed\n";
    }
    for(unsigned pattern=0;pattern<3;++pattern) for(unsigned scale:{1U,2U,3U,4U,5U,6U}) {
        Framebuffer art(33,25,scale);art.enable_layer_tags(true);
        for(unsigned y=0;y<art.stored_height();++y) for(unsigned x=0;x<art.stored_width();++x) {
            const auto index=std::size_t(y)*art.stored_width()+x;
            const auto sx=x/scale,sy=y/scale;
            art.pixels()[index]=std::uint8_t(pattern==0?(sx/3+sy/2)*17:
                pattern==1?((sx+sy)/3+(sx>sy?3:7))*23:(sx*747796405U+sy*2891336453U)>>16);
            art.layer_tags()[index]=std::uint8_t(((x/scale)/5+(y/scale)/4)%5);
        }
        for(unsigned filter=1;filter<=4;++filter) {
            std::vector<std::uint8_t> cpu;expand_rgba(art,cpu,palette);auto hardware=cpu;
            apply_two_d_filter(TwoDFilter(filter),art,palette,cpu,filterScratch,workers);
            GpuEffectSettings settings;settings.filter=filter;
            if(!gpu.apply(device.Get(),art,hardware,settings)) return 8;
            unsigned maximum=0;std::size_t different=0;
            for(std::size_t i=0;i<cpu.size();++i) {maximum=std::max(maximum,unsigned(std::abs(int(cpu[i])-int(hardware[i]))));different+=cpu[i]!=hardware[i];}
            std::cout<<"pattern="<<pattern<<" filter="<<filter<<" scale="<<scale<<" differing_bytes="<<different<<" max_delta="<<maximum<<'\n';
            if(maximum>1) return 9;
        }
    }
}
