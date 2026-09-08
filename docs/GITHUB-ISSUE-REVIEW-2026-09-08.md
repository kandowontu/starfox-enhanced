# Open GitHub issue review — 2026-09-08

Review of all 12 open issues in `kandowontu/starfox-enhanced`, including
comments, compared with the current working tree. No issues were closed, edited,
or commented on. Local repairs are not assumed to be in a published release.

## Actionable implementation work

| Issue | Evidence / current status | Proposed next work |
| --- | --- | --- |
| [#29 Reset shortcut conflicts with Steam](https://github.com/kandowontu/starfox-enhanced/issues/29) | Subsequently addressed locally at the user's request: RESET is a keyboard-remap action; Ctrl+Shift stays fixed and only the final key changes. | Default remains R, so users avoiding Steam's collision must select another suffix. No issue status was changed. |
| [#30 Portable mode](https://github.com/kandowontu/starfox-enhanced/issues/30) | Subsequently implemented at the user's request: desktop SRAM, settings, HUD layouts and bindings share the executable folder. Missing legacy files are copied without overwriting portable files or deleting originals. | Verify in the next packaged desktop build. Mobile/console storage remains platform-specific. |
| [#21 Upscale locks up setup](https://github.com/kandowontu/starfox-enhanced/issues/21) | Resolved per the user's assessment: 4× limit plus the new live preview. | The user will close this issue; no GitHub state was changed here. Additional rollback UI is not required for this issue. |
| [#24 Automatic full-width aspect](https://github.com/kandowontu/starfox-enhanced/issues/24) | Current menu chooses fixed display profiles; no automatic profile found. | Add an automatic viewport policy derived from drawable dimensions, with tested HUD bounds. First define whether the intended policy is aspect-fit or integer-scaled full-width cropping—the issue's examples imply more than simply matching aspect ratio. |

## Correctness bugs to reproduce and repair

| Issue | Assessment | Next concrete check |
| --- | --- | --- |
| [#32 Missing continue credits count](https://github.com/kandowontu/starfox-enhanced/issues/32) | Good candidate for a focused fix, but not reproduced in this review. The native Continue source reads the credit count; game state and visible numeric rendering must both be checked. | Capture Continue with several known counts, trace the source digit-drawing path, and add pixel assertions before changing rendering. |
| [#28 Boss theme persists after death](https://github.com/kandowontu/starfox-enhanced/issues/28) | Still needs checkpoint-specific verification. Existing tests cover replacing the death cue and explicitly expect encounter track `$66` after restart; they do not establish the correct stage theme while replaying a pre-boss checkpoint. | Reproduce death during a real boss fight, trace the restart destination and background-music command, then distinguish stage-checkpoint restart from immediate boss restart. Test SPC-only, matching the report. |
| [#18 “Good luck” voice cut off](https://github.com/kandowontu/starfox-enhanced/issues/18) | Reproduced stage-bank replacement at 0.60 seconds, cutting a cue whose tail lasts approximately 0.95 seconds. Added a one-second native-video-time guard from cue dispatch; the existing fade duration is unchanged. | Original and EX real-SPC regressions now match uninterrupted reference effect PCM through the complete cue and verify subsequent stage entry. Fixed locally; reporter verification remains appropriate. |
| [#16 Space Armada / Venom interior backgrounds](https://github.com/kandowontu/starfox-enhanced/issues/16) | Current code has Mode 2/tunnel and offset tests, but that does not prove these reported interior frames match the cartridge. | Capture the actual reported interiors, compare tile/scroll/offset state, and add scene-specific visual regressions. Medium-to-large investigation. |

## Platform-dependent work

| Issue | Assessment | Next concrete check |
| --- | --- | --- |
| [#33 Android crash after Mario/Luigi](https://github.com/kandowontu/starfox-enhanced/issues/33) | A targeted local fix now suppresses the escape camera's cosmetic explosion burst when MAPVAR1 no longer points to an active building. Reproduced the exact reported list-coverage exception by removing that anchor through the native removal routine before the burst. Native l_add otherwise inserts after the freed slot and loses pool ownership. | Original/EX regressions preserve both explosions with a valid anchor and preserve exact active/free lists with a stale anchor. No orphan reclamation is used. Android-device verification and reporter confirmation remain outstanding. |
| [#34 Poor Android performance](https://github.com/kandowontu/starfox-enhanced/issues/34) | Reporter specifies Huawei Y7a / Kirin 710A / 4 GB, but gives no FPS/settings measurements. No device profile was available. | Establish a 1×, effects-off baseline; profile rasterization, filtering, audio, frame pacing and allocations. Add a conservative mobile preset and optimize measured hotspots. Cannot promise a frame rate without device tests. |
| [#27 Switch audio delay](https://github.com/kandowontu/starfox-enhanced/issues/27) | Reporter confirms delay was fixed, but reports remaining static/distortion. This is not still an unconfirmed latency complaint. | Treat remaining audio quality separately: measure underruns, clipping/resampling and device-buffer behavior on Switch. |

## Addressed locally

### Follow-up investigation: #35

[#35 Windows crash after credits](https://github.com/kandowontu/starfox-enhanced/issues/35)
is **not yet confirmed fixed**. The report uses Original speed, 20 FPS, GPU,
1x rendering and no MSU. Its exception is UPDATE_OBJECTS_L exceeding the
instruction limit, not the MSU end-of-file problem.

Expanded the ending regression to use live SPC output-port feedback, 20 FPS
for both timing modes, and 20,000 additional ticks on the final score screen.
Original and EX tests pass, including the real escape, score tally, boss roll
and credits, with populated fixture route history. An additional headless
application run passed 18,000 presentations at 20 FPS with Original timing,
1x rendering, effects off and MSU off; its final capture shows THE END and the
score. SDL's dummy video driver means this does not validate the reporter's
actual GPU/driver. No speculative crash workaround was added. A reproducing
save/replay and exact affected release would let us investigate the remaining
state-dependent difference.

- [#20 ENEMY placement](https://github.com/kandowontu/starfox-enhanced/issues/20):
  current work anchors the label to the variable-width boss meter and preserves
  custom/widescreen offsets; regression covers maximum health 1–255. Candidate
  for reporter verification in the next build, not automatic closure.

Recommended next order: #32 and #28 for focused
gameplay correctness. Start collecting Android
diagnostics for #33 immediately because it is a crash. #24 is a bounded
enhancement; #34 and Switch distortion require hardware-backed profiling.

## Effects work completed before this review

DITHERED, BLUEPRINT and BLOOM are added. INTENSITY is saved and adjusts 0–100%
in 10% steps, including live preview. Existing effect IDs remain unchanged.
The build passed and seven targeted tests passed (20.60 seconds), including
configuration round-trip, menu input, pixel effects and Original/EX runtime smoke.
Screenshot inspection confirmed Blueprint at 90% with readable HUD/menu text.
After adding the requested RESET suffix remap, the build and final targeted
input/effects/runtime checks passed. Reset tests cover modifier requirements,
repeated-key rejection, old-key deactivation, independent gameplay bindings,
invalid suffix rejection, persistence and restoring defaults.
