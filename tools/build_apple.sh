#!/usr/bin/env bash
set -euo pipefail

source_root="${1:-$(pwd)}"
platform="${2:-macos}"
case "${platform}" in
macos)
    build_root="${source_root}/build/macos-universal"
    cmake -S "${source_root}" -B "${build_root}" -G Xcode \
        -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' \
        -DSTARFOX_BUILD_TESTS=OFF
    ;;
ios)
    build_root="${source_root}/build/ios"
    cmake -S "${source_root}" -B "${build_root}" -G Xcode \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
        -DSTARFOX_BUILD_TESTS=OFF
    ;;
*)
    echo "usage: $0 [source-root] [macos|ios]" >&2
    exit 2
    ;;
esac
cmake --build "${build_root}" --config Release --target starfox_pc
