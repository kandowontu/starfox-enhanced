#include "starfox/simulation/path_vm.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace starfox::simulation {
namespace {

constexpr unsigned kFlagCollisionDisabled = 8;
constexpr unsigned kFlagRelativeToPlayer = 12;
constexpr unsigned kFlagAlwaysGenerateVectors = 13;
constexpr unsigned kFlagHelicopter = 14;
constexpr unsigned kFlagSpaceship = 15;
constexpr unsigned kFlagSmoke = 17;
constexpr unsigned kFlagNoHitAffect = 21;
constexpr unsigned kFlagInvisible = 27;
constexpr unsigned kFlagPath = 29;
constexpr unsigned kFlagShadow = 3;
constexpr std::uint8_t kTypeZRemove = 8;

std::uint32_t rom_symbol(const assets::SymbolMap& symbols, const std::string& name) {
    for (const auto address : symbols.find(name)) {
        if ((address & 0xffffU) >= 0x8000U && ((address >> 16U) & 0xffU) < 0x7eU) {
            return address;
        }
    }
    throw std::runtime_error{"missing ROM symbol: " + name};
}

bool flag(const GameObject& object, unsigned bit) noexcept {
    return (object.strategy_flags[bit / 8U] & (1U << (bit % 8U))) != 0;
}

void set_flag(GameObject& object, unsigned bit, bool enabled) noexcept {
    const auto mask = static_cast<std::uint8_t>(1U << (bit % 8U));
    if (enabled) object.strategy_flags[bit / 8U] |= mask;
    else object.strategy_flags[bit / 8U] &= static_cast<std::uint8_t>(~mask);
}

std::uint8_t bits(std::int8_t value) noexcept {
    return std::bit_cast<std::uint8_t>(value);
}

std::int8_t signed_byte(std::uint8_t value) noexcept {
    return std::bit_cast<std::int8_t>(value);
}

std::int8_t multiply_original(std::int8_t left, std::int8_t right) noexcept {
    const auto left_bits = bits(left);
    const auto right_bits = bits(right);
    const auto left_abs = left < 0
        ? static_cast<std::uint8_t>(0U - left_bits)
        : left_bits;
    const auto right_abs = right < 0
        ? static_cast<std::uint8_t>(0U - right_bits)
        : right_bits;
    const auto doubled = static_cast<std::uint8_t>(left_abs << 1U);
    auto result = static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(doubled) * right_abs) >> 8U);
    if ((left < 0) != (right < 0)) result = static_cast<std::uint8_t>(0U - result);
    return signed_byte(result);
}

std::int16_t signed_difference16(std::uint16_t left, std::uint16_t right) noexcept {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(left - right));
}

std::int8_t signed_difference8(std::uint8_t left, std::uint8_t right) noexcept {
    return signed_byte(static_cast<std::uint8_t>(left - right));
}

} // namespace

PathVm::PathVm(
    const assets::RomImage& rom,
    std::uint32_t paths_address,
    std::uint32_t path_strategy_address,
    ObjectPool& objects,
    TrigTables trig,
    OriginalPrng& random,
    std::uint32_t particle_strategy_address,
    std::uint16_t null_shape) noexcept
    : rom_(&rom),
      paths_address_(paths_address),
      path_strategy_address_(path_strategy_address),
      particle_strategy_address_(particle_strategy_address),
      null_shape_(null_shape),
      objects_(&objects),
      trig_(std::move(trig)),
      random_(&random) {}

PathVm::PathVm(
    const assets::RomImage& rom,
    const assets::SymbolMap& symbols,
    ObjectPool& objects,
    OriginalPrng& random)
    : PathVm(
          rom,
          rom_symbol(symbols, "PATHS"),
          rom_symbol(symbols, "PATH_ISTRAT"),
          objects,
          TrigTables::load(rom, symbols),
          random,
          rom_symbol(symbols, "PARTICLEEXPLODE_ISTRAT"),
          static_cast<std::uint16_t>(rom_symbol(symbols, "NULLSHAPE"))) {}

void PathVm::set_player(ObjectHandle player) {
    if (!objects_->is_active(player)) {
        throw std::invalid_argument{"path player handle must be active"};
    }
    player_ = player;
}

