#!/usr/bin/env bash
set -euo pipefail

source_root="${1:-$(pwd)}"
configuration="${2:-debug}"
android_root="${source_root}/platform/android"
aar_name="SDL3-3.4.14.aar"
aar_path="${android_root}/app/libs/${aar_name}"
archive="${source_root}/tmp/SDL3-devel-3.4.14-android.zip"
archive_sha256="e41691e75433b2a0a75685781bed2160fe4a85f75f3803f7f43d1811e212e3ef"
archive_url="https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-devel-3.4.14-android.zip"

mkdir -p "$(dirname "${aar_path}")" "$(dirname "${archive}")"
if [[ ! -f "${aar_path}" ]]; then
    curl --fail --location --retry 3 --output "${archive}" "${archive_url}"
    echo "${archive_sha256}  ${archive}" | sha256sum --check --status
    unzip -p "${archive}" "${aar_name}" > "${aar_path}"
fi

case "${configuration}" in
debug)
    gradle_task=assembleDebug
    apk_source="${android_root}/app/build/outputs/apk/debug/app-debug.apk"
    ;;
release)
    gradle_task=assembleRelease
    apk_source="${android_root}/app/build/outputs/apk/release/app-release-unsigned.apk"
    ;;
*)
    echo "usage: $0 [source-root] [debug|release]" >&2
    exit 2
    ;;
esac

(cd "${android_root}" && ./gradlew --no-daemon ":app:${gradle_task}")
mkdir -p "${source_root}/dist"
cp "${apk_source}" "${source_root}/dist/StarFoxEnhanced-android-arm64.apk"
