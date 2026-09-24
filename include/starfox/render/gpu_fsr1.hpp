#pragma once
#include "starfox/render/fsr1_settings.hpp"
#include "starfox/render/gpu_composite.hpp"
#include <memory>
#include <string>

namespace starfox::render {
class GpuFsr1 {
public:
    GpuFsr1();
    ~GpuFsr1();
    // Borrowed SDL device, command buffer and sampled opaque scene texture.
    // Records EASU then RCAS without submission, readback or CPU/GPU waits.
    // No pass may be active. On failure cancel the caller's command buffer.
    // Returned texture is borrowed; consume before next enqueue. Never use it
    // as input. Submit/cancel all commands before release_device/device teardown.
    void* enqueue(void* device,void* command,void* input,Fsr1Extent source,
        Fsr1Extent output,float sharpness=0.2F);
    // Upscale the HUD-free scene, then restore the original full-resolution
    // HUD. Background artwork is spatially reconstructed, unlike DLSS.
    GpuCompositeOutput enqueue_composite(void* command,const GpuCompositeOutput& scene,
        const GpuCompositeOutput& original,float sharpness=0.2F);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
