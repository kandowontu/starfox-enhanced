#include "starfox/app/runtime_input.hpp"
#include "starfox/input/buttons.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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
#if defined(STARFOX_UWP)
    require(SDL_GetHintBoolean(SDL_HINT_JOYSTICK_WGI, false),
            "Xbox Windows Gaming Input backend was disabled at initialization");
#endif
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

    require(SDL_SetJoystickVirtualButton(
                joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, false),
            "virtual Steam Deck paddle could not release");
    bindings.reset(starfox::app::BindingDevice::gamepad);
    auto second_description = description;
    second_description.vendor_id = 0x045eU;
    second_description.product_id = 0x028eU;
    second_description.name = "Virtual XInput Controller";
    const auto second_identifier = SDL_AttachVirtualJoystick(
        &second_description);
    require(second_identifier != 0U,
            "second virtual XInput gamepad could not be attached");
    auto* second_gamepad = SDL_OpenGamepad(second_identifier);
    require(second_gamepad != nullptr,
            "second virtual XInput gamepad could not be opened");
    require(SDL_SetGamepadPlayerIndex(gamepad, 0)
                && SDL_SetGamepadPlayerIndex(second_gamepad, 1),
            "virtual gamepads could not be assigned player indexes");
    SDL_CloseGamepad(second_gamepad);
    SDL_CloseGamepad(gamepad);
    gamepad = nullptr;

    auto player_gamepads = starfox::app::open_player_gamepads();
    require(player_gamepads.size() == 2U
                && SDL_GetGamepadID(player_gamepads[0]) == identifier
                && SDL_GetGamepadID(player_gamepads[1]) == second_identifier,
            "multiple native gamepads were not opened in player-index order");
    auto* second_joystick = SDL_GetGamepadJoystick(player_gamepads[1]);
    require(second_joystick != nullptr
                && SDL_SetJoystickVirtualButton(
                    second_joystick, SDL_GAMEPAD_BUTTON_EAST, true),
            "player-two virtual gamepad could not press a button");
    SDL_UpdateGamepads();
    require((bindings.sample_gamepad_only(player_gamepads[1])
                & starfox::input::a) != 0U
                && (bindings.sample_gamepad_only(player_gamepads[0])
                    & starfox::input::a) == 0U,
            "secondary gamepad sampling leaked across EX player slots");
    for (auto* opened : player_gamepads) SDL_CloseGamepad(opened);

#if defined(STARFOX_UWP)
    char* preference_path = SDL_GetPrefPath("StarFoxEnhanced", "StarFoxEnhanced");
    require(preference_path != nullptr, "UWP preference directory is unavailable");
    const auto settings_directory = std::filesystem::path{preference_path};
    SDL_free(preference_path);
    require(starfox::app::hud_layout_settings_path() == settings_directory / "hud-layout.cfg"
            && starfox::app::pregame_settings_path() == settings_directory / "pregame.cfg"
            && starfox::app::starfox_ex_save_ram_path() == settings_directory / "starfox-ex.srm",
            "UWP settings did not stay in writable app storage");
#else
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
    const auto documents_ex_save = starfox::app::starfox_ex_save_ram_path();
    require(documents_ex_save.filename() == "starfox-ex.srm"
                && documents_ex_save.parent_path().filename()
                    == "Star Fox Enhanced",
            "Star Fox EX SRAM path is not in its Documents subfolder");
