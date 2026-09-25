# Release-readiness audit — September 24, 2026

This is a source/build checkpoint, **not** release or hardware sign-off. The
worktree contains substantial ongoing changes; no tag, release, issue state, or
public artifact was changed in this pass.

## Corrected in the consolidated pass

- GPU ray-caster preparation now omits whole-object sprite billboards, matching
  the software shadow caster. A sprite no longer invalidates otherwise usable
  model ray geometry in the same scene. The GPU ray geometry checker covers
  sprite-only and mixed model/sprite scenes.
- Desktop setup-menu A/B mapping is consistent on the controller-remap page:
  South/A confirms and East/B backs out. The displayed remap action order now
  matches the actual controls without changing saved binding indices.
- The Cheats page fits without a redundant Back row. The HUD editor has separate
  Y Reset, B Cancel, and A Apply actions; Cancel restores the original layout,
  Apply persists it, and leaving an unfinished edit no longer saves it. The
  translated footer is short enough to fit its three buttons.
- The Quest package oracle now validates the current 37-resource backdrop
  catalogue and its newest entries instead of rejecting any catalogue larger
  than the former 35-resource snapshot.
- A real-SPC Original route-2 stage-select audio regression was added. It
  passes on desktop; this does **not** resolve the Android-only report by itself.
- Corneria and Original Training Enhanced Sky now use the existing green-hills
  panorama instead of snowy alpine artwork. Original/EX desktop and VR scene
  captures were inspected; no new resource or bitmap was added.

## Evidence from this source state

- Windows desktop application and checkers compile. The full 104-case CTest
  suite passed after the last source/header change (104/104, 198.81 seconds).
- The ray geometry checker passes on available D3D12/Vulkan adapters. The
  German HUD editor capture in `tmp/release-audit-2026-09-24` was inspected;
  all three footer labels fit.
- The ordinary Android and Quest arm64 Debug APKs both build. Payload checks
  pass with all 37 backdrop resources and no bundled ROM/BIN.
- Local debug outputs: `platform/android/app/build/outputs/apk/debug/app-debug.apk`
  (SHA-256 `46D1F326863E03B0C9737925B245D6F6D976A5B3F0077CC1AE7EF929D7756889`)
  and `platform/quest/build/outputs/apk/debug/quest-debug.apk`
  (SHA-256 `7E8D21D978139F283FEF77B276473EDCEF3A43FA65F82B42A238AD72F9457CAF`).
- The PCVR executable builds; all six selected VR application, packet,
  backdrop, and Original/EX game-input tests pass.
- These are local build, package, and sampled rendering results, not sustained
  FPS, whole-route visual parity, or physical-device acceptance.

## Remaining release gates

1. **Hardware:** native Steam Deck Game Mode controls (issue #44); the first
   Titania corridor on the affected Android device (issue #48); ordinary
   Android GPU startup/performance and white/split-frame reports (issue #64
   and the Retroid report); iPhone 4× intro/menu/gameplay stability. Desktop
   and package tests cannot establish any of these.
2. **Headsets:** PCVR/Quest acceptance for the panorama seam and ground join,
   Space Armada cruiser/boss instances, obstacle/cockpit stereo depth,
   boss-room surround and post-clear letterbox, tunnel flicker, ground-dot
   shimmer, and SBS depth. See `PCVR-TESTER-FEEDBACK-2026-09-24.md` for
   specifics. ScaleX/xBR in VR and replacement 3D asteroids are not shipped.
3. **Gameplay/source parity:** the separate Colony/native corridor camera
   composition remains unresolved even though the first Titania corridor
   desktop GPU/Software captures pass. The Android stage-select audio report
   needs on-device reproduction despite the new passing desktop SPC test.
4. **Release automation:** cross-platform CI (Linux, macOS, iOS, UWP and
   distributable/signing variants) has not run on this exact source state.
   `.github/workflows/portable-builds.yml` still names 0.0.6.7 archives and
   must be updated for the chosen next release before publishing. No public
   push, tag, or release was requested or performed here.

The September 24 open-issue inventory contains further reports that this
local batch does not close. Platform/startup/performance: #77 (Vita launch),
#64/#34 (Android), #61/#60/#59 (Mac), #58 (black startup screen), #57/#44
(native Deck), #55/#53 (Linux), and #56 (fullscreen). Scene/gameplay:
#73 (Android city), #71 (Space Armada corridor), #67 (widescreen background
coverage), #65 (Android stage-select audio), #48 (Titania), and #24 (automatic
wide aspect). #66 covers menu inputs; the remap/HUD subcases above are fixed
locally, but the full report remains open. #68/#69/#70/#72 have documented
source corrections in this worktree, but remain open pending release/reporter
acceptance. #50 is a title-logo feature request, not a release crash gate.
Issue numbers here are triage, not claims that every report reproduces on the
current unreleased source.

The appropriate next gate is a single current-source CI run plus focused
physical-device checks, followed by a version/changelog review before release.
