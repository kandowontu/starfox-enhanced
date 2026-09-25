#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace starfox::render {
// Append styles so existing saved effect IDs retain their meaning.
enum class Effect : std::uint8_t {
    off, cel_drawn, ink, neon, monochrome, dithered, blueprint, bloom,
    sepia, thermal, night_vision, pastel, comic, vaporwave, posterized, ice, film,
    negative, solarized, amber, emerald, cyanotype, copper, lavender, cga,
    scanlines, crosshatch, metallic, mirror, gold_metal, copper_metal,
    teal_orange, handheld, watercolour, chalk, emboss, bleach_bypass,
    stained_glass, risograph, hologram, mosaic, pencil, oil_paint,
    kaleidoscope, prism_split, pixel_sort, trails, long_exposure,
    shatter, melt, ripple_warp, barrel_warp, venetian, checker_fold,
    prism, glass, obsidian, pearl, twist, ring_ripple, shard_split,
    ruby, jade, porcelain,
    duotone, tritone, woodcut, xray, pop_art, iridescent,
    crt_phosphor, noir, uv_glow, topographic,
    silver, brass, rose_gold, titanium, amethyst, sapphire,
    opal, marble, graphite, molten_glass, count
};
inline constexpr auto effect_count = static_cast<std::uint8_t>(Effect::count);
inline constexpr bool spatial_manipulation(Effect value) {
    return (value>=Effect::kaleidoscope && value<=Effect::pixel_sort)
        || (value>=Effect::shatter && value<=Effect::checker_fold)
        || (value>=Effect::twist && value<=Effect::shard_split);
}
inline constexpr bool manipulation(Effect value) {
    return spatial_manipulation(value) || value==Effect::trails || value==Effect::long_exposure;
}
inline constexpr bool valid_manipulation(unsigned value) {
    return value==0 || (value<effect_count && manipulation(static_cast<Effect>(value)));
}
inline constexpr std::uint8_t next_manipulation(std::uint8_t value,bool backwards) {
    do {value=(value+(backwards?effect_count-1:1))%effect_count;}
    while(!valid_manipulation(value));
    return value;
}
inline constexpr unsigned persistence_mode(Effect value) {
    return value==Effect::trails?1U:value==Effect::long_exposure?2U:0U;
}
inline constexpr bool reflective_material(Effect value) {
    return value==Effect::metallic || value==Effect::mirror
        || value==Effect::gold_metal || value==Effect::copper_metal;
}
inline constexpr bool decorative_material(Effect value) {
    return (value>=Effect::prism && value<=Effect::pearl)
        || (value>=Effect::ruby && value<=Effect::porcelain)
        || (value>=Effect::silver && value<=Effect::molten_glass);
}
inline constexpr bool material(Effect value) { return reflective_material(value)||decorative_material(value); }
inline constexpr bool valid_material(unsigned value) { return value==0 || (value<effect_count && material(static_cast<Effect>(value))); }
inline constexpr std::uint8_t next_material(std::uint8_t value,bool backwards) {
    do {value=(value+(backwards?effect_count-1:1))%effect_count;} while(!valid_material(value));
    return value;
}
// Shared conductor codes for software and GPU shading: 0 dielectric, 1 palette,
// 2 gold, 3 copper. Keep these stable in the GPU material settings buffer.
inline constexpr unsigned conductor(Effect value) {
    return value==Effect::gold_metal || value==Effect::brass?2U:
        value==Effect::copper_metal || value==Effect::rose_gold?3U:
        value==Effect::metallic || value==Effect::silver || value==Effect::titanium?1U:0U;
}
inline constexpr float material_roughness(Effect value) {
    switch(value) {
    case Effect::mirror: return 0.0F;
    case Effect::silver: return 0.12F;
    case Effect::brass: return 0.28F;
    case Effect::rose_gold: return 0.24F;
    case Effect::titanium: return 0.43F;
    case Effect::amethyst: case Effect::sapphire: return 0.08F;
    case Effect::opal: return 0.27F;
    case Effect::marble: return 0.55F;
    case Effect::graphite: return 0.62F;
    case Effect::molten_glass: return 0.04F;
    default: return conductor(value)?0.35F:0.20F;
    }
}
inline constexpr std::uint8_t canonical_effect(std::uint8_t value) {
    if(value==static_cast<std::uint8_t>(Effect::crosshatch)) return 0;
    return value==static_cast<std::uint8_t>(Effect::ice)?static_cast<std::uint8_t>(Effect::cyanotype):value;
}
inline constexpr bool selectable_effect(std::uint8_t value, bool world) {
    return value < effect_count && value != static_cast<std::uint8_t>(Effect::bloom)
        && value != static_cast<std::uint8_t>(Effect::crosshatch)
        && (!world || !reflective_material(static_cast<Effect>(value)))
        && (!world || !decorative_material(static_cast<Effect>(value)))
        && (!world || persistence_mode(static_cast<Effect>(value))==0)
        && value != static_cast<std::uint8_t>(world ? Effect::cel_drawn : Effect::blueprint);
}
// Display order is independent of serialized IDs. Ice remains readable in old
// saves, but its near-identical Cyanotype replaces it in the selector.
inline constexpr std::array effect_order{
    Effect::off,
    Effect::cel_drawn, Effect::comic, Effect::ink, Effect::pastel, Effect::posterized,
    Effect::watercolour, Effect::chalk, Effect::emboss,
    Effect::stained_glass, Effect::risograph, Effect::mosaic, Effect::pencil, Effect::oil_paint,
    Effect::dithered, Effect::cga, Effect::handheld, Effect::scanlines, Effect::film,
    Effect::monochrome, Effect::sepia, Effect::amber, Effect::emerald, Effect::cyanotype,
    Effect::copper, Effect::lavender, Effect::teal_orange, Effect::bleach_bypass,
    Effect::neon, Effect::vaporwave, Effect::blueprint, Effect::thermal, Effect::night_vision,
    Effect::hologram,
    Effect::negative, Effect::solarized,
    Effect::duotone, Effect::tritone, Effect::woodcut, Effect::xray, Effect::pop_art,
    Effect::iridescent, Effect::crt_phosphor, Effect::noir, Effect::uv_glow, Effect::topographic,
    Effect::kaleidoscope, Effect::prism_split, Effect::pixel_sort,
    Effect::trails, Effect::long_exposure,
    Effect::shatter, Effect::melt, Effect::ripple_warp, Effect::barrel_warp, Effect::venetian, Effect::checker_fold,
    Effect::twist, Effect::ring_ripple, Effect::shard_split,
    Effect::metallic, Effect::gold_metal, Effect::copper_metal, Effect::mirror,
    Effect::prism, Effect::glass, Effect::obsidian, Effect::pearl, Effect::ruby, Effect::jade, Effect::porcelain,
    Effect::silver, Effect::brass, Effect::rose_gold, Effect::titanium, Effect::amethyst,
    Effect::sapphire, Effect::opal, Effect::marble, Effect::graphite, Effect::molten_glass};
