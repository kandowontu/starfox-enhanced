#include "starfox/render/vulkan_hardware_rt.hpp"
#include "starfox/render/environment_effects.hpp"

#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(__linux__)
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include "starfox/render/sdl_vulkan_bridge.h"
#include "shaders/generated/vulkan_shadow_rayquery.hpp"
#include "shaders/generated/vulkan_reflection_rayquery.hpp"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace starfox::render::shadows {
#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(__linux__)
namespace {
struct alignas(16) Float4 {float x{},y{},z{},w{};};
struct Parameters {
    Float4 extent_focal{},center_ground{},ground_point{},ground_normal{};
    std::array<Float4,8> lights{};
};
static_assert(sizeof(Parameters)==192);
struct alignas(16) ReflectionParameters {
    std::array<std::uint32_t,4> dimensions{};
    Float4 camera{},settings{},ground_point{},ground_normal{};
    std::array<std::uint32_t,4> environment{};
    Float4 water_settings{},water_row0{},water_row1{},water_row2{};
    std::array<std::uint32_t,4> enhanced_size{},enhanced_modes{};
    Float4 enhanced_motion{},enhanced_plane{},enhanced_projection{},enhanced_palette{};
    Float4 enhanced_keep0{},enhanced_keep1{};
};
static_assert(sizeof(ReflectionParameters)==288);
Float4 vector4(Vec3 v) {return {float(v.x),float(v.y),float(v.z),0};}
Float4 vector4(const std::array<float,4>& v) {return {v[0],v[1],v[2],v[3]};}
std::array<Float4,8> light_samples(Vec3 light) {
    light=light*(1.0/std::sqrt(dot(light,light)));
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
        result[i]=vector4(direction);
    }
    return result;
}
struct Buffer {VkBuffer handle{};VkDeviceMemory memory{};VkDeviceSize size{};};
void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": Vulkan error "+std::to_string(result));
}
}
#endif

struct VulkanHardwareRt::Impl {
    std::string status{"Vulkan hardware rays unavailable"};
    GpuShadowOutput output{};
    GpuReflectionOutput reflection{};
#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(__linux__)
    SDL_GPUDevice* sdl{};
    const StarfoxSdlVulkanBridgeV2* bridge{};
    const StarfoxSdlVulkanRayBridgeV3* ray_bridge{};
    VkDevice device{};
    VkPhysicalDeviceMemoryProperties memory{};
    SDL_GPUBuffer* sdl_output{};
    std::uint32_t output_capacity{};
    SDL_GPUBuffer* sdl_reflection{};
    std::uint32_t reflection_capacity{};
    GpuBackground reflection_backdrop{};
    VkDescriptorSetLayout set_layout{};
    VkPipelineLayout pipeline_layout{};
    VkPipeline pipeline{};
    VkDescriptorPool descriptor_pool{};
    VkShaderModule shader{};
    VkDescriptorSetLayout reflection_set_layout{};
    VkPipelineLayout reflection_pipeline_layout{};
    VkPipeline reflection_pipeline{};
    VkDescriptorPool reflection_descriptor_pool{};
    VkShaderModule reflection_shader{};
    struct Slot {
        SDL_GPUFence* fence{};
        Buffer vertices{},instances{},scratch{},blas_buffer{},tlas_buffer{},parameters{};
        Buffer reflection_parameters{},materials{},palette{},texels{},backdrop{},enhanced_backdrop{};
        BackdropUploadCache enhanced_upload{};
        VkAccelerationStructureKHR blas{},tlas{};
        VkDescriptorSet descriptors{};
        VkDescriptorSet reflection_descriptors{};
    };
    std::array<Slot,3> slots{};
    unsigned serial{};

#define VK_FN(name) PFN_##name name{}
    VK_FN(vkCreateBuffer);VK_FN(vkDestroyBuffer);VK_FN(vkGetBufferMemoryRequirements);
    VK_FN(vkAllocateMemory);VK_FN(vkFreeMemory);VK_FN(vkBindBufferMemory);
    VK_FN(vkMapMemory);VK_FN(vkUnmapMemory);VK_FN(vkGetBufferDeviceAddress);
    VK_FN(vkCreateAccelerationStructureKHR);VK_FN(vkDestroyAccelerationStructureKHR);
    VK_FN(vkGetAccelerationStructureBuildSizesKHR);VK_FN(vkGetAccelerationStructureDeviceAddressKHR);
    VK_FN(vkCmdBuildAccelerationStructuresKHR);VK_FN(vkCmdPipelineBarrier);
    VK_FN(vkCreateDescriptorSetLayout);VK_FN(vkDestroyDescriptorSetLayout);
    VK_FN(vkCreatePipelineLayout);VK_FN(vkDestroyPipelineLayout);
    VK_FN(vkCreateComputePipelines);VK_FN(vkDestroyPipeline);
    VK_FN(vkCreateDescriptorPool);VK_FN(vkDestroyDescriptorPool);
    VK_FN(vkAllocateDescriptorSets);VK_FN(vkUpdateDescriptorSets);
    VK_FN(vkCreateShaderModule);VK_FN(vkDestroyShaderModule);
    VK_FN(vkCmdBindPipeline);VK_FN(vkCmdBindDescriptorSets);VK_FN(vkCmdDispatch);
#undef VK_FN

