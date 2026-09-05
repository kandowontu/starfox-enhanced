# Active regression hit list

Restored from the user's reports on 2026-09-04. This file is the durable
handoff record; do not infer completion from an older chat or a passing build.
Updated after the 2026-09-04 source/regression/build pass and included in
0.0.4 ALPHA; a passing test is not a physical-console test.

Priority added by the user: make the **entire ending** work, including correct
timing, Pepper's scene, score tallies, boss roll, credits and final display.
Compare both Star Fox and EX against their original source sequences.

## Reports and status

| Report | Current status |
| --- | --- |
| First-person reticle remains on the map | Presentation suppression and transition regressions pass in both variants |
| Cockpit marker triangles need smooth 60+ FPS motion | Interpolate native HUDROT with wrap-safe fractional angles and high-resolution line placement; 60/90/120/240/360/480 FPS and raster tests pass |
| Stage 2 clear delay; score/avatars remain during warp; verify all clears | Launch-only pacing is now strategy-scoped; corrected tally visibility through hide/exit, EX bitmap/portrait presentation and EX Earth-clear countdown clobber. 244 clear runs across both games, FPS and pace settings plus four colony continuations pass. See LEVEL-CLEAR-VALIDATION.md for measured fixture times and limits |
| EX reticle is jittery | Fixed recycled-slot identity and newborn reticle/player interpolation; removed ambiguous same-shape matching. Coordinate-wrap and owner-relative interpolation tests pass. Static 120 Hz/4x captures checked; motion comparison against hardware remains |
| Black-hole background has jumpy/off frames | Fixed 9-bit vertical-scroll wrap interpolation, retaining HDMA flag bits; wrap tests cover all eight FPS settings. Retail/EX 120 Hz/4x captures checked |
| Sector Y becomes a black hole after exiting the black hole | Confirmed and fixed after 0.0.4: flat/zoom sprite drawing always selected the upper packed nibble. Source bit-5 selection now distinguishes Sector Y from Black Hole. 52 full icon comparisons and a runtime map capture supplement the earlier pre/post-exit comparisons, which could not detect consistently wrong art |
| Out of This Dimension shows half of Andross's face | Not reproduced in current retail/EX direct-stage captures; source mode setup now clears stale tunnel/scroll state. Full trigger-bird playthrough still needs visual verification |
| Trigger-bird transition into Out of This Dimension is missing | Special-exit white-fade regressions pass; actual bird-trigger visual comparison remains |
| Option to hide Android/iOS on-screen controls | Option and settings persistence tested; physical mobile UI test remains |
| Black-hole map music absent without MSU | Real non-MSU SPC test verifies track $0f and nonzero music PCM during travel, in retail and EX |
| Xbox UWP returns Home without displaying anything | User confirmed Series S boot, then reported dead controls. WGI was disabled; explicitly enabled before initialization. Revision 0.0.3.4 rebuilt with input logging and all shared fixes. PE imports, framework symbols, signature/package checks and virtual/UWP-configured input tests pass; physical console input still unconfirmed |
| Right-mouse drag should orbit the player, not the camera | Code orbits the camera-player vector around the player target; manual mouse feel/playthrough check remains |
| Switch audio is delayed | User clarified delay is immediate. Switch startup prime is 8 ms and device request 512 frames (~10.7 ms at 48 kHz). Added 100 ms source FIFO ceiling; stalls cannot accumulate unbounded lag. Real SDL stream tests pass, including 32-to-48 kHz resampling. Native NRO rebuilt locally. Immediate latency is NOT explained solely by backlog; on-device/output-path measurement remains |
| Switch controls must use Nintendo labels, not Xbox | Platform bindings implemented; physical input check remains |
| Damage lacks flash effects | Actual PCOLB_ISTRAT collision regression verifies flash counter and active colour math in both games; hardware visual comparison remains |
| Damage lacks screen shake | Same actual collision test observes native VIEWSHAKEX/Y/Z. No failure reproduced in that path; frame-by-frame presentation comparison remains |
| Original pace runs bosses too fast, notably boss 1 | Fixed scheduler revisiting already-updated objects when another object removes itself. Regression fails with old code and passes in both games with fix. Original workload pacing is still an approximation, not cycle-exact SNES slowdown |
| Route 2/3 speed surges and dips on an older PC, possibly worse with FPS display | Fixed the output limiter's accumulated-deadline bursts after expensive frames. 56 one-minute route replays match source movement/roll/cadence under 60/90 FPS, varying 47–59 FPS and 100 ms stalls, FPS display off/on, both games/paces. Mutation confirms the old limiter fails. Original pace's object-count slowdown remains an approximation; the reporter's old PC still needs a retest. |
| Map planets rotate too fast while non-planet stages are paced correctly | All six planet angles tested for one source second at 20/30/60/90/120/240/360/480 Hz, both pace modes and both games; same native rotation deltas at every setting |
| Map shown before route 1-6 Venom Base, repeats route 1-5 intro | Fixed EX clearing late LEVELFINISHED=7 after the tally; latch direct-stage transfer before clearing. Real LEVEL1_5 CL_DIVE routine now goes to LEVEL1_6 without entering the map in both games |
| Starship/Andross interior floors and ceilings misplaced | Restored source tunnel HDMA per-scanline vertical-page selection (24/280) and OLDVIEWPOSZ phase. Tests cover all 32 native tables and four widths. Visual parity remains: direct LEVEL1_END is not a valid standalone tunnel capture and produced black frames, so those are NOT validation |
| Ending: Pepper scene/camera broken, endless flight | Fixed native object-entry Y register / camera target and suspended-task ownership; source escape, radio and camera phases pass in retail + EX. Normal full-playthrough visual/audio comparison still needed |
| Ending: score tallies, correct timing, boss roll, credits | Added full source-sequence regressions (six bosses, totals/average, special-score exclusion, timed cards/wipes, staff text, final score). Boss-scroll IRQ, description typing, CGRAM ownership, layering/clipping and final-score presentation corrected. Headless images checked; not a claim of a complete hardware-reference visual match. See ENDING-VALIDATION.md |
| EX bosses oversized (Luigi Hydra, Crimson King) | Corrected word-coordinate models receiving SH_SHIFT and big-head scaling. Hydra's LUIGIHSBW head was 16 times too large; Crimson King's BOXXIE was 8 times too large. Both pass all nine native angle/depth pixel comparisons. Earlier audit labeling was corrected: POINTYANIM is the stage 7-4 boss; Crimson King is BOXXIE on stage 6-3 |
| Camera, shadows and star-field parity audit | Corrected model-matrix shortcut rounding, unchanged-matrix interpolation, source object-position rounding, star recycling/random carry and coordinate shifts, star projection/two-pixel placement, and snow/pollen colours. Both games pass independent matrix/point checks and continuous star-field comparisons, including EX's expanded point count. Full scene and timing parity remain open. |
| Ground-grid parity audit | Read the actual EX cartridge's 25×25 loop count and 96-unit secondary-dot threshold, retaining its CPU's 1,920-unit origin. Corrected secondary pixels and EX line endpoints/byte-coordinate aliasing. All 576 independent grid frames match, including repeated presentation checks. |
| Object view flags | The cartridge resets flags before skipping invisible objects, and sets left/in-view bits on surviving behind-camera objects. Corrected both paths. A disposable native CPU/GSU snapshot found 239 mismatches in Original route 2 before the fix and none afterward. Both routes 2/3 and both games have dedicated comparison tests. |
| Original pace cycle audit | Added an optional pinned Ares GSU adapter. All 70,980 numeric/pixel rows agree with the Snes9x reference, including the same documented fixture exceptions; 73,536 isolated calls have cycle traces. CPU/GSU overlap, DMA and video-phase integration remain open, so the runtime still uses its documented workload approximation. |
| Start at credits crashes | Reproduced Original's native restart loop; corrected ownership handoffs, including Start held across the final-score task. Both games' early/late Start tests and packaged Windows captures pass |
| Delayed credits jingle missing | Full ending SPC tests and actual desktop mixer traces produce the jingle in both standard soundtracks after about 591 seconds of silence. MSU's recording ended without the jingle; natural track-49 EOF now hands off to the running SPC timeline, verified through the desktop mix after 570 seconds of silence. The reported MSU-off failure has not been reproduced |
| EX Star Wolf only spawns one ship / fight fails | All four source actors spawn and render on LEVEL5_5, LEVEL6_6 and LEVEL7_5. Normal Y fire and directional steering shoot down all four and verify source boss/stage continuation in both pace modes; enemy health is not forced. Independent GSU raster checks cover all ship models. The reported one-ship state was not reproduced |
| EX L+R+Select exhausts the route | Preserve DRAWPLANETLINES_L's final destination at the route terminator and support EX's additional route-choice slots. Actual shortcut tests pass at all seven campaign endpoints |
| PS Vita release | Native ARM VitaSDK/SDL3 VPK built; eboot.bin/param.sfo and ELF architecture verified. Package prepared locally, no game assets included. Boot, input, sound and performance require Vita/PSTV hardware |

