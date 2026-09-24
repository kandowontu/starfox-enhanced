#include "starfox/vr/vulkan_scene_pipeline.hpp"
#include "starfox/vr/vulkan_pipeline_cache.hpp"
#include "shaders/scene_spirv.hpp"
#include <array>
#include <cstddef>
#include <stdexcept>
#include <chrono>
#include <iostream>
namespace starfox::vr {
namespace {
template<class T> T entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    auto fn=reinterpret_cast<T>(get(device,name));
    if(!fn) throw std::runtime_error(std::string("Missing Vulkan entry point: ")+name);
    return fn;
}
void check(VkResult value,const char* operation) {
    if(value!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+": "+std::to_string(value));
}
}
VulkanScenePipeline::~VulkanScenePipeline() {close();}
void VulkanScenePipeline::close() noexcept {
    if(pipeline_) destroy_pipeline_(device_,pipeline_,nullptr);
    if(layout_) destroy_layout_(device_,layout_,nullptr);
    pipeline_={};layout_={};device_={};
}
bool VulkanScenePipeline::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,VkRenderPass pass,bool depth_test,SceneTopology topology,VkDescriptorSetLayout textures,SceneBlend mode,VulkanPipelineCache* cache) {
    close();
    std::array<VkShaderModule,2> modules{};
    PFN_vkDestroyShaderModule destroy_shader{};
    const auto clean_modules=[&] {for(auto module:modules) if(module) destroy_shader(device,module,nullptr);};
    try {
        if(!device || !get || !pass) throw std::runtime_error("Missing scene device/render pass");
        if(topology!=SceneTopology::triangles && topology!=SceneTopology::lines)
            throw std::runtime_error("Invalid scene topology");
        topology_=topology;
        textured_=textures!=VK_NULL_HANDLE;
        if(textured_) bind_descriptors_=entry<PFN_vkCmdBindDescriptorSets>(get,device,"vkCmdBindDescriptorSets");
        device_=device;
#define LOAD(member,type,name) member=entry<type>(get,device,name)
        LOAD(destroy_pipeline_,PFN_vkDestroyPipeline,"vkDestroyPipeline");
        LOAD(destroy_layout_,PFN_vkDestroyPipelineLayout,"vkDestroyPipelineLayout");
        LOAD(bind_pipeline_,PFN_vkCmdBindPipeline,"vkCmdBindPipeline");
        LOAD(bind_vertices_,PFN_vkCmdBindVertexBuffers,"vkCmdBindVertexBuffers");
        LOAD(push_,PFN_vkCmdPushConstants,"vkCmdPushConstants");
        LOAD(viewport_,PFN_vkCmdSetViewport,"vkCmdSetViewport");
        LOAD(scissor_,PFN_vkCmdSetScissor,"vkCmdSetScissor");
        LOAD(draw_,PFN_vkCmdDraw,"vkCmdDraw");
#undef LOAD
        destroy_shader=entry<PFN_vkDestroyShaderModule>(get,device,"vkDestroyShaderModule");
        auto create_shader=entry<PFN_vkCreateShaderModule>(get,device,"vkCreateShaderModule");
        auto create_layout=entry<PFN_vkCreatePipelineLayout>(get,device,"vkCreatePipelineLayout");
        auto create_pipeline=entry<PFN_vkCreateGraphicsPipelines>(get,device,"vkCreateGraphicsPipelines");
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize=textured_?sizeof(shader::vertex_textured):sizeof(shader::vertex);
        module.pCode=textured_?shader::vertex_textured:shader::vertex;
        check(create_shader(device,&module,nullptr,&modules[0]),"Create vertex shader");
        module.codeSize=textured_?sizeof(shader::fragment_textured):sizeof(shader::fragment);
        module.pCode=textured_?shader::fragment_textured:shader::fragment;
        check(create_shader(device,&module,nullptr,&modules[1]),"Create fragment shader");
        VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(SceneConstants)};
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&range;
        if(textured_) {layout.setLayoutCount=1;layout.pSetLayouts=&textures;}
        check(create_layout(device,&layout,nullptr,&layout_),"Create scene layout");
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        for(auto& stage:stages) stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=modules[0];stages[0].pName=textured_?"vertex_textured_main":"vertex_main";
        stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=modules[1];stages[1].pName=textured_?"fragment_textured_main":"fragment_main";
        VkVertexInputBindingDescription binding{0,sizeof(SceneVertex),VK_VERTEX_INPUT_RATE_VERTEX};
        std::array<VkVertexInputAttributeDescription,15> attributes{{
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,position)},
            {1,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(SceneVertex,color)},
            {2,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(SceneVertex,odd_color)},
            {3,0,VK_FORMAT_R32_UINT,offsetof(SceneVertex,dither_scale)},
            {4,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,visibility_a)},
            {5,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,visibility_b)},
            {6,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,visibility_c)},
            {7,0,VK_FORMAT_R32_UINT,offsetof(SceneVertex,visibility_enabled)},
            {8,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,group_a)},
            {9,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,group_b)},
            {10,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(SceneVertex,group_c)},
            {11,0,VK_FORMAT_R32_UINT,offsetof(SceneVertex,group_enabled)},
            {12,0,VK_FORMAT_R32G32_SFLOAT,offsetof(SceneVertex,uv)},
            {13,0,VK_FORMAT_R32G32B32A32_UINT,offsetof(SceneVertex,texture)},
            {14,0,VK_FORMAT_R32G32_SFLOAT,offsetof(SceneVertex,billboard)}}};
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount=1;input.pVertexBindingDescriptions=&binding;
        input.vertexAttributeDescriptionCount=static_cast<uint32_t>(attributes.size());input.pVertexAttributeDescriptions=attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology=topology==SceneTopology::lines?VK_PRIMITIVE_TOPOLOGY_LINE_LIST:VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo view{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        view.viewportCount=view.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable=depth.depthWriteEnable=depth_test;
        depth.depthCompareOp=VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState attachment{};attachment.colorWriteMask=0xf;
        if(mode!=SceneBlend::opaque) {
            attachment.blendEnable=VK_TRUE;
            attachment.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
            attachment.dstColorBlendFactor=(mode==SceneBlend::half_add || mode==SceneBlend::half_subtract)
                ?VK_BLEND_FACTOR_SRC_ALPHA:VK_BLEND_FACTOR_ONE;
            attachment.colorBlendOp=(mode==SceneBlend::subtract || mode==SceneBlend::half_subtract)
                ?VK_BLEND_OP_REVERSE_SUBTRACT:VK_BLEND_OP_ADD;
            attachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;
            attachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;attachment.alphaBlendOp=VK_BLEND_OP_ADD;
            if(mode==SceneBlend::alpha) {
                attachment.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                attachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
                attachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            }
            if(mode==SceneBlend::shadow) {
                // Coverage darkens the existing scene proportionally, retaining
                // destination alpha. Source RGB is deliberately irrelevant.
                attachment.srcColorBlendFactor=VK_BLEND_FACTOR_ZERO;
                attachment.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            }
        }
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount=1;blend.pAttachments=&attachment;
        const VkDynamicState dynamic_states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamic_states;
        VkGraphicsPipelineCreateInfo create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        create.stageCount=2;create.pStages=stages.data();create.pVertexInputState=&input;
        create.pInputAssemblyState=&assembly;create.pViewportState=&view;create.pRasterizationState=&raster;
        create.pMultisampleState=&samples;create.pColorBlendState=&blend;create.pDynamicState=&dynamic;
        create.pDepthStencilState=&depth;
        create.layout=layout_;create.renderPass=pass;
        const auto compile_start=std::chrono::steady_clock::now();
        check(create_pipeline(device,cache?cache->get():VK_NULL_HANDLE,1,&create,nullptr,&pipeline_),"Create scene pipeline");
        const auto compile_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-compile_start).count();
        if(compile_ms>50) std::cout<<"VR slow graphics pipeline: topology="<<int(topology)
            <<" depth="<<depth_test<<" blend="<<int(mode)<<" ms="<<compile_ms<<'\n';
        if(cache) cache->checkpoint();
        clean_modules();status_="Native Vulkan scene pipeline ready";return true;
    } catch(const std::exception& e) {clean_modules();status_=e.what();close();return false;}
}
bool VulkanScenePipeline::record(VkCommandBuffer command,VkExtent2D extent,VkBuffer vertices,
    uint32_t count,const EyeCamera& camera,VkDescriptorSet textures) const {
    return record_range(command,extent,vertices,count,0,count,camera,textures);
}
bool VulkanScenePipeline::record_range(VkCommandBuffer command,VkExtent2D extent,VkBuffer vertices,
    uint32_t total,uint32_t first,uint32_t count,const EyeCamera& camera,VkDescriptorSet textures) const {
    const uint32_t primitive=topology_==SceneTopology::lines?2:3;
    if(!pipeline_ || !command || !vertices || !count || first>total || count>total-first
        || first%primitive || count%primitive || !extent.width || !extent.height) return false;
    if(textured_ && !textures) return false;
    const VkViewport viewport{0,0,float(extent.width),float(extent.height),0,1};
    const VkRect2D scissor{{0,0},extent};const VkDeviceSize offset=0;
    bind_pipeline_(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_);
    if(textured_) bind_descriptors_(command,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,0,1,&textures,0,nullptr);
    viewport_(command,0,1,&viewport);scissor_(command,0,1,&scissor);
    bind_vertices_(command,0,1,&vertices,&offset);
    const auto constants=scene_constants(camera);
    push_(command,layout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(constants),&constants);
    draw_(command,count,1,first,0);return true;
}
bool VulkanScenePipeline::record_model(VkCommandBuffer command,VkExtent2D extent,VkBuffer vertices,
    uint32_t count,const EyeCamera& camera,const Matrix4& model,VkDescriptorSet textures) const {
    const auto combined=model_eye_camera(camera,model);
    return combined && record(command,extent,vertices,count,*combined,textures);
}
}
