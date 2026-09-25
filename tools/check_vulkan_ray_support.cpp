#include "starfox/render/vulkan_ray_support.hpp"
#include "starfox/render/vulkan_hardware_rt.hpp"
#include "starfox/render/environment_effects.hpp"

#include <SDL3/SDL.h>

#include <iostream>
#include <stdexcept>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

int main() {
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr<<"SDL video: "<<SDL_GetError()<<'\n';
        return 1;
    }
    const auto props=SDL_CreateProperties();
    if(!props) {
        std::cerr<<"SDL properties: "<<SDL_GetError()<<'\n';
        SDL_Quit();
        return 1;
    }
    SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,"vulkan");
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(props,
        SDL_PROP_GPU_DEVICE_CREATE_VULKAN_REQUIRE_HARDWARE_ACCELERATION_BOOLEAN,true);
    const bool requested=starfox::render::shadows::request_vulkan_ray_query(props);
    auto* device=requested?SDL_CreateGPUDeviceWithProperties(props):nullptr;
    const bool enabled=device!=nullptr;
    if(!device) {
        std::cerr<<"Vulkan ray-query device: "<<SDL_GetError()<<'\n';
        SDL_ClearProperty(props,SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER);
        device=SDL_CreateGPUDeviceWithProperties(props);
    }
    SDL_DestroyProperties(props);
    if(!device) {
        std::cerr<<"Vulkan fallback device: "<<SDL_GetError()<<'\n';
        SDL_Quit();
        return 1;
    }
    SDL_SetBooleanProperty(SDL_GetGPUDeviceProperties(device),
        "starfox.vulkan.ray_query.enabled",enabled);
    const auto support=starfox::render::shadows::query_vulkan_ray_query(device);
    std::cout<<support.status<<'\n';
    bool dispatched=false;
    if(support.available) {
        using namespace starfox::render::shadows;
        Scene scene;
        scene.add({{-50,-50,50},{50,-50,50},{0,50,50}});
        scene.add({{-100,-100,100},{100,-100,100},{0,100,100}});
        scene.build();
        VulkanHardwareRt rays;
        const Camera camera{64,64,64,32,32};
        dispatched=rays.render_shadows(device,scene,camera,{0,0,1},{});
        if(dispatched) {
            // Exercise the same GPU-resident geometry copy used by gameplay,
            // not only the CPU-upload fallback.
            const std::array<std::array<float,4>,6> vertices{{
                {-50,-50,50,0},{50,-50,50,0},{0,50,50,0},
                {-100,-100,100,0},{100,-100,100,0},{0,100,100,0}}};
            SDL_GPUBufferCreateInfo geometry_info{
                SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
                unsigned(sizeof(vertices)+2*sizeof(starfox::render::RayMaterial)),0};
            auto* geometry=SDL_CreateGPUBuffer(device,&geometry_info);
            SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                unsigned(sizeof(vertices)),0};
            auto* upload=SDL_CreateGPUTransferBuffer(device,&upload_info);
            if(!geometry || !upload) throw std::runtime_error(SDL_GetError());
            void* mapped=SDL_MapGPUTransferBuffer(device,upload,false);
            if(!mapped) throw std::runtime_error(SDL_GetError());
            std::memcpy(mapped,vertices.data(),sizeof(vertices));
            SDL_UnmapGPUTransferBuffer(device,upload);
            auto* upload_command=SDL_AcquireGPUCommandBuffer(device);
            auto* upload_pass=SDL_BeginGPUCopyPass(upload_command);
            SDL_GPUTransferBufferLocation upload_from{upload,0};
            SDL_GPUBufferRegion upload_to{geometry,0,unsigned(sizeof(vertices))};
            SDL_UploadToGPUBuffer(upload_pass,&upload_from,&upload_to,false);
            SDL_EndGPUCopyPass(upload_pass);
            auto* upload_fence=SDL_SubmitGPUCommandBufferAndAcquireFence(upload_command);
            if(!upload_fence || !SDL_WaitForGPUFences(device,true,&upload_fence,1))
                throw std::runtime_error(SDL_GetError());
            SDL_ReleaseGPUFence(device,upload_fence);
            SDL_ReleaseGPUTransferBuffer(device,upload);
            const starfox::render::GpuScene::RayGeometryOutput resident{
                device,geometry,6,true,nullptr,0};
            const Scene empty;
            dispatched=rays.render_shadows(device,empty,camera,{0,0,1},{},&resident);
            if(!dispatched) throw std::runtime_error(rays.status());
            const auto output=rays.shadow_output();
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,64*64*4,0};
            auto* transfer=SDL_CreateGPUTransferBuffer(device,&info);
            auto* command=SDL_AcquireGPUCommandBuffer(device);
            if(!transfer || !command) throw std::runtime_error(SDL_GetError());
            auto* copy=SDL_BeginGPUCopyPass(command);
            SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(output.buffer),0,64*64*4};
            SDL_GPUTransferBufferLocation destination{transfer,0};
            SDL_DownloadFromGPUBuffer(copy,&source,&destination);
            SDL_EndGPUCopyPass(copy);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
            if(!fence || !SDL_WaitForGPUFences(device,true,&fence,1))
                throw std::runtime_error(SDL_GetError());
            auto* values=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,transfer,false));
            if(!values) throw std::runtime_error(SDL_GetError());
            const auto center=values[32*64+32];
            SDL_UnmapGPUTransferBuffer(device,transfer);
            SDL_ReleaseGPUFence(device,fence);
            SDL_ReleaseGPUTransferBuffer(device,transfer);
            dispatched=center>0 && center<=160;
            std::cout<<"GPU-resident ray dispatch center shade: "<<center<<'\n';
            starfox::render::RayMaterials materials;
            materials.triangles.resize(2);
            materials.triangles[0].even=materials.triangles[0].odd=1;
            materials.triangles[1].even=materials.triangles[1].odd=1;
            std::array<std::uint32_t,256> palette{};
            palette[0]=0xff302010U;palette[1]=0xff80b0f0U;
            auto reflection_geometry=resident;
            reflection_geometry.materials=&materials;
            const bool reflected=rays.render_reflections(device,reflection_geometry,camera,
                palette,palette[0],2,0.f,0,{});
            if(!reflected) throw std::runtime_error(rays.status());
            const auto reflection=rays.reflection_output();
            auto* reflection_transfer=SDL_CreateGPUTransferBuffer(device,&info);
            auto* reflection_command=SDL_AcquireGPUCommandBuffer(device);
            if(!reflection_transfer || !reflection_command) throw std::runtime_error(SDL_GetError());
            auto* reflection_copy=SDL_BeginGPUCopyPass(reflection_command);
            SDL_GPUBufferRegion reflection_source{static_cast<SDL_GPUBuffer*>(reflection.buffer),
                0,64*64*4};
            SDL_GPUTransferBufferLocation reflection_destination{reflection_transfer,0};
            SDL_DownloadFromGPUBuffer(reflection_copy,&reflection_source,&reflection_destination);
            SDL_EndGPUCopyPass(reflection_copy);
            auto* reflection_fence=SDL_SubmitGPUCommandBufferAndAcquireFence(reflection_command);
            if(!reflection_fence || !SDL_WaitForGPUFences(device,true,&reflection_fence,1))
                throw std::runtime_error(SDL_GetError());
            const auto* reflection_values=static_cast<const std::uint32_t*>(
                SDL_MapGPUTransferBuffer(device,reflection_transfer,false));
            if(!reflection_values) throw std::runtime_error(SDL_GetError());
            const auto reflection_center=reflection_values[32*64+32];
            SDL_UnmapGPUTransferBuffer(device,reflection_transfer);
            SDL_ReleaseGPUFence(device,reflection_fence);
            SDL_ReleaseGPUTransferBuffer(device,reflection_transfer);
            // Live scenes pack materials after their vertices in the same
            // resident GPU buffer. Prove that offset/range path actually
            // reflects the target's palette colour, not just the environment.
            const std::array<std::array<float,4>,6> mirror_vertices{{
                {-20,-20,30,0},{20,-20,70,0},{0,20,50,0},
                {30,-30,20,0},{30,30,20,0},{30,0,80,0}}};
            std::array<starfox::render::RayMaterial,2> records{};
            records[0].even=records[0].odd=1;
            records[1].even=records[1].odd=2;
            std::array<std::uint8_t,sizeof(mirror_vertices)+sizeof(records)> payload{};
            std::memcpy(payload.data(),mirror_vertices.data(),sizeof(mirror_vertices));
            std::memcpy(payload.data()+sizeof(mirror_vertices),records.data(),sizeof(records));
            SDL_GPUTransferBufferCreateInfo refill_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                unsigned(payload.size()),0};
            auto* refill=SDL_CreateGPUTransferBuffer(device,&refill_info);
            if(!refill) throw std::runtime_error(SDL_GetError());
            auto* refill_data=SDL_MapGPUTransferBuffer(device,refill,false);
            if(!refill_data) throw std::runtime_error(SDL_GetError());
            std::memcpy(refill_data,payload.data(),payload.size());
            SDL_UnmapGPUTransferBuffer(device,refill);
            auto* refill_command=SDL_AcquireGPUCommandBuffer(device);
            auto* refill_pass=SDL_BeginGPUCopyPass(refill_command);
            SDL_GPUTransferBufferLocation refill_from{refill,0};
            SDL_GPUBufferRegion refill_to{geometry,0,unsigned(payload.size())};
            SDL_UploadToGPUBuffer(refill_pass,&refill_from,&refill_to,false);
            SDL_EndGPUCopyPass(refill_pass);
            auto* refill_fence=SDL_SubmitGPUCommandBufferAndAcquireFence(refill_command);
            if(!refill_fence || !SDL_WaitForGPUFences(device,true,&refill_fence,1))
                throw std::runtime_error(SDL_GetError());
            SDL_ReleaseGPUFence(device,refill_fence);
            SDL_ReleaseGPUTransferBuffer(device,refill);
            starfox::render::RayMaterials gpu_materials;
            auto gpu_geometry=resident;
            gpu_geometry.materials=&gpu_materials;
            gpu_geometry.material_offset=unsigned(sizeof(mirror_vertices));
            palette[2]=0xff11aa22U;
            if(!rays.render_reflections(device,gpu_geometry,camera,palette,palette[0],1,0.f,0,{}))
                throw std::runtime_error(rays.status());
            const auto gpu_reflection=rays.reflection_output();
            auto* gpu_transfer=SDL_CreateGPUTransferBuffer(device,&info);
            auto* gpu_command=SDL_AcquireGPUCommandBuffer(device);
            if(!gpu_transfer || !gpu_command) throw std::runtime_error(SDL_GetError());
            auto* gpu_copy=SDL_BeginGPUCopyPass(gpu_command);
            SDL_GPUBufferRegion gpu_source{static_cast<SDL_GPUBuffer*>(gpu_reflection.buffer),0,64*64*4};
            SDL_GPUTransferBufferLocation gpu_destination{gpu_transfer,0};
            SDL_DownloadFromGPUBuffer(gpu_copy,&gpu_source,&gpu_destination);
            SDL_EndGPUCopyPass(gpu_copy);
            auto* gpu_fence=SDL_SubmitGPUCommandBufferAndAcquireFence(gpu_command);
            if(!gpu_fence || !SDL_WaitForGPUFences(device,true,&gpu_fence,1))
                throw std::runtime_error(SDL_GetError());
            const auto* gpu_values=static_cast<const std::uint32_t*>(
                SDL_MapGPUTransferBuffer(device,gpu_transfer,false));
            if(!gpu_values) throw std::runtime_error(SDL_GetError());
            const auto gpu_center=gpu_values[32*64+32];
            SDL_UnmapGPUTransferBuffer(device,gpu_transfer);
            SDL_ReleaseGPUFence(device,gpu_fence);
            SDL_ReleaseGPUTransferBuffer(device,gpu_transfer);
            // A miss in the reflected ray must see authored BG2, not the
            // flat environment fallback. One solid 4bpp tile makes the
            // expected palette result unambiguous on the actual device.
            auto ppu=std::make_shared<starfox::simulation::SnesPpuState>();
            ppu->bg2_screen_size=0;
            for(unsigned row=0;row<8;++row) ppu->vram[std::size_t(ppu->bg2_character_base)*2+row*2]=255;
            ppu->cgram[1]=0x7fff;
            starfox::render::GpuBackgroundDraw bg;
            bg.ppu=ppu;
            bg.settings.layer=2;
            gpu_geometry.vertex_count=3;
            palette[1]=0xffdd6633U;
            if(!rays.render_reflections(device,gpu_geometry,camera,palette,palette[0],1,0.f,0,{},&bg))
                throw std::runtime_error(rays.status());
            const auto backdrop_reflection=rays.reflection_output();
            auto* backdrop_transfer=SDL_CreateGPUTransferBuffer(device,&info);
            auto* backdrop_command=SDL_AcquireGPUCommandBuffer(device);
            if(!backdrop_transfer || !backdrop_command) throw std::runtime_error(SDL_GetError());
            auto* backdrop_copy=SDL_BeginGPUCopyPass(backdrop_command);
            SDL_GPUBufferRegion backdrop_source{static_cast<SDL_GPUBuffer*>(backdrop_reflection.buffer),0,64*64*4};
            SDL_GPUTransferBufferLocation backdrop_destination{backdrop_transfer,0};
            SDL_DownloadFromGPUBuffer(backdrop_copy,&backdrop_source,&backdrop_destination);
            SDL_EndGPUCopyPass(backdrop_copy);
            auto* backdrop_fence=SDL_SubmitGPUCommandBufferAndAcquireFence(backdrop_command);
            if(!backdrop_fence || !SDL_WaitForGPUFences(device,true,&backdrop_fence,1))
                throw std::runtime_error(SDL_GetError());
            const auto* backdrop_values=static_cast<const std::uint32_t*>(
                SDL_MapGPUTransferBuffer(device,backdrop_transfer,false));
            if(!backdrop_values) throw std::runtime_error(SDL_GetError());
            const auto backdrop_center=backdrop_values[32*64+32];
            SDL_UnmapGPUTransferBuffer(device,backdrop_transfer);
            SDL_ReleaseGPUFence(device,backdrop_fence);
            SDL_ReleaseGPUTransferBuffer(device,backdrop_transfer);
            const auto read_reflection_center=[&]() {
                auto* transfer=SDL_CreateGPUTransferBuffer(device,&info);
                auto* command=SDL_AcquireGPUCommandBuffer(device);
                if(!transfer || !command) throw std::runtime_error(SDL_GetError());
                auto* pass=SDL_BeginGPUCopyPass(command);
                SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(rays.reflection_output().buffer),0,64*64*4};
                SDL_GPUTransferBufferLocation target{transfer,0};
                SDL_DownloadFromGPUBuffer(pass,&source,&target);
                SDL_EndGPUCopyPass(pass);
                auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
                if(!fence || !SDL_WaitForGPUFences(device,true,&fence,1))
                    throw std::runtime_error(SDL_GetError());
                const auto* values=static_cast<const std::uint32_t*>(SDL_MapGPUTransferBuffer(device,transfer,false));
                if(!values) throw std::runtime_error(SDL_GetError());
                const auto center=values[32*64+32];
                SDL_UnmapGPUTransferBuffer(device,transfer);
                SDL_ReleaseGPUFence(device,fence);
                SDL_ReleaseGPUTransferBuffer(device,transfer);
                return center;
            };
            starfox::render::BackdropImage enhanced_sky{
                2,2,std::vector<std::uint32_t>(4,0xff1040d0U)};
            enhanced_sky.seal_for_upload();
            starfox::render::EnvironmentEffects enhanced_environment;
            enhanced_environment.backdrop=&enhanced_sky;
            enhanced_environment.modes[2]=1;
            enhanced_environment.motion[0]=1000.f;
            enhanced_environment.plane[3]=1.f;
            bg.settings.reflection_environment=&enhanced_environment;
            if(!rays.render_reflections(device,gpu_geometry,camera,palette,palette[0],1,0.f,0,{},&bg))
                throw std::runtime_error(rays.status());
            const auto enhanced_center=read_reflection_center();
            RayWater water;water.time=1.f;water.reflection_strength=1.f;
            const ReceiverPlane water_plane{{0,.1,0},{0,1,0}};
            water.material=1;
            if(!rays.render_reflections(device,gpu_geometry,camera,palette,palette[0],1,0.f,0,
                    water_plane,&bg,&water)) throw std::runtime_error(rays.status());
            const auto enhanced_mirror_center=read_reflection_center();
            bg.settings.reflection_environment=nullptr;
            water.material=0;
            if(!rays.render_reflections(device,gpu_geometry,camera,palette,palette[0],1,0.f,0,
                    water_plane,&bg,&water)) throw std::runtime_error(rays.status());
            const auto water_center=read_reflection_center();
            water.time=20.f;
            if(!rays.render_reflections(device,gpu_geometry,camera,palette,palette[0],1,0.f,0,
                    water_plane,&bg,&water)) throw std::runtime_error(rays.status());
            const auto moving_water_center=read_reflection_center();
            water.material=2;
            if(!rays.render_reflections(device,gpu_geometry,camera,palette,palette[0],1,0.f,0,
                    water_plane,&bg,&water)) throw std::runtime_error(rays.status());
            const auto gold_center=read_reflection_center();
            water.material=3;
            if(!rays.render_reflections(device,gpu_geometry,camera,palette,palette[0],1,0.f,0,
                    water_plane,&bg,&water)) throw std::runtime_error(rays.status());
            const auto lava_center=read_reflection_center();
            water.time=31.f;
            if(!rays.render_reflections(device,gpu_geometry,camera,palette,palette[0],1,0.f,0,
                    water_plane,&bg,&water)) throw std::runtime_error(rays.status());
            const auto moving_lava_center=read_reflection_center();
            SDL_ReleaseGPUBuffer(device,geometry);
            dispatched=dispatched && (reflection_center>>24)==255;
            std::cout<<"GPU-resident reflection center RGBA: 0x"<<std::hex
                <<reflection_center<<std::dec<<'\n';
            dispatched=dispatched && gpu_center==palette[2];
            std::cout<<"GPU-material reflection center RGBA: 0x"<<std::hex
                <<gpu_center<<std::dec<<'\n';
            dispatched=dispatched && backdrop_center==palette[1];
            std::cout<<"Authored BG2 reflection center RGBA: 0x"<<std::hex
                <<backdrop_center<<std::dec<<'\n';
            dispatched=dispatched && enhanced_center==0xff1040d0U
                && (enhanced_mirror_center>>24)==254;
            std::cout<<"Enhanced sky / mirror reflection center RGBA: 0x"<<std::hex
                <<enhanced_center<<" / 0x"<<enhanced_mirror_center<<std::dec<<'\n';
            dispatched=dispatched && (water_center>>24)==254 && (gold_center>>24)==254
                && water_center!=gold_center && water_center!=moving_water_center
                && (gold_center&0xffu)>((gold_center>>8)&0xffu)
                && ((gold_center>>8)&0xffu)>((gold_center>>16)&0xffu);
            std::cout<<"Ray-water / gold ground center RGBA: 0x"<<std::hex
                <<water_center<<" / 0x"<<moving_water_center
                <<" / 0x"<<gold_center<<std::dec<<'\n';
            dispatched=dispatched && (lava_center>>24)==253
                && lava_center!=moving_lava_center;
            std::cout<<"Ray-lava surface center RGBA: 0x"<<std::hex
                <<lava_center<<" / 0x"<<moving_lava_center<<std::dec<<'\n';
        }
        if(!dispatched) std::cerr<<"Vulkan ray dispatch: "<<rays.status()<<'\n';
    }
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    return !support.available?2:dispatched?0:3;
}
