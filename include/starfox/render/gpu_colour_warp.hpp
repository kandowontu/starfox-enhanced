#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
namespace starfox::render {
struct GpuWarpSettings {
    // Seed low 16 bits are PRNG state; bit 31 consumes every face during
    // destruction, independently of the later sprite visibility test.
    std::uint32_t capacity{},face_count{},visibility_count{},seed{};
    std::uint32_t corner_count{},texture_count{},coordinate_count{},colour_base{};
    // flags: palette override 1, forced colour 2, diffuse 4, depth tables 8,
    // axis line 16 (uses material colours but never texture sampling).
    std::uint32_t depth_band{},flags{},override_colour{},forced_colour{};
    std::array<std::int32_t,4> light{};
    std::array<std::uint32_t,4> shade_counts{};
    std::int32_t scroll_x{},scroll_y{};
    bool reflection_materials{}; // Canonical hidden faces followed by untouched actual occurrences; never rasterize this output.
};
struct GpuWarpInputs {
    void *order{},*traversal{},*polygons{},*corners{},*visibility{},*materials{};
    void *normals{},*diffuse{},*depth_colours{},*texture_lookup{},*textures{},*coordinates{};
};
struct GpuWarpOutput {void *polygons{},*corners{},*materials{},*result{},*face_lookup{};};
// Borrowed SDL buffers. Inputs must cover settings ranges, with order sized to
// capacity, diffuse padded to 4*62*10 bytes, depth colours 128 bytes and lookup
// 65536 uints. Texture/coordinate buffers must exist even when empty (dummy).
// Enqueues only: caller owns submission/cancellation and must consume outputs
// before reuse. Release before destroying the device. No CPU count readback.
class GpuColourWarp {
public:
    GpuColourWarp();~GpuColourWarp();
    GpuWarpOutput enqueue(void* device,void* command,const GpuWarpInputs&,const GpuWarpSettings&);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
