# VR needs companion validation resources even without the SDL desktop runtime.
# Embed validation patches, symbol tables and authored backdrop artwork, never
# retail/prepared ROMs. The artwork uses the shared desktop resource identities.
if(STARFOX_EMBED_RUNTIME_ASSETS)
    set(vr_resource_ids 101 102 108 109 120 121 122 123 124 125 126)
    set(vr_resource_files
        "${STARFOX_ORIGINAL_PATCH_FILE}" "${STARFOX_SYMBOLS_FILE}"
        "${STARFOX_EX_PATCH_FILE}" "${STARFOX_EX_SYMBOLS_FILE}"
        "${STARFOX_RETAIL_JAPAN_V10_PATCH_FILE}" "${STARFOX_RETAIL_JAPAN_V11_PATCH_FILE}"
        "${STARFOX_RETAIL_USA_V10_PATCH_FILE}" "${STARFOX_RETAIL_USA_V11_PATCH_FILE}"
        "${STARFOX_RETAIL_EUROPE_V10_PATCH_FILE}" "${STARFOX_RETAIL_EUROPE_V11_PATCH_FILE}"
        "${STARFOX_RETAIL_GERMANY_V10_PATCH_FILE}")
    set(vr_resource_arguments)
    foreach(index RANGE 0 10)
        list(GET vr_resource_ids ${index} identifier)
        list(GET vr_resource_files ${index} resource_file)
        if(NOT EXISTS "${resource_file}")
            message(FATAL_ERROR "VR companion validation resource ${identifier} missing: ${resource_file}")
        endif()
        list(APPEND vr_resource_arguments --resource "${identifier}=${resource_file}")
    endforeach()
    include("${CMAKE_CURRENT_LIST_DIR}/EnhancedBackdropAssets.cmake")
    starfox_enhanced_backdrop_resources(vr_backdrop_arguments vr_backdrop_files)
    list(APPEND vr_resource_arguments ${vr_backdrop_arguments})
    list(APPEND vr_resource_files ${vr_backdrop_files})
    set(vr_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(vr_generated_assets "${vr_generated_dir}/vr_embedded_assets.cpp")
    set(vr_split_outputs)
    set(vr_embed_options)
    if(ANDROID)
        # Quest has the same limited CI/compiler memory as the Android build.
        # A single translation unit containing all backdrop BMPs can exhaust
        # the hosted runner before it can report a compiler error.
        list(APPEND vr_embed_options --chunk-bytes 8192 --split-dir
            "${vr_generated_dir}")
        foreach(identifier IN LISTS vr_resource_ids)
            list(APPEND vr_split_outputs
                "${vr_generated_dir}/embedded_resource_${identifier}.cpp")
        endforeach()
        list(LENGTH vr_backdrop_files vr_backdrop_count)
        math(EXPR vr_backdrop_last_id "199 + ${vr_backdrop_count}")
        foreach(identifier RANGE 200 ${vr_backdrop_last_id})
            list(APPEND vr_split_outputs
                "${vr_generated_dir}/embedded_resource_${identifier}.cpp")
        endforeach()
    endif()
    add_custom_command(OUTPUT "${vr_generated_assets}" ${vr_split_outputs}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_runtime_assets.py"
            --output "${vr_generated_assets}" ${vr_embed_options}
            ${vr_resource_arguments}
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_runtime_assets.py" ${vr_resource_files}
        VERBATIM)
    target_sources(starfox_vr_game PRIVATE "${vr_generated_assets}"
        ${vr_split_outputs})
    target_compile_definitions(starfox_vr_game PRIVATE STARFOX_VR_BUNDLE_ASSETS=1)
endif()
