set(msu_modes 0)
if(EXPERIENCE STREQUAL "ORIGINAL" AND TEST_MSU_PACK)
    list(APPEND msu_modes 1)
endif()
foreach(msu IN LISTS msu_modes)
    foreach(exit IN ITEMS 1 9 10 16)
        if(exit EQUAL 1)
            set(handoff_flow 10)
            set(final_flow 10)
        elseif(exit EQUAL 9)
            set(handoff_flow 13)
            set(final_flow 13)
        elseif(exit EQUAL 10)
            set(handoff_flow 11)
            set(final_flow 11)
        else()
            set(handoff_flow 9)
            set(final_flow 8)
        endif()
        set(presses "")
        if(exit EQUAL 9)
            set(presses "180:0x1000,183:0x1000,186:0x1000")
        endif()
        execute_process(COMMAND "${CMAKE_COMMAND}" -E env
            SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
            STARFOX_TEST_TIMING_MODE=ACCURATE STARFOX_TRACE_NATIVE_GAMEPLAY=1
            STARFOX_TEST_UNPACED=1
            STARFOX_TEST_SKIP_PREROLL=1 STARFOX_TEST_RENDER_SCALE=1
            STARFOX_TEST_VSYNC=0 "STARFOX_TEST_MSU1=${msu}"
            STARFOX_TRACE_MSU1=1
            "STARFOX_TEST_EXPERIENCE=${EXPERIENCE}"
            STARFOX_TEST_PRESENTATION_FPS=60 STARFOX_TEST_FRAMES=300
            "STARFOX_TEST_NATIVE_EXIT=${exit}" "STARFOX_TEST_PRESSES=${presses}"
            "${RUNTIME}" "${ROM}" "${SYMBOLS}" "${MAP}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 60)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Native exit ${EXPERIENCE}/${exit}/MSU${msu} failed: ${result}\n${output}\n${errors}")
        endif()
        if(NOT errors MATCHES "native-handoff exit=${exit} flow=${handoff_flow} ready=0")
            message(FATAL_ERROR "Native exit did not hand off to its frontend: ${errors}")
        endif()
        if(NOT errors MATCHES "native-desktop updates=13 master=0 flow=${final_flow}")
            message(FATAL_ERROR "Native gameplay restarted during the frontend or did not finish: ${errors}")
        endif()
        if(msu AND NOT errors MATCHES "msu1 enabled=1 available=1 track=[1-9][0-9]*")
            message(FATAL_ERROR "MSU handoff did not exercise an available recording: ${errors}")
        endif()
        message(STATUS "${EXPERIENCE} exit ${exit}, MSU ${msu}: ${errors}")
    endforeach()
endforeach()
