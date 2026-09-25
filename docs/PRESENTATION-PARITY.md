# Presentation parity checks (unreleased)

## Destruction interpolation — September 24

Desktop and PCVR presentation now interpolate each surviving object's
destruction counter between source frames. Both software and GPU fragment
offsets blend the exact signed, quarter-scale integer offsets at the two
neighboring source counts. Completed source frames retain the original byte
and integer displacement; recycled slots, counter resets and newly created
debris do not inherit unrelated motion.

Original and EX `KICHI_0` fractional GPU/Software comparisons each pass 90
images with exact coverage and palettes. The original integer fixture passes
90 images too. Windows app and PCVR packet-check builds succeed; Original/EX
hitlist, packed-face/projection and VR packet tests pass. This verifies one
real model family and isolated packet behavior, not every natural enemy death,
particle burst or physical headset cadence. No release was published.

## Source-submission comms timing — September 20

`starfox_comms_timing_check` sends message 36 through SEND_MESSAGE_L and calls
the real FRIENDS_MESSAGES_L animator for 300 updates, with/without its meter.
It clears each source bitmap via M_CLRBITMAPS and checks four planar-bitmap
border pixels (retail Y177, EX Y156), independently of host visibility.
Before the fix, the host showed the meter on the last opening update and hid
it on the final speaking update. Both cartridges now match all updates:
50 source meter frames when requested, zero otherwise, and 241 closed frames.

The CPU adapter captures the speaking phase at MCOPYFACE/MCOPYFACE2, before
the routine increments/decrements its counters. Presentation uses that phase
for text and meter rather than inferring the submitted image from counters
that have already advanced. The optional phase is appended to CPU save states;
older snapshots use their previous counter fallback until a face is submitted.
Source counters, message duration and CPU bus latch are not changed.

Boundary save/load checks and EX secondary-channel opening/speaking/closing
checks pass. Registered CTests: starfox_upstream_comms_timing and
starfox_ex_comms_timing. Retail/EX simulation and state fixtures also passed
during this batch; subsequent bus-read removal is covered by the direct comms
tests. This does not assert all localized text layouts or natural game events.

## Whole-sequence final-image shutter checks — September 20

Fresh GPU/software sequences match every frame in Original and EX: 144 frames
at 240 Hz/32:9 and 36 frames at 60 Hz/16:9, all 2x. A new independent image
analyzer samples 96 columns across the final BMPs, requiring a closed frame,
straight full-width edges, monotonic opening and sub-source-update pixel steps.
It does not accept renderer-reported mask bounds as its visual oracle.

| Fixture | Visible opening frames | Distinct edge pairs | Largest pixel step |
| --- | ---: | ---: | ---: |
| Original 240 Hz | 104 | 89 | 2 |
| EX 240 Hz | 100 | 90 | 1 |
| Original 60 Hz | 26 | 26 | 4 |
| EX 60 Hz | 25 | 25 | 4 |

Evidence: `tmp/scramble-sequence-ultrawide{,-ex}-sep20` and
`tmp/scramble-sequence-60-{ORIGINAL,EX}-sep20`. Checker:
`tools/check_scramble_sequence.py PREFIX --frames N --fps N --scale 2`.
Seven negative/positive oracle tests reject slits, ragged edges, backward motion
and un-interpolated source-size jumps; registered as
`starfox_scramble_sequence_oracle` and passed. These execute the actual source
wipe program via its animation register, not an entire natural launch sequence.
No production shutter arithmetic changed to pass these checks.

## Current high-FPS shutter/upgrade presentation (September 20)

The capture harness now explicitly selects D3D12 or Vulkan and saves/restores
SDL_GPU_DRIVER, preventing an inherited backend override from silently changing
the comparison. Current 240 Hz, 2x, 16:9 final-target captures at presentation 70
show straight horizontal scramble openings and aligned upgrade wireframes in
both Original and EX. All four GPU images were inspected and match their
software counterparts byte-for-byte. Evidence:
`tmp/parity-current-sep20/{original,ex}-{wipe,upgrade}-{gpu,software}/presentation.bmp`.
These snapshots verify current final composition, not a complete motion sequence;
the existing twelve-phase tests and earlier sequence evidence remain necessary.