void PathVm::attach(ObjectHandle object, std::uint16_t path_offset_value) {
    if (!objects_->is_active(object)) {
        throw std::invalid_argument{"cannot attach a path to an inactive object"};
    }
    auto& value = objects_->at(object);
    value.strategy_address = path_strategy_address_;
    set_path_offset(object, path_offset_value);
    value.scratch_bytes[1] = 0;
    value.scratch_bytes[2] = 0;
    states_[object] = {};
}

void PathVm::detach(ObjectHandle object) noexcept {
    states_.erase(object);
}

bool PathVm::is_attached(ObjectHandle object) const noexcept {
    return objects_->is_active(object)
        && objects_->at(object).strategy_address == path_strategy_address_;
}

std::uint16_t PathVm::path_offset(ObjectHandle object) const {
    return std::bit_cast<std::uint16_t>(objects_->at(object).scratch_words[1]);
}

void PathVm::set_path_offset(ObjectHandle object, std::uint16_t offset) {
    objects_->at(object).scratch_words[1] = std::bit_cast<std::int16_t>(offset);
}

std::uint8_t PathVm::read8(std::uint16_t offset) const {
    return rom_->read8(paths_address_ + offset);
}

std::uint16_t PathVm::read16(std::uint16_t offset) const {
    return rom_->read16(paths_address_ + offset);
}

std::int16_t PathVm::read_i16(std::uint16_t offset) const {
    return rom_->read_i16(paths_address_ + offset);
}

std::uint8_t PathVm::chase_byte(std::uint8_t current, std::uint8_t target) const noexcept {
    const auto difference = signed_difference8(target, current);
    if (difference == 0) return current;
    const auto step = difference > 0
        ? std::max(1, static_cast<int>(difference) / 8)
        : std::min(-1, static_cast<int>(difference) / 8);
    return static_cast<std::uint8_t>(current + step);
}

std::uint16_t PathVm::chase_word(std::uint16_t current, std::uint16_t target) const noexcept {
    const auto difference = signed_difference16(target, current);
    if (difference == 0) return current;
    const auto step = difference > 0
        ? std::max(1, static_cast<int>(difference) / 8)
        : std::min(-1, static_cast<int>(difference) / 8);
    return static_cast<std::uint16_t>(current + step);
}

void PathVm::generate_vectors(GameObject& object) const noexcept {
    if (flag(object, kFlagHelicopter)) {
        const auto angle = static_cast<std::uint8_t>(1U - object.rotation_y);
        object.velocity_x = multiply_original(object.velocity, trig_.sin8(angle));
        object.velocity_z = multiply_original(object.velocity, trig_.cos8(angle));
        return;
    }
    const auto pitch = static_cast<std::uint8_t>(0U - object.rotation_x);
    const auto x = multiply_original(object.velocity, trig_.sin8(object.rotation_y));
    const auto z = multiply_original(object.velocity, trig_.cos8(object.rotation_y));
    object.velocity_x = multiply_original(x, trig_.cos8(pitch));
    object.velocity_z = multiply_original(z, trig_.cos8(pitch));
    object.velocity_y = multiply_original(object.velocity, trig_.sin8(pitch));
}

void PathVm::move(ObjectHandle handle) {
    if (!objects_->is_active(handle)) return;
    auto& object = objects_->at(handle);
    if (object.count1 != 0) {
        const auto target = bits(signed_byte(object.count));
        const auto current = bits(object.velocity);
        const auto difference = signed_difference8(target, current);
        auto next = current;
        if (difference > 0) {
            next = static_cast<std::uint8_t>(current + object.count1);
            if (signed_difference8(target, next) <= 0) {
                next = target;
                object.count1 = 0;
            }
        } else if (difference < 0) {
            next = static_cast<std::uint8_t>(current - object.count1);
            if (signed_difference8(target, next) >= 0) {
                next = target;
                object.count1 = 0;
            }
        } else {
            object.count1 = 0;
        }
        object.velocity = signed_byte(next);
        if (!flag(object, kFlagAlwaysGenerateVectors)) generate_vectors(object);
    }
    if (flag(object, kFlagSpaceship)) {
        object.rotation_y = static_cast<std::uint8_t>(
            object.rotation_y + signed_byte(object.rotation_z) / 4);
    }
    if (flag(object, kFlagHelicopter)) {
        object.velocity = signed_byte(object.rotation_x);
        if (!flag(object, kFlagAlwaysGenerateVectors)) generate_vectors(object);
    }
    if (flag(object, kFlagRelativeToPlayer)) {
        object.world_z = add16(object.world_z, view_velocity_z_);
    }
    if (flag(object, kFlagAlwaysGenerateVectors)) generate_vectors(object);
    object.world_x = add16(object.world_x, object.velocity_x);
    object.world_y = add16(object.world_y, object.velocity_y);
    object.world_z = add16(object.world_z, object.velocity_z);
}

