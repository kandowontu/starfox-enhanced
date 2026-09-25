#include "starfox/vr/vulkan_connected_grid.hpp"
#include "shaders/scene_spirv.hpp"
#include <stdexcept>
namespace starfox::vr {
namespace {
template<class T> T fn(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    auto p=reinterpret_cast<T>(get(device,name));
    if(!p) throw std::runtime_error(name);return p;
}
void check(VkResult result) {if(result!=VK_SUCCESS) throw std::runtime_error("Connected grid Vulkan error "+std::to_string(result));}
}
VulkanConnectedGridPipeline::~VulkanConnectedGridPipeline() {
    if(!device_) return;
    for(auto p:pipelines_) if(p) fn<PFN_vkDestroyPipeline>(get_,device_,"vkDestroyPipeline")(device_,p,nullptr);
    if(layout_) fn<PFN_vkDestroyPipelineLayout>(get_,device_,"vkDestroyPipelineLayout")(device_,layout_,nullptr);
    if(set_) fn<PFN_vkDestroyDescriptorSetLayout>(get_,device_,"vkDestroyDescriptorSetLayout")(device_,set_,nullptr);
}
bool VulkanConnectedGridPipeline::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,VkPipelineCache cache) {
    try {
        if(device_ || !device || !get) throw std::runtime_error("Invalid connected grid pipeline");
        fn<PFN_vkDestroyPipeline>(get,device,"vkDestroyPipeline");
        fn<PFN_vkDestroyPipelineLayout>(get,device,"vkDestroyPipelineLayout");
        fn<PFN_vkDestroyDescriptorSetLayout>(get,device,"vkDestroyDescriptorSetLayout");
        device_=device;get_=get;
        VkDescriptorSetLayoutBinding bindings[2]{{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
        VkDescriptorSetLayoutCreateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};set.bindingCount=2;set.pBindings=bindings;
        check(fn<PFN_vkCreateDescriptorSetLayout>(get,device,"vkCreateDescriptorSetLayout")(device,&set,nullptr,&set_));
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.setLayoutCount=1;layout.pSetLayouts=&set_;
        check(fn<PFN_vkCreatePipelineLayout>(get,device,"vkCreatePipelineLayout")(device,&layout,nullptr,&layout_));
        auto destroy=fn<PFN_vkDestroyShaderModule>(get,device,"vkDestroyShaderModule");
        auto create=fn<PFN_vkCreateComputePipelines>(get,device,"vkCreateComputePipelines");
        for(unsigned i=0;i<2;++i) {
            VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            info.pCode=i?shader::connected_rows:shader::connected_project;
            info.codeSize=i?sizeof(shader::connected_rows):sizeof(shader::connected_project);
            VkShaderModule module{};
            check(fn<PFN_vkCreateShaderModule>(get,device,"vkCreateShaderModule")(device,&info,nullptr,&module));
            VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipeline.layout=layout_;pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=module;
            pipeline.stage.pName=i?"connected_rows_main":"connected_project_main";
            auto result=create(device,cache,1,&pipeline,nullptr,&pipelines_[i]);destroy(device,module,nullptr);check(result);
        }
        return true;
    } catch(const std::exception& e) {status_=e.what();return false;}
}
bool VulkanConnectedGridPipeline::record(VkCommandBuffer command,VkDescriptorSet set) const {
    if(!command || !set || !pipelines_[0] || !pipelines_[1]) return false;
    auto barrier=fn<PFN_vkCmdPipelineBarrier>(get_,device_,"vkCmdPipelineBarrier");
    VkMemoryBarrier memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    // Also protects rewriting a reused output after an earlier eye read.
    memory.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
    memory.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
    barrier(command,VK_PIPELINE_STAGE_HOST_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&memory,0,nullptr,0,nullptr);
    fn<PFN_vkCmdBindDescriptorSets>(get_,device_,"vkCmdBindDescriptorSets")(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout_,0,1,&set,0,nullptr);
    for(unsigned i=0;i<2;++i) {
        fn<PFN_vkCmdBindPipeline>(get_,device_,"vkCmdBindPipeline")(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines_[i]);
        fn<PFN_vkCmdDispatch>(get_,device_,"vkCmdDispatch")(command,i?3:4,1,1);
        memory.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;memory.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|(i?VK_ACCESS_HOST_READ_BIT:0);
        barrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,i?(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_HOST_BIT):VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,1,&memory,0,nullptr,0,nullptr);
    }
    return true;
}
VulkanConnectedGrid::~VulkanConnectedGrid() {
    if(pool_) fn<PFN_vkDestroyDescriptorPool>(get_,device_,"vkDestroyDescriptorPool")(device_,pool_,nullptr);
}
bool VulkanConnectedGrid::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,const VkPhysicalDeviceMemoryProperties& memory,
    std::span<const uint32_t> source,std::shared_ptr<VulkanConnectedGridPipeline> pipeline,const VulkanConnectedGrid* prior) {
    try {
        if(device_ || !device || !get || !pipeline || !pipeline->descriptor_layout() || source.size()!=14)
            throw std::runtime_error("Invalid connected grid source");
        for(auto word:source) if(int32_t(word)<-32768 || int32_t(word)>32767)
            throw std::runtime_error("Connected grid source exceeds signed word");
        fn<PFN_vkDestroyDescriptorPool>(get,device,"vkDestroyDescriptorPool");
        device_=device;get_=get;pipeline_=std::move(pipeline);
        if(!input_.initialize(device,get,memory,source.size_bytes()) || !input_.upload(0,std::as_bytes(source)))
            throw std::runtime_error(input_.status());
        if(prior && prior->device_==device && prior->get_==get && prior->output_
            && prior->size()==uint64_t(connected_grid_output_words)*4) {
            output_=prior->output_;reused_output_=true;
        } else {
            output_=std::make_shared<VulkanSourceStorage>();
            if(!output_->initialize(device,get,memory,uint64_t(connected_grid_output_words)*4)) throw std::runtime_error(output_->status());
        }
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
        check(fn<PFN_vkCreateDescriptorPool>(get,device,"vkCreateDescriptorPool")(device,&pool,nullptr,&pool_));
        const auto layout=pipeline_->descriptor_layout();
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocate.descriptorPool=pool_;allocate.descriptorSetCount=1;allocate.pSetLayouts=&layout;
        check(fn<PFN_vkAllocateDescriptorSets>(get,device,"vkAllocateDescriptorSets")(device,&allocate,&set_));
        VkDescriptorBufferInfo buffers[2]{{input_.buffer(),0,input_.size()},{buffer(),0,this->size()}};
        VkWriteDescriptorSet writes[2]{};
        for(unsigned i=0;i<2;++i) {
            writes[i]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};writes[i].dstSet=set_;writes[i].dstBinding=i;
            writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&buffers[i];
        }
        fn<PFN_vkUpdateDescriptorSets>(get,device,"vkUpdateDescriptorSets")(device,2,writes,0,nullptr);
        return true;
    } catch(const std::exception& e) {status_=e.what();return false;}
}
bool VulkanConnectedGrid::record(VkCommandBuffer command) const {
    if(!pipeline_ || !pipeline_->record(command,set_)) return false;
    prepared_=true;return true;
}
}