The upgrade test now also exercises owner shape/type/strategy changes, absent
previous history and a missing current owner. It checks pose cuts while retaining
the native blink shape. Rebuilt Original and EX hitlist tests pass (2/2).
Rebuilt cartridge-backed Original and EX simulation suites also pass (2/2,
133.70 seconds combined wall time), including the twelve-phase shutter checks.
No gameplay behavior was changed in this verification pass.

## Six-language ultrawide comms coverage (September 20)

`capture_presentation.ps1` now exposes Language 0..5 instead of forcing English.
Its effect ranges also include the new styles through 50, while rejecting
model-only Trails/Long Exposure in the world slot.

Captured message 36 with the source-requested teammate meter at presentation
60, 60 Hz, 32:9 and 2x for Original and EX in English, Japanese, German,
French, Spanish and English (Europe). All 12 GPU/software final-target pairs
match byte-for-byte. All 12 GPU frames were visually inspected: portrait
dimensions remain consistent and the translated text fits above the visible
meter without overlap. English/Europe intentionally share the dialogue.

Evidence: `tmp/comms-languages-sep20/{ORIGINAL,EX}-{0..5}-{GPU,SOFTWARE}/presentation.bmp`
and per-case runtime logs. Reproduce using `-Message 36 -Meter -Frames 60
-DisplayMode 32_9 -FinalTarget -Language N` with each experience/renderer.
This fills the language/ultrawide final-composition gap for this message only;
it does not prove every translation, natural conversation, opening/closing
timing, custom layout or physical platform. The existing meter-off source-gate
tests and final captures below remain separate evidence. No game behavior was
changed to obtain these images.

## Current requested/nonrequested teammate meter final captures (September 19)

Source message 36 (Rabbit) at presentation 60, 16:9/2x: Original and EX each
have GPU/software final-image equality with FRIENDS_METER requested and absent.
The two EX images were inspected: the requested case contains the meter and
raised text, while the absent case has no meter. Portrait proportions remain
consistent. Evidence: `tmp/comms-final-{ORIGINAL,EX}-{True,False}-{GPU,SOFTWARE}-sep19/presentation.bmp`.
This is an injected source-message fixture, not all natural conversations or
opening/closing timing acceptance. The harness now pins effects and restores
its environment; final-target captures avoid writing unwanted frame sequences.

## Current EX native-width final target (September 12)

Fresh EX 4:3, 2x, message 1 at 60 presentations: final GPU and software
597x448 BMPs match byte-for-byte (SHA256
896021E0302D9570E868D036E318564761BFA4DAF3A0A777108002E23EC3121E).
Visually inspected `tmp/comms-ex-native-gpu-final.bmp`; reference is
`tmp/comms-ex-native-cpu-final.bmp`. Logs confirm resident GPU geometry/raster.
This message depicts Fox, not Falco: it proves final-target portrait aspect
agreement, not a teammate-meter timing test or all-language coverage.
The capture tool now accepts explicit -Renderer GPU/SOFTWARE and -FinalTarget
to capture presentation.bmp with the required startup-preroll skip.

## Scramble shutter

`SF/MARIO/MDATA.MC:MSCRAMWIPE` opens a horizontal band eight source
scanlines per edge per update. Interpolating the left/right mask values of
each changing row produced vertical slits instead of moving horizontal edges.
The host now recognizes that exact contiguous band pattern and interpolates
its top/bottom in source-line units, applying the mask at output-pixel
precision. Other wipe patterns retain their table interpolation. Completion
still follows the source transition, without an extra held frame.

Tests cover twelve fractional phases (240 FPS over 20 Hz), exact band bounds,
unchanged row masks, rejection of noncontiguous masks, and normal wipe cuts.
Runtime source-program captures:

