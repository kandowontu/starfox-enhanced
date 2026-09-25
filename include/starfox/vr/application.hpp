#pragma once
#include "starfox/vr/openxr_runtime.hpp"
#include "starfox/vr/openxr_input.hpp"
#include <chrono>
#include <functional>
#include <filesystem>
namespace starfox::vr {
// Callbacks run on the rendering thread. Retain Android JNI global references
// until run_application returns. Zero frame/time limits mean unlimited.
struct ApplicationHost {
    const AndroidXrContext* android{};
    unsigned frame_limit{120};
    std::chrono::seconds time_limit{30};
    std::function<bool()> stop_requested;
    // Optional desktop gamepad input, sampled once per stereo frame.
    std::function<VrControls()> desktop_controls;
    std::filesystem::path cartridge_save_path;
};
// Shared experimental loop. Full game presentation parity remains incomplete.
int run_application(int argc, char** argv, const ApplicationHost& host = {});
}
