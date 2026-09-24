#import <Metal/Metal.h>
#include <SDL3/SDL.h>

#include "starfox/render/metal_hardware_rt.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

// These are supplied by the pinned SDL Metal backend patch. They expose no
// Metal struct layout to the game and preserve SDL's buffer-cycle tracking.
extern "C" {
void* SDL_StarfoxMetalDevice(SDL_GPUDevice*);
void* SDL_StarfoxMetalCommandBuffer(SDL_GPUCommandBuffer*);
void* SDL_StarfoxMetalBuffer(SDL_GPUBuffer*);
bool SDL_StarfoxMetalTrackBuffer(SDL_GPUCommandBuffer*, SDL_GPUBuffer*);
bool SDL_StarfoxMetalPrepareWrite(SDL_GPUDevice*, SDL_GPUCommandBuffer*, SDL_GPUBuffer*);
}

namespace starfox::render::shadows {
namespace {
struct alignas(16) Float4 { float x{}, y{}, z{}, w{}; };
struct alignas(16) Parameters {
    std::uint32_t width{}, height{};
    float focal_x{}, focal_y{};
    float center_x{}, center_y{}, has_ground{}, unused{};
    Float4 ground_point{}, ground_normal{};
    std::array<Float4,8> lights{};
};
static_assert(sizeof(Parameters)==192);
struct alignas(16) ReflectionParameters {
    std::uint32_t width{},height{},quality{},metallic{};
    float focal_x{},focal_y{},center_x{},center_y{};
    float roughness{},has_ground{};
    std::uint32_t environment{},texel_count{};
    Float4 ground_point{},ground_normal{};
};
static_assert(sizeof(ReflectionParameters)==80);

constexpr char kShadowShader[] = R"METAL(
#include <metal_stdlib>
#include <metal_raytracing>
using namespace metal;
using namespace metal::raytracing;
struct Parameters {
    uint width, height;
    float focal_x, focal_y;
    float center_x, center_y, has_ground, unused;
    float4 ground_point, ground_normal;
    float4 lights[8];
};
kernel void starfox_hardware_shadow(
    primitive_acceleration_structure scene [[buffer(0)]],
    device uint* output [[buffer(1)]],
    constant Parameters& p [[buffer(2)]],
    uint id [[thread_position_in_grid]]) {
    if (id >= p.width*p.height) return;
    uint x=id%p.width, y=id/p.width;
    float3 direction=float3((float(x)+0.5f-p.center_x)/p.focal_x,
        (float(y)+0.5f-p.center_y)/p.focal_y,1.0f);
    intersector<triangle_data> tracer;
    ray primary(float3(0.0f),direction,1.0f,65536.0f);
    auto hit=tracer.intersect(primary,scene);
    float receiver=hit.type==intersection_type::triangle ? hit.distance : 65536.0f;
    if (p.has_ground>0.5f) {
        float denominator=dot(direction,p.ground_normal.xyz);
        if (abs(denominator)>1.e-10f) {
            float ground=dot(p.ground_point.xyz,p.ground_normal.xyz)/denominator;
            if (ground>1.0f && ground<receiver) receiver=ground;
        }
    }
    if (receiver>=65536.0f) {output[id]=0;return;}
    float3 point=direction*receiver;
    float bias=max(0.1f,receiver*1.e-5f);
    uint blocked=0;
    tracer.accept_any_intersection(true);
    for(uint sample=0;sample<8;++sample) {
        ray shadow(point,p.lights[sample].xyz,bias,65536.0f);
        auto obstacle=tracer.intersect(shadow,scene);
        blocked+=obstacle.type!=intersection_type::none;
    }
    output[id]=160u*blocked/8u;
}
)METAL";

constexpr char kReflectionShader[] = R"METAL(
#include <metal_stdlib>
#include <metal_raytracing>
using namespace metal;
using namespace metal::raytracing;
struct Parameters {
    uint width,height,quality,metallic;
    float focal_x,focal_y,center_x,center_y;
    float roughness,has_ground;
    uint environment,texel_count;
    float4 ground_point,ground_normal;
};
struct RayMaterial {
    float uv[6];
    uint textured,dither,even,odd,colour_base,face;
    uint offset,u_mask,v_mask,reserved;
};
float3 rgb(uint packed) {
    return float3(float(packed&255u),float((packed>>8)&255u),
        float((packed>>16)&255u))/255.0f;
}
uint rgba(float3 colour) {
    uint3 c=uint3(round(clamp(colour,0.0f,1.0f)*255.0f));
    return c.x|(c.y<<8)|(c.z<<16)|0xff000000u;
}
uint palette_hit(uint primitive,float2 bary,uint2 pixel,
    device const RayMaterial* materials,device const uint* palette,
    device const uchar* texels,uint texel_count) {
    const RayMaterial material=materials[primitive];
    uint index=((pixel.x+pixel.y)&1u)&&material.dither!=0u
        ?material.odd:material.even;
    if(material.textured!=0u) {
        float2 uv=float2(material.uv[0],material.uv[1])*(1.0f-bary.x-bary.y)
            +float2(material.uv[2],material.uv[3])*bary.x
            +float2(material.uv[4],material.uv[5])*bary.y;
        uint2 tile=uint2(int2(floor(uv)))&uint2(material.u_mask,material.v_mask);
        uint at=material.offset+tile.y*(material.u_mask+1u)+tile.x;
        if(at<texel_count) index=(uint(texels[at])+material.colour_base)&255u;
    }
    return palette[index&255u];
}
float hash(uint n) {
    n=(n^61u)^(n>>16u);n*=9u;n^=n>>4u;n*=0x27d4eb2du;n^=n>>15u;
    return float(n&65535u)/65535.0f;
}
kernel void starfox_hardware_reflection(
    primitive_acceleration_structure scene [[buffer(0)]],
    device uint* output [[buffer(1)]],
    constant Parameters& p [[buffer(2)]],
    device const float4* vertices [[buffer(3)]],
    device const RayMaterial* materials [[buffer(4)]],
    device const uint* palette [[buffer(5)]],
    device const uchar* texels [[buffer(6)]],
    uint id [[thread_position_in_grid]]) {
    if(id>=p.width*p.height) return;
    uint2 pixel=uint2(id%p.width,id/p.width);
    float3 direction=float3((float(pixel.x)+0.5f-p.center_x)/p.focal_x,
        (float(pixel.y)+0.5f-p.center_y)/p.focal_y,1.0f);
    intersector<triangle_data> tracer;
    ray primary(float3(0.0f),direction,1.0f,65536.0f);
    auto hit=tracer.intersect(primary,scene);
    float distance=hit.type==intersection_type::triangle?hit.distance:65536.0f;
    bool ground_hit=false;
    if(p.has_ground>0.5f) {
        float denominator=dot(direction,p.ground_normal.xyz);
        if(abs(denominator)>1.e-10f) {
            float ground=dot(p.ground_point.xyz,p.ground_normal.xyz)/denominator;
            if(ground>1.0f && ground<distance) {
                distance=ground;ground_hit=true;
            }
        }
    }
    if(distance>=65536.0f) {output[id]=0u;return;}
    float3 normal;
    if(ground_hit) normal=normalize(p.ground_normal.xyz);
    else {
        uint first=hit.primitive_id*3u;
        float3 a=vertices[first].xyz,b=vertices[first+1u].xyz,c=vertices[first+2u].xyz;
        normal=normalize(cross(b-a,c-a));
    }
    if(dot(normal,direction)>0.0f) normal=-normal;
    float3 point=direction*distance;
    float3 reflected=normalize(reflect(normalize(direction),normal));
    uint rays=p.quality>=3u?4u:p.quality==2u?2u:1u;
    float3 sum=float3(0.0f);
    for(uint sample=0u;sample<rays;++sample) {
        float3 cast=reflected;
        if(sample>0u && p.roughness>0.0f) {
            float3 tangent=normalize(cross(reflected,
                abs(reflected.y)<0.9f?float3(0.0f,1.0f,0.0f):float3(1.0f,0.0f,0.0f)));
            float3 bitangent=cross(reflected,tangent);
            float angle=6.2831853f*hash(id*17u+sample*101u);
            float spread=p.roughness*0.08f*sqrt(hash(id*29u+sample*47u));
            cast=normalize(reflected+spread*(cos(angle)*tangent+sin(angle)*bitangent));
        }
        ray secondary(point,cast,max(0.1f,distance*1.e-5f),65536.0f);
        auto bounced=tracer.intersect(secondary,scene);
        uint colour=bounced.type==intersection_type::triangle
            ?palette_hit(bounced.primitive_id,bounced.triangle_barycentric_coord,
                pixel,materials,palette,texels,p.texel_count)
            :p.environment;
        sum+=rgb(colour);
    }
    float3 value=sum/float(rays);
    if(p.metallic==2u) value*=float3(1.0f,0.82f,0.34f);
    else if(p.metallic==3u) value*=float3(1.0f,0.59f,0.38f);
    output[id]=rgba(value);
}
)METAL";

