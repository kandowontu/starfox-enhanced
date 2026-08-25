#include "starfox/simulation/map_vm.hpp"

#include "starfox/simulation/math.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>
#include <string>

namespace starfox::simulation {
namespace {

constexpr std::uint16_t kOriginalObjectBase = 0x0338U;
constexpr std::uint16_t kOriginalObjectSize = 56U;
constexpr std::uint32_t kOriginalExtendedObjectBase = 0x7e2000U;
constexpr std::uint32_t kOriginalActiveList = 0x0012adU;
constexpr std::uint32_t kOriginalFreeList = 0x0012afU;
constexpr std::uint32_t kOriginalFadeDirection = 0x001930U;
constexpr std::uint32_t kOriginalFade = 0x001931U;
constexpr std::uint32_t kOriginalDisplay = 0x7e4655U;
constexpr std::uint32_t kOriginalBackgroundFlags = 0x001a16U;
constexpr std::uint32_t kOriginalBackgroundDmaList = 0x001764U;
constexpr std::uint32_t kOriginalCurrentBackground = 0x0017c6U;
constexpr std::uint32_t kOriginalMapCount = 0x001780U;
constexpr std::uint32_t kOriginalMapPointer = 0x001782U;
constexpr std::uint32_t kOriginalLastPlayerZ = 0x001784U;
constexpr std::uint32_t kOriginalMapJsrStack = 0x001788U;
constexpr std::uint32_t kOriginalMapJsrPointer = 0x0017b5U;
constexpr std::uint32_t kOriginalNumberMapJsrs = 0x0017b7U;
constexpr std::uint32_t kOriginalLastMapObject = 0x00177cU;
constexpr std::uint32_t kOriginalDotsFlag = 0x00177eU;
constexpr std::uint32_t kOriginalMapLoops = 0x0017c8U;
constexpr std::uint32_t kOriginalMapAddresses = 0x0017d0U;
constexpr std::uint32_t kOriginalNumberMapLoops = 0x0017d8U;
constexpr std::uint32_t kOriginalMapBank = 0x001af7U;

std::uint32_t rom_symbol(const assets::SymbolMap& symbols, const std::string& name) {
    for (const auto address : symbols.find(name)) {
        if ((address & 0xffffU) >= 0x8000U && ((address >> 16U) & 0xffU) < 0x7eU) {
            return address;
        }
    }
    throw std::runtime_error{"missing ROM symbol: " + name};
}

std::int8_t signed_byte(std::uint8_t value) noexcept {
    return std::bit_cast<std::int8_t>(value);
}

} // namespace

MapDatabase::MapDatabase(
    const assets::RomImage& rom,
    std::uint32_t shapes_table,
    std::uint32_t strategies_table) noexcept
    : rom_(&rom), shapes_table_(shapes_table), strategies_table_(strategies_table) {}

MapDatabase::MapDatabase(const assets::RomImage& rom, const assets::SymbolMap& symbols)
    : MapDatabase(rom, rom_symbol(symbols, "SHAPES"), rom_symbol(symbols, "ISTRATS")) {}

std::uint16_t MapDatabase::shape(std::uint8_t id) const {
    return rom_->read16(shapes_table_ + static_cast<std::uint32_t>(id) * 2U);
}

StrategyEntry MapDatabase::strategy(std::uint8_t id) const {
    const auto address = strategies_table_ + static_cast<std::uint32_t>(id) * 4U;
    return {
        static_cast<std::uint32_t>(rom_->read16(address))
            | (static_cast<std::uint32_t>(rom_->read8(address + 2U)) << 16U),
        rom_->read8(address + 3U),
    };
}

MapVm::MapVm(
    const assets::RomImage& rom,
    MapDatabase database,
    ObjectPool& objects,
    const assets::SymbolMap* symbols)
    : rom_(&rom), database_(database), objects_(&objects), cpu_(rom, symbols) {}

void MapVm::start(std::uint32_t address, ObjectHandle player) {
    if (player != 0 && !objects_->is_active(player)) {
        throw std::invalid_argument{"map player handle must be active"};
    }
    player_ = player;
    cursor_ = address;
    countdown_ = 0;
    last_player_z_ = player_world_z();
    last_spawned_ = 0;
    ended_ = false;
    background_request_pending_ = false;
    call_stack_.clear();
    loop_counters_.clear();
    unsupported_controls_.clear();
    sync_map_state_to_cpu();
}

std::int16_t MapVm::player_world_z() const noexcept {
    return objects_->is_active(player_) ? objects_->at(player_).world_z : 0;
}

std::uint8_t MapVm::read_native_byte(std::uint32_t address) const noexcept {
    return cpu_.read8(address);
}

std::uint16_t MapVm::read_native_word(std::uint32_t address) const noexcept {
    return cpu_.read16(address);
}

std::int8_t MapVm::dots_mode() const noexcept {
    // DOTSFLAG is also written by the original SETBGINFOREQ_L routine when
    // a background declares ground, space, or neither. Reading the shared
    // source byte keeps those native transitions visible to the host renderer
    // instead of observing only explicit map-stream override controls.
    return std::bit_cast<std::int8_t>(read_native_byte(kOriginalDotsFlag));
}

void MapVm::write_native_byte(std::uint32_t address, std::uint8_t value) {
    native_memory_[address] = value;
    cpu_.write8(address, value);
}

void MapVm::write_native_word(std::uint32_t address, std::uint16_t value) {
    write_native_byte(address, static_cast<std::uint8_t>(value));
    write_native_byte(address + 1U, static_cast<std::uint8_t>(value >> 8U));
}

void MapVm::sync_display_from_cpu() noexcept {
    fade_direction_ = std::bit_cast<std::int8_t>(cpu_.read8(kOriginalFadeDirection));
    fade_value_ = static_cast<std::uint8_t>(cpu_.read8(kOriginalFade) & 0x0fU);
    const auto display = cpu_.read8(kOriginalDisplay);
    screen_enabled_ = (display & 0x80U) == 0U;
    display_brightness_ = screen_enabled_
        ? static_cast<std::uint8_t>(display & 0x0fU) : 0U;
}

void MapVm::sync_display_to_cpu() {
    write_native_byte(kOriginalFadeDirection,
                      std::bit_cast<std::uint8_t>(fade_direction_));
    write_native_byte(kOriginalFade, fade_value_);
    display_brightness_ = screen_enabled_ ? fade_value_ : 0U;
    write_native_byte(kOriginalDisplay,
        screen_enabled_ ? display_brightness_ : 0x80U);
}

void MapVm::set_display_brightness(std::uint8_t brightness) {
    brightness &= 0x0fU;
    fade_direction_ = 0;
    fade_value_ = brightness;
    screen_enabled_ = true;
    sync_display_to_cpu();
    write_native_byte(0x002100U, brightness);
}

void MapVm::tick_video_phase() {
    if (fade_direction_ == 0) {
        return;
    }
    if (fade_direction_ < 0) {
        const auto steps = fade_direction_ == -2 ? 2U : 1U;
        if (fade_value_ <= steps) {
            fade_value_ = 0;
            fade_direction_ = 0;
            screen_enabled_ = false;
        } else {
            fade_value_ = static_cast<std::uint8_t>(fade_value_ - steps);
            screen_enabled_ = true;
        }
    } else if (fade_direction_ > 0) {
        // IRQ.ASM's quick fade-up path increments twice before falling into
        // the normal increment/store path, for three brightness steps total.
        const auto steps = fade_direction_ == 2 ? 3U : 1U;
        if (fade_value_ + steps >= 15U) {
            fade_value_ = 15;
            fade_direction_ = 0;
        } else {
            fade_value_ = static_cast<std::uint8_t>(fade_value_ + steps);
        }
        screen_enabled_ = true;
    }
    sync_display_to_cpu();
}

void MapVm::complete_background_request() {
    background_request_pending_ = false;
    // The original NMI-side mode-change code walks BG_DMALIST until its
    // terminator before WORLD.ASM's waitsetbg can advance. The PC renderer
    // does not DMA SNES character/tile data, so completion is represented by
    // the transfer-side background routine returning.
    write_native_word(kOriginalBackgroundDmaList, 0);
    if (!ended_ && rom_->read8(cursor_) == 100U) {
        ++cursor_;
        countdown_ = 0;
        execute_ready_records();
    }
    sync_map_state_to_cpu();
}

void MapVm::sync_map_state_to_cpu() {
    write_native_word(kOriginalMapCount, std::bit_cast<std::uint16_t>(countdown_));
    write_native_word(kOriginalMapPointer,
                      static_cast<std::uint16_t>(cursor_ & 0x7fffU));
    write_native_byte(kOriginalMapBank, static_cast<std::uint8_t>(cursor_ >> 16U));
    write_native_word(kOriginalLastPlayerZ,
                      std::bit_cast<std::uint16_t>(last_player_z_));
    write_native_word(kOriginalLastMapObject,
                      original_object_pointer(last_spawned_));

    for (std::uint32_t offset = 0; offset < 15U * 3U; ++offset) {
        write_native_byte(kOriginalMapJsrStack + offset, 0);
    }
    const auto jsr_count = std::min<std::size_t>(call_stack_.size(), 15U);
    for (std::size_t index = 0; index < jsr_count; ++index) {
        const auto return_address = call_stack_[index];
        const auto call_address = return_address - 4U;
        const auto offset = static_cast<std::uint32_t>(index * 3U);
        write_native_word(kOriginalMapJsrStack + offset,
                          static_cast<std::uint16_t>(call_address & 0x7fffU));
        write_native_byte(kOriginalMapJsrStack + offset + 2U,
                          static_cast<std::uint8_t>(call_address >> 16U));
    }
    write_native_word(kOriginalMapJsrPointer,
                      static_cast<std::uint16_t>(jsr_count * 3U));
    write_native_word(kOriginalNumberMapJsrs,
                      static_cast<std::uint16_t>(jsr_count));

    for (std::uint32_t offset = 0; offset < 8U; ++offset) {
        write_native_byte(kOriginalMapAddresses + offset, 0);
        write_native_byte(kOriginalMapLoops + offset, 0);
    }
    std::size_t loop_index = 0;
    for (const auto& [address, count] : loop_counters_) {
        if (loop_index == 4U) break;
        const auto offset = static_cast<std::uint32_t>(loop_index * 2U);
        write_native_word(kOriginalMapAddresses + offset,
                          static_cast<std::uint16_t>(address & 0x7fffU));
        write_native_word(kOriginalMapLoops + offset, count);
        ++loop_index;
    }
    write_native_word(kOriginalNumberMapLoops,
                      static_cast<std::uint16_t>(loop_index * 2U));
    write_native_word(kOriginalCurrentBackground, background_);
}

void MapVm::restore_map_state_from_native() {
    cursor_ = (static_cast<std::uint32_t>(read_native_byte(kOriginalMapBank)) << 16U)
        | 0x8000U | (read_native_word(kOriginalMapPointer) & 0x7fffU);
    countdown_ = std::bit_cast<std::int16_t>(read_native_word(kOriginalMapCount));
    last_player_z_ = std::bit_cast<std::int16_t>(
        read_native_word(kOriginalLastPlayerZ));
    last_spawned_ = object_handle(read_native_word(kOriginalLastMapObject));
    if (!objects_->is_active(last_spawned_)) last_spawned_ = 0;
    background_ = read_native_word(kOriginalCurrentBackground);
    background_request_pending_ =
        (read_native_byte(kOriginalBackgroundFlags) & 4U) != 0U;
    ended_ = rom_->read8(cursor_) == 2U;

    call_stack_.clear();
    const auto jsr_bytes = std::min<std::uint16_t>(
        read_native_word(kOriginalMapJsrPointer), 15U * 3U);
    for (std::uint16_t offset = 0; offset + 2U < jsr_bytes; offset += 3U) {
        const auto call_address =
            (static_cast<std::uint32_t>(read_native_byte(
                 kOriginalMapJsrStack + offset + 2U)) << 16U)
            | 0x8000U
            | (read_native_word(kOriginalMapJsrStack + offset) & 0x7fffU);
        call_stack_.push_back(call_address + 4U);
    }

    loop_counters_.clear();
    const auto loop_bytes = std::min<std::uint16_t>(
        read_native_word(kOriginalNumberMapLoops), 8U);
    const auto bank = cursor_ & 0xff0000U;
    for (std::uint16_t offset = 0; offset + 1U < loop_bytes; offset += 2U) {
        const auto address = read_native_word(kOriginalMapAddresses + offset);
        if (address == 0) continue;
        loop_counters_[bank | 0x8000U | (address & 0x7fffU)] =
            read_native_word(kOriginalMapLoops + offset);
    }
}

void MapVm::set_player(ObjectHandle player) {
    if (player != 0 && !objects_->is_active(player)) {
        throw std::invalid_argument{"map player handle must be active"};
    }
    player_ = player;
}

std::size_t MapVm::call_native_object_routine(
    std::uint32_t address,
    ObjectHandle object,
    std::uint8_t data_bank,
    std::uint8_t status,
    std::size_t instruction_limit) {
    if (!objects_->is_active(object)) {
        throw std::invalid_argument{"native routine object handle must be active"};
    }
    sync_objects_to_cpu();
    Wdc65816Registers registers;
    registers.x = original_object_pointer(object);
    registers.data_bank = data_bank;
    registers.status = status;
    const auto instructions = cpu_.call_long(address, registers, instruction_limit);
    sync_objects_from_cpu();
    sync_display_from_cpu();
    return instructions;
}

std::size_t MapVm::call_native_routine(
    std::uint32_t address,
    Wdc65816Registers& registers,
    std::size_t instruction_limit,
    bool service_transfer_flag) {
    sync_objects_to_cpu();
    const auto instructions = cpu_.call_long(
        address, registers, instruction_limit, service_transfer_flag);
    sync_objects_from_cpu();
    sync_display_from_cpu();
    return instructions;
}

std::size_t MapVm::call_native_near_routine(
    std::uint32_t address,
    Wdc65816Registers& registers,
    std::size_t instruction_limit,
    bool service_transfer_flag) {
    sync_objects_to_cpu();
    const auto instructions = cpu_.call_near(
        address, registers, instruction_limit, service_transfer_flag);
    sync_objects_from_cpu();
    sync_display_from_cpu();
    return instructions;
}

void MapVm::advance_to_player_z(std::int16_t player_z) {
    const auto distance = subtract16(player_z, last_player_z_);
    last_player_z_ = player_z;
    advance_distance(distance);
}

void MapVm::advance_distance(std::int16_t distance) {
    if (ended_) {
        return;
    }
    countdown_ = subtract16(countdown_, distance);
    if (countdown_ < 0) {
        execute_ready_records();
    }
    sync_map_state_to_cpu();
}

std::uint32_t MapVm::read_pointer24(std::uint32_t address) const {
    return static_cast<std::uint32_t>(rom_->read16(address))
        | (static_cast<std::uint32_t>(rom_->read8(address + 2U)) << 16U);
}

std::uint32_t MapVm::read_map_pointer(std::uint32_t address) const {
    return (static_cast<std::uint32_t>(rom_->read8(address + 2U)) << 16U)
        | 0x8000U | (rom_->read16(address) & 0x7fffU);
}

std::uint32_t MapVm::skip_inline_65816(std::uint32_t address) const {
    // MAPMACS end_65816 emits `LDX #next_map_offset ; RTL`. Prefer only the
    // self-verifying fall-through form here: the encoded destination must be
    // the byte immediately following RTL. Conditional `switch` sequences are
    // left to the future CPU bridge rather than guessed.
    const auto bank = address & 0xff0000U;
    for (auto candidate = address + 1U;
         (candidate & 0xff0000U) == bank && (candidate & 0xffffU) <= 0xfffcU;
         ++candidate) {
        if (rom_->read8(candidate) != 0xa2U || rom_->read8(candidate + 3U) != 0x6bU) {
            continue;
        }
        const auto target = bank | 0x8000U | (rom_->read16(candidate + 1U) & 0x7fffU);
        if (target == candidate + 4U) return target;
    }
    throw std::runtime_error{"could not find end_65816 boundary in map stream"};
}

std::uint16_t MapVm::original_object_pointer(ObjectHandle handle) const noexcept {
    if (handle == 0 || !objects_->is_active(handle)) {
        return handle;
    }
    return static_cast<std::uint16_t>(
        kOriginalObjectBase + static_cast<std::uint16_t>(handle - 1U) * kOriginalObjectSize);
}

ObjectHandle MapVm::object_handle(std::uint16_t pointer) const noexcept {
    const auto handle = native_object_handle(pointer);
    return objects_->is_active(handle) ? handle : pointer;
}

ObjectHandle MapVm::native_object_handle(std::uint16_t pointer) noexcept {
    if (pointer < kOriginalObjectBase) {
        return 0;
    }
    const auto displacement = static_cast<std::uint16_t>(pointer - kOriginalObjectBase);
    if (displacement % kOriginalObjectSize != 0) {
        return 0;
    }
    const auto handle = static_cast<ObjectHandle>(displacement / kOriginalObjectSize + 1U);
    return handle <= kMaximumObjects ? handle : 0;
}

void MapVm::sync_objects_to_cpu() {
    const auto active = objects_->active_handles();
    const auto free = objects_->free_handles();
    cpu_.write16(kOriginalActiveList,
                 active.empty() ? 0U : original_object_pointer(active.front()));
    cpu_.write16(kOriginalFreeList,
                 free.empty() ? 0U : static_cast<std::uint16_t>(
                     kOriginalObjectBase + (free.front() - 1U) * kOriginalObjectSize));
    for (std::size_t index = 0; index < active.size(); ++index) {
        const auto handle = active[index];
        const auto base = static_cast<std::uint32_t>(original_object_pointer(handle));
        const auto extended_base = kOriginalExtendedObjectBase
            + static_cast<std::uint32_t>(handle - 1U) * kOriginalObjectSize;
        cpu_.write16(base, index + 1U < active.size()
            ? original_object_pointer(active[index + 1U]) : 0U);
        cpu_.write16(base + 2U, index != 0
            ? original_object_pointer(active[index - 1U]) : 0U);
        for (std::uint16_t offset = 4; offset < kOriginalObjectSize; ++offset) {
            cpu_.write8(base + offset, objects_->read_base_byte(handle, offset));
        }
        const auto& object = objects_->at(handle);
        cpu_.write16(base + 6U, original_object_pointer(object.attached));
        cpu_.write16(base + 25U, original_object_pointer(object.immune_object));
        cpu_.write16(base + 27U, original_object_pointer(object.collision_object));
        for (std::size_t offset = 0; offset < kExtendedObjectBytes; ++offset) {
            cpu_.write8(extended_base + offset, object.extended[offset]);
        }
        cpu_.write16(extended_base + 19U, original_object_pointer(object.fire_object));
    }
    for (std::size_t index = 0; index < free.size(); ++index) {
        const auto base = static_cast<std::uint32_t>(
            kOriginalObjectBase + (free[index] - 1U) * kOriginalObjectSize);
        const auto next = index + 1U == free.size() ? 0U : static_cast<std::uint16_t>(
            kOriginalObjectBase + (free[index + 1U] - 1U) * kOriginalObjectSize);
        cpu_.write16(base, next);
    }
}

void MapVm::sync_objects_from_cpu() {
    const auto read_list = [this](std::uint16_t pointer) {
        std::vector<ObjectHandle> result;
        std::array<bool, kMaximumObjects + 1> seen{};
        while (pointer != 0) {
            const auto handle = native_object_handle(pointer);
            if (handle == 0 || seen[handle]) {
                throw std::runtime_error{"native 65C816 produced an invalid object list"};
            }
            seen[handle] = true;
            result.push_back(handle);
            pointer = cpu_.read16(pointer);
        }
        return result;
    };
    auto active = read_list(cpu_.read16(kOriginalActiveList));
    auto free = read_list(cpu_.read16(kOriginalFreeList));
    if (active.size() + free.size() != kMaximumObjects) {
        throw std::runtime_error{"native active/free lists do not cover the object pool"};
    }
    objects_->restore_lists(active, free);
    for (const auto handle : objects_->active_handles()) {
        const auto base = static_cast<std::uint32_t>(original_object_pointer(handle));
        const auto extended_base = kOriginalExtendedObjectBase
            + static_cast<std::uint32_t>(handle - 1U) * kOriginalObjectSize;
        for (std::uint16_t offset = 4; offset < kOriginalObjectSize; ++offset) {
            objects_->write_base_byte(handle, offset, cpu_.read8(base + offset));
        }
        auto& object = objects_->at(handle);
        object.attached = object_handle(cpu_.read16(base + 6U));
        object.immune_object = object_handle(cpu_.read16(base + 25U));
        object.collision_object = object_handle(cpu_.read16(base + 27U));
        for (std::size_t offset = 0; offset < kExtendedObjectBytes; ++offset) {
            objects_->write_path_byte(handle, static_cast<std::uint8_t>(0x80U + offset),
                                      cpu_.read8(extended_base + offset));
        }
        object.fire_object = object_handle(cpu_.read16(extended_base + 19U));
    }
}

void MapVm::execute_inline_65816() {
    sync_map_state_to_cpu();
    sync_objects_to_cpu();
    Wdc65816Registers registers;
    registers.x = original_object_pointer(last_spawned_);
    registers.data_bank = static_cast<std::uint8_t>(cursor_ >> 16U);
    registers.status = 0x24U; // native A8/I16, matching WORLD.ASM map65816
    cpu_.call_long(cursor_ + 1U, registers);
    sync_objects_from_cpu();
    sync_display_from_cpu();
    cursor_ = (cursor_ & 0xff0000U) | 0x8000U | (registers.x & 0x7fffU);
}

void MapVm::execute_mapcode_jsl() {
    sync_map_state_to_cpu();
    sync_objects_to_cpu();
    Wdc65816Registers registers;
    registers.x = original_object_pointer(last_spawned_);
    registers.status = 0x24U; // mapcodejsl also enters its target as A8/I16
    const auto encoded = read_pointer24(cursor_ + 1U);
    const auto target = (encoded & 0xff0000U)
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(encoded) + 1U);
    cpu_.call_long(target, registers);
    sync_objects_from_cpu();
    sync_display_from_cpu();
    cursor_ += 4U;
}

