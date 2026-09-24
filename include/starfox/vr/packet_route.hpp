#pragma once
#include "starfox/vr/shape_batch.hpp"
#include <algorithm>
namespace starfox::vr {
// Diagnostic producer inventory, not GPU execution or shadow-caster coverage.
enum class PacketRoute { empty, sprite, particle, text, grid, dust, cpu_connected_grid, ordinary, procedural, connected_grid, count };
inline const char* packet_route_name(PacketRoute route) {
    constexpr const char* names[]{"empty", "gpu-whole-object-sprite", "gpu-particle",
        "gpu-scaled-text", "gpu-grid", "gpu-dust", "cpu-connected-grid",
        "ordinary-legacy", "procedural-legacy", "gpu-connected-grid"};
    return names[static_cast<unsigned>(route)];
}
inline PacketRoute packet_route(std::span<const SceneVertex> triangles,std::span<const SceneVertex> lines) {
    if(triangles.empty() && lines.empty()) return PacketRoute::empty;
    const auto all=[&](auto predicate) {
        return std::all_of(triangles.begin(),triangles.end(),predicate)
            && std::all_of(lines.begin(),lines.end(),predicate);
    };
    const auto flags=[&](uint32_t value,uint32_t optional=2U) {
        return all([&](const auto& v){return (v.texture[3]&~optional)==value;});
    };
    if(flags(0x28000005U)) return PacketRoute::sprite;
    if(flags(0x80000000U,6U)) return PacketRoute::particle;
    if(flags(134217728U|1028U)) return PacketRoute::text;
    if(flags(68U,0U)) return PacketRoute::grid;
    if(flags(132U,2048U|16384U)) return PacketRoute::dust;
    if(flags(512U,0U)) return PacketRoute::cpu_connected_grid;
    if(flags(512U|4194304U,0U)) return PacketRoute::connected_grid;
    if(all([](const auto& v){return v.visibility_enabled!=2 && !(v.texture[3]&~0x28000007U);}))
        return PacketRoute::ordinary;
    return PacketRoute::procedural;
}
}
