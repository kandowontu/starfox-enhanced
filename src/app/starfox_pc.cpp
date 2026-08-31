#include "starfox/audio/spc700_audio.hpp"
#include "starfox/audio/msu1_audio.hpp"
#include "starfox/audio/msu1_pack.hpp"
#include "starfox/app/runtime_input.hpp"
#include "starfox/assets/bps.hpp"
#include "starfox/assets/embedded.hpp"
#include "starfox/assets/rom.hpp"
#include "starfox/assets/runtime_bundle.hpp"
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
#if defined(__ANDROID__) || defined(__IPHONEOS__) || defined(__SWITCH__)
#include <SDL3/SDL_main.h>
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

class FrameStepRepeater {
public:
    enum class Direction : std::uint8_t { forward, backward };
    using clock = std::chrono::steady_clock;

    void press(Direction direction, clock::time_point now) noexcept {
        direction_ = direction;
        repeat_at_ = now + std::chrono::milliseconds{450};
    }

    void release(Direction direction) noexcept {
        if (direction_ == direction) direction_.reset();
    }

    void reset() noexcept { direction_.reset(); }

    [[nodiscard]] std::optional<Direction> poll(
        clock::time_point now) noexcept {
        if (!direction_.has_value() || now < repeat_at_) return std::nullopt;
        constexpr auto interval = std::chrono::milliseconds{125};
        do {
            repeat_at_ += interval;
        } while (repeat_at_ <= now);
        return direction_;
    }

private:
    std::optional<Direction> direction_;
    clock::time_point repeat_at_{};
};

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
    const starfox::render::Framebuffer* confirmation_overlay{};
    std::uint8_t background_fixed_white_subtract{};
    const starfox::render::Framebuffer* fixed_subtract_foreground{};
    std::int32_t fixed_subtract_foreground_x{};
    std::int32_t fixed_subtract_foreground_y{};
    starfox::simulation::PlanetPresentationState planet;
    starfox::simulation::WindowWipeState wipe;
    bool expand_wipe{};
    bool expand_wipe_vertical{};
    bool clip_circle{};
    std::int16_t circle_left{};
    std::int16_t circle_top{};
    std::int16_t circle_right{};
    std::int16_t circle_bottom{};
    bool black_side_bars{};
    std::int32_t side_bar_left{};
    std::int32_t side_bar_right{};
    const starfox::render::SurfaceBuffer* model_surfaces{};
    std::int32_t model_surface_x{};
    std::int32_t model_surface_y{};
    bool touch_controls{};
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
    starfox::simulation::DisplayMode mode,
    starfox::simulation::Experience experience) noexcept {
    auto result = std::size_t{};
    switch (mode) {
    case starfox::simulation::DisplayMode::widescreen_16_9:
        result = 1U;
        break;
    case starfox::simulation::DisplayMode::widescreen_16_10:
        result = 2U;
        break;
    case starfox::simulation::DisplayMode::ultrawide_21_9:
        result = 3U;
        break;
    case starfox::simulation::DisplayMode::super_ultrawide_32_9:
        result = 4U;
        break;
    case starfox::simulation::DisplayMode::standard_4_3:
    default:
        break;
    }
    if (experience == starfox::simulation::Experience::starfox_ex) {
        result += starfox::render::hud_display_profile_count;
    }
    return result;
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

std::string_view anti_aliasing_name(
    starfox::simulation::AntiAliasingMode mode) noexcept {
    switch (mode) {
    case starfox::simulation::AntiAliasingMode::light:
        return "LIGHT";
    case starfox::simulation::AntiAliasingMode::medium:
        return "MEDIUM";
    case starfox::simulation::AntiAliasingMode::heavy:
        return "HEAVY";
    case starfox::simulation::AntiAliasingMode::off:
    default:
        return "OFF";
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

std::uint8_t nearest_palette_index(
    std::span<const starfox::render::Rgba8> palette,
    starfox::render::Rgba8 target) noexcept {
    std::uint8_t best{};
    auto best_distance = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t index = 0U;
         index < std::min<std::size_t>(palette.size(), 256U); ++index) {
        const auto colour = palette[index];
        const auto red = static_cast<std::int32_t>(colour.r) - target.r;
        const auto green = static_cast<std::int32_t>(colour.g) - target.g;
        const auto blue = static_cast<std::int32_t>(colour.b) - target.b;
        const auto distance = static_cast<std::uint32_t>(
            red * red + green * green + blue * blue);
        if (distance >= best_distance) continue;
        best = static_cast<std::uint8_t>(index);
        best_distance = distance;
    }
    return best;
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
    std::uint32_t width,
    starfox::simulation::Experience experience) noexcept {
    switch (element) {
    case starfox::render::HudElement::lives:
        // EX locates lives immediately above its lower-left shield label;
        // retail keeps the same three source sprites at the upper left.
        return experience == starfox::simulation::Experience::starfox_ex
            ? HudRect{26, 174, 26, 10}
            : HudRect{15, 16, 26, 10};
    case starfox::render::HudElement::shield:
        return {20, 179, 48, 25};
    case starfox::render::HudElement::bombs_boost:
        return {static_cast<std::int32_t>(width) - 68, 178, 48, 26};
    case starfox::render::HudElement::comms:
        return {static_cast<std::int32_t>(width) / 2 - 68, 164, 136, 48};
    case starfox::render::HudElement::boss_health:
        // Reserve ENEMY plus the complete legal 8-bit meter span. Meter
        // layers are composited into the 224-line screen at y=16.
        return {static_cast<std::int32_t>(width) - 195, 17, 181, 10};
    case starfox::render::HudElement::count:
    default:
        return {};
    }
}

HudRect placed_hud_rect(
    starfox::render::HudElement element,
    std::uint32_t width,
    const starfox::render::HudLayout& layout,
    starfox::simulation::Experience experience) noexcept {
    auto result = default_hud_rect(element, width, experience);
    const auto offset = layout[element];
    result.x += offset.x;
    result.y += offset.y;
    return result;
}

void clamp_hud_element(
    starfox::render::HudLayout& layout,
    starfox::render::HudElement element,
    std::uint32_t width,
    starfox::simulation::Experience experience) noexcept {
    const auto base = default_hud_rect(element, width, experience);
    auto& offset = layout[element];
    offset.x = static_cast<std::int16_t>(std::clamp<std::int32_t>(offset.x,
        -base.x,
        static_cast<std::int32_t>(width) - base.x - base.width));
    offset.y = static_cast<std::int16_t>(std::clamp<std::int32_t>(offset.y,
        -base.y, static_cast<std::int32_t>(snes_height) - base.y - base.height));
}

void clamp_hud_layout(
    starfox::render::HudLayout& layout,
    std::uint32_t width,
    starfox::simulation::Experience experience) noexcept {
    for (std::uint8_t value = 0U;
         value < static_cast<std::uint8_t>(starfox::render::HudElement::count);
         ++value) {
        clamp_hud_element(layout,
            static_cast<starfox::render::HudElement>(value), width,
            experience);
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

#if defined(STARFOX_HAS_EMBEDDED_ASSETS)
std::span<const std::uint8_t> embedded_resource(int identifier) {
#if defined(_WIN32)
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
#else
    return starfox::assets::embedded_asset(identifier);
#endif
}

RuntimeAssets make_runtime_assets(
    std::vector<std::uint8_t> rom,
    std::string symbols) {
    auto parsed_symbols = starfox::assets::SymbolMap::parse(symbols);
    return {starfox::assets::RomImage{std::move(rom)},
        std::move(parsed_symbols)};
}

struct RuntimeAssetSet {
    RuntimeAssets original;
    RuntimeAssets starfox_ex;
};

std::vector<std::uint8_t> read_binary_file(
    const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error{"unable to open file: " + path.string()};
    }
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{},
    };
}

struct RetailVariant {
    std::string_view name;
    std::uint32_t crc32;
    int canonicalization_resource;
};

constexpr auto retail_size = std::size_t{1U << 20U};
constexpr auto retail_v12_crc32 = std::uint32_t{0x8fc4e6d0U};
constexpr std::array retail_variants{
    RetailVariant{"Star Fox (USA) (Rev 2)", retail_v12_crc32, 0},
    RetailVariant{"Star Fox (Japan)", 0x41a60b3fU, 120},
    RetailVariant{"Star Fox (Japan) (Rev 1)", 0xad668a41U, 121},
    RetailVariant{"Star Fox (USA)", 0x0bae0941U, 122},
    RetailVariant{"Star Fox (USA) (Rev 1)", 0xb18676b2U, 123},
    RetailVariant{"Starwing (Europe)", 0x865f1a71U, 124},
    RetailVariant{"Starwing (Europe) (Rev 1)", 0xba64da2bU, 125},
    RetailVariant{"Starwing (Germany)", 0xb48ca238U, 126},
};

std::vector<std::uint8_t> canonicalize_retail_rom(
    const std::filesystem::path& path) {
    auto bytes = read_binary_file(path);
    if (bytes.size() == retail_size + 512U) {
        bytes.erase(bytes.begin(), bytes.begin() + 512U);
    }
    const auto checksum = bytes.size() == retail_size
        ? starfox::assets::crc32(bytes)
        : std::uint32_t{};
    const auto variant = std::find_if(retail_variants.begin(),
        retail_variants.end(), [checksum](const RetailVariant& candidate) {
            return candidate.crc32 == checksum;
        });
    if (variant == retail_variants.end()) {
        throw std::runtime_error{
            "the selected file is not a supported unmodified retail Star "
            "Fox/Starwing ROM: " + path.string()};
    }
    if (variant->canonicalization_resource == 0) return bytes;

    auto canonical = starfox::assets::apply_bps_patch(bytes,
        embedded_resource(variant->canonicalization_resource));
    if (canonical.size() != retail_size
        || starfox::assets::crc32(canonical) != retail_v12_crc32) {
        throw std::runtime_error{
            "the embedded canonicalization data for "
            + std::string{variant->name} + " is invalid"};
    }
    return canonical;
}

std::pair<std::filesystem::path, std::vector<std::uint8_t>>
load_required_retail(const std::filesystem::path& executable_directory) {
    std::vector<std::filesystem::path> candidates;
    if (const auto* override_path = std::getenv("STARFOX_RETAIL_ROM");
        override_path != nullptr && *override_path != '\0') {
        // An explicit override is authoritative: report a bad selection
        // rather than silently finding a different ROM elsewhere.
        const auto path = std::filesystem::path{override_path};
        return {path, canonicalize_retail_rom(path)};
    }
    constexpr std::array filenames{
        "Star Fox (USA) (Rev 2).sfc",
        "Star Fox (USA) (Rev 1).sfc",
        "Star Fox (USA).sfc",
        "Star Fox (Japan) (Rev 1).sfc",
        "Star Fox (Japan).sfc",
        "Starwing (Europe) (Rev 1).sfc",
        "Starwing (Europe).sfc",
        "Starwing (Germany).sfc",
        "Star Fox v1.2.sfc",
    };
    auto directories = std::vector<std::filesystem::path>{
        executable_directory};
#if defined(_WIN32)
    directories.insert(directories.begin(), std::filesystem::path{
        R"(C:\NTSC-US Super Nintendo System Roms)"});
#endif
    if (const auto* documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
        documents != nullptr && *documents != '\0') {
        directories.emplace_back(documents);
        directories.emplace_back(
            std::filesystem::path{documents} / "Star Fox Enhanced");
    }
    for (const auto& directory : directories) {
        for (const auto* filename : filenames) {
            candidates.emplace_back(directory / filename);
        }
    }
    for (const auto& path : candidates) {
        if (!std::filesystem::is_regular_file(path)) continue;
        try {
            return {path, canonicalize_retail_rom(path)};
        } catch (const std::runtime_error&) {
            // Automatic discovery may encounter a corrupt or modified dump.
            // Continue looking for another supported retail revision; an
            // explicit STARFOX_RETAIL_ROM selection remains authoritative.
        }
    }
    throw std::runtime_error{
        "Star Fox Enhanced requires an unmodified retail Star Fox/Starwing "
        "ROM: Japan 1.0/1.1, USA 1.0/1.1/1.2, Europe 1.0/1.1, or Germany "
        "1.0. Place it beside the game or in the Star Fox Enhanced "
        "Documents folder, or set STARFOX_RETAIL_ROM to its full path."};
}

std::uint32_t embedded_asset_manifest() {
    std::vector<std::uint8_t> manifest_bytes;
    for (const auto identifier : {
             101, 102, 108, 109, 120, 121, 122, 123, 124, 125, 126}) {
        const auto resource = embedded_resource(identifier);
        const auto size = static_cast<std::uint32_t>(resource.size());
        for (std::uint32_t shift = 0; shift < 32U; shift += 8U) {
            manifest_bytes.push_back(static_cast<std::uint8_t>(size >> shift));
        }
        manifest_bytes.insert(
            manifest_bytes.end(), resource.begin(), resource.end());
    }
    return starfox::assets::crc32(manifest_bytes);
}

std::string embedded_text_resource(int identifier) {
    const auto resource = embedded_resource(identifier);
    return {reinterpret_cast<const char*>(resource.data()), resource.size()};
}

RuntimeAssetSet unpack_runtime_assets(
    starfox::assets::RuntimeBundlePayload payload) {
    return {
        make_runtime_assets(std::move(payload.original_rom),
            std::move(payload.original_symbols)),
        make_runtime_assets(std::move(payload.starfox_ex_rom),
            std::move(payload.starfox_ex_symbols)),
    };
}

void write_asset_companion(
    const std::filesystem::path& path,
    std::span<const std::uint8_t> bytes) {
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream{temporary,
            std::ios::binary | std::ios::trunc};
        if (!stream) {
            throw std::runtime_error{
                "unable to create asset companion: " + path.string()};
        }
        stream.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!stream) {
            throw std::runtime_error{
                "unable to write asset companion: " + path.string()};
        }
    }
#if defined(_WIN32)
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto error = GetLastError();
        static_cast<void>(DeleteFileW(temporary.c_str()));
        throw std::runtime_error{
            "unable to install asset companion (Windows error "
            + std::to_string(error) + "): " + path.string()};
    }
#else
    std::error_code error;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error{
            "unable to install asset companion: " + path.string()
            + ": " + error.message()};
    }
#endif
}

RuntimeAssetSet load_or_compile_runtime_assets(
    const std::filesystem::path& executable_directory) {
    const auto companion_path =
        executable_directory / "Starfox-Assets.BIN";
    const auto manifest = embedded_asset_manifest();
    if (std::filesystem::is_regular_file(companion_path)) {
        try {
            return unpack_runtime_assets(
                starfox::assets::decode_runtime_bundle(
                    read_binary_file(companion_path), manifest));
        } catch (const std::exception&) {
            // An update can legitimately invalidate a previously compiled
            // companion. Rebuild it below from the user's validated retail
            // image; if that image is unavailable, the resulting error tells
            // the user exactly how to supply it.
        }
    }

    const auto [retail_path, retail_rom] =
        load_required_retail(executable_directory);
    static_cast<void>(retail_path);
    starfox::assets::RuntimeBundlePayload payload;
    payload.original_rom = starfox::assets::apply_bps_patch(
        retail_rom, embedded_resource(101));
    payload.original_symbols = embedded_text_resource(102);
    payload.starfox_ex_rom = starfox::assets::apply_bps_patch(
        retail_rom, embedded_resource(108));
    payload.starfox_ex_symbols = embedded_text_resource(109);
    const auto companion =
        starfox::assets::encode_runtime_bundle(payload, manifest);
    write_asset_companion(companion_path, companion);
    return unpack_runtime_assets(std::move(payload));
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
    explicit Window(starfox::simulation::RendererMode renderer_mode) {
        window_ = SDL_CreateWindow(
            "Star Fox Enhanced - native PC runtime", 1024, 896,
            SDL_WINDOW_RESIZABLE);
        if (window_ == nullptr) {
            throw std::runtime_error{
                std::string{"SDL_CreateWindow: "} + SDL_GetError()};
        }
        recreate_renderer(renderer_mode);
        SDL_ShowWindow(window_);
        static_cast<void>(SDL_SyncWindow(window_));
        // Put an actual black frame on the desktop before ROM decoding, game
        // construction or audio-device setup can begin. A merely-created SDL
        // window can remain compositor-transparent until its first present.
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderPresent(renderer_);
        static_cast<void>(SDL_SyncWindow(window_));
    }

    ~Window() {
        SDL_DestroyTexture(smooth_model_texture_);
        SDL_DestroyTexture(smooth_target_texture_);
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
        const auto subtract_fixed_white = [this](
                                              std::size_t pixel,
                                              std::uint8_t amount) {
            const auto fixed = static_cast<std::int32_t>(
                std::min<std::uint32_t>(amount, 31U));
            for (std::size_t component = 0; component < 3U; ++component) {
                const auto source_five = static_cast<std::int32_t>(
                    (static_cast<std::uint32_t>(rgba_[pixel + component])
                        * 31U + 127U) / 255U);
                const auto result_five = std::max(0, source_five - fixed);
                rgba_[pixel + component] = static_cast<std::uint8_t>(
                    (result_five << 3U) | (result_five >> 2U));
            }
        };
        if (effects.background_fixed_white_subtract != 0U) {
            for (std::int32_t y = 0;
                 y < static_cast<std::int32_t>(framebuffer.height()); ++y) {
                for (std::int32_t x = 0;
                     x < static_cast<std::int32_t>(framebuffer.width()); ++x) {
                    // GAMEOVER_L subtracts from BG2/BG3/backdrop but not the
                    // Super FX BG1 model layer or SNES OBJ sprites.
                    if (framebuffer.get(x, y) >= 128U) continue;
                    if (effects.fixed_subtract_foreground != nullptr) {
                        const auto mask_x = x
                            - effects.fixed_subtract_foreground_x;
                        const auto mask_y = y
                            - effects.fixed_subtract_foreground_y;
                        if (mask_x >= 0 && mask_y >= 0
                            && mask_x < static_cast<std::int32_t>(
                                effects.fixed_subtract_foreground->width())
                            && mask_y < static_cast<std::int32_t>(
                                effects.fixed_subtract_foreground->height())
                            && effects.fixed_subtract_foreground->get(
                                mask_x, mask_y) != 0U) {
                            continue;
                        }
                    }
                    const auto pixel = (static_cast<std::size_t>(y)
                        * framebuffer.width() + static_cast<std::size_t>(x))
                        * 4U;
                    subtract_fixed_white(pixel,
                        effects.background_fixed_white_subtract);
                }
            }
        }
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
                    subtract_fixed_white(
                        pixel, effects.planet.isolate_amount);
                }
            }
        }
        if (effects.planet.level_fade) {
            for (std::size_t pixel = 0; pixel < rgba_.size(); pixel += 4U) {
                subtract_fixed_white(
                    pixel, effects.planet.level_fade_amount);
            }
        }
        if (effects.wipe.active) {
            const auto origin_x = static_cast<std::int32_t>(
                framebuffer.width() > snes_width
                    ? (framebuffer.width() - snes_width) / 2U : 0U);
            const auto source_width = static_cast<std::int32_t>(
                framebuffer.width());
            const auto logic = static_cast<std::uint8_t>(
                effects.wipe.logic & 3U);
            const auto output_top = effects.expand_wipe_vertical
                ? 0 : superfx_offset_y;
            const auto output_bottom = effects.expand_wipe_vertical
                ? static_cast<std::int32_t>(framebuffer.height())
                : superfx_offset_y
                    + static_cast<std::int32_t>(effects.wipe.left.size());
            for (std::int32_t y = output_top; y < output_bottom; ++y) {
                if (y < 0 || y >= static_cast<std::int32_t>(
                        framebuffer.height())) continue;
                // The cartridge's colour-window table covers the 192-line
                // Super FX viewport. Gameplay is presented over all 224
                // output lines, so map that table over the complete host
                // raster during the launch reveal. Leaving the original
                // 16-line guards outside this loop made the top and bottom
                // of the world remain visible while the centre was closed.
                const auto line = effects.expand_wipe_vertical
                    ? static_cast<std::size_t>(std::clamp(
                        y * static_cast<std::int32_t>(
                            effects.wipe.left.size() - 1U)
                            / std::max(static_cast<std::int32_t>(
                                framebuffer.height()) - 1, 1),
                        0, static_cast<std::int32_t>(
                            effects.wipe.left.size() - 1U)))
                    : static_cast<std::size_t>(y - superfx_offset_y);
                const auto left = static_cast<std::uint8_t>(
                    effects.wipe.left[line]);
                const auto right = static_cast<std::uint8_t>(
                    effects.wipe.right[line]);
                for (std::int32_t x = 0;
                     x < static_cast<std::int32_t>(framebuffer.width()); ++x) {
                    // The source window tables describe the centred Super FX
                    // viewport. For a wide presentation, scale that active
                    // mask over the complete host scene too; merely centring
                    // it made the added columns enter/leave as black slabs.
                    const auto source_x = effects.expand_wipe
                        // Stretch the actual $10-$ef Super FX window, not
                        // the unused 16-pixel source guards. Mapping the host
                        // through $00-$ff made those guards become visible
                        // side slabs while the central shutter was black.
                        ? 16 + std::clamp(x * 223
                                / std::max(source_width - 1, 1), 0, 223)
                        : x - origin_x;
                    const auto inside_dynamic = left <= right
                        ? source_x >= left && source_x <= right
                        : source_x >= left || source_x <= right;
                    // W12SEL/W34SEL=$bb select the outside of dynamic window
                    // 1 and the inside of fixed viewport window 2 ($10-$f0).
                    const auto window_1 = !inside_dynamic;
                    const auto window_2 =
                        source_x >= 16 && source_x <= 240;
                    bool masked{};
                    switch (logic) {
                    case 1U: masked = window_1 && window_2; break; // AND
                    case 2U: masked = window_1 != window_2; break; // XOR
                    case 3U: masked = window_1 == window_2; break; // XNOR
                    case 0U:
                    default: masked = window_1 || window_2; break; // OR
                    }
                    if (!masked) continue;
                    const auto pixel = (static_cast<std::size_t>(y)
                        * framebuffer.width() + static_cast<std::size_t>(x))
                        * 4U;
                    rgba_[pixel] = 0U;
                    rgba_[pixel + 1U] = 0U;
                    rgba_[pixel + 2U] = 0U;
                    rgba_[pixel + 3U] = 255U;
                }
            }
        }
        if (effects.black_side_bars) {
            const auto left = std::clamp(effects.side_bar_left, 0,
                static_cast<std::int32_t>(framebuffer.width()));
            const auto right = std::clamp(effects.side_bar_right, left,
                static_cast<std::int32_t>(framebuffer.width()));
            for (std::int32_t y = 0;
                 y < static_cast<std::int32_t>(framebuffer.height()); ++y) {
                for (std::int32_t x = 0;
                     x < static_cast<std::int32_t>(framebuffer.width()); ++x) {
                    if (x >= left && x < right) continue;
                    const auto pixel = (static_cast<std::size_t>(y)
                        * framebuffer.width() + static_cast<std::size_t>(x)) * 4U;
                    rgba_[pixel] = 0U;
                    rgba_[pixel + 1U] = 0U;
                    rgba_[pixel + 2U] = 0U;
                    rgba_[pixel + 3U] = 255U;
                }
            }
        }
        smooth_layer_ready_ = false;
        if (enhanced_graphics_) apply_enhanced_surfaces(framebuffer, effects);
        if (rtx_lighting_) apply_rtx_lighting(framebuffer, effects);
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
        if (effects.confirmation_overlay != nullptr) {
            const auto& overlay = *effects.confirmation_overlay;
            const auto left = (static_cast<std::int32_t>(framebuffer.width())
                - static_cast<std::int32_t>(overlay.width())) / 2;
            const auto top = (static_cast<std::int32_t>(framebuffer.height())
                - static_cast<std::int32_t>(overlay.height())) / 2;
            const auto right = left + static_cast<std::int32_t>(overlay.width());
            const auto bottom = top + static_cast<std::int32_t>(overlay.height());
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
            for (auto y = top - 4; y < bottom + 4; ++y) {
                for (auto x = left - 6; x < right + 6; ++x) {
                    const auto border = x == left - 6 || x == right + 5
                        || y == top - 4 || y == bottom + 3;
                    paint(x, y, border ? 255U : 0U);
                }
            }
            for (std::uint32_t y = 0U; y < overlay.height(); ++y) {
                for (std::uint32_t x = 0U; x < overlay.width(); ++x) {
                    if (overlay.get(x, y) == 0U) continue;
                    paint(left + static_cast<std::int32_t>(x),
                        top + static_cast<std::int32_t>(y), 255U);
                }
            }
        }
        if (anti_aliasing_ != starfox::simulation::AntiAliasingMode::off) {
            apply_fxaa(anti_aliasing_);
        }
        if (effects.touch_controls) {
            apply_touch_controls(framebuffer.width(), framebuffer.height());
        }
        if (smooth_polys_) {
            prepare_1440p_model_layer(framebuffer, effects);
        }
        present_rgba_pixels(framebuffer.width(), framebuffer.height(), rgba_);
    }

    void present_rgba(std::uint32_t width, std::uint32_t height,
        std::span<const std::uint8_t> rgba) {
        if (rgba.size() != static_cast<std::size_t>(width) * height * 4U) {
            throw std::invalid_argument{"RGBA presentation size mismatch"};
        }
        ensure_dimensions(width, height);
        smooth_layer_ready_ = false;
        rgba_.assign(rgba.begin(), rgba.end());
        present_rgba_pixels(width, height, rgba_);
    }

    [[nodiscard]] std::span<const std::uint8_t> rgba() const noexcept {
        return rgba_;
    }

    void set_render_options(
        starfox::simulation::RendererMode renderer_mode,
        starfox::simulation::AntiAliasingMode anti_aliasing,
        bool enhanced_graphics,
        bool smooth_polys, bool rtx_lighting, bool vsync) {
        if (renderer_mode_ != renderer_mode) {
            recreate_renderer(renderer_mode);
        }
        anti_aliasing_ = anti_aliasing;
        smooth_polys_ = smooth_polys;
        rtx_lighting_ = rtx_lighting;
        if (enhanced_graphics_ != enhanced_graphics) {
            enhanced_graphics_ = enhanced_graphics;
            if (texture_ != nullptr) {
                static_cast<void>(SDL_SetTextureScaleMode(texture_,
                    enhanced_graphics_ ? SDL_SCALEMODE_LINEAR
                                       : SDL_SCALEMODE_NEAREST));
            }
        }
        if (vsync_ != vsync) {
            vsync_ = vsync;
            static_cast<void>(SDL_SetRenderVSync(renderer_, vsync_ ? 1 : 0));
        }
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
        if (relative_mouse_mode_ == enabled) return;
        if (SDL_SetWindowRelativeMouseMode(window_, enabled)) {
            relative_mouse_mode_ = enabled;
        }
    }

    void toggle_fullscreen() {
        const auto enter_fullscreen =
            (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) == 0U;
        if (!SDL_SetWindowFullscreen(window_, enter_fullscreen)) {
            throw std::runtime_error{
                std::string{"SDL_SetWindowFullscreen: "} + SDL_GetError()};
        }
        static_cast<void>(SDL_SyncWindow(window_));
        if (!enter_fullscreen) {
            set_windowed_size(texture_width_, texture_height_);
            static_cast<void>(SDL_SyncWindow(window_));
        }
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
        temporary_status_until_.reset();
        if (!frozen) {
            static_cast<void>(SDL_SetWindowTitle(window_, base_title.data()));
            return;
        }
        const auto title = std::string{base_title} + " [FROZEN "
            + std::to_string(count == 0U ? 0U : cursor + 1U) + "/"
            + std::to_string(count) + "]";
        static_cast<void>(SDL_SetWindowTitle(window_, title.c_str()));
    }

    void show_temporary_status(std::string_view status) {
        constexpr std::string_view base_title =
            "Star Fox Enhanced - native PC runtime";
        const auto title = std::string{base_title} + " [" + std::string{status}
            + "]";
        static_cast<void>(SDL_SetWindowTitle(window_, title.c_str()));
        temporary_status_until_ = std::chrono::steady_clock::now()
            + std::chrono::seconds{2};
    }

    void update_temporary_status() noexcept {
        if (!temporary_status_until_
            || std::chrono::steady_clock::now() < *temporary_status_until_) {
            return;
        }
        temporary_status_until_.reset();
        static_cast<void>(SDL_SetWindowTitle(window_,
            "Star Fox Enhanced - native PC runtime"));
    }

