#include "starfox/app/runtime_input.hpp"
#include "starfox/app/touch_overlay.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/render/effect_types.hpp"
#include "starfox/render/display_aspect.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cstddef>
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
    using starfox::app::steam_virtual_gamepad_ids;
    {
        using starfox::app::TouchOverlayLayout;
        const auto layout=TouchOverlayLayout::make(852,393,{59,0,793,393});
        const auto tap=[&layout](float x,float y) {
            return layout.hit_test(x/layout.width,y/layout.height);
        };
        require(layout.dpad.x<layout.width*0.25F
            && layout.actions[0].x>layout.width*0.75F,
            "touch groups are not at safe-area screen edges");
        require(tap(layout.dpad.x-layout.unit*2,layout.dpad.y)
            ==starfox::input::left,"touch left is offset from its drawing");
        require(tap(layout.dpad.x+layout.unit*2,layout.dpad.y)
            ==starfox::input::right,"touch right is offset from its drawing");
        require(tap(layout.dpad.x,layout.dpad.y-layout.unit*2)
            ==starfox::input::up,"touch up is offset from its drawing");
        require(tap(layout.dpad.x,layout.dpad.y+layout.unit*2)
            ==starfox::input::down,"touch down is offset from its drawing");
        constexpr std::array<starfox::input::ButtonMask,4> buttons{
            starfox::input::a,starfox::input::b,starfox::input::x,
            starfox::input::y};
        for(std::size_t i=0;i<buttons.size();++i)
            require(tap(layout.actions[i].x,layout.actions[i].y)==buttons[i],
                "touch action is offset from its drawing");
        for(unsigned i=0;i<2;++i) {
            const auto shoulder=layout.shoulders[i];
            const auto system=layout.system[i];
            require(tap((shoulder.left+shoulder.right)/2,
                (shoulder.top+shoulder.bottom)/2)
                    ==(i?starfox::input::right_shoulder:starfox::input::left_shoulder),
                "touch shoulder is offset from its drawing");
            require(tap((system.left+system.right)/2,
                (system.top+system.bottom)/2)
                    ==(i?starfox::input::start:starfox::input::select),
                "touch system button is offset from its drawing");
        }
        const auto left=layout.shoulders[0],right=layout.shoulders[1];
        const auto left_touch=tap((left.left+left.right)/2,(left.top+left.bottom)/2);
        const auto right_touch=tap((right.left+right.right)/2,(right.top+right.bottom)/2);
        require(starfox::app::menu_settings_reset_chord(0,
            static_cast<starfox::input::ButtonMask>(left_touch|right_touch)),
            "two on-screen shoulders did not form the menu reset chord");
        require(starfox::app::menu_settings_reset_chord(left_touch,right_touch),
            "mapped and on-screen shoulders did not form the menu reset chord");
        require(!starfox::app::menu_settings_reset_chord(left_touch,0),
            "one shoulder formed the menu reset chord");
        require(tap(layout.width/2,layout.height/2)==0,
            "touch controls intercept the game centre");
        const auto resized=TouchOverlayLayout::make(1179,546,{80,0,1099,546});
        require(resized.hit_test(resized.actions[0].x/resized.width,
            resized.actions[0].y/resized.height)==starfox::input::a,
            "touch target did not follow window resize");
    }
    {
        using namespace starfox::app;
        TouchLayoutConfig config{};
        const auto base=TouchOverlayLayout::make(1179,546,{80,0,1099,546});
        TouchLayoutGesture gesture;
        gesture.down(1,base.dpad,base,config);
        gesture.move(1,{base.dpad.x+75,base.dpad.y-42},base,config);
        gesture.up(1,config);
        const auto moved=TouchOverlayLayout::make(1179,546,{80,0,1099,546},config);
        require(moved.dpad.x>base.dpad.x+70 && moved.dpad.y<base.dpad.y-38,
            "D-pad group did not drag in window coordinates");
        require(moved.hit_test((moved.dpad.x-moved.dpad_unit*2)/moved.width,
            moved.dpad.y/moved.height)==starfox::input::left,
            "dragged D-pad hit target disagrees with visual position");
        gesture.down(4,moved.dpad,moved,config);
        gesture.move(4,{moved.dpad.x-500,moved.dpad.y},moved,config);
        gesture.up(4,config);
        const auto edged=TouchOverlayLayout::make(1179,546,{80,0,1099,546},config);
        gesture.down(5,edged.dpad,edged,config);
        gesture.move(5,{edged.dpad.x+20,edged.dpad.y},edged,config);
        gesture.up(5,config);
        const auto unedged=TouchOverlayLayout::make(1179,546,{80,0,1099,546},config);
        require(unedged.dpad.x>edged.dpad.x+15,
            "dragging back from a safe-area edge was stuck behind hidden offset");
        for(std::size_t i=0;i<unedged.actions.size();++i)
            require(unedged.actions[i].x==base.actions[i].x
                && unedged.actions[i].y==base.actions[i].y,
                "moving the D-pad altered the face-button group");
        const auto action_centre=TouchPoint{(unedged.actions[0].x+unedged.actions[3].x)*.5F,
            (unedged.actions[1].y+unedged.actions[2].y)*.5F};
        gesture.down(2,action_centre,unedged,config);
        gesture.down(3,{action_centre.x+40,action_centre.y},unedged,config);
        gesture.move(3,{action_centre.x+90,action_centre.y},unedged,config);
        gesture.up(2,config);
        gesture.up(3,config);
        const auto grown=TouchOverlayLayout::make(1179,546,{80,0,1099,546},config);
        require(grown.action_unit>unedged.action_unit,
            "pinch did not enlarge the four face buttons together");
        constexpr std::array<starfox::input::ButtonMask,4> expected{
            starfox::input::a,starfox::input::b,starfox::input::x,starfox::input::y};
        for(std::size_t i=0;i<expected.size();++i)
            require(grown.hit_test(grown.actions[i].x/grown.width,
                grown.actions[i].y/grown.height)==expected[i],
                "resized face button hit target disagrees with visual position");
        const auto resized=TouchOverlayLayout::make(852,393,{59,0,793,393},config);
        for(std::size_t i=0;i<expected.size();++i)
            require(resized.hit_test(resized.actions[i].x/resized.width,
                resized.actions[i].y/resized.height)==expected[i],
                "custom face-button targets shifted after phone window resize");
        const auto path=std::filesystem::temp_directory_path()
            / "starfox-enhanced-touch-layout-test.cfg";
        require(save_touch_layout(path,config),"touch layout did not save");
        TouchLayoutConfig restored{};
        require(load_touch_layout(path,restored),"touch layout did not load");
        require(std::abs(restored[TouchGroup::dpad].x-config[TouchGroup::dpad].x)<.00001F
            && std::abs(restored[TouchGroup::actions].scale
                -config[TouchGroup::actions].scale)<.00001F,
            "touch layout did not round-trip");
        std::error_code error;
        std::filesystem::remove(path,error);
        require(!error,"touch layout test file could not be removed");
    }
    require(steam_virtual_gamepad_ids(0x28de,0x1205,0x28de,0x11ff),
        "Steam's Deck metadata hid its virtual transport");
    require(steam_virtual_gamepad_ids(0x045e,0x028e,0x28de,0x11ff),
        "Steam's Xbox metadata hid its virtual transport");
    require(steam_virtual_gamepad_ids(0x28de,0x11ff,0,0),
        "reported Steam virtual IDs require an unavailable transport GUID");
    require(!steam_virtual_gamepad_ids(0x28de,0x1205,0x28de,0x1205)
        && !steam_virtual_gamepad_ids(0x045e,0x028e,0x045e,0x028e)
        && !steam_virtual_gamepad_ids(0,0,0,0),
        "physical or unknown controller was classified as Steam virtual");
    for(unsigned scale:{1U,2U,4U,10U}) {
        using namespace starfox::render;
        const auto w=256U*scale,h=224U*scale;
        const auto presented=presentation_width(w,h);
        require(presented==(h*4U+1U)/3U,"native display is not corrected to 4:3");
        require(presentation_to_raster_x(float(presented),w,h)==float(w),
            "4:3 right-edge pointer does not map back to raster");
        require(presentation_to_raster_x(float(presented)/2,w,h)==float(w)/2,
            "4:3 centre pointer does not map back to raster");
        for(unsigned wide:{360U,400U,520U,800U}) {
            require(presentation_width(wide*scale,h)==wide*scale,
                "native aspect correction changed a wide canvas");
        }
    }
    require(starfox::render::device_fitted_width(224,2400,1080,256,800)==498,
        "mobile canvas did not fill the display aspect");
    require(starfox::render::device_fitted_width(224,0,1080,256,800)==256
        && starfox::render::device_fitted_width(224,8000,1080,256,800)==800,
        "mobile canvas did not handle missing or extreme dimensions");
    {
        starfox::input::InputLatch menu_input;
        menu_input.sample(starfox::input::a);
        require(menu_input.consume().pressed == starfox::input::a,
                "menu A press was not detected");
        // The runtime keeps this latch across preview rebuilds and page changes.
        for (unsigned frame = 0; frame < 480; ++frame) {
            menu_input.sample(starfox::input::a);
            require(menu_input.consume().pressed == 0,
                    "holding menu A repeated its action");
        }
        menu_input.sample(0);
        static_cast<void>(menu_input.consume());
        menu_input.sample(starfox::input::a);
        require(menu_input.consume().pressed == starfox::input::a,
                "menu A did not re-arm after release");
    }
    require(starfox::app::peek_setup_menu(true, true, false),
            "holding Tab did not hide the setup menu");
    require(!starfox::app::peek_setup_menu(true, false, false),
            "releasing Tab did not restore the setup menu");
    require(!starfox::app::peek_setup_menu(false, true, false),
            "menu peek intercepted gameplay Tab");
    require(!starfox::app::peek_setup_menu(true, true, true),
            "menu peek interfered with binding capture or the HUD editor");
    starfox::app::configure_native_gamepad_support();
