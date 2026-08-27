#include "starfox/app/runtime_input.hpp"

#include "starfox/input/buttons.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace starfox::app {
namespace {

constexpr std::array<input::ButtonMask, InputBindings::action_count>
    kActionButtons{
        input::b,
        input::y,
        input::select,
        input::start,
        input::up,
        input::down,
        input::left,
        input::right,
        input::a,
        input::x,
        input::left_shoulder,
        input::right_shoulder,
    };

constexpr std::array<std::string_view, InputBindings::action_count>
    kActionNames{
        "B", "Y", "SELECT", "START", "UP", "DOWN",
        "LEFT", "RIGHT", "A", "X", "L", "R",
    };

constexpr std::array<SDL_Scancode, InputBindings::action_count>
    kDefaultKeyboard{
        SDL_SCANCODE_Z,
        SDL_SCANCODE_A,
        SDL_SCANCODE_BACKSPACE,
        SDL_SCANCODE_RETURN,
        SDL_SCANCODE_UP,
        SDL_SCANCODE_DOWN,
        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_X,
        SDL_SCANCODE_S,
        SDL_SCANCODE_Q,
        SDL_SCANCODE_W,
    };

constexpr std::array<GamepadBinding, InputBindings::action_count>
    kDefaultGamepad{{
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_SOUTH},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_WEST},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_BACK},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_START},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_DPAD_UP},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_EAST},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_NORTH},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    }};

constexpr std::int16_t kAxisThreshold = 16'000;

std::filesystem::path settings_path() {
    char* preference_path = SDL_GetPrefPath("StarFoxEnhanced", "StarFoxEnhanced");
    if (preference_path == nullptr) return {};
    const std::filesystem::path result =
        std::filesystem::path{preference_path} / "input-bindings.cfg";
    SDL_free(preference_path);
    return result;
}

void add_keyboard_button(
    input::ButtonMask& result,
    const bool* keys,
    SDL_Scancode scancode,
    input::ButtonMask button) noexcept {
    if (scancode >= 0 && scancode < SDL_SCANCODE_COUNT && keys[scancode]) {
        result = static_cast<input::ButtonMask>(result | button);
    }
}

void add_gamepad_button(
    input::ButtonMask& result,
    SDL_Gamepad* gamepad,
    SDL_GamepadButton physical,
    input::ButtonMask button) noexcept {
    if (gamepad != nullptr
        && physical >= 0 && physical < SDL_GAMEPAD_BUTTON_COUNT
        && SDL_GetGamepadButton(gamepad, physical)) {
        result = static_cast<input::ButtonMask>(result | button);
    }
}

} // namespace

InputBindings::InputBindings() {
    reset(BindingDevice::keyboard);
    reset(BindingDevice::gamepad);
}

input::ButtonMask InputBindings::sample(SDL_Gamepad* gamepad) const noexcept {
    const auto* keys = SDL_GetKeyboardState(nullptr);
    input::ButtonMask result{};
    for (std::size_t action = 0; action < action_count; ++action) {
        add_keyboard_button(
            result, keys, keyboard_[action], kActionButtons[action]);
        if (gamepad == nullptr) continue;
        const auto binding = gamepad_[action];
        if (binding.kind == GamepadBindingKind::button) {
            add_gamepad_button(result, gamepad,
                static_cast<SDL_GamepadButton>(binding.control),
                kActionButtons[action]);
            continue;
        }
        const auto axis = static_cast<SDL_GamepadAxis>(binding.control);
        if (axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) continue;
        const auto value = SDL_GetGamepadAxis(gamepad, axis);
        const auto active = binding.kind == GamepadBindingKind::axis_positive
            ? value >= kAxisThreshold : value <= -kAxisThreshold;
        if (active) {
            result = static_cast<input::ButtonMask>(
                result | kActionButtons[action]);
        }
    }
    return result;
}

input::ButtonMask InputBindings::sample_fixed_menu_navigation(
    SDL_Gamepad* gamepad) const noexcept {
    const auto* keys = SDL_GetKeyboardState(nullptr);
    input::ButtonMask result{};
    add_keyboard_button(result, keys, SDL_SCANCODE_UP, input::up);
    add_keyboard_button(result, keys, SDL_SCANCODE_DOWN, input::down);
    add_keyboard_button(result, keys, SDL_SCANCODE_LEFT, input::left);
    add_keyboard_button(result, keys, SDL_SCANCODE_RIGHT, input::right);
    add_keyboard_button(result, keys, SDL_SCANCODE_X, input::a);
    add_keyboard_button(result, keys, SDL_SCANCODE_A, input::y);
    add_keyboard_button(result, keys, SDL_SCANCODE_Z, input::b);
    add_keyboard_button(result, keys, SDL_SCANCODE_RETURN, input::start);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP, input::up);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN, input::down);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT, input::left);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, input::right);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_EAST, input::a);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_WEST, input::y);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_SOUTH, input::b);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_START, input::start);
    return result;
}

