#pragma once
#include "starfox/render/shadow_mask.hpp"
#include "starfox/render/ray_materials.hpp"
#include <memory>
#include <string>
#include <array>
#include <span>

namespace starfox::render { struct GpuBackgroundDraw; }
namespace starfox::render::shadows {
// World-anchored analytic water receiver. Palette classification is applied
// by the compositor, so UI and non-water ground cannot become reflective.
struct RayWater {
    float time{};
    float reflection_strength{};
    unsigned material{}; // 0 water, 1 mirror ground, 2 gold ground.
    std::array<float,9> world_to_view{1,0,0,0,1,0,0,0,1};
    std::array<float,3> camera_position{};
};
// Optional Windows DXR 1.1 backend. Unsupported devices/platforms return false;
// callers retain the CPU implementation. Unchanged geometry may reuse its BLAS.
class DxrShadows {
public:
    struct ResidentOutput {
        void* device{};void* resource{};
        std::uint32_t width{},height{},row_bytes{};
        std::array<std::uint8_t,8> adapter_luid{};
        std::uint64_t ready_value{};
        std::uint32_t bytes_per_pixel{1}; // Shadow=1, reflected RGBA=4.
    };
    struct ResidentGeometry {
        void* resource{}; // ID3D12Resource on this producer's device, COMMON state.
        std::uint32_t vertex_count{},stride{16}; // Triangle list, float3 positions.
        std::array<std::uint8_t,8> adapter_luid{};
        std::uint64_t ready_value{}; // Initial shared-fence value, before first trace.
    };
    struct TriangleCoverage {
        std::array<float,6> uv{};
        std::uint32_t offset{},u_mask{},v_mask{},flags{}; // flags: 0 opaque, 1 alpha texture
    };
    struct Coverage {
        std::span<const TriangleCoverage> triangles;
        std::span<const std::uint32_t> texels; // RGBA8, alpha in high byte; zero alpha is a hole.
    };
    struct ReflectionInput {
        const render::RayMaterials* materials{};
        std::span<const std::uint32_t> palette; // Exactly 256 RGBA entries.
        std::uint32_t environment{};
        float roughness{}; // 0: sharp mirror; bounded deterministic ray cone otherwise.
        std::uint32_t metallic{}; // 0 dielectric, 1 palette conductor, 2 gold, 3 copper.
        // Optional camera-space cube, faces +X,-X,+Y,-Y,+Z,-Z. Each face is
        // face_size squared packed RGBA pixels. Scene hits take precedence.
        std::span<const std::uint32_t> environment_cube;
        std::uint32_t face_size{};
        // Camera ray -> cube-space direction, row-major. Identity preserves
        // legacy camera-space cubes; world-aligned cubes use inverse view.
        std::array<float,9> environment_rotation{1,0,0,0,1,0,0,0,1};
        // Borrowed authored BG2 tile source; uploaded as tiles, never a HUD screenshot.
        const render::GpuBackgroundDraw* background{};
        // Camera-space physical receiver, only for scenes with real ground.
        std::optional<ReceiverPlane> ground;
        float background_eye_x{}; // Eye-space hit -> central authored camera.
        // Optional ID3D12Resource on this producer's device, COMMON state.
        // Exactly one 64-byte RayMaterial per triangle; caller synchronizes GPU
        // writes before dispatch. CPU materials then supply texels only.
        void* resident_materials{};
        std::uint32_t resident_material_offset{}; // 16-byte aligned, may follow geometry.
        const RayWater* water{};
    };
    // When supplied, only this Windows adapter may produce shared output.
    // Never falls back to a different GPU if the requested adapter lacks DXR.
    explicit DxrShadows(std::optional<std::array<std::uint8_t,8>> adapter_luid=std::nullopt);
    ~DxrShadows();
    DxrShadows(const DxrShadows&) = delete;
    DxrShadows& operator=(const DxrShadows&) = delete;
    [[nodiscard]] bool available();
    bool render(const Scene&, Camera, Vec3 light, std::optional<ReceiverPlane>,
        std::vector<std::uint8_t>&);
    // Diagnostic first-bounce reflected RGBA. Materials follow Scene::triangles
    // order; do not build/reorder the Scene after preparing them. Transparent
    // output means no primary surface; misses reflect the supplied environment.
    bool render_reflections(const Scene&,Camera,const render::RayMaterials&,
        std::span<const std::uint32_t,256> palette,std::uint32_t environment,
        std::vector<std::uint8_t>& rgba);
    // Native D3D12 buffer, not an SDL buffer. Normally completed GPU work, UAV state
    // (COMMON when release_for_external is selected);
    // borrowed until the next render/destruction. No mask readback or copy.
    // Optional geometry replaces Scene input without a CPU vertex upload.
    // Caller finishes external writes/releases ownership first. Resource is
    // borrowed until completion and returned to COMMON by the trace commands.
    // release_for_external folds the UAV->COMMON release into the trace
    // submission; exporting its fence then requires no second GPU submission.
    // defer_completion requires release_for_external. The consumer must wait
    // on exported ready_value before reading; producer reuse waits internally.
    bool render_resident(const Scene&,Camera,Vec3,std::optional<ReceiverPlane>,const ResidentGeometry* geometry=nullptr,const Coverage* coverage=nullptr,bool release_for_external=false,bool defer_completion=false,const ReflectionInput* reflection=nullptr);
    // Explicit diagnostic/fallback download; presentation need not call this.
    bool readback_resident(std::vector<std::uint8_t>&);
    // Diagnostic image transfer count; camera/palette metadata is separate.
    [[nodiscard]] std::size_t last_backdrop_upload_bytes() const noexcept;
    [[nodiscard]] ResidentOutput resident_output() const noexcept {return resident_;}
    // Windows NT handle for current output. Caller must CloseHandle and, for
    // deferred completion, wait on the exported ready fence before consuming.
    // Consume before the next render; importing does not provide frame ownership.
    // Null on unsupported platforms or when no resident output is valid.
    [[nodiscard]] void* export_resident_handle();
    // Shared UAV triangle storage, fixed float4 stride. No vertex upload.
    // Complete all external use before resizing/reusing or destroying it.
    [[nodiscard]] ResidentGeometry prepare_shared_geometry(std::uint32_t vertex_count);
    // Independent incoming timeline: consumer submits its signal BEFORE asking
    // DXR to wait. This never advances/waits the outgoing producer serial.
    // Exported handle is owned by caller. No CPU geometry download or wait.
    void* export_geometry_completion_handle();
    bool wait_for_geometry(std::uint64_t value);
    [[nodiscard]] void* export_geometry_handle();
    // Releases valid output to COMMON and returns a caller-owned shared fence handle.
    // Also works after prepare_shared_geometry, before any output exists; use
    // the returned geometry's initial ready_value for that first submission.
    // Read ready_value AFTER this call. Consumer must finish and release external
    // ownership before any subsequent render/readback/destruction on this object.
    [[nodiscard]] void* export_ready_fence_handle();
    [[nodiscard]] const std::string& status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ResidentOutput resident_{};
};
}