#if defined(STARFOX_UWP)
    require(SDL_GetHintBoolean(SDL_HINT_JOYSTICK_WGI, false),
            "Xbox Windows Gaming Input backend was disabled at initialization");
#endif
    require(std::strcmp(SDL_GetHint(SDL_HINT_XINPUT_ENABLED), "1") == 0,
            "XInput support was not enabled before SDL initialization");
    // The application must preserve SDL's global-to-device inheritance and
    // explicit launcher choices. Test without initializing physical devices.
    const auto* deck_hint = SDL_GetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK);
    const std::string original_deck_hint = deck_hint ? deck_hint : "";
    const bool had_deck_hint = deck_hint != nullptr;
    const auto* hidapi_hint = SDL_GetHint(SDL_HINT_JOYSTICK_HIDAPI);
    const std::string original_hidapi_hint = hidapi_hint ? hidapi_hint : "";
    const bool had_hidapi_hint = hidapi_hint != nullptr;
    SDL_ResetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK);
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI, "0", SDL_HINT_OVERRIDE);
    const char* steam_allow=SDL_getenv_unsafe("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD");
    const std::string saved_steam_allow=steam_allow?steam_allow:"";
    const bool had_steam_allow=steam_allow!=nullptr;
    SDL_unsetenv_unsafe("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD");
    starfox::app::configure_native_gamepad_support();
    require(std::strcmp(SDL_getenv_unsafe("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD"),"1")==0,
        "Steam virtual gamepads remain filtered by default");
    SDL_setenv_unsafe("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD","0",1);
    starfox::app::configure_native_gamepad_support();
    require(std::strcmp(SDL_getenv_unsafe("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD"),"0")==0,
        "Explicit Steam virtual gamepad opt-out was overridden");
    if(had_steam_allow) SDL_setenv_unsafe("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD",saved_steam_allow.c_str(),1);
    else SDL_unsetenv_unsafe("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD");
    starfox::app::configure_native_gamepad_support();
    require(!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK,
                SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, true)),
            "application overrode launcher's disabled HIDAPI backend");
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, "0", SDL_HINT_OVERRIDE);
    starfox::app::configure_native_gamepad_support();
    require(!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, true),
            "application overrode explicit Deck backend disable");
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, "1", SDL_HINT_OVERRIDE);
    starfox::app::configure_native_gamepad_support();
    require(SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, false),
            "application overrode explicit Deck backend enable");
    SDL_ResetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK);
    SDL_ResetHint(SDL_HINT_JOYSTICK_HIDAPI);
    if(had_deck_hint) SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK,
        original_deck_hint.c_str(), SDL_HINT_OVERRIDE);
    if(had_hidapi_hint) SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI,
        original_hidapi_hint.c_str(), SDL_HINT_OVERRIDE);
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
    require(starfox::app::handheld_menu_layout_default(),
            "raw Steam Deck controller did not enable the native menu layout");
    require(starfox::app::handheld_menu_layout_identity("Valve", "Jupiter")
            && starfox::app::handheld_menu_layout_identity("Valve", "Galileo")
            && starfox::app::handheld_menu_layout_identity("Moorechip", "Retroid Pocket Flip")
            && !starfox::app::handheld_menu_layout_identity("Dell", "Latitude"),
            "handheld menu device identity was misclassified");
    for (const auto* virtual_name : {"Steam Input compatibility fixture",
                                    "Steam Deck Virtual Controller",
                                    "Steam Virtual Gamepad - Steam Deck"}) {
        auto steam_description=description;
        steam_description.product_id=0x11ffU;
        // Detection must work from Valve's device ID, not a required name.
        steam_description.name=virtual_name;
        const auto steam_id=SDL_AttachVirtualJoystick(&steam_description);
        require(steam_id!=0,"Steam Input virtual fixture could not attach");
        auto* preferred=starfox::app::open_preferred_gamepad();
        require(preferred && SDL_GetGamepadID(preferred)==steam_id,
            "raw Deck controller displaced Steam Input");
        // Exercise the selected SDL stream, not only its identity. Simultaneous
        // raw-device input must not leak into Steam's translated controls.
        auto* translated=SDL_GetGamepadJoystick(preferred);
        auto* raw=SDL_GetGamepadJoystick(gamepad);
        starfox::app::InputBindings translated_bindings;
        require(SDL_SetJoystickVirtualButton(raw,SDL_GAMEPAD_BUTTON_EAST,true),
            "raw duplicate input could not be set");
        for(const auto button:{SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_START}) {
            require(SDL_SetJoystickVirtualButton(translated,button,true),
                "Steam translated input could not be set");
            SDL_UpdateGamepads();
            const auto expected=button==SDL_GAMEPAD_BUTTON_SOUTH
                ?starfox::input::b:starfox::input::start;
            require(translated_bindings.sample_gamepad_only(preferred)==expected,
                "Steam translated button lost or raw duplicate leaked through");
            require(SDL_SetJoystickVirtualButton(translated,button,false),
                "Steam translated input could not be released");
            SDL_UpdateGamepads();
            require(translated_bindings.sample_gamepad_only(preferred)==0,
                "Steam translated release retained raw duplicate input");
        }
        require(SDL_SetJoystickVirtualButton(raw,SDL_GAMEPAD_BUTTON_EAST,false),
            "raw duplicate input could not be released");
        SDL_UpdateGamepads();
        SDL_CloseGamepad(preferred);
        auto players=starfox::app::open_player_gamepads();
        require(players.size()==1 && SDL_GetGamepadID(players.front())==steam_id,
            "raw Deck controller was duplicated as an EX player");
        for(auto* player:players) SDL_CloseGamepad(player);
        require(SDL_DetachVirtualJoystick(steam_id),"Steam Input fixture could not detach");
        preferred=starfox::app::open_preferred_gamepad();
        require(preferred && SDL_GetGamepadID(preferred)==identifier,
            "native Deck fallback failed after Steam Input disconnected");
        SDL_CloseGamepad(preferred);
    }
    auto* joystick = SDL_GetGamepadJoystick(gamepad);
    require(joystick != nullptr, "opened gamepad has no joystick interface");

    starfox::app::InputBindings bindings;
    {
        using Hold = starfox::app::MenuSettingsResetHold;
        Hold hold;
        const auto start = Hold::clock::time_point{std::chrono::seconds{1}};
        require(!hold.update(true,start), "L+R reset fired on press");
        require(!hold.update(true,start+std::chrono::milliseconds{4999}),
            "L+R reset fired before five seconds");
        require(hold.update(true,start+std::chrono::seconds{5}),
            "L+R reset did not fire at five seconds");
        require(!hold.update(true,start+std::chrono::seconds{6}),
            "L+R reset repeated while held");
        require(!hold.update(false,start+std::chrono::seconds{7})
                && !hold.update(true,start+std::chrono::seconds{8})
                && hold.update(true,start+std::chrono::seconds{13}),
            "L+R reset did not require a fresh complete hold");
    }
    {
        SDL_KeyboardEvent key{};
        key.type = SDL_EVENT_KEY_DOWN;
        key.scancode = SDL_SCANCODE_R;
        key.mod = SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT;
        require(bindings.matches_reset_shortcut(key), "default Ctrl+Shift+R did not reset");
        key.mod = SDL_KMOD_LCTRL;
        auto god_key = key;
        god_key.scancode = SDL_SCANCODE_F12;
        god_key.mod = SDL_KMOD_LCTRL | SDL_KMOD_LALT;
        require(bindings.matches_god_mode_shortcut(god_key), "Ctrl+Alt+F12 did not toggle god mode");
        god_key.mod = SDL_KMOD_RCTRL | SDL_KMOD_RALT;
        require(bindings.matches_god_mode_shortcut(god_key), "right-side god-mode modifiers failed");
        god_key.repeat = true;
        require(!bindings.matches_god_mode_shortcut(god_key), "god-mode key repeat retriggered");
        god_key.repeat = false;
        god_key.mod = SDL_KMOD_CTRL;
        require(!bindings.matches_god_mode_shortcut(god_key), "god-mode hotkey accepted missing Alt");
        god_key.mod = SDL_KMOD_CTRL | SDL_KMOD_ALT;
        god_key.type = SDL_EVENT_KEY_UP;
        require(!bindings.matches_god_mode_shortcut(god_key), "key release toggled god mode");
        require(!bindings.matches_reset_shortcut(key), "Ctrl alone incorrectly reset");
        key.mod = SDL_KMOD_RCTRL | SDL_KMOD_RSHIFT;
        require(bindings.matches_reset_shortcut(key), "right-side modifiers did not reset");
        key.repeat = true;
        require(!bindings.matches_reset_shortcut(key), "key repeat retriggered reset");
        key.repeat = false;
        const auto gameplay_binding = bindings.binding_name(starfox::app::BindingDevice::keyboard, 8U);
        require(bindings.bind_reset_key(SDL_SCANCODE_X), "reset suffix could not be remapped");
        require(bindings.binding_name(starfox::app::BindingDevice::keyboard, 8U) == gameplay_binding,
            "reset suffix changed the unrelated gameplay binding");
        require(!bindings.matches_reset_shortcut(key), "old reset suffix remained active");
        key.scancode = SDL_SCANCODE_X;
        require(bindings.matches_reset_shortcut(key), "new reset suffix did not trigger");
        key.mod |= SDL_KMOD_ALT;
        require(!bindings.matches_reset_shortcut(key), "extra Alt modifier incorrectly reset");
        require(!bindings.bind_reset_key(SDL_SCANCODE_LCTRL)
                && !bindings.bind_reset_key(SDL_SCANCODE_ESCAPE), "invalid reset suffix accepted");
        const auto reset_path = std::filesystem::temp_directory_path()
            / (std::string{"sfe-reset-binding-"} + std::to_string(SDL_GetPerformanceCounter()) + ".cfg");
        bindings.save(reset_path);
        starfox::app::InputBindings restored;
        restored.load(reset_path);
        require(restored.binding_name(starfox::app::BindingDevice::keyboard,
                    starfox::app::InputBindings::reset_action) == "CTRL+SHIFT+X",
            "reset binding did not survive save/load");
        std::filesystem::remove(reset_path);
        bindings.reset(starfox::app::BindingDevice::keyboard);
        require(bindings.binding_name(starfox::app::BindingDevice::keyboard,
                    starfox::app::InputBindings::reset_action) == "CTRL+SHIFT+R",
            "keyboard defaults did not restore reset suffix");
    }
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
    const auto setup_buttons=bindings.sample_fixed_menu_navigation(gamepad,true);
    require((setup_buttons & starfox::input::a)!=0 && (setup_buttons & starfox::input::b)==0,
            "setup confirm leaked gameplay Back mapping");

    require(SDL_SetJoystickVirtualButton(
                joystick, SDL_GAMEPAD_BUTTON_SOUTH, false),
            "virtual Steam Deck south button could not release");
    require(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_EAST,true),"setup back press");
    SDL_UpdateGamepads();
    const auto setup_back=bindings.sample_fixed_menu_navigation(gamepad,true);
    require((setup_back & starfox::input::b)!=0 && (setup_back & starfox::input::a)==0,
            "setup back leaked gameplay Confirm mapping");
    require(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_EAST,false),"setup back release");
    {
        starfox::app::InputBindings handheld{true};
        for (const auto [button, expected] : std::array{
             std::pair<SDL_GamepadButton, starfox::input::ButtonMask>{
                 SDL_GAMEPAD_BUTTON_SOUTH, starfox::input::b},
             std::pair<SDL_GamepadButton, starfox::input::ButtonMask>{
                 SDL_GAMEPAD_BUTTON_EAST, starfox::input::a}}) {
            require(SDL_SetJoystickVirtualButton(joystick, button, true),
                    "handheld menu button press failed");
            SDL_UpdateGamepads();
            require(handheld.sample_fixed_menu_navigation(gamepad, true) == expected
                    && handheld.sample_gamepad_only(gamepad) == expected,
                    "handheld setup menu disagreed with gameplay face positions");
            require(SDL_SetJoystickVirtualButton(joystick, button, false),
                    "handheld menu button release failed");
            SDL_UpdateGamepads();
        }
    }
    {
        starfox::app::InputBindings remapped;
        for (std::size_t action = 0; action < starfox::app::InputBindings::action_count; ++action)
            remapped.bind_gamepad_button(action, SDL_GAMEPAD_BUTTON_NORTH);
        const std::pair<SDL_GamepadButton,starfox::input::ButtonMask> fixed_buttons[]{
            {SDL_GAMEPAD_BUTTON_DPAD_LEFT,starfox::input::left},
            {SDL_GAMEPAD_BUTTON_DPAD_RIGHT,starfox::input::right},
            {SDL_GAMEPAD_BUTTON_DPAD_UP,starfox::input::up},
            {SDL_GAMEPAD_BUTTON_DPAD_DOWN,starfox::input::down},
            {SDL_GAMEPAD_BUTTON_SOUTH,starfox::input::a},
            {SDL_GAMEPAD_BUTTON_EAST,starfox::input::b},
            {SDL_GAMEPAD_BUTTON_START,starfox::input::start},
        };
        starfox::input::InputLatch navigation;
        for (const auto [button, expected] : fixed_buttons) {
            require(SDL_SetJoystickVirtualButton(joystick,button,true),"fixed navigation press failed");
            SDL_UpdateGamepads();
            const auto held = remapped.sample_fixed_gamepad_navigation(gamepad,true);
            require(held == expected,"gameplay remapping changed save-slot controls");
            navigation.sample(held);
            require(navigation.consume().pressed == expected,"fixed navigation missed press");
            navigation.sample(held);
            require(navigation.consume().pressed == 0,"held slot action repeated");
            require(SDL_SetJoystickVirtualButton(joystick,button,false),"fixed navigation release failed");
            SDL_UpdateGamepads();
            navigation.sample(remapped.sample_fixed_gamepad_navigation(gamepad,true));
            static_cast<void>(navigation.consume());
        }
    }
    const std::pair<SDL_GamepadButton,starfox::input::ButtonMask> deck_buttons[]{
        {SDL_GAMEPAD_BUTTON_SOUTH,starfox::input::b},
        {SDL_GAMEPAD_BUTTON_EAST,starfox::input::a},
        {SDL_GAMEPAD_BUTTON_WEST,starfox::input::y},
        {SDL_GAMEPAD_BUTTON_NORTH,starfox::input::x},
        {SDL_GAMEPAD_BUTTON_START,starfox::input::start},
        {SDL_GAMEPAD_BUTTON_BACK,starfox::input::select},
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,starfox::input::left_shoulder},
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,starfox::input::right_shoulder},
        {SDL_GAMEPAD_BUTTON_DPAD_UP,starfox::input::up},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN,starfox::input::down},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT,starfox::input::left},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT,starfox::input::right},
        {SDL_GAMEPAD_BUTTON_GUIDE,0}, {SDL_GAMEPAD_BUTTON_MISC1,0}};
    for(const auto [button,expected]:deck_buttons) {
        require(SDL_SetJoystickVirtualButton(joystick,button,true),"Deck button press failed");
        SDL_UpdateGamepads();
        require(bindings.sample_gamepad_only(gamepad)==expected,
            "Deck button produced wrong or additional gameplay actions");
        require(SDL_SetJoystickVirtualButton(joystick,button,false),"Deck button release failed");
        SDL_UpdateGamepads();
        require(bindings.sample_gamepad_only(gamepad)==0,"Deck release left input stuck");
    }
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
#elif defined(__APPLE__) && !defined(SDL_PLATFORM_IOS)
    const auto executable_directory = std::filesystem::path{SDL_GetBasePath()};
    starfox::app::set_portable_data_directory(executable_directory);
    char* preference_path = SDL_GetPrefPath("StarFoxEnhanced", "StarFoxEnhanced");
    require(preference_path != nullptr, "macOS preference directory is unavailable");
    const auto settings_directory = std::filesystem::path{preference_path};
    SDL_free(preference_path);
    require(settings_directory != executable_directory
            && starfox::app::pregame_settings_path() == settings_directory / "pregame.cfg"
            && starfox::app::input_bindings_path() == settings_directory / "input-bindings.cfg"
            && starfox::app::starfox_ex_save_ram_path() == settings_directory / "starfox-ex.srm"
            && starfox::app::single_instance_lock_path() == settings_directory / "runtime.lock",
        "macOS data path still points into the translocated app bundle");
