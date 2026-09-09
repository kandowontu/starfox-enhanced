#include "starfox/render/gpu_effects.hpp"
#include "starfox/render/effects.hpp"
#include "starfox/render/hdr_effect.hpp"
#include "starfox/render/chromatic_aberration.hpp"
#include "starfox/render/model_smoothing.hpp"
#include "starfox/render/bloom.hpp"
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
int main() {
    using namespace starfox::render;
#if defined(STARFOX_TEST_PORTABLE_GPU)
    if(!SDL_Init(SDL_INIT_VIDEO)) return 1;
    struct Device {
        SDL_GPUDevice* value{};
        SDL_GPUDevice* Get() {return value;}
        ~Device(){if(value) SDL_DestroyGPUDevice(value);SDL_Quit();}
    } device{SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_MSL,true,nullptr)};
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
    std::vector<std::uint8_t> source(frame.pixels().size()*4),scratch;
    for(std::size_t i=0;i<frame.pixels().size();++i) {
        frame.layer_tags()[i]=(i/17)%5;
        source[i*4]=(i*13)%256; source[i*4+1]=(i*19)%256;
        source[i*4+2]=(i*7)%256; source[i*4+3]=255;
    }
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
