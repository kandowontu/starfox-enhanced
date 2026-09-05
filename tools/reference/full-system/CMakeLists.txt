cmake_minimum_required(VERSION 3.28)
project(starfox_full_reference LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT WIN32 OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "This development reference build currently supports Windows/MinGW")
endif()
if(NOT IS_DIRECTORY "${ARES_SOURCE}" OR NOT IS_DIRECTORY "${STARFOX_CORE_BUILD_DIR}")
    message(FATAL_ERROR "Set ARES_SOURCE to pinned Ares and STARFOX_CORE_BUILD_DIR to the built port")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/prepare.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/prepare.py" "${ARES_SOURCE}" "${GEN}"
    COMMAND_ERROR_IS_FATAL ANY)
add_library(ares_full STATIC
    "${GEN}/ares.cpp"
    "${ARES_SOURCE}/ares/ares/memory/fixed-allocator.cpp"
    "${ARES_SOURCE}/nall/nall/nall.cpp"
    "${ARES_SOURCE}/nall/nall/sljitAllocator.cpp"
    "${ARES_SOURCE}/libco/libco.c"
    "${ARES_SOURCE}/thirdparty/sljit/sljit_src/sljitLir.c")
foreach(part system controller cartridge memory cpu smp dsp ppu ppu-performance coprocessor expansion slot)
    if(part STREQUAL "system" OR part STREQUAL "cpu" OR part STREQUAL "coprocessor")
        target_sources(ares_full PRIVATE "${GEN}/${part}.cpp")
    elseif(part STREQUAL "ppu-performance")
        target_sources(ares_full PRIVATE "${ARES_SOURCE}/ares/sfc/${part}/ppu.cpp")
    else()
        target_sources(ares_full PRIVATE "${ARES_SOURCE}/ares/sfc/${part}/${part}.cpp")
    endif()
endforeach()
foreach(part arm7tdmi gsu hg51b spc700 upd96050 wdc65816)
    target_sources(ares_full PRIVATE "${ARES_SOURCE}/ares/component/processor/${part}/${part}.cpp")
endforeach()
target_include_directories(ares_full SYSTEM PUBLIC
    "${GEN}" "${ARES_SOURCE}/ares" "${ARES_SOURCE}/nall" "${ARES_SOURCE}" "${ARES_SOURCE}/thirdparty")
target_compile_definitions(ares_full PUBLIC
    PROFILE_ACCURACY BUILD_RELEASE SLJIT_HAVE_CONFIG_PRE=1 SLJIT_HAVE_CONFIG_POST=1)
target_compile_options(ares_full PRIVATE -w)
target_link_libraries(ares_full PUBLIC ws2_32 ole32 shell32 shlwapi)
add_executable(full_reference main.cpp)
target_compile_definitions(full_reference PRIVATE STARFOX_REFERENCE_ARES_ROOT="${ARES_SOURCE}")
target_include_directories(full_reference PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../include")
target_link_libraries(full_reference PRIVATE ares_full
    "${STARFOX_CORE_BUILD_DIR}/libstarfox_core.a"
    "${STARFOX_CORE_BUILD_DIR}/libretro_cpu_65816.a"
    "${STARFOX_CORE_BUILD_DIR}/libretro_cpu_core.a"
    "${STARFOX_CORE_BUILD_DIR}/libsnes_spc_core.a")
target_link_options(full_reference PRIVATE -static -Wl,--stack,16777216)
