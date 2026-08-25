#include "starfox/assets/shape_decoder.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace starfox::assets {
namespace {

constexpr std::uint8_t kEndShape = 0;
constexpr std::uint8_t kPoints8 = 4;
constexpr std::uint8_t kPoints16 = 8;
constexpr std::uint8_t kEndPoints = 12;
constexpr std::uint8_t kFaces = 20;
constexpr std::uint8_t kFrames = 28;
constexpr std::uint8_t kJump = 32;
constexpr std::uint8_t kBsp = 40;
constexpr std::uint8_t kVisibilities = 48;
constexpr std::uint8_t kPointsX16 = 52;
constexpr std::uint8_t kPointsX8 = 56;
constexpr std::uint8_t kBspInit = 60;
constexpr std::uint8_t kBspEnd = 64;
constexpr std::uint8_t kBspExit = 68;
constexpr std::uint8_t kQuit = 72;
constexpr std::uint8_t kSprite = 80;
constexpr std::uint8_t kSpriteVisibility = 84;
constexpr std::uint8_t kFaceEndQuit = 0xff;
constexpr std::uint8_t kFaceEndContinue = 0xfe;
constexpr std::uint8_t kColourTableBank = 0x03;

std::int8_t signed8(std::uint8_t value) {
    return static_cast<std::int8_t>(value);
}

bool is_point_opcode(std::uint8_t opcode) {
    return opcode == kPoints8 || opcode == kPoints16 || opcode == kPointsX8
        || opcode == kPointsX16 || opcode == kFrames;
}

bool is_frame_target_opcode(std::uint8_t opcode) {
    return is_point_opcode(opcode) || opcode == kJump || opcode == kEndPoints
        || opcode == kEndShape;
}

bool is_face_opcode(std::uint8_t opcode) {
    return opcode == kVisibilities || opcode == kFaces || opcode == kBspInit
        || opcode == kSprite || opcode == kSpriteVisibility || opcode == kEndShape;
}

} // namespace

ShapeDecoder::ShapeDecoder(const RomImage& rom) : rom_(rom) {}

ShapeDecoder::ShapeDecoder(const RomImage& rom, const SymbolMap& symbols)
    : rom_(rom) {
    const auto find_rom = [&symbols](const char* name) {
        for (const auto address : symbols.find(name)) {
            if ((address & 0xffffU) >= 0x8000U && ((address >> 16U) & 0xffU) < 0x7eU) {
                return address;
            }
        }
        return std::uint32_t{};
    };
    texture_address_table_ = find_rom("TEXTUREADDRTAB");
    texture_coordinate_table_ = find_rom("TEXTUREXYTAB");
    has_diffuse_shade_tables_ = true;
    for (std::size_t depth = 0; depth < diffuse_shade_tables_.size(); ++depth) {
        const auto name = std::string{"SHADESTAB2_"} + std::to_string(depth);
        const auto pointer_table = find_rom(name.c_str());
        if (pointer_table == 0U) {
            has_diffuse_shade_tables_ = false;
            break;
        }
        for (std::size_t light = 0;
             light < diffuse_shade_tables_[depth].size(); ++light) {
            const auto shade_pointer = rom_.read16(
                pointer_table + static_cast<std::uint32_t>(light * 2U));
            const auto shade_address = (pointer_table & 0xff0000U) | shade_pointer;
            for (std::size_t intensity = 0;
                 intensity < diffuse_shade_tables_[depth][light].size(); ++intensity) {
                diffuse_shade_tables_[depth][light][intensity] = rom_.read8(
                    shade_address + static_cast<std::uint32_t>(intensity));
            }
        }
    }
    for (const auto address : symbols.find("NULLSHAPE")) {
        // Objects store the zero-geometry Super FX sentinel, not the separate
        // ROM declaration of the diagnostic null header.
        if ((address >> 16U) == 0U && address >= 0x8000U) {
            null_shape_address_ = address;
            break;
        }
    }
}

