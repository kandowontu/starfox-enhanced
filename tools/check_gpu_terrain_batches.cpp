#include "starfox/render/enhanced_terrain.hpp"
#include "starfox/render/gpu_scene.hpp"
#include <SDL3/SDL.h>
#include <deque>
#include <cstring>
#include <iostream>
#include <stdexcept>

int main() try {
    using namespace starfox::render;
    if(!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    auto* device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_MSL,true,nullptr);
    if(!device) throw std::runtime_error(SDL_GetError());
    unsigned cases=0;
    {
        GpuScene reference,merged;
        const auto ray_data=[&](GpuScene& scene) {
            const auto rays=scene.ray_geometry_output();
            if(!rays.complete || !rays.buffer || !rays.materials || rays.vertex_count!=12*32*3 || !rays.material_offset)
                throw std::runtime_error("Batched terrain lost ray casters/materials");
            std::pair<std::vector<std::array<float,4>>,std::vector<RayMaterial>> result;
            result.first.resize(rays.vertex_count);result.second.resize(rays.vertex_count/3);
            const unsigned vertices=rays.vertex_count*16,materials=unsigned(result.second.size()*sizeof(RayMaterial));
            SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,vertices+materials,0};
            auto* transfer=SDL_CreateGPUTransferBuffer(device,&info);
            auto* command=SDL_AcquireGPUCommandBuffer(device);
            if(!transfer || !command) throw std::runtime_error(SDL_GetError());
            auto* copy=SDL_BeginGPUCopyPass(command);
            SDL_GPUBufferRegion source{static_cast<SDL_GPUBuffer*>(rays.buffer),0,vertices};
            SDL_GPUTransferBufferLocation target{transfer,0};
            SDL_DownloadFromGPUBuffer(copy,&source,&target);
            source.offset=rays.material_offset;source.size=materials;target.offset=vertices;
            SDL_DownloadFromGPUBuffer(copy,&source,&target);SDL_EndGPUCopyPass(copy);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
            if(!fence || !SDL_WaitForGPUFences(device,true,&fence,1)) throw std::runtime_error(SDL_GetError());
            const auto* data=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(device,transfer,false));
            if(!data) throw std::runtime_error(SDL_GetError());
            std::memcpy(result.first.data(),data,vertices);std::memcpy(result.second.data(),data+vertices,materials);
            SDL_UnmapGPUTransferBuffer(device,transfer);SDL_ReleaseGPUFence(device,fence);SDL_ReleaseGPUTransferBuffer(device,transfer);
            return result;
        };
        for(unsigned kind=1;kind<=4;++kind) for(unsigned scale:{1U,2U,4U}) for(unsigned view=0;view<8;++view) {
            EnhancedTerrain terrain;
            std::deque<EnhancedTerrain::Batch> batches;
            std::vector<GpuSceneDraw> individual,combined;
            RenderSettings settings;settings.render_scale=scale;
            RenderPose base;base.x=11.25;base.y=120.5;base.z=512.25;
            base.vanish_x=112;base.vanish_y=70;
            base.continuous_geometry=base.subpixel_projection=base.terrain_geometry=base.use_rotation_matrix=true;
            base.rotation_matrix=view==0?starfox::simulation::MatrixQ15{32767,0,0,0,32767,0,0,0,32767}:
                view==1?starfox::simulation::MatrixQ15{32137,-6393,0,6393,32137,0,0,0,32767}:
                view==2?starfox::simulation::MatrixQ15{32137,6393,0,-6393,32137,0,0,0,32767}:
                starfox::simulation::MatrixQ15{23170,0,23170,0,32767,0,-23170,0,23170};
            if(view>=4) {
                base.x=base.y=0;base.vanish_y=96;
                if(view==4) {base.rotation_matrix={-32768,0,0,0,-32768,0,0,0,-32768};base.z=1024;}
                if(view==5) {base.rotation_matrix={0,0,-32768,0,-32768,0,32767,0,0};base.z=256;}
                if(view==6) base.z=32;
                if(view==7) {base.x=11.75;base.y=-4.25;base.z=256.125;}
            }
            const auto pose=[&](int x,int z) {
                auto p=base;const auto& m=p.rotation_matrix;
                p.x+=(double(x)*m[0]+double(z)*m[6])/32768.;
                p.y+=(double(x)*m[1]+double(z)*m[7])/32768.;
                p.z+=(double(x)*m[2]+double(z)*m[8])/32768.;
                return p;
            };
            for(int z=2;z>=0;--z) for(int x=-2;x<2;++x) {
                const auto& patch=terrain.patch(x,z,kind,0,{1,2,3,4});
                GpuModelDraw draw{&patch.shape,pose(x*EnhancedTerrain::patch_size,z*EnhancedTerrain::patch_size),settings,true};
                draw.geometry_depth=draw.ray_geometry=draw.ray_materials=true;individual.emplace_back(draw);
                if(batches.empty() || !batches.back().append(patch)) {
                    batches.emplace_back();
                    if(!batches.back().append(patch)) throw std::runtime_error("Fixture batch overflow");
                }
            }
            for(const auto& b:batches) {
                GpuModelDraw draw{&b.shape,pose(b.x*EnhancedTerrain::patch_size,b.z*EnhancedTerrain::patch_size),settings,true};
                draw.geometry_depth=draw.ray_geometry=draw.ray_materials=true;combined.emplace_back(draw);
            }
            const unsigned w=224*scale,h=192*scale;
            Framebuffer a(224,192,scale),b(224,192,scale);
            a.enable_layer_tags(true);b.enable_layer_tags(true);a.begin_write_coverage();b.begin_write_coverage();
            SurfaceBuffer sa(w,h),sb(w,h);
            if(!reference.render_resident(device,w,h,individual) || !reference.readback(a,&sa)) throw std::runtime_error(reference.status());
            if(!merged.render_resident(device,w,h,combined) || !merged.readback(b,&sb)) throw std::runtime_error(merged.status());
            const auto ra=ray_data(reference),rb=ray_data(merged);
            for(unsigned i=0;i<ra.first.size();++i) for(unsigned c=0;c<4;++c)
                if(!std::isfinite(rb.first[i][c]) || std::abs(ra.first[i][c]-rb.first[i][c])>1e-4f)
                    throw std::runtime_error("Terrain batching moved/reordered a ray triangle");
            for(unsigned i=0;i<ra.second.size();++i) {
                auto x=ra.second[i],y=rb.second[i];
                // Local face IDs change when models merge; shading must not.
                x.face=y.face=0;
                if(std::memcmp(&x,&y,sizeof(x))) throw std::runtime_error("Terrain batching changed reflection materials");
            }
            for(std::size_t i=0;i<a.pixels().size();++i) {
                const auto& x=sa.samples()[i];const auto& y=sb.samples()[i];
                const auto close=[](double x,double y){return std::isfinite(x) && std::isfinite(y) && std::abs(x-y)<=1e-4+std::abs(x)*1e-6;};
                if(a.pixels()[i]!=b.pixels()[i] || a.layer_tags()[i]!=b.layer_tags()[i]
                    || a.write_coverage()[i]!=b.write_coverage()[i] || x.valid!=y.valid
                    || (x.valid && (x.palette_index!=y.palette_index || !close(x.depth,y.depth)
                        || !close(x.normal_x,y.normal_x) || !close(x.normal_y,y.normal_y) || !close(x.normal_z,y.normal_z))))
                    throw std::runtime_error("Terrain batching mismatch: kind="+std::to_string(kind)+" scale="+std::to_string(scale)
                        +" view="+std::to_string(view)+" pixel="+std::to_string(i));
            }
            ++cases;
        }
    }
    SDL_DestroyGPUDevice(device);SDL_Quit();
    std::cout<<cases<<" merged/unmerged terrain cases: colors, coverage, layers, surface normals/depth, ray triangles and reflection materials preserved\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
