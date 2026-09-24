#pragma once
#include "starfox/vr/draw_packet.hpp"
#include <memory>

namespace starfox::vr {
// Immutable GPU geometry with replaceable model transforms. Keep alive until
// both eyes' fences complete. Successful
// replacement/close also requires completed GPU use; failed initialization
// preserves the previous scene. No uploads or allocations during record().
class VulkanDrawPackets {
public:
    struct RaySource {VkDescriptorBufferInfo vertices{};uint32_t count{};Matrix4 model{};};
    // Borrow resident position-first triangle vertices by full packet key.
    // Valid until initialize/update_models/close. Rejects shader-generated
    // positions; texture alpha still needs coverage handling by the ray scene.
    bool ray_source(uint32_t key,RaySource&) const;
    VulkanDrawPackets();
    ~VulkanDrawPackets();
    VulkanDrawPackets(const VulkanDrawPackets&)=delete;
    VulkanDrawPackets& operator=(const VulkanDrawPackets&)=delete;
    // Borrowed render-thread cache; must outlive initialization calls.
    void set_pipeline_cache(VulkanPipelineCache* cache) noexcept {cache_=cache;}
    bool initialize(VkDevice,PFN_vkGetDeviceProcAddr,const VkPhysicalDeviceMemoryProperties&,
        VkRenderPass,std::span<const DrawPacket>,std::span<const uint32_t> object_keys={},
        bool depth_test=true);
    // depth_test=false is an ordered layer pass: neither reads nor writes
    // scene depth. Geometry reuse is independent of this pipeline policy.
    // Optional unique keys cover every packet, including empty packets. Keys
    // may encode a source handle plus a draw-pass identity. They only locate
    // reuse candidates: exact geometry comparison is still required,
    // so recycled object slots cannot inherit stale GPU data.
    bool record(VkCommandBuffer,VkExtent2D,const EyeCamera&) const;
    // Prepare shader-generated grid storage before entering the render pass.
    // Other immutable packet types need no work. No allocations or CPU waits.
    bool record_compute(VkCommandBuffer) const;
    // Diagnostic readback only, after all prior GPU use completes.
    bool readback_connected_grid(std::size_t,std::span<uint32_t>) const;
    // Draw a contiguous range of nonempty uploaded items, allowing resident
    // compute models to be interleaved without changing painter/pass order.
    // Bounds and transforms are checked before recording any commands.
    bool record_range(VkCommandBuffer,VkExtent2D,const EyeCamera&,std::size_t first,std::size_t count) const;
    // After prior eye fences complete, update only nonempty packets' model
    // matrices in submission order. Caller must establish identical geometry.
    // Transactional validation; no Vulkan calls, uploads or allocations.
    bool update_models(std::span<const Matrix4>) noexcept;
    void close() noexcept;
    std::size_t size() const noexcept;
    std::size_t reused_packets() const noexcept;
    std::size_t uploaded_packets() const noexcept;
    std::size_t uploaded_vertex_buffers() const noexcept;
    std::size_t uploaded_texture_buffers() const noexcept;
    std::size_t allocated_grid_outputs() const noexcept;
    std::size_t reused_grid_outputs() const noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    VulkanPipelineCache* cache_{};
    struct State;
    std::unique_ptr<State> state_;
    std::string status_{"Draw packets not initialized"};
};
}
