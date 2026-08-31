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
    -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
cmake --build "${build_root}" -j "$(nproc)"

# A clean public checkout deliberately contains no cartridge-derived runtime
# bundle. Developers who generated one locally still get the full SDL smoke
# and frame-rate matrix without making that private file part of the build.
runtime_bundle="${source_root}/dist/StarFoxEnhanced/Starfox-Assets.BIN"
if [[ -f "${runtime_bundle}" ]]; then
    cp "${runtime_bundle}" "${build_root}/Starfox-Assets.BIN"
fi

ctest --test-dir "${build_root}" --output-on-failure
if [[ -f "${build_root}/Starfox-Assets.BIN" ]]; then
    "${source_root}/tools/test_runtime_matrix.sh" \
        "${build_root}/starfox_pc"
else
    echo "Skipping runtime timing matrix: private Starfox-Assets.BIN is unavailable."
fi
cmake --install "${build_root}" --prefix "${install_root}"
