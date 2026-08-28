#include "starfox/audio/spc700_audio.hpp"
#include "starfox/app/runtime_input.hpp"
#include "starfox/assets/rom.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/input/buttons.hpp"
#include "starfox/input/input_latch.hpp"
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/palette.hpp"
#include "starfox/render/particle_renderer.hpp"
#include "starfox/render/presentation_history.hpp"
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
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

using starfox::input::ButtonMask;

constexpr std::uint32_t snes_width = 256U;
constexpr std::uint32_t snes_height = 224U;
constexpr std::uint32_t widescreen_16_9_width = 400U;
constexpr std::uint32_t widescreen_16_10_width = 360U;
constexpr std::uint32_t ultrawide_width = 520U;
constexpr std::uint32_t super_ultrawide_width = 800U;
constexpr std::uint32_t superfx_height = 192U;
constexpr std::int32_t superfx_offset_y = 16;
constexpr std::uint32_t superfx_ui_width = 224U;

struct RuntimeAssets {
    starfox::assets::RomImage rom;
    starfox::assets::SymbolMap symbols;
};

struct ScriptedPress {
    std::uint64_t presentation_frame{};
    ButtonMask buttons{};
};

struct PresentationEffects {
    const starfox::render::Framebuffer* overlay{};
    std::uint8_t overlay_brightness{30U};
    const starfox::render::Framebuffer* text_overlay{};
    std::uint8_t text_overlay_brightness{30U};
    const starfox::render::Framebuffer* host_overlay{};
    std::int32_t host_overlay_x{};
    std::int32_t host_overlay_y{};
    starfox::simulation::PlanetPresentationState planet;
    bool clip_circle{};
    std::int16_t circle_left{};
    std::int16_t circle_top{};
    std::int16_t circle_right{};
    std::int16_t circle_bottom{};
};

std::uint32_t display_width_for(
    starfox::simulation::DisplayMode mode) noexcept {
    switch (mode) {
    case starfox::simulation::DisplayMode::widescreen_16_9:
        return widescreen_16_9_width;
    case starfox::simulation::DisplayMode::widescreen_16_10:
        return widescreen_16_10_width;
    case starfox::simulation::DisplayMode::ultrawide_21_9:
        return ultrawide_width;
    case starfox::simulation::DisplayMode::super_ultrawide_32_9:
        return super_ultrawide_width;
    case starfox::simulation::DisplayMode::standard_4_3:
    default:
        return snes_width;
    }
}

std::size_t hud_profile_index(
    starfox::simulation::DisplayMode mode) noexcept {
    switch (mode) {
    case starfox::simulation::DisplayMode::widescreen_16_9:
        return 1U;
    case starfox::simulation::DisplayMode::widescreen_16_10:
        return 2U;
    case starfox::simulation::DisplayMode::ultrawide_21_9:
        return 3U;
    case starfox::simulation::DisplayMode::super_ultrawide_32_9:
        return 4U;
    case starfox::simulation::DisplayMode::standard_4_3:
    default:
        return 0U;
    }
}

std::string_view display_profile_name(
    starfox::simulation::DisplayMode mode) noexcept {
    switch (mode) {
    case starfox::simulation::DisplayMode::widescreen_16_9:
        return "16 BY 9";
    case starfox::simulation::DisplayMode::widescreen_16_10:
        return "16 BY 10";
    case starfox::simulation::DisplayMode::ultrawide_21_9:
        return "21 BY 9";
    case starfox::simulation::DisplayMode::super_ultrawide_32_9:
        return "32 BY 9";
    case starfox::simulation::DisplayMode::standard_4_3:
    default:
        return "4 BY 3";
    }
}

std::string_view crosshair_colour_name(
    starfox::simulation::CrosshairColour colour) noexcept {
    switch (colour) {
    case starfox::simulation::CrosshairColour::white:
        return "WHITE";
    case starfox::simulation::CrosshairColour::blue:
        return "BLUE";
    case starfox::simulation::CrosshairColour::red:
        return "RED";
    case starfox::simulation::CrosshairColour::yellow:
        return "YELLOW";
    case starfox::simulation::CrosshairColour::cyan:
        return "CYAN";
    case starfox::simulation::CrosshairColour::magenta:
        return "MAGENTA";
    case starfox::simulation::CrosshairColour::orange:
        return "ORANGE";
    case starfox::simulation::CrosshairColour::green:
    default:
        return "GREEN";
    }
}

std::optional<starfox::render::Rgba8> crosshair_tint(
    starfox::simulation::CrosshairColour colour) noexcept {
    switch (colour) {
    case starfox::simulation::CrosshairColour::white:
        return starfox::render::Rgba8{255U, 255U, 255U, 255U};
    case starfox::simulation::CrosshairColour::blue:
        return starfox::render::Rgba8{72U, 136U, 255U, 255U};
    case starfox::simulation::CrosshairColour::red:
        return starfox::render::Rgba8{255U, 64U, 64U, 255U};
    case starfox::simulation::CrosshairColour::yellow:
        return starfox::render::Rgba8{255U, 232U, 64U, 255U};
    case starfox::simulation::CrosshairColour::cyan:
        return starfox::render::Rgba8{64U, 240U, 255U, 255U};
    case starfox::simulation::CrosshairColour::magenta:
        return starfox::render::Rgba8{255U, 96U, 255U, 255U};
    case starfox::simulation::CrosshairColour::orange:
        return starfox::render::Rgba8{255U, 152U, 48U, 255U};
    case starfox::simulation::CrosshairColour::green:
    default:
        return std::nullopt;
    }
}

void apply_crosshair_tint(
    starfox::render::Palette256& palette,
    starfox::simulation::CrosshairColour colour) noexcept {
    const auto tint = crosshair_tint(colour);
    if (!tint) return;
    // SPRITES.ASM reserves OBJ palette 4 for the four tile-061 crosshair
    // quadrants. Tinting this one row cannot affect lives, bombs, portraits,
    // or map sprites. Preserve the tile's source shading while replacing hue.
    constexpr std::size_t first = 128U + 4U * 16U;
    for (std::size_t index = 1U; index < 16U; ++index) {
        const auto source = palette[first + index];
        const auto intensity = std::max({source.r, source.g, source.b});
        palette[first + index] = {
            static_cast<std::uint8_t>(
                static_cast<std::uint32_t>(tint->r) * intensity / 255U),
            static_cast<std::uint8_t>(
                static_cast<std::uint32_t>(tint->g) * intensity / 255U),
            static_cast<std::uint8_t>(
                static_cast<std::uint32_t>(tint->b) * intensity / 255U),
            255U,
        };
    }
    // MHUD's accompanying triangles use this reserved bright entry whenever
    // a non-original colour is selected.
    palette[first + 15U] = *tint;
}

struct HudRect {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};

    [[nodiscard]] bool contains(float px, float py) const noexcept {
        return px >= static_cast<float>(x)
            && py >= static_cast<float>(y)
            && px < static_cast<float>(x + width)
            && py < static_cast<float>(y + height);
    }
};

HudRect default_hud_rect(
    starfox::render::HudElement element,
    std::uint32_t width) noexcept {
    switch (element) {
    case starfox::render::HudElement::lives:
        return {12, 13, 32, 16};
    case starfox::render::HudElement::shield:
        return {20, 179, 48, 25};
    case starfox::render::HudElement::bombs_boost:
        return {static_cast<std::int32_t>(width) - 68, 178, 48, 26};
    case starfox::render::HudElement::comms:
        return {static_cast<std::int32_t>(width) / 2 - 68, 164, 136, 48};
    case starfox::render::HudElement::count:
    default:
        return {};
    }
}

HudRect placed_hud_rect(
    starfox::render::HudElement element,
    std::uint32_t width,
    const starfox::render::HudLayout& layout) noexcept {
    auto result = default_hud_rect(element, width);
    const auto offset = layout[element];
    result.x += offset.x;
    result.y += offset.y;
    return result;
}

void clamp_hud_element(
    starfox::render::HudLayout& layout,
    starfox::render::HudElement element,
    std::uint32_t width) noexcept {
    const auto base = default_hud_rect(element, width);
    auto& offset = layout[element];
    offset.x = static_cast<std::int16_t>(std::clamp<std::int32_t>(offset.x,
        -base.x,
        static_cast<std::int32_t>(width) - base.x - base.width));
    offset.y = static_cast<std::int16_t>(std::clamp<std::int32_t>(offset.y,
        -base.y, static_cast<std::int32_t>(snes_height) - base.y - base.height));
}

void clamp_hud_layout(
    starfox::render::HudLayout& layout,
    std::uint32_t width) noexcept {
    for (std::uint8_t value = 0U;
         value < static_cast<std::uint8_t>(starfox::render::HudElement::count);
         ++value) {
        clamp_hud_element(layout,
            static_cast<starfox::render::HudElement>(value), width);
    }
}

HudRect hud_reset_button_rect(std::uint32_t width) noexcept {
    return {static_cast<std::int32_t>(width) / 2 - 76, 210, 68, 14};
}

HudRect hud_done_button_rect(std::uint32_t width) noexcept {
    return {static_cast<std::int32_t>(width) / 2 + 8, 210, 52, 14};
}

std::vector<ScriptedPress> parse_scripted_presses(const char* text) {
    std::vector<ScriptedPress> result;
    if (text == nullptr || *text == '\0') return result;
    const std::string script{text};
    std::size_t begin = 0U;
    while (begin < script.size()) {
        const auto end = script.find(',', begin);
        const auto separator = script.find(':', begin);
        if (separator == std::string::npos
            || (end != std::string::npos && separator >= end)) {
            throw std::runtime_error{
                "STARFOX_TEST_PRESSES must use frame:button-mask entries"};
        }
        const auto item_end = end == std::string::npos ? script.size() : end;
        result.push_back({
            static_cast<std::uint64_t>(std::stoull(
                script.substr(begin, separator - begin), nullptr, 0)),
            static_cast<ButtonMask>(std::stoul(
                script.substr(separator + 1U, item_end - separator - 1U),
                nullptr, 0)),
        });
        begin = item_end + 1U;
    }
    return result;
}

#if defined(_WIN32) && defined(STARFOX_HAS_EMBEDDED_ASSETS)
std::span<const std::uint8_t> embedded_resource(int identifier) {
    const auto module = GetModuleHandleW(nullptr);
    const auto resource = FindResourceW(
        module, MAKEINTRESOURCEW(identifier), MAKEINTRESOURCEW(10));
    if (resource == nullptr) {
        throw std::runtime_error{"embedded Star Fox asset resource is missing"};
    }
    const auto loaded = LoadResource(module, resource);
    const auto size = SizeofResource(module, resource);
    if (loaded == nullptr || size == 0) {
        throw std::runtime_error{"embedded Star Fox asset resource is invalid"};
    }
    const auto* data = static_cast<const std::uint8_t*>(LockResource(loaded));
    if (data == nullptr) {
        throw std::runtime_error{"embedded Star Fox asset resource is invalid"};
    }
    return std::span<const std::uint8_t>{
        data, static_cast<std::size_t>(size)};
}

RuntimeAssets load_embedded_assets() {
    constexpr int rom_resource = 101;
    constexpr int symbols_resource = 102;
    const auto rom_data = embedded_resource(rom_resource);
    const auto symbol_data = embedded_resource(symbols_resource);
    return {
        starfox::assets::RomImage{
            std::vector<std::uint8_t>{rom_data.begin(), rom_data.end()}},
        starfox::assets::SymbolMap::parse(std::string_view{
            reinterpret_cast<const char*>(symbol_data.data()), symbol_data.size()}),
    };
}
#endif

RuntimeAssets load_external_assets(
    const std::filesystem::path& rom_path,
    const std::filesystem::path& symbols_path) {
    return {
        starfox::assets::RomImage::load(rom_path),
        starfox::assets::SymbolMap::load(symbols_path),
    };
}

struct HudEditorBitmap {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> rgba;
};

struct HudEditorAssets {
    HudEditorBitmap background;
    std::array<HudEditorBitmap,
        static_cast<std::size_t>(starfox::render::HudElement::count)> groups;
};

HudEditorBitmap hud_editor_bitmap_from_surface(
    SDL_Surface* source, bool colour_key) {
    if (source == nullptr) {
        throw std::runtime_error{
            std::string{"could not load HUD editor artwork: "} + SDL_GetError()};
    }
    auto* converted = SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(source);
    if (converted == nullptr) {
        throw std::runtime_error{
            std::string{"could not convert HUD editor artwork: "} + SDL_GetError()};
    }
    HudEditorBitmap result;
    result.width = static_cast<std::uint32_t>(converted->w);
    result.height = static_cast<std::uint32_t>(converted->h);
    result.rgba.resize(static_cast<std::size_t>(result.width)
        * result.height * 4U);
    const auto* pixels = static_cast<const std::uint8_t*>(converted->pixels);
    for (std::uint32_t y = 0; y < result.height; ++y) {
        const auto* source_row = pixels
            + static_cast<std::size_t>(y) * converted->pitch;
        auto* target_row = result.rgba.data()
            + static_cast<std::size_t>(y) * result.width * 4U;
        std::copy_n(source_row,
            static_cast<std::size_t>(result.width) * 4U, target_row);
        if (!colour_key) continue;
        for (std::uint32_t x = 0; x < result.width; ++x) {
            auto* pixel = target_row + static_cast<std::size_t>(x) * 4U;
            pixel[3] = pixel[0] == 1U && pixel[1] == 2U && pixel[2] == 3U
                ? 0U : 255U;
        }
    }
    SDL_DestroySurface(converted);
    return result;
}

