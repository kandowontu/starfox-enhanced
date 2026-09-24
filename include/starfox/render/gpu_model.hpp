#pragma once
#include "starfox/render/gpu_raster.hpp"
#include "starfox/render/ray_materials.hpp"
#include <array>
#include <vector>
namespace starfox::render {
// Optional resident camera geometry for a ray-tracing consumer. The only CPU
// data is immutable topology (point indices + face ID), never transformed XYZ.
// Empty sources mean the model needs its existing specialized fallback.
// GPU buffers are borrowed until the next enqueue/release on their GpuModel;
// consume them on the owning command before reusing that model object.
struct GpuModelRaySource {
    void* points{};void* residuals{};
    std::uint32_t point_count{},mode{};
    std::vector<std::array<std::uint32_t,4>> triangles;
    bool request_materials{};
    bool reference_materials{}; // Diagnostic CPU packing; never needed for GPU transport.
    bool materials_complete{};
    bool reflection_excluded{}; // Axis-line raster has no polygon reflection surface; retains shadow casters.
    RayMaterials materials;
    void *material_corners{},*material_polygons{},*material_commands{};
    std::uint32_t material_corner_count{},material_count{};
    void* material_lookup{};
    std::uint32_t material_lookup_count{};
    std::vector<std::array<std::uint32_t,4>> material_topology;
};
// Optional diagnostic handles only; enqueue never submits or reads them back.
// Borrowed through the next enqueue/release, with the same command lifetime.
struct GpuModelDiagnostics {
    void* projected_points{};
    void* clipped_polygons{};
    std::uint32_t point_count{},polygon_count{};
    bool continuous{};
};
// Model geometry pipeline. Supports ordinary polygons, lines and sprite faces;
// unsupported primitives/effects explicitly fail instead of dropping faces.
    // Indexed output starts cleared unless a background is supplied.
    // Packed bit 26 marks coverage, including black writes, for GpuScene merging.
class GpuModel {
public:
    GpuModel();~GpuModel();
    // Caller-owned SDL device and command. Packs/uploads model data, then chains
    // transform, visibility, BSP, clipping, spans and raster without readback.
    // Output borrowed until next call. Cancel command on failure; release before
    // device destruction. Optional normals/depth are generated on-device.
    // GpuScene::enqueue_batch composes multiple ordered model/legacy draws.
    // This primitive still encodes one model's geometry at a time.
    // An optional non-aliased background is composited in the raster dispatch.
    // geometry_depth requests separate per-pixel camera Z for planar polygons.
    // Folded faces and screen-space/sprite effects remain unknown (zero or a
    // null depth buffer); effects' historical mean depth is never substituted.
    // previous_pose opts into rigid per-pixel motion and depth. It must belong
    // to the same entity/topology and preceding submitted frame. Unsupported
    // correspondence remains null (never valid zero motion). Motion requires
    // an unfused draw: merge its output through GpuScene afterwards.
    // raster_jitter is a current-frame displacement in output raster pixels;
    // both supplied poses remain unjittered. Requires continuous subpixel
    // projection. Motion removes this displacement, depth follows the raster.
    // Optional raster_size decouples logical viewport/projection from output
    // dimensions. Continuous subpixel geometry is required; whole-object
    // billboards and wave distortion currently require the normal path.
    GpuRasterOutput enqueue(void* device,void* command,const assets::Shape&,const RenderPose&,
        const RenderSettings&,std::uint32_t width,std::uint32_t height,bool surface_metadata=false,
        const GpuRasterOutput* background=nullptr,GpuModelDiagnostics* diagnostics=nullptr,
        bool geometry_depth=false,GpuModelRaySource* ray_source=nullptr,
        const RenderPose* previous_pose=nullptr,std::array<float,2> raster_jitter={},
        std::array<std::uint32_t,2> raster_size={});
    void release_device() noexcept;
    const std::string& status()const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
