#pragma once
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/particle_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include <cstdint>
#include <memory>
#include <string>
namespace starfox::render {
struct GridLattice;
struct NativeProjectionPoint {
    std::int32_t x{},y{},z{},reserved{};
    std::int32_t vanish_x{},vanish_y{},reserved1{},reserved2{};
};
static_assert(sizeof(NativeProjectionPoint)==32);
// Vertices are pre-scaled source words; one pose is shared by many vertices.
struct NativeTransformVertex { std::int32_t x{},y{},z{}; std::uint32_t pose{}; };
struct NativeTransformPose {
    std::int32_t row0[4]{},row1[4]{},row2[4]{};
    std::int32_t translation[4]{},vanish[4]{};
};
// Opt-in destruction payload. Matrix-row W carries the authored face normal;
// vanish.z carries al_count. The GPU rotates the normal without vertex scale,
// forces it downward, and adds its offset after the wrapped object transform.
inline constexpr std::int32_t kNativeExplosionPose=0x455850;
constexpr NativeTransformPose native_explosion_pose(NativeTransformPose pose,
    std::int32_t nx,std::int32_t ny,std::int32_t nz,std::uint8_t progress) noexcept {
    pose.row0[3]=nx;pose.row1[3]=ny;pose.row2[3]=nz;
    pose.vanish[2]=progress;pose.vanish[3]=kNativeExplosionPose;return pose;
}
static_assert(sizeof(NativeTransformVertex)==16 && sizeof(NativeTransformPose)==80);
struct ContinuousTransformVertex { float x{},y{},z{}; std::uint32_t pose{}; };
// Matrix rows include the vertex scale; translation.w is the focal length.
// vanish.w=1 enables a coefficient-residual pose at index+2. That opt-in
// performs compensated transform/projection; otherwise the original FP32
// path remains available. Both records must fit the supplied pose count.
// vanish.z optionally indexes a destruction-normal record plus one and its
// residual record. That record carries an unscaled matrix, authored normal in
// row W, progress in translation.w, and vanish.w=1 for source Q15 rounding.
struct ContinuousTransformPose {
    float row0[4]{},row1[4]{},row2[4]{};
    float translation[4]{},vanish[4]{};
};
struct ContinuousProjectedPoint { float camera[4]{},screen[4]{}; };
static_assert(sizeof(ContinuousTransformVertex)==16 && sizeof(ContinuousTransformPose)==80
    && sizeof(ContinuousProjectedPoint)==32);
class GpuProjection {
public:
    // Reconstruct current camera positions from an owned model's planar depth,
    // then project the same points through its previous camera-space transform.
    // Caller supplies correspondence only for an unchanged rigid model; do not
    // apply one object's transform to a merged scene. Projections are in stored
    // pixels (focal X/Y, center X/Y); matrix rows map current camera to previous.
    struct MotionSurfaceSettings {
        std::uint32_t width{},height{},reset_history{},reserved{};
        float current_projection[4]{1,1,0,0};
        float previous_projection[4]{1,1,0,0};
        float previous_row0[4]{1,0,0,0};
        float previous_row1[4]{0,1,0,0};
        float previous_row2[4]{0,0,1,0};
        // Current raster jitter in pixels; previous projection is unjittered.
        float jitter_x{},jitter_y{},previous_near{},padding{};
    };
    // Borrowed float4 XY previous-current (pixels), linear Z, validity. Invalid
    // depth/history/behind-camera points are explicitly invalid, not zero MV.
    void* enqueue_motion_surface(void* device,void* command,void* camera_depth,
        const MotionSurfaceSettings& settings);
    // Paired, unjittered ContinuousProjectedPoint buffers. Output float4 is
    // previous-current XY in render pixels, current linear Z, and validity.
    // Invalid/disoccluded vertices have validity=0, not valid zero motion.
    // Caller owns input sizes, correspondence, command submission and lifetime.
    // This is vertex motion, not yet a rasterized DLSS input surface.
    void* enqueue_motion(void* device, void* command, void* current_points,
        void* previous_points, std::uint32_t count, float scale_x, float scale_y,
        bool reset_history=false);
    // Borrowed packed pixels (including coverage), no CPU projection/readback.
    void* enqueue_text(void* device,void* command,const ScaledTextRenderer::ProjectedFrame&,
        std::uint32_t width,std::uint32_t height,std::uint32_t scale,std::uint8_t tag,
        float eye_x=0,float convergence=512,std::array<std::uint32_t,2> logical_viewport={},std::array<float,2> raster_jitter={});
    struct ParticleSettings {
        double owner_x{},owner_y{},owner_z{},alpha{};
        std::uint32_t width{},height{},count{},padding{};
        float eye_x{},convergence{512};std::uint32_t reserved[2]{};
    };
    static_assert(sizeof(ParticleSettings)==64);
    // Input: two int4 records, current XYZ/palette+trail, previous XYZ/padding.
    // Output: two int4 records (current XY/colour/flags, previous XY/0/visible).
    // Source words must be sign-extended; borrowed output, caller submits.
    void* enqueue_particles(void* device,void* command,void* points,const ParticleSettings&);
    // Upload an owned source snapshot; interpolation and projection stay on GPU.
    void* enqueue_particle_frame(void* device,void* command,const ParticleRenderer::OwnerFrame&,
        std::uint32_t width,std::uint32_t height,float eye_x=0,float convergence=512);
    void* enqueue_particle_spans(void* command,std::uint32_t height,std::uint32_t scale,
        std::uint8_t tag,std::int16_t clip_left=0,std::int16_t clip_right=0,
        std::array<std::uint32_t,3> raster_mapping={},std::array<float,2> raster_jitter={}); // logical W/H, output W
    struct DustSettings {
        std::int32_t row_x[4]{},row_y[4]{},row_z[4]{};
        std::int32_t viewport[4]{};
        std::uint32_t count{};float eye_x{},convergence{512};std::uint32_t padding{};
    };
    static_assert(sizeof(DustSettings)==80);
    // Input: 32-byte records, raw binary64 relative XYZ plus 8 padding bytes.
    // Colours: 64 uint32 palette entries. Output: int4 XY, palette, flags
    // (bit0 visible, bit1 near companion). Borrowed, no submit/readback.
    void* enqueue_dust(void* device,void* command,void* points,void* colours,const DustSettings&);
    // Upload immutable source data and project on the same caller command.
    void* enqueue_dust_frame(void* device,void* command,const DustRenderer::DustFrame&,
        std::uint32_t width,std::uint32_t height,float eye_x=0,float convergence=512);
    void* enqueue_dust_spans(void* command,std::uint32_t height,std::uint32_t scale,
        std::uint8_t tag,std::int16_t exclude_left=0,std::int16_t exclude_right=0,
        std::array<std::uint32_t,3> raster_mapping={},std::array<float,2> raster_jitter={});
    GpuProjection();
    ~GpuProjection();
    // 225 source-order int4 screen X/Y, depth, visible records. Retains
    // offscreen lattice geometry until each eye is projected. Borrowed output;
    // caller owns submission and must cancel its command on failure.
    void* enqueue_grid(void* device,void* command,const GridLattice& lattice,
        std::uint32_t width,std::uint32_t height,float eye_x=0,float convergence=512);
    // Convert the last grid projection to 225 height-sized row-span blocks.
    // Pass directly to GpuRaster::enqueue_row_spans; no readback/upload.
    // A line_start selects connected lines: 675 blocks (marker, connection,
    // companion per point), using the source-frame-owned starting endpoint.
    void* enqueue_grid_spans(void* command,std::uint32_t height,
        std::uint32_t scale,std::uint8_t colour,std::uint8_t tag,
        const std::int16_t* line_start=nullptr,std::array<std::uint32_t,3> raster_mapping={},std::array<float,2> raster_jitter={});
    // Existing SDL device/command, storage-readable NativeProjectionPoint
    // array. Output is borrowed int4: source screen X/Y, Z word, front flag.
    // No submission/readback/wait. Consume before next enqueue; caller must
    // cancel its command on failure. Release before destroying the device.
    void* enqueue(void* device,void* command,void* input,std::uint32_t count);
    // Q15 transform -> projection, entirely on the caller's command buffer.
    // Invalid pose indices become invisible points. Continuous/subpixel poses
    // are not represented by this source-word interface.
    void* enqueue_transformed(void* device,void* command,void* vertices,
        std::uint32_t count,void* poses,std::uint32_t pose_count,void** camera_points=nullptr);
    // Optional camera_points receives the borrowed native camera/vanish buffer
    // for near clipping, or null on failure. Same lifetime as projected output.
    // Fractional camera and screen coordinates. Camera.w/screen.w is -1 for
    // invalid poses, otherwise camera.w=1 and screen.w is the front flag.
    // Separate from word visibility: its signed-word rules must not be used.
    void* enqueue_continuous(void* device,void* command,void* vertices,
        std::uint32_t count,void* poses,std::uint32_t pose_count,void** residual_points=nullptr,bool lossless_camera=false);
    // Optional residual output: two float4 records per point, camera then
    // screen payload. Camera W=-1 is invalid; W=1 stores screen XY high
    // followed by XY low floats; W=2 stores raw binary64 X low/high then
    // Y low/high uint32 words in that float4. Do not float-validate format 2.
    // Camera XYZ remains residual floats in both formats.
    // lossless_camera enables format 3 for exact Euler cameras: camera
    // XYZ holds raw low words and screen XYZ raw high words of binary64 XYZ.
    // Consumed by the axis reducer and continuous polygon clipper.
    // Borrowed until next enqueue/release; no readback.
    // Reduce two source-ordered index ranges into ContinuousProjectedPoints.
    // Input points have the native camera/vanish or continuous camera/screen
    // 32-byte layout. Camera w=1 for valid groups, -1 for invalid/empty groups.
    // Screen projection is fractional, including for native camera input.
    // Each group is bounded to 65536 indices. Output borrowed until next call.
    // native_words instead returns two NativeProjectionPoint records after
    // rounded source-word means and wrapped midpoint near clipping. Requires
    // native input, no residuals, focal 256; feed into enqueue for projection.
    void* enqueue_axis_points(void* device,void* command,void* points,
        std::uint32_t point_count,void* indices,std::uint32_t index_count,
        const std::uint32_t ranges[4],bool fractional,float vanish_x=112,
        float vanish_y=96,float focal_length=256,void* input_residuals=nullptr,
        void** output_residuals=nullptr,bool native_words=false);
    // Chain after enqueue: uint4 face indices (a,b,c,flags) -> uint flags.
    // flags==1 produces an unconditional visible entry, for unculled faces.
    // Native flags==3 tests only point a's front flag (sprite centre gate).
    // Continuous only: flags==2 asserts a source-collinear triple sharing one
    // affine transform. It remains visible after validating projected indices.
    // Reads the last projected buffer directly. Invalid indices are invisible.
    void* enqueue_visibility(void* command,void* faces,std::uint32_t count);
    // Uses camera-space determinant with the source continuous tangent rule.
    void* enqueue_continuous_visibility(void* command,void* faces,std::uint32_t count);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    void* enqueue_motion_impl(void* device,void* command,void* current_points,
        void* previous_points,std::uint32_t count,float scale_x,float scale_y,
        bool reset_history,const MotionSurfaceSettings* surface);
    void* enqueue_point_spans(void* command,std::uint32_t height,std::uint32_t scale,
        std::uint8_t colour,std::uint8_t tag,const std::int16_t* start,unsigned kind,
        std::array<std::uint32_t,3> raster_mapping={},std::array<float,2> raster_jitter={});
    void* enqueue_visibility_impl(void* command,void* faces,std::uint32_t count,bool continuous);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
