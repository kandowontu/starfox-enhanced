#pragma once
#include "starfox/render/software_renderer.hpp"
#include <memory>
#include <string>
namespace starfox::render {
void replay_raster_commands(const RasterCommands&,Framebuffer&,SurfaceBuffer*,bool clear_target=true);
// Borrowed SDL GPU buffers, valid until the next render or device release.
// pixels packs index/tag/surface-palette/valid into four bytes; surfaces is
// an optional float4 normal/depth buffer. Consumers must use the same device.
// Bit 27 denotes explicitly classified terrain in a background draw; it must
// follow visible ownership, never be inherited through model/HUD overwrites.
// Bit 28 marks a world sprite that retains its 2D styling tag but must not be
// excluded from temporal reconstruction or restored as screen-space HUD.
struct GpuRasterOutput {
    void* device{};
    void* pixels{};
    void* surfaces{};
    std::uint32_t width{},height{};
    std::uint64_t generation{};
    // Optional positive camera-space Z per visible pixel, zero where unknown.
    // Separate from the historical face-mean effects depth in surfaces.w.
    void* geometry_depth{};
    // Optional float4 previous-current XY pixels, camera Z, validity. Follows
    // visible colour ownership; unknown motion is not valid zero motion.
    void* motion{};
};
struct GpuGeometryDepthInput {
    void* planes{}; // float4 camera-space plane (normal.xyz, dot(normal, point)).
    std::uint32_t count{};
    float focal_x{},focal_y{},center_x{},center_y{};
    bool screen_aligned{}; // Constant-Z sprite planes tolerate sample remapping.
};
class GpuRaster {
public:
    GpuRaster();
    ~GpuRaster();
    bool render(RasterCommands&,Framebuffer&,SurfaceBuffer*);
    // Binning may switch per frame; the compute pipeline is created lazily.
    bool render_resident(void* device,RasterCommands&,bool surface_metadata,bool gpu_binning=false);
    // Consumes GpuClip's row-span layout directly.
    // A zero polygon count accepts null spans and produces cleared output,
    // including empty coverage/surface ownership, without uploading commands.
    // Texels must be a valid storage buffer for textured commands;
    // null is only valid for solid or empty input.
    // No upload,
    // submission, fence or readback. Caller owns command lifetime and must
    // cancel it on failure. Return buffers are borrowed until next operation.
    // Does not populate resident_output()/wait_for_completion(); caller submits.
    // Optional background fuses painter composition into rasterization. It must
    // match dimensions/device and must not alias this raster's output buffers.
    // Background surface ownership survives non-surface foreground writes.
    // wave_rows checks the inverse-wave source row plus the unshifted row.
    // All wave spans in this call must share wave_offset/wave_frame;
    // ordinary/textured spans still use their destination row.
    // texel_bytes is the bound buffer size for repeated-row mask bounds checks;
    // zero rejects mask reads. Ordinary texture commands retain their UV rules.
    GpuRasterOutput enqueue_row_spans(void* device,void* command,void* spans,
        std::uint32_t polygon_count,std::uint32_t width,std::uint32_t height,bool surface_metadata=false,void* texels=nullptr,bool pixel_coverage=false,
        const GpuRasterOutput* background=nullptr,bool wave_rows=false,
        std::int16_t wave_offset=0,std::uint32_t wave_frame=0,std::uint32_t texel_bytes=0,
        const GpuGeometryDepthInput* geometry_depth=nullptr,std::array<std::uint32_t,2> raster_size={},std::array<float,2> raster_jitter={});
    // Upload legacy raster commands onto a caller-owned command buffer, for
    // ordered interleaving with GpuModel/GpuScene. No submit/readback/wait.
    // Result has explicit write coverage; consume before the next operation.
    // Caller cancels the command on failure. Do not mix with pending submits.
    GpuRasterOutput enqueue_commands(void* device,void* command,RasterCommands&,
        bool surface_metadata=false,bool gpu_binning=false,std::array<std::uint32_t,2> raster_size={},std::array<float,2> raster_jitter={});
    // With pixel_coverage=true, packed bit 26 marks every geometry write,
    // including palette index zero. Used when merging separate model layers.
    [[nodiscard]] GpuRasterOutput resident_output() const;
    bool readback(Framebuffer&,SurfaceBuffer*);
    // Fence-only diagnostic synchronization; retains resident data, no readback.
    bool wait_for_completion();
    // Required before destroying a borrowed renderer/device.
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
