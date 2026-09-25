/* Star Fox private extension to the pinned SDL backend. */
#include "starfox/render/sdl_vulkan_bridge.h"
static bool Starfox_Vulkan_WaitTimeline(void *device, VkSemaphore semaphore, uint64_t value)
{
    VulkanRenderer *renderer;
    VkResult result;
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkTimelineSemaphoreSubmitInfo timeline = {0};
    VkSubmitInfo submit = {0};
    if (!device || !semaphore) return SDL_SetError("Invalid external timeline dependency");
    renderer = (VulkanRenderer *)((SDL_GPUDevice *)device)->driverData;
    timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline.waitSemaphoreValueCount = 1; timeline.pWaitSemaphoreValues = &value;
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submit.pNext = &timeline;
    submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &semaphore;
    submit.pWaitDstStageMask = &stage;
    SDL_LockMutex(renderer->submitLock);
    result = renderer->vkQueueSubmit(renderer->unifiedQueue, 1, &submit, VK_NULL_HANDLE);
    SDL_UnlockMutex(renderer->submitLock);
    if (result != VK_SUCCESS) return SDL_SetError("External timeline submission failed: %d", (int)result);
    return true;
}
static bool Starfox_Vulkan_CopyExternal(void *command, VkBuffer source, uint64_t source_bytes,
                                       void *destination, uint32_t bytes)
{
    VulkanCommandBuffer *cmd = (VulkanCommandBuffer *)command;
    VulkanBufferContainer *container = (VulkanBufferContainer *)destination;
    VulkanRenderer *renderer;
    VulkanBuffer *target;
    VkBufferMemoryBarrier barrier = {0};
    VkBufferCopy copy = {0};
    if (!cmd || !source || !container || !bytes || bytes > source_bytes || bytes > container->activeBuffer->size || (bytes & 3))
        return SDL_SetError("Invalid external Vulkan copy range");
    renderer = cmd->renderer;
    SDL_LockRWLockForReading(renderer->defragLock);
    target = VULKAN_INTERNAL_PrepareBufferForWrite(renderer, cmd, container, true,
                                                   VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION);
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.buffer = source; barrier.size = VK_WHOLE_SIZE;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
    barrier.dstQueueFamilyIndex = renderer->queueFamilyIndex;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    renderer->vkCmdPipelineBarrier(cmd->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);
    copy.size = bytes;
    renderer->vkCmdCopyBuffer(cmd->commandBuffer, source, target->buffer, 1, &copy);
    barrier.srcQueueFamilyIndex = renderer->queueFamilyIndex;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT; barrier.dstAccessMask = 0;
    renderer->vkCmdPipelineBarrier(cmd->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);
    VULKAN_INTERNAL_BufferTransitionToDefaultUsage(renderer, cmd,
        VULKAN_BUFFER_USAGE_MODE_COPY_DESTINATION, target);
    VULKAN_INTERNAL_TrackBuffer(cmd, target);
    VULKAN_INTERNAL_TrackBufferTransfer(cmd, target);
    SDL_UnlockRWLock(renderer->defragLock);
    return true;
}
static void SDLCALL Starfox_Vulkan_DestroyBridge(void *userdata, void *value)
{
    (void)userdata;
    SDL_free(value);
}
static bool Starfox_Vulkan_CopyToExternal(void *command, void *source, VkBuffer destination,
                                         uint64_t destination_bytes, uint32_t bytes)
{
    VulkanCommandBuffer *cmd = (VulkanCommandBuffer *)command;
    VulkanBufferContainer *container = (VulkanBufferContainer *)source;
    VulkanRenderer *renderer;
    VulkanBuffer *buffer;
    VkBufferMemoryBarrier barrier = {0};
    VkBufferCopy copy = {0};
    if (!cmd || !container || !destination || !bytes || (bytes & 3)
        || bytes > destination_bytes || bytes > container->activeBuffer->size)
        return SDL_SetError("Invalid external Vulkan geometry copy range");
    renderer = cmd->renderer;
    SDL_LockRWLockForReading(renderer->defragLock);
    buffer = container->activeBuffer;
    VULKAN_INTERNAL_BufferTransitionFromDefaultUsage(renderer, cmd, VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE, buffer);
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.buffer = destination; barrier.size = VK_WHOLE_SIZE;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
    barrier.dstQueueFamilyIndex = renderer->queueFamilyIndex;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    renderer->vkCmdPipelineBarrier(cmd->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);
    copy.size = bytes;
    renderer->vkCmdCopyBuffer(cmd->commandBuffer, buffer->buffer, destination, 1, &copy);
    barrier.srcQueueFamilyIndex = renderer->queueFamilyIndex;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = 0;
    renderer->vkCmdPipelineBarrier(cmd->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);
    VULKAN_INTERNAL_BufferTransitionToDefaultUsage(renderer, cmd, VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE, buffer);
    VULKAN_INTERNAL_TrackBuffer(cmd, buffer);
    VULKAN_INTERNAL_TrackBufferTransfer(cmd, buffer);
    SDL_UnlockRWLock(renderer->defragLock);
    return true;
}
static bool Starfox_Vulkan_SignalTimeline(void *device, VkSemaphore semaphore, uint64_t value)
{
    VulkanRenderer *renderer;
    VkResult result;
    VkTimelineSemaphoreSubmitInfo timeline = {0};
    VkSubmitInfo submit = {0};
    if (!device || !semaphore || !value) return SDL_SetError("Invalid geometry completion timeline");
    renderer = (VulkanRenderer *)((SDL_GPUDevice *)device)->driverData;
    timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timeline.signalSemaphoreValueCount = 1; timeline.pSignalSemaphoreValues = &value;
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submit.pNext = &timeline;
    submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &semaphore;
    SDL_LockMutex(renderer->submitLock);
    result = renderer->vkQueueSubmit(renderer->unifiedQueue, 1, &submit, VK_NULL_HANDLE);
    SDL_UnlockMutex(renderer->submitLock);
    if (result != VK_SUCCESS) return SDL_SetError("Geometry completion submission failed: %d", (int)result);
    return true;
}
static const StarfoxSdlVulkanGeometryBridgeV1 starfox_vulkan_geometry_bridge = {1, Starfox_Vulkan_CopyToExternal, Starfox_Vulkan_SignalTimeline};
static VkCommandBuffer Starfox_Vulkan_RayCommand(void *command)
{
    VulkanCommandBuffer *cmd = (VulkanCommandBuffer *)command;
    return cmd ? cmd->commandBuffer : VK_NULL_HANDLE;
}
static VkBuffer Starfox_Vulkan_RayPrepareWrite(void *command, void *buffer)
{
    VulkanCommandBuffer *cmd = (VulkanCommandBuffer *)command;
    VulkanBufferContainer *container = (VulkanBufferContainer *)buffer;
    VulkanBuffer *active;
    if (!cmd || !container) { SDL_SetError("Invalid Vulkan ray output"); return VK_NULL_HANDLE; }
    SDL_LockRWLockForReading(cmd->renderer->defragLock);
    active = VULKAN_INTERNAL_PrepareBufferForWrite(cmd->renderer, cmd, container, true,
        VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE);
    SDL_UnlockRWLock(cmd->renderer->defragLock);
    return active ? active->buffer : VK_NULL_HANDLE;
}
static bool Starfox_Vulkan_RayFinishWrite(void *command, void *buffer)
{
    VulkanCommandBuffer *cmd = (VulkanCommandBuffer *)command;
    VulkanBufferContainer *container = (VulkanBufferContainer *)buffer;
    if (!cmd || !container) return SDL_SetError("Invalid Vulkan ray output finish");
    SDL_LockRWLockForReading(cmd->renderer->defragLock);
    VULKAN_INTERNAL_BufferTransitionToDefaultUsage(cmd->renderer, cmd,
        VULKAN_BUFFER_USAGE_MODE_COMPUTE_STORAGE_READ_WRITE, container->activeBuffer);
    VULKAN_INTERNAL_TrackBuffer(cmd, container->activeBuffer);
    SDL_UnlockRWLock(cmd->renderer->defragLock);
    return true;
}
static bool Starfox_Vulkan_RayCopyRange(void *command, void *source, uint64_t source_offset,
                                       VkBuffer destination, uint64_t capacity, uint32_t bytes)
{
    VulkanCommandBuffer *cmd = (VulkanCommandBuffer *)command;
    VulkanBufferContainer *container = (VulkanBufferContainer *)source;
    VulkanBuffer *active;
    VkBufferMemoryBarrier barrier = {0};
    VkBufferCopy copy = {0};
    if (!cmd || !container || !destination || !bytes || (bytes & 3) || (source_offset & 3)
        || bytes > capacity || source_offset > container->activeBuffer->size
        || bytes > container->activeBuffer->size - source_offset)
        return SDL_SetError("Invalid Vulkan ray buffer copy range");
    SDL_LockRWLockForReading(cmd->renderer->defragLock);
    active = container->activeBuffer;
    VULKAN_INTERNAL_BufferTransitionFromDefaultUsage(cmd->renderer, cmd,
        VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE, active);
    copy.srcOffset = source_offset;
    copy.size = bytes;
    cmd->renderer->vkCmdCopyBuffer(cmd->commandBuffer, active->buffer, destination, 1, &copy);
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    /* Reflections read the same vertex buffer in the compute shader after
       BLAS construction. Make the geometry copy visible to both consumers. */
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = destination;
    barrier.size = bytes;
    cmd->renderer->vkCmdPipelineBarrier(cmd->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, NULL, 1, &barrier, 0, NULL);
    VULKAN_INTERNAL_BufferTransitionToDefaultUsage(cmd->renderer, cmd,
        VULKAN_BUFFER_USAGE_MODE_COPY_SOURCE, active);
    VULKAN_INTERNAL_TrackBuffer(cmd, active);
    VULKAN_INTERNAL_TrackBufferTransfer(cmd, active);
    SDL_UnlockRWLock(cmd->renderer->defragLock);
    return true;
}
static const StarfoxSdlVulkanRayBridgeV3 starfox_vulkan_ray_bridge = {
    3, Starfox_Vulkan_RayCommand, Starfox_Vulkan_RayPrepareWrite,
    Starfox_Vulkan_RayFinishWrite, Starfox_Vulkan_RayCopyRange};
static void Starfox_Vulkan_PublishBridge(VulkanRenderer *renderer)
{
    StarfoxSdlVulkanBridgeV2 *bridge = SDL_calloc(1, sizeof(*bridge));
    if (!bridge) return;
    bridge->version = 2;
    bridge->instance = renderer->instance;
    bridge->physical_device = renderer->physicalDevice;
    bridge->device = renderer->logicalDevice;
    bridge->get_instance_proc = vkGetInstanceProcAddr;
    bridge->get_device_proc = renderer->vkGetDeviceProcAddr;
    bridge->queue_family = renderer->queueFamilyIndex;
    bridge->wait_timeline = Starfox_Vulkan_WaitTimeline;
    bridge->copy_external = Starfox_Vulkan_CopyExternal;
    SDL_SetPointerPropertyWithCleanup(renderer->props, STARFOX_SDL_VULKAN_BRIDGE,
                                     bridge, Starfox_Vulkan_DestroyBridge, NULL);
    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_VULKAN_GEOMETRY_BRIDGE, (void *)&starfox_vulkan_geometry_bridge);
    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_VULKAN_RAY_BRIDGE, (void *)&starfox_vulkan_ray_bridge);
}
