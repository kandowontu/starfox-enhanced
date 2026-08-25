#pragma once

#include "starfox/assets/rom.hpp"
#include "starfox/simulation/math.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/simulation/prng.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace starfox::simulation {

enum class PathEventKind : std::uint8_t {
    message,
    sound_effect,
    positional_sound,
    fire_weapon,
};

struct PathEvent {
    PathEventKind kind{};
    ObjectHandle source{};
    std::uint16_t value{};
};

// Interpreter for the compact PATH stream emitted by PATHMACS.ASM. Path
// offsets are the original 16-bit offsets from the ROM's `paths` symbol.
class PathVm {
public:
    PathVm(
        const assets::RomImage& rom,
        std::uint32_t paths_address,
        std::uint32_t path_strategy_address,
        ObjectPool& objects,
        TrigTables trig,
        OriginalPrng& random,
        std::uint32_t particle_strategy_address = 0,
        std::uint16_t null_shape = 0) noexcept;
    PathVm(
        const assets::RomImage& rom,
        const assets::SymbolMap& symbols,
        ObjectPool& objects,
        OriginalPrng& random);

    void set_player(ObjectHandle player);
    void set_view_velocity_z(std::int16_t velocity) noexcept { view_velocity_z_ = velocity; }
    void set_current_level(std::uint8_t level) noexcept { current_level_ = level; }
    void set_player_dead(bool dead) noexcept { player_dead_ = dead; }

    void attach(ObjectHandle object, std::uint16_t path_offset);
    void detach(ObjectHandle object) noexcept;
    void tick(ObjectHandle object);
    void tick_all();

    [[nodiscard]] bool is_attached(ObjectHandle object) const noexcept;
    [[nodiscard]] std::uint16_t path_offset(ObjectHandle object) const;
    [[nodiscard]] std::uint32_t path_strategy_address() const noexcept {
        return path_strategy_address_;
    }
    [[nodiscard]] const std::vector<PathEvent>& events() const noexcept { return events_; }
    void clear_events() noexcept { events_.clear(); }
    [[nodiscard]] const std::vector<std::uint8_t>& unsupported_opcodes() const noexcept {
        return unsupported_opcodes_;
    }

private:
    struct RuntimeState {
        struct Trigger {
            std::uint16_t path{};
            std::uint8_t condition{};
        };
        std::vector<std::uint16_t> stack;
        std::vector<Trigger> triggers;
        bool if_not{};
    };

    [[nodiscard]] std::uint8_t read8(std::uint16_t offset) const;
    [[nodiscard]] std::uint16_t read16(std::uint16_t offset) const;
    [[nodiscard]] std::int16_t read_i16(std::uint16_t offset) const;
    void set_path_offset(ObjectHandle object, std::uint16_t offset);
    void move(ObjectHandle object);
    void generate_vectors(GameObject& object) const noexcept;
    [[nodiscard]] ObjectHandle spawn(ObjectHandle parent, std::uint16_t shape);
    [[nodiscard]] std::uint8_t chase_byte(std::uint8_t current, std::uint8_t target) const noexcept;
    [[nodiscard]] std::uint16_t chase_word(std::uint16_t current, std::uint16_t target) const noexcept;

    const assets::RomImage* rom_{};
    std::uint32_t paths_address_{};
    std::uint32_t path_strategy_address_{};
    std::uint32_t particle_strategy_address_{};
    std::uint16_t null_shape_{};
    ObjectPool* objects_{};
    TrigTables trig_;
    OriginalPrng* random_{};
    ObjectHandle player_{};
    ObjectHandle pending_link_{};
    std::int16_t view_velocity_z_{};
    std::uint8_t current_level_{};
    bool player_dead_{};
    std::unordered_map<ObjectHandle, RuntimeState> states_;
    std::vector<PathEvent> events_;
    std::vector<std::uint8_t> unsupported_opcodes_;
};

} // namespace starfox::simulation
