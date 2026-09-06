#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <vector>

namespace starfox::assets {

struct Vec3i {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};

    friend bool operator==(const Vec3i&, const Vec3i&) = default;
};

enum class PointEncoding : std::uint8_t {
    signed8,
    signed16,
    mirrored_x_signed8,
    mirrored_x_signed16,
};

struct PointBlock {
    PointEncoding encoding{};
    std::vector<Vec3i> source_points;
};

struct ShapeFrame {
    std::vector<PointBlock> point_blocks;
    std::vector<Vec3i> vertices;
};

struct Visibility {
    std::uint8_t a{};
    std::uint8_t b{};
    std::uint8_t c{};
};

struct Face {
    std::int8_t visibility_index{};
    std::uint8_t colour_id{};
    Vec3i normal{};
    std::vector<std::uint8_t> vertex_indices;
    bool sprite{};
    std::uint8_t sprite_visibility_parameter{};
    std::uint8_t sprite_size{};

    [[nodiscard]] bool is_line() const noexcept {
        return vertex_indices.size() == 2;
    }
};

struct FaceBatch {
    std::uint32_t address{};
    std::vector<Face> faces;
};

struct BspNode {
    std::uint32_t address{};
    std::uint8_t visibility_index{};
    std::uint32_t face_batch_address{};
    std::uint32_t fallthrough_address{};
    std::uint32_t alternate_address{};
};

struct BspLeaf {
    std::uint32_t address{};
    std::uint32_t face_batch_address{};
};

struct ColourMaterial {
    std::uint16_t raw{};
    // COLANIM points at a count byte followed by this many material words.
    // Keeping the original words preserves normal, diffuse and texture
    // frames without baking a renderer-specific interpretation into assets.
    std::vector<std::uint16_t> animation_frames;
};

struct TextureCoordinate {
    std::uint8_t u{};
    std::uint8_t v{};
};

struct TextureImage {
    std::uint16_t descriptor{};
    std::uint32_t address{};
    std::uint8_t u_mask{};
    std::uint8_t v_mask{};
    std::array<TextureCoordinate, 4> coordinates{};
    std::vector<std::uint8_t> texels;
};

using DiffuseShadeTables = std::array<
    std::array<std::array<std::uint8_t, 10>, 12>, 4>;

struct ShapeHeader {
    std::uint32_t address{};
    bool compact{};
    std::uint32_t points_address{};
    std::uint32_t faces_address{};
    std::int16_t sort_z{};
    std::uint8_t shift{};
    std::uint16_t radius{};
    std::int16_t x_max{};
    std::int16_t y_max{};
    std::int16_t z_max{};
    std::int16_t size{};
    std::uint16_t colour_pointer{};
    std::uint16_t shadow_pointer{};
    std::uint16_t lod1_pointer{};
    std::uint16_t lod2_pointer{};
    std::uint16_t lod3_pointer{};
};

struct Shape {
    std::string name;
    ShapeHeader header;
    std::uint8_t declared_frame_count{1};
    std::vector<PointBlock> point_blocks;
    std::vector<Vec3i> vertices;
    std::vector<ShapeFrame> frames;
    std::vector<Visibility> visibilities;
    std::vector<Face> faces;
    std::vector<FaceBatch> face_batches;
    std::vector<BspNode> bsp_nodes;
    std::vector<BspLeaf> bsp_leaves;
    std::uint32_t bsp_root_address{};
    std::vector<std::uint16_t> colour_words;
    std::vector<ColourMaterial> colour_materials;
    std::vector<TextureImage> textures;
    DiffuseShadeTables diffuse_shade_tables{};
    bool has_diffuse_shade_tables{};
};

} // namespace starfox::assets
