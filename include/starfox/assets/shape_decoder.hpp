#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape.hpp"

#include <cstdint>
#include <string>

namespace starfox::assets {

class ShapeDecoder {
public:
    explicit ShapeDecoder(const RomImage& rom);
    ShapeDecoder(const RomImage& rom, const SymbolMap& symbols);

    [[nodiscard]] Shape decode(
        std::uint32_t header_address,
        std::string name = {},
        std::uint16_t colour_pointer_override = 0) const;
    [[nodiscard]] Shape decode_lod(
        const ShapeHeader& parent,
        std::uint16_t header_pointer,
        std::uint16_t colour_pointer_override = 0) const;
    [[nodiscard]] static std::uint16_t select_lod_pointer(
        const ShapeHeader& header, double camera_z) noexcept;
    [[nodiscard]] Shape decode_by_name(const SymbolMap& symbols, const std::string& name) const;
    [[nodiscard]] bool looks_like_shape_header(std::uint32_t address) const noexcept;

private:
    [[nodiscard]] ShapeHeader decode_header(std::uint32_t address) const;
    void decode_points(Shape& shape) const;
    void decode_faces(Shape& shape) const;
    void decode_colours(Shape& shape) const;
    void decode_texture(Shape& shape, std::uint16_t descriptor) const;

    const RomImage& rom_;
    std::uint32_t null_shape_address_{};
    std::uint32_t texture_address_table_{};
    std::uint32_t texture_coordinate_table_{};
    DiffuseShadeTables diffuse_shade_tables_{};
    bool has_diffuse_shade_tables_{};
};

} // namespace starfox::assets
