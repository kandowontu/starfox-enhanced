cmake_minimum_required(VERSION 3.22)

project(starfox_enhanced VERSION 0.0.6.7 LANGUAGES CXX)

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
option(STARFOX_BUILD_VR "Build the OpenXR VR integration and runtime diagnostic" OFF)
option(STARFOX_BUILD_TOOLS "Build host-side trace and asset tools"
    ${STARFOX_BUILD_TOOLS_DEFAULT})
option(STARFOX_EMBED_RUNTIME_ASSETS
    "Embed source-built BPS deltas and symbol tables into the Windows runtime" ON)
option(STARFOX_PACKAGE_MSU1_MUSIC
    "Build and install the optional Starfox-MSU1.PAK companion" ON)
# xBRZ is enabled by default; minimal builds may disable the backend.
option(STARFOX_ENABLE_XBRZ
    "Fetch and compile the GPLv3 xBRZ backend for the 2D presentation filter"
    ON)
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
if(STARFOX_BUILD_VR)
    include(cmake/OpenXR.cmake)
endif()
FetchContent_Declare(
    retro_cpu
    GIT_REPOSITORY https://github.com/achaulk/retro_cpu.git
    GIT_TAG ea9049ab25084334f7cc1907b3a98bf1c2604a03
    SOURCE_SUBDIR _starfox_no_upstream_project
)
FetchContent_MakeAvailable(retro_cpu)

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
    # Only the standalone decoding headers are used. Its test-only miniaudio
    # submodule is large and unnecessary for any Star Fox target.
    GIT_SUBMODULES ""
    SOURCE_SUBDIR _starfox_no_upstream_project
)
FetchContent_MakeAvailable(dr_libs)