inline constexpr std::string_view effect_group(std::uint8_t value) {
    const auto effect=static_cast<Effect>(canonical_effect(value));
    if(effect==Effect::off) return "EFFECTS OFF";
    if(reflective_material(effect)) return "REFLECTIVE MATERIALS";
    if(decorative_material(effect)) return "MODEL MATERIALS";
    if(spatial_manipulation(effect) || persistence_mode(effect)) return "MANIPULATIONS";
    switch(effect) {
    case Effect::cel_drawn: case Effect::comic: case Effect::ink: case Effect::pastel:
    case Effect::posterized: case Effect::watercolour: case Effect::chalk: case Effect::emboss:
    case Effect::stained_glass: case Effect::risograph: case Effect::mosaic: case Effect::pencil: case Effect::oil_paint:
    case Effect::woodcut: case Effect::pop_art:
        return "DRAWN EFFECTS";
    case Effect::dithered: case Effect::cga: case Effect::handheld: case Effect::scanlines: case Effect::film:
    case Effect::crt_phosphor:
        return "RETRO EFFECTS";
    case Effect::neon: case Effect::vaporwave: case Effect::blueprint: case Effect::thermal:
    case Effect::night_vision: case Effect::negative: case Effect::solarized:
    case Effect::hologram: case Effect::xray: case Effect::iridescent: case Effect::uv_glow:
        return "STYLIZED EFFECTS";
    default: return "COLOUR EFFECTS";
    }
}
inline constexpr std::uint8_t next_effect(std::uint8_t value, bool world, bool backwards) {
    value=canonical_effect(value);
    unsigned index=0;
    for(unsigned i=0;i<effect_order.size();++i) if(static_cast<std::uint8_t>(effect_order[i])==value) index=i;
    do { index=(index+(backwards?effect_order.size()-1:1))%effect_order.size();
         value=static_cast<std::uint8_t>(effect_order[index]); }
    while(!selectable_effect(value,world) || (!world && (manipulation(static_cast<Effect>(value)) || material(static_cast<Effect>(value)))));
    return value;
}
inline constexpr std::array<std::string_view, 4> bloom_names{"OFF", "LOW", "MEDIUM", "HEAVY"};
inline constexpr std::array<std::string_view, effect_count> effect_names{
    "OFF", "CEL-DRAWN", "INK", "NEON", "MONOCHROME", "DITHERED", "BLUEPRINT", "BLOOM",
    "SEPIA", "THERMAL", "NIGHT VISION", "PASTEL", "COMIC", "VAPORWAVE", "POSTERIZED", "ICE", "FILM",
    "NEGATIVE", "SOLARIZED", "AMBER", "EMERALD", "CYANOTYPE", "COPPER TONE", "LAVENDER", "CGA",
    "SCANLINES", "CROSSHATCH", "METALLIC", "MIRROR", "GOLD METAL", "COPPER METAL",
    "TEAL/ORANGE", "HANDHELD", "WATERCOLOUR", "CHALK", "EMBOSS", "BLEACH BYPASS",
    "STAINED GLASS", "RISOGRAPH", "HOLOGRAM", "MOSAIC", "PENCIL", "OIL PAINT",
    "KALEIDOSCOPE", "PRISM SPLIT", "PIXEL SORT", "TRAILS", "LONG EXPOSURE",
    "SHATTER", "MELT", "RIPPLE WARP", "BARREL WARP", "VENETIAN", "CHECKER FOLD",
    "PRISM", "GLASS", "OBSIDIAN", "PEARL", "TWIST", "RING RIPPLE", "SHARD SPLIT",
    "RUBY", "JADE", "PORCELAIN",
    "DUOTONE", "TRITONE", "WOODCUT", "X-RAY", "POP ART", "IRIDESCENT",
    "CRT PHOSPHOR", "NOIR", "UV GLOW", "TOPOGRAPHIC",
    "SILVER", "BRASS", "ROSE GOLD", "TITANIUM", "AMETHYST", "SAPPHIRE",
    "OPAL", "MARBLE", "GRAPHITE", "MOLTEN GLASS"};
}
