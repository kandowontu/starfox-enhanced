#include "starfox/render/gpu_projection.hpp"
#include "starfox/render/grid_projection.hpp"
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/packed_projection.hpp"
#include <iomanip>
#include <numbers>
#include <SDL3/SDL.h>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <source_location>
#include <vector>
namespace {
void require(bool ok,std::source_location where=std::source_location::current()) {
    if(!ok) throw std::runtime_error("Projection check line "+std::to_string(where.line())+": "+SDL_GetError());
}
int word(int value){return std::bit_cast<std::int16_t>(std::uint16_t(value));}
std::array<int,4> reference(const starfox::render::NativeProjectionPoint& source) {
    if(source.reserved) return {0,0,0,-1};
    const int x=word(source.x),y=word(source.y),z=word(source.z);
    const double depth=std::max(1,std::abs(z));
    const int dominant=std::max(std::abs(x),std::abs(y));
    const bool saturated=dominant*256.0/depth>=16384.;
    const auto axis=[&](int coordinate,int vanish) {
        int result=int(std::floor(std::abs(coordinate)*(saturated?16383.:256.)/(saturated?dominant:depth)));
        if((coordinate<0)!=(z<0)) result=-result;
        return word(result+word(vanish));
    };
    return {axis(x,source.vanish_x),axis(y,source.vanish_y),z,z>=0?1:0};
}
struct Resources {
    SDL_GPUDevice* device{};SDL_GPUBuffer* input{},*faces{},*poses{};
    SDL_GPUTransferBuffer *upload{},*download{};
    ~Resources(){
        if(device) {
            SDL_WaitForGPUIdle(device);
            if(input) SDL_ReleaseGPUBuffer(device,input);
            if(faces) SDL_ReleaseGPUBuffer(device,faces);
            if(poses) SDL_ReleaseGPUBuffer(device,poses);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            if(download) SDL_ReleaseGPUTransferBuffer(device,download);
            SDL_DestroyGPUDevice(device);
        }
        SDL_Quit();
    }
};
}
int main()try {
    using starfox::render::NativeProjectionPoint;
    using starfox::render::NativeTransformPose;
    using starfox::render::NativeTransformVertex;
    Resources r;require(SDL_Init(SDL_INIT_VIDEO));
    r.device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_MSL|SDL_GPU_SHADERFORMAT_DXIL,true,nullptr);require(r.device);
    starfox::render::GpuProjection projection;
    require(!projection.enqueue(nullptr,nullptr,nullptr,1));
    constexpr unsigned maximum=131071;
    SDL_GPUBufferCreateInfo input_info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,maximum*sizeof(NativeProjectionPoint),0};
    r.input=SDL_CreateGPUBuffer(r.device,&input_info);require(r.input);
    input_info.size=maximum*16;r.faces=SDL_CreateGPUBuffer(r.device,&input_info);require(r.faces);
    constexpr unsigned pose_count=17;
    input_info.size=pose_count*sizeof(NativeTransformPose);
    r.poses=SDL_CreateGPUBuffer(r.device,&input_info);require(r.poses);
    SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,maximum*48+pose_count*sizeof(NativeTransformPose),0};
    SDL_GPUTransferBufferCreateInfo download_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,maximum*256,0};
    r.upload=SDL_CreateGPUTransferBuffer(r.device,&upload_info);require(r.upload);
    r.download=SDL_CreateGPUTransferBuffer(r.device,&download_info);require(r.download);
    std::vector<NativeProjectionPoint> points(maximum);
    starfox::render::GpuScene text_scene;
    // Fractional raster output must preserve source texture coordinates,
    // checkerboard phase, painter order and explicit coverage. Exercise both
    // binning implementations and the ordered scene path.
    for(unsigned fixture=0;fixture<16;++fixture) {
        const std::array<float,2> jitter=fixture>=8?std::array<float,2>{.375f,-.25f}:std::array<float,2>{};
        using namespace starfox::render;
        RasterCommands batch;batch.reset(224,192);
        batch.add({-9,3,211,188,31,67,1,2});
        std::array<std::uint8_t,64> texels{};
        for(unsigned i=0;i<texels.size();++i) texels[i]=i%5?std::uint8_t(i+1):0;
        RasterCommand texture{};
        texture.left=37;texture.top=24;texture.right=220;texture.bottom=166;
        texture.textured=3;texture.texture_offset=batch.texture(texels);
        texture.u_mask=texture.v_mask=7;texture.du=32;texture.dv=3;texture.tag=3;
        batch.add(texture);
        batch.add({82,67,119,111,0,0,0,1}); // Covered black is not transparency.
        const unsigned width=fixture&1?299:149,height=fixture&1?255:127;
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        GpuRaster raster;GpuScene scene;
        GpuRasterOutput output;
        if(fixture&4) {
            const std::array<GpuSceneDraw,1> draws{GpuRasterDraw{&batch,false,bool(fixture&2)}};
            const auto resized=resize_scene_raster(draws,224,192);require(bool(resized));
            require(!std::get<GpuRasterDraw>(draws[0]).independent_raster_size);
            output=scene.enqueue_batch(r.device,command,width,height,*resized,jitter);
        } else output=raster.enqueue_commands(r.device,command,batch,false,bool(fixture&2),{width,height},jitter);
        require(output.pixels && output.width==width && output.height==height);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.pixels),0,width*height*4};
        SDL_GPUTransferBufferLocation to{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        Framebuffer expected(224,192);expected.enable_layer_tags(true);
        replay_raster_commands(batch,expected,nullptr);
        const auto* actual=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const float center=jitter==std::array<float,2>{}?0.f:.5f;
            const int sx=int(std::floor((float(x)+center-jitter[0])*224/width)),sy=int(std::floor((float(y)+center-jitter[1])*192/height));
            if(sx<0 || sx>=224 || sy<0 || sy>=192) {require(actual[y*width+x]==0);continue;}
            const unsigned index=sy*224+sx;
            if((actual[y*width+x]&255U)!=expected.pixels()[index]) throw std::runtime_error("Raster fixture="+std::to_string(fixture)+" x="+std::to_string(x)+" y="+std::to_string(y)+" source="+std::to_string(sx)+","+std::to_string(sy)+" got="+std::to_string(actual[y*width+x]&255U)+" expected="+std::to_string(expected.pixels()[index]));
            require(((actual[y*width+x]>>8)&255U)==expected.layer_tags()[index]);
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"Fractional raster: 16 zero/jittered direct/scene, CPU/GPU binning fixtures passed\n";
    for(unsigned fixture=0;fixture<48;++fixture) {
        const bool custom=fixture%24>=12;
        const std::array<float,2> jitter=fixture>=24?std::array<float,2>{-.375f,.25f}:std::array<float,2>{};
        const unsigned scale=1U<<(fixture%3),width=custom?299:224*scale,height=custom?255:192*scale;
        const std::array<std::uint32_t,2> logical=custom?std::array<std::uint32_t,2>{224,192}:std::array<std::uint32_t,2>{};
        starfox::render::ScaledTextRenderer::ProjectedFrame text;
        text.pose.x=-12.125;text.pose.y=4.375;text.pose.z=fixture<9?256.125:127;
        text.character_size=17;text.colour=114;
        std::array<std::uint16_t,16> glyph{};
        for(unsigned row=0;row<16;++row) glyph[row]=std::uint16_t(0xa53cU^(row*137));
        text.glyphs={glyph,{},glyph};
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        const auto text_tag=fixture%2?unsigned(starfox::render::PixelLayer::three_d):1U;
        if(custom) text.pose.z=256.125;
        auto* output=static_cast<SDL_GPUBuffer*>(projection.enqueue_text(r.device,command,text,width,height,scale,std::uint8_t(text_tag),0,512,logical,jitter));require(output);
        if(fixture%2) {
            const std::array<starfox::render::GpuSceneDraw,1> draws{starfox::render::GpuTextDraw{text,scale,0,512,logical}};
            output=static_cast<SDL_GPUBuffer*>(text_scene.enqueue_batch(r.device,command,width,height,draws,jitter).pixels);require(output);
        }
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion from{output,0,width*height*4};SDL_GPUTransferBufferLocation to{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        starfox::render::Framebuffer expected(224*scale,192*scale);expected.set_draw_scale(scale);
        starfox::render::ScaledTextRenderer::draw_projected(text,expected);
        const auto* actual=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
        for(unsigned i=0;i<width*height;++i) {
            const float center=jitter==std::array<float,2>{}?0.f:.5f;
            const int sx=int(std::floor((float(i%width)+center-jitter[0])*(224*scale)/width));
            const int sy=int(std::floor((float(i/width)+center-jitter[1])*(192*scale)/height));
            const auto pixel=sx>=0 && sy>=0 && sx<int(224*scale) && sy<int(192*scale)?expected.pixels()[sy*(224*scale)+sx]:0;
            if(actual[i]!=(pixel?unsigned(pixel)|(text_tag<<8)|(1U<<26):0U))
                throw std::runtime_error("Projected text pixel mismatch "+std::to_string(fixture));
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"Projected text GPU/CPU pixels pass at 1x/2x/4x and independent 299x255 output\n";
    starfox::render::GpuStereoScene stereo_text;
    for(unsigned scale:{1U,2U,4U}) for(double depth:{256.,512.,1024.}) {
        const unsigned width=224*scale,height=192*scale,bytes=width*height*4;
        starfox::render::ScaledTextRenderer::ProjectedFrame text;
        text.pose.x=0;text.pose.y=0;text.pose.z=depth;text.character_size=32;text.colour=114;
        std::array<std::uint16_t,16> glyph{};glyph.fill(0x9669);text.glyphs={glyph};
        const std::array<starfox::render::GpuSceneDraw,1> draws{starfox::render::GpuTextDraw{text,scale}};
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        const auto outputs=stereo_text.enqueue(r.device,command,width,height,draws,8,512);require(outputs.has_value());
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        for(unsigned eye=0;eye<2;++eye) {
            SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>((*outputs)[eye].pixels),0,bytes};
            SDL_GPUTransferBufferLocation to{r.download,eye*bytes};SDL_DownloadFromGPUBuffer(copy,&from,&to);
        }
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* actual=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
        for(unsigned eye=0;eye<2;++eye) {
            auto reference_text=text;
            reference_text.pose.x=(eye?4.:-4.)*(depth/512.-1.);
            starfox::render::Framebuffer expected(width,height);expected.set_draw_scale(scale);
            starfox::render::ScaledTextRenderer::draw_projected(reference_text,expected);
            for(unsigned i=0;i<width*height;++i) {
                const auto pixel=expected.pixels()[i];
                const unsigned packed=pixel?unsigned(pixel)|(unsigned(starfox::render::PixelLayer::three_d)<<8)|(1U<<26):0;
                if(actual[eye*width*height+i]!=packed) throw std::runtime_error("Stereo text depth mismatch");
                if(depth==512 && actual[i]!=actual[width*height+i]) throw std::runtime_error("Text convergence mismatch");
            }
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"Stereo text near/convergence/far pixel checks pass in both eyes\n";
    starfox::render::GpuRaster particle_raster;
    starfox::render::GpuScene particle_scene;
    for(unsigned fixture=0;fixture<15;++fixture) {
        std::array<std::array<int,8>,300> particles{};
        for(unsigned i=0;i<300;++i) {
            particles[i]={int(i*53%2000)-1000,int(i*97%1200)-600,int(i*73%6000)-500,
                int(112+i%16)|(i%2?256:0),int(i*59%2000)-1000,int(i*101%1200)-600,int(i*79%6000)-500,0};
        }
        particles[0]={0,0,512,114|256,-10,0,512,0};
        particles[1]={-32768,0,512,115,32767,0,512,0};
        particles[2]={32767,0,512,115,-32768,0,512,0};
        particles[3]={0,0,256,116|256,0,0,255,0};
        // All octants, horizontal/vertical/zero-length and half-step ties.
        // Depth 512 maps these source deltas to the small integer slopes.
        for(unsigned i=4;i<148;++i) {
            const int dx=int((i-4)%12)-6,dy=int((i-4)/12)-6;
            particles[i]={dx*2,dy*2,512,int(112+i%16)|256,0,0,512,0};
        }
        auto* mapped=SDL_MapGPUTransferBuffer(r.device,r.upload,false);require(mapped);
        std::memcpy(mapped,particles.data(),300*32);SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUTransferBufferLocation source{r.upload,0};SDL_GPUBufferRegion destination{r.input,0,300*32};
        SDL_UploadToGPUBuffer(copy,&source,&destination,true);SDL_EndGPUCopyPass(copy);
        starfox::render::GpuProjection::ParticleSettings settings;
        settings.width=fixture%2?400:224;settings.height=192;settings.count=300;
        const double alphas[]{0,.125,.5,.999999,1};settings.alpha=alphas[fixture%5];
        settings.owner_x=.125;settings.owner_y=-.375;settings.owner_z=.0625;
        settings.eye_x=fixture/5==0?0:fixture/5==1?-3.2f:3.2f;
        starfox::render::ParticleRenderer::OwnerFrame owner_frame;
        owner_frame.pose.x=settings.owner_x;owner_frame.pose.y=settings.owner_y;owner_frame.pose.z=settings.owner_z;
        owner_frame.alpha=settings.alpha;
        for(const auto& p:particles) {
            starfox::simulation::ParticleState particle{};particle.life=1;particle.owner=owner_frame.owner;
            particle.x=std::int16_t(p[0]);particle.y=std::int16_t(p[1]);particle.z=std::int16_t(p[2]);
            particle.previous_x=std::int16_t(p[4]);particle.previous_y=std::int16_t(p[5]);particle.previous_z=std::int16_t(p[6]);
            particle.colour=std::uint8_t((p[3]&255)-112);particle.flags=(p[3]&256)?4:0;
            owner_frame.particles.push_back(particle);
        }
        auto* output=static_cast<SDL_GPUBuffer*>(fixture%2
            ?projection.enqueue_particle_frame(r.device,command,owner_frame,settings.width,settings.height,settings.eye_x,settings.convergence)
            :projection.enqueue_particles(r.device,command,r.input,settings));require(output);
        const unsigned scale=1U<<(fixture%3),width=settings.width*scale,height=192*scale;
        auto* spans=projection.enqueue_particle_spans(command,height,scale,1,20,200);require(spans);
        auto raster=particle_raster.enqueue_row_spans(r.device,command,spans,300,width,height,false,nullptr,true);require(raster.pixels);
        if(fixture%2) {
            owner_frame.pose.effect_clip_left=20;owner_frame.pose.effect_clip_right=200;
            const std::array<starfox::render::GpuSceneDraw,1> draws{
                starfox::render::GpuParticleDraw{owner_frame,scale,settings.eye_x,settings.convergence}};
            raster=particle_scene.enqueue_batch(r.device,command,width,height,draws);require(raster.pixels);
        }
        copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion result{output,0,300*32};SDL_GPUTransferBufferLocation download{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&result,&download);
        result={static_cast<SDL_GPUBuffer*>(raster.pixels),0,width*height*4};download.offset=300*32;
        SDL_DownloadFromGPUBuffer(copy,&result,&download);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* actual=static_cast<const int*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
        std::vector<unsigned> expected_pixels(width*height);
        const auto put=[&](int x,int y,int colour) {
            if(x<20 || x>=200 || y<0 || y>=192) return;
            for(unsigned dy=0;dy<scale;++dy) for(unsigned dx=0;dx<scale;++dx)
                expected_pixels[(unsigned(y)*scale+dy)*width+unsigned(x)*scale+dx]=unsigned(colour)
                    |((fixture%2?unsigned(starfox::render::PixelLayer::three_d):1U)<<8)|(1U<<26);
        };
        for(unsigned i=0;i<300;++i) {
            const auto& p=particles[i];
            const auto interpolate=[&](unsigned axis) {int delta=p[axis]-p[axis+4];if(delta>32767)delta-=65536;else if(delta< -32768)delta+=65536;return p[axis+4]+delta*settings.alpha;};
            const auto project=[&](double x,double y,double z) {
                std::array<int,4> result{};if(z<256) return result;
                auto px=x*256./z;
                if(settings.eye_x!=0) px+=double(256.f*settings.eye_x)*(1./settings.convergence-1./z);
                const int sx=int(settings.width/2)+int(std::trunc(px)),sy=96+int(std::trunc(y*256./z));
                if(sx>=0 && sy>=0 && sx<int(settings.width)-1 && sy<191) result={sx,sy,0,1};
                return result;
            };
            auto current=project(settings.owner_x+interpolate(0),settings.owner_y+interpolate(1),settings.owner_z+interpolate(2));
            std::array<int,4> previous{};
            if(current[3]) {
                current[2]=p[3]&255;
                if(p[3]&256) {
                    previous=project(settings.owner_x+p[4],settings.owner_y+p[5],settings.owner_z+p[6]);
                    if(!previous[3]) current={};else current[3]=3;
                }
            }
            for(unsigned a=0;a<4;++a) if(actual[i*8+a]!=current[a] || actual[i*8+4+a]!=previous[a])
                throw std::runtime_error("Particle mismatch fixture "+std::to_string(fixture)+" point "+std::to_string(i));
            if(current[3]==1) {
                put(current[0],current[1],current[2]);put(current[0]+1,current[1],current[2]);
                put(current[0],current[1]+1,current[2]);put(current[0]+1,current[1]+1,current[2]);
            } else if(current[3]==3) {
                int x=previous[0],y=previous[1];
                const int dx=std::abs(current[0]-x),dy=-std::abs(current[1]-y);
                const int sx=x<current[0]?1:-1,sy=y<current[1]?1:-1;
                int error=dx+dy;
                for(;;) {
                    put(x,y,current[2]);if(x==current[0] && y==current[1]) break;
                    const int doubled=error*2;
                    if(doubled>=dy) {error+=dy;x+=sx;}if(doubled<=dx) {error+=dx;y+=sy;}
                }
            }
        }
        for(unsigned i=0;i<expected_pixels.size();++i) if(unsigned(actual[300*8+i])!=expected_pixels[i])
            throw std::runtime_error("Particle raster mismatch fixture "+std::to_string(fixture));
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"4500 GPU particles: snapshot upload/interpolation/wrapping/trail/stereo projection and raster passed\n";
    {
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        const std::array<starfox::render::GpuSceneDraw,2> draws{
            starfox::render::GpuParticleDraw{},starfox::render::GpuDustDraw{}};
        const auto cleared=particle_scene.enqueue_batch(r.device,command,64,64,draws);require(cleared.pixels);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(cleared.pixels),0,64*64*4};
        SDL_GPUTransferBufferLocation to{r.download,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* pixels=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(pixels);
        for(unsigned i=0;i<64*64;++i) if(pixels[i]) throw std::runtime_error("Inactive particle scene retained pixels");
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    starfox::render::GpuRaster dust_raster;
    for(unsigned fixture=0;fixture<18;++fixture) {
        const unsigned dust_scale=1U<<((fixture/3)%3),dust_height=192*dust_scale;
        std::array<std::array<double,4>,511> dust{};
        std::array<unsigned,64> palette{};
        for(unsigned i=0;i<64;++i) palette[i]=(i*7+i/16)%16;
        for(unsigned i=0;i<511;++i) {
            dust[i]={double(int(i*137%2000)-1000)+.125,double(int(i*73%1200)-600)-.375,
                double(int(i*101%8000)-1000)+.0625,0};
        }
        const double depths[]{0,255.999999,256,256.000001,1023.99999,1024,4095,4095.00001,-256};
        for(unsigned i=0;i<9;++i) dust[i]={0,0,depths[i],0};
        constexpr unsigned dust_bytes=511*32;
        auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(r.device,r.upload,false));require(mapped);
        std::memcpy(mapped,dust.data(),dust_bytes);std::memcpy(mapped+dust_bytes,palette.data(),256);
        SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUTransferBufferLocation from{r.upload,0};SDL_GPUBufferRegion to{r.input,0,dust_bytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=dust_bytes;to={r.faces,0,256};SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        starfox::render::GpuProjection::DustSettings settings;
        settings.row_x[0]=settings.row_y[1]=settings.row_z[2]=32767;
        if(fixture>=9) {settings.row_x[2]=1234;settings.row_y[0]=-4567;settings.row_z[1]=8910;}
        settings.viewport[0]=fixture%2?400:224;settings.viewport[1]=192;settings.count=511;
        settings.viewport[2]=fixture%3?-48:0;settings.viewport[3]=fixture%3?-32:0;
        settings.eye_x=fixture%3==0?0:fixture%3==1?-3.2f:3.2f;
        auto* output=static_cast<SDL_GPUBuffer*>(projection.enqueue_dust(r.device,command,r.input,r.faces,settings));require(output);
        auto* dust_spans=projection.enqueue_dust_spans(command,dust_height,dust_scale,1,80,100);require(dust_spans);
        const unsigned dust_width=unsigned(settings.viewport[0])*dust_scale;
        const auto rendered=dust_raster.enqueue_row_spans(r.device,command,dust_spans,511,
            dust_width,dust_height,false,nullptr,true);require(rendered.pixels);
        copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion source{output,0,511*16};SDL_GPUTransferBufferLocation destination{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&source,&destination);
        source={static_cast<SDL_GPUBuffer*>(rendered.pixels),0,dust_width*dust_height*4};destination.offset=511*16;
        SDL_DownloadFromGPUBuffer(copy,&source,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        auto* actual=static_cast<const int*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
        std::vector<unsigned> dust_pixels(dust_width*dust_height);
        const auto put_dust=[&](int x,int y,int colour) {
            if(x>=0 && y>=0 && x<settings.viewport[0] && y<192 && (x<80 || x>=100))
                for(unsigned dy=0;dy<dust_scale;++dy) for(unsigned dx=0;dx<dust_scale;++dx)
                    dust_pixels[(unsigned(y)*dust_scale+dy)*dust_width+unsigned(x)*dust_scale+dx]=unsigned(colour)|(1U<<8)|(1U<<26);
        };
        for(unsigned i=0;i<511;++i) {
            const auto transform=[&](const auto& row) {return (dust[i][0]*row[0]+dust[i][1]*row[1]+dust[i][2]*row[2])/32768.;};
            const auto z=transform(settings.row_z);
            std::array<int,4> expected{};
            if(z>=256.) {
                const auto depth=std::min(z,4095.);
                auto x=transform(settings.row_x)*256./depth;
                const auto y=transform(settings.row_y)*256./depth;
                if(settings.eye_x!=0) x+=double(256.f*settings.eye_x)*(1./settings.convergence-1./depth);
                const int sx=settings.viewport[0]/2+settings.viewport[2]+int(std::trunc(x));
                const int sy=settings.viewport[1]/2+settings.viewport[3]+int(std::trunc(y));
                if(sx>=0 && sy>=0 && sx<settings.viewport[0] && sy<settings.viewport[1])
                    expected={sx,sy,int(112+palette[((511-i)&3)*16+unsigned(int(depth)>>8)]),z<1024?3:1};
            }
            for(unsigned a=0;a<4;++a) if(actual[i*4+a]!=expected[a])
                throw std::runtime_error("Dust projection mismatch fixture "+std::to_string(fixture)+" point "+std::to_string(i));
            if(expected[3]) {
                put_dust(expected[0],expected[1],expected[2]);
                if(expected[3]&2) put_dust(expected[0]-1,expected[1]+1,expected[2]);
            }
        }
        for(unsigned i=0;i<dust_pixels.size();++i) require(unsigned(actual[511*4+i])==dust_pixels[i]);
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"9198 dust points: matrix, depth, palette, viewport and stereo parity passed\n";
    for(unsigned frame_index=0;frame_index<6;++frame_index) {
        const bool reduced=frame_index>=3;
        const unsigned out_width=reduced?149:224,out_height=reduced?127:192;
        starfox::render::GpuScene dust_scene;
        starfox::render::DustRenderer::DustFrame frame;
        frame.points={{-32768,0,512},{32767,4,1024},{0,-4,2048}};
        frame.camera.x=frame_index?32767.5:0.125;
        frame.matrix={32767,0,0,0,32767,0,0,0,32767};
        for(unsigned i=0;i<64;++i) frame.colours[i]=std::uint8_t((i*7+i/16)%16);
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        require(projection.enqueue_dust_frame(r.device,command,frame,224,192));
        auto* spans=projection.enqueue_dust_spans(command,out_height,1,1,0,0,
            reduced?std::array<std::uint32_t,3>{224,192,out_width}:std::array<std::uint32_t,3>{});require(spans);
        auto rendered=dust_raster.enqueue_row_spans(r.device,command,spans,3,out_width,out_height,false,nullptr,true);require(rendered.pixels);
        if(frame_index%3==1) {
            const std::array<starfox::render::GpuSceneDraw,1> draws{starfox::render::GpuDustDraw{frame,1,0,512,
                reduced?std::array<std::uint32_t,2>{224,192}:std::array<std::uint32_t,2>{}}};
            rendered=dust_scene.enqueue_batch(r.device,command,out_width,out_height,draws);require(rendered.pixels);
        }
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(rendered.pixels),0,out_width*out_height*4};
        SDL_GPUTransferBufferLocation destination{r.download,0};SDL_DownloadFromGPUBuffer(copy,&source,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* actual=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
        starfox::render::Framebuffer reference_frame(224,192);
        starfox::render::DustRenderer::draw_dust_frame(frame,reference_frame);
        for(unsigned y=0;y<out_height;++y) for(unsigned x=0;x<out_width;++x)
            require((actual[y*out_width+x]&255U)==reference_frame.pixels()[(y*192/out_height)*224+x*224/out_width]);
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"Owned dust upload/wrapping/reuse pixel checks passed\n";
    std::uint32_t random=0x719c1234;
    const auto next=[&](){random=random*1664525U+1013904223U;return word(int(random&65535));};
    std::size_t grid_checked=0;
    starfox::render::GpuRaster grid_raster;
    for(unsigned sample=0;sample<36;++sample) {
        const unsigned scale=1U<<(sample%3),height=224*scale,width=400*scale;
        starfox::render::GridLattice lattice{};
        for(unsigned axis=0;axis<3;++axis) {
            lattice.origin[axis]=std::int16_t(next());
            lattice.x_step[axis]=std::int16_t(next());
            lattice.z_step[axis]=std::int16_t(next());
        }
        // Near-plane gates, companion dots, viewport boundaries and depth
        // clamp transitions must not depend on random fixture coverage.
        if(sample>=32) {
            constexpr std::int16_t depths[]{256,257,511,12287};
            lattice.origin={-200,-112,depths[sample-32]};
            lattice.x_step={32,0,1};lattice.z_step={0,24,1};
        }
        for(float eye:{-3.2f,0.f,3.2f}) {
            auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
            auto* output=static_cast<SDL_GPUBuffer*>(projection.enqueue_grid(r.device,command,lattice,400,224,eye,512));require(output);
            auto* grid_spans=static_cast<SDL_GPUBuffer*>(projection.enqueue_grid_spans(command,height,scale,126,1));require(grid_spans);
            const auto raster=grid_raster.enqueue_row_spans(r.device,command,grid_spans,225,width,height,false,nullptr,true);
            require(raster.pixels);
            auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
            SDL_GPUBufferRegion source{output,0,225*16};
            SDL_GPUTransferBufferLocation destination{r.download,0};
            SDL_DownloadFromGPUBuffer(copy,&source,&destination);
            SDL_GPUBufferRegion span_source{grid_spans,0,225*height*96};
            SDL_GPUTransferBufferLocation span_destination{r.download,225*16};
            SDL_DownloadFromGPUBuffer(copy,&span_source,&span_destination);
            const unsigned pixel_offset=225*16+225*height*96;
            SDL_GPUBufferRegion pixel_source{static_cast<SDL_GPUBuffer*>(raster.pixels),0,width*height*4};
            SDL_GPUTransferBufferLocation pixel_destination{r.download,pixel_offset};
            SDL_DownloadFromGPUBuffer(copy,&pixel_source,&pixel_destination);SDL_EndGPUCopyPass(copy);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
            require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
            auto* actual=static_cast<const int*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
            std::vector<unsigned> expected_pixels(width*height);
            for(unsigned i=0;i<225;++i) {
                int p[3];for(unsigned a=0;a<3;++a)
                    p[a]=word(lattice.origin[a]+lattice.x_step[a]*int(i%15)+lattice.z_step[a]*int(i/15));
                std::array<int,4> expected{0,0,p[2],0};
                if(p[2]>256) {
                    const int depth=std::min(p[2],12287),reciprocal=word((32767*256)/(depth&~1));
                    int x=word(word((p[0]*reciprocal)>>15)+200);
                    const int y=word(word((p[1]*reciprocal)>>15)+112);
                    if(eye!=0) x=int(std::floor(float(x)+256.f*eye*(1.f/512.f-1.f/float(depth))+.5f));
                    if(x>=0 && y>=0 && x<400 && y<224) expected={x,y,p[2],1};
                }
                for(unsigned a=0;a<4;++a) if(actual[i*4+a]!=expected[a])
                    throw std::runtime_error("Grid GPU/reference mismatch at sample "+std::to_string(sample));
                for(unsigned row=0;row<height;++row) {
                    std::array<int,24> span{};
                    const int logical_row=int(row/scale);
                    if(expected[3] && (logical_row==expected[1] || (expected[2]<512 && logical_row==expected[1]+1))) {
                        const int x=(expected[0]-(logical_row!=expected[1]))*int(scale);
                        span[0]=x;span[1]=int(row);span[2]=x+int(scale);span[3]=int(row)+1;
                        span[4]=span[5]=126;span[7]=1;
                        for(int px=std::max(0,x);px<std::min(int(width),x+int(scale));++px)
                            expected_pixels[row*width+unsigned(px)]=126U|(1U<<8)|(1U<<26);
                    }
                    const auto offset=225*4+(i*height+row)*24;
                    for(unsigned a=0;a<24;++a) if(actual[offset+a]!=span[a])
                        throw std::runtime_error("Grid span GPU/reference mismatch");
                }
                ++grid_checked;
            }
            for(unsigned pixel=0;pixel<width*height;++pixel)
                if(unsigned(actual[pixel_offset/4+pixel])!=expected_pixels[pixel])
                    throw std::runtime_error("Grid final raster mismatch at scale "+std::to_string(scale));
            SDL_UnmapGPUTransferBuffer(r.device,r.download);
        }
    }
    std::cout<<"Grid mono/stereo projection records checked: "<<grid_checked
        <<"; row commands and final packed pixels exact at 1x/2x/4x\n";
    for(unsigned fixture=0;fixture<32;++fixture) {
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        starfox::render::GridLattice lattice{{-20,-10,300},{3,0,0},{0,3,20}};
        if(fixture) {
            lattice.origin={std::int16_t(next()%80),std::int16_t(next()%80),std::int16_t(257+fixture*17)};
            lattice.x_step={std::int16_t(next()%9),std::int16_t(next()%9),std::int16_t(next()%30)};
            lattice.z_step={std::int16_t(next()%9),std::int16_t(next()%9),std::int16_t(next()%30)};
        }
        auto* projected=static_cast<SDL_GPUBuffer*>(projection.enqueue_grid(r.device,command,lattice,64,64));require(projected);
        const std::int16_t start[]{std::int16_t(fixture?next():50),std::int16_t(fixture?next():2)};
        auto* spans=projection.enqueue_grid_spans(command,64,1,126,1,start);require(spans);
        const auto raster=grid_raster.enqueue_row_spans(r.device,command,spans,675,64,64,false,nullptr,true);require(raster.pixels);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion src{projected,0,225*16};SDL_GPUTransferBufferLocation dst{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&src,&dst);
        src={static_cast<SDL_GPUBuffer*>(raster.pixels),0,64*64*4};dst.offset=225*16;
        SDL_DownloadFromGPUBuffer(copy,&src,&dst);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* actual=static_cast<const int*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
        std::array<unsigned,64*64> reference{};
        const auto put=[&](int x,int y) {if(x>=0 && y>=0 && x<64 && y<64) reference[unsigned(y*64+x)]=126U|(1U<<8)|(1U<<26);};
        int previous_x=start[0],previous_y=start[1];
        for(unsigned i=0;i<225;++i) {
            const auto* p=actual+i*4;if(!p[3]) continue;
            int x=p[0]-1,y=p[1],dx=x-previous_x,adx=std::abs(dx),ady=std::abs(y-previous_y);
            const int step=y<previous_y?1:-1;
            put(x,y+2);int error=adx,remaining=dx;
            do {put(x-2,y);--x;error-=ady;if(error<0) {y+=step;error+=adx;}--remaining;} while(remaining>=0);
            previous_x=p[0]-1;previous_y=p[1];
            if(p[2]<512) put(p[0]-2,p[1]+1);
        }
        for(unsigned i=0;i<reference.size();++i) require(unsigned(actual[225*4+i])==reference[i]);
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"32 connected grid final-pixel fixtures match source line walk\n";
    {
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        auto* other=SDL_AcquireGPUCommandBuffer(r.device);require(other);
        starfox::render::GridLattice lattice{};
        require(projection.enqueue_grid(r.device,command,lattice,400,224));
        require(!projection.enqueue_grid_spans(other,224,1,126,1));
        require(!projection.enqueue_grid_spans(command,448,1,126,1));
        require(!projection.enqueue_grid(r.device,command,lattice,0,224));
        require(!projection.enqueue_grid_spans(command,224,1,126,1));
        SDL_CancelGPUCommandBuffer(other);SDL_CancelGPUCommandBuffer(command);
        std::cout<<"Grid stale-command, mismatched-height and failed-projection guards passed\n";
    }
    for(unsigned i=0;i<maximum;++i) {
        auto& p=points[i];p.x=next();p.y=next();p.z=word(i);p.vanish_x=next();p.vanish_y=next();
    }
    // Explicit zero, signed minimum and dominant-axis saturation boundaries.
    unsigned corner=0;
    for(int x:{-32768,-16384,-64,0,64,16384,32767})
        for(int y:{-32768,-1,0,1,32767}) for(int z:{-32768,-256,-1,0,1,256,32767})
            points[corner++]={x,y,z,0,32760,-32760,0,0};
    std::size_t checked=0,visible_count=0;
    std::array<NativeTransformPose,pose_count> poses{};
    for(auto& p:poses) {
        for(auto* row:{p.row0,p.row1,p.row2,p.translation,p.vanish})
            for(unsigned c=0;c<4;++c) row[c]=next();
    }
    // Signed-min product overflows the Q15 word; negative products must floor,
    // not truncate toward zero. Include both explicitly along with random poses.
    poses[0].row0[0]=-32768;poses[0].row1[1]=-1;poses[0].row2[2]=32767;
    const auto original_points=points;
    std::vector<NativeTransformVertex> vertices(maximum);
    for(unsigned phase=0;phase<3;++phase) {
    const bool transform=phase!=0;
    if(phase==2) for(unsigned i=0;i<pose_count;++i) {
        constexpr std::uint8_t progress[]{0,1,2,127,255};
        poses[i]=starfox::render::native_explosion_pose(poses[i],int(i)*15-128,127-int(i)*13,
            int(i)*11-100,progress[i%5]);
    }
    points=original_points;
    if(transform) for(unsigned i=0;i<maximum;++i) {
        auto& p=points[i];vertices[i]={p.x,p.y,p.z,i%pose_count};
        if(i%23==0) vertices[i].pose=UINT32_MAX;
        if(vertices[i].pose>=pose_count) {p.reserved=1;continue;}
        const auto& pose=poses[vertices[i].pose];
        const auto q15=[](int a,int b) {return word(int(std::floor(double(word(a))*word(b)/32768.)));};
        int xyz[3];
        for(unsigned c=0;c<3;++c) xyz[c]=word(q15(p.x,pose.row0[c])+q15(p.y,pose.row1[c])
            +q15(p.z,pose.row2[c])+word(pose.translation[c]));
        if(phase==2) {
            int direction[3];
            for(unsigned c=0;c<3;++c) direction[c]=word(q15(-pose.row0[3],pose.row0[c])
                +q15(pose.row1[3],pose.row1[c])+q15(-pose.row2[3],pose.row2[c]));
            direction[1]=-std::abs(direction[1]);
            for(unsigned c=0;c<3;++c) xyz[c]+=int(std::floor(double(direction[c])*pose.vanish[2]/4.));
        }
        p.x=xyz[0];p.y=xyz[1];p.z=xyz[2];p.vanish_x=pose.vanish[0];p.vanish_y=pose.vanish[1];
    }
    for(unsigned count:{1U,63U,64U,65U,maximum,65U}) {
        std::vector<std::array<Uint32,4>> faces(count);
        for(unsigned i=0;i<count;++i) {
            faces[i]={(i*3)%count,(i*11+1)%count,(i*37+2)%count,0};
            if(i%17==0) faces[i][2]=count;
            if(i%19==0) faces[i][0]=UINT32_MAX;
            if(i%23==22) faces[i]={UINT32_MAX,UINT32_MAX,UINT32_MAX,1};
            if(i%7==0) faces[i][3]=3;
        }
        auto* data=SDL_MapGPUTransferBuffer(r.device,r.upload,true);require(data);
        if(transform) std::memcpy(data,vertices.data(),count*sizeof(NativeTransformVertex));
        else std::memcpy(data,points.data(),count*sizeof(NativeProjectionPoint));
        std::memcpy(static_cast<std::uint8_t*>(data)+count*32,faces.data(),count*16);
        std::memcpy(static_cast<std::uint8_t*>(data)+count*48,poses.data(),sizeof(poses));
        SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUTransferBufferLocation from{r.upload,0};
        SDL_GPUBufferRegion to{r.input,0,count*(transform?16U:32U)};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=count*32;to={r.faces,0,count*16};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=count*48;to={r.poses,0,sizeof(poses)};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        void* camera_points=nullptr;
        auto* output=static_cast<SDL_GPUBuffer*>(transform
            ?projection.enqueue_transformed(r.device,command,r.input,count,r.poses,pose_count,&camera_points)
            :projection.enqueue(r.device,command,r.input,count));
        if(!output){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(projection.status());}
        auto* visibility=static_cast<SDL_GPUBuffer*>(projection.enqueue_visibility(command,r.faces,count));
        if(!visibility){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(projection.status());}
        copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion result{output,0,count*16};SDL_GPUTransferBufferLocation destination{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&result,&destination);
        result={visibility,0,count*4};destination.offset=count*16;
        SDL_DownloadFromGPUBuffer(copy,&result,&destination);
        if(transform) {
            result={static_cast<SDL_GPUBuffer*>(camera_points),0,count*32};destination.offset=count*20;
            SDL_DownloadFromGPUBuffer(copy,&result,&destination);
        }
        SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* values=static_cast<const std::array<int,4>*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(values);
        for(unsigned i=0;i<count;++i) if(values[i]!=reference(points[i])) {
            SDL_UnmapGPUTransferBuffer(r.device,r.download);
            throw std::runtime_error("Native projection mismatch at "+std::to_string(i));
        }
        const auto* flags=reinterpret_cast<const Uint32*>(values+count);
        if(transform) {
            const auto* camera=reinterpret_cast<const NativeProjectionPoint*>(reinterpret_cast<const std::uint8_t*>(values)+count*20);
            for(unsigned i=0;i<count;++i) if(camera[i].reserved!=points[i].reserved
                || (!points[i].reserved && (camera[i].x!=points[i].x || camera[i].y!=points[i].y || camera[i].z!=points[i].z)))
                throw std::runtime_error("Native camera/destruction offset mismatch at "+std::to_string(i));
        }
        for(unsigned i=0;i<count;++i) {
            const auto& f=faces[i];bool expected=false;
            if(f[0]<count && f[1]<count && f[2]<count) {
                const auto a=reference(points[f[0]]),b=reference(points[f[1]]),c=reference(points[f[2]]);
                const auto area=std::int64_t(word(b[0]-a[0]))*word(c[1]-a[1])
                    -std::int64_t(word(b[1]-a[1]))*word(c[0]-a[0]);
                const bool behind=(a[2]<0)!=((b[2]<0)!=(c[2]<0));
                expected=a[3]>=0 && b[3]>=0 && c[3]>=0 && ((std::bit_cast<std::int32_t>(std::uint32_t(area))<0)!=behind);
            }
            if(f[3]==1) expected=true;
            if(f[3]==3) expected=f[0]<count && values[f[0]][3]>0;
            if(flags[i]!=unsigned(expected)) {
                SDL_UnmapGPUTransferBuffer(r.device,r.download);
                throw std::runtime_error("Native visibility mismatch at "+std::to_string(i));
            }
            visible_count+=flags[i]!=0;
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);checked+=count;
    }
    }
    if(!visible_count || visible_count==checked) throw std::runtime_error("Visibility fixture did not cover both outcomes");
    std::cout<<projection.status()<<": "<<checked<<" exact points and faces; "<<visible_count
        <<" visible; Q15 transforms/destruction offsets, camera coordinates, all depth words, saturation/seams, invalid poses/indices and allocation reuse passed\n";
    using starfox::render::ContinuousTransformPose;
    using starfox::render::ContinuousTransformVertex;
    using starfox::render::ContinuousProjectedPoint;
    std::array<ContinuousTransformPose,pose_count> continuous_poses{};
    for(unsigned i=0;i<pose_count;++i) {
        auto& p=continuous_poses[i];
        p.row0[0]=1.f;p.row1[1]=1.f;p.row2[2]=1.f;
        // Dyadic coefficients cover fractional scale/shear/rotation without
        // introducing input conversion error into the double oracle.
        if(i>0) {p.row0[1]=float(i)/32;p.row1[0]=-float(i)/32;}
        p.translation[0]=float(i)/8;p.translation[1]=-float(i)/16;
        p.translation[2]=float(i)/4;p.translation[3]=256;
        p.vanish[0]=112.25f;p.vanish[1]=96.125f;
    }
    std::vector<ContinuousTransformVertex> continuous_vertices(maximum);
    for(unsigned i=0;i<maximum;++i) {
        auto& v=continuous_vertices[i];
        v={float(next())/16,float(next())/16,float(next())/32,i%pose_count};
        if(i%23==0) v.pose=UINT32_MAX;
    }
    continuous_vertices[0]={3.25f,-7.125f,0,0};
    continuous_vertices[1]={3.25f,-7.125f,1.f/4096,0};
    continuous_vertices[2]={3.25f,-7.125f,-1.f/4096,0};
    continuous_vertices[3]={std::numeric_limits<float>::infinity(),0,1,0};
    continuous_vertices[4]={std::numeric_limits<float>::quiet_NaN(),0,1,0};
    for(unsigned i=6;i<60;i+=3) {
        const float k=float(i)/8;
        continuous_vertices[i]={1000*k,1001*k,1002*k,0};
        continuous_vertices[i+1]={2000*k,2002*k,2004*k,0};
        continuous_vertices[i+2]={3001*k,17*k,-503*k,0};
        if(i%9==0) continuous_vertices[i+1].z=std::nextafter(continuous_vertices[i+1].z,INFINITY);
        if(i%9==3) continuous_vertices[i+1].z=std::nextafter(continuous_vertices[i+1].z,-INFINITY);
    }
    std::size_t continuous_checked=0;
    std::size_t naive_visibility_mismatches=0;
    double maximum_relative_error=0;
    double maximum_onscreen_error=0;
    for(unsigned count:{1U,63U,64U,65U,maximum,65U}) {
        std::vector<std::array<Uint32,4>> continuous_faces(count);
        for(unsigned i=0;i<count;++i) {
            continuous_faces[i]={(i*3)%count,(i*11+1)%count,(i*37+2)%count,0};
            if(i%17==0) continuous_faces[i][2]=count;
            if(i>=6 && i<60 && i%3==0) continuous_faces[i]={i,i+1,i+2,0};
            if(i%23==22) continuous_faces[i]={UINT32_MAX,UINT32_MAX,UINT32_MAX,1};
            if(i%31==30) {
                continuous_faces[i]={i,i,i,2};
                if(i%62==61) continuous_faces[i][0]=count;
            }
            if(i%7==0) continuous_faces[i][3]=3;
        }
        auto* data=SDL_MapGPUTransferBuffer(r.device,r.upload,true);require(data);
        std::memcpy(data,continuous_vertices.data(),count*16);
        std::memcpy(static_cast<std::uint8_t*>(data)+count*16,continuous_poses.data(),sizeof(continuous_poses));
        std::memcpy(static_cast<std::uint8_t*>(data)+count*16+sizeof(continuous_poses),continuous_faces.data(),count*16);
        SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(r.device);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUTransferBufferLocation from{r.upload,0};SDL_GPUBufferRegion to{r.input,0,count*16};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=count*16;to={r.poses,0,sizeof(continuous_poses)};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=count*16+sizeof(continuous_poses);to={r.faces,0,count*16};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        auto* output=static_cast<SDL_GPUBuffer*>(projection.enqueue_continuous(r.device,command,r.input,count,r.poses,pose_count));
        if(!output) {SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(projection.status());}
        if(projection.enqueue_visibility(command,r.faces,1)) {
            SDL_CancelGPUCommandBuffer(command);throw std::runtime_error("Word visibility accepted continuous geometry");
        }
        auto* visibility=static_cast<SDL_GPUBuffer*>(projection.enqueue_continuous_visibility(command,r.faces,count));
        if(!visibility) {SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(projection.status());}
        copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion result{output,0,count*32};SDL_GPUTransferBufferLocation destination{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&result,&destination);
        result={visibility,0,count*4};destination.offset=count*32;
        SDL_DownloadFromGPUBuffer(copy,&result,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* values=static_cast<const ContinuousProjectedPoint*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(values);
        for(unsigned i=0;i<count;++i) {
            const auto& v=continuous_vertices[i];const auto& actual=values[i];
            if(v.pose>=pose_count || !std::isfinite(v.x)) {
                if(actual.camera[3]!=-1 || actual.screen[3]!=-1)
                    throw std::runtime_error("Continuous invalid input was not rejected");
                continue;
            }
            const auto& p=continuous_poses[v.pose];double camera[3];
            for(unsigned c=0;c<3;++c) camera[c]=double(v.x)*p.row0[c]+double(v.y)*p.row1[c]
                +double(v.z)*p.row2[c]+p.translation[c];
            const double z=camera[2]==0?1:camera[2];
            const double expected[]{camera[0],camera[1],camera[2],1,
                p.vanish[0]+camera[0]*p.translation[3]/z,p.vanish[1]+camera[1]*p.translation[3]/z,
                camera[2],camera[2]>=0?1.:0.};
            for(unsigned c=0;c<8;++c) {
                const double observed=c<4?actual.camera[c]:actual.screen[c-4];
                const double error=std::abs(observed-expected[c])/std::max(1.,std::abs(expected[c]));
                maximum_relative_error=std::max(maximum_relative_error,error);
                if((c==4 || c==5) && std::abs(expected[c])<2048)
                    maximum_onscreen_error=std::max(maximum_onscreen_error,std::abs(observed-expected[c]));
                // Two rounded FP32 operations around vanish-point cancellation
                // need an absolute pixel bound, not only relative output error.
                if(!std::isfinite(observed) || std::abs(observed-expected[c])>0.0001+2e-6*std::abs(expected[c]))
                    throw std::runtime_error("Continuous projection mismatch at "+std::to_string(i)
                        +" component "+std::to_string(c)+" observed "+std::to_string(observed)
                        +" expected "+std::to_string(expected[c]));
            }
        }
        const auto* visibility_flags=reinterpret_cast<const Uint32*>(values+count);
        for(unsigned i=0;i<count;++i) {
            const auto& f=continuous_faces[i];bool expected=false;
            if(f[0]<count && f[1]<count && f[2]<count) {
                const auto* av=values[f[0]].camera;const auto* bv=values[f[1]].camera;const auto* cv=values[f[2]].camera;
                if(av[3]>=0 && bv[3]>=0 && cv[3]>=0) {
                    const double a[]{av[0],av[1],av[2]},b[]{bv[0],bv[1],bv[2]},c[]{cv[0],cv[1],cv[2]};
                    const double determinant=a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])
                        +a[2]*(b[0]*c[1]-b[1]*c[0]);
                    double scale=1;for(auto* vector:{a,b,c}) for(unsigned k=0;k<3;++k) scale=std::max(scale,std::abs(vector[k]));
                    expected=determinant<=scale*scale*scale*1e-12;
                    const float naive=av[0]*(bv[1]*cv[2]-bv[2]*cv[1])-av[1]*(bv[0]*cv[2]-bv[2]*cv[0])
                        +av[2]*(bv[0]*cv[1]-bv[1]*cv[0]);
                    naive_visibility_mismatches+=(double(naive)<=scale*scale*scale*1e-12)!=expected;
                }
            }
            if(f[3]==1) expected=true;
            if(f[3]==3) expected=f[0]<count && values[f[0]].screen[3]>0;
            if(visibility_flags[i]!=unsigned(expected))
                throw std::runtime_error("Continuous visibility mismatch at "+std::to_string(i));
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);continuous_checked+=count;
    }
    std::cout<<"Continuous transforms: "<<continuous_checked<<" points passed; maximum relative error "
        <<maximum_relative_error<<", screen-region absolute error "<<maximum_onscreen_error
        <<" source pixels; fractional poses, zero/near/negative depth, tangent visibility and invalid inputs passed\n";
    if(!naive_visibility_mismatches) throw std::runtime_error("Tangent fixture did not exercise compensated visibility");
    std::cout<<"Compensated visibility avoided "<<naive_visibility_mismatches<<" naive FP32 decisions\n";
    // FACE_B's destruction places an Euler-rotated side exactly at nominal
    // zero depth. Keep a camera-level diagnostic separate from raster coverage.
    {
        starfox::assets::Shape shape;shape.header.shift=4;
        shape.vertices={{30,50,5},{30,-50,5},{30,-50,-5},{30,50,-5}};
        starfox::render::RenderPose pose;pose.yaw=16384;pose.pitch=8192;pose.z=512;pose.continuous_geometry=true;
        auto packed=starfox::render::pack_projection(shape,pose,{});
        std::array<ContinuousTransformPose,6> poses{};
        std::copy(packed.continuous_poses.begin(),packed.continuous_poses.end(),poses.begin());
        poses[0].vanish[2]=poses[1].vanish[2]=5;
        poses[4]=poses[1];poses[5]=poses[3];poses[4].row0[3]=-127;
        poses[4].translation[3]=1;poses[4].vanish[3]=0;
        auto* mapped=SDL_MapGPUTransferBuffer(r.device,r.upload,true);require(mapped);
        std::memcpy(mapped,packed.continuous_vertices.data(),64);
        std::memcpy(static_cast<char*>(mapped)+64,poses.data(),sizeof(poses));SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        auto* cmd=SDL_AcquireGPUCommandBuffer(r.device);require(cmd);
        require(!projection.enqueue_motion(r.device,cmd,r.input,r.poses,65535U*64U+1,2,4));
        auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
        SDL_GPUTransferBufferLocation from{r.upload,0};SDL_GPUBufferRegion to{r.input,0,64};SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=64;to={r.poses,0,sizeof(poses)};SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        void* residual_output=nullptr;
        auto* output=static_cast<SDL_GPUBuffer*>(projection.enqueue_continuous(r.device,cmd,r.input,4,r.poses,6,&residual_output));require(output);require(residual_output);
        copy=SDL_BeginGPUCopyPass(cmd);to={output,0,128};SDL_GPUTransferBufferLocation destination{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&to,&destination);
        to={static_cast<SDL_GPUBuffer*>(residual_output),0,128};destination.offset=128;
        SDL_DownloadFromGPUBuffer(copy,&to,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* actual=static_cast<const ContinuousProjectedPoint*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
        const auto* tails=actual+4;bool retained_tail=false;
        for(unsigned i=0;i<4;++i) {
            const auto& v=shape.vertices[i];double x=v.x*16.,y=v.y*16.,z=v.z*16.;
            const double pitch=8192*(2*std::numbers::pi)/65536.,yaw=16384*(2*std::numbers::pi)/65536.;
            const double z1=y*std::sin(pitch)+z*std::cos(pitch);
            const double z2=-x*std::sin(yaw)+z1*std::cos(yaw);
            const double expected=(z2+512)-32;
            std::cout<<std::setprecision(17)<<"Destruction cancellation vertex "<<i<<": CPU depth "<<expected<<", GPU depth "<<actual[i].camera[2]<<'\n';
            if(actual[i].camera[2]!=float(expected))
                throw std::runtime_error("Destruction zero-depth cancellation lost source rounding");
            const double expected_camera[]{x*std::cos(yaw)+z1*std::sin(yaw),y*std::cos(pitch)-z*std::sin(pitch),expected};
            if(tails[i].camera[3]!=2) throw std::runtime_error("Missing exact screen payload marker");
            for(unsigned c=0;c<3;++c) {
                const double reconstructed=double(actual[i].camera[c])+tails[i].camera[c];
                if(!std::isfinite(reconstructed) || std::abs(reconstructed-expected_camera[c])>1e-9)
                    throw std::runtime_error("Compensated camera residual differs from source");
                retained_tail|=tails[i].camera[c]!=0;
            }
            for(unsigned c=0;c<2;++c) {
                // Preserve project_point's separate double operations, even
                // when cancellation leaves a tiny nonzero projection depth.
                const double numerator=expected_camera[c]*poses[0].translation[3];
                const double quotient=numerator/(expected==0?1:expected);
                const double expected_screen=double(poses[0].vanish[c])+quotient;
                const auto lo=std::bit_cast<std::uint32_t>(tails[i].screen[c*2]);
                const auto hi=std::bit_cast<std::uint32_t>(tails[i].screen[c*2+1]);
                const double reconstructed=std::bit_cast<double>((std::uint64_t(hi)<<32)|lo);
                if(!std::isfinite(reconstructed) || std::abs(reconstructed-expected_screen)
                    >std::max(1.,std::abs(expected_screen))*1e-12)
                    throw std::runtime_error("Destruction screen high/low pair differs from source projection");
            }
        }
        if(!retained_tail) throw std::runtime_error("Residual fixture failed to exercise lost precision");
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    for(bool reset:{false,true}) {
        using Point=starfox::render::ContinuousProjectedPoint;
        std::array<Point,6> now{},old{};
        for(unsigned i=0;i<now.size();++i) {
            now[i]={{0,0,100,1},{24,28,100,1}};
            old[i]={{0,0,100,1},{20,30,100,1}};
        }
        old[1]=now[1]; // valid zero motion must be distinct from invalid
        old[2].camera[2]=-1;
        now[3].screen[0]=std::numeric_limits<float>::quiet_NaN();
        old[4].camera[3]=-1;
        old[5].screen[0]=std::numeric_limits<float>::max(); // scaled overflow
        auto* mapped=static_cast<std::byte*>(SDL_MapGPUTransferBuffer(r.device,r.upload,true));require(mapped);
        std::memcpy(mapped,now.data(),sizeof(now));
        std::memcpy(mapped+sizeof(now),old.data(),sizeof(old));
        SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        auto* cmd=SDL_AcquireGPUCommandBuffer(r.device);require(cmd);
        auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
        SDL_GPUTransferBufferLocation from{r.upload,0};SDL_GPUBufferRegion to{r.input,0,sizeof(now)};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=sizeof(now);to={r.poses,0,sizeof(old)};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        auto* output=static_cast<SDL_GPUBuffer*>(projection.enqueue_motion(r.device,cmd,r.input,r.poses,6,2,4,reset));require(output);
        require(!projection.enqueue_motion(r.device,cmd,output,r.poses,6,2,4));
        copy=SDL_BeginGPUCopyPass(cmd);require(copy);
        to={output,0,6*16};SDL_GPUTransferBufferLocation dest{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&to,&dest);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* result=static_cast<const float*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(result);
        for(unsigned i=0;i<6;++i) {
            const bool valid=!reset && i<2;
            require(result[i*4+3]==(valid?1.f:0.f));
            require(result[i*4+2]==(valid?100.f:0.f));
            require(result[i*4]==(valid && i==0?-8.f:0.f));
            require(result[i*4+1]==(valid && i==0?8.f:0.f));
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"GPU vertex motion: direction, scale, reset, near-plane, NaN, overflow and alias checks pass\n";
    for(unsigned fixture=0;fixture<8;++fixture) {
        starfox::render::GpuProjection::MotionSurfaceSettings s;
        s.width=13;s.height=7;s.reset_history=fixture==6;
        s.current_projection[0]=81;s.current_projection[1]=93;
        s.current_projection[2]=5.75f;s.current_projection[3]=3.125f;
        std::copy_n(s.current_projection,4,s.previous_projection);
        if(fixture!=0) {s.previous_row0[3]=-2;s.previous_row1[3]=1;}
        if(fixture==2) {s.previous_row0[0]=.8f;s.previous_row0[2]=.6f;s.previous_row2[0]=-.6f;s.previous_row2[2]=.8f;}
        if(fixture==3) {s.previous_projection[0]=100;s.previous_projection[2]=7;}
        if(fixture==4) {s.jitter_x=.375f;s.jitter_y=-.125f;}
        if(fixture==5) s.previous_row2[3]=-200;
        if(fixture==7) s.previous_near=105;
        std::array<float,91> depth{};
        for(unsigned i=0;i<depth.size();++i) depth[i]=100.f+float(i)*.25f;
        depth[0]=0;depth[1]=-1;depth[2]=std::numeric_limits<float>::quiet_NaN();
        depth[3]=std::numeric_limits<float>::infinity();
        auto* mapped=SDL_MapGPUTransferBuffer(r.device,r.upload,true);require(mapped);
        std::memcpy(mapped,depth.data(),sizeof(depth));SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        auto* cmd=SDL_AcquireGPUCommandBuffer(r.device);require(cmd);
        auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
        SDL_GPUTransferBufferLocation from{r.upload,0};SDL_GPUBufferRegion to{r.input,0,sizeof(depth)};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        auto* output=static_cast<SDL_GPUBuffer*>(projection.enqueue_motion_surface(r.device,cmd,r.input,s));require(output);
        require(!projection.enqueue_motion_surface(r.device,cmd,output,s));
        auto invalid=s;invalid.current_projection[0]=0;
        require(!projection.enqueue_motion_surface(r.device,cmd,r.input,invalid));
        invalid=s;invalid.width=4097;require(!projection.enqueue_motion_surface(r.device,cmd,r.input,invalid));
        copy=SDL_BeginGPUCopyPass(cmd);require(copy);
        to={output,0,91*16};SDL_GPUTransferBufferLocation dest{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&to,&dest);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);require(fence);
        require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* result=static_cast<const float*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(result);
        for(unsigned i=0;i<depth.size();++i) {
            const double x=double(i%s.width)+.5-s.jitter_x,y=double(i/s.width)+.5-s.jitter_y;
            const double camera[]{(x-s.current_projection[2])/s.current_projection[0]*depth[i],
                (y-s.current_projection[3])/s.current_projection[1]*depth[i],depth[i],1};
            double old[3]{};unsigned row_index=0;
            for(const auto* row:{s.previous_row0,s.previous_row1,s.previous_row2}) {
                for(unsigned j=0;j<4;++j) old[row_index]+=row[j]*camera[j];
                ++row_index;
            }
            const bool valid=i>=4 && !s.reset_history && old[2]>s.previous_near;
            require(result[i*4+3]==float(valid));
            require(result[i*4+2]==(valid?depth[i]:0));
            const double dx=valid?old[0]/old[2]*s.previous_projection[0]+s.previous_projection[2]-x:0;
            const double dy=valid?old[1]/old[2]*s.previous_projection[1]+s.previous_projection[3]-y:0;
            require(std::abs(result[i*4]-dx)<.0001 && std::abs(result[i*4+1]-dy)<.0001);
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"GPU per-pixel motion: 728 perspective, translation, rotation, jitter, invalid-depth and reset samples pass\n";
    projection.release_device();
    for(unsigned fractional=0;fractional<2;++fractional) for(unsigned count:{4U,65U,1024U,65536U,65537U}) for(unsigned variant=0;variant<6;++variant) {
        std::vector<NativeProjectionPoint> camera(count);
        std::vector<std::array<float,3>> xyz(count);
        xyz[0]={16777216,4,-8};xyz[1]={1,2,4};xyz[2]={-16777216,0,1};
        for(unsigned i=3;i<count;++i) for(unsigned c=0;c<3;++c)
            xyz[i][c]=float(int((i*(37+c*12))%65536)-32768)*(fractional?.25f:1.f);
        for(unsigned i=0;i<count;++i) {
            if(variant==5) xyz[i][2]=0;
            if(fractional) {
                camera[i].x=std::bit_cast<int>(xyz[i][0]);camera[i].y=std::bit_cast<int>(xyz[i][1]);camera[i].z=std::bit_cast<int>(xyz[i][2]);camera[i].reserved=std::bit_cast<int>(1.f);
            } else {camera[i].x=int(xyz[i][0]);camera[i].y=int(xyz[i][1]);camera[i].z=int(xyz[i][2]);}
        }
        std::vector<Uint32> indices(count);for(unsigned i=0;i<count;++i) indices[i]=i;
        Uint32 ranges[]{0,count-1,count-1,1};
        if(variant==1) ranges[1]=0;
        if(variant==2) indices[1]=UINT32_MAX;
        if(variant==3) camera[1].reserved=fractional?std::bit_cast<int>(-1.f):1;
        if(variant==4) ranges[0]=count+1;
        auto* mapped=SDL_MapGPUTransferBuffer(r.device,r.upload,true);require(mapped);
        const auto camera_bytes=Uint32(camera.size()*sizeof(camera[0])),index_bytes=Uint32(indices.size()*sizeof(indices[0]));
        std::memcpy(mapped,camera.data(),camera_bytes);std::memcpy(static_cast<char*>(mapped)+camera_bytes,indices.data(),index_bytes);
        SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        auto* cmd=SDL_AcquireGPUCommandBuffer(r.device);require(cmd);auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
        SDL_GPUTransferBufferLocation from{r.upload,0};SDL_GPUBufferRegion to{r.input,0,camera_bytes};SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=camera_bytes;to={r.faces,0,index_bytes};SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        auto* output=static_cast<SDL_GPUBuffer*>(projection.enqueue_axis_points(r.device,cmd,r.input,count,r.faces,count,ranges,fractional!=0));
        if(!output) throw std::runtime_error(projection.status());
        copy=SDL_BeginGPUCopyPass(cmd);to={output,0,64};SDL_GPUTransferBufferLocation destination{r.download,0};
        SDL_DownloadFromGPUBuffer(copy,&to,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);require(fence);require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* result=static_cast<const float*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(result);
        const bool valid=variant==0 || variant==5;
        if(result[3]!=(valid?1.f:-1.f) || result[11]!=1) throw std::runtime_error("Axis group validity mismatch");
        for(unsigned c=0;c<3;++c) {
            if(result[8+c]!=xyz.back()[c]) throw std::runtime_error("Axis singleton mean mismatch");
            double sum=0;for(unsigned i=0;i<count-1;++i) sum+=xyz[i][c];
            const float expected=float(sum/(count-1));
            if(valid && result[c]!=expected) throw std::runtime_error("Axis compensated mean mismatch, count "+std::to_string(count)+" component "+std::to_string(c));
        }
        for(unsigned group=0;group<2;++group) {
            if(group==0 && !valid) {if(result[7]!=-1) throw std::runtime_error("Invalid axis screen accepted");continue;}
            double mean[3]{};
            const unsigned first=group==0?0:count-1,last=group==0?count-1:count;
            for(unsigned i=first;i<last;++i) for(unsigned c=0;c<3;++c) mean[c]+=xyz[i][c];
            for(auto& component:mean) component/=last-first;
            if(result[group*8+7]!=(mean[2]>=0?1.f:0.f)) throw std::runtime_error("Axis projected front flag mismatch");
            for(unsigned c=0;c<2;++c) {
                const double expected=(c==0?112:96)+mean[c]*256/(mean[2]==0?1:mean[2]);
                if(std::abs(double(result[group*8+4+c])-expected)>1e-4+std::abs(expected)*2e-6)
                    throw std::runtime_error("Axis endpoint projection mismatch");
            }
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"Axis GPU reduction/projection: 60 native/fractional cases through 65536-member groups; zero/negative depth, cancellation, singleton, empty and invalid groups passed\n";
    for(unsigned fixture=0;fixture<12;++fixture) {
        const unsigned members=fixture==0?65536U:fixture==1?65537U:2U,count=members+1;
        std::vector<NativeProjectionPoint> points(count);
        std::vector<Uint32> indices(count);for(unsigned i=0;i<count;++i)indices[i]=i;
        for(unsigned i=0;i<members;++i)points[i]={-32768,32767,128,0,0,0,0,0};
        points.back()={32,-16,256,0,0,0,0,0};
        if(fixture>=2){points[0]={-1,1,1,0};points[1]={0,0,0,0};}
        if(fixture==3){points[0].z=-1;points[1].z=0;points.back().z=-1;}
        if(fixture==4){points[0]={-8,4,-8,0};points[1]=points[0];points.back()={8,-4,8,0};}
        if(fixture==5){points[0]={8,-4,8,0};points[1]=points[0];points.back()={-8,4,-8,0};}
        if(fixture==6)indices[0]=count;
        if(fixture==7)points[1].reserved=1;
        if(fixture==8)points[1].x=32768;
        Uint32 ranges[]{0,members,members,1};
        if(fixture==9)ranges[1]=0;
        if(fixture==10)ranges[2]=count;
        auto* mapped=SDL_MapGPUTransferBuffer(r.device,r.upload,true);require(mapped);
        const Uint32 bytes=Uint32(points.size()*32),index_bytes=Uint32(indices.size()*4);
        std::memcpy(mapped,points.data(),bytes);std::memcpy(static_cast<char*>(mapped)+bytes,indices.data(),index_bytes);SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        auto* cmd=SDL_AcquireGPUCommandBuffer(r.device);require(cmd);auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
        SDL_GPUTransferBufferLocation from{r.upload,0};SDL_GPUBufferRegion to{r.input,0,bytes};SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=bytes;to={r.faces,0,index_bytes};SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        auto* output=static_cast<SDL_GPUBuffer*>(projection.enqueue_axis_points(r.device,cmd,r.input,count,r.faces,count,ranges,false,112.5f,-96.5f,256,nullptr,nullptr,true));
        if(!output)throw std::runtime_error(projection.status());
        copy=SDL_BeginGPUCopyPass(cmd);to={output,0,64};SDL_GPUTransferBufferLocation destination{r.download,0};SDL_DownloadFromGPUBuffer(copy,&to,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);require(fence);require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* actual=static_cast<const NativeProjectionPoint*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
        const bool valid=fixture==0 || fixture==2 || fixture==4 || fixture==5 || fixture==11;
        for(unsigned group=0;group<2;++group){
            if(actual[group].reserved!=(valid?0:1))throw std::runtime_error("Native axis validity mismatch fixture "+std::to_string(fixture));
            if(!valid)continue;
            int x,y,z;
            if(group==1){x=32;y=-16;z=256;}else if(fixture==0){x=-32768;y=32767;z=128;}else{x=-1;y=1;z=1;}
            if(fixture==4 || fixture==5){
                const bool clipped=group==(fixture==4?0U:1U);
                x=clipped?0:8;y=clipped?0:-4;z=clipped?0:8;
            }
            if(actual[group].x!=x || actual[group].y!=y || actual[group].z!=z || actual[group].vanish_x!=113 || actual[group].vanish_y!=-97)
                throw std::runtime_error("Native axis camera/rounding mismatch fixture "+std::to_string(fixture));
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"12 native word-axis fixtures: extreme sums, half-away rounding, near clipping, hidden/invalid lists and reuse pass\n";
    for(unsigned variant=0;variant<6;++variant) {
        std::array<ContinuousProjectedPoint,4> points{},tails{};
        double inputExact[4][3]{};
        for(unsigned i=0;i<4;++i) {
            points[i].camera[0]=i<2?256.f:-256.f;points[i].camera[1]=float(i);points[i].camera[2]=512;points[i].camera[3]=1;
            tails[i].camera[0]=variant==1?-1e-6f:1e-6f;tails[i].camera[1]=2e-7f;tails[i].camera[3]=1;
        }
        if(variant==2) tails[0].camera[3]=-1;
        if(variant==3) tails[0].camera[0]=std::numeric_limits<float>::quiet_NaN();
        for(unsigned i=0;i<4;++i)for(unsigned c=0;c<3;++c)
            inputExact[i][c]=double(points[i].camera[c])+tails[i].camera[c];
        if(variant>=4) {
            for(unsigned i=0;i<4;++i) {
                tails[i].camera[3]=3;
                for(unsigned c=0;c<3;++c) {
                    const auto bits=std::bit_cast<std::uint64_t>(inputExact[i][c]);
                    tails[i].camera[c]=std::bit_cast<float>(std::uint32_t(bits));
                    tails[i].screen[c]=std::bit_cast<float>(std::uint32_t(bits>>32));
                }
            }
            if(variant==5)tails[0].screen[0]=std::bit_cast<float>(0x7ff80000U);
        }
        const Uint32 indices[]{0,1,2,3},ranges[]{0,3,3,1};
        auto* mapped=static_cast<char*>(SDL_MapGPUTransferBuffer(r.device,r.upload,true));require(mapped);
        std::memcpy(mapped,points.data(),128);std::memcpy(mapped+128,tails.data(),128);std::memcpy(mapped+256,indices,16);
        SDL_UnmapGPUTransferBuffer(r.device,r.upload);
        auto* cmd=SDL_AcquireGPUCommandBuffer(r.device);require(cmd);auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
        SDL_GPUTransferBufferLocation from{r.upload,0};SDL_GPUBufferRegion to{r.input,0,128};SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=128;to={r.poses,0,128};SDL_UploadToGPUBuffer(copy,&from,&to,true);
        from.offset=256;to={r.faces,0,16};SDL_UploadToGPUBuffer(copy,&from,&to,true);SDL_EndGPUCopyPass(copy);
        void* lows=nullptr;
        auto* output=static_cast<SDL_GPUBuffer*>(projection.enqueue_axis_points(r.device,cmd,r.input,4,r.faces,4,ranges,true,112,96,256,r.poses,&lows));require(output);require(lows);
        copy=SDL_BeginGPUCopyPass(cmd);require(copy);to={output,0,64};SDL_GPUTransferBufferLocation destination{r.download,0};SDL_DownloadFromGPUBuffer(copy,&to,&destination);
        to={static_cast<SDL_GPUBuffer*>(lows),0,64};destination.offset=64;SDL_DownloadFromGPUBuffer(copy,&to,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);require(fence);require(SDL_WaitForGPUFences(r.device,true,&fence,1));SDL_ReleaseGPUFence(r.device,fence);
        const auto* actual=static_cast<const ContinuousProjectedPoint*>(SDL_MapGPUTransferBuffer(r.device,r.download,false));require(actual);
        const auto* low=actual+2;
        for(unsigned group=0;group<2;++group) {
            const bool valid=group==1 || variant<2 || variant==4;
            if(actual[group].camera[3]!=(valid?1.f:-1.f) || low[group].camera[3]!=(valid?2.f:-1.f)) throw std::runtime_error("Residual axis validity mismatch");
            if(!valid) continue;
            double expected[3]{};
            const unsigned first=group==0?0:3,last=group==0?3:4;
            for(unsigned i=first;i<last;++i) for(unsigned c=0;c<3;++c) expected[c]+=inputExact[i][c];
            for(auto& value:expected) value/=last-first;
            for(unsigned c=0;c<3;++c) if(std::abs(double(actual[group].camera[c])+low[group].camera[c]-expected[c])>1e-12)
                throw std::runtime_error("Residual axis mean mismatch");
            for(unsigned c=0;c<2;++c) {
                double screen;
                std::memcpy(&screen,&low[group].screen[c*2],sizeof(screen));
                if(std::abs(screen-((c==0?112:96)+expected[c]*256/expected[2]))>1e-12)
                    throw std::runtime_error("Residual axis binary64 screen mismatch");
            }
        }
        SDL_UnmapGPUTransferBuffer(r.device,r.download);
    }
    std::cout<<"Six residual-aware axis mean/projection and invalid-input fixtures passed\n";
    projection.release_device();
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
