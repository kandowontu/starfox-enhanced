#include "starfox/render/gpu_model.hpp"
#include "starfox/render/packed_projection.hpp"
#include "starfox/render/packed_faces.hpp"
#include "starfox/render/gpu_bsp.hpp"
#include "starfox/render/gpu_colour_warp.hpp"
#include "starfox/render/source_shading.hpp"
#include "starfox/render/gpu_clip.hpp"
#include "starfox/render/face_material.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <cmath>
#include <iostream>
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "shaders/generated/surface_portable.hpp"
#include "shaders/generated/billboard_portable.hpp"
#endif
namespace starfox::render {
struct GpuModel::Impl {
    std::string status{"GPU model rendering unavailable"};
    // Upload copies consume these immediately; no command retains host pointers.
    // Reuse capacity across models instead of allocating twice per draw.
    std::vector<std::array<float,4>> near_scratch;
    std::vector<std::array<std::int32_t,4>> normal_scratch;
    GpuProjection projection;GpuBsp bsp;GpuClip clip;GpuRaster raster;GpuColourWarp warp,reflection_warp;
    GpuProjection axis_ray_projection;
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};
    std::array<SDL_GPUBuffer*,18> buffers{};std::array<std::uint32_t,18> capacities{};
    SDL_GPUComputePipeline* surface_pipeline{};SDL_GPUBuffer* surface_materials{};std::uint32_t surface_capacity{};
    SDL_GPUBuffer* geometry_planes{};std::uint32_t geometry_capacity{};
    SDL_GPUTransferBuffer* upload{};std::uint32_t upload_capacity{};
    SDL_GPUComputePipeline* billboard_pipeline{};
    SDL_GPUBuffer* billboard_spans{};std::uint32_t billboard_rows{};
    std::array<SDL_GPUBuffer*,2> axis_ray_buffers{};
    std::array<std::uint32_t,2> axis_ray_capacities{};
    SDL_GPUTransferBuffer* axis_ray_upload{};std::uint32_t axis_ray_upload_capacity{};
    ~Impl(){release();}
    void release()noexcept {
        projection.release_device();bsp.release_device();clip.release_device();raster.release_device();
        warp.release_device();
        reflection_warp.release_device();
        axis_ray_projection.release_device();
        for(auto* buffer:axis_ray_buffers) if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        if(axis_ray_upload) SDL_ReleaseGPUTransferBuffer(device,axis_ray_upload);
        axis_ray_buffers={};axis_ray_capacities={};axis_ray_upload=nullptr;axis_ray_upload_capacity=0;
        for(auto* buffer:buffers) if(buffer) SDL_ReleaseGPUBuffer(device,buffer);
        if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
        if(surface_pipeline) SDL_ReleaseGPUComputePipeline(device,surface_pipeline);
        if(surface_materials) SDL_ReleaseGPUBuffer(device,surface_materials);
        if(geometry_planes) SDL_ReleaseGPUBuffer(device,geometry_planes);
        if(billboard_pipeline) SDL_ReleaseGPUComputePipeline(device,billboard_pipeline);
        if(billboard_spans) SDL_ReleaseGPUBuffer(device,billboard_spans);
        billboard_pipeline=nullptr;billboard_spans=nullptr;billboard_rows=0;
        surface_pipeline=nullptr;surface_materials=nullptr;surface_capacity=0;
        geometry_planes=nullptr;geometry_capacity=0;
        device=nullptr;buffers={};capacities={};upload=nullptr;upload_capacity=0;
    }
    static void require(bool value){if(!value) throw std::runtime_error(SDL_GetError());}
    void exploding_axis_casters(SDL_GPUCommandBuffer* command,const assets::Shape& shape,
        RenderPose pose,const RenderSettings& settings,GpuModelRaySource& out) {
        // Axis display deliberately suppresses fragment motion. Its shadow
        // still follows the exploding source faces, so use a separate GPU
        // transform stream instead of changing the visible laser's geometry.
        pose.collapse_to_axis_line=false;
        auto vertices=pack_projection(shape,pose,settings);
        const auto graph=pack_bsp(shape,true);
        const bool colour_warp=pose.colour_warp && !pose.force_colour;
        auto faces=colour_warp?pack_warp_faces(shape,graph,pose,settings):pack_faces(shape,graph,pose,settings);
        std::vector<ContinuousTransformPose> continuous_poses;
        std::vector<NativeTransformPose> native_poses;
        if(vertices.continuous) continuous_poses=pack_continuous_fragments(vertices,faces,graph,pose,settings,colour_warp);
        else {
            const auto original=std::move(vertices.native_vertices);
            vertices.native_vertices.clear();
            for(std::size_t f=0;f<graph.faces.size();++f) {
                const auto& face=graph.faces[f];
                native_poses.push_back(native_explosion_pose(vertices.native_pose,face.normal.x,face.normal.y,face.normal.z,pose.explosion_progress));
                const auto first=faces.polygons[f][0];
                for(std::size_t c=0;c<face.vertex_indices.size();++c) {
                    const auto index=face.vertex_indices[c];
                    NativeTransformVertex vertex{};vertex.pose=UINT32_MAX;
                    if(index<original.size()) {vertex=original[index];vertex.pose=std::uint32_t(f);}
                    faces.corners[first+c][0]=std::uint32_t(vertices.native_vertices.size());
                    vertices.native_vertices.push_back(vertex);
                }
            }
        }
        const auto point_count=vertices.continuous?vertices.continuous_vertices.size():vertices.native_vertices.size();
        const auto pose_count=vertices.continuous?continuous_poses.size():native_poses.size();
        if(!point_count || !pose_count || point_count>1'000'000 || pose_count>1'000'000)
            throw std::runtime_error("Exploding axis caster storage limit exceeded");
        const std::array<Uint32,2> sizes{Uint32(point_count*16),Uint32(pose_count*80)};
        const std::array<const void*,2> data{vertices.continuous?static_cast<const void*>(vertices.continuous_vertices.data()):vertices.native_vertices.data(),
            vertices.continuous?static_cast<const void*>(continuous_poses.data()):native_poses.data()};
        for(unsigned i=0;i<2;++i) if(axis_ray_capacities[i]<sizes[i]) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,sizes[i],0};
            auto* buffer=SDL_CreateGPUBuffer(device,&info);require(buffer);
            if(axis_ray_buffers[i]) SDL_ReleaseGPUBuffer(device,axis_ray_buffers[i]);
            axis_ray_buffers[i]=buffer;axis_ray_capacities[i]=sizes[i];
        }
        const auto bytes=sizes[0]+sizes[1];
        if(axis_ray_upload_capacity<bytes) {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes,0};
            auto* upload=SDL_CreateGPUTransferBuffer(device,&info);require(upload);
            if(axis_ray_upload) SDL_ReleaseGPUTransferBuffer(device,axis_ray_upload);
            axis_ray_upload=upload;axis_ray_upload_capacity=bytes;
        }
        auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device,axis_ray_upload,true));require(mapped);
        std::memcpy(mapped,data[0],sizes[0]);std::memcpy(mapped+sizes[0],data[1],sizes[1]);
        SDL_UnmapGPUTransferBuffer(device,axis_ray_upload);
        auto* copy=SDL_BeginGPUCopyPass(command);require(copy);
        for(unsigned i=0;i<2;++i) {
            SDL_GPUTransferBufferLocation from{axis_ray_upload,i?sizes[0]:0};
            SDL_GPUBufferRegion to{axis_ray_buffers[i],0,sizes[i]};SDL_UploadToGPUBuffer(copy,&from,&to,true);
        }
        SDL_EndGPUCopyPass(copy);
        if(vertices.continuous) out.points=axis_ray_projection.enqueue_continuous(device,command,axis_ray_buffers[0],Uint32(point_count),axis_ray_buffers[1],Uint32(pose_count),&out.residuals);
        else {
            auto* projected=axis_ray_projection.enqueue_transformed(device,command,axis_ray_buffers[0],Uint32(point_count),axis_ray_buffers[1],Uint32(pose_count),&out.points);
            if(!projected) throw std::runtime_error(axis_ray_projection.status());
        }
        if(!out.points) throw std::runtime_error(axis_ray_projection.status());
        out.point_count=Uint32(point_count);out.mode=vertices.continuous?(out.residuals?2U:1U):0U;
        for(std::size_t f=0;f<faces.polygons.size();++f) if(faces.primitives[f]==PackedPrimitive::polygon) {
            const auto first=faces.polygons[f][0],count=faces.polygons[f][1];
            for(Uint32 c=1;c+1<count;++c) out.triangles.push_back({faces.corners[first][0],faces.corners[first+c][0],faces.corners[first+c+1][0],Uint32(f)});
        }
    }
    GpuRasterOutput billboard(void* next_device,void* command,const assets::Shape& shape,
        const RenderPose& pose,const RenderSettings& settings,std::uint32_t width,std::uint32_t height,
        bool surfaces,const GpuRasterOutput* background,std::array<std::uint32_t,2> raster_size={},std::array<float,2> jitter={},bool depth=false) {
        auto* next=static_cast<SDL_GPUDevice*>(next_device);
        if(device!=next){release();device=next;}
        const auto scale=settings.render_scale;
        const auto* texture=texture_for_colour(shape,pose.simple_sprite_colour,pose.colour_frame);
        if(!texture || pose.simple_sprite_world_size<=0 || pose.z<128)
            return raster.enqueue_row_spans(device,command,nullptr,0,width*scale,height*scale,surfaces,nullptr,true,background,false,0,0,0,nullptr,raster_size,jitter);
        for(auto value:{pose.x,pose.y,pose.z,pose.vanish_x,pose.vanish_y})
            if(!std::isfinite(value) || std::abs(value)>1000000)
                throw std::runtime_error("GPU billboard coordinate exceeds compensated range");
        if(!std::isfinite(settings.focal_length) || settings.focal_length<0 || settings.focal_length>4096
            || std::abs(std::int64_t(pose.effect_clip_left))>1000000 || std::abs(std::int64_t(pose.effect_clip_right))>1000000)
            throw std::runtime_error("Invalid GPU billboard focal length or clip");
        const auto required=(std::uint64_t(texture->u_mask)+1)*(std::uint64_t(texture->v_mask)+1);
        if(required>texture->texels.size() || texture->texels.size()>256U*1024*1024)
            throw std::runtime_error("Invalid GPU billboard texture size");
        const auto bytes=std::uint32_t((texture->texels.size()+3)&~std::size_t(3));
        if(capacities[9]<bytes) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,bytes,0};
            auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
            if(buffers[9]) SDL_ReleaseGPUBuffer(device,buffers[9]);
            buffers[9]=replacement;capacities[9]=bytes;
        }
        const auto upload_bytes=bytes+(depth?16U:0U);
        if(depth && geometry_capacity<1) {
            // This cache is shared with polygon plane generation, which may
            // reuse the one-plane allocation on a later draw.
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,16,0};
            auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
            if(geometry_planes) SDL_ReleaseGPUBuffer(device,geometry_planes);
            geometry_planes=replacement;geometry_capacity=1;
        }
        if(upload_capacity<upload_bytes) {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,upload_bytes,0};
            auto* replacement=SDL_CreateGPUTransferBuffer(device,&info);require(replacement);
            if(upload) SDL_ReleaseGPUTransferBuffer(device,upload);
            upload=replacement;upload_capacity=upload_bytes;
        }
        auto* mapped=SDL_MapGPUTransferBuffer(device,upload,true);require(mapped);
        std::memset(mapped,0,bytes);std::memcpy(mapped,texture->texels.data(),texture->texels.size());
        if(depth) {
            const std::array<float,4> plane{0,0,1,float(pose.z)};
            std::memcpy(static_cast<std::uint8_t*>(mapped)+bytes,plane.data(),16);
        }
        SDL_UnmapGPUTransferBuffer(device,upload);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);
        auto* copy=SDL_BeginGPUCopyPass(cmd);require(copy);
        SDL_GPUTransferBufferLocation from{upload,0};SDL_GPUBufferRegion to{buffers[9],0,bytes};
        SDL_UploadToGPUBuffer(copy,&from,&to,true);
        if(depth) {
            SDL_GPUTransferBufferLocation plane_from{upload,bytes};
            SDL_GPUBufferRegion plane_to{geometry_planes,0,16};
            SDL_UploadToGPUBuffer(copy,&plane_from,&plane_to,true);
        }
        SDL_EndGPUCopyPass(copy);
        if(!billboard_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            if(spirv) {
                info.format=SDL_GPU_SHADERFORMAT_SPIRV;
                info.code=billboard_shader::spirv;info.code_size=sizeof(billboard_shader::spirv);
            }
#if defined(_WIN32)
            else if(dxil) {
                info.format=SDL_GPU_SHADERFORMAT_DXIL;
                info.code=billboard_shader::dxil;info.code_size=sizeof(billboard_shader::dxil);
            }
#endif
#if defined(__APPLE__)
            else {
                info.format=SDL_GPU_SHADERFORMAT_MSL;
                info.code=reinterpret_cast<const Uint8*>(billboard_shader::metal);info.code_size=std::strlen(billboard_shader::metal);
            }
#else
            else throw std::runtime_error("No supported billboard shader format");
#endif
            info.entrypoint=(spirv||dxil)?"main":"main0";info.num_uniform_buffers=1;info.num_readwrite_storage_buffers=1;
            info.threadcount_x=64;info.threadcount_y=info.threadcount_z=1;
            billboard_pipeline=SDL_CreateGPUComputePipeline(device,&info);require(billboard_pipeline);
        }
        const auto rows=height*scale;
        if(billboard_rows<rows) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,rows*96,0};
            auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
            if(billboard_spans) SDL_ReleaseGPUBuffer(device,billboard_spans);
            billboard_spans=replacement;billboard_rows=rows;
        }
        struct Settings {
            std::array<Uint32,4> camera_lo{},camera_hi{},view_lo{},view_hi{};
            std::array<std::int32_t,4> bounds{};
            std::array<Uint32,4> material{};
        } config;
        static_assert(sizeof(Settings)==96);
        const std::array<double,4> camera{pose.x,pose.y,pose.z,settings.focal_length};
        const std::array<double,4> view{pose.vanish_x,pose.vanish_y,double(pose.simple_sprite_world_size),double(scale)};
        for(unsigned i=0;i<4;++i) {
            const auto camera_bits=std::bit_cast<std::uint64_t>(camera[i]);
            const auto view_bits=std::bit_cast<std::uint64_t>(view[i]);
            config.camera_lo[i]=Uint32(camera_bits);config.camera_hi[i]=Uint32(camera_bits>>32);
            config.view_lo[i]=Uint32(view_bits);config.view_hi[i]=Uint32(view_bits>>32);
        }
        config.bounds={pose.effect_clip_left,pose.effect_clip_right,int(width*scale),int(rows)};
        config.material={texture->u_mask,texture->v_mask,settings.colour_index_base,pose.palette_override?256U+*pose.palette_override:0U};
        SDL_PushGPUComputeUniformData(cmd,0,&config,sizeof(config));
        SDL_GPUStorageBufferReadWriteBinding binding{};binding.buffer=billboard_spans;binding.cycle=true;
        auto* pass=SDL_BeginGPUComputePass(cmd,nullptr,0,&binding,1);require(pass);
        SDL_BindGPUComputePipeline(pass,billboard_pipeline);SDL_DispatchGPUCompute(pass,(rows+63)/64,1,1);SDL_EndGPUComputePass(pass);
        const GpuGeometryDepthInput plane{geometry_planes,1,float(settings.focal_length*scale),float(settings.focal_length*scale),float(pose.vanish_x*scale),float(pose.vanish_y*scale),true};
        // Sprites remain unlit; their plane identifier is only a temporal guide.
        return raster.enqueue_row_spans(device,command,billboard_spans,1,width*scale,rows,surfaces,buffers[9],true,background,false,0,0,0,depth?&plane:nullptr,raster_size,jitter);
    }
    struct SurfaceSettings {
        Uint32 count,points,corners,fractional_camera;
        std::array<std::int32_t,4> row0{},row1{},row2{};
        Uint32 fractional_normal{},want_geometry{},padding[2]{};
    };
    static_assert(sizeof(SurfaceSettings)==80);
    void* emit_surface(SDL_GPUCommandBuffer* command,void* camera,const SurfaceSettings& settings) {
        if(!surface_pipeline) {
            const bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        const bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
            SDL_GPUComputePipelineCreateInfo info{};
            if(spirv) {
                info.format=SDL_GPU_SHADERFORMAT_SPIRV;
                info.code=surface_shader::spirv;info.code_size=sizeof(surface_shader::spirv);
            }
#if defined(_WIN32)
            else if(dxil) {
                info.format=SDL_GPU_SHADERFORMAT_DXIL;
                info.code=surface_shader::dxil;info.code_size=sizeof(surface_shader::dxil);
            }
#endif
#if defined(__APPLE__)
            else {
                info.format=SDL_GPU_SHADERFORMAT_MSL;
                info.code=reinterpret_cast<const Uint8*>(surface_shader::metal);info.code_size=std::strlen(surface_shader::metal);
            }
#else
            else throw std::runtime_error("No supported surface shader format");
#endif
            info.entrypoint=(spirv||dxil)?"main":"main0";info.num_uniform_buffers=1;
            info.num_readonly_storage_buffers=5;info.num_readwrite_storage_buffers=2;
            info.threadcount_x=32;info.threadcount_y=info.threadcount_z=1;
            surface_pipeline=SDL_CreateGPUComputePipeline(device,&info);require(surface_pipeline);
        }
        if(surface_capacity<settings.count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,settings.count*96,0};
            auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
            if(surface_materials) SDL_ReleaseGPUBuffer(device,surface_materials);
            surface_materials=replacement;surface_capacity=settings.count;
        }
        const auto plane_count=settings.want_geometry?settings.count:1U;
        if(geometry_capacity<plane_count) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,plane_count*16,0};
            auto* replacement=SDL_CreateGPUBuffer(device,&info);require(replacement);
            if(geometry_planes) SDL_ReleaseGPUBuffer(device,geometry_planes);
            geometry_planes=replacement;geometry_capacity=plane_count;
        }
        SDL_PushGPUComputeUniformData(command,0,&settings,sizeof(settings));
        SDL_GPUStorageBufferReadWriteBinding bindings[2]{};
        bindings[0].buffer=surface_materials;bindings[1].buffer=geometry_planes;
        bindings[0].cycle=bindings[1].cycle=true;
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,bindings,2);require(pass);
        SDL_BindGPUComputePipeline(pass,surface_pipeline);
        SDL_GPUBuffer* inputs[]{static_cast<SDL_GPUBuffer*>(camera),buffers[6],buffers[7],buffers[8],buffers[11]};
        SDL_BindGPUComputeStorageBuffers(pass,0,inputs,5);SDL_DispatchGPUCompute(pass,(settings.count+31)/32,1,1);SDL_EndGPUComputePass(pass);
        return surface_materials;
    }
