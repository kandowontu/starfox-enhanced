#include "starfox/app/runtime_input.hpp"

#include "starfox/input/buttons.hpp"
#include "starfox/render/effect_types.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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
        SDL_SCANCODE_APOSTROPHE,
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
#if defined(__SWITCH__)
        // The pinned libnx SDL backend currently publishes physical
        // A/B/X/Y as logical SOUTH/EAST/WEST/NORTH in Xbox label order.
        // Bind the SNES-labelled actions to Nintendo's printed buttons.
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_EAST},  // B
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_NORTH}, // Y
#else
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_SOUTH},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_WEST},
#endif
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_BACK},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_START},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_DPAD_UP},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
#if defined(__SWITCH__)
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_SOUTH}, // A
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_WEST},  // X
#else
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_EAST},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_NORTH},
#endif
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
        {GamepadBindingKind::button, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    }};

constexpr std::int16_t kAxisThreshold = 16'000;

std::string lower_ascii(std::string_view text) {
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return result;
}

bool contains(std::string_view text, std::string_view fragment) {
    return lower_ascii(text).find(fragment) != std::string::npos;
}

int gamepad_preference(SDL_JoystickID identifier) {
    const auto* raw_name = SDL_GetGamepadNameForID(identifier);
    const auto name = raw_name == nullptr
        ? std::string_view{} : std::string_view{raw_name};
    auto score = SDL_GetGamepadPlayerIndexForID(identifier) == 0 ? 100 : 0;
    if (contains(name, "steam deck") || contains(name, "steam virtual")) {
        score += 1'000;
    } else {
        const auto type = SDL_GetGamepadTypeForID(identifier);
        if (type == SDL_GAMEPAD_TYPE_XBOX360
            || type == SDL_GAMEPAD_TYPE_XBOXONE) score += 800;
        else if (type == SDL_GAMEPAD_TYPE_STANDARD) score += 200;
    }
    return score;
}

// Pre-game settings file format. Bump kPregameRevision when a field is added;
// the reader accepts every revision up to it.
constexpr std::string_view kPregameTag{"SFE_PREGAME_V"};
// 12, not 11: upstream 0.0.4.1 also used 11, for the RTX LIGHTING toggle ->
// 0-3 intensity migration. Reusing that number would make this build reject a
// settings file written by upstream 0.0.4.1 -- it says V11 and legitimately has
// no TWO_D_FILTER key, so the missing-key check would fail the whole load and
// silently reset every setting. Take the next number and default TWO_D_FILTER
// for anything older.
constexpr int kPregameRevision = 12;

std::filesystem::path portable_directory;

std::filesystem::path desktop_data_directory() {
#if defined(STARFOX_UWP) || defined(__ANDROID__) || defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_VITA) || defined(__SWITCH__)
    return {};
#else
    if (!portable_directory.empty()) return portable_directory;
    const auto* base = SDL_GetBasePath();
    if (base == nullptr || *base == '\0') throw std::runtime_error{"Cannot locate portable data directory"};
    return std::filesystem::path{base};
#endif
}

std::filesystem::path legacy_bindings_path() {
    char* preference_path = SDL_GetPrefPath("StarFoxEnhanced", "StarFoxEnhanced");
    if (preference_path == nullptr) return {};
    const std::filesystem::path result =
        std::filesystem::path{preference_path} / "input-bindings.cfg";
    SDL_free(preference_path);
    return result;
}

std::filesystem::path legacy_documents_path(std::string_view filename) {
#if defined(SDL_PLATFORM_VITA)
    return std::filesystem::path{"ux0:data/StarFoxEnhanced"} / filename;
#elif defined(STARFOX_UWP)
    // Xbox UWP package files are read-only. SDL maps its preference path to
    // the app's persistent LocalState internal-storage directory.
    if (char* preference_path =
            SDL_GetPrefPath("StarFoxEnhanced", "StarFoxEnhanced");
        preference_path != nullptr) {
        const auto result = std::filesystem::path{preference_path} / filename;
        SDL_free(preference_path);
        return result;
    }
    return {};
#else
    if (const auto* documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
        documents != nullptr && *documents != '\0') {
        return std::filesystem::path{documents}
            / "Star Fox Enhanced" / filename;
    }
#if defined(_WIN32)
    if (const auto* profile = std::getenv("USERPROFILE");
        profile != nullptr && *profile != '\0') {
        return std::filesystem::path{profile}
            / "Documents" / "Star Fox Enhanced" / filename;
    }
#else
    // Minimal/headless Linux installations often have no XDG user-dirs
    // database, but the documented settings location remains Documents.
    if (const auto* home = std::getenv("HOME");
        home != nullptr && *home != '\0') {
        return std::filesystem::path{home}
            / "Documents" / "Star Fox Enhanced" / filename;
    }
#endif
    // Android does not currently expose the standard user-folder API. The
    // SDL preference directory is writable on every supported desktop/mobile
    // target and keeps settings persistent inside the application sandbox.
    if (char* preference_path =
            SDL_GetPrefPath("StarFoxEnhanced", "StarFoxEnhanced");
        preference_path != nullptr) {
        const auto result =
            std::filesystem::path{preference_path} / filename;
        SDL_free(preference_path);
        return result;
    }
    return {};
#endif
}