bool MapVm::execute_native_condition(std::uint32_t address) {
    sync_map_state_to_cpu();
    sync_objects_to_cpu();
    Wdc65816Registers registers;
    registers.x = static_cast<std::uint16_t>(cursor_ & 0x7fffU);
    registers.status = 0x24U;
    cpu_.call_long(address, registers);
    sync_objects_from_cpu();
    sync_display_from_cpu();
    return (registers.status & 0x01U) != 0;
}

ObjectHandle MapVm::allocate_map_object() {
    const auto handle = objects_->allocate_after(objects_->first_active());
    last_spawned_ = handle;
    return handle;
}

void MapVm::spawn_table_object(std::uint8_t opcode) {
    const auto handle = allocate_map_object();
    if (opcode == 0) {
        countdown_ = rom_->read_i16(cursor_ + 1U);
        if (handle != 0) {
            auto& object = objects_->at(handle);
            object.world_x = rom_->read_i16(cursor_ + 3U);
            object.world_y = rom_->read_i16(cursor_ + 5U);
            object.world_z = add16(player_world_z(), rom_->read_i16(cursor_ + 7U));
            object.shape = database_.shape(rom_->read8(cursor_ + 9U));
            object.strategy_address = database_.strategy(rom_->read8(cursor_ + 10U)).address;
        }
        cursor_ += 11U;
        return;
    }

    if (opcode == 112 || opcode == 118) {
        countdown_ = static_cast<std::int16_t>(rom_->read8(cursor_ + 1U) << 4U);
        const auto strategy_offset = opcode == 112 ? 6U : 5U;
        const auto strategy = database_.strategy(rom_->read8(cursor_ + strategy_offset));
        if (handle != 0) {
            auto& object = objects_->at(handle);
            object.world_x = static_cast<std::int16_t>(signed_byte(rom_->read8(cursor_ + 2U)) * 4);
            object.world_y = static_cast<std::int16_t>(signed_byte(rom_->read8(cursor_ + 3U)) * 4);
            object.world_z = add16(player_world_z(),
                static_cast<std::int16_t>(rom_->read8(cursor_ + 4U) << 4U));
            object.shape = database_.shape(opcode == 112
                ? rom_->read8(cursor_ + 5U)
                : strategy.default_shape_id);
            object.strategy_address = strategy.address;
        }
        cursor_ += opcode == 112 ? 7U : 6U;
        return;
    }

    if (opcode == 116) {
        countdown_ = rom_->read_i16(cursor_ + 1U);
        const auto strategy = database_.strategy(rom_->read8(cursor_ + 9U));
        if (handle != 0) {
            auto& object = objects_->at(handle);
            object.world_x = rom_->read_i16(cursor_ + 3U);
            object.world_y = rom_->read_i16(cursor_ + 5U);
            object.world_z = add16(player_world_z(), rom_->read_i16(cursor_ + 7U));
            object.shape = database_.shape(strategy.default_shape_id);
            object.strategy_address = strategy.address;
        }
        cursor_ += 10U;
        return;
    }
    throw std::runtime_error{"invalid table-object map opcode"};
}

