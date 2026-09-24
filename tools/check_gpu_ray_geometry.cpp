#include "starfox/render/gpu_ray_geometry.hpp"
#include "starfox/render/gpu_model.hpp"
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/sdl_dxr_shadows.hpp"
#include "starfox/render/sdl_gpu_effects.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* what) {if(!value) throw std::runtime_error(std::string(what)+": "+SDL_GetError());}
using Point=std::array<std::uint32_t,8>;
using Triangle=std::array<std::uint32_t,4>;
using Position=std::array<float,4>;
void check_materials(SDL_GPUDevice* device,starfox::render::GpuRayGeometry& geometry,unsigned texel_base=0) {
    using namespace starfox::render;
    PackedFaces faces;faces.corners={{{0,2,3,2}},{{1,4,5,2}},{{2,6,7,2}}};
    faces.polygons={{{0,3,0,0}},{{0,3,0,0}}};faces.materials.resize(2);faces.texels.resize(67);
    faces.materials[0].even=33;faces.materials[0].odd=44;faces.materials[0].dither=1;
    auto& textured=faces.materials[1];textured.textured=1;textured.u_mask=7;textured.v_mask=7;
    textured.texture_offset=3;textured.colour_base=128;textured.reserved0=std::bit_cast<unsigned>(-1);textured.reserved1=2;
    std::vector<Triangle> topology(68);for(unsigned i=0;i<68;++i) topology[i]={0,1,2,i%2};
    topology[66][3]=1; // Last occurrence of source face 1 is textured.
    RayMaterials expected;require(pack_ray_materials(faces,std::span(topology).first(67),expected),"reference materials");
    for(auto& material:expected.triangles) if(material.textured) material.offset+=texel_base;
    topology[66][3]=0x80000001U; // Resolve local corners by source identity on GPU.
    topology.back()[3]=2;
    const std::array<Uint32,4> sizes{Uint32(topology.size()*16),48,32,192};
    const void* data[]{topology.data(),faces.corners.data(),faces.polygons.data(),faces.materials.data()};
    std::array<SDL_GPUBuffer*,4> inputs{};Uint32 total=0;
    for(unsigned i=0;i<4;++i) {SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
        inputs[i]=SDL_CreateGPUBuffer(device,&info);require(inputs[i],"material input");total+=sizes[i];}
    std::vector<unsigned char> sentinel(68*64+32,0xa5);
    SDL_GPUBufferCreateInfo target_info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,Uint32(sentinel.size()),0};
    auto* destination=SDL_CreateGPUBuffer(device,&target_info);require(destination,"material target");
    const auto sentinel_offset=total;total+=Uint32(sentinel.size());
    SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,total,0};
    auto* upload=SDL_CreateGPUTransferBuffer(device,&upload_info);require(upload,"material upload");
    auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,false));require(mapped,"material mapping");
    Uint32 offset=0;for(unsigned i=0;i<4;++i){std::memcpy(mapped+offset,data[i],sizes[i]);offset+=sizes[i];}
    std::memcpy(mapped+sentinel_offset,sentinel.data(),sentinel.size());
    SDL_UnmapGPUTransferBuffer(device,upload);
    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"material command");
    auto* copy=SDL_BeginGPUCopyPass(command);offset=0;
    for(unsigned i=0;i<4;++i){SDL_GPUTransferBufferLocation from{upload,offset};SDL_GPUBufferRegion to{inputs[i],0,sizes[i]};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);offset+=sizes[i];}
    SDL_GPUTransferBufferLocation sentinel_from{upload,sentinel_offset};SDL_GPUBufferRegion sentinel_to{destination,0,target_info.size};
    SDL_UploadToGPUBuffer(copy,&sentinel_from,&sentinel_to,false);SDL_EndGPUCopyPass(copy);
    auto* output=static_cast<SDL_GPUBuffer*>(geometry.enqueue_materials(device,command,inputs[0],inputs[1],inputs[2],inputs[3],68,3,2,67,texel_base));
    require(output,geometry.status().c_str());
    require(!geometry.enqueue_materials(device,command,output,inputs[1],inputs[2],inputs[3],68,3,2,67),"material alias accepted");
    const GpuRayMaterialTarget target{destination,target_info.size,16,false};
    require(geometry.enqueue_materials(device,command,inputs[0],inputs[1],inputs[2],inputs[3],68,3,2,67,texel_base,&target)==destination,"material direct target rejected");
    for(const auto invalid:std::array<GpuRayMaterialTarget,4>{{{destination,target_info.size,1,false},
        {destination,target_info.size,48,false},{inputs[0],target_info.size,16,false},{nullptr,target_info.size,16,false}}})
        require(!geometry.enqueue_materials(device,command,inputs[0],inputs[1],inputs[2],inputs[3],68,3,2,67,texel_base,&invalid),"invalid direct material target accepted");
    SDL_GPUTransferBufferCreateInfo down_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,68*64+target_info.size,0};
    auto* download=SDL_CreateGPUTransferBuffer(device,&down_info);require(download,"material download");
    copy=SDL_BeginGPUCopyPass(command);SDL_GPUBufferRegion from{output,0,68*64};SDL_GPUTransferBufferLocation to{download,0};
    SDL_DownloadFromGPUBuffer(copy,&from,&to);
    from={destination,0,target_info.size};to={download,68*64};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"material submit");
    require(SDL_WaitForGPUFences(device,true,&fence,1),"material wait");
    auto* actual=static_cast<const RayMaterial*>(SDL_MapGPUTransferBuffer(device,download,false));require(actual,"material result");
    require(std::memcmp(actual,expected.triangles.data(),67*64)==0,"material CPU/GPU mismatch");
    require(actual[67].reserved==1,"invalid occurrence silently became black");
    const auto* direct=reinterpret_cast<const unsigned char*>(actual)+68*64;
    require(std::memcmp(direct+16,actual,68*64)==0,"direct material target mismatch");
    require(std::memcmp(direct,sentinel.data(),16)==0 && std::memcmp(direct+16+68*64,sentinel.data(),16)==0,"material target overwrote adjacent scene data");
    SDL_UnmapGPUTransferBuffer(device,download);SDL_ReleaseGPUFence(device,fence);
    SDL_ReleaseGPUTransferBuffer(device,download);SDL_ReleaseGPUTransferBuffer(device,upload);
    for(auto* input:inputs) SDL_ReleaseGPUBuffer(device,input);
    SDL_ReleaseGPUBuffer(device,destination);
    std::cout<<"GPU ray materials: occurrence slots, UV scroll, texture bounds and partial group passed\n";
}
std::vector<Position> run(SDL_GPUDevice* device,starfox::render::GpuRayGeometry& geometry,
    const std::vector<Point>& points,const std::vector<Point>& residuals,
    const std::vector<Triangle>& triangles,starfox::render::GpuRayGeometrySettings settings) {
    settings.points=std::uint32_t(points.size());settings.triangles=std::uint32_t(triangles.size());
    const std::array<std::uint32_t,3> sizes{settings.points*32,settings.points*32,settings.triangles*16};
    std::array<SDL_GPUBuffer*,3> inputs{};
    std::uint32_t total=0;
    for(unsigned i=0;i<3;++i) {
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
        inputs[i]=SDL_CreateGPUBuffer(device,&info);require(inputs[i],"input buffer");total+=sizes[i];
    }
    SDL_GPUTransferBufferCreateInfo up_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,total,0};
    auto* upload=SDL_CreateGPUTransferBuffer(device,&up_info);require(upload,"upload");
    auto* mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,false));require(mapped,"map upload");
    const void* data[]{points.data(),residuals.data(),triangles.data()};
    std::uint32_t offset=0;for(unsigned i=0;i<3;++i) {std::memcpy(mapped+offset,data[i],sizes[i]);offset+=sizes[i];}
    SDL_UnmapGPUTransferBuffer(device,upload);
    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"command");
    auto* copy=SDL_BeginGPUCopyPass(command);offset=0;
    for(unsigned i=0;i<3;++i) {
        SDL_GPUTransferBufferLocation from{upload,offset};SDL_GPUBufferRegion to{inputs[i],0,sizes[i]};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);offset+=sizes[i];
    }
    SDL_EndGPUCopyPass(copy);
    auto* output=static_cast<SDL_GPUBuffer*>(geometry.enqueue(device,command,inputs[0],inputs[1],inputs[2],settings));
    if(!output) {SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(geometry.status());}
    auto bad=settings;bad.row0[0]=std::numeric_limits<float>::infinity();
    require(!geometry.enqueue(device,command,inputs[0],inputs[1],inputs[2],bad),"invalid transform accepted");
    require(!geometry.enqueue(device,command,output,inputs[1],inputs[2],settings),"output alias accepted");
    bad=settings;bad.mode=3;
    require(!geometry.enqueue(device,command,inputs[0],inputs[1],inputs[2],bad),"unknown mode accepted");
    const starfox::render::GpuRayGeometryTarget overflow{output,settings.triangles*3U,1,false};
    require(!geometry.enqueue(device,command,inputs[0],inputs[1],inputs[2],settings,&overflow),"external target overflow accepted");
    const starfox::render::GpuRayGeometryTarget alias{inputs[0],settings.triangles*3U,0,false};
    require(!geometry.enqueue(device,command,inputs[0],inputs[1],inputs[2],settings,&alias),"external target aliases source");
    SDL_GPUTransferBufferCreateInfo down_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,settings.triangles*48,0};
    auto* download=SDL_CreateGPUTransferBuffer(device,&down_info);require(download,"download");
    copy=SDL_BeginGPUCopyPass(command);
    SDL_GPUBufferRegion from{output,0,down_info.size};SDL_GPUTransferBufferLocation to{download,0};
    SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"submit");
    require(SDL_WaitForGPUFences(device,true,&fence,1),"wait");SDL_ReleaseGPUFence(device,fence);
    std::vector<Position> result(settings.triangles*3);
    mapped=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,download,false));require(mapped,"map download");
    std::memcpy(result.data(),mapped,down_info.size);SDL_UnmapGPUTransferBuffer(device,download);
    for(auto* input:inputs) SDL_ReleaseGPUBuffer(device,input);
    SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUTransferBuffer(device,download);
    return result;
}
void check_model(SDL_GPUDevice* device,starfox::render::GpuRayGeometry& geometry,bool continuous,unsigned effect=0,bool reference=true) {
    starfox::render::GpuModel model;
    starfox::assets::Shape shape;shape.vertices={{80,60,-360},{-80,60,-440},{0,-70,-400}};
    shape.word_coordinates=std::vector<bool>(3,true);
    starfox::assets::Face face;face.visibility_index=-1;face.normal={0,0,127};face.vertex_indices={0,2,1};
    shape.faces={face};shape.colour_words={0x3f11};shape.colour_materials={{0x3f11,{}}};
    starfox::render::RenderPose pose;pose.z=0;pose.use_rotation_matrix=true;
    pose.rotation_matrix={-32768,0,0,0,-32768,0,0,0,-32768};
    pose.continuous_geometry=continuous;pose.subpixel_projection=continuous;pose.vanish_x=112;pose.vanish_y=96;
    pose.wave_mode=effect==1;pose.wave_offset=11;
    pose.wobble_mode=effect>=2 && effect<=4?effect-1:0;
    pose.colour_warp=effect==5 || effect==7 || effect==9;
    pose.collapse_to_axis_line=effect>=6;
    pose.explosion_progress=effect>=8?1:0;
    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"model command");
    starfox::render::GpuModelRaySource source;
    source.request_materials=true;
    source.reference_materials=reference;
    auto raster=model.enqueue(device,command,shape,pose,{},224,192,false,nullptr,nullptr,false,&source);
    require(raster.pixels && source.points && source.triangles.size()==1,"model ray source unavailable");
    if(effect<5) require(source.materials_complete && source.materials.triangles.size()==1
        && source.materials.triangles[0].face==source.triangles[0][3],"model reflection material correspondence");
    else if(effect==5 && !reference) require(source.materials_complete && source.materials.triangles.empty()
        && source.material_commands && source.material_corners && source.material_polygons
        && source.material_topology.size()==1 && source.material_topology[0]==Triangle{0,1,2,0x80000000U},
        "warp model did not export source-face occurrence mapping");
    else if(effect>=6) require(source.materials_complete && source.reflection_excluded
        && source.materials.triangles.size()==1 && source.materials.triangles[0].reserved==1,
        "axis line did not retain caster with explicit invalid reflection material");
    else require(!source.materials_complete,"unresolved reference warp materials incorrectly accepted");
    SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,16,0};
    auto* topology=SDL_CreateGPUBuffer(device,&info);require(topology,"topology");
    SDL_GPUTransferBufferCreateInfo up_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,16,0};
    auto* upload=SDL_CreateGPUTransferBuffer(device,&up_info);require(upload,"topology upload");
    auto* bytes=SDL_MapGPUTransferBuffer(device,upload,false);require(bytes,"topology map");
    std::memcpy(bytes,source.triangles.data(),16);SDL_UnmapGPUTransferBuffer(device,upload);
    auto* copy=SDL_BeginGPUCopyPass(command);
    SDL_GPUTransferBufferLocation from{upload,0};SDL_GPUBufferRegion to{topology,0,16};
    SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
    starfox::render::GpuRayGeometrySettings settings;settings.triangles=1;settings.points=source.point_count;settings.mode=source.mode;
    auto* expanded=static_cast<SDL_GPUBuffer*>(geometry.enqueue(device,command,source.points,source.residuals,topology,settings));require(expanded,"model expansion");
    SDL_GPUTransferBufferCreateInfo down_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,48,0};
    auto* download=SDL_CreateGPUTransferBuffer(device,&down_info);require(download,"model download");
    copy=SDL_BeginGPUCopyPass(command);SDL_GPUBufferRegion gpu{expanded,0,48};SDL_GPUTransferBufferLocation cpu{download,0};
    SDL_DownloadFromGPUBuffer(copy,&gpu,&cpu);SDL_EndGPUCopyPass(copy);
    auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"model submit");
    require(SDL_WaitForGPUFences(device,true,&fence,1),"model wait");SDL_ReleaseGPUFence(device,fence);
    const auto* result=static_cast<const Position*>(SDL_MapGPUTransferBuffer(device,download,false));require(result,"model result");
    std::array<Position,3> expected{{{-80,-60,360,1},{0,70,400,1},{80,-60,440,1}}};
    if(effect>=8) for(auto& point:expected) point[2]+=31;
    for(unsigned c=0;c<3;++c) require(result[c]==expected[c],"model producer/expander mismatch");
    SDL_UnmapGPUTransferBuffer(device,download);
    SDL_ReleaseGPUTransferBuffer(device,download);SDL_ReleaseGPUTransferBuffer(device,upload);SDL_ReleaseGPUBuffer(device,topology);
}
void check_scene(SDL_GPUDevice* device) {
    using namespace starfox::render;
    GpuScene scene;
#if defined(_WIN32)
    shadows::SdlDxrShadows resident_shadows;
    shadows::DxrShadows reference_shadows;
#endif
    starfox::assets::Shape shape;shape.vertices={{80,60,-360},{-80,60,-440},{0,-70,-400}};
    shape.word_coordinates=std::vector<bool>(3,true);
    starfox::assets::Face face;face.visibility_index=-1;face.normal={0,0,127};face.vertex_indices={0,2,1};
    shape.faces={face};shape.colour_words={0x3f11};shape.colour_materials={{0x3f11,{}}};
    for(unsigned model_count:{1U,33U,2U}) {
        std::vector<GpuSceneDraw> draws;std::vector<Position> expected;
        for(unsigned i=0;i<model_count;++i) {
            GpuModelDraw draw;draw.shape=&shape;draw.pose.z=0;draw.pose.x=i*13;
            draw.pose.use_rotation_matrix=true;draw.pose.rotation_matrix={-32768,0,0,0,-32768,0,0,0,-32768};
            draw.pose.continuous_geometry=(i%2)!=0;draw.pose.subpixel_projection=(i%2)!=0;
            draw.pose.vanish_x=112;draw.pose.vanish_y=96;draw.ray_geometry=i!=1;
            draw.ray_materials=true;
            draw.ray_material_reference=true;
            draws.push_back(draw);
            if(draw.ray_geometry) for(const auto point:std::array<Position,3>{{{-80,-60,360,1},{0,70,400,1},{80,-60,440,1}}}) {
                auto p=point;p[0]+=float(i*13);expected.push_back(p);
            }
        }
        auto* command=SDL_AcquireGPUCommandBuffer(device);require(command,"scene command");
        require(scene.enqueue_batch(device,command,224,192,draws).pixels,"scene raster");
        const auto rays=scene.ray_geometry_output();
        require(rays.complete && rays.buffer && rays.vertex_count==expected.size(),"scene caster count/completeness");
        require(rays.materials && rays.materials->triangles.size()*3==rays.vertex_count,
            "scene reflection materials lost triangle correspondence");
        require(rays.material_offset>=rays.vertex_count*16 && rays.material_offset%16==0,
            "scene did not publish GPU-packed materials after its vertices");
        SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,rays.vertex_count*16,0};
        auto* download=SDL_CreateGPUTransferBuffer(device,&info);require(download,"scene download");
        auto* copy=SDL_BeginGPUCopyPass(command);SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(rays.buffer),0,info.size};
        SDL_GPUTransferBufferLocation to{download,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"scene submit");
        require(SDL_WaitForGPUFences(device,true,&fence,1),"scene wait");SDL_ReleaseGPUFence(device,fence);
        const auto* points=static_cast<const Position*>(SDL_MapGPUTransferBuffer(device,download,false));require(points,"scene map");
        for(unsigned i=0;i<expected.size();++i) require(points[i]==expected[i],"scene caster overwritten or reordered");
        SDL_UnmapGPUTransferBuffer(device,download);SDL_ReleaseGPUTransferBuffer(device,download);