std::filesystem::path documents_settings_path(std::string_view filename) {
    const auto directory = desktop_data_directory();
    return directory.empty() ? legacy_documents_path(filename) : directory / filename;
}

std::filesystem::path settings_path() {
    const auto directory = desktop_data_directory();
    return directory.empty() ? legacy_bindings_path() : directory / "input-bindings.cfg";
}

constexpr std::array<std::string_view, 5> kHudElementNames{
    "LIVES", "SHIELD", "BOMBS_BOOST", "COMMS", "BOSS_HEALTH"};
constexpr std::array<std::string_view, 5> kLegacyHudProfileNames{
    "4_3", "16_9", "16_10", "21_9", "32_9"};
constexpr std::array<std::string_view, 10> kHudProfileNames{
    "ORIGINAL_4_3", "ORIGINAL_16_9", "ORIGINAL_16_10", "ORIGINAL_21_9",
    "ORIGINAL_32_9", "EX_4_3", "EX_16_9", "EX_16_10", "EX_21_9",
    "EX_32_9"};

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

void add_gamepad_axis(
    input::ButtonMask& result,
    SDL_Gamepad* gamepad,
    SDL_GamepadAxis axis,
    bool positive,
    input::ButtonMask button) noexcept {
    if (gamepad == nullptr || axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) return;
    const auto value = SDL_GetGamepadAxis(gamepad, axis);
    if ((positive && value >= kAxisThreshold)
        || (!positive && value <= -kAxisThreshold)) {
        result = static_cast<input::ButtonMask>(result | button);
    }
}

bool is_default_direction(
    std::size_t action, const GamepadBinding& binding) noexcept {
    if (action < 4U || action > 7U) return false;
    const auto& expected = kDefaultGamepad[action];
    return binding.kind == expected.kind && binding.control == expected.control;
}

} // namespace

std::filesystem::path single_instance_lock_path() {
    if (const auto directory = desktop_data_directory(); !directory.empty()) {
        return directory / "runtime.lock";
    }
    char* preference_path =
        SDL_GetPrefPath("StarFoxEnhanced", "StarFoxEnhanced");
    if (preference_path == nullptr) return {};
    const auto result =
        std::filesystem::path{preference_path} / "runtime.lock";
    SDL_free(preference_path);
    return result;
}

void set_portable_data_directory(const std::filesystem::path& directory) {
#if defined(STARFOX_UWP) || defined(__ANDROID__) || defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_VITA) || defined(__SWITCH__)
    static_cast<void>(directory);
#else
    if (directory.empty() || !directory.is_absolute()) {
        throw std::invalid_argument{"Portable data directory must be absolute"};
    }
    portable_directory = directory.lexically_normal();
#endif
}

std::filesystem::path input_bindings_path() { return settings_path(); }

void migrate_legacy_data(const std::filesystem::path& destination,
    const std::filesystem::path& legacy_settings,
    const std::filesystem::path& legacy_bindings) {
    if (destination.empty()) return;
    for (const auto* filename : {"pregame.cfg", "hud-layout.cfg", "starfox-ex.srm", "input-bindings.cfg"}) {
        const auto target = destination / filename;
        if (std::filesystem::exists(target)) continue;
        const auto& source_directory = std::string_view{filename} == "input-bindings.cfg"
            ? legacy_bindings : legacy_settings;
        if (source_directory.empty()) continue;
        const auto source = source_directory / filename;
        if (!std::filesystem::is_regular_file(source)) continue;
        std::filesystem::create_directories(destination);
        // skip_existing also protects a target created between the check and copy.
        std::filesystem::copy_file(source, target, std::filesystem::copy_options::skip_existing);
    }
}

