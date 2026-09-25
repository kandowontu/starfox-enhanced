#include "starfox/render/gpu_composite.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/sdl_gpu_effects.hpp"
#include "starfox/render/colour_math.hpp"
#include "starfox/render/pixel_filter.hpp"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>
namespace {
struct SplitTextures {
    SDL_GPUDevice* device;
    unsigned width,height;
    std::array<SDL_GPUTexture*,3> textures{};
    SDL_GPUTransferBuffer* download{};
    SplitTextures(void* source,unsigned w,unsigned h):device(static_cast<SDL_GPUDevice*>(source)),width(w),height(h) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
        info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
        info.width=w;info.height=h;info.layer_count_or_depth=1;info.num_levels=1;
        for(auto& texture:textures) texture=SDL_CreateGPUTexture(device,&info);
        SDL_GPUTransferBufferCreateInfo transfer{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,w*h*12,0};
        download=SDL_CreateGPUTransferBuffer(device,&transfer);
        if(!download || !textures[0] || !textures[1] || !textures[2]) {
            const std::string error=SDL_GetError();
            for(auto* t:textures) if(t) SDL_ReleaseGPUTexture(device,t);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);
            throw std::runtime_error(error);
        }
    }
    ~SplitTextures() {for(auto* t:textures) SDL_ReleaseGPUTexture(device,t);SDL_ReleaseGPUTransferBuffer(device,download);}
    std::vector<uint8_t> read(unsigned count) {
        auto* command=SDL_AcquireGPUCommandBuffer(device);
        if(!command) throw std::runtime_error(SDL_GetError());
        auto* pass=SDL_BeginGPUCopyPass(command);
        if(!pass) {const std::string error=SDL_GetError();SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(error);}
        for(unsigned i=0;i<count;++i) {
            SDL_GPUTextureRegion region{textures[i],0,0,0,0,0,width,height,1};
            SDL_GPUTextureTransferInfo target{download,i*width*height*4,0,0};
            SDL_DownloadFromGPUTexture(pass,&region,&target);
        }
        SDL_EndGPUCopyPass(pass);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
        if(!fence) throw std::runtime_error(SDL_GetError());
        if(!SDL_WaitForGPUFences(device,true,&fence,1)) {
            const std::string error=SDL_GetError();SDL_ReleaseGPUFence(device,fence);throw std::runtime_error(error);
        }
        SDL_ReleaseGPUFence(device,fence);
        const auto* bytes=static_cast<const uint8_t*>(SDL_MapGPUTransferBuffer(device,download,false));
        if(!bytes) throw std::runtime_error(SDL_GetError());
        std::vector<uint8_t> result(bytes,bytes+std::size_t(width)*height*4*count);
        SDL_UnmapGPUTransferBuffer(device,download);return result;
    }
};
bool reference_subtractive(starfox::render::SdlGpuEffects& gpu,void* device,
    const starfox::render::Framebuffer& ink,std::span<const starfox::render::Rgba8> palette,
    unsigned scale,unsigned filter,unsigned brightness,std::vector<uint8_t>& pixels,bool independent_cpu=false) {
    using namespace starfox::render;
    Framebuffer frame(ink.width(),ink.height(),scale);frame.enable_layer_tags(true);
    std::vector<uint8_t> filtered;
    if(filter) {
        filtered.resize(frame.pixels().size()*4,0);
        for(unsigned y=0;y<ink.height();++y) for(unsigned x=0;x<ink.width();++x) {
            const auto index=ink.get(x,y);if(!index || index>=palette.size()) continue;
            const auto i=(std::size_t(y)*frame.stored_width()+x)*4;
            filtered[i]=palette[index].r;filtered[i+1]=palette[index].g;
            filtered[i+2]=palette[index].b;filtered[i+3]=255;
        }
        GpuEffectSettings settings;settings.filter=filter;settings.overlay_filter=true;
        if(!independent_cpu && !gpu.apply(device,frame,filtered,settings)) return false;
        if(independent_cpu) {
            PixelFilterScratch scratch;RowWorkers workers;
            std::vector<std::uint32_t> independent;
            if(!filter_overlay_layer(TwoDFilter::scalefx,ink,palette,scale,independent,scratch,workers)) return false;
            for(std::size_t i=0;i<independent.size();++i) {
                const auto c=independent[i];
                filtered[i*4]=c>>16;filtered[i*4+1]=c>>8;filtered[i*4+2]=c;filtered[i*4+3]=c>>24;
            }
        }
    }
    for(unsigned y=0;y<frame.stored_height();++y) for(unsigned x=0;x<frame.stored_width();++x) {
        const auto i=(std::size_t(y)*frame.stored_width()+x)*4;
        std::array<unsigned,4> c{};
        if(filter) {
            for(unsigned n=0;n<4;++n) c[n]=filtered[i+n];
            if(!c[3]) continue;
        } else {
            const auto index=ink.get(x/scale,y/scale);if(!index || index>=palette.size()) continue;
            c={palette[index].r,palette[index].g,palette[index].b,palette[index].a};
        }
        for(unsigned n=0;n<3;++n) {
            const auto five=std::max(0,int((c[n]*31+127)/255)-int(30-std::min(brightness,30U)));
            const unsigned rgb=(five<<3)|(five>>2);
            pixels[i+n]=filter?(rgb*c[3]+pixels[i+n]*(255-c[3])+127)/255:rgb;
        }
        if(!filter) pixels[i+3]=c[3];
    }
    return true;
}
void reference_touch(std::vector<uint8_t>& pixels,int w,int h,unsigned scale) {
    const auto box=[&](int l,int t,int r,int b,std::array<unsigned,3> colour) {
        for(int y=t;y<=b;++y) for(int x=l;x<=r;++x) {
            if(x<0 || y<0 || x>=w || y>=h) continue;
            const bool edge=x==l || x==r || y==t || y==b;
            const unsigned alpha=edge?170:76;
            const auto rgb=edge?colour:std::array<unsigned,3>{18,28,42};
            for(unsigned by=0;by<scale;++by) for(unsigned bx=0;bx<scale;++bx) {
                const auto i=((std::size_t(y)*scale+by)*w*scale+x*scale+bx)*4;
                for(unsigned c=0;c<3;++c) pixels[i+c]=(pixels[i+c]*(255-alpha)+rgb[c]*alpha+127)/255;
            }
        }
    };
    const int x=w*20/100,y=h*73/100,u=std::max(7,h/28);
    box(x-u,y-u*3,x+u,y-u,{235,245,255});box(x-u,y+u,x+u,y+u*3,{235,245,255});
    box(x-u*3,y-u,x-u,y+u,{235,245,255});box(x+u,y-u,x+u*3,y+u,{235,245,255});
    box(x-u,y-u,x+u,y+u,{150,180,210});
    const auto action=[&](int px,int py,std::array<unsigned,3> rgb){const int cx=w*px/100,cy=h*py/100;box(cx-u,cy-u,cx+u,cy+u,rgb);};
    action(89,69,{100,235,120});action(77,81,{245,105,105});
    action(77,57,{100,155,255});action(65,69,{250,220,95});
    box(7,7,w*30/100,20,{205,215,230});box(w*70/100,7,w-8,20,{205,215,230});
    box(w*36/100,h-20,w*47/100,h-7,{205,215,230});box(w*53/100,h-20,w*64/100,h-7,{205,215,230});
}
}
int main(int argc,char** argv) {
    using namespace starfox::render;
    if(!SDL_Init(SDL_INIT_VIDEO)) {std::cerr<<"SDL video initialization failed: "<<SDL_GetError()<<'\n';return 1;}
    struct Lifetime {~Lifetime(){SDL_Quit();}} lifetime;
    GpuRaster raster;GpuScene overlays;GpuComposite composite;SdlGpuEffects effects,referenceEffects;
    shadows::PortableShadows residentShadows;
    if(argc==3) {
        std::array<std::optional<Framebuffer>,2> layers;
        Palette256 colours{};unsigned next=1;
        for(unsigned layer=0;layer<2;++layer) {
            auto* source=SDL_LoadBMP(argv[layer+1]);if(!source) return 110;
            auto* image=SDL_ConvertSurface(source,SDL_PIXELFORMAT_RGBA32);SDL_DestroySurface(source);
            if(!image) return 111;
            layers[layer].emplace(image->w,image->h);
            for(int y=0;y<image->h;++y) for(int x=0;x<image->w;++x) {
                const auto* p=static_cast<const uint8_t*>(image->pixels)+y*image->pitch+x*4;
                if(!(p[0]|p[1]|p[2])) continue;
                unsigned index=1;
                while(index<next && (colours[index].r!=p[0] || colours[index].g!=p[1] || colours[index].b!=p[2])) ++index;
                if(index==next) {if(next==256) return 112;colours[next++]={p[0],p[1],p[2],255};}
                layers[layer]->set(x,y,index);
            }
            SDL_DestroySurface(image);
        }
        Framebuffer frame(layers[0]->width(),layers[0]->height(),2);frame.enable_layer_tags(true);
        RasterCommands empty;empty.reset(frame.stored_width(),frame.stored_height());
        if(!raster.render(empty,frame,nullptr)) return 113;
        GpuEffectSettings s;s.filter=5;s.overlay_palette=colours;
        SurfaceBuffer emptySurfaces(frame.stored_width(),frame.stored_height());
        s.lighting=3;s.surfaces=&emptySurfaces;
        for(unsigned i=0;i<2;++i) s.subtractive_overlays[i]=GpuEffectSettings::SubtractiveOverlay{&*layers[i],30};
        std::vector<uint8_t> pixels;expand_rgba(frame,pixels,colours);
        for(unsigned repeat=0;repeat<3;++repeat) {
            expand_rgba(frame,pixels,colours);
            if(!effects.apply(raster.resident_output().device,frame,pixels,s)) return 114;
        }
        auto* image=SDL_CreateSurfaceFrom(frame.stored_width(),frame.stored_height(),SDL_PIXELFORMAT_RGBA32,pixels.data(),frame.stored_width()*4);
        if(!image) return 115;
        const auto output=std::string(argv[1])+"-filtered.bmp";
        const bool saved=SDL_SaveBMP(image,output.c_str());SDL_DestroySurface(image);
        return saved?0:116;
    }
    {
        Framebuffer empty(8,8);SurfaceBuffer normals(8,8);
        std::vector<std::uint8_t> rgba;
        for(unsigned released=0;released<2;++released) {
            if(composite.readback(empty,rgba,&normals) || raster.readback(empty,&normals)
                || overlays.readback(empty,&normals) || residentShadows.readback(rgba)) {
                std::cerr<<"Uninitialized/released GPU readback must be rejected";return 98;
            }
            composite.release_device();raster.release_device();overlays.release_device();residentShadows.release_device();
        }
    }
    shadows::Scene shadowScene;
    shadowScene.add({{-10,-10,40},{10,-10,40},{0,10,45}});shadowScene.build();
    Palette256 palette;
    for(unsigned i=0;i<256;++i) palette[i]={std::uint8_t(i),std::uint8_t(i*7),std::uint8_t(255-i),std::uint8_t(i*13)};
    {
        Framebuffer base(400,224,2),portrait(400,224),text(400,224),deviceFrame(1,1);
        base.enable_layer_tags(true);
        RasterCommands empty;empty.reset(1,1);
        if(!raster.render(empty,deviceFrame,nullptr)) {std::cerr<<"Composition fixture device setup failed: "<<raster.status()<<'\n';return 107;}
        Palette256 colours{};for(auto& c:colours) c.a=255;
        // Exactly representable channels avoid UNORM/float edge-choice noise
        // and give an independent CPU oracle for separated transparent layers.
        for(unsigned i=1;i<9;++i) colours[i]={uint8_t(i&1?255:0),uint8_t(i&2?255:0),uint8_t(i&4?255:0),255};
        for(unsigned y=80;y<160;++y) for(unsigned x=88;x<136;++x) portrait.set(x,y,1+(x*3+y*7)%8);
        for(unsigned y=39;y<51;++y) for(unsigned x=120;x<260;++x) if((x+y)%3) text.set(x,y,2);
        GpuEffectSettings s;s.filter=5;s.overlay_palette=colours;
        s.subtractive_overlays[0]=GpuEffectSettings::SubtractiveOverlay{&portrait,30};
        s.subtractive_overlays[1]=GpuEffectSettings::SubtractiveOverlay{&text,30};
        SurfaceBuffer normals(base.stored_width(),base.stored_height());
        for(unsigned y=0;y<base.stored_height();++y) for(unsigned x=0;x<base.stored_width();++x)
            normals.set(x,y,{-1,0,0,100},0);
        s.lighting=3;s.surfaces=&normals;
        std::vector<std::uint8_t> pixels;expand_rgba(base,pixels,colours);
        auto independent=pixels;
        for(const auto& layer:s.subtractive_overlays)
            if(!reference_subtractive(referenceEffects,raster.resident_output().device,*layer->frame,colours,2,5,30,independent,true)) return 117;
        for(unsigned repeat=0;repeat<6;++repeat) {
            expand_rgba(base,pixels,colours);
            if(!effects.apply(raster.resident_output().device,base,pixels,s)) return 108;
            if(pixels!=independent) {
                std::cerr<<"ScaleFX separated overlays differ from independent CPU oracle on repetition "<<repeat;return 109;
            }
        }
    }
    {
        // A real resident frame can carry a uniform CPU backing layer: all
        // color-zero pixels with the background tag. Repeated frames must
        // retain that GPU buffer without another full-image upload, then
        // invalidate it after one ordinary foreground write.
        GpuComposite cached;
        Framebuffer blank(31,25),read(31,25);
        blank.enable_layer_tags(true);
        std::fill(blank.layer_tags().begin(),blank.layer_tags().end(),
            static_cast<std::uint8_t>(PixelLayer::background));
        LayerCompositeSettings plain;
        std::vector<std::uint8_t> first,second,changed,restored;
        const auto image_bytes=blank.pixels().size()*4U;
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,palette)
            || cached.last_cpu_upload_bytes()!=0U
            || cached.last_palette_upload_bytes()!=1024U
            || !cached.readback(read,first)) return 118;
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,palette)
            || cached.last_cpu_upload_bytes()!=0U
            || cached.last_palette_upload_bytes()!=0U
            || !cached.readback(read,second) || second!=first) return 119;
        blank.set_stored(7,9,53,PixelLayer::two_d);
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,palette)
            || cached.last_cpu_upload_bytes()!=image_bytes
            || !cached.readback(read,changed) || changed==first) return 120;
        blank.set_stored(7,9,0,PixelLayer::background);
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,palette)
            || cached.last_cpu_upload_bytes()!=0U
            || !cached.readback(read,restored) || restored!=first) return 121;
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,palette)
            || cached.last_cpu_upload_bytes()!=0U) return 122;
        std::vector<std::uint8_t> covered(blank.pixels().size());
        covered[5]=1;
        if(!cached.compose(raster.resident_output(),1,blank,covered,plain,palette)
            || cached.last_cpu_upload_bytes()!=image_bytes) return 123;
        covered[5]=0;
        if(!cached.compose(raster.resident_output(),1,blank,covered,plain,palette)
            || cached.last_cpu_upload_bytes()!=0U) return 124;
        if(!cached.compose(raster.resident_output(),1,blank,covered,plain,palette)
            || cached.last_cpu_upload_bytes()!=0U) return 125;
        blank.layer_tags()[5]=static_cast<std::uint8_t>(PixelLayer::two_d);
        if(!cached.compose(raster.resident_output(),1,blank,covered,plain,palette)
            || cached.last_cpu_upload_bytes()!=image_bytes) return 126;
        blank.layer_tags()[5]=static_cast<std::uint8_t>(PixelLayer::background);
        if(!cached.compose(raster.resident_output(),1,blank,covered,plain,palette)
            || cached.last_cpu_upload_bytes()!=0U) return 127;
        std::fill(blank.pixels().begin(),blank.pixels().end(),53);
        std::fill(blank.layer_tags().begin(),blank.layer_tags().end(),
            static_cast<std::uint8_t>(PixelLayer::two_d));
        auto altered_palette=palette;
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,altered_palette)
            || cached.last_cpu_upload_bytes()!=0U
            || cached.last_palette_upload_bytes()!=0U
            || !cached.readback(read,first)) return 128;
        altered_palette[53]={255,17,82,255};
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,altered_palette)
            || cached.last_cpu_upload_bytes()!=0U
            || cached.last_palette_upload_bytes()!=1024U
            || !cached.readback(read,second) || second==first) return 129;
        blank.set_stored(1,1,52,PixelLayer::two_d);
        blank.set_stored(29,23,54,PixelLayer::two_d);
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,altered_palette)
            || cached.last_cpu_upload_bytes()!=image_bytes
            || cached.last_palette_upload_bytes()!=0U
            || !cached.readback(read,changed) || changed==second) return 130;
        blank.set_stored(1,1,51,PixelLayer::two_d);
        blank.set_stored(29,23,55,PixelLayer::two_d);
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,altered_palette)
            || cached.last_cpu_upload_bytes()!=8U
            || !cached.readback(read,restored) || restored==changed) return 131;
        std::vector<std::uint8_t> after_late(blank.pixels().size());
        after_late[27]=1;
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,altered_palette,
            nullptr,nullptr,after_late) || cached.last_cpu_upload_bytes()!=4U) return 132;
        after_late[27]=0;
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,altered_palette,
            nullptr,nullptr,after_late) || cached.last_cpu_upload_bytes()!=4U) return 133;
        blank.set_stored(29,1,52,PixelLayer::two_d);
        blank.set_stored(1,2,54,PixelLayer::two_d);
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,altered_palette)
            || cached.last_cpu_upload_bytes()!=16U) return 134;
        blank.set_stored(29,1,51,PixelLayer::two_d);
        blank.set_stored(1,2,55,PixelLayer::two_d);
        if(!cached.compose(raster.resident_output(),1,blank,{},plain,altered_palette)
            || cached.last_cpu_upload_bytes()!=16U) return 135;
        GpuComposite fragmented;
        Framebuffer tall(31,160);tall.enable_layer_tags(true);
        std::fill(tall.layer_tags().begin(),tall.layer_tags().end(),
            static_cast<std::uint8_t>(PixelLayer::background));
        if(!fragmented.compose(raster.resident_output(),1,tall,{},plain,palette)
            || fragmented.last_cpu_upload_bytes()!=0U) return 136;
        for(unsigned row=0;row<140;row+=2) tall.set_stored(0,row,53,PixelLayer::two_d);
        if(!fragmented.compose(raster.resident_output(),1,tall,{},plain,palette)
            || fragmented.last_cpu_upload_bytes()!=tall.pixels().size()*4U) return 137;

        // Full-height cartridge side strips have a compact GPU constant
        // representation; one changed row must return to the packed upload.
        Framebuffer striped(31,25),stripeRead(31,25);
        striped.enable_layer_tags(true);
        std::vector<std::uint8_t> stripeCoverage(striped.pixels().size());
        for(unsigned y=0;y<striped.stored_height();++y)
            for(unsigned x:{2U,3U,4U,25U,26U,27U}) {
                striped.set_stored(x,y,53,PixelLayer::background);
                stripeCoverage[std::size_t(y)*striped.stored_width()+x]=1;
            }
        GpuComposite stripeComposite;
        std::vector<std::uint8_t> stripeImage,stripeRepeat,stripeChanged,stripeRestored;
        if(!stripeComposite.compose(raster.resident_output(),1,striped,stripeCoverage,plain,palette)
            || stripeComposite.last_cpu_upload_bytes()!=0U
            || !stripeComposite.readback(stripeRead,stripeImage)) return 138;
        if(!stripeComposite.compose(raster.resident_output(),1,striped,stripeCoverage,plain,palette)
            || stripeComposite.last_cpu_upload_bytes()!=0U
            || !stripeComposite.readback(stripeRead,stripeRepeat)
            || stripeRepeat!=stripeImage) return 139;
        striped.set_stored(3,4,54,PixelLayer::background);
        if(!stripeComposite.compose(raster.resident_output(),1,striped,stripeCoverage,plain,palette)
            || stripeComposite.last_cpu_upload_bytes()!=striped.pixels().size()*4U
            || !stripeComposite.readback(stripeRead,stripeChanged)
            || stripeChanged==stripeImage) return 140;
        striped.set_stored(3,4,53,PixelLayer::background);
        if(!stripeComposite.compose(raster.resident_output(),1,striped,stripeCoverage,plain,palette)
            || stripeComposite.last_cpu_upload_bytes()!=0U
            || !stripeComposite.readback(stripeRead,stripeRestored)
            || stripeRestored!=stripeImage) return 141;
        const auto* previousCache=std::getenv("STARFOX_DISABLE_CPU_UPLOAD_CACHE");
        const bool hadPreviousCache=previousCache!=nullptr;
        const std::string previousCacheValue=hadPreviousCache?previousCache:"";
