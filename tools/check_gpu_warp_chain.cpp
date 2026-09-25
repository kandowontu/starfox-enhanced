#include <SDL3/SDL.h>
#include "starfox/render/gpu_colour_warp.hpp"
#include "starfox/render/gpu_ray_geometry.hpp"
#include "starfox/render/ray_materials.hpp"
#include "starfox/render/raster_commands.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using U=Uint32;using Four=std::array<U,4>;
using namespace starfox::render;
void check(bool v){if(!v)throw std::runtime_error(SDL_GetError());}
int main()try {
    check(SDL_Init(SDL_INIT_VIDEO));auto* device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_MSL,true,nullptr);check(device);
    GpuColourWarp warp,reflection_warp;GpuRayGeometry rays,mapped_rays;
    RasterCommand canonical_reference{};
    std::array<Four,4> topology{};
    SDL_GPUBufferCreateInfo topology_info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,64,0};
    auto* topology_buffer=SDL_CreateGPUBuffer(device,&topology_info);check(topology_buffer);
    std::array<U,4> order{0,0,0,0};std::array<U,2> traversal{4,0};
    Four polygon{0,3,0,0};std::array<Four,3> corners{{{5,0,0,0},{7,0,0,0},{9,0,0,0}}};U visible=1;
    RasterCommand material{};material.has_surface=1;material.surface={0,1,0,100};
    Four normal{};std::array<unsigned char,2480> diffuse{};std::array<unsigned char,128> depth{};
    std::vector<U> lookup(65536,UINT32_MAX);Four texture{};std::array<U,8> coordinates{};
    const std::array<U,12> sizes{16,8,16,48,4,96,16,2480,128,262144,16,32};
    std::array<SDL_GPUBuffer*,12> buffers{};U total=0;
    for(U i=0;i<12;++i){SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};buffers[i]=SDL_CreateGPUBuffer(device,&info);check(buffers[i]);total+=sizes[i];}
    SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,total+64,0};auto* upload=SDL_CreateGPUTransferBuffer(device,&ti);check(upload);
    ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;ti.size=64+2048+384+8+256;auto* download=SDL_CreateGPUTransferBuffer(device,&ti);check(download);
    GpuWarpInputs inputs{buffers[0],buffers[1],buffers[2],buffers[3],buffers[4],buffers[5],buffers[6],buffers[7],buffers[8],buffers[9],buffers[10],buffers[11]};
    GpuWarpSettings settings{};settings.capacity=4;settings.face_count=1;settings.visibility_count=1;settings.corner_count=3;settings.colour_base=128;settings.flags=1;settings.override_colour=143;
    for(U fixture=0;fixture<11;++fixture){
        settings.flags=fixture>=8?0U:1U;settings.seed=12345;
        topology={{{0,1,2,0},{32,33,34,1},{64,65,66,2},{96,97,98,3}}};
        if(fixture>=9) topology={{{0,1,2,0x80000000U},{0,1,2,0x80000001U},{0,1,33,0x80000000U},{0,1,2,0x80000000U}}};
        traversal={fixture==1?1U:fixture==2?0U:4U,fixture==3?1U:0U};visible=fixture==4 || fixture==10?0U:1U;order[1]=fixture==5?1U:0U;
        const void* data[]{order.data(),traversal.data(),polygon.data(),corners.data(),&visible,&material,normal.data(),diffuse.data(),depth.data(),lookup.data(),texture.data(),coordinates.data()};
        auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,true));check(mapped);U offset=0;
        for(U i=0;i<12;++i){std::memcpy(mapped+offset,data[i],sizes[i]);offset+=sizes[i];}
        std::memcpy(mapped+offset,topology.data(),64);SDL_UnmapGPUTransferBuffer(device,upload);
        auto* command=SDL_AcquireGPUCommandBuffer(device);check(command);auto* copy=SDL_BeginGPUCopyPass(command);check(copy);offset=0;
        for(U i=0;i<12;++i){SDL_GPUTransferBufferLocation from{upload,offset};SDL_GPUBufferRegion to{buffers[i],0,sizes[i]};SDL_UploadToGPUBuffer(copy,&from,&to,true);offset+=sizes[i];}
        SDL_GPUTransferBufferLocation topology_from{upload,offset};SDL_GPUBufferRegion topology_to{topology_buffer,0,64};
        SDL_UploadToGPUBuffer(copy,&topology_from,&topology_to,true);SDL_EndGPUCopyPass(copy);
        auto output=warp.enqueue(device,command,inputs,settings);if(!output.polygons)throw std::runtime_error(warp.status());
        auto* ray_materials=rays.enqueue_materials(device,command,topology_buffer,output.corners,output.polygons,output.materials,4,128,4,0);
        if(!ray_materials)throw std::runtime_error(rays.status());
        if(fixture==6){check(SDL_CancelGPUCommandBuffer(command));continue;}
        copy=SDL_BeginGPUCopyPass(command);check(copy);offset=0;
        void* outputs[]{output.polygons,output.corners,output.materials,output.result,ray_materials};const U lengths[]{64,2048,384,8,256};
        for(U i=0;i<5;++i){SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(outputs[i]),0,lengths[i]};SDL_GPUTransferBufferLocation to{download,offset};SDL_DownloadFromGPUBuffer(copy,&from,&to);offset+=lengths[i];}SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence);check(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        auto* raw=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(device,download,false));check(raw);
        auto* polys=reinterpret_cast<const Four*>(raw);auto* verts=reinterpret_cast<const Four*>(raw+64);auto* mats=reinterpret_cast<const RasterCommand*>(raw+2112);auto* result=reinterpret_cast<const U*>(raw+2496);
        const bool failed=fixture==3 || fixture==5;
        const auto* reflected=reinterpret_cast<const RayMaterial*>(raw+2504);
        if(result[0]!=(failed?0U:traversal[0]) || result[1]!=U(failed))throw std::runtime_error("chain status mismatch");
        for(U i=0;i<4;++i){bool valid=!failed && visible && i<traversal[0];
            const bool ray_valid=valid && (fixture<9 || i==0 || i==3);
            const U selected=fixture>=9?3:i;
            if(reflected[i].reserved!=U(!ray_valid))throw std::runtime_error("warp ray material validity mismatch");
            if(ray_valid && (reflected[i].even!=mats[selected].even || reflected[i].odd!=mats[selected].odd || reflected[i].textured || reflected[i].face!=selected))
                throw std::runtime_error("warp reflection lost occurrence material");
            if(!valid){if(polys[i]!=Four{} || mats[i].has_surface)throw std::runtime_error("chain stale output");continue;}
            if(polys[i]!=Four{i*32,3,0,0} || (fixture<8 && (mats[i].even!=143 || mats[i].odd!=143)) || mats[i].textured || mats[i].surface!=material.surface)throw std::runtime_error("chain material mismatch");
            for(U c=0;c<3;++c){auto expected=corners[c];expected[3]=1;if(verts[i*32+c]!=expected)throw std::runtime_error("chain corner/source-face mismatch");}
        }
        if(fixture==8 && reflected[0].even==reflected[1].even && reflected[0].odd==reflected[1].odd)
            throw std::runtime_error("random fixture did not distinguish duplicate-face occurrences");
        std::array<RasterCommand,4> visible_reference{};
        std::memcpy(visible_reference.data(),mats,sizeof(visible_reference));
        SDL_UnmapGPUTransferBuffer(device,download);
        // The reflection extension must not perturb a single visible command.
        auto reflection_settings=settings;reflection_settings.reflection_materials=true;
        command=SDL_AcquireGPUCommandBuffer(device);check(command);
        const std::array<Four,4> reflection_topology{{{0,1,2,0x80000000U},{0,1,2,0x80000000U},{0,1,2,0x80000000U},{0,1,2,0x80000000U}}};
        mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,true));check(mapped);
        std::memcpy(mapped+total,reflection_topology.data(),64);SDL_UnmapGPUTransferBuffer(device,upload);
        copy=SDL_BeginGPUCopyPass(command);check(copy);
        SDL_GPUTransferBufferLocation reflection_from{upload,total};SDL_GPUBufferRegion reflection_to{topology_buffer,0,64};
        SDL_UploadToGPUBuffer(copy,&reflection_from,&reflection_to,true);SDL_EndGPUCopyPass(copy);
        auto extended=reflection_warp.enqueue(device,command,inputs,reflection_settings);
        if(!extended.polygons)throw std::runtime_error(reflection_warp.status());
        auto* extended_rays=rays.enqueue_materials(device,command,topology_buffer,extended.corners,extended.polygons,extended.materials,4,160,5,0);
        if(!extended_rays)throw std::runtime_error(rays.status());
        check(extended.face_lookup);
        const GpuRayMaterialLookup lookup{extended.face_lookup,1};
        auto* mapped_materials=mapped_rays.enqueue_materials(device,command,topology_buffer,extended.corners,extended.polygons,extended.materials,4,160,5,0,0,nullptr,false,&lookup);
        if(!mapped_materials)throw std::runtime_error(mapped_rays.status());
        copy=SDL_BeginGPUCopyPass(command);check(copy);
        void* extended_outputs[]{extended.polygons,extended.materials,extended.result,extended_rays,mapped_materials};
        const U extended_lengths[]{80,480,8,256,256};offset=0;
        for(U i=0;i<5;++i){SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(extended_outputs[i]),0,extended_lengths[i]};
            SDL_GPUTransferBufferLocation to{download,offset};SDL_DownloadFromGPUBuffer(copy,&from,&to);offset+=extended_lengths[i];}
        SDL_EndGPUCopyPass(copy);
        fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence);check(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
        raw=static_cast<const unsigned char*>(SDL_MapGPUTransferBuffer(device,download,false));check(raw);
        polys=reinterpret_cast<const Four*>(raw);mats=reinterpret_cast<const RasterCommand*>(raw+80);result=reinterpret_cast<const U*>(raw+560);
        if(result[0]!=(failed?0U:1+traversal[0]) || result[1]!=U(failed))throw std::runtime_error("reflection extension status mismatch");
        if(!failed && (polys[0]!=Four{0,3,0,0} || !mats[0].has_surface))throw std::runtime_error("hidden face has no reflection material");
        if(std::memcmp(mats+1,visible_reference.data(),sizeof(visible_reference))!=0)throw std::runtime_error("reflection extension changed visible random stream");
        if(fixture==8)canonical_reference=mats[0];
        if(fixture>=9 && std::memcmp(mats,&canonical_reference,sizeof(canonical_reference))!=0)throw std::runtime_error("hidden canonical material depends on visibility");
        const auto* extended_materials=reinterpret_cast<const RayMaterial*>(raw+568);
        if(std::memcmp(raw+568,raw+824,256)!=0)throw std::runtime_error("cached source-face lookup differs from full occurrence search");
        const U chosen=visible?traversal[0]:0U;
        for(U i=0;i<4;++i) {
            if(extended_materials[i].reserved!=U(failed))throw std::runtime_error("reflection extension retained a hidden-face hole");
            if(!failed && (extended_materials[i].face!=chosen || extended_materials[i].even!=mats[chosen].even
                || extended_materials[i].odd!=mats[chosen].odd))throw std::runtime_error("reflection extension did not prefer visible occurrence");
        }
        SDL_UnmapGPUTransferBuffer(device,download);
    }
    mapped_rays.release_device();rays.release_device();reflection_warp.release_device();warp.release_device();SDL_ReleaseGPUBuffer(device,topology_buffer);SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,download);for(auto* b:buffers)SDL_ReleaseGPUBuffer(device,b);SDL_DestroyGPUDevice(device);SDL_Quit();
    std::cout<<"Resident warp chain and ray materials: duplicate-face random colours, short/empty/hidden/invalid lists, cancellation and reuse pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