#if defined(_WIN32)
        shadows::Scene reference;
        const auto vertex=[](const Position& p){return shadows::Vec3{p[0],p[1],p[2]};};
        for(unsigned i=0;i<expected.size();i+=3)
            reference.add({vertex(expected[i]),vertex(expected[i+1]),vertex(expected[i+2])});
        const shadows::Camera camera{224,192,256,112,96};
        const shadows::Vec3 light{-1,-1,-1};
        const shadows::ReceiverPlane ground{{0,100,0},{0,1,0}};
        std::vector<std::uint8_t> actual_mask,expected_mask;
        require(reference_shadows.render(reference,camera,light,ground,expected_mask),reference_shadows.status().c_str());
        require(resident_shadows.render_resident(device,{},camera,light,ground,&rays),resident_shadows.status().c_str());
        require(resident_shadows.readback(actual_mask),resident_shadows.status().c_str());
        require(actual_mask==expected_mask,"GPU scene -> shared geometry -> DXR mask mismatch");
        // Use the actual SDL-produced 16-byte vertices for reflected normals,
        // not the diagnostic producer's tightly packed CPU triangles.
        std::array<std::uint32_t,256> reflection_palette{};
        for(unsigned i=0;i<reflection_palette.size();++i)
            reflection_palette[i]=0xff000000U | (i*65793U);
        std::vector<std::uint8_t> expected_reflections,actual_reflections;
        require(reference_shadows.render_reflections(reference,camera,*rays.materials,
            reflection_palette,0xff345678U,expected_reflections),reference_shadows.status().c_str());
        require(resident_shadows.render_reflections(device,camera,rays,
            reflection_palette,0xff345678U),resident_shadows.status().c_str());
        const auto reflected=resident_shadows.reflection_output();
        require(reflected.buffer && reflected.width==camera.width && reflected.height==camera.height
            && reflected.row_bytes==camera.width*4 && !resident_shadows.output().buffer,
            "RGBA reflection was exposed as a shadow mask");
        require(resident_shadows.readback(actual_reflections)
            && actual_reflections==expected_reflections,"SDL geometry reflection transport mismatch");
        Framebuffer reflection_frame(camera.width,camera.height+4);
        reflection_frame.enable_layer_tags(true);
        SurfaceBuffer reflection_surfaces(camera.width,camera.height+4);
        for(unsigned y=0;y<reflection_frame.height();++y) for(unsigned x=0;x<camera.width;++x) {
            reflection_frame.set_stored(x,y,1,static_cast<PixelLayer>(x%5));
            if(y%3) reflection_surfaces.set(x,y,{},1);
        }
        SdlGpuEffects reflection_effects;
        for(unsigned intensity:{0U,37U,100U}) for(int offset:{-2,2}) {
            GpuEffectSettings settings;
            settings.surfaces=&reflection_surfaces;settings.resident_reflection=reflected;
            settings.reflection_intensity=intensity;settings.reflection_offset_y=offset;
            std::vector<std::uint8_t> pixels(reflection_frame.pixels().size()*4,80),wanted=pixels;
            for(unsigned y=0;y<reflection_frame.height();++y) for(unsigned x=0;x<camera.width;++x) {
                const int sy=int(y)-offset;
                if((x%5!=0 && x%5!=4) || y%3==0 || sy<0 || sy>=int(camera.height)) continue;
                const auto source=(std::size_t(sy)*camera.width+x)*4;
                const unsigned alpha=expected_reflections[source+3]*intensity/100;
                for(unsigned channel=0;channel<3;++channel)
                    wanted[(std::size_t(y)*camera.width+x)*4+channel]=
                        (expected_reflections[source+channel]*alpha+80*(255-alpha)+127)/255;
            }
            if(intensity) require(wanted!=pixels,"reflection composition fixture contains no visible reflected pixels");
            require(reflection_effects.apply(device,reflection_frame,pixels,settings),reflection_effects.status().c_str());
            require(pixels==wanted,"reflection composition intensity/offset/layer isolation mismatch");
        }
        reflection_effects.release_device();
        auto alternate_shape=shape;alternate_shape.faces[0].vertex_indices={0,1,2};
        auto cancelled_draws=draws;std::get<GpuModelDraw>(cancelled_draws[0]).shape=&alternate_shape;
        command=SDL_AcquireGPUCommandBuffer(device);require(command,"cancelled topology command");
        require(scene.enqueue_batch(device,command,224,192,cancelled_draws).pixels,"cancelled topology encode");
        SDL_CancelGPUCommandBuffer(command);
        require(scene.render_resident(device,224,192,cancelled_draws),scene.status().c_str());
        auto reuploaded=scene.ray_geometry_output();
        require(resident_shadows.render_resident(device,{},camera,light,ground,&reuploaded),resident_shadows.status().c_str());
        require(resident_shadows.readback(actual_mask) && actual_mask==expected_mask,"cancelled topology upload reused");
        require(scene.wait_for_completion(),"reuploaded scene completion");
        // Exercise owned submissions too: do not CPU-wait the scene before
        // its consumer copies the resident geometry on the same SDL queue.
        require(scene.render_resident(device,224,192,draws),scene.status().c_str());
        auto owned=scene.ray_geometry_output();
        require(resident_shadows.render_resident(device,{},camera,light,ground,&owned),resident_shadows.status().c_str());
        require(resident_shadows.readback(actual_mask) && actual_mask==expected_mask,"owned caster submission mismatch");
        require(scene.wait_for_completion(),"owned scene completion");
        require(reference_shadows.render_reflections(reference,camera,*owned.materials,
            reflection_palette,0xff345678U,expected_reflections),"CPU material reference");
        auto gpu_only_draws=draws;
        for(auto& draw:gpu_only_draws) std::get<GpuModelDraw>(draw).ray_material_reference=false;
        require(scene.render_resident(device,224,192,gpu_only_draws),"GPU-only material scene");
        const auto gpu_only=scene.ray_geometry_output();
        require(gpu_only.material_offset && gpu_only.materials && gpu_only.materials->triangles.empty(),
            "live material path still generated CPU triangle records");
        require(resident_shadows.render_reflections(device,camera,gpu_only,reflection_palette,0xff345678U),
            resident_shadows.status().c_str());
        require(resident_shadows.readback(actual_reflections) && actual_reflections==expected_reflections,
            "GPU-only materials differ from independent CPU reference");
        require(scene.wait_for_completion(),"GPU-only scene completion");
        auto warp_draws=gpu_only_draws;
        for(auto& draw:warp_draws) std::get<GpuModelDraw>(draw).pose.colour_warp=true;
        require(scene.render_resident(device,224,192,warp_draws),"live warp material scene");
        const auto warped=scene.ray_geometry_output();
        require(warped.complete && warped.materials && warped.material_offset && warped.vertex_count==expected.size(),
            "warp reflection mapping changed full caster geometry");
        require(resident_shadows.render_resident(device,{},camera,light,ground,&warped),"warp shadow submission");
        require(resident_shadows.readback(actual_mask) && actual_mask==expected_mask,
            "warp material selection changed shadow coverage");
        require(resident_shadows.render_reflections(device,camera,warped,reflection_palette,0xff345678U),
            "live warp reflection submission");
        require(resident_shadows.readback(actual_reflections),"live warp reflection readback");
        bool warp_visible=false;
        for(std::size_t pixel=3;pixel<actual_reflections.size();pixel+=4) warp_visible|=actual_reflections[pixel]!=0;
        require(warp_visible,"live warp reflection contains no emitted surfaces");
        require(scene.wait_for_completion(),"warp scene completion");
        auto axis_draws=gpu_only_draws;
        std::get<GpuModelDraw>(axis_draws[0]).pose.collapse_to_axis_line=true;
        require(scene.render_resident(device,224,192,axis_draws),"axis/ordinary mixed scene");
        const auto axis_scene=scene.ray_geometry_output();
        require(axis_scene.complete && axis_scene.materials && axis_scene.material_offset && axis_scene.vertex_count==expected.size(),
            "axis line disabled scene reflections or removed shadow casters");
        require(resident_shadows.render_resident(device,{},camera,light,ground,&axis_scene),"axis scene shadows");
        require(resident_shadows.readback(actual_mask) && actual_mask==expected_mask,"axis reflection exclusion changed shadows");
        require(resident_shadows.render_reflections(device,camera,axis_scene,reflection_palette,0xff345678U),"axis scene reflections");
        require(resident_shadows.readback(actual_reflections),"axis reflection readback");
        bool axis_visible=false;
        for(std::size_t pixel=3;pixel<actual_reflections.size();pixel+=4) axis_visible|=actual_reflections[pixel]!=0;
        require(axis_visible==(model_count>2),"axis line reflected as polygon or suppressed ordinary scene models");
        require(scene.wait_for_completion(),"axis scene completion");
        if(model_count==2) {
            GpuStereoScene stereo;
            require(stereo.render_resident(device,224,192,draws,6.4,512),"stereo caster submission");
            for(unsigned eye=0;eye<2;++eye) {
                shadows::Scene eye_reference;
                const double eye_x=eye?3.2:-3.2;
                const auto eye_vertex=[&](const Position& p){return shadows::Vec3{float(double(p[0])-eye_x),p[1],p[2]};};
                for(unsigned i=0;i<expected.size();i+=3)
                    eye_reference.add({eye_vertex(expected[i]),eye_vertex(expected[i+1]),eye_vertex(expected[i+2])});
                auto eye_camera=camera;eye_camera.center_x+=camera.focal_length*eye_x/512.;
                require(reference_shadows.render(eye_reference,eye_camera,light,ground,expected_mask),"stereo reference");
                auto eye_rays=stereo.ray_geometry_output(eye);
                require(resident_shadows.render_resident(device,{},eye_camera,light,ground,&eye_rays),resident_shadows.status().c_str());
                require(resident_shadows.readback(actual_mask) && actual_mask==expected_mask,"stereo caster eye mismatch");
                require(eye_rays.materials,"stereo reflection materials missing");
                require(reference_shadows.render_reflections(eye_reference,eye_camera,*eye_rays.materials,
                    reflection_palette,0xff345678U,expected_reflections),reference_shadows.status().c_str());
                require(resident_shadows.render_reflections(device,eye_camera,eye_rays,
                    reflection_palette,0xff345678U),resident_shadows.status().c_str());
                require(resident_shadows.readback(actual_reflections) && actual_reflections==expected_reflections,
                    "stereo reflected rays differ from eye-shifted reference");
            }
        }
