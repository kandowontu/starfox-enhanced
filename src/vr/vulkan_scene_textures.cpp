#include "starfox/vr/vulkan_scene_textures.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include <cstring>
#include <stdexcept>
namespace starfox::vr {
namespace {
template<class T> T entry(PFN_vkGetDeviceProcAddr get,VkDevice device,const char* name) {
    auto fn=reinterpret_cast<T>(get(device,name));
    if(!fn) throw std::runtime_error(std::string("Missing Vulkan entry point: ")+name);
    return fn;
}
void check(VkResult result,const char* name) {if(result!=VK_SUCCESS) throw std::runtime_error(std::string(name)+": "+std::to_string(result));}
}
VulkanSceneTextures::~VulkanSceneTextures() {close();}
void VulkanSceneTextures::close() noexcept {
    if(!device_) return;
    if(pool_) reinterpret_cast<PFN_vkDestroyDescriptorPool>(get_(device_,"vkDestroyDescriptorPool"))(device_,pool_,nullptr);
    if(layout_) reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(get_(device_,"vkDestroyDescriptorSetLayout"))(device_,layout_,nullptr);
    if(buffer_) reinterpret_cast<PFN_vkDestroyBuffer>(get_(device_,"vkDestroyBuffer"))(device_,buffer_,nullptr);
    if(memory_) reinterpret_cast<PFN_vkFreeMemory>(get_(device_,"vkFreeMemory"))(device_,memory_,nullptr);
    device_={};buffer_={};memory_={};pool_={};layout_={};set_={};get_={};
}
bool VulkanSceneTextures::initialize(VkDevice device,PFN_vkGetDeviceProcAddr get,
    const VkPhysicalDeviceMemoryProperties& properties,std::span<const uint32_t> texels) {
    close();
    try {
        if(!device || !get || texels.empty() || texels.size()>backdrop_texture_word_limit || properties.memoryTypeCount>VK_MAX_MEMORY_TYPES)
            throw std::runtime_error("Invalid scene texture upload");
        // Resolve cleanup operations before creating anything.
        entry<PFN_vkDestroyDescriptorPool>(get,device,"vkDestroyDescriptorPool");
        entry<PFN_vkDestroyDescriptorSetLayout>(get,device,"vkDestroyDescriptorSetLayout");
        entry<PFN_vkDestroyBuffer>(get,device,"vkDestroyBuffer");
        entry<PFN_vkFreeMemory>(get,device,"vkFreeMemory");
        device_=device;get_=get;
#define FN(name) entry<PFN_##name>(get,device,#name)
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size=texels.size_bytes();info.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(FN(vkCreateBuffer)(device,&info,nullptr,&buffer_),"Create texture buffer");
        VkMemoryRequirements required{};FN(vkGetBufferMemoryRequirements)(device,buffer_,&required);
        if(required.size<info.size) throw std::runtime_error("Invalid texture allocation size");
        uint32_t selected=VK_MAX_MEMORY_TYPES;
        for(uint32_t i=0;i<properties.memoryTypeCount;++i)
            if((required.memoryTypeBits&(1U<<i)) && (properties.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                selected=i;
                if(properties.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) break;
            }
        if(selected==VK_MAX_MEMORY_TYPES) throw std::runtime_error("No host-visible texture memory");
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize=required.size;allocation.memoryTypeIndex=selected;
        check(FN(vkAllocateMemory)(device,&allocation,nullptr,&memory_),"Allocate texture memory");
        check(FN(vkBindBufferMemory)(device,buffer_,memory_,0),"Bind texture memory");
        const auto unmap=FN(vkUnmapMemory);const auto flush=FN(vkFlushMappedMemoryRanges);
        void* mapped{};check(FN(vkMapMemory)(device,memory_,0,VK_WHOLE_SIZE,0,&mapped),"Map texture memory");
        std::memcpy(mapped,texels.data(),texels.size_bytes());
        VkResult flushed=VK_SUCCESS;
        if(!(properties.memoryTypes[selected].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=memory_;range.size=VK_WHOLE_SIZE;
            flushed=flush(device,1,&range);
        }
        unmap(device,memory_);check(flushed,"Flush texture memory");
        bind_storage({buffer_,0,info.size});
#undef FN
        status_="Scene texels uploaded";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
void VulkanSceneTextures::bind_storage(const VkDescriptorBufferInfo& storage) {
        const auto device=device_;const auto get=get_;
#define FN(name) entry<PFN_##name>(get,device,#name)
        VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};layout.bindingCount=1;layout.pBindings=&binding;
        check(FN(vkCreateDescriptorSetLayout)(device,&layout,nullptr,&layout_),"Create texture layout");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
        check(FN(vkCreateDescriptorPool)(device,&pool,nullptr,&pool_),"Create texture pool");
        VkDescriptorSetAllocateInfo sets{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};sets.descriptorPool=pool_;sets.descriptorSetCount=1;sets.pSetLayouts=&layout_;
        check(FN(vkAllocateDescriptorSets)(device,&sets,&set_),"Allocate texture set");
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=set_;write.descriptorCount=1;
        write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&storage;
        FN(vkUpdateDescriptorSets)(device,1,&write,0,nullptr);
#undef FN
}
bool VulkanSceneTextures::initialize_external(VkDevice device,PFN_vkGetDeviceProcAddr get,VkBuffer buffer,VkDeviceSize bytes) {
    close();
    try {
        if(!device || !get || !buffer || !bytes || bytes%4 || bytes>256ULL*1024*1024)
            throw std::runtime_error("Invalid external scene storage");
        entry<PFN_vkDestroyDescriptorPool>(get,device,"vkDestroyDescriptorPool");
        entry<PFN_vkDestroyDescriptorSetLayout>(get,device,"vkDestroyDescriptorSetLayout");
        device_=device;get_=get;
        // buffer_ and memory_ intentionally remain empty: close() owns only
        // the descriptor resources, never the producer's buffer/allocation.
        bind_storage({buffer,0,bytes});
        status_="External scene storage bound";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
}
