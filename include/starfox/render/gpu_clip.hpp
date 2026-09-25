#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <array>
namespace starfox::render {
struct NativeClipSettings {
    std::uint32_t polygon_count{},point_count{},corner_count{},visibility_count{};
    std::int32_t width{},height{};
    std::uint32_t reserved[2]{};
};
static_assert(sizeof(NativeClipSettings)==32);
struct GpuSpanOrder {
    void* indices{};void* results{};
    std::uint32_t first{},capacity{},tree_index{};
};
class GpuClip {
public:
    [[nodiscard]] std::uint32_t mask_buffer_bytes() const noexcept;
    GpuClip();~GpuClip();
    // Caller-owned SDL device, command and storage buffers; no submit/readback.
    // points=int4 projection output, corners=uint4(point,U,V,reserved),
    // polygons=uint4(first,count,visibility,reserved), visibility=uint flags.
    // Borrowed output: 129 int4 per polygon (header + 128 clipped XYUV corners).
    // Header(count,status,0,0): status 1 invalid input, 2 scratch overflow.
    // Invalid/overflow results must not render. Consume before next enqueue;
    // cancel command on failure and release before destroying the device.
    void* enqueue(void* device,void* command,void* points,void* corners,
        void* polygons,void* visibility,const NativeClipSettings& settings,bool continuous=false,
        void* projection_params=nullptr,std::uint32_t projection_count=0,
        void* point_residuals=nullptr,std::uint32_t residual_count=0);
    // Optional continuous projection residuals (32 bytes/point). Retains screen
    // tails through clipping; count must cover every referenced point. Format 2
    // retains raw binary64 screen coordinates (see ContinuousProjectedPoint).
    // Format 3 carries raw binary64 camera XYZ low/high words. Projection
    // parameters are required; polygon backface evaluation preserves these
    // values through near-plane intersections before narrowing screen output.
    // With continuous=true, points are ContinuousProjectedPoint records and
    // output payload is float4(X,Y,U,V), with the same bitwise int4 header.
    // Optional per-polygon float4(vanish X,Y,focal,accurate_projection) enables
    // near clipping. W=1 also reprojects camera points with compensated math,
    // retaining projection residuals through screen clipping. W=2 preserves
    // supplied screen XY while enabling exact area/line boundary arithmetic;
    // W=0 uses the generic screen-XY path.
    // Descriptor.w bit 0 marks textures, which source rules skip when behind.
    // Bit 1 with two corners selects line clipping. Successful line output has
    // header (2,0,1,0), preserving endpoint order for source line stepping.
    // Bit 2 with one corner selects a textured sprite face; header (1,0,2,0)
    // Bit 3 rejects nonnegative projected polygon area after near clipping,
    // before viewport clipping. Lines and sprites do not use this optional cull.
    // carries projected centre XY and depth for the source-size span emitter.
    // Status 3 means near clipping needs missing projection parameters.
    // For continuous=false, the optional buffer instead contains one
    // NativeProjectionPoint per vertex (source camera words + vanish point),
    // and projection_count is its vertex count. This enables source midpoint
    // near clipping. Every polygon must use one common vanishing point.
    // Native solid/affine texture fill. Reads last clipped output; materials
    // is one RasterCommand template per polygon. Output has polygon_count *
    // (height * render_scale) RasterCommands in painter order, including empty rows.
    // Lines use source major-axis stepping, scaled thickness and dither, and
    // do not replace surface metadata. Repeated-row EX modes require masks.
    // Optional BSP order: indices/results are GpuBsp's resident outputs. first,
    // capacity and tree_index must match the tree descriptor and buffer sizes.
    // Ordered output has capacity * scaled height rows; failed trees emit none.
    // Supplying masked_texels enables solid repeated-row coverage masks. The
    // source's aligned texel prefix is copied GPU-to-GPU, followed by one mask
    // per painter slot. The returned combined buffer must replace source_texels
    // for raster consumption, before the next enqueue. No CPU mask readback.
    // Mask storage is bounded to 256 MiB and output is null on failure.
    // reuse_span_scratch requires every consumer to be encoded before the next
    // enqueue on the same ordered queue. Diagnostic/deferred consumers retain
    // the default cycling behavior; no CPU access or cross-queue reuse allowed.
    void* enqueue_spans(void* command,void* materials,bool winding_independent=false,std::uint32_t render_scale=1,
        const GpuSpanOrder* order=nullptr,std::uint32_t line_thickness=1,
        void* source_texels=nullptr,std::uint32_t source_texel_bytes=0,void** masked_texels=nullptr,
        std::array<std::uint32_t,2> raster_size={},bool reuse_span_scratch=false);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