if(STARFOX_ENABLE_XBRZ)
    FetchContent_Declare(
        xbrz
        GIT_REPOSITORY https://github.com/janisozaur/xbrz.git
        GIT_TAG 93c54433fa0df37c689c919e8152fb0b9136584a
        SOURCE_SUBDIR _starfox_no_upstream_project
    )
    FetchContent_MakeAvailable(xbrz)
    add_library(xbrz_filter STATIC "${xbrz_SOURCE_DIR}/xbrz.cpp")
    target_include_directories(xbrz_filter SYSTEM PUBLIC "${xbrz_SOURCE_DIR}")
    target_compile_features(xbrz_filter PUBLIC cxx_std_20)
    # Upstream's debug-only pixel breakpoint uses MSVC intrinsics even on
    # other compilers. Keep this optimized third-party filter in release mode.
    target_compile_definitions(xbrz_filter PRIVATE NDEBUG)
    set_target_properties(xbrz_filter PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # The pinned xBRZ revision calls std::sqrt without including <cmath>; it
    # only ever compiled because older standard libraries leaked the
    # declaration through another header. Force the include rather than
    # patching a third-party source tree.
    if(MSVC)
        target_compile_options(xbrz_filter PRIVATE /FI cmath)
    else()
        target_compile_options(xbrz_filter PRIVATE -include cmath)
    endif()
endif()

include(cmake/SpcState.cmake)
add_library(snes_spc_core STATIC
    "${STARFOX_SPC_SOURCE}/spc.cpp"
    "${STARFOX_SPC_SOURCE}/SNES_SPC.cpp"
    "${STARFOX_SPC_SOURCE}/SNES_SPC_misc.cpp"
    "${STARFOX_SPC_SOURCE}/SNES_SPC_state.cpp"
    "${STARFOX_SPC_SOURCE}/SPC_DSP.cpp"
    "${STARFOX_SPC_SOURCE}/SPC_Filter.cpp"
)
target_include_directories(snes_spc_core SYSTEM PUBLIC
    "${STARFOX_SPC_SOURCE}"
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
    src/assets/bps.cpp
    src/assets/decrunch.cpp
    src/assets/rom.cpp
    src/assets/runtime_bundle.cpp
    src/assets/shape_decoder.cpp
    src/render/background_renderer.cpp
    src/render/dust_renderer.cpp
    src/render/dxr_shadows.cpp
    src/render/gpu_projection.cpp
    src/render/gpu_clip.cpp
    src/render/gpu_bsp.cpp
    src/render/gpu_ray_geometry.cpp
    src/render/gpu_background.cpp
    src/render/gpu_colour_warp.cpp
    src/render/packed_bsp.cpp
    src/render/packed_projection.cpp
    src/render/packed_faces.cpp
    src/render/gpu_model.cpp
    src/render/gpu_scene.cpp
    src/render/gpu_effects.cpp
    src/render/sdl_gpu_effects.cpp
    src/render/portable_shadows.cpp
    src/render/sdl_dxr_shadows.cpp
    src/render/gpu_raster.cpp
    src/render/gpu_composite.cpp
    src/render/gpu_temporal_inputs.cpp
    src/render/gpu_scalefx.cpp
    src/render/gpu_fsr1.cpp
    src/render/scalefx.cpp
    src/render/framebuffer.cpp
    src/render/palette.cpp
    src/render/particle_renderer.cpp
    src/render/pixel_filter.cpp
    src/render/presentation_history.cpp
    src/render/row_workers.cpp
    src/render/scaled_text_renderer.cpp
    src/render/software_renderer.cpp
    src/render/face_material.cpp
    src/render/sprite_renderer.cpp
    src/simulation/math.cpp
    src/simulation/dust_system.cpp
    src/simulation/game_simulation.cpp
    src/simulation/game_state.cpp
    src/simulation/map_vm.cpp
    src/simulation/object_pool.cpp
    src/simulation/state_container.cpp
    src/simulation/state_files.cpp
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
if(STARFOX_ENABLE_XBRZ)
    target_link_libraries(starfox_core PRIVATE xbrz_filter)
    target_compile_definitions(starfox_core PRIVATE STARFOX_ENABLE_XBRZ)
endif()

# Ordinary effects use SM5 compute, not RT cores. Compile DXBC at build time.
if(WIN32 AND NOT WINDOWS_STORE)
    file(GLOB STARFOX_SDK_FXC "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/fxc.exe")
    list(SORT STARFOX_SDK_FXC COMPARE NATURAL ORDER DESCENDING)
    find_program(STARFOX_FXC NAMES fxc)
    if(NOT STARFOX_FXC AND STARFOX_SDK_FXC)
        list(GET STARFOX_SDK_FXC 0 STARFOX_FXC)
    endif()
    if(STARFOX_FXC)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/effects_compute_shader.hpp"
            COMMAND "${STARFOX_FXC}" /nologo /T cs_5_0 /E main /O3
                /D "STARFOX_ENABLE_XBRZ=$<BOOL:${STARFOX_ENABLE_XBRZ}>"
                /Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/effects_compute_shader.hpp"
                /Vn effects_compute_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/effects_compute.hlsl"
            DEPENDS src/render/shaders/effects_compute.hlsl src/render/shaders/xbrz_compute.hlsli src/render/shaders/environment_material.hlsli
                src/render/shaders/backdrop_sample.hlsli
                include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc
                src/render/shaders/xbrz_weights.hlsli src/render/shaders/edge_corners.hlsli VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/effects_compute_shader.hpp")
        target_include_directories(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(starfox_core PRIVATE STARFOX_GPU_EFFECTS)
    endif()
endif()

# DXR shaders are compiled at build time; no compiler DLL is shipped or loaded
# on user machines. Other platforms retain the CPU shadow implementation.
if(WIN32 AND NOT WINDOWS_STORE AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND STARFOX_BUILD_RUNTIME)
    file(GLOB STARFOX_SDK_DXC "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe")
    list(SORT STARFOX_SDK_DXC COMPARE NATURAL ORDER DESCENDING)
    find_program(STARFOX_DXC NAMES dxc)
    if(NOT STARFOX_DXC AND STARFOX_SDK_DXC)
        list(GET STARFOX_SDK_DXC 0 STARFOX_DXC)
    endif()
    if(STARFOX_DXC)
        set(STARFOX_DXR_ENABLED TRUE)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_shader.hpp"
            COMMAND "${STARFOX_DXC}" -T cs_6_5 -E main -O3
                -Fh "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_shader.hpp"
                -Vn shadow_dxr_shader "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/shadow_dxr.hlsl"
            DEPENDS src/render/shaders/shadow_dxr.hlsl src/render/shaders/environment_material.hlsli src/render/shaders/backdrop_sample.hlsli include/starfox/render/ex_face_planet_regions.inc include/starfox/render/ex_city_moons.inc VERBATIM)
        target_sources(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/shadow_dxr_shader.hpp")
        target_include_directories(starfox_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
        target_compile_definitions(starfox_core PRIVATE STARFOX_DXR)
        if(STARFOX_BUILD_TOOLS)
            add_executable(starfox_dxr_check tools/benchmark_dxr.cpp)
            target_link_libraries(starfox_dxr_check PRIVATE starfox_core)
        endif()
    else()
        message(STATUS "DXC not found: hardware DXR disabled")
    endif()
endif()

if(MSVC)
    target_compile_options(starfox_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(starfox_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(STARFOX_BUILD_TOOLS)
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
                    "${STARFOX_SDL3_SWITCH_PATCH}"
                COMMAND "${CMAKE_COMMAND}"
                    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
                    "-DSOURCE_DIR=<SOURCE_DIR>"
                    "-DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-switch-audio-pipeline.patch"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_git_patch_once.cmake")
        elseif(APPLE OR STARFOX_METAL_INTEROP)
            set(STARFOX_SDL3_PATCH_COMMAND "")
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
        if(APPLE OR STARFOX_METAL_INTEROP)
            # URL-based FetchContent patch hooks did not update iOS's SDL
            # source in CI. Inject the narrow native-handle functions into
            # this private pinned SDL copy, before its Metal guard closes.
            set(STARFOX_SDL3_METAL_SOURCE
                "${SDL3_SOURCE_DIR}/src/gpu/metal/SDL_gpu_metal.m")
            if(NOT EXISTS "${STARFOX_SDL3_METAL_SOURCE}")
                message(FATAL_ERROR "SDL Metal backend source missing: ${STARFOX_SDL3_METAL_SOURCE}")
            endif()
            file(READ "${STARFOX_SDL3_METAL_SOURCE}" STARFOX_SDL3_METAL_TEXT)
            string(FIND "${STARFOX_SDL3_METAL_TEXT}" "SDL_StarfoxMetalDevice" STARFOX_SDL3_METAL_SYMBOL_POS)
            string(FIND "${STARFOX_SDL3_METAL_TEXT}" "starfox_metal_interop.inc"
                STARFOX_SDL3_METAL_INCLUDE_POS)
            file(COPY_FILE
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-metal-ray-interop.inc"
                "${SDL3_SOURCE_DIR}/src/gpu/metal/starfox_metal_interop.inc")
            if(STARFOX_SDL3_METAL_SYMBOL_POS EQUAL -1
                AND STARFOX_SDL3_METAL_INCLUDE_POS EQUAL -1)
                set(STARFOX_SDL3_METAL_END "#endif // SDL_GPU_METAL")
                string(FIND "${STARFOX_SDL3_METAL_TEXT}" "${STARFOX_SDL3_METAL_END}"
                    STARFOX_SDL3_METAL_END_POS)
                if(STARFOX_SDL3_METAL_END_POS EQUAL -1)
                    message(FATAL_ERROR "SDL Metal backend guard changed upstream")
                endif()
                string(REPLACE "${STARFOX_SDL3_METAL_END}"
                    "#include \"starfox_metal_interop.inc\"\n${STARFOX_SDL3_METAL_END}"
                    STARFOX_SDL3_METAL_TEXT "${STARFOX_SDL3_METAL_TEXT}")
                file(WRITE "${STARFOX_SDL3_METAL_SOURCE}" "${STARFOX_SDL3_METAL_TEXT}")
            endif()
        endif()
        if(NOT WINDOWS_STORE)
            execute_process(COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-adapter-vendor.cmake"
                COMMAND_ERROR_IS_FATAL ANY)
        endif()
        if(STARFOX_DXR_ENABLED AND NOT WINDOWS_STORE)
            execute_process(COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-d3d12-interop.cmake"
                COMMAND_ERROR_IS_FATAL ANY)
            execute_process(COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=${sdl3_SOURCE_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl3-vulkan-interop.cmake"
                COMMAND_ERROR_IS_FATAL ANY)
            get_target_property(STARFOX_SDL_NATIVE_TARGET SDL3::SDL3 ALIASED_TARGET)
            target_include_directories(starfox_core PRIVATE "${sdl3_SOURCE_DIR}/src/video/khronos")
            target_include_directories(${STARFOX_SDL_NATIVE_TARGET} PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/render")
            if(STARFOX_BUILD_TOOLS)
                add_executable(starfox_sdl_vulkan_interop_check tools/check_sdl_vulkan_interop.cpp)
                target_link_libraries(starfox_sdl_vulkan_interop_check PRIVATE starfox_core SDL3::SDL3)
                target_include_directories(starfox_sdl_vulkan_interop_check PRIVATE
                    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${sdl3_SOURCE_DIR}/src/video/khronos")
                add_executable(starfox_sdl_d3d12_interop_check tools/check_sdl_d3d12_interop.cpp)
                target_link_libraries(starfox_sdl_d3d12_interop_check PRIVATE starfox_core SDL3::SDL3)
                if(MINGW)
                    target_include_directories(starfox_sdl_d3d12_interop_check BEFORE PRIVATE
                        "${sdl3_SOURCE_DIR}/src/video/directx")
                endif()
            endif()
        endif()
    endif()

    set(STARFOX_RUNTIME_SOURCES
        src/app/starfox_pc.cpp
        src/app/runtime_input.cpp)
    if(STARFOX_DXR_ENABLED)
        # SDL's pinned DirectX headers include DXR interfaces missing from
        # older MinGW installations. MSVC uses its Windows SDK headers.
        if(MINGW AND EXISTS "${sdl3_SOURCE_DIR}/src/video/directx/d3d12.h")
            target_include_directories(starfox_core BEFORE PRIVATE "${sdl3_SOURCE_DIR}/src/video/directx")
        endif()
    endif()
    if(ANDROID)
        # SDLActivity loads libmain.so. SDL_main.h exports SDL_main from the
        # otherwise-identical portable runtime entry point.
        add_library(starfox_pc SHARED ${STARFOX_RUNTIME_SOURCES})
        set_target_properties(starfox_pc PROPERTIES OUTPUT_NAME main)
    else()
        add_executable(starfox_pc ${STARFOX_RUNTIME_SOURCES})
    endif()
    if(IOS)
        enable_language(OBJCXX)
        target_sources(starfox_pc PRIVATE src/app/ios_runtime_input_picker.mm)
        set_source_files_properties(src/app/ios_runtime_input_picker.mm
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
        target_link_libraries(starfox_pc PRIVATE
            "-framework UIKit" "-framework UniformTypeIdentifiers")
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
        if(PROJECT_VERSION_TWEAK STREQUAL "")
            set(PROJECT_VERSION_TWEAK 0)
        endif()
        set(STARFOX_UWP_BUILD_REVISION "${PROJECT_VERSION_TWEAK}" CACHE STRING
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
        if(NOT SWITCH AND NOT VITA AND NOT WINDOWS_STORE)
            # Checked-in portable shaders must match their shared source.
            set(STARFOX_EFFECT_SOURCE_TEXT "")
            foreach(shader effects_compute.hlsl xbrz_compute.hlsli xbrz_weights.hlsli edge_corners.hlsli environment_material.hlsli backdrop_sample.hlsli ../../../include/starfox/render/ex_face_planet_regions.inc ../../../include/starfox/render/ex_city_moons.inc)
                get_filename_component(shader_source "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/${shader}" ABSOLUTE)
                file(READ "${shader_source}" shader_text)
                string(REPLACE "\r\n" "\n" shader_text "${shader_text}")
                string(APPEND STARFOX_EFFECT_SOURCE_TEXT "${shader_text}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${shader_source}")
            endforeach()
            string(SHA256 STARFOX_EFFECT_SOURCE_HASH "${STARFOX_EFFECT_SOURCE_TEXT}")
            foreach(variant 0 1)
                file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/generated/effects_portable_${variant}.hpp"
                    shader_stamp REGEX "^// Source SHA-256:" LIMIT_COUNT 1)
                if(NOT shader_stamp STREQUAL "// Source SHA-256: ${STARFOX_EFFECT_SOURCE_HASH}")
                    message(FATAL_ERROR "Portable effects shaders are stale. Run tools/generate_portable_effects.py; see docs/PORTABLE-GPU.md.")
                endif()
            endforeach()
            target_link_libraries(starfox_core PRIVATE SDL3::SDL3)
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "third_party/fsr1/ffx_a.h" "third_party/fsr1/ffx_fsr1.h")
            # Discover the entire portable family so newly migrated passes cannot
            # accidentally bypass source/header verification. raster_bins predates
            # the naming convention; effects_compute has its combined check above.
            file(GLOB native_shader_sources CONFIGURE_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/*_portable.hlsl")
            list(APPEND native_shader_sources
                "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/raster_bins.hlsl")
            foreach(native_shader_source IN LISTS native_shader_sources)
            get_filename_component(native_shader "${native_shader_source}" NAME_WE)
            file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/${native_shader}.hlsl" shadow_text)
            string(REPLACE "\r\n" "\n" shadow_text "${shadow_text}")
            string(SHA256 shadow_hash "${shadow_text}")
            if(shadow_text MATCHES "#[ \t]*include")
                find_package(Python3 REQUIRED COMPONENTS Interpreter)
                execute_process(COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/portable_shader_source.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/${native_shader}.hlsl"
                    OUTPUT_VARIABLE shadow_hash OUTPUT_STRIP_TRAILING_WHITESPACE
                    COMMAND_ERROR_IS_FATAL ANY)
                file(GLOB_RECURSE shader_helpers CONFIGURE_DEPENDS
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/*.hlsli")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                    ${shader_helpers} "${CMAKE_CURRENT_SOURCE_DIR}/tools/portable_shader_source.py")
            endif()
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "src/render/shaders/${native_shader}.hlsl"
                "src/render/shaders/generated/${native_shader}.hpp")
            file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/generated/${native_shader}.hpp"
                shadow_stamp REGEX "^// Source SHA-256:" LIMIT_COUNT 1)
            if(NOT shadow_stamp STREQUAL "// Source SHA-256: ${shadow_hash}")
                message(FATAL_ERROR "Portable shader ${native_shader} is stale. Regenerate with tools/generate_portable_shadows.py, or tools/generate_scalefx.py for ScaleFX.")
            endif()
            endforeach()
            target_compile_definitions(starfox_core PRIVATE STARFOX_SDL_GPU_EFFECTS)
            target_compile_definitions(starfox_pc PRIVATE STARFOX_SDL_GPU_EFFECTS)
            if(STARFOX_BUILD_TOOLS)
                add_executable(starfox_gpu_effects_check tools/benchmark_gpu_effects.cpp)
                add_executable(starfox_present_check tools/check_present_pacing.cpp)
                target_link_libraries(starfox_present_check PRIVATE SDL3::SDL3)
                if(MINGW)
                    target_link_options(starfox_present_check PRIVATE -static -static-libgcc -static-libstdc++)
                endif()
                add_executable(starfox_gpu_fsr1_check tools/check_gpu_fsr1.cpp)
                target_link_libraries(starfox_gpu_fsr1_check PRIVATE starfox_core SDL3::SDL3)
                target_compile_definitions(starfox_gpu_effects_check PRIVATE STARFOX_TEST_PORTABLE_GPU)
                target_link_libraries(starfox_gpu_effects_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_portable_shadows_check tools/benchmark_dxr.cpp)
                target_compile_definitions(starfox_portable_shadows_check PRIVATE STARFOX_TEST_PORTABLE_SHADOWS)
                target_link_libraries(starfox_portable_shadows_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_raster_check tools/benchmark_gpu_raster.cpp)
                target_link_libraries(starfox_gpu_raster_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_sprites_check tools/check_gpu_sprites.cpp)
                target_link_libraries(starfox_gpu_sprites_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_projection_check tools/check_gpu_projection.cpp)
                target_link_libraries(starfox_gpu_projection_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_bsp_check tools/check_gpu_bsp.cpp)
                add_executable(starfox_gpu_model_check tools/check_gpu_model.cpp)
                add_executable(starfox_gpu_terrain_batches_check tools/check_gpu_terrain_batches.cpp)
                target_link_libraries(starfox_gpu_terrain_batches_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_depth_check tools/check_gpu_depth.cpp)
                target_link_libraries(starfox_gpu_depth_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_ray_geometry_check tools/check_gpu_ray_geometry.cpp)
                add_executable(starfox_gpu_background_check tools/check_gpu_background.cpp)
                target_link_libraries(starfox_gpu_background_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_ray_geometry_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_colour_warp_check tools/check_gpu_colour_warp.cpp)
                add_executable(starfox_gpu_fp64_check tools/check_gpu_fp64.cpp)
                target_compile_features(starfox_gpu_fp64_check PRIVATE cxx_std_20)
                target_link_libraries(starfox_gpu_fp64_check PRIVATE SDL3::SDL3)
                add_executable(starfox_gpu_warp_material_check tools/check_gpu_warp_material.cpp)
                add_executable(starfox_gpu_warp_expand_check tools/check_gpu_warp_expand.cpp)
                add_executable(starfox_gpu_warp_chain_check tools/check_gpu_warp_chain.cpp)
                target_link_libraries(starfox_gpu_warp_chain_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_warp_expand_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_warp_material_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_colour_warp_check PRIVATE SDL3::SDL3)
                add_executable(starfox_euler_precision_check tools/check_euler_precision.cpp)
                target_link_libraries(starfox_euler_precision_check PRIVATE starfox_core)
                target_link_libraries(starfox_gpu_model_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_bsp_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_clip_check tools/check_gpu_clip.cpp)
                target_link_libraries(starfox_gpu_clip_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_gpu_composite_check tools/check_gpu_composite.cpp)
                add_executable(starfox_gpu_stereo_check tools/check_gpu_stereo.cpp)
                target_link_libraries(starfox_gpu_stereo_check PRIVATE starfox_core SDL3::SDL3)
                target_link_libraries(starfox_gpu_composite_check PRIVATE starfox_core SDL3::SDL3)
                add_executable(starfox_scalefx_check tools/check_scalefx.cpp)
                target_link_libraries(starfox_scalefx_check PRIVATE starfox_core SDL3::SDL3)
            endif()
        endif()
    endif()
    if(APPLE)
        enable_language(OBJCXX)
        target_sources(starfox_pc PRIVATE src/render/metal_hardware_rt.mm)
        set_source_files_properties(src/render/metal_hardware_rt.mm
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
        target_link_libraries(starfox_pc PRIVATE "-framework Metal")
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
                --resource "200=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/alpine-day-v2.bmp"
                --resource "201=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/macbeth-dusk-v1.bmp"
                --resource "202=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/storm-night-v1.bmp"
                --resource "203=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/desert-horizon-v1.bmp"
                --resource "204=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-clouds-v1.bmp"
                --resource "205=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-volcanic-v1.bmp"
                --resource "206=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/city-night-v1.bmp"
                --resource "207=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/nebula-red-blue-v1.bmp"
                --resource "208=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/cloud-plains-v1.bmp"
                --resource "209=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/gold-storm-v1.bmp"
                --resource "210=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dune-horizon-v2.bmp"
                --resource "211=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/distant-snow-v1.bmp"
                --resource "212=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/titania-clouds-v1.bmp"
                --resource "213=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ocean-clouds-v2.bmp"
                --resource "214=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/night-coast-v1.bmp"
                --resource "215=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/day-coast-v1.bmp"
                --resource "216=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/red-cloud-band-v1.bmp"
                --resource "217=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/rocky-green-v1.bmp"
                --resource "218=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dense-city-v1.bmp"
                --resource "219=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/asteroid-belt-v1.bmp"
                --resource "220=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/fine-debris-v1.bmp"
                --resource "221=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ember-nebula-v2.bmp"
                --resource "222=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/comet-corona-v1.bmp"
                --resource "223=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/cratered-planet-v1.bmp"
                --resource "224=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/storm-planet-v1.bmp"
                --resource "225=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/banded-planet-v1.bmp"
                --resource "226=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/blue-cloud-v1.bmp"
                --resource "227=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ember-sky-v1.bmp"
                --resource "228=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/jade-planet-horizon-v1.bmp"
                --resource "229=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dimension-vortex-v1.bmp"
                --resource "230=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/amber-blue-cloud-v1.bmp"
                --resource "231=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/inferno-band-v1.bmp"
                --resource "232=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/spectral-clouds-v1.bmp"
                --resource "233=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/face-moon-v1.bmp"
                --resource "234=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/deep-space-v1.bmp"
                --resource "235=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-ocean-surface-v1.bmp"
                --resource "236=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-lava-surface-v1.bmp"
            DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_runtime_assets.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/alpine-day-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/macbeth-dusk-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/storm-night-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/desert-horizon-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-clouds-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-volcanic-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/city-night-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/nebula-red-blue-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/cloud-plains-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/gold-storm-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dune-horizon-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/distant-snow-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/titania-clouds-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ocean-clouds-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/night-coast-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/day-coast-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/red-cloud-band-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/rocky-green-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dense-city-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/asteroid-belt-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/fine-debris-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ember-nebula-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/comet-corona-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/cratered-planet-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/storm-planet-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/banded-planet-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/blue-cloud-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ember-sky-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/jade-planet-horizon-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dimension-vortex-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/amber-blue-cloud-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/inferno-band-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/spectral-clouds-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/face-moon-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/deep-space-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-ocean-surface-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-lava-surface-v1.bmp"
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
            if(STARFOX_BUILD_VR)
                install(TARGETS starfox_asset_builder
                    RUNTIME DESTINATION . COMPONENT pcvr)
            endif()
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
            set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/starfox_assets.rc" APPEND PROPERTY OBJECT_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/alpine-day-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/macbeth-dusk-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/storm-night-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/desert-horizon-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-clouds-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-volcanic-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/city-night-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/nebula-red-blue-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/cloud-plains-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/gold-storm-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dune-horizon-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/distant-snow-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/titania-clouds-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ocean-clouds-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/night-coast-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/day-coast-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/red-cloud-band-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/rocky-green-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dense-city-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/asteroid-belt-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/fine-debris-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ember-nebula-v2.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/comet-corona-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/cratered-planet-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/storm-planet-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/banded-planet-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/blue-cloud-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/ember-sky-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/jade-planet-horizon-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/dimension-vortex-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/amber-blue-cloud-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/inferno-band-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/spectral-clouds-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/face-moon-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/deep-space-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-ocean-surface-v1.bmp"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/orbital-lava-surface-v1.bmp")
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
        set(STARFOX_VITA_APP_VERSION "00.07" CACHE STRING
            "Two-component PS Vita application version")
        # SCE metadata is appended to the executable segment by vita-elf-create.
        # A separate adjacent read-only segment leaves no room for that data.
        target_link_options(starfox_pc PRIVATE
            "-Wl,--defsym=__sce_headroom=0x10000")
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
    install(FILES README.md CREDITS.md THIRD_PARTY_NOTICES.md LICENSE-XBRZ.txt DESTINATION .)
    install(FILES assets/fonts/README.md assets/fonts/misaki.txt
        DESTINATION licenses/fonts)
    # Player packages contain runtime files and notices only. Development
    # audits, architecture notes and release-work checkpoints stay in source.

endif()

if(STARFOX_BUILD_TESTS)
    # Headless smoke coverage is independent of local saved graphics options.
    set(STARFOX_SMOKE_BASELINE "STARFOX_TEST_RENDERER=SOFTWARE")
    foreach(setting RAY_TRACING SOFTWARE_SHADOWS REFLECTIVE_SURFACES RTX_LIGHTING
            BLOOM BLOOM_2D EFFECT WORLD_EFFECT MATERIAL MANIPULATION HDR_EFFECT
            CHROMATIC_ABERRATION MODEL_SMOOTHING ANTI_ALIASING 2D_FILTER
            DLSS_SELECTION FSR1_SELECTION STEREO_OUTPUT ENVIRONMENT_0
            ENVIRONMENT_1 ENVIRONMENT_2 ENVIRONMENT_3 ENVIRONMENT_4 ENVIRONMENT_5)
        list(APPEND STARFOX_SMOKE_BASELINE "STARFOX_TEST_${setting}=0")
    endforeach()
    enable_testing()
    add_executable(starfox_irq_palette_tests tests/irq_palette_tests.cpp)
    target_include_directories(starfox_irq_palette_tests PRIVATE include)
    target_compile_features(starfox_irq_palette_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_irq_palette_tests COMMAND starfox_irq_palette_tests)
    add_executable(starfox_comms_timing_check tools/check_comms_timing.cpp)
    target_link_libraries(starfox_comms_timing_check PRIVATE starfox_core)
    add_executable(starfox_bps_tests tests/bps_tests.cpp)
    target_link_libraries(starfox_bps_tests PRIVATE starfox_core)
    add_test(NAME starfox_bps_tests COMMAND starfox_bps_tests)
    add_executable(starfox_pixel_filter_tests tests/pixel_filter_tests.cpp)
    add_executable(starfox_enhanced_terrain_tests tests/enhanced_terrain_tests.cpp)
    target_link_libraries(starfox_enhanced_terrain_tests PRIVATE starfox_core)
    add_test(NAME starfox_enhanced_terrain_tests COMMAND starfox_enhanced_terrain_tests)
    add_executable(starfox_backdrop_image_tests tests/backdrop_image_tests.cpp)
    target_include_directories(starfox_backdrop_image_tests PRIVATE include)
    target_compile_features(starfox_backdrop_image_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_backdrop_image_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/alpine-day-v2.bmp")
    add_test(NAME starfox_face_moon_backdrop_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/face-moon-v1.bmp")
    add_test(NAME starfox_deep_space_backdrop_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/deep-space-v1.bmp")
    add_test(NAME starfox_macbeth_backdrop_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/macbeth-dusk-v1.bmp")
    add_test(NAME starfox_storm_backdrop_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/storm-night-v1.bmp")
    add_test(NAME starfox_desert_backdrop_tests COMMAND starfox_backdrop_image_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/desert-horizon-v1.bmp")
    foreach(backdrop IN ITEMS orbital-clouds-v1 orbital-volcanic-v1 city-night-v1 nebula-red-blue-v1 cloud-plains-v1 gold-storm-v1 dune-horizon-v2 distant-snow-v1 titania-clouds-v1 ocean-clouds-v1 ocean-clouds-v2 night-coast-v1 red-cloud-band-v1 rocky-green-v1 dense-city-v1 asteroid-belt-v1 fine-debris-v1 ember-nebula-v2 lava-cavern-v1 comet-corona-v1 cratered-planet-v1 storm-planet-v1 banded-planet-v1 blue-cloud-v1 ember-sky-v1 jade-planet-horizon-v1 dimension-vortex-v1 amber-blue-cloud-v1 inferno-band-v1 spectral-clouds-v1)
        add_test(NAME starfox_${backdrop}_tests COMMAND starfox_backdrop_image_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/${backdrop}.bmp")
    endforeach()
    add_executable(starfox_frame_persistence_tests tests/frame_persistence_tests.cpp)
    target_link_libraries(starfox_frame_persistence_tests PRIVATE starfox_core)
    add_test(NAME starfox_frame_persistence_tests COMMAND starfox_frame_persistence_tests)
    target_link_libraries(starfox_pixel_filter_tests PRIVATE starfox_core)
    if(STARFOX_ENABLE_XBRZ)
        target_compile_definitions(
            starfox_pixel_filter_tests PRIVATE STARFOX_ENABLE_XBRZ)
    endif()
    add_test(
        NAME starfox_pixel_filter_tests COMMAND starfox_pixel_filter_tests)
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
    # Compile the unsupported backend independently, including on DXR hosts.
    # Do not link starfox_core, whose private STARFOX_DXR definition selects DXR.
    add_executable(starfox_dxr_stub_tests tools/check_dxr_stub.cpp src/render/dxr_shadows.cpp)
    target_include_directories(starfox_dxr_stub_tests PRIVATE include)
    target_compile_features(starfox_dxr_stub_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_dxr_stub_tests COMMAND starfox_dxr_stub_tests)

    add_executable(starfox_state_tests tests/state_tests.cpp)
    add_executable(starfox_title_logo_check tests/title_logo_tests.cpp)
    target_link_libraries(starfox_title_logo_check PRIVATE starfox_core)
    add_executable(starfox_background_transition_check tools/check_background_transitions.cpp)
    target_link_libraries(starfox_background_transition_check PRIVATE starfox_core)
    target_link_libraries(starfox_state_tests PRIVATE starfox_core)
    add_test(NAME starfox_state_tests COMMAND starfox_state_tests)
    add_executable(starfox_shadow_geometry_tests tests/shadow_geometry_tests.cpp)
    target_include_directories(starfox_shadow_geometry_tests PRIVATE include)
    target_compile_features(starfox_shadow_geometry_tests PRIVATE cxx_std_20)
    target_link_libraries(starfox_shadow_geometry_tests PRIVATE starfox_core)
    add_test(NAME starfox_shadow_geometry_tests COMMAND starfox_shadow_geometry_tests)
    add_executable(starfox_software_reflection_tests tests/software_reflection_tests.cpp)
    target_link_libraries(starfox_software_reflection_tests PRIVATE starfox_core)
    add_test(NAME starfox_software_reflection_tests COMMAND starfox_software_reflection_tests)
    add_executable(starfox_raster_commands_tests tools/benchmark_gpu_raster.cpp)
    add_executable(starfox_geometry_fp64_tests tests/geometry_fp64_tests.cpp)
    target_compile_features(starfox_geometry_fp64_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_geometry_fp64_tests COMMAND starfox_geometry_fp64_tests)
    add_executable(starfox_face_material_tests tests/face_material_tests.cpp)
    add_executable(starfox_packed_bsp_tests tests/packed_bsp_tests.cpp)
    add_executable(starfox_packed_projection_tests tests/packed_projection_tests.cpp)
    add_executable(starfox_temporal_projection_tests tests/temporal_projection_tests.cpp)
    target_include_directories(starfox_temporal_projection_tests PRIVATE include)
    target_compile_features(starfox_temporal_projection_tests PRIVATE cxx_std_20)
    add_test(NAME starfox_temporal_projection_tests COMMAND starfox_temporal_projection_tests)
    add_executable(starfox_packed_faces_tests tests/packed_faces_tests.cpp)
    target_link_libraries(starfox_packed_faces_tests PRIVATE starfox_core)
    add_test(NAME starfox_packed_faces_tests COMMAND starfox_packed_faces_tests)
    target_link_libraries(starfox_packed_projection_tests PRIVATE starfox_core)
    add_test(NAME starfox_packed_projection_tests COMMAND starfox_packed_projection_tests)
    target_link_libraries(starfox_packed_bsp_tests PRIVATE starfox_core)
    add_test(NAME starfox_packed_bsp_tests COMMAND starfox_packed_bsp_tests)
    target_link_libraries(starfox_face_material_tests PRIVATE starfox_core)
    add_test(NAME starfox_face_material_tests COMMAND starfox_face_material_tests)
    target_compile_definitions(starfox_raster_commands_tests PRIVATE STARFOX_TEST_RASTER_CPU)
    target_link_libraries(starfox_raster_commands_tests PRIVATE starfox_core)
    add_test(NAME starfox_raster_commands_tests COMMAND starfox_raster_commands_tests)
    target_link_libraries(starfox_core_tests PRIVATE starfox_core)
    add_test(NAME starfox_core_tests COMMAND starfox_core_tests)

    add_executable(starfox_msu1_pack_tests tests/msu1_pack_tests.cpp)
    target_link_libraries(starfox_msu1_pack_tests PRIVATE starfox_core)
    add_test(NAME starfox_msu1_pack_tests COMMAND starfox_msu1_pack_tests)

    add_executable(starfox_msu1_audio_tests tests/msu1_audio_tests.cpp)
    target_link_libraries(starfox_msu1_audio_tests PRIVATE starfox_core)
    add_test(NAME starfox_msu1_audio_tests COMMAND starfox_msu1_audio_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/msu-tone-320.flac")

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

    add_executable(starfox_direct_audio_tests tests/direct_audio_tests.cpp)
    target_link_libraries(starfox_direct_audio_tests PRIVATE starfox_core)

    add_executable(starfox_transition_parity_tests
        tests/transition_parity_tests.cpp)
    target_link_libraries(starfox_transition_parity_tests PRIVATE starfox_core)

    add_executable(starfox_ending_tests tests/ending_tests.cpp)
    target_link_libraries(starfox_ending_tests PRIVATE starfox_core)
    add_executable(starfox_intro_pacing_tests tests/intro_pacing_tests.cpp)
    add_executable(starfox_geometry_pacing_tests tests/geometry_pacing_tests.cpp)
    target_link_libraries(starfox_geometry_pacing_tests PRIVATE starfox_core)
    target_link_libraries(starfox_intro_pacing_tests PRIVATE starfox_core)
    if(MINGW)
        target_link_options(starfox_intro_pacing_tests PRIVATE "-Wl,--stack,16777216")
    elseif(MSVC)
        target_link_options(starfox_intro_pacing_tests PRIVATE "/STACK:16777216")
    endif()

    add_executable(starfox_stereo_output_tests tests/stereo_output_tests.cpp)
    target_link_libraries(starfox_stereo_output_tests PRIVATE starfox_core)
    add_test(NAME starfox_stereo_output_tests COMMAND starfox_stereo_output_tests)
    add_executable(starfox_hitlist_tests tests/hitlist_tests.cpp)
    add_executable(starfox_level_clear_tests tests/level_clear_tests.cpp)
    target_link_libraries(starfox_level_clear_tests PRIVATE starfox_core)
    target_link_libraries(starfox_hitlist_tests PRIVATE starfox_core)

    if(STARFOX_BUILD_RUNTIME)
        if(WIN32 AND NOT WINDOWS_STORE)
            # Requires a visible Windows desktop; run explicitly, not in headless CTest.
            add_executable(starfox_renderer_switch_tests tests/renderer_switch_tests.cpp)
            target_link_libraries(starfox_renderer_switch_tests PRIVATE SDL3::SDL3)
        endif()
        add_executable(starfox_runtime_input_tests
            tests/runtime_input_tests.cpp
            src/app/runtime_input.cpp)
        target_link_libraries(
            starfox_runtime_input_tests PRIVATE starfox_core SDL3::SDL3)
        add_test(NAME starfox_runtime_input_tests COMMAND starfox_runtime_input_tests)
        if(NOT WINDOWS_STORE AND NOT ANDROID AND NOT SWITCH AND NOT VITA AND NOT IOS)
            add_executable(starfox_controller_check tools/check_controller_route.cpp src/app/runtime_input.cpp)
            target_link_libraries(starfox_controller_check PRIVATE starfox_core SDL3::SDL3)
        endif()
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
        add_test(NAME starfox_upstream_state_data COMMAND starfox_state_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_comms_timing COMMAND starfox_comms_timing_check
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
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
        add_test(NAME starfox_upstream_intro_pacing COMMAND starfox_intro_pacing_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
        add_test(NAME starfox_upstream_geometry_pacing COMMAND starfox_geometry_pacing_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
            "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
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
            add_test(
                NAME starfox_runtime_smoke
                COMMAND starfox_pc
                        "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                        "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                        LEVEL1_1
            )
            set_tests_properties(starfox_runtime_smoke PROPERTIES
                ENVIRONMENT
                    "${STARFOX_SMOKE_BASELINE};SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_TEST_FRAMES=6;STARFOX_TEST_RENDER_SCALE=1"
                TIMEOUT 10
            )
            add_test(
                NAME starfox_exit_confirmation_smoke
                COMMAND starfox_pc
                        "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                        "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT"
                        BOOT
            )
            set_tests_properties(starfox_exit_confirmation_smoke PROPERTIES
                ENVIRONMENT
                    "${STARFOX_SMOKE_BASELINE};SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_TEST_EXIT_CONFIRMATION=1;STARFOX_TEST_FRAMES=1000000;STARFOX_TEST_PRESSES=1:0x0200,5:0x0080;STARFOX_TEST_UNPACED=1;STARFOX_TEST_SKIP_PREROLL=1;STARFOX_TEST_RENDER_SCALE=10"
                # The scripted LEFT, then A must select YES and terminate long
                # before the large frame sentinel. This also exercises the
                # confirmation compositor at the maximum upscale setting.
                TIMEOUT 10
            )
            if(STARFOX_RUNTIME_ASSETS_EMBEDDED)
                # Embedded resources are patches, not a cartridge. A clean
                # first launch requires a user-supplied retail ROM; otherwise
                # this headless fixture waits forever in the asset picker.
                if(EXISTS "${STARFOX_RETAIL_V12_FILE}")
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
                        "${STARFOX_SMOKE_BASELINE};SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_RETAIL_ROM=${STARFOX_RETAIL_V12_FILE};STARFOX_TEST_FRAMES=6;STARFOX_TEST_RENDER_SCALE=1"
                        TIMEOUT 10
                    )
                endif()
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
        add_test(NAME starfox_ex_intro_pacing COMMAND starfox_intro_pacing_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_geometry_pacing COMMAND starfox_geometry_pacing_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
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
        add_test(NAME starfox_ex_state_data COMMAND starfox_state_tests
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        add_test(NAME starfox_ex_comms_timing COMMAND starfox_comms_timing_check
            "${STARFOX_EX_ROM_FILE}" "${STARFOX_EX_SYMBOLS_FILE}")
        set_tests_properties(starfox_ex_simulation_data PROPERTIES
            ENVIRONMENT "STARFOX_EX_ROUTE_AUDIT_TICKS=2000"
            # The audit boots every shipped EX stage and advances each for
            # 2,000 source ticks. Renderer/route regressions have expanded
            # its coverage enough that Windows debug-host load can exceed
            # four minutes without a hang; retain the full audit and give it
            # a deterministic margin instead of shortening stage coverage.
            TIMEOUT 420)
        if(STARFOX_BUILD_RUNTIME AND STARFOX_RUNTIME_ASSETS_EMBEDDED)
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
                    "${STARFOX_SMOKE_BASELINE};SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;STARFOX_TEST_EXPERIENCE=EX;STARFOX_TEST_FRAMES=900;STARFOX_TEST_PRESSES=6:0x1000;STARFOX_TEST_UNPACED=1;STARFOX_TEST_RENDER_SCALE=1"
                # The embedded EX boot renders 900 full frames. Sanitizer and
                # busy Windows hosts can exceed 20 seconds without a hang.
                TIMEOUT 45
            )
        endif()
    endif()

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME starfox_backdrop_catalog
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_backdrop_catalog.py")
        add_test(NAME starfox_enhanced_palette_trace
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_enhanced_palette_trace.py")
        add_test(NAME starfox_backdrop_region_inks
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_backdrop_region_inks.py")
        add_test(NAME starfox_android_package_oracle
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_android_package.py")
        add_test(NAME starfox_quest_package_oracle
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_quest_package.py")
        if(UNIX AND NOT CMAKE_CROSSCOMPILING)
            add_test(NAME starfox_embedded_assets_roundtrip
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_embed_runtime_assets.py"
                    --compiler "${CMAKE_CXX_COMPILER}")
        endif()
        add_test(NAME starfox_scramble_sequence_oracle
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scramble_sequence.py")
        if(STARFOX_BUILD_RUNTIME AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC")
            add_test(NAME starfox_isolated_effects
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_isolated_effects.py"
                    "$<TARGET_FILE:starfox_pc>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SF.SFC"
                    "${CMAKE_CURRENT_SOURCE_DIR}/upstream-ultrastarfox/SYMBOLS.TXT")
            set_tests_properties(starfox_isolated_effects PROPERTIES TIMEOUT 240)
        endif()
        add_test(
            NAME starfox_dialogue_catalog_tests
            COMMAND "${Python3_EXECUTABLE}" -m unittest discover
                    -s "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                    -p "test_dialogue_catalog.py"
        )
        set_tests_properties(starfox_dialogue_catalog_tests PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}"
        )
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
