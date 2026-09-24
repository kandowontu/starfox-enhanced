#pragma once
#include "starfox/vr/eye_camera.hpp"
#include <vulkan/vulkan.h>
#include <string>
#include <cstdint>
namespace starfox::vr {
class VulkanPipelineCache;
struct SceneVertex {
    bool operator==(const SceneVertex&) const = default;
    float position[3];float color[4];
    float odd_color[4]{};
    std::uint32_t dither_scale{}; // 0: solid; otherwise logical pixel size.
    float visibility_a[3]{},visibility_b[3]{},visibility_c[3]{};
    // 0 disabled, 1 visibility; 2 explosion: visibility triples = source
    // rotation rows, group_a translation, group_b normal, group_c count/units/Q15.
    std::uint32_t visibility_enabled{};
    float group_a[3]{},group_b[3]{},group_c[3]{};
    std::uint32_t group_enabled{};
    float uv[2]{};
    std::uint32_t texture[4]{}; // offset, u mask, v mask, flags (enabled, sRGB).
    // Flag 1073741824: R8 shadow coverage, four-byte row alignment; y/z
    // hold width-1/height-1 (not wrap masks). RGB comes from vertex color and
    // coverage modulates its alpha. x is the storage-buffer word offset.
    float billboard[2]{}; // Eye-facing offset in model units; texture flag 4.
    // Span flag 65536: uv holds source XY projection numerators, billboard[0]
    // holds source depth (zero means legacy divisor 1). Interpolate then divide
    // so source coverage stays attached to a tilted face in either eye.
    // Flag 131072 selects the span header through resident lookup metadata:
    // texture[0] = metadata word offset, texture[1] = ordered occurrence slot.
    // Positions and source numerators remain explicitly supplied by the caller.
    // Combined flag 262144 fetches a transformed polygon corner instead:
    // texture[2] = corner index; billboard[1] = source units per world unit.
    // Combined flag 524288 instead constructs a coverage quad on the source
    // face plane; texture[2] selects one of four expanded projected corners.
};
// Flat-color triangle/line pass. Input positions are right-handed world coordinates
// in the same units as EyeCamera. Caller owns vertex buffers and GPU lifetime.
enum class SceneTopology {triangles,lines};
enum class SceneBlend {opaque,add,subtract,half_add,half_subtract,shadow,alpha};
class VulkanScenePipeline {
public:
    ~VulkanScenePipeline();
    VulkanScenePipeline()=default;
    VulkanScenePipeline(const VulkanScenePipeline&)=delete;
    VulkanScenePipeline& operator=(const VulkanScenePipeline&)=delete;
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,VkRenderPass,bool depth_test=false,
        SceneTopology topology=SceneTopology::triangles,VkDescriptorSetLayout textures=VK_NULL_HANDLE,
        SceneBlend blend=SceneBlend::opaque,VulkanPipelineCache* cache=nullptr);
    void close() noexcept;
    bool record(VkCommandBuffer,VkExtent2D,VkBuffer,uint32_t vertices,const EyeCamera&,VkDescriptorSet textures=VK_NULL_HANDLE) const;
    // Draw a primitive-aligned subrange without uploading another buffer.
    // total_vertices is the allocation's vertex count, not the requested count.
    bool record_range(VkCommandBuffer,VkExtent2D,VkBuffer,uint32_t total_vertices,
        uint32_t first,uint32_t count,const EyeCamera&,VkDescriptorSet textures=VK_NULL_HANDLE) const;
    bool record_model(VkCommandBuffer,VkExtent2D,VkBuffer,uint32_t vertices,
        const EyeCamera&,const Matrix4& model,VkDescriptorSet textures=VK_NULL_HANDLE) const;
    const std::string& status() const noexcept {return status_;}
private:
    SceneTopology topology_{SceneTopology::triangles};
    bool textured_{};
    PFN_vkCmdBindDescriptorSets bind_descriptors_{};
    VkDevice device_{};VkPipeline pipeline_{};VkPipelineLayout layout_{};
    PFN_vkDestroyPipeline destroy_pipeline_{};
    PFN_vkDestroyPipelineLayout destroy_layout_{};
    PFN_vkCmdBindPipeline bind_pipeline_{};
    PFN_vkCmdBindVertexBuffers bind_vertices_{};
    PFN_vkCmdPushConstants push_{};
    PFN_vkCmdSetViewport viewport_{};
    PFN_vkCmdSetScissor scissor_{};
    PFN_vkCmdDraw draw_{};
    std::string status_{"Scene pipeline not initialized"};
};
}