#if defined(_WIN32)
        if(_putenv_s("STARFOX_DISABLE_CPU_UPLOAD_CACHE","1")!=0) return 142;
#else
        if(setenv("STARFOX_DISABLE_CPU_UPLOAD_CACHE","1",1)!=0) return 142;
#endif
        GpuComposite stripeReference;
        std::vector<std::uint8_t> referenceImage;
        const bool referenceReady=stripeReference.compose(raster.resident_output(),1,striped,stripeCoverage,plain,palette)
            && stripeReference.last_cpu_upload_bytes()==striped.pixels().size()*4U
            && stripeReference.readback(stripeRead,referenceImage);
#if defined(_WIN32)
        _putenv_s("STARFOX_DISABLE_CPU_UPLOAD_CACHE",hadPreviousCache?previousCacheValue.c_str():"");
#else
        if(hadPreviousCache) setenv("STARFOX_DISABLE_CPU_UPLOAD_CACHE",previousCacheValue.c_str(),1);
        else unsetenv("STARFOX_DISABLE_CPU_UPLOAD_CACHE");
#endif
        if(!referenceReady || referenceImage!=stripeImage) {
            std::cerr<<"Stripe reference mismatch: ready="<<referenceReady
                <<" bytes="<<stripeReference.last_cpu_upload_bytes()
                <<" status="<<stripeReference.status()
                <<" image="<<referenceImage.size()<<'/'<<stripeImage.size()<<'\n';
            return 143;
        }
    }
    unsigned cases=0;
    for(unsigned sourceScale:{1U,2U,4U}) for(unsigned scale:{1U,2U,4U})
    for(int offset:{-7,0,3}) for(unsigned mosaic:{0U,0x31U}) for(bool clipped:{false,true}) {
        Framebuffer native(23,17,sourceScale),cpu(31,25,scale),expected(31,25,scale),actual(31,25,scale);
        for(auto* f:{&native,&cpu,&expected,&actual}) f->enable_layer_tags(true);
        RasterCommands commands;commands.reset(native.stored_width(),native.stored_height());
        for(unsigned y=0;y<native.stored_height();++y) for(unsigned x=0;x<native.stored_width();++x) {
            RasterCommand c;c.left=x;c.right=x+1;c.top=y;c.bottom=y+1;
            c.even=c.odd=(x+y)%7?((x*3+y*5)%254)+1:0;c.tag=(x+y)%5;
            c.has_surface=(x+y)%3!=0;c.surface[0]=0.3F;c.surface[1]=0.4F;c.surface[2]=0.5F;c.surface[3]=120.F;
            commands.add(c);
        }
        SurfaceBuffer nativeSurface(native.stored_width(),native.stored_height());
        if(!raster.render(commands,native,&nativeSurface)) {std::cerr<<raster.status();return 2;}
        for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x)
            cpu.set_stored(x,y,std::uint8_t((x*17+y)%256),PixelLayer::background);
        expected=cpu;
        LayerCompositeSettings settings;settings.offset_x=offset;settings.offset_y=offset+2;
        settings.mosaic=mosaic;settings.mosaic_layer_mask=1;settings.mosaic_origin_x=-3;settings.mosaic_origin_y=2;
        if(clipped) {settings.clip_left=2;settings.clip_top=3;settings.clip_right=21;settings.clip_bottom=20;}
        composite_transparent_layer(native,expected,settings);
        std::vector<std::uint8_t> coverage(cpu.pixels().size());
        cpu.begin_write_coverage();
        for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) if((x+y)%11==0) {
            const auto i=std::size_t(y)*cpu.stored_width()+x;
            // Includes black and unchanged background writes that comparison-
            // based foreground detection would miss.
            const int native_y=int(y)-(offset+2)*int(scale),native_x=int(x)-offset*int(scale);
            const auto colour=sourceScale==scale && native_x>=0 && native_y>=0
                && native_x<int(native.stored_width()) && native_y<int(native.stored_height())
                ? native.get_stored(native_x,native_y) // Explicit same-palette HUD/model collision.
                : (x%2)?cpu.pixels()[i]:0;
            coverage[i]=1;cpu.set_stored(x,y,colour,PixelLayer::two_d);expected.set_stored(x,y,colour,PixelLayer::two_d);
        }
        if(!std::equal(coverage.begin(),coverage.end(),cpu.write_coverage().begin())) return 9;
        cpu.end_write_coverage();
        if(!composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette)) {
            std::cerr<<composite.status();return 3;
        }
        std::vector<std::uint8_t> rgba,reference;
        SurfaceBuffer surface(cpu.stored_width(),cpu.stored_height());
        if(!composite.readback(actual,rgba,&surface)) {std::cerr<<composite.status();return 4;}
        for(unsigned y=0;y<actual.stored_height();++y) for(unsigned x=0;x<actual.stored_width();++x)
            if(coverage[std::size_t(y)*actual.stored_width()+x] && surface.get(x,y).valid) {
                std::cerr<<"CPU foreground inherited hidden model lighting";return 99;
            }
        if(offset==0 && mosaic==0 && !clipped) {
            Framebuffer base(23,17,sourceScale),world(23,17,sourceScale);base.enable_layer_tags(true);world.enable_layer_tags(true);
            for(unsigned y=0;y<base.stored_height();++y) for(unsigned x=0;x<base.stored_width();++x)
                base.set_stored(x,y,x?9:7,x?PixelLayer::background:PixelLayer::two_d);
            GpuComposite separate;LayerCompositeSettings plain;
            if(!separate.compose(raster.resident_output(),sourceScale,base,{},plain,palette,nullptr,nullptr,{},true)) {std::cerr<<separate.status();return 51;}
            std::vector<std::uint8_t> world_rgba;
            if(!separate.readback(world,world_rgba)) return 52;
            for(unsigned y=0;y<base.stored_height();++y) for(unsigned x=0;x<base.stored_width();++x) {
                const auto i=std::size_t(y)*base.stored_width()+x;
                const auto wanted=native.pixels()[i] && native.layer_tags()[i]!=1?native.pixels()[i]:x?9:0;
                if(world.pixels()[i]!=wanted) {std::cerr<<"World-only native/CPU HUD exclusion mismatch";return 53;}
            }
        }
        expand_rgba(expected,reference,palette);
        if(expected.pixels()!=actual.pixels() || expected.layer_tags()!=actual.layer_tags() || reference!=rgba) {
            std::cerr<<"Composition mismatch sourceScale="<<sourceScale<<" scale="<<scale<<" offset="<<offset<<" mosaic="<<mosaic<<" clip="<<clipped;return 5;
        }
        // Independent output sampling keeps authored clipping, mosaic, layer
        // offsets and covered-black HUD writes in the original canvas.
        if(offset==3) {
            constexpr unsigned rw=19,rh=13;
            GpuComposite reduced;Framebuffer reducedFrame(rw,rh);reducedFrame.enable_layer_tags(true);
            if(!reduced.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,
                nullptr,nullptr,{},false,{native.stored_width(),native.stored_height(),rw,rh})) {
                std::cerr<<reduced.status();return 90;
            }
            std::vector<std::uint8_t> reducedRgba;
            if(!reduced.readback(reducedFrame,reducedRgba)) return 91;
            for(unsigned y=0;y<rh;++y) for(unsigned x=0;x<rw;++x) {
                const auto from=(y*cpu.stored_height()/rh)*cpu.stored_width()+x*cpu.stored_width()/rw;
                const auto to=y*rw+x;
                if(reducedFrame.pixels()[to]!=actual.pixels()[from]
                    || reducedFrame.layer_tags()[to]!=actual.layer_tags()[from]) return 92;
                for(unsigned c=0;c<4;++c) if(reducedRgba[to*4+c]!=rgba[from*4+c]) return 93;
            }
            if(scale==1 && sourceScale==1 && !mosaic && !clipped) {
                RasterCommands layerCommands;layerCommands.reset(rw,rh);
                for(unsigned y=0;y<rh;++y) for(unsigned x=0;x<rw;++x) {
                    RasterCommand c;c.left=x;c.right=x+1;c.top=y;c.bottom=y+1;
                    c.even=c.odd=32+(x+y*7)%127;c.tag=unsigned(PixelLayer::background);layerCommands.add(c);
                }
                GpuScene layerScene;const std::array<GpuSceneDraw,1> layerDraws{GpuRasterDraw{&layerCommands}};
                if(!layerScene.render_resident(raster.resident_output().device,rw,rh,layerDraws)) return 94;
                const auto layerOutput=layerScene.resident_output();
                // Native-size composition must preserve every native sample,
                // even when its authored reference canvas is larger.
                LayerCompositeSettings nativeOnly;
                if(!reduced.compose(layerOutput,1,cpu,{},nativeOnly,palette,
                    nullptr,nullptr,{},false,{cpu.stored_width(),cpu.stored_height(),rw,rh})
                    || !reduced.readback(reducedFrame,reducedRgba)) return 97;
                for(unsigned y=0;y<rh;++y) for(unsigned x=0;x<rw;++x)
                    if(reducedFrame.pixels()[y*rw+x]!=32+(x+y*7)%127) {
                        std::cerr<<"Native reference double-rounding at "<<x<<","<<y;return 98;
                    }
                const GpuCompositeBackground bg{layerOutput};
                LayerCompositeSettings noNative;noNative.clip_left=noNative.clip_right=0;
                // Check both native-size consumption and enlargement back to
                // the reference canvas (the non-DLSS fallback path). Neither
                // may double-round through the CPU reference coordinates.
                for(const auto extent:std::array<std::array<unsigned,2>,3>{{{rw,rh},{31,25},{37,29}}})
                for(bool lateLayer:{false,true}) {
                    Framebuffer layerFrame(extent[0],extent[1]);layerFrame.enable_layer_tags(true);
                    if(!reduced.compose(raster.resident_output(),1,cpu,{},noNative,palette,
                        lateLayer?&layerOutput:nullptr,lateLayer?nullptr:&bg,{},false,
                        {native.stored_width(),native.stored_height(),extent[0],extent[1]})
                        || !reduced.readback(layerFrame,reducedRgba)) return 95;
                    for(unsigned y=0;y<extent[1];++y) for(unsigned x=0;x<extent[0];++x)
                        if(layerFrame.pixels()[y*extent[0]+x]
                            !=32+((x*rw/extent[0])+(y*rh/extent[1])*7)%127) {
                            std::cerr<<"Reduced layer coordinate mismatch, late="<<lateLayer
                                <<" output="<<extent[0]<<"x"<<extent[1];return 96;
                        }
                }
            }
        }
        {
            RasterCommands bg;bg.reset(cpu.stored_width(),cpu.stored_height());
            auto bgExpected=cpu;
            std::vector<std::uint8_t> early(cpu.pixels().size());
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                const auto i=std::size_t(y)*cpu.stored_width()+x;
                early[i]=(x+y)%5==0;
                if((x+y)%3==0) continue;
                RasterCommand c;c.left=x;c.right=x+1;c.top=y;c.bottom=y+1;
                c.even=c.odd=x%2?91:0;c.tag=unsigned(PixelLayer::background);bg.add(c);
                if(!early[i]) bgExpected.set_stored(x,y,std::uint8_t(c.even),PixelLayer::background);
            }
            composite_transparent_layer(native,bgExpected,settings);
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                const auto i=std::size_t(y)*cpu.stored_width()+x;
                if(coverage[i]) bgExpected.set_stored(x,y,cpu.pixels()[i],PixelLayer::two_d);
            }
            const std::array<GpuSceneDraw,1> draws{GpuRasterDraw{&bg,false,false}};
            if(!overlays.render_resident(raster.resident_output().device,cpu.stored_width(),cpu.stored_height(),draws)) return 69;
            GpuCompositeBackground background{overlays.resident_output(),early};
            if(!composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,nullptr,&background)
                || !composite.readback(actual,rgba,&surface)) return 70;
            expand_rgba(bgExpected,reference,palette);
            if(actual.pixels()!=bgExpected.pixels() || actual.layer_tags()!=bgExpected.layer_tags() || rgba!=reference) {
                std::cerr<<"Resident background/early sprite/model/foreground ordering mismatch "<<cases;return 71;
            }
            {
                auto repairCpu=cpu;
                Framebuffer repaired(31,25,scale);repaired.enable_layer_tags(true);repaired.clear();
                for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                    const auto i=std::size_t(y)*cpu.stored_width()+x;
                    if(!early[i] && !coverage[i]) repairCpu.set_stored(x,y,0,PixelLayer::three_d);
                    if((x+y)%3) repaired.set_stored(x,y,x%2?91:0,PixelLayer::background);
                }
                const auto backdrop=repaired.get(5,0);
                for(unsigned y=0;y<repaired.height();++y) for(unsigned x=0;x<repaired.width();++x)
                    if((x<5 || x>=24) && !repaired.get(x,y)) repaired.set(x,y,backdrop);
                for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                    const auto i=std::size_t(y)*cpu.stored_width()+x;
                    if(early[i]) repaired.set_stored(x,y,cpu.pixels()[i],PixelLayer(cpu.layer_tags()[i]));
                }
                composite_transparent_layer(native,repaired,settings);
                for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                    const auto i=std::size_t(y)*cpu.stored_width()+x;
                    if(coverage[i]) repaired.set_stored(x,y,cpu.pixels()[i],PixelLayer(cpu.layer_tags()[i]));
                }
                background.margin_origin=5;background.margin_width=19;background.repair_transparent_margins=true;
                if(!composite.compose(raster.resident_output(),sourceScale,repairCpu,coverage,settings,palette,nullptr,&background)
                    || !composite.readback(actual,rgba,&surface)) return 82;
                expand_rgba(repaired,reference,palette);
                if(actual.pixels()!=repaired.pixels() || actual.layer_tags()!=repaired.layer_tags() || rgba!=reference) {
                    std::cerr<<"GPU transparent margin repair mismatch "<<cases;return 83;
                }
                background.repair_transparent_margins=false;
            }
            background.margin_origin=4;background.margin_width=19;
            auto marginExpected=bgExpected;
            const auto edge=[&](unsigned x) {
                std::array<unsigned,256> counts{};
                for(unsigned y=0;y<marginExpected.height();++y) ++counts[marginExpected.get(x,y)];
                return std::uint8_t(std::distance(counts.begin(),std::max_element(counts.begin(),counts.end())));
            };
            const auto left=edge(4),right=edge(22);
            for(unsigned y=0;y<marginExpected.height();++y) for(unsigned x=0;x<marginExpected.width();++x)
                if(x<4 || x>=23) marginExpected.set(x,y,x<4?left:right);
            if(!composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,nullptr,&background)
                || !composite.readback(actual,rgba,&surface)) return 77;
            expand_rgba(marginExpected,reference,palette);
            if(actual.pixels()!=marginExpected.pixels() || actual.layer_tags()!=marginExpected.layer_tags() || rgba!=reference) {
                std::cerr<<"GPU edge reduction mismatch "<<cases;return 78;
            }
            // Late star/particle coverage must remain above solid margins.
            auto lateMarginExpected=marginExpected;
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x)
                if((x+y)%3) lateMarginExpected.set_stored(x,y,x%2?91:0,PixelLayer::background);
            if(!composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,&background.raster,&background)
                || !composite.readback(actual,rgba,&surface)) return 79;
            expand_rgba(lateMarginExpected,reference,palette);
            if(actual.pixels()!=lateMarginExpected.pixels() || rgba!=reference) return 80;
            background.margin_width=UINT32_MAX;
            if(composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,nullptr,&background)) return 81;
            background.margin_origin=0;background.margin_width=256;
            // Bad coverage and output aliases must decline without exposing a
            // previous successful frame as current output.
            background.cpu_coverage=std::span<const std::uint8_t>(early).first(1);
            if(composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,nullptr,&background)
                || composite.output().rgba) return 72;
            background.cpu_coverage=early;
            if(!composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,nullptr,&background)) return 73;
            background.raster.pixels=composite.output().packed;
            if(composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,nullptr,&background)
                || composite.output().rgba) return 74;
            if(!composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette)
                || !composite.readback(actual,rgba,&surface)) return 75;
            expand_rgba(expected,reference,palette);
            if(actual.pixels()!=expected.pixels() || rgba!=reference) return 76;
        }
        {
            RasterCommands late;late.reset(cpu.stored_width(),cpu.stored_height());
            auto lateExpected=expected;
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) if((x+y)%13==0) {
                RasterCommand c;c.left=x;c.right=x+1;c.top=y;c.bottom=y+1;
                c.even=c.odd=x%3?127:0;c.tag=static_cast<unsigned>(PixelLayer::world_geometry);late.add(c);
                lateExpected.set_stored(x,y,std::uint8_t(c.even),PixelLayer::world_geometry);
            }
            const std::array<GpuSceneDraw,1> draws{GpuRasterDraw{&late,false,false}};
            if(!overlays.render_resident(raster.resident_output().device,cpu.stored_width(),cpu.stored_height(),draws)) return 60;
            auto output=overlays.resident_output();
            if(!composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,&output)
                || !composite.readback(actual,rgba,&surface)) return 61;
            expand_rgba(lateExpected,reference,palette);
            if(actual.pixels()!=lateExpected.pixels() || actual.layer_tags()!=lateExpected.layer_tags() || rgba!=reference) {
                std::cerr<<"Late overlay colour/coverage mismatch "<<cases;return 62;
            }
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x)
                if((x+y)%13==0 && surface.get(x,y).valid) return 63;
            std::vector<std::uint8_t> afterLate(cpu.pixels().size());
            auto afterExpected=lateExpected;
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                const auto i=std::size_t(y)*cpu.stored_width()+x;
                if((x+y)%5==0) {
                    afterLate[i]=1;afterExpected.set_stored(x,y,cpu.pixels()[i],PixelLayer(cpu.layer_tags()[i]));
                }
            }
            if(!composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,&output,nullptr,afterLate)
                || !composite.readback(actual,rgba,&surface)) return 84;
            expand_rgba(afterExpected,reference,palette);
            if(actual.pixels()!=afterExpected.pixels() || actual.layer_tags()!=afterExpected.layer_tags() || rgba!=reference) return 85;
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x)
                if(afterLate[std::size_t(y)*cpu.stored_width()+x] && surface.get(x,y).valid) return 86;
            if(composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,&output,nullptr,
                std::span<const std::uint8_t>(afterLate).first(1)) || composite.output().rgba) return 87;
            ++output.width;
            if(composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,&output)
                || composite.output().rgba) return 64;
            // A missing next-frame overlay must not reuse its pixels/metadata.
            if(!composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette)
                || !composite.readback(actual,rgba,&surface)) return 65;
            expand_rgba(expected,reference,palette);
            if(actual.pixels()!=expected.pixels() || rgba!=reference) return 66;
            output.width=cpu.stored_width();output.pixels=composite.output().packed;
            if(composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette,&output)
                || composite.output().rgba) return 67;
            if(!composite.compose(raster.resident_output(),sourceScale,cpu,coverage,settings,palette)) return 68;
        }
        GpuEffectSettings s;s.surfaces=&surface;s.hdr=1;s.chromatic=2;s.lighting=2;
        s.model_effect=1;s.world_effect=2;s.bloom_model=1;s.bloom_world=2;s.anti_aliasing=1;
        if(!referenceEffects.apply(raster.resident_output().device,actual,reference,s)) {std::cerr<<referenceEffects.status();return 6;}
        std::vector<std::uint8_t> result;
        if(!effects.apply_resident(composite.output(),cpu,result,s)) {std::cerr<<effects.status();return 7;}
        if(result!=reference) {std::cerr<<"Resident effects mismatch case "<<cases;return 8;}
        if(effects.last_staging_upload_bytes()!=0) {std::cerr<<"Resident effects uploaded placeholder data";return 53;}
        {
            SdlGpuEffects backdropEffects;
            BackdropImage sky;sky.width=16;sky.height=16;sky.pixels.assign(256,0xff553311);
            GpuEffectSettings skySettings;auto& env=skySettings.environment;
            env.backdrop=&sky;env.modes[2]=1;env.motion[0]=1000;env.plane[3]=1;
            for(unsigned iteration=0;iteration<3;++iteration) {
                if(iteration==2) std::fill(sky.pixels.begin(),sky.pixels.end(),0xff224466);
                auto wanted=rgba;apply_environment(env,actual,wanted);
                if(!backdropEffects.apply_resident(composite.output(),cpu,result,skySettings)
                    || result!=wanted) {std::cerr<<"Resident backdrop cache pixels mismatch";return 110;}
                if(backdropEffects.last_staging_upload_bytes()!=(iteration==1?0u:1024u)) {
                    std::cerr<<"Resident backdrop reuploaded unchanged image";return 111;
                }
            }
        }
        {
            SplitTextures split(raster.resident_output().device,cpu.stored_width(),cpu.stored_height());
            for(bool bloom:{false,true}) {
                auto separated=bloom?s:GpuEffectSettings{};separated.surfaces=&surface;
                separated.presentation_texture=split.textures[0];
                separated.presentation_model_texture=split.textures[1];
                if(bloom) separated.presentation_glow_texture=split.textures[2];
                const std::vector<uint8_t> sentinel{1,2,3};result=sentinel;
                if(!effects.apply_resident(composite.output(),cpu,result,separated) || result!=sentinel) return 42;
                const auto resident=split.read(bloom?3:2);
                auto uploaded=rgba;
                if(!referenceEffects.apply(raster.resident_output().device,actual,uploaded,separated) || uploaded!=rgba) return 43;
                if(split.read(bloom?3:2)!=resident) {std::cerr<<"Resident model/glow split mismatch "<<cases;return 44;}
                if(!bloom) {
                    auto expected_base=rgba;std::vector<uint8_t> expected_model(rgba.size(),0);
                    const auto owns=[&](unsigned x,unsigned y) {
                        const auto& sample=surface.get(x,y);
                        return sample.valid && sample.palette_index==actual.get_stored(x,y);
                    };
                    const std::array<std::array<int,2>,8> neighbours{{{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}}};
                    for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) if(owns(x,y)) {
                        const auto i=(std::size_t(y)*cpu.stored_width()+x)*4;
                        for(unsigned c=0;c<3;++c) expected_model[i+c]=rgba[i+c];expected_model[i+3]=255;
                        for(const auto& delta:neighbours) {
                            const int nx=int(x)+delta[0],ny=int(y)+delta[1];
                            if(nx<0 || ny<0 || nx>=int(cpu.stored_width()) || ny>=int(cpu.stored_height())) continue;
                            if(!owns(nx,ny)) {
                                const auto j=(std::size_t(ny)*cpu.stored_width()+nx)*4;
                                for(unsigned c=0;c<4;++c) expected_base[i+c]=rgba[j+c];break;
                            }
                        }
                    }
                    expected_base.insert(expected_base.end(),expected_model.begin(),expected_model.end());
                    if(resident!=expected_base) {std::cerr<<"Independent model split mismatch "<<cases;return 45;}
                }
            }
        }
        // The combined path must shadow before smoothing/styles/bloom/AA,
        // matching the game's existing early and late effect batches.
        std::vector<std::uint8_t> shadow(cpu.pixels().size());
        for(std::size_t i=0;i<shadow.size();++i) shadow[i]=std::uint8_t(i%81);
        GpuEffectSettings early;
        early.filter=cases%5;early.surfaces=&surface;early.lighting=s.lighting;
        early.hdr=s.hdr;early.chromatic=s.chromatic;early.shadow_mask=shadow;
        early.shadow_width=cpu.stored_width();early.shadow_height=cpu.stored_height();
        expand_rgba(expected,reference,palette);
        if(!referenceEffects.apply(raster.resident_output().device,actual,reference,early)) return 10;
        auto late=s;late.hdr=late.chromatic=late.lighting=0;
        if(!referenceEffects.apply(raster.resident_output().device,actual,reference,late)) return 11;
        s.filter=early.filter;s.shadow_mask=shadow;s.shadow_width=early.shadow_width;
        s.shadow_height=early.shadow_height;s.shadow_before_style=true;
        if(!effects.apply_resident(composite.output(),cpu,result,s) || result!=reference) {
            std::cerr<<"Early shadow/filter order mismatch case "<<cases;return 12;
        }
        const shadows::Camera camera{cpu.stored_width(),cpu.stored_height(),50,
            cpu.stored_width()/2.0,cpu.stored_height()/2.0};
        if(!residentShadows.render_resident(raster.resident_output().device,shadowScene,camera,{-1,-1,-1},
            shadows::ReceiverPlane{{0,15,0},{0,1,0}}) || !residentShadows.readback(shadow)) return 13;
        s.shadow_mask=shadow;
        if(!effects.apply_resident(composite.output(),cpu,reference,s)) return 14;
        s.shadow_mask={};s.resident_shadow=residentShadows.output();
        if(!effects.apply_resident(composite.output(),cpu,result,s) || result!=reference) {
            std::cerr<<"Resident shadow composition mismatch case "<<cases<<": "<<effects.status();return 15;
        }
        for(bool expanded:{false,true}) for(bool closed:{false,true}) {
            GpuEffectSettings shutter;
            shutter.horizontal_wipe=GpuEffectSettings::HorizontalWipe{
                int(2*scale),int(23*scale),int(8*scale),int((closed?8:16)*scale),
                int(2*scale),-5,expanded};
            expand_rgba(expected,reference,palette);
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                if(y<2*scale || y>=23*scale) continue;
                const bool outside=y<8*scale || y>=(closed?8U:16U)*scale;
                const int sx=int(x/scale)+5;
                const bool guard=expanded?x<2*scale:((!(sx>=15 && sx<=16))!=(sx>=16 && sx<=240));
                if(outside || guard) {
                    const auto i=(std::size_t(y)*cpu.stored_width()+x)*4;
                    reference[i]=reference[i+1]=reference[i+2]=0;reference[i+3]=255;
                }
            }
            if(!effects.apply_resident(composite.output(),cpu,result,shutter) || result!=reference) {
                std::cerr<<"Resident shutter mismatch case "<<cases;return 16;
            }
        }
        for(unsigned circle_shape=0;circle_shape<4;++circle_shape)
        for(unsigned flags=0;flags<8;++flags) {
            GpuEffectSettings disk;
            GpuEffectSettings::Circle c{int(10*scale),int(8*scale),int(17*scale),
                int(2*scale),int(3*scale),int(29*scale),int(24*scale),
                31,9,0,bool(flags&1),bool(flags&2),bool(flags&4)};
            if(circle_shape==1) {c.radius=65535*int(scale);c.x=int(10*scale)-c.radius;}
            // Exact 3/4/5 tangent plus neighboring pixels exercises low-limb
            // carries and inclusivity, rather than an entirely filled screen.
            if(circle_shape==2) {c.radius=65535;c.x=int(10*scale)-39321;c.y=int(8*scale)-52428;}
            if(circle_shape==3) {c.radius=1048000;c.x=int(10*scale)-628800;c.y=int(8*scale)-838400;}
            disk.circle=c;
            expand_rgba(expected,reference,palette);
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                const auto dx=std::int64_t(x)-c.x,dy=std::int64_t(y)-c.y;
                if(int(x)<c.left || int(x)>=c.right || int(y)<c.top || int(y)>=c.bottom
                    || dx*dx+dy*dy>std::int64_t(c.radius)*c.radius
                    || (!c.affect_sprites && expected.get_stored(x,y)>=128)) continue;
                const unsigned fixed[]{c.red,c.green,c.blue};
                for(unsigned channel=0;channel<3;++channel) {
                    auto& value=reference[(std::size_t(y)*cpu.stored_width()+x)*4+channel];
                    int v=(unsigned(value)*31+127)/255;
                    v+=c.subtract?-int(fixed[channel]):int(fixed[channel]);
                    if(c.half) v/=2;
                    v=std::clamp(v,0,31);value=std::uint8_t((v<<3)|(v>>2));
                }
            }
            if(!effects.apply_resident(composite.output(),cpu,result,disk) || result!=reference) {
                std::cerr<<"Resident circle mismatch case "<<cases<<" shape="<<circle_shape<<" flags="<<flags;return 17;
            }
        }
        for(unsigned amount:{1U,15U,31U,255U}) for(bool protect:{false,true}) {
            GpuEffectSettings fade;fade.background_subtract=amount;
            fade.background_subtract_protect_models=protect;
            expand_rgba(expected,reference,palette);
            for(unsigned y=0;y<cpu.height();++y) for(unsigned x=0;x<cpu.width();++x) {
                if(expected.get(x,y)>=128) continue;
                const int sx=int(x)-settings.offset_x,sy=int(y)-settings.offset_y;
                if(protect && sx>=0 && sy>=0 && sx<int(native.width()) && sy<int(native.height())
                    && native.get(sx,sy)!=0) continue;
                for(unsigned by=0;by<scale;++by) for(unsigned bx=0;bx<scale;++bx)
                    for(unsigned channel=0;channel<3;++channel) {
                        auto& v=reference[((std::size_t(y)*scale+by)*cpu.stored_width()+x*scale+bx)*4+channel];
                        int five=std::max(0,int((unsigned(v)*31+127)/255)-int(std::min(amount,31U)));
                        v=std::uint8_t((five<<3)|(five>>2));
                    }
            }
            if(!effects.apply_resident(composite.output(),cpu,result,fade) || result!=reference) {
                std::cerr<<"Resident background fade mismatch case "<<cases;return 18;
            }
        }
        for(unsigned flags=0;flags<8;++flags) for(bool black:{false,true}) {
            starfox::simulation::ColourMathEffectState tint;
            tint.active=true;tint.affected_layers=(flags&4)?0x3f:0x0f;
            tint.subtract=(flags&1)!=0;tint.half=(flags&2)!=0;
            tint.red=black?0:31;tint.green=black?0:17;tint.blue=black?0:3;
            GpuEffectSettings color;
            color.colour_math=GpuEffectSettings::ColourMath{
                std::uint8_t(black?0:255),std::uint8_t(black?0:140),std::uint8_t(black?0:24),
                tint.subtract,tint.half,bool(flags&4)};
            expand_rgba(expected,reference,palette);
            apply_colour_math(tint,expected,reference);
            if(!effects.apply_resident(composite.output(),cpu,result,color) || result!=reference) {
                std::cerr<<"Resident colour math mismatch case "<<cases;return 19;
            }
            expand_rgba(expected,result,palette);
            if(!referenceEffects.apply(raster.resident_output().device,expected,result,color) || result!=reference) {
                std::cerr<<"Uploaded colour math mismatch case "<<cases;return 20;
            }
        }
        for(unsigned logic=0;logic<4;++logic) for(unsigned expand=0;expand<4;++expand) {
            GpuEffectSettings masked;
            GpuEffectSettings::WindowMask w;
            w.logic=logic;w.expand_x=expand&1;w.expand_y=expand&2;w.origin_x=-9;w.origin_y=4;
            for(unsigned row=0;row<192;++row) w.rows[row]=((row*13)&255)|(((255-row*7)&255)<<8);
            masked.window_mask=w;
            expand_rgba(expected,reference,palette);
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                const int lx=int(x/scale),ly=int(y/scale);
                const int row=w.expand_y?ly*191/std::max(int(cpu.height())-1,1):ly-w.origin_y;
                if(row<0 || row>=192) continue;
                const int left=w.rows[row]&255,right=(w.rows[row]>>8)&255;
                const int sx=w.expand_x?16+lx*223/std::max(int(cpu.width())-1,1):lx-w.origin_x;
                const bool first=!(left<=right ? sx>=left && sx<=right : sx>=left || sx<=right);
                const bool second=sx>=16 && sx<=240;
                const bool hide=logic==1?first&&second:logic==2?first!=second:logic==3?first==second:first||second;
                if(hide) {const auto i=(std::size_t(y)*cpu.stored_width()+x)*4;
                    reference[i]=reference[i+1]=reference[i+2]=0;reference[i+3]=255;}
            }
            if(!effects.apply_resident(composite.output(),cpu,result,masked) || result!=reference) {
                std::cerr<<"Resident cartridge window mismatch case "<<cases;return 21;
            }
        }
        for(unsigned filter=0;filter<6;++filter) for(unsigned brightness:{0U,13U,255U}) {
            Framebuffer portrait(cpu.width(),cpu.height()),text(cpu.width(),cpu.height());
            for(unsigned y=0;y<cpu.height();++y) for(unsigned x=0;x<cpu.width();++x) {
                portrait.set(x,y,(x+y)%5 ? uint8_t(1+(x*3+y*11)%90) : 0);
                text.set(x,y,(x+y)%3 ? 0 : uint8_t(1+(x*7+y*3)%63));
            }
            GpuEffectSettings overlay;overlay.filter=filter;overlay.overlay_palette=std::span<const Rgba8>(palette.data(),64);
            overlay.subtractive_overlays[0]=GpuEffectSettings::SubtractiveOverlay{&portrait,brightness};
            overlay.subtractive_overlays[1]=GpuEffectSettings::SubtractiveOverlay{&text,30};
            std::vector<uint8_t> reference;expand_rgba(expected,reference,palette);
            GpuEffectSettings base;base.filter=filter;
            if(!referenceEffects.apply(raster.resident_output().device,actual,reference,base)) return 33;
            for(const auto& layer:overlay.subtractive_overlays)
                if(!reference_subtractive(referenceEffects,raster.resident_output().device,*layer->frame,
                    overlay.overlay_palette,scale,filter,layer->brightness,reference)) return 34;
            if(!effects.apply_resident(composite.output(),cpu,result,overlay) || result!=reference) {
                std::cerr<<"Resident subtractive overlay mismatch case "<<cases<<" filter "<<filter<<" brightness "<<brightness<<" "<<effects.status();return 35;
            }
            std::vector<uint8_t> uploaded;expand_rgba(expected,uploaded,palette);
            if(!effects.apply(raster.resident_output().device,actual,uploaded,overlay) || uploaded!=reference) {
                std::cerr<<"Uploaded subtractive overlay mismatch case "<<cases<<" filter "<<filter;return 36;
            }
            std::array<GpuRaster,2> residentLayers;
            for(unsigned layer=0;layer<2;++layer) {
                const auto& frame=*overlay.subtractive_overlays[layer]->frame;
                RasterCommands commands;commands.reset(frame.width(),frame.height());
                RasterCommand c;c.right=frame.width();c.bottom=frame.height();
                c.textured=5;c.texture_offset=commands.snapshot(frame.pixels());
                c.u_mask=frame.width();c.v_mask=frame.height();c.du=c.dv=1;c.reserved0=1;
                commands.add(c);
                if(!residentLayers[layer].render_resident(raster.resident_output().device,commands,false)) return 101;
                overlay.subtractive_overlays[layer]->resident=residentLayers[layer].resident_output();
            }
            for(unsigned uploadedLayer=0;uploadedLayer<2;++uploadedLayer) {
                auto mixed=overlay;mixed.subtractive_overlays[uploadedLayer]->resident={};
                if(!effects.apply_resident(composite.output(),cpu,result,mixed) || result!=reference) {
                    std::cerr<<"Mixed GPU/uploaded overlay mismatch "<<cases;return 105;
                }
            }
            // Empty the CPU copies: resident input must be authoritative.
            portrait.clear();text.clear();
            if(!effects.apply_resident(composite.output(),cpu,result,overlay) || result!=reference) {
                std::cerr<<"GPU overlay input mismatch case "<<cases<<" filter "<<filter<<" brightness "<<brightness;return 102;
            }
            expand_rgba(expected,uploaded,palette);
            if(!effects.apply(raster.resident_output().device,actual,uploaded,overlay) || uploaded!=reference) return 106;
            if(cases==0 && filter==0 && brightness==0) {
                SdlGpuEffects invalidExtent,invalidDevice;
                auto invalid=overlay;invalid.subtractive_overlays[0]->resident.width++;
                if(invalidExtent.apply_resident(composite.output(),cpu,result,invalid)) return 103;
                invalid=overlay;invalid.subtractive_overlays[0]->resident.device=nullptr;
                if(invalidDevice.apply_resident(composite.output(),cpu,result,invalid)) return 104;
            }
        }
        for(unsigned mode=0;mode<8;++mode) for(unsigned amount:{0U,7U,31U,255U}) {
            GpuEffectSettings fade;
            fade.planet_fade=GpuEffectSettings::PlanetFade{3,4,13,17,amount,amount/2,bool(mode&1),bool(mode&2)};
            if(mode&4) {fade.planet_fade->coverage=true;fade.planet_fade->rows.fill(0x155U);}
            std::vector<std::uint8_t> reference;expand_rgba(expected,reference,palette);
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                const int lx=int(x/scale),ly=int(y/scale);
                const bool inside=lx>=3 && lx<=13 && ly>=4 && ly<=17 && (!(mode&4) || ((0x155U>>(lx-3))&1U));
                const auto i=(std::size_t(y)*cpu.stored_width()+x)*4;
                for(unsigned channel=0;channel<3;++channel) {
                    auto subtract=[&](unsigned value) {
                        const int five=std::max(0,int((unsigned(reference[i+channel])*31+127)/255)-int(std::min(value,31U)));
                        reference[i+channel]=std::uint8_t((five<<3)|(five>>2));
                    };
                    if((mode&1) && !inside) subtract(amount);
                    if(mode&2) subtract(amount/2);
                }
            }
            if(!effects.apply_resident(composite.output(),cpu,result,fade) || result!=reference) {
                std::cerr<<"Resident planet fade mismatch case "<<cases;return 31;
            }
            std::vector<std::uint8_t> uploaded;expand_rgba(expected,uploaded,palette);
            if(!effects.apply(raster.resident_output().device,cpu,uploaded,fade) || uploaded!=reference) {
                std::cerr<<"Uploaded planet fade mismatch case "<<cases;return 32;
            }
        }
        for(unsigned brightness:{0U,7U,15U}) for(bool styled:{false,true}) {
            Framebuffer ink(19,23);
            for(unsigned y=0;y<ink.height();++y) for(unsigned x=0;x<ink.width();++x)
                ink.set(x,y,std::array<uint8_t,5>{0,14,10,3,16}[(x+y)%5]);
            const auto origin=static_cast<int>((cpu.width()-256U)/2U);
            GpuEffectSettings setup;
            if(styled) {setup.hdr=1;setup.bloom_model=1;setup.bloom_world=2;setup.anti_aliasing=1;}
            expand_rgba(expected,reference,palette);
            if(!referenceEffects.apply(raster.resident_output().device,expected,reference,setup)) return 26;
            setup.setup_overlay=GpuEffectSettings::SetupOverlay{&ink,3-origin,13-origin,brightness};
            for(unsigned y=0;y<cpu.stored_height();++y) for(unsigned x=0;x<cpu.stored_width();++x) {
                const unsigned sx=x/scale,sy=y/scale;const auto i=(std::size_t(y)*cpu.stored_width()+x)*4;
                if(sx>=3 && sx<=13 && sy>=20 && sy<=222)
                    for(unsigned c=0;c<3;++c) reference[i+c]/=4;
                const auto pixel=sx<ink.width() && sy<ink.height()?ink.get(sx,sy):0;if(!pixel) continue;
                const auto colour=pixel&15;
                const auto rgb=colour==14?std::array<unsigned,3>{255,255,255}:
                    colour==10?std::array<unsigned,3>{255,220,64}:std::array<unsigned,3>{180,200,215};
                for(unsigned c=0;c<3;++c) reference[i+c]=rgb[c]*brightness/15;
            }
            if(!effects.apply_resident(composite.output(),cpu,result,setup) || result!=reference) {
                std::cerr<<"Resident setup overlay mismatch case "<<cases<<" brightness="<<brightness<<" styled="<<styled<<" "<<effects.status();return 26;
            }
            std::vector<uint8_t> uploaded;expand_rgba(expected,uploaded,palette);
            if(!referenceEffects.apply(raster.resident_output().device,expected,uploaded,setup) || uploaded!=reference) {
                std::cerr<<"Uploaded setup overlay mismatch case "<<cases;return 27;
            }
        }
        {
            expand_rgba(expected,reference,palette);
            reference_touch(reference,cpu.width(),cpu.height(),scale);
            GpuEffectSettings touch;touch.touch_controls=true;
            if(!effects.apply_resident(composite.output(),cpu,result,touch) || result!=reference) {
                std::cerr<<"Resident clipped touch overlay mismatch "<<cases;return 30;
            }
        }
        for(int at:{-5,0,23}) for(bool styled:{false,true}) for(bool panel:{false,true}) {
            GpuEffectSettings overlay;
            GpuEffectSettings::HostOverlay h;
            h.width=37;h.height=9;h.x=at;h.y=at-2;
            for(unsigned i=0;i<h.width*h.height;++i) if(i%5<2) h.bits[i/32]|=1U<<(i%32);
            if(panel) overlay.confirmation_overlay=h;else overlay.host_overlay=h;
            expand_rgba(expected,reference,palette);
            if(styled) {
                GpuEffectSettings early;early.hdr=1;early.chromatic=2;
                overlay.hdr=early.hdr;overlay.chromatic=early.chromatic;
                if(!referenceEffects.apply(raster.resident_output().device,expected,reference,early)) return 24;
            }
            if(panel) {
                for(int y=h.y-4;y<h.y+int(h.height)+4;++y) for(int x=h.x-6;x<h.x+int(h.width)+6;++x) {
                    if(x<0 || y<0 || x>=int(cpu.width()) || y>=int(cpu.height())) continue;
                    const bool border=x==h.x-6 || x==h.x+int(h.width)+5 || y==h.y-4 || y==h.y+int(h.height)+3;
                    for(unsigned by=0;by<scale;++by) for(unsigned bx=0;bx<scale;++bx) {
                        const auto p=((std::size_t(y)*scale+by)*cpu.stored_width()+x*scale+bx)*4;
                        reference[p]=reference[p+1]=reference[p+2]=border?255:0;reference[p+3]=255;
                    }
                }
            }
            // Independent source algorithm: shadow (FPS only), then white ink.
            for(unsigned pass=0;pass<2;++pass) for(unsigned y=0;y<h.height;++y) for(unsigned x=0;x<h.width;++x) {
                if(panel && pass==0) continue;
                const auto i=y*h.width+x;if(!(h.bits[i/32]&(1U<<(i%32)))) continue;
                int dx=h.x+int(x)+(pass==0),dy=h.y+int(y)+(pass==0);
                if(dx<0 || dy<0 || dx>=int(cpu.width()) || dy>=int(cpu.height())) continue;
                for(unsigned by=0;by<scale;++by) for(unsigned bx=0;bx<scale;++bx) {
                    const auto p=((std::size_t(dy)*scale+by)*cpu.stored_width()+dx*scale+bx)*4;
                    reference[p]=reference[p+1]=reference[p+2]=pass?255:0;reference[p+3]=255;
                }
            }
            if(styled) {
                GpuEffectSettings late;late.model_effect=1;late.world_effect=2;
                late.bloom_model=1;late.bloom_world=2;late.anti_aliasing=1;
                overlay.model_effect=late.model_effect;overlay.world_effect=late.world_effect;
                overlay.bloom_model=late.bloom_model;overlay.bloom_world=late.bloom_world;
                overlay.anti_aliasing=late.anti_aliasing;
                if(!referenceEffects.apply(raster.resident_output().device,expected,reference,late)) return 25;
            }
            if(!effects.apply_resident(composite.output(),cpu,result,overlay) || result!=reference) {
                std::cerr<<"Resident host glyph mismatch case "<<cases;return 22;
            }
            expand_rgba(expected,result,palette);
            if(!referenceEffects.apply(raster.resident_output().device,expected,result,overlay) || result!=reference) {
                std::cerr<<"Uploaded host glyph mismatch case "<<cases;return 23;
            }
        }
        ++cases;
    }
    {
        SdlGpuEffects lean;
        Framebuffer frame(400,224,4),ink(400,224);frame.enable_layer_tags(true);
        std::vector<uint8_t> pixels(frame.pixels().size()*4,90);
        GpuEffectSettings plain;
        const auto device=raster.resident_output().device;
        const auto full=std::uint64_t(frame.stored_width())*frame.stored_height()*4;
        if(!lean.apply(device,frame,pixels,plain) || lean.texture_payload_bytes()!=full*2+32) return 46;
        const auto minimal=lean.texture_payload_bytes();
        auto filtered=plain;filtered.filter=3;
        if(!lean.apply(device,frame,pixels,filtered) || lean.texture_payload_bytes()!=minimal+400*224*4) return 47;
        auto bright=plain;bright.bloom_model=1;
        if(!lean.apply(device,frame,pixels,bright)
            || lean.texture_payload_bytes()!=minimal+400*224*4+full*2+200*112*16*4) return 48;
        auto overlay=filtered;overlay.overlay_palette=palette;
        overlay.subtractive_overlays[0]=GpuEffectSettings::SubtractiveOverlay{&ink,30};
        if(!lean.apply(device,frame,pixels,overlay)
            || lean.texture_payload_bytes()!=minimal+400*224*4+full*3+200*112*16*4) return 49;
        const auto expanded=lean.texture_payload_bytes();
        if(!lean.apply(device,frame,pixels,plain) || lean.texture_payload_bytes()!=expanded) return 50;
        std::cout<<"Optional texture allocation: plain 400x224 at 4x uses "<<minimal
            <<" payload bytes instead of "<<expanded<<"; optional targets reused until resize\n";
        Framebuffer small(32,24);small.enable_layer_tags(true);pixels.assign(small.pixels().size()*4,90);
        if(!lean.apply(device,small,pixels,plain) || lean.texture_payload_bytes()!=32*24*8+32) return 51;
        lean.release_device();if(lean.texture_payload_bytes()!=0) return 52;
    }
    for(unsigned width:{256U,400U,800U}) for(unsigned scale:{1U,2U,4U}) {
        Framebuffer frame(width,224,scale),ink(width,224);frame.enable_layer_tags(true);
        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<width;++x)
            if((x+y)%17==0) ink.set(x,y,(x%3)==0?14:(x%3)==1?10:3);
        std::vector<uint8_t> pixels(std::size_t(frame.stored_width())*frame.stored_height()*4);
        for(std::size_t i=0;i<pixels.size();i+=4) {pixels[i]=100;pixels[i+1]=104;pixels[i+2]=108;pixels[i+3]=57;}
        auto reference=pixels;
        const auto origin=int((width-256)/2);
        for(unsigned y=0;y<frame.stored_height();++y) for(unsigned x=0;x<frame.stored_width();++x) {
            const unsigned sx=x/scale,sy=y/scale;const auto i=(std::size_t(y)*frame.stored_width()+x)*4;
            if(int(sx)-origin>=16 && int(sx)-origin<=239 && sy>=20 && sy<=222)
                for(unsigned c=0;c<3;++c) reference[i+c]/=4;
            const auto pixel=ink.get(sx,sy);if(!pixel) continue;
            const auto rgb=pixel==14?std::array<unsigned,3>{255,255,255}:pixel==10?std::array<unsigned,3>{255,220,64}:std::array<unsigned,3>{180,200,215};
            for(unsigned c=0;c<3;++c) reference[i+c]=rgb[c]*7/15;
        }
        GpuEffectSettings setup;setup.setup_overlay=GpuEffectSettings::SetupOverlay{&ink,16,239,7};
        if(!effects.apply(raster.resident_output().device,frame,pixels,setup) || pixels!=reference) {
            std::cerr<<"Full-size setup overlay mismatch "<<width<<" scale "<<scale;return 28;
        }
        reference_touch(reference,width,224,scale);
        GpuEffectSettings touch;touch.touch_controls=true;
        if(!effects.apply(raster.resident_output().device,frame,pixels,touch) || pixels!=reference) {
            std::cerr<<"Full-size touch overlay mismatch "<<width<<" scale "<<scale;return 29;
        }
        for(std::size_t i=0;i<pixels.size();i+=4) {pixels[i]=100;pixels[i+1]=104;pixels[i+2]=108;pixels[i+3]=57;}
        setup.touch_controls=true;
        if(!effects.apply(raster.resident_output().device,frame,pixels,setup) || pixels!=reference) {
            std::cerr<<"Combined setup/touch ordering mismatch "<<width<<" scale "<<scale;return 31;
        }
        auto* device=static_cast<SDL_GPUDevice*>(raster.resident_output().device);
        SDL_GPUTextureCreateInfo texture_info{};texture_info.type=SDL_GPU_TEXTURETYPE_2D;
        texture_info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        texture_info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
        texture_info.width=frame.stored_width();texture_info.height=frame.stored_height();texture_info.layer_count_or_depth=1;texture_info.num_levels=1;
        auto* texture=SDL_CreateGPUTexture(device,&texture_info);
        if(!texture) return 32;
        struct TextureLifetime {SDL_GPUDevice* device;SDL_GPUTexture* texture;~TextureLifetime(){SDL_ReleaseGPUTexture(device,texture);}} texture_lifetime{device,texture};
        for(std::size_t i=0;i<pixels.size();i+=4) {pixels[i]=100;pixels[i+1]=104;pixels[i+2]=108;pixels[i+3]=57;}
        const auto original=pixels;setup.presentation_texture=texture;
        if(!effects.apply(device,frame,pixels,setup) || pixels!=original) {
            std::cerr<<"Direct setup/touch presentation changed CPU pixels";return 33;
        }
        if(!effects.readback(pixels) || pixels!=reference) {
            std::cerr<<"Deferred setup/touch capture differs from CPU reference";return 34;
        }
        Framebuffer portrait(width,224),briefing(width,224);
        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<width;++x) {
            if(x%47<20 && y%37<28) portrait.set(x,y,1+(x+y)%255);
            if((x+y)%13<2) briefing.set(x,y,1+(x*3+y)%255);
        }
        auto combined=setup;combined.filter=width==400?5:3;combined.overlay_palette=palette;
        combined.subtractive_overlays[0]=GpuEffectSettings::SubtractiveOverlay{&portrait,19};
        combined.subtractive_overlays[1]=GpuEffectSettings::SubtractiveOverlay{&briefing,30};
        combined.planet_fade=GpuEffectSettings::PlanetFade{20,30,100,190,5,2,true,true};
        combined.hdr=1;combined.bloom_model=1;combined.bloom_world=2;combined.anti_aliasing=1;
        reference=original;
        GpuEffectSettings base_filter;base_filter.filter=combined.filter;
        if(!referenceEffects.apply(device,frame,reference,base_filter)) return 37;
        for(const auto& layer:combined.subtractive_overlays)
            if(!reference_subtractive(referenceEffects,device,*layer->frame,palette,
                scale,combined.filter,layer->brightness,reference)) return 38;
        auto tail=combined;tail.filter=0;tail.subtractive_overlays={};tail.presentation_texture=nullptr;
        if(!referenceEffects.apply(device,frame,reference,tail)) return 39;
        pixels=original;
        if(!effects.apply(device,frame,pixels,combined) || pixels!=original) {
            std::cerr<<"Direct planet/briefing presentation changed CPU pixels";return 40;
        }
        if(!effects.readback(pixels) || pixels!=reference) {
            std::cerr<<"Combined planet/briefing/filter/fade/style/setup/touch mismatch "<<width<<" scale "<<scale;return 41;
        }
    }
    std::cout<<cases<<" GPU composition cases match native pixels/tags/RGBA, late-overlay coverage/ownership/recovery, resident effects/shadows and early shadow/filter ordering exactly; "
        <<cases*4<<" horizontal shutter, "<<cases*32<<" circle (including wide/tangent), "<<cases*8<<" background fade and "
        <<cases*16<<" colour math cases (resident/uploaded), "<<cases*64<<" planet fade cases (resident/uploaded), "<<cases*16<<" cartridge window and "
        <<cases*12<<" host/panel and "<<cases*6<<" setup overlay cases (resident/uploaded), "
        <<cases*18<<" two-layer subtractive overlay cases (resident/uploaded), "<<cases
        <<" resident touch cases, "<<cases*2<<" direct model/glow split cases, plus 9 full-size setup/touch and deferred planet/briefing composition cases pass\n";
}
