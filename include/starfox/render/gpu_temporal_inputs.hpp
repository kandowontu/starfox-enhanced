#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <array>

namespace starfox::render {
struct GpuTemporalTextures {
    void *device{}, *depth{}, *motion{}, *exposure{};
    std::uint32_t width{},height{};
};
struct GpuTemporalResampled {void* color{};GpuTemporalTextures guides;};
struct TemporalGroundInputs {
    void* coverage{}; // uint per stored pixel: exactly 1 means visible terrain.
    bool packed_coverage{}; // instead read compositor's explicit terrain bit 27
    std::array<float,4> plane{}; // camera-space normal.xyz, -dot(point,normal)
    std::array<float,4> projection{1,1,0,0},previous_projection{1,1,0,0};
    std::array<float,16> current_to_previous{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<float,2> raster_jitter{}; // current raster-pixel displacement
    bool previous_valid{};
};
// Convert resident linear camera Z / float4(pixel motion,Z,valid) buffers into
// projected R32 depth, RG32 motion and 1x1 R32 exposure. Unknown correspondence
// is encoded as -FLT_MAX, never fabricated valid zero motion. No submission or
// CPU readback; call between passes and retain this object through completion.
// Motion may be null on reset; that branch never reads the motion buffer.
class GpuTemporalInputs {
public:
    GpuTemporalInputs();~GpuTemporalInputs();
    GpuTemporalResampled resample(void* device,void* command,void* color,
        const GpuTemporalTextures&,std::uint32_t width,std::uint32_t height);
    GpuTemporalTextures enqueue(void* device,void* command,void* camera_depth,void* motion,
        std::uint32_t width,std::uint32_t height,float near_plane,float far_plane,bool reset,
        const TemporalGroundInputs* ground=nullptr);
    // Restore exact HUD and non-terrain tilemap artwork after neural
    // evaluation. This is output protection, not pre-evaluation HUD exclusion.
    void* restore_hud(void* device,void* command,void* original_rgba,void* reconstructed_rgba,
        void* packed_pixels,std::uint32_t width,std::uint32_t height,bool preserve_artwork=true);
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