void migrate_legacy_user_data() {
    const auto destination = desktop_data_directory();
    if (destination.empty()) return;
    migrate_legacy_data(destination, legacy_documents_path("pregame.cfg").parent_path(),
        legacy_bindings_path().parent_path());
}

void configure_native_gamepad_support() noexcept {
#if defined(STARFOX_UWP)
    // SDL3 disables WGI by default on desktop. It is the ONLY native
    // controller backend in our WinRT build (XInput/RawInput are absent).
    // Set this before SDL_INIT_GAMEPAD, including for controllers already
    // connected when the Xbox activates the app.
    static_cast<void>(SDL_SetHintWithPriority(
        SDL_HINT_JOYSTICK_WGI, "1", SDL_HINT_OVERRIDE));
#endif
    // XInput is the native Windows path used by Xbox controllers and by Steam
    // Input under Proton. The dedicated HIDAPI path exposes the Steam Deck's
    // built-in controls, paddles, and trackpad buttons when Steam Input is not
    // translating it into a virtual Xbox-layout device.
    static_cast<void>(SDL_SetHintWithPriority(
        SDL_HINT_XINPUT_ENABLED, "1", SDL_HINT_DEFAULT));
    static_cast<void>(SDL_SetHintWithPriority(
        SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, "1", SDL_HINT_DEFAULT));
    static_cast<void>(SDL_SetHintWithPriority(
        SDL_HINT_JOYSTICK_RAWINPUT_CORRELATE_XINPUT,
        "1", SDL_HINT_DEFAULT));
}

SDL_Gamepad* open_preferred_gamepad() noexcept {
    auto opened = open_player_gamepads(1U);
    return opened.empty() ? nullptr : opened.front();
}

std::vector<SDL_Gamepad*> open_player_gamepads(std::size_t maximum) noexcept {
    std::vector<SDL_Gamepad*> result;
    if (maximum == 0U) return result;
    int count = 0;
    SDL_JoystickID* identifiers = SDL_GetGamepads(&count);
    if (identifiers == nullptr || count <= 0) {
        SDL_free(identifiers);
        return result;
    }
    std::vector<SDL_JoystickID> ordered{
        identifiers, identifiers + static_cast<std::size_t>(count)};
    SDL_free(identifiers);
    std::stable_sort(ordered.begin(), ordered.end(), [](auto left, auto right) {
        const auto left_player = SDL_GetGamepadPlayerIndexForID(left);
        const auto right_player = SDL_GetGamepadPlayerIndexForID(right);
        if (left_player >= 0 || right_player >= 0) {
            if (left_player < 0) return false;
            if (right_player < 0) return true;
            if (left_player != right_player) return left_player < right_player;
        }
        return gamepad_preference(left) > gamepad_preference(right);
    });
    for (const auto identifier : ordered) {
        if (auto* gamepad = SDL_OpenGamepad(identifier); gamepad != nullptr) {
            result.push_back(gamepad);
            if (result.size() >= maximum) break;
        }
    }
    return result;
}

std::string gamepad_device_label(SDL_Gamepad* gamepad) {
    if (gamepad == nullptr) return "NO GAMEPAD";
    const auto* raw_name = SDL_GetGamepadName(gamepad);
    const auto name = raw_name == nullptr
        ? std::string_view{} : std::string_view{raw_name};
    if (contains(name, "steam deck")) return "STEAM DECK";
    if (contains(name, "steam virtual")) return "STEAM INPUT";
    const auto type = SDL_GetGamepadType(gamepad);
    if (type == SDL_GAMEPAD_TYPE_XBOX360
        || type == SDL_GAMEPAD_TYPE_XBOXONE
        || contains(name, "xinput")) return "XINPUT / XBOX";
    auto result = name.empty() ? std::string{"GAMEPAD"} : std::string{name};
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    if (result.size() > 20U) result.resize(20U);
    return result;
}

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
    }
    return static_cast<input::ButtonMask>(
        result | sample_gamepad_only(gamepad));
}

