#include "starfox/render/gpu_colour_warp.hpp"
#if defined(STARFOX_SDL_GPU_EFFECTS)
#include <SDL3/SDL.h>
#include "shaders/generated/colour_warp_portable.hpp"
#include "shaders/generated/warp_material_portable.hpp"
#include "shaders/generated/warp_expand_portable.hpp"
#include "shaders/generated/warp_reflection_portable.hpp"
#include "shaders/generated/warp_lookup_portable.hpp"
#include <cstring>
#include <stdexcept>
#endif
namespace starfox::render {
struct GpuColourWarp::Impl {
    std::string status{"GPU colour warp unavailable"};
#if defined(STARFOX_SDL_GPU_EFFECTS)
    SDL_GPUDevice* device{};
    std::array<SDL_GPUComputePipeline*,5> pipelines{};
    std::array<SDL_GPUBuffer*,10> buffers{};
    std::array<std::uint32_t,10> sizes{};
    static void require(bool value){if(!value)throw std::runtime_error(SDL_GetError());}
    ~Impl(){release();}
    void release() noexcept {
        for(auto* b:buffers)if(b)SDL_ReleaseGPUBuffer(device,b);
        for(auto* p:pipelines)if(p)SDL_ReleaseGPUComputePipeline(device,p);
        buffers={};pipelines={};sizes={};device=nullptr;
    }
    void initialize(SDL_GPUDevice* next,bool reflection) {
        if(device==next && pipelines[2] && (!reflection || pipelines[4]))return;
        if(device!=next) {release();device=next;}
        bool spirv=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_SPIRV)!=0;
        bool dxil=(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_DXIL)!=0;
        if(!spirv && !dxil && !(SDL_GetGPUShaderFormats(device)&SDL_GPU_SHADERFORMAT_MSL))throw std::runtime_error("GPU colour warp requires Vulkan, Metal or D3D12");
        const unsigned char* codes[]{colour_warp_shader::spirv,warp_material_shader::spirv,warp_expand_shader::spirv,warp_reflection_shader::spirv,warp_lookup_shader::spirv};
        const std::size_t bytes[]{sizeof(colour_warp_shader::spirv),sizeof(warp_material_shader::spirv),sizeof(warp_expand_shader::spirv),sizeof(warp_reflection_shader::spirv),sizeof(warp_lookup_shader::spirv)};
        const char* metal[]{colour_warp_shader::metal,warp_material_shader::metal,warp_expand_shader::metal,warp_reflection_shader::metal,warp_lookup_shader::metal};
        const unsigned char* native[]{colour_warp_shader::dxil,warp_material_shader::dxil,warp_expand_shader::dxil,warp_reflection_shader::dxil,warp_lookup_shader::dxil};
        const std::size_t native_bytes[]{sizeof(colour_warp_shader::dxil),sizeof(warp_material_shader::dxil),sizeof(warp_expand_shader::dxil),sizeof(warp_reflection_shader::dxil),sizeof(warp_lookup_shader::dxil)};
        const unsigned reads[]{4,6,8,3,2},writes[]{2,1,3,3,1},threads[]{1,64,32,1,1};
        for(unsigned i=0;i<(reflection?5U:3U);++i){
            if(pipelines[i]) continue;
            SDL_GPUComputePipelineCreateInfo info{};
            info.format=spirv?SDL_GPU_SHADERFORMAT_SPIRV:dxil?SDL_GPU_SHADERFORMAT_DXIL:SDL_GPU_SHADERFORMAT_MSL;
            info.code=spirv?codes[i]:dxil?native[i]:reinterpret_cast<const Uint8*>(metal[i]);
            info.code_size=spirv?bytes[i]:dxil?native_bytes[i]:std::strlen(metal[i]);info.entrypoint=(spirv||dxil)?"main":"main0";
            info.num_readonly_storage_buffers=reads[i];info.num_readwrite_storage_buffers=writes[i];info.num_uniform_buffers=1;
            info.threadcount_x=threads[i];info.threadcount_y=info.threadcount_z=1;
            pipelines[i]=SDL_CreateGPUComputePipeline(device,&info);require(pipelines[i]);
        }
    }
    void allocate(unsigned i,std::uint32_t bytes){
        if(sizes[i]>=bytes)return;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,bytes,0};
        auto* b=SDL_CreateGPUBuffer(device,&info);require(b);
        if(buffers[i])SDL_ReleaseGPUBuffer(device,buffers[i]);
        buffers[i]=b;sizes[i]=bytes;
    }
    void stage(SDL_GPUCommandBuffer* command,unsigned index,SDL_GPUBuffer** inputs,unsigned reads,
               unsigned first,unsigned writes,const void* settings,unsigned bytes,unsigned groups){
        SDL_PushGPUComputeUniformData(command,0,settings,bytes);
        SDL_GPUStorageBufferReadWriteBinding bindings[3]{};
        for(unsigned i=0;i<writes;++i){bindings[i].buffer=buffers[first+i];bindings[i].cycle=true;}
        auto* pass=SDL_BeginGPUComputePass(command,nullptr,0,bindings,writes);require(pass);
        SDL_BindGPUComputePipeline(pass,pipelines[index]);SDL_BindGPUComputeStorageBuffers(pass,0,inputs,reads);
        SDL_DispatchGPUCompute(pass,groups,1,1);SDL_EndGPUComputePass(pass);
    }