[[nodiscard]] Float4 as_float4(Vec3 v) {
    return {float(v.x),float(v.y),float(v.z),0.0F};
}
[[nodiscard]] std::array<Float4,8> light_samples(Vec3 light) {
    const auto length=std::sqrt(dot(light,light));
    light=light*(1.0/length);
    const auto reference=std::abs(light.y)<.9?Vec3{0,1,0}:Vec3{1,0,0};
    auto tangent=cross(light,reference);
    tangent=tangent*(1.0/std::sqrt(dot(tangent,tangent)));
    const auto bitangent=cross(light,tangent);
    std::array<Float4,8> result{};
    for(unsigned i=0;i<result.size();++i) {
        const auto radius=.015*std::sqrt((i+.5)/result.size());
        const auto angle=i*2.399963229728653;
        auto direction=light+tangent*(radius*std::cos(angle))
            +bitangent*(radius*std::sin(angle));
        direction=direction*(1.0/std::sqrt(dot(direction,direction)));
        result[i]=as_float4(direction);
    }
    return result;
}
[[nodiscard]] std::string metal_error(NSError* error) {
    return error ? std::string(error.localizedDescription.UTF8String)
                 : std::string("Metal ray-tracing operation failed");
}
[[nodiscard]] bool hardware_ray_tracing(id<MTLDevice> device) {
    // Apple7/8 can expose ray intersections without dedicated RT hardware.
    // Apple identifies Apple9 (A17 Pro/M3) and later as the hardware RT family.
    if(!device || !device.supportsRaytracing) return false;
    if(@available(iOS 17.0, macOS 14.0, *))
        return [device supportsFamily:MTLGPUFamilyApple9];
    return false;
}
} // namespace

