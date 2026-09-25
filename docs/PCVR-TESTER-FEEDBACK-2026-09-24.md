# PCVR tester feedback — September 24, 2026

This is PCVR (OpenXR desktop), not a Quest APK report. Do not close an item
merely because a flat-window or 256-pixel synthetic capture passes.

## Addressed in this worktree

- The PCVR install component and CI job now build/package the asset builder
  with the player, both from the same source revision. The ZIP provides only
  `BUILD-ASSETS.bat` and `LAUNCH-PCVR.bat`, so missing assets and invalid ROM
  errors remain visible. No ROM, generated BIN or soundtrack is included.
- A standard SDL gamepad can drive gameplay/menu alongside OpenXR hand
  controllers. PCVR input mapping is documented in `START-HERE.txt`; an
  Options sensitivity setting scales gameplay steering from 40–100%.
- `Starfox-MSU1.PAK` beside the PCVR executable or a separately supplied BIN
  is auto-detected; an explicit `--msu` still wins. Startup now logs the chosen
  path and whether the saved music setting is on. Switching Original/EX no
  longer silently drops the soundtrack path.
- Corneria/Training's nonmatching panorama edges receive a wider periodic
  blend. This is a candidate visual fix; headset acceptance remains open.
- Tunnel/boss-room VR surrounds now sample the authored BG2 ceiling and floor
  palette separately from the wall. They fill the upper/lower peripheral view
  instead of framing the boss room as a small solid-color rectangle. Front and
  rear simulated stereo captures pass; full headset acceptance remains open.
- The GPU ground-dot grid now retains fractional camera motion between source
  ticks instead of snapping every dot to an integer source unit. Original/EX
  input checks, all 17 VR CTests and the real Vulkan stereo scene checker
  pass; headset shimmer and aliasing still need visual confirmation.
- The current PCVR test archive is `build/pcvr-tester-2026-09-24.zip`.
  Local compile, input, audio, package, and backdrop checks pass. Its asset
  builder rejects the local patched development ROM as designed; a supported
  retail dump is needed for an end-to-end builder test.

## Still open for headset/source-reference verification

- Corneria sky seam and ground/mountain join at full headset resolution; the
  native 256-pixel rear capture is not proof of photographic panorama parity.
- Space Armada cruisers and apparent duplicate boss ship. Earlier natural
  route diagnostics found source `SHIP_4` objects remain active; deleting them
  without identifying the rendered offender risks breaking authentic ships.
- Space Armada obstacle-ship and cockpit depth, residual boss-room side-wall
  composition, post-clear tan letterbox, and entrance tunnel texture/geometry
  flicker. The ceiling/floor surround correction is not a full 3D rebuild.
- SBS depth calibration against headset stereo. The flat renderer has separate
  eye projections, but current Half/Full SBS depth has not been accepted by a
  viewer and the background remains largely non-stereo.
- Residual ground-dot shimmer and perceived low-rate source animation at
  Original pace. Fractional grid movement is corrected locally, but its
  appearance at headset resolution and object motion still need dedicated
  source-to-headset interpolation captures. Head tracking is independent.
- ScaleX/xBR filters in VR and optional 3D asteroid replacements. Neither is
  in this PCVR archive. Asteroid mesh replacement needs a source-palette,
  collision/occlusion, and timing review before shipping.

## Evidence gathered

- Current flat EX and Original direct `LEVEL1_3` entry captures at 100/800/1600
  ticks: EX already shows the mothership approach at tick 100 while Original
  shows space. The EX route table does resolve course-1 stage-3 to the same
  `LEVEL1_3` script address (`$06CD24`) as direct entry. This narrows the
  discrepancy to that script/its initial state or VR composition; it does not
  justify removing ships without a synchronized original/EX reference.
- Follow-up inspection of the tick-100 EX trace puts its map cursor at `$06B062`,
  before `MAP1_3C` (`$06BBD4`). The upstream EX `LEVEL1_3` script begins with
  `MAP1_3A`, whose opening explicitly spawns distant ships. The busy entry
  image is therefore not evidence that the port skipped straight to the third
  script section; a natural-route/source comparison is still needed for the
  reported timing and apparent duplicate boss ship.
- Current flat EX Controls and Original/EX stereo Controls entry captures show
  a uniform background color. The reported left-half miscolor did not reproduce
  at those entry states; check the actual preceding transition.
- Existing natural-route Armada samples and object-state notes are in
  `docs/BACKGROUND-AUDIT.md`.
- A fresh Original/EX direct `LEVEL1_3` VR preflight logs background ID,
  tunnel state, flow, shadow enablement and grid mode at each transition.
  Neither variant draws a ground shadow in open space; shadows become enabled
  only on entering the tunnel. Early EX `SHIP_4` cruisers are live source
  objects, not a duplicate shadow. This does not yet identify the reported
  apparently duplicate boss or establish natural-route timing.
- The local optional `Starfox-MSU1.PAK` passed a 120-tick Original PCVR
  bundle preflight with `--msu`; the cartridge selected track 10 and the
  decoded MSU stream reported playing on all 121 sampled states. All 120
  generated PCM blocks were non-silent (peak 32,591). This proves pack loading,
  track control and software mixing, not audible playback on a tester's
  OpenXR/SDL audio device or the saved startup-menu preference.
- A second preflight from the real `INTROMAP` startup path selected intro
  track 1 and produced 160 non-silent PCM blocks (peak 25,924). Thus the
  soundtrack is not confined to the direct-level diagnostic path.
- The September 24 PCVR startup probe selected a dummy pack beside a custom
  `--bundle` path and rejected its invalid contents; without that file it
  clearly logged native SPC mode. The dummy file was removed. All 17 VR
  CTests pass after this discovery/logging change. Headset audibility is
  still unverified.
- Original Armada at tick 7,500 was captured from simulated front and rear
  VR eyes in `tmp/pcvr-armada-boss-{front-,wide-,wide-rear-}sep24`. The first
  front capture reproduced a narrow boss-room panel with navy upper/lower
  surround; the corrected front capture continues its grey ceiling and floor
  across the eye while the rear retains the source wall color without a
  duplicate boss room.
