#pragma once

#include "starfox/simulation/math.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/timing/fixed_step.hpp"

#include <unordered_map>
#include <limits>
#include <cstdlib>

namespace starfox::render {

struct ObjectPresentationSnapshot {
    timing::TransformSnapshot transform;
    simulation::MatrixQ15 rotation_matrix{};
    std::uint16_t shape{};
    std::uint32_t strategy_address{};
    std::uint8_t type{};
    std::uint64_t generation{};
};

using ObjectSnapshotMap = std::unordered_map<simulation::ObjectHandle,
    ObjectPresentationSnapshot>;

// UPDOORCOL_ISTRAT adds deg180 in one source tick to change the arrow's
// direction. That is a discrete state change, not a rotating-door animation.
inline simulation::MatrixQ15 interpolate_object_rotation(
    const ObjectPresentationSnapshot& previous,
    const ObjectPresentationSnapshot& current, double alpha,
    std::uint16_t discrete_rotation_shape) {
    if (discrete_rotation_shape != 0U && current.shape == discrete_rotation_shape)
        return current.rotation_matrix;
    return simulation::interpolate_rotation_matrix_q15(
        previous.rotation_matrix, current.rotation_matrix, alpha);
}

// EX implements the sight line as recycled, advancing particles. Presentation
// must match the sight's depth stations, not the particle that moves from one
// station to the next during a native tick.
inline const ObjectPresentationSnapshot* reticle_previous_snapshot(
    const ObjectPresentationSnapshot& sight, const ObjectSnapshotMap& current,
    const ObjectSnapshotMap& previous, simulation::ObjectHandle player) {
    const auto owner = current.find(player), old_owner = previous.find(player);
    if (owner == current.end() || old_owner == previous.end()) return nullptr;
    const auto radius = [](const timing::TransformSnapshot& a,
                           const timing::TransformSnapshot& b) {
        const auto dx = static_cast<std::int64_t>(simulation::wrap16(a.x-b.x));
        const auto dy = static_cast<std::int64_t>(simulation::wrap16(a.y-b.y));
        const auto dz = static_cast<std::int64_t>(simulation::wrap16(a.z-b.z));
        return dx*dx + dy*dy + dz*dz;
    };
    const auto target = radius(sight.transform, owner->second.transform);
    const ObjectPresentationSnapshot* best = nullptr;
    auto distance = std::numeric_limits<std::int64_t>::max();
    for (const auto& [handle, candidate] : previous) {
        if (candidate.strategy_address != sight.strategy_address
            || candidate.shape != sight.shape || candidate.type != sight.type) continue;
        const auto difference = std::abs(radius(candidate.transform,
            old_owner->second.transform) - target);
        if (difference < distance) { best = &candidate; distance = difference; }
    }
    return best;
}

inline ObjectSnapshotMap capture_object_snapshots(
    const simulation::ObjectPool& objects, const simulation::TrigTables& trig) {
    ObjectSnapshotMap result;
    for (const auto handle : objects.active_handles()) {
        const auto& object = objects.at(handle);
        // Invisible strategies can leave placeholder coordinates until their
        // first visible update (notably Attack Carrier's child components).
        // They are not valid interpolation endpoints. Reappearing objects
        // start a new visible history without changing native simulation.
        if ((object.strategy_flags[3] & 0x08U) != 0U) continue;
        const auto transform = timing::TransformSnapshot{
            object.world_x, object.world_y, object.world_z,
            static_cast<std::uint16_t>(object.rotation_x << 8U),
            static_cast<std::uint16_t>(object.rotation_y << 8U),
            static_cast<std::uint16_t>(object.rotation_z << 8U)};
        const auto matrix = simulation::transpose_q15(
            simulation::rotation_matrix_q15(trig,
                simulation::wrap16(-static_cast<std::int32_t>(transform.pitch)),
                simulation::wrap16(-static_cast<std::int32_t>(transform.yaw)),
                simulation::wrap16(-static_cast<std::int32_t>(transform.roll))));
        result.emplace(handle, ObjectPresentationSnapshot{transform, matrix,
            object.shape, object.strategy_address, object.type,
            objects.generation(handle)});
    }
    return result;
}

} // namespace starfox::render
