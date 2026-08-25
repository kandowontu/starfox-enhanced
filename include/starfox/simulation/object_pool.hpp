#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace starfox::simulation {

using ObjectHandle = std::uint16_t;
inline constexpr std::size_t kMaximumObjects = 70;
inline constexpr std::size_t kExtendedObjectBytes = 54;

// Semantic form of the original al_/alx_ blocks. Narrow fields intentionally
// retain the 65816 wrapping behavior expected by strategy code.
struct GameObject {
    std::uint16_t shape{};
    ObjectHandle attached{};
    std::uint8_t flags{};
    std::uint8_t type{};
    std::uint8_t count{};
    std::uint8_t count1{};
    std::int16_t world_x{};
    std::int16_t world_y{};
    std::int16_t world_z{};
    std::uint8_t rotation_x{};
    std::uint8_t rotation_y{};
    std::uint8_t rotation_z{};
    std::int8_t velocity{};
    std::uint32_t strategy_address{};
    ObjectHandle immune_object{};
    ObjectHandle collision_object{};
    std::array<std::uint8_t, 4> strategy_flags{};
    std::int8_t skid_y{};
    std::array<std::int8_t, 6> scratch_bytes{};
    std::array<std::int16_t, 2> scratch_words{};
    std::uint8_t health{};
    std::uint8_t attack_power{};
    std::uint8_t weapon_type{};
    std::uint8_t collision_count{};
    std::uint8_t collision_flags{};
    std::int16_t velocity_x{};
    std::int16_t velocity_y{};
    std::int16_t velocity_z{};
    std::uint8_t hit_flags{};
    std::uint8_t colour_frame{};
    std::uint8_t animation_frame{};
    std::uint8_t sound1{};
    std::uint8_t sound2{};
    std::uint16_t colour_table{};
    std::uint8_t texture_scroll_x{};
    std::uint8_t texture_scroll_y{};
    ObjectHandle fire_object{};
    std::uint8_t strategy_state{};
    // PALVAROFFSET encodes the original parallel alx_ block as 0x80 plus
    // this zero-based index. Keeping its complete byte image lets the path
    // and strategy VMs preserve fields that do not yet have semantic names.
    std::array<std::uint8_t, kExtendedObjectBytes> extended{};
};

class ObjectPool {
public:
    ObjectPool() noexcept;

    void reset() noexcept;
    [[nodiscard]] ObjectHandle allocate_after(ObjectHandle previous = 0) noexcept;
    [[nodiscard]] bool remove(ObjectHandle handle) noexcept;
    [[nodiscard]] bool is_active(ObjectHandle handle) const noexcept;
    [[nodiscard]] GameObject& at(ObjectHandle handle);
    [[nodiscard]] const GameObject& at(ObjectHandle handle) const;
    [[nodiscard]] std::vector<ObjectHandle> active_handles() const;
    [[nodiscard]] std::vector<ObjectHandle> free_handles() const;
    void restore_lists(
        const std::vector<ObjectHandle>& active,
        const std::vector<ObjectHandle>& free);
    [[nodiscard]] ObjectHandle first_active() const noexcept { return first_active_; }
    [[nodiscard]] ObjectHandle next_active(ObjectHandle handle) const noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept { return active_count_; }
    [[nodiscard]] std::uint8_t read_base_byte(ObjectHandle handle, std::uint16_t offset) const;
    [[nodiscard]] std::uint16_t read_base_word(ObjectHandle handle, std::uint16_t offset) const;
    void write_base_byte(ObjectHandle handle, std::uint16_t offset, std::uint8_t value);
    void write_base_word(ObjectHandle handle, std::uint16_t offset, std::uint16_t value);
    void write_base_long(ObjectHandle handle, std::uint16_t offset, std::uint32_t value);
    [[nodiscard]] std::uint8_t read_path_byte(ObjectHandle handle, std::uint8_t encoded_offset) const;
    [[nodiscard]] std::uint16_t read_path_word(ObjectHandle handle, std::uint8_t encoded_offset) const;
    void write_path_byte(ObjectHandle handle, std::uint8_t encoded_offset, std::uint8_t value);
    void write_path_word(ObjectHandle handle, std::uint8_t encoded_offset, std::uint16_t value);

private:
    struct Slot {
        GameObject object;
        ObjectHandle next{};
        ObjectHandle previous{};
        bool active{};
    };

    [[nodiscard]] bool valid_handle(ObjectHandle handle) const noexcept;

    std::array<Slot, kMaximumObjects + 1> slots_{};
    std::array<ObjectHandle, kMaximumObjects + 1> free_next_{};
    ObjectHandle first_active_{};
    ObjectHandle first_free_{};
    std::size_t active_count_{};
};

} // namespace starfox::simulation
