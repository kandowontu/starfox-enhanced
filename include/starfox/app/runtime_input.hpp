#pragma once

#include "starfox/input/input_latch.hpp"
#include "starfox/render/hud_layout.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace starfox::app {

// Installs controller-driver defaults before SDL_INIT_GAMEPAD. Explicit user
// or environment overrides retain priority over these application defaults.
void configure_native_gamepad_support() noexcept;

// Opens the most useful player controller when more than one mapped device is
// present (Steam virtual/Deck first, then XInput/Xbox, then generic gamepads).
[[nodiscard]] SDL_Gamepad* open_preferred_gamepad() noexcept;
[[nodiscard]] std::vector<SDL_Gamepad*> open_player_gamepads(
    std::size_t maximum = 5U) noexcept;

[[nodiscard]] std::string gamepad_device_label(SDL_Gamepad* gamepad);

enum class BindingDevice : std::uint8_t {
    keyboard,
    gamepad,
};

enum class GamepadBindingKind : std::uint8_t {
    button,
    axis_negative,
    axis_positive,
};

struct GamepadBinding {
    GamepadBindingKind kind{GamepadBindingKind::button};
    std::int16_t control{};
};

class InputBindings {
public:
    static constexpr std::size_t action_count = 12U;

    InputBindings();

    [[nodiscard]] input::ButtonMask sample(
        SDL_Gamepad* gamepad) const noexcept;
    [[nodiscard]] input::ButtonMask sample_gamepad_only(
        SDL_Gamepad* gamepad) const noexcept;
    [[nodiscard]] input::ButtonMask sample_fixed_menu_navigation(
        SDL_Gamepad* gamepad) const noexcept;

    void bind_keyboard(std::size_t action, SDL_Scancode scancode) noexcept;
    void bind_gamepad_button(
        std::size_t action, SDL_GamepadButton button) noexcept;
    void bind_gamepad_axis(
        std::size_t action, SDL_GamepadAxis axis, bool positive) noexcept;
    void reset(BindingDevice device) noexcept;

    [[nodiscard]] std::string binding_name(
        BindingDevice device, std::size_t action) const;
    [[nodiscard]] static std::string_view action_name(
        std::size_t action) noexcept;

    void load();
    void save() const;

private:
    std::array<SDL_Scancode, action_count> keyboard_{};
    std::array<GamepadBinding, action_count> gamepad_{};
};

struct PregameSettings {
    std::uint8_t timing_mode{1U};
    std::uint16_t presentation_fps{60U};
    std::uint8_t display_mode{};
    bool god_mode{};
    bool show_fps{};
    // 0=off, 1=light, 2=medium, 3=heavy.
    std::uint8_t anti_aliasing{};
    bool enhanced_graphics{};
    bool smooth_polys{};
    bool rtx_lighting{};
    bool vsync{};
    // 0=GPU (default), 1=SDL's portable software rasterizer.
    std::uint8_t renderer_mode{};
    bool msu1_music{};
    bool rumble{true};
    std::uint8_t crosshair_colour{};
    std::uint8_t experience{};
    std::uint8_t music_volume{100U};
    std::uint8_t sfx_volume{100U};
    std::uint8_t render_scale{};

    [[nodiscard]] bool operator==(const PregameSettings&) const = default;
};

// Guards the per-user preference directory against a second desktop runtime.
[[nodiscard]] std::filesystem::path single_instance_lock_path();

// Front-end choices live beside HUD layouts in persistent per-user storage so
// presentation and accessibility settings survive upgrades and app moves.
[[nodiscard]] std::filesystem::path pregame_settings_path();
[[nodiscard]] bool load_pregame_settings(
    const std::filesystem::path& path,
    PregameSettings& settings) noexcept;
[[nodiscard]] bool save_pregame_settings(
    const std::filesystem::path& path,
    const PregameSettings& settings) noexcept;

inline constexpr std::size_t starfox_ex_save_ram_size = 0x10000U;
[[nodiscard]] std::filesystem::path starfox_ex_save_ram_path();
[[nodiscard]] bool load_starfox_ex_save_ram(
    const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes) noexcept;
[[nodiscard]] bool save_starfox_ex_save_ram(
    const std::filesystem::path& path,
    std::span<const std::uint8_t> bytes) noexcept;

// HUD placement is deliberately human-readable and stored separately from
// controller bindings so it can be copied, edited, or reset independently.
[[nodiscard]] std::filesystem::path hud_layout_settings_path();
[[nodiscard]] bool load_hud_layout(
    const std::filesystem::path& path,
    render::HudLayoutProfiles& layouts) noexcept;
[[nodiscard]] bool save_hud_layout(
    const std::filesystem::path& path,
    const render::HudLayoutProfiles& layouts) noexcept;

} // namespace starfox::app
