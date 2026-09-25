# Checked-in VR binaries must follow their HLSL and local include graph.
# Validation requires Python, never a shader compiler on an end-user build.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
foreach(starfox_vr_generator IN ITEMS generate_vr_shaders.py generate_vr_ray_shader.py)
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/${starfox_vr_generator}" --check
        COMMAND_ERROR_IS_FATAL ANY)
endforeach()
file(GLOB_RECURSE starfox_vr_shader_inputs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vr/shaders/*.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vr/shaders/*.hlsli"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders/*.hlsli"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/starfox/render/*.inc")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${starfox_vr_shader_inputs}
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vr/shaders/scene_spirv.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vr/shaders/ray_expand_spirv.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_vr_shaders.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_vr_ray_shader.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/portable_shader_source.py")
