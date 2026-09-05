#!/usr/bin/env bash
set -euo pipefail

: "${VITASDK:?Set VITASDK to the VitaSDK installation}"
source_root="${1:-$(pwd)}"
build_root="${2:-${source_root}/build/vita}"
dist_root="${3:-${source_root}/dist/StarFoxEnhanced-vita}"

cmake -S "${source_root}" -B "${build_root}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${VITASDK}/share/vita.toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTARFOX_BUILD_TESTS=OFF \
    -DSTARFOX_BUILD_TOOLS=OFF \
    -DSTARFOX_BUILD_RUNTIME=ON \
    -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
cmake --build "${build_root}" --target StarFoxEnhanced.vpk-vpk

mkdir -p "${dist_root}"
cp "${build_root}/StarFoxEnhanced.vpk" "${dist_root}/"
cp "${source_root}/platform/vita/README.md" "${dist_root}/"
cp "${source_root}/platform/mobile/ASSET_BUILDER.md" \
    "${dist_root}/ASSET_BUILDER.md"
cp "${source_root}/CREDITS.md" "${dist_root}/"
cp "${source_root}/THIRD_PARTY_NOTICES.md" "${dist_root}/"

printf 'PS Vita package: %s\n' "${dist_root}/StarFoxEnhanced.vpk"
