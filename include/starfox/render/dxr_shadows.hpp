#pragma once
#include "starfox/render/shadow_mask.hpp"
#include <memory>
#include <string>

namespace starfox::render::shadows {
// Optional Windows DXR 1.1 backend. Unsupported devices/platforms return false;
// callers retain the CPU implementation. No geometry survives between frames.
class DxrShadows {
public:
    DxrShadows();
    ~DxrShadows();
    DxrShadows(const DxrShadows&) = delete;
    DxrShadows& operator=(const DxrShadows&) = delete;
    bool render(const Scene&, Camera, Vec3 light, std::optional<ReceiverPlane>,
        std::vector<std::uint8_t>&);
    [[nodiscard]] const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