void MapVm::spawn_direct_object() {
    const auto handle = allocate_map_object();
    countdown_ = rom_->read_i16(cursor_ + 1U);
    if (handle != 0) {
        auto& object = objects_->at(handle);
        object.world_x = rom_->read_i16(cursor_ + 3U);
        object.world_y = rom_->read_i16(cursor_ + 5U);
        object.world_z = add16(player_world_z(), rom_->read_i16(cursor_ + 7U));
        object.shape = rom_->read16(cursor_ + 9U);
        object.strategy_address = read_pointer24(cursor_ + 11U);
    }
    cursor_ += 14U;
}

void MapVm::execute_ready_records() {
    for (std::size_t operations = 0; operations < 65'536; ++operations) {
        const auto opcode = rom_->read8(cursor_);
        if (opcode == 0 || opcode == 112 || opcode == 116 || opcode == 118) {
            spawn_table_object(opcode);
            if (countdown_ != 0) return;
            continue;
        }
        if (opcode == 134) {
            spawn_direct_object();
            if (countdown_ != 0) return;
            continue;
        }
        if (opcode == 2) {
            ended_ = true;
            return;
        }
        if (opcode == 4) {
            const auto instruction = cursor_;
            const auto target = (cursor_ & 0xff0000U) | 0x8000U
                | (rom_->read16(cursor_ + 1U) & 0x7fffU);
            const auto initial = rom_->read16(cursor_ + 3U);
            auto [entry, inserted] = loop_counters_.try_emplace(instruction, initial);
            if (inserted || entry->second > 1U) {
                if (!inserted) {
                    --entry->second;
                }
                cursor_ = target;
            } else {
                loop_counters_.erase(entry);
                cursor_ += 5U;
            }
            continue;
        }
        if (opcode == 18) {
            countdown_ = rom_->read_i16(cursor_ + 1U);
            cursor_ += 3U;
            if (countdown_ != 0) return;
            continue;
        }
        if (opcode == 138) {
            countdown_ = static_cast<std::int16_t>(rom_->read8(cursor_ + 1U) << 4U);
            cursor_ += 2U;
            return;
        }
        if (opcode == 20) {
            background_music_ = rom_->read8(cursor_ + 1U);
            cursor_ += 2U;
            continue;
        }
        if (opcode == 6 || opcode == 8) {
            cursor_ += 1U;
            continue;
        }
        if (opcode == 10) {
            const auto handle = allocate_map_object();
            countdown_ = rom_->read_i16(cursor_ + 1U);
            if (handle != 0) {
                auto& object = objects_->at(handle);
                object.world_x = rom_->read_i16(cursor_ + 3U);
                object.world_y = rom_->read_i16(cursor_ + 5U);
                object.world_z = add16(player_world_z(), rom_->read_i16(cursor_ + 7U));
                object.shape = rom_->read16(cursor_ + 9U);
                object.strategy_address = read_pointer24(cursor_ + 11U);
                object.attached = rom_->read16(cursor_ + 14U);
                object.type = 8;
            }
            cursor_ += 16U;
            if (countdown_ != 0) return;
            continue;
        }
        if (opcode == 12) {
            const auto shape = rom_->read16(cursor_ + 3U);
            for (const auto handle : objects_->active_handles()) {
                if (handle != player_ && objects_->at(handle).shape == shape) {
                    (void)objects_->remove(handle);
                }
            }
            cursor_ += 5U;
            continue;
        }
        if (opcode == 14) {
            stage_counter_ = 50;
            cursor_ += 1U;
            continue;
        }
        if (opcode == 16) {
            background_ = rom_->read16(cursor_ + 1U);
            write_native_word(kOriginalCurrentBackground, background_);
            write_native_byte(kOriginalBackgroundFlags,
                static_cast<std::uint8_t>(read_native_byte(kOriginalBackgroundFlags) | 4U));
            background_request_pending_ = true;
            cursor_ += 3U;
            continue;
        }
        if (opcode == 22 || opcode == 24 || opcode == 26) {
            dots_mode_ = opcode == 22 ? 0 : opcode == 24 ? 1 : -1;
            write_native_word(kOriginalDotsFlag,
                static_cast<std::uint16_t>(static_cast<std::int16_t>(dots_mode_)));
            cursor_ += 1U;
            continue;
        }
        if (opcode == 28) {
            other_music_ = rom_->read8(cursor_ + 1U);
            cursor_ += 2U;
            continue;
        }
        if (opcode == 30 || opcode == 32 || opcode == 34 || opcode == 36) {
            if (opcode == 30) vertical_offset_enabled_ = true;
            if (opcode == 32) vertical_offset_enabled_ = false;
            if (opcode == 34) horizontal_offset_enabled_ = true;
            if (opcode == 36) horizontal_offset_enabled_ = false;
            cursor_ += 1U;
            continue;
        }
        if (opcode == 38) {
            spawn_table_object(0);
            if (last_spawned_ != 0 && objects_->is_active(last_spawned_)) {
                objects_->at(last_spawned_).rotation_z = rom_->read8(cursor_);
            }
            // mapobjzrot is one byte longer than a normal table object.
            cursor_ += 1U;
            if (countdown_ != 0) return;
            continue;
        }
        if (opcode == 40) {
            call_stack_.push_back(cursor_ + 4U);
            cursor_ = read_map_pointer(cursor_ + 1U);
            continue;
        }
        if (opcode == 44) {
            const auto condition_address = read_pointer24(cursor_ + 1U);
            bool take{};
            const auto condition = conditions_.find(condition_address);
            if (condition != conditions_.end()) {
                take = condition->second(*this);
            } else if (unknown_condition_result_.has_value()) {
                take = *unknown_condition_result_;
            } else {
                take = execute_native_condition(condition_address);
            }
            if (take) {
                cursor_ = (cursor_ & 0xff0000U) | 0x8000U
                    | (rom_->read16(cursor_ + 4U) & 0x7fffU);
                continue;
            }
            cursor_ += 6U;
            countdown_ = 1;
            return;
        }
        if (opcode == 42) {
            if (call_stack_.empty()) {
                ended_ = true;
                return;
            }
            cursor_ = call_stack_.back();
            call_stack_.pop_back();
            continue;
        }
        if (opcode == 46) {
            cursor_ = read_map_pointer(cursor_ + 1U);
            continue;
        }
        if (opcode == 48 || opcode == 50 || opcode == 52) {
            if (last_spawned_ != 0 && objects_->is_active(last_spawned_)) {
                auto& object = objects_->at(last_spawned_);
                const auto value = rom_->read8(cursor_ + 1U);
                if (opcode == 48) object.rotation_x = value;
                if (opcode == 50) object.rotation_y = value;
                if (opcode == 52) object.rotation_z = value;
            }
            cursor_ += 2U;
            continue;
        }
        if (opcode == 54 || opcode == 56 || opcode == 58) {
            if (last_spawned_ != 0 && objects_->is_active(last_spawned_)) {
                const auto offset = rom_->read16(cursor_ + 1U);
                if (opcode == 54) objects_->write_base_byte(last_spawned_, offset, rom_->read8(cursor_ + 3U));
                if (opcode == 56) objects_->write_base_word(last_spawned_, offset, rom_->read16(cursor_ + 3U));
                if (opcode == 58) objects_->write_base_long(last_spawned_, offset, read_pointer24(cursor_ + 3U));
            }
            cursor_ += opcode == 54 ? 4U : opcode == 56 ? 5U : 6U;
            continue;
        }
        if (opcode == 60 || opcode == 62 || opcode == 64) {
            if (last_spawned_ != 0 && objects_->is_active(last_spawned_)) {
                // MAPMACS emits alx_field-xalblks. The 65816 then adds the
                // object's alblks address before indexing xalblks, so remove
                // the base-array displacement (ALBLKS=$0338) here.
                const auto extended_index = static_cast<std::uint16_t>(
                    rom_->read16(cursor_ + 1U) + 0x0338U);
                if (extended_index >= kExtendedObjectBytes) {
                    throw std::runtime_error{"map ALX offset is outside alx_size"};
                }
                const auto encoded_offset = static_cast<std::uint8_t>(
                    0x80U | static_cast<std::uint8_t>(extended_index));
                if (opcode == 60) {
                    objects_->write_path_byte(last_spawned_, encoded_offset,
                                              rom_->read8(cursor_ + 3U));
                } else if (opcode == 62) {
                    objects_->write_path_word(last_spawned_, encoded_offset,
                                              rom_->read16(cursor_ + 3U));
                } else {
                    objects_->write_path_word(last_spawned_, encoded_offset,
                                              rom_->read16(cursor_ + 3U));
                    objects_->write_path_byte(last_spawned_,
                        static_cast<std::uint8_t>(encoded_offset + 2U),
                        rom_->read8(cursor_ + 5U));
                }
            }
            cursor_ += opcode == 60 ? 4U : opcode == 62 ? 5U : 6U;
            continue;
        }
        if (opcode == 66 || opcode == 68 || opcode == 78 || opcode == 80) {
            fade_direction_ = opcode == 66 ? 1 : opcode == 68 ? -1 : opcode == 78 ? 2 : -2;
            write_native_byte(kOriginalFadeDirection,
                std::bit_cast<std::uint8_t>(fade_direction_));
            cursor_ += 1U;
            continue;
        }
        if (opcode == 70 || opcode == 72 || opcode == 104 || opcode == 106) {
            if (last_spawned_ != 0 && objects_->is_active(last_spawned_)) {
                const auto offset = rom_->read16(cursor_ + 1U);
                const auto address = read_pointer24(cursor_ + 3U);
                const auto value = opcode == 70 || opcode == 104
                    ? static_cast<std::uint16_t>(read_native_byte(address))
                    : static_cast<std::uint16_t>(read_native_byte(address))
                        | (static_cast<std::uint16_t>(read_native_byte(address + 1U)) << 8U);
                if (opcode == 70) objects_->write_base_byte(last_spawned_, offset, static_cast<std::uint8_t>(value));
                if (opcode == 72) objects_->write_base_word(last_spawned_, offset, value);
                if (opcode == 104) objects_->write_base_byte(last_spawned_, offset,
                    static_cast<std::uint8_t>(objects_->read_base_byte(last_spawned_, offset) + value));
                if (opcode == 106) objects_->write_base_word(last_spawned_, offset,
                    static_cast<std::uint16_t>(objects_->read_base_word(last_spawned_, offset) + value));
            }
            cursor_ += 6U;
            continue;
        }
        if (opcode == 74) {
            if (last_spawned_ != 0 && objects_->is_active(last_spawned_)) {
                const auto address = read_pointer24(cursor_ + 1U);
                const auto pointer = original_object_pointer(last_spawned_);
                write_native_byte(address, static_cast<std::uint8_t>(pointer));
                write_native_byte(address + 1U, static_cast<std::uint8_t>(pointer >> 8U));
            }
            cursor_ += 4U;
            continue;
        }
        if (opcode == 76) {
            if (fade_direction_ == 0 && !screen_enabled_) {
                cursor_ += 1U;
                continue;
            }
            countdown_ = 1;
            return;
        }
        if (opcode == 82 || opcode == 84) {
            screen_enabled_ = opcode == 84;
            fade_direction_ = 0;
            fade_value_ = opcode == 84 ? 15U : 0U;
            sync_display_to_cpu();
            cursor_ += 1U;
            continue;
        }
        if (opcode == 86 || opcode == 88) {
            z_rotation_enabled_ = opcode == 88;
            cursor_ += 1U;
            continue;
        }
        if (opcode == 90 || opcode == 132) {
            if (last_spawned_ != 0 && objects_->is_active(last_spawned_)) {
                auto& flags = objects_->at(last_spawned_).strategy_flags;
                if (opcode == 90) flags[0] |= 1U;
                else flags[3] |= 0x80U;
            }
            cursor_ += 1U;
            continue;
        }
        if (opcode == 92 || opcode == 94) {
            const auto width = opcode == 92 ? 1U : 2U;
            const auto address = read_pointer24(cursor_ + 1U + width);
            for (std::uint32_t byte = 0; byte < width; ++byte) {
                write_native_byte(address + byte, rom_->read8(cursor_ + 1U + byte));
            }
            cursor_ += 1U + width + 3U;
            continue;
        }
        if (opcode == 96) {
            const auto address = read_pointer24(cursor_ + 4U);
            write_native_byte(address, rom_->read8(cursor_ + 1U));
            write_native_byte(address + 1U, rom_->read8(cursor_ + 2U));
            write_native_byte(address + 2U, rom_->read8(cursor_ + 3U));
            cursor_ += 7U;
            continue;
        }
        if (opcode == 98) {
            background_ = rom_->read16(cursor_ + 2U);
            write_native_word(kOriginalCurrentBackground, background_);
            write_native_word(kOriginalBackgroundDmaList, background_);
            cursor_ += 4U;
            continue;
        }
        if (opcode == 100) {
            if (background_request_pending_) {
                countdown_ = 1;
                return;
            }
            cursor_ += 1U;
            continue;
        }
        if (opcode == 102) {
            write_native_byte(kOriginalBackgroundFlags,
                static_cast<std::uint8_t>(read_native_byte(kOriginalBackgroundFlags) | 8U));
            cursor_ += 1U;
            continue;
        }
        if (opcode == 108 || opcode == 110) {
            cursor_ += 1U;
            continue;
        }
        if (opcode == 124 || opcode == 126 || opcode == 128) {
            const auto address = read_pointer24(cursor_ + 1U);
            const auto left = read_native_byte(address);
            const auto right = rom_->read8(cursor_ + 4U);
            const auto difference = std::bit_cast<std::int8_t>(
                static_cast<std::uint8_t>(left - right));
            const auto take = opcode == 124 ? difference < 0
                : opcode == 126 ? difference > 0
                : difference == 0;
            if (take) {
                cursor_ = (cursor_ & 0xff0000U) | 0x8000U
                    | (rom_->read16(cursor_ + 5U) & 0x7fffU);
            } else {
                cursor_ += 7U;
            }
            continue;
        }
        if (opcode == 120) {
            execute_inline_65816();
            continue;
        }
        if (opcode == 122) {
            execute_mapcode_jsl();
            continue;
        }
        if (opcode == 130) {
            messages_.push_back(rom_->read8(cursor_ + 1U));
            cursor_ += 2U;
            continue;
        }
        if (opcode == 140) {
            if (last_spawned_ != 0 && objects_->is_active(last_spawned_)) {
                objects_->at(last_spawned_).scratch_words[1] = rom_->read_i16(cursor_ + 1U);
            }
            cursor_ += 3U;
            continue;
        }

        const auto fixed_size = [opcode]() -> std::uint32_t {
            switch (opcode) {
            case 136: return 2;
            case 28: case 130: return 2;
            default: return 0;
            }
        }();
        if (fixed_size != 0) {
            unsupported_controls_.push_back(opcode);
            cursor_ += fixed_size;
            continue;
        }
        throw std::runtime_error{"unsupported map control " + std::to_string(opcode)};
    }
    throw std::runtime_error{"map bytecode exceeded the per-update operation limit"};
}

} // namespace starfox::simulation
