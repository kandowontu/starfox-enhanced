#include <SDL3/SDL.h>
#include "starfox/render/raster_commands.hpp"
#include "../src/render/shaders/generated/warp_expand_portable.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using U=Uint32;using Four=std::array<U,4>;using Command=starfox::render::RasterCommand;
void check(bool v){if(!v)throw std::runtime_error(SDL_GetError());}
int main()try{
    check(SDL_Init(SDL_INIT_VIDEO));auto* d=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,true,nullptr);check(d);
    SDL_GPUComputePipelineCreateInfo pi{};pi.code=starfox::render::warp_expand_shader::spirv;pi.code_size=sizeof(starfox::render::warp_expand_shader::spirv);
    pi.format=SDL_GPU_SHADERFORMAT_SPIRV;pi.entrypoint="main";pi.num_readonly_storage_buffers=8;pi.num_readwrite_storage_buffers=3;pi.num_uniform_buffers=1;pi.threadcount_x=32;pi.threadcount_y=pi.threadcount_z=1;
    auto* pipeline=SDL_CreateGPUComputePipeline(d,&pi);check(pipeline);
    std::array<U,4> order{0,0,0,0};
    std::array<U,2> traversal{4,0};std::array<Four,1> sourcePolygons{{{0,3,7,0}}};
    std::array<Four,3> sourceCorners{{{3,0,0,0},{8,0,0,0},{5,0,0,0}}};
    std::array<Command,1> sourceMaterials{};sourceMaterials[0].has_surface=1;sourceMaterials[0].surface={0,1,0,128};sourceMaterials[0].reserved1=2;
    std::array<Four,4> decoded{{{129,130,1,0},{131,131,0,1},{0,0,0,0xfffffffeU},{132,133,1,UINT32_MAX}}};
    std::array<Four,2> textures{{{0,7,7,0},{64,15,15,4}}};
    std::array<std::array<U,2>,8> uv{{{1,2},{3,4},{5,6},{7,0},{10,11},{12,13},{14,15},{0,1}}};
    const std::array<U,11> sizes{16,8,16,48,96,64,32,64,64,4*32*16,4*96};
    std::array<SDL_GPUBuffer*,11> buffers{};U uploadSize=0,downloadSize=0;
    for(U i=0;i<11;++i){SDL_GPUBufferCreateInfo bi{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,sizes[i],0};buffers[i]=SDL_CreateGPUBuffer(d,&bi);check(buffers[i]);if(i<8)uploadSize+=sizes[i];else downloadSize+=sizes[i];}
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,uploadSize,0};auto* upload=SDL_CreateGPUTransferBuffer(d,&ti);check(upload);ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;ti.size=downloadSize;auto* download=SDL_CreateGPUTransferBuffer(d,&ti);check(download);
    for(U fixture=0;fixture<5;++fixture){
        textures[1][3]=fixture==1?7:4;traversal[1]=fixture==2?1:0;order[1]=fixture==3?1:0;sourcePolygons[0][1]=fixture==4?33:3;
        const void* data[]{order.data(),traversal.data(),sourcePolygons.data(),sourceCorners.data(),sourceMaterials.data(),decoded.data(),textures.data(),uv.data()};
        auto* p=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(d,upload,false));check(p);U offset=0;for(U i=0;i<8;++i){std::memcpy(p+offset,data[i],sizes[i]);offset+=sizes[i];}SDL_UnmapGPUTransferBuffer(d,upload);
        auto* command=SDL_AcquireGPUCommandBuffer(d);check(command);auto* copy=SDL_BeginGPUCopyPass(command);check(copy);offset=0;
        for(U i=0;i<8;++i){SDL_GPUTransferBufferLocation from{upload,offset};SDL_GPUBufferRegion to{buffers[i],0,sizes[i]};SDL_UploadToGPUBuffer(copy,&from,&to,true);offset+=sizes[i];}SDL_EndGPUCopyPass(copy);
        const std::array<U,8> settings{4,1,3,2,8,128,U(-3),7};SDL_PushGPUComputeUniformData(command,0,settings.data(),sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding outputs[3]{};for(U i=0;i<3;++i)outputs[i].buffer=buffers[8+i];
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,outputs,3);check(pass);SDL_BindGPUComputePipeline(pass,pipeline);SDL_BindGPUComputeStorageBuffers(pass,0,buffers.data(),8);SDL_DispatchGPUCompute(pass,1,1,1);SDL_EndGPUComputePass(pass);
        copy=SDL_BeginGPUCopyPass(command);check(copy);offset=0;for(U i=8;i<11;++i){SDL_GPUBufferRegion from{buffers[i],0,sizes[i]};SDL_GPUTransferBufferLocation to{download,offset};SDL_DownloadFromGPUBuffer(copy,&from,&to);offset+=sizes[i];}SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence);check(SDL_WaitForGPUFences(d,true,&fence,1));SDL_ReleaseGPUFence(d,fence);
        auto* raw=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(d,download,false));check(raw);
        auto* polys=reinterpret_cast<const Four*>(raw);auto* corners=reinterpret_cast<const Four*>(raw+64);auto* materials=reinterpret_cast<const Command*>(raw+64+2048);
        for(U slot=0;slot<4;++slot){bool valid=fixture!=2 && fixture!=4 && slot!=2 && !(slot==1 && (fixture==1 || fixture==3));if(!valid){if(polys[slot][1]!=0 || materials[slot].has_surface!=0)throw std::runtime_error("invalid occurrence retained stale output");continue;}
            const bool textured=slot<2;if(polys[slot]!=Four{slot*32,3,7,U(textured)})throw std::runtime_error("polygon expansion mismatch");
            for(U c=0;c<3;++c){auto coord=textured?uv[slot*4+c]:std::array<U,2>{0,0};if(corners[slot*32+c]!=Four{sourceCorners[c][0],coord[0],coord[1],1})throw std::runtime_error("UV/source-face expansion mismatch");}
            const auto& m=materials[slot];if(m.even!=decoded[slot][0] || m.odd!=decoded[slot][1] || m.tag!=(textured?4U:0U) || m.surface!=sourceMaterials[0].surface || m.has_surface!=1 || m.textured!=U(textured))throw std::runtime_error("material expansion mismatch");
            if(textured && (m.texture_offset!=textures[slot][0] || m.reserved0!=U(-3) || m.reserved1!=7))throw std::runtime_error("texture scroll mismatch");}
        SDL_UnmapGPUTransferBuffer(d,download);
    }
    SDL_ReleaseGPUTransferBuffer(d,upload);SDL_ReleaseGPUTransferBuffer(d,download);for(auto* b:buffers)SDL_ReleaseGPUBuffer(d,b);SDL_ReleaseGPUComputePipeline(d,pipeline);SDL_DestroyGPUDevice(d);SDL_Quit();std::cout<<"Repeated-face materials/UVs, hidden draws and invalid-range reuse pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
