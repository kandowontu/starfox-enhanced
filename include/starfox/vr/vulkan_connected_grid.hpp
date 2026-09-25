#pragma once
#include "starfox/vr/vulkan_source_storage.hpp"
#include <array>
#include <memory>
namespace starfox::vr {
inline constexpr uint32_t gpu_connected_grid_flag=512U|4194304U;
inline constexpr uint32_t connected_grid_output_words=384+225*15+192*675+225*4;
// Shared pipelines; frame-local descriptors/storage. Finish prior GPU use
// before destruction. record() runs outside a render pass, no allocation/wait.
class VulkanConnectedGridPipeline {
public:
    VulkanConnectedGridPipeline()=default;
    VulkanConnectedGridPipeline(const VulkanConnectedGridPipeline&)=delete;
    VulkanConnectedGridPipeline& operator=(const VulkanConnectedGridPipeline&)=delete;
    ~VulkanConnectedGridPipeline();
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,VkPipelineCache);
    bool record(VkCommandBuffer,VkDescriptorSet) const;
    VkDescriptorSetLayout descriptor_layout() const {return set_;}
    const std::string& status() const {return status_;}
private:
    VkDevice device_{};PFN_vkGetDeviceProcAddr get_{};
    VkDescriptorSetLayout set_{};VkPipelineLayout layout_{};
    std::array<VkPipeline,2> pipelines_{};
    std::string status_;
};
class VulkanConnectedGrid {
public:
    VulkanConnectedGrid()=default;
    VulkanConnectedGrid(const VulkanConnectedGrid&)=delete;
    VulkanConnectedGrid& operator=(const VulkanConnectedGrid&)=delete;
    ~VulkanConnectedGrid();
    // prior is a completed outgoing frame. Share only its output arena; new
    // inputs/descriptors remain private until the owning scene commits. No
    // output writes occur during initialize, including failed replacements.
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,
        std::span<const uint32_t>,std::shared_ptr<VulkanConnectedGridPipeline>,const VulkanConnectedGrid* prior=nullptr);
    bool record(VkCommandBuffer) const;
    VkBuffer buffer() const {return output_?output_->buffer():VK_NULL_HANDLE;}
    VkDeviceSize size() const {return output_?output_->size():0;}
    bool reused_output() const {return reused_output_;}
    bool prepared() const {return prepared_;}
    // Diagnostic only; caller must have waited for the submission fence.
    bool readback(std::span<uint32_t> words) {
        return prepared_ && output_ && words.size()==connected_grid_output_words && output_->readback(0,std::as_writable_bytes(words));
    }
    const std::string& status() const {return status_;}
private:
    VkDevice device_{};PFN_vkGetDeviceProcAddr get_{};VkDescriptorPool pool_{};VkDescriptorSet set_{};
    VulkanSourceStorage input_;
    std::shared_ptr<VulkanSourceStorage> output_;
    std::shared_ptr<VulkanConnectedGridPipeline> pipeline_;
    mutable bool prepared_{};
    bool reused_output_{};
    std::string status_;
};
}