input::ButtonMask InputBindings::sample_gamepad_only(
    SDL_Gamepad* gamepad) const noexcept {
    input::ButtonMask result{};
    if (gamepad == nullptr) return result;
    for (std::size_t action = 0; action < action_count; ++action) {
        const auto binding = gamepad_[action];
        if (binding.kind == GamepadBindingKind::button) {
            add_gamepad_button(result, gamepad,
                static_cast<SDL_GamepadButton>(binding.control),
                kActionButtons[action]);
            // The standard Xbox/Steam layout uses both the D-pad and left
            // stick for movement out of the box. Once a direction is remapped
            // away from its default D-pad binding, that custom binding fully
            // replaces this fallback.
            if (is_default_direction(action, binding)) {
                const auto vertical = action == 4U || action == 5U;
                add_gamepad_axis(result, gamepad,
                    vertical ? SDL_GAMEPAD_AXIS_LEFTY
                             : SDL_GAMEPAD_AXIS_LEFTX,
                    action == 5U || action == 7U,
                    kActionButtons[action]);
            }
        } else {
            add_gamepad_axis(result, gamepad,
                static_cast<SDL_GamepadAxis>(binding.control),
                binding.kind == GamepadBindingKind::axis_positive,
                kActionButtons[action]);
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
#if defined(__SWITCH__)
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_SOUTH, input::a);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_NORTH, input::y);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_EAST, input::b);
#else
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_EAST, input::a);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_WEST, input::y);
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_SOUTH, input::b);
#endif
    add_gamepad_button(result, gamepad, SDL_GAMEPAD_BUTTON_START, input::start);
    add_gamepad_axis(result, gamepad,
        SDL_GAMEPAD_AXIS_LEFTY, false, input::up);
    add_gamepad_axis(result, gamepad,
        SDL_GAMEPAD_AXIS_LEFTY, true, input::down);
    add_gamepad_axis(result, gamepad,
        SDL_GAMEPAD_AXIS_LEFTX, false, input::left);
    add_gamepad_axis(result, gamepad,
        SDL_GAMEPAD_AXIS_LEFTX, true, input::right);
    return result;
}

void InputBindings::bind_keyboard(
    std::size_t action, SDL_Scancode scancode) noexcept {
    if (action == reset_action) {
        static_cast<void>(bind_reset_key(scancode));
        return;
    }
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
    if (device == BindingDevice::keyboard) {
        keyboard_ = kDefaultKeyboard;
        reset_key_ = SDL_SCANCODE_R;
    }
    else gamepad_ = kDefaultGamepad;
}

std::string InputBindings::binding_name(
    BindingDevice device, std::size_t action) const {
    if (device == BindingDevice::keyboard && action == reset_action) {
        return std::string{"CTRL+SHIFT+"} + SDL_GetScancodeName(reset_key_);
    }
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
    if (action == reset_action) return "RESET";
    return action < action_count ? kActionNames[action] : std::string_view{"?"};
}

bool InputBindings::bind_reset_key(SDL_Scancode scancode) noexcept {
    if (scancode <= SDL_SCANCODE_UNKNOWN || scancode >= SDL_SCANCODE_COUNT
        || scancode == SDL_SCANCODE_ESCAPE
        || (scancode >= SDL_SCANCODE_LCTRL && scancode <= SDL_SCANCODE_RGUI)) return false;
    const auto* name = SDL_GetScancodeName(scancode);
    if (name == nullptr || *name == '\0') return false;
    reset_key_ = scancode;
    return true;
}

bool InputBindings::matches_god_mode_shortcut(const SDL_KeyboardEvent& event) noexcept {
    return event.type == SDL_EVENT_KEY_DOWN && !event.repeat
        && event.scancode == SDL_SCANCODE_F12
        && (event.mod & SDL_KMOD_CTRL) != 0U
        && (event.mod & SDL_KMOD_ALT) != 0U
        && (event.mod & (SDL_KMOD_SHIFT | SDL_KMOD_GUI)) == 0U;
}

bool InputBindings::matches_reset_shortcut(const SDL_KeyboardEvent& event) const noexcept {
    return event.type == SDL_EVENT_KEY_DOWN && !event.repeat
        && event.scancode == reset_key_
        && (event.mod & SDL_KMOD_CTRL) != 0U
        && (event.mod & SDL_KMOD_SHIFT) != 0U
        && (event.mod & (SDL_KMOD_ALT | SDL_KMOD_GUI)) == 0U;
}

