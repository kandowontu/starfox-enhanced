#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/simulation/wdc65816.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace starfox::simulation {

struct StrategyEntry {
    std::uint32_t address{};
    std::uint8_t default_shape_id{};
};

class MapDatabase {
public:
    MapDatabase(
        const assets::RomImage& rom,
        std::uint32_t shapes_table,
        std::uint32_t strategies_table) noexcept;
    MapDatabase(const assets::RomImage& rom, const assets::SymbolMap& symbols);

    [[nodiscard]] std::uint16_t shape(std::uint8_t id) const;
    [[nodiscard]] StrategyEntry strategy(std::uint8_t id) const;

private:
    const assets::RomImage* rom_{};
    std::uint32_t shapes_table_{};
    std::uint32_t strategies_table_{};
};

class MapVm {
public:
    using ConditionHandler = std::function<bool(MapVm&)>;
    MapVm(
        const assets::RomImage& rom,
        MapDatabase database,
        ObjectPool& objects,
        const assets::SymbolMap* symbols = nullptr);

    void start(std::uint32_t address, ObjectHandle player);
    void set_player(ObjectHandle player);
    void advance_to_player_z(std::int16_t player_z);
    void advance_distance(std::int16_t distance);

    [[nodiscard]] std::uint32_t cursor() const noexcept { return cursor_; }
    [[nodiscard]] std::int16_t countdown() const noexcept { return countdown_; }
    [[nodiscard]] bool ended() const noexcept { return ended_; }
    [[nodiscard]] ObjectHandle last_spawned() const noexcept { return last_spawned_; }
    [[nodiscard]] std::uint8_t background_music() const noexcept { return background_music_; }
    [[nodiscard]] std::uint8_t other_music() const noexcept { return other_music_; }
    [[nodiscard]] std::uint16_t background() const noexcept { return background_; }
    [[nodiscard]] std::int8_t dots_mode() const noexcept;
    [[nodiscard]] bool vertical_offset_enabled() const noexcept { return vertical_offset_enabled_; }
    [[nodiscard]] bool horizontal_offset_enabled() const noexcept { return horizontal_offset_enabled_; }
    [[nodiscard]] bool z_rotation_enabled() const noexcept { return z_rotation_enabled_; }
    [[nodiscard]] bool screen_enabled() const noexcept { return screen_enabled_; }
    [[nodiscard]] std::int8_t fade_direction() const noexcept { return fade_direction_; }
    [[nodiscard]] std::uint8_t fade_value() const noexcept { return fade_value_; }
    [[nodiscard]] std::uint8_t display_brightness() const noexcept {
        return display_brightness_;
    }
    void set_display_brightness(std::uint8_t brightness);
    void start_display_fade(std::int8_t direction);
    [[nodiscard]] std::uint16_t stage_counter() const noexcept { return stage_counter_; }
    [[nodiscard]] bool background_request_pending() const noexcept {
        return background_request_pending_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& messages() const noexcept { return messages_; }
    void clear_messages() noexcept { messages_.clear(); }
    void tick_video_phase();
    void complete_background_request();
    // Import WORLD.ASM interpreter registers after an original routine such
    // as RESTART_L has advanced the native map directly.
    void restore_map_state_from_native();
    void write_native_byte(std::uint32_t address, std::uint8_t value);
    [[nodiscard]] std::uint8_t read_native_byte(std::uint32_t address) const noexcept;
    [[nodiscard]] std::uint16_t read_native_word(std::uint32_t address) const noexcept;
    void write_native_word(std::uint32_t address, std::uint16_t value);
    [[nodiscard]] std::vector<ApuPortWrite> take_apu_port_writes() {
        return cpu_.take_apu_port_writes();
    }
    void set_apu_clock_offset(std::uint32_t clocks) noexcept {
        cpu_.set_apu_clock_offset(clocks);
    }
    void set_apu_output_ports(
        const std::array<std::uint8_t, 4>& ports) noexcept {
        cpu_.set_apu_output_ports(ports);
    }
    [[nodiscard]] const SnesPpuState& ppu_state() const noexcept {
        return cpu_.ppu_state();
    }
    [[nodiscard]] const std::vector<std::uint32_t>& unknown_superfx_launches()
        const noexcept {
        return cpu_.unknown_superfx_launches();
    }
    [[nodiscard]] std::uint64_t apu_upload_generation() const noexcept {
        return cpu_.apu_upload_generation();
    }
    void write_cgram(
        std::uint16_t first_colour,
        std::span<const std::uint16_t> colours) noexcept {
        cpu_.write_cgram(first_colour, colours);
    }
    void write_vram(
        std::uint16_t byte_offset,
        std::span<const std::uint8_t> bytes) noexcept {
        cpu_.write_vram(byte_offset, bytes);
    }
    void set_bg1_scroll(std::int16_t x, std::int16_t y) noexcept {
        cpu_.set_bg1_scroll(x, y);
    }
    void draw_planet_sphere(std::uint16_t sprite) {
        cpu_.draw_planet_sphere(sprite);
    }
    void upload_oam(std::uint32_t source, std::size_t length) {
        cpu_.upload_oam(source, length);
    }
    void set_bg2_vertical_offsets_enabled(bool enabled) noexcept {
        cpu_.set_bg2_vertical_offsets_enabled(enabled);
    }
    void capture_bg2_horizontal_offsets(
        std::uint16_t source, bool enabled) noexcept {
        cpu_.capture_bg2_horizontal_offsets(source, enabled);
    }
    void register_condition(std::uint32_t address, ConditionHandler handler) {
        conditions_[address] = std::move(handler);
    }
    void set_unknown_condition_result(std::optional<bool> result) noexcept {
        unknown_condition_result_ = result;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& unsupported_controls() const noexcept {
        return unsupported_controls_;
    }
    std::size_t call_native_object_routine(
        std::uint32_t address,
        ObjectHandle object,
        std::uint8_t data_bank = 0x7e,
        std::uint8_t status = 0x24,
        std::size_t instruction_limit = 1'000'000);
    std::size_t call_native_routine(
        std::uint32_t address,
        Wdc65816Registers& registers,
        std::size_t instruction_limit = 1'000'000,
        bool service_transfer_flag = false);
    std::size_t call_native_near_routine(
        std::uint32_t address,
        Wdc65816Registers& registers,
        std::size_t instruction_limit = 1'000'000,
        bool service_transfer_flag = false);

private:
    void execute_ready_records();
    void spawn_table_object(std::uint8_t opcode);
    void spawn_direct_object();
    [[nodiscard]] ObjectHandle allocate_map_object();
    [[nodiscard]] std::uint32_t read_pointer24(std::uint32_t address) const;
    [[nodiscard]] std::uint32_t read_map_pointer(std::uint32_t address) const;
    [[nodiscard]] std::int16_t player_world_z() const noexcept;
    [[nodiscard]] std::uint32_t skip_inline_65816(std::uint32_t address) const;
    [[nodiscard]] std::uint16_t original_object_pointer(ObjectHandle handle) const noexcept;
    [[nodiscard]] static ObjectHandle native_object_handle(std::uint16_t pointer) noexcept;
    [[nodiscard]] ObjectHandle object_handle(std::uint16_t pointer) const noexcept;
    void sync_objects_to_cpu();
    void sync_objects_from_cpu();
    void execute_inline_65816();
    void execute_mapcode_jsl();
    [[nodiscard]] bool execute_native_condition(std::uint32_t address);
    void sync_map_state_to_cpu();
    void sync_display_from_cpu() noexcept;
    void sync_display_to_cpu();

    const assets::RomImage* rom_{};
    MapDatabase database_;
    ObjectPool* objects_{};
    ObjectHandle player_{};
    ObjectHandle last_spawned_{};
    std::uint32_t cursor_{};
    std::int16_t countdown_{};
    std::int16_t last_player_z_{};
    bool ended_{};
    std::uint8_t background_music_{};
    std::uint8_t other_music_{};
    std::uint16_t background_{};
    std::uint16_t stage_counter_{};
    std::int8_t dots_mode_{};
    std::int8_t fade_direction_{};
    std::uint8_t fade_value_{};
    std::uint8_t display_brightness_{15};
    std::uint8_t slow_fade_frame_{};
    bool slow_fade_frame_valid_{};
    bool vertical_offset_enabled_{};
    bool horizontal_offset_enabled_{};
    bool z_rotation_enabled_{};
    bool screen_enabled_{true};
    bool background_request_pending_{};
    std::vector<std::uint8_t> messages_;
    std::vector<std::uint32_t> call_stack_;
    std::unordered_map<std::uint32_t, std::uint16_t> loop_counters_;
    std::unordered_map<std::uint32_t, std::uint8_t> native_memory_;
    std::unordered_map<std::uint32_t, ConditionHandler> conditions_;
    std::optional<bool> unknown_condition_result_;
    std::vector<std::uint8_t> unsupported_controls_;
    Wdc65816 cpu_;
};

} // namespace starfox::simulation
