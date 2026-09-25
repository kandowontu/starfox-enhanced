# Ray-tracing visual investigation

## September 20 light-upload precision isolation

Added an opt-in diagnostic to the CPU mask reference that rounds the final
eight normalized light samples to float, matching DXR's constant upload. Its
default is false; ordinary software rendering is unchanged. Combined with
float vertices/camera/plane, the pillar fixture still differs at 966/358400
pixels, maximum 40: exactly the preceding geometry-only result. Thus neither
camera/plane nor light upload conversions explain that residual in this case.
Receiver-ray arithmetic and hardware/software triangle intersection precision
remain unisolated; no bias or lighting workaround has been applied.

Windows application rebuild and shadow-geometry unit test pass. The runtime
ground-display, resident/readback equality and off/unavailable/legacy checks
also pass. Evidence: `tmp/shadow-light-precision-sep20/221530d462994a58b8a7418ebd8a0d0c/reference.log`.
The residual numerical difference alone does not demonstrate missing casters or
duplicate shadows, and bit-identical software/hardware intersections were not
a user requirement. Do not treat this diagnostic discrepancy by itself as
proof that the requested visual shadow behavior is broken or complete.

## September 20 camera/plane precision isolation

Extended the last-frame, test-only shadow reference to round camera focal
length/center and ground point/normal to the same float inputs uploaded to DXR,
in addition to the existing float-vertex conversion. Light sample generation
and CPU intersection arithmetic deliberately remain unchanged, isolating just
those upload conversions. Production rendering is unaffected.

Current pillar fixture (Original LEVEL1_1, tick 300, six frames, 2x) still reports
967 differing pixels / maximum 160 for full-double CPU geometry; float geometry
reduces this to 966 / maximum 40. Rounding camera/plane too remains exactly
966 / maximum 40. Thus camera/ground input conversion does not explain the
remaining discrepancy in this fixture. Light-sample rounding and intersection/
receiver arithmetic remain candidates, not established causes. The diagnostic
does not claim bit-exact equivalence with hardware triangle traversal.

Displayed-grass shadow assertion, resident/readback equality and native fallback
checks pass. Evidence: `tmp/shadow-camera-precision-sep20/5b9dad232d2c4442bae02faf8b1fa3da/reference.log`.
Windows application rebuilt; no bias/light-angle workaround was introduced.

## September 20 EX ground-stage coverage

Current EX LEVEL2_3 and LEVEL3_3, tick 1000, six frames, 2x/16:9, pass
resident GPU caster, required-ground, CPU/GPU caster geometry, CPU/GPU
composition and resident/readback equality checks. Both final resident images
were inspected: angled ship shadows are visible on the ice/sea ground areas.
Ground/model shaded pixel counts are respectively 1297/1457 and 1272/1251.
Off, unavailable DXR and the obsolete Enhanced Shadows override are identical.
The later user-requested Software-only Enhanced Shadows option is unaffected.

Proof: `tmp/shadows-ex-ground-sep20/7f018e04549e4be982d72bad0761e845`
and `tmp/shadows-ex-fortuna-sep20/8e5d7fcde571450a8dad76ec57ff0964`.
These are bounded Windows/NVIDIA D3D12 samples. CPU/GPU geometry and
composition equality must not be confused with exact CPU-versus-DXR ray
intersection equality: the earlier precision difference remains unresolved.
No production rendering change was needed for these two samples.

## September 19 neutral pillar recheck

The comparison harness now explicitly disables newer 2D Bloom, software
shadows, reflections and neural enhancement so saved preferences cannot alter
this shadow-only fixture. The float-coordinate diagnostic preserves triangle
material metadata rather than reconstructing position-only triangles.

Current Level 1-1/tick 300/6-frame/2x runs pass resident-caster, ground,
CPU/GPU geometry, CPU/GPU composition, resident/readback and unavailable-DXR
fallback checks. The displayed-grass assertion passes. The resident screenshot
was inspected: pillars and ships cast angled ground shadows. Proof:
`tmp/shadow-pillar-neutral-sep19/4ddf18cf9d9547b08b5f2f11e2d0d7e3/resident.bmp`.

CPU versus DXR remains 967/358400 differing mask pixels, maximum 160;
float-coordinate reference remains 966, maximum 40. This recheck confirms
the discrepancy persists with neutral settings; it does not fix or explain
the remaining tracing precision difference. The checker prints that mismatch
separately; its success must not be interpreted as exact CPU/DXR equality.

Latest September 19 user revision: Enhanced Shadows is restored for Software
only, under a new SOFTWARE_SHADOWS preference. GPU still uses only Ray Tracing;
the old ENHANCED_SHADOWS key is ignored. References below to its removal describe
the earlier GPU audit. Current software proof and reflection quality tradeoffs
are in EXTRA-EFFECTS-STATUS. No duplicate native/enhanced shadow pass is enabled.