- [240 FPS widescreen](../tmp/scramble-240-fixed/000070.bmp)
- [60 FPS 4:3](../tmp/scramble-60-4x3-fixed/000020.bmp)

These are captures of the actual source wipe program triggered through its
native animation register, not mockups. The 144-frame 240 FPS capture has 88
distinct visible top-edge positions measured from the BMP centre column;
frames 52-67 progress from row 213 to 202 instead of holding source-sized
eight-line jumps. This sample does not claim to cover every level transition.

## Upgrade wireframe

`SF/STRAT/GASTRATS.ASM:FLASHPLAYER_STRAT` copies the player's position and
rotation but alternates between NULLSHAPE and the wire model. The generic
shape-change guard consequently discarded its motion history on each flash.
Presentation now anchors only this named attached effect to the same player
snapshots. Shape/blink/colour timing remains native. Missing/recycled owners
and owner shape/strategy changes use the same history cuts as the player.

Tests compare position and rotation across twelve subframes, coordinate wrap,
source blink identity and recycled player slots. A source FLASHPLAYER effect
while holding Right at 240 FPS is visible in this
[runtime capture](../tmp/upgrade-240-fixed/000070.bmp).

## Optional teammate meter

`SF/ASM/CONTINUE.ASM:FRIENDS_MESSAGES_L` shows MSHOWTEAMMATE2 only while
FRIENDS_METER is active and the portrait has finished opening. It raises the
message by sixteen pixels to make room. The host dialogue state and HUD now
consume that source gate and animated health value. Opening/closing frames,
ordinary calls, Fox/non-team speakers and EX's alternate channel do not inherit
a teammate bar.

Source-requested Falco call at the same capture frame:

- [Meter requested](../tmp/comms-falco-meter/000050.bmp)
- [No meter requested](../tmp/comms-falco-no-meter/000050.bmp)
- [EX meter requested](../tmp/comms-ex-falco-meter/000050.bmp)

The source portrait copy and host decoder both use 32x40 pixels / 640 bytes.
The 4:3 presenter was incorrectly displaying a square-pixel 256x224 (8:7)
canvas. Its final presentation now uses a 4:3 extent, without resampling the
game raster or changing portrait decoding. Windowed dimensions and pointer
coordinates use the same correction. Tests cover 1x/2x/4x/10x and ensure
widescreen canvases remain unchanged.

Actual software-renderer output at 2x is 597x448 (rounded 4:3), rather than
512x448. Captured through SDL to an off-screen presentation target:

![Corrected 4:3 comms proportions](../tmp/comms-aspect-4x3-software-display.bmp)

Reproduce with STARFOX_TEST_RENDERER=SOFTWARE,
STARFOX_TEST_SKIP_PREROLL=1 and STARFOX_CAPTURE_PRESENTATION_PATH, then
tools/capture_presentation.ps1 -DisplayMode 4_3 -Message 1 -Frames 60.
The ordinary numbered BMPs preserve the source raster and intentionally do
not reflect display-aspect correction. Omitting SKIP_PREROLL can capture the
black startup preroll instead. GPU target readback produced an unreliable
uniform-colour image and is not counted as visual proof. The separate
widescreen portrait discrepancy is addressed separately below.

Rechecked after the desktop build was restored: the current resident GPU path
now produces a valid final presentation readback. Fresh Original GPU and
software captures at 60 FPS/60 presentations, message 1 and 4:3 are both
597x448 and byte-identical (SHA256
`FDCA49D9960249BA63BFBDEC71C5C915AEB860ADE83014B1BF9B1C0A237DB6CB`).
The GPU runtime log confirms resident raster/composition/effects/presentation.
This supersedes the earlier failed-readback observation for this fixture.

![Current GPU 4:3 comms presentation](../tmp/comms-gpu-presentation-restored.bmp)

