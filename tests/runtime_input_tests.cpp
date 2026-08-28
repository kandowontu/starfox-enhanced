#include "starfox/app/runtime_input.hpp"
#include "starfox/input/buttons.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message;
        const auto* error = SDL_GetError();
        if (error != nullptr && *error != '\0') std::cerr << ": " << error;
        std::cerr << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    starfox::app::configure_native_gamepad_support();
    require(std::strcmp(SDL_GetHint(SDL_HINT_XINPUT_ENABLED), "1") == 0,
            "XInput support was not enabled before SDL initialization");
    require(std::strcmp(
                SDL_GetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK), "1") == 0,
            "Steam Deck HIDAPI support was not enabled before initialization");
    require(SDL_Init(SDL_INIT_GAMEPAD), "SDL gamepad initialization failed");

    SDL_VirtualJoystickDesc description{};
    SDL_INIT_INTERFACE(&description);
    description.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    description.vendor_id = 0x28deU;
    description.product_id = 0x1205U;
    description.naxes = SDL_GAMEPAD_AXIS_COUNT;
    description.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    description.axis_mask = (1U << SDL_GAMEPAD_AXIS_COUNT) - 1U;
    description.button_mask = (1U << SDL_GAMEPAD_BUTTON_COUNT) - 1U;
    description.name = "Steam Deck Builtin Controller";
    const auto identifier = SDL_AttachVirtualJoystick(&description);
    require(identifier != 0U, "virtual Steam Deck could not be attached");

    auto* gamepad = starfox::app::open_preferred_gamepad();
    require(gamepad != nullptr, "preferred Steam Deck gamepad was not opened");
    require(starfox::app::gamepad_device_label(gamepad) == "STEAM DECK",
            "Steam Deck was not identified in the remapping UI");
    auto* joystick = SDL_GetGamepadJoystick(gamepad);
    require(joystick != nullptr, "opened gamepad has no joystick interface");

    starfox::app::InputBindings bindings;
    require(bindings.binding_name(starfox::app::BindingDevice::keyboard, 2U)
                == SDL_GetScancodeName(SDL_SCANCODE_APOSTROPHE),
            "keyboard Select did not default to apostrophe");
    bindings.bind_keyboard(2U, SDL_SCANCODE_BACKSPACE);
    bindings.reset(starfox::app::BindingDevice::keyboard);
    require(bindings.binding_name(starfox::app::BindingDevice::keyboard, 2U)
                == SDL_GetScancodeName(SDL_SCANCODE_APOSTROPHE),
            "reset keyboard bindings did not restore apostrophe Select");
    require(SDL_SetJoystickVirtualAxis(
                joystick, SDL_GAMEPAD_AXIS_LEFTX, 24'000),
            "virtual Steam Deck left stick could not move");
    SDL_UpdateGamepads();
    require((bindings.sample(gamepad) & starfox::input::right) != 0U,
            "default Steam Deck/XInput left stick did not steer right");

    require(SDL_SetJoystickVirtualAxis(
                joystick, SDL_GAMEPAD_AXIS_LEFTX, 0),
            "virtual Steam Deck left stick could not centre");
    require(SDL_SetJoystickVirtualButton(
                joystick, SDL_GAMEPAD_BUTTON_SOUTH, true),
            "virtual Steam Deck south button could not press");
    SDL_UpdateGamepads();
    require((bindings.sample(gamepad) & starfox::input::b) != 0U,
            "standard Xbox/Steam south button did not map to SNES B");

    require(SDL_SetJoystickVirtualButton(
                joystick, SDL_GAMEPAD_BUTTON_SOUTH, false),
            "virtual Steam Deck south button could not release");
    bindings.bind_gamepad_button(
        8U, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1);
    require(SDL_SetJoystickVirtualButton(
                joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, true),
            "virtual Steam Deck paddle could not press");
    SDL_UpdateGamepads();
    require((bindings.sample(gamepad) & starfox::input::a) != 0U,
            "Steam Deck back paddle could not be remapped");

    const auto documents_layout = starfox::app::hud_layout_settings_path();
    require(documents_layout.filename() == "hud-layout.cfg"
                && documents_layout.parent_path().filename()
                    == "Star Fox Enhanced",
            "HUD layout path is not in its Documents subfolder");
    const auto documents_pregame = starfox::app::pregame_settings_path();
    require(documents_pregame.filename() == "pregame.cfg"
                && documents_pregame.parent_path().filename()
                    == "Star Fox Enhanced",
            "pre-game settings path is not in its Documents subfolder");
    const auto pregame_test_path = std::filesystem::temp_directory_path()
        / "starfox-enhanced-pregame-test.cfg";
    const starfox::app::PregameSettings saved_pregame{
        1U, 90U, 3U, true, true, 5U};
    require(starfox::app::save_pregame_settings(
                pregame_test_path, saved_pregame),
            "pre-game settings could not be saved");
    auto loaded_pregame = starfox::app::PregameSettings{};
    require(starfox::app::load_pregame_settings(
                pregame_test_path, loaded_pregame)
                && loaded_pregame == saved_pregame,
            "pre-game settings did not round-trip");
    std::error_code pregame_remove_error;
    std::filesystem::remove(pregame_test_path, pregame_remove_error);
    require(!pregame_remove_error,
            "pre-game settings test file could not be removed");
    const auto layout_test_path = std::filesystem::temp_directory_path()
        / "starfox-enhanced-hud-layout-test.cfg";
    starfox::render::HudLayoutProfiles saved_layouts{};
    saved_layouts[0][starfox::render::HudElement::lives] = {12, -3};
    saved_layouts[4][starfox::render::HudElement::comms] = {-144, 9};
    require(starfox::app::save_hud_layout(
                layout_test_path, saved_layouts),
            "per-video-size HUD layouts could not be saved");
    starfox::render::HudLayoutProfiles loaded_layouts{};
    require(starfox::app::load_hud_layout(
                layout_test_path, loaded_layouts)
                && loaded_layouts[0][starfox::render::HudElement::lives].x == 12
                && loaded_layouts[0][starfox::render::HudElement::lives].y == -3
                && loaded_layouts[4][starfox::render::HudElement::comms].x == -144
                && loaded_layouts[4][starfox::render::HudElement::comms].y == 9
                && loaded_layouts[1][starfox::render::HudElement::lives].x == 0,
            "HUD layout profiles did not round-trip independently");
    std::error_code layout_remove_error;
    std::filesystem::remove(layout_test_path, layout_remove_error);
    require(!layout_remove_error, "HUD layout test file could not be removed");

    SDL_CloseGamepad(gamepad);
    require(SDL_DetachVirtualJoystick(identifier),
            "virtual Steam Deck could not be detached");
    SDL_Quit();
    std::cout << "All runtime input tests passed.\n";
    return 0;
}
