# Later timing fixes may change lines introduced by earlier patches. Validate
# cumulative file contents rather than reverse-checking each patch in isolation.
foreach(required GIT_EXECUTABLE SOURCE_DIR CHECK_DIR GIT_REVISION PATCH_FILES)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE revision
    OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT revision STREQUAL GIT_REVISION)
    message(FATAL_ERROR "RetroCPU checkout is not at the pinned revision")
endif()

set(patched_files cpu/65816/cpu_65c816.h cpu/65816/cpu_65c816.cc
    cpu/65816/cpu_65c816_instructions.inl)
file(MAKE_DIRECTORY "${CHECK_DIR}/cpu/65816")
execute_process(COMMAND "${GIT_EXECUTABLE}" init --quiet "${CHECK_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)
foreach(path IN LISTS patched_files)
    execute_process(COMMAND "${GIT_EXECUTABLE}" show "${GIT_REVISION}:${path}"
        WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE pristine
        COMMAND_ERROR_IS_FATAL ANY)
    file(WRITE "${CHECK_DIR}/${path}" "${pristine}")
endforeach()

function(matches_expected result)
    set(matches TRUE)
    foreach(path IN LISTS patched_files)
        file(READ "${SOURCE_DIR}/${path}" actual)
        file(READ "${CHECK_DIR}/${path}" expected)
        # Git may check out the Windows dependency with CRLF.
        string(REPLACE "\r\n" "\n" actual "${actual}")
        string(REPLACE "\r\n" "\n" expected "${expected}")
        string(SHA256 actual_hash "${actual}")
        string(SHA256 expected_hash "${expected}")
        if(NOT actual_hash STREQUAL expected_hash)
            set(matches FALSE)
        endif()
    endforeach()
    set(${result} ${matches} PARENT_SCOPE)
endfunction()

set(matched_prefix -1)
matches_expected(matches)
if(matches)
    set(matched_prefix 0)
endif()
set(index 0)
foreach(patch IN LISTS PATCH_FILES)
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply --whitespace=nowarn "${patch}"
        WORKING_DIRECTORY "${CHECK_DIR}" COMMAND_ERROR_IS_FATAL ANY)
    math(EXPR index "${index} + 1")
    matches_expected(matches)
    if(matches)
        set(matched_prefix ${index})
    endif()
endforeach()
if(matched_prefix LESS 0)
    message(FATAL_ERROR "RetroCPU files differ from every supported patch prefix; preserving the checkout")
endif()

set(index 0)
foreach(patch IN LISTS PATCH_FILES)
    if(index GREATER_EQUAL matched_prefix)
        execute_process(COMMAND "${GIT_EXECUTABLE}" apply --check "${patch}"
            WORKING_DIRECTORY "${SOURCE_DIR}" COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND "${GIT_EXECUTABLE}" apply --whitespace=nowarn "${patch}"
            WORKING_DIRECTORY "${SOURCE_DIR}" COMMAND_ERROR_IS_FATAL ANY)
    endif()
    math(EXPR index "${index} + 1")
endforeach()
matches_expected(matches)
if(NOT matches)
    message(FATAL_ERROR "RetroCPU patch series did not produce the expected files")
endif()