#else
    const auto executable_directory = std::filesystem::path{SDL_GetBasePath()};
    require(starfox::app::hud_layout_settings_path() == executable_directory / "hud-layout.cfg"
            && starfox::app::pregame_settings_path() == executable_directory / "pregame.cfg"
            && starfox::app::starfox_ex_save_ram_path() == executable_directory / "starfox-ex.srm"
            && starfox::app::input_bindings_path() == executable_directory / "input-bindings.cfg",
        "desktop data did not default beside the executable");
    const auto fixture_root = std::filesystem::temp_directory_path()
        / (std::string{"sfe-portable-"} + std::to_string(SDL_GetPerformanceCounter()));
    const auto portable = fixture_root / "portable";
    const auto legacy = fixture_root / "legacy";
    const auto old_bindings = fixture_root / "bindings";
    std::filesystem::create_directories(portable);
    std::filesystem::create_directories(legacy);
    starfox::app::PregameSettings old_settings;
    old_settings.effect = 5U;
    auto new_settings = old_settings;
    new_settings.effect = 6U;
    require(starfox::app::save_pregame_settings(legacy / "pregame.cfg", old_settings)
            && starfox::app::save_pregame_settings(portable / "pregame.cfg", new_settings),
        "portable fixture settings could not be written");
    require(starfox::app::save_hud_layout(legacy / "hud-layout.cfg", {}), "legacy HUD fixture failed");
    const std::vector<std::uint8_t> old_sram(starfox::app::starfox_ex_save_ram_size, 0x5aU);
    require(starfox::app::save_starfox_ex_save_ram(legacy / "starfox-ex.srm", old_sram),
        "legacy SRAM fixture failed");
    starfox::app::InputBindings old_input;
    old_input.bind_reset_key(SDL_SCANCODE_T);
    old_input.save(old_bindings / "input-bindings.cfg");
    starfox::app::set_portable_data_directory(portable);
    const auto original_cwd = std::filesystem::current_path();
    std::filesystem::current_path(legacy);
    require(starfox::app::pregame_settings_path() == portable / "pregame.cfg"
            && starfox::app::input_bindings_path() == portable / "input-bindings.cfg"
            && starfox::app::single_instance_lock_path() == portable / "runtime.lock",
        "portable data followed the working directory instead of the executable");
    std::filesystem::current_path(original_cwd);
    starfox::app::migrate_legacy_data(portable, legacy, old_bindings);
    starfox::app::migrate_legacy_data(portable, legacy, old_bindings);
    starfox::app::PregameSettings migrated;
    require(starfox::app::load_pregame_settings(portable / "pregame.cfg", migrated)
            && migrated == new_settings, "migration overwrote existing portable settings");
    std::vector<std::uint8_t> migrated_sram;
    require(starfox::app::load_starfox_ex_save_ram(portable / "starfox-ex.srm", migrated_sram)
            && migrated_sram == old_sram, "migration changed SRAM bytes");
    starfox::app::InputBindings migrated_input;
    migrated_input.load();
    require(migrated_input.binding_name(starfox::app::BindingDevice::keyboard,
                starfox::app::InputBindings::reset_action) == "CTRL+SHIFT+T",
        "legacy input bindings were not migrated");
    require(std::filesystem::exists(legacy / "starfox-ex.srm")
            && std::filesystem::exists(portable / "hud-layout.cfg"),
        "migration removed originals or omitted HUD data");
    starfox::app::set_portable_data_directory(executable_directory);
    for (const auto& directory : {portable, legacy, old_bindings}) {
        for (const auto* filename : {"pregame.cfg", "hud-layout.cfg", "starfox-ex.srm", "input-bindings.cfg"}) {
            std::filesystem::remove(directory / filename);
        }
        std::filesystem::remove(directory);
    }
    std::filesystem::remove(fixture_root);
