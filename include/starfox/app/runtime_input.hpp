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

[[nodiscard]] constexpr bool peek_setup_menu(
    bool in_menu, bool tab_held, bool input_capture_active) noexcept {
    return in_menu && tab_held && !input_capture_active;
}

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
    static constexpr std::size_t reset_action = action_count;
    [[nodiscard]] static constexpr std::size_t remap_action_count(BindingDevice device) noexcept {
        return action_count + (device == BindingDevice::keyboard ? 1U : 0U);
    }
    bool bind_reset_key(SDL_Scancode scancode) noexcept;
    [[nodiscard]] bool matches_reset_shortcut(const SDL_KeyboardEvent& event) const noexcept;
    [[nodiscard]] static bool matches_god_mode_shortcut(const SDL_KeyboardEvent& event) noexcept;

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

    void load(const std::filesystem::path& override_path = {});
    void save(const std::filesystem::path& override_path = {}) const;

private:
    std::array<SDL_Scancode, action_count> keyboard_{};
    SDL_Scancode reset_key_{SDL_SCANCODE_R};
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
    // 0=off, 1=low, 2=medium, 3=high.
    std::uint8_t rtx_lighting{};
    // 0=off, 1=EDGE, 2=XBRZ. See starfox/render/pixel_filter.hpp.
    std::uint8_t two_d_filter{};
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
    bool on_screen_controls{true};
    bool swap_face_buttons{};
    std::uint8_t effect{};
    std::uint8_t effect_intensity{100U};
    std::uint8_t world_effect{};
    std::uint8_t world_effect_intensity{100U};
    std::uint8_t bloom{};
    std::uint8_t bloom_2d{};
    std::uint8_t model_smoothing{};
    // 0=English, 1=Japanese, 2=German, 3=French, 4=Spanish.
    std::uint8_t language{};
    std::uint8_t wireframe_thickness{1U};
    bool enhanced_shadows{};
    std::uint8_t chromatic_aberration{};
    std::uint8_t hdr_effect{};

    [[nodiscard]] bool operator==(const PregameSettings&) const = default;
};

// Guards the per-user preference directory against a second desktop runtime.
[[nodiscard]] std::filesystem::path single_instance_lock_path();

// Desktop data lives beside the executable. Packaged/mobile/console targets
// retain writable platform storage. Set once at startup, before loading data.
void set_portable_data_directory(const std::filesystem::path& directory);
[[nodiscard]] std::filesystem::path input_bindings_path();
// Copy missing legacy files without overwriting portable data or deleting originals.
void migrate_legacy_data(const std::filesystem::path& destination,
    const std::filesystem::path& legacy_settings,
    const std::filesystem::path& legacy_bindings);
void migrate_legacy_user_data();

// Front-end choices share the same data root as HUD, bindings and EX SRAM.
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
