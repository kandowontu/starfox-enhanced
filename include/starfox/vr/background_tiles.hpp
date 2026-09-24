#pragma once
#include "starfox/simulation/snes_ppu.hpp"
#include "starfox/vr/draw_packet.hpp"
#include <array>
#include <optional>
#include <vector>

namespace starfox::vr {
struct BackgroundTileOptions;
enum class BackgroundLayer { bg1, bg2, bg3 };
struct PhotographicLandscape {
    bool operator==(const PhotographicLandscape&) const = default;
    // Match desktop's source-pixel scale near the forward direction. The
    // repeated panorama closes at the rear without putting imagery on ground.
    float horizon_v{1.F},vertical_scale{512.F/160.F},horizontal_offset{.25F};
    float horizontal_scale{1.F};
    unsigned repeats{6};
    bool full_sphere{};
    bool latitude_uv{}; // All-sky stars: distribute vertically instead of clamping a horizon strip.
    bool orbital_surface{}; // Local-direction surface continuation; blend to separate stars above the limb.
    bool preserve_game_over_front{};
    std::array<float,4> response{1,1,1,1};
    std::array<float,3> palette_shift{};
    // Native BGR555 ramps; index zero selects Titania (1) or Sector K (2).
    // Draw data, not image pixels: weather must never reload the photograph.
    std::array<uint16_t,16> cloud_palette{};
};
struct PhotographicBody {
    bool operator==(const PhotographicBody&) const = default;
    // Source-pixel registration before the shared landscape motion matrix.
    std::array<float,2> center{128,112},diameter{56,56};
    std::array<float,4> response{1,1,1,1};
    // Optional two-tone native shading (bright/dark BGR555).
    std::array<float,3> palette_shift{};
    bool two_tone{};
    uint16_t bright{},dark{};
    // 4: face ramp; 5: cloud limb; 6/7: gray/blue city moons; 8: orbital moon.
    std::array<uint16_t,16> palette{};
};
[[nodiscard]] DrawPacket photographic_body_packet(
    std::shared_ptr<const std::vector<uint32_t>>,const PhotographicBody&,bool srgb=false);
[[nodiscard]] Matrix4 photographic_body_motion(std::array<float,2> center);
// Correct current-tick panorama geometry toward the interpolated scroll. Delta
// is wrapped source pixels; no texture/vertex upload is needed between ticks.
[[nodiscard]] Matrix4 photographic_scroll_correction(float dx,float dy,double alpha,
    float horizontal_scale=1.F,float vertical_scale=512.F/224.F);
// One indexed landmark over a replaced sky. Only selected native ink runs are
// geometry; palette fades stay in the original GPU tile sampler.
[[nodiscard]] DrawPacket landscape_landmark_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,const std::array<unsigned,4>&,
    const std::array<bool,256>&,bool srgb=false);
// Upper hemisphere only. Apply the same landscape_camera_motion as the native
// ground packet. Palette response is independent of the retained pixel payload.
[[nodiscard]] DrawPacket photographic_landscape_packet(
    std::shared_ptr<const std::vector<uint32_t>> texture,
    const PhotographicLandscape& options={},bool srgb=false);
// Opaque enclosure with a 256x224 source-window opening. Draw after world
// geometry with depth disabled, before HUD; caller supplies source placement.
[[nodiscard]] DrawPacket tunnel_surround_packet(const std::array<float,4>& colour);
// Opaque CGRAM-zero backdrop, with source integer fading followed by target
// color-space conversion. Shared by both eyes, independent of layer enables.
[[nodiscard]] std::array<float,4> source_backdrop_colour(uint16_t bgr555,
    unsigned brightness,bool srgb=false);
[[nodiscard]] std::array<float,4> source_background_border_colour(
    const std::array<uint16_t,256>& palette,unsigned brightness,bool srgb=false);
[[nodiscard]] std::array<float,4> source_menu_background_colour(
    const simulation::SnesPpuState&,unsigned brightness,bool srgb=false);
