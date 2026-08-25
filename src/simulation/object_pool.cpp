#include "starfox/simulation/object_pool.hpp"

#include <algorithm>
#include <bit>
#include <string>
#include <stdexcept>
#include <type_traits>

namespace starfox::simulation {

ObjectPool::ObjectPool() noexcept {
    reset();
}

void ObjectPool::reset() noexcept {
    slots_ = {};
    free_next_ = {};
    first_active_ = 0;
    first_free_ = 1;
    active_count_ = 0;
    for (ObjectHandle handle = 1; handle <= kMaximumObjects; ++handle) {
        free_next_[handle] = handle == kMaximumObjects
            ? ObjectHandle{0}
            : static_cast<ObjectHandle>(handle + 1U);
    }
}

ObjectHandle ObjectPool::allocate_after(ObjectHandle previous) noexcept {
    if (first_free_ == 0 || (previous != 0 && !is_active(previous))) {
        return 0;
    }
    const auto handle = first_free_;
    first_free_ = free_next_[handle];
    free_next_[handle] = 0;
    auto& slot = slots_[handle];
    slot = {};
    slot.active = true;

    if (previous == 0) {
        slot.next = first_active_;
        if (first_active_ != 0) {
            slots_[first_active_].previous = handle;
        }
        first_active_ = handle;
    } else {
        slot.previous = previous;
        slot.next = slots_[previous].next;
        slots_[previous].next = handle;
        if (slot.next != 0) {
            slots_[slot.next].previous = handle;
        }
    }
    ++active_count_;
    return handle;
}

bool ObjectPool::remove(ObjectHandle handle) noexcept {
    if (!is_active(handle)) {
        return false;
    }
    for (auto current = first_active_; current != 0; current = slots_[current].next) {
        auto& object = slots_[current].object;
        if (object.attached == handle) object.attached = 0;
        if (object.immune_object == handle) object.immune_object = 0;
        if (object.collision_object == handle) object.collision_object = 0;
        if (object.fire_object == handle) object.fire_object = 0;
    }

    auto& slot = slots_[handle];
    if (slot.previous == 0) {
        first_active_ = slot.next;
    } else {
        slots_[slot.previous].next = slot.next;
    }
    if (slot.next != 0) {
        slots_[slot.next].previous = slot.previous;
    }
    slot = {};
    free_next_[handle] = first_free_;
    first_free_ = handle;
    --active_count_;
    return true;
}

bool ObjectPool::valid_handle(ObjectHandle handle) const noexcept {
    return handle > 0 && handle <= kMaximumObjects;
}

bool ObjectPool::is_active(ObjectHandle handle) const noexcept {
    return valid_handle(handle) && slots_[handle].active;
}

ObjectHandle ObjectPool::next_active(ObjectHandle handle) const noexcept {
    return is_active(handle) ? slots_[handle].next : 0;
}

GameObject& ObjectPool::at(ObjectHandle handle) {
    if (!is_active(handle)) {
        throw std::out_of_range{"inactive object handle " + std::to_string(handle)};
    }
    return slots_[handle].object;
}

const GameObject& ObjectPool::at(ObjectHandle handle) const {
    if (!is_active(handle)) {
        throw std::out_of_range{"inactive object handle " + std::to_string(handle)};
    }
    return slots_[handle].object;
}

std::vector<ObjectHandle> ObjectPool::active_handles() const {
    std::vector<ObjectHandle> result;
    result.reserve(active_count_);
    for (auto handle = first_active_; handle != 0; handle = slots_[handle].next) {
        result.push_back(handle);
    }
    return result;
}

std::vector<ObjectHandle> ObjectPool::free_handles() const {
    std::vector<ObjectHandle> result;
    result.reserve(kMaximumObjects - active_count_);
    for (auto handle = first_free_; handle != 0; handle = free_next_[handle]) {
        result.push_back(handle);
    }
    return result;
}

void ObjectPool::restore_lists(
    const std::vector<ObjectHandle>& active,
    const std::vector<ObjectHandle>& free) {
    if (active.size() + free.size() != kMaximumObjects) {
        throw std::invalid_argument{"restored object lists do not cover every slot"};
    }
    std::array<bool, kMaximumObjects + 1> seen{};
    const auto validate = [&seen](const std::vector<ObjectHandle>& list) {
        for (const auto handle : list) {
            if (handle == 0 || handle > kMaximumObjects || seen[handle]) {
                throw std::invalid_argument{"restored object list contains an invalid slot"};
            }
            seen[handle] = true;
        }
    };
    validate(active);
    validate(free);

    for (ObjectHandle handle = 1; handle <= kMaximumObjects; ++handle) {
        const auto was_active = slots_[handle].active;
        const auto will_be_active = std::find(active.begin(), active.end(), handle) != active.end();
        if (was_active && !will_be_active) {
            slots_[handle].object = {};
        }
        slots_[handle].next = 0;
        slots_[handle].previous = 0;
        slots_[handle].active = will_be_active;
        free_next_[handle] = 0;
    }
    for (std::size_t index = 0; index < active.size(); ++index) {
        auto& slot = slots_[active[index]];
        slot.previous = index == 0 ? 0 : active[index - 1U];
        slot.next = index + 1U == active.size() ? 0 : active[index + 1U];
    }
    for (std::size_t index = 0; index < free.size(); ++index) {
        free_next_[free[index]] = index + 1U == free.size() ? 0 : free[index + 1U];
    }
    first_active_ = active.empty() ? 0 : active.front();
    first_free_ = free.empty() ? 0 : free.front();
    active_count_ = active.size();
}

std::uint8_t ObjectPool::read_base_byte(
    ObjectHandle handle, std::uint16_t offset) const {
    const auto& object = at(handle);
    const auto read_u16_byte = [offset](auto field, std::uint16_t base) {
        return static_cast<std::uint8_t>(
            static_cast<std::uint16_t>(field) >> ((offset - base) * 8U));
    };
    const auto read_i16_byte = [offset](std::int16_t field, std::uint16_t base) {
        return static_cast<std::uint8_t>(
            std::bit_cast<std::uint16_t>(field) >> ((offset - base) * 8U));
    };

    if (offset >= 4 && offset <= 5) return read_u16_byte(object.shape, 4);
    if (offset >= 6 && offset <= 7) return read_u16_byte(object.attached, 6);
    if (offset == 8) return object.flags;
    if (offset == 9) return object.type;
    if (offset == 10) return object.count;
    if (offset == 11) return object.count1;
    if (offset >= 12 && offset <= 13) return read_i16_byte(object.world_x, 12);
    if (offset >= 14 && offset <= 15) return read_i16_byte(object.world_y, 14);
    if (offset >= 16 && offset <= 17) return read_i16_byte(object.world_z, 16);
    if (offset == 18) return object.rotation_x;
    if (offset == 19) return object.rotation_y;
    if (offset == 20) return object.rotation_z;
    if (offset == 21) return std::bit_cast<std::uint8_t>(object.velocity);
    if (offset >= 22 && offset <= 24) {
        return static_cast<std::uint8_t>(object.strategy_address >> ((offset - 22U) * 8U));
    }
    if (offset >= 25 && offset <= 26) return read_u16_byte(object.immune_object, 25);
    if (offset >= 27 && offset <= 28) return read_u16_byte(object.collision_object, 27);
    if (offset >= 29 && offset <= 32) return object.strategy_flags[offset - 29U];
    if (offset == 33) return std::bit_cast<std::uint8_t>(object.skid_y);
    if (offset >= 34 && offset <= 37) {
        return std::bit_cast<std::uint8_t>(object.scratch_bytes[offset - 34U]);
    }
    if (offset >= 38 && offset <= 39) return read_i16_byte(object.scratch_words[0], 38);
    if (offset >= 40 && offset <= 41) return read_i16_byte(object.scratch_words[1], 40);
    if (offset == 42) return object.health;
    if (offset == 43) return object.attack_power;
    if (offset == 44) return object.weapon_type;
    if (offset == 45) return object.collision_count;
    if (offset == 46) return object.collision_flags;
    if (offset >= 47 && offset <= 48) return read_i16_byte(object.velocity_x, 47);
    if (offset >= 49 && offset <= 50) return read_i16_byte(object.velocity_y, 49);
    if (offset >= 51 && offset <= 52) return read_i16_byte(object.velocity_z, 51);
    if (offset == 53) return object.hit_flags;
    if (offset >= 54 && offset <= 55) {
        return std::bit_cast<std::uint8_t>(object.scratch_bytes[offset - 50U]);
    }
    throw std::out_of_range{"alien-block byte offset is outside al_size"};
}

std::uint16_t ObjectPool::read_base_word(
    ObjectHandle handle, std::uint16_t offset) const {
    return static_cast<std::uint16_t>(read_base_byte(handle, offset))
        | (static_cast<std::uint16_t>(read_base_byte(handle, offset + 1U)) << 8U);
}

void ObjectPool::write_base_byte(
    ObjectHandle handle, std::uint16_t offset, std::uint8_t value) {
    auto& object = at(handle);
    const auto write_u16_byte = [offset, value](auto& field, std::uint16_t base) {
        auto bits = static_cast<std::uint16_t>(field);
        const auto shift = static_cast<unsigned>((offset - base) * 8U);
        bits = static_cast<std::uint16_t>(
            (bits & ~(std::uint16_t{0xff} << shift)) | (std::uint16_t{value} << shift));
        field = static_cast<std::remove_reference_t<decltype(field)>>(bits);
    };
    const auto write_i16_byte = [offset, value](std::int16_t& field, std::uint16_t base) {
        auto bits = std::bit_cast<std::uint16_t>(field);
        const auto shift = static_cast<unsigned>((offset - base) * 8U);
        bits = static_cast<std::uint16_t>(
            (bits & ~(std::uint16_t{0xff} << shift)) | (std::uint16_t{value} << shift));
        field = std::bit_cast<std::int16_t>(bits);
    };

    if (offset >= 4 && offset <= 5) write_u16_byte(object.shape, 4);
    else if (offset >= 6 && offset <= 7) write_u16_byte(object.attached, 6);
    else if (offset == 8) object.flags = value;
    else if (offset == 9) object.type = value;
    else if (offset == 10) object.count = value;
    else if (offset == 11) object.count1 = value;
    else if (offset >= 12 && offset <= 13) write_i16_byte(object.world_x, 12);
    else if (offset >= 14 && offset <= 15) write_i16_byte(object.world_y, 14);
    else if (offset >= 16 && offset <= 17) write_i16_byte(object.world_z, 16);
    else if (offset == 18) object.rotation_x = value;
    else if (offset == 19) object.rotation_y = value;
    else if (offset == 20) object.rotation_z = value;
    else if (offset == 21) object.velocity = std::bit_cast<std::int8_t>(value);
    else if (offset >= 22 && offset <= 24) {
        const auto shift = static_cast<unsigned>((offset - 22U) * 8U);
        object.strategy_address = (object.strategy_address & ~(std::uint32_t{0xff} << shift))
            | (std::uint32_t{value} << shift);
    } else if (offset >= 25 && offset <= 26) write_u16_byte(object.immune_object, 25);
    else if (offset >= 27 && offset <= 28) write_u16_byte(object.collision_object, 27);
    else if (offset >= 29 && offset <= 32) object.strategy_flags[offset - 29U] = value;
    else if (offset == 33) object.skid_y = std::bit_cast<std::int8_t>(value);
    else if (offset >= 34 && offset <= 37) {
        object.scratch_bytes[offset - 34U] = std::bit_cast<std::int8_t>(value);
    } else if (offset >= 38 && offset <= 39) write_i16_byte(object.scratch_words[0], 38);
    else if (offset >= 40 && offset <= 41) write_i16_byte(object.scratch_words[1], 40);
    else if (offset == 42) object.health = value;
    else if (offset == 43) object.attack_power = value;
    else if (offset == 44) object.weapon_type = value;
    else if (offset == 45) object.collision_count = value;
    else if (offset == 46) object.collision_flags = value;
    else if (offset >= 47 && offset <= 48) write_i16_byte(object.velocity_x, 47);
    else if (offset >= 49 && offset <= 50) write_i16_byte(object.velocity_y, 49);
    else if (offset >= 51 && offset <= 52) write_i16_byte(object.velocity_z, 51);
    else if (offset == 53) object.hit_flags = value;
    else if (offset >= 54 && offset <= 55) {
        object.scratch_bytes[offset - 50U] = std::bit_cast<std::int8_t>(value);
    } else {
        throw std::out_of_range{"alien-block byte offset is outside al_size"};
    }
}

void ObjectPool::write_base_word(
    ObjectHandle handle, std::uint16_t offset, std::uint16_t value) {
    write_base_byte(handle, offset, static_cast<std::uint8_t>(value));
    write_base_byte(handle, static_cast<std::uint16_t>(offset + 1U),
                    static_cast<std::uint8_t>(value >> 8U));
}

void ObjectPool::write_base_long(
    ObjectHandle handle, std::uint16_t offset, std::uint32_t value) {
    write_base_word(handle, offset, static_cast<std::uint16_t>(value));
    write_base_byte(handle, static_cast<std::uint16_t>(offset + 2U),
                    static_cast<std::uint8_t>(value >> 16U));
}

std::uint8_t ObjectPool::read_path_byte(
    ObjectHandle handle, std::uint8_t encoded_offset) const {
    if ((encoded_offset & 0x80U) == 0) {
        return read_base_byte(handle, encoded_offset);
    }
    const auto index = static_cast<std::size_t>(encoded_offset & 0x7fU);
    if (index >= kExtendedObjectBytes) {
        throw std::out_of_range{"extended alien-block byte offset is outside alx_size"};
    }
    return at(handle).extended[index];
}

std::uint16_t ObjectPool::read_path_word(
    ObjectHandle handle, std::uint8_t encoded_offset) const {
    return static_cast<std::uint16_t>(read_path_byte(handle, encoded_offset))
        | (static_cast<std::uint16_t>(read_path_byte(
               handle, static_cast<std::uint8_t>(encoded_offset + 1U)))
           << 8U);
}

void ObjectPool::write_path_byte(
    ObjectHandle handle, std::uint8_t encoded_offset, std::uint8_t value) {
    if ((encoded_offset & 0x80U) == 0) {
        write_base_byte(handle, encoded_offset, value);
        return;
    }
    const auto index = static_cast<std::size_t>(encoded_offset & 0x7fU);
    if (index >= kExtendedObjectBytes) {
        throw std::out_of_range{"extended alien-block byte offset is outside alx_size"};
    }
    auto& object = at(handle);
    object.extended[index] = value;
    // Mirror the semantic members already exposed by GameObject.
    if (index == 18) object.strategy_state = value;
    if (index >= 19 && index <= 20) {
        const auto bits = static_cast<std::uint16_t>(object.fire_object);
        const auto shift = static_cast<unsigned>((index - 19U) * 8U);
        object.fire_object = static_cast<ObjectHandle>(
            (bits & ~(std::uint16_t{0xff} << shift)) | (std::uint16_t{value} << shift));
    }
    if (index == 28) object.colour_frame = value;
    if (index == 29) object.animation_frame = value;
    if (index == 30) object.sound1 = value;
    if (index == 31) object.sound2 = value;
    if (index >= 32 && index <= 33) {
        const auto shift = static_cast<unsigned>((index - 32U) * 8U);
        object.colour_table = static_cast<std::uint16_t>(
            (object.colour_table & ~(std::uint16_t{0xff} << shift))
            | (std::uint16_t{value} << shift));
    }
    if (index == 42) object.texture_scroll_x = value;
    if (index == 43) object.texture_scroll_y = value;
}

void ObjectPool::write_path_word(
    ObjectHandle handle, std::uint8_t encoded_offset, std::uint16_t value) {
    write_path_byte(handle, encoded_offset, static_cast<std::uint8_t>(value));
    write_path_byte(handle, static_cast<std::uint8_t>(encoded_offset + 1U),
                    static_cast<std::uint8_t>(value >> 8U));
}

} // namespace starfox::simulation
