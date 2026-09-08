#pragma once

#include "starfox/simulation/math.hpp"
#include "starfox/simulation/object_pool.hpp"
#include "starfox/timing/fixed_step.hpp"

#include <unordered_map>

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
