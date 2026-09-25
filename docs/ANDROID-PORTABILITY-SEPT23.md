# Android portability and package checkpoint — September 23

The ordinary Android rebuild exposed a real Clang compilation failure:
`environment_names` deduced `const char*` elements, so its index assertions
compared string-literal addresses. GCC accepted those particular expressions,
but their value is unspecified and Clang rejected them as constant expressions.
The table now explicitly stores `std::string_view`; assertions compare contents.
Symbol lookup still receives the literal's null-terminated data.

The ordinary Android workflow ships `assembleDebug`. Its native compilation
previously had debug symbols but no optimization option (effective `-O0`).
The debug variant now explicitly uses `-g -O2` for C and C++, preserving Java
debugging and native symbols. The generated CMake cache and Ninja commands
confirm the optimization is actually applied, rather than merely requested in
an unused Gradle block. No physical-device FPS gain is claimed yet.

Eight newer backdrop files were present in the embedding command but absent
from its `DEPENDS` list. They are now dependencies too, so editing their pixels
invalidates the portable embedded asset build.

## Artifact integrity

The Android GitHub workflow now invokes `check_android_package.py --source-root`.
It derives the active backdrop list from the portable CMake resource declarations
instead of indiscriminately collecting obsolete BMP revisions. The checker
requires every full BMP byte sequence in the APK's actual native library.
It also checks required arm64 ELF runtimes, duplicate/CRC failures, and rejects
wrong-ABI libraries, the Quest entry point, system stubs, ROM/BIN bundles,
signing keys and development docs. Android and Quest negative package fixtures
are registered with CTest; both pass.

## Current evidence

- Ordinary Android `:app:assembleDebug` succeeds in 2m18s after the fixes.
- `platform/android/app/.cxx/Debug/2a843593/arm64-v8a/CMakeCache.txt`
  contains `CMAKE_CXX_FLAGS_DEBUG:STRING=-g -O2`; actual Ninja commands match.
- APK checker verifies **35 complete backdrops**, totaling 165,139,066 source
  bytes, including the latest comet and Fortuna artwork.
- `aapt` confirms `com.starfox.enhanced`, API 26/35, arm64-v8a and the correct
  ordinary Android launch activity. `apksigner verify` succeeds.
- Local APK is **debug-signed**, not signed with the permanent release key.
  It was not installed or uploaded. CI retains permanent signing/cert checks.
- Windows runtime rebuild and both package CTests pass after the portability
  change. No new physical Android fullscreen/input/rendering evidence exists.

APK: `platform/android/app/build/outputs/apk/debug/app-debug.apk`

Current SHA-256 after menu contrast, terrain batching and clip precision:
`61f6f4827db3684f256638918cd6c77081d091f85a390faf6c2f9adcee6f7a94`.
The final incremental build succeeds in 38s and passes the same 35-resource
payload check (`tmp/terrain-edge-android{,-payload}-sep23.log`). Earlier
city-moon and optimization artifact hashes remain in their original logs.

Logs: `tmp/android-backdrops-sep22.log` (original Clang failure),
`tmp/android-optimized-sep23.log`, `tmp/android-payload-sep23.log`,
`tmp/android-portability-windows-sep23.log`.

This is not a GitHub workflow run, release, Quest update, #48 reporter
confirmation or a claim that the overall goal is complete.
