# Guard every insertion against the exact pinned SDL source. Device extension
# options are validated upstream but were omitted from VkDeviceCreateInfo.
set(backend "${SOURCE_DIR}/src/gpu/vulkan/SDL_gpu_vulkan.c")
file(COPY_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../src/render/sdl_vulkan_bridge.inc"
    "${SOURCE_DIR}/src/gpu/vulkan/sdl_vulkan_bridge.inc"
    ONLY_IF_DIFFERENT)
file(READ "${backend}" code)
set(marker "/* Star Fox Vulkan interop v1 */")
set(ray_marker "/* Star Fox Vulkan ray options v1 */")
string(FIND "${code}" "${ray_marker}" ray_applied)
if(ray_applied GREATER_EQUAL 0)
    return()
endif()
string(FIND "${code}" "${marker}" applied)
function(replace_exact old new)
    string(FIND "${code}" "${old}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Unexpected pinned SDL Vulkan source: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" code "${code}")
    set(code "${code}" PARENT_SCOPE)
endfunction()
if(applied LESS 0)
    replace_exact("        &renderer->supports);\n    deviceExtensions = SDL_stack_alloc("
        "        &renderer->supports) + features->additionalDeviceExtensionCount;\n    deviceExtensions = SDL_stack_alloc(")
    replace_exact("    CreateDeviceExtensionArray(&renderer->supports, deviceExtensions);"
        "    CreateDeviceExtensionArray(&renderer->supports, deviceExtensions);\n    for (Uint32 extra = 0; extra < features->additionalDeviceExtensionCount; ++extra) {\n        deviceExtensions[GetDeviceExtensionCount(&renderer->supports) + extra] = features->additionalDeviceExtensionNames[extra];\n    }")
    replace_exact("static SDL_GPUDevice *VULKAN_CreateDevice(bool debugMode, bool preferLowPower, SDL_PropertiesID props)"
        "${marker}\n#include \"sdl_vulkan_bridge.inc\"\n\nstatic SDL_GPUDevice *VULKAN_CreateDevice(bool debugMode, bool preferLowPower, SDL_PropertiesID props)")
    replace_exact("    result->driverData = (SDL_GPURenderer *)renderer;"
        "    result->driverData = (SDL_GPURenderer *)renderer;\n    Starfox_Vulkan_PublishBridge(renderer);")
endif()
replace_exact("    VkPhysicalDeviceVulkan13Features desiredVulkan13DeviceFeatures;"
    "    VkPhysicalDeviceVulkan13Features desiredVulkan13DeviceFeatures;\n    VkPhysicalDeviceAccelerationStructureFeaturesKHR desiredAccelerationStructure;\n    VkPhysicalDeviceRayQueryFeaturesKHR desiredRayQuery;")
replace_exact("            features->desiredVulkan13DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;"
    "            features->desiredVulkan13DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;\n            SDL_zero(features->desiredAccelerationStructure);\n            SDL_zero(features->desiredRayQuery);\n            features->desiredAccelerationStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;\n            features->desiredRayQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;")
replace_exact("                        VULKAN_INTERNAL_TryAddDeviceFeatures_Vulkan_12_Or_Later(vk10Features,\n                                                                                vk11Features,\n                                                                                vk12Features,\n                                                                                vk13Features,\n                                                                                features->desiredApiVersion,\n                                                                                nextStructure);\n                        nextStructure = nextStructure->pNext;"
    "                        VULKAN_INTERNAL_TryAddDeviceFeatures_Vulkan_12_Or_Later(vk10Features,\n                                                                                vk11Features,\n                                                                                vk12Features,\n                                                                                vk13Features,\n                                                                                features->desiredApiVersion,\n                                                                                nextStructure);\n                        if (nextStructure->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR) {\n                            features->desiredAccelerationStructure.accelerationStructure = ((VkPhysicalDeviceAccelerationStructureFeaturesKHR *)nextStructure)->accelerationStructure;\n                        } else if (nextStructure->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR) {\n                            features->desiredRayQuery.rayQuery = ((VkPhysicalDeviceRayQueryFeaturesKHR *)nextStructure)->rayQuery;\n                        }\n                        nextStructure = nextStructure->pNext;")
replace_exact("        deviceCreateInfo.pEnabledFeatures = NULL;\n        deviceCreateInfo.pNext = &featureList;"
    "        deviceCreateInfo.pEnabledFeatures = NULL;\n        deviceCreateInfo.pNext = &featureList;\n        ${ray_marker}\n        if (features->desiredAccelerationStructure.accelerationStructure && features->desiredRayQuery.rayQuery) {\n            VkBaseOutStructure *tail = (VkBaseOutStructure *)&featureList;\n            while (tail->pNext) tail = tail->pNext;\n            tail->pNext = (VkBaseOutStructure *)&features->desiredAccelerationStructure;\n            features->desiredAccelerationStructure.pNext = &features->desiredRayQuery;\n            features->desiredRayQuery.pNext = NULL;\n        }")
file(WRITE "${backend}" "${code}")
