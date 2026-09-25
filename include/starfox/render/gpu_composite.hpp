#pragma once
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/palette.hpp"
namespace starfox::render {
// All handles are borrowed and share one SDL GPU device. Output is valid until
// the next composition or release. packed uses GpuRasterOutput's byte layout.
struct GpuCompositeOutput {
    void* device{};
    void* rgba{};
    void* packed{};
    void* surfaces{};
    std::uint32_t width{},height{};
    void* geometry_depth{};
    void* motion{};
};
// Resident background is in final stored coordinates. CPU writes made after
// that background but before the native model insertion have their own mask;
// they must survive the background without incorrectly occluding the models.
struct GpuCompositeBackground {
    GpuRasterOutput raster;
    std::span<const std::uint8_t> cpu_coverage;
    // Optional solid outer margins sampled from the composed native edges.
    std::uint32_t margin_origin{},margin_width{256};
    // Controls uses its right-hand panel ink on both widescreen margins.
    bool match_right_margin{};
    // Repair palette-zero background cells before models/foreground, using
    // the early background's native top-left colour (EX introductory logo).
    bool repair_transparent_margins{};
};
class GpuComposite {
public:
    GpuComposite();
    ~GpuComposite();
    // cpu contains the background plus subsequent foreground writes. Coverage
    // must mark EVERY subsequent write, including black and same-colour writes.
    // world_only excludes two_d-tagged CPU/native/late artwork except native
    // world sprites explicitly marked with packed bit 28. Supply an
    // actual pre-HUD CPU background; a final HUD-painted frame cannot recover
    // the background it overwrote. Used by experimental temporal reconstruction.
    bool compose(const GpuRasterOutput&,std::uint32_t source_scale,
        const Framebuffer& cpu,std::span<const std::uint8_t> foreground,
        const LayerCompositeSettings&,std::span<const Rgba8>,
        const GpuRasterOutput* late_overlay=nullptr,
        const GpuCompositeBackground* background=nullptr,
        std::span<const std::uint8_t> after_late_coverage={},bool world_only=false,
        // Optional source reference size and independent output size, in pixels.
        // CPU retains reference dimensions; late/background may independently
        // use reduced dimensions and are sampled directly into the output.
        std::array<std::uint32_t,4> raster_mapping={},std::array<float,2> cpu_jitter={});
    // CPU writes after the late GPU layer are restored last, including zero
    // and unchanged colours. This mask is independent of pre-late foreground.
    // Optional late overlay is already in final stored-pixel coordinates,
    // on the same device. Explicit coverage (bit 26), including colour zero,
    // replaces earlier colour/tags and invalidates underlying model normals.
    GpuCompositeOutput output() const;
    // CPU image bytes submitted on the most recent successful composition.
    // Palette uploads are independent and are not included.
    std::size_t last_cpu_upload_bytes() const noexcept;
    // Palette bytes submitted on the most recent successful composition.
    std::size_t last_palette_upload_bytes() const noexcept;
    bool readback(Framebuffer&,std::vector<std::uint8_t>& rgba,SurfaceBuffer* = nullptr);
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
