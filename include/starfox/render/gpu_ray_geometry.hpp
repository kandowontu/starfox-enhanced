#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
namespace starfox::render {
struct GpuRayGeometrySettings {
    std::uint32_t triangles{},points{},mode{},reserved{};
    // mode: 0 native integer camera records, 1 float camera records,
    // 2 float camera records plus compensated/binary64 residuals.
    std::array<float,4> row0{1,0,0,0},row1{0,1,0,0},row2{0,0,1,0};
};
static_assert(sizeof(GpuRayGeometrySettings)==64);
struct GpuRayGeometryTarget {
    void* buffer{};
    std::uint32_t vertex_capacity{},first_vertex{};
    bool cycle{};
};
struct GpuRayMaterialTarget {
    void* buffer{};
    std::uint32_t byte_capacity{},byte_offset{};
    bool cycle{};
};
struct GpuRayMaterialLookup {void* buffer{};std::uint32_t face_count{};};
class GpuRayGeometry {
public:
    GpuRayGeometry();~GpuRayGeometry();
    // Camera records are 32 bytes. Triangle uint4 records contain three point
    // indices and a retained face ID. Include offscreen/back-facing triangles.
    // Output: 3 float4 positions per triangle; invalid triangles become zero.
    // Caller supplies storage-readable buffers sized for settings.points and
    // settings.triangles (residuals also points records in mode 2), submits/cancels the command,
    // and consumes the borrowed output before the next enqueue/release.
    // This does not submit, read back, or build a ray-tracing acceleration structure.
    // Optional caller-owned target writes directly into a scene range; its
    // storage-read/write buffer must hold vertex_capacity float4 records.
    void* enqueue(void* device,void* command,void* points,void* residuals,
        void* triangles,const GpuRayGeometrySettings&,const GpuRayGeometryTarget* target=nullptr);
    // Pack already-resolved occurrence materials. Topology holds three corner
    // indices and a material slot; polygons/corners/96-byte commands retain the
    // producer's slot ordering. Output is triangle_count 64-byte RayMaterials.
    // Topology face bit 31 instead selects a source face: XYZ become local
    // corner ordinals, and corner.w must carry source-face+1. The last emitted
    // occurrence wins; a face without an emitted occurrence remains invalid.
    // Reserved=1 marks an invalid record, never an opaque black substitute.
    // Optional target writes directly into a 16-byte-aligned scene subrange.
    // The caller owns its storage-read/write buffer and actual capacity.
    // reject_all emits invalid records without reading inputs (non-null dummy
    // bindings still required); used for known non-polygon reflection surfaces.
    void* enqueue_materials(void* device,void* command,void* topology,void* corners,
        void* polygons,void* materials,std::uint32_t triangle_count,std::uint32_t corner_count,
        std::uint32_t material_count,std::uint32_t texel_count,std::uint32_t texel_base=0,
        const GpuRayMaterialTarget* target=nullptr,bool reject_all=false,
        const GpuRayMaterialLookup* lookup=nullptr);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