September 19 EX horizon coverage: LEVEL5_1 at preroll 2200, six frames,
2x/16:9, passes resident GPU caster, CPU/GPU caster geometry, CPU/GPU
composition and resident/readback presentation equality. Hardware DXR shades
543 model pixels and zero ground pixels. Off, unavailable DXR and the removed
Enhanced Shadows override produce identical images. The resident capture was
inspected: the ship remains above the planet horizon, with no invented floor.
No boss is visible in this sample; despite the artifact directory name this is
not evidence of boss-caster coverage. Proof:
`tmp/shadows-ex-five-boss-sep19/e80618682e8b4c7c9e9840b5f309abb3`.
This verifies this EX space/horizon state, not every model or physical VR.

September 19 wider ground coverage: current Original Titania (LEVEL2_3) and
Fortuna (LEVEL3_3), tick 1000, six frames at 2x/16:9, pass resident GPU caster,
readback/resident presentation, CPU/GPU caster geometry and CPU/GPU composition
comparisons. Both resident images were inspected: angled ship ground shadows
are visible. Recorded ground/model shaded pixels are 1798/1448 and 4956/1174,
respectively. Off, unavailable DXR and the removed Enhanced Shadows override
remain byte-identical; other optional visual effects are pinned off.

Proof: `tmp/shadows-LEVEL2_3-sep19/f71b2f9d7b854552b15b2e810f07f2f2`
and `tmp/shadows-LEVEL3_3-sep19/d2f128d6b7db40018e0b0795a22035df`.
Each directory contains final `resident.bmp`, off/reference images and logs.

The checker now fails CPU/GPU geometry or composition mismatches instead of
merely printing False. Ground is required for the known Corneria/Training
fixtures or explicit `-RequireGround`, not all LEVEL names: space must not
invent a floor. An asteroid tick-1000 attempt correctly failed the resident
caster requirement: its captured cockpit scene contains sprites, no eligible
model casters and zero ground/model shadow pixels. It is not counted as a
successful model-shadow test (`tmp/shadows-space-sep19/a721dabd6b054a3184c1173c7ed440c1`).
These checks do not close all-scene visual acceptance or the precision issue below.

Space Armada (Original LEVEL3_2, tick 300, six frames) subsequently passes the
same resident/geometry/composition and off/unavailable/legacy checks. Its final
capture was inspected: space remains floorless, with 245 shaded model pixels
and zero ground pixels. This establishes a live space-model case, not support
for casting shadows from flat sprite-only asteroid art. Proof:
`tmp/shadows-armada-sep19/b30057fd070d43968fee8762fefad384`.

September 13 follow-up: the reported scene was **Training**. The PC shadow
dispatch had a gameplay/menu-preview flow gate, so Training never used DXR.
Removed that gate: all model-bearing flows can use tracing. The native EX
viewer/continue model now joins the caster scene before dispatch too.
Ground receivers remain conditional on the source shadow plane.

Rebuilt `build/current/starfox_pc.exe`. New resident-versus-readback capture
assertion passes byte-for-byte for Original/EX Corneria, Training-map geometry,
and the Original title. Training capture has 1,727 shaded ground pixels and
1,505 model pixels; title has 8,633 model pixels and no invented ground.
Proof: `tmp/shadow-training-enabled/bb2dead5ee2d4cf7b870daedc7c82f6d`
and `tmp/shadow-title-enabled/8329c6408ca24479ab94114a58a28a52`.
These captures do not constitute visual acceptance of every scene.

Follow-up EX Continue/model-viewer capture also passes normal resident versus
readback byte equality: 2,004 shaded model pixels, zero ground pixels.
Inspected `tmp/shadow-continue-enabled/c700971682e24428a4f762bdfb7d2b08/resident.bmp`.
Actual F1 open/resume/reopen events still pass for EX after this dispatch change.

September 13 current-build recheck: the tilted pillar fixture still differs at
967 mask pixels (maximum 160), or 966 / maximum 40 with float-quantized geometry.
CPU/GPU geometry and CPU/GPU composition captures are byte-identical; the
displayed-grass assertion passes. Inspected the DXR capture: diagonal pillar
and ship ground shadows are present. This narrows the remaining difference to
tracing/precision, not either presentation path; it does not resolve that
difference. Proof: `tmp/shadow-current-pillar-audit/476eb581321d4144991f89981b580450`.

September 12: user reports DXR looks worse than the former Enhanced Shadows.
Reported missing pillar/angled ground shadows fixed locally after the user
identified the Level 1-1 pillars. Headset/user acceptance remains outstanding.

## Confirmed cause and fix

Scenery classification depended on model/world effects, bloom or smoothing
being enabled. With those off, ray tracing generated a correct ground mask,
but the grass retained protected 2D/HUD tags and rejected the mask. Background
classification is now independent of effect settings. No light angle change
was necessary: left/behind illumination again casts diagonal ground shadows.

Level 1-1, tick 300, six frames, 2x: the pre-fix mask had 8,902 ground-shadow
pixels while a pillar floor sample remained unchanged. After the fix, the
sample matches RGB * (255 - mask) / 255; an automated assertion now checks
displayed grass rather than merely counting mask pixels. All other optional
visual effects are explicitly disabled in this regression. CPU/GPU composition
captures are byte-identical. Ray tracing off/unavailable preserves native
shadows, and the removed Enhanced Shadows override remains inactive.