ShapeHeader ShapeDecoder::decode_header(std::uint32_t address) const {
    ShapeHeader header;
    header.address = address;
    const auto points_pointer = rom_.read16(address);
    const auto data_bank = rom_.read8(address + 2U);
    const auto faces_pointer = rom_.read16(address + 3U);
    header.points_address = (static_cast<std::uint32_t>(data_bank) << 16U) | points_pointer;
    header.faces_address = (static_cast<std::uint32_t>(data_bank) << 16U)
        | faces_pointer;
    header.sort_z = rom_.read_i16(address + 5U);
    header.shift = rom_.read8(address + 7U);
    header.radius = rom_.read16(address + 8U);
    header.x_max = rom_.read_i16(address + 10U);
    header.y_max = rom_.read_i16(address + 12U);
    header.z_max = rom_.read_i16(address + 14U);
    header.size = rom_.read_i16(address + 16U);
    header.colour_pointer = rom_.read16(address + 18U);
    header.shadow_pointer = rom_.read16(address + 20U);
    header.lod1_pointer = rom_.read16(address + 22U);
    header.lod2_pointer = rom_.read16(address + 24U);
    header.lod3_pointer = rom_.read16(address + 26U);

    // Several source objects (MOTHER1, NULLPLAYER and invisible composite
    // components) deliberately use ShapeHdr 0,0,... as a strategy-only
    // controller. MOBJ treats the missing point/face streams as no geometry;
    // it is not a corrupt or unsupported model.
    if (points_pointer == 0U && faces_pointer == 0U) return header;

    if (!is_point_opcode(rom_.read8(header.points_address))
        || !is_face_opcode(rom_.read8(header.faces_address))) {
        throw std::runtime_error{"symbol does not reference a supported shape header"};
    }
    if (header.shift > 15U || header.colour_pointer < 0x8000U) {
        // ShapeHdr_s stores only points bank/faces. Compact LOD headers inherit
        // the remaining fields from their parent shape at runtime.
        header.compact = true;
        header.sort_z = 0;
        header.shift = 0;
        header.radius = 0;
        header.x_max = 0;
        header.y_max = 0;
        header.z_max = 0;
        header.size = 0;
        header.colour_pointer = 0x8213; // ID_0_C fallback for standalone previews
        const auto self = static_cast<std::uint16_t>(address & 0xffffU);
        header.shadow_pointer = self;
        header.lod1_pointer = self;
        header.lod2_pointer = self;
        header.lod3_pointer = self;
    }
    return header;
}

Shape ShapeDecoder::decode(
    std::uint32_t header_address,
    std::string name,
    std::uint16_t colour_pointer_override) const {
    Shape shape;
    shape.name = std::move(name);
    if (null_shape_address_ != 0U && header_address == null_shape_address_) {
        shape.header.address = header_address;
        return shape;
    }
    shape.header = decode_header(header_address);
    if (shape.header.points_address == 0U && shape.header.faces_address == 0U) {
        return shape;
    }
    if (colour_pointer_override != 0U) {
        shape.header.colour_pointer = colour_pointer_override;
    }
    decode_points(shape);
    decode_faces(shape);
    decode_colours(shape);
    return shape;
}

std::uint16_t ShapeDecoder::select_lod_pointer(
    const ShapeHeader& header, double camera_z) noexcept {
    if (camera_z >= 3000.0) return header.lod3_pointer;
    if (camera_z >= 2000.0) return header.lod2_pointer;
    if (camera_z >= 1000.0) return header.lod1_pointer;
    return static_cast<std::uint16_t>(header.address);
}

Shape ShapeDecoder::decode_lod(
    const ShapeHeader& parent,
    std::uint16_t header_pointer,
    std::uint16_t colour_pointer_override) const {
    const auto address = (parent.address & 0xff0000U) | header_pointer;
    if (address == parent.address) {
        return decode(address, {}, colour_pointer_override);
    }
    auto shape = decode(address);
    if (shape.header.compact) {
        // Compact headers contain only points, bank and faces.
        shape.header.sort_z = parent.sort_z;
        shape.header.radius = parent.radius;
        shape.header.x_max = parent.x_max;
        shape.header.y_max = parent.y_max;
        shape.header.z_max = parent.z_max;
        shape.header.size = parent.size;
    }
    // MDRAWLIS reads colour and shift from the base header before selecting
    // either a compact or a full points/faces header for the chosen LOD.
    shape.header.shift = parent.shift;
    shape.header.colour_pointer = colour_pointer_override != 0U
        ? colour_pointer_override : parent.colour_pointer;
    shape.colour_words.clear();
    shape.colour_materials.clear();
    shape.textures.clear();
    decode_colours(shape);
    return shape;
}

