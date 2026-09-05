# 0.0.4 ALPHA

This alpha includes the latest shared runtime fixes for Star Fox and Star Fox
EX, plus the first native PS Vita package.

## Highlights

- Repaired ending camera/task state and sequence progression through Pepper,
  stage scores, totals, boss roll, credits and the final score screen. Corrected
  boss-roll scrolling, text, palettes and layering.
- Audited every level-clear routine family in both games. Removed an unintended
  launch slowdown from clear sequences, fixed the EX Earth-clear countdown
  hang, and corrected score/portrait visibility and cleanup before warp.
- Fixed EX's direct Venom Space-to-Base handoff, map planet rotation cadence,
  stale reticle/transition state, background scroll wrapping and tunnel
  floor/ceiling scanline selection.
- Improved EX reticle interpolation and smoothed first-person cockpit marker
  triangles at 60+ FPS. Fixed scheduler double-updates that could accelerate
  objects when another object removed itself.
- Limited Render Upscale to 4x, repaired software-renderer switching, and added
  options for mobile on-screen controls and swapping A/B and X/Y.
- Restored non-MSU black-hole map music, bounded audio backlog after stalls,
  and reduced Switch audio startup buffering with native Nintendo bindings.
- Reworked Xbox UWP startup/runtime dependencies and enabled the native WinRT
  controller backend, with diagnostic logging and LocalState asset storage.
- Added the native PS Vita VPK target, persistent data storage and controls.

## Downloads and setup

Builds cover Windows x64/x86, Linux x64, unsigned universal macOS, unsigned
iOS arm64, Android arm64, Xbox UWP x64 (Developer Mode), Switch homebrew NRO,
and PS Vita VPK. A standalone Windows asset builder is included separately.

Game assets are not redistributed. Use a supported clean retail ROM with the
included runtime or asset builder to prepare `Starfox-Assets.BIN`. Follow each
platform's included instructions for placing/importing it. Vita uses
`ux0:data/StarFoxEnhanced/`; Xbox uses the app's `LocalState` directory.

`Starfox-MSU1.PAK` remains an optional, separate companion download. It is not
embedded in the executables. Existing compatible BIN/PAK files can be reused.
Android uses `com.starfox.enhanced`. Xbox package version is **0.0.4.0**; install
its included public certificate and x64 VCLibs dependency as instructed.

## Validation and remaining checks

- Local full CTest suite: **35/35 passed**. Source-driven clear coverage includes
  **244 clear runs plus four colony transitions**, and both complete ending
  regression suites. Final tally/portrait/warp captures were rechecked.
- These are source fixtures and desktop checks, not proof of a complete
  hardware-reference playthrough. Original pace still approximates SNES
  workload slowdown; it is not cycle-exact emulation.
- Xbox Series S boot was confirmed on an earlier candidate. The new controller
  fix still needs on-console confirmation. Switch's reported immediate audio
  delay needs on-device measurement. Vita boot, controls, audio and performance
  remain hardware tests; start with 1x Render Upscale.
- iOS requires user signing/sideloading; macOS is unsigned. Switch provides an
  NRO and a local NSP-forwarder helper, not a prebuilt NSP. Vita requires a
  homebrew-capable device.

See `docs/ACTIVE-HITLIST.md`, `docs/LEVEL-CLEAR-VALIDATION.md` and
`docs/ENDING-VALIDATION.md` for detailed test coverage and remaining visual
checks. SHA-256 checksums are supplied alongside the release downloads.

Build provenance: the runtime is tagged at `bdb046a`. Seven platform targets
and the asset builder were packaged by GitHub Actions; Xbox and Vita were
rebuilt and verified locally from the same source after CI packaging/runner
failures. The overall CI run is therefore not green. See
[`RELEASE-0.0.4-VALIDATION.md`](https://github.com/kandowontu/starfox-enhanced/blob/main/docs/RELEASE-0.0.4-VALIDATION.md)
for details.

**Full changelog:** https://github.com/kandowontu/starfox-enhanced/compare/v0.0.3...v0.0.4