Visually inspected proof: tmp/shadow-pillars-fixed/ddf451c3c1bc4309bd25bc745ca5479c/dxr.bmp.
Fresh regression: tmp/shadow-pillars-regression/bcd8941ad00e407a9004b97bd704e9cc.

Separate remaining detail: this tilted pillar fixture has 967 CPU/DXR mask
differences (max 160), predominantly on pillar faces. This is not the cause of
the missing ground shadows, which affected both backends' composition. It
still needs investigation; the earlier flat-camera equality is not universal.

Follow-up precision isolation: the same CPU algorithm supplied with DXR's
float-quantized vertices differs at 966 pixels, maximum 40 rather than 160.
Thus geometry quantization explains the worst outlier, but not all residual
sample differences. This does not prove a receiver-bias fix; ray arithmetic
and edge/sample classification remain to investigate. The added comparison is
test-only, executes on the last requested frame and does not alter normal
rendering. Fresh ground-composition assertion still passes. Evidence:
tmp/shadow-pillar-precision/f8d08fb0817c4567b1c1fa96e1fcf08a/reference.log.

## Earlier evidence (before the pillar-specific reproduction)

- v0.0.6 and current runtime use the same world light vector (-1,-1,-1),
  transformed into camera space. Old CPU and current DXR use the same 0.015
  angular spread, eight samples, depth-dependent bias and maximum darkening 160.
- Fresh hardware diagnostic on RTX 5070 Ti: CPU/DXR masks identical at 1x/2x;
  one pixel out of 1,433,600 differs by 20 at 4x. Geometry/light changes,
  receiver tests, packing and resource reuse pass.
- Added test-only STARFOX_TEST_SHADOW_REFERENCE and check_ray_tracing.ps1
  -CompareCpu to compare identical live geometry/camera/light. This does not
  restore a public CPU shadow setting or change release visuals.
- Corneria, Original, GPU, 16:9, 2x, tick-1000 preroll and six presentation
  frames: zero differences across 358,400 mask pixels; final CPU-reference/DXR
  BMPs byte-identical. DXR capture visually inspected. Proof directory:
  tmp/shadow-visual-investigation/8e3e4bf259f2467e98505d9f10e3b1da.

This rules out a backend light-angle or mask mismatch for this fixture only.
An additional `-CompareGeometry` diagnostic selects CPU-generated geometry
with DXR still enabled, while retaining the shared presentation path. In the
same Corneria fixture, the complete CPU/GPU geometry BMPs are also byte-identical.
Evidence: tmp/shadow-geometry-investigation/0d8cd5c34ab44976b063f36c24d80516.
The historical caster audit found the same face eligibility, triangulation and
destruction offset; the newer shadow-only entry skips rasterization, not casters.
This does not establish equality for animated/rotated or other stage geometry.
The independent Level 3 Corneria fixture (LEVEL3_1, same tick/scale settings)
also has zero mask differences and byte-identical CPU-reference/DXR and
CPU/GPU-geometry presentations. Proof:
tmp/shadow-corneria3-audit/b763cc3d51974e68abd6d467889d25e9.
The diagnostic now accepts Level, PrerollTicks and RenderScale parameters so
reported scenes can be reproduced without editing its environment setup.
It does not compare the entire old release's geometry/presentation pipeline,
nor establish which stage/settings caused the user's visual regression. Next:
compare historical caster/presentation behavior and broader live scenes before
changing lighting parameters arbitrarily.

Space scenes do not have a physical ground-shadow receiver. The runtime now
excludes the source-identified starfield, orbital, and planet-limb background
lists from native, software, and DXR ground-shadow passes. It keeps the
training exception and ground-stage shadows. In Original LEVEL1_2 the
30-frame 16:9 hardware-DXR-on/off captures differed only in the FPS overlay
(649 pixels in rows 0-24), with no ground-shadow mask or backend dispatch:
`tmp/space-shadow-rt-{on,off}-sep22.bmp`. Training still selected the GPU-
resident DXR backend, and the Corneria fixture still reported a nonempty
ground receiver (`tmp/training-shadow-rt-on-sep22.log`,
`tmp/ray-shadow-ground-final-sep22/db988265123d4992a1bf03335af648f0`).
The complete DXR comparison passes. Its script now disables the volatile
FPS overlay so a live FPS digit cannot fail its image hashes.

EX Training uses `BG_TRAINING` with `bg2chr stars`/`space night`, whereas
Original Training has physical ground. The shared training exception is now
experience-aware. `check_ray_tracing.ps1 -Experience EX -Level TRAININGMAP
-PrerollTicks 1000 -Frames 12 -RenderScale 1 -NoShadowsExpected` passes seven
captures: GPU off/removed-override/DXR-readback/resident/unavailable, plus
software with Enhanced Shadows off/on. Both backend pairs are byte-identical,
and no shadow backend dispatches. Evidence:
`tmp/ex-training-space-shadows-sep22/532119e03116401c952565a4c339ac88`.
