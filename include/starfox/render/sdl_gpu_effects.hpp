#pragma once
#include "starfox/render/gpu_effects.hpp"
namespace starfox::render {
class SdlGpuEffects {
public:
    SdlGpuEffects();
    ~SdlGpuEffects();
    bool apply(void* device,const Framebuffer&,std::vector<std::uint8_t>&,const GpuEffectSettings&);
    bool readback(std::vector<std::uint8_t>&);
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