Reference: `tmp/comms-software-presentation-restored.bmp`; logs and source
frames: `tmp/comms-gpu-presentation-frames` and
`tmp/comms-software-presentation-frames`. This is an Original 2x display
fixture, not verification of every language, platform or display mode.

Widescreen uses square output pixels and did not receive the native canvas's
7:6 horizontal pixel-aspect correction. Comms portraits now apply that ratio
at stored-pixel resolution, retaining their original height and right edge.
Text/meter coordinates, wrapping and custom HUD offsets are unchanged. Native
width and non-comms portraits retain their existing rendering. Tests compare
every resampled pixel against the original decoded face at 1x/2x/4x/10x and
verify the unchanged height/right edge and untouched surrounding pixels.

![Original widescreen corrected comms](../tmp/comms-aspect-wide-fixed/000059.bmp)
![EX ultrawide corrected comms](../tmp/comms-aspect-ex-wide-fixed/000059.bmp)

`tools/capture_presentation.ps1` provides source-message, shutter and upgrade
fixtures without saving settings. `STARFOX_TEST_PRESSES` accepts frame:mask
input for moving-ship capture. All hooks require STARFOX_TEST_FRAMES.

Verification: Windows builds succeed; Original/EX simulation and hit-list
suites pass (4/4). Linux builds succeed and Original/EX hit-list/transition
suites pass (4/4). Those suite counts predate the widescreen portrait update;
its Windows/Linux Original simulation regressions and Windows Original/EX
runtime captures pass separately.

## EX title widescreen guards

The EX BG1 diagnostic capture identifies two opaque 16-pixel guard columns
outside the 224-pixel Super FX bitmap. In widescreen title presentation, both
the background pass and restored foreground pass now omit these guards.
Interior opaque black text remains opaque; native-width presentation is
unchanged. Substrate tests cover both guard edges and retained interior ink.

Matched Windows EX TITLEMAP captures (300 preroll ticks, 180 frames, 16:9, 2x):

![Before title guard removal](../tmp/title-guards-before.bmp)
![After title guard removal](../tmp/title-guards-after.bmp)
![BG1 diagnostic: magenta is transparent](../tmp/title-guards-layer1.bmp)
# Sector Z tally capture interpretation (September 19)

## Macbeth stage-clear top-strip recheck

The report's red-sky STAGE-5 CLEAR scene is exercised separately with Original
LEVEL3_5 / CL_UNDER. `tmp/macbeth-clear-border-full-sep19` contains 1,801
presentation frames sampled every 60 frames at 16:9, through gameplay clear,
tally, the closing wipe and planet travel. All 29 sampled non-wipe frames have
nonblack pixels on the top row; frames 540 and 720 were visually inspected and
show red sky reaching the top edge, with no reported full-width black strip.
The host's explicit 16-row black mask is restricted to gameplay with disabled
meters and the source launch flag, not an unconditional clear-screen letterbox.

No rendering change is justified by this reproduction. This is a direct-entry
clear-routine fixture using SDL's dummy display (not native hardware GPU
presentation), sampled every second; it does not rule out an intermittent or
different-route/hardware issue. The original report remains unreproduced.

Fresh `capture_level_clear.ps1 -Level LEVEL3_4 -ClearRoutine CL_SHIP3_4
-Experiences ORIGINAL -Frames 1201 -OutputDirectory tmp/sector-z-tally-current-sep19`
reproduces the old apparent black model occlusion at presentation 1200.
The matching state log identifies an active window wipe (logic 170/$AA), not
an unobstructed tally. CL_SHIP.ASM explicitly invokes `mapendwipe roundwipefill`
after the completed score display. The inspected frame 1140 has wipe inactive
and all three portraits/names unobscured; frame 1200 is already closing.

Therefore the earlier 1200 screenshot is not evidence that model draw order
needs changing. No rendering change was made on that basis. This does not
prove pixel-exact closing-wipe parity or resolve the separate top-letterbox
report. Both images and per-60-frame state logs are retained in that directory.