#endif
    const auto pregame_test_path = std::filesystem::temp_directory_path()
        / "starfox-enhanced-pregame-test.cfg";
    require(starfox::app::PregameSettings{}.timing_mode == 1U,
            "new pre-game settings did not default to Original pace");
    const starfox::app::PregameSettings saved_pregame{
        1U, 90U, 3U, true, true,
        3U, true, false, true, 2U, true, 1U, true, false, 5U, 1U, 70U, 30U,
        3U, false, true, 7U, 60U, 6U, 40U};
    require(starfox::app::save_pregame_settings(
                pregame_test_path, saved_pregame),
            "pre-game settings could not be saved");
    auto loaded_pregame = starfox::app::PregameSettings{};
    require(starfox::app::load_pregame_settings(
                pregame_test_path, loaded_pregame)
                && loaded_pregame == saved_pregame,
            "pre-game settings did not round-trip");
    {
        std::ifstream current{pregame_test_path};
        std::string legacy, line;
        while (std::getline(current, line)) {
            if (line.starts_with("TWO_D_FILTER ")) continue;
            legacy += (line.starts_with("SFE_PREGAME_V") ? "SFE_PREGAME_V11" : line) + "\n";
        }
        current.close();
        std::ofstream previous{pregame_test_path, std::ios::trunc};
        previous << legacy;
        previous.close();
        auto expected = saved_pregame;
        expected.two_d_filter = expected.enhanced_graphics ? 1U : 0U;
        require(starfox::app::load_pregame_settings(pregame_test_path, loaded_pregame)
                    && loaded_pregame == expected,
                "V11 migration changed unrelated settings or lost the filter");
    }
    for (std::uint8_t filter = 0; filter < 6; ++filter) {
        auto settings = saved_pregame;
        settings.two_d_filter = filter;
        require(starfox::app::save_pregame_settings(pregame_test_path, settings)
            && starfox::app::load_pregame_settings(pregame_test_path, loaded_pregame)
            && loaded_pregame == settings, "filter setting did not round-trip");
    }
    {
        require(starfox::app::save_pregame_settings(pregame_test_path, saved_pregame),
            "could not write pre-stereo migration fixture");
        std::ifstream current{pregame_test_path};
        std::string legacy, line;
        while (std::getline(current, line)) {
            if (line.starts_with("STEREO_OUTPUT ")) continue;
            legacy += (line.starts_with("SFE_PREGAME_V") ? "SFE_PREGAME_V12" : line) + "\n";
        }
        current.close();
        std::ofstream previous{pregame_test_path, std::ios::trunc};
        previous << legacy;
        previous.close();
        loaded_pregame.stereo_output = 2;
        require(starfox::app::load_pregame_settings(pregame_test_path, loaded_pregame)
            && loaded_pregame == saved_pregame && loaded_pregame.stereo_output == 0,
            "pre-stereo settings did not default to OFF");
    }
    for (std::uint8_t mode = 0; mode < 3; ++mode) {
        auto settings = saved_pregame;
        settings.stereo_output = mode;
        require(starfox::app::save_pregame_settings(pregame_test_path, settings)
            && starfox::app::load_pregame_settings(pregame_test_path, loaded_pregame)
            && loaded_pregame == settings, "stereo output did not round-trip");
    }
    for (const int invalid : {-1, 3, 256}) {
        require(starfox::app::save_pregame_settings(pregame_test_path, saved_pregame),
            "could not write stereo validation fixture");
        std::ofstream bad{pregame_test_path, std::ios::app};
        bad << "STEREO_OUTPUT " << invalid << '\n';
        bad.close();
        auto unchanged = saved_pregame;
        require(!starfox::app::load_pregame_settings(pregame_test_path, unchanged)
            && unchanged == saved_pregame, "invalid stereo output accepted or mutated settings");
    }
    {
        auto settings = saved_pregame;
        settings.stereo_output = 3;
        require(!starfox::app::save_pregame_settings(pregame_test_path, settings),
            "invalid stereo output saved");
    }
    for (std::uint8_t language = 0; language < 6; ++language) {
        auto settings = saved_pregame;
        settings.language = language;
        settings.ray_tracing = (language % 2) == 0;
        settings.infinite_bombs = (language % 2) == 0;
        settings.infinite_lives = (language % 2) != 0;
        settings.infinite_boost = (language % 2) != 0;
        settings.default_laser = language % 3;
        settings.selected_level = language == 0 ? 0U : 11U + language;
        settings.chromatic_aberration = language % 4;
        settings.hdr_effect = language % 4;
        require(starfox::app::save_pregame_settings(pregame_test_path, settings)
            && starfox::app::load_pregame_settings(pregame_test_path, loaded_pregame)
            && loaded_pregame == settings, "language setting did not round-trip");
    }
    {
        require(starfox::app::save_pregame_settings(pregame_test_path,saved_pregame),
            "could not write thickness migration fixture");
        std::ofstream legacy{pregame_test_path,std::ios::app};
        legacy << "WIREFRAME_THICKNESS 4\n";
        legacy.close();
        require(starfox::app::load_pregame_settings(pregame_test_path,loaded_pregame)
            && loaded_pregame.wireframe_thickness==1U,
            "legacy line thickness override was not ignored");
    }
    for(std::uint8_t mode=0;mode<=4;++mode) {
        auto settings=saved_pregame;settings.fsr1_mode=mode;settings.dlss_mode=4-mode;
        require(starfox::app::save_pregame_settings(pregame_test_path,settings),"FSR1 mode save failed");
        require(starfox::app::load_pregame_settings(pregame_test_path,loaded_pregame)
            && loaded_pregame.fsr1_mode==mode && loaded_pregame.dlss_mode==4-mode,
            "FSR1/DLSS preferences must round trip independently");
    }
    {
        auto settings=saved_pregame;settings.fsr1_mode=5;
        require(!starfox::app::save_pregame_settings(pregame_test_path,settings),"Invalid FSR1 mode saved");
        require(starfox::app::save_pregame_settings(pregame_test_path,saved_pregame),"FSR1 fixture save failed");
        std::ofstream corrupt{pregame_test_path,std::ios::app};corrupt<<"FSR1_MODE -1\n";corrupt.close();
        const auto before=loaded_pregame;
        require(!starfox::app::load_pregame_settings(pregame_test_path,loaded_pregame)
            && loaded_pregame==before,"Invalid FSR1 load changed live settings");
    }
    for(std::uint8_t mode=0;mode<=4;++mode) {
        auto settings=saved_pregame;settings.dlss_mode=mode;
        require(starfox::app::save_pregame_settings(pregame_test_path,settings),"DLSS mode save failed");
        require(starfox::app::load_pregame_settings(pregame_test_path,loaded_pregame)
            && loaded_pregame.dlss_mode==mode,"DLSS mode round trip failed");
    }
    {
        auto settings=saved_pregame;settings.dlss_mode=5;
        require(!starfox::app::save_pregame_settings(pregame_test_path,settings),"Invalid DLSS mode saved");
    }
    for(const bool software_shadows:{false,true}) {
        auto settings=saved_pregame;settings.enhanced_shadows=software_shadows;
        require(starfox::app::save_pregame_settings(pregame_test_path,settings),"software shadows save failed");
        std::ofstream legacy{pregame_test_path,std::ios::app};legacy<<"ENHANCED_SHADOWS 1\n";legacy.close();
        require(starfox::app::load_pregame_settings(pregame_test_path,loaded_pregame)
            && loaded_pregame.enhanced_shadows==software_shadows,"software shadows round trip/legacy precedence failed");
    }
    for(const bool ray_tracing:{false,true}) {
        for(std::uint8_t level=0;level<4;++level) {
            auto reflection=saved_pregame;reflection.ray_tracing=ray_tracing;reflection.reflective_surfaces=level;
            require(starfox::app::save_pregame_settings(pregame_test_path,reflection)
                && starfox::app::load_pregame_settings(pregame_test_path,loaded_pregame)
                && loaded_pregame.reflective_surfaces==level,"reflection setting round trip failed");
        }
        auto invalid_reflection=saved_pregame;invalid_reflection.reflective_surfaces=4;
        require(!starfox::app::save_pregame_settings(pregame_test_path,invalid_reflection),
            "invalid reflection intensity saved");
        auto settings=saved_pregame;
        settings.ray_tracing=ray_tracing;
        require(starfox::app::save_pregame_settings(pregame_test_path,settings),
            "could not write shadow migration fixture");
        std::ofstream legacy{pregame_test_path,std::ios::app};
        legacy << "ENHANCED_SHADOWS 1\n";
        legacy.close();
        require(starfox::app::load_pregame_settings(pregame_test_path,loaded_pregame)
            && !loaded_pregame.enhanced_shadows
            && loaded_pregame.ray_tracing==ray_tracing,
            "legacy Enhanced Shadows changed the ray-tracing choice");
    }
    {
        auto settings = saved_pregame;
        settings.language = 6;
        require(!starfox::app::save_pregame_settings(pregame_test_path, settings),
            "invalid language setting was saved");
    }
    for (std::uint8_t style = 0; style < starfox::render::effect_count; ++style) {
        auto settings = saved_pregame;
        settings.effect = style;
        settings.world_effect = starfox::render::effect_count - 1U - style;
        settings.effect_intensity = 70U;
        settings.world_effect_intensity = 40U;
        settings.bloom = style % 4U;
        settings.bloom_2d = (style + 2U) % 4U;
        settings.model_smoothing = (style + 1U) % 4U;
        require(starfox::app::save_pregame_settings(pregame_test_path, settings)
            && starfox::app::load_pregame_settings(pregame_test_path, loaded_pregame), "model/world styles did not load");
        if(starfox::render::manipulation(static_cast<starfox::render::Effect>(style))) {
            settings.effect=0;settings.manipulation=style;settings.manipulation_intensity=70;
        }
        if(starfox::render::material(static_cast<starfox::render::Effect>(style))) {
            settings.effect=0;settings.material=style;
        }
        require(loaded_pregame==settings,"model/world styles or legacy manipulation migration failed");
        settings.effect=1;settings.manipulation=unsigned(starfox::render::Effect::checker_fold);
        settings.manipulation_intensity=60;
        settings.material=unsigned(starfox::render::Effect::pearl);
        settings.environment={1,5,3,1,3,2};
        settings.planet_select_cheat=true;
        require(starfox::app::save_pregame_settings(pregame_test_path,settings)
            && starfox::app::load_pregame_settings(pregame_test_path,loaded_pregame)
            && loaded_pregame==settings,"independent manipulation failed round-trip");
    }
    for (std::uint8_t level = 0; level <= 3; ++level) {
        // Lighting remains independently configurable alongside all styles.
        auto lighting_settings = saved_pregame;
        lighting_settings.rtx_lighting = level;
        require(starfox::app::save_pregame_settings(pregame_test_path, lighting_settings)
                    && starfox::app::load_pregame_settings(pregame_test_path, loaded_pregame)
                    && loaded_pregame.rtx_lighting == level,
                "lighting intensity did not round-trip");
    }
    {
        std::ofstream legacy_pregame{pregame_test_path, std::ios::trunc};
        legacy_pregame
            << "SFE_PREGAME_V4\n"
            << "EXPERIENCE 0\nTIMING_MODE 0\nPRESENTATION_FPS 60\n"
            << "DISPLAY_MODE 0\nGOD_MODE 0\nSHOW_FPS 0\n"
            << "ANTI_ALIASING 1\nENHANCED_GRAPHICS 1\nSMOOTH_POLYS 0\n"
            << "RTX_LIGHTING 1\nVSYNC 0\nCROSSHAIR_COLOUR 0\n";
    }
    loaded_pregame = {};
    require(starfox::app::load_pregame_settings(
                pregame_test_path, loaded_pregame)
        && loaded_pregame.anti_aliasing == 2U,
            "legacy enabled FXAA was not migrated to medium strength");
    require(loaded_pregame.two_d_filter == 1U,
            "legacy enhanced textures did not migrate to EDGE");
    require(loaded_pregame.rtx_lighting == 3U,
            "legacy lighting On did not retain its original High strength");
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
