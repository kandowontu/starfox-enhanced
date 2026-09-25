#!/usr/bin/env bash
set -euo pipefail

source_root="${1:-$(pwd)}"
configuration="${2:-debug}"
module="${3:-app}"
case "${module}" in
app) module_root="${source_root}/platform/android/app"; package_name=android-arm64 ;;
quest) module_root="${source_root}/platform/quest"; package_name=quest-arm64 ;;
*) echo 'module must be app or quest' >&2; exit 2 ;;
esac
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
    apk_source="${module_root}/build/outputs/apk/debug/${module}-debug.apk"
    ;;
release)
    gradle_task=assembleRelease
    apk_source="${module_root}/build/outputs/apk/release/${module}-release-unsigned.apk"
    if [[ -n "${ANDROID_RELEASE_KEYSTORE_PATH:-}" ]]; then
        apk_source="${module_root}/build/outputs/apk/release/${module}-release.apk"
    fi
    ;;
*)
    echo "usage: $0 [source-root] [debug|release] [app|quest]" >&2
    exit 2
    ;;
esac

(cd "${android_root}" && ./gradlew --no-daemon ":${module}:${gradle_task}")
mkdir -p "${source_root}/dist"
cp "${apk_source}" "${source_root}/dist/StarFoxEnhanced-${package_name}.apk"
