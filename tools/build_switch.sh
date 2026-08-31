#!/usr/bin/env bash
set -euo pipefail

: "${DEVKITPRO:?Set DEVKITPRO to the devkitPro installation}"
source_root="${1:-$(pwd)}"
build_root="${source_root}/build/switch"
toolchain="${DEVKITPRO}/cmake/Switch.cmake"
dist_root="${source_root}/dist/StarFoxEnhanced-switch"

cmake -S "${source_root}" -B "${build_root}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${toolchain}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTARFOX_BUILD_TESTS=OFF \
    -DSTARFOX_BUILD_RUNTIME=ON \
    -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
cmake --build "${build_root}" --target starfox_pc
cmake --build "${build_root}" --target starfox_pc_nro

mkdir -p "${dist_root}/switch/StarFoxEnhanced"
cp "${build_root}/StarFoxEnhanced.nro" \
    "${dist_root}/switch/StarFoxEnhanced/StarFoxEnhanced.nro"
cp "${source_root}/platform/switch/README.md" "${dist_root}/README.md"
cp "${source_root}/platform/mobile/ASSET_BUILDER.md" \
    "${dist_root}/ASSET_BUILDER.md"
cp "${source_root}/tools/package_switch_nsp.ps1" "${dist_root}/"
