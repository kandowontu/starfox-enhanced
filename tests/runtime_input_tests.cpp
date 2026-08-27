#include "starfox/app/runtime_input.hpp"
#include "starfox/input/buttons.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstring>
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

    SDL_CloseGamepad(gamepad);
    require(SDL_DetachVirtualJoystick(identifier),
            "virtual Steam Deck could not be detached");
    SDL_Quit();
    std::cout << "All runtime input tests passed.\n";
    return 0;
}
