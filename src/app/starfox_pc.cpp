#include "starfox/audio/spc700_audio.hpp"
#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/input/input_latch.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/particle_renderer.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/sprite_renderer.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/simulation/math.hpp"
#include "starfox/timing/fixed_step.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using starfox::input::ButtonMask;

class SdlContext {
public:
    SdlContext() {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
            throw std::runtime_error{std::string{"SDL_Init: "} + SDL_GetError()};
        }
    }

    ~SdlContext() { SDL_Quit(); }
    SdlContext(const SdlContext&) = delete;
    SdlContext& operator=(const SdlContext&) = delete;
};

class Window {
public:
    Window() {
        if (!SDL_CreateWindowAndRenderer(
                "Star Fox Enhanced - native PC runtime", 896, 768,
                SDL_WINDOW_RESIZABLE, &window_, &renderer_)) {
            throw std::runtime_error{
                std::string{"SDL_CreateWindowAndRenderer: "} + SDL_GetError()};
        }
        // Presentation has its own exact 60 Hz schedule below. Following the
        // display's vsync would run at 75/120/144 Hz on common PC monitors.
        SDL_SetRenderVSync(renderer_, 0);
        SDL_SetRenderLogicalPresentation(
            renderer_, 224, 192, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        texture_ = SDL_CreateTexture(
            renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 224, 192);
        if (texture_ == nullptr) {
            throw std::runtime_error{std::string{"SDL_CreateTexture: "} + SDL_GetError()};
        }
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
    }

    ~Window() {
        SDL_DestroyTexture(texture_);
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void present(
        const starfox::render::Framebuffer& framebuffer,
        std::span<const starfox::render::Rgba8> palette) {
        starfox::render::expand_rgba(framebuffer, rgba_, palette);
        if (!SDL_UpdateTexture(texture_, nullptr, rgba_.data(), 224 * 4)) {
            throw std::runtime_error{std::string{"SDL_UpdateTexture: "} + SDL_GetError()};
        }
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }

private:
    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* texture_{};
    std::vector<std::uint8_t> rgba_;
};

class AudioOutput {
public:
    AudioOutput() {
        constexpr SDL_AudioSpec spec{
            SDL_AUDIO_S16, 2,
            static_cast<int>(starfox::audio::Spc700Audio::sample_rate)};
        stream_ = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (stream_ == nullptr) {
            throw std::runtime_error{
                std::string{"SDL_OpenAudioDeviceStream: "} + SDL_GetError()};
        }
        if (!SDL_ResumeAudioStreamDevice(stream_)) {
            throw std::runtime_error{
                std::string{"SDL_ResumeAudioStreamDevice: "} + SDL_GetError()};
        }
        queue_logic_tick({});
        queue_logic_tick({});
    }

    ~AudioOutput() { SDL_DestroyAudioStream(stream_); }
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    void queue_logic_tick(
        std::span<const starfox::simulation::ApuPortWrite> writes) {
        const auto samples = emulator_.render_logic_tick(writes);
        if (!SDL_PutAudioStreamData(
                stream_, samples.data(),
                static_cast<int>(samples.size() * sizeof(samples.front())))) {
            throw std::runtime_error{
                std::string{"SDL_PutAudioStreamData: "} + SDL_GetError()};
        }
    }

private:
    starfox::audio::Spc700Audio emulator_;
    SDL_AudioStream* stream_{};
};

class PresentationPacer {
public:
    void wait_for_next_frame() {
        ++frame_;
        auto deadline = epoch_ + std::chrono::nanoseconds{
            static_cast<std::chrono::nanoseconds::rep>(
                frame_ * 1'000'000'000ULL / starfox::timing::kPresentationHz)};
        auto now = std::chrono::steady_clock::now();
        if (now < deadline) {
            std::this_thread::sleep_until(deadline);
            return;
        }
        // Do not emit a burst of catch-up presentations after a debugger stop
        // or suspended laptop; the simulation clock already clamps that gap.
        if (now - deadline > std::chrono::milliseconds{250}) {
            epoch_ = now;
            frame_ = 0;
        }
    }

private:
    std::chrono::steady_clock::time_point epoch_{std::chrono::steady_clock::now()};
    std::uint64_t frame_{};
};

ButtonMask sample_keyboard() {
    const auto* keys = SDL_GetKeyboardState(nullptr);
    ButtonMask result{};
    const auto add = [&result, keys](SDL_Scancode key, ButtonMask button) {
        if (keys[key]) {
            result = static_cast<ButtonMask>(result | button);
        }
    };
    add(SDL_SCANCODE_Z, starfox::input::b);
    add(SDL_SCANCODE_A, starfox::input::y);
    add(SDL_SCANCODE_BACKSPACE, starfox::input::select);
    add(SDL_SCANCODE_RETURN, starfox::input::start);
    add(SDL_SCANCODE_UP, starfox::input::up);
    add(SDL_SCANCODE_DOWN, starfox::input::down);
    add(SDL_SCANCODE_LEFT, starfox::input::left);
    add(SDL_SCANCODE_RIGHT, starfox::input::right);
    add(SDL_SCANCODE_X, starfox::input::a);
    add(SDL_SCANCODE_S, starfox::input::x);
    add(SDL_SCANCODE_Q, starfox::input::left_shoulder);
    add(SDL_SCANCODE_W, starfox::input::right_shoulder);
    return result;
}

ButtonMask sample_gamepad(SDL_Gamepad* gamepad) {
    if (gamepad == nullptr) {
        return 0;
    }
    ButtonMask result{};
    const auto add = [&result, gamepad](SDL_GamepadButton key, ButtonMask button) {
        if (SDL_GetGamepadButton(gamepad, key)) {
            result = static_cast<ButtonMask>(result | button);
        }
    };
    add(SDL_GAMEPAD_BUTTON_SOUTH, starfox::input::b);
    add(SDL_GAMEPAD_BUTTON_WEST, starfox::input::y);
    add(SDL_GAMEPAD_BUTTON_BACK, starfox::input::select);
    add(SDL_GAMEPAD_BUTTON_START, starfox::input::start);
    add(SDL_GAMEPAD_BUTTON_DPAD_UP, starfox::input::up);
    add(SDL_GAMEPAD_BUTTON_DPAD_DOWN, starfox::input::down);
    add(SDL_GAMEPAD_BUTTON_DPAD_LEFT, starfox::input::left);
    add(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, starfox::input::right);
    add(SDL_GAMEPAD_BUTTON_EAST, starfox::input::a);
    add(SDL_GAMEPAD_BUTTON_NORTH, starfox::input::x);
    add(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, starfox::input::left_shoulder);
    add(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, starfox::input::right_shoulder);
    return result;
}

SDL_Gamepad* open_first_gamepad() {
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    SDL_Gamepad* result = count > 0 ? SDL_OpenGamepad(gamepads[0]) : nullptr;
    SDL_free(gamepads);
    return result;
}

struct CameraPoint {
    double x{};
    double y{};
    double z{};
};

double source_word_difference(double value, double origin) noexcept {
    auto difference = std::fmod(value - origin, 65'536.0);
    if (difference > 32'767.0) difference -= 65'536.0;
    else if (difference < -32'768.0) difference += 65'536.0;
    return difference;
}

CameraPoint world_to_camera(
    double x, double y, double z,
    const starfox::timing::RenderTransform& camera,
    const starfox::simulation::MatrixQ15& matrix) {
    x = source_word_difference(x, camera.x);
    y = source_word_difference(y, camera.y);
    z = source_word_difference(z, camera.z);
    constexpr auto q15 = 32'768.0;
    return {
        (x * matrix[0] + y * matrix[3] + z * matrix[6]) / q15,
        (x * matrix[1] + y * matrix[4] + z * matrix[7]) / q15,
        (x * matrix[2] + y * matrix[5] + z * matrix[8]) / q15,
    };
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::filesystem::path rom_path;
        std::filesystem::path symbols_path;
        std::string initial_map = "TITLEMAP";
        if (argc == 1 || argc == 2) {
            const auto executable_directory =
                std::filesystem::absolute(argv[0]).parent_path();
            const auto workspace = executable_directory.parent_path().parent_path();
            const auto current = std::filesystem::current_path();
            const std::array candidates{
                std::pair{executable_directory / "SF.SFC",
                    executable_directory / "SYMBOLS.TXT"},
                std::pair{current / "SF.SFC", current / "SYMBOLS.TXT"},
                std::pair{current / "upstream-ultrastarfox" / "SF.SFC",
                    current / "upstream-ultrastarfox" / "SYMBOLS.TXT"},
                std::pair{workspace / "upstream-ultrastarfox" / "SF.SFC",
                    workspace / "upstream-ultrastarfox" / "SYMBOLS.TXT"},
            };
            for (const auto& [candidate_rom, candidate_symbols] : candidates) {
                if (std::filesystem::exists(candidate_rom)
                    && std::filesystem::exists(candidate_symbols)) {
                    rom_path = candidate_rom;
                    symbols_path = candidate_symbols;
                    break;
                }
            }
            if (rom_path.empty()) {
                throw std::runtime_error{
                    "SF.SFC and SYMBOLS.TXT were not found beside the executable "
                    "or in upstream-ultrastarfox"};
            }
            if (argc == 2) initial_map = argv[1];
        } else if (argc == 3 || argc == 4) {
            rom_path = argv[1];
            symbols_path = argv[2];
            if (argc == 4) initial_map = argv[3];
        } else {
            std::cerr << "usage: starfox_pc [MAP]\n"
                         "   or: starfox_pc ROM SYMBOLS [MAP]\n";
            return 2;
        }
        const auto rom = starfox::assets::RomImage::load(rom_path);
        const auto symbols = starfox::assets::SymbolMap::load(symbols_path);
        const starfox::assets::ShapeDecoder decoder{rom, symbols};
        const auto trigonometry = starfox::simulation::TrigTables::load(rom, symbols);
        starfox::simulation::GameSimulation game{
            rom, symbols, initial_map};
        const auto ram_symbol = [&symbols](const char* name) {
            for (const auto address : symbols.find(name)) {
                if ((address >> 16U) == 0 || (address >> 16U) == 0x7eU) return address;
            }
            throw std::runtime_error{std::string{"missing runtime RAM symbol: "} + name};
        };
        const auto mario_symbol = [&symbols](const char* name) {
            for (const auto address : symbols.find(name)) {
                if ((address >> 16U) == 0x70U) return address;
            }
            throw std::runtime_error{
                std::string{"missing runtime Super FX symbol: "} + name};
        };
        const auto colour_symbol = [&symbols](const char* name) {
            for (const auto address : symbols.find(name)) {
                if ((address >> 16U) == 0x03U) {
                    return static_cast<std::uint16_t>(address);
                }
            }
            throw std::runtime_error{std::string{"missing colour symbol: "} + name};
        };
        const auto camera_x_address = ram_symbol("VIEWPOSX");
        const auto camera_y_address = ram_symbol("VIEWPOSY");
        const auto camera_z_address = ram_symbol("VIEWPOSZ");
        const auto camera_pitch_address = ram_symbol("VIEWROTXW");
        const auto camera_yaw_address = ram_symbol("VIEWROTYW");
        const auto camera_roll_address = ram_symbol("VIEWROTZW");
        const auto game_frame_address = ram_symbol("GAMEFRAME");
        const auto background_x_address = ram_symbol("BG2XSCROLL");
        const auto background_y_address = ram_symbol("BG2SCROLL");
        const auto player_fly_mode_address = ram_symbol("PLAYERFLYMODE");
        const auto shadow_height_address = ram_symbol("SHADOWHEIGHT");
        const auto vanish_x_address = mario_symbol("M_VANISHX");
        const auto vanish_y_address = mario_symbol("M_VANISHY");
        const auto depth_colours_address = mario_symbol("M_DEPTHSTAB");
        const auto depth_thresholds_address = mario_symbol("M_DEPTHTABLE");
        const auto special_colour = colour_symbol("ID_1_C");
        const auto red_colour = colour_symbol("RED_C");
        const auto white_colour = colour_symbol("WHITE_C");
        const auto pause_text = [&symbols]() {
            for (const auto address : symbols.find("PAUSETXT")) {
                if ((address & 0xffffU) >= 0x8000U
                    && ((address >> 16U) & 0xffU) < 0x70U) return address;
            }
            throw std::runtime_error{"missing pause text symbol"};
        }();

        const SdlContext sdl;
        Window window;
        AudioOutput audio;
        SDL_Gamepad* gamepad = open_first_gamepad();
        starfox::input::InputLatch input;
        starfox::timing::FixedStepClock clock;
        PresentationPacer pacer;
        starfox::render::Framebuffer framebuffer{224, 192};
        starfox::render::RenderSettings render_settings;
        render_settings.colour_index_base = 7U * 16U;
        const starfox::render::SoftwareRenderer renderer{render_settings};
        const starfox::render::ParticleRenderer particle_renderer;
        const starfox::render::ScaledTextRenderer text_renderer{rom, symbols};
        const starfox::render::BackgroundRenderer background_renderer;
        const starfox::render::DustRenderer dust_renderer{rom, symbols};
        const starfox::render::SpriteRenderer sprite_renderer;
        using SnapshotMap = std::unordered_map<
            starfox::simulation::ObjectHandle, starfox::timing::TransformSnapshot>;
        const auto capture = [&game]() {
            SnapshotMap result;
            for (const auto handle : game.objects().active_handles()) {
                const auto& object = game.objects().at(handle);
                result.emplace(handle, starfox::timing::TransformSnapshot{
                    object.world_x, object.world_y, object.world_z,
                    static_cast<std::uint16_t>(
                        static_cast<std::uint16_t>(object.rotation_x) << 8U),
                    static_cast<std::uint16_t>(
                        static_cast<std::uint16_t>(object.rotation_y) << 8U),
                    static_cast<std::uint16_t>(
                        static_cast<std::uint16_t>(object.rotation_z) << 8U)});
            }
            return result;
        };
        auto previous = capture();
        auto current = previous;
        const auto capture_camera = [&game, camera_x_address, camera_y_address,
                                     camera_z_address, camera_pitch_address,
                                     camera_yaw_address, camera_roll_address]() {
            return starfox::timing::TransformSnapshot{
                static_cast<std::int16_t>(game.map().read_native_word(camera_x_address)),
                static_cast<std::int16_t>(game.map().read_native_word(camera_y_address)),
                static_cast<std::int16_t>(game.map().read_native_word(camera_z_address)),
                game.map().read_native_word(camera_pitch_address),
                game.map().read_native_word(camera_yaw_address),
                game.map().read_native_word(camera_roll_address)};
        };
        auto previous_camera = capture_camera();
        auto current_camera = previous_camera;
        std::unordered_map<std::uint32_t, starfox::assets::Shape> shape_cache;
        std::unordered_set<std::uint32_t> invalid_shapes;
        std::uint64_t presented_frames = 0;
        const auto test_frames_text = std::getenv("STARFOX_TEST_FRAMES");
        const auto test_frames = test_frames_text == nullptr
            ? std::uint64_t{0}
            : static_cast<std::uint64_t>(std::stoull(test_frames_text));
        auto prior_time = std::chrono::steady_clock::now();
        bool running = true;

        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_GAMEPAD_ADDED && gamepad == nullptr) {
                    gamepad = SDL_OpenGamepad(event.gdevice.which);
                } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED && gamepad != nullptr
                           && SDL_GetGamepadID(gamepad) == event.gdevice.which) {
                    SDL_CloseGamepad(gamepad);
                    gamepad = open_first_gamepad();
                }
            }

            pacer.wait_for_next_frame();
            input.sample(static_cast<ButtonMask>(sample_keyboard() | sample_gamepad(gamepad)));
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - prior_time);
            prior_time = now;
            const auto batch = clock.advance(elapsed);
            for (std::uint32_t step = 0; step < batch.simulation_steps; ++step) {
                const auto controls = input.consume();
                if ((controls.pressed & starfox::input::start) != 0
                    && (controls.held & starfox::input::select) != 0) {
                    running = false;
                }
                previous = current;
                previous_camera = current_camera;
                const auto previous_scene = game.scene_revision();
                const auto tick_result = game.tick(controls);
                current = capture();
                current_camera = capture_camera();
                if (game.scene_revision() != previous_scene) {
                    previous = current;
                    previous_camera = current_camera;
                }
                for (const auto& [handle, snapshot] : current) {
                    previous.try_emplace(handle, snapshot);
                }
                audio.queue_logic_tick(tick_result.audio_port_writes);
            }

            // PLANETSEQ is one of the cartridge's true 60 Hz loops. Its
            // textured bitmap and ship path advance once per presentation,
            // independently of the fixed 20 Hz gameplay simulation.
            game.present_frame();

            const auto background_x = static_cast<std::int16_t>(
                game.map().read_native_word(background_x_address));
            const auto background_y = static_cast<std::int16_t>(
                game.map().read_native_word(background_y_address));
            const auto& ppu = game.map().ppu_state();
            framebuffer.clear(0U);
            if (ppu.background_mode == 1U) {
                background_renderer.draw_bg3(
                    ppu, framebuffer, starfox::render::TilePriorityPass::low);
                sprite_renderer.draw_objects(ppu, framebuffer, 0U);
                if (!ppu.bg3_high_priority) {
                    background_renderer.draw_bg3(
                        ppu, framebuffer, starfox::render::TilePriorityPass::high);
                }
                sprite_renderer.draw_objects(ppu, framebuffer, 1U);
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::low);
                sprite_renderer.draw_objects(ppu, framebuffer, 2U);
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::high);
            } else if (ppu.background_mode == 2U) {
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::low);
                sprite_renderer.draw_objects(ppu, framebuffer, 0U);
                sprite_renderer.draw_objects(ppu, framebuffer, 1U);
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::high);
                sprite_renderer.draw_objects(ppu, framebuffer, 2U);
            } else if (ppu.background_mode == 3U) {
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::low);
                sprite_renderer.draw_objects(ppu, framebuffer, 0U);
                background_renderer.draw_bg1(
                    ppu, framebuffer, starfox::render::TilePriorityPass::low);
                sprite_renderer.draw_objects(ppu, framebuffer, 1U);
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::high);
                sprite_renderer.draw_objects(ppu, framebuffer, 2U);
                background_renderer.draw_bg1(
                    ppu, framebuffer, starfox::render::TilePriorityPass::high);
            } else {
                background_renderer.draw_bg2(
                    ppu, background_x, background_y, framebuffer);
                background_renderer.draw_bg3(ppu, framebuffer);
                for (std::uint8_t priority = 0; priority < 3U; ++priority) {
                    sprite_renderer.draw_objects(ppu, framebuffer, priority);
                }
            }
            struct VisibleObject {
                starfox::simulation::ObjectHandle handle{};
                starfox::timing::RenderTransform transform;
                CameraPoint position;
                double source_depth{};
            };
            std::vector<VisibleObject> visible;
            const auto camera = starfox::timing::interpolate(
                previous_camera, current_camera, batch.interpolation_alpha);
            const auto view_matrix = starfox::simulation::rotation_matrix_q15(
                trigonometry,
                static_cast<std::int16_t>(static_cast<std::uint16_t>(camera.pitch)),
                static_cast<std::int16_t>(static_cast<std::uint16_t>(camera.yaw)),
                static_cast<std::int16_t>(static_cast<std::uint16_t>(camera.roll)));
            const starfox::timing::RenderTransform source_camera{
                static_cast<double>(current_camera.x),
                static_cast<double>(current_camera.y),
                static_cast<double>(current_camera.z),
                static_cast<double>(current_camera.pitch),
                static_cast<double>(current_camera.yaw),
                static_cast<double>(current_camera.roll)};
            const auto source_view_matrix = starfox::simulation::rotation_matrix_q15(
                trigonometry,
                static_cast<std::int16_t>(current_camera.pitch),
                static_cast<std::int16_t>(current_camera.yaw),
                static_cast<std::int16_t>(current_camera.roll));
            const auto planet_screen = game.flow_state()
                == starfox::simulation::GameFlowState::planet_select
                || game.flow_state()
                == starfox::simulation::GameFlowState::planet_travel
                || game.flow_state()
                == starfox::simulation::GameFlowState::continue_choice;
            if (!planet_screen && game.map().dots_mode() < 0) {
                dust_renderer.draw(game.dust(), camera, view_matrix, framebuffer);
            } else if (!planet_screen && game.map().dots_mode() > 0) {
                dust_renderer.draw_grid(camera, view_matrix, framebuffer);
            }
            for (const auto handle : game.draw_order()) {
                if (!game.objects().is_active(handle)) continue;
                const auto& object = game.objects().at(handle);
                // invisible is sflag 27, stored in the fourth strategy byte.
                if ((object.strategy_flags[3] & 0x08U) != 0U) continue;
                const auto current_transform = current.find(handle);
                if (current_transform == current.end()) continue;
                const auto prior = previous.find(handle);
                const auto transform = starfox::timing::interpolate(
                    prior == previous.end() ? current_transform->second : prior->second,
                    current_transform->second,
                    batch.interpolation_alpha);
                const auto position = world_to_camera(
                    transform.x, transform.y, transform.z, camera, view_matrix);
                const auto source_position = world_to_camera(
                    current_transform->second.x,
                    current_transform->second.y,
                    current_transform->second.z,
                    source_camera, source_view_matrix);
                visible.push_back({handle, transform, position, source_position.z});
            }
            const auto game_frame = static_cast<std::uint8_t>(
                game.map().read_native_byte(game_frame_address) & 0x7fU);
            const auto depth_colours = game.map().read_native_word(
                depth_colours_address);
            const auto depth_thresholds = game.map().read_native_word(
                depth_thresholds_address);
            const auto display_frame = [game_frame](std::uint8_t object_frame) {
                return (object_frame & 0x80U) != 0U
                    ? static_cast<std::uint32_t>(object_frame & 0x7fU)
                    : static_cast<std::uint32_t>(game_frame);
            };
            const auto effective_colour_table = [special_colour, red_colour,
                                                   white_colour](const auto& object) {
                const auto flags = object.strategy_flags[0];
                if ((flags & 0x40U) != 0U) return std::uint16_t{};
                if ((flags & 0x02U) != 0U && (flags & 0x20U) == 0U) {
                    return static_cast<std::uint16_t>(
                        (flags & 0x01U) != 0U ? red_colour : white_colour);
                }
                return static_cast<std::uint16_t>(
                    (flags & 0x01U) != 0U ? special_colour : object.colour_table);
            };
            // The simulation captured the enabled retail
            // marioshowview/mallrotzsort list at the 20 Hz source boundary.
            // Decode headers here only for visibility/LOD metadata; never
            // resort interpolated presentation coordinates.
            for (auto& item : visible) {
                const auto& object = game.objects().at(item.handle);
                const auto colour_table = effective_colour_table(object);
                const auto base_shape_key = (static_cast<std::uint32_t>(object.shape) << 16U)
                    | colour_table;
                if (object.shape == 0 || invalid_shapes.contains(base_shape_key)) continue;
                auto base = shape_cache.find(base_shape_key);
                if (base == shape_cache.end()) {
                    try {
                        base = shape_cache.emplace(base_shape_key,
                            decoder.decode(object.shape, {}, colour_table)).first;
                    } catch (const std::exception&) {
                        invalid_shapes.insert(base_shape_key);
                        continue;
                    }
                }
            }
            std::erase_if(visible, [](const auto& item) { return item.handle == 0; });
            const auto shadows_enabled =
                (game.map().read_native_byte(player_fly_mode_address) & 0x08U) != 0U;
            const auto shadow_height = static_cast<std::int16_t>(
                game.map().read_native_word(shadow_height_address));
            const auto make_pose = [&](const VisibleObject& item, bool shadow) {
                const auto& object = game.objects().at(item.handle);
                const auto true_colour_shadow =
                    (object.strategy_flags[0] & 0x04U) != 0U;
                const auto position = shadow && !true_colour_shadow
                    ? world_to_camera(item.transform.x, shadow_height,
                        item.transform.z, camera, view_matrix)
                    : item.position;
                starfox::render::RenderPose pose;
                pose.x = position.x;
                pose.y = position.y;
                pose.z = position.z;
                pose.pitch = item.transform.pitch - camera.pitch;
                pose.yaw = item.transform.yaw - camera.yaw;
                pose.roll = item.transform.roll - camera.roll;
                pose.vanish_x = static_cast<std::int16_t>(
                    game.map().read_native_word(vanish_x_address));
                pose.vanish_y = static_cast<std::int16_t>(
                    game.map().read_native_word(vanish_y_address));
                auto object_matrix = starfox::simulation::transpose_q15(
                    starfox::simulation::rotation_matrix_q15(
                        trigonometry,
                        starfox::simulation::wrap16(-static_cast<std::int32_t>(
                            static_cast<std::uint16_t>(item.transform.pitch))),
                        starfox::simulation::wrap16(-static_cast<std::int32_t>(
                            static_cast<std::uint16_t>(item.transform.yaw))),
                        starfox::simulation::wrap16(-static_cast<std::int32_t>(
                            static_cast<std::uint16_t>(item.transform.roll)))));
                if (shadow) {
                    // mshowshadow clears rmat12/rmat22/rmat32 before the
                    // object matrix is composed with the view matrix.
                    object_matrix[1] = 0;
                    object_matrix[4] = 0;
                    object_matrix[7] = 0;
                    if (!true_colour_shadow) {
                        pose.force_colour = true;
                        pose.forced_colour = 0x09U;
                    }
                }
                pose.rotation_matrix = starfox::simulation::multiply_matrix_q15(
                    object_matrix, view_matrix);
                pose.use_rotation_matrix = true;
                pose.animation_frame = display_frame(object.animation_frame);
                pose.colour_frame = display_frame(object.colour_frame);
                pose.texture_scroll_x = object.texture_scroll_x;
                pose.texture_scroll_y = object.texture_scroll_y;
                starfox::render::apply_original_depth_tables(rom,
                    depth_thresholds, depth_colours, object.extended[21], pose);
                return pose;
            };
            // mshowview traverses the complete ordered list once for shadows,
            // then traverses it again for normal objects.
            if (shadows_enabled) {
                for (const auto& item : visible) {
                    const auto& object = game.objects().at(item.handle);
                    if ((object.strategy_flags[0] & 0x0cU) == 0U) continue;
                    const auto colour_table = effective_colour_table(object);
                    const auto base_shape_key =
                        (static_cast<std::uint32_t>(object.shape) << 16U)
                        | colour_table;
                    const auto base = shape_cache.find(base_shape_key);
                    if (base == shape_cache.end()) continue;
                    const auto shadow_pointer = base->second.header.shadow_pointer;
                    const auto shape_key =
                        (static_cast<std::uint32_t>(shadow_pointer) << 16U)
                        | colour_table;
                    if (invalid_shapes.contains(shape_key)) continue;
                    auto found = shape_cache.find(shape_key);
                    if (found == shape_cache.end()) {
                        try {
                            found = shape_cache.emplace(shape_key, decoder.decode_lod(
                                base->second.header, shadow_pointer,
                                colour_table)).first;
                        } catch (const std::exception&) {
                            invalid_shapes.insert(shape_key);
                            continue;
                        }
                    }
                    renderer.draw(found->second, make_pose(item, true),
                        framebuffer, false);
                }
            }
            for (const auto& item : visible) {
                const auto& object = game.objects().at(item.handle);
                if ((object.strategy_flags[0] & 0x04U) != 0U) continue;
                const auto colour_table = effective_colour_table(object);
                const auto base_shape_key = (static_cast<std::uint32_t>(object.shape) << 16U)
                    | colour_table;
                if (object.shape == 0 || invalid_shapes.contains(base_shape_key)) continue;
                const auto base = shape_cache.find(base_shape_key);
                if (base == shape_cache.end()) continue;
                if ((object.strategy_flags[0] & 0x10U) != 0U) {
                    particle_renderer.draw_owner(game.particles(), item.handle,
                        make_pose(item, false), batch.interpolation_alpha,
                        framebuffer);
                    continue;
                }
                if ((object.strategy_flags[0] & 0x40U) != 0U) {
                    text_renderer.draw(object.colour_table, object.extended[21],
                        std::bit_cast<std::int8_t>(object.texture_scroll_x),
                        make_pose(item, false), framebuffer);
                    continue;
                }
                const auto base_header = base->second.header;
                const auto selected_pointer = starfox::assets::ShapeDecoder::select_lod_pointer(
                    base_header, item.source_depth);
                const auto shape_key = (static_cast<std::uint32_t>(selected_pointer) << 16U)
                    | colour_table;
                auto found = shape_cache.find(shape_key);
                if (found == shape_cache.end()) {
                    try {
                        found = shape_cache.emplace(shape_key, decoder.decode_lod(
                            base_header, selected_pointer, colour_table)).first;
                    } catch (const std::exception&) {
                        invalid_shapes.insert(shape_key);
                        continue;
                    }
                }
                auto pose = make_pose(item, false);
                if ((object.strategy_flags[0] & 0x20U) != 0U) {
                    auto size_adjustment = static_cast<std::int16_t>(
                        std::bit_cast<std::int8_t>(object.texture_scroll_x));
                    for (std::uint8_t shift = 0; shift < base_header.shift; ++shift) {
                        size_adjustment = starfox::simulation::add16(
                            size_adjustment, size_adjustment);
                    }
                    auto diameter = starfox::simulation::add16(
                        base_header.size, size_adjustment);
                    diameter = starfox::simulation::add16(diameter, diameter);
                    if (diameter == 0) diameter = 1;
                    pose.simple_scaled_sprite = true;
                    pose.simple_sprite_colour = object.extended[21];
                    pose.simple_sprite_world_size = diameter;
                }
                renderer.draw(found->second, pose, framebuffer, false);
            }
            if (game.paused()) {
                text_renderer.draw_game_text(
                    pause_text, 90, 90, framebuffer);
            }
            sprite_renderer.draw_meters(game.meter_state(), framebuffer);
            sprite_renderer.draw_objects(ppu, framebuffer, 3U);
            if (ppu.background_mode == 1U && ppu.bg3_high_priority) {
                background_renderer.draw_bg3(
                    ppu, framebuffer, starfox::render::TilePriorityPass::high);
            }
            const auto base_palette = starfox::render::decode_bgr555_palette(
                game.map().ppu_state().cgram);
            const auto palette = starfox::render::apply_snes_brightness(
                base_palette, game.map().display_brightness());
            window.present(framebuffer, palette);
            ++presented_frames;
            if (test_frames != 0 && presented_frames >= test_frames) {
                running = false;
            }
        }

        if (gamepad != nullptr) {
            SDL_CloseGamepad(gamepad);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "starfox_pc failed: " << error.what() << '\n';
        return 1;
    }
}