#endif
    const auto pregame_test_path = std::filesystem::temp_directory_path()
        / "starfox-enhanced-pregame-test.cfg";
    require(starfox::app::PregameSettings{}.timing_mode == 1U,
            "new pre-game settings did not default to Original pace");
    const starfox::app::PregameSettings saved_pregame{
        1U, 90U, 3U, true, true,
        3U, true, false, true, true, 1U, true, false, 5U, 1U, 70U, 30U, 3U,
        false, true};
    require(starfox::app::save_pregame_settings(
                pregame_test_path, saved_pregame),
            "pre-game settings could not be saved");
    auto loaded_pregame = starfox::app::PregameSettings{};
    require(starfox::app::load_pregame_settings(
                pregame_test_path, loaded_pregame)
                && loaded_pregame == saved_pregame,
            "pre-game settings did not round-trip");
    {
        std::ofstream legacy_pregame{pregame_test_path, std::ios::trunc};
        legacy_pregame
            << "SFE_PREGAME_V4\n"
            << "EXPERIENCE 0\nTIMING_MODE 0\nPRESENTATION_FPS 60\n"
            << "DISPLAY_MODE 0\nGOD_MODE 0\nSHOW_FPS 0\n"
            << "ANTI_ALIASING 1\nENHANCED_GRAPHICS 0\nSMOOTH_POLYS 0\n"
            << "RTX_LIGHTING 0\nVSYNC 0\nCROSSHAIR_COLOUR 0\n";
    }
    loaded_pregame = {};
    require(starfox::app::load_pregame_settings(
                pregame_test_path, loaded_pregame)
        && loaded_pregame.anti_aliasing == 2U,
            "legacy enabled FXAA was not migrated to medium strength");
    require(loaded_pregame.music_volume == 100U
                && loaded_pregame.sfx_volume == 100U
                && loaded_pregame.renderer_mode == 0U
                && loaded_pregame.render_scale == 0U
                && loaded_pregame.on_screen_controls
                && !loaded_pregame.swap_face_buttons,
            "legacy settings did not migrate to audio/GPU/native-scale defaults");
    std::error_code pregame_remove_error;
    std::filesystem::remove(pregame_test_path, pregame_remove_error);
    require(!pregame_remove_error,
            "pre-game settings test file could not be removed");
    const auto ex_save_test_path = std::filesystem::temp_directory_path()
        / "starfox-enhanced-ex-save-test.srm";
    auto saved_ex_ram = std::vector<std::uint8_t>(
        starfox::app::starfox_ex_save_ram_size);
    for (std::size_t index = 0; index < saved_ex_ram.size(); ++index) {
        saved_ex_ram[index] = static_cast<std::uint8_t>(index * 37U + 11U);
    }
    require(starfox::app::save_starfox_ex_save_ram(
                ex_save_test_path, saved_ex_ram),
            "Star Fox EX cartridge RAM could not be saved");
    auto loaded_ex_ram = std::vector<std::uint8_t>{};
    require(starfox::app::load_starfox_ex_save_ram(
                ex_save_test_path, loaded_ex_ram)
                && loaded_ex_ram == saved_ex_ram,
            "Star Fox EX cartridge RAM did not round-trip exactly");
    require(!starfox::app::save_starfox_ex_save_ram(
                ex_save_test_path,
                std::span<const std::uint8_t>{saved_ex_ram}.first(32U)),
            "truncated Star Fox EX cartridge RAM was accepted");
    std::error_code ex_save_remove_error;
    std::filesystem::remove(ex_save_test_path, ex_save_remove_error);
    require(!ex_save_remove_error,
            "Star Fox EX cartridge RAM test file could not be removed");
    const auto layout_test_path = std::filesystem::temp_directory_path()
        / "starfox-enhanced-hud-layout-test.cfg";
    starfox::render::HudLayoutProfiles saved_layouts{};
    for (std::size_t profile = 0; profile < saved_layouts.size(); ++profile) {
        for (std::size_t element = 0;
             element < saved_layouts[profile].offsets.size(); ++element) {
            const auto marker = static_cast<std::int16_t>(
                profile * saved_layouts[profile].offsets.size() + element + 1U);
            saved_layouts[profile].offsets[element] = {marker,
                static_cast<std::int16_t>(-marker)};
        }
    }
    require(starfox::app::save_hud_layout(
                layout_test_path, saved_layouts),
            "per-video-size HUD layouts could not be saved");
    starfox::render::HudLayoutProfiles loaded_layouts{};
    auto layouts_match = starfox::app::load_hud_layout(
        layout_test_path, loaded_layouts);
    for (std::size_t profile = 0;
         layouts_match && profile < saved_layouts.size(); ++profile) {
        for (std::size_t element = 0;
             element < saved_layouts[profile].offsets.size(); ++element) {
            layouts_match = loaded_layouts[profile].offsets[element].x
                    == saved_layouts[profile].offsets[element].x
                && loaded_layouts[profile].offsets[element].y
                    == saved_layouts[profile].offsets[element].y;
            if (!layouts_match) break;
        }
    }
    require(layouts_match,
            "per-experience HUD layout profiles did not round-trip independently");
    {
        std::ofstream legacy_layout{layout_test_path, std::ios::trunc};
        legacy_layout << "SFE_HUD_LAYOUT_V2\n";
        constexpr std::array profiles{"4_3", "16_9", "16_10", "21_9", "32_9"};
        constexpr std::array elements{
            "LIVES", "SHIELD", "BOMBS_BOOST", "COMMS"};
        for (std::size_t profile = 0; profile < profiles.size(); ++profile) {
            for (const auto* element : elements) {
                legacy_layout << profiles[profile] << ' ' << element << ' '
                              << static_cast<int>(profile + 1U) << " -2\n";
            }
        }
    }
    loaded_layouts = {};
    require(starfox::app::load_hud_layout(layout_test_path, loaded_layouts)
                && loaded_layouts[0][starfox::render::HudElement::lives].x == 1
                && loaded_layouts[5][starfox::render::HudElement::lives].x == 1
                && loaded_layouts[4][starfox::render::HudElement::comms].x == 5
                && loaded_layouts[9][starfox::render::HudElement::comms].x == 5,
            "legacy HUD layouts were not migrated into both experiences");
    std::error_code layout_remove_error;
    std::filesystem::remove(layout_test_path, layout_remove_error);
    require(!layout_remove_error, "HUD layout test file could not be removed");

    require(SDL_DetachVirtualJoystick(second_identifier),
            "second virtual XInput gamepad could not be detached");
    require(SDL_DetachVirtualJoystick(identifier),
            "virtual Steam Deck could not be detached");
    SDL_Quit();
    std::cout << "All runtime input tests passed.\n";
    return 0;
}