struct MetalHardwareRt::Impl {
    SDL_GPUDevice* device{}; // Borrowed from SDL renderer.
    id<MTLDevice> metal;
    id<MTLComputePipelineState> shadow_pipeline;
    id<MTLComputePipelineState> reflection_pipeline;
    SDL_GPUBuffer* output{};
    std::uint32_t output_capacity{};
    SDL_GPUBuffer* reflection_buffer{};
    std::uint32_t reflection_capacity{};
    struct InFlight {
        SDL_GPUFence* fence{};
        id<MTLBuffer> cpu_vertices;
        id<MTLBuffer> cpu_materials;
        id<MTLBuffer> palette;
        id<MTLBuffer> texels;
        id<MTLBuffer> scratch;
        id<MTLAccelerationStructure> acceleration;
    };
    std::array<InFlight,3> inflight{};
    unsigned serial{};
    GpuShadowOutput shadow{};
    GpuReflectionOutput reflection{};
    std::string status{"Metal hardware ray tracing not initialized"};

    ~Impl() {
        if(!device) return;
        for(auto& frame:inflight) {
            if(frame.fence) {
                SDL_WaitForGPUFences(device,true,&frame.fence,1);
                SDL_ReleaseGPUFence(device,frame.fence);
            }
        }
        if(output) SDL_ReleaseGPUBuffer(device,output);
        if(reflection_buffer) SDL_ReleaseGPUBuffer(device,reflection_buffer);
    }
    void initialize(SDL_GPUDevice* next) {
        if(device==next && shadow_pipeline) return;
        if(device && device!=next) throw std::runtime_error("Metal device changed before release");
        if(!next || !SDL_GetGPUDeviceDriver(next)
            || std::strcmp(SDL_GetGPUDeviceDriver(next),"metal")!=0)
            throw std::runtime_error("Metal GPU renderer is not active");
        metal=(__bridge id<MTLDevice>)SDL_StarfoxMetalDevice(next);
        if(!hardware_ray_tracing(metal))
            throw std::runtime_error("This Metal device has no dedicated ray-tracing hardware");
        NSError* error=nil;
        NSString* source=[NSString stringWithUTF8String:kShadowShader];
        id<MTLLibrary> library=[metal newLibraryWithSource:source options:nil error:&error];
        if(!library) throw std::runtime_error(metal_error(error));
        id<MTLFunction> function=[library newFunctionWithName:@"starfox_hardware_shadow"];
        shadow_pipeline=[metal newComputePipelineStateWithFunction:function error:&error];
        if(!shadow_pipeline) throw std::runtime_error(metal_error(error));
        device=next;
        status="Metal hardware acceleration structures and ray intersector";
    }
    void ensure_reflection_pipeline() {
        if(reflection_pipeline) return;
        NSError* error=nil;
        NSString* source=[NSString stringWithUTF8String:kReflectionShader];
        id<MTLLibrary> library=[metal newLibraryWithSource:source options:nil error:&error];
        if(!library) throw std::runtime_error(metal_error(error));
        id<MTLFunction> function=[library newFunctionWithName:@"starfox_hardware_reflection"];
        reflection_pipeline=[metal newComputePipelineStateWithFunction:function error:&error];
        if(!reflection_pipeline) throw std::runtime_error(metal_error(error));
    }
    void wait_slot(InFlight& slot) {
        if(slot.fence) {
            if(!SDL_WaitForGPUFences(device,true,&slot.fence,1))
                throw std::runtime_error(SDL_GetError());
            SDL_ReleaseGPUFence(device,slot.fence);slot.fence=nullptr;
        }
        slot.cpu_vertices=nil;slot.cpu_materials=nil;
        slot.palette=nil;slot.texels=nil;
        slot.scratch=nil;slot.acceleration=nil;
    }
    void ensure_output(std::uint32_t pixels) {
        if(output && output_capacity>=pixels) return;
        SDL_GPUBufferCreateInfo info{};
        info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        info.size=pixels*4U;
        auto* next=SDL_CreateGPUBuffer(device,&info);
        if(!next) throw std::runtime_error(SDL_GetError());
        if(output) SDL_ReleaseGPUBuffer(device,output);
        output=next;output_capacity=pixels;
    }
    void ensure_reflection_output(std::uint32_t pixels) {
        if(reflection_buffer && reflection_capacity>=pixels) return;
        SDL_GPUBufferCreateInfo info{};
        info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        info.size=pixels*4U;
        auto* next=SDL_CreateGPUBuffer(device,&info);
        if(!next) throw std::runtime_error(SDL_GetError());
        if(reflection_buffer) SDL_ReleaseGPUBuffer(device,reflection_buffer);
        reflection_buffer=next;reflection_capacity=pixels;
    }
};

