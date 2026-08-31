set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

if(NOT DEFINED LLVM_MINGW_ROOT)
    if(DEFINED ENV{LLVM_MINGW_ROOT})
        file(TO_CMAKE_PATH "$ENV{LLVM_MINGW_ROOT}" LLVM_MINGW_ROOT)
    else()
        message(FATAL_ERROR
            "Set LLVM_MINGW_ROOT to the extracted llvm-mingw directory")
    endif()
endif()
file(TO_CMAKE_PATH "${LLVM_MINGW_ROOT}" LLVM_MINGW_ROOT)
set(LLVM_MINGW_ROOT "${LLVM_MINGW_ROOT}" CACHE PATH
    "Extracted llvm-mingw root" FORCE)
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_MINGW_ROOT)

set(_llvm_mingw_bin "${LLVM_MINGW_ROOT}/bin")
set(CMAKE_C_COMPILER
    "${_llvm_mingw_bin}/i686-w64-mingw32-clang.exe")
set(CMAKE_CXX_COMPILER
    "${_llvm_mingw_bin}/i686-w64-mingw32-clang++.exe")
set(CMAKE_RC_COMPILER
    "${_llvm_mingw_bin}/i686-w64-mingw32-windres.exe")
set(CMAKE_AR "${_llvm_mingw_bin}/i686-w64-mingw32-ar.exe")
set(CMAKE_RANLIB "${_llvm_mingw_bin}/i686-w64-mingw32-ranlib.exe")

set(CMAKE_FIND_ROOT_PATH
    "${LLVM_MINGW_ROOT}/i686-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
