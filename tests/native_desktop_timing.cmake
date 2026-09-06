foreach(fps IN ITEMS 20 60 144)
    math(EXPR frames "${fps} * 3")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
        STARFOX_TEST_NATIVE_GAMEPLAY=1 STARFOX_TEST_UNPACED=1
        STARFOX_TEST_SKIP_PREROLL=1 STARFOX_TEST_RENDER_SCALE=1
        STARFOX_TEST_VSYNC=0 STARFOX_TEST_MSU1=0
        "STARFOX_TEST_EXPERIENCE=${EXPERIENCE}"
        "STARFOX_TEST_PRESENTATION_FPS=${fps}" "STARFOX_TEST_FRAMES=${frames}"
        "${RUNTIME}" "${ROM}" "${SYMBOLS}" "${MAP}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 45)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Native desktop ${EXPERIENCE}/${fps} failed: ${result}\n${output}\n${errors}")
    endif()
    string(REGEX MATCH "native-desktop updates=([1-9][0-9]*) master=([1-9][0-9]*) flow=([0-9]+)" state "${errors}")
    if(NOT state)
        message(FATAL_ERROR "Native desktop did not complete live gameplay: ${errors}")
    endif()
    if(DEFINED expected AND NOT state STREQUAL expected)
        message(FATAL_ERROR "Presentation FPS changed native pace: ${expected} versus ${state}")
    endif()
    set(expected "${state}")
    message(STATUS "${EXPERIENCE} ${fps} FPS: ${state}")
endforeach()