MetalHardwareRt::MetalHardwareRt():impl_(std::make_unique<Impl>()) {}
MetalHardwareRt::~MetalHardwareRt()=default;
bool MetalHardwareRt::available(void* raw) const noexcept {
    auto* device=static_cast<SDL_GPUDevice*>(raw);
    if(!device || !SDL_GetGPUDeviceDriver(device)
        || std::strcmp(SDL_GetGPUDeviceDriver(device),"metal")!=0) return false;
    auto metal=(__bridge id<MTLDevice>)SDL_StarfoxMetalDevice(device);
    return hardware_ray_tracing(metal);
}
bool MetalHardwareRt::render_shadows(void* raw,const Scene& scene,
    const render::GpuScene::RayGeometryOutput* resident_geometry,
    Camera camera,Vec3 light,std::optional<ReceiverPlane> ground) {
    impl_->shadow={};
    try {
        auto* device=static_cast<SDL_GPUDevice*>(raw);
        const auto pixels=std::uint64_t(camera.width)*camera.height;
        const auto length=std::sqrt(dot(light,light));
        if(!pixels || pixels>std::numeric_limits<std::uint32_t>::max()/4U || !std::isfinite(length)
            || length<1.e-10 || camera.focal_length<=0
            || camera.vertical_focal_length()<=0) return false;
        impl_->initialize(device);
        auto& slot=impl_->inflight[impl_->serial++%impl_->inflight.size()];
        impl_->wait_slot(slot);
        id<MTLBuffer> vertices=nil;
        std::uint32_t vertex_count{};
        const bool resident=resident_geometry && resident_geometry->complete
            && resident_geometry->device==raw && resident_geometry->buffer
            && resident_geometry->vertex_count>=3
            && resident_geometry->vertex_count%3==0;
        if(resident) {
            vertex_count=resident_geometry->vertex_count;
            vertices=(__bridge id<MTLBuffer>)SDL_StarfoxMetalBuffer(
                static_cast<SDL_GPUBuffer*>(resident_geometry->buffer));
        } else {
            if(scene.triangle_count()>std::numeric_limits<std::uint32_t>::max()/3U) return false;
            std::vector<Float4> packed;
            packed.reserve(scene.triangle_count()*3U);
            for(const auto& t:scene.triangles()) {
                packed.push_back(as_float4(t.a));
                packed.push_back(as_float4(t.b));
                packed.push_back(as_float4(t.c));
            }
            vertex_count=std::uint32_t(packed.size());
            if(!vertex_count) return false;
            slot.cpu_vertices=[impl_->metal newBufferWithBytes:packed.data()
                length:packed.size()*sizeof(Float4) options:MTLResourceStorageModeShared];
            vertices=slot.cpu_vertices;
        }
        if(!vertices) return false;
        impl_->ensure_output(std::uint32_t(pixels));
        auto* command=SDL_AcquireGPUCommandBuffer(device);
        if(!command) throw std::runtime_error(SDL_GetError());
        bool submitted=false;
        try {
            if(resident && !SDL_StarfoxMetalTrackBuffer(command,
                static_cast<SDL_GPUBuffer*>(resident_geometry->buffer)))
                throw std::runtime_error("Could not retain Metal ray geometry");
            if(!SDL_StarfoxMetalPrepareWrite(device,command,impl_->output))
                throw std::runtime_error("Could not cycle Metal ray output");
            auto target=(__bridge id<MTLBuffer>)SDL_StarfoxMetalBuffer(impl_->output);
            auto native=(__bridge id<MTLCommandBuffer>)SDL_StarfoxMetalCommandBuffer(command);
            if(!target || !native) throw std::runtime_error("Missing native Metal command resource");
            auto* triangles=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
            triangles.vertexBuffer=vertices;
            triangles.vertexStride=sizeof(Float4);
            triangles.triangleCount=vertex_count/3U;
            auto* descriptor=[MTLPrimitiveAccelerationStructureDescriptor descriptor];
            descriptor.geometryDescriptors=@[triangles];
            auto sizes=[impl_->metal accelerationStructureSizesWithDescriptor:descriptor];
            slot.acceleration=[impl_->metal newAccelerationStructureWithSize:sizes.accelerationStructureSize];
            slot.scratch=[impl_->metal newBufferWithLength:sizes.buildScratchBufferSize
                options:MTLResourceStorageModePrivate];
            if(!slot.acceleration || !slot.scratch)
                throw std::runtime_error("Metal acceleration-structure allocation failed");
            auto builder=[native accelerationStructureCommandEncoder];
            [builder buildAccelerationStructure:slot.acceleration descriptor:descriptor
                scratchBuffer:slot.scratch scratchBufferOffset:0];
            [builder endEncoding];
            Parameters p{};
            p.width=camera.width;p.height=camera.height;
            p.focal_x=float(camera.focal_length);
            p.focal_y=float(camera.vertical_focal_length());
            p.center_x=float(camera.center_x);p.center_y=float(camera.center_y);
            p.has_ground=ground?1.0F:0.0F;
            if(ground) {
                p.ground_point=as_float4(ground->point);
                p.ground_normal=as_float4(ground->normal);
            }
            p.lights=light_samples(light);
            auto encoder=[native computeCommandEncoder];
            [encoder setComputePipelineState:impl_->shadow_pipeline];
            [encoder setAccelerationStructure:slot.acceleration atBufferIndex:0];
            [encoder setBuffer:target offset:0 atIndex:1];
            [encoder setBytes:&p length:sizeof(p) atIndex:2];
            [encoder useResource:slot.acceleration usage:MTLResourceUsageRead];
            const auto grid=MTLSizeMake(NSUInteger(pixels),1,1);
            const auto group=MTLSizeMake(std::min<NSUInteger>(64,
                impl_->shadow_pipeline.maxTotalThreadsPerThreadgroup),1,1);
            [encoder dispatchThreads:grid threadsPerThreadgroup:group];
            [encoder endEncoding];
            slot.fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
            submitted=true;
            if(!slot.fence) throw std::runtime_error(SDL_GetError());
        } catch(...) {
            if(!submitted) SDL_CancelGPUCommandBuffer(command);
            throw;
        }
        impl_->shadow={device,impl_->output,camera.width,camera.height,0};
        impl_->status=resident
            ?"Metal hardware rays from resident GPU casters"
            :"Metal hardware rays from CPU fallback casters";
        return true;
    } catch(const std::exception& error) {
        impl_->status=error.what();
        return false;
    }
}
GpuShadowOutput MetalHardwareRt::shadow_output() const noexcept {return impl_->shadow;}
bool MetalHardwareRt::render_reflections(void* raw,
    const render::GpuScene::RayGeometryOutput& geometry,
    Camera camera,std::span<const std::uint32_t,256> palette,
    std::uint32_t environment,std::uint8_t quality,float roughness,
    std::uint32_t metallic,std::optional<ReceiverPlane> ground) {
    impl_->reflection={};
    try {
        auto* device=static_cast<SDL_GPUDevice*>(raw);
        const auto pixels=std::uint64_t(camera.width)*camera.height;
        if(!quality || quality>3 || !pixels || pixels>std::numeric_limits<std::uint32_t>::max()/4U
            || camera.focal_length<=0 || camera.vertical_focal_length()<=0
            || !std::isfinite(roughness) || roughness<0
            || !geometry.complete || geometry.device!=raw || !geometry.buffer
            || geometry.vertex_count<3 || geometry.vertex_count%3
            || !geometry.materials
            || geometry.materials->triangles.size()!=geometry.vertex_count/3U
            || geometry.materials->texels.size()>std::numeric_limits<std::uint32_t>::max()) return false;
        impl_->initialize(device);
        impl_->ensure_reflection_pipeline();
        auto& slot=impl_->inflight[impl_->serial++%impl_->inflight.size()];
        impl_->wait_slot(slot);
        impl_->ensure_reflection_output(std::uint32_t(pixels));
        auto* command=SDL_AcquireGPUCommandBuffer(device);
        if(!command) throw std::runtime_error(SDL_GetError());
        bool submitted=false;
        try {
            auto* source=static_cast<SDL_GPUBuffer*>(geometry.buffer);
            if(!SDL_StarfoxMetalTrackBuffer(command,source)
                || !SDL_StarfoxMetalPrepareWrite(device,command,impl_->reflection_buffer))
                throw std::runtime_error("Could not retain Metal reflection buffers");
            auto vertices=(__bridge id<MTLBuffer>)SDL_StarfoxMetalBuffer(source);
            auto target=(__bridge id<MTLBuffer>)SDL_StarfoxMetalBuffer(impl_->reflection_buffer);
            auto native=(__bridge id<MTLCommandBuffer>)SDL_StarfoxMetalCommandBuffer(command);
            if(!vertices || !target || !native)
                throw std::runtime_error("Missing native Metal reflection resource");
            auto* triangles=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
            triangles.vertexBuffer=vertices;
            triangles.vertexStride=sizeof(Float4);
            triangles.triangleCount=geometry.vertex_count/3U;
            auto* descriptor=[MTLPrimitiveAccelerationStructureDescriptor descriptor];
            descriptor.geometryDescriptors=@[triangles];
            const auto sizes=[impl_->metal accelerationStructureSizesWithDescriptor:descriptor];
            slot.acceleration=[impl_->metal newAccelerationStructureWithSize:sizes.accelerationStructureSize];
            slot.scratch=[impl_->metal newBufferWithLength:sizes.buildScratchBufferSize
                options:MTLResourceStorageModePrivate];
            slot.palette=[impl_->metal newBufferWithBytes:palette.data()
                length:palette.size_bytes() options:MTLResourceStorageModeShared];
            const std::uint8_t blank=0;
            const auto& texels=geometry.materials->texels;
            slot.texels=[impl_->metal newBufferWithBytes:texels.empty()?&blank:texels.data()
                length:std::max<std::size_t>(1,texels.size())
                options:MTLResourceStorageModeShared];
            id<MTLBuffer> materials=vertices;
            NSUInteger material_offset=geometry.material_offset;
            if(!material_offset) {
                const auto& cpu=geometry.materials->triangles;
                slot.cpu_materials=[impl_->metal newBufferWithBytes:cpu.data()
                    length:cpu.size()*sizeof(render::RayMaterial)
                    options:MTLResourceStorageModeShared];
                materials=slot.cpu_materials;
            }
            if(!slot.acceleration || !slot.scratch || !slot.palette || !slot.texels
                || !materials)
                throw std::runtime_error("Metal reflection resource allocation failed");
            auto builder=[native accelerationStructureCommandEncoder];
            [builder buildAccelerationStructure:slot.acceleration descriptor:descriptor
                scratchBuffer:slot.scratch scratchBufferOffset:0];
            [builder endEncoding];
            ReflectionParameters p{};
            p.width=camera.width;p.height=camera.height;p.quality=quality;
            p.metallic=metallic;p.focal_x=float(camera.focal_length);
            p.focal_y=float(camera.vertical_focal_length());
            p.center_x=float(camera.center_x);p.center_y=float(camera.center_y);
            p.roughness=roughness;p.environment=environment;
            p.texel_count=std::uint32_t(texels.size());
            p.has_ground=ground?1.0F:0.0F;
            if(ground) {
                p.ground_point=as_float4(ground->point);
                p.ground_normal=as_float4(ground->normal);
            }
            auto encoder=[native computeCommandEncoder];
            [encoder setComputePipelineState:impl_->reflection_pipeline];
            [encoder setAccelerationStructure:slot.acceleration atBufferIndex:0];
            [encoder setBuffer:target offset:0 atIndex:1];
            [encoder setBytes:&p length:sizeof(p) atIndex:2];
            [encoder setBuffer:vertices offset:0 atIndex:3];
            [encoder setBuffer:materials offset:material_offset atIndex:4];
            [encoder setBuffer:slot.palette offset:0 atIndex:5];
            [encoder setBuffer:slot.texels offset:0 atIndex:6];
            [encoder useResource:slot.acceleration usage:MTLResourceUsageRead];
            const auto grid=MTLSizeMake(NSUInteger(pixels),1,1);
            const auto group=MTLSizeMake(std::min<NSUInteger>(64,
                impl_->reflection_pipeline.maxTotalThreadsPerThreadgroup),1,1);
            [encoder dispatchThreads:grid threadsPerThreadgroup:group];
            [encoder endEncoding];
            slot.fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
            submitted=true;
            if(!slot.fence) throw std::runtime_error(SDL_GetError());
        } catch(...) {
            if(!submitted) SDL_CancelGPUCommandBuffer(command);
            throw;
        }
        impl_->reflection={device,impl_->reflection_buffer,camera.width,camera.height,
            camera.width*4U};
        impl_->status=quality==1?"Metal hardware reflections LOW (1 ray)"
            :quality==2?"Metal hardware reflections MEDIUM (2 rays)"
            :"Metal hardware reflections HIGH (4 rays)";
        return true;
    } catch(const std::exception& error) {
        impl_->status=error.what();
        return false;
    }
}
GpuReflectionOutput MetalHardwareRt::reflection_output() const noexcept {return impl_->reflection;}
const std::string& MetalHardwareRt::status() const noexcept {return impl_->status;}
void MetalHardwareRt::release_device() noexcept {impl_.reset(new Impl);}
} // namespace starfox::render::shadows
