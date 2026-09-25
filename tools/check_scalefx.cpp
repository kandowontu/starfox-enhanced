#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include "starfox/render/gpu_scalefx.hpp"
#include "starfox/render/sdl_gpu_effects.hpp"
#include "starfox/render/scalefx.hpp"
#include "starfox/render/pixel_filter.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <set>
#include <filesystem>
#include <fstream>

namespace {
void require(bool value) { if (!value) throw std::runtime_error(SDL_GetError()); }
using Pixel = std::array<float, 4>;
struct Device {
    SDL_GPUDevice* gpu{};
    SDL_GPUBuffer* buffers[6]{};
    SDL_GPUTexture* image{};
    SDL_GPUTransferBuffer *upload{}, *download{};
    ~Device() {
        if (gpu) {
            SDL_WaitForGPUIdle(gpu);
            for (auto* b : buffers) if (b) SDL_ReleaseGPUBuffer(gpu,b);
            if(image) SDL_ReleaseGPUTexture(gpu,image);
            if (upload) SDL_ReleaseGPUTransferBuffer(gpu,upload);
            if (download) SDL_ReleaseGPUTransferBuffer(gpu,download);
            SDL_DestroyGPUDevice(gpu);
        }
        SDL_Quit();
    }
};
}

int main(int argc, char** argv) try {
    if(argc>2) throw std::runtime_error("Usage: starfox_scalefx_check [reference-fixture-directory]");
    if(argc==2) std::filesystem::create_directories(argv[1]);
    Device d;
    require(SDL_Init(SDL_INIT_VIDEO));
    d.gpu=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_MSL|SDL_GPU_SHADERFORMAT_DXIL,true,nullptr);
    require(d.gpu);
    starfox::render::GpuScaleFx filter;
    starfox::render::ScaleFxScratch cpuScratch;
    starfox::render::RowWorkers workers;workers.set_worker_count(4);
    if(filter.enqueue(nullptr,nullptr,nullptr,17,11).buffer)
        throw std::runtime_error("ScaleFX accepted missing device/input");
    unsigned cases=0;
    for(const auto dimensions: {std::array<Uint32,2>{17,11}, {1,1}, {31,23}, {7,3}, {17,11}, {400,224}}) {
    const auto width=dimensions[0],height=dimensions[1];
    const Uint32 bytes=width*height*sizeof(Pixel);
    for (unsigned i=0;i<1;++i) {
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
            |SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,i==5?bytes*9:bytes,0};
        d.buffers[i]=SDL_CreateGPUBuffer(d.gpu,&info);require(d.buffers[i]);
    }
    SDL_GPUTransferBufferCreateInfo uploadInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes,0};
    SDL_GPUTransferBufferCreateInfo downloadInfo{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes*9,0};
    d.upload=SDL_CreateGPUTransferBuffer(d.gpu,&uploadInfo);require(d.upload);
    d.download=SDL_CreateGPUTransferBuffer(d.gpu,&downloadInfo);require(d.download);
    SDL_GPUTextureCreateInfo textureInfo{};
    textureInfo.type=SDL_GPU_TEXTURETYPE_2D;
    textureInfo.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    textureInfo.usage=SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    textureInfo.width=width;textureInfo.height=height;textureInfo.layer_count_or_depth=1;
    textureInfo.num_levels=1;
    d.image=SDL_CreateGPUTexture(d.gpu,&textureInfo);require(d.image);
    for(const bool textureInput:{false,true}) {
    for (unsigned pattern=0;pattern<6;++pattern) {
        std::vector<Pixel> input(width*height);
        for (unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const bool lit=pattern==0 || (pattern==1?x>y:pattern==2?(x+y)%2==0:x==width-1);
            input[y*width+x]=lit?Pixel{64.F/255,128.F/255,192.F/255,1}:Pixel{0,0,0,1};
            if(pattern==4) input[y*width+x]={float((x*71+y*13)%256)/255,
                float((x*19+y*97)%256)/255,float((x*53+y*47)%256)/255,1};
            if(pattern==5 && (x+y)%3==0) input[y*width+x]={0,0,0,0};
        }
        const auto packed=[](const Pixel& p) {
            const auto byte=[&](unsigned i){return std::uint32_t(p[i]*255.F+.5F);};
            return (byte(3)<<24)|(byte(0)<<16)|(byte(1)<<8)|byte(2);
        };
        std::vector<std::uint32_t> cpuInput(input.size()),cpuOutput(input.size()*9);
        for(unsigned i=0;i<input.size();++i) cpuInput[i]=packed(input[i]);
        starfox::render::scale_scalefx(cpuInput,cpuOutput,width,height,cpuScratch,workers);
        if(argc==2 && !textureInput) {
            const auto stem=std::to_string(width)+"x"+std::to_string(height)+"-"+std::to_string(pattern);
            const auto dump=[&](const char* suffix,const auto& pixels) {
                std::ofstream file(std::filesystem::path(argv[1])/(stem+suffix),std::ios::binary);
                for(const auto pixel:pixels) {
                    const char rgba[]{char(pixel>>16),char(pixel>>8),char(pixel),char(pixel>>24)};
                    file.write(rgba,4);
                }
                if(!file) throw std::runtime_error("Cannot write ScaleFX reference fixture");
            };
            dump("-input.rgba",cpuInput);dump("-port.rgba",cpuOutput);
            for(unsigned stage=0;stage<4;++stage) {
                std::ofstream file(std::filesystem::path(argv[1])/(stem+"-pass"+std::to_string(stage)+".f32"),std::ios::binary);
                file.write(reinterpret_cast<const char*>(cpuScratch.passes[stage].data()),
                    static_cast<std::streamsize>(cpuScratch.passes[stage].size()*sizeof(Pixel)));
                if(!file) throw std::runtime_error("Cannot write ScaleFX intermediate fixture");
            }
        }
        auto* mapped=SDL_MapGPUTransferBuffer(d.gpu,d.upload,false);require(mapped);
        std::memcpy(mapped,input.data(),bytes);SDL_UnmapGPUTransferBuffer(d.gpu,d.upload);
        auto* command=SDL_AcquireGPUCommandBuffer(d.gpu);require(command);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUTransferBufferLocation from{d.upload,0};SDL_GPUBufferRegion to{d.buffers[0],0,bytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);
        SDL_GPUTextureTransferInfo textureSource{d.upload,0,width,height};
        SDL_GPUTextureRegion textureDestination{};
        textureDestination.texture=d.image;textureDestination.w=width;textureDestination.h=height;
        textureDestination.d=1;
        SDL_UploadToGPUTexture(copy,&textureSource,&textureDestination,false);
        SDL_EndGPUCopyPass(copy);
        const auto filtered=textureInput
            ?filter.enqueue_texture(d.gpu,command,d.image,width,height)
            :filter.enqueue(d.gpu,command,d.buffers[0],width,height);
        if(!filtered.buffer) {
            SDL_CancelGPUCommandBuffer(command);
            throw std::runtime_error(filter.status());
        }
        if(filtered.width!=width*3 || filtered.height!=height*3)
            throw std::runtime_error("ScaleFX output extent mismatch");
        copy=SDL_BeginGPUCopyPass(command);require(copy);
        SDL_GPUBufferRegion result{static_cast<SDL_GPUBuffer*>(filtered.buffer),0,bytes*9};
        SDL_GPUTransferBufferLocation destination{d.download,0};
        SDL_DownloadFromGPUBuffer(copy,&result,&destination);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
        const bool waited=SDL_WaitForGPUFences(d.gpu,true,&fence,1);
        SDL_ReleaseGPUFence(d.gpu,fence);require(waited);
        const auto* pixels=static_cast<const Pixel*>(SDL_MapGPUTransferBuffer(d.gpu,d.download,false));require(pixels);
        bool valid=true;
        const std::set<Pixel> existingColours(input.begin(),input.end());
        unsigned reconstructed=0;
        for (unsigned y=0;y<height*3;++y) for(unsigned x=0;x<width*3;++x) {
            const auto& p=pixels[y*width*3+x];
            if(packed(p)!=cpuOutput[y*width*3+x]) {
                SDL_UnmapGPUTransferBuffer(d.gpu,d.download);
                throw std::runtime_error("ScaleFX CPU/GPU mismatch, pattern "+std::to_string(pattern)
                    +" at "+std::to_string(x)+","+std::to_string(y));
            }
            reconstructed += p != input[(y/3)*width+x/3];
            // ScaleFX must only select existing colours, preserve every central
            // subpixel and leave a constant image unchanged, including edges.
            const bool existing=existingColours.contains(p);
            valid=valid && existing;
            if ((x%3==1 && y%3==1) || pattern==0)
                valid=valid && p==input[(y/3)*width+x/3];
        }
        SDL_UnmapGPUTransferBuffer(d.gpu,d.download);
        if(!valid) throw std::runtime_error("ScaleFX colour/centre invariant failed");
        if(pattern==1 && width>4 && height>4 && reconstructed==0)
            throw std::runtime_error("ScaleFX diagonal remained nearest-neighbour");
        std::cout<<"Pattern "<<pattern<<": "<<reconstructed<<" reconstructed subpixels\n";
        ++cases;
    }
    }
    SDL_ReleaseGPUTexture(d.gpu,d.image);d.image=nullptr;
    SDL_ReleaseGPUBuffer(d.gpu,d.buffers[0]);d.buffers[0]=nullptr;
    SDL_ReleaseGPUTransferBuffer(d.gpu,d.upload);d.upload=nullptr;
    SDL_ReleaseGPUTransferBuffer(d.gpu,d.download);d.download=nullptr;
    }
    starfox::render::SdlGpuEffects effects;
    for(unsigned scale:{1U,2U,3U,4U}) {
        using namespace starfox::render;
        Framebuffer frame(17,11,scale);frame.enable_layer_tags(true);
        Palette256 palette{};palette[0]={0,0,0,255};palette[1]={64,128,192,255};
        palette[2]={211,97,23,255};
        frame.set_layer_override(PixelLayer::textured_geometry);
        for(int y=0;y<11;++y) for(int x=0;x<17;++x)
            frame.set(x,y,x>y?1:0);
        frame.set_layer_override(PixelLayer::three_d);
        frame.set(9,5,2);
        frame.clear_layer_override();
        std::vector<std::uint8_t> rgba;expand_rgba(frame,rgba,palette);
        const auto before=rgba;
        auto cpuRgba=rgba;
        PixelFilterScratch filterScratch;
        apply_two_d_filter(TwoDFilter::scalefx,frame,palette,cpuRgba,filterScratch,workers);
        GpuEffectSettings settings;settings.filter=5;
        if(!effects.apply(d.gpu,frame,rgba,settings)) throw std::runtime_error(effects.status());
        if(cpuRgba!=rgba) throw std::runtime_error("ScaleFX CPU/GPU masked composition mismatch");
        unsigned changed=0;
        for(unsigned y=0;y<frame.stored_height();++y) for(unsigned x=0;x<frame.stored_width();++x) {
            const auto i=(y*frame.stored_width()+x)*4;
            bool equal=true;
            for(unsigned c=0;c<4;++c) equal=equal && rgba[i+c]==before[i+c];
            if(x/scale==9 && y/scale==5 && !equal)
                throw std::runtime_error("ScaleFX changed untextured geometry coverage");
            changed+=!equal;
        }
        if(!changed) throw std::runtime_error("ScaleFX effects integration did not filter diagonal art");
    }
    std::cout<<"ScaleFX effects integration passed at 1x/2x/3x/4x.\n";
    std::cout<<"ScaleFX GPU five-pass invariants passed: "<<cases<<" cases ("
        <<SDL_GetGPUDeviceDriver(d.gpu)<<"). Not a reference-parity proof.\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