private:
    void recreate_renderer(starfox::simulation::RendererMode mode) {
        SDL_DestroyTexture(smooth_model_texture_);
        SDL_DestroyTexture(smooth_target_texture_);
        SDL_DestroyTexture(texture_);
        SDL_DestroyRenderer(renderer_);
        smooth_model_texture_ = nullptr;
        smooth_target_texture_ = nullptr;
        texture_ = nullptr;
        renderer_ = SDL_CreateRenderer(window_,
            mode == starfox::simulation::RendererMode::software
                ? "software" : nullptr);
        if (renderer_ == nullptr) {
            throw std::runtime_error{
                std::string{"SDL_CreateRenderer: "} + SDL_GetError()};
        }
        renderer_mode_ = mode;
        // Presentation has its own exact schedule. Following an arbitrary
        // desktop refresh is opt-in through the saved VSYNC menu choice.
        static_cast<void>(SDL_SetRenderVSync(renderer_, vsync_ ? 1 : 0));
        if (!SDL_SetRenderLogicalPresentation(renderer_,
                static_cast<int>(texture_width_),
                static_cast<int>(texture_height_),
                SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
            throw std::runtime_error{
                std::string{"SDL_SetRenderLogicalPresentation: "}
                + SDL_GetError()};
        }
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING, static_cast<int>(texture_width_),
            static_cast<int>(texture_height_));
        if (texture_ == nullptr) {
            throw std::runtime_error{
                std::string{"SDL_CreateTexture: "} + SDL_GetError()};
        }
        static_cast<void>(SDL_SetTextureScaleMode(texture_,
            enhanced_graphics_ ? SDL_SCALEMODE_LINEAR
                               : SDL_SCALEMODE_NEAREST));
        smooth_layer_ready_ = false;
        smooth_source_width_ = 0U;
        smooth_source_height_ = 0U;
        smooth_target_width_ = 0U;
    }

    void apply_touch_controls(std::uint32_t width, std::uint32_t height) {
        const auto blend = [this, width, height](
                               std::int32_t x, std::int32_t y,
                               std::uint8_t red, std::uint8_t green,
                               std::uint8_t blue, std::uint8_t alpha) {
            if (x < 0 || y < 0
                || x >= static_cast<std::int32_t>(width)
                || y >= static_cast<std::int32_t>(height)) return;
            const auto pixel = (static_cast<std::size_t>(y) * width
                + static_cast<std::size_t>(x)) * 4U;
            const auto inverse = static_cast<std::uint32_t>(255U - alpha);
            rgba_[pixel] = static_cast<std::uint8_t>((rgba_[pixel] * inverse
                + red * alpha + 127U) / 255U);
            rgba_[pixel + 1U] = static_cast<std::uint8_t>((rgba_[pixel + 1U]
                * inverse + green * alpha + 127U) / 255U);
            rgba_[pixel + 2U] = static_cast<std::uint8_t>((rgba_[pixel + 2U]
                * inverse + blue * alpha + 127U) / 255U);
        };
        const auto box = [&blend](std::int32_t left, std::int32_t top,
                                  std::int32_t right, std::int32_t bottom,
                                  std::uint8_t red = 235U,
                                  std::uint8_t green = 245U,
                                  std::uint8_t blue = 255U) {
            for (auto y = top; y <= bottom; ++y) {
                for (auto x = left; x <= right; ++x) {
                    const auto edge = x == left || x == right
                        || y == top || y == bottom;
                    blend(x, y, edge ? red : 18U, edge ? green : 28U,
                        edge ? blue : 42U, edge ? 170U : 76U);
                }
            }
        };
        const auto w = static_cast<std::int32_t>(width);
        const auto h = static_cast<std::int32_t>(height);
        const auto dpad_x = w * 20 / 100;
        const auto dpad_y = h * 73 / 100;
        const auto unit = std::max(7, h / 28);
        box(dpad_x - unit, dpad_y - unit * 3,
            dpad_x + unit, dpad_y - unit);
        box(dpad_x - unit, dpad_y + unit,
            dpad_x + unit, dpad_y + unit * 3);
        box(dpad_x - unit * 3, dpad_y - unit,
            dpad_x - unit, dpad_y + unit);
        box(dpad_x + unit, dpad_y - unit,
            dpad_x + unit * 3, dpad_y + unit);
        box(dpad_x - unit, dpad_y - unit,
            dpad_x + unit, dpad_y + unit, 150U, 180U, 210U);

        const auto action = [&](std::int32_t percent_x,
                                std::int32_t percent_y,
                                std::uint8_t red, std::uint8_t green,
                                std::uint8_t blue) {
            const auto cx = w * percent_x / 100;
            const auto cy = h * percent_y / 100;
            box(cx - unit, cy - unit, cx + unit, cy + unit,
                red, green, blue);
        };
        action(89, 69, 100U, 235U, 120U);
        action(77, 81, 245U, 105U, 105U);
        action(77, 57, 100U, 155U, 255U);
        action(65, 69, 250U, 220U, 95U);
        box(7, 7, w * 30 / 100, 20, 205U, 215U, 230U);
        box(w * 70 / 100, 7, w - 8, 20, 205U, 215U, 230U);
        box(w * 36 / 100, h - 20, w * 47 / 100, h - 7,
            205U, 215U, 230U);
        box(w * 53 / 100, h - 20, w * 64 / 100, h - 7,
            205U, 215U, 230U);
    }

    void prepare_1440p_model_layer(
        const starfox::render::Framebuffer& framebuffer,
        const PresentationEffects& effects) {
        if (effects.model_surfaces == nullptr
            || effects.model_surfaces->empty()) return;
        const auto width = static_cast<std::size_t>(framebuffer.width());
        const auto height = static_cast<std::size_t>(framebuffer.height());
        const auto pixels = width * height;
        smooth_base_rgba_ = rgba_;
        smooth_model_rgba_.assign(rgba_.size(), 0U);
        smooth_model_mask_.assign(pixels, 0U);

        const auto first_x = std::max(0, effects.model_surface_x
            + static_cast<std::int32_t>(effects.model_surfaces->minimum_x()));
        const auto first_y = std::max(0, effects.model_surface_y
            + static_cast<std::int32_t>(effects.model_surfaces->minimum_y()));
        const auto last_x = std::min(
            static_cast<std::int32_t>(framebuffer.width()),
            effects.model_surface_x
                + static_cast<std::int32_t>(
                    effects.model_surfaces->maximum_x()) + 1);
        const auto last_y = std::min(
            static_cast<std::int32_t>(framebuffer.height()),
            effects.model_surface_y
                + static_cast<std::int32_t>(
                    effects.model_surfaces->maximum_y()) + 1);
        for (auto y = first_y; y < last_y; ++y) {
            for (auto x = first_x; x < last_x; ++x) {
                if (model_surface_at(framebuffer, effects, x, y) == nullptr) {
                    continue;
                }
                const auto index = static_cast<std::size_t>(y) * width
                    + static_cast<std::size_t>(x);
                const auto rgba_index = index * 4U;
                smooth_model_mask_[index] = 1U;
                std::copy_n(rgba_.begin()
                        + static_cast<std::ptrdiff_t>(rgba_index),
                    4, smooth_model_rgba_.begin()
                        + static_cast<std::ptrdiff_t>(rgba_index));
                smooth_model_rgba_[rgba_index + 3U] = 255U;
            }
        }

        // Remove only the one-pixel source silhouette from the base layer.
        // The linearly sampled model texture then owns those pixels at 1440p,
        // including fractional-alpha edge coverage, without leaving the old
        // low-resolution stair-step underneath it. Opaque interior pixels do
        // not need reconstruction and remain hidden by the model layer.
        constexpr std::array<std::array<std::int32_t, 2>, 8> neighbours{{
            {{-1, 0}}, {{1, 0}}, {{0, -1}}, {{0, 1}},
            {{-1, -1}}, {{1, -1}}, {{-1, 1}}, {{1, 1}},
        }};
        for (auto y = first_y; y < last_y; ++y) {
            for (auto x = first_x; x < last_x; ++x) {
                const auto index = static_cast<std::size_t>(y) * width
                    + static_cast<std::size_t>(x);
                if (smooth_model_mask_[index] == 0U) continue;
                std::optional<std::size_t> background;
                for (const auto& offset : neighbours) {
                    const auto nx = x + offset[0];
                    const auto ny = y + offset[1];
                    if (nx < 0 || ny < 0
                        || nx >= static_cast<std::int32_t>(width)
                        || ny >= static_cast<std::int32_t>(height)) continue;
                    const auto neighbour = static_cast<std::size_t>(ny) * width
                        + static_cast<std::size_t>(nx);
                    if (smooth_model_mask_[neighbour] == 0U) {
                        background = neighbour;
                        break;
                    }
                }
                if (!background) continue;
                std::copy_n(rgba_.begin()
                        + static_cast<std::ptrdiff_t>(*background * 4U),
                    4, smooth_base_rgba_.begin()
                        + static_cast<std::ptrdiff_t>(index * 4U));
            }
        }
        smooth_layer_ready_ = true;
    }

    void ensure_1440p_model_textures(
        std::uint32_t width, std::uint32_t height) {
        constexpr std::uint32_t target_height = 1440U;
        const auto target_width = static_cast<std::uint32_t>(std::llround(
            static_cast<double>(width) * target_height
            / static_cast<double>(height)));
        if (smooth_model_texture_ != nullptr
            && smooth_target_texture_ != nullptr
            && smooth_source_width_ == width
            && smooth_source_height_ == height
            && smooth_target_width_ == target_width) return;
        SDL_DestroyTexture(smooth_model_texture_);
        SDL_DestroyTexture(smooth_target_texture_);
        smooth_model_texture_ = SDL_CreateTexture(renderer_,
            SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
            static_cast<int>(width), static_cast<int>(height));
        smooth_target_texture_ = SDL_CreateTexture(renderer_,
            SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
            static_cast<int>(target_width), static_cast<int>(target_height));
        if (smooth_model_texture_ == nullptr
            || smooth_target_texture_ == nullptr) {
            throw std::runtime_error{
                std::string{"SDL_CreateTexture (1440p model target): "}
                + SDL_GetError()};
        }
        static_cast<void>(SDL_SetTextureScaleMode(
            smooth_model_texture_, SDL_SCALEMODE_LINEAR));
        static_cast<void>(SDL_SetTextureBlendMode(
            smooth_model_texture_, SDL_BLENDMODE_BLEND));
        static_cast<void>(SDL_SetTextureScaleMode(
            smooth_target_texture_, SDL_SCALEMODE_LINEAR));
        smooth_source_width_ = width;
        smooth_source_height_ = height;
        smooth_target_width_ = target_width;
    }

    [[nodiscard]] static std::uint16_t luma(
        std::span<const std::uint8_t> pixels, std::size_t pixel) noexcept {
        return static_cast<std::uint16_t>(
            (static_cast<std::uint32_t>(pixels[pixel]) * 77U
                + static_cast<std::uint32_t>(pixels[pixel + 1U]) * 150U
                + static_cast<std::uint32_t>(pixels[pixel + 2U]) * 29U)
            >> 8U);
    }

    [[nodiscard]] static const starfox::render::SurfaceSample* model_surface_at(
        const starfox::render::Framebuffer& framebuffer,
        const PresentationEffects& effects,
        std::int32_t x,
        std::int32_t y) noexcept {
        if (effects.model_surfaces == nullptr) return nullptr;
        const auto local_x = x - effects.model_surface_x;
        const auto local_y = y - effects.model_surface_y;
        if (local_x < 0 || local_y < 0
            || local_x >= static_cast<std::int32_t>(
                effects.model_surfaces->width())
            || local_y >= static_cast<std::int32_t>(
                effects.model_surfaces->height())
            || x < 0 || y < 0
            || x >= static_cast<std::int32_t>(framebuffer.width())
            || y >= static_cast<std::int32_t>(framebuffer.height())) {
            return nullptr;
        }
        const auto& sample = effects.model_surfaces->get(
            static_cast<std::uint32_t>(local_x),
            static_cast<std::uint32_t>(local_y));
        // A later particle, HUD element, or cartridge layer may cover the
        // polygon. Only shade the surface if its indexed colour still owns the
        // final composite pixel.
        if (!sample.valid || framebuffer.get(
                static_cast<std::uint32_t>(x),
                static_cast<std::uint32_t>(y)) != sample.palette_index) {
            return nullptr;
        }
        return &sample;
    }

    void capture_effect_source_region(
        std::int32_t first_x, std::int32_t first_y,
        std::int32_t last_x, std::int32_t last_y) {
        first_x = std::clamp(first_x, 0,
            static_cast<std::int32_t>(texture_width_));
        first_y = std::clamp(first_y, 0,
            static_cast<std::int32_t>(texture_height_));
        last_x = std::clamp(last_x, first_x,
            static_cast<std::int32_t>(texture_width_));
        last_y = std::clamp(last_y, first_y,
            static_cast<std::int32_t>(texture_height_));
        effect_source_x_ = first_x;
        effect_source_y_ = first_y;
        effect_source_width_ = static_cast<std::size_t>(last_x - first_x);
        const auto height = static_cast<std::size_t>(last_y - first_y);
        effect_source_.resize(effect_source_width_ * height * 4U);
        const auto source_width = static_cast<std::size_t>(texture_width_);
        for (std::size_t row = 0U; row < height; ++row) {
            const auto source = ((static_cast<std::size_t>(first_y) + row)
                * source_width + static_cast<std::size_t>(first_x)) * 4U;
            std::copy_n(rgba_.begin() + static_cast<std::ptrdiff_t>(source),
                static_cast<std::ptrdiff_t>(effect_source_width_ * 4U),
                effect_source_.begin()
                    + static_cast<std::ptrdiff_t>(row * effect_source_width_ * 4U));
        }
    }

    [[nodiscard]] std::size_t effect_source_pixel(
        std::int32_t x, std::int32_t y) const noexcept {
        return (static_cast<std::size_t>(y - effect_source_y_)
            * effect_source_width_
            + static_cast<std::size_t>(x - effect_source_x_)) * 4U;
    }

    void apply_enhanced_surfaces(
        const starfox::render::Framebuffer& framebuffer,
        const PresentationEffects& effects) {
        if (effects.model_surfaces == nullptr
            || effects.model_surfaces->empty()
            || framebuffer.width() < 3U || framebuffer.height() < 3U) {
            return;
        }
        const auto width = static_cast<std::size_t>(framebuffer.width());
        const auto first_x = std::max(1, effects.model_surface_x
            + static_cast<std::int32_t>(effects.model_surfaces->minimum_x()) - 1);
        const auto first_y = std::max(1, effects.model_surface_y
            + static_cast<std::int32_t>(effects.model_surfaces->minimum_y()) - 1);
        const auto last_x = std::min(
            static_cast<std::int32_t>(framebuffer.width()) - 1,
            effects.model_surface_x
                + static_cast<std::int32_t>(effects.model_surfaces->maximum_x()) + 1);
        const auto last_y = std::min(
            static_cast<std::int32_t>(framebuffer.height()) - 1,
            effects.model_surface_y
                + static_cast<std::int32_t>(effects.model_surfaces->maximum_y()) + 1);
        capture_effect_source_region(
            first_x - 1, first_y - 1, last_x + 1, last_y + 1);
        constexpr std::array<std::array<std::int32_t, 2>, 8> neighbours{{
            {{-1, 0}}, {{1, 0}}, {{0, -1}}, {{0, 1}},
            {{-1, -1}}, {{1, -1}}, {{-1, 1}}, {{1, 1}},
        }};
        for (auto y = first_y; y < last_y; ++y) {
            for (auto x = first_x; x < last_x; ++x) {
                const auto pixel = (static_cast<std::size_t>(y) * width
                    + static_cast<std::size_t>(x)) * 4U;
                const auto* centre = model_surface_at(framebuffer, effects, x, y);
                if (centre != nullptr) {
                    // Bilateral filtering smooths the source's coarse texture
                    // samples and checkerboard material dithering, but refuses
                    // to smear across a real polygon crease or depth break.
                    std::array<std::uint32_t, 3> total{
                        static_cast<std::uint32_t>(effect_source_[
                            effect_source_pixel(x, y)]) * 6U,
                        static_cast<std::uint32_t>(effect_source_[
                            effect_source_pixel(x, y) + 1U]) * 6U,
                        static_cast<std::uint32_t>(effect_source_[
                            effect_source_pixel(x, y) + 2U]) * 6U,
                    };
                    auto weight = 6U;
                    for (std::size_t index = 0; index < neighbours.size(); ++index) {
                        const auto nx = x + neighbours[index][0];
                        const auto ny = y + neighbours[index][1];
                        const auto neighbour_pixel = effect_source_pixel(nx, ny);
                        const auto* neighbour = model_surface_at(
                            framebuffer, effects, nx, ny);
                        if (neighbour == nullptr) continue;
                        const auto normal_dot = centre->normal_x * neighbour->normal_x
                            + centre->normal_y * neighbour->normal_y
                            + centre->normal_z * neighbour->normal_z;
                        const auto depth_limit = std::max(
                            24.0F, std::abs(centre->depth) * 0.025F);
                        if (normal_dot < 0.94F
                            || std::abs(centre->depth - neighbour->depth)
                                > depth_limit) {
                            continue;
                        }
                        const auto sample_weight = index < 4U ? 2U : 1U;
                        for (std::size_t component = 0U;
                             component < 3U; ++component) {
                            total[component] += effect_source_[
                                neighbour_pixel + component]
                                * sample_weight;
                        }
                        weight += sample_weight;
                    }
                    for (std::size_t component = 0U; component < 3U; ++component) {
                        rgba_[pixel + component] = static_cast<std::uint8_t>(
                            (total[component] + weight / 2U) / weight);
                    }
                }
            }
        }
    }

    void apply_smooth_polygons(
        const starfox::render::Framebuffer& framebuffer,
        const PresentationEffects& effects) {
        if (effects.model_surfaces == nullptr
            || effects.model_surfaces->empty()
            || framebuffer.width() < 3U || framebuffer.height() < 3U) {
            return;
        }
        const auto width = static_cast<std::size_t>(framebuffer.width());
        const auto first_x = std::max(1, effects.model_surface_x
            + static_cast<std::int32_t>(effects.model_surfaces->minimum_x()) - 1);
        const auto first_y = std::max(1, effects.model_surface_y
            + static_cast<std::int32_t>(effects.model_surfaces->minimum_y()) - 1);
        const auto last_x = std::min(
            static_cast<std::int32_t>(framebuffer.width()) - 1,
            effects.model_surface_x
                + static_cast<std::int32_t>(effects.model_surfaces->maximum_x()) + 1);
        const auto last_y = std::min(
            static_cast<std::int32_t>(framebuffer.height()) - 1,
            effects.model_surface_y
                + static_cast<std::int32_t>(effects.model_surfaces->maximum_y()) + 1);
        capture_effect_source_region(
            first_x - 1, first_y - 1, last_x + 1, last_y + 1);
        constexpr std::array<std::array<std::int32_t, 2>, 9> samples{{
            {{0, 0}}, {{-1, 0}}, {{1, 0}}, {{0, -1}}, {{0, 1}},
            {{-1, -1}}, {{1, -1}}, {{-1, 1}}, {{1, 1}},
        }};
        for (auto y = first_y; y < last_y; ++y) {
            for (auto x = first_x; x < last_x; ++x) {
                const auto pixel = (static_cast<std::size_t>(y) * width
                    + static_cast<std::size_t>(x)) * 4U;
                const auto* centre = model_surface_at(framebuffer, effects, x, y);
                std::array<std::uint32_t, 3> model_total{};
                std::array<std::uint32_t, 3> background_total{};
                std::array<std::uint32_t, 3> crease_total{};
                auto model_count = 0U;
                auto background_count = 0U;
                auto crease_count = 0U;
                for (const auto& offset : samples) {
                    const auto sample_x = x + offset[0];
                    const auto sample_y = y + offset[1];
                    const auto sample_pixel = effect_source_pixel(
                        sample_x, sample_y);
                    const auto* surface = model_surface_at(
                        framebuffer, effects, sample_x, sample_y);
                    if (surface == nullptr) {
                        for (std::size_t component = 0U;
                             component < 3U; ++component) {
                            background_total[component] += effect_source_[
                                sample_pixel + component];
                        }
                        ++background_count;
                        continue;
                    }
                    for (std::size_t component = 0U; component < 3U; ++component) {
                        model_total[component] += effect_source_[
                            sample_pixel + component];
                    }
                    ++model_count;
                    if (centre != nullptr && surface != centre) {
                        const auto normal_dot = centre->normal_x * surface->normal_x
                            + centre->normal_y * surface->normal_y
                            + centre->normal_z * surface->normal_z;
                        if (normal_dot < 0.90F) {
                            for (std::size_t component = 0U;
                                 component < 3U; ++component) {
                                crease_total[component] += effect_source_[
                                    sample_pixel + component];
                            }
                            ++crease_count;
                        }
                    }
                }

                if (centre != nullptr && background_count != 0U) {
                    // Reconstruct a narrow fractional-coverage edge on both
                    // sides of the binary source silhouette. Keep most of the
                    // owning face at boundary pixels so the low-resolution
                    // models become clean rather than soft or out of focus.
                    const auto coverage = std::clamp(
                        (model_count + 5U) * 256U / 14U, 184U, 246U);
                    for (std::size_t component = 0U; component < 3U; ++component) {
                        const auto model_colour = (model_total[component]
                            + model_count / 2U) / model_count;
                        const auto background_colour = (background_total[component]
                            + background_count / 2U) / background_count;
                        rgba_[pixel + component] = static_cast<std::uint8_t>(
                            (model_colour * coverage
                                + background_colour * (256U - coverage) + 128U)
                            / 256U);
                    }
                } else if (centre == nullptr && model_count != 0U) {
                    const auto coverage = std::min(51U, model_count * 9U);
                    const auto background_divisor = std::max(1U, background_count);
                    for (std::size_t component = 0U; component < 3U; ++component) {
                        const auto model_colour = (model_total[component]
                            + model_count / 2U) / model_count;
                        const auto background_colour = (background_total[component]
                            + background_divisor / 2U) / background_divisor;
                        rgba_[pixel + component] = static_cast<std::uint8_t>(
                            (background_colour * (256U - coverage)
                                + model_colour * coverage + 128U) / 256U);
                    }
                } else if (crease_count != 0U) {
                    // Internal polygon boundaries receive a much narrower
                    // sub-pixel blend than the silhouette, retaining the
                    // low-poly facets while removing diagonal stair-steps.
                    const auto amount = std::min(38U, crease_count * 9U);
                    const auto source_pixel = effect_source_pixel(x, y);
                    for (std::size_t component = 0U; component < 3U; ++component) {
                        const auto crease_colour = (crease_total[component]
                            + crease_count / 2U) / crease_count;
                        rgba_[pixel + component] = static_cast<std::uint8_t>(
                            (effect_source_[source_pixel + component]
                                    * (256U - amount)
                                + crease_colour * amount + 128U) / 256U);
                    }
                }
            }
        }
    }

    void apply_rtx_lighting(
        const starfox::render::Framebuffer& framebuffer,
        const PresentationEffects& effects) {
        if (effects.model_surfaces == nullptr
            || effects.model_surfaces->empty()
            || framebuffer.width() < 3U || framebuffer.height() < 3U) {
            return;
        }
        const auto width = static_cast<std::size_t>(framebuffer.width());
        const auto first_x = std::max(1, effects.model_surface_x
            + static_cast<std::int32_t>(effects.model_surfaces->minimum_x()));
        const auto first_y = std::max(1, effects.model_surface_y
            + static_cast<std::int32_t>(effects.model_surfaces->minimum_y()));
        const auto last_x = std::min(
            static_cast<std::int32_t>(framebuffer.width()) - 1,
            effects.model_surface_x
                + static_cast<std::int32_t>(effects.model_surfaces->maximum_x()));
        const auto last_y = std::min(
            static_cast<std::int32_t>(framebuffer.height()) - 1,
            effects.model_surface_y
                + static_cast<std::int32_t>(effects.model_surfaces->maximum_y()));
        // Camera-space key light from above-left, a cool frontal fill, and the
        // camera-facing half vector used for a tight material highlight.
        constexpr std::array<float, 3> key{-0.474F, -0.632F, -0.613F};
        constexpr std::array<float, 3> fill{0.422F, 0.211F, -0.881F};
        constexpr std::array<float, 3> half_vector{-0.267F, -0.356F, -0.895F};
        for (auto y = first_y; y < last_y; ++y) {
            for (auto x = first_x; x < last_x; ++x) {
                const auto* sample = model_surface_at(framebuffer, effects, x, y);
                if (sample == nullptr) continue;
                auto nx = sample->normal_x;
                auto ny = sample->normal_y;
                auto nz = sample->normal_z;
                // Source shapes are not consistent about winding. Orient the
                // visible flat toward the camera before evaluating PC lights.
                if (nz > 0.0) {
                    nx = -nx;
                    ny = -ny;
                    nz = -nz;
                }
                const auto key_light = std::max(
                    0.0F, nx * key[0] + ny * key[1] + nz * key[2]);
                const auto fill_light = std::max(
                    0.0F, nx * fill[0] + ny * fill[1] + nz * fill[2]);
                const auto facing = std::clamp(-nz, 0.0F, 1.0F);
                const auto rim_base = 1.0F - facing;
                const auto rim = rim_base * rim_base * 0.32F;
                const auto specular_dot = std::max(0.0F,
                    nx * half_vector[0] + ny * half_vector[1]
                        + nz * half_vector[2]);
                const auto specular_2 = specular_dot * specular_dot;
                const auto specular_4 = specular_2 * specular_2;
                const auto specular_8 = specular_4 * specular_4;
                const auto specular_16 = specular_8 * specular_8;
                const auto specular = specular_16 * specular_4 * 105.0F;

                auto nearer_neighbours = 0U;
                constexpr std::array<std::array<std::int32_t, 2>, 4> adjacent{{
                    {{-1, 0}}, {{1, 0}}, {{0, -1}}, {{0, 1}},
                }};
                for (const auto& offset : adjacent) {
                    const auto* neighbour = model_surface_at(
                        framebuffer, effects, x + offset[0], y + offset[1]);
                    if (neighbour != nullptr
                        && neighbour->depth < sample->depth
                            - std::max(20.0F, std::abs(sample->depth) * 0.02F)) {
                        ++nearer_neighbours;
                    }
                }
                const auto occlusion = static_cast<float>(nearer_neighbours) * 0.055F;
                const auto illumination = std::clamp(
                    0.34F + key_light * 1.02F + fill_light * 0.24F
                        + rim - occlusion,
                    0.28F, 1.58F);
                const auto pixel = (static_cast<std::size_t>(y) * width
                    + static_cast<std::size_t>(x)) * 4U;
                constexpr std::array<float, 3> warmth{1.08F, 1.0F, 0.91F};
                constexpr std::array<float, 3> highlight{1.0F, 0.94F, 0.78F};
                for (std::size_t component = 0U; component < 3U; ++component) {
                    const auto value = rgba_[pixel + component] * illumination
                            * warmth[component]
                        + specular * highlight[component];
                    rgba_[pixel + component] = static_cast<std::uint8_t>(
                        std::clamp(static_cast<std::int32_t>(value + 0.5F),
                            0, 255));
                }
            }
        }
    }

    void apply_fxaa(starfox::simulation::AntiAliasingMode mode) {
        if (texture_width_ < 3U || texture_height_ < 3U) return;
        auto threshold_floor = std::uint16_t{12U};
        auto relative_divisor = std::uint16_t{8U};
        auto centre_weight = std::uint32_t{2U};
        auto neighbour_weight = std::uint32_t{1U};
        switch (mode) {
        case starfox::simulation::AntiAliasingMode::light:
            threshold_floor = 20U;
            relative_divisor = 6U;
            centre_weight = 6U;
            break;
        case starfox::simulation::AntiAliasingMode::heavy:
            threshold_floor = 6U;
            relative_divisor = 12U;
            centre_weight = 1U;
            break;
        case starfox::simulation::AntiAliasingMode::medium:
        case starfox::simulation::AntiAliasingMode::off:
        default:
            break;
        }
        const auto total_weight = centre_weight + neighbour_weight * 2U;
        capture_effect_source_region(
            0, 0, static_cast<std::int32_t>(texture_width_),
            static_cast<std::int32_t>(texture_height_));
        const auto& source = effect_source_;
        const auto width = static_cast<std::size_t>(texture_width_);
        const auto pixel_count = width * texture_height_;
        luma_scratch_.resize(pixel_count);
        for (std::size_t index = 0U; index < pixel_count; ++index) {
            luma_scratch_[index] = luma(source, index * 4U);
        }
        for (std::size_t y = 1U; y + 1U < texture_height_; ++y) {
            for (std::size_t x = 1U; x + 1U < texture_width_; ++x) {
                const auto pixel = (y * width + x) * 4U;
                const auto left = pixel - 4U;
                const auto right = pixel + 4U;
                const auto up = pixel - width * 4U;
                const auto down = pixel + width * 4U;
                const auto luma_pixel = y * width + x;
                const auto centre_luma = luma_scratch_[luma_pixel];
                const auto left_luma = luma_scratch_[luma_pixel - 1U];
                const auto right_luma = luma_scratch_[luma_pixel + 1U];
                const auto up_luma = luma_scratch_[luma_pixel - width];
                const auto down_luma = luma_scratch_[luma_pixel + width];
                const auto minimum = std::min({centre_luma, left_luma,
                    right_luma, up_luma, down_luma});
                const auto maximum = std::max({centre_luma, left_luma,
                    right_luma, up_luma, down_luma});
                const auto range = static_cast<std::uint16_t>(maximum - minimum);
                if (range < std::max<std::uint16_t>(
                        threshold_floor, maximum / relative_divisor)) continue;
                const auto horizontal = std::abs(
                    static_cast<std::int32_t>(left_luma)
                    - static_cast<std::int32_t>(right_luma));
                const auto vertical = std::abs(
                    static_cast<std::int32_t>(up_luma)
                    - static_cast<std::int32_t>(down_luma));
                const auto first = horizontal >= vertical ? up : left;
                const auto second = horizontal >= vertical ? down : right;
                for (std::size_t component = 0U; component < 3U; ++component) {
                    rgba_[pixel + component] = static_cast<std::uint8_t>(
                        (static_cast<std::uint32_t>(source[pixel + component])
                                * centre_weight
                            + static_cast<std::uint32_t>(
                                source[first + component]) * neighbour_weight
                            + static_cast<std::uint32_t>(
                                source[second + component]) * neighbour_weight)
                            / total_weight);
                }
            }
        }
    }

    void set_windowed_size(std::uint32_t width, std::uint32_t height) noexcept {
        const auto integer_scale = width <= snes_width
            ? 4U : (width <= widescreen_16_9_width ? 3U : 2U);
        SDL_SetWindowSize(window_,
            static_cast<int>(width * integer_scale),
            static_cast<int>(height * integer_scale));
    }

    void present_rgba_pixels(std::uint32_t width, std::uint32_t height,
        std::span<const std::uint8_t> rgba) {
        update_temporary_status();
        const auto base = smooth_layer_ready_
            ? std::span<const std::uint8_t>{smooth_base_rgba_} : rgba;
        if (!SDL_UpdateTexture(texture_, nullptr, base.data(),
                static_cast<int>(width * 4U))) {
            throw std::runtime_error{std::string{"SDL_UpdateTexture: "} + SDL_GetError()};
        }
        if (smooth_layer_ready_) {
            ensure_1440p_model_textures(width, height);
            if (!SDL_UpdateTexture(smooth_model_texture_, nullptr,
                    smooth_model_rgba_.data(), static_cast<int>(width * 4U))) {
                throw std::runtime_error{
                    std::string{"SDL_UpdateTexture (1440p model): "}
                    + SDL_GetError()};
            }
            if (!SDL_SetRenderTarget(renderer_, smooth_target_texture_)) {
                throw std::runtime_error{
                    std::string{"SDL_SetRenderTarget (1440p model): "}
                    + SDL_GetError()};
            }
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
            SDL_RenderClear(renderer_);
            SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
            SDL_RenderTexture(renderer_, smooth_model_texture_, nullptr, nullptr);
            if (!SDL_SetRenderTarget(renderer_, nullptr)) {
                throw std::runtime_error{
                    std::string{"SDL_SetRenderTarget (window): "}
                    + SDL_GetError()};
            }
        }
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderTexture(renderer_, smooth_layer_ready_
            ? smooth_target_texture_ : texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }
    void ensure_dimensions(std::uint32_t width, std::uint32_t height) {
        if (width == texture_width_ && height == texture_height_) return;
        SDL_DestroyTexture(texture_);
        SDL_DestroyTexture(smooth_model_texture_);
        SDL_DestroyTexture(smooth_target_texture_);
        smooth_model_texture_ = nullptr;
        smooth_target_texture_ = nullptr;
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            static_cast<int>(width), static_cast<int>(height));
        if (texture_ == nullptr) {
            throw std::runtime_error{
                std::string{"SDL_CreateTexture: "} + SDL_GetError()};
        }
        SDL_SetTextureScaleMode(texture_, enhanced_graphics_
            ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        if (!SDL_SetRenderLogicalPresentation(renderer_,
                static_cast<int>(width), static_cast<int>(height),
                SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
            throw std::runtime_error{
                std::string{"SDL_SetRenderLogicalPresentation: "}
                + SDL_GetError()};
        }
        if ((SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) == 0U) {
            set_windowed_size(width, height);
        }
        texture_width_ = width;
        texture_height_ = height;
    }

    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* texture_{};
    SDL_Texture* smooth_model_texture_{};
    SDL_Texture* smooth_target_texture_{};
    std::uint32_t texture_width_{snes_width};
    std::uint32_t texture_height_{snes_height};
    bool relative_mouse_mode_{};
    starfox::simulation::AntiAliasingMode anti_aliasing_{
        starfox::simulation::AntiAliasingMode::off};
    bool enhanced_graphics_{};
    bool smooth_polys_{};
    bool rtx_lighting_{};
    bool vsync_{};
    starfox::simulation::RendererMode renderer_mode_{
        starfox::simulation::RendererMode::gpu};
    bool smooth_layer_ready_{};
    std::uint32_t smooth_source_width_{};
    std::uint32_t smooth_source_height_{};
    std::uint32_t smooth_target_width_{};
    std::optional<std::chrono::steady_clock::time_point>
        temporary_status_until_;
    std::vector<std::uint8_t> rgba_;
    std::vector<std::uint8_t> smooth_base_rgba_;
    std::vector<std::uint8_t> smooth_model_rgba_;
    std::vector<std::uint8_t> smooth_model_mask_;
    // Reused presentation scratch avoids allocating and copying a complete
    // 32:9 frame separately for every optional model effect.
    std::vector<std::uint8_t> effect_source_;
    std::vector<std::uint16_t> luma_scratch_;
    std::int32_t effect_source_x_{};
    std::int32_t effect_source_y_{};
    std::size_t effect_source_width_{};
};

class AudioOutput {
public:
    explicit AudioOutput(const starfox::audio::Msu1Pack& pack)
        : msu1_([&pack](std::uint16_t track) {
              return pack.load_track(track);
          }) {
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

    void set_msu1_enabled(bool enabled) noexcept {
        msu1_.set_enabled(enabled);
    }
    void set_volumes(std::uint8_t music, std::uint8_t effects) noexcept {
        music_volume_ = std::min<std::uint8_t>(music, 100U);
        sfx_volume_ = std::min<std::uint8_t>(effects, 100U);
    }
    void set_game_paused(bool paused) noexcept {
        msu1_.set_paused(paused);
    }
    [[nodiscard]] std::uint16_t msu1_track() const noexcept {
        return msu1_.selected_track();
    }
    [[nodiscard]] bool msu1_playing() const noexcept {
        return msu1_.playing();
    }

    [[nodiscard]] std::array<std::uint8_t, 4> queue_logic_tick(
        std::span<const starfox::simulation::ApuPortWrite> writes,
        std::span<const starfox::simulation::MsuRegisterWrite> msu_writes,
        bool fast_forward, bool queue_output = true) {
        static_cast<void>(emulator_.render_logic_tick(writes));
        msu1_.process_register_writes(msu_writes);
        const auto music = msu1_.enabled()
            ? std::span<const std::int16_t>{msu1_.render(
                starfox::audio::Spc700Audio::stereo_frames_per_logic_tick,
                starfox::audio::Spc700Audio::sample_rate)}
            : emulator_.last_music_samples();
        const auto effects = emulator_.last_effect_samples();
        mixed_samples_.resize(std::min(music.size(), effects.size()));
        for (std::size_t index = 0U; index < mixed_samples_.size(); ++index) {
            const auto mixed = static_cast<std::int32_t>(music[index])
                    * music_volume_ / 100
                + static_cast<std::int32_t>(effects[index])
                    * sfx_volume_ / 100;
            mixed_samples_[index] = static_cast<std::int16_t>(std::clamp(
                mixed,
                static_cast<std::int32_t>(
                    std::numeric_limits<std::int16_t>::min()),
                static_cast<std::int32_t>(
                    std::numeric_limits<std::int16_t>::max())));
        }
        auto& samples = mixed_samples_;
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

    [[nodiscard]] std::array<std::uint8_t, 4> prime_upload_sequence(
        std::span<const starfox::simulation::ApuPortWrite> writes) {
        static_cast<void>(emulator_.prime_upload_sequence(writes));
        return emulator_.output_ports();
    }

private:
    starfox::audio::Spc700Audio emulator_;
    starfox::audio::Msu1Audio msu1_;
    SDL_AudioStream* stream_{};
    std::vector<std::int16_t> fast_samples_;
    std::vector<std::int16_t> mixed_samples_;
    std::uint8_t music_volume_{100U};
    std::uint8_t sfx_volume_{100U};
    bool started_{};
    bool paused_{};
};

class RumbleOutput {
public:
    explicit RumbleOutput(const starfox::assets::SymbolMap& symbols)
        : command_(address(symbols, "RUMBLE_CMD")),
          time_(address(symbols, "RUMBLE_TIME")),
          index_(address(symbols, "RUMBLE_INDEX")),
          table_(address(symbols, "RUMBLE_TABLE")) {}

    void advance(starfox::simulation::MapVm& map, SDL_Gamepad* gamepad,
        bool enabled) noexcept {
        if (!available() || !enabled || gamepad == nullptr) {
            stop(gamepad);
            return;
        }
        auto output = std::uint8_t{};
        auto sequence_index = map.read_native_byte(index_);
        for (std::size_t guard = 0U; guard < 4U; ++guard) {
            if (sequence_index == 0U) {
                output = map.read_native_byte(time_) == 0U
                    ? 0U : map.read_native_byte(command_);
                break;
            }
            output = map.read_native_byte(
                table_ + static_cast<std::uint32_t>(sequence_index - 1U));
            sequence_index = static_cast<std::uint8_t>(sequence_index + 1U);
            map.write_native_byte(index_, sequence_index);
            if (output == 0x19U) {
                map.write_native_byte(index_, 0U);
                output = 0U;
                break;
            }
            if (output != 0x91U) break;
            sequence_index = 1U;
            map.write_native_byte(index_, sequence_index);
        }
        const auto remaining = map.read_native_byte(time_);
        if (remaining != 0U) {
            map.write_native_byte(time_,
                static_cast<std::uint8_t>(remaining - 1U));
        }
        const auto high_frequency = static_cast<std::uint16_t>(
            (output & 0x0fU) * 0x1111U);
        const auto low_frequency = static_cast<std::uint16_t>(
            ((output >> 4U) & 0x0fU) * 0x1111U);
        static_cast<void>(SDL_RumbleGamepad(
            gamepad, low_frequency, high_frequency, 40U));
        active_ = output != 0U;
    }

    void stop(SDL_Gamepad* gamepad) noexcept {
        if (!active_) return;
        if (gamepad != nullptr) {
            static_cast<void>(SDL_RumbleGamepad(gamepad, 0U, 0U, 0U));
        }
        active_ = false;
    }

private:
    static std::uint32_t address(
        const starfox::assets::SymbolMap& symbols, const char* name) noexcept {
        const auto found = symbols.find(name);
        return found.empty() ? 0U : found.front();
    }
    [[nodiscard]] bool available() const noexcept {
        return command_ != 0U && time_ != 0U && index_ != 0U && table_ != 0U;
    }

    std::uint32_t command_{};
    std::uint32_t time_{};
    std::uint32_t index_{};
    std::uint32_t table_{};
    bool active_{};
};

class MsuFadeOutput {
public:
    explicit MsuFadeOutput(const starfox::assets::SymbolMap& symbols)
        : flag_(address(symbols, "MSUFADEFLAG")),
          volume_(address(symbols, "CURMSUVOLUME")) {}

    [[nodiscard]] std::optional<starfox::simulation::MsuRegisterWrite>
        advance(starfox::simulation::MapVm& map) const noexcept {
        if (flag_ == 0U || volume_ == 0U
            || map.read_native_byte(flag_) == 0U) return std::nullopt;
        const auto current = map.read_native_byte(volume_);
        if (current == 0U) return std::nullopt;
        const auto next = static_cast<std::uint8_t>(
            current > 3U ? current - 3U : 0U);
        map.write_native_byte(volume_, next);
        return starfox::simulation::MsuRegisterWrite{0x2006U, next, 0U};
    }

private:
    static std::uint32_t address(
        const starfox::assets::SymbolMap& symbols, const char* name) noexcept {
        const auto found = symbols.find(name);
        return found.empty() ? 0U : found.front();
    }
    std::uint32_t flag_{};
    std::uint32_t volume_{};
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

void draw_hud_editor_chrome(
    starfox::render::Framebuffer& framebuffer,
    const starfox::render::ScaledTextRenderer& text_renderer,
    const HudEditorState& editor,
    const starfox::render::HudLayout& layout,
    starfox::simulation::Experience experience,
    starfox::simulation::DisplayMode display_mode,
    std::uint8_t background_colour,
    std::uint8_t foreground_colour) {
    const auto width = framebuffer.width();
    const auto solid = [&framebuffer](
                           std::int32_t x, std::int32_t y,
                           std::int32_t box_width, std::int32_t box_height,
                           std::uint8_t colour) {
        for (std::int32_t row = 0; row < box_height; ++row) {
            for (std::int32_t column = 0; column < box_width; ++column) {
                framebuffer.set(x + column, y + row, colour);
            }
        }
    };
    const auto box = [&solid](HudRect rect, std::uint8_t colour) {
        solid(rect.x, rect.y, rect.width, 1, colour);
        solid(rect.x, rect.y + rect.height - 1, rect.width, 1, colour);
        solid(rect.x, rect.y, 1, rect.height, colour);
        solid(rect.x + rect.width - 1, rect.y, 1, rect.height, colour);
    };
    solid(0, 0, static_cast<std::int32_t>(width), 11,
        background_colour);
    const auto title = (experience
            == starfox::simulation::Experience::starfox_ex
            ? std::string{"EX HUD  "}
            : (width == snes_width ? std::string{"HUD  "}
                                   : std::string{"HUD LAYOUT  "}))
        + std::string{display_profile_name(display_mode)};
    const auto title_width = text_renderer.measure_ascii(title);
    text_renderer.draw_ascii(title,
        std::max<std::int32_t>(0,
            (static_cast<std::int32_t>(width) - title_width) / 2),
        2, framebuffer, 0U, foreground_colour);
    solid(0, 210, static_cast<std::int32_t>(width), 14,
        background_colour);
    const auto reset = hud_reset_button_rect(width);
    const auto done = hud_done_button_rect(width);
    if (reset.contains(editor.pointer_x, editor.pointer_y)) {
        box(reset, foreground_colour);
    }
    if (done.contains(editor.pointer_x, editor.pointer_y)) {
        box(done, foreground_colour);
    }
    text_renderer.draw_ascii("Y RESET", reset.x + 6,
        reset.y + 3, framebuffer, 0U, foreground_colour);
    text_renderer.draw_ascii("B DONE", done.x + 2,
        done.y + 3, framebuffer, 0U, foreground_colour);

    std::optional<starfox::render::HudElement> hovered;
    auto hovered_area = std::numeric_limits<std::int32_t>::max();
    for (std::uint8_t value = 0U;
         value < static_cast<std::uint8_t>(starfox::render::HudElement::count);
         ++value) {
        const auto element =
            static_cast<starfox::render::HudElement>(value);
        const auto rect = placed_hud_rect(
            element, width, layout, experience);
        const auto area = rect.width * rect.height;
        if (rect.contains(editor.pointer_x, editor.pointer_y)
            && area < hovered_area) {
            hovered = element;
            hovered_area = area;
        }
    }
    const auto selected = editor.dragging ? editor.dragging : hovered;
    if (selected) {
        auto rect = placed_hud_rect(
            *selected, width, layout, experience);
        constexpr std::int32_t length = 5;
        --rect.x;
        --rect.y;
        rect.width += 2;
        rect.height += 2;
        solid(rect.x, rect.y, length, 1, foreground_colour);
        solid(rect.x, rect.y, 1, length, foreground_colour);
        solid(rect.x + rect.width - length, rect.y,
            length, 1, foreground_colour);
        solid(rect.x + rect.width - 1, rect.y,
            1, length, foreground_colour);
        solid(rect.x, rect.y + rect.height - 1,
            length, 1, foreground_colour);
        solid(rect.x, rect.y + rect.height - length,
            1, length, foreground_colour);
        solid(rect.x + rect.width - length,
            rect.y + rect.height - 1, length, 1, foreground_colour);
        solid(rect.x + rect.width - 1,
            rect.y + rect.height - length, 1, length,
            foreground_colour);
    }
}

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

struct ExMouseInputLatch {
    std::int32_t delta_x{};
    std::int32_t delta_y{};
    std::uint8_t buttons{};
    std::int32_t scope_x{0x8a};
    std::int32_t scope_y{0x62};

    void add_motion(float x, float y) noexcept {
        const auto rounded_x = std::lround(x);
        const auto rounded_y = std::lround(y);
        delta_x = std::clamp<std::int64_t>(
            static_cast<std::int64_t>(delta_x) + rounded_x,
            std::numeric_limits<std::int16_t>::min(),
            std::numeric_limits<std::int16_t>::max());
        delta_y = std::clamp<std::int64_t>(
            static_cast<std::int64_t>(delta_y) + rounded_y,
            std::numeric_limits<std::int16_t>::min(),
            std::numeric_limits<std::int16_t>::max());
        scope_x = std::clamp<std::int64_t>(
            static_cast<std::int64_t>(scope_x) + rounded_x, 0, 255);
        scope_y = std::clamp<std::int64_t>(
            static_cast<std::int64_t>(scope_y) + rounded_y, 0, 223);
    }

    void set_button(std::uint8_t mask, bool held) noexcept {
        if (held) buttons = static_cast<std::uint8_t>(buttons | mask);
        else buttons = static_cast<std::uint8_t>(buttons & ~mask);
    }

    [[nodiscard]] starfox::simulation::MouseInputState consume() noexcept {
        const auto result = starfox::simulation::MouseInputState{
            static_cast<std::int16_t>(delta_x),
            static_cast<std::int16_t>(delta_y),
            buttons,
            static_cast<std::uint8_t>(scope_x),
            static_cast<std::uint8_t>(scope_y),
        };
        delta_x = 0;
        delta_y = 0;
        return result;
    }

    void release() noexcept {
        delta_x = 0;
        delta_y = 0;
        buttons = 0U;
    }
};

class TouchControls {
public:
    TouchControls() noexcept {
#if defined(__ANDROID__) || defined(__IPHONEOS__) || defined(__SWITCH__)
        visible_ = true;
#endif
    }

    void update(SDL_FingerID finger, float x, float y) {
        visible_ = true;
        fingers_[finger] = hit_test(x, y);
    }
    void release(SDL_FingerID finger) noexcept { fingers_.erase(finger); }
    void reset() noexcept { fingers_.clear(); }
    [[nodiscard]] bool visible() const noexcept { return visible_; }
    [[nodiscard]] ButtonMask buttons() const noexcept {
        auto result = ButtonMask{};
        for (const auto& [finger, buttons] : fingers_) {
            static_cast<void>(finger);
            result = static_cast<ButtonMask>(result | buttons);
        }
        return result;
    }

private:
    [[nodiscard]] static ButtonMask hit_test(float x, float y) noexcept {
        using namespace starfox::input;
        auto result = ButtonMask{};
        if (y < 0.18F) {
            if (x < 0.30F) result |= left_shoulder;
            if (x > 0.70F) result |= right_shoulder;
        }
        if (x >= 0.34F && x <= 0.47F && y >= 0.87F) {
            result |= starfox::input::select;
        }
        if (x >= 0.53F && x <= 0.66F && y >= 0.87F) result |= start;

        if (x < 0.43F && y > 0.43F) {
            constexpr float centre_x = 0.20F;
            constexpr float centre_y = 0.73F;
            const auto dx = x - centre_x;
            const auto dy = y - centre_y;
            if (dx < -0.045F) result |= left;
            if (dx > 0.045F) result |= right;
            if (dy < -0.045F) result |= up;
            if (dy > 0.045F) result |= down;
        }
        const auto inside = [x, y](float cx, float cy) {
            const auto dx = x - cx;
            const auto dy = y - cy;
            return dx * dx + dy * dy <= 0.0064F;
        };
        if (inside(0.89F, 0.69F)) result |= starfox::input::a;
        if (inside(0.77F, 0.81F)) result |= starfox::input::b;
        if (inside(0.77F, 0.57F)) result |= starfox::input::x;
        if (inside(0.65F, 0.69F)) result |= starfox::input::y;
        return result;
    }

    std::unordered_map<SDL_FingerID, ButtonMask> fingers_;
    bool visible_{};
};

std::uint16_t sample_ntt_data_pad(const bool* keys) noexcept {
    const auto shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    const auto digit = [keys, shift](SDL_Scancode primary, SDL_Scancode keypad) {
        return (!shift && keys[primary]) || keys[keypad];
    };
    std::uint16_t held{};
    if (digit(SDL_SCANCODE_0, SDL_SCANCODE_KP_0)) held |= 0x0001U;
    if (digit(SDL_SCANCODE_1, SDL_SCANCODE_KP_1)) held |= 0x0002U;
    if (digit(SDL_SCANCODE_2, SDL_SCANCODE_KP_2)) held |= 0x0004U;
    if (digit(SDL_SCANCODE_3, SDL_SCANCODE_KP_3)) held |= 0x0008U;
    if (digit(SDL_SCANCODE_4, SDL_SCANCODE_KP_4)) held |= 0x0010U;
    if (digit(SDL_SCANCODE_5, SDL_SCANCODE_KP_5)) held |= 0x0020U;
    if (digit(SDL_SCANCODE_6, SDL_SCANCODE_KP_6)) held |= 0x0040U;
    if (digit(SDL_SCANCODE_7, SDL_SCANCODE_KP_7)) held |= 0x0080U;
    if (digit(SDL_SCANCODE_8, SDL_SCANCODE_KP_8)) held |= 0x0100U;
    if (digit(SDL_SCANCODE_9, SDL_SCANCODE_KP_9)) held |= 0x0200U;
    if (keys[SDL_SCANCODE_KP_MULTIPLY]
        || (shift && keys[SDL_SCANCODE_8])) held |= 0x0400U;
    if (keys[SDL_SCANCODE_KP_DIVIDE]
        || (shift && keys[SDL_SCANCODE_3])) held |= 0x0800U;
    if (keys[SDL_SCANCODE_PERIOD] || keys[SDL_SCANCODE_KP_PERIOD]) {
        held |= 0x1000U;
    }
    if (keys[SDL_SCANCODE_C]) held |= 0x2000U;
    if (keys[SDL_SCANCODE_H]) held |= 0x8000U;
    return held;
}

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
#if defined(_WIN32)
    // Keep the lock alive through the catch block and its modal error dialog.
    // If it lived inside try, stack unwinding released it before MessageBoxA;
    // a second launch could then enter and display an identical second box.
    struct SingleInstanceMutex {
        HANDLE handle{};
        ~SingleInstanceMutex() {
            if (handle != nullptr) CloseHandle(handle);
        }
    } single_instance;
    if (std::getenv("STARFOX_TEST_FRAMES") == nullptr) {
        single_instance.handle = CreateMutexW(nullptr, FALSE,
            L"Local\\StarFoxEnhanced.NativePCRuntime.SingleInstance");
        if (single_instance.handle == nullptr) return 1;
        if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    }
#endif
    try {
        const auto saved_pregame_path = starfox::app::pregame_settings_path();
        auto saved_pregame = starfox::app::PregameSettings{};
        static_cast<void>(starfox::app::load_pregame_settings(
            saved_pregame_path, saved_pregame));
        const SdlContext sdl;
        Window window{static_cast<starfox::simulation::RendererMode>(
            saved_pregame.renderer_mode)};
        std::string initial_map = "BOOT";
        const auto executable_directory =
            std::filesystem::absolute(argv[0]).parent_path();
        const starfox::audio::Msu1Pack msu1_pack{
            executable_directory
                / std::filesystem::path{starfox::audio::msu1_pack_filename}};
#if defined(STARFOX_HAS_EMBEDDED_ASSETS)
        auto embedded_runtime_assets =
            load_or_compile_runtime_assets(executable_directory);
#endif
        const auto original_assets = [&]() -> RuntimeAssets {
            if (argc == 1 || argc == 2) {
                if (argc == 2) initial_map = argv[1];
#if defined(STARFOX_HAS_EMBEDDED_ASSETS)
                return std::move(embedded_runtime_assets.original);
#else
                std::filesystem::path rom_path;
                std::filesystem::path symbols_path;
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
        std::optional<RuntimeAssets> starfox_ex_assets;
#if defined(STARFOX_HAS_EMBEDDED_ASSETS)
        starfox_ex_assets.emplace(
            std::move(embedded_runtime_assets.starfox_ex));
#else
        const auto workspace = executable_directory.parent_path().parent_path();
        const auto current = std::filesystem::current_path();
        const std::array ex_candidates{
            std::pair{executable_directory / "SFES.SFC",
                executable_directory / "SFES-SYMBOLS.TXT"},
            std::pair{current / "upstream-star-fox-ex" / "SFES" / "SFES.SFC",
                current / "upstream-star-fox-ex" / "SYMBOLS.TXT"},
            std::pair{current / "build" / "upstream-star-fox-ex" / "SFES" / "SFES.SFC",
                current / "build" / "upstream-star-fox-ex" / "SYMBOLS.TXT"},
            std::pair{workspace / "upstream-star-fox-ex" / "SFES" / "SFES.SFC",
                workspace / "upstream-star-fox-ex" / "SYMBOLS.TXT"},
        };
        for (const auto& [candidate_rom, candidate_symbols] : ex_candidates) {
            if (std::filesystem::exists(candidate_rom)
                && std::filesystem::exists(candidate_symbols)) {
                starfox_ex_assets.emplace(
                    load_external_assets(candidate_rom, candidate_symbols));
                break;
            }
        }
#endif
        const auto ex_save_path = starfox::app::starfox_ex_save_ram_path();
        auto persisted_ex_save = std::vector<std::uint8_t>{};
        const auto persist_ex_save =
            std::getenv("STARFOX_TEST_FRAMES") == nullptr
            && std::getenv("STARFOX_TEST_EXPERIENCE") == nullptr
            && std::getenv("STARFOX_TEST_PRESSES") == nullptr;
        if (persist_ex_save) {
            static_cast<void>(starfox::app::load_starfox_ex_save_ram(
                ex_save_path, persisted_ex_save));
        }
        auto active_experience = static_cast<starfox::simulation::Experience>(
            saved_pregame.experience);
        if (const auto* forced_experience = std::getenv(
                "STARFOX_TEST_EXPERIENCE")) {
            active_experience = std::string_view{forced_experience} == "EX"
                ? starfox::simulation::Experience::starfox_ex
                : starfox::simulation::Experience::original;
        }
        bool restart_runtime = true;
        bool first_runtime = true;
        std::optional<starfox::render::PresentationHistory>
            presentation_history;
        bool launch_hud_editor_preview =
            std::getenv("STARFOX_TEST_HUD_EDITOR") != nullptr;
        if (launch_hud_editor_preview) {
            initial_map = "LEVEL1_1";
        }
        while (restart_runtime) {
        restart_runtime = false;
        const auto hud_editor_preview =
            std::exchange(launch_hud_editor_preview, false);
        if (active_experience == starfox::simulation::Experience::starfox_ex
            && !starfox_ex_assets) {
            throw std::runtime_error{
                "Star Fox EX runtime assets are not installed in this build"};
        }
        const auto& assets = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? *starfox_ex_assets : original_assets;
        const auto& rom = assets.rom;
        const auto& symbols = assets.symbols;
        const starfox::assets::ShapeDecoder decoder{rom, symbols};
        const auto trigonometry = starfox::simulation::TrigTables::load(rom, symbols);
        const auto initial_ex_save = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? std::span<const std::uint8_t>{persisted_ex_save}
            : std::span<const std::uint8_t>{};
        starfox::simulation::GameSimulation game{
            rom, symbols, initial_map, initial_ex_save, true};
        auto warned_ex_save_failure = false;
        const auto synchronize_ex_save = [&] {
            if (active_experience
                    != starfox::simulation::Experience::starfox_ex) return;
            const auto current_save = game.ex_save_ram();
            if (persisted_ex_save.size() == current_save.size()
                && std::equal(persisted_ex_save.begin(),
                    persisted_ex_save.end(), current_save.begin())) return;
            if (persist_ex_save
                && !starfox::app::save_starfox_ex_save_ram(
                    ex_save_path, current_save)
                && !warned_ex_save_failure) {
                std::cerr << "warning: could not save Star Fox EX cartridge RAM to "
                          << ex_save_path << '\n';
                warned_ex_save_failure = true;
            }
            // Keep the last observed image even if the filesystem is
            // unavailable. This prevents a failed write from being retried
            // every 20 Hz logic tick and preserves the choices if the user
            // switches experiences again within this process.
            persisted_ex_save.assign(current_save.begin(), current_save.end());
        };
        if (!hud_editor_preview) synchronize_ex_save();
        const auto capture_pregame_settings = [&game] {
            return starfox::app::PregameSettings{
                static_cast<std::uint8_t>(game.timing_mode()),
                game.presentation_fps(),
                static_cast<std::uint8_t>(game.display_mode()),
                game.god_mode(),
                game.show_fps(),
                static_cast<std::uint8_t>(game.anti_aliasing_mode()),
                game.enhanced_graphics(),
                game.smooth_polys(),
                game.rtx_lighting(),
                game.vsync(),
                static_cast<std::uint8_t>(game.renderer_mode()),
                game.msu1_music(),
                game.rumble(),
                static_cast<std::uint8_t>(game.crosshair_colour()),
                static_cast<std::uint8_t>(game.experience()),
                game.music_volume(),
                game.sfx_volume(),
            };
        };
        {
            game.set_timing_mode(static_cast<starfox::simulation::TimingMode>(
                saved_pregame.timing_mode));
            if (const auto* forced_timing = std::getenv(
                    "STARFOX_TEST_TIMING_MODE")) {
                game.set_timing_mode(std::string_view{forced_timing}
                        == "UNLOCKED"
                    ? starfox::simulation::TimingMode::unlocked_20_fps
                    : starfox::simulation::TimingMode::original_speed);
            }
            game.set_presentation_fps(saved_pregame.presentation_fps);
            if (const auto* forced_fps = std::getenv(
                    "STARFOX_TEST_PRESENTATION_FPS")) {
                game.set_presentation_fps(static_cast<std::uint16_t>(
                    std::stoul(forced_fps)));
            }
            game.set_display_mode(static_cast<starfox::simulation::DisplayMode>(
                saved_pregame.display_mode));
            if (const auto* forced_display = std::getenv(
                    "STARFOX_TEST_DISPLAY_MODE")) {
                game.set_display_mode(static_cast<
                    starfox::simulation::DisplayMode>(std::clamp(
                        std::stoi(forced_display), 0, 4)));
            }
            game.set_god_mode(saved_pregame.god_mode);
            game.set_show_fps(saved_pregame.show_fps);
            game.set_anti_aliasing_mode(
                static_cast<starfox::simulation::AntiAliasingMode>(
                    saved_pregame.anti_aliasing));
            if (const auto* forced_aa = std::getenv(
                    "STARFOX_TEST_ANTI_ALIASING")) {
                const auto mode = std::string_view{forced_aa};
                game.set_anti_aliasing_mode(mode == "LIGHT" || mode == "1"
                        ? starfox::simulation::AntiAliasingMode::light
                    : mode == "MEDIUM" || mode == "2"
                        ? starfox::simulation::AntiAliasingMode::medium
                    : mode == "HEAVY" || mode == "3"
                        ? starfox::simulation::AntiAliasingMode::heavy
                        : starfox::simulation::AntiAliasingMode::off);
            }
            game.set_enhanced_graphics(saved_pregame.enhanced_graphics);
            game.set_smooth_polys(saved_pregame.smooth_polys);
            game.set_rtx_lighting(saved_pregame.rtx_lighting);
            game.set_vsync(saved_pregame.vsync);
            game.set_renderer_mode(
                static_cast<starfox::simulation::RendererMode>(
                    saved_pregame.renderer_mode));
            game.set_msu1_available(msu1_pack.available());
            game.set_msu1_music(saved_pregame.msu1_music);
            game.set_rumble(saved_pregame.rumble);
            game.set_music_volume(saved_pregame.music_volume);
            game.set_sfx_volume(saved_pregame.sfx_volume);
            if (const auto* forced_msu = std::getenv("STARFOX_TEST_MSU1")) {
                game.set_msu1_music(std::string_view{forced_msu} != "0");
            }
            if (const auto* forced_vsync = std::getenv("STARFOX_TEST_VSYNC")) {
                game.set_vsync(std::string_view{forced_vsync} != "0");
            }
            if (const auto* forced_renderer = std::getenv(
                    "STARFOX_TEST_RENDERER")) {
                const auto value = std::string_view{forced_renderer};
                game.set_renderer_mode(value == "SOFTWARE" || value == "1"
                    ? starfox::simulation::RendererMode::software
                    : starfox::simulation::RendererMode::gpu);
            }
            if (const auto* forced_enhanced = std::getenv(
                    "STARFOX_TEST_ENHANCED")) {
                game.set_enhanced_graphics(std::string_view{forced_enhanced}
                    != "0");
            }
            if (const auto* forced_smoothing = std::getenv(
                    "STARFOX_TEST_SMOOTH_POLYS")) {
                game.set_smooth_polys(std::string_view{forced_smoothing} != "0");
            }
            if (const auto* forced_lighting = std::getenv(
                    "STARFOX_TEST_RTX_LIGHTING")) {
                game.set_rtx_lighting(std::string_view{forced_lighting} != "0");
            }
            game.set_crosshair_colour(
                static_cast<starfox::simulation::CrosshairColour>(
                    saved_pregame.crosshair_colour));
            game.set_experience(active_experience);
            if (hud_editor_preview) {
                // Build the editor's static reference image from a genuine
                // cartridge-rendered Corneria frame. This hidden preroll stops
                // at the first stable gameplay chatter frame, so opening the
                // editor never exposes or continues the scramble sequence.
                game.set_god_mode(true);
                std::optional<std::uint32_t> first_meter_tick;
                std::uint32_t previous_dialogue_address{};
                std::uint8_t dialogue_count{};
                constexpr std::uint32_t maximum_preview_ticks = 2'400U;
                constexpr std::uint32_t meter_fallback_ticks = 720U;
                for (std::uint32_t tick = 0U;
                     tick < maximum_preview_ticks; ++tick) {
                    static_cast<void>(game.tick({}));
                    const auto meters = game.meter_state();
                    if (meters.enabled && !first_meter_tick) {
                        first_meter_tick = tick;
                    }
                    const auto dialogue = game.dialogue_state();
                    if (meters.enabled && dialogue.active
                        && dialogue.text_visible
                        && dialogue.text_address
                            != previous_dialogue_address) {
                        previous_dialogue_address = dialogue.text_address;
                        if (++dialogue_count >= 4U) {
                            // Let the formation finish crossing the viewport
                            // while retaining the same fourth chatter card.
                            // This is the clean, unobstructed static frame used
                            // by the original editor artwork.
                            constexpr std::uint8_t settle_ticks = 12U;
                            for (std::uint8_t settle = 0U;
                                 settle < settle_ticks; ++settle) {
                                static_cast<void>(game.tick({}));
                            }
                            break;
                        }
                    }
                    if (first_meter_tick
                        && tick - *first_meter_tick >= meter_fallback_ticks) {
                        break;
                    }
                }
            }
        }
        const auto persist_pregame_changes =
            std::getenv("STARFOX_TEST_PRESSES") == nullptr
            && std::getenv("STARFOX_TEST_DISPLAY_MODE") == nullptr;
        const auto save_pregame_settings = [&] {
            saved_pregame = capture_pregame_settings();
            if (persist_pregame_changes) {
                static_cast<void>(starfox::app::save_pregame_settings(
                    saved_pregame_path, saved_pregame));
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
                if ((address & 0xffffU) >= 0x8000U
                    && ((address >> 16U) & 0xffU) < 0x70U) {
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
        const auto native_model_z_address = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? mario_symbol("M_BIGZ") : 0U;
        const auto depth_colours_address = mario_symbol("M_DEPTHSTAB");
        const auto depth_thresholds_address = mario_symbol("M_DEPTHTABLE");
        const auto depth_table_addresses = symbols.find("DEPTHTABLES");
        if (depth_table_addresses.empty()) {
            throw std::runtime_error{"missing depth-table ROM symbol"};
        }
        const auto depth_table_address = depth_table_addresses.front();
        const auto hud_colour_address = mario_symbol("M_HUDCOLOUR");
        const auto hud_flags_address = mario_symbol("M_HUDFLAGS");
        const auto wire_mode_address = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? mario_symbol("M_WIREMODE") : 0U;
        const auto wobble_mode_address = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? mario_symbol("M_WOBBLEMODE") : 0U;
        const auto wave_mode_address = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? mario_symbol("M_WABBLEMODE") : 0U;
        const auto cel_mode_address = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? mario_symbol("M_CELMODE") : 0U;
        const auto wave_offset_address = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? mario_symbol("M_SINEOFFSET") : 0U;
        const auto grid_lines_address = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? mario_symbol("M_GRIDLINES") : 0U;
        const auto colour_warp_address = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? mario_symbol("M_COLORWARP") : 0U;
        const auto projected_points_address = active_experience
                == starfox::simulation::Experience::starfox_ex
            ? mario_symbol("M_PROJPNTS") : 0U;
        const auto ex_title_intro_background = [&symbols]() {
            const auto& title_intro = symbols.find("BG_TITLEI");
            const auto& background_lists = symbols.find("BGLISTS");
            if (title_intro.empty() || background_lists.empty()
                || (title_intro.front() & 0xff0000U)
                    != (background_lists.front() & 0xff0000U)) {
                return std::uint16_t{};
            }
            return static_cast<std::uint16_t>(
                title_intro.front() - background_lists.front());
        }();
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
            const auto find_text = [&symbols](const char* candidate) {
                for (const auto address : symbols.find(candidate)) {
                    if ((address & 0xffffU) >= 0x8000U
                        && ((address >> 16U) & 0xffU) < 0x70U) return address;
                }
                return std::uint32_t{};
            };
            if (const auto address = find_text(name); address != 0U) return address;
            const auto requested = std::string_view{name};
            const auto* alias = requested == "PEPPYTXT" ? "BUNNYTXT"
                : requested == "FALCOTXT" ? "COCKTXT"
                : requested == "SLIPPYTXT" ? "FROGTXT" : nullptr;
            if (alias != nullptr) {
                if (const auto address = find_text(alias); address != 0U) return address;
            }
            throw std::runtime_error{
                std::string{"missing game text symbol: "} + name};
        };
        const auto score_text = game_text_symbol("SCORETXT");
        const auto total_score_text = game_text_symbol("TOTALSCORETXT");
        const auto team_text = game_text_symbol("TEAMTXT");
        const auto teammate_down_text = game_text_symbol("DEADTXT");
        const std::array teammate_text{
            game_text_symbol("PEPPYTXT"),
            game_text_symbol("FALCOTXT"),
            game_text_symbol("SLIPPYTXT"),
        };

        AudioOutput audio{msu1_pack};
        audio.set_volumes(game.music_volume(), game.sfx_volume());
        audio.set_msu1_enabled(game.msu1_music()
            && active_experience
                == starfox::simulation::Experience::original);
        // DO_BGM_INIT is part of the cartridge's boot sequence and completes
        // before a level is selected. A command-line level starts its stage
        // bank on the very first logic tick; previously both complete IPL
        // transfers were handed to the SPC emulator in one 50 ms batch. That
        // restarted a driver which had never been allowed to initialize its
        // base sound0 workspace, leaving effects backed by incomplete state.
        // Advance every captured upload separately, without queueing inaudible
        // preroll, so direct-map audio follows the same base-bank -> stage-bank
        // order and timing as the ordinary title/map flow. In particular, the
        // base driver gets one complete SPC frame before the level bank's $ff
        // restart snapshots and overlays its initialized ARAM workspace.
        const auto boot_audio_writes = game.map().take_apu_port_writes();
        if (!boot_audio_writes.empty()) {
            auto ports = audio.prime_upload_sequence(boot_audio_writes);
            std::string direct_entry_name = initial_map;
            std::ranges::transform(direct_entry_name, direct_entry_name.begin(),
                [](unsigned char character) {
                    return static_cast<char>(std::toupper(character));
                });
            const auto direct_level_entry = direct_entry_name != "BOOT";
            if (direct_level_entry) {
                // A normal launch leaves sound0 running throughout the title,
                // controls and planet flow before a level bank overlays its
                // initialized ARAM workspace. A CLI map skips that wall-clock
                // interval. Advance the base driver silently for the same
                // 1.5 seconds as the desktop's opening black preroll so the
                // first stage upload never lands on half-initialized state.
                constexpr std::size_t direct_entry_settle_ticks = 30U;
                for (std::size_t tick = 0U;
                     tick < direct_entry_settle_ticks; ++tick) {
                    ports = audio.queue_logic_tick({}, {}, false, false);
                }
            }
            game.synchronize_apu_output_ports(ports);
        }
        auto gamepads = starfox::app::open_player_gamepads();
        SDL_Gamepad* gamepad = gamepads.empty() ? nullptr : gamepads.front();
        RumbleOutput rumble{symbols};
        MsuFadeOutput msu_fade{symbols};
        const auto close_gamepads = [&] {
            rumble.stop(gamepad);
            for (auto* opened : gamepads) {
                if (opened != nullptr) SDL_CloseGamepad(opened);
            }
            gamepads.clear();
            gamepad = nullptr;
        };
        const auto refresh_gamepads = [&] {
            close_gamepads();
            gamepads = starfox::app::open_player_gamepads();
            gamepad = gamepads.empty() ? nullptr : gamepads.front();
        };
        starfox::app::InputBindings bindings;
        bindings.load();
        const auto hud_layout_path = starfox::app::hud_layout_settings_path();
        starfox::render::HudLayoutProfiles hud_layouts{};
        static_cast<void>(starfox::app::load_hud_layout(
            hud_layout_path, hud_layouts));
        starfox::input::InputLatch input;
        std::array<starfox::input::InputLatch, 4> secondary_inputs{};
        starfox::input::InputLatch remap_input;
        RemapMenuState remap_menu;
        HudEditorState hud_editor;
        hud_editor.active = hud_editor_preview;
        bool running = true;
        bool exit_confirmation =
            std::getenv("STARFOX_TEST_EXIT_CONFIRMATION") != nullptr;
        bool exit_yes_selected{};
        const auto save_hud_layout = [&] {
            static_cast<void>(starfox::app::save_hud_layout(
                hud_layout_path, hud_layouts));
        };
        const auto close_hud_editor = [&] {
            save_hud_layout();
            hud_editor.active = false;
            hud_editor.dragging.reset();
            if (hud_editor_preview) {
                initial_map = "BOOT";
                restart_runtime = true;
                running = false;
            }
        };
        PresentationPacer pacer;
        starfox::timing::RasterPhaseClock raster_clock;
        starfox::timing::RasterPhaseClock frame_step_clock;
        starfox::timing::FixedStepClock realtime_raster_clock{
            starfox::timing::kPresentationHz};
        // Standard presentation keeps the complete 256x224 PPU raster.
        // Widescreen grows the scene symmetrically to 400x224 while HUD and
        // dialogue retain their original 224x192 coordinates in a centred
        // inset layer.
        starfox::render::Framebuffer framebuffer{snes_width, snes_height};
        starfox::render::Framebuffer superfx_frame{
            snes_width, superfx_height};
        starfox::render::SurfaceBuffer superfx_surfaces{
            snes_width, superfx_height};
        starfox::render::Framebuffer superfx_ui{
            superfx_ui_width, superfx_height};
        starfox::render::Framebuffer comms_hud{
            superfx_ui_width, superfx_height};
        starfox::render::Framebuffer superfx_hud{
            snes_width, superfx_height};
        starfox::render::Framebuffer controls_player_layer{
            snes_width, superfx_height};
        starfox::render::Framebuffer native_ex_overlay{
            snes_width, snes_height};
        starfox::render::Framebuffer planet_overlay{snes_width, snes_height};
        starfox::render::Framebuffer planet_text_overlay{
            snes_width, snes_height};
        starfox::render::Framebuffer live_fps_overlay{64U, 12U};
        starfox::render::Framebuffer exit_confirmation_overlay{112U, 40U};
        starfox::render::Framebuffer mode2_background_cache{
            snes_width, snes_height};
        starfox::simulation::SnesPpuState mode2_background_ppu;
        std::array<std::uint16_t, 32U> mode2_background_vertical{};
        std::int32_t mode2_background_x{};
        std::int32_t mode2_background_y{};
        std::uint64_t mode2_background_source_frame{};
        std::uint64_t mode2_background_scene_revision{};
        std::uint16_t mode2_background_id{};
        bool mode2_background_valid{};
        std::uint64_t mode2_background_temporal_hits{};
        std::uint64_t mode2_background_exact_hits{};
        std::uint64_t mode2_background_misses{};
        starfox::render::Framebuffer cartridge_layer_cache{
            snes_width, snes_height};
        std::uint64_t cartridge_layer_scene_revision{};
        std::uint16_t cartridge_layer_background_id{};
        std::uint8_t cartridge_layer_background_mode{};
        std::uint8_t cartridge_layer_flow_state{};
        bool cartridge_layer_valid{};
        std::uint64_t cartridge_layer_temporal_hits{};
        std::uint64_t cartridge_layer_misses{};
        std::array<std::uint64_t, 8U> profiled_background_modes{};
        std::uint64_t profiled_gameplay_hud_frames{};
        starfox::render::RenderSettings render_settings;
        render_settings.colour_index_base = 7U * 16U;
        const starfox::render::SoftwareRenderer renderer{render_settings};
        const starfox::render::ParticleRenderer particle_renderer;
        const starfox::render::ScaledTextRenderer text_renderer{rom, symbols};
        const starfox::render::BackgroundRenderer background_renderer;
        const starfox::render::DustRenderer dust_renderer{rom, symbols};
        const starfox::render::SpriteRenderer sprite_renderer;
        struct ObjectPresentationSnapshot {
            starfox::timing::TransformSnapshot transform;
            starfox::simulation::MatrixQ15 rotation_matrix{};
            std::uint16_t shape{};
            std::uint32_t strategy_address{};
            std::uint8_t type{};
        };
        using SnapshotMap = std::unordered_map<starfox::simulation::ObjectHandle,
            ObjectPresentationSnapshot>;
        const auto capture = [&game, &trigonometry]() {
            SnapshotMap result;
            for (const auto handle : game.objects().active_handles()) {
                const auto& object = game.objects().at(handle);
                const auto transform = starfox::timing::TransformSnapshot{
                    object.world_x, object.world_y, object.world_z,
                    static_cast<std::uint16_t>(
                        static_cast<std::uint16_t>(object.rotation_x) << 8U),
                    static_cast<std::uint16_t>(
                        static_cast<std::uint16_t>(object.rotation_y) << 8U),
                    static_cast<std::uint16_t>(
                        static_cast<std::uint16_t>(object.rotation_z) << 8U)};
                const auto rotation_matrix = starfox::simulation::transpose_q15(
                    starfox::simulation::rotation_matrix_q15(
                        trigonometry,
                        starfox::simulation::wrap16(-static_cast<std::int32_t>(
                            transform.pitch)),
                        starfox::simulation::wrap16(-static_cast<std::int32_t>(
                            transform.yaw)),
                        starfox::simulation::wrap16(-static_cast<std::int32_t>(
                            transform.roll))));
                result.emplace(handle, ObjectPresentationSnapshot{
                    transform, rotation_matrix, object.shape,
                    object.strategy_address, object.type});
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
            std::uint16_t background{};
            std::uint8_t background_mode{};
            std::uint8_t main_screen{};
            bool bg1_tile_size_16{};
            bool bg2_tile_size_16{};
            bool bg3_tile_size_16{};
            bool bg2_vertical_offsets_enabled{};
            bool bg2_horizontal_offsets_enabled{};
            std::uint16_t bg1_character_base{};
            std::uint16_t bg1_screen_base{};
            std::uint16_t bg2_character_base{};
            std::uint16_t bg2_screen_base{};
            std::uint16_t bg3_character_base{};
            std::uint16_t bg3_screen_base{};
            std::int16_t background_x{};
            std::int16_t background_y{};
            std::int16_t bg2_scroll_x{};
            std::int16_t bg2_scroll_y{};
            std::int16_t bg1_scroll_x{};
            std::int16_t bg1_scroll_y{};
            std::int16_t bg3_scroll_x{};
            std::int16_t bg3_scroll_y{};
            std::array<std::int16_t, 224> bg2_horizontal_offsets{};
            std::array<std::uint16_t, 32> bg2_vertical_offsets{};
        };
        const auto capture_raster_motion = [&game, background_x_address,
                                             background_y_address]() {
            const auto& ppu = game.map().ppu_state();
            auto snapshot = RasterMotionSnapshot{
                game.map().background(),
                ppu.background_mode,
                ppu.main_screen,
                ppu.bg1_tile_size_16,
                ppu.bg2_tile_size_16,
                ppu.bg3_tile_size_16,
                ppu.bg2_vertical_offsets_enabled,
                ppu.bg2_horizontal_offsets_enabled,
                ppu.bg1_character_base,
                ppu.bg1_screen_base,
                ppu.bg2_character_base,
                ppu.bg2_screen_base,
                ppu.bg3_character_base,
                ppu.bg3_screen_base,
                static_cast<std::int16_t>(
                    game.map().read_native_word(background_x_address)),
                static_cast<std::int16_t>(
                    game.map().read_native_word(background_y_address)),
                ppu.bg2_scroll_x,
                ppu.bg2_scroll_y,
                ppu.bg1_scroll_x,
                ppu.bg1_scroll_y,
                ppu.bg3_scroll_x,
                ppu.bg3_scroll_y,
                ppu.bg2_horizontal_offsets,
            };
            for (std::size_t index = 0;
                 index < snapshot.bg2_vertical_offsets.size(); ++index) {
                const auto byte = (0x2fa0U + index) * 2U;
                snapshot.bg2_vertical_offsets[index] =
                    static_cast<std::uint16_t>(ppu.vram[byte])
                    | (static_cast<std::uint16_t>(ppu.vram[byte + 1U]) << 8U);
            }
            return snapshot;
        };
        const auto raster_source_changed = [](
            const RasterMotionSnapshot& previous,
            const RasterMotionSnapshot& current) {
            return previous.background != current.background
                || previous.background_mode != current.background_mode
                || previous.main_screen != current.main_screen
                || previous.bg1_tile_size_16 != current.bg1_tile_size_16
                || previous.bg2_tile_size_16 != current.bg2_tile_size_16
                || previous.bg3_tile_size_16 != current.bg3_tile_size_16
                || previous.bg2_vertical_offsets_enabled
                    != current.bg2_vertical_offsets_enabled
                || previous.bg2_horizontal_offsets_enabled
                    != current.bg2_horizontal_offsets_enabled
                || previous.bg1_character_base != current.bg1_character_base
                || previous.bg1_screen_base != current.bg1_screen_base
                || previous.bg2_character_base != current.bg2_character_base
                || previous.bg2_screen_base != current.bg2_screen_base
                || previous.bg3_character_base != current.bg3_character_base
                || previous.bg3_screen_base != current.bg3_screen_base;
        };
        auto previous_raster_motion = capture_raster_motion();
        auto current_raster_motion = previous_raster_motion;
        auto previous_oam = game.map().ppu_state().oam;
        auto current_oam = previous_oam;
        auto previous_circle = game.circle_effect_state();
        auto current_circle = previous_circle;
        std::unordered_map<std::uint32_t, starfox::assets::Shape> shape_cache;
        std::unordered_set<std::uint32_t> invalid_shapes;
        std::uint64_t presented_frames = 0;
        std::uint64_t source_logic_frames = 0;
        std::uint64_t profile_background_ns{};
        std::uint64_t profile_world_ns{};
        std::uint64_t profile_composite_ns{};
        std::uint64_t profile_present_ns{};
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
        std::vector<starfox::simulation::MsuRegisterWrite> pending_msu_writes;
        std::uint8_t audio_video_phases{};
        MouseCameraState mouse_camera;
        ExMouseInputLatch ex_mouse_input;
        TouchControls touch_controls;
        std::uint32_t launch_wipe_reveal_frames{};
        bool window_focused = true;
        bool frame_frozen{};
        FrameStepRepeater frame_step_repeater;
        std::optional<bool> volume_slider_drag_music;
        bool suppress_fullscreen_start{};
        double last_phase_fraction{};

        // Open and synchronize the native window before the cartridge flow
        // begins. This leaves a stable one-and-a-half-second black preroll instead of
        // allowing ROM loading or the first APU upload to race the desktop
        // compositor and become audible/visible before the window appears.
        if (first_runtime
            && std::getenv("STARFOX_TEST_SKIP_PREROLL") == nullptr) {
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
            first_runtime = false;
        }
        // The HUD editor is a frozen visual workspace. Its hidden cartridge
        // preroll must not leak stage music or effects into the options menu.
        if (running && !hud_editor_preview) audio.start();
        auto raster_timestamp = std::chrono::steady_clock::now();
        starfox::timing::LiveFpsCounter live_fps{
            std::chrono::milliseconds{250}};
        live_fps.reset(raster_timestamp, game.presentation_fps());
        while (running) {
            window.update_temporary_status();
            bool toggle_frame_freeze{};
            bool step_frame_forward{};
            bool step_frame_backward{};
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                const auto reset_to_setup_key =
                    event.type == SDL_EVENT_KEY_DOWN
                    && !event.key.repeat
                    && event.key.scancode == SDL_SCANCODE_R
                    && (event.key.mod & SDL_KMOD_CTRL) != 0U;
                if (reset_to_setup_key) {
                    // Reconstruct the runtime at BOOT so this works from any
                    // cartridge or host-owned screen, including paused play,
                    // EX native menus and the HUD editor.
                    initial_map = "BOOT";
                    restart_runtime = true;
                    running = false;
                    break;
                }
                const auto fullscreen_key =
                    event.type == SDL_EVENT_KEY_DOWN
                    && !event.key.repeat
                    && (event.key.scancode == SDL_SCANCODE_RETURN
                        || event.key.scancode == SDL_SCANCODE_KP_ENTER)
                    && (event.key.mod & SDL_KMOD_ALT) != 0U;
                if (fullscreen_key) {
                    window.toggle_fullscreen();
                    suppress_fullscreen_start = true;
                } else if (event.type == SDL_EVENT_KEY_UP
                           && (event.key.scancode == SDL_SCANCODE_RETURN
                               || event.key.scancode
                                   == SDL_SCANCODE_KP_ENTER)) {
                    suppress_fullscreen_start = false;
                }
                const auto frame_debug_key = event.type == SDL_EVENT_KEY_DOWN
                    && (event.key.scancode == SDL_SCANCODE_F5
                        || event.key.scancode == SDL_SCANCODE_F6
                        || event.key.scancode == SDL_SCANCODE_F7);
                const auto frame_debug_key_down =
                    frame_debug_key && !event.key.repeat;
                if (frame_debug_key_down
                    && event.key.scancode == SDL_SCANCODE_F5) {
                    toggle_frame_freeze = true;
                } else if (frame_debug_key_down
                           && event.key.scancode == SDL_SCANCODE_F6) {
                    frame_step_repeater.press(
                        FrameStepRepeater::Direction::forward,
                        FrameStepRepeater::clock::now());
                    step_frame_forward = true;
                } else if (frame_debug_key_down
                           && event.key.scancode == SDL_SCANCODE_F7) {
                    frame_step_repeater.press(
                        FrameStepRepeater::Direction::backward,
                        FrameStepRepeater::clock::now());
                    step_frame_backward = true;
                } else if (event.type == SDL_EVENT_KEY_UP
                           && event.key.scancode == SDL_SCANCODE_F6) {
                    frame_step_repeater.release(
                        FrameStepRepeater::Direction::forward);
                } else if (event.type == SDL_EVENT_KEY_UP
                           && event.key.scancode == SDL_SCANCODE_F7) {
                    frame_step_repeater.release(
                        FrameStepRepeater::Direction::backward);
                }
                const auto toggle_rewind_key =
                    event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                    && event.key.scancode == SDL_SCANCODE_F12
                    && game.flow_state()
                        == starfox::simulation::GameFlowState::pregame_menu
                    && game.pregame_page()
                        == starfox::simulation::PregamePage::main;
                if (toggle_rewind_key) {
                    if (presentation_history) presentation_history.reset();
                    else presentation_history.emplace();
                    window.show_temporary_status(presentation_history
                        ? "REWIND ENABLED" : "REWIND DISABLED");
                }
                const auto exit_confirmation_key =
                    event.type == SDL_EVENT_KEY_DOWN
                    && !event.key.repeat
                    && event.key.scancode == SDL_SCANCODE_ESCAPE;
                if (exit_confirmation_key && !hud_editor.active
                    && !remap_menu.active) {
                    if (exit_confirmation) {
                        exit_confirmation = false;
                    } else {
                        exit_confirmation = true;
                        // Default to the non-destructive choice. Keyboard and
                        // controller navigation can move to YES explicitly.
                        exit_yes_selected = false;
                        mouse_camera.active = false;
                        window.set_relative_mouse_mode(false);
                    }
                }
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                    window_focused = true;
                } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                    window_focused = false;
                    ex_mouse_input.release();
                    touch_controls.reset();
                    frame_step_repeater.reset();
                    if (volume_slider_drag_music) save_pregame_settings();
                    volume_slider_drag_music.reset();
                } else if (event.type == SDL_EVENT_GAMEPAD_ADDED
                           || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
                    refresh_gamepads();
                    for (std::size_t player = 0;
                         player < secondary_inputs.size(); ++player) {
                        const auto held = player + 1U < gamepads.size()
                            ? bindings.sample_gamepad_only(gamepads[player + 1U])
                            : starfox::input::ButtonMask{};
                        secondary_inputs[player].reset(held);
                    }
                }
                if (event.type == SDL_EVENT_FINGER_DOWN
                    || event.type == SDL_EVENT_FINGER_MOTION) {
                    touch_controls.update(event.tfinger.fingerID,
                        event.tfinger.x, event.tfinger.y);
                } else if (event.type == SDL_EVENT_FINGER_UP
                           || event.type == SDL_EVENT_FINGER_CANCELED) {
                    touch_controls.release(event.tfinger.fingerID);
                }
                if (!hud_editor.active && !remap_menu.active
                    && game.flow_state()
                        == starfox::simulation::GameFlowState::pregame_menu
                    && game.pregame_page()
                        == starfox::simulation::PregamePage::options) {
                    constexpr float slider_left = 147.0F;
                    constexpr float slider_right = 235.0F;
                    constexpr float music_top = 118.0F;
                    constexpr float sfx_top = 144.0F;
                    constexpr float slider_bottom_offset = 10.0F;
                    const auto update_volume_slider = [&](float window_x,
                                                          float window_y,
                                                          bool begin_drag) {
                        float logical_x{};
                        float logical_y{};
                        if (!window.window_to_logical(window_x, window_y,
                                logical_x, logical_y)) return;
                        const auto viewport = static_cast<float>((
                            display_width_for(game.display_mode())
                            - snes_width) / 2U);
                        const auto local_x = logical_x - viewport;
                        if (begin_drag) {
                            if (logical_y >= music_top
                                && logical_y <= music_top
                                    + slider_bottom_offset) {
                                volume_slider_drag_music = true;
                            } else if (logical_y >= sfx_top
                                       && logical_y <= sfx_top
                                           + slider_bottom_offset) {
                                volume_slider_drag_music = false;
                            } else {
                                return;
                            }
                        }
                        if (!volume_slider_drag_music) return;
                        const auto percent = static_cast<std::uint8_t>(
                            std::clamp(std::lround((local_x - slider_left)
                                * 100.0F / (slider_right - slider_left)),
                                0L, 100L));
                        if (*volume_slider_drag_music) {
                            game.set_music_volume(percent);
                        } else {
                            game.set_sfx_volume(percent);
                        }
                        audio.set_volumes(
                            game.music_volume(), game.sfx_volume());
                    };
                    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                        && event.button.button == SDL_BUTTON_LEFT) {
                        update_volume_slider(
                            event.button.x, event.button.y, true);
                    } else if (event.type == SDL_EVENT_MOUSE_MOTION
                               && volume_slider_drag_music) {
                        update_volume_slider(
                            event.motion.x, event.motion.y, false);
                    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP
                               && event.button.button == SDL_BUTTON_LEFT
                               && volume_slider_drag_music) {
                        update_volume_slider(
                            event.button.x, event.button.y, false);
                        save_pregame_settings();
                        volume_slider_drag_music.reset();
                    }
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP
                           && event.button.button == SDL_BUTTON_LEFT) {
                    volume_slider_drag_music.reset();
                }
                if (hud_editor.active) {
                    const auto editor_width = display_width_for(
                        game.display_mode());
                    auto& editor_layout = hud_layouts[
                        hud_profile_index(
                            game.display_mode(), game.experience())];
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
                                element, editor_width, game.experience());
                            auto& offset = editor_layout[element];
                            offset.x = static_cast<std::int16_t>(std::lround(
                                hud_editor.pointer_x - hud_editor.grab_x
                                - static_cast<float>(base.x)));
                            offset.y = static_cast<std::int16_t>(std::lround(
                                hud_editor.pointer_y - hud_editor.grab_y
                                - static_cast<float>(base.y)));
                            clamp_hud_element(
                                editor_layout, element, editor_width,
                                game.experience());
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
                                    element, editor_width, editor_layout,
                                    game.experience());
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
                                    *picked, editor_width, editor_layout,
                                    game.experience());
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
                const auto ex_mouse_owns_event =
                    game.ex_pointing_control_enabled()
                    && !remap_menu.active && !hud_editor.active;
                if (event.type == SDL_EVENT_MOUSE_MOTION
                    && ex_mouse_owns_event) {
                    ex_mouse_input.add_motion(
                        event.motion.xrel, event.motion.yrel);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                           && ex_mouse_owns_event
                           && event.button.button == SDL_BUTTON_LEFT) {
                    ex_mouse_input.set_button(0x01U, true);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                           && ex_mouse_owns_event
                           && event.button.button == SDL_BUTTON_RIGHT) {
                    ex_mouse_input.set_button(0x02U, true);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                           && ex_mouse_owns_event
                           && event.button.button == SDL_BUTTON_MIDDLE) {
                    ex_mouse_input.set_button(0x04U, true);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                           && ex_mouse_owns_event
                           && (event.button.button == SDL_BUTTON_X1
                               || event.button.button == SDL_BUTTON_X2)) {
                    ex_mouse_input.set_button(0x08U, true);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP
                           && event.button.button == SDL_BUTTON_LEFT) {
                    ex_mouse_input.set_button(0x01U, false);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP
                           && event.button.button == SDL_BUTTON_RIGHT) {
                    ex_mouse_input.set_button(0x02U, false);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP
                           && event.button.button == SDL_BUTTON_MIDDLE) {
                    ex_mouse_input.set_button(0x04U, false);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP
                           && (event.button.button == SDL_BUTTON_X1
                               || event.button.button == SDL_BUTTON_X2)) {
                    ex_mouse_input.set_button(0x08U, false);
                }
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                    && event.button.button == SDL_BUTTON_RIGHT
                    && mouse_camera_scene && !remap_menu.active
                    && !hud_editor.active && !ex_mouse_owns_event) {
                    mouse_camera.active = true;
                    window.set_relative_mouse_mode(true);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP
                           && event.button.button == SDL_BUTTON_RIGHT
                           && !ex_mouse_owns_event) {
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
                if (frame_debug_key || fullscreen_key || !remap_menu.active
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
            window.set_render_options(game.renderer_mode(),
                game.anti_aliasing_mode(),
                game.enhanced_graphics(), game.smooth_polys(),
                game.rtx_lighting(), game.vsync());
            if (toggle_frame_freeze) {
                frame_frozen = !frame_frozen;
                input.reset();
                for (std::size_t player = 0;
                     player < secondary_inputs.size(); ++player) {
                    const auto held = player + 1U < gamepads.size()
                        ? bindings.sample_gamepad_only(gamepads[player + 1U])
                        : starfox::input::ButtonMask{};
                    secondary_inputs[player].reset(held);
                }
                remap_input.reset(
                    bindings.sample_fixed_menu_navigation(gamepad));
                if (presentation_history) presentation_history->to_live();
                if (frame_frozen) {
                    frame_step_clock.synchronize(last_phase_fraction);
                    audio.set_paused(true);
                    mouse_camera.active = false;
                    window.set_relative_mouse_mode(false);
                    window.set_frame_debug_status(true,
                        presentation_history
                            ? presentation_history->cursor() : 0U,
                        presentation_history
                            ? presentation_history->frame_count() : 0U);
                } else {
                    if (const auto* live = presentation_history
                            ? presentation_history->current() : nullptr) {
                        window.present_rgba(
                            live->width, live->height, live->rgba);
                    }
                    audio.set_paused(false);
                    window.set_frame_debug_status(false);
                    realtime_raster_clock.reset();
                    raster_timestamp = std::chrono::steady_clock::now();
                    live_fps.reset(raster_timestamp, game.presentation_fps());
                    frame_step_repeater.reset();
                }
            }

            if (frame_frozen) {
                if (const auto repeated = frame_step_repeater.poll(
                        FrameStepRepeater::clock::now())) {
                    step_frame_forward = *repeated
                        == FrameStepRepeater::Direction::forward;
                    step_frame_backward = *repeated
                        == FrameStepRepeater::Direction::backward;
                }
            }

            if (frame_frozen && step_frame_backward) {
                if (presentation_history
                    && presentation_history->step_back()) {
                    const auto* frame = presentation_history->current();
                    window.present_rgba(frame->width, frame->height, frame->rgba);
                    window.set_frame_debug_status(true,
                        presentation_history->cursor(),
                        presentation_history->frame_count());
                }
                continue;
            }
            if (frame_frozen && step_frame_forward
                && presentation_history
                && !presentation_history->at_live()) {
                static_cast<void>(presentation_history->step_forward());
                const auto* frame = presentation_history->current();
                window.present_rgba(frame->width, frame->height, frame->rgba);
                window.set_frame_debug_status(true,
                    presentation_history->cursor(),
                    presentation_history->frame_count());
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
            const auto ex_mouse_capture = window_focused && !frame_frozen
                && game.ex_pointing_control_enabled()
                && !remap_menu.active && !hud_editor.active
                && !exit_confirmation;
            if (mouse_camera.active
                && (!mouse_camera_scene || hud_editor.active
                    || ex_mouse_capture)) {
                mouse_camera.active = false;
            }
            if (!ex_mouse_capture && (remap_menu.active || hud_editor.active
                    || !game.ex_pointing_control_enabled())) {
                ex_mouse_input.release();
            }
            window.set_relative_mouse_mode(
                ex_mouse_capture || mouse_camera.active);

            if (!test_unpaced && !advance_frozen_frame) {
                pacer.wait_for_next_frame(game.presentation_fps());
            }
            const auto* keyboard_state = SDL_GetKeyboardState(nullptr);
            if (suppress_fullscreen_start
                && !keyboard_state[SDL_SCANCODE_RETURN]
                && !keyboard_state[SDL_SCANCODE_KP_ENTER]) {
                suppress_fullscreen_start = false;
            }
            auto sampled_buttons = bindings.sample(gamepad);
            sampled_buttons = static_cast<ButtonMask>(
                sampled_buttons | touch_controls.buttons());
            for (const auto& press : scripted_presses) {
                if (presented_frames >= press.presentation_frame
                    && presented_frames < press.presentation_frame + 3U) {
                    sampled_buttons = static_cast<ButtonMask>(
                        sampled_buttons | press.buttons);
                }
            }
            if (suppress_fullscreen_start) {
                sampled_buttons = static_cast<ButtonMask>(
                    sampled_buttons & ~starfox::input::start);
            }
            input.sample(sampled_buttons);
            for (std::size_t player = 0;
                 player < secondary_inputs.size(); ++player) {
                secondary_inputs[player].sample(
                    player + 1U < gamepads.size()
                        ? bindings.sample_gamepad_only(gamepads[player + 1U])
                        : starfox::input::ButtonMask{});
            }
            remap_input.sample(
                bindings.sample_fixed_menu_navigation(gamepad));
            const auto fast_forward = !advance_frozen_frame
                && (test_fast_forward || (keyboard_state[SDL_SCANCODE_TAB]
                    && !(remap_menu.active && remap_menu.waiting_for_input)));
            // Presentation FPS is independent of the cartridge's 60 Hz
            // raster. Low output rates may service multiple raster phases
            // before one draw; high rates expose fractional progress between
            // phases for smooth interpolation without accelerating gameplay.
            const auto raster_batch = [&]() {
                if (advance_frozen_frame) {
                    return frame_step_clock.advance(
                        starfox::timing::frame_debug_presentation_hz(
                            game.presentation_fps()));
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
                if (hud_editor.active) {
                    const auto editor_controls = remap_input.consume();
                    if ((editor_controls.pressed
                         & starfox::input::y) != 0U) {
                        hud_layouts[hud_profile_index(
                            game.display_mode(), game.experience())] = {};
                        save_hud_layout();
                    }
                    if ((editor_controls.pressed
                         & (starfox::input::b | starfox::input::start)) != 0U) {
                        close_hud_editor();
                    }
                    // Do not advance video phases, strategies, interpolation,
                    // particles, dialogue, or audio while editing. Mouse and
                    // controller editor input remains live around this frozen
                    // cartridge snapshot.
                    continue;
                }
                if (exit_confirmation) {
                    const auto controls = input.consume();
                    if ((controls.pressed & (starfox::input::left
                            | starfox::input::right | starfox::input::up
                            | starfox::input::down)) != 0U) {
                        exit_yes_selected = !exit_yes_selected;
                    }
                    if ((controls.pressed & starfox::input::b) != 0U) {
                        exit_confirmation = false;
                    } else if ((controls.pressed & (starfox::input::a
                                   | starfox::input::start)) != 0U) {
                        if (exit_yes_selected) {
                            running = false;
                        } else {
                            exit_confirmation = false;
                        }
                    }
                    // Freeze source video, simulation and input underneath
                    // the host confirmation card.
                    continue;
                }
                game.present_frame();
                rumble.advance(game.map(), gamepad,
                    game.rumble()
                    && active_experience
                        == starfox::simulation::Experience::original);
                if (game.msu1_music()
                    && active_experience
                        == starfox::simulation::Experience::original) {
                    if (const auto fade = msu_fade.advance(game.map())) {
                        pending_msu_writes.push_back(*fade);
                    }
                }
                if (game.logic_tick_ready()) {
                    auto controls = input.consume();
                    std::array<starfox::input::TickInput, 4>
                        secondary_controls{};
                    for (std::size_t player = 0;
                         player < secondary_controls.size(); ++player) {
                        secondary_controls[player] =
                            secondary_inputs[player].consume();
                    }
                    const auto remap_controls = remap_input.consume();
                    if (remap_menu.active) {
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
                        secondary_controls = {};
                    } else if (game.flow_state()
                                   == starfox::simulation::GameFlowState::pregame_menu
                               && game.pregame_page()
                                   == starfox::simulation::PregamePage::main
                               && game.pregame_selection() == 12U
                               && (controls.pressed
                                   & (starfox::input::a | starfox::input::b))
                                   != 0U) {
                        remap_menu.active = true;
                        remap_menu.waiting_for_input = false;
                        remap_input.reset(
                            bindings.sample_fixed_menu_navigation(gamepad));
                        controls = {};
                        secondary_controls = {};
                    } else if (game.flow_state()
                                   == starfox::simulation::GameFlowState::pregame_menu
                               && game.pregame_page()
                                   == starfox::simulation::PregamePage::options
                               && game.pregame_selection() == 3U
                               && (controls.pressed
                                   & (starfox::input::a
                                      | starfox::input::select)) != 0U) {
                        auto& editor_layout = hud_layouts[
                            hud_profile_index(
                                game.display_mode(), game.experience())];
                        clamp_hud_layout(editor_layout,
                            display_width_for(game.display_mode()),
                            game.experience());
                        launch_hud_editor_preview = true;
                        initial_map = "LEVEL1_1";
                        restart_runtime = true;
                        running = false;
                        controls = {};
                        secondary_controls = {};
                    }
                    previous = current;
                    previous_camera = current_camera;
                    previous_raster_motion = current_raster_motion;
                    previous_oam = current_oam;
                    previous_circle = current_circle;
                    const auto previous_scene = game.scene_revision();
                    const auto settings_before_tick = capture_pregame_settings();
                    game.set_secondary_inputs(secondary_controls);
                    game.set_mouse_input(ex_mouse_input.consume());
                    game.set_ntt_input(remap_menu.active || hud_editor.active
                            ? 0U
                            : sample_ntt_data_pad(keyboard_state));
                    const auto tick_result = game.tick(controls);
                    // Cartridge PAUSESND commands still run through the SPC
                    // streams so their pause/unpause effects are audible.
                    // Companion MSU playback is host-decoded, so freeze only
                    // that music cursor at the same gameplay boundary.
                    audio.set_game_paused(game.paused());
                    audio.set_volumes(game.music_volume(), game.sfx_volume());
                    ++source_logic_frames;
                    // EX commits its option pages to $71:f000 inside RESTART.
                    // Mirror that battery-backed bank as soon as the source
                    // changes it so an ordinary window close cannot lose the
                    // just-confirmed cartridge settings.
                    synchronize_ex_save();
                    if (capture_pregame_settings() != settings_before_tick) {
                        save_pregame_settings();
                    }
                    if (game.experience() != active_experience) {
                        active_experience = game.experience();
                        save_pregame_settings();
                        restart_runtime = true;
                        running = false;
                        break;
                    }
                    current = capture();
                    current_camera = capture_camera();
                    current_raster_motion = capture_raster_motion();
                    current_oam = game.map().ppu_state().oam;
                    current_circle = game.circle_effect_state();
                    const auto camera_cut =
                        starfox::timing::camera_transform_is_discontinuous(
                            previous_camera, current_camera);
                    const auto raster_cut = raster_source_changed(
                        previous_raster_motion, current_raster_motion);
                    if (game.scene_revision() != previous_scene || camera_cut
                        || raster_cut) {
                        // LEVEL1_1 replaces the scramble camera with ExitBase's
                        // view in one source update. Interpolating that cut put
                        // the newly spawned docking station off-screen, then
                        // huge at the right edge, for two 60 FPS presentations.
                        // Snap the complete presentation state at any such view
                        // discontinuity, just as we already do for scene loads.
                        previous = current;
                        previous_camera = current_camera;
                        previous_raster_motion = current_raster_motion;
                        previous_oam = current_oam;
                        previous_circle = current_circle;
                    }
                    for (const auto& [handle, snapshot] : current) {
                        previous.try_emplace(handle, snapshot);
                    }
                    pending_audio_writes.insert(pending_audio_writes.end(),
                        tick_result.audio_port_writes.begin(),
                        tick_result.audio_port_writes.end());
                    auto msu_writes = game.map().take_msu_register_writes();
                    pending_msu_writes.insert(pending_msu_writes.end(),
                        msu_writes.begin(), msu_writes.end());
                    audio.set_msu1_enabled(game.msu1_music()
                        && active_experience
                            == starfox::simulation::Experience::original);
                }
                if (++audio_video_phases >= 3U) {
                    game.synchronize_apu_output_ports(
                        audio.queue_logic_tick(
                            pending_audio_writes, pending_msu_writes,
                            fast_forward,
                            !advance_frozen_frame));
                    pending_audio_writes.clear();
                    pending_msu_writes.clear();
                    audio_video_phases = 0U;
                }
            }
            if (restart_runtime) break;
            const auto profile_frame_start = std::chrono::steady_clock::now();
            const auto display_width = display_width_for(game.display_mode());
            auto active_hud_layout = hud_layouts[
                hud_profile_index(game.display_mode(), game.experience())];
            clamp_hud_layout(
                active_hud_layout, display_width, game.experience());
            const auto viewport_origin = static_cast<std::int32_t>(
                (display_width - snes_width) / 2U);
            const auto superfx_ui_offset_x = static_cast<std::int32_t>(
                (display_width - superfx_ui_width) / 2U);
            const auto extend_cartridge_scene = game.flow_state()
                    == starfox::simulation::GameFlowState::intro
                || game.flow_state()
                    == starfox::simulation::GameFlowState::ex_pregame_menu
                || game.flow_state()
                    == starfox::simulation::GameFlowState::gameplay
                || game.flow_state()
                    == starfox::simulation::GameFlowState::training
                || game.flow_state()
                    == starfox::simulation::GameFlowState::stage_results
                || game.flow_state()
                    == starfox::simulation::GameFlowState::credits;
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
            superfx_surfaces.resize(display_width, scene_height);
            superfx_hud.resize(display_width, superfx_height);
            controls_player_layer.resize(display_width, superfx_height);
            // EX's native BG1 diagnostics occupy the cartridge's 256-pixel
            // canvas. Keeping this staging layer as wide as 32:9 needlessly
            // cleared and composited hundreds of thousands of transparent
            // pixels on every high-refresh gameplay presentation.
            native_ex_overlay.resize(snes_width, snes_height);
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
            const auto ex_native_menu = game.flow_state()
                == starfox::simulation::GameFlowState::ex_pregame_menu;
            const auto background_x = interpolate_raster_word(
                ex_native_menu ? previous_raster_motion.bg2_scroll_x
                               : previous_raster_motion.background_x,
                ex_native_menu ? current_raster_motion.bg2_scroll_x
                               : current_raster_motion.background_x);
            const auto background_y = interpolate_raster_word(
                ex_native_menu ? previous_raster_motion.bg2_scroll_y
                               : previous_raster_motion.background_y,
                ex_native_menu ? current_raster_motion.bg2_scroll_y
                               : current_raster_motion.background_y);
            auto ppu = game.map().ppu_state();
            starfox::render::interpolate_crosshair_oam(
                previous_oam, interpolation_alpha, ppu);
            const auto ex_title_logo_screen = game.experience()
                    == starfox::simulation::Experience::starfox_ex
                && game.flow_state()
                    == starfox::simulation::GameFlowState::title
                && ex_title_intro_background != 0U
                && game.map().background() == ex_title_intro_background;
            const auto extend_ex_title_art = display_width > snes_width
                && game.experience()
                    == starfox::simulation::Experience::starfox_ex
                && game.flow_state()
                    == starfox::simulation::GameFlowState::title
                && !ex_title_logo_screen;
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
            for (std::size_t index = 0;
                 index < current_raster_motion.bg2_vertical_offsets.size();
                 ++index) {
                const auto previous_value =
                    previous_raster_motion.bg2_vertical_offsets[index];
                const auto current_value =
                    current_raster_motion.bg2_vertical_offsets[index];
                auto interpolated = current_value;
                // DOVOFS words contain a 13-bit wrapping scroll value and a
                // validity flag. Interpolate only while the same table entry
                // remains active, following the shortest wrapped distance.
                if (((previous_value ^ current_value) & 0x4000U) == 0U
                    && (current_value & 0x4000U) != 0U) {
                    const auto previous_scroll =
                        static_cast<std::int32_t>(previous_value & 0x1fffU);
                    const auto current_scroll =
                        static_cast<std::int32_t>(current_value & 0x1fffU);
                    auto difference = (current_scroll - previous_scroll)
                        & 0x1fff;
                    if (difference > 4'095) difference -= 8'192;
                    auto scroll = previous_scroll + static_cast<std::int32_t>(
                        std::lround(static_cast<double>(difference)
                            * interpolation_alpha));
                    scroll %= 8'192;
                    if (scroll < 0) scroll += 8'192;
                    interpolated = static_cast<std::uint16_t>(
                        (current_value & 0xe000U)
                        | static_cast<std::uint16_t>(scroll));
                }
                const auto byte = (0x2fa0U + index) * 2U;
                ppu.vram[byte] = static_cast<std::uint8_t>(interpolated);
                ppu.vram[byte + 1U] =
                    static_cast<std::uint8_t>(interpolated >> 8U);
            }
            auto circle = interpolate_circle_effect(
                previous_circle, current_circle, interpolation_alpha);
            circle.centre_x = static_cast<std::int16_t>(
                circle.centre_x + viewport_origin);
            auto planet_presentation = game.planet_presentation_state();
            const auto controls_scene = game.flow_state()
                    == starfox::simulation::GameFlowState::controls_type
                || game.flow_state()
                    == starfox::simulation::GameFlowState::controls_choice;
            framebuffer.clear(0U);
            superfx_frame.clear(0U);
            superfx_surfaces.clear();
            superfx_ui.clear(0U);
            superfx_hud.clear(0U);
            comms_hud.clear(0U);
            if (controls_scene) controls_player_layer.clear(0U);
            if (game.experience()
                    == starfox::simulation::Experience::starfox_ex
                && gameplay_hud) {
                native_ex_overlay.clear(0U);
            }
            if (planet_presentation.briefing_layers) {
                planet_overlay.clear(0U);
                planet_text_overlay.clear(0U);
            }
            // EX's 224-pixel Super FX bitmap is centred inside a 256-pixel
            // BG1 surface with 16-pixel black guard columns. Retain those at
            // source 4:3, but omit them when the surrounding BG2 is expanded.
            const auto native_menu_guard_inset =
                ex_native_menu && display_width > snes_width ? 16U : 0U;
            planet_presentation.isolate_left = static_cast<std::int16_t>(
                planet_presentation.isolate_left + viewport_origin);
            planet_presentation.isolate_right = static_cast<std::int16_t>(
                planet_presentation.isolate_right + viewport_origin);
            // Tile/sprite presentation is still vastly oversampled at the
            // 360/480 Hz output choices. Sample that cartridge layer at a
            // smooth 180/160 Hz while the interpolated Super FX world, HUD,
            // cursor, and window effects continue at the requested rate.
            const auto presentation_background_cadence =
                static_cast<std::uint16_t>(
                    game.presentation_fps() <= 240U
                        ? 1U
                        : (game.presentation_fps() + 179U) / 180U);
            const auto cache_complete_cartridge_layer =
                ppu.background_mode == 1U
                || (ppu.background_mode == 2U && !gameplay_hud);
            const auto reuse_complete_cartridge_layer =
                cache_complete_cartridge_layer
                && presentation_background_cadence > 1U
                && presented_frames % presentation_background_cadence != 0U
                && cartridge_layer_valid
                && cartridge_layer_cache.width() == framebuffer.width()
                && cartridge_layer_cache.height() == framebuffer.height()
                && cartridge_layer_scene_revision == game.scene_revision()
                && cartridge_layer_background_id == game.map().background()
                && cartridge_layer_background_mode == ppu.background_mode
                && cartridge_layer_flow_state == static_cast<std::uint8_t>(
                    game.flow_state());
            if (reuse_complete_cartridge_layer) {
                framebuffer.pixels() = cartridge_layer_cache.pixels();
                ++cartridge_layer_temporal_hits;
            } else if (ppu.background_mode == 1U) {
                ++profiled_background_modes[1U];
                const auto native_menu_bg1 = game.flow_state()
                    == starfox::simulation::GameFlowState::ex_pregame_menu;
                const auto extend_title_backdrop = display_width > snes_width
                    && game.flow_state()
                        == starfox::simulation::GameFlowState::title;
                // The title's low-priority BG3 cells are its sparse native
                // star/backdrop layer. Repeat only that pass through wide
                // margins so the moving Super FX ship never enters a solid
                // 4:3 side band. High BG3 (PRESS START) and BG2's logo/roster
                // remain centred and are restored in their source priority
                // order after the model pass below.
                background_renderer.draw_bg3(
                    ppu, framebuffer, starfox::render::TilePriorityPass::low,
                    viewport_origin,
                    extend_cartridge_scene || extend_title_backdrop);
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
                    viewport_origin,
                    extend_cartridge_scene || extend_ex_title_art,
                    !extend_ex_title_art);
                if (native_menu_bg1) {
                    // CONTINUE.ASM uses Mode 1 for its first seven random
                    // backdrops and keeps the source menu text in BG1. The
                    // host normally replaces BG1 with 3D geometry, so expose
                    // this cartridge bitmap only for EX's native menu and
                    // keep it confined to the original 256-pixel canvas.
                    background_renderer.draw_bg1(ppu, framebuffer,
                        starfox::render::TilePriorityPass::low,
                        viewport_origin, false, native_menu_guard_inset);
                }
                sprite_renderer.draw_objects(ppu, framebuffer, 2U, viewport_origin,
                    extend_cartridge_scene, anchor_edge_hud, gameplay_layout,
                    suppress_configurable_hud && gameplay_hud);
                background_renderer.draw_bg2(ppu, background_x, background_y,
                    framebuffer, starfox::render::TilePriorityPass::high,
                    viewport_origin,
                    extend_cartridge_scene || extend_ex_title_art,
                    !extend_ex_title_art);
                if (native_menu_bg1) {
                    background_renderer.draw_bg1(ppu, framebuffer,
                        starfox::render::TilePriorityPass::high,
                        viewport_origin, false, native_menu_guard_inset);
                }
            } else if (ppu.background_mode == 2U) {
                ++profiled_background_modes[2U];
                if (gameplay_hud) ++profiled_gameplay_hud_frames;
                const auto native_mode2_bg1 = game.flow_state()
                    == starfox::simulation::GameFlowState::ex_pregame_menu;
                if (gameplay_hud) {
                    // Gameplay's complete OBJ HUD is intentionally restored
                    // after the Super FX world below. Rendering BG2 twice and
                    // three disposable OAM priority passes here therefore did
                    // no visible work. Combine the two tile priorities in one
                    // traversal; this is the dominant wide/high-FPS path.
                    std::array<std::uint16_t, 32U> vertical_key{};
                    for (std::size_t index = 0U;
                         index < vertical_key.size(); ++index) {
                        const auto byte = (0x2fa0U + index) * 2U;
                        vertical_key[index] = static_cast<std::uint16_t>(
                            ppu.vram[byte])
                            | (static_cast<std::uint16_t>(
                                ppu.vram[byte + 1U]) << 8U);
                    }
                    const auto structural_match = mode2_background_valid
                        && mode2_background_cache.width()
                            == framebuffer.width()
                        && mode2_background_scene_revision
                            == game.scene_revision()
                        && mode2_background_id == game.map().background()
                        && mode2_background_ppu.background_mode
                            == ppu.background_mode
                        && mode2_background_ppu.bg2_tile_size_16
                            == ppu.bg2_tile_size_16
                        && mode2_background_ppu.mosaic == ppu.mosaic
                        && mode2_background_ppu.bg2_character_base
                            == ppu.bg2_character_base
                        && mode2_background_ppu.bg2_screen_base
                            == ppu.bg2_screen_base
                        && mode2_background_ppu.bg2_screen_size
                            == ppu.bg2_screen_size
                        && mode2_background_ppu.main_screen == ppu.main_screen
                        && mode2_background_ppu.bg2_vertical_offsets_enabled
                            == ppu.bg2_vertical_offsets_enabled
                        && mode2_background_ppu.bg2_horizontal_offsets_enabled
                            == ppu.bg2_horizontal_offsets_enabled;
                    // At the two extreme presentation rates the source
                    // raster does not need to be rebuilt hundreds of times
                    // per second: 360 Hz samples it at 180 Hz and 480 Hz at
                    // 160 Hz. Models, HUD, the crosshair, and presentation
                    // wipes still update at the requested refresh rate.
                    const auto temporal_reuse = structural_match
                        && presented_frames % presentation_background_cadence
                            != 0U;
                    const auto same_background = temporal_reuse
                        || (structural_match
                        // VRAM can change only at a completed source update.
                        // This invalidates animated/reloaded source graphics
                        // without comparing the live Super FX bitmap, whose
                        // unrelated BG1 writes previously defeated the cache.
                        && mode2_background_source_frame == source_logic_frames
                        && mode2_background_x == background_x
                        && mode2_background_y == background_y
                        && mode2_background_vertical == vertical_key
                        && mode2_background_ppu.bg2_horizontal_offsets
                            == ppu.bg2_horizontal_offsets);
                    if (same_background) {
                        if (temporal_reuse) {
                            ++mode2_background_temporal_hits;
                        } else {
                            ++mode2_background_exact_hits;
                        }
                        framebuffer.pixels() =
                            mode2_background_cache.pixels();
                    } else {
                        ++mode2_background_misses;
                        background_renderer.draw_bg2(ppu, background_x,
                            background_y, framebuffer,
                            starfox::render::TilePriorityPass::all,
                            viewport_origin, extend_cartridge_scene);
                        mode2_background_cache.resize(
                            framebuffer.width(), framebuffer.height());
                        mode2_background_cache.pixels() = framebuffer.pixels();
                        mode2_background_ppu = ppu;
                        mode2_background_vertical = vertical_key;
                        mode2_background_x = background_x;
                        mode2_background_y = background_y;
                        mode2_background_source_frame = source_logic_frames;
                        mode2_background_scene_revision =
                            game.scene_revision();
                        mode2_background_id = game.map().background();
                        mode2_background_valid = true;
                    }
                } else {
                    background_renderer.draw_bg2(ppu, background_x, background_y,
                        framebuffer, starfox::render::TilePriorityPass::low,
                        viewport_origin, extend_cartridge_scene);
                    sprite_renderer.draw_objects(ppu, framebuffer, 0U,
                        viewport_origin, extend_cartridge_scene,
                        anchor_edge_hud, gameplay_layout,
                        suppress_configurable_hud && gameplay_hud);
                    if (native_mode2_bg1) {
                        background_renderer.draw_bg1(ppu, framebuffer,
                            starfox::render::TilePriorityPass::low,
                            viewport_origin, false, native_menu_guard_inset);
                    }
                    sprite_renderer.draw_objects(ppu, framebuffer, 1U,
                        viewport_origin, extend_cartridge_scene,
                        anchor_edge_hud, gameplay_layout,
                        suppress_configurable_hud && gameplay_hud);
                    background_renderer.draw_bg2(ppu, background_x, background_y,
                        framebuffer, starfox::render::TilePriorityPass::high,
                        viewport_origin, extend_cartridge_scene);
                    sprite_renderer.draw_objects(ppu, framebuffer, 2U,
                        viewport_origin, extend_cartridge_scene,
                        anchor_edge_hud, gameplay_layout,
                        suppress_configurable_hud && gameplay_hud);
                    if (native_mode2_bg1) {
                        background_renderer.draw_bg1(ppu, framebuffer,
                            starfox::render::TilePriorityPass::high,
                            viewport_origin, false, native_menu_guard_inset);
                    }
                }
            } else if (ppu.background_mode == 3U) {
                ++profiled_background_modes[3U];
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
                ++profiled_background_modes[std::min<std::size_t>(
                    ppu.background_mode, profiled_background_modes.size() - 1U)];
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
            if (ex_title_logo_screen && viewport_origin > 0) {
                // TITLEI uses a black BG2 tile while CGRAM colour zero is the
                // brown Macbeth backdrop. The original 256-pixel canvas never
                // exposes colour zero, but a wide host framebuffer otherwise
                // turns untouched margin pixels brown. Replace only those
                // transparent pixels with the source tile's indexed black;
                // the extended low-priority title stars must survive instead
                // of being wiped back into solid side bands.
                const auto backdrop = framebuffer.get(
                    static_cast<std::uint32_t>(viewport_origin), 0U);
                const auto right = viewport_origin
                    + static_cast<std::int32_t>(snes_width);
                for (std::int32_t y = 0;
                     y < static_cast<std::int32_t>(framebuffer.height()); ++y) {
                    for (std::int32_t x = 0; x < viewport_origin; ++x) {
                        if (framebuffer.get(x, y) == 0U) {
                            framebuffer.set(x, y, backdrop);
                        }
                    }
                    for (std::int32_t x = right;
                         x < static_cast<std::int32_t>(framebuffer.width()); ++x) {
                        if (framebuffer.get(x, y) == 0U) {
                            framebuffer.set(x, y, backdrop);
                        }
                    }
                }
            }
            if (cache_complete_cartridge_layer
                && !reuse_complete_cartridge_layer) {
                cartridge_layer_cache.resize(
                    framebuffer.width(), framebuffer.height());
                cartridge_layer_cache.pixels() = framebuffer.pixels();
                cartridge_layer_scene_revision = game.scene_revision();
                cartridge_layer_background_id = game.map().background();
                cartridge_layer_background_mode = ppu.background_mode;
                cartridge_layer_flow_state = static_cast<std::uint8_t>(
                    game.flow_state());
                cartridge_layer_valid = true;
                ++cartridge_layer_misses;
            }
            const auto profile_background_done =
                std::chrono::steady_clock::now();
            struct VisibleObject {
                starfox::simulation::ObjectHandle handle{};
                starfox::timing::RenderTransform transform;
                CameraPoint position;
                double source_depth{};
                starfox::simulation::MatrixQ15 object_matrix{};
            };
            std::vector<VisibleObject> visible;
            auto camera = starfox::timing::interpolate(
                previous_camera, current_camera, interpolation_alpha);
            if (mouse_camera_scene) {
                camera.pitch += mouse_camera.pitch_offset;
                camera.yaw += mouse_camera.yaw_offset;
            }
            const auto camera_matrix_at = [&](const auto& snapshot) {
                return starfox::simulation::rotation_matrix_q15(
                    trigonometry,
                    static_cast<std::int16_t>(static_cast<std::uint16_t>(
                        static_cast<double>(snapshot.pitch)
                            + (mouse_camera_scene ? mouse_camera.pitch_offset : 0.0))),
                    static_cast<std::int16_t>(static_cast<std::uint16_t>(
                        static_cast<double>(snapshot.yaw)
                            + (mouse_camera_scene ? mouse_camera.yaw_offset : 0.0))),
                    static_cast<std::int16_t>(snapshot.roll));
            };
            // Interpolate complete orthonormal transforms at the selected
            // headset/output cadence. Euler interpolation can accelerate or
            // kink compound rotations even though the fixed simulation clock
            // is correct; normalized matrix interpolation changes only the
            // presentation between the same two 20 Hz source states.
            const auto view_matrix =
                starfox::simulation::interpolate_rotation_matrix_q15(
                    camera_matrix_at(previous_camera),
                    camera_matrix_at(current_camera), interpolation_alpha);
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
                dust_renderer.draw(game.dust(), game.dust_point_count(),
                    camera, view_matrix, superfx_frame);
            } else if (!planet_screen && game.map().dots_mode() > 0) {
                if (grid_lines_address != 0U
                    && game.map().read_native_word(grid_lines_address) != 0U) {
                    dust_renderer.draw_grid_lines(camera, view_matrix,
                        source_logic_frames, superfx_frame);
                } else {
                    dust_renderer.draw_grid(camera, view_matrix, superfx_frame);
                }
            }
            for (const auto handle : game.draw_order()) {
                if (!game.objects().is_active(handle)) continue;
                const auto& object = game.objects().at(handle);
                // invisible is sflag 27, stored in the fourth strategy byte.
                if ((object.strategy_flags[3] & 0x08U) != 0U) continue;
                const auto current_transform = current.find(handle);
                if (current_transform == current.end()) continue;
                auto prior = previous.find(handle);
                if (prior != previous.end()
                    && (prior->second.shape
                            != current_transform->second.shape
                        || prior->second.strategy_address
                            != current_transform->second.strategy_address
                        || prior->second.type
                            != current_transform->second.type)) {
                    // Object handles are cartridge slots, not stable entity
                    // IDs. A removed object can be replaced in the same slot
                    // between two source frames. Interpolating that new model
                    // from the old slot's pose made fresh controller/training
                    // ships appear off-screen and made multi-part bosses such
                    // as Linktron jump between unrelated component poses.
                    prior = previous.end();
                }
                // TRAIL_ISTRAT pieces are discrete source afterimages. Moving
                // every clone through fractional positions made the Nintendo
                // logo look smeared after its main text had already settled.
                const auto transform = starfox::timing::interpolate(
                    prior == previous.end() ? current_transform->second.transform
                                            : prior->second.transform,
                    current_transform->second.transform,
                    object.strategy_address == trail_strategy_address
                        ? 1.0 : interpolation_alpha);
                const auto transform_alpha =
                    object.strategy_address == trail_strategy_address
                        ? 1.0 : interpolation_alpha;
                const auto& prior_snapshot = prior == previous.end()
                    ? current_transform->second : prior->second;
                const auto object_matrix =
                    starfox::simulation::interpolate_rotation_matrix_q15(
                        prior_snapshot.rotation_matrix,
                        current_transform->second.rotation_matrix,
                        transform_alpha);
                const auto position = world_to_camera(
                    transform.x, transform.y, transform.z, camera, view_matrix);
                const auto source_position = world_to_camera(
                    current_transform->second.transform.x,
                    current_transform->second.transform.y,
                    current_transform->second.transform.z,
                    source_camera, source_view_matrix);
                visible.push_back({handle, transform, position,
                    source_position.z, object_matrix});
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
            const auto model_colour_override =
                game.model_colour_table_override();
            const auto effective_colour_table = [special_colour, red_colour,
                                                   white_colour,
                                                   model_colour_override](
                                                      const auto& object) {
                // MDRAWLIS.MC's -NAN modes 1-5 replace M_COLOURPTR before
                // hit-flash/special-colour handling, so the selected texture
                // table has priority for every object in the source list.
                if (model_colour_override) return *model_colour_override;
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
                // Text/trail objects use NULLSHAPE only as a strategy carrier;
                // TEXTURE_SCROLL_X, AL_COLTAB and AL_DEPTHOFFSET contain the
                // actual raster data. Requiring NULLSHAPE to decode before
                // this branch silently discarded Meteor's chained fire trail
                // whenever that placeholder was absent from the shape cache.
                if ((object.strategy_flags[0] & 0x40U) != 0U) continue;
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
            const auto model_scale = static_cast<double>(
                game.model_scale_multiplier());
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
                pose.scale = model_scale;
                pose.vanish_x = static_cast<std::int16_t>(
                    game.map().read_native_word(vanish_x_address)
                    + superfx_ui_offset_x);
                pose.vanish_y = static_cast<std::int16_t>(
                    game.map().read_native_word(vanish_y_address)
                    + (extend_scene_vertical ? superfx_offset_y : 0));
                auto object_matrix = item.object_matrix;
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
                pose.subpixel_projection = !game.paused()
                    && game.presentation_fps() > 20U
                    && interpolation_alpha > 0.0
                    && interpolation_alpha < 1.0
                    && object.strategy_address != trail_strategy_address;
                pose.animation_frame = display_frame(object.animation_frame);
                pose.colour_frame = display_frame(object.colour_frame);
                pose.texture_scroll_x = object.texture_scroll_x;
                pose.texture_scroll_y = object.texture_scroll_y;
                pose.wireframe_mode = wire_mode_address != 0U
                    ? game.map().read_native_byte(wire_mode_address) : 0U;
                pose.wobble_mode = wobble_mode_address != 0U
                    ? game.map().read_native_byte(wobble_mode_address) : 0U;
                pose.wave_mode = wave_mode_address != 0U
                    && game.map().read_native_byte(wave_mode_address) != 0U;
                pose.cel_mode = cel_mode_address != 0U
                    && game.map().read_native_byte(cel_mode_address) != 0U;
                pose.wave_offset = wave_offset_address != 0U
                    ? static_cast<std::int16_t>(game.map().read_native_word(
                        wave_offset_address)) : 0;
                pose.colour_warp = colour_warp_address != 0U
                    && game.map().read_native_word(colour_warp_address) != 0U;
                if (projected_points_address != 0U) {
                    pose.projected_points_address = static_cast<std::uint16_t>(
                        projected_points_address);
                }
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
                starfox::render::apply_source_depth_tables(rom,
                    depth_table_address, depth_thresholds, depth_colours,
                    object.extended[21], pose);
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
                auto& target = controls_screen && item.handle == game.player()
                    ? controls_player_layer : superfx_frame;
                if ((object.strategy_flags[0] & 0x40U) != 0U) {
                    text_renderer.draw(object.colour_table, object.extended[21],
                        std::bit_cast<std::int8_t>(object.texture_scroll_x),
                        make_pose(item, false), target);
                    continue;
                }
                const auto colour_table = effective_colour_table(object);
                const auto base_shape_key = (static_cast<std::uint32_t>(object.shape) << 16U)
                    | colour_table;
                if (object.shape == 0 || invalid_shapes.contains(base_shape_key)) continue;
                const auto base = shape_cache.find(base_shape_key);
                if (base == shape_cache.end()) continue;
                if ((object.strategy_flags[0] & 0x10U) != 0U) {
                    particle_renderer.draw_owner(game.particles(), item.handle,
                        make_pose(item, false), interpolation_alpha,
                        target);
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
                renderer.draw(found->second, pose, target, false,
                    &target == &superfx_frame
                            && (game.enhanced_graphics() || game.smooth_polys()
                                || game.rtx_lighting())
                        ? &superfx_surfaces : nullptr);
            }
            if (game.flow_state()
                    == starfox::simulation::GameFlowState::ex_pregame_menu
                || game.flow_state()
                    == starfox::simulation::GameFlowState::continue_choice) {
                const auto& native_model = game.map().native_model_draw();
                if (native_model.active && native_model.shape != 0U) {
                    const auto shape_key =
                        (static_cast<std::uint32_t>(native_model.shape) << 16U)
                        | native_model.colour_table;
                    if (!invalid_shapes.contains(shape_key)) {
                        auto found = shape_cache.find(shape_key);
                        if (found == shape_cache.end()) {
                            try {
                                found = shape_cache.emplace(shape_key,
                                    decoder.decode(native_model.shape, {},
                                        native_model.colour_table)).first;
                            } catch (const std::exception&) {
                                invalid_shapes.insert(shape_key);
                            }
                        }
                        if (found != shape_cache.end()) {
                            starfox::render::RenderPose pose;
                            pose.x = native_model.x;
                            pose.y = native_model.y;
                            // Model-viewer shoulder zoom updates M_BIGZ in
                            // CONTINUE.ASM after the Super FX draw snapshot is
                            // launched. Read that live source word so every
                            // visible presentation reflects the new distance.
                            pose.z = native_model_z_address != 0U
                                ? static_cast<std::int16_t>(
                                    game.map().read_native_word(
                                        native_model_z_address))
                                : native_model.z;
                            pose.pitch = static_cast<std::uint16_t>(
                                (native_model.rotation_x & 0x00ffU) << 8U);
                            pose.yaw = static_cast<std::uint16_t>(
                                (native_model.rotation_y & 0x00ffU) << 8U);
                            pose.roll = static_cast<std::uint16_t>(
                                (native_model.rotation_z & 0x00ffU) << 8U);
                            pose.rotation_matrix =
                                starfox::simulation::rotation_matrix_q15(
                                    trigonometry,
                                    static_cast<std::int16_t>(pose.pitch),
                                    static_cast<std::int16_t>(pose.yaw),
                                    static_cast<std::int16_t>(pose.roll));
                            pose.use_rotation_matrix = true;
                            // MSHOWOBJ3 (the source model viewer) enters
                            // MSHOWOBJECT directly; only the normal draw-list
                            // MSHOWOBJ2 path applies Huge Models.
                            pose.scale = 1.0;
                            pose.vanish_x = native_model.vanish_x
                                + superfx_ui_offset_x;
                            pose.vanish_y = native_model.vanish_y;
                            pose.animation_frame = native_model.animation_frame;
                            pose.colour_frame = native_model.colour_frame;
                            pose.wireframe_mode = wire_mode_address != 0U
                                ? game.map().read_native_byte(wire_mode_address)
                                : 0U;
                            pose.wobble_mode = wobble_mode_address != 0U
                                ? game.map().read_native_byte(wobble_mode_address)
                                : 0U;
                            pose.wave_mode = wave_mode_address != 0U
                                && game.map().read_native_byte(wave_mode_address)
                                    != 0U;
                            pose.cel_mode = cel_mode_address != 0U
                                && game.map().read_native_byte(cel_mode_address)
                                    != 0U;
                            pose.wave_offset = wave_offset_address != 0U
                                ? static_cast<std::int16_t>(
                                    game.map().read_native_word(
                                        wave_offset_address))
                                : 0;
                            pose.colour_warp = colour_warp_address != 0U
                                && game.map().read_native_word(
                                    colour_warp_address) != 0U;
                            if (projected_points_address != 0U) {
                                pose.projected_points_address =
                                    static_cast<std::uint16_t>(
                                        projected_points_address);
                            }
                            starfox::render::apply_source_depth_tables(rom,
                                depth_table_address, depth_thresholds,
                                depth_colours, 0U, pose);
                            renderer.draw(
                                found->second, pose, superfx_frame, false,
                                game.enhanced_graphics() || game.smooth_polys()
                                        || game.rtx_lighting()
                                    ? &superfx_surfaces : nullptr);
                        }
                    }
                }
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
                    dialogue.portrait_frame, 48, 152, comms_hud,
                    7U * 16U, dialogue.alternate_portraits);
                if (dialogue.text_visible) {
                    const auto text_y = dialogue.three_lines ? 153 : 169;
                    text_renderer.draw_game_text(dialogue.text_address,
                        83, text_y + 1, comms_hud, 7U * 16U, 9U, 175);
                    text_renderer.draw_game_text(dialogue.text_address,
                        82, text_y, comms_hud, 7U * 16U, std::nullopt, 174);
                }
            }
            const auto results = game.stage_results_state();
            if (results.active
                && game.experience()
                    != starfox::simulation::Experience::starfox_ex) {
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
                constexpr std::array<std::int32_t, 3> down_x{11, 91, 170};
                constexpr std::array<std::uint8_t, 3> live_face_frame{
                    7U, 9U, 11U};
                for (std::size_t teammate = 0; teammate < 3U; ++teammate) {
                    const auto alive = results.teammate_health[teammate] != 0U;
                    const auto face_frame = alive
                        ? live_face_frame[teammate]
                        : static_cast<std::uint8_t>(
                            (game.map().read_native_byte(game_frame_address) & 1U)
                                != 0U ? 4U : 17U);
                    text_renderer.draw_face(
                        face_frame, face_x[teammate], 96, superfx_ui);
                    if (alive) {
                        text_renderer.draw_game_text(teammate_text[teammate],
                            name_x[teammate], 152, superfx_ui);
                        for (std::int32_t y = 138; y < 150; ++y) {
                            for (std::int32_t x = bar_x[teammate];
                                 x < bar_x[teammate] + 44; ++x) {
                                const auto border = y == 138 || y == 149
                                    || x == bar_x[teammate]
                                    || x == bar_x[teammate] + 43;
                                const auto filled = y >= 140 && y < 148
                                    && x >= bar_x[teammate] + 2
                                    && x - bar_x[teammate] - 2
                                        < std::min<std::uint8_t>(
                                            results.teammate_health[teammate], 40U);
                                superfx_ui.set(x, y, static_cast<std::uint8_t>(
                                    7U * 16U + (border ? 14U
                                        : (filled ? 2U : 0U))));
                            }
                        }
                    } else {
                        text_renderer.draw_game_text(teammate_text[teammate],
                            name_x[teammate], 137, superfx_ui);
                        text_renderer.draw_game_text(teammate_down_text,
                            down_x[teammate], 151, superfx_ui);
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
                auto preview_meters = game.meter_state();
                if (hud_editor.active) {
                    // The editor must expose complete, independent shield,
                    // boost/bomb, and boss groups regardless of the exact
                    // cartridge tick selected for the frozen background.
                    preview_meters.enabled = true;
                    preview_meters.extended = false;
                    preview_meters.damage = 30U;
                    preview_meters.boost = 26U;
                    preview_meters.shield_up = false;
                    preview_meters.boost_enabled = true;
                    preview_meters.player_two_activated = false;
                    preview_meters.second_player_view = false;
                    preview_meters.player_one_dead = false;
                    preview_meters.boss_max_health = 0xffU;
                    preview_meters.boss_health = 180U;
                }
                sprite_renderer.draw_meters(
                    preview_meters, superfx_hud, true,
                    gameplay_hud ? &active_hud_layout : nullptr);
                if (hud_editor.active) {
                    constexpr std::int32_t preview_boss_meter_width = 131;
                    const auto boss_offset = active_hud_layout[
                        starfox::render::HudElement::boss_health];
                    const auto boss_x = static_cast<std::int32_t>(
                        superfx_hud.width()) - 18
                        - preview_boss_meter_width + boss_offset.x;
                    constexpr std::string_view enemy_label{"ENEMY"};
                    text_renderer.draw_ascii(enemy_label,
                        boss_x - text_renderer.measure_ascii(enemy_label) - 4,
                        1 + boss_offset.y, superfx_hud, 14U);
                }
            }

            const auto profile_world_done = std::chrono::steady_clock::now();
            // Colour zero is transparent in every host Super FX layer.
            const auto composite_superfx = [&framebuffer, viewport_origin,
                                                &ppu](
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
                starfox::render::LayerCompositeSettings settings;
                settings.offset_x = offset_x;
                settings.offset_y = offset_y;
                // Every separated host SuperFX layer represents BG1 pixels.
                // Apply $2106 before compositing so EX's mosaic shortcut
                // affects host-rendered models, particles, meters and text in
                // exactly the same way as the cartridge bitmap.
                settings.mosaic = ppu.mosaic;
                settings.mosaic_layer_mask = 0x01U;
                settings.mosaic_origin_x = viewport_origin;
                if (clip_controls) {
                    settings.clip_left = controls_left;
                    settings.clip_right = controls_right;
                    settings.clip_top = controls_top;
                    settings.clip_bottom = controls_bottom;
                }
                starfox::render::composite_transparent_layer(
                    source, framebuffer, settings);
            };

            if (controls_screen) {
                // CONT draws demo lasers and bombs before its player pass.
                // Keeping those effects in the ordinary foreground layer
                // painted them across the Arwing; composite them first, then
                // the isolated player, and finally the controller artwork.
                composite_superfx(
                    superfx_frame, 0, scene_offset_y, true);
                composite_superfx(
                    controls_player_layer, 0, superfx_offset_y, true);
                if (ppu.background_mode == 1U) {
                    background_renderer.draw_bg2(ppu, background_x,
                        background_y, framebuffer,
                        starfox::render::TilePriorityPass::high,
                        viewport_origin, false);
                }
            } else {
                composite_superfx(
                    superfx_frame, 0, scene_offset_y, false);
            }
            if (game.experience()
                    == starfox::simulation::Experience::starfox_ex
                && gameplay_hud) {
                // EX draws its scored/FPS/multiplayer diagnostics and full
                // interactive pause menu into the native Super FX BG1
                // bitmap. Render it into a transparent staging layer first:
                // source guard pixels can use non-zero palette entries whose
                // RGB value is black, and drawing those directly created 4:3
                // bars over the expanded EX world. The PC communication HUD
                // is authoritative while a message is active, avoiding a
                // second copy of the same EX portrait/text from this bitmap.
                background_renderer.draw_bg1(ppu, native_ex_overlay,
                    starfox::render::TilePriorityPass::all,
                    0, false);
                if (dialogue.active && !game.paused()) {
                    native_ex_overlay.clear(0U);
                } else {
                    for (std::uint32_t y = 0U;
                         y < native_ex_overlay.height(); ++y) {
                        for (std::uint32_t x = 0U;
                             x < native_ex_overlay.width(); ++x) {
                            const auto index = native_ex_overlay.get(x, y);
                            if (index != 0U
                                && (ppu.cgram[index] & 0x7fffU) == 0U) {
                                native_ex_overlay.set(x, y, 0U);
                            }
                        }
                    }
                }
                composite_superfx(
                    native_ex_overlay, viewport_origin, 0, false);
            }
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
                // Source Mode 1 places the title model above BG2's logo/team
                // artwork but below BG3's high-priority PRESS START prompt.
                // EX's introductory logo uses its whole BG1 bitmap for the
                // animation; the regular retail/EX title uses BG1 only for
                // source-authored text that must survive host model drawing.
                background_renderer.draw_title_foreground(ppu,
                    background_x, background_y, framebuffer, viewport_origin,
                    !ex_title_logo_screen, extend_ex_title_art);
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
                    framebuffer.set(x, 216, border_colour);
                }
                for (std::int32_t y = 20; y <= 216; ++y) {
                    framebuffer.set(menu_left + viewport_origin, y, border_colour);
                    framebuffer.set(menu_right + viewport_origin, y, border_colour);
                }
                const auto draw_centred = [&text_renderer, &framebuffer,
                                            viewport_origin](
                                               std::string_view text,
                                               std::int32_t y,
                                               std::uint8_t colour) {
                    text_renderer.draw_ascii(text,
                        128 - text_renderer.measure_ascii(text) / 2
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
                        hud_profile_index(
                            game.display_mode(), game.experience())];
                    std::optional<starfox::render::HudElement> hovered;
                    std::int32_t hovered_area = std::numeric_limits<std::int32_t>::max();
                    for (std::uint8_t value = 0U;
                         value < static_cast<std::uint8_t>(
                             starfox::render::HudElement::count); ++value) {
                        const auto element = static_cast<starfox::render::HudElement>(
                            value);
                        const auto rect = placed_hud_rect(
                            element, display_width, editor_layout,
                            game.experience());
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
                            display_width, editor_layout, game.experience()),
                            static_cast<std::uint8_t>(palette_base + 14U));
                    }

                    // The preview itself is composited from a captured native
                    // gameplay frame below. This indexed layer is deliberately
                    // limited to unobtrusive editor chrome and drag handles.
                    solid(0, 0, static_cast<std::int32_t>(display_width),
                        11, static_cast<std::uint8_t>(palette_base + 1U));
                    const auto editor_title = std::string{"HUD LAYOUT  "}
                        + (game.experience()
                                == starfox::simulation::Experience::starfox_ex
                            ? "STARFOX EX  " : "ORIGINAL  ")
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
                    draw_centred("STAR FOX ENHANCED", 23, 14U);
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
                                menu_value_right
                                    - text_renderer.measure_ascii(value)
                                    + viewport_origin,
                                y, framebuffer, colour);
                        }
                    };

                    if (game.pregame_page()
                        == starfox::simulation::PregamePage::options) {
                        draw_centred("OPTIONS", 39, 10U);
                        const auto god_value = game.god_mode()
                            ? std::string_view{"ON"} : std::string_view{"OFF"};
                        const auto fps_value = game.show_fps()
                            ? std::string_view{"ON"} : std::string_view{"OFF"};
                        const auto crosshair = crosshair_colour_name(
                            game.crosshair_colour());
                        draw_row("GOD MODE", god_value, 47,
                            game.pregame_selection() == 0U);
                        draw_row("ON-SCREEN FPS", fps_value, 63,
                            game.pregame_selection() == 1U);
                        draw_row("CROSSHAIR COLOR", crosshair, 79,
                            game.pregame_selection() == 2U);
                        draw_row("CUSTOMIZE SCREEN", "A  OPEN", 95,
                            game.pregame_selection() == 3U);
                        const auto music_volume =
                            std::to_string(game.music_volume()) + "%";
                        const auto sfx_volume =
                            std::to_string(game.sfx_volume()) + "%";
                        draw_row("MUSIC VOLUME", music_volume, 111,
                            game.pregame_selection() == 4U);
                        draw_row("SFX VOLUME", sfx_volume, 137,
                            game.pregame_selection() == 5U);
                        draw_row("BACK", "", 163,
                            game.pregame_selection() == 6U);
                        const auto draw_volume_bar = [&framebuffer,
                                                         viewport_origin](
                                                         std::int32_t y,
                                                         std::uint8_t volume,
                                                         bool selected) {
                            constexpr std::int32_t left = 147;
                            constexpr std::int32_t width = 89;
                            constexpr std::int32_t height = 6;
                            const auto border = static_cast<std::uint8_t>(
                                7U * 16U + (selected ? 14U : 7U));
                            const auto fill = static_cast<std::uint8_t>(
                                7U * 16U + 10U);
                            for (std::int32_t x = 0; x < width; ++x) {
                                framebuffer.set(left + viewport_origin + x,
                                    y, border);
                                framebuffer.set(left + viewport_origin + x,
                                    y + height - 1, border);
                            }
                            for (std::int32_t row = 1; row < height - 1; ++row) {
                                framebuffer.set(left + viewport_origin,
                                    y + row, border);
                                framebuffer.set(left + viewport_origin
                                    + width - 1, y + row, border);
                            }
                            const auto filled = static_cast<std::int32_t>(
                                (width - 2) * volume / 100U);
                            for (std::int32_t row = 1; row < height - 1; ++row) {
                                for (std::int32_t x = 1; x <= filled; ++x) {
                                    framebuffer.set(left + viewport_origin + x,
                                        y + row, fill);
                                }
                            }
                        };
                        draw_volume_bar(120, game.music_volume(),
                            game.pregame_selection() == 4U);
                        draw_volume_bar(146, game.sfx_volume(),
                            game.pregame_selection() == 5U);
                        constexpr std::array<std::int32_t, 7> cursor_y{
                            50, 66, 82, 98, 114, 140, 166};
                        draw_cursor(cursor_y[game.pregame_selection()]);
                        draw_centred("A/LEFT/RIGHT  CHANGE", 184, 13U);
                        draw_centred("B  BACK", 198, 13U);
                    } else {
                        draw_centred("PRE-GAME SETUP", 37, 10U);
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
                        const auto experience = game.experience()
                            == starfox::simulation::Experience::original
                            ? std::string_view{"ORIGINAL"}
                            : std::string_view{"STARFOX EX"};
                        constexpr std::array<std::int32_t, 15> row_y{
                            50, 59, 68, 77, 86, 95, 104, 113,
                            122, 131, 140, 149, 158, 167, 176};
                        const auto on_off = [](bool enabled) {
                            return enabled ? std::string_view{"ON"}
                                           : std::string_view{"OFF"};
                        };
                        const auto draw_compact_row =
                            [&text_renderer, &framebuffer, viewport_origin,
                                menu_label_x, menu_value_right](
                                std::string_view label, std::string_view value,
                                std::int32_t y, bool selected) {
                                const auto colour = static_cast<std::uint8_t>(
                                    selected ? 14U : 7U);
                                text_renderer.draw_ascii_compact(label,
                                    menu_label_x + viewport_origin,
                                    y, framebuffer, colour);
                                if (!value.empty()) {
                                    text_renderer.draw_ascii_compact(value,
                                        menu_value_right
                                            - text_renderer.measure_ascii(value)
                                            + viewport_origin,
                                        y, framebuffer, colour);
                                }
                            };
                        draw_compact_row("EXPERIENCE", experience, row_y[0],
                            game.pregame_selection() == 0U);
                        draw_compact_row("PACE/SPEED", timing, row_y[1],
                            game.pregame_selection() == 1U);
                        draw_compact_row("RENDER FPS", presentation, row_y[2],
                            game.pregame_selection() == 2U);
                        draw_compact_row("DISPLAY", display, row_y[3],
                            game.pregame_selection() == 3U);
                        draw_compact_row("RENDERER",
                            game.renderer_mode()
                                    == starfox::simulation::RendererMode::gpu
                                ? std::string_view{"GPU"}
                                : std::string_view{"SOFTWARE"},
                            row_y[4], game.pregame_selection() == 4U);
                        const auto msu1_value = game.msu1_available()
                            ? on_off(game.msu1_music())
                            : std::string_view{"NOT FOUND"};
                        draw_compact_row("MSU-1 MUSIC", msu1_value,
                            row_y[5], game.pregame_selection() == 5U);
                        draw_compact_row("RUMBLE", on_off(game.rumble()), row_y[6],
                            game.pregame_selection() == 6U);
                        draw_compact_row("ANTI-ALIASING",
                            anti_aliasing_name(game.anti_aliasing_mode()), row_y[7],
                            game.pregame_selection() == 7U);
                        draw_compact_row("ENHANCED TEXTURES",
                            on_off(game.enhanced_graphics()), row_y[8],
                            game.pregame_selection() == 8U);
                        draw_compact_row("UPSCALED POLYS",
                            on_off(game.smooth_polys()), row_y[9],
                            game.pregame_selection() == 9U);
                        draw_compact_row("RTX LIGHTING",
                            on_off(game.rtx_lighting()), row_y[10],
                            game.pregame_selection() == 10U);
                        draw_compact_row("VSYNC", on_off(game.vsync()), row_y[11],
                            game.pregame_selection() == 11U);
                        draw_compact_row("CONTROLLER", "A  REMAP", row_y[12],
                            game.pregame_selection() == 12U);
                        draw_compact_row("OPTIONS", "A  OPEN", row_y[13],
                            game.pregame_selection() == 13U);
                        draw_compact_row("START GAME", "", row_y[14],
                            game.pregame_selection() == 14U);
                        constexpr std::array<std::int32_t, 15> cursor_y{
                            54, 63, 72, 81, 90, 99, 108, 117,
                            126, 135, 144, 153, 162, 171, 180};
                        draw_cursor(cursor_y[game.pregame_selection()]);
                        const auto footer = std::string_view{
                            "D-PAD/A  CHOOSE    START  BEGIN"};
                        text_renderer.draw_ascii_compact(footer,
                            (static_cast<std::int32_t>(framebuffer.width())
                                - text_renderer.measure_ascii(footer)) / 2,
                            202, framebuffer, 13U);
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
            const auto window_wipe = game.window_wipe_state();
            const auto forced_black = uses_black_window
                && game.map().read_native_byte(stay_black_address) != 0xffU;
            const auto launch_wipe = game.flow_state()
                    == starfox::simulation::GameFlowState::gameplay
                // ExitBase hands the source colour-window reveal to the
                // first Corneria background ($33) before normal play begins.
                && game.map().background() == 0x33U
                && window_wipe.active;
            auto wipe_has_started_revealing = false;
            if (launch_wipe) {
                for (std::size_t line = 0U;
                     line < window_wipe.left.size(); ++line) {
                    if (window_wipe.left[line] != 16U
                        || window_wipe.right[line] != 239U) {
                        wipe_has_started_revealing = true;
                        break;
                    }
                }
            }
            if (wipe_has_started_revealing) {
                ++launch_wipe_reveal_frames;
            } else if (!launch_wipe) {
                launch_wipe_reveal_frames = 0U;
            }
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
            exit_confirmation_overlay.clear();
            if (exit_confirmation) {
                constexpr std::string_view prompt{"EXIT GAME?"};
                text_renderer.draw_ascii(prompt,
                    (static_cast<std::int32_t>(
                         exit_confirmation_overlay.width())
                        - text_renderer.measure_ascii(prompt)) / 2,
                    4, exit_confirmation_overlay, 1U, 0U);
                const auto choices = exit_yes_selected
                    ? std::string_view{"> YES       NO"}
                    : std::string_view{"  YES     > NO"};
                text_renderer.draw_ascii(choices,
                    (static_cast<std::int32_t>(
                         exit_confirmation_overlay.width())
                        - text_renderer.measure_ascii(choices)) / 2,
                    22, exit_confirmation_overlay, 1U, 0U);
            }
            auto base_palette = starfox::render::decode_bgr555_palette(
                game.map().ppu_state().cgram);
            apply_crosshair_tint(base_palette, game.crosshair_colour());
            auto presentation_brightness = game.map().display_brightness();
            if (wipe_has_started_revealing
                && launch_wipe_reveal_frames > 1U) {
                // ExitBase arms its visible window a few raster phases before
                // its coarse 20 Hz map loop begins FADEUP.  Let the reveal and
                // fade overlap at a physical 60 Hz cadence, as they do on the
                // cartridge, instead of presenting an extra dead-black hold
                // followed by a 0->9 brightness pop on low output rates.
                const auto reveal_phases = static_cast<std::uint32_t>(
                    (launch_wipe_reveal_frames - 1U) * 60U
                    / std::max<std::uint16_t>(game.presentation_fps(), 1U));
                presentation_brightness = static_cast<std::uint8_t>(
                    std::max<std::uint32_t>(presentation_brightness,
                        std::min<std::uint32_t>(15U, reveal_phases * 3U)));
            }
            auto palette = starfox::render::apply_snes_brightness(
                base_palette, presentation_brightness);
            if (forced_black) palette.fill({0U, 0U, 0U, 255U});
            if (hud_editor.active) {
                // Pick neutral editor colours already present in the active
                // cartridge palette. This keeps the static scene's genuine
                // model, portrait, meter, and level hues intact instead of
                // replacing their shared Super FX palette bank.
                const auto editor_background = nearest_palette_index(
                    palette, {48U, 48U, 60U, 255U});
                const auto editor_foreground = nearest_palette_index(
                    palette, {238U, 238U, 242U, 255U});
                draw_hud_editor_chrome(framebuffer, text_renderer,
                    hud_editor, active_hud_layout,
                    game.experience(), game.display_mode(),
                    editor_background, editor_foreground);
            }
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
            presentation_effects.wipe = window_wipe;
            presentation_effects.model_surfaces =
                game.enhanced_graphics() || game.smooth_polys()
                        || game.rtx_lighting()
                ? &superfx_surfaces : nullptr;
            presentation_effects.model_surface_y = scene_offset_y;
            presentation_effects.background_fixed_white_subtract =
                game.game_over_background_subtract();
            if (presentation_effects.background_fixed_white_subtract != 0U) {
                presentation_effects.fixed_subtract_foreground =
                    &superfx_frame;
                presentation_effects.fixed_subtract_foreground_y =
                    scene_offset_y;
            }
            presentation_effects.expand_wipe = display_width > snes_width
                && extend_cartridge_scene;
            presentation_effects.expand_wipe_vertical = extend_scene_vertical;
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
            if (exit_confirmation) {
                presentation_effects.confirmation_overlay =
                    &exit_confirmation_overlay;
            }
            presentation_effects.touch_controls = touch_controls.visible();
            const auto profile_composite_done =
                std::chrono::steady_clock::now();
            window.present(
                framebuffer, palette, circle, presentation_effects);
            const auto profile_present_done = std::chrono::steady_clock::now();
            profile_background_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    profile_background_done - profile_frame_start).count());
            profile_world_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    profile_world_done - profile_background_done).count());
            profile_composite_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    profile_composite_done - profile_world_done).count());
            profile_present_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    profile_present_done - profile_composite_done).count());
            if (presentation_history
                && std::getenv("STARFOX_TEST_DISABLE_HISTORY") == nullptr) {
                presentation_history->record(
                    framebuffer.width(), framebuffer.height(), window.rgba());
            }
            if (advance_frozen_frame) {
                window.set_frame_debug_status(true,
                    presentation_history
                        ? presentation_history->cursor() : 0U,
                    presentation_history
                        ? presentation_history->frame_count() : 0U);
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
                if (std::getenv("STARFOX_TRACE_FPS") != nullptr) {
                    std::cerr << "fps-matrix display="
                              << static_cast<unsigned>(game.display_mode())
                              << " requested=" << game.presentation_fps()
                              << " pace="
                              << static_cast<unsigned>(game.timing_mode())
                              << " measured=" << live_fps.fps()
                              << " frames=" << presented_frames << '\n';
                }
                if (std::getenv("STARFOX_TRACE_PROFILE") != nullptr
                    && presented_frames != 0U) {
                    const auto average_us = [presented_frames](
                                                std::uint64_t total) {
                        return total / presented_frames / 1'000U;
                    };
                    std::cerr << "render-profile-us background="
                              << average_us(profile_background_ns)
                              << " world=" << average_us(profile_world_ns)
                              << " composite=" << average_us(profile_composite_ns)
                              << " present=" << average_us(profile_present_ns)
                              << " bg-cache="
                              << mode2_background_temporal_hits << '/'
                              << mode2_background_exact_hits << '/'
                              << mode2_background_misses
                              << " layer-cache="
                              << cartridge_layer_temporal_hits << '/'
                              << cartridge_layer_misses
                              << " modes=" << profiled_background_modes[0U]
                              << '/' << profiled_background_modes[1U]
                              << '/' << profiled_background_modes[2U]
                              << '/' << profiled_background_modes[3U]
                              << '/' << profiled_background_modes[4U]
                              << '/' << profiled_background_modes[5U]
                              << '/' << profiled_background_modes[6U]
                              << '/' << profiled_background_modes[7U]
                              << " hud=" << profiled_gameplay_hud_frames
                              << '\n';
                }
                if (std::getenv("STARFOX_TRACE_MSU1") != nullptr) {
                    std::cerr << "msu1 enabled=" << game.msu1_music()
                              << " available=" << game.msu1_available()
                              << " track=" << audio.msu1_track()
                              << " playing=" << audio.msu1_playing() << '\n';
                }
                if (!capture_path.empty()) window.save_bmp(capture_path);
                if (std::getenv("STARFOX_TRACE_RENDER_STATE") != nullptr) {
                    const auto& trace_ppu = game.map().ppu_state();
                    std::cerr << "render-state flow="
                              << static_cast<unsigned>(game.flow_state())
                              << " mode="
                              << static_cast<unsigned>(trace_ppu.background_mode)
                              << " tm=$" << std::hex
                              << static_cast<unsigned>(trace_ppu.main_screen)
                              << " bg2sc=$" << trace_ppu.bg2_screen_base
                              << " bg2chr=$" << trace_ppu.bg2_character_base
                              << " bg2tile="
                              << (trace_ppu.bg2_tile_size_16 ? 16 : 8)
                              << " bg3sc=$" << trace_ppu.bg3_screen_base
                              << " bg3chr=$" << trace_ppu.bg3_character_base
                              << " bg=" << game.map().background()
                              << std::dec << " scroll=("
                              << trace_ppu.bg2_scroll_x << ','
                              << trace_ppu.bg2_scroll_y << ") vofs="
                              << trace_ppu.bg2_vertical_offsets_enabled
                              << " hofs="
                              << trace_ppu.bg2_horizontal_offsets_enabled
                              << " dots="
                              << static_cast<int>(game.map().dots_mode())
                              << '\n';
                }
                running = false;
            }
        }

        if (!hud_editor_preview) synchronize_ex_save();
        if (hud_editor.active || hud_editor.dragging) save_hud_layout();
        close_gamepads();
        if (restart_runtime) continue;
        return 0;
        }
        return 0;
    } catch (const std::exception& error) {
        const std::string message =
            std::string{"Star Fox Enhanced could not start:\n\n"} + error.what();
        std::cerr << "starfox_pc failed: " << error.what() << '\n';
#if defined(_WIN32)
        // Automated runtime checks must remain headless even when they find a
        // regression; stderr and the non-zero exit status are sufficient and
        // cannot strand modal dialogs on the user's desktop.
        if (std::getenv("STARFOX_TEST_FRAMES") == nullptr) {
            MessageBoxA(nullptr, message.c_str(), "Star Fox Enhanced",
                MB_OK | MB_ICONERROR | MB_TASKMODAL);
        }
#endif
        return 1;
    }
}