void InputBindings::load(const std::filesystem::path& override_path) {
    const auto path = override_path.empty() ? settings_path() : override_path;
    if (path.empty()) return;
    std::ifstream input{path};
    std::string line;
    if (!std::getline(input, line)) return;
    const auto migrate_select_default = line == "SFE_INPUT_V1";
    while (std::getline(input, line)) {
        std::istringstream fields{line};
        char device{};
        std::size_t action{};
        fields >> device >> action;
        if (!fields || action > reset_action
            || (action == reset_action && device != 'K')) continue;
        if (device == 'K') {
            int scancode{};
            fields >> scancode;
            if (fields) bind_keyboard(action, static_cast<SDL_Scancode>(scancode));
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
    // V1 shipped Backspace as Select. Preserve every user remap while moving
    // that exact former default to the new apostrophe default.
    if (migrate_select_default && keyboard_[2U] == SDL_SCANCODE_BACKSPACE) {
        keyboard_[2U] = SDL_SCANCODE_APOSTROPHE;
    }
}

void InputBindings::save(const std::filesystem::path& override_path) const {
    const auto path = override_path.empty() ? settings_path() : override_path;
    if (path.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output{path, std::ios::trunc};
    if (!output) return;
    output << "SFE_INPUT_V2\n";
    output << "K " << reset_action << ' ' << static_cast<int>(reset_key_) << '\n';
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

std::filesystem::path pregame_settings_path() {
    return documents_settings_path("pregame.cfg");
}

bool load_pregame_settings(
    const std::filesystem::path& path,
    PregameSettings& settings) noexcept {
    if (path.empty()) return false;
    std::ifstream input{path};
    std::string version;
    if (!(input >> version) || !version.starts_with(kPregameTag)) return false;
    const auto digits = std::string_view{version}.substr(kPregameTag.size());
    if (digits.empty() || !std::all_of(digits.begin(), digits.end(),
            [](unsigned char character) {
                return std::isdigit(character) != 0;
            })) {
        return false;
    }
    int revision{};
    try {
        revision = std::stoi(std::string{digits});
    } catch (...) {
        return false;
    }
    if (revision < 1 || revision > kPregameRevision) return false;

    auto loaded = PregameSettings{};
    bool found_bloom_2d = false;
    std::array<bool, 21> found{};
    if (revision < 12) found[20] = true;
    if (revision < 10) {
        found[18] = true;
        found[19] = true;
    }
    if (revision < 9) found[17] = true;
    if (revision < 8) found[16] = true;
    if (revision < 7) {
        found[14] = true;
        found[15] = true;
    }
    if (revision < 6) {
        found[12] = true;
        found[13] = true;
    }
    found[6] = revision == 1;
    if (revision <= 2) {
        std::fill(found.begin() + 7, found.end(), true);
    } else if (revision == 3) {
        // V3's combined Enhanced option already included polygon edge
        // smoothing. Preserve that visible result when splitting it into a
        // dedicated V4 choice.
        found[9] = true;
    }
    std::string name;
    int value{};
    while (input >> name >> value) {
        if (name == "TIMING_MODE") {
            loaded.timing_mode = static_cast<std::uint8_t>(value);
            found[0] = value >= 0 && value <= 1;
        } else if (name == "PRESENTATION_FPS") {
            constexpr std::array valid{20, 30, 60, 90, 120, 240, 360, 480};
            loaded.presentation_fps = static_cast<std::uint16_t>(value);
            found[1] = std::find(valid.begin(), valid.end(), value) != valid.end();
        } else if (name == "DISPLAY_MODE") {
            loaded.display_mode = static_cast<std::uint8_t>(value);
            found[2] = value >= 0 && value <= 4;
        } else if (name == "GOD_MODE") {
            loaded.god_mode = value != 0;
            found[3] = value == 0 || value == 1;
        } else if (name == "SHOW_FPS") {
            loaded.show_fps = value != 0;
            found[4] = value == 0 || value == 1;
        } else if (name == "ANTI_ALIASING") {
            loaded.anti_aliasing = static_cast<std::uint8_t>(value);
            found[7] = value >= 0 && value <= (revision >= 5 ? 3 : 1);
        } else if (name == "ENHANCED_GRAPHICS") {
            loaded.enhanced_graphics = value != 0;
            found[8] = value == 0 || value == 1;
        } else if (name == "SMOOTH_POLYS") {
            loaded.smooth_polys = value != 0;
            found[9] = value == 0 || value == 1;
        } else if (name == "RTX_LIGHTING") {
            loaded.rtx_lighting = static_cast<std::uint8_t>(
                revision < 11 ? (value != 0 ? 3 : 0) : value);
            found[10] = value >= 0 && value <= (revision < 11 ? 1 : 3);
        } else if (name == "TWO_D_FILTER") {
            loaded.two_d_filter = static_cast<std::uint8_t>(value);
            found[20] = value >= 0 && value <= 4;
        } else if (name == "EFFECTS") {
            if (value < 0 || value >= render::effect_count) return false;
            loaded.effect = static_cast<std::uint8_t>(value);
        } else if (name == "BLOOM") {
            if (value < 0 || value > 3) return false;
            loaded.bloom = static_cast<std::uint8_t>(value);
        } else if (name == "BLOOM_2D") {
            if (value < 0 || value > 3) return false;
            loaded.bloom_2d = static_cast<std::uint8_t>(value);
            found_bloom_2d = true;
        } else if (name == "MODEL_SMOOTHING") {
            if (value < 0 || value > 3) return false;
            loaded.model_smoothing = static_cast<std::uint8_t>(value);
        } else if (name == "EFFECT_INTENSITY") {
            if (value < 0 || value > 100) return false;
            loaded.effect_intensity = static_cast<std::uint8_t>(value);
        } else if (name == "WORLD_EFFECTS") {
            if (value < 0 || value >= render::effect_count) return false;
            loaded.world_effect = static_cast<std::uint8_t>(value);
        } else if (name == "WORLD_EFFECT_INTENSITY") {
            if (value < 0 || value > 100) return false;
            loaded.world_effect_intensity = static_cast<std::uint8_t>(value);
        } else if (name == "VSYNC") {
            loaded.vsync = value != 0;
            found[11] = value == 0 || value == 1;
        } else if (name == "RENDERER_MODE") {
            loaded.renderer_mode = static_cast<std::uint8_t>(value);
            found[16] = value >= 0 && value <= 1;
        } else if (name == "MSU1_MUSIC") {
            loaded.msu1_music = value != 0;
            found[12] = value == 0 || value == 1;
        } else if (name == "RUMBLE") {
            loaded.rumble = value != 0;
            found[13] = value == 0 || value == 1;
        } else if (name == "CROSSHAIR_COLOUR") {
            loaded.crosshair_colour = static_cast<std::uint8_t>(value);
            found[5] = value >= 0 && value <= 7;
        } else if (name == "EXPERIENCE") {
            loaded.experience = static_cast<std::uint8_t>(value);
            found[6] = value >= 0 && value <= 1;
        } else if (name == "MUSIC_VOLUME") {
            loaded.music_volume = static_cast<std::uint8_t>(value);
            found[14] = value >= 0 && value <= 100;
        } else if (name == "SFX_VOLUME") {
            loaded.sfx_volume = static_cast<std::uint8_t>(value);
            found[15] = value >= 0 && value <= 100;
        } else if (name == "RENDER_SCALE") {
            loaded.render_scale = static_cast<std::uint8_t>(value);
            found[17] = value >= 0 && value <= 9;
        } else if (name == "ON_SCREEN_CONTROLS") {
            loaded.on_screen_controls = value != 0;
            found[18] = value == 0 || value == 1;
        } else if (name == "SWAP_FACE_BUTTONS") {
            loaded.swap_face_buttons = value != 0;
            found[19] = value == 0 || value == 1;
        }
    }
    if (!found_bloom_2d) loaded.bloom_2d = loaded.bloom;
    if (!std::all_of(found.begin(), found.end(),
            [](bool value) { return value; })) return false;
    if (revision == 3) {
        loaded.smooth_polys = loaded.enhanced_graphics;
    }
    if (revision < 5 && loaded.anti_aliasing != 0U) {
        // The original ON setting used what is now the medium FXAA kernel.
        loaded.anti_aliasing = 2U;
    }
    // Versions through V9 offered wasteful 5x-10x modes. Preserve those
    // users' intent at the new supported maximum instead of rejecting their
    // otherwise valid settings file.
    loaded.render_scale = std::min<std::uint8_t>(loaded.render_scale, 3U);
    if (revision < 12) {
        loaded.two_d_filter = loaded.enhanced_graphics ? 1U : 0U;
    }
    settings = loaded;
    return true;
}

bool save_pregame_settings(
    const std::filesystem::path& path,
    const PregameSettings& settings) noexcept {
    if (path.empty() || settings.timing_mode > 1U
        || settings.display_mode > 4U || settings.crosshair_colour > 7U
        || settings.anti_aliasing > 3U || settings.rtx_lighting > 3U
        || settings.two_d_filter > 4U || settings.effect >= render::effect_count
        || settings.effect_intensity > 100U || settings.renderer_mode > 1U
        || settings.world_effect >= render::effect_count || settings.world_effect_intensity > 100U || settings.bloom > 3U || settings.bloom_2d > 3U
        || settings.experience > 1U || settings.music_volume > 100U
        || settings.sfx_volume > 100U || settings.render_scale > 3U || settings.model_smoothing > 3U) {
        return false;
    }
    constexpr std::array<std::uint16_t, 8> valid_fps{
        20U, 30U, 60U, 90U, 120U, 240U, 360U, 480U};
    if (std::find(valid_fps.begin(), valid_fps.end(),
            settings.presentation_fps) == valid_fps.end()) return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream output{path, std::ios::trunc};
    if (!output) return false;
    output << kPregameTag << kPregameRevision << '\n'
           << "EXPERIENCE " << static_cast<unsigned>(settings.experience) << '\n'
           << "TIMING_MODE " << static_cast<unsigned>(settings.timing_mode) << '\n'
           << "PRESENTATION_FPS " << settings.presentation_fps << '\n'
           << "DISPLAY_MODE " << static_cast<unsigned>(settings.display_mode) << '\n'
           << "GOD_MODE " << static_cast<unsigned>(settings.god_mode) << '\n'
           << "SHOW_FPS " << static_cast<unsigned>(settings.show_fps) << '\n'
           << "ANTI_ALIASING "
           << static_cast<unsigned>(settings.anti_aliasing) << '\n'
           << "ENHANCED_GRAPHICS "
           << static_cast<unsigned>(settings.enhanced_graphics) << '\n'
           << "SMOOTH_POLYS "
           << static_cast<unsigned>(settings.smooth_polys) << '\n'
           << "RTX_LIGHTING "
           << static_cast<unsigned>(settings.rtx_lighting) << '\n'
           << "TWO_D_FILTER "
           << static_cast<unsigned>(settings.two_d_filter) << '\n'
           << "EFFECTS " << static_cast<unsigned>(settings.effect) << '\n'
           << "EFFECT_INTENSITY " << static_cast<unsigned>(settings.effect_intensity) << '\n'
           << "WORLD_EFFECTS " << static_cast<unsigned>(settings.world_effect) << '\n'
           << "WORLD_EFFECT_INTENSITY " << static_cast<unsigned>(settings.world_effect_intensity) << '\n'
           << "BLOOM " << static_cast<unsigned>(settings.bloom) << '\n'
           << "BLOOM_2D " << static_cast<unsigned>(settings.bloom_2d) << '\n'
           << "MODEL_SMOOTHING " << static_cast<unsigned>(settings.model_smoothing) << '\n'
           << "VSYNC " << static_cast<unsigned>(settings.vsync) << '\n'
           << "RENDERER_MODE "
           << static_cast<unsigned>(settings.renderer_mode) << '\n'
           << "MSU1_MUSIC "
           << static_cast<unsigned>(settings.msu1_music) << '\n'
           << "RUMBLE " << static_cast<unsigned>(settings.rumble) << '\n'
           << "CROSSHAIR_COLOUR "
           << static_cast<unsigned>(settings.crosshair_colour) << '\n'
           << "MUSIC_VOLUME "
           << static_cast<unsigned>(settings.music_volume) << '\n'
           << "SFX_VOLUME "
           << static_cast<unsigned>(settings.sfx_volume) << '\n'
           << "RENDER_SCALE "
           << static_cast<unsigned>(settings.render_scale) << '\n'
           << "ON_SCREEN_CONTROLS "
           << static_cast<unsigned>(settings.on_screen_controls) << '\n'
           << "SWAP_FACE_BUTTONS "
           << static_cast<unsigned>(settings.swap_face_buttons) << '\n';
    return static_cast<bool>(output);
}

std::filesystem::path starfox_ex_save_ram_path() {
    return documents_settings_path("starfox-ex.srm");
}

bool load_starfox_ex_save_ram(
    const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes) noexcept {
    try {
        if (path.empty()) return false;
        std::ifstream input{path, std::ios::binary | std::ios::ate};
        if (!input || input.tellg() != static_cast<std::streamoff>(
                starfox_ex_save_ram_size)) return false;
        input.seekg(0, std::ios::beg);
        auto loaded = std::vector<std::uint8_t>(starfox_ex_save_ram_size);
        input.read(reinterpret_cast<char*>(loaded.data()),
            static_cast<std::streamsize>(loaded.size()));
        if (!input) return false;
        bytes = std::move(loaded);
        return true;
    } catch (...) {
        return false;
    }
}

bool save_starfox_ex_save_ram(
    const std::filesystem::path& path,
    std::span<const std::uint8_t> bytes) noexcept {
    try {
        if (path.empty() || bytes.size() != starfox_ex_save_ram_size) {
            return false;
        }
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return false;
        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        if (!output) return false;
        output.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(output);
    } catch (...) {
        return false;
    }
}

std::filesystem::path hud_layout_settings_path() {
    return documents_settings_path("hud-layout.cfg");
}

bool load_hud_layout(
    const std::filesystem::path& path,
    render::HudLayoutProfiles& layouts) noexcept {
    if (path.empty()) return false;
    std::ifstream input{path};
    std::string version;
    if (!(input >> version)
        || (version != "SFE_HUD_LAYOUT_V2"
            && version != "SFE_HUD_LAYOUT_V3"
            && version != "SFE_HUD_LAYOUT_V4")) return false;
    const auto legacy = version == "SFE_HUD_LAYOUT_V2";
    const auto missing_boss_health = version != "SFE_HUD_LAYOUT_V4";

    auto loaded = render::HudLayoutProfiles{};
    std::array<std::array<bool, kHudElementNames.size()>,
        kHudProfileNames.size()> found{};
    if (missing_boss_health) {
        for (auto& profile : found) {
            profile[static_cast<std::size_t>(
                render::HudElement::boss_health)] = true;
        }
    }
    std::string profile;
    std::string name;
    int x{};
    int y{};
    while (input >> profile >> name >> x >> y) {
        const auto item = std::find(kHudElementNames.begin(),
            kHudElementNames.end(), name);
        if (item == kHudElementNames.end()) continue;
        const auto index = static_cast<std::size_t>(
            std::distance(kHudElementNames.begin(), item));
        const auto offset = render::HudOffset{
            static_cast<std::int16_t>(std::clamp(x, -1'000, 1'000)),
            static_cast<std::int16_t>(std::clamp(y, -1'000, 1'000)),
        };
        if (legacy) {
            const auto profile_item = std::find(kLegacyHudProfileNames.begin(),
                kLegacyHudProfileNames.end(), profile);
            if (profile_item == kLegacyHudProfileNames.end()) continue;
            const auto profile_index = static_cast<std::size_t>(
                std::distance(kLegacyHudProfileNames.begin(), profile_item));
            loaded[profile_index].offsets[index] = offset;
            loaded[profile_index + render::hud_display_profile_count]
                .offsets[index] = offset;
            found[profile_index][index] = true;
            found[profile_index + render::hud_display_profile_count][index] = true;
        } else {
            const auto profile_item = std::find(kHudProfileNames.begin(),
                kHudProfileNames.end(), profile);
            if (profile_item == kHudProfileNames.end()) continue;
            const auto profile_index = static_cast<std::size_t>(
                std::distance(kHudProfileNames.begin(), profile_item));
            loaded[profile_index].offsets[index] = offset;
            found[profile_index][index] = true;
        }
    }
    if (!std::all_of(found.begin(), found.end(), [](const auto& profile) {
            return std::all_of(profile.begin(), profile.end(),
                [](bool value) { return value; });
        })) return false;
    layouts = loaded;
    return true;
}

bool save_hud_layout(
    const std::filesystem::path& path,
    const render::HudLayoutProfiles& layouts) noexcept {
    if (path.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream output{path, std::ios::trunc};
    if (!output) return false;
    output << "SFE_HUD_LAYOUT_V4\n";
    for (std::size_t profile = 0; profile < kHudProfileNames.size(); ++profile) {
        for (std::size_t index = 0; index < kHudElementNames.size(); ++index) {
            output << kHudProfileNames[profile] << ' '
                   << kHudElementNames[index] << ' '
                   << layouts[profile].offsets[index].x << ' '
                   << layouts[profile].offsets[index].y << '\n';
        }
    }
    return static_cast<bool>(output);
}

} // namespace starfox::app