#endif
};
GpuModel::GpuModel():impl_(std::make_unique<Impl>()){}
GpuModel::~GpuModel()=default;
const std::string& GpuModel::status()const noexcept{return impl_->status;}
void GpuModel::release_device()noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
GpuRasterOutput GpuModel::enqueue(void* device,void* command,const assets::Shape& shape,const RenderPose& unjittered_pose,
    const RenderSettings& settings,std::uint32_t width,std::uint32_t height,bool surface_metadata,const GpuRasterOutput* background,GpuModelDiagnostics* diagnostics,bool geometry_depth,GpuModelRaySource* ray_source,const RenderPose* previous_pose,std::array<float,2> raster_jitter,std::array<std::uint32_t,2> raster_size) {
    auto pose=unjittered_pose;
    if(diagnostics) *diagnostics={};
    if(ray_source) {const bool requested=ray_source->request_materials,reference=ray_source->reference_materials;
        *ray_source={};ray_source->request_materials=requested;ray_source->reference_materials=reference;}
    geometry_depth=geometry_depth || previous_pose;
    // Temporal depth must not opt unlit native shadows/helpers into lighting.
    // Plane preparation can run without publishing effects surface metadata.
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        if(SDL_getenv("STARFOX_TRACE_GPU_MODEL_DISPATCH")) std::cerr<<"model-enqueue: "<<shape.name<<'\n';
        if(!device || !command || !width || !height || width>32767 || height>32767 || settings.render_scale<1 || settings.render_scale>4)
            throw std::runtime_error("Invalid GPU model input");
        if(previous_pose && background)
            throw std::runtime_error("Temporal model draws must be merged after motion generation");
        const bool custom_raster=raster_size[0] || raster_size[1];
        if(custom_raster && (!raster_size[0] || !raster_size[1] || raster_size[0]>32767 || raster_size[1]>32767
            || !pose.continuous_geometry || !pose.subpixel_projection || pose.wave_mode))
            throw std::runtime_error("Custom model raster requires continuous geometry without wave effects");
        const auto raster_width=custom_raster?raster_size[0]:width*settings.render_scale;
        const auto raster_height=custom_raster?raster_size[1]:height*settings.render_scale;
        const double scale_x=double(raster_width)/width,scale_y=double(raster_height)/height;
        if(!std::isfinite(raster_jitter[0]) || !std::isfinite(raster_jitter[1]))
            throw std::runtime_error("Invalid model raster jitter");
        if(!pose.simple_scaled_sprite && (raster_jitter[0]!=0 || raster_jitter[1]!=0)) {
            if(!pose.continuous_geometry || !pose.subpixel_projection)
                throw std::runtime_error("Model jitter requires continuous subpixel projection");
            pose.vanish_x+=raster_jitter[0]/scale_x;
            pose.vanish_y+=raster_jitter[1]/scale_y;
        }
        if(pose.simple_scaled_sprite) {
            auto output=impl_->billboard(device,command,shape,pose,settings,width,height,surface_metadata,background,raster_size,raster_jitter,geometry_depth);
            if(!output.pixels) throw std::runtime_error(impl_->raster.status());
            if(previous_pose && output.geometry_depth && !background
                && previous_pose->simple_scaled_sprite
                && texture_for_colour(shape,previous_pose->simple_sprite_colour,previous_pose->colour_frame)
                    ==texture_for_colour(shape,pose.simple_sprite_colour,pose.colour_frame)) {
                // Match the source's rounded sprite rectangles, not a rigid
                // model transform: billboards always face the camera.
                const auto rectangle=[&](const RenderPose& p)->std::optional<std::array<float,4>> {
                    for(double v:{p.x,p.y,p.z,p.vanish_x,p.vanish_y})
                        if(!std::isfinite(v) || std::abs(v)>1000000) return {};
                    if(p.z<128 || p.simple_sprite_world_size<=0) return {};
                    const auto size=std::clamp(std::trunc(double(p.simple_sprite_world_size)*settings.focal_length/p.z),0.,240.);
                    if(size<1) return {};
                    const auto left=std::round(p.vanish_x)+std::trunc(p.x*settings.focal_length/p.z)-int(size)/2;
                    const auto top=std::round(p.vanish_y)+std::trunc(p.y*settings.focal_length/p.z)-int(size)/2;
                    return std::array<float,4>{float(size*scale_x),float(size*scale_y),float(left*scale_x),float(top*scale_y)};
                };
                const auto current=rectangle(pose),previous=rectangle(*previous_pose);
                if(current && previous) {
                    GpuProjection::MotionSurfaceSettings motion;
                    motion.width=output.width;motion.height=output.height;
                    std::copy(current->begin(),current->end(),motion.current_projection);
                    std::copy(previous->begin(),previous->end(),motion.previous_projection);
                    // Normalized coordinates within each rectangle correspond
                    // across depth/size changes; retain current Z for validity.
                    motion.jitter_x=raster_jitter[0];motion.jitter_y=raster_jitter[1];
                    output.motion=impl_->projection.enqueue_motion_surface(device,command,output.geometry_depth,motion);
                    if(!output.motion) throw std::runtime_error(impl_->projection.status());
                }
            }
            impl_->status="Whole-object billboard GPU resident";return output;
        }
        auto vertices=pack_projection(shape,pose,settings);
        const auto source_vertex_count=std::uint32_t(vertices.continuous?vertices.continuous_vertices.size():vertices.native_vertices.size());
        const auto axis_groups=pose.collapse_to_axis_line?pack_axis_groups(shape,pose.animation_frame)
            :std::array<std::vector<std::uint32_t>,2>{};
        const bool axis=pose.collapse_to_axis_line && !axis_groups[0].empty() && !shape.faces.empty();
        const bool colour_warp=pose.colour_warp && !pose.force_colour;
        auto material_pose=pose;material_pose.collapse_to_axis_line=false;
        // Collapsed axes bypass the source BSP entirely, just like the software
        // renderer. An unused source graph must not reject an otherwise valid line.
        PackedBsp graph;
        std::vector<std::uint32_t> axis_indices;std::uint32_t axis_ranges[4]{};
        if(axis) {
            for(unsigned group=0;group<2;++group) {
                if(axis_groups[group].size()>65536) throw std::runtime_error("Axis group exceeds reduction budget");
                axis_ranges[group*2]=std::uint32_t(axis_indices.size());axis_ranges[group*2+1]=std::uint32_t(axis_groups[group].size());
                axis_indices.insert(axis_indices.end(),axis_groups[group].begin(),axis_groups[group].end());
            }
            assets::Shape topology;topology.faces={shape.faces.front()};
            topology.faces[0].vertex_indices={0,1};topology.faces[0].sprite=false;topology.faces[0].visibility_index=-1;
            graph=pack_bsp(topology);vertices.visibility_faces={{{UINT32_MAX,UINT32_MAX,UINT32_MAX,1}}};
        } else graph=pack_bsp(shape,pose.explosion_progress!=0);
        auto faces=colour_warp?pack_warp_faces(shape,graph,material_pose,settings):pack_faces(shape,graph,material_pose,settings);
        PackedWarpTextures warp_textures;
        std::array<std::uint8_t,2480> warp_diffuse{};
        GpuWarpSettings warp_settings{};
        if(colour_warp) {
            warp_textures=pack_warp_textures(shape);faces.texels=warp_textures.texels;
            const auto shading=pack_warp_shading(shape,pose,settings,axis);
            warp_settings=shading.settings;warp_diffuse=shading.diffuse;
        }
        if(axis) {
            faces.polygons[0][2]=0;faces.polygons[0][3]=2;
            faces.materials[0].textured=0;faces.materials[0].tag=std::uint32_t(PixelLayer::three_d);
        }
        std::vector<NativeTransformPose> destruction_poses;
        std::vector<ContinuousTransformPose> continuous_destruction_poses;
        if(pose.explosion_progress && vertices.continuous && !axis) {
            continuous_destruction_poses=pack_continuous_fragments(vertices,faces,graph,pose,settings,colour_warp);
        }
        if(pose.explosion_progress && !vertices.continuous && !axis) {
            const auto original=std::move(vertices.native_vertices);
            vertices.native_vertices.clear();vertices.native_vertices.reserve(faces.corners.size()+graph.faces.size());
            destruction_poses.reserve(graph.faces.size()+1);
            vertices.visibility_faces={{{UINT32_MAX,UINT32_MAX,UINT32_MAX,1}}};
            vertices.visibility_faces.reserve(graph.faces.size()+1);
            for(std::size_t f=0;f<graph.faces.size();++f) {
                const auto& face=graph.faces[f];
                destruction_poses.push_back(native_explosion_pose(vertices.native_pose,
                    face.normal.x,face.normal.y,face.normal.z,pose.explosion_progress));
                auto& polygon=faces.polygons[f];polygon[2]=0;
                if(face.sprite && (colour_warp || faces.materials[f].textured) && face.vertex_indices.size()==1) {
                    const auto source=face.vertex_indices[0];
                    std::uint32_t centre=UINT32_MAX;
                    if(source<original.size()) {
                        auto vertex=original[source];
                        // The final pose is an unmodified object transform.
                        vertex.pose=std::uint32_t(graph.faces.size());
                        centre=std::uint32_t(vertices.native_vertices.size());
                        vertices.native_vertices.push_back(vertex);
                    }
                    polygon[2]=std::uint32_t(vertices.visibility_faces.size());
                    vertices.visibility_faces.push_back({centre,0,0,3});
                }
                for(std::size_t j=0;j<face.vertex_indices.size();++j) {
                    const auto source=face.vertex_indices[j];
                    auto& corner=faces.corners[polygon[0]+j];
                    if(source>=original.size()) {polygon[1]=0;corner[0]=UINT32_MAX;continue;}
                    auto vertex=original[source];vertex.pose=std::uint32_t(f);
                    corner[0]=std::uint32_t(vertices.native_vertices.size());vertices.native_vertices.push_back(vertex);
                }
            }
            destruction_poses.push_back(vertices.native_pose);
        }
        const auto vertex_count=std::uint32_t(vertices.continuous?vertices.continuous_vertices.size():vertices.native_vertices.size());
        if(!vertex_count || faces.polygons.empty()) {
            auto output=impl_->raster.enqueue_row_spans(device,command,nullptr,0,raster_width,raster_height,surface_metadata,nullptr,true,background);
            if(!output.pixels) throw std::runtime_error(impl_->raster.status());
            impl_->status="Empty GPU model cleared resident";return output;
        }
        const bool sequential_euler=vertices.continuous && !pose.use_rotation_matrix && !pose.explosion_progress;
        if(sequential_euler) {
            continuous_destruction_poses.assign(vertices.continuous_poses.begin(),vertices.continuous_poses.end());
            continuous_destruction_poses.insert(continuous_destruction_poses.end(),vertices.euler_operands.begin(),vertices.euler_operands.end());
            for(unsigned kind=0;kind<2;++kind) continuous_destruction_poses[kind].vanish[3]=2.f;
        }
        const auto polygons=std::uint32_t(faces.polygons.size()),slots=std::max(1U,graph.output_capacity);
        const auto visibility_count=std::uint32_t(vertices.visibility_faces.size());
        const std::array<std::uint32_t,4> tree{graph.root,0,slots,graph.work_limit};
        // Keep source-matrix fragment projection residuals through viewport
        // clipping. Euler packets retain their compensated projected result.
        const bool accurate_fragment_clip=axis || (pose.explosion_progress && vertices.continuous && pose.use_rotation_matrix && !pose.subpixel_projection);
        auto& near=impl_->near_scratch;
        near.assign(colour_warp?slots:polygons,{float(pose.vanish_x),float(pose.vanish_y),float(settings.focal_length),accurate_fragment_clip?1.f:(vertices.continuous?2.f:0.f)});
        auto& normals=impl_->normal_scratch;
        normals.clear();normals.reserve(graph.faces.size());
        for(const auto& face:graph.faces) normals.push_back({face.normal.x,face.normal.y,face.normal.z,0});
        const std::uint32_t zero=0;
        std::array<const void*,18> data{vertices.continuous?static_cast<const void*>(vertices.continuous_vertices.data()):vertices.native_vertices.data(),
            vertices.continuous?static_cast<const void*>(continuous_destruction_poses.empty()?vertices.continuous_poses.data():continuous_destruction_poses.data()):destruction_poses.empty()?&vertices.native_pose:destruction_poses.data(),
            vertices.visibility_faces.data(),graph.nodes.data(),graph.face_ids.data(),tree.data(),faces.corners.data(),faces.polygons.data(),
            faces.materials.data(),faces.texels.empty()?static_cast<const void*>(&zero):faces.texels.data(),near.data(),axis?static_cast<const void*>(axis_indices.data()):normals.data(),
            warp_diffuse.data(),pose.depth_colour_tables.data(),warp_textures.lookup.data(),warp_textures.textures.data(),warp_textures.coordinates.data(),normals.data()};
        const auto native_pose_count=std::uint32_t(std::max(std::size_t(1),destruction_poses.size()));
        const auto continuous_pose_count=std::uint32_t(std::max(std::size_t(4),continuous_destruction_poses.size()));
        std::array<std::uint32_t,18> sizes{vertex_count*16,vertices.continuous?continuous_pose_count*80U:native_pose_count*80U,visibility_count*16,
            std::uint32_t(graph.nodes.size()*32),std::uint32_t(graph.face_ids.size()*4),16,std::uint32_t(faces.corners.size()*16),polygons*16,
            polygons*96,std::uint32_t(std::max(std::size_t(4),faces.texels.size())),std::uint32_t(near.size()*16),axis?std::uint32_t(axis_indices.size()*4):polygons*16,
            colour_warp?2480U:0U,colour_warp?128U:0U,std::uint32_t(warp_textures.lookup.size()*4),std::uint32_t(warp_textures.textures.size()*16),std::uint32_t(warp_textures.coordinates.size()*8),colour_warp&&axis?polygons*16:0U};
        auto* next=static_cast<SDL_GPUDevice*>(device);
        if(impl_->device!=next){impl_->release();impl_->device=next;}
        std::uint64_t total=0;for(auto size:sizes) total+=size;
        if(total>256U*1024*1024) throw std::runtime_error("GPU model upload exceeds budget");
        for(unsigned i=0;i<sizes.size();++i) if(impl_->capacities[i]<std::max(4U,sizes[i])) {
            SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,std::max(4U,sizes[i]),0};
            auto* replacement=SDL_CreateGPUBuffer(next,&info);Impl::require(replacement);
            if(impl_->buffers[i]) SDL_ReleaseGPUBuffer(next,impl_->buffers[i]);
            impl_->buffers[i]=replacement;impl_->capacities[i]=info.size;
        }
        if(impl_->upload_capacity<total) {
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,std::uint32_t(total),0};
            auto* replacement=SDL_CreateGPUTransferBuffer(next,&info);Impl::require(replacement);
            if(impl_->upload) SDL_ReleaseGPUTransferBuffer(next,impl_->upload);
            impl_->upload=replacement;impl_->upload_capacity=info.size;
        }
        auto* mapped=static_cast<Uint8*>(SDL_MapGPUTransferBuffer(next,impl_->upload,true));Impl::require(mapped);
        std::uint32_t offset=0;for(unsigned i=0;i<sizes.size();++i){if(sizes[i]) std::memcpy(mapped+offset,data[i],sizes[i]);offset+=sizes[i];}
        SDL_UnmapGPUTransferBuffer(next,impl_->upload);
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);auto* copy=SDL_BeginGPUCopyPass(cmd);Impl::require(copy);offset=0;
        for(unsigned i=0;i<sizes.size();++i){
            if(sizes[i]){SDL_GPUTransferBufferLocation from{impl_->upload,offset};SDL_GPUBufferRegion to{impl_->buffers[i],0,sizes[i]};SDL_UploadToGPUBuffer(copy,&from,&to,true);}
            offset+=sizes[i];
        }
        SDL_EndGPUCopyPass(copy);const auto& b=impl_->buffers;void* camera=nullptr;
        void* point_residuals=nullptr;
        // Matrix-interpolated ordinary models also cross half-pixel edges.
        // Dropping their residuals changes coverage for distant small models.
        const bool retain_projection=vertices.continuous;
        auto* projected=vertices.continuous?impl_->projection.enqueue_continuous(device,command,b[0],vertex_count,b[1],continuous_pose_count,retain_projection?&point_residuals:nullptr,axis || settings.backface_culling)
            :impl_->projection.enqueue_transformed(device,command,b[0],vertex_count,b[1],native_pose_count,&camera);
        if(!projected) throw std::runtime_error(impl_->projection.status());
        // Shadow casters use the source mesh, not screen-space wave/wobble,
        // material warping or the collapsed laser-axis display substitute.
        // Preserve these records before axis reduction replaces raster inputs.
        auto* ray_points=vertices.continuous?projected:camera;
        auto* ray_residuals=point_residuals;
        if(axis) {
            auto* centres=impl_->projection.enqueue_axis_points(device,command,vertices.continuous?projected:camera,vertex_count,b[11],
                std::uint32_t(axis_indices.size()),axis_ranges,vertices.continuous,float(pose.vanish_x),float(pose.vanish_y),float(settings.focal_length),point_residuals,vertices.continuous?&point_residuals:nullptr,!vertices.continuous);
            if(!centres)throw std::runtime_error(impl_->projection.status());
            if(vertices.continuous)projected=centres;
            else {camera=centres;projected=impl_->projection.enqueue(device,command,camera,2);}
            if(!projected) throw std::runtime_error(impl_->projection.status());
        }
        auto* visible=vertices.continuous?impl_->projection.enqueue_continuous_visibility(command,b[2],visibility_count)
            :impl_->projection.enqueue_visibility(command,b[2],visibility_count);
        if(!visible) throw std::runtime_error(impl_->projection.status());
        const GpuBspSettings bs{1,std::uint32_t(graph.nodes.size()),visibility_count,std::uint32_t(graph.face_ids.size()),slots,{}};
        auto ordered=impl_->bsp.enqueue(device,command,b[3],visible,b[4],b[5],bs);
        if(!ordered.order) throw std::runtime_error(impl_->bsp.status());
        NativeClipSettings cs{polygons,axis?2U:vertex_count,std::uint32_t(faces.corners.size()),visibility_count,int(width),int(height),{}};
        const GpuSpanOrder order{ordered.order,ordered.results,0,slots,0};
        void* materials=b[8];
        const bool planar_depth=geometry_depth && !axis && !colour_warp && !pose.wave_mode && pose.wobble_mode==0;
        if((surface_metadata || planar_depth) && !axis) {
            Impl::SurfaceSettings ss{polygons,vertex_count,std::uint32_t(faces.corners.size()),vertices.continuous?1U:0U};
            ss.want_geometry=planar_depth?1U:0U;
            ss.fractional_normal=!pose.use_rotation_matrix || pose.subpixel_projection;
            if(ss.fractional_normal) {
                const auto& normal_pose=vertices.continuous_poses[1];
                for(unsigned i=0;i<3;++i) {ss.row0[i]=std::bit_cast<std::int32_t>(normal_pose.row0[i]);ss.row1[i]=std::bit_cast<std::int32_t>(normal_pose.row1[i]);ss.row2[i]=std::bit_cast<std::int32_t>(normal_pose.row2[i]);}
            } else for(unsigned i=0;i<3;++i) {ss.row0[i]=pose.rotation_matrix[i];ss.row1[i]=pose.rotation_matrix[3+i];ss.row2[i]=pose.rotation_matrix[6+i];}
            materials=impl_->emit_surface(cmd,vertices.continuous?projected:camera,ss);
        }
        void* clip_corners=b[6];void* clip_polygons=b[7];
        GpuWarpOutput reflection_warp_output{};
        if(colour_warp) {
            warp_settings.capacity=slots;warp_settings.face_count=polygons;warp_settings.visibility_count=visibility_count;
            warp_settings.seed=std::uint16_t(pose.projected_points_address+source_vertex_count*6U)
                |(pose.explosion_progress?0x80000000U:0U);
            warp_settings.corner_count=cs.corner_count;
            warp_settings.texture_count=std::uint32_t(warp_textures.textures.size());
            warp_settings.coordinate_count=std::uint32_t(warp_textures.coordinates.size());
            const GpuWarpInputs inputs{ordered.order,ordered.results,b[7],b[6],visible,materials,axis?b[17]:b[11],b[12],b[13],b[14],b[15],b[16]};
            const auto expanded=impl_->warp.enqueue(device,command,inputs,warp_settings);
            if(!expanded.polygons)throw std::runtime_error(impl_->warp.status());
            clip_corners=expanded.corners;clip_polygons=expanded.polygons;materials=expanded.materials;
            cs.polygon_count=slots;cs.corner_count=slots*32U;
            if(ray_source && ray_source->request_materials && !ray_source->reference_materials && !axis) {
                auto reflection_settings=warp_settings;reflection_settings.reflection_materials=true;
                reflection_warp_output=impl_->reflection_warp.enqueue(device,command,inputs,reflection_settings);
                if(!reflection_warp_output.polygons) throw std::runtime_error(impl_->reflection_warp.status());
            }
        }
        auto* clipped=impl_->clip.enqueue(device,command,projected,clip_corners,clip_polygons,visible,cs,vertices.continuous,
            vertices.continuous?b[10]:camera,vertices.continuous?cs.polygon_count:vertex_count,point_residuals,point_residuals?cs.point_count:0);
        if(!clipped) throw std::runtime_error(impl_->clip.status());
        void* masked_texels=nullptr;
        const bool repeated_rows=(pose.wobble_mode&1U)!=0;
        auto* spans=impl_->clip.enqueue_spans(command,materials,custom_raster || settings.render_scale>1,settings.render_scale,colour_warp?nullptr:&order,settings.wireframe_thickness,
            repeated_rows?b[9]:nullptr,repeated_rows?std::uint32_t(faces.texels.size()):0,repeated_rows?&masked_texels:nullptr,raster_size,
            diagnostics==nullptr);
        if(!spans) throw std::runtime_error(impl_->clip.status());
        const GpuGeometryDepthInput depth_input{impl_->geometry_planes,polygons,
            float(settings.focal_length*scale_x),float(settings.focal_length*scale_y),
            float((vertices.continuous?pose.vanish_x:vertices.native_pose.vanish[0])*scale_x),
            float((vertices.continuous?pose.vanish_y:vertices.native_pose.vanish[1])*scale_y)};
        auto output=impl_->raster.enqueue_row_spans(device,command,spans,slots,raster_width,raster_height,surface_metadata,repeated_rows?masked_texels:b[9],true,background,pose.wave_mode,pose.wave_offset,pose.animation_frame,
            repeated_rows?impl_->clip.mask_buffer_bytes():std::uint32_t(faces.texels.size()),planar_depth?&depth_input:nullptr);
        if(!output.pixels) throw std::runtime_error(impl_->raster.status());
        if(previous_pose && output.geometry_depth && !background && planar_depth
            && !pose.explosion_progress && !previous_pose->explosion_progress
            && !previous_pose->wave_mode && !previous_pose->wobble_mode
            && !previous_pose->simple_scaled_sprite && !previous_pose->collapse_to_axis_line
            && !(previous_pose->colour_warp && !previous_pose->force_colour)
            && (shape.frames.empty() || previous_pose->animation_frame%shape.frames.size()
                ==pose.animation_frame%shape.frames.size())) {
            const auto old=pack_projection(shape,*previous_pose,settings);
            if(auto motion=pack_motion_surface(vertices,old,output.width,output.height,settings.render_scale)) {
                for(unsigned i=0;i<4;++i) {
                    const float ratio=float((i%2?scale_y:scale_x)/settings.render_scale);
                    motion->current_projection[i]*=ratio;motion->previous_projection[i]*=ratio;
                }
                motion->current_projection[2]-=raster_jitter[0];
                motion->current_projection[3]-=raster_jitter[1];
                motion->jitter_x=raster_jitter[0];
                motion->jitter_y=raster_jitter[1];
                output.motion=impl_->projection.enqueue_motion_surface(device,command,output.geometry_depth,*motion);
                if(!output.motion) throw std::runtime_error(impl_->projection.status());
            }
        }
        if(diagnostics) *diagnostics={projected,clipped,cs.point_count,cs.polygon_count,vertices.continuous};
        if(ray_source && axis && pose.explosion_progress)
            impl_->exploding_axis_casters(cmd,shape,pose,settings,*ray_source);
        else if(ray_source) {
            ray_source->points=ray_points;
            ray_source->residuals=ray_residuals;
            ray_source->point_count=vertex_count;
            ray_source->mode=vertices.continuous?(ray_residuals?2U:1U):0U;
            if(axis) {
                for(std::size_t face=0;face<shape.faces.size();++face) {
                    const auto& f=shape.faces[face];
                    if(f.sprite || f.vertex_indices.size()<3) continue;
                    for(std::size_t c=1;c+1<f.vertex_indices.size();++c)
                        ray_source->triangles.push_back({f.vertex_indices[0],f.vertex_indices[c],f.vertex_indices[c+1],std::uint32_t(face)});
                }
            } else {
              std::vector<std::array<std::uint32_t,4>> material_topology;
              for(std::size_t face=0;face<faces.polygons.size();++face) {
                if(faces.primitives[face]!=PackedPrimitive::polygon) continue;
                const auto first=faces.polygons[face][0],count=faces.polygons[face][1];
                for(std::uint32_t corner=1;corner+1<count;++corner) {
                    ray_source->triangles.push_back({faces.corners[first][0],faces.corners[first+corner][0],
                        faces.corners[first+corner+1][0],std::uint32_t(face)});
                    if(ray_source->request_materials)
                        material_topology.push_back({first,first+corner,first+corner+1,std::uint32_t(face)});
                }
              }
              // Warp materials are resolved per occurrence on GPU. Keep all
              // original geometry for shadows; ray material lookup selects the
              // last emitted occurrence without CPU readback or placeholder ink.
              if(ray_source->request_materials && colour_warp && !ray_source->reference_materials) {
                  std::size_t triangle=0;
                  for(std::size_t face=0;face<faces.polygons.size();++face) {
                      if(faces.primitives[face]!=PackedPrimitive::polygon) continue;
                      for(std::uint32_t corner=1;corner+1<faces.polygons[face][1];++corner)
                          material_topology[triangle++]={0,corner,corner+1,std::uint32_t(face)|0x80000000U};
                  }
                  ray_source->materials.texels=faces.texels;
                  ray_source->materials_complete=true;
                  ray_source->material_corners=reflection_warp_output.corners;ray_source->material_polygons=reflection_warp_output.polygons;
                  ray_source->material_commands=reflection_warp_output.materials;
                  ray_source->material_corner_count=(slots+polygons)*32U;ray_source->material_count=slots+polygons;
                  ray_source->material_lookup=reflection_warp_output.face_lookup;ray_source->material_lookup_count=polygons;
                  ray_source->material_topology=std::move(material_topology);
              } else if(ray_source->request_materials && !colour_warp) {
                  if(ray_source->reference_materials)
                      ray_source->materials_complete=pack_ray_materials(faces,material_topology,ray_source->materials);
                  else {
                      ray_source->materials.texels=faces.texels;
                      ray_source->materials_complete=true; // GPU pack validates individual records.
                  }
                  ray_source->material_corners=clip_corners;ray_source->material_polygons=clip_polygons;
                  ray_source->material_commands=materials;
                  ray_source->material_corner_count=std::uint32_t(faces.corners.size());
                  ray_source->material_count=std::uint32_t(faces.polygons.size());
                  ray_source->material_topology=std::move(material_topology);
              }
            }
        }
        if(ray_source && ray_source->request_materials && axis) {
            // Axis effects rasterize lines, not filled polygons. Keep their
            // original caster geometry but explicitly exclude polygon mirrors.
            ray_source->reflection_excluded=true;ray_source->materials_complete=true;
            if(ray_source->reference_materials) {
                ray_source->materials.triangles.resize(ray_source->triangles.size());
                for(auto& material:ray_source->materials.triangles) material.reserved=1;
            }
        }
        impl_->status="Packed model geometry GPU resident";return output;
    }catch(const std::exception& error){if(ray_source) *ray_source={};impl_->status=error.what();}
#endif
    return {};
}
}