#endif
        // Emissive objects remain visible but cannot accidentally enter the
        // caster/reflection set, even if a caller also requests ray geometry.
        auto emissive_draws=draws;
        for(auto& draw:emissive_draws) std::get<GpuModelDraw>(draw).emissive=true;
        require(scene.render_resident(device,224,192,emissive_draws),scene.status().c_str());
        require(!scene.ray_geometry_output().buffer && !scene.ray_geometry_output().vertex_count,
            "emissive beams entered ray geometry");
        require(scene.wait_for_completion(),"emissive scene completion");
        if(model_count>2) {
            auto mixed=draws;std::get<GpuModelDraw>(mixed[0]).emissive=true;
            require(scene.render_resident(device,224,192,mixed),scene.status().c_str());
            const auto non_emissive=scene.ray_geometry_output();
            require(non_emissive.complete && non_emissive.materials
                && non_emissive.vertex_count==expected.size()-3
                && non_emissive.materials->triangles.size()*3==non_emissive.vertex_count,
                "beam exclusion damaged other model rays/material ordering");
            require(scene.wait_for_completion(),"mixed emissive scene completion");
        }
        require(scene.render_resident(device,224,192,draws),scene.status().c_str());
        require(scene.ray_geometry_output().complete && scene.ray_geometry_output().materials,
            "emissive toggle lost ordinary reflection materials");
        require(scene.wait_for_completion(),"restored scene completion");
        // One unsupported caster must invalidate the aggregate, never silently
        // export the other models as a supposedly complete shadow scene.
        std::get<GpuModelDraw>(draws[0]).pose.simple_scaled_sprite=true;
        command=SDL_AcquireGPUCommandBuffer(device);require(command,"unsupported scene command");
        require(scene.enqueue_batch(device,command,224,192,draws).pixels,"unsupported caster raster fallback");
        require(!scene.ray_geometry_output().complete && !scene.ray_geometry_output().buffer,"partial caster scene escaped");
        SDL_CancelGPUCommandBuffer(command);
        command=SDL_AcquireGPUCommandBuffer(device);require(command,"empty scene command");
        require(scene.enqueue_batch(device,command,224,192,{}).pixels,"empty scene");
        require(!scene.ray_geometry_output().buffer && !scene.ray_geometry_output().vertex_count,"empty scene retained old casters");
        SDL_CancelGPUCommandBuffer(command);
    }
}
}
int main() try {
    // Long software-Vulkan shader compilation must not hide completed stages
    // behind stdout buffering when this checker runs under a harness.
    std::cout.setf(std::ios::unitbuf);
    require(SDL_Init(SDL_INIT_VIDEO),"SDL");
    auto props=SDL_CreateProperties();
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
#if defined(_WIN32)
    starfox::render::shadows::SdlDxrShadows::request_vulkan_interop(props);
#endif
    auto* device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);require(device,"device");
    starfox::render::GpuRayGeometry geometry;
    check_materials(device,geometry);
    check_materials(device,geometry,93);
    check_model(device,geometry,false);check_model(device,geometry,true);
    check_model(device,geometry,false,5,false);check_model(device,geometry,true,5,false);
    for(unsigned effect=1;effect<=9;++effect) {
        check_model(device,geometry,false,effect);check_model(device,geometry,true,effect);
    }
    std::cout<<"GPU model ray inputs passed; checking scene assembly\n";
    check_scene(device);
    std::size_t checked=0;
    for(unsigned mode:{0U,1U,2U}) for(unsigned count:{1U,65U,257U,2U}) {
        std::vector<Point> points(4),tails(4);
        std::array<Position,4> expected{};
        for(unsigned i=0;i<4;++i) {
            const float xyz[]{float(int(i)*11-20),float(int(i)*7-9),float(int(i)*19-30)};
            for(unsigned a=0;a<3;++a) {
                float value=xyz[a];
                points[i][a]=mode?std::bit_cast<std::uint32_t>(value):std::bit_cast<std::uint32_t>(int(value));
                if(mode==2 && i%2==0) {tails[i][a]=std::bit_cast<std::uint32_t>(.25F);value+=.25F;}
                if(mode==2 && i%2==1) {
                    const auto bits=std::bit_cast<std::uint64_t>(double(value)+.125);
                    tails[i][a]=std::uint32_t(bits);tails[i][a+4]=std::uint32_t(bits>>32);value+=.125F;
                }
                expected[i][a]=value;
            }
            points[i][3]=mode?std::bit_cast<std::uint32_t>(1.F):0;
            tails[i][3]=std::bit_cast<std::uint32_t>(i%2==1?3.F:1.F);
            expected[i][0]+=17;expected[i][1]-=3;expected[i][2]+=10;expected[i][3]=1;
        }
        std::vector<Triangle> triangles(count);
        for(unsigned i=0;i<count;++i) triangles[i]={i%4,(i+1)%4,(i+2)%4,i};
        starfox::render::GpuRayGeometrySettings settings;settings.mode=mode;
        settings.row0[3]=17;settings.row1[3]=-3;settings.row2[3]=10;
        auto result=run(device,geometry,points,tails,triangles,settings);
        for(unsigned i=0;i<count;++i) for(unsigned c=0;c<3;++c) {require(result[i*3+c]==expected[triangles[i][c]],"expanded point mismatch");++checked;}
        triangles[0][1]=4;result=run(device,geometry,points,tails,triangles,settings);
        for(unsigned c=0;c<3;++c) require(result[c]==Position{},"invalid triangle retained stale positions");
        triangles[0]={0,1,2,0};points[0][3]=mode?std::bit_cast<std::uint32_t>(-1.F):1U;
        result=run(device,geometry,points,tails,triangles,settings);
        for(unsigned c=0;c<3;++c) require(result[c]==Position{},"invalid source point retained stale positions");
    }
    geometry.release_device();
    check_model(device,geometry,true);
    geometry.release_device();
    SDL_DestroyGPUDevice(device);SDL_Quit();
    std::cout<<"GPU ray expansion: "<<checked<<" native/fractional/compensated vertices, negative/offscreen coordinates, resize/reuse and invalidation passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