    void load_functions() {
#define LOAD(name) name=reinterpret_cast<PFN_##name>(bridge->get_device_proc(device,#name)); \
        if(!name) throw std::runtime_error("Missing Vulkan entry " #name)
        LOAD(vkCreateBuffer);LOAD(vkDestroyBuffer);LOAD(vkGetBufferMemoryRequirements);
        LOAD(vkAllocateMemory);LOAD(vkFreeMemory);LOAD(vkBindBufferMemory);
        LOAD(vkMapMemory);LOAD(vkUnmapMemory);LOAD(vkGetBufferDeviceAddress);
        LOAD(vkCreateAccelerationStructureKHR);LOAD(vkDestroyAccelerationStructureKHR);
        LOAD(vkGetAccelerationStructureBuildSizesKHR);LOAD(vkGetAccelerationStructureDeviceAddressKHR);
        LOAD(vkCmdBuildAccelerationStructuresKHR);LOAD(vkCmdPipelineBarrier);
        LOAD(vkCreateDescriptorSetLayout);LOAD(vkDestroyDescriptorSetLayout);
        LOAD(vkCreatePipelineLayout);LOAD(vkDestroyPipelineLayout);
        LOAD(vkCreateComputePipelines);LOAD(vkDestroyPipeline);
        LOAD(vkCreateDescriptorPool);LOAD(vkDestroyDescriptorPool);
        LOAD(vkAllocateDescriptorSets);LOAD(vkUpdateDescriptorSets);
        LOAD(vkCreateShaderModule);LOAD(vkDestroyShaderModule);
        LOAD(vkCmdBindPipeline);LOAD(vkCmdBindDescriptorSets);LOAD(vkCmdDispatch);
#undef LOAD
        auto get_memory=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
            bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceMemoryProperties"));
        if(!get_memory) throw std::runtime_error("Missing Vulkan memory properties");
        get_memory(bridge->physical_device,&memory);
    }
    Buffer create_buffer(VkDeviceSize size,VkBufferUsageFlags usage,bool host,bool address) {
        Buffer out{};out.size=size;
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=size;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device,&info,nullptr,&out.handle),"create ray buffer");
        VkMemoryRequirements requirement{};
        vkGetBufferMemoryRequirements(device,out.handle,&requirement);
        const auto wanted=host
            ?VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            :VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        unsigned index=memory.memoryTypeCount;
        for(unsigned i=0;i<memory.memoryTypeCount;++i)
            if((requirement.memoryTypeBits&(1u<<i))
                && (memory.memoryTypes[i].propertyFlags&wanted)==wanted) {index=i;break;}
        if(index==memory.memoryTypeCount)
            throw std::runtime_error("No suitable Vulkan ray buffer memory");
        VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
        flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocate.allocationSize=requirement.size;allocate.memoryTypeIndex=index;
        if(address) allocate.pNext=&flags;
        check(vkAllocateMemory(device,&allocate,nullptr,&out.memory),"allocate ray buffer");
        check(vkBindBufferMemory(device,out.handle,out.memory,0),"bind ray buffer");
        return out;
    }
    void upload(Buffer& buffer,const void* data,std::size_t bytes) {
        void* destination{};
        check(vkMapMemory(device,buffer.memory,0,bytes,0,&destination),"map ray input");
        std::memcpy(destination,data,bytes);
        vkUnmapMemory(device,buffer.memory);
    }
    VkDeviceAddress address(Buffer buffer) const {
        VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        info.buffer=buffer.handle;return vkGetBufferDeviceAddress(device,&info);
    }
    void destroy(Buffer& buffer) {
        if(buffer.handle) vkDestroyBuffer(device,buffer.handle,nullptr);
        if(buffer.memory) vkFreeMemory(device,buffer.memory,nullptr);
        buffer={};
    }
    void clear(Slot& slot,bool release_cached=false) {
        if(slot.fence) {SDL_WaitForGPUFences(sdl,true,&slot.fence,1);SDL_ReleaseGPUFence(sdl,slot.fence);slot.fence=nullptr;}
        if(slot.blas) vkDestroyAccelerationStructureKHR(device,slot.blas,nullptr);
        if(slot.tlas) vkDestroyAccelerationStructureKHR(device,slot.tlas,nullptr);
        slot.blas=slot.tlas=VK_NULL_HANDLE;
        for(auto* buffer:{&slot.vertices,&slot.instances,&slot.scratch,&slot.blas_buffer,
                          &slot.tlas_buffer,&slot.parameters,&slot.reflection_parameters,
                          &slot.materials,&slot.palette,&slot.texels,&slot.backdrop}) destroy(*buffer);
        if(release_cached) {destroy(slot.enhanced_backdrop);slot.enhanced_upload={};}
    }
    void release() {
        if(!sdl) return;
        for(auto& slot:slots) clear(slot,true);
        if(sdl_output) SDL_ReleaseGPUBuffer(sdl,sdl_output);
        if(sdl_reflection) SDL_ReleaseGPUBuffer(sdl,sdl_reflection);
        reflection_backdrop.release_device();
        if(reflection_descriptor_pool) vkDestroyDescriptorPool(device,reflection_descriptor_pool,nullptr);
        if(reflection_pipeline) vkDestroyPipeline(device,reflection_pipeline,nullptr);
        if(reflection_pipeline_layout) vkDestroyPipelineLayout(device,reflection_pipeline_layout,nullptr);
        if(reflection_set_layout) vkDestroyDescriptorSetLayout(device,reflection_set_layout,nullptr);
        if(reflection_shader) vkDestroyShaderModule(device,reflection_shader,nullptr);
        if(descriptor_pool) vkDestroyDescriptorPool(device,descriptor_pool,nullptr);
        if(pipeline) vkDestroyPipeline(device,pipeline,nullptr);
        if(pipeline_layout) vkDestroyPipelineLayout(device,pipeline_layout,nullptr);
        if(set_layout) vkDestroyDescriptorSetLayout(device,set_layout,nullptr);
        if(shader) vkDestroyShaderModule(device,shader,nullptr);
        sdl_output=sdl_reflection=nullptr;output_capacity=reflection_capacity=0;
        output={};reflection={};sdl=nullptr;
    }
    void initialize(SDL_GPUDevice* source) {
        if(sdl==source) return;
        release();
        const auto properties=SDL_GetGPUDeviceProperties(source);
        bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(
            SDL_GetPointerProperty(properties,STARFOX_SDL_VULKAN_BRIDGE,nullptr));
        ray_bridge=static_cast<const StarfoxSdlVulkanRayBridgeV3*>(
            SDL_GetPointerProperty(properties,STARFOX_SDL_VULKAN_RAY_BRIDGE,nullptr));
        if(!bridge || bridge->version!=2 || !ray_bridge || ray_bridge->version!=3
            || !ray_bridge->command || !ray_bridge->prepare_write || !ray_bridge->finish_write
            || !ray_bridge->copy_ray_range)
            throw std::runtime_error("Vulkan ray command bridge unavailable");
        sdl=source;device=bridge->device;load_functions();
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize=vulkan_shadow_rayquery_spirv.size()*4;
        module.pCode=vulkan_shadow_rayquery_spirv.data();
        check(vkCreateShaderModule(device,&module,nullptr,&shader),"create ray shader");
        const std::array<VkDescriptorSetLayoutBinding,3> bindings{{
            {0,VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {2,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}}};
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout.bindingCount=unsigned(bindings.size());layout.pBindings=bindings.data();
        check(vkCreateDescriptorSetLayout(device,&layout,nullptr,&set_layout),"create ray descriptors");
        VkPipelineLayoutCreateInfo pipeline_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipeline_info.setLayoutCount=1;pipeline_info.pSetLayouts=&set_layout;
        check(vkCreatePipelineLayout(device,&pipeline_info,nullptr,&pipeline_layout),"create ray layout");
        VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        compute.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        compute.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;compute.stage.module=shader;
        compute.stage.pName="main";compute.layout=pipeline_layout;
        check(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&compute,nullptr,&pipeline),"create ray pipeline");
        const std::array<VkDescriptorPoolSize,3> pools{{
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3}}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.maxSets=3;pool.poolSizeCount=unsigned(pools.size());pool.pPoolSizes=pools.data();
        check(vkCreateDescriptorPool(device,&pool,nullptr,&descriptor_pool),"create ray descriptor pool");
        const std::array<VkDescriptorSetLayout,3> layouts{set_layout,set_layout,set_layout};
        std::array<VkDescriptorSet,3> sets{};
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocate.descriptorPool=descriptor_pool;allocate.descriptorSetCount=3;
        allocate.pSetLayouts=layouts.data();
        check(vkAllocateDescriptorSets(device,&allocate,sets.data()),"allocate ray descriptors");
        for(unsigned i=0;i<3;++i) slots[i].descriptors=sets[i];
    }
    void ensure_output(std::uint32_t pixels) {
        if(sdl_output && output_capacity>=pixels) return;
        SDL_GPUBufferCreateInfo info{};
        info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        info.size=pixels*4U;
        auto* next=SDL_CreateGPUBuffer(sdl,&info);
        if(!next) throw std::runtime_error(SDL_GetError());
        if(sdl_output) SDL_ReleaseGPUBuffer(sdl,sdl_output);
        sdl_output=next;output_capacity=pixels;
    }
    void ensure_reflection_output(std::uint32_t pixels) {
        if(sdl_reflection && reflection_capacity>=pixels) return;
        SDL_GPUBufferCreateInfo info{};
        info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        info.size=pixels*4U;
        auto* next=SDL_CreateGPUBuffer(sdl,&info);
        if(!next) throw std::runtime_error(SDL_GetError());
        if(sdl_reflection) SDL_ReleaseGPUBuffer(sdl,sdl_reflection);
        sdl_reflection=next;reflection_capacity=pixels;
    }
    void ensure_reflection_pipeline() {
        if(reflection_pipeline) return;
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize=vulkan_reflection_rayquery_spirv.size()*4;
        module.pCode=vulkan_reflection_rayquery_spirv.data();
        check(vkCreateShaderModule(device,&module,nullptr,&reflection_shader),"create reflection shader");
        const std::array<VkDescriptorSetLayoutBinding,9> bindings{{
            {0,VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {2,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {4,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {5,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {6,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {7,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {8,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}}};
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout.bindingCount=unsigned(bindings.size());layout.pBindings=bindings.data();
        check(vkCreateDescriptorSetLayout(device,&layout,nullptr,&reflection_set_layout),
            "create reflection descriptors");
        VkPipelineLayoutCreateInfo pipeline_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipeline_info.setLayoutCount=1;pipeline_info.pSetLayouts=&reflection_set_layout;
        check(vkCreatePipelineLayout(device,&pipeline_info,nullptr,&reflection_pipeline_layout),
            "create reflection layout");
        VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        compute.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        compute.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;compute.stage.module=reflection_shader;
        compute.stage.pName="main";compute.layout=reflection_pipeline_layout;
        check(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&compute,nullptr,&reflection_pipeline),
            "create reflection pipeline");
        const std::array<VkDescriptorPoolSize,3> pools{{
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,21},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3}}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.maxSets=3;pool.poolSizeCount=unsigned(pools.size());pool.pPoolSizes=pools.data();
        check(vkCreateDescriptorPool(device,&pool,nullptr,&reflection_descriptor_pool),
            "create reflection descriptor pool");
        const std::array<VkDescriptorSetLayout,3> layouts{
            reflection_set_layout,reflection_set_layout,reflection_set_layout};
        std::array<VkDescriptorSet,3> sets{};
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocate.descriptorPool=reflection_descriptor_pool;allocate.descriptorSetCount=3;
        allocate.pSetLayouts=layouts.data();
        check(vkAllocateDescriptorSets(device,&allocate,sets.data()),
            "allocate reflection descriptors");
        for(unsigned i=0;i<3;++i) slots[i].reflection_descriptors=sets[i];
    }
    VkAccelerationStructureKHR acceleration(Buffer& storage,VkAccelerationStructureTypeKHR type,
        VkDeviceSize bytes) {
        storage=create_buffer(bytes,VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
            |VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,false,true);
        VkAccelerationStructureCreateInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        info.buffer=storage.handle;info.size=bytes;info.type=type;
        VkAccelerationStructureKHR result{};
        check(vkCreateAccelerationStructureKHR(device,&info,nullptr,&result),"create acceleration structure");
        return result;
    }
    VkDeviceAddress acceleration_address(VkAccelerationStructureKHR structure) const {
        VkAccelerationStructureDeviceAddressInfoKHR info{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
        info.accelerationStructure=structure;
        return vkGetAccelerationStructureDeviceAddressKHR(device,&info);
    }
    void record_build(VkCommandBuffer command,Slot& slot,std::uint32_t vertices) {
        VkAccelerationStructureGeometryTrianglesDataKHR triangles{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
        triangles.vertexFormat=VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData.deviceAddress=address(slot.vertices);
        triangles.vertexStride=sizeof(Float4);triangles.maxVertex=vertices-1;
        triangles.indexType=VK_INDEX_TYPE_NONE_KHR;
        VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geometry.geometryType=VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags=VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles=triangles;
        VkAccelerationStructureBuildGeometryInfoKHR build{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        build.type=VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        build.flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build.mode=VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build.geometryCount=1;build.pGeometries=&geometry;
        const std::uint32_t count=vertices/3;
        VkAccelerationStructureBuildSizesInfoKHR sizes{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(device,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &build,&count,&sizes);
        slot.blas=acceleration(slot.blas_buffer,VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            sizes.accelerationStructureSize);
        VkAccelerationStructureInstanceKHR instance{};
        instance.transform.matrix[0][0]=instance.transform.matrix[1][1]=
            instance.transform.matrix[2][2]=1;
        instance.mask=0xff;
        instance.flags=VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference=acceleration_address(slot.blas);
        slot.instances=create_buffer(sizeof(instance),VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
            |VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,true,true);
        upload(slot.instances,&instance,sizeof(instance));
        VkAccelerationStructureGeometryInstancesDataKHR instances{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
        instances.data.deviceAddress=address(slot.instances);
        VkAccelerationStructureGeometryKHR top_geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        top_geometry.geometryType=VK_GEOMETRY_TYPE_INSTANCES_KHR;
        top_geometry.geometry.instances=instances;
        VkAccelerationStructureBuildGeometryInfoKHR top{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        top.type=VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        top.flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        top.mode=VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        top.geometryCount=1;top.pGeometries=&top_geometry;
        const std::uint32_t one=1;
        VkAccelerationStructureBuildSizesInfoKHR top_sizes{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(device,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &top,&one,&top_sizes);
        slot.tlas=acceleration(slot.tlas_buffer,VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            top_sizes.accelerationStructureSize);
        slot.scratch=create_buffer(std::max(sizes.buildScratchSize,top_sizes.buildScratchSize),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,false,true);
        build.dstAccelerationStructure=slot.blas;build.scratchData.deviceAddress=address(slot.scratch);
        VkAccelerationStructureBuildRangeInfoKHR range{};range.primitiveCount=count;
        const VkAccelerationStructureBuildRangeInfoKHR* ranges=&range;
        vkCmdBuildAccelerationStructuresKHR(command,1,&build,&ranges);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,0,1,&barrier,0,nullptr,0,nullptr);
        top.dstAccelerationStructure=slot.tlas;top.scratchData.deviceAddress=address(slot.scratch);
        VkAccelerationStructureBuildRangeInfoKHR top_range{};top_range.primitiveCount=1;
        const VkAccelerationStructureBuildRangeInfoKHR* top_ranges=&top_range;
        vkCmdBuildAccelerationStructuresKHR(command,1,&top,&top_ranges);
        barrier.dstAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
    }
    void record_dispatch(VkCommandBuffer command,Slot& slot,VkBuffer target,
        std::uint32_t pixels) {
        VkWriteDescriptorSetAccelerationStructureKHR structure{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        structure.accelerationStructureCount=1;structure.pAccelerationStructures=&slot.tlas;
        VkDescriptorBufferInfo output_info{target,0,VkDeviceSize(pixels)*4};
        VkDescriptorBufferInfo params_info{slot.parameters.handle,0,sizeof(Parameters)};
        std::array<VkWriteDescriptorSet,3> writes{};
        for(unsigned i=0;i<3;++i) {
            writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet=slot.descriptors;writes[i].dstBinding=i;
            writes[i].descriptorCount=1;
        }
        writes[0].pNext=&structure;writes[0].descriptorType=VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        writes[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[1].pBufferInfo=&output_info;
        writes[2].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;writes[2].pBufferInfo=&params_info;
        vkUpdateDescriptorSets(device,unsigned(writes.size()),writes.data(),0,nullptr);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,
            0,1,&slot.descriptors,0,nullptr);
        vkCmdDispatch(command,(pixels+63)/64,1,1);
    }
    void record_reflection_dispatch(VkCommandBuffer command,Slot& slot,VkBuffer target,
        std::uint32_t pixels) {
        VkWriteDescriptorSetAccelerationStructureKHR structure{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        structure.accelerationStructureCount=1;structure.pAccelerationStructures=&slot.tlas;
        const std::array<VkDescriptorBufferInfo,8> info{{
            {target,0,VkDeviceSize(pixels)*4},
            {slot.reflection_parameters.handle,0,sizeof(ReflectionParameters)},
            {slot.vertices.handle,0,slot.vertices.size},
            {slot.materials.handle,0,slot.materials.size},
            {slot.palette.handle,0,slot.palette.size},
            {slot.texels.handle,0,slot.texels.size},
            {slot.backdrop.handle,0,slot.backdrop.size},
            {slot.enhanced_backdrop.handle,0,slot.enhanced_backdrop.size}}};
        std::array<VkWriteDescriptorSet,9> writes{};
        for(unsigned i=0;i<writes.size();++i) {
            writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet=slot.reflection_descriptors;
            writes[i].dstBinding=i;
            writes[i].descriptorCount=1;
            writes[i].descriptorType=i==0?VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
                :i==2?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            if(i) writes[i].pBufferInfo=&info[i-1];
        }
        writes[0].pNext=&structure;
        vkUpdateDescriptorSets(device,unsigned(writes.size()),writes.data(),0,nullptr);
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,reflection_pipeline);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,reflection_pipeline_layout,
            0,1,&slot.reflection_descriptors,0,nullptr);
        vkCmdDispatch(command,(pixels+63)/64,1,1);
    }
#endif
};

VulkanHardwareRt::VulkanHardwareRt():impl_(std::make_unique<Impl>()) {}
VulkanHardwareRt::~VulkanHardwareRt(){release_device();}
bool VulkanHardwareRt::available(void* raw) const noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(__linux__)
    auto* device=static_cast<SDL_GPUDevice*>(raw);
    return device && SDL_GetBooleanProperty(SDL_GetGPUDeviceProperties(device),
        "starfox.vulkan.ray_query.enabled",false)
        && SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(device),
            STARFOX_SDL_VULKAN_RAY_BRIDGE,nullptr);
#else
    (void)raw;return false;
#endif
}
bool VulkanHardwareRt::render_shadows(void* raw,const Scene& scene,Camera camera,
    Vec3 light,std::optional<ReceiverPlane> ground,const GpuScene::RayGeometryOutput* geometry) {
    impl_->output={};
#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(__linux__)
    try {
        const std::uint64_t pixels=std::uint64_t(camera.width)*camera.height;
        const auto length=std::sqrt(dot(light,light));
        const bool resident=geometry && geometry->complete && geometry->device==raw
            && geometry->buffer && geometry->vertex_count && geometry->vertex_count%3==0;
        const auto vertex_count=resident?geometry->vertex_count:scene.triangle_count()*3;
        if(!available(raw) || !pixels || pixels>std::numeric_limits<std::uint32_t>::max()/4
            || !std::isfinite(length) || length<1.e-10 || camera.focal_length<=0
            || camera.vertical_focal_length()<=0 || !vertex_count
            || vertex_count>std::numeric_limits<std::uint32_t>::max()/sizeof(Float4))
            return false;
        impl_->initialize(static_cast<SDL_GPUDevice*>(raw));
        auto& slot=impl_->slots[impl_->serial++%impl_->slots.size()];
        impl_->clear(slot);
        std::vector<Float4> vertices;
        if(!resident) {
            vertices.reserve(vertex_count);
            for(const auto& triangle:scene.triangles()) {
                vertices.push_back(vector4(triangle.a));
                vertices.push_back(vector4(triangle.b));
                vertices.push_back(vector4(triangle.c));
            }
        }
        slot.vertices=impl_->create_buffer(vertex_count*sizeof(Float4),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                |VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                |VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                |(resident?VK_BUFFER_USAGE_TRANSFER_DST_BIT:0),!resident,true);
        if(!resident) impl_->upload(slot.vertices,vertices.data(),vertices.size()*sizeof(Float4));
        Parameters params{};
        params.extent_focal={float(camera.width),float(camera.height),
            float(camera.focal_length),float(camera.vertical_focal_length())};
        params.center_ground={float(camera.center_x),float(camera.center_y),ground?1.f:0.f,0};
        if(ground) {params.ground_point=vector4(ground->point);params.ground_normal=vector4(ground->normal);}
        params.lights=light_samples(light);
        slot.parameters=impl_->create_buffer(sizeof(params),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true,false);
        impl_->upload(slot.parameters,&params,sizeof(params));
        impl_->ensure_output(std::uint32_t(pixels));
        auto* command=SDL_AcquireGPUCommandBuffer(impl_->sdl);
        if(!command) throw std::runtime_error(SDL_GetError());
        bool submitted=false;
        try {
            const auto native=impl_->ray_bridge->command(command);
            if(!native) throw std::runtime_error("Missing native Vulkan ray command");
            if(resident && !impl_->ray_bridge->copy_ray_range(command,geometry->buffer,0,
                slot.vertices.handle,slot.vertices.size,unsigned(vertex_count*sizeof(Float4))))
                throw std::runtime_error(SDL_GetError());
            impl_->record_build(native,slot,unsigned(vertex_count));
            const auto target=impl_->ray_bridge->prepare_write(command,impl_->sdl_output);
            if(!target) throw std::runtime_error(SDL_GetError());
            impl_->record_dispatch(native,slot,target,std::uint32_t(pixels));
            if(!impl_->ray_bridge->finish_write(command,impl_->sdl_output))
                throw std::runtime_error(SDL_GetError());
            slot.fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
            submitted=true;
            if(!slot.fence) throw std::runtime_error(SDL_GetError());
        } catch(...) {
            if(!submitted) SDL_CancelGPUCommandBuffer(command);
            throw;
        }
        impl_->output={raw,impl_->sdl_output,camera.width,camera.height,0};
        impl_->status=resident?"Vulkan hardware rays from resident GPU casters"
            :"Vulkan hardware ray queries from CPU casters";
        return true;
    } catch(const std::exception& error) {impl_->status=error.what();return false;}
#else
    (void)raw;(void)scene;(void)camera;(void)light;(void)ground;(void)geometry;return false;
#endif
}
bool VulkanHardwareRt::render_reflections(void* raw,
    const GpuScene::RayGeometryOutput& geometry,Camera camera,
    std::span<const std::uint32_t,256> palette,std::uint32_t environment,
    std::uint8_t quality,float roughness,std::uint32_t metallic,
    std::optional<ReceiverPlane> ground,const GpuBackgroundDraw* background,
    const RayWater* water) {
    impl_->reflection={};
#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(__linux__)
    try {
        const std::uint64_t pixels=std::uint64_t(camera.width)*camera.height;
        const auto reject=[&](const char* reason){impl_->status=reason;return false;};
        if(!available(raw)) return reject("Vulkan ray-query device unavailable");
        if(!quality || quality>3 || !pixels
            || pixels>std::numeric_limits<std::uint32_t>::max()/4U)
            return reject("Invalid reflection quality or frame dimensions");
        if(camera.focal_length<=0 || camera.vertical_focal_length()<=0)
            return reject("Invalid reflection camera focal length");
        if(!std::isfinite(roughness) || roughness<0)
            return reject("Invalid reflection roughness");
        if(water && (!ground || water->material>3
            || !std::isfinite(water->time) || !std::isfinite(water->reflection_strength)
            || water->reflection_strength<0 || water->reflection_strength>1))
            return reject("Invalid ray-water settings");
        if(!geometry.complete || geometry.device!=raw || !geometry.buffer)
            return reject("Resident reflection geometry unavailable");
        if(geometry.vertex_count<3 || geometry.vertex_count%3
            || geometry.vertex_count>std::numeric_limits<std::uint32_t>::max()/sizeof(Float4))
            return reject("Invalid reflection triangle vertex count");
        if(!geometry.materials) return reject("Reflection triangle materials missing");
        if(!geometry.material_offset
            && geometry.materials->triangles.size()!=geometry.vertex_count/3U)
            return reject("Reflection material/vertex topology mismatch");
        if(geometry.materials->texels.size()>16'000'000)
            return reject("Reflection texture atlas too large");
        impl_->initialize(static_cast<SDL_GPUDevice*>(raw));
        impl_->ensure_reflection_pipeline();
        auto& slot=impl_->slots[impl_->serial++%impl_->slots.size()];
        impl_->clear(slot);
        slot.vertices=impl_->create_buffer(std::size_t(geometry.vertex_count)*sizeof(Float4),
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                |VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                |VK_BUFFER_USAGE_TRANSFER_DST_BIT,false,true);
        const auto& materials=*geometry.materials;
        const auto material_bytes=std::size_t(geometry.vertex_count/3U)*sizeof(render::RayMaterial);
        slot.materials=impl_->create_buffer(material_bytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                |(geometry.material_offset?VK_BUFFER_USAGE_TRANSFER_DST_BIT:0),
            !geometry.material_offset,false);
        if(!geometry.material_offset)
            impl_->upload(slot.materials,materials.triangles.data(),material_bytes);
        slot.palette=impl_->create_buffer(palette.size_bytes(),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            true,false);
        impl_->upload(slot.palette,palette.data(),palette.size_bytes());
        std::vector<std::uint32_t> texels(std::max<std::size_t>(1,(materials.texels.size()+3)/4));
        if(!materials.texels.empty())
            std::memcpy(texels.data(),materials.texels.data(),materials.texels.size());
        slot.texels=impl_->create_buffer(texels.size()*sizeof(std::uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true,false);
        impl_->upload(slot.texels,texels.data(),slot.texels.size);
        // atan2 spans +/-pi, or about 804 source pixels at 256 px/radian.
        // Leave a small guard on both sides without rasterizing empty columns.
        constexpr std::uint32_t backdrop_width=1664,backdrop_height=224;
        constexpr std::uint32_t backdrop_origin=(backdrop_width-256)/2;
        const bool backdrop_requested=background && background->ppu
            && background->settings.layer==2 && (background->ppu->main_screen&2);
        slot.backdrop=impl_->create_buffer(backdrop_requested
                ?std::size_t(backdrop_width)*backdrop_height*4:4,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                |(backdrop_requested?VK_BUFFER_USAGE_TRANSFER_DST_BIT:0),
            !backdrop_requested,false);
        if(!backdrop_requested) {
            constexpr std::uint32_t empty=0;
            impl_->upload(slot.backdrop,&empty,sizeof(empty));
        }
        const auto* enhanced_environment=backdrop_requested
            ?background->settings.reflection_environment:nullptr;
        const auto* enhanced_image=enhanced_environment
            && enhanced_environment->modes[2]
            && enhanced_environment->backdrop_projection[3]==0
            ?enhanced_environment->backdrop:nullptr;
        if(enhanced_image) {
            if(!enhanced_image->width || !enhanced_image->height
                || enhanced_image->width>8192 || enhanced_image->height>8192
                || enhanced_image->pixels.size()!=std::size_t(enhanced_image->width)*enhanced_image->height)
                throw std::runtime_error("Invalid enhanced reflection sky");
            const auto bytes=enhanced_image->pixels.size()*sizeof(std::uint32_t);
            if(slot.enhanced_backdrop.size!=bytes) {
                impl_->destroy(slot.enhanced_backdrop);
                slot.enhanced_backdrop=impl_->create_buffer(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true,false);
                slot.enhanced_upload={};
            }
            if(!slot.enhanced_upload.matches(*enhanced_image)) {
                impl_->upload(slot.enhanced_backdrop,enhanced_image->pixels.data(),bytes);
                slot.enhanced_upload.remember(*enhanced_image);
            }
        } else if(!slot.enhanced_backdrop.handle) {
            slot.enhanced_backdrop=impl_->create_buffer(4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true,false);
            constexpr std::uint32_t empty=0;
            impl_->upload(slot.enhanced_backdrop,&empty,sizeof(empty));
        }
        ReflectionParameters params{};
        params.dimensions={camera.width,camera.height,quality,metallic};
        params.camera={float(camera.focal_length),float(camera.vertical_focal_length()),
            float(camera.center_x),float(camera.center_y)};
        params.settings={roughness,ground?1.f:0.f,float(materials.texels.size()),0};
        if(ground) {
            params.ground_point=vector4(ground->point);
            params.ground_normal=vector4(ground->normal);
        }
        params.environment[0]=environment;
        if(enhanced_image) {
            params.enhanced_size={enhanced_image->width,enhanced_image->height,1,0};
            params.enhanced_modes=enhanced_environment->modes;
            params.enhanced_motion=vector4(enhanced_environment->motion);
            params.enhanced_plane=vector4(enhanced_environment->plane);
            params.enhanced_projection=vector4(enhanced_environment->backdrop_projection);
            params.enhanced_palette=vector4(enhanced_environment->backdrop_palette[0]);
            params.enhanced_keep0=vector4(enhanced_environment->backdrop_keep[0]);
            params.enhanced_keep1=vector4(enhanced_environment->backdrop_keep[1]);
        }
        if(water) {
            params.water_settings={water->time,water->reflection_strength,1.f,float(water->material)};
            params.water_row0={water->world_to_view[0],water->world_to_view[1],water->world_to_view[2],water->camera_position[0]};
            params.water_row1={water->world_to_view[3],water->world_to_view[4],water->world_to_view[5],water->camera_position[1]};
            params.water_row2={water->world_to_view[6],water->world_to_view[7],water->world_to_view[8],water->camera_position[2]};
        }
        slot.reflection_parameters=impl_->create_buffer(sizeof(params),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true,false);
        impl_->ensure_reflection_output(std::uint32_t(pixels));
        auto* command=SDL_AcquireGPUCommandBuffer(impl_->sdl);
        if(!command) throw std::runtime_error(SDL_GetError());
        bool submitted=false;
        try {
            if(backdrop_requested) {
                auto settings=background->settings;
                settings.priority=TilePriorityPass::all;
                settings.horizontal_origin=int(backdrop_origin);
                settings.extend_horizontal=true;
                settings.logical_viewport={backdrop_width,backdrop_height};
                settings.raster_jitter={};
                const auto panorama=impl_->reflection_backdrop.enqueue(raw,command,
                    *background->ppu,backdrop_width,backdrop_height,1,settings);
                if(panorama.pixels) {
                    if(!impl_->ray_bridge->copy_ray_range(command,panorama.pixels,0,
                        slot.backdrop.handle,slot.backdrop.size,
                        backdrop_width*backdrop_height*4))
                        throw std::runtime_error(SDL_GetError());
                    params.environment[1]=backdrop_width;
                    params.environment[2]=backdrop_height;
                    params.environment[3]=backdrop_origin;
                }
            }
            impl_->upload(slot.reflection_parameters,&params,sizeof(params));
            const auto native=impl_->ray_bridge->command(command);
            if(!native) throw std::runtime_error("Missing native Vulkan ray command");
            if(!impl_->ray_bridge->copy_ray_range(command,geometry.buffer,0,
                slot.vertices.handle,slot.vertices.size,
                unsigned(std::size_t(geometry.vertex_count)*sizeof(Float4))))
                throw std::runtime_error(SDL_GetError());
            if(geometry.material_offset
                && !impl_->ray_bridge->copy_ray_range(command,geometry.buffer,
                    geometry.material_offset,slot.materials.handle,slot.materials.size,
                    unsigned(material_bytes)))
                throw std::runtime_error(SDL_GetError());
            impl_->record_build(native,slot,geometry.vertex_count);
            const auto target=impl_->ray_bridge->prepare_write(command,impl_->sdl_reflection);
            if(!target) throw std::runtime_error(SDL_GetError());
            impl_->record_reflection_dispatch(native,slot,target,std::uint32_t(pixels));
            if(!impl_->ray_bridge->finish_write(command,impl_->sdl_reflection))
                throw std::runtime_error(SDL_GetError());
            slot.fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
            submitted=true;
            if(!slot.fence) throw std::runtime_error(SDL_GetError());
        } catch(...) {
            if(!submitted) SDL_CancelGPUCommandBuffer(command);
            throw;
        }
        impl_->reflection={raw,impl_->sdl_reflection,camera.width,camera.height,camera.width*4U};
        impl_->status=quality==1?"Vulkan hardware reflections LOW (1 ray)"
            :quality==2?"Vulkan hardware reflections MEDIUM (2 rays)"
            :"Vulkan hardware reflections HIGH (4 rays)";
        return true;
    } catch(const std::exception& error) {impl_->status=error.what();return false;}
#else
    (void)raw;(void)geometry;(void)camera;(void)palette;(void)environment;
    (void)quality;(void)roughness;(void)metallic;(void)ground;(void)background;(void)water;return false;
#endif
}
GpuShadowOutput VulkanHardwareRt::shadow_output() const noexcept{return impl_->output;}
GpuReflectionOutput VulkanHardwareRt::reflection_output() const noexcept{return impl_->reflection;}
const std::string& VulkanHardwareRt::status() const noexcept{return impl_->status;}
void VulkanHardwareRt::release_device() noexcept {
#if defined(STARFOX_SDL_GPU_EFFECTS) && defined(__linux__)
    impl_->release();
#endif
}
}
