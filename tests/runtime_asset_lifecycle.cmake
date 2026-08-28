if(NOT DEFINED RUNTIME OR NOT EXISTS "${RUNTIME}")
    message(FATAL_ERROR "RUNTIME must name the built starfox_pc executable")
endif()
if(NOT DEFINED RETAIL OR NOT EXISTS "${RETAIL}")
    message(FATAL_ERROR "RETAIL must name the clean Star Fox USA v1.2 ROM")
endif()
foreach(required VERIFIER ORIGINAL_ROM ORIGINAL_SYMBOLS EX_ROM EX_SYMBOLS)
    if(NOT DEFINED ${required} OR NOT EXISTS "${${required}}")
        message(FATAL_ERROR "${required} must name its pinned build output")
    endif()
endforeach()
if(NOT DEFINED WORK_DIR OR WORK_DIR STREQUAL "")
    message(FATAL_ERROR "WORK_DIR is required")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")
get_filename_component(runtime_name "${RUNTIME}" NAME)
set(copied_runtime "${WORK_DIR}/${runtime_name}")
set(companion "${WORK_DIR}/Starfox-Assets.BIN")
set(companion_temporary "${companion}.tmp")
set(bad_rom "${WORK_DIR}/not-star-fox-v12.sfc")
file(REMOVE "${copied_runtime}" "${companion}" "${companion_temporary}"
    "${bad_rom}")
file(COPY_FILE "${RUNTIME}" "${copied_runtime}" ONLY_IF_DIFFERENT)
file(WRITE "${bad_rom}" "not a Star Fox cartridge")

function(run_runtime result_var retail_path experience frames)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "SDL_VIDEODRIVER=dummy"
            "SDL_AUDIODRIVER=dummy"
            "STARFOX_RETAIL_ROM=${retail_path}"
            "STARFOX_TEST_EXPERIENCE=${experience}"
            "STARFOX_TEST_FRAMES=${frames}"
            "STARFOX_TEST_UNPACED=1"
            "${copied_runtime}"
        WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 20)
    set(${result_var} "${result}" PARENT_SCOPE)
    set(runtime_output "${output}${error}" PARENT_SCOPE)
endfunction()

run_runtime(bad_result "${bad_rom}" ORIGINAL 1)
if(bad_result EQUAL 0
   OR NOT runtime_output MATCHES "not an unmodified Star Fox USA v1.2")
    message(FATAL_ERROR
        "runtime accepted a non-v1.2 first-run cartridge:\n${runtime_output}")
endif()
if(EXISTS "${companion}" OR EXISTS "${companion_temporary}")
    message(FATAL_ERROR "rejected first-run cartridge left an asset companion")
endif()

run_runtime(first_result "${RETAIL}" ORIGINAL 6)
if(NOT first_result EQUAL 0)
    message(FATAL_ERROR
        "runtime could not compile its first-run companion:\n${runtime_output}")
endif()
if(NOT EXISTS "${companion}")
    message(FATAL_ERROR "first run did not create Starfox-Assets.BIN")
endif()
file(READ "${companion}" companion_magic LIMIT 8 HEX)
if(NOT companion_magic STREQUAL "53464f5841533031")
    message(FATAL_ERROR "first-run companion has the wrong format signature")
endif()
if(EXISTS "${companion_temporary}")
    message(FATAL_ERROR "first run left its temporary companion behind")
endif()
execute_process(
    COMMAND "${VERIFIER}" --verify-bundle "${companion}"
        "${ORIGINAL_ROM}" "${ORIGINAL_SYMBOLS}" "${EX_ROM}" "${EX_SYMBOLS}"
    RESULT_VARIABLE verify_result
    OUTPUT_VARIABLE verify_output
    ERROR_VARIABLE verify_error
    TIMEOUT 20)
if(NOT verify_result EQUAL 0)
    message(FATAL_ERROR
        "compiled companion differs from pinned assembler outputs:\n"
        "${verify_output}${verify_error}")
endif()
file(SHA256 "${companion}" first_companion_hash)

# A damaged companion must be replaced from the validated retail image rather
# than reaching either experience with partial or stale assets.
file(WRITE "${companion}" "corrupt companion")
run_runtime(rebuild_result "${RETAIL}" ORIGINAL 6)
if(NOT rebuild_result EQUAL 0)
    message(FATAL_ERROR
        "runtime could not reconstruct a corrupt companion:\n${runtime_output}")
endif()
file(READ "${companion}" rebuilt_magic LIMIT 8 HEX)
if(NOT rebuilt_magic STREQUAL "53464f5841533031")
    message(FATAL_ERROR "runtime did not replace its corrupt companion")
endif()
file(SHA256 "${companion}" rebuilt_companion_hash)
if(NOT rebuilt_companion_hash STREQUAL first_companion_hash)
    message(FATAL_ERROR "companion reconstruction was not deterministic")
endif()

# The explicit path is intentionally absent. A successful EX boot proves the
# second launch used the compiled companion and never tried to reopen retail.
run_runtime(second_result "${WORK_DIR}/retail-is-deliberately-absent.sfc" EX 900)
if(NOT second_result EQUAL 0)
    message(FATAL_ERROR
        "EX could not boot from the compiled companion alone:\n${runtime_output}")
endif()
file(SHA256 "${companion}" second_companion_hash)
if(NOT second_companion_hash STREQUAL first_companion_hash)
    message(FATAL_ERROR "companion-only EX boot modified the asset payload")
endif()

message(STATUS "runtime asset lifecycle passed")