#endif
};
GpuColourWarp::GpuColourWarp():impl_(std::make_unique<Impl>()){}
GpuColourWarp::~GpuColourWarp()=default;
const std::string& GpuColourWarp::status()const noexcept{return impl_->status;}
void GpuColourWarp::release_device()noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS)
    impl_->release();
#endif
}
GpuWarpOutput GpuColourWarp::enqueue(void* device,void* command,const GpuWarpInputs& in,const GpuWarpSettings& s){
    // 32 corners * 16 bytes per occurrence; bound allocations and dispatches.
    if(!device || !command || !s.capacity || s.capacity>1048576 || !s.face_count){impl_->status="Invalid GPU warp settings";return {};}
    const auto expanded_capacity=std::uint64_t(s.capacity)+(s.reflection_materials?s.face_count:0U);
    if(expanded_capacity>1048576){impl_->status="GPU reflection warp capacity exceeded";return {};}
    for(auto count:s.shade_counts)if(count>62){impl_->status="Invalid GPU warp shade count";return {};}
#if defined(STARFOX_SDL_GPU_EFFECTS)
    try {
        void* pointers[]{in.order,in.traversal,in.polygons,in.corners,in.visibility,in.materials,in.normals,in.diffuse,in.depth_colours,in.texture_lookup,in.textures,in.coordinates};
        for(auto* input:pointers){
            if(!input)throw std::runtime_error("Missing GPU warp input");
            for(auto* output:impl_->buffers)if(input==output)throw std::runtime_error("GPU warp input aliases output");
        }
        impl_->initialize(static_cast<SDL_GPUDevice*>(device),s.reflection_materials);
        const auto count=std::uint32_t(expanded_capacity);
        const std::uint32_t sizes[]{s.capacity*4,8,count*16,count*16,count*512,count*96};
        for(unsigned i=0;i<6;++i)impl_->allocate(i,sizes[i]);
        auto buffer=[](void* p){return static_cast<SDL_GPUBuffer*>(p);};
        auto* cmd=static_cast<SDL_GPUCommandBuffer*>(command);auto& b=impl_->buffers;
        SDL_GPUBuffer* generate[]{buffer(in.order),buffer(in.traversal),buffer(in.polygons),buffer(in.visibility)};
        const std::array<std::uint32_t,4> generation{s.capacity,s.face_count,s.visibility_count,s.seed};
        impl_->stage(cmd,0,generate,4,0,2,generation.data(),sizeof(generation),1);
        auto* descriptors=b[0];auto* order=buffer(in.order);auto* result=b[1];
        if(s.reflection_materials) {
            impl_->allocate(6,count*4);impl_->allocate(7,8);impl_->allocate(8,count*4);
            SDL_GPUBuffer* reflection_inputs[]{b[0],b[1],buffer(in.order)};
            const std::array<std::uint32_t,4> reflection_settings{s.capacity,s.face_count,s.seed,0};
            impl_->stage(cmd,3,reflection_inputs,3,6,3,reflection_settings.data(),sizeof(reflection_settings),1);
            descriptors=b[6];result=b[7];order=b[8];
        }
        SDL_GPUBuffer* decode[]{descriptors,order,buffer(in.normals),buffer(in.diffuse),buffer(in.depth_colours),buffer(in.texture_lookup)};
        struct MaterialSettings {
            std::uint32_t count,faces,band,flags;
            std::array<std::int32_t,4> light;
            std::array<std::uint32_t,4> shades;
            std::uint32_t base,override_colour,forced,padding;
        } material{count,s.face_count,s.depth_band,s.flags,s.light,s.shade_counts,s.colour_base,s.override_colour,s.forced_colour,0};
        static_assert(sizeof(MaterialSettings)==64);
        impl_->stage(cmd,1,decode,6,2,1,&material,sizeof(material),(count+63)/64);
        SDL_GPUBuffer* expand[]{order,result,buffer(in.polygons),buffer(in.corners),buffer(in.materials),b[2],buffer(in.textures),buffer(in.coordinates)};
        const std::array<std::uint32_t,8> expansion{count,s.face_count,s.corner_count,s.texture_count,s.coordinate_count,s.colour_base,std::uint32_t(s.scroll_x),std::uint32_t(s.scroll_y)};
        impl_->stage(cmd,2,expand,8,3,3,expansion.data(),sizeof(expansion),(count+31)/32);
        if(s.reflection_materials) {
            impl_->allocate(9,s.face_count*4);
            SDL_GPUBuffer* lookup_inputs[]{b[3],b[4]};
            const std::array<std::uint32_t,4> lookup_settings{count,s.face_count,count*32,0};
            impl_->stage(cmd,4,lookup_inputs,2,9,1,lookup_settings.data(),sizeof(lookup_settings),1);
        }
        impl_->status="Ordered colour warp GPU resident";
        return {b[3],b[4],b[5],result,s.reflection_materials?b[9]:nullptr};
    }catch(const std::exception& e){impl_->status=e.what();}
#endif
    return {};
}
}
