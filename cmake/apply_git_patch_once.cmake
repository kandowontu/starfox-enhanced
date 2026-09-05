if(NOT DEFINED GIT_EXECUTABLE OR NOT DEFINED SOURCE_DIR OR NOT DEFINED PATCH_FILE)
    message(FATAL_ERROR "GIT_EXECUTABLE, SOURCE_DIR and PATCH_FILE are required")
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check "${PATCH_FILE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE patch_check_result
    OUTPUT_VARIABLE patch_check_output
    ERROR_VARIABLE patch_check_error)

if(patch_check_result EQUAL 0)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --whitespace=nowarn "${PATCH_FILE}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE patch_apply_result
        OUTPUT_VARIABLE patch_apply_output
        ERROR_VARIABLE patch_apply_error)
    if(NOT patch_apply_result EQUAL 0)
        message(FATAL_ERROR
            "Could not apply ${PATCH_FILE}:\n${patch_apply_output}${patch_apply_error}")
    endif()
else()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${PATCH_FILE}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE reverse_check_result
        OUTPUT_VARIABLE reverse_check_output
        ERROR_VARIABLE reverse_check_error)
    if(NOT reverse_check_result EQUAL 0)
        message(FATAL_ERROR
            "${PATCH_FILE} is neither applicable nor already applied.\n"
            "Apply check:\n${patch_check_output}${patch_check_error}\n"
            "Reverse check:\n${reverse_check_output}${reverse_check_error}")
    endif()
endif()