Reporter baseline: route 1 full playthrough, Original pace, 120 Hz presentation,
4x Render Upscale. Retest source cadence separately from presentation FPS.

## Latest local validation and deliverables

Post-0.0.4 follow-up: see [PARITY-AUDIT-2026-09-05.md](PARITY-AUDIT-2026-09-05.md)
for model scaling, credits restart, delayed SPC/MSU music, route termination,
Star Wolf reproduction coverage, additional EX decoder fixes, and remaining
whole-port parity limits. The validation and packages below are the earlier
release baseline; they do not contain this follow-up automatically.

- Full CTest: **35/35 passed in 375.64 seconds**, including both complete
  ending suites, both hit-list suites, direct audio, transitions, input and
  the new bounded audio-FIFO and level-clear tests. Full log:
  `build/current/Testing/Temporary/LastFullTest-20260904.log`.
  The final EX tally compositor also has fresh visual captures and focused
  runtime/ending/hit-list checks after this full suite.
- Windows runtime: `dist/StarFoxEnhanced/starfox_pc.exe`.
- Xbox test package: `dist/StarFoxEnhanced-0.0.3.4-xbox-uwp-x64-test.zip`.
- Vita package: `dist/StarFoxEnhanced-0.0.3-vita.zip`.
- Switch package: `dist/StarFoxEnhanced-0.0.3-switch-test.zip`.
- Final payload hashes and package checks are recorded in `BUILD-ARTIFACTS.md`.
- Vita SDK: official 2026.08 distribution, GCC 15.2; SDL 3.4.14 vita_gxm,
  Vita audio and native joystick backends. No mobile touch overlay.
- Switch SDK installed in a user-owned WSL cache from the same pinned
  devkitPro container layers as CI; SHA-256 verified. NRO is native AArch64.
- These local validation candidates precede 0.0.4 ALPHA. Published packages
  are listed on the v0.0.4 GitHub release. Console builds remain hardware-test
  candidates, not verified on-device releases.

The remaining checks above are intentionally explicit. Do not relabel this
whole list "verified fixed" based on source fixtures, synthetic captures or
successful cross-compilation alone.

## Validation rules

- Record what was actually exercised, including game variant and entry path.
- Synthetic exits are useful but do not alone validate real boss completion.
- A generic `credits` flow flag or eventual termination does not prove that
  Pepper, individual bosses, credits text or final score appeared correctly.
- Keep Xbox/Switch/mobile hardware checks distinct from desktop tests.
- Preserve the rest of this hit list while prioritizing the ending.