#if defined(_WIN32) && defined(STARFOX_HAS_EMBEDDED_ASSETS)
HudEditorBitmap load_embedded_hud_editor_bitmap(
    int identifier, bool colour_key) {
    const auto data = embedded_resource(identifier);
    auto* stream = SDL_IOFromConstMem(data.data(), data.size());
    if (stream == nullptr) {
        throw std::runtime_error{
            std::string{"could not open embedded HUD artwork: "} + SDL_GetError()};
    }
    return hud_editor_bitmap_from_surface(
        SDL_LoadBMP_IO(stream, true), colour_key);
}
#else
HudEditorBitmap load_external_hud_editor_bitmap(
    const std::filesystem::path& path, bool colour_key) {
    return hud_editor_bitmap_from_surface(
        SDL_LoadBMP(path.string().c_str()), colour_key);
}
#endif

HudEditorAssets load_hud_editor_assets() {
#if defined(_WIN32) && defined(STARFOX_HAS_EMBEDDED_ASSETS)
    return {
        load_embedded_hud_editor_bitmap(103, false),
        {
            load_embedded_hud_editor_bitmap(104, true),
            load_embedded_hud_editor_bitmap(105, true),
            load_embedded_hud_editor_bitmap(106, true),
            load_embedded_hud_editor_bitmap(107, true),
        },
    };
#else
    const auto directory = std::filesystem::path{"assets"} / "hud_editor";
    return {
        load_external_hud_editor_bitmap(directory / "background.bmp", false),
        {
            load_external_hud_editor_bitmap(directory / "lives.bmp", true),
            load_external_hud_editor_bitmap(directory / "shield.bmp", true),
            load_external_hud_editor_bitmap(directory / "bombs_boost.bmp", true),
            load_external_hud_editor_bitmap(directory / "comms.bmp", true),
        },
    };
#endif
}

void compose_hud_editor(
    const HudEditorAssets& assets,
    std::uint32_t width,
    const starfox::render::HudLayout& layout,
    const starfox::render::Framebuffer& ui,
    std::span<const starfox::render::Rgba8> palette,
    std::vector<std::uint8_t>& result) {
    if (assets.background.width != super_ultrawide_width
        || assets.background.height != snes_height || width > super_ultrawide_width) {
        throw std::runtime_error{"HUD editor background has invalid dimensions"};
    }
    result.resize(static_cast<std::size_t>(width) * snes_height * 4U);
    const auto crop_x = (super_ultrawide_width - width) / 2U;
    for (std::uint32_t y = 0; y < snes_height; ++y) {
        const auto* source = assets.background.rgba.data()
            + (static_cast<std::size_t>(y) * super_ultrawide_width + crop_x) * 4U;
        auto* target = result.data()
            + static_cast<std::size_t>(y) * width * 4U;
        std::copy_n(source, static_cast<std::size_t>(width) * 4U, target);
    }
    for (std::uint8_t value = 0U;
         value < static_cast<std::uint8_t>(starfox::render::HudElement::count);
         ++value) {
        const auto element = static_cast<starfox::render::HudElement>(value);
        const auto rect = placed_hud_rect(element, width, layout);
        const auto& bitmap = assets.groups[value];
        for (std::uint32_t y = 0; y < bitmap.height; ++y) {
            for (std::uint32_t x = 0; x < bitmap.width; ++x) {
                const auto source_pixel = (static_cast<std::size_t>(y)
                    * bitmap.width + x) * 4U;
                if (bitmap.rgba[source_pixel + 3U] == 0U) continue;
                const auto target_x = rect.x + static_cast<std::int32_t>(x);
                const auto target_y = rect.y + static_cast<std::int32_t>(y);
                if (target_x < 0 || target_y < 0
                    || target_x >= static_cast<std::int32_t>(width)
                    || target_y >= static_cast<std::int32_t>(snes_height)) continue;
                const auto target_pixel = (static_cast<std::size_t>(target_y)
                    * width + static_cast<std::size_t>(target_x)) * 4U;
                std::copy_n(bitmap.rgba.data() + source_pixel, 4U,
                    result.data() + target_pixel);
            }
        }
    }
    for (std::uint32_t y = 0; y < ui.height(); ++y) {
        for (std::uint32_t x = 0; x < ui.width(); ++x) {
            const auto colour = ui.get(x, y);
            if (colour == 0U || colour >= palette.size()) continue;
            const auto pixel = (static_cast<std::size_t>(y) * width + x) * 4U;
            result[pixel] = palette[colour].r;
            result[pixel + 1U] = palette[colour].g;
            result[pixel + 2U] = palette[colour].b;
            result[pixel + 3U] = 255U;
        }
    }
}

class SdlContext {
public:
    SdlContext() {
        starfox::app::configure_native_gamepad_support();
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
                "Star Fox Enhanced - native PC runtime", 1024, 896,
                SDL_WINDOW_RESIZABLE, &window_, &renderer_)) {
            throw std::runtime_error{
                std::string{"SDL_CreateWindowAndRenderer: "} + SDL_GetError()};
        }
        SDL_ShowWindow(window_);
        static_cast<void>(SDL_SyncWindow(window_));
        // Presentation has its own exact 60 Hz schedule below. Following the
        // display's vsync would run at 75/120/144 Hz on common PC monitors.
        SDL_SetRenderVSync(renderer_, 0);
        SDL_SetRenderLogicalPresentation(
            renderer_, snes_width, snes_height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        texture_ = SDL_CreateTexture(
            renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
            snes_width, snes_height);
        if (texture_ == nullptr) {
            throw std::runtime_error{std::string{"SDL_CreateTexture: "} + SDL_GetError()};
        }
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
        // Put an actual black frame on the desktop before ROM decoding, game
        // construction or audio-device setup can begin. A merely-created SDL
        // window can remain compositor-transparent until its first present.
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderPresent(renderer_);
        static_cast<void>(SDL_SyncWindow(window_));
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
        std::span<const starfox::render::Rgba8> palette,
        const starfox::simulation::CircleEffectState& circle,
        const PresentationEffects& effects = {}) {
        ensure_dimensions(framebuffer.width(), framebuffer.height());
        starfox::render::expand_rgba(framebuffer, rgba_, palette);
        const auto composite_subtractive_overlay = [this, &framebuffer, palette](
                                                       const auto* overlay_pointer,
                                                       std::uint8_t requested_brightness) {
            if (overlay_pointer == nullptr) return;
            const auto& overlay = *overlay_pointer;
            const auto brightness = std::min<std::uint32_t>(
                requested_brightness, 30U);
            for (std::uint32_t y = 0; y < framebuffer.height(); ++y) {
                for (std::uint32_t x = 0; x < framebuffer.width(); ++x) {
                    const auto colour = overlay.get(x, y);
                    if (colour == 0U || colour >= palette.size()) continue;
                    const auto pixel = (static_cast<std::size_t>(y)
                        * framebuffer.width() + x) * 4U;
                    const auto& source = palette[colour];
                    // PLANETS.ASM fades BG2 by subtracting a fixed white
                    // colour through CGADSUB. Multiplying RGB made the
                    // Pepper/Fox layer much too bright through most of the
                    // fade because every channel remained visible. Recreate
                    // the SNES five-bit subtraction instead.
                    const auto fixed = static_cast<std::int32_t>(30U - brightness);
                    const auto fade_component = [fixed](std::uint8_t component) {
                        const auto source_five = static_cast<std::int32_t>(
                            (static_cast<std::uint32_t>(component) * 31U + 127U)
                            / 255U);
                        const auto result_five = std::max(0, source_five - fixed);
                        return static_cast<std::uint8_t>(
                            (result_five << 3U) | (result_five >> 2U));
                    };
                    rgba_[pixel] = fade_component(source.r);
                    rgba_[pixel + 1U] = fade_component(source.g);
                    rgba_[pixel + 2U] = fade_component(source.b);
                    rgba_[pixel + 3U] = source.a;
                }
            }
        };
        composite_subtractive_overlay(
            effects.overlay, effects.overlay_brightness);
        composite_subtractive_overlay(
            effects.text_overlay, effects.text_overlay_brightness);
        if (circle.active && circle.radius != 0U
            && (circle.affected_layers & 0x3fU) != 0U) {
            const auto expand_five = [](std::uint8_t value) {
                value &= 0x1fU;
                return static_cast<std::int32_t>((value << 3U) | (value >> 2U));
            };
            const std::array<std::int32_t, 3> fixed{
                expand_five(circle.red),
                expand_five(circle.green),
                expand_five(circle.blue),
            };
            const auto radius_squared = static_cast<std::int64_t>(circle.radius)
                * static_cast<std::int64_t>(circle.radius);
            const auto subtract = (circle.affected_layers & 0x80U) != 0U;
            const auto half = (circle.affected_layers & 0x40U) != 0U;
            for (std::int32_t y = 0;
                 y < static_cast<std::int32_t>(framebuffer.height()); ++y) {
                for (std::int32_t x = 0;
                     x < static_cast<std::int32_t>(framebuffer.width()); ++x) {
                    if (effects.clip_circle
                        && (x < effects.circle_left || x >= effects.circle_right
                            || y < effects.circle_top
                            || y >= effects.circle_bottom)) continue;
                    const auto dx = static_cast<std::int64_t>(x - circle.centre_x);
                    const auto dy = static_cast<std::int64_t>(y - circle.centre_y);
                    if (dx * dx + dy * dy > radius_squared) continue;
                    const auto source_index = framebuffer.get(
                        static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                    // CGADSUB bit 4 controls OBJ independently of BG1-4 and
                    // the backdrop. Star Fox's bomb program deliberately
                    // excludes sprites, so its HUD and communication OAM are
                    // not washed into the expanding disk.
                    if (source_index >= 128U
                        && (circle.affected_layers & 0x10U) == 0U) continue;
                    const auto pixel = (static_cast<std::size_t>(y)
                        * framebuffer.width()
                        + static_cast<std::size_t>(x)) * 4U;
                    for (std::size_t component = 0; component < 3U; ++component) {
                        const auto main = static_cast<std::int32_t>(
                            (static_cast<std::uint32_t>(rgba_[pixel + component])
                                * 31U + 127U) / 255U);
                        auto value = subtract ? main - (fixed[component] >> 3U)
                                              : main + (fixed[component] >> 3U);
                        if (half) value /= 2;
                        value = std::clamp(value, 0, 31);
                        rgba_[pixel + component] = static_cast<std::uint8_t>(
                            (value << 3U) | (value >> 2U));
                    }
                }
            }
        }
        if (effects.planet.isolate_fade) {
            const auto remaining = 31U - std::min<std::uint32_t>(
                effects.planet.isolate_amount, 31U);
            for (std::int32_t y = 0;
                 y < static_cast<std::int32_t>(framebuffer.height()); ++y) {
                for (std::int32_t x = 0;
                     x < static_cast<std::int32_t>(framebuffer.width()); ++x) {
                    if (x >= effects.planet.isolate_left
                        && x <= effects.planet.isolate_right
                        && y >= effects.planet.isolate_top
                        && y <= effects.planet.isolate_bottom) continue;
                    const auto pixel = (static_cast<std::size_t>(y)
                        * framebuffer.width() + static_cast<std::size_t>(x)) * 4U;
                    for (std::size_t component = 0; component < 3U; ++component) {
                        rgba_[pixel + component] = static_cast<std::uint8_t>(
                            rgba_[pixel + component] * remaining / 31U);
                    }
                }
            }
        }
        if (effects.host_overlay != nullptr) {
            const auto& overlay = *effects.host_overlay;
            const auto paint = [this, &framebuffer](
                                   std::int32_t x, std::int32_t y,
                                   std::uint8_t value) {
                if (x < 0 || y < 0
                    || x >= static_cast<std::int32_t>(framebuffer.width())
                    || y >= static_cast<std::int32_t>(framebuffer.height())) {
                    return;
                }
                const auto pixel = (static_cast<std::size_t>(y)
                    * framebuffer.width() + static_cast<std::size_t>(x)) * 4U;
                rgba_[pixel] = value;
                rgba_[pixel + 1U] = value;
                rgba_[pixel + 2U] = value;
                rgba_[pixel + 3U] = 255U;
            };
            // Draw a one-pixel black shadow first, then opaque white glyphs.
            // This host diagnostic remains legible through every cartridge
            // palette, fade, bomb circle, and planet-isolation effect.
            for (std::uint32_t y = 0; y < overlay.height(); ++y) {
                for (std::uint32_t x = 0; x < overlay.width(); ++x) {
                    if (overlay.get(x, y) == 0U) continue;
                    paint(effects.host_overlay_x + static_cast<std::int32_t>(x) + 1,
                        effects.host_overlay_y + static_cast<std::int32_t>(y) + 1,
                        0U);
                }
            }
            for (std::uint32_t y = 0; y < overlay.height(); ++y) {
                for (std::uint32_t x = 0; x < overlay.width(); ++x) {
                    if (overlay.get(x, y) == 0U) continue;
                    paint(effects.host_overlay_x + static_cast<std::int32_t>(x),
                        effects.host_overlay_y + static_cast<std::int32_t>(y),
                        255U);
                }
            }
        }
        present_rgba_pixels(framebuffer.width(), framebuffer.height(), rgba_);
    }

    void present_rgba(std::uint32_t width, std::uint32_t height,
        std::span<const std::uint8_t> rgba) {
        if (rgba.size() != static_cast<std::size_t>(width) * height * 4U) {
            throw std::invalid_argument{"RGBA presentation size mismatch"};
        }
        ensure_dimensions(width, height);
        rgba_.assign(rgba.begin(), rgba.end());
        present_rgba_pixels(width, height, rgba_);
    }

    [[nodiscard]] std::span<const std::uint8_t> rgba() const noexcept {
        return rgba_;
    }

    void save_bmp(const std::filesystem::path& path) const {
        auto* surface = SDL_CreateSurfaceFrom(
            texture_width_, texture_height_, SDL_PIXELFORMAT_RGBA32,
            const_cast<std::uint8_t*>(rgba_.data()), texture_width_ * 4U);
        if (surface == nullptr) {
            throw std::runtime_error{
                std::string{"SDL_CreateSurfaceFrom: "} + SDL_GetError()};
        }
        const auto path_text = path.string();
        const auto saved = SDL_SaveBMP(surface, path_text.c_str());
        SDL_DestroySurface(surface);
        if (!saved) {
            throw std::runtime_error{
                std::string{"SDL_SaveBMP: "} + SDL_GetError()};
        }
    }

    void set_relative_mouse_mode(bool enabled) noexcept {
        static_cast<void>(SDL_SetWindowRelativeMouseMode(window_, enabled));
    }

    [[nodiscard]] bool window_to_logical(
        float window_x, float window_y,
        float& logical_x, float& logical_y) const noexcept {
        return SDL_RenderCoordinatesFromWindow(
            renderer_, window_x, window_y, &logical_x, &logical_y);
    }

    void set_frame_debug_status(
        bool frozen, std::size_t cursor = 0U, std::size_t count = 0U) noexcept {
        constexpr std::string_view base_title =
            "Star Fox Enhanced - native PC runtime";
        if (!frozen) {
            static_cast<void>(SDL_SetWindowTitle(window_, base_title.data()));
            return;
        }
        const auto title = std::string{base_title} + " [FROZEN "
            + std::to_string(count == 0U ? 0U : cursor + 1U) + "/"
            + std::to_string(count) + "]";
        static_cast<void>(SDL_SetWindowTitle(window_, title.c_str()));
    }

private:
    void present_rgba_pixels(std::uint32_t width, std::uint32_t height,
        std::span<const std::uint8_t> rgba) {
        if (!SDL_UpdateTexture(texture_, nullptr, rgba.data(),
                static_cast<int>(width * 4U))) {
            throw std::runtime_error{std::string{"SDL_UpdateTexture: "} + SDL_GetError()};
        }
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }
    void ensure_dimensions(std::uint32_t width, std::uint32_t height) {
        if (width == texture_width_ && height == texture_height_) return;
        SDL_DestroyTexture(texture_);
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            static_cast<int>(width), static_cast<int>(height));
        if (texture_ == nullptr) {
            throw std::runtime_error{
                std::string{"SDL_CreateTexture: "} + SDL_GetError()};
        }
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
        if (!SDL_SetRenderLogicalPresentation(renderer_,
                static_cast<int>(width), static_cast<int>(height),
                SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
            throw std::runtime_error{
                std::string{"SDL_SetRenderLogicalPresentation: "}
                + SDL_GetError()};
        }
        const auto integer_scale = width <= snes_width
            ? 4U : (width <= widescreen_16_9_width ? 3U : 2U);
        SDL_SetWindowSize(window_,
            static_cast<int>(width * integer_scale),
            static_cast<int>(height * integer_scale));
        texture_width_ = width;
        texture_height_ = height;
    }

    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* texture_{};
    std::uint32_t texture_width_{snes_width};
    std::uint32_t texture_height_{snes_height};
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
    }

    ~AudioOutput() { SDL_DestroyAudioStream(stream_); }
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    void start() {
        if (started_) return;
        // Keep the device paused throughout the desktop/window preroll. The
        // previous 100 ms prime was consumed during that 1.5-second pause in
        // game activity, so source audio began with an empty SDL queue and
        // underruns on the first scheduling wobble. A small, non-emulated
        // lead-in starts the real stream with one raster phase of headroom
        // without advancing the SPC ahead of cartridge state.
        constexpr std::size_t startup_frames =
            starfox::audio::Spc700Audio::sample_rate * 64U / 1'000U;
        const std::array<std::int16_t, startup_frames * 2U> silence{};
        if (!SDL_PutAudioStreamData(stream_, silence.data(),
                static_cast<int>(silence.size() * sizeof(silence.front())))) {
            throw std::runtime_error{
                std::string{"SDL_PutAudioStreamData: "} + SDL_GetError()};
        }
        if (!SDL_ResumeAudioStreamDevice(stream_)) {
            throw std::runtime_error{
                std::string{"SDL_ResumeAudioStreamDevice: "} + SDL_GetError()};
        }
        started_ = true;
    }

    void set_paused(bool paused) {
        if (!started_ || paused == paused_) return;
        const auto succeeded = paused
            ? SDL_PauseAudioStreamDevice(stream_)
            : SDL_ResumeAudioStreamDevice(stream_);
        if (!succeeded) {
            throw std::runtime_error{
                std::string{paused ? "SDL_PauseAudioStreamDevice: "
                                   : "SDL_ResumeAudioStreamDevice: "}
                + SDL_GetError()};
        }
        paused_ = paused;
    }

    [[nodiscard]] std::array<std::uint8_t, 4> queue_logic_tick(
        std::span<const starfox::simulation::ApuPortWrite> writes,
        bool fast_forward, bool queue_output = true) {
        const auto samples = emulator_.render_logic_tick(writes);
        std::span<const std::int16_t> queued_samples{samples};
        if (fast_forward) {
            // The SPC still advances through its complete 50 ms source tick,
            // but 2x playback consumes that tick in 25 ms. Preserve stereo
            // pairs while selecting every other native sample frame so music
            // and sound effects stay synchronized with the doubled game clock.
            fast_samples_.resize(samples.size() / 2U);
            for (std::size_t destination = 0U;
                 destination < fast_samples_.size(); destination += 2U) {
                const auto source = destination * 2U;
                fast_samples_[destination] = samples[source];
                fast_samples_[destination + 1U] = samples[source + 1U];
            }
            queued_samples = fast_samples_;
        }
        if (queue_output && !SDL_PutAudioStreamData(stream_, queued_samples.data(),
                static_cast<int>(queued_samples.size()
                    * sizeof(queued_samples.front())))) {
            throw std::runtime_error{
                std::string{"SDL_PutAudioStreamData: "} + SDL_GetError()};
        }
        return emulator_.output_ports();
    }