struct BackgroundTileOptions {
    unsigned priority{}; // 0 all, 1 low, 2 high
    unsigned brightness{15}; // source display brightness, 0..15
    unsigned colour_subtract{}; // BG2-only native HALFFADE, 0..31.
    bool expanded_horizontal{};
    bool transparent_black{}; // Test source palette before brightness fading.
    bool wrap_horizontal{true}; // False: expose only one authored tilemap occurrence.
    unsigned single_occurrence_top_rows{}; // 0..224: unique top band; 512: one complete atlas in both axes.
    bool ex_twin_planets{}; // BG_5_4: unique planet ink, repeatable cloud tiles.
    bool ex_face_planets{}; // BG_6_3H: unique planets/saucers, repeatable stars.
    bool ex_ocean_island{}; // BG_6_2: preserve the first island, repeat open water.
    bool ex_volcanic_horizon{}; // BG_6_6: one bright eruption on a repeatable horizon.
    bool ex_city_planets{}; // BG_5_2: unique planets/palace, surrounding stars/city.
    unsigned guard_inset{}; // Native pixel columns omitted from each side.
    std::array<int32_t,2> horizontal_bounds{0,256}; // Logical pixels, not stretched UVs.
    std::optional<std::array<int16_t,2>> scroll_override;
};
// Encode source data only: no CPU pixel decoding, projection, or layer order.
// Caller supplies brightness and owns screen enables, clipping and composition.
// Unsupported source modes throw instead of silently substituting a layer.
[[nodiscard]] std::vector<uint32_t> background_tile_payload(
    const simulation::SnesPpuState&,BackgroundLayer,
    const BackgroundTileOptions& = {});
// Logical-coordinate quad (256x224 by default); caller supplies its world transform.
// Disabled main-screen layers produce empty packets, without uploading VRAM.
[[nodiscard]] DrawPacket background_tile_packet(const simulation::SnesPpuState&,
    BackgroundLayer,const BackgroundTileOptions& = {},bool srgb=false);
// Original intro DEMO atlas: its upper 256 rows contain stars only.
[[nodiscard]] DrawPacket intro_star_sphere_packet(const simulation::SnesPpuState&,
    unsigned brightness=15,bool srgb=false,bool full_atlas=false,bool upper_left_only=false,
    bool retain_source_scroll=false);
[[nodiscard]] DrawPacket game_over_star_sphere_packet(const simulation::SnesPpuState&,
    unsigned brightness=15,unsigned colour_subtract=0,bool srgb=false);
// Cut out only exterior black ink; enclosed black facial details stay opaque.
[[nodiscard]] std::vector<SceneVertex> game_over_foreground_vertices(
    const simulation::SnesPpuState&,bool srgb=false);
[[nodiscard]] DrawPacket intro_planet_packet(const simulation::SnesPpuState&,
    unsigned brightness=15,bool srgb=false);
[[nodiscard]] DrawPacket unique_planet_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,const std::array<unsigned,4>& rectangle,bool srgb=false);
[[nodiscard]] Matrix4 intro_planet_motion(int16_t previous_scroll,int16_t current_scroll,
    double alpha,float horizon_y=112);
[[nodiscard]] Matrix4 landscape_pitch_motion(uint16_t previous_pitch,uint16_t current_pitch,double alpha);
[[nodiscard]] Matrix4 landscape_scroll_motion(int16_t previous_scroll,int16_t current_scroll,double alpha,
    uint16_t previous_bank=0,uint16_t current_bank=0,uint16_t atlas_origin=232);
[[nodiscard]] DrawPacket landscape_sphere_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,float horizon_y=112,bool srgb=false,bool unique_half=false,uint16_t atlas_origin=232,bool unique_right_half=false);
// Lower hemisphere becomes a receiver at the grid's camera-relative Y.
// Upper sky/mountains retain the enclosing background projection.
void place_landscape_ground(DrawPacket&,float ground_y,bool gpu=false);
[[nodiscard]] DrawPacket water_surround_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,bool srgb=false);
// Inverse-projected Mode-1 foreground. Height is camera-to-receiver distance.
[[nodiscard]] DrawPacket water_surface_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,float height,bool srgb=false);
[[nodiscard]] Matrix4 water_height_motion(float previous_height,float current_height,double alpha);
// Orbital horizon: preserve the authored vertical band instead of clamping
// its final texture row over a hemisphere. Unique upper rows remain unique.
[[nodiscard]] DrawPacket space_horizon_sphere_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,bool srgb=false);
// Original LSB atlas: stars above the horizon, planet across the entire lower hemisphere.
[[nodiscard]] Matrix4 orbital_horizon_motion(float previous_scroll,float current_scroll,double alpha,
    bool thin_horizon=false,bool entry_horizon=false,bool gameplay=false);
[[nodiscard]] DrawPacket orbital_planet_sphere_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,bool srgb=false,bool thin_horizon=false,bool entry_horizon=false,bool gameplay=false);
// Source Mode-1 title overlays, in painter order after models and before OAM.
// BG1 is omitted for EX's introductory logo backdrop. Black palette entries
// are opaque here: they supply outlines and intentional model occlusion.
[[nodiscard]] std::vector<DrawPacket> title_foreground_packets(
    const simulation::SnesPpuState&,unsigned brightness,bool include_bg1,
    std::optional<std::array<int16_t,2>> bg2_scroll={},bool srgb=false);
}
