#!/usr/bin/env bash
set -euo pipefail

source_root="${1:-$(pwd)}"
platform="${2:-macos}"
case "${platform}" in
macos)
    build_root="${source_root}/build/macos-universal"
    cmake -S "${source_root}" -B "${build_root}" -G Xcode \
        -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
        -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
        -DSTARFOX_BUILD_TESTS=ON \
        -DSTARFOX_EMBED_RUNTIME_ASSETS=ON \
        -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
    ;;
ios)
    build_root="${source_root}/build/ios"
    cmake -S "${source_root}" -B "${build_root}" -G Xcode \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
        -DCMAKE_OSX_SYSROOT=iphoneos \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
        -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO \
        -DSTARFOX_BUILD_TESTS=OFF \
        -DSTARFOX_EMBED_RUNTIME_ASSETS=ON \
        -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
    ;;
*)
    echo "usage: $0 [source-root] [macos|ios]" >&2
    exit 2
    ;;
esac
cmake --build "${build_root}" --config Release --target starfox_pc

if [[ "${platform}" == "macos" ]]; then
    cmake --build "${build_root}" --config Release --target starfox_asset_builder
    cmake --build "${build_root}" --config Release --target starfox_core_tests
    ctest --test-dir "${build_root}" -C Release \
        -R '^starfox_core_tests$' --output-on-failure
fi