void InputBindings::bind_keyboard(
    std::size_t action, SDL_Scancode scancode) noexcept {
    if (action < action_count && scancode >= 0
        && scancode < SDL_SCANCODE_COUNT) keyboard_[action] = scancode;
}

void InputBindings::bind_gamepad_button(
    std::size_t action, SDL_GamepadButton button) noexcept {
    if (action < action_count && button >= 0
        && button < SDL_GAMEPAD_BUTTON_COUNT) {
        gamepad_[action] = {
            GamepadBindingKind::button, static_cast<std::int16_t>(button)};
    }
}

void InputBindings::bind_gamepad_axis(
    std::size_t action, SDL_GamepadAxis axis, bool positive) noexcept {
    if (action < action_count && axis >= 0 && axis < SDL_GAMEPAD_AXIS_COUNT) {
        gamepad_[action] = {
            positive ? GamepadBindingKind::axis_positive
                     : GamepadBindingKind::axis_negative,
            static_cast<std::int16_t>(axis),
        };
    }
}

void InputBindings::reset(BindingDevice device) noexcept {
    if (device == BindingDevice::keyboard) keyboard_ = kDefaultKeyboard;
    else gamepad_ = kDefaultGamepad;
}

std::string InputBindings::binding_name(
    BindingDevice device, std::size_t action) const {
    if (action >= action_count) return "?";
    if (device == BindingDevice::keyboard) {
        const auto* name = SDL_GetScancodeName(keyboard_[action]);
        return name == nullptr || *name == '\0' ? "UNKNOWN KEY" : name;
    }
    const auto binding = gamepad_[action];
    if (binding.kind == GamepadBindingKind::button) {
        const auto* name = SDL_GetGamepadStringForButton(
            static_cast<SDL_GamepadButton>(binding.control));
        return name == nullptr || *name == '\0' ? "UNKNOWN BUTTON" : name;
    }
    const auto* axis_name = SDL_GetGamepadStringForAxis(
        static_cast<SDL_GamepadAxis>(binding.control));
    std::string result = axis_name == nullptr || *axis_name == '\0'
        ? "UNKNOWN AXIS" : axis_name;
    result += binding.kind == GamepadBindingKind::axis_positive ? " +" : " -";
    return result;
}

std::string_view InputBindings::action_name(std::size_t action) noexcept {
    return action < action_count ? kActionNames[action] : std::string_view{"?"};
}

void InputBindings::load() {
    const auto path = settings_path();
    if (path.empty()) return;
    std::ifstream input{path};
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream fields{line};
        char device{};
        std::size_t action{};
        fields >> device >> action;
        if (!fields || action >= action_count) continue;
        if (device == 'K') {
            int scancode{};
            fields >> scancode;
            bind_keyboard(action, static_cast<SDL_Scancode>(scancode));
        } else if (device == 'G') {
            char kind{};
            int control{};
            fields >> kind >> control;
            if (kind == 'B') {
                bind_gamepad_button(
                    action, static_cast<SDL_GamepadButton>(control));
            } else if (kind == '+' || kind == '-') {
                bind_gamepad_axis(action,
                    static_cast<SDL_GamepadAxis>(control), kind == '+');
            }
        }
    }
}

void InputBindings::save() const {
    const auto path = settings_path();
    if (path.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output{path, std::ios::trunc};
    if (!output) return;
    output << "SFE_INPUT_V1\n";
    for (std::size_t action = 0; action < action_count; ++action) {
        output << "K " << action << ' '
               << static_cast<int>(keyboard_[action]) << '\n';
        const auto binding = gamepad_[action];
        const auto kind = binding.kind == GamepadBindingKind::button ? 'B'
            : binding.kind == GamepadBindingKind::axis_positive ? '+' : '-';
        output << "G " << action << ' ' << kind << ' '
               << binding.control << '\n';
    }
}

} // namespace starfox::app
