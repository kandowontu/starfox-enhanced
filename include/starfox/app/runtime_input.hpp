#pragma once

#include "starfox/input/input_latch.hpp"
#include "starfox/render/hud_layout.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace starfox::app {

// Installs controller-driver defaults before SDL_INIT_GAMEPAD. Explicit user
// or environment overrides retain priority over these application defaults.
void configure_native_gamepad_support() noexcept;

// Opens the most useful player controller when more than one mapped device is
// present (Steam virtual/Deck first, then XInput/Xbox, then generic gamepads).
[[nodiscard]] SDL_Gamepad* open_preferred_gamepad() noexcept;

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
    std::uint8_t timing_mode{};
    std::uint16_t presentation_fps{60U};
    std::uint8_t display_mode{};
    bool god_mode{};
    bool show_fps{};
    std::uint8_t crosshair_colour{};

    [[nodiscard]] bool operator==(const PregameSettings&) const = default;
};

// Front-end choices live beside HUD layouts in Documents so presentation and
// accessibility settings survive upgrades and self-contained EXE moves.
[[nodiscard]] std::filesystem::path pregame_settings_path();
[[nodiscard]] bool load_pregame_settings(
    const std::filesystem::path& path,
    PregameSettings& settings) noexcept;
[[nodiscard]] bool save_pregame_settings(
    const std::filesystem::path& path,
    const PregameSettings& settings) noexcept;

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
