#pragma once

#include "starfox/simulation/math.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/timing/fixed_step.hpp"

#include <unordered_map>
#include <limits>
#include <cstdlib>
#include <cmath>
#include <optional>

namespace starfox::render {

struct ObjectPresentationSnapshot {
    timing::TransformSnapshot transform;
    simulation::MatrixQ15 rotation_matrix{};
    std::uint16_t shape{};
    std::uint32_t strategy_address{};
    std::uint8_t type{};
    std::uint64_t generation{};
    std::uint8_t explosion_progress{};
};

// Destruction advances once per source frame. Interpolate only an existing
// entity's advancing counter; never blend a newly recycled slot or a reset.
inline std::optional<double> interpolate_explosion_progress(
    const ObjectPresentationSnapshot* previous,
    const ObjectPresentationSnapshot& current,double alpha) noexcept {
    if(!previous || !current.explosion_progress
        || previous->generation!=current.generation
        || previous->shape!=current.shape
        || previous->strategy_address!=current.strategy_address
        || previous->type!=current.type
        || previous->explosion_progress>current.explosion_progress
        || current.explosion_progress-previous->explosion_progress>4
        || alpha<=0.0 || alpha>=1.0) return std::nullopt;
    return std::lerp(double(previous->explosion_progress),
        double(current.explosion_progress),alpha);
}

using ObjectSnapshotMap = std::unordered_map<simulation::ObjectHandle,
    ObjectPresentationSnapshot>;

// FLASHPLAYER_STRAT is an attached visual, not an independently moving item.
// It alternates NULLSHAPE/wireframe, which must not discard the owner's motion
// history. Preserve the overlay's shape/colour lifetime but share ship poses.
inline bool anchor_player_overlay(ObjectPresentationSnapshot& before,
    ObjectPresentationSnapshot& now,const ObjectSnapshotMap& previous,
    const ObjectSnapshotMap& current,simulation::ObjectHandle player) {
    const auto owner=current.find(player);
    if(owner==current.end()) return false;
    const auto old=previous.find(player);
    const auto& prior=old!=previous.end() && old->second.generation==owner->second.generation
        && old->second.shape==owner->second.shape && old->second.type==owner->second.type
        && old->second.strategy_address==owner->second.strategy_address
        ? old->second : owner->second;
    before.transform=prior.transform;before.rotation_matrix=prior.rotation_matrix;
    now.transform=owner->second.transform;now.rotation_matrix=owner->second.rotation_matrix;
    return true;
}

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
            objects.generation(handle),
            static_cast<std::uint8_t>((object.flags & 0x01U) != 0U ? object.count : 0U)});
    }
    return result;
}

} // namespace starfox::render
