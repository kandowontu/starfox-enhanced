#!/usr/bin/env bash
set -euo pipefail

: "${DEVKITPRO:?Set DEVKITPRO to the devkitPro installation}"
source_root="${1:-$(pwd)}"
build_root="${source_root}/build/switch"
toolchain="${DEVKITPRO}/cmake/Switch.cmake"

cmake -S "${source_root}" -B "${build_root}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${toolchain}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTARFOX_BUILD_TESTS=OFF \
    -DSTARFOX_BUILD_RUNTIME=ON \
    -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
cmake --build "${build_root}" --target starfox_pc