ObjectHandle PathVm::spawn(ObjectHandle parent, std::uint16_t shape) {
    const auto child = objects_->allocate_after(parent);
    if (child == 0) return 0;
    auto& object = objects_->at(child);
    object.shape = shape;
    object.strategy_address = path_strategy_address_;
    states_[child] = {};
    return child;
}

void PathVm::tick_all() {
    const auto handles = objects_->active_handles();
    for (const auto handle : handles) {
        if (is_attached(handle)) tick(handle);
    }
}

void PathVm::tick(ObjectHandle handle) {
    if (!is_attached(handle)) return;
    if (player_ == 0 || !objects_->is_active(player_)) {
        throw std::runtime_error{"path VM has no active player"};
    }
    auto& state = states_[handle];
    for (std::size_t operations = 0; operations < 65'536; ++operations) {
        if (!objects_->is_active(handle)) {
            states_.erase(handle);
            return;
        }
        auto& object = objects_->at(handle);
        auto pc = path_offset(handle);
        const auto opcode = read8(pc);
        const auto advance = [&](std::uint16_t amount) {
            pc = static_cast<std::uint16_t>(pc + amount);
            set_path_offset(handle, pc);
        };
        const auto jump = [&](std::uint16_t target) {
            pc = target;
            set_path_offset(handle, pc);
        };
        const auto set_object_flag = [&](unsigned bit, bool enabled) {
            set_flag(object, bit, enabled);
        };

        switch (opcode) {
        case 0: set_object_flag(kFlagRelativeToPlayer, true); advance(1); continue;
        case 1: set_object_flag(kFlagRelativeToPlayer, false); advance(1); continue;
        case 2: {
            const auto target = read8(pc + 1U);
            const auto count = bits(object.scratch_bytes[2]);
            if (count == target) {
                object.scratch_bytes[2] = 0;
                advance(2);
                continue;
            }
            object.scratch_bytes[2] = signed_byte(static_cast<std::uint8_t>(count + 1U));
            move(handle);
            return;
        }
        case 3: set_object_flag(kFlagAlwaysGenerateVectors, true); advance(1); continue;
        case 4: set_object_flag(kFlagAlwaysGenerateVectors, false); advance(1); continue;
        case 5:
            object.velocity = signed_byte(read8(pc + 1U));
            if (!flag(object, kFlagAlwaysGenerateVectors)) generate_vectors(object);
            advance(2);
            continue;
        case 6: {
            const auto limit = read8(pc + 1U);
            const auto count = bits(object.scratch_bytes[1]);
            if (count == limit) {
                object.scratch_bytes[1] = 0;
                advance(4);
                continue;
            }
            object.scratch_bytes[1] = signed_byte(static_cast<std::uint8_t>(count + 1U));
            jump(read16(pc + 2U));
            move(handle);
            return;
        }
        case 7: {
            const auto offset = read8(pc + 1U);
            objects_->write_path_byte(handle, offset,
                static_cast<std::uint8_t>(objects_->read_path_byte(handle, offset) + read8(pc + 2U)));
            advance(3);
            continue;
        }
        case 8: {
            const auto offset = read8(pc + 1U);
            objects_->write_path_word(handle, offset,
                static_cast<std::uint16_t>(objects_->read_path_word(handle, offset) + read16(pc + 2U)));
            advance(4);
            continue;
        }
        case 11: case 13: {
            const auto target = read8(pc + 1U);
            const auto offset = read8(pc + 2U);
            const auto current = objects_->read_path_byte(handle, offset);
            const auto next = chase_byte(current, target);
            objects_->write_path_byte(handle, offset, next);
            if (opcode == 13 && next != target) {
                move(handle);
                return;
            }
            advance(3);
            continue;
        }
        case 12: case 14: {
            const auto target = read16(pc + 1U);
            const auto offset = read8(pc + 3U);
            const auto current = objects_->read_path_word(handle, offset);
            const auto next = chase_word(current, target);
            objects_->write_path_word(handle, offset, next);
            if (opcode == 14 && next != target) {
                move(handle);
                return;
            }
            advance(4);
            continue;
        }
        case 15:
            object.type |= kTypeZRemove;
            move(handle);
            return;
        case 16: objects_->write_path_byte(handle, read8(pc + 2U), read8(pc + 1U)); advance(3); continue;
        case 17: objects_->write_path_word(handle, read8(pc + 3U), read16(pc + 1U)); advance(4); continue;
        case 18: case 19: {
            const auto wanted_shape = read16(pc + 1U);
            auto found = ObjectHandle{0};
            bool after_current = opcode == 18;
            for (const auto candidate : objects_->active_handles()) {
                if (candidate == handle) continue;
                if (opcode == 19 && !after_current) {
                    if (candidate == object.attached) after_current = true;
                    continue;
                }
                const auto& other = objects_->at(candidate);
                if (other.shape == wanted_shape
                    && std::abs(static_cast<int>(subtract16(other.world_z, object.world_z))) < 7000) {
                    found = candidate;
                    break;
                }
            }
            object.attached = found;
            advance(3);
            continue;
        }
        case 21:
            (void)objects_->remove(handle);
            states_.erase(handle);
            return;
        case 25:
            if (objects_->is_active(object.attached)) {
                object.immune_object = object.attached;
                objects_->at(object.attached).immune_object = handle;
            }
            advance(1);
            continue;
        case 26: set_object_flag(kFlagSpaceship, true); advance(1); continue;
        case 27: set_object_flag(kFlagSpaceship, false); advance(1); continue;
        case 28: set_object_flag(kFlagHelicopter, true); advance(1); continue;
        case 29: set_object_flag(kFlagHelicopter, false); advance(1); continue;
        case 30: {
            const auto distance = std::abs(static_cast<int>(subtract16(
                object.world_z, objects_->at(player_).world_z)));
            auto condition = distance < static_cast<int>(read16(pc + 1U));
            if (state.if_not) { condition = !condition; state.if_not = false; }
            if (condition) jump(read16(pc + 3U)); else advance(5);
            continue;
        }
        case 31: {
            auto condition = false;
            if (objects_->is_active(object.attached)) {
                condition = std::abs(static_cast<int>(subtract16(
                    object.world_z, objects_->at(object.attached).world_z)))
                    < static_cast<int>(read16(pc + 1U));
            }
            if (state.if_not) { condition = !condition; state.if_not = false; }
            if (condition) jump(read16(pc + 3U)); else advance(5);
            continue;
        }
        case 32: case 33: jump(read16(pc + 1U)); continue;
        case 34:
            object.count = read8(pc + 1U);
            object.count1 = read8(pc + 2U);
            advance(3);
            continue;
        case 37: object.animation_frame = read8(pc + 1U); advance(2); continue;
        case 38:
            object.animation_frame = static_cast<std::uint8_t>(
                object.animation_frame + read8(pc + 1U));
            if (object.animation_frame >= read8(pc + 2U)) object.animation_frame = 0;
            advance(3);
            continue;
        case 40: object.scratch_bytes[3] = signed_byte(read8(pc + 1U)); advance(2); continue;
        case 41:
            if (pending_link_ == 0 || !objects_->is_active(pending_link_)) pending_link_ = handle;
            else {
                object.attached = pending_link_;
                objects_->at(pending_link_).attached = handle;
                pending_link_ = 0;
            }
            advance(1);
            continue;
        case 42:
            if (!objects_->is_active(object.attached)) jump(read16(pc + 1U));
            else advance(3);
            continue;
        case 43: {
            auto condition = static_cast<std::uint8_t>(read8(pc + 1U) - 1U) == current_level_;
            if (state.if_not) { condition = !condition; state.if_not = false; }
            if (condition) jump(read16(pc + 2U)); else advance(4);
            continue;
        }
        case 44: case 45: case 46: case 47: case 122: {
            const auto& player = objects_->at(player_);
            bool condition{};
            if (opcode == 44) condition = player.world_x < object.world_x;
            if (opcode == 45) condition = player.world_x >= object.world_x;
            if (opcode == 46) condition = player.world_y >= object.world_y;
            if (opcode == 47) condition = player.world_y < object.world_y;
            if (opcode == 122) condition = player.world_z >= object.world_z;
            if (condition) jump(read16(pc + 1U)); else advance(3);
            continue;
        }
        case 48: {
            const auto requested = read8(pc + 1U);
            const auto current = bits(object.scratch_bytes[3]);
            auto condition = requested == 0xffU ? current == 0 : current != requested;
            if (state.if_not) { condition = !condition; state.if_not = false; }
            if (condition) jump(read16(pc + 2U)); else advance(4);
            continue;
        }
        case 49: case 50: case 137:
            events_.push_back({PathEventKind::message, handle,
                opcode == 50 ? bits(object.scratch_bytes[0]) : read8(pc + 1U)});
            advance(2);
            continue;
        case 51:
            object.health = object.health > 10 ? static_cast<std::uint8_t>(object.health - 10U) : 0;
            advance(1);
            continue;
        case 53: set_object_flag(kFlagSmoke, true); advance(1); continue;
        case 54: set_object_flag(kFlagSmoke, false); advance(1); continue;
        case 55:
            if (random_->next() < 127U) jump(read16(pc + 1U)); else advance(3);
            continue;
        case 56: case 57: {
            const auto offset = read8(pc + 1U);
            const auto value = opcode == 56 ? read8(pc + 2U) : read16(pc + 2U);
            auto condition = (opcode == 56 ? objects_->read_path_byte(handle, offset)
                                           : objects_->read_path_word(handle, offset)) == value;
            if (state.if_not) { condition = !condition; state.if_not = false; }
            const auto target_offset = opcode == 56 ? 3U : 4U;
            const auto length = opcode == 56 ? 5U : 6U;
            if (condition) jump(read16(static_cast<std::uint16_t>(pc + target_offset)));
            else advance(static_cast<std::uint16_t>(length));
            continue;
        }
        case 58: case 59: {
            const auto offset = read8(pc + 1U);
            const auto value = opcode == 58 ? objects_->read_path_byte(handle, offset)
                                           : objects_->read_path_word(handle, offset);
            const auto low = opcode == 58 ? read8(pc + 2U) : read16(pc + 2U);
            const auto high = opcode == 58 ? read8(pc + 3U) : read16(pc + 4U);
            auto condition = value > low && value <= high;
            if (state.if_not) { condition = !condition; state.if_not = false; }
            const auto target_offset = opcode == 58 ? 4U : 6U;
            const auto length = opcode == 58 ? 6U : 8U;
            if (condition) jump(read16(static_cast<std::uint16_t>(pc + target_offset)));
            else advance(static_cast<std::uint16_t>(length));
            continue;
        }
        case 60: set_object_flag(kFlagNoHitAffect, true); advance(1); continue;
        case 61: set_object_flag(kFlagNoHitAffect, false); advance(1); continue;
        case 62: if (player_dead_) jump(read16(pc + 1U)); else advance(3); continue;
        case 63: case 64: case 65: {
            const auto child = spawn(handle, read16(pc + 1U));
            if (child != 0) {
                auto& spawned = objects_->at(child);
                set_path_offset(child, read16(pc + 3U));
                spawned.rotation_x = static_cast<std::uint8_t>(object.rotation_x + read8(pc + 5U));
                spawned.rotation_y = static_cast<std::uint8_t>(object.rotation_y + read8(pc + 6U));
                spawned.rotation_z = static_cast<std::uint8_t>(object.rotation_z + read8(pc + 7U));
                spawned.health = read8(pc + 8U);
                spawned.attack_power = read8(pc + 9U);
                spawned.world_x = add16(object.world_x,
                    static_cast<std::int16_t>(signed_byte(read8(pc + 10U)) * 4));
                spawned.world_y = add16(object.world_y,
                    static_cast<std::int16_t>(signed_byte(read8(pc + 11U)) * 4));
                spawned.world_z = add16(object.world_z,
                    static_cast<std::int16_t>(signed_byte(read8(pc + 12U)) * 4));
                if (opcode == 64) {
                    object.attached = child;
                    spawned.attached = handle;
                }
                if (opcode == 65) {
                    spawned.scratch_bytes[0] = signed_byte(read8(pc + 13U));
                    spawned.attached = handle;
                }
            }
            advance(opcode == 65 ? 14 : 13);
            continue;
        }
        case 66: object.type |= kTypeZRemove; advance(1); continue;
        case 67: object.type &= static_cast<std::uint8_t>(~kTypeZRemove); advance(1); continue;
        case 69: case 70: case 71: case 72: case 73: case 74:
            events_.push_back({PathEventKind::fire_weapon, handle, object.weapon_type});
            advance(1);
            continue;
        case 75: object.weapon_type = read8(pc + 1U); advance(2); continue;
        case 78:
            if (objects_->is_active(object.attached)) set_flag(objects_->at(object.attached), kFlagPath, true);
            advance(1);
            continue;
        case 81:
            if (flag(object, kFlagPath)) {
                set_object_flag(kFlagPath, false);
                jump(read16(pc + 1U));
            } else advance(3);
            continue;
        case 83: object.extended[43] = read8(pc + 1U); advance(2); continue;
        case 84:
            state.stack.push_back(pc);
            jump(read16(pc + 1U));
            continue;
        case 85:
            if (state.stack.empty()) throw std::runtime_error{"PATH return stack underflow"};
            pc = state.stack.back();
            state.stack.pop_back();
            set_path_offset(handle, static_cast<std::uint16_t>(pc + 3U));
            continue;
        case 86:
            state.stack.push_back(static_cast<std::uint16_t>(pc + 3U));
            state.stack.push_back(read16(pc + 1U));
            advance(3);
            continue;
        case 87: case 88: {
            if (state.stack.size() < 2) throw std::runtime_error{"PATH loop stack underflow"};
            auto count = state.stack.back(); state.stack.pop_back();
            const auto body = state.stack.back(); state.stack.pop_back();
            --count;
            if (count == 0) {
                advance(1);
                continue;
            }
            state.stack.push_back(body);
            state.stack.push_back(count);
            jump(body);
            if (opcode == 87) { move(handle); return; }
            continue;
        }
        case 89: case 90:
            if (state.stack.size() < 2) throw std::runtime_error{"PATH break stack underflow"};
            state.stack.resize(state.stack.size() - 2U);
            if (opcode == 89) jump(read16(pc + 1U)); else advance(1);
            continue;
        case 91:
            set_object_flag(kFlagInvisible, true);
            set_object_flag(kFlagCollisionDisabled, true);
            advance(1);
            continue;
        case 92:
            set_object_flag(kFlagInvisible, false);
            set_object_flag(kFlagCollisionDisabled, false);
            advance(1);
            continue;
        case 93:
            state.triggers.push_back({read16(pc + 1U), read8(pc + 3U)});
            advance(4);
            continue;
        case 94: {
            const auto target = read16(pc + 1U);
            const auto found = std::find_if(state.triggers.begin(), state.triggers.end(),
                [target](const RuntimeState::Trigger& trigger) { return trigger.path == target; });
            if (found != state.triggers.end()) state.triggers.erase(found);
            advance(3);
            continue;
        }
        case 95:
            // FORCE only redirects the currently executing trigger. Outside a
            // trigger (the common case) the original routine is a three-byte no-op.
            advance(3);
            continue;
        case 97:
            object.strategy_address = static_cast<std::uint32_t>(read16(pc + 1U))
                | (static_cast<std::uint32_t>(read8(pc + 3U)) << 16U);
            object.strategy_state = 0;
            object.scratch_words[1] = 0;
            states_.erase(handle);
            move(handle);
            return;
        case 98: case 99: case 100: case 101: case 102: case 103: case 104: case 105: {
            const auto destination = read8(pc + 1U);
            const auto source = read8(pc + 2U);
            const auto source_word = opcode == 99 || opcode == 100 || opcode == 103 || opcode == 104;
            const auto destination_word = opcode == 100 || opcode == 101 || opcode == 104 || opcode == 105;
            const auto source_value = source_word ? objects_->read_path_word(handle, source)
                                                  : objects_->read_path_byte(handle, source);
            if (opcode <= 101) {
                if (destination_word) objects_->write_path_word(handle, destination, source_value);
                else objects_->write_path_byte(handle, destination, static_cast<std::uint8_t>(source_value));
            } else {
                if (destination_word) objects_->write_path_word(handle, destination,
                    static_cast<std::uint16_t>(objects_->read_path_word(handle, destination) + source_value));
                else objects_->write_path_byte(handle, destination,
                    static_cast<std::uint8_t>(objects_->read_path_byte(handle, destination) + source_value));
            }
            advance(3);
            continue;
        }
        case 106: case 107: {
            const auto offset = static_cast<std::uint8_t>(read16(pc + 1U));
            if (opcode == 106) objects_->write_path_byte(handle, offset,
                static_cast<std::uint8_t>(0U - objects_->read_path_byte(handle, offset)));
            else objects_->write_path_word(handle, offset,
                static_cast<std::uint16_t>(0U - objects_->read_path_word(handle, offset)));
            advance(3);
            continue;
        }
        case 108: case 109: {
            const auto offset = static_cast<std::uint8_t>(read16(pc + 1U));
            if (opcode == 108) {
                objects_->write_path_byte(handle, offset,
                    static_cast<std::uint8_t>(random_->next() & read8(pc + 3U)));
                advance(4);
            } else {
                const auto value = static_cast<std::uint16_t>(random_->next()) << 8U | random_->next();
                objects_->write_path_word(handle, offset,
                    static_cast<std::uint16_t>(value & read16(pc + 3U)));
                advance(5);
            }
            continue;
        }
        case 110:
            if ((object.hit_flags & read8(pc + 3U)) != 0) {
                object.hit_flags &= static_cast<std::uint8_t>(~read8(pc + 3U));
                jump(read16(pc + 1U));
            } else advance(4);
            continue;
        case 111: set_object_flag(kFlagCollisionDisabled, false); advance(1); continue;
        case 112: set_object_flag(kFlagCollisionDisabled, true); advance(1); continue;
        case 114: state.if_not = true; advance(1); continue;
        case 115: {
            // PATHS.ASM P_PARTICLES creates a null-shape object, assigns the
            // regular particle explosion initializer, and copies only xyz.
            const auto particle = objects_->allocate_after(handle);
            if (particle != 0U) {
                auto& spawned = objects_->at(particle);
                spawned.shape = null_shape_;
                spawned.strategy_address = particle_strategy_address_;
                spawned.world_x = object.world_x;
                spawned.world_y = object.world_y;
                spawned.world_z = object.world_z;
            }
            advance(1);
            continue;
        }
        case 116: set_object_flag(kFlagShadow, true); advance(1); continue;
        case 117: set_object_flag(kFlagShadow, false); advance(1); continue;
        case 118: case 119:
            events_.push_back({opcode == 118 ? PathEventKind::sound_effect
                                             : PathEventKind::positional_sound,
                               handle, read8(pc + 1U)});
            advance(2);
            continue;
        case 120: {
            const auto child = spawn(handle, read16(pc + 1U));
            if (child != 0) {
                auto& spawned = objects_->at(child);
                spawned.world_x = object.world_x;
                spawned.world_y = object.world_y;
                spawned.world_z = object.world_z;
                spawned.rotation_x = object.rotation_x;
                spawned.rotation_y = object.rotation_y;
                spawned.rotation_z = object.rotation_z;
                spawned.health = read8(pc + 5U);
                spawned.attack_power = read8(pc + 6U);
                set_path_offset(child, read16(pc + 3U));
            }
            advance(7);
            continue;
        }
        case 123: case 124: case 125: case 126: {
            // Import/export targets are 16-bit direct-page variables in bank 7e.
            // The native-global memory bridge is added with the 65816 strategy VM;
            // retain the exact instruction boundary until then.
            unsupported_opcodes_.push_back(opcode);
            advance(4);
            continue;
        }
        case 127: case 128: {
            const auto offset = static_cast<std::uint8_t>(read16(pc + 1U));
            if (opcode == 127) {
                const auto value = signed_byte(objects_->read_path_byte(handle, offset));
                objects_->write_path_byte(handle, offset,
                    bits(static_cast<std::int8_t>(value / 2)));
            } else {
                const auto value = std::bit_cast<std::int16_t>(objects_->read_path_word(handle, offset));
                objects_->write_path_word(handle, offset,
                    std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(value / 2)));
            }
            advance(3);
            continue;
        }
        case 131: case 132:
            state.stack.push_back(opcode == 131
                ? objects_->read_path_byte(handle, read8(pc + 1U))
                : objects_->read_path_word(handle, read8(pc + 1U)));
            advance(2);
            continue;
        case 133: case 134: {
            if (state.stack.empty()) throw std::runtime_error{"PATH data stack underflow"};
            const auto value = state.stack.back(); state.stack.pop_back();
            if (opcode == 133) objects_->write_path_byte(handle, read8(pc + 1U), static_cast<std::uint8_t>(value));
            else objects_->write_path_word(handle, read8(pc + 1U), value);
            advance(2);
            continue;
        }
        case 138:
            state.stack.push_back(static_cast<std::uint16_t>(pc + 2U));
            state.stack.push_back(read8(pc + 1U));
            advance(2);
            continue;
        case 139: case 140:
            state.stack.push_back(static_cast<std::uint16_t>(pc + 2U));
            state.stack.push_back(opcode == 139
                ? objects_->read_path_byte(handle, read8(pc + 1U))
                : objects_->read_path_word(handle, read8(pc + 1U)));
            advance(2);
            continue;
        case 141: {
            const auto address = objects_->read_path_word(handle, read8(pc + 1U));
            state.stack.push_back(pc);
            jump(static_cast<std::uint16_t>(address - (paths_address_ & 0xffffU)));
            continue;
        }
        case 148:
            if (objects_->is_active(object.attached)) (void)objects_->remove(object.attached);
            advance(2);
            continue;
        case 149: case 150: case 151: case 152: {
            const auto offset = read8(pc + 1U);
            const auto zero = opcode == 149 || opcode == 151
                ? objects_->read_path_byte(handle, offset) == 0
                : objects_->read_path_word(handle, offset) == 0;
            const auto condition = opcode == 149 || opcode == 150 ? zero : !zero;
            if (condition) jump(read16(pc + 2U)); else advance(4);
            continue;
        }
        case 153: objects_->write_path_byte(handle, read8(pc + 1U), 0); advance(2); continue;
        case 154: objects_->write_path_word(handle, read8(pc + 1U), 0); advance(2); continue;
        case 155: case 157: {
            const auto offset = read8(pc + 1U);
            const auto delta = opcode == 155 ? 1U : static_cast<unsigned>(-1);
            objects_->write_path_byte(handle, offset,
                static_cast<std::uint8_t>(objects_->read_path_byte(handle, offset) + delta));
            advance(2);
            continue;
        }
        case 156: case 158: {
            const auto offset = read8(pc + 1U);
            const auto delta = opcode == 156 ? 1U : static_cast<unsigned>(-1);
            objects_->write_path_word(handle, offset,
                static_cast<std::uint16_t>(objects_->read_path_word(handle, offset) + delta));
            advance(2);
            continue;
        }
        case 159: object.rotation_x = static_cast<std::uint8_t>(object.rotation_x + read8(pc + 1U)); advance(2); continue;
        case 160: object.rotation_y = static_cast<std::uint8_t>(object.rotation_y + read8(pc + 1U)); advance(2); continue;
        case 161: object.rotation_z = static_cast<std::uint8_t>(object.rotation_z + read8(pc + 1U)); advance(2); continue;
        case 162: object.world_x = add16(object.world_x, signed_byte(read8(pc + 1U))); advance(2); continue;
        case 163: object.world_y = add16(object.world_y, signed_byte(read8(pc + 1U))); advance(2); continue;
        case 164: object.world_z = add16(object.world_z, signed_byte(read8(pc + 1U))); advance(2); continue;
        case 165: {
            const auto offset = read8(pc + 1U);
            objects_->write_path_word(handle, offset,
                static_cast<std::uint16_t>(objects_->read_path_word(handle, offset)
                    + signed_byte(read8(pc + 2U))));
            advance(3);
            continue;
        }
        case 166:
            advance(1);
            move(handle);
            return;
        default:
            unsupported_opcodes_.push_back(opcode);
            throw std::runtime_error{"unsupported PATH opcode " + std::to_string(opcode)};
        }
    }
    throw std::runtime_error{"PATH bytecode exceeded the per-tick operation limit"};
}

} // namespace starfox::simulation
