#!/usr/bin/env bash
set -euo pipefail

: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to the installed Android NDK}"
source_root="${1:-$(pwd)}"
abi="${2:-arm64-v8a}"
build_root="${source_root}/build/android-${abi}"

cmake -S "${source_root}" -B "${build_root}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="${abi}" \
    -DANDROID_PLATFORM=android-26 \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTARFOX_BUILD_TESTS=OFF \
    -DSTARFOX_BUILD_RUNTIME=ON \
    -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
cmake --build "${build_root}" --target starfox_pc