private:
    starfox::audio::Spc700Audio emulator_;
    SDL_AudioStream* stream_{};
    std::vector<std::int16_t> fast_samples_;
    bool started_{};
    bool paused_{};
};

class PresentationPacer {
public:
    void wait_for_next_frame(
        std::uint32_t presentation_hz = starfox::timing::kPresentationHz) {
        if (presentation_hz == 0U) {
            throw std::invalid_argument{"presentation FPS cannot be zero"};
        }
        auto now = std::chrono::steady_clock::now();
        if (presentation_hz != presentation_hz_) {
            epoch_ = now;
            frame_ = 0U;
            presentation_hz_ = presentation_hz;
        }
        ++frame_;
        auto deadline = epoch_ + std::chrono::nanoseconds{
            static_cast<std::chrono::nanoseconds::rep>(
                frame_ * 1'000'000'000ULL / presentation_hz_)};
        now = std::chrono::steady_clock::now();
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
    std::uint32_t presentation_hz_{starfox::timing::kPresentationHz};
};

struct RemapMenuState {
    bool active{};
    bool waiting_for_input{};
    starfox::app::BindingDevice device{
        starfox::app::BindingDevice::gamepad};
    std::size_t action{};
};

struct HudEditorState {
    bool active{};
    std::optional<starfox::render::HudElement> dragging;
    float pointer_x{-1.0F};
    float pointer_y{-1.0F};
    float grab_x{};
    float grab_y{};
};

struct CameraPoint {
    double x{};
    double y{};
    double z{};
};

struct MouseCameraState {
    bool active{};
    double pitch_offset{};
    double yaw_offset{};
    double zoom_offset{};
};

double source_word_difference(double value, double origin) noexcept {
    auto difference = std::fmod(value - origin, 65'536.0);
    if (difference > 32'767.0) difference -= 65'536.0;
    else if (difference < -32'768.0) difference += 65'536.0;
    return difference;
}

std::int16_t interpolate_source_word(
    std::int16_t previous, std::int16_t current, double alpha) noexcept {
    alpha = std::clamp(alpha, 0.0, 1.0);
    const auto value = static_cast<std::int64_t>(std::lround(
        static_cast<double>(previous)
        + source_word_difference(current, previous) * alpha));
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

starfox::simulation::CircleEffectState interpolate_circle_effect(
    const starfox::simulation::CircleEffectState& previous,
    const starfox::simulation::CircleEffectState& current,
    double alpha) noexcept {
    alpha = std::clamp(alpha, 0.0, 1.0);
    if (!previous.active && !current.active) return {};
    if (previous.active && !current.active) return previous;
    auto result = current;
    const auto start_radius = previous.active ? previous.radius : 0U;
    result.radius = static_cast<std::uint16_t>(std::lround(
        static_cast<double>(start_radius)
        + (static_cast<double>(current.radius) - start_radius) * alpha));
    if (previous.active) {
        result.centre_x = interpolate_source_word(
            previous.centre_x, current.centre_x, alpha);
        result.centre_y = interpolate_source_word(
            previous.centre_y, current.centre_y, alpha);
        const auto interpolate_component = [alpha](
            std::uint8_t from, std::uint8_t to) {
            return static_cast<std::uint8_t>(std::lround(
                static_cast<double>(from)
                + (static_cast<double>(to) - from) * alpha));
        };
        result.red = interpolate_component(previous.red, current.red);
        result.green = interpolate_component(previous.green, current.green);
        result.blue = interpolate_component(previous.blue, current.blue);
    }
    return result;
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
        const SdlContext sdl;
        Window window;
        const auto hud_editor_art = load_hud_editor_assets();
        std::string initial_map = "BOOT";
        const auto assets = [&]() -> RuntimeAssets {
            if (argc == 1 || argc == 2) {
                if (argc == 2) initial_map = argv[1];
#if defined(_WIN32) && defined(STARFOX_HAS_EMBEDDED_ASSETS)
                return load_embedded_assets();
#else
                std::filesystem::path rom_path;
                std::filesystem::path symbols_path;
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
                return load_external_assets(rom_path, symbols_path);
#endif
            }
            if (argc == 3 || argc == 4) {
                if (argc == 4) initial_map = argv[3];
                return load_external_assets(argv[1], argv[2]);
            }
            std::cerr << "usage: starfox_pc [MAP]\n"
                         "   or: starfox_pc ROM SYMBOLS [MAP]\n";
            throw std::runtime_error{"invalid command-line arguments"};
        }();
        const auto& rom = assets.rom;
        const auto& symbols = assets.symbols;
        const starfox::assets::ShapeDecoder decoder{rom, symbols};
        const auto trigonometry = starfox::simulation::TrigTables::load(rom, symbols);
        starfox::simulation::GameSimulation game{
            rom, symbols, initial_map};
        const auto capture_pregame_settings = [&game] {
            return starfox::app::PregameSettings{
                static_cast<std::uint8_t>(game.timing_mode()),
                game.presentation_fps(),
                static_cast<std::uint8_t>(game.display_mode()),
                game.god_mode(),
                game.show_fps(),
                static_cast<std::uint8_t>(game.crosshair_colour()),
            };
        };
        const auto saved_pregame_path = starfox::app::pregame_settings_path();
        auto saved_pregame = starfox::app::PregameSettings{};
        if (starfox::app::load_pregame_settings(
                saved_pregame_path, saved_pregame)) {
            game.set_timing_mode(static_cast<starfox::simulation::TimingMode>(
                saved_pregame.timing_mode));
            game.set_presentation_fps(saved_pregame.presentation_fps);
            game.set_display_mode(static_cast<starfox::simulation::DisplayMode>(
                saved_pregame.display_mode));
            game.set_god_mode(saved_pregame.god_mode);
            game.set_show_fps(saved_pregame.show_fps);
            game.set_crosshair_colour(
                static_cast<starfox::simulation::CrosshairColour>(
                    saved_pregame.crosshair_colour));
        }
        const auto persist_pregame_changes =
            std::getenv("STARFOX_TEST_PRESSES") == nullptr
            && std::getenv("STARFOX_TEST_DISPLAY_MODE") == nullptr;
        const auto save_pregame_settings = [&] {
            if (persist_pregame_changes) {
                static_cast<void>(starfox::app::save_pregame_settings(
                    saved_pregame_path, capture_pregame_settings()));
            }
        };
        if (const auto* forced_display = std::getenv(
                "STARFOX_TEST_DISPLAY_MODE")) {
            const auto mode = std::string_view{forced_display};
            if (mode == "4_3") {
                game.set_display_mode(
                    starfox::simulation::DisplayMode::standard_4_3);
            } else if (mode == "16_9") {
                game.set_display_mode(
                    starfox::simulation::DisplayMode::widescreen_16_9);
            } else if (mode == "16_10") {
                game.set_display_mode(
                    starfox::simulation::DisplayMode::widescreen_16_10);
            } else if (mode == "21_9") {
                game.set_display_mode(
                    starfox::simulation::DisplayMode::ultrawide_21_9);
            } else if (mode == "32_9") {
                game.set_display_mode(
                    starfox::simulation::DisplayMode::super_ultrawide_32_9);
            }
        }
        const auto suppress_configurable_hud =
            std::getenv("STARFOX_TEST_HIDE_CONFIGURABLE_HUD") != nullptr;
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
        const auto player_ship_flags_address = ram_symbol("PSHIPFLAGS");
        const auto hud_rotation_address = ram_symbol("HUDROT");
        const auto shadow_height_address = ram_symbol("SHADOWHEIGHT");
        const auto stay_black_address = ram_symbol("STAYBLACK");
        const auto vanish_x_address = mario_symbol("M_VANISHX");
        const auto vanish_y_address = mario_symbol("M_VANISHY");
        const auto depth_colours_address = mario_symbol("M_DEPTHSTAB");
        const auto depth_thresholds_address = mario_symbol("M_DEPTHTABLE");
        const auto hud_colour_address = mario_symbol("M_HUDCOLOUR");
        const auto hud_flags_address = mario_symbol("M_HUDFLAGS");
        const auto special_colour = colour_symbol("ID_1_C");
        const auto red_colour = colour_symbol("RED_C");
        const auto white_colour = colour_symbol("WHITE_C");
        // TRAIL_ISTRAT is only the initializer. Its first invocation changes
        // al_strat to the internal .strat continuation 0x19 bytes later;
        // visible afterimages therefore carry this active address.
        const auto trail_strategy_address =
            symbols.find("TRAIL_ISTRAT").front() + 0x19U;
        const auto intro_laser_shape = static_cast<std::uint16_t>(
            symbols.find("ELASER2A").front());
        const auto pause_text = [&symbols]() {
            for (const auto address : symbols.find("PAUSETXT")) {
                if ((address & 0xffffU) >= 0x8000U
                    && ((address >> 16U) & 0xffU) < 0x70U) return address;
            }
            throw std::runtime_error{"missing pause text symbol"};
        }();
        const auto game_text_symbol = [&symbols](const char* name) {
            for (const auto address : symbols.find(name)) {
                if ((address & 0xffffU) >= 0x8000U
                    && ((address >> 16U) & 0xffU) < 0x70U) return address;
            }
            throw std::runtime_error{
                std::string{"missing game text symbol: "} + name};
        };
        const auto score_text = game_text_symbol("SCORETXT");
        const auto total_score_text = game_text_symbol("TOTALSCORETXT");
        const auto team_text = game_text_symbol("TEAMTXT");
        const std::array teammate_text{
            game_text_symbol("PEPPYTXT"),
            game_text_symbol("FALCOTXT"),
            game_text_symbol("SLIPPYTXT"),
        };

        AudioOutput audio;
        SDL_Gamepad* gamepad = starfox::app::open_preferred_gamepad();
        starfox::app::InputBindings bindings;
        bindings.load();
        const auto hud_layout_path = starfox::app::hud_layout_settings_path();
        starfox::render::HudLayoutProfiles hud_layouts{};
        static_cast<void>(starfox::app::load_hud_layout(
            hud_layout_path, hud_layouts));
        starfox::input::InputLatch input;
        starfox::input::InputLatch remap_input;
        RemapMenuState remap_menu;
        HudEditorState hud_editor;
        const auto save_hud_layout = [&] {
            static_cast<void>(starfox::app::save_hud_layout(
                hud_layout_path, hud_layouts));
        };
        const auto close_hud_editor = [&] {
            save_hud_layout();
            hud_editor.active = false;
            hud_editor.dragging.reset();
        };
        PresentationPacer pacer;
        starfox::timing::RasterPhaseClock raster_clock;
        starfox::timing::RasterPhaseClock frame_step_clock;
        starfox::timing::FixedStepClock realtime_raster_clock{
            starfox::timing::kPresentationHz};
        starfox::render::PresentationHistory presentation_history;
        // Standard presentation keeps the complete 256x224 PPU raster.
        // Widescreen grows the scene symmetrically to 400x224 while HUD and
        // dialogue retain their original 224x192 coordinates in a centred
        // inset layer.
        starfox::render::Framebuffer framebuffer{snes_width, snes_height};
        starfox::render::Framebuffer superfx_frame{
            snes_width, superfx_height};
        starfox::render::Framebuffer superfx_ui{
            superfx_ui_width, superfx_height};
        starfox::render::Framebuffer comms_hud{
            superfx_ui_width, superfx_height};
        starfox::render::Framebuffer superfx_hud{
            snes_width, superfx_height};
        starfox::render::Framebuffer controls_player_layer{
            snes_width, superfx_height};
        starfox::render::Framebuffer planet_overlay{snes_width, snes_height};
        starfox::render::Framebuffer planet_text_overlay{
            snes_width, snes_height};
        starfox::render::Framebuffer live_fps_overlay{64U, 12U};
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
        struct RasterMotionSnapshot {
            std::int16_t background_x{};
            std::int16_t background_y{};
            std::int16_t bg1_scroll_x{};
            std::int16_t bg1_scroll_y{};
            std::int16_t bg3_scroll_x{};
            std::int16_t bg3_scroll_y{};
            std::array<std::int16_t, 224> bg2_horizontal_offsets{};
        };
        const auto capture_raster_motion = [&game, background_x_address,
                                             background_y_address]() {
            const auto& ppu = game.map().ppu_state();
            return RasterMotionSnapshot{
                static_cast<std::int16_t>(
                    game.map().read_native_word(background_x_address)),
                static_cast<std::int16_t>(
                    game.map().read_native_word(background_y_address)),
                ppu.bg1_scroll_x,
                ppu.bg1_scroll_y,
                ppu.bg3_scroll_x,
                ppu.bg3_scroll_y,
                ppu.bg2_horizontal_offsets,
            };
        };
        auto previous_raster_motion = capture_raster_motion();
        auto current_raster_motion = previous_raster_motion;
        auto previous_circle = game.circle_effect_state();
        auto current_circle = previous_circle;
        std::unordered_map<std::uint32_t, starfox::assets::Shape> shape_cache;
        std::unordered_set<std::uint32_t> invalid_shapes;
        std::uint64_t presented_frames = 0;
        const auto test_frames_text = std::getenv("STARFOX_TEST_FRAMES");
        const auto test_frames = test_frames_text == nullptr
            ? std::uint64_t{0}
            : static_cast<std::uint64_t>(std::stoull(test_frames_text));
        const auto capture_path_text = std::getenv("STARFOX_CAPTURE_PATH");
        const auto capture_path = capture_path_text == nullptr
            ? std::filesystem::path{} : std::filesystem::path{capture_path_text};
        const auto capture_directory_text = std::getenv("STARFOX_CAPTURE_DIR");
        const auto capture_directory = capture_directory_text == nullptr
            ? std::filesystem::path{}
            : std::filesystem::path{capture_directory_text};
        if (!capture_directory.empty()) {
            std::filesystem::create_directories(capture_directory);
        }
        const auto capture_start_text = std::getenv("STARFOX_CAPTURE_START");
        const auto capture_start = capture_start_text == nullptr
            ? std::uint64_t{0}
            : static_cast<std::uint64_t>(std::stoull(capture_start_text));
        const auto scripted_presses = parse_scripted_presses(
            std::getenv("STARFOX_TEST_PRESSES"));
        const auto test_unpaced = std::getenv("STARFOX_TEST_UNPACED") != nullptr;
        const auto test_fast_forward =
            std::getenv("STARFOX_TEST_FAST_FORWARD") != nullptr;
        std::vector<starfox::simulation::ApuPortWrite> pending_audio_writes;
        std::vector<std::uint8_t> hud_editor_rgba;
        std::uint8_t audio_video_phases{};
        MouseCameraState mouse_camera;
        bool frame_frozen{};
        double last_phase_fraction{};
        bool running = true;

        // Open and synchronize the native window before the cartridge flow
        // begins. This leaves a stable one-and-a-half-second black preroll instead of
        // allowing ROM loading or the first APU upload to race the desktop
        // compositor and become audible/visible before the window appears.
        framebuffer.clear(0U);
        std::array<starfox::render::Rgba8, 256> startup_palette{};
        PresentationPacer startup_pacer;
        for (std::uint32_t frame = 0; frame < 90U; ++frame) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) running = false;
            }
            if (!running) break;
            if (!test_unpaced) startup_pacer.wait_for_next_frame();
            window.present(framebuffer, startup_palette, {});
        }
        if (running) audio.start();
        auto raster_timestamp = std::chrono::steady_clock::now();
        starfox::timing::LiveFpsCounter live_fps{
            std::chrono::milliseconds{250}};
        live_fps.reset(raster_timestamp, game.presentation_fps());
        while (running) {
            bool toggle_frame_freeze{};
            bool step_frame_forward{};
            bool step_frame_backward{};
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                const auto frame_debug_key = event.type == SDL_EVENT_KEY_DOWN
                    && !event.key.repeat
                    && (event.key.scancode == SDL_SCANCODE_F5
                        || event.key.scancode == SDL_SCANCODE_F6
                        || event.key.scancode == SDL_SCANCODE_F7);
                if (frame_debug_key) {
                    if (event.key.scancode == SDL_SCANCODE_F5) {
                        toggle_frame_freeze = true;
                    } else if (event.key.scancode == SDL_SCANCODE_F6) {
                        step_frame_forward = true;
                    } else {
                        step_frame_backward = true;
                    }
                }
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_GAMEPAD_ADDED && gamepad == nullptr) {
                    gamepad = starfox::app::open_preferred_gamepad();
                } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED && gamepad != nullptr
                           && SDL_GetGamepadID(gamepad) == event.gdevice.which) {
                    SDL_CloseGamepad(gamepad);
                    gamepad = starfox::app::open_preferred_gamepad();
                }
                if (hud_editor.active) {
                    const auto editor_width = display_width_for(
                        game.display_mode());
                    auto& editor_layout = hud_layouts[
                        hud_profile_index(game.display_mode())];
                    const auto update_pointer = [&](float x, float y) {
                        float logical_x{};
                        float logical_y{};
                        if (window.window_to_logical(
                                x, y, logical_x, logical_y)) {
                            hud_editor.pointer_x = logical_x;
                            hud_editor.pointer_y = logical_y;
                        }
                    };
                    if (event.type == SDL_EVENT_KEY_DOWN
                        && !event.key.repeat
                        && event.key.scancode == SDL_SCANCODE_ESCAPE) {
                        close_hud_editor();
                    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                        update_pointer(event.motion.x, event.motion.y);
                        if (hud_editor.dragging) {
                            const auto element = *hud_editor.dragging;
                            const auto base = default_hud_rect(
                                element, editor_width);
                            auto& offset = editor_layout[element];
                            offset.x = static_cast<std::int16_t>(std::lround(
                                hud_editor.pointer_x - hud_editor.grab_x
                                - static_cast<float>(base.x)));
                            offset.y = static_cast<std::int16_t>(std::lround(
                                hud_editor.pointer_y - hud_editor.grab_y
                                - static_cast<float>(base.y)));
                            clamp_hud_element(
                                editor_layout, element, editor_width);
                        }
                    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                               && event.button.button == SDL_BUTTON_LEFT) {
                        update_pointer(event.button.x, event.button.y);
                        if (hud_reset_button_rect(editor_width).contains(
                                hud_editor.pointer_x,
                                hud_editor.pointer_y)) {
                            editor_layout = {};
                            save_hud_layout();
                        } else if (hud_done_button_rect(editor_width).contains(
                                       hud_editor.pointer_x,
                                       hud_editor.pointer_y)) {
                            close_hud_editor();
                        } else {
                            std::optional<starfox::render::HudElement> picked;
                            auto picked_area = std::numeric_limits<std::int32_t>::max();
                            for (std::uint8_t value = 0U;
                                 value < static_cast<std::uint8_t>(
                                     starfox::render::HudElement::count);
                                 ++value) {
                                const auto element = static_cast<
                                    starfox::render::HudElement>(value);
                                const auto rect = placed_hud_rect(
                                    element, editor_width, editor_layout);
                                if (!rect.contains(hud_editor.pointer_x,
                                        hud_editor.pointer_y)) continue;
                                const auto area = rect.width * rect.height;
                                if (area < picked_area) {
                                    picked = element;
                                    picked_area = area;
                                }
                            }
                            if (picked) {
                                const auto rect = placed_hud_rect(
                                    *picked, editor_width, editor_layout);
                                hud_editor.dragging = *picked;
                                hud_editor.grab_x = hud_editor.pointer_x
                                    - static_cast<float>(rect.x);
                                hud_editor.grab_y = hud_editor.pointer_y
                                    - static_cast<float>(rect.y);
                            }
                        }
                    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP
                               && event.button.button == SDL_BUTTON_LEFT) {
                        update_pointer(event.button.x, event.button.y);
                        if (hud_editor.dragging) save_hud_layout();
                        hud_editor.dragging.reset();
                    } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                        if (hud_editor.dragging) save_hud_layout();
                        hud_editor.dragging.reset();
                    }
                }
                const auto mouse_camera_scene = game.flow_state()
                        == starfox::simulation::GameFlowState::gameplay
                    || game.flow_state()
                        == starfox::simulation::GameFlowState::training;
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                    && event.button.button == SDL_BUTTON_RIGHT
                    && mouse_camera_scene && !remap_menu.active
                    && !hud_editor.active) {
                    mouse_camera.active = true;
                    window.set_relative_mouse_mode(true);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP
                           && event.button.button == SDL_BUTTON_RIGHT) {
                    mouse_camera.active = false;
                    window.set_relative_mouse_mode(false);
                } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                    mouse_camera.active = false;
                    window.set_relative_mouse_mode(false);
                } else if (event.type == SDL_EVENT_MOUSE_MOTION
                           && mouse_camera.active) {
                    constexpr double mouse_angle_units_per_pixel = 72.0;
                    mouse_camera.yaw_offset += static_cast<double>(
                        event.motion.xrel) * mouse_angle_units_per_pixel;
                    mouse_camera.pitch_offset = std::clamp(
                        mouse_camera.pitch_offset - static_cast<double>(
                            event.motion.yrel) * mouse_angle_units_per_pixel,
                        -16'000.0, 16'000.0);
                } else if (event.type == SDL_EVENT_MOUSE_WHEEL
                           && mouse_camera.active) {
                    constexpr double zoom_units_per_wheel_step = 320.0;
                    mouse_camera.zoom_offset = std::clamp(
                        mouse_camera.zoom_offset - static_cast<double>(
                            event.wheel.y) * zoom_units_per_wheel_step,
                        -1'600.0, 12'000.0);
                }
                if (frame_debug_key || !remap_menu.active
                    || event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
                    // Keyboard capture is handled below only while the
                    // remapping screen owns input.
                } else if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    if (remap_menu.waiting_for_input) {
                        remap_menu.waiting_for_input = false;
                    } else {
                        bindings.save();
                        remap_menu.active = false;
                    }
                    remap_input.reset(
                        bindings.sample_fixed_menu_navigation(gamepad));
                } else if (remap_menu.waiting_for_input
                           && remap_menu.device
                               == starfox::app::BindingDevice::keyboard) {
                    bindings.bind_keyboard(
                        remap_menu.action, event.key.scancode);
                    bindings.save();
                    remap_menu.waiting_for_input = false;
                    remap_input.reset(
                        bindings.sample_fixed_menu_navigation(gamepad));
                }
                if (remap_menu.active && remap_menu.waiting_for_input
                    && remap_menu.device
                        == starfox::app::BindingDevice::gamepad
                    && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                    && (gamepad == nullptr
                        || event.gbutton.which == SDL_GetGamepadID(gamepad))) {
                    bindings.bind_gamepad_button(remap_menu.action,
                        static_cast<SDL_GamepadButton>(event.gbutton.button));
                    bindings.save();
                    remap_menu.waiting_for_input = false;
                    remap_input.reset(
                        bindings.sample_fixed_menu_navigation(gamepad));
                }
                if (remap_menu.active && remap_menu.waiting_for_input
                    && remap_menu.device
                        == starfox::app::BindingDevice::gamepad
                    && event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION
                    && (event.gaxis.value >= 24'000
                        || event.gaxis.value <= -24'000)
                    && (gamepad == nullptr
                        || event.gaxis.which == SDL_GetGamepadID(gamepad))) {
                    bindings.bind_gamepad_axis(remap_menu.action,
                        static_cast<SDL_GamepadAxis>(event.gaxis.axis),
                        event.gaxis.value > 0);
                    bindings.save();
                    remap_menu.waiting_for_input = false;
                    remap_input.reset(
                        bindings.sample_fixed_menu_navigation(gamepad));
                }
            }

            if (!running) break;
            if (toggle_frame_freeze) {
                frame_frozen = !frame_frozen;
                input.reset();
                remap_input.reset(
                    bindings.sample_fixed_menu_navigation(gamepad));
                presentation_history.to_live();
                if (frame_frozen) {
                    frame_step_clock.synchronize(last_phase_fraction);
                    audio.set_paused(true);
                    mouse_camera.active = false;
                    window.set_relative_mouse_mode(false);
                    window.set_frame_debug_status(true,
                        presentation_history.cursor(),
                        presentation_history.frame_count());
                } else {
                    if (const auto* live = presentation_history.current()) {
                        window.present_rgba(
                            live->width, live->height, live->rgba);
                    }
                    audio.set_paused(false);
                    window.set_frame_debug_status(false);
                    realtime_raster_clock.reset();
                    raster_timestamp = std::chrono::steady_clock::now();
                    live_fps.reset(raster_timestamp, game.presentation_fps());
                }
            }

            if (frame_frozen && step_frame_backward) {
                if (presentation_history.step_back()) {
                    const auto* frame = presentation_history.current();
                    window.present_rgba(frame->width, frame->height, frame->rgba);
                    window.set_frame_debug_status(true,
                        presentation_history.cursor(),
                        presentation_history.frame_count());
                }
                continue;
            }
            if (frame_frozen && step_frame_forward
                && !presentation_history.at_live()) {
                static_cast<void>(presentation_history.step_forward());
                const auto* frame = presentation_history.current();
                window.present_rgba(frame->width, frame->height, frame->rgba);
                window.set_frame_debug_status(true,
                    presentation_history.cursor(),
                    presentation_history.frame_count());
                continue;
            }
            const auto advance_frozen_frame = frame_frozen && step_frame_forward;
            if (frame_frozen && !advance_frozen_frame) {
                std::this_thread::sleep_for(std::chrono::milliseconds{8});
                continue;
            }

            const auto mouse_camera_scene = game.flow_state()
                    == starfox::simulation::GameFlowState::gameplay
                || game.flow_state()
                    == starfox::simulation::GameFlowState::training;
            if (mouse_camera.active
                && (!mouse_camera_scene || hud_editor.active)) {
                mouse_camera.active = false;
                window.set_relative_mouse_mode(false);
            }

            if (!test_unpaced && !advance_frozen_frame) {
                pacer.wait_for_next_frame(game.presentation_fps());
            }
            auto sampled_buttons = bindings.sample(gamepad);
            for (const auto& press : scripted_presses) {
                if (presented_frames >= press.presentation_frame
                    && presented_frames < press.presentation_frame + 3U) {
                    sampled_buttons = static_cast<ButtonMask>(
                        sampled_buttons | press.buttons);
                }
            }
            input.sample(sampled_buttons);
            remap_input.sample(
                bindings.sample_fixed_menu_navigation(gamepad));
            const auto* keyboard_state = SDL_GetKeyboardState(nullptr);
            const auto fast_forward = !advance_frozen_frame
                && (test_fast_forward || (keyboard_state[SDL_SCANCODE_TAB]
                    && !(remap_menu.active && remap_menu.waiting_for_input)));
            // Presentation FPS is independent of the cartridge's 60 Hz
            // raster. Low output rates may service multiple raster phases
            // before one draw; high rates expose fractional progress between
            // phases for smooth interpolation without accelerating gameplay.
            const auto raster_batch = [&]() {
                if (advance_frozen_frame) {
                    return frame_step_clock.advance(game.presentation_fps());
                }
                if (test_unpaced) {
                    return raster_clock.advance(
                        game.presentation_fps(), fast_forward ? 2U : 1U);
                }
                const auto now = std::chrono::steady_clock::now();
                const auto elapsed = std::chrono::duration_cast<
                    starfox::timing::FixedStepClock::duration>(
                        now - raster_timestamp);
                raster_timestamp = now;
                const auto realtime = realtime_raster_clock.advance(
                    fast_forward ? elapsed * 2 : elapsed);
                return starfox::timing::RasterPhaseBatch{
                    realtime.simulation_steps,
                    realtime.interpolation_alpha,
                };
            }();
            last_phase_fraction = raster_batch.phase_fraction;
            for (std::uint32_t phase = 0;
                 phase < raster_batch.video_phases; ++phase) {
                game.present_frame();
                if (game.logic_tick_ready()) {
                    auto controls = input.consume();
                    const auto remap_controls = remap_input.consume();
                    if ((controls.pressed & starfox::input::start) != 0
                        && (controls.held & starfox::input::select) != 0) {
                        running = false;
                    }
                    if (hud_editor.active) {
                        if ((remap_controls.pressed
                             & starfox::input::y) != 0U) {
                            hud_layouts[hud_profile_index(
                                game.display_mode())] = {};
                            save_hud_layout();
                        }
                        if ((remap_controls.pressed
                             & (starfox::input::b
                                | starfox::input::start)) != 0U) {
                            close_hud_editor();
                        }
                        controls = {};
                    } else if (remap_menu.active) {
                        if (!remap_menu.waiting_for_input) {
                            if ((remap_controls.pressed
                                 & starfox::input::up) != 0U) {
                                remap_menu.action = (remap_menu.action
                                    + starfox::app::InputBindings::action_count
                                    - 1U)
                                    % starfox::app::InputBindings::action_count;
                            } else if ((remap_controls.pressed
                                        & starfox::input::down) != 0U) {
                                remap_menu.action = (remap_menu.action + 1U)
                                    % starfox::app::InputBindings::action_count;
                            }
                            if ((remap_controls.pressed
                                 & (starfox::input::left
                                    | starfox::input::right)) != 0U) {
                                remap_menu.device = remap_menu.device
                                        == starfox::app::BindingDevice::keyboard
                                    ? starfox::app::BindingDevice::gamepad
                                    : starfox::app::BindingDevice::keyboard;
                            }
                            if ((remap_controls.pressed
                                 & starfox::input::y) != 0U) {
                                bindings.reset(remap_menu.device);
                                bindings.save();
                            }
                            if ((remap_controls.pressed
                                 & starfox::input::a) != 0U) {
                                remap_menu.waiting_for_input = true;
                            } else if ((remap_controls.pressed
                                        & (starfox::input::b
                                           | starfox::input::start)) != 0U) {
                                bindings.save();
                                remap_menu.active = false;
                            }
                        }
                        controls = {};
                    } else if (game.flow_state()
                                   == starfox::simulation::GameFlowState::pregame_menu
                               && game.pregame_page()
                                   == starfox::simulation::PregamePage::main
                               && game.pregame_selection() == 3U
                               && (controls.pressed
                                   & (starfox::input::a | starfox::input::b))
                                   != 0U) {
                        remap_menu.active = true;
                        remap_menu.waiting_for_input = false;
                        remap_input.reset(
                            bindings.sample_fixed_menu_navigation(gamepad));
                        controls = {};
                    } else if (game.flow_state()
                                   == starfox::simulation::GameFlowState::pregame_menu
                               && game.pregame_page()
                                   == starfox::simulation::PregamePage::options
                               && game.pregame_selection() == 3U
                               && (controls.pressed
                                   & (starfox::input::a
                                      | starfox::input::select)) != 0U) {
                        auto& editor_layout = hud_layouts[
                            hud_profile_index(game.display_mode())];
                        clamp_hud_layout(editor_layout,
                            display_width_for(game.display_mode()));
                        hud_editor.active = true;
                        hud_editor.dragging.reset();
                        remap_input.reset(
                            bindings.sample_fixed_menu_navigation(gamepad));
                        controls = {};
                    }
                    previous = current;
                    previous_camera = current_camera;
                    previous_raster_motion = current_raster_motion;
                    previous_circle = current_circle;
                    const auto previous_scene = game.scene_revision();
                    const auto settings_before_tick = capture_pregame_settings();
                    const auto tick_result = game.tick(controls);
                    if (capture_pregame_settings() != settings_before_tick) {
                        save_pregame_settings();
                    }
                    current = capture();
                    current_camera = capture_camera();
                    current_raster_motion = capture_raster_motion();
                    current_circle = game.circle_effect_state();
                    const auto camera_cut =
                        starfox::timing::camera_transform_is_discontinuous(
                            previous_camera, current_camera);
                    if (game.scene_revision() != previous_scene || camera_cut) {
                        // LEVEL1_1 replaces the scramble camera with ExitBase's
                        // view in one source update. Interpolating that cut put
                        // the newly spawned docking station off-screen, then
                        // huge at the right edge, for two 60 FPS presentations.
                        // Snap the complete presentation state at any such view
                        // discontinuity, just as we already do for scene loads.
                        previous = current;
                        previous_camera = current_camera;
                        previous_raster_motion = current_raster_motion;
                        previous_circle = current_circle;
                    }
                    for (const auto& [handle, snapshot] : current) {
                        previous.try_emplace(handle, snapshot);
                    }
                    pending_audio_writes.insert(pending_audio_writes.end(),
                        tick_result.audio_port_writes.begin(),
                        tick_result.audio_port_writes.end());
                }
                if (++audio_video_phases >= 3U) {
                    game.synchronize_apu_output_ports(
                        audio.queue_logic_tick(
                            pending_audio_writes, fast_forward,
                            !advance_frozen_frame));
                    pending_audio_writes.clear();
                    audio_video_phases = 0U;
                }
            }
            const auto display_width = display_width_for(game.display_mode());
            auto active_hud_layout = hud_layouts[
                hud_profile_index(game.display_mode())];
            clamp_hud_layout(active_hud_layout, display_width);
            const auto viewport_origin = static_cast<std::int32_t>(
                (display_width - snes_width) / 2U);
            const auto superfx_ui_offset_x = static_cast<std::int32_t>(
                (display_width - superfx_ui_width) / 2U);
            const auto extend_cartridge_scene = game.flow_state()
                    == starfox::simulation::GameFlowState::intro
                || game.flow_state()
                    == starfox::simulation::GameFlowState::gameplay
                || game.flow_state()
                    == starfox::simulation::GameFlowState::training
                || game.flow_state()
                    == starfox::simulation::GameFlowState::stage_results;
            const auto gameplay_hud = game.flow_state()
                    == starfox::simulation::GameFlowState::gameplay
                || game.flow_state()
                    == starfox::simulation::GameFlowState::training;
            const auto* gameplay_layout = gameplay_hud
                ? &active_hud_layout : nullptr;
            // Gameplay now exposes the complete 224-line host raster in every
            // aspect ratio. Keeping the 192-line Super FX target at 4:3 left
            // the upper and lower two tile rows unable to receive models even
            // though their BG pixels were visible. Other cartridge scenes
            // retain their original vertical window unless a wide mode is in
            // use; the +16 vanishing-point adjustment preserves screen centre.
            const auto extend_scene_vertical = gameplay_hud
                || (display_width > snes_width && extend_cartridge_scene);
            const auto anchor_edge_hud = display_width > snes_width
                && gameplay_hud;
            const auto scene_height = extend_scene_vertical
                ? snes_height : superfx_height;
            const auto scene_offset_y = extend_scene_vertical
                ? 0 : superfx_offset_y;
            framebuffer.resize(display_width, snes_height);
            superfx_frame.resize(display_width, scene_height);
            superfx_hud.resize(display_width, superfx_height);
            controls_player_layer.resize(display_width, superfx_height);
            planet_overlay.resize(display_width, snes_height);
            planet_text_overlay.resize(display_width, snes_height);
            // A paused cartridge presents one completed source frame. Do not
            // keep traversing the fractional interpolation interval while
            // source state is frozen; that made star/dust pixels alternate
            // between adjacent integer projections on high-refresh displays.
            const auto interpolation_alpha = game.paused() ? 1.0
                : game.logic_interpolation_alpha(raster_batch.phase_fraction);
            const auto interpolate_raster_word = [interpolation_alpha](
                std::int16_t previous_value, std::int16_t current_value) {
                return interpolate_source_word(
                    previous_value, current_value, interpolation_alpha);
            };
            const auto background_x = interpolate_raster_word(
                previous_raster_motion.background_x,
                current_raster_motion.background_x);
            const auto background_y = interpolate_raster_word(
                previous_raster_motion.background_y,
                current_raster_motion.background_y);
            auto ppu = game.map().ppu_state();
            ppu.bg1_scroll_x = interpolate_raster_word(
                previous_raster_motion.bg1_scroll_x,
                current_raster_motion.bg1_scroll_x);
            ppu.bg1_scroll_y = interpolate_raster_word(
                previous_raster_motion.bg1_scroll_y,
                current_raster_motion.bg1_scroll_y);
            ppu.bg3_scroll_x = interpolate_raster_word(
                previous_raster_motion.bg3_scroll_x,
                current_raster_motion.bg3_scroll_x);
            ppu.bg3_scroll_y = interpolate_raster_word(
                previous_raster_motion.bg3_scroll_y,
                current_raster_motion.bg3_scroll_y);
            for (std::size_t line = 0;
                 line < ppu.bg2_horizontal_offsets.size(); ++line) {
                ppu.bg2_horizontal_offsets[line] = interpolate_raster_word(
                    previous_raster_motion.bg2_horizontal_offsets[line],
                    current_raster_motion.bg2_horizontal_offsets[line]);
            }
            auto circle = interpolate_circle_effect(
                previous_circle, current_circle, interpolation_alpha);
            circle.centre_x = static_cast<std::int16_t>(
                circle.centre_x + viewport_origin);
            framebuffer.clear(0U);
            superfx_frame.clear(0U);
            superfx_ui.clear(0U);
            superfx_hud.clear(0U);
            comms_hud.clear(0U);
            controls_player_layer.clear(0U);
            planet_overlay.clear(0U);
            planet_text_overlay.clear(0U);
            auto planet_presentation = game.planet_presentation_state();
            planet_presentation.isolate_left = static_cast<std::int16_t>(
                planet_presentation.isolate_left + viewport_origin);
            planet_presentation.isolate_right = static_cast<std::int16_t>(
                planet_presentation.isolate_right + viewport_origin);
            if (ppu.background_mode == 1U) {
                background_renderer.draw_bg3(
                    ppu, framebuffer, starfox::render::TilePriorityPass::low,
                    viewport_origin, extend_cartridge_scene);
                sprite_renderer.draw_objects(ppu, framebuffer, 0U, viewport_origin,
                    extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                    suppress_configurable_hud && gameplay_hud);
                if (!ppu.bg3_high_priority) {
                    background_renderer.draw_bg3(
                        ppu, framebuffer, starfox::render::TilePriorityPass::high,
                        viewport_origin, extend_cartridge_scene);
                }
                sprite_renderer.draw_objects(ppu, framebuffer, 1U, viewport_origin,
                    extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                    suppress_configurable_hud && gameplay_hud);
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::low,
                    viewport_origin, extend_cartridge_scene);
                sprite_renderer.draw_objects(ppu, framebuffer, 2U, viewport_origin,
                    extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                    suppress_configurable_hud && gameplay_hud);
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::high,
                    viewport_origin, extend_cartridge_scene);
            } else if (ppu.background_mode == 2U) {
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::low,
                    viewport_origin, extend_cartridge_scene);
                sprite_renderer.draw_objects(ppu, framebuffer, 0U, viewport_origin,
                    extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                    suppress_configurable_hud && gameplay_hud);
                sprite_renderer.draw_objects(ppu, framebuffer, 1U, viewport_origin,
                    extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                    suppress_configurable_hud && gameplay_hud);
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::high,
                    viewport_origin, extend_cartridge_scene);
                sprite_renderer.draw_objects(ppu, framebuffer, 2U, viewport_origin,
                    extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                    suppress_configurable_hud && gameplay_hud);
            } else if (ppu.background_mode == 3U) {
                auto& bg2_target = planet_presentation.briefing_layers
                    ? planet_overlay : framebuffer;
                if ((ppu.main_screen & 0x02U) != 0U) {
                    background_renderer.draw_bg2(ppu, background_x, background_y,
                        bg2_target, starfox::render::TilePriorityPass::low,
                        viewport_origin, extend_cartridge_scene);
                }
                if ((ppu.main_screen & 0x10U) != 0U) {
                    sprite_renderer.draw_objects(
                        ppu, framebuffer, 0U, viewport_origin,
                        extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                        suppress_configurable_hud && gameplay_hud);
                }
                if ((ppu.main_screen & 0x01U) != 0U) {
                    background_renderer.draw_bg1(
                        ppu, framebuffer, starfox::render::TilePriorityPass::low,
                        viewport_origin, extend_cartridge_scene);
                }
                if ((ppu.main_screen & 0x10U) != 0U) {
                    sprite_renderer.draw_objects(
                        ppu, framebuffer, 1U, viewport_origin,
                        extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                        suppress_configurable_hud && gameplay_hud);
                }
                if ((ppu.main_screen & 0x02U) != 0U) {
                    background_renderer.draw_bg2(ppu, background_x, background_y,
                        bg2_target, starfox::render::TilePriorityPass::high,
                        viewport_origin, extend_cartridge_scene);
                }
                if ((ppu.main_screen & 0x10U) != 0U) {
                    sprite_renderer.draw_objects(
                        ppu, framebuffer, 2U, viewport_origin,
                        extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                        suppress_configurable_hud && gameplay_hud);
                }
                if ((ppu.main_screen & 0x01U) != 0U) {
                    background_renderer.draw_bg1(
                        ppu, framebuffer, starfox::render::TilePriorityPass::high,
                        viewport_origin, extend_cartridge_scene);
                }
            } else {
                background_renderer.draw_bg2(
                    ppu, background_x, background_y, framebuffer,
                    starfox::render::TilePriorityPass::all, viewport_origin,
                    extend_cartridge_scene);
                background_renderer.draw_bg3(ppu, framebuffer,
                    starfox::render::TilePriorityPass::all, viewport_origin,
                    extend_cartridge_scene);
                for (std::uint8_t priority = 0; priority < 3U; ++priority) {
                    sprite_renderer.draw_objects(
                        ppu, framebuffer, priority, viewport_origin,
                        extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                        suppress_configurable_hud && gameplay_hud);
                }
            }
            struct VisibleObject {
                starfox::simulation::ObjectHandle handle{};
                starfox::timing::RenderTransform transform;
                CameraPoint position;
                double source_depth{};
            };
            std::vector<VisibleObject> visible;
            auto camera = starfox::timing::interpolate(
                previous_camera, current_camera, interpolation_alpha);
            if (mouse_camera_scene) {
                camera.pitch += mouse_camera.pitch_offset;
                camera.yaw += mouse_camera.yaw_offset;
            }
            const auto view_matrix = starfox::simulation::rotation_matrix_q15(
                trigonometry,
                static_cast<std::int16_t>(static_cast<std::uint16_t>(camera.pitch)),
                static_cast<std::int16_t>(static_cast<std::uint16_t>(camera.yaw)),
                static_cast<std::int16_t>(static_cast<std::uint16_t>(camera.roll)));
            if (mouse_camera_scene && mouse_camera.zoom_offset != 0.0) {
                constexpr double q15 = 32'768.0;
                // The third transform column is the adjusted camera's world-
                // space forward axis. Moving opposite it increases distance;
                // wheel-up decreases the offset and therefore zooms inward.
                camera.x -= static_cast<double>(view_matrix[2]) / q15
                    * mouse_camera.zoom_offset;
                camera.y -= static_cast<double>(view_matrix[5]) / q15
                    * mouse_camera.zoom_offset;
                camera.z -= static_cast<double>(view_matrix[8]) / q15
                    * mouse_camera.zoom_offset;
            }
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
            const auto controls_screen = game.flow_state()
                    == starfox::simulation::GameFlowState::controls_type
                || game.flow_state()
                    == starfox::simulation::GameFlowState::controls_choice;
            if (!planet_screen && game.map().dots_mode() < 0) {
                dust_renderer.draw(game.dust(), camera, view_matrix, superfx_frame);
            } else if (!planet_screen && game.map().dots_mode() > 0) {
                dust_renderer.draw_grid(camera, view_matrix, superfx_frame);
            }
            for (const auto handle : game.draw_order()) {
                if (!game.objects().is_active(handle)) continue;
                const auto& object = game.objects().at(handle);
                // invisible is sflag 27, stored in the fourth strategy byte.
                if ((object.strategy_flags[3] & 0x08U) != 0U) continue;
                const auto current_transform = current.find(handle);
                if (current_transform == current.end()) continue;
                const auto prior = previous.find(handle);
                // TRAIL_ISTRAT pieces are discrete source afterimages. Moving
                // every clone through fractional positions made the Nintendo
                // logo look smeared after its main text had already settled.
                const auto transform = starfox::timing::interpolate(
                    prior == previous.end() ? current_transform->second : prior->second,
                    current_transform->second,
                    object.strategy_address == trail_strategy_address
                        ? 1.0 : interpolation_alpha);
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
                    game.map().read_native_word(vanish_x_address)
                    + superfx_ui_offset_x);
                pose.vanish_y = static_cast<std::int16_t>(
                    game.map().read_native_word(vanish_y_address)
                    + (extend_scene_vertical ? superfx_offset_y : 0));
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
                pose.explosion_progress = (object.flags & 0x01U) != 0U
                    ? object.count : 0U;
                if (game.flow_state()
                        == starfox::simulation::GameFlowState::intro
                    && display_width > snes_width) {
                    // The cinematic's smoke, fireball, and particle spawners
                    // assume the 256-pixel cartridge camera. Extending their
                    // visibility with the 3D scene reveals random off-camera
                    // effects in ultrawide modes, so retain that source mask
                    // for transient effects only.
                    pose.effect_clip_left = viewport_origin;
                    pose.effect_clip_right = viewport_origin
                        + static_cast<std::int32_t>(snes_width);
                }
                // RELFASTELASER is a long tapered solid. Near the intro
                // camera, clipping its broad tail through z=0 exposes a
                // screen-filling triangle. The captured cartridge sequence
                // retains only the beam axis at that crossing.
                pose.collapse_to_axis_line = game.flow_state()
                        == starfox::simulation::GameFlowState::intro
                    && object.shape == intro_laser_shape
                    && position.z < 1'024.0;
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
                    auto& target = controls_screen && item.handle == game.player()
                        ? controls_player_layer : superfx_frame;
                    renderer.draw(found->second, make_pose(item, true), target, false);
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
                auto& target = controls_screen && item.handle == game.player()
                    ? controls_player_layer : superfx_frame;
                if ((object.strategy_flags[0] & 0x10U) != 0U) {
                    particle_renderer.draw_owner(game.particles(), item.handle,
                        make_pose(item, false), interpolation_alpha,
                        target);
                    continue;
                }
                if ((object.strategy_flags[0] & 0x40U) != 0U) {
                    text_renderer.draw(object.colour_table, object.extended[21],
                        std::bit_cast<std::int8_t>(object.texture_scroll_x),
                        make_pose(item, false), target);
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
                renderer.draw(found->second, pose, target, false);
            }
            const auto hud_rotation = game.map().read_native_word(
                hud_rotation_address);
            if ((hud_rotation & 0x8000U) != 0U) {
                auto hud_angle = static_cast<std::uint8_t>(hud_rotation);
                const auto player_pose = std::find_if(visible.begin(), visible.end(),
                    [&game](const VisibleObject& item) {
                        return item.handle == game.player();
                    });
                if (player_pose != visible.end()) {
                    // HUDROT is the player's eight-bit roll. Use the same
                    // presentation-only interpolation as its model so the
                    // restored indicators do not step at 20 Hz in a 60+ FPS
                    // output mode.
                    hud_angle = static_cast<std::uint8_t>(
                        static_cast<std::uint32_t>(std::lround(
                            player_pose->transform.roll / 256.0)));
                }
                // INIT_STRATS enables MHUD only while the player is inside
                // the cockpit. It is a source Super FX line pass, so place it
                // above world models but below the complete SNES OBJ HUD.
                renderer.draw_cockpit_hud(
                    trigonometry,
                    hud_angle,
                    game.map().read_native_byte(hud_colour_address),
                    game.map().read_native_byte(hud_flags_address),
                    superfx_ui_offset_x,
                    superfx_hud,
                    crosshair_tint(game.crosshair_colour())
                        ? static_cast<std::uint8_t>(
                            128U + 4U * 16U + 15U)
                        : 0U);
            }
            const auto dialogue = game.dialogue_state();
            if (dialogue.active && !suppress_configurable_hud) {
                text_renderer.draw_face(
                    dialogue.portrait_frame, 48, 152, comms_hud);
                if (dialogue.text_visible) {
                    const auto text_y = dialogue.three_lines ? 153 : 169;
                    text_renderer.draw_game_text(dialogue.text_address,
                        83, text_y + 1, comms_hud, 7U * 16U, 9U, 175);
                    text_renderer.draw_game_text(dialogue.text_address,
                        82, text_y, comms_hud, 7U * 16U, std::nullopt, 174);
                }
            }
            const auto results = game.stage_results_state();
            if (results.active) {
                text_renderer.draw_game_text(
                    score_text, 16, 24, superfx_ui);
                text_renderer.draw_game_text(
                    total_score_text, 16, 40, superfx_ui);
                text_renderer.draw_game_text(
                    team_text, 48, 69, superfx_ui);
                text_renderer.draw_ascii(
                    std::to_string(results.displayed_percentage) + "%",
                    176, 24, superfx_ui);
                text_renderer.draw_ascii(
                    std::to_string(results.total_percentage * 100U),
                    158, 40, superfx_ui);
                constexpr std::array<std::int32_t, 3> face_x{16, 96, 176};
                constexpr std::array<std::int32_t, 3> bar_x{11, 91, 171};
                constexpr std::array<std::int32_t, 3> name_x{15, 96, 173};
                constexpr std::array<std::uint8_t, 3> face_frame{7U, 9U, 11U};
                for (std::size_t teammate = 0; teammate < 3U; ++teammate) {
                    text_renderer.draw_face(
                        face_frame[teammate], face_x[teammate], 88, superfx_ui);
                    if (results.teammate_health[teammate] != 0U) {
                        text_renderer.draw_game_text(teammate_text[teammate],
                            name_x[teammate], 150, superfx_ui);
                    }
                    for (std::int32_t y = 136; y < 148; ++y) {
                        for (std::int32_t x = bar_x[teammate];
                             x < bar_x[teammate] + 44; ++x) {
                            const auto border = y == 136 || y == 147
                                || x == bar_x[teammate]
                                || x == bar_x[teammate] + 43;
                            const auto filled = x - bar_x[teammate] - 2
                                < std::min<std::uint8_t>(
                                    results.teammate_health[teammate], 40U);
                            superfx_ui.set(x, y, static_cast<std::uint8_t>(
                                7U * 16U + (border ? 14U
                                    : (filled ? 2U : 0U))));
                        }
                    }
                }
            }
            if (game.paused()) {
                text_renderer.draw_game_text(
                    pause_text, 90, 90, superfx_ui);
            }
            // A full-width layer keeps custom meter placements unclipped in
            // every aspect ratio. The default full-width coordinates are
            // pixel-identical to the former centred 224-pixel path at 4:3.
            if (!suppress_configurable_hud) {
                sprite_renderer.draw_meters(
                    game.meter_state(), superfx_hud, true,
                    gameplay_hud ? &active_hud_layout : nullptr);
            }

            // Colour zero is transparent in every host Super FX layer.
            const auto composite_superfx = [&framebuffer, viewport_origin](
                                               const auto& source,
                                               std::int32_t offset_x,
                                               std::int32_t offset_y,
                                               bool clip_controls) {
                // CONT.SCR's black flight panel is exactly 112x88 pixels;
                // the surrounding pixels belong to its bevelled frame.
                constexpr auto controls_top = 24;
                constexpr auto controls_bottom = 112;
                const auto controls_left = 24 + viewport_origin;
                const auto controls_right = 136 + viewport_origin;
                for (std::uint32_t y = 0; y < source.height(); ++y) {
                    for (std::uint32_t x = 0; x < source.width(); ++x) {
                        const auto destination_x = static_cast<std::int32_t>(x)
                            + offset_x;
                        const auto destination_y = static_cast<std::int32_t>(y)
                            + offset_y;
                        if (clip_controls
                            && (destination_x < controls_left
                                || destination_x >= controls_right
                                || destination_y < controls_top
                                || destination_y >= controls_bottom)) continue;
                        const auto colour = source.get(x, y);
                        if (colour != 0U) {
                            framebuffer.set(destination_x, destination_y, colour);
                        }
                    }
                }
            };

            // CONT's Arwing is behind the controller artwork. Reapply BG2's
            // high-priority screen tiles after that one object, then place
            // shots, bombs and action effects in the foreground pass.
            composite_superfx(
                controls_player_layer, 0, superfx_offset_y, controls_screen);
            if (controls_screen && ppu.background_mode == 1U) {
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::high,
                    viewport_origin, false);
            }
            composite_superfx(
                superfx_frame, 0, scene_offset_y, controls_screen);
            composite_superfx(
                superfx_hud, 0, superfx_offset_y, false);
            if (gameplay_hud) {
                // The Super FX world is below the complete gameplay OBJ HUD.
                // The priority bits order HUD sprites against one another;
                // they do not place labels behind projected model faces.
                for (std::uint8_t priority = 0U; priority < 4U; ++priority) {
                    sprite_renderer.draw_objects(ppu, framebuffer, priority,
                        viewport_origin, extend_cartridge_scene, anchor_edge_hud,
                        &active_hud_layout,
                        suppress_configurable_hud && gameplay_hud);
                }
            }
            const auto comms_offset = active_hud_layout[
                starfox::render::HudElement::comms];
            composite_superfx(comms_hud,
                superfx_ui_offset_x + comms_offset.x,
                superfx_offset_y + comms_offset.y, false);
            composite_superfx(
                superfx_ui, superfx_ui_offset_x, superfx_offset_y, false);

            if (game.flow_state() == starfox::simulation::GameFlowState::title
                && ppu.background_mode == 1U) {
                // The title flyby belongs behind the complete logo/team
                // artwork. Some retail title tiles carry low priority even
                // though TITLESEQ composites the prepared screen over the
                // ship, so reapplying only the high pass tears the artwork
                // into strips as the Arwing crosses it.
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::low,
                    viewport_origin, false);
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::high,
                    viewport_origin, false);
            }

            // PLANET's briefing is copied through the full-width Mode 3
            // screen buffer rather than the inset Super FX character layer.
            const auto briefing = game.briefing_state();
            if (briefing.active) {
                auto& briefing_target = planet_presentation.briefing_layers
                    ? planet_text_overlay : framebuffer;
                if (briefing.message_address != 0U) {
                    text_renderer.draw_game_text(briefing.message_address,
                        30 + viewport_origin, 173, briefing_target, 0U, 5U,
                        218 + viewport_origin,
                        briefing.visible_message_characters);
                    text_renderer.draw_game_text(briefing.message_address,
                        28 + viewport_origin, 171, briefing_target, 0U, 13U,
                        216 + viewport_origin,
                        briefing.visible_message_characters);
                }
                if (briefing.planet_name_address != 0U) {
                    text_renderer.draw_game_text(briefing.planet_name_address,
                        30 + viewport_origin, 41, briefing_target, 0U, 1U,
                        214 + viewport_origin,
                        briefing.visible_planet_characters);
                    text_renderer.draw_game_text(briefing.planet_name_address,
                        28 + viewport_origin, 39, briefing_target, 0U, 4U,
                        212 + viewport_origin,
                        briefing.visible_planet_characters);
                }
            }
            if ((ppu.main_screen & 0x10U) != 0U
                && !planet_presentation.briefing_layers
                && !gameplay_hud) {
                sprite_renderer.draw_objects(
                    ppu, framebuffer, 3U, viewport_origin,
                    extend_cartridge_scene, anchor_edge_hud);
            }
            if (ppu.background_mode == 1U && ppu.bg3_high_priority) {
                background_renderer.draw_bg3(
                    ppu, framebuffer, starfox::render::TilePriorityPass::high,
                    viewport_origin, extend_cartridge_scene);
            }
            if (controls_screen && viewport_origin > 0) {
                // The controller screen's backdrop is a BG tile colour rather
                // than CGRAM colour zero. Continue that exact indexed colour
                // through both wide margins so brightness fades remain
                // identical to the centred cartridge canvas.
                const auto backdrop = framebuffer.get(
                    static_cast<std::uint32_t>(viewport_origin), 0U);
                const auto right = viewport_origin
                    + static_cast<std::int32_t>(snes_width);
                for (std::int32_t y = 0;
                     y < static_cast<std::int32_t>(framebuffer.height()); ++y) {
                    for (std::int32_t x = 0; x < viewport_origin; ++x) {
                        framebuffer.set(x, y, backdrop);
                    }
                    for (std::int32_t x = right;
                         x < static_cast<std::int32_t>(framebuffer.width()); ++x) {
                        framebuffer.set(x, y, backdrop);
                    }
                }
            }
            if (game.flow_state()
                == starfox::simulation::GameFlowState::pregame_menu) {
                framebuffer.clear(0U);
                constexpr auto border_colour = static_cast<std::uint8_t>(
                    7U * 16U + 4U);
                constexpr std::int32_t menu_left = 12;
                constexpr std::int32_t menu_right = 243;
                constexpr std::int32_t menu_label_x = 32;
                constexpr std::int32_t menu_value_right = 236;
                constexpr std::int32_t menu_cursor_x = 20;
                for (std::int32_t x = menu_left + viewport_origin;
                     x <= menu_right + viewport_origin; ++x) {
                    framebuffer.set(x, 20, border_colour);
                    framebuffer.set(x, 203, border_colour);
                }
                for (std::int32_t y = 20; y <= 203; ++y) {
                    framebuffer.set(menu_left + viewport_origin, y, border_colour);
                    framebuffer.set(menu_right + viewport_origin, y, border_colour);
                }
                const auto draw_centred = [&text_renderer, &framebuffer,
                                            viewport_origin](
                                               std::string_view text,
                                               std::int32_t y,
                                               std::uint8_t colour) {
                    text_renderer.draw_ascii(text,
                        128 - static_cast<std::int32_t>(text.size() * 4U)
                            + viewport_origin,
                        y, framebuffer, colour);
                };
                if (hud_editor.active) {
                    constexpr auto palette_base = static_cast<std::uint8_t>(
                        7U * 16U);
                    const auto solid = [&framebuffer](
                                           std::int32_t x, std::int32_t y,
                                           std::int32_t width, std::int32_t height,
                                           std::uint8_t colour) {
                        for (std::int32_t row = 0; row < height; ++row) {
                            for (std::int32_t column = 0; column < width; ++column) {
                                framebuffer.set(x + column, y + row, colour);
                            }
                        }
                    };
                    const auto box = [&solid](HudRect rect, std::uint8_t colour) {
                        solid(rect.x, rect.y, rect.width, 1, colour);
                        solid(rect.x, rect.y + rect.height - 1,
                            rect.width, 1, colour);
                        solid(rect.x, rect.y, 1, rect.height, colour);
                        solid(rect.x + rect.width - 1, rect.y,
                            1, rect.height, colour);
                    };
                    framebuffer.clear(0U);

                    const auto& editor_layout = hud_layouts[
                        hud_profile_index(game.display_mode())];
                    std::optional<starfox::render::HudElement> hovered;
                    std::int32_t hovered_area = std::numeric_limits<std::int32_t>::max();
                    for (std::uint8_t value = 0U;
                         value < static_cast<std::uint8_t>(
                             starfox::render::HudElement::count); ++value) {
                        const auto element = static_cast<starfox::render::HudElement>(
                            value);
                        const auto rect = placed_hud_rect(
                            element, display_width, editor_layout);
                        const auto area = rect.width * rect.height;
                        if (rect.contains(hud_editor.pointer_x,
                                hud_editor.pointer_y) && area < hovered_area) {
                            hovered = element;
                            hovered_area = area;
                        }
                    }
                    const auto selected = hud_editor.dragging
                        ? hud_editor.dragging : hovered;
                    const auto corner_brackets = [&solid](
                                                     HudRect rect,
                                                     std::uint8_t colour) {
                        constexpr std::int32_t length = 5;
                        --rect.x;
                        --rect.y;
                        rect.width += 2;
                        rect.height += 2;
                        solid(rect.x, rect.y, length, 1, colour);
                        solid(rect.x, rect.y, 1, length, colour);
                        solid(rect.x + rect.width - length, rect.y,
                            length, 1, colour);
                        solid(rect.x + rect.width - 1, rect.y,
                            1, length, colour);
                        solid(rect.x, rect.y + rect.height - 1,
                            length, 1, colour);
                        solid(rect.x, rect.y + rect.height - length,
                            1, length, colour);
                        solid(rect.x + rect.width - length,
                            rect.y + rect.height - 1, length, 1, colour);
                        solid(rect.x + rect.width - 1,
                            rect.y + rect.height - length, 1, length, colour);
                    };
                    if (selected) {
                        corner_brackets(placed_hud_rect(*selected,
                            display_width, editor_layout),
                            static_cast<std::uint8_t>(palette_base + 14U));
                    }

                    // The preview itself is composited from a captured native
                    // gameplay frame below. This indexed layer is deliberately
                    // limited to unobtrusive editor chrome and drag handles.
                    solid(0, 0, static_cast<std::int32_t>(display_width),
                        11, static_cast<std::uint8_t>(palette_base + 1U));
                    const auto editor_title = std::string{"HUD LAYOUT  "}
                        + std::string{display_profile_name(game.display_mode())};
                    text_renderer.draw_ascii(editor_title,
                        static_cast<std::int32_t>(display_width / 2U)
                            - static_cast<std::int32_t>(editor_title.size() * 4U),
                        2, framebuffer, 14U);
                    solid(0, 210, static_cast<std::int32_t>(display_width),
                        14, static_cast<std::uint8_t>(palette_base + 1U));
                    const auto reset = hud_reset_button_rect(display_width);
                    const auto done = hud_done_button_rect(display_width);
                    if (reset.contains(hud_editor.pointer_x,
                            hud_editor.pointer_y)) {
                        box(reset, static_cast<std::uint8_t>(palette_base + 14U));
                    }
                    if (done.contains(hud_editor.pointer_x,
                            hud_editor.pointer_y)) {
                        box(done, static_cast<std::uint8_t>(palette_base + 14U));
                    }
                    text_renderer.draw_ascii("Y RESET", reset.x + 6,
                        reset.y + 3, framebuffer, 15U);
                    text_renderer.draw_ascii("B DONE", done.x + 2,
                        done.y + 3, framebuffer, 15U);
                } else if (remap_menu.active) {
                    draw_centred("CONTROLLER REMAP", 34, 14U);
                    draw_centred("D-PAD  CHOOSE", 51, 10U);
                    const auto device = remap_menu.device
                            == starfox::app::BindingDevice::keyboard
                        ? std::string{"KEYBOARD"}
                        : starfox::app::gamepad_device_label(gamepad);
                    draw_centred(device, 74, 13U);
                    const auto action = std::string{"ACTION  "}
                        + std::string{starfox::app::InputBindings::action_name(
                            remap_menu.action)} + "  "
                        + std::to_string(remap_menu.action + 1U) + "/"
                        + std::to_string(
                            starfox::app::InputBindings::action_count);
                    draw_centred(action, 98, 14U);
                    auto binding = remap_menu.waiting_for_input
                        ? std::string{"PRESS A KEY OR CONTROL"}
                        : bindings.binding_name(
                            remap_menu.device, remap_menu.action);
                    if (binding.size() > 25U) binding.resize(25U);
                    draw_centred(binding, 116,
                        remap_menu.waiting_for_input ? 14U : 7U);
                    draw_centred("LEFT/RIGHT  DEVICE", 143, 13U);
                    draw_centred("A  BIND   Y  DEFAULTS", 158, 13U);
                    draw_centred("B/START/ESC  DONE", 177, 13U);
                } else {
                    draw_centred("STAR FOX ENHANCED", 31, 14U);
                    const auto draw_cursor = [&framebuffer, viewport_origin,
                                                  menu_cursor_x](
                                                 std::int32_t y) {
                        for (std::int32_t column = 0; column < 5; ++column) {
                            const auto half_height = 4 - column;
                            for (std::int32_t row = -half_height;
                                 row <= half_height; ++row) {
                                framebuffer.set(menu_cursor_x + viewport_origin + column,
                                    y + row, static_cast<std::uint8_t>(
                                        7U * 16U + 14U));
                            }
                        }
                    };
                    const auto draw_row = [&text_renderer, &framebuffer,
                                              viewport_origin, menu_label_x,
                                              menu_value_right](
                                              std::string_view label,
                                              std::string_view value,
                                              std::int32_t y, bool selected) {
                        const auto colour = static_cast<std::uint8_t>(
                            selected ? 14U : 7U);
                        text_renderer.draw_ascii(label,
                            menu_label_x + viewport_origin,
                            y, framebuffer, colour);
                        if (!value.empty()) {
                            text_renderer.draw_ascii(value,
                                menu_value_right - static_cast<std::int32_t>(
                                    value.size() * 8U) + viewport_origin,
                                y, framebuffer, colour);
                        }
                    };

                    if (game.pregame_page()
                        == starfox::simulation::PregamePage::options) {
                        draw_centred("OPTIONS", 40, 10U);
                        const auto god_value = game.god_mode()
                            ? std::string_view{"ON"} : std::string_view{"OFF"};
                        const auto fps_value = game.show_fps()
                            ? std::string_view{"ON"} : std::string_view{"OFF"};
                        const auto crosshair = crosshair_colour_name(
                            game.crosshair_colour());
                        draw_row("GOD MODE", god_value, 56,
                            game.pregame_selection() == 0U);
                        draw_row("ON-SCREEN FPS", fps_value, 76,
                            game.pregame_selection() == 1U);
                        draw_row("CROSSHAIR COLOR", crosshair, 96,
                            game.pregame_selection() == 2U);
                        draw_row("CUSTOMIZE SCREEN", "A  OPEN", 116,
                            game.pregame_selection() == 3U);
                        draw_row("BACK", "", 146,
                            game.pregame_selection() == 4U);
                        constexpr std::array<std::int32_t, 5> cursor_y{
                            59, 79, 99, 119, 149};
                        draw_cursor(cursor_y[game.pregame_selection()]);
                        draw_centred("A/LEFT/RIGHT  CHANGE", 181, 13U);
                        draw_centred("B  BACK", 191, 13U);
                    } else {
                        draw_centred("PRE-GAME SETUP", 46, 10U);
                        const auto timing = game.timing_mode()
                            == starfox::simulation::TimingMode::unlocked_20_fps
                            ? std::string_view{"UNLOCKED 20 HZ"}
                            : std::string_view{"ORIGINAL"};
                        const auto presentation =
                            std::to_string(game.presentation_fps()) + " FPS";
                        const auto display = [mode = game.display_mode()]()
                            -> std::string_view {
                            switch (mode) {
                            case starfox::simulation::DisplayMode::widescreen_16_9:
                                return "16 BY 9 WIDE";
                            case starfox::simulation::DisplayMode::widescreen_16_10:
                                return "16 BY 10 WIDE";
                            case starfox::simulation::DisplayMode::ultrawide_21_9:
                                return "21 BY 9 ULTRA";
                            case starfox::simulation::DisplayMode::super_ultrawide_32_9:
                                return "32 BY 9 SUPER";
                            case starfox::simulation::DisplayMode::standard_4_3:
                            default:
                                return "4 BY 3 STANDARD";
                            }
                        }();
                        constexpr std::array<std::int32_t, 6> row_y{
                            62, 80, 98, 116, 134, 158};
                        draw_row("GAME PACE", timing, row_y[0],
                            game.pregame_selection() == 0U);
                        draw_row("RENDER FPS", presentation, row_y[1],
                            game.pregame_selection() == 1U);
                        draw_row("DISPLAY", display, row_y[2],
                            game.pregame_selection() == 2U);
                        draw_row("CONTROLLER", "A  REMAP", row_y[3],
                            game.pregame_selection() == 3U);
                        draw_row("OPTIONS", "A  OPEN", row_y[4],
                            game.pregame_selection() == 4U);
                        draw_row("START GAME", "", row_y[5],
                            game.pregame_selection() == 5U);
                        constexpr std::array<std::int32_t, 6> cursor_y{
                            65, 83, 101, 119, 137, 161};
                        draw_cursor(cursor_y[game.pregame_selection()]);
                        draw_centred("D-PAD CHOOSE   A SELECT", 181, 13U);
                        draw_centred("START  BEGIN", 191, 13U);
                    }
                }
            }
            if (game.flow_state() == starfox::simulation::GameFlowState::gameplay
                && !game.meter_state().enabled
                && (game.map().read_native_byte(player_ship_flags_address)
                    & 0x20U) != 0U) {
                // Normal gameplay INIDISP HDMA starts with a 16-scanline
                // forced-blank band. During the launch this hides BG2's
                // unused tile row above the 224x192 Super FX window; exposing
                // it produces the red/white "corrupt top bar" seen by the PC
                // renderer. The meter handoff ends this launch-only mask.
                const auto black = std::find_if(ppu.cgram.begin(), ppu.cgram.end(),
                    [](std::uint16_t colour) { return (colour & 0x7fffU) == 0U; });
                const auto black_index = black == ppu.cgram.end()
                    ? std::uint8_t{} : static_cast<std::uint8_t>(
                        std::distance(ppu.cgram.begin(), black));
                for (std::int32_t y = 0; y < 16; ++y) {
                    for (std::int32_t x = 0;
                         x < static_cast<std::int32_t>(display_width); ++x) {
                        framebuffer.set(x, y, black_index);
                    }
                }
            }
            // INITBLACK_L's STAYBLACK window subtracts full white from every
            // main-screen layer. Honouring that window masks the prepared but
            // not-yet-visible PPU pages at both briefing->scramble and
            // scramble->Corneria handoffs.
            const auto uses_black_window = game.flow_state()
                    == starfox::simulation::GameFlowState::gameplay
                || game.flow_state()
                    == starfox::simulation::GameFlowState::intro;
            const auto forced_black = uses_black_window
                && game.map().read_native_byte(stay_black_address) != 0xffU;
            if (forced_black) {
                framebuffer.clear(0U);
                planet_overlay.clear(0U);
                planet_text_overlay.clear(0U);
            }
            live_fps_overlay.clear();
            if (game.show_fps()) {
                const auto fps_text = std::string{"FPS "}
                    + std::to_string(live_fps.fps());
                text_renderer.draw_ascii(
                    fps_text, 0, 0, live_fps_overlay, 1U, 0U);
            }
            auto base_palette = starfox::render::decode_bgr555_palette(
                game.map().ppu_state().cgram);
            if (hud_editor.active) {
                const auto preview = starfox::render::preview_palette();
                std::copy(preview.begin(), preview.end(),
                    base_palette.begin() + 7U * 16U);
                base_palette[7U * 16U + 8U] =
                    crosshair_tint(game.crosshair_colour()).value_or(
                        starfox::render::Rgba8{64U, 255U, 96U, 255U});
            }
            apply_crosshair_tint(base_palette, game.crosshair_colour());
            auto palette = starfox::render::apply_snes_brightness(
                base_palette, game.map().display_brightness());
            if (forced_black) palette.fill({0U, 0U, 0U, 255U});
            PresentationEffects presentation_effects;
            if (planet_presentation.briefing_layers) {
                presentation_effects.overlay = &planet_overlay;
                presentation_effects.overlay_brightness =
                    planet_presentation.portrait_brightness;
                presentation_effects.text_overlay = &planet_text_overlay;
                // PLANETS.ASM's text sits in the dimmed briefing composition;
                // the host-rendered glyphs previously bypassed that residual
                // attenuation and reached full CGRAM intensity. Six SNES
                // five-bit steps reproduce the subdued title/message hues.
                presentation_effects.text_overlay_brightness =
                    planet_presentation.portrait_brightness > 6U
                    ? static_cast<std::uint8_t>(
                        planet_presentation.portrait_brightness - 6U)
                    : 0U;
            }
            presentation_effects.planet = planet_presentation;
            presentation_effects.clip_circle = controls_screen;
            presentation_effects.circle_left = static_cast<std::int16_t>(
                24 + viewport_origin);
            presentation_effects.circle_top = 24;
            presentation_effects.circle_right = static_cast<std::int16_t>(
                136 + viewport_origin);
            presentation_effects.circle_bottom = 112;
            if (game.show_fps()) {
                presentation_effects.host_overlay = &live_fps_overlay;
                presentation_effects.host_overlay_x =
                    static_cast<std::int32_t>(display_width
                        - live_fps_overlay.width() - 4U);
                presentation_effects.host_overlay_y = 4;
            }
            if (hud_editor.active) {
                const auto& editor_layout = hud_layouts[
                    hud_profile_index(game.display_mode())];
                compose_hud_editor(hud_editor_art, display_width,
                    editor_layout, framebuffer, palette, hud_editor_rgba);
                window.present_rgba(
                    display_width, snes_height, hud_editor_rgba);
            } else {
                window.present(
                    framebuffer, palette, circle, presentation_effects);
            }
            presentation_history.record(
                framebuffer.width(), framebuffer.height(), window.rgba());
            if (advance_frozen_frame) {
                window.set_frame_debug_status(true,
                    presentation_history.cursor(),
                    presentation_history.frame_count());
            }
            if (!advance_frozen_frame) {
                live_fps.record_frame(std::chrono::steady_clock::now());
            }
            if (!capture_directory.empty() && presented_frames >= capture_start) {
                auto name = std::to_string(presented_frames);
                if (name.size() < 6U) name.insert(0U, 6U - name.size(), '0');
                window.save_bmp(capture_directory / (name + ".bmp"));
            }
            ++presented_frames;
            if (test_frames != 0 && presented_frames >= test_frames) {
                if (!capture_path.empty()) window.save_bmp(capture_path);
                running = false;
            }
        }

        if (hud_editor.active || hud_editor.dragging) save_hud_layout();
        if (gamepad != nullptr) {
            SDL_CloseGamepad(gamepad);
        }
        return 0;
    } catch (const std::exception& error) {
        const std::string message =
            std::string{"Star Fox Enhanced could not start:\n\n"} + error.what();
        std::cerr << "starfox_pc failed: " << error.what() << '\n';
#if defined(_WIN32)
        MessageBoxA(nullptr, message.c_str(), "Star Fox Enhanced",
            MB_OK | MB_ICONERROR | MB_TASKMODAL);
#endif
        return 1;
    }
}
