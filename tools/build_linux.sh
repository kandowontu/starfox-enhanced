#!/usr/bin/env bash
set -euo pipefail

source_root="${1:-$(pwd)}"
build_root="${2:-/tmp/starfox-enhanced-linux-x64}"
install_root="${3:-${source_root}/dist/StarFoxEnhanced-linux-x64}"

cmake -S "${source_root}" -B "${build_root}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSTARFOX_BUILD_TESTS=ON \
    -DSTARFOX_BUILD_RUNTIME=ON \
    -DSTARFOX_EMBED_RUNTIME_ASSETS=ON \
    -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF \
    -DSTARFOX_EX_ROM_FILE="${source_root}/tmp/runtime-inputs/starfox-ex/SFES.SFC" \
    -DSTARFOX_EX_SYMBOLS_FILE="${source_root}/tmp/runtime-inputs/starfox-ex/SYMBOLS.TXT"
cmake --build "${build_root}" --target starfox_pc starfox_core_tests -j "$(nproc)"
ctest --test-dir "${build_root}" -R '^starfox_core_tests$' --output-on-failure
cmake --install "${build_root}" --prefix "${install_root}"
