cmake_minimum_required(VERSION 3.22)

project(starfox_enhanced VERSION 0.0.4 LANGUAGES CXX)

if(WINDOWS_STORE)
    # Apply the UWP ABI to the entire dependency graph, not only the SDL entry
    # point. The redistributable UWP framework still uses the pre-17.10 mutex
    # constructor ABI; ask it to initialize mutexes instead of inlining the
    # newer desktop runtime's layout.
    add_compile_definitions(WINAPI_FAMILY=WINAPI_FAMILY_APP
        _CRT_APP=1 _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR=1)
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:/Zi>")
    # Ninja links with the caller's LIB environment. A later incremental build
    # from a desktop shell must not silently replace *_app.dll imports with
    # desktop CRT imports (which fail before WinMain on Xbox).
    get_filename_component(STARFOX_MSVC_X64_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(STARFOX_MSVC_ROOT "${STARFOX_MSVC_X64_BIN}/../../.." ABSOLUTE)
    set(STARFOX_UWP_CRT_LIB_DIR "${STARFOX_MSVC_ROOT}/lib/x64/store")
    if(NOT EXISTS "${STARFOX_UWP_CRT_LIB_DIR}/msvcprt.lib")
        message(FATAL_ERROR "The MSVC x64 UWP/Store libraries are required")
    endif()
    link_directories(BEFORE "${STARFOX_UWP_CRT_LIB_DIR}")
endif()

# devkitPro's current toolchain identifies Horizon as NintendoSwitch but does
# not define the legacy SWITCH convenience variable used by projects and SDL.
if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    set(SWITCH TRUE)
endif()

if(WINDOWS_STORE OR ANDROID OR IOS OR SWITCH OR VITA)
    set(STARFOX_BUILD_TOOLS_DEFAULT OFF)
else()
    set(STARFOX_BUILD_TOOLS_DEFAULT ON)
endif()

option(STARFOX_BUILD_TESTS "Build deterministic timing tests" ON)
option(STARFOX_BUILD_RUNTIME "Build the SDL3 native PC runtime" ON)
option(STARFOX_BUILD_TOOLS "Build host-side trace and asset tools"
    ${STARFOX_BUILD_TOOLS_DEFAULT})
option(STARFOX_EMBED_RUNTIME_ASSETS
    "Embed source-built BPS deltas and symbol tables into the Windows runtime" ON)
option(STARFOX_PACKAGE_MSU1_MUSIC
    "Build and install the optional Starfox-MSU1.PAK companion" ON)
set(STARFOX_UWP_PACKAGE_CERTIFICATE "" CACHE FILEPATH
    "Development PFX used to sign the Xbox UWP package")
set(STARFOX_ROM_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
    CACHE FILEPATH "Assembled UltraStarFox ROM used by the native runtime")
set(STARFOX_SYMBOLS_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/symbols/ultrastarfox.txt"
    CACHE FILEPATH "Pinned UltraStarFox symbol table used by the native runtime")
set(STARFOX_EX_ROM_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-star-fox-ex/SFES/SFES.SFC"
    CACHE FILEPATH "Assembled Star Fox EX ROM used by the native runtime")
set(STARFOX_EX_SYMBOLS_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/symbols/starfox-ex.txt"
    CACHE FILEPATH "Pinned Star Fox EX symbol table used by the native runtime")
set(STARFOX_ORIGINAL_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/ultrastarfox-v12.bps"
    CACHE FILEPATH "UltraStarFox BPS delta for a clean Star Fox USA v1.2 ROM")
set(STARFOX_EX_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/starfox-ex-v12.bps"
    CACHE FILEPATH "Star Fox EX BPS delta for a clean Star Fox USA v1.2 ROM")
if(WIN32)
    set(STARFOX_RETAIL_ROM_DIR "C:/NTSC-US Super Nintendo System Roms"
        CACHE PATH "Directory searched for clean retail validation ROMs")
else()
    set(STARFOX_RETAIL_ROM_DIR "$ENV{HOME}/NTSC-US Super Nintendo System Roms"
        CACHE PATH "Directory searched for clean retail validation ROMs")
endif()
set(STARFOX_RETAIL_V12_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Star Fox (USA) (Rev 2).sfc"
    CACHE FILEPATH "Clean Star Fox USA v1.2 ROM used only for local validation")
set(STARFOX_RETAIL_JAPAN_V10_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Star Fox (Japan).sfc")
set(STARFOX_RETAIL_JAPAN_V11_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Star Fox (Japan) (Rev 1).sfc")
set(STARFOX_RETAIL_USA_V10_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Star Fox (USA).sfc")
set(STARFOX_RETAIL_USA_V11_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Star Fox (USA) (Rev 1).sfc")
set(STARFOX_RETAIL_EUROPE_V10_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Starwing (Europe).sfc")
set(STARFOX_RETAIL_EUROPE_V11_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Starwing (Europe) (Rev 1).sfc")
set(STARFOX_RETAIL_GERMANY_V10_FILE
    "${STARFOX_RETAIL_ROM_DIR}/Starwing (Germany).sfc")
set(STARFOX_RETAIL_JAPAN_V10_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-japan-v10-to-usa-v12.bps")
set(STARFOX_RETAIL_JAPAN_V11_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-japan-v11-to-usa-v12.bps")
set(STARFOX_RETAIL_USA_V10_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-usa-v10-to-v12.bps")
set(STARFOX_RETAIL_USA_V11_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-usa-v11-to-v12.bps")
set(STARFOX_RETAIL_EUROPE_V10_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-europe-v10-to-usa-v12.bps")
set(STARFOX_RETAIL_EUROPE_V11_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-europe-v11-to-usa-v12.bps")
set(STARFOX_RETAIL_GERMANY_V10_PATCH_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/patches/retail-germany-v10-to-usa-v12.bps")

include(FetchContent)
FetchContent_Declare(
    retro_cpu
    GIT_REPOSITORY https://github.com/achaulk/retro_cpu.git
    GIT_TAG ea9049ab25084334f7cc1907b3a98bf1c2604a03
    SOURCE_SUBDIR _starfox_no_upstream_project
)
FetchContent_MakeAvailable(retro_cpu)

# Retain the pinned dependency, with the independently audited native-mode
# arithmetic, bus-order and instruction-timing corrections in a reviewable patch.
find_package(Git REQUIRED)
set(retro_cpu_patch_files)
foreach(retro_cpu_patch IN ITEMS retro-cpu-parity.patch retro-cpu-interrupts.patch retro-cpu-direct-index.patch retro-cpu-bus-clock.patch retro-cpu-bus-order.patch retro-cpu-interrupt-status.patch retro-cpu-last-cycle.patch retro-cpu-halt.patch)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${retro_cpu_patch}")
    list(APPEND retro_cpu_patch_files "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${retro_cpu_patch}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_retro_cpu_patch_series.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    "-DSOURCE_DIR=${retro_cpu_SOURCE_DIR}"
    "-DCHECK_DIR=${CMAKE_CURRENT_BINARY_DIR}/_retro_cpu_patch_check"
    "-DGIT_REVISION=ea9049ab25084334f7cc1907b3a98bf1c2604a03"
    "-DPATCH_FILES=${retro_cpu_patch_files}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_retro_cpu_patch_series.cmake"
    RESULT_VARIABLE retro_cpu_patch_result)
if(NOT retro_cpu_patch_result EQUAL 0)
    message(FATAL_ERROR "Could not validate/apply the pinned RetroCPU patch series")
endif()

FetchContent_Declare(
    snes_spc
    GIT_REPOSITORY https://github.com/blarggs-audio-libraries/snes_spc.git
    GIT_TAG ec8ee2bbe30451614c1d02a83f7af1c97d497d45
    SOURCE_SUBDIR _starfox_no_upstream_project
)
FetchContent_MakeAvailable(snes_spc)

FetchContent_Declare(
    dr_libs
    GIT_REPOSITORY https://github.com/mackron/dr_libs.git
    GIT_TAG b55a0d9a30b91ad8901f89ecf05f76a33186c185
    SOURCE_SUBDIR _starfox_no_upstream_project
)
FetchContent_MakeAvailable(dr_libs)

add_library(snes_spc_core STATIC
    "${snes_spc_SOURCE_DIR}/snes_spc/spc.cpp"
    "${snes_spc_SOURCE_DIR}/snes_spc/SNES_SPC.cpp"
    "${snes_spc_SOURCE_DIR}/snes_spc/SNES_SPC_misc.cpp"
    "${snes_spc_SOURCE_DIR}/snes_spc/SNES_SPC_state.cpp"
    "${snes_spc_SOURCE_DIR}/snes_spc/SPC_DSP.cpp"
    "${snes_spc_SOURCE_DIR}/snes_spc/SPC_Filter.cpp"
)
target_include_directories(snes_spc_core SYSTEM PUBLIC
    "${snes_spc_SOURCE_DIR}/snes_spc"
)

# RetroCPU's top-level build includes an unused legacy host layer that does
# not compile on modern MinGW. Build only the MIT CPU/bus sources we use.
add_library(retro_cpu_core STATIC
    "${retro_cpu_SOURCE_DIR}/cpu.cc"
)
target_include_directories(retro_cpu_core SYSTEM PUBLIC "${retro_cpu_SOURCE_DIR}")
target_compile_features(retro_cpu_core PUBLIC cxx_std_17)
add_library(retro_cpu_65816 STATIC
    "${retro_cpu_SOURCE_DIR}/cpu/65816/cpu_65c816.cc"
)
target_include_directories(retro_cpu_65816 SYSTEM PUBLIC "${retro_cpu_SOURCE_DIR}")
target_include_directories(retro_cpu_65816 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(retro_cpu_65816 PUBLIC retro_cpu_core)
target_compile_features(retro_cpu_65816 PUBLIC cxx_std_17)
if(MINGW)
    target_compile_definitions(retro_cpu_core PUBLIC __FUNCSIG__=__PRETTY_FUNCTION__)
    target_compile_definitions(retro_cpu_65816 PUBLIC __FUNCSIG__=__PRETTY_FUNCTION__)
endif()

add_library(starfox_core
    src/audio/msu1_audio.cpp
    src/audio/msu1_pack.cpp
    src/audio/spc700_audio.cpp
    src/audio/native_audio_clock.cpp
    src/audio/game_audio_timeline.cpp
    src/audio/msu_audio_timeline.cpp
    src/assets/bps.cpp
    src/assets/decrunch.cpp
    src/assets/rom.cpp
    src/assets/runtime_bundle.cpp
    src/assets/shape_decoder.cpp
    src/render/background_renderer.cpp
    src/render/dust_renderer.cpp
    src/render/framebuffer.cpp
    src/render/palette.cpp
    src/render/particle_renderer.cpp
    src/render/presentation_history.cpp
    src/render/scaled_text_renderer.cpp
    src/render/software_renderer.cpp
    src/render/sprite_renderer.cpp
    src/simulation/math.cpp
    src/simulation/dust_system.cpp
    src/simulation/game_simulation.cpp
    src/simulation/native_transfer.cpp
    src/simulation/gsu_device.cpp
    src/simulation/map_vm.cpp
    src/simulation/object_pool.cpp
    src/simulation/particle_system.cpp
    src/simulation/path_vm.cpp
    src/simulation/prng.cpp
    src/simulation/strategy_scheduler.cpp
    src/simulation/wdc65816.cpp
    src/timing/fixed_step.cpp
)

target_include_directories(starfox_core PUBLIC include)
target_include_directories(starfox_core SYSTEM PRIVATE "${dr_libs_SOURCE_DIR}")
target_compile_features(starfox_core PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(starfox_core PRIVATE
    retro_cpu_65816 snes_spc_core Threads::Threads)

if(MSVC)
    target_compile_options(starfox_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(starfox_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(STARFOX_BUILD_TOOLS)
    set(STARFOX_REFERENCE_CORE_DIR "" CACHE PATH
        "Optional local Snes9x checkout for the Windows cartridge-render audit")
    if(WIN32 AND EXISTS "${STARFOX_REFERENCE_CORE_DIR}/libretro/libretro.h")
        add_executable(starfox_reference_render tools/reference/render_audit.cpp)
        target_include_directories(starfox_reference_render PRIVATE
            "${STARFOX_REFERENCE_CORE_DIR}/libretro")
        target_link_libraries(starfox_reference_render PRIVATE starfox_core)
        set(STARFOX_REFERENCE_ARES_DIR "" CACHE PATH
            "Optional pinned Ares checkout for isolated GSU cycle measurements")
        if(EXISTS "${STARFOX_REFERENCE_ARES_DIR}/ares/component/processor/gsu/gsu.hpp")
            add_library(starfox_reference_ares STATIC tools/reference/ares_gsu.cpp)
            add_executable(starfox_live_game_timing tools/reference/live_game_timing.cpp)
            target_link_libraries(starfox_live_game_timing PRIVATE starfox_core)
            target_include_directories(starfox_reference_ares SYSTEM PRIVATE
                "${STARFOX_REFERENCE_ARES_DIR}" "${STARFOX_REFERENCE_ARES_DIR}/nall")
            target_compile_features(starfox_reference_ares PRIVATE cxx_std_20)
            target_link_libraries(starfox_reference_ares PRIVATE starfox_core)
            add_executable(starfox_reference_ares_render tools/reference/render_audit.cpp)
            target_include_directories(starfox_reference_ares_render PRIVATE
                "${STARFOX_REFERENCE_CORE_DIR}/libretro")
            target_compile_definitions(starfox_reference_ares_render PRIVATE STARFOX_REFERENCE_ARES=1)
            target_link_libraries(starfox_reference_ares_render PRIVATE starfox_core starfox_reference_ares)
            add_executable(starfox_reference_ares_tests tools/reference/ares_gsu_tests.cpp)
            target_compile_features(starfox_reference_ares_tests PRIVATE cxx_std_20)
            target_link_libraries(starfox_reference_ares_tests PRIVATE starfox_reference_ares)
            add_executable(starfox_reference_gsu_device_tests tools/reference/gsu_device_audit.cpp)
            target_link_libraries(starfox_reference_gsu_device_tests PRIVATE starfox_core starfox_reference_ares)
            add_executable(starfox_reference_view tools/reference/view_audit.cpp)
            target_link_libraries(starfox_reference_view PRIVATE starfox_core starfox_reference_ares)
            if(EXISTS "${STARFOX_REFERENCE_ARES_DIR}/ares/component/processor/wdc65816/wdc65816.hpp")
                add_library(starfox_reference_cpu STATIC tools/reference/ares_cpu.cpp)
                target_include_directories(starfox_reference_cpu SYSTEM PRIVATE
                    "${STARFOX_REFERENCE_ARES_DIR}" "${STARFOX_REFERENCE_ARES_DIR}/nall")
                target_link_libraries(starfox_reference_cpu PRIVATE starfox_core)
                add_executable(starfox_reference_cpu_tests tools/reference/cpu_audit.cpp)
                target_link_libraries(starfox_reference_cpu_tests PRIVATE starfox_core starfox_reference_cpu)
                add_executable(starfox_reference_interrupt_tests tools/reference/interrupt_audit.cpp)
                target_link_libraries(starfox_reference_interrupt_tests PRIVATE starfox_core starfox_reference_cpu)
                add_executable(starfox_reference_halt_tests tools/reference/halt_audit.cpp)
                target_link_libraries(starfox_reference_halt_tests PRIVATE starfox_core starfox_reference_cpu)
                if(EXISTS "${STARFOX_REFERENCE_ARES_DIR}/ares/sfc/cpu/irq.cpp")
                    file(READ "${STARFOX_REFERENCE_ARES_DIR}/ares/sfc/cpu/timing.cpp" dma_timing_source)
                    string(FIND "${dma_timing_source}" "alwaysinline auto CPU::dmaEdge() -> void {" dma_edge_begin)
                    string(FIND "${dma_timing_source}" "//called every 128 clocks" dma_edge_end)
                    if(dma_edge_begin LESS 0 OR dma_edge_end LESS dma_edge_begin)
                        message(FATAL_ERROR "Pinned DMA edge source boundaries were not found")
                    endif()
                    math(EXPR dma_edge_length "${dma_edge_end} - ${dma_edge_begin}")
                    string(SUBSTRING "${dma_timing_source}" ${dma_edge_begin} ${dma_edge_length} dma_edge_source)
                    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/reference-dma-edge.inl" "${dma_edge_source}")
                    string(FIND "${dma_timing_source}" "  if(!status.hdmaSetupTriggered" hdma_events_start)
                    string(FIND "${dma_timing_source}" "  Thread::step(clocks);" hdma_events_end)
                    if(hdma_events_start LESS 0 OR hdma_events_end LESS hdma_events_start)
                        message(FATAL_ERROR "Pinned HDMA event source boundaries were not found")
                    endif()
                    math(EXPR hdma_events_length "${hdma_events_end} - ${hdma_events_start}")
                    string(SUBSTRING "${dma_timing_source}" ${hdma_events_start} ${hdma_events_length} hdma_events_source)
                    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/reference-hdma-events.inl" "${hdma_events_source}")
                    string(FIND "${dma_timing_source}" "  if(vcounter() == 0) {" hdma_scanline_start)
                    string(FIND "${dma_timing_source}" "alwaysinline auto CPU::aluEdge()" hdma_scanline_end)
                    if(hdma_scanline_start LESS 0 OR hdma_scanline_end LESS hdma_scanline_start)
                        message(FATAL_ERROR "Pinned HDMA scanline source boundaries were not found")
                    endif()
                    # Omit only the enclosing function's final brace and blank line.
                    math(EXPR hdma_scanline_length "${hdma_scanline_end} - ${hdma_scanline_start} - 3")
                    string(SUBSTRING "${dma_timing_source}" ${hdma_scanline_start} ${hdma_scanline_length} hdma_scanline_source)
                    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/reference-hdma-scanline.inl" "${hdma_scanline_source}")
                    foreach(part IN ITEMS step scanline)
                        string(FIND "${dma_timing_source}" "auto CPU::${part}(" cpu_part_start)
                        if(part STREQUAL "step")
                            string(FIND "${dma_timing_source}" "//called by ppu.tick()" cpu_part_end)
                        else()
                            string(FIND "${dma_timing_source}" "alwaysinline auto CPU::aluEdge()" cpu_part_end)
                        endif()
                        if(cpu_part_start LESS 0 OR cpu_part_end LESS cpu_part_start)
                            message(FATAL_ERROR "Pinned CPU ${part} source boundaries were not found")
                        endif()
                        math(EXPR cpu_part_length "${cpu_part_end} - ${cpu_part_start}")
                        string(SUBSTRING "${dma_timing_source}" ${cpu_part_start} ${cpu_part_length} cpu_part_source)
                        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/reference-cpu-${part}.inl" "${cpu_part_source}")
                    endforeach()
                    file(READ "${STARFOX_REFERENCE_ARES_DIR}/ares/component/processor/gsu/gsu.cpp" gsu_power_source)
                    string(FIND "${gsu_power_source}" "auto GSU::power() -> void {" gsu_power_start)
                    if(gsu_power_start LESS 0)
                        message(FATAL_ERROR "Pinned GSU power source boundary was not found")
                    endif()
                    string(SUBSTRING "${gsu_power_source}" ${gsu_power_start} -1 gsu_power_source)
                    # The final brace closes namespace ares, supplied by the harness.
                    string(REGEX REPLACE "[\r\n]+}[\r\n]*$" "\n" gsu_power_source "${gsu_power_source}")
                    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/reference-gsu-power.inl" "${gsu_power_source}")
                    enable_language(C)
                    add_executable(starfox_reference_gsu_overlap_tests tools/reference/gsu_overlap_audit.cpp
                        "${STARFOX_REFERENCE_ARES_DIR}/libco/libco.c")
                    target_include_directories(starfox_reference_gsu_overlap_tests SYSTEM PRIVATE
                        "${STARFOX_REFERENCE_ARES_DIR}" "${STARFOX_REFERENCE_ARES_DIR}/nall" "${CMAKE_CURRENT_BINARY_DIR}")
                    target_link_libraries(starfox_reference_gsu_overlap_tests PRIVATE starfox_core)
                    add_executable(starfox_reference_dma_tests tools/reference/dma_audit.cpp)
                    target_include_directories(starfox_reference_dma_tests SYSTEM PRIVATE
                        "${STARFOX_REFERENCE_ARES_DIR}" "${STARFOX_REFERENCE_ARES_DIR}/nall" "${CMAKE_CURRENT_BINARY_DIR}")
                    target_link_libraries(starfox_reference_dma_tests PRIVATE starfox_core)
                    add_executable(starfox_reference_timer_tests tools/reference/timer_audit.cpp)
                    target_include_directories(starfox_reference_timer_tests SYSTEM PRIVATE
                        "${STARFOX_REFERENCE_ARES_DIR}" "${STARFOX_REFERENCE_ARES_DIR}/nall")
                    target_link_libraries(starfox_reference_timer_tests PRIVATE starfox_core)
                    add_executable(starfox_reference_raster_tests tools/reference/raster_audit.cpp)
                    target_include_directories(starfox_reference_raster_tests SYSTEM PRIVATE
                        "${STARFOX_REFERENCE_ARES_DIR}" "${STARFOX_REFERENCE_ARES_DIR}/nall")
                    target_link_libraries(starfox_reference_raster_tests PRIVATE starfox_core)
                endif()
            endif()
        endif()
    endif()

    add_executable(starfox_timing_demo src/app/timing_demo.cpp)
    target_link_libraries(starfox_timing_demo PRIVATE starfox_core)

    add_executable(starfox_shape_preview src/app/shape_preview.cpp)
    target_link_libraries(starfox_shape_preview PRIVATE starfox_core)

    add_executable(starfox_shape_coverage src/app/shape_coverage.cpp)
    target_link_libraries(starfox_shape_coverage PRIVATE starfox_core)

    add_executable(starfox_map_trace src/app/map_trace.cpp)
    target_link_libraries(starfox_map_trace PRIVATE starfox_core)

    add_executable(starfox_strategy_trace src/app/strategy_trace.cpp)
    target_link_libraries(starfox_strategy_trace PRIVATE starfox_core)

    add_executable(starfox_game_init_trace src/app/game_init_trace.cpp)
    target_link_libraries(starfox_game_init_trace PRIVATE starfox_core)

    add_executable(starfox_stage_trace src/app/stage_trace.cpp)
    target_link_libraries(starfox_stage_trace PRIVATE starfox_core)

    add_executable(starfox_stage_preview src/app/stage_preview.cpp)
    target_link_libraries(starfox_stage_preview PRIVATE starfox_core)

    add_executable(starfox_planet_probe src/app/planet_probe.cpp)
    target_link_libraries(starfox_planet_probe PRIVATE starfox_core)
endif()

if(STARFOX_BUILD_RUNTIME)
    if(ANDROID AND NOT TARGET SDL3::SDL3)
        # The Android application consumes SDL's official AAR. Its Prefab
        # package supplies both headers and libSDL3.so to this CMake build.
        find_package(SDL3 REQUIRED CONFIG)
    endif()
    if(NOT TARGET SDL3::SDL3)
        set(SDL_SHARED OFF CACHE BOOL "Avoid a runtime SDL DLL" FORCE)
        set(SDL_STATIC ON CACHE BOOL "Link SDL into the native executable" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build the SDL test library" FORCE)
        if(SWITCH)
            # Public SDL3 does not ship its NDA-gated Nintendo backend. Apply a
            # pinned, zlib-licensed libnx backend for the open devkitPro build.
            # Both the backend revision and its bytes are fixed here so the
            # console build remains reproducible and cannot drift silently.
            set(STARFOX_SDL3_SWITCH_PATCH
                "${CMAKE_CURRENT_BINARY_DIR}/downloads/sdl3-switch.patch")
            file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/downloads")
            file(DOWNLOAD
                "https://raw.githubusercontent.com/neomody77/sdl3-switch/182e511214d7600e4bdab8606d7caf0ef744afd6/sdl3-switch.patch"
                "${STARFOX_SDL3_SWITCH_PATCH}"
                EXPECTED_HASH
                    "SHA256=9f694599723d78b08c09d99f8203ddea52ee5178575a807263fe79f8ab1f28dc"
                TLS_VERIFY ON
                STATUS STARFOX_SDL3_SWITCH_PATCH_STATUS)
            list(GET STARFOX_SDL3_SWITCH_PATCH_STATUS 0
                STARFOX_SDL3_SWITCH_PATCH_RESULT)
            if(NOT STARFOX_SDL3_SWITCH_PATCH_RESULT EQUAL 0)
                list(GET STARFOX_SDL3_SWITCH_PATCH_STATUS 1
                    STARFOX_SDL3_SWITCH_PATCH_ERROR)
                message(FATAL_ERROR
                    "Unable to download the pinned SDL3 Switch backend: "
                    "${STARFOX_SDL3_SWITCH_PATCH_ERROR}")
            endif()
            find_package(Git REQUIRED)
            set(STARFOX_SDL3_PATCH_COMMAND
                PATCH_COMMAND "${GIT_EXECUTABLE}" apply
                    --whitespace=nowarn -p3
                    "${STARFOX_SDL3_SWITCH_PATCH}")
        else()
            set(STARFOX_SDL3_PATCH_COMMAND "")
        endif()
        if(WINDOWS_STORE)
            # Upstream SDL dropped UWP. This maintained SDL 3.4-compatible
            # fork keeps WinRT video/audio, Windows Gaming Input and Xbox Dev
            # Mode.
            # Its SDL3 forward-port retained a desktop-only D3D11 window lookup
            # and omitted thread IDs needed by WASAPI's reentrancy checks.
            # Apply the local UWP runtime fixes reproducibly before configuring
            # the dependency. The helper tolerates CMake reconfiguration after
            # the patch is already present in the fetched source tree.
            find_package(Git REQUIRED)
            FetchContent_Declare(
                SDL3
                GIT_REPOSITORY https://github.com/SternXD/SDL3-uwp.git
                GIT_TAG 8fd8db768df139e9bb4c7a7869fb913a0e29ea89
                GIT_SHALLOW TRUE
                PATCH_COMMAND "${CMAKE_COMMAND}"
                    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
                    "-DSOURCE_DIR=<SOURCE_DIR>"
                    "-DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-uwp-d3d11.patch"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_git_patch_once.cmake")
        else()
            FetchContent_Declare(
                SDL3
                URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz
                URL_HASH SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                ${STARFOX_SDL3_PATCH_COMMAND})
        endif()
        FetchContent_MakeAvailable(SDL3)
    endif()

    set(STARFOX_RUNTIME_SOURCES
        src/app/starfox_pc.cpp
        src/app/runtime_input.cpp)
    if(ANDROID)
        # SDLActivity loads libmain.so. SDL_main.h exports SDL_main from the
        # otherwise-identical portable runtime entry point.
        add_library(starfox_pc SHARED ${STARFOX_RUNTIME_SOURCES})
        set_target_properties(starfox_pc PROPERTIES OUTPUT_NAME main)
    else()
        add_executable(starfox_pc ${STARFOX_RUNTIME_SOURCES})
    endif()
    if(WINDOWS_STORE)
        # The Ninja generator's WindowsStore platform adds /APPCONTAINER at
        # link time but does not consistently publish WINAPI_FAMILY to
        # application translation units.  SDL then compiles its library as
        # WinRT while SDL_main.h in the executable selects the desktop Win32
        # entry point.  That mixed startup path packages successfully but is
        # rejected/terminated during Xbox activation.  Make both halves select
        # the same CoreApplication implementation explicitly.
        target_compile_definitions(starfox_pc PRIVATE
            STARFOX_UWP=1
            WINAPI_FAMILY=WINAPI_FAMILY_APP)
        add_library(starfox_uwp_main OBJECT src/app/uwp_main.cpp)
        target_link_libraries(starfox_uwp_main PRIVATE SDL3::Headers)
        target_compile_definitions(starfox_uwp_main PRIVATE
            WINAPI_FAMILY=WINAPI_FAMILY_APP)
        target_compile_options(starfox_uwp_main PRIVATE /ZW)
        set_target_properties(starfox_uwp_main PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)
        target_sources(starfox_pc PRIVATE
            $<TARGET_OBJECTS:starfox_uwp_main>)
        # SDL's WinRT entry point expects the blank-cursor resource to be part
        # of the application binary.  The Visual Studio UWP template adds
        # these files automatically, but the Ninja package path does not.
        # Without them SDL can fail during activation before the first frame.
        target_sources(starfox_pc PRIVATE
            "${SDL3_SOURCE_DIR}/src/main/winrt/SDL3-WinRTResources.rc"
            "${SDL3_SOURCE_DIR}/src/main/winrt/SDL3-WinRTResource_BlankCursor.cur")
        target_link_options(starfox_pc PRIVATE
            /APPCONTAINER
            /NODEFAULTLIB:kernel32.lib
            /NODEFAULTLIB:user32.lib
            /DEBUG:FULL
            /OPT:REF /OPT:ICF
            /WINMD
            "/WINMDFILE:${CMAKE_CURRENT_BINARY_DIR}/starfox_pc.winmd")

        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(STARFOX_UWP_ASSET_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/uwp-assets")
        set(STARFOX_UWP_VISUAL_ASSETS
            "${STARFOX_UWP_ASSET_DIRECTORY}/StoreLogo.png"
            "${STARFOX_UWP_ASSET_DIRECTORY}/Square44x44Logo.png"
            "${STARFOX_UWP_ASSET_DIRECTORY}/Square150x150Logo.png"
            "${STARFOX_UWP_ASSET_DIRECTORY}/Wide310x150Logo.png"
            "${STARFOX_UWP_ASSET_DIRECTORY}/SplashScreen.png")
        add_custom_command(
            OUTPUT ${STARFOX_UWP_VISUAL_ASSETS}
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_uwp_assets.py"
                --output "${STARFOX_UWP_ASSET_DIRECTORY}"
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_uwp_assets.py"
            COMMENT "Generating Xbox UWP visual assets"
            VERBATIM)
        set(STARFOX_UWP_MANIFEST
            "${CMAKE_CURRENT_BINARY_DIR}/Package.appxmanifest")
        set(STARFOX_UWP_BUILD_REVISION "0" CACHE STRING
            "Fourth component of the Xbox UWP package version")
        if(NOT STARFOX_UWP_BUILD_REVISION MATCHES "^[0-9]+$")
            message(FATAL_ERROR
                "STARFOX_UWP_BUILD_REVISION must be an integer from 0 to 65535")
        elseif(STARFOX_UWP_BUILD_REVISION GREATER 65535)
            message(FATAL_ERROR
                "STARFOX_UWP_BUILD_REVISION must be an integer from 0 to 65535")
        endif()
        configure_file(platform/uwp/Package.appxmanifest.in
            "${STARFOX_UWP_MANIFEST}" @ONLY)
        target_sources(starfox_pc PRIVATE
            "${STARFOX_UWP_MANIFEST}" ${STARFOX_UWP_VISUAL_ASSETS})
        set_source_files_properties("${STARFOX_UWP_MANIFEST}" PROPERTIES
            VS_DEPLOYMENT_CONTENT 1)
        set_source_files_properties(${STARFOX_UWP_VISUAL_ASSETS} PROPERTIES
            VS_DEPLOYMENT_CONTENT 1
            VS_DEPLOYMENT_LOCATION "Assets")
        set_target_properties(starfox_pc PROPERTIES
            VS_GLOBAL_AppContainerApplication true
            VS_GLOBAL_ApplicationType "Windows Store"
            VS_GLOBAL_ApplicationTypeRevision "10.0"
            VS_GLOBAL_AppxPackage true
            VS_GLOBAL_AppxPackageSigningEnabled true
            VS_GLOBAL_GenerateAppxPackageOnBuild true
            VS_GLOBAL_UapAppxPackageBuildMode SideloadOnly
            VS_GLOBAL_DefaultLanguage en-US)
        if(STARFOX_UWP_PACKAGE_CERTIFICATE)
            set_target_properties(starfox_pc PROPERTIES
                VS_GLOBAL_PackageCertificateKeyFile
                    "${STARFOX_UWP_PACKAGE_CERTIFICATE}")
        endif()
    endif()
    if(WIN32)
        # The packaged runtime reports failures through a native message box;
        # it should not open a second console window beside the game.
        set_target_properties(starfox_pc PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
    if(SWITCH)
        # The public libnx SDL3 backend uses switch-mesa's EGL/GLES renderer.
        # Keep the static graphics libraries in one rescan group because Mesa
        # contains circular references.
        target_link_libraries(starfox_pc PRIVATE starfox_core
            "-Wl,--start-group" SDL3::SDL3 EGL GLESv2 glapi drm_nouveau
            "-Wl,--end-group" nx m)
    else()
        target_link_libraries(starfox_pc PRIVATE starfox_core SDL3::SDL3)
    endif()
    if(APPLE)
        set_target_properties(starfox_pc PROPERTIES
            MACOSX_BUNDLE TRUE
            OUTPUT_NAME "StarFoxEnhanced"
            MACOSX_BUNDLE_BUNDLE_NAME "Star Fox Enhanced"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.starfox.enhanced"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_INFO_PLIST
                "${CMAKE_CURRENT_SOURCE_DIR}/platform/apple/Info.plist.in")
    endif()
    if(IOS)
        set_target_properties(starfox_pc PROPERTIES
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
    endif()
    if(SWITCH)
        set(STARFOX_SWITCH_NACP
            "${CMAKE_CURRENT_BINARY_DIR}/StarFoxEnhanced.nacp")
        if(COMMAND nx_generate_nacp AND COMMAND nx_create_nro)
            nx_generate_nacp(
                OUTPUT "${STARFOX_SWITCH_NACP}"
                NAME "Star Fox Enhanced"
                AUTHOR "Star Fox Enhanced team"
                VERSION "${PROJECT_VERSION}")
            nx_create_nro(starfox_pc_nro
                TARGET starfox_pc
                OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/StarFoxEnhanced.nro"
                NACP "${STARFOX_SWITCH_NACP}")
        elseif(COMMAND nx_generate_nro)
            nx_generate_nro(starfox_pc
                NAME "Star Fox Enhanced"
                AUTHOR "Star Fox Enhanced team")
        else()
            message(FATAL_ERROR
                "The devkitPro Switch CMake helpers are unavailable")
        endif()
    endif()

    if(STARFOX_PACKAGE_MSU1_MUSIC)
        FetchContent_Declare(
            starfox_msu_pack
            GIT_REPOSITORY https://github.com/Sunlitspace542/star-fox-msu1-pcm.git
            GIT_TAG 7a86296d7f62fd26bc2c51a7ed43eda8e4b21588
            SOURCE_SUBDIR _starfox_no_upstream_project
        )
        FetchContent_MakeAvailable(starfox_msu_pack)
        set(STARFOX_MSU_TRACK_FILES
            titledemo title controls trainingmode map blackholemap
            briefing briefingshort scramble level1_1 level1_2 level1_3
            level1_4 level1_6 level2_2 level2_3 level2_4 level2_6
            level3_3 level3_5 boss1_1 boss1_2 boss1_3 boss1_4 boss2_3
            boss3_3 boss3_5 boss1_5a boss1_5b boss1_5c bossandross
            bossootd levelblackhole levelootd clearband clearorch
            clearootd downband downorch gameover continue secretactivated
            blackholewarp blackholeexit dangerous lastbossclear
            gamecleardemo bossroll staffroll enterhyperspace1
            enterhyperspace2 exithyperspace)
        set(STARFOX_MSU_TRACK_PATHS "")
        foreach(STARFOX_MSU_TRACK IN LISTS STARFOX_MSU_TRACK_FILES)
            set(STARFOX_MSU_TRACK_PATH
                "${starfox_msu_pack_SOURCE_DIR}/${STARFOX_MSU_TRACK}.flac")
            if(NOT EXISTS "${STARFOX_MSU_TRACK_PATH}")
                message(FATAL_ERROR
                    "MSU-1 pack is missing ${STARFOX_MSU_TRACK}.flac")
            endif()
            list(APPEND STARFOX_MSU_TRACK_PATHS "${STARFOX_MSU_TRACK_PATH}")
        endforeach()
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(STARFOX_MSU1_PACK
            "${CMAKE_CURRENT_BINARY_DIR}/Starfox-MSU1.PAK")
        add_custom_command(
            OUTPUT "${STARFOX_MSU1_PACK}"
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_msu1.py"
                --output "${STARFOX_MSU1_PACK}"
                ${STARFOX_MSU_TRACK_PATHS}
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_msu1.py"
                ${STARFOX_MSU_TRACK_PATHS}
            COMMENT "Packing optional MSU-1 soundtrack companion"
            VERBATIM)
        add_custom_target(starfox_msu1_pack ALL
            DEPENDS "${STARFOX_MSU1_PACK}")
        add_dependencies(starfox_pc starfox_msu1_pack)
        install(FILES "${STARFOX_MSU1_PACK}" DESTINATION .)
    endif()

    set(STARFOX_EMBEDDED_INPUTS_AVAILABLE FALSE)
    if(STARFOX_EMBED_RUNTIME_ASSETS
       AND EXISTS "${STARFOX_ORIGINAL_PATCH_FILE}"
       AND EXISTS "${STARFOX_SYMBOLS_FILE}"
       AND EXISTS "${STARFOX_EX_PATCH_FILE}"
       AND EXISTS "${STARFOX_EX_SYMBOLS_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
        set(STARFOX_EMBEDDED_INPUTS_AVAILABLE TRUE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(STARFOX_GENERATED_EMBEDDED_ASSETS
            "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_assets.cpp")
        add_custom_command(
            OUTPUT "${STARFOX_GENERATED_EMBEDDED_ASSETS}"
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_runtime_assets.py"
                --output "${STARFOX_GENERATED_EMBEDDED_ASSETS}"
                --resource "101=${STARFOX_ORIGINAL_PATCH_FILE}"
                --resource "102=${STARFOX_SYMBOLS_FILE}"
                --resource "108=${STARFOX_EX_PATCH_FILE}"
                --resource "109=${STARFOX_EX_SYMBOLS_FILE}"
                --resource "120=${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
                --resource "121=${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
                --resource "122=${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
                --resource "123=${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
                --resource "124=${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
                --resource "125=${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
                --resource "126=${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}"
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_runtime_assets.py"
                "${STARFOX_ORIGINAL_PATCH_FILE}"
                "${STARFOX_SYMBOLS_FILE}"
                "${STARFOX_EX_PATCH_FILE}"
                "${STARFOX_EX_SYMBOLS_FILE}"
                "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}"
            COMMENT "Embedding portable runtime patch resources"
            VERBATIM)
        add_library(starfox_embedded_assets STATIC
            "${STARFOX_GENERATED_EMBEDDED_ASSETS}")
        target_include_directories(starfox_embedded_assets PRIVATE include)
        target_compile_features(starfox_embedded_assets PRIVATE cxx_std_20)

        if(NOT ANDROID AND NOT IOS AND NOT WINDOWS_STORE
           AND NOT SWITCH AND NOT VITA)
            add_executable(starfox_asset_builder src/app/asset_builder.cpp)
            target_link_libraries(starfox_asset_builder PRIVATE
                starfox_core starfox_embedded_assets)
        endif()
    endif()

    if(WIN32 AND NOT WINDOWS_STORE AND STARFOX_EMBED_RUNTIME_ASSETS)
        if(EXISTS "${STARFOX_ORIGINAL_PATCH_FILE}"
           AND EXISTS "${STARFOX_SYMBOLS_FILE}"
           AND EXISTS "${STARFOX_EX_PATCH_FILE}"
           AND EXISTS "${STARFOX_EX_SYMBOLS_FILE}"
           AND EXISTS "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
           AND EXISTS "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
            enable_language(RC)
            file(TO_CMAKE_PATH
                "${STARFOX_ORIGINAL_PATCH_FILE}" STARFOX_EMBEDDED_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_SYMBOLS_FILE}" STARFOX_EMBEDDED_SYMBOLS)
            file(TO_CMAKE_PATH
                "${STARFOX_EX_PATCH_FILE}" STARFOX_EX_EMBEDDED_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_EX_SYMBOLS_FILE}" STARFOX_EX_EMBEDDED_SYMBOLS)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
                STARFOX_RETAIL_JAPAN_V10_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
                STARFOX_RETAIL_JAPAN_V11_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
                STARFOX_RETAIL_USA_V10_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
                STARFOX_RETAIL_USA_V11_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
                STARFOX_RETAIL_EUROPE_V10_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
                STARFOX_RETAIL_EUROPE_V11_PATCH)
            file(TO_CMAKE_PATH "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}"
                STARFOX_RETAIL_GERMANY_V10_PATCH)
            configure_file(
                src/app/starfox_assets.rc.in
                "${CMAKE_CURRENT_BINARY_DIR}/starfox_assets.rc"
                @ONLY
            )
            # Keep the resource compiler isolated from the C++ target's
            # include path. GNU windres forwards quoted -I paths through its
            # preprocessor incorrectly when the workspace contains spaces.
            add_library(starfox_runtime_resources OBJECT
                "${CMAKE_CURRENT_BINARY_DIR}/starfox_assets.rc")
            set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/starfox_assets.rc"
                PROPERTY OBJECT_DEPENDS
                    "${STARFOX_ORIGINAL_PATCH_FILE};${STARFOX_SYMBOLS_FILE};${STARFOX_EX_PATCH_FILE};${STARFOX_EX_SYMBOLS_FILE};${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE};${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE};${STARFOX_RETAIL_USA_V10_PATCH_FILE};${STARFOX_RETAIL_USA_V11_PATCH_FILE};${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE};${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE};${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
            target_sources(starfox_pc PRIVATE
                $<TARGET_OBJECTS:starfox_runtime_resources>)
            target_compile_definitions(starfox_pc PRIVATE
                STARFOX_HAS_EMBEDDED_ASSETS=1)
            set(STARFOX_RUNTIME_ASSETS_EMBEDDED TRUE)
        else()
            message(WARNING
                "Embedded runtime assets requested, but an Original or Star Fox EX "
                "BPS/symbol pair is missing. starfox_pc will require external assets.")
        endif()
    endif()
    if((NOT WIN32 OR WINDOWS_STORE) AND STARFOX_EMBED_RUNTIME_ASSETS)
        if(STARFOX_EMBEDDED_INPUTS_AVAILABLE)
            target_link_libraries(starfox_pc PRIVATE starfox_embedded_assets)
            target_compile_definitions(starfox_pc PRIVATE
                STARFOX_HAS_EMBEDDED_ASSETS=1)
            set(STARFOX_RUNTIME_ASSETS_EMBEDDED TRUE)
        else()
            message(WARNING
                "Portable embedded assets requested, but a BPS/symbol pair "
                "is missing. starfox_pc will require external assets.")
        endif()
    endif()
    if(MINGW)
        target_link_options(starfox_pc PRIVATE -static -static-libgcc -static-libstdc++)
        if(TARGET starfox_asset_builder)
            # The builder is distributed as a single standalone executable.
            # Do not leave users dependent on whichever MinGW runtime happened
            # to be installed on the CI worker that produced the release.
            target_link_options(starfox_asset_builder PRIVATE
                -static -static-libgcc -static-libstdc++)
        endif()
    endif()
    if(VITA)
        if(NOT DEFINED ENV{VITASDK} OR "$ENV{VITASDK}" STREQUAL "")
            message(FATAL_ERROR "VITASDK must point to an installed VitaSDK")
        endif()
        include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
        set(STARFOX_VITA_TITLE_ID "SFXE00001" CACHE STRING
            "Nine-character PS Vita title id")
        set(STARFOX_VITA_APP_VERSION "00.04" CACHE STRING
            "Two-component PS Vita application version")
        vita_create_self(eboot.bin starfox_pc)
        vita_create_vpk(StarFoxEnhanced.vpk
            "${STARFOX_VITA_TITLE_ID}" eboot.bin
            VERSION "${STARFOX_VITA_APP_VERSION}"
            NAME "Star Fox Enhanced")
    endif()

    install(TARGETS starfox_pc
        RUNTIME DESTINATION .
        LIBRARY DESTINATION .
        BUNDLE DESTINATION .)
    if(TARGET starfox_asset_builder)
        install(TARGETS starfox_asset_builder RUNTIME DESTINATION .)
    endif()
    install(FILES LICENSE README.md CREDITS.md THIRD_PARTY_NOTICES.md DESTINATION .)

endif()

if(STARFOX_BUILD_TESTS)
    enable_testing()
    if(TARGET starfox_reference_ares_tests)
        add_test(NAME starfox_reference_ares_adapter COMMAND starfox_reference_ares_tests)
    endif()
    if(TARGET starfox_reference_cpu_tests)
        add_test(NAME starfox_reference_cpu COMMAND starfox_reference_cpu_tests
            "${CMAKE_CURRENT_BINARY_DIR}/reference-cpu.csv")
        set_tests_properties(starfox_reference_cpu PROPERTIES TIMEOUT 180)
    endif()
    if(TARGET starfox_reference_interrupt_tests)
        add_test(NAME starfox_reference_interrupts COMMAND starfox_reference_interrupt_tests
            "${CMAKE_CURRENT_BINARY_DIR}/reference-interrupts.csv")
        set_tests_properties(starfox_reference_interrupts PROPERTIES TIMEOUT 120)
    endif()
    if(TARGET starfox_reference_timer_tests)
        add_test(NAME starfox_reference_timers COMMAND starfox_reference_timer_tests)
        set_tests_properties(starfox_reference_timers PROPERTIES TIMEOUT 120)
    endif()
    if(TARGET starfox_reference_halt_tests)
        add_test(NAME starfox_reference_halt COMMAND starfox_reference_halt_tests)
        set_tests_properties(starfox_reference_halt PROPERTIES TIMEOUT 30)
    endif()
    if(TARGET starfox_reference_gsu_overlap_tests)
        add_test(NAME starfox_reference_gsu_overlap COMMAND starfox_reference_gsu_overlap_tests)
        set_tests_properties(starfox_reference_gsu_overlap PROPERTIES TIMEOUT 120)
    endif()
    if(TARGET starfox_reference_gsu_device_tests)
        add_test(NAME starfox_reference_gsu_device COMMAND starfox_reference_gsu_device_tests)
        set_tests_properties(starfox_reference_gsu_device PROPERTIES TIMEOUT 120)
    endif()
    if(TARGET starfox_reference_dma_tests)
        add_test(NAME starfox_reference_dma COMMAND starfox_reference_dma_tests)
        set_tests_properties(starfox_reference_dma PROPERTIES TIMEOUT 60)
    endif()
    if(TARGET starfox_reference_raster_tests)
        add_test(NAME starfox_reference_raster COMMAND starfox_reference_raster_tests)
        set_tests_properties(starfox_reference_raster PROPERTIES TIMEOUT 120)
    endif()
    add_executable(starfox_bps_tests tests/bps_tests.cpp)
    target_link_libraries(starfox_bps_tests PRIVATE starfox_core)
    add_test(NAME starfox_bps_tests COMMAND starfox_bps_tests)
    if(EXISTS "${STARFOX_RETAIL_V12_FILE}"
       AND EXISTS "${STARFOX_ORIGINAL_PATCH_FILE}"
       AND EXISTS "${STARFOX_ROM_FILE}"
       AND EXISTS "${STARFOX_EX_PATCH_FILE}"
       AND EXISTS "${STARFOX_EX_ROM_FILE}")
        add_test(
            NAME starfox_retail_v12_reconstruction
            COMMAND starfox_bps_tests
                "${STARFOX_RETAIL_V12_FILE}"
                "${STARFOX_ORIGINAL_PATCH_FILE}"
                "${STARFOX_ROM_FILE}"
                "${STARFOX_EX_PATCH_FILE}"
                "${STARFOX_EX_ROM_FILE}")
    endif()
    if(EXISTS "${STARFOX_RETAIL_V12_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V10_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V11_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V10_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V11_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V10_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V11_FILE}"
       AND EXISTS "${STARFOX_RETAIL_GERMANY_V10_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
       AND EXISTS "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
        add_test(
            NAME starfox_retail_variant_canonicalization
            COMMAND starfox_bps_tests --verify-canonicalizations
                "${STARFOX_RETAIL_V12_FILE}"
                "${STARFOX_RETAIL_JAPAN_V10_FILE}"
                "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_JAPAN_V11_FILE}"
                "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_USA_V10_FILE}"
                "${STARFOX_RETAIL_USA_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_USA_V11_FILE}"
                "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_EUROPE_V10_FILE}"
                "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}"
                "${STARFOX_RETAIL_EUROPE_V11_FILE}"
                "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
                "${STARFOX_RETAIL_GERMANY_V10_FILE}"
                "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
    endif()
    add_executable(starfox_core_tests tests/timing_tests.cpp)
    target_link_libraries(starfox_core_tests PRIVATE starfox_core)
    add_test(NAME starfox_core_tests COMMAND starfox_core_tests)

    add_executable(starfox_msu1_pack_tests tests/msu1_pack_tests.cpp)
    target_link_libraries(starfox_msu1_pack_tests PRIVATE starfox_core)
    add_test(NAME starfox_msu1_pack_tests COMMAND starfox_msu1_pack_tests)
    add_executable(starfox_msu_timeline_tests tests/msu_timeline_tests.cpp)
    target_link_libraries(starfox_msu_timeline_tests PRIVATE starfox_core)
    add_test(NAME starfox_msu_timeline_tests COMMAND starfox_msu_timeline_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/msu-tone-320.flac")

    add_executable(starfox_shape_decoder_tests tests/shape_decoder_tests.cpp)
    target_link_libraries(starfox_shape_decoder_tests PRIVATE starfox_core)
    add_test(NAME starfox_shape_decoder_tests COMMAND starfox_shape_decoder_tests)

    add_executable(starfox_simulation_tests tests/simulation_tests.cpp)
    target_link_libraries(starfox_simulation_tests PRIVATE starfox_core)
    # The cartridge-wide integration fixture intentionally keeps several full
    # GameSimulation snapshots alive in nested parity sections. PE/COFF's
    # small default stack can overflow after earlier sections begin reaching
    # their now-correct transition endpoints, masking the actual assertions.
    if(MINGW)
        target_link_options(
            starfox_simulation_tests PRIVATE "-Wl,--stack,16777216")
    elseif(MSVC)
        target_link_options(starfox_simulation_tests PRIVATE "/STACK:16777216")
    endif()
    add_test(NAME starfox_simulation_tests COMMAND starfox_simulation_tests)

    add_executable(starfox_cpu_timing_tests tests/cpu_timing_tests.cpp)
    target_link_libraries(starfox_cpu_timing_tests PRIVATE starfox_core)
    add_test(NAME starfox_cpu_timing_tests COMMAND starfox_cpu_timing_tests)

    add_executable(starfox_bitmap_transfer_tests tests/bitmap_transfer_tests.cpp)
    target_link_libraries(starfox_bitmap_transfer_tests PRIVATE starfox_core)
    add_test(NAME starfox_bitmap_transfer_tests COMMAND starfox_bitmap_transfer_tests)
    add_executable(starfox_dma_tests tests/dma_tests.cpp)
    target_link_libraries(starfox_dma_tests PRIVATE starfox_core)
    add_test(NAME starfox_dma_tests COMMAND starfox_dma_tests)

    add_executable(starfox_live_gsu_tests tests/live_gsu_tests.cpp)
    add_executable(starfox_presentation_hold_tests tests/presentation_hold_tests.cpp)
    target_link_libraries(starfox_presentation_hold_tests PRIVATE starfox_core)
    add_test(NAME starfox_presentation_hold_tests COMMAND starfox_presentation_hold_tests)
    target_link_libraries(starfox_live_gsu_tests PRIVATE starfox_core)
    add_test(NAME starfox_live_gsu_tests COMMAND starfox_live_gsu_tests)
    add_executable(starfox_gsu_device_tests tests/gsu_device_tests.cpp)
    target_link_libraries(starfox_gsu_device_tests PRIVATE starfox_core)
    add_test(NAME starfox_gsu_device_tests COMMAND starfox_gsu_device_tests)
    add_executable(starfox_snes_interrupt_tests tests/snes_interrupt_tests.cpp)
    target_link_libraries(starfox_snes_interrupt_tests PRIVATE starfox_core)
    add_test(NAME starfox_snes_interrupt_tests COMMAND starfox_snes_interrupt_tests)
    add_executable(starfox_snes_timeline_tests tests/snes_timeline_tests.cpp)
    target_link_libraries(starfox_snes_timeline_tests PRIVATE starfox_core)
    add_test(NAME starfox_snes_timeline_tests COMMAND starfox_snes_timeline_tests)
    add_executable(starfox_timing_io_tests tests/timing_io_tests.cpp)
    target_link_libraries(starfox_timing_io_tests PRIVATE starfox_core)
    add_test(NAME starfox_timing_io_tests COMMAND starfox_timing_io_tests)

    add_executable(starfox_direct_audio_tests tests/direct_audio_tests.cpp)
    target_link_libraries(starfox_direct_audio_tests PRIVATE starfox_core)

    add_executable(starfox_transition_parity_tests
        tests/transition_parity_tests.cpp)
    target_link_libraries(starfox_transition_parity_tests PRIVATE starfox_core)

    add_executable(starfox_ending_tests tests/ending_tests.cpp)
    target_link_libraries(starfox_ending_tests PRIVATE starfox_core)
    add_executable(starfox_port_regression_tests tests/port_regression_tests.cpp)
    target_link_libraries(starfox_port_regression_tests PRIVATE starfox_core)
    add_executable(starfox_reference_render_tests tests/reference_render_tests.cpp)
    target_link_libraries(starfox_reference_render_tests PRIVATE starfox_core)
    add_executable(starfox_reference_math_tests tests/reference_math_tests.cpp)
    target_link_libraries(starfox_reference_math_tests PRIVATE starfox_core)
    add_executable(starfox_reference_dust_tests tests/reference_dust_tests.cpp)
    target_link_libraries(starfox_reference_dust_tests PRIVATE starfox_core)
    add_executable(starfox_reference_grid_tests tests/reference_grid_tests.cpp)
    target_link_libraries(starfox_reference_grid_tests PRIVATE starfox_core)
    add_executable(starfox_timing_parity_tests tests/timing_parity_tests.cpp)
    target_link_libraries(starfox_timing_parity_tests PRIVATE starfox_core)
    add_executable(starfox_multiplayer_input_tests tests/multiplayer_input_tests.cpp)
    add_executable(starfox_native_transfer_tests tests/native_transfer_tests.cpp)
    add_executable(starfox_native_transfer_input_tests tests/native_transfer_input_tests.cpp)
    target_link_libraries(starfox_native_transfer_input_tests PRIVATE starfox_core)
    target_link_libraries(starfox_native_transfer_tests PRIVATE starfox_core)
    target_link_libraries(starfox_multiplayer_input_tests PRIVATE starfox_core)
    add_executable(starfox_ending_audio_tests tests/ending_audio_tests.cpp)
    target_link_libraries(starfox_ending_audio_tests PRIVATE starfox_core)
    add_executable(starfox_stream_audio_tests tests/stream_audio_tests.cpp)
    target_link_libraries(starfox_stream_audio_tests PRIVATE starfox_core)
    add_executable(starfox_native_audio_tests tests/native_audio_tests.cpp)
    target_link_libraries(starfox_native_audio_tests PRIVATE starfox_core)

    add_executable(starfox_hitlist_tests tests/hitlist_tests.cpp)
    add_executable(starfox_level_clear_tests tests/level_clear_tests.cpp)
    target_link_libraries(starfox_level_clear_tests PRIVATE starfox_core)
    target_link_libraries(starfox_hitlist_tests PRIVATE starfox_core)

    if(STARFOX_BUILD_RUNTIME)
        add_executable(starfox_runtime_input_tests
            tests/runtime_input_tests.cpp
            src/app/runtime_input.cpp)
        target_link_libraries(
            starfox_runtime_input_tests PRIVATE starfox_core SDL3::SDL3)
        add_test(NAME starfox_runtime_input_tests COMMAND starfox_runtime_input_tests)
        add_executable(starfox_audio_queue_tests tests/audio_queue_tests.cpp)
        target_link_libraries(starfox_audio_queue_tests PRIVATE starfox_core SDL3::SDL3)
        add_test(NAME starfox_audio_queue_tests COMMAND starfox_audio_queue_tests)
        set_tests_properties(starfox_audio_queue_tests PROPERTIES
            ENVIRONMENT "SDL_AUDIODRIVER=dummy")
        set_tests_properties(starfox_runtime_input_tests PROPERTIES
            ENVIRONMENT "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1"
            TIMEOUT 10)
        if(WIN32 AND NOT WINDOWS_STORE)
            # Exercise UWP's pre-initialization configuration on the host as
            # well; ordinary desktop tests can pass via XInput without WGI.
            add_executable(starfox_uwp_input_tests
                tests/runtime_input_tests.cpp src/app/runtime_input.cpp)
            target_compile_definitions(starfox_uwp_input_tests PRIVATE STARFOX_UWP=1)
            target_link_libraries(starfox_uwp_input_tests PRIVATE starfox_core SDL3::SDL3)
            add_test(NAME starfox_uwp_input_tests COMMAND starfox_uwp_input_tests)
            set_tests_properties(starfox_uwp_input_tests PROPERTIES
                ENVIRONMENT "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1" TIMEOUT 10)
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
       AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(
            NAME starfox_upstream_shape_coverage
            COMMAND starfox_shape_coverage
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
        )
        add_test(
            NAME starfox_upstream_simulation_data
            COMMAND starfox_simulation_tests
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
        )
        add_test(
            NAME starfox_upstream_direct_audio
            COMMAND starfox_direct_audio_tests
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
        )
        add_test(
            NAME starfox_upstream_transition_parity
            COMMAND starfox_transition_parity_tests
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
        )
        add_test(NAME starfox_upstream_ending COMMAND starfox_ending_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_port_regressions COMMAND starfox_port_regression_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_reference_render COMMAND starfox_reference_render_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-render-original.csv")
        add_test(NAME starfox_upstream_reference_math COMMAND starfox_reference_math_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-matrices-original.csv"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-points-original.csv")
        add_test(NAME starfox_upstream_reference_dust COMMAND starfox_reference_dust_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-dust-original.csv")
        add_test(NAME starfox_upstream_reference_grid COMMAND starfox_reference_grid_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-grid-original.csv")
        if(TARGET starfox_reference_view)
            foreach(route IN ITEMS 2 3)
                add_test(NAME starfox_upstream_reference_view_${route} COMMAND starfox_reference_view
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    "LEVEL${route}_1" 1800 "${CMAKE_CURRENT_BINARY_DIR}/reference-view-original-${route}.csv")
            endforeach()
        endif()
        add_test(NAME starfox_upstream_ending_audio COMMAND starfox_ending_audio_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_reference_sprites COMMAND starfox_reference_render_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-sprites-original.csv")
        add_test(NAME starfox_upstream_timing_parity COMMAND starfox_timing_parity_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        set_tests_properties(starfox_upstream_timing_parity PROPERTIES TIMEOUT 180)
        add_test(NAME starfox_upstream_native_transfer COMMAND starfox_native_transfer_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT" LEVEL2_1)
        set_tests_properties(starfox_upstream_native_transfer PROPERTIES TIMEOUT 120)
        add_test(NAME starfox_upstream_native_transfer_input COMMAND starfox_native_transfer_input_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT" LEVEL2_1)
        set_tests_properties(starfox_upstream_native_transfer_input PROPERTIES TIMEOUT 180)
        add_test(NAME starfox_upstream_native_main COMMAND starfox_native_transfer_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT" LEVEL2_1 --main)
        add_test(NAME starfox_upstream_native_main_input COMMAND starfox_native_transfer_input_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT" LEVEL2_1 --main)
        set_tests_properties(starfox_upstream_native_main starfox_upstream_native_main_input PROPERTIES TIMEOUT 180)
        add_test(NAME starfox_upstream_stream_audio COMMAND starfox_stream_audio_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_native_audio COMMAND starfox_native_audio_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT" LEVEL2_1)
        add_test(NAME starfox_upstream_native_msu_audio COMMAND starfox_native_audio_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT" LEVEL2_1
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/msu-tone-320.flac")
        set(STARFOX_TEST_MSU_PACK "${STARFOX_MSU1_PACK}")
        if(NOT STARFOX_TEST_MSU_PACK AND EXISTS "${CMAKE_CURRENT_BINARY_DIR}/Starfox-MSU1.PAK")
            set(STARFOX_TEST_MSU_PACK "${CMAKE_CURRENT_BINARY_DIR}/Starfox-MSU1.PAK")
        endif()
        if(STARFOX_TEST_MSU_PACK)
            add_test(NAME starfox_upstream_msu_ending_audio COMMAND starfox_ending_audio_tests
                "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                "${STARFOX_TEST_MSU_PACK}")
            set_tests_properties(starfox_upstream_msu_ending_audio PROPERTIES TIMEOUT 180)
        endif()
        set_tests_properties(starfox_upstream_port_regressions starfox_upstream_ending_audio
            PROPERTIES TIMEOUT 180)
        add_test(NAME starfox_upstream_hitlist COMMAND starfox_hitlist_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_level_clear COMMAND starfox_level_clear_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(
            NAME starfox_upstream_map_trace
            COMMAND starfox_map_trace
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    MAP1_1A
        )
        add_test(
            NAME starfox_upstream_strategy_trace
            COMMAND starfox_strategy_trace
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    MAP1_1A
                    1
        )
        add_test(
            NAME starfox_upstream_game_init
            COMMAND starfox_game_init_trace
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    20
        )
        add_test(
            NAME starfox_upstream_stage_trace
            COMMAND starfox_stage_trace
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    LEVEL1_1
                    100
        )
        add_test(
            NAME starfox_upstream_planet_probe
            COMMAND starfox_planet_probe
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
        )
        add_test(
            NAME starfox_upstream_route_lifecycle
            COMMAND starfox_stage_trace
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                    LEVEL1_3
                    500
        )
        if(STARFOX_BUILD_RUNTIME)
            add_test(NAME starfox_native_desktop_timing COMMAND "${CMAKE_COMMAND}"
                "-DRUNTIME=$<TARGET_FILE:starfox_pc>"
                "-DROM=${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                "-DSYMBOLS=${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                -DMAP=LEVEL2_1 -DEXPERIENCE=ORIGINAL
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_desktop_timing.cmake")
            set_tests_properties(starfox_native_desktop_timing PROPERTIES TIMEOUT 150)
            add_test(NAME starfox_native_desktop_exits COMMAND "${CMAKE_COMMAND}"
                "-DRUNTIME=$<TARGET_FILE:starfox_pc>"
                "-DROM=${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                "-DSYMBOLS=${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                -DMAP=LEVEL2_1 -DEXPERIENCE=ORIGINAL "-DTEST_MSU_PACK=${STARFOX_TEST_MSU_PACK}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_desktop_exits.cmake")
            set_tests_properties(starfox_native_desktop_exits PROPERTIES TIMEOUT 500)
            add_test(
                NAME starfox_runtime_smoke
                COMMAND starfox_pc
                        "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                        "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                        LEVEL1_1
            )
            set_tests_properties(starfox_runtime_smoke PROPERTIES
                ENVIRONMENT
                    "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_TEST_FRAMES=6;STARFOX_TEST_RENDER_SCALE=1"
                TIMEOUT 10
            )
            add_test(
                NAME starfox_exit_confirmation_smoke
                COMMAND starfox_pc
            )
            set_tests_properties(starfox_exit_confirmation_smoke PROPERTIES
                ENVIRONMENT
                    "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_TEST_EXIT_CONFIRMATION=1;STARFOX_TEST_FRAMES=1000000;STARFOX_TEST_PRESSES=1:0x0200,5:0x0080;STARFOX_TEST_UNPACED=1;STARFOX_TEST_SKIP_PREROLL=1;STARFOX_TEST_RENDER_SCALE=10"
                # The scripted LEFT, then A must select YES and terminate long
                # before the large frame sentinel. This also exercises the
                # confirmation compositor at the maximum upscale setting.
                TIMEOUT 10
            )
            if(STARFOX_RUNTIME_ASSETS_EMBEDDED)
                file(MAKE_DIRECTORY
                    "${CMAKE_CURRENT_BINARY_DIR}/embedded-runtime-smoke")
                add_test(
                    NAME starfox_embedded_runtime_smoke
                    COMMAND "$<TARGET_FILE:starfox_pc>"
                )
                set_tests_properties(starfox_embedded_runtime_smoke PROPERTIES
                    WORKING_DIRECTORY
                        "${CMAKE_CURRENT_BINARY_DIR}/embedded-runtime-smoke"
                    ENVIRONMENT
                        "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_TEST_FRAMES=6;STARFOX_TEST_RENDER_SCALE=1"
                    TIMEOUT 10
                )
                if(EXISTS "${STARFOX_EX_ROM_FILE}"
                   AND EXISTS "${STARFOX_EX_SYMBOLS_FILE}")
                    add_test(
                        NAME starfox_embedded_asset_lifecycle
                        COMMAND "${CMAKE_COMMAND}"
                            "-DRUNTIME=$<TARGET_FILE:starfox_pc>"
                            "-DVERIFIER=$<TARGET_FILE:starfox_bps_tests>"
                            "-DRETAIL=${STARFOX_RETAIL_V12_FILE}"
                            "-DRETAIL_JAPAN_V10=${STARFOX_RETAIL_JAPAN_V10_FILE}"
                            "-DRETAIL_JAPAN_V11=${STARFOX_RETAIL_JAPAN_V11_FILE}"
                            "-DRETAIL_USA_V10=${STARFOX_RETAIL_USA_V10_FILE}"
                            "-DRETAIL_USA_V11=${STARFOX_RETAIL_USA_V11_FILE}"
                            "-DRETAIL_EUROPE_V10=${STARFOX_RETAIL_EUROPE_V10_FILE}"
                            "-DRETAIL_EUROPE_V11=${STARFOX_RETAIL_EUROPE_V11_FILE}"
                            "-DRETAIL_GERMANY_V10=${STARFOX_RETAIL_GERMANY_V10_FILE}"
                            "-DORIGINAL_ROM=${STARFOX_ROM_FILE}"
                            "-DORIGINAL_SYMBOLS=${STARFOX_SYMBOLS_FILE}"
                            "-DEX_ROM=${STARFOX_EX_ROM_FILE}"
                            "-DEX_SYMBOLS=${STARFOX_EX_SYMBOLS_FILE}"
                            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/asset-lifecycle"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime_asset_lifecycle.cmake"
                    )
                    set_tests_properties(
                        starfox_embedded_asset_lifecycle PROPERTIES TIMEOUT 90)
                endif()
            endif()
        endif()
    endif()

    if(EXISTS "${STARFOX_EX_ROM_FILE}"
       AND EXISTS "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(
            NAME starfox_ex_direct_audio
            COMMAND starfox_direct_audio_tests
                    "${STARFOX_EX_ROM_FILE}"
                    "${STARFOX_EX_SYMBOLS_FILE}"
        )
        add_test(
            NAME starfox_ex_transition_parity
            COMMAND starfox_transition_parity_tests
                    "${STARFOX_EX_ROM_FILE}"
                    "${STARFOX_EX_SYMBOLS_FILE}"
        )
        add_test(NAME starfox_ex_ending COMMAND starfox_ending_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_shape_coverage COMMAND starfox_shape_coverage
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_ending_audio COMMAND starfox_ending_audio_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_orchestra_audio COMMAND starfox_ending_audio_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" "-" "orchestra")
        set_tests_properties(starfox_ex_ending_audio starfox_ex_orchestra_audio
            PROPERTIES TIMEOUT 180)
        add_test(NAME starfox_ex_port_regressions COMMAND starfox_port_regression_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_reference_render COMMAND starfox_reference_render_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-render-ex.csv")
        add_test(NAME starfox_ex_reference_math COMMAND starfox_reference_math_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-matrices-ex.csv"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-points-ex.csv")
        add_test(NAME starfox_ex_reference_dust COMMAND starfox_reference_dust_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-dust-ex.csv")
        add_test(NAME starfox_ex_reference_grid COMMAND starfox_reference_grid_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-grid-ex.csv")
        if(TARGET starfox_reference_view)
            foreach(route IN ITEMS 2 3)
                add_test(NAME starfox_ex_reference_view_${route} COMMAND starfox_reference_view
                    "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}"
                    "LEVEL${route}_1" 1800 "${CMAKE_CURRENT_BINARY_DIR}/reference-view-ex-${route}.csv")
            endforeach()
        endif()
        add_test(NAME starfox_ex_reference_sprites COMMAND starfox_reference_render_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/reference-sprites-ex.csv")
        add_test(NAME starfox_ex_timing_parity COMMAND starfox_timing_parity_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        set_tests_properties(starfox_ex_timing_parity PROPERTIES TIMEOUT 180)
        add_test(NAME starfox_ex_native_transfer COMMAND starfox_native_transfer_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" LEVEL7_2)
        set_tests_properties(starfox_ex_native_transfer PROPERTIES TIMEOUT 120)
        add_test(NAME starfox_ex_native_transfer_input COMMAND starfox_native_transfer_input_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" LEVEL7_2)
        set_tests_properties(starfox_ex_native_transfer_input PROPERTIES TIMEOUT 180)
        add_test(NAME starfox_ex_native_main COMMAND starfox_native_transfer_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" LEVEL7_2 --main)
        add_test(NAME starfox_ex_native_main_input COMMAND starfox_native_transfer_input_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" LEVEL7_2 --main)
        set_tests_properties(starfox_ex_native_main starfox_ex_native_main_input PROPERTIES TIMEOUT 180)
        add_test(NAME starfox_ex_stream_audio COMMAND starfox_stream_audio_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_native_audio COMMAND starfox_native_audio_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}" LEVEL7_2)
        add_test(NAME starfox_ex_multiplayer_input COMMAND starfox_multiplayer_input_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        set_tests_properties(starfox_ex_multiplayer_input PROPERTIES TIMEOUT 120)
        set_tests_properties(starfox_ex_port_regressions PROPERTIES TIMEOUT 120)
        add_test(NAME starfox_ex_hitlist COMMAND starfox_hitlist_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_level_clear COMMAND starfox_level_clear_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(
            NAME starfox_ex_simulation_data
            COMMAND starfox_simulation_tests
                    "${STARFOX_EX_ROM_FILE}"
                    "${STARFOX_EX_SYMBOLS_FILE}"
        )
        set_tests_properties(starfox_ex_simulation_data PROPERTIES
            ENVIRONMENT "STARFOX_EX_ROUTE_AUDIT_TICKS=2000"
            # The audit boots every shipped EX stage and advances each for
            # 2,000 source ticks. Renderer/route regressions have expanded
            # its coverage enough that Windows debug-host load can exceed
            # four minutes without a hang; retain the full audit and give it
            # a deterministic margin instead of shortening stage coverage.
            TIMEOUT 420)
        if(STARFOX_BUILD_RUNTIME AND STARFOX_RUNTIME_ASSETS_EMBEDDED)
            add_test(NAME starfox_ex_native_desktop_timing COMMAND "${CMAKE_COMMAND}"
                "-DRUNTIME=$<TARGET_FILE:starfox_pc>"
                "-DROM=${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                "-DSYMBOLS=${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                -DMAP=LEVEL7_2 -DEXPERIENCE=EX
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_desktop_timing.cmake")
            set_tests_properties(starfox_ex_native_desktop_timing PROPERTIES TIMEOUT 150)
            add_test(NAME starfox_ex_native_desktop_exits COMMAND "${CMAKE_COMMAND}"
                "-DRUNTIME=$<TARGET_FILE:starfox_pc>"
                "-DROM=${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                "-DSYMBOLS=${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                -DMAP=LEVEL7_2 -DEXPERIENCE=EX
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_desktop_exits.cmake")
            set_tests_properties(starfox_ex_native_desktop_exits PROPERTIES TIMEOUT 250)
            file(MAKE_DIRECTORY
                "${CMAKE_CURRENT_BINARY_DIR}/embedded-ex-runtime-smoke")
            add_test(
                NAME starfox_ex_intro_runtime_smoke
                COMMAND "$<TARGET_FILE:starfox_pc>"
            )
            set_tests_properties(starfox_ex_intro_runtime_smoke PROPERTIES
                WORKING_DIRECTORY
                    "${CMAKE_CURRENT_BINARY_DIR}/embedded-ex-runtime-smoke"
                ENVIRONMENT
                    "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_TEST_EXPERIENCE=EX;STARFOX_TEST_FRAMES=900;STARFOX_TEST_PRESSES=6:0x1000;STARFOX_TEST_UNPACED=1;STARFOX_TEST_RENDER_SCALE=1"
                # The embedded EX boot renders 900 full frames. Sanitizer and
                # busy Windows hosts can exceed 20 seconds without a hang.
                TIMEOUT 45
            )
        endif()
    endif()

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME starfox_shape_audit_tests
            COMMAND "${Python3_EXECUTABLE}" -m unittest discover
                    -s "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                    -p "test_shape_audit.py"
        )
        set_tests_properties(starfox_shape_audit_tests PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endif()
endif()