Shape ShapeDecoder::decode_by_name(const SymbolMap& symbols, const std::string& name) const {
    std::ostringstream failures;
    for (const auto candidate : symbols.find(name)) {
        try {
            return decode(candidate, name);
        } catch (const std::exception& error) {
            failures << " $" << std::hex << candidate << " (" << error.what() << ')';
        }
    }
    throw std::runtime_error{"no decodable shape header found for " + name + failures.str()};
}

bool ShapeDecoder::looks_like_shape_header(std::uint32_t address) const noexcept {
    try {
        static_cast<void>(decode_header(address));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void ShapeDecoder::decode_points(Shape& shape) const {
    const auto first_frame_counts = [this, &shape]() {
        auto cursor = shape.header.points_address;
        for (std::size_t command_count = 0; command_count < 4096; ++command_count) {
            const auto opcode = rom_.read8(cursor++);
            if (opcode == kEndPoints || opcode == kEndShape) {
                return std::pair{std::uint8_t{1}, std::uint8_t{1}};
            }
            if (opcode == kFrames) {
                const auto declared_count = rom_.read8(cursor++);
                if (declared_count == 0) {
                    throw std::runtime_error{"animated point table has zero frames"};
                }
                auto usable_count = std::uint8_t{0};
                for (std::uint16_t index = 0; index < declared_count; ++index) {
                    const auto entry_address = cursor + index * 2U;
                    const auto relative = rom_.read_i16(entry_address);
                    const auto target = static_cast<std::uint32_t>(
                        static_cast<std::int64_t>(entry_address) + 1 + relative);
                    if (!is_frame_target_opcode(rom_.read8(target))) {
                        break;
                    }
                    ++usable_count;
                }
                if (usable_count == 0) {
                    throw std::runtime_error{"animated point table has no valid frame targets"};
                }
                // A small number of original assets advertise a larger modulo
                // than the table they actually contain. PAPER_1 declares 32,
                // supplies 20 entries, and its strategy only selects 0..19.
                return std::pair{declared_count, usable_count};
            }
            if (opcode == kJump) {
                const auto relative_address = cursor;
                const auto relative = rom_.read_i16(relative_address);
                cursor = static_cast<std::uint32_t>(
                    static_cast<std::int64_t>(relative_address) + 1 + relative);
                continue;
            }
            if (opcode != kPoints8 && opcode != kPoints16 && opcode != kPointsX8
                && opcode != kPointsX16) {
                throw std::runtime_error{"unsupported point opcode " + std::to_string(opcode)};
            }
            const auto count = rom_.read8(cursor++);
            const auto bytes_per_point = opcode == kPoints16 || opcode == kPointsX16 ? 6U : 3U;
            cursor += static_cast<std::uint32_t>(count) * bytes_per_point;
        }
        throw std::runtime_error{"point command stream did not terminate"};
    }();
    shape.declared_frame_count = first_frame_counts.first;

    const auto decode_frame = [this, &shape](std::uint32_t selected_frame) {
        auto cursor = shape.header.points_address;
        ShapeFrame frame;
        for (std::size_t command_count = 0; command_count < 16'384; ++command_count) {
            const auto opcode = rom_.read8(cursor++);
            if (opcode == kEndPoints || opcode == kEndShape) {
                return frame;
            }
            if (opcode == kFrames) {
                const auto count = rom_.read8(cursor++);
                if (count == 0) {
                    throw std::runtime_error{"animated point table has zero frames"};
                }
                const auto entry_address = cursor + (selected_frame % count) * 2U;
                const auto relative = rom_.read_i16(entry_address);
                cursor = static_cast<std::uint32_t>(
                    static_cast<std::int64_t>(entry_address) + 1 + relative);
                continue;
            }
            if (opcode == kJump) {
                const auto relative_address = cursor;
                const auto relative = rom_.read_i16(relative_address);
                cursor = static_cast<std::uint32_t>(
                    static_cast<std::int64_t>(relative_address) + 1 + relative);
                continue;
            }
            if (opcode != kPoints8 && opcode != kPoints16 && opcode != kPointsX8
                && opcode != kPointsX16) {
                throw std::runtime_error{"unsupported point opcode " + std::to_string(opcode)};
            }

            const auto count = rom_.read8(cursor++);
            PointBlock block;
            const auto word = opcode == kPoints16 || opcode == kPointsX16;
            const auto mirrored = opcode == kPointsX8 || opcode == kPointsX16;
            block.encoding = opcode == kPoints8 ? PointEncoding::signed8
                : opcode == kPoints16 ? PointEncoding::signed16
                : opcode == kPointsX8 ? PointEncoding::mirrored_x_signed8
                                      : PointEncoding::mirrored_x_signed16;
            for (std::uint16_t index = 0; index < count; ++index) {
                Vec3i point;
                if (word) {
                    point.x = rom_.read_i16(cursor);
                    point.y = rom_.read_i16(cursor + 2U);
                    point.z = rom_.read_i16(cursor + 4U);
                    cursor += 6U;
                } else {
                    point.x = signed8(rom_.read8(cursor));
                    point.y = signed8(rom_.read8(cursor + 1U));
                    point.z = signed8(rom_.read8(cursor + 2U));
                    cursor += 3U;
                }
                block.source_points.push_back(point);
                frame.vertices.push_back(point);
                if (mirrored) {
                    frame.vertices.push_back(Vec3i{-point.x, point.y, point.z});
                }
            }
            frame.point_blocks.push_back(std::move(block));
        }
        throw std::runtime_error{"point command stream did not terminate"};
    };

    shape.frames.reserve(first_frame_counts.second);
    for (std::uint32_t frame = 0; frame < first_frame_counts.second; ++frame) {
        shape.frames.push_back(decode_frame(frame));
    }
    shape.point_blocks = shape.frames.front().point_blocks;
    shape.vertices = shape.frames.front().vertices;
}

void ShapeDecoder::decode_faces(Shape& shape) const {
    auto cursor = shape.header.faces_address;
    bool in_faces = false;

    const auto decode_face = [this, &shape](std::uint32_t& face_cursor, std::uint8_t vertex_count) {
        Face face;
        face.visibility_index = signed8(rom_.read8(face_cursor++));
        face.colour_id = rom_.read8(face_cursor++);
        face.normal.x = signed8(rom_.read8(face_cursor++));
        face.normal.y = signed8(rom_.read8(face_cursor++));
        face.normal.z = signed8(rom_.read8(face_cursor++));
        face.vertex_indices.reserve(vertex_count);
        for (std::uint8_t index = 0; index < vertex_count; ++index) {
            const auto vertex_index = rom_.read8(face_cursor++);
            face.vertex_indices.push_back(vertex_index);
        }
        return face;
    };

    std::unordered_set<std::uint32_t> decoded_face_lists;
    const auto decode_face_list = [this, &shape, &decode_face, &decoded_face_lists](
                                      std::uint32_t address) {
        if (!decoded_face_lists.insert(address).second) {
            return;
        }
        auto list_cursor = address;
        if (rom_.read8(list_cursor++) != kFaces) {
            throw std::runtime_error{"BSP face pointer does not reference a Faces command"};
        }
        FaceBatch batch;
        batch.address = address;
        for (std::size_t face_count = 0; face_count < 4096; ++face_count) {
            const auto opcode = rom_.read8(list_cursor++);
            if (opcode == kFaceEndQuit || opcode == kFaceEndContinue) {
                for (const auto& face : batch.faces) {
                    shape.faces.push_back(face);
                }
                shape.face_batches.push_back(std::move(batch));
                return;
            }
            if (opcode < 2U || opcode > 12U) {
                throw std::runtime_error{"invalid face record in BSP face list"};
            }
            batch.faces.push_back(decode_face(list_cursor, opcode));
        }
        throw std::runtime_error{"BSP face list did not terminate"};
    };

    const auto decode_bsp = [this, &shape, &decode_face_list](std::uint32_t root) {
        shape.bsp_root_address = root;
        std::unordered_set<std::uint32_t> visited;
        std::function<void(std::uint32_t)> visit = [&](std::uint32_t address) {
            if (!visited.insert(address).second) {
                return;
            }
            const auto opcode = rom_.read8(address);
            if (opcode == kBsp) {
                const auto visibility = rom_.read8(address + 1U);
                const auto face_relative_address = address + 2U;
                const auto face_relative = rom_.read_i16(face_relative_address);
                const auto face_address = static_cast<std::uint32_t>(
                    static_cast<std::int64_t>(face_relative_address) + 1 + face_relative);
                const auto alternate_offset_address = address + 4U;
                const auto alternate_offset = signed8(rom_.read8(alternate_offset_address));
                const auto fallthrough = address + 5U;
                const auto alternate = alternate_offset == 0
                    ? 0U
                    : static_cast<std::uint32_t>(
                          static_cast<std::int64_t>(alternate_offset_address) + alternate_offset);
                shape.bsp_nodes.push_back(
                    {address, visibility, face_address, fallthrough, alternate});
                decode_face_list(face_address);
                visit(fallthrough);
                if (alternate != 0) {
                    visit(alternate);
                }
                return;
            }
            if (opcode == kBspExit) {
                const auto relative_address = address + 1U;
                const auto relative = rom_.read_i16(relative_address);
                const auto face_address = static_cast<std::uint32_t>(
                    static_cast<std::int64_t>(relative_address) + 1 + relative);
                shape.bsp_leaves.push_back({address, face_address});
                decode_face_list(face_address);
                return;
            }
            if (opcode == kBspEnd || opcode == kEndShape || opcode == kQuit) {
                return;
            }
            throw std::runtime_error{"invalid BSP control opcode " + std::to_string(opcode)};
        };
        visit(root);
    };

    for (std::size_t command_count = 0; command_count < 65'536; ++command_count) {
        const auto opcode = rom_.read8(cursor++);
        if (in_faces && opcode >= 2U && opcode <= 12U) {
            shape.faces.push_back(decode_face(cursor, opcode));
            continue;
        }
        if (opcode == kEndShape || opcode == kQuit || opcode == kFaceEndQuit) {
            return;
        }
        if (opcode == kFaceEndContinue) {
            in_faces = false;
            continue;
        }
        if (opcode == kVisibilities) {
            const auto count = rom_.read8(cursor++);
            shape.visibilities.reserve(shape.visibilities.size() + count);
            for (std::uint16_t index = 0; index < count; ++index) {
                shape.visibilities.push_back({
                    rom_.read8(cursor), rom_.read8(cursor + 1U), rom_.read8(cursor + 2U)});
                cursor += 3U;
            }
            continue;
        }
        if (opcode == kFaces) {
            in_faces = true;
            continue;
        }
        if (opcode == kBspInit) {
            decode_bsp(cursor);
            return;
        }
        if (opcode == kBspEnd) {
            continue;
        }
        if (opcode == kBsp || opcode == kBspExit) {
            throw std::runtime_error{"BSP face streams are not decoded yet"};
        }
        if (opcode == kSprite) {
            Face face;
            face.visibility_index = -1;
            face.sprite = true;
            face.vertex_indices.push_back(rom_.read8(cursor++));
            face.colour_id = rom_.read8(cursor++);
            face.sprite_size = rom_.read8(cursor++);
            shape.faces.push_back(std::move(face));
            continue;
        }
        if (opcode == kSpriteVisibility) {
            Face face;
            face.visibility_index = signed8(rom_.read8(cursor++));
            face.sprite_visibility_parameter =
                static_cast<std::uint8_t>(face.visibility_index);
            face.sprite = true;
            face.vertex_indices.push_back(rom_.read8(cursor++));
            face.colour_id = rom_.read8(cursor++);
            face.sprite_size = rom_.read8(cursor++);
            shape.faces.push_back(std::move(face));
            continue;
        }
        throw std::runtime_error{"unsupported face opcode " + std::to_string(opcode)};
    }
    throw std::runtime_error{"face command stream did not terminate"};
}

void ShapeDecoder::decode_colours(Shape& shape) const {
    shape.diffuse_shade_tables = diffuse_shade_tables_;
    shape.has_diffuse_shade_tables = has_diffuse_shade_tables_;
    if (shape.faces.empty()) {
        return;
    }
    const auto maximum = std::max_element(shape.faces.begin(), shape.faces.end(), [](const Face& a, const Face& b) {
        return a.colour_id < b.colour_id;
    })->colour_id;
    shape.colour_words.reserve(static_cast<std::size_t>(maximum) + 1U);
    const auto base = (static_cast<std::uint32_t>(kColourTableBank) << 16U)
        | shape.header.colour_pointer;
    for (std::uint16_t index = 0; index <= maximum; ++index) {
        const auto word = rom_.read16(base + index * 2U);
        shape.colour_words.push_back(word);
        ColourMaterial material;
        material.raw = word;
        if ((word & 0xc000U) == 0x8000U) {
            const auto animation = (static_cast<std::uint32_t>(kColourTableBank) << 16U)
                | 0x8000U | (word & 0x3fffU);
            const auto frame_count = rom_.read8(animation);
            if (frame_count == 0U || frame_count > 128U) {
                throw std::runtime_error{"invalid colour animation frame count"};
            }
            material.animation_frames.reserve(frame_count);
            for (std::uint16_t frame = 0; frame < frame_count; ++frame) {
                material.animation_frames.push_back(
                    rom_.read16(animation + 1U + frame * 2U));
            }
        }
        shape.colour_materials.push_back(std::move(material));
        decode_texture(shape, word);
        for (const auto frame : shape.colour_materials.back().animation_frames) {
            decode_texture(shape, frame);
        }
    }
}

void ShapeDecoder::decode_texture(Shape& shape, std::uint16_t descriptor) const {
    if ((descriptor & 0xc000U) != 0x4000U || texture_address_table_ == 0U
        || texture_coordinate_table_ == 0U) {
        return;
    }
    if (std::any_of(shape.textures.begin(), shape.textures.end(), [descriptor](const auto& texture) {
            return texture.descriptor == descriptor;
        })) {
        return;
    }

    const auto texture_index = static_cast<std::uint8_t>(descriptor);
    const auto coordinate_index = static_cast<std::uint8_t>((descriptor >> 8U) & 0x1fU);
    const auto texture_count = static_cast<std::uint32_t>(
        (texture_coordinate_table_ - texture_address_table_) / 3U);
    // Some shapes intentionally index past their static colour table and
    // provide an object-specific table at runtime. Bytes following the static
    // table can coincidentally have bit 14 set; only descriptors valid against
    // both canonical lookup tables are textures.
    if (coordinate_index >= 9U || texture_index >= texture_count) {
        return;
    }
    const auto address_entry = texture_address_table_
        + static_cast<std::uint32_t>(texture_index) * 3U;
    TextureImage texture;
    texture.descriptor = descriptor;
    texture.address = static_cast<std::uint32_t>(rom_.read16(address_entry))
        | (static_cast<std::uint32_t>(rom_.read8(address_entry + 2U)) << 16U);
    const auto coordinate_pointer = rom_.read16(
        texture_coordinate_table_ + static_cast<std::uint32_t>(coordinate_index) * 2U);
    const auto coordinate_address = (texture_coordinate_table_ & 0xff0000U)
        | coordinate_pointer;
    texture.u_mask = rom_.read8(coordinate_address);
    texture.v_mask = rom_.read8(coordinate_address + 1U);
    if (texture.u_mask > 127U || texture.v_mask > 127U) {
        throw std::runtime_error{"invalid texture-coordinate mask"};
    }
    for (std::size_t vertex = 0; vertex < texture.coordinates.size(); ++vertex) {
        texture.coordinates[vertex] = {
            rom_.read8(coordinate_address + 2U + vertex * 2U),
            rom_.read8(coordinate_address + 3U + vertex * 2U),
        };
    }
    const auto width = static_cast<std::size_t>(texture.u_mask) + 1U;
    const auto height = static_cast<std::size_t>(texture.v_mask) + 1U;
    texture.texels.reserve(width * height);
    const auto high_nibble = (descriptor & 0x2000U) != 0U;
    const auto texture_file_offset = static_cast<std::size_t>(
        ((texture.address >> 16U) & 0x7fU) * 0x8000U
        + (texture.address & 0x7fffU));
    for (std::size_t v = 0; v < height; ++v) {
        for (std::size_t u = 0; u < width; ++u) {
            const auto file_offset = texture_file_offset + v * 256U + u;
            if (file_offset >= rom_.bytes().size()) {
                throw std::runtime_error{"texture pixels exceed the ROM image"};
            }
            const auto packed = rom_.bytes()[file_offset];
            texture.texels.push_back(high_nibble
                ? static_cast<std::uint8_t>(packed >> 4U)
                : static_cast<std::uint8_t>(packed & 0x0fU));
        }
    }
    shape.textures.push_back(std::move(texture));
}

} // namespace starfox::assets
