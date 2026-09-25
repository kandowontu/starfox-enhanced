# Single enhanced cratered planet

September 22 follow-up: at the user's request, enhanced preview 19 now places
the moon 128 native pixels lower and removes its original partial footprint.
Preview 28/gameplay and the unenhanced source renderer stay unchanged. See
`BACKDROP-CORRECTIONS-SEPT22.md`. The original-placement evidence below
describes the previous enhanced-preview behavior.

EX pre-game choices 19 and 28 now upgrade the existing orange cratered planet
when Enhanced Sky is enabled. Both use the same native atlas but different
vertical offsets; 19 only exposes its bottom edge at the sampled phase.
Native rendering remains unchanged when the option is off.

Resource 223 is the built-in image-generated `cratered-planet-v1.bmp`, with
PNG master and generation/cleanup prompts beside it. The first generated
candidate had stray edge colors; a targeted image edit removed those before
integration. Disk registration follows the measured native center (384,152)
and 110-pixel diameter rather than expanding to the enclosing tile rectangle.

## Rendering

Projection mode 3 is a bounded single-object replacement. It draws only in
the primary ellipse, with a clamped bilinear texture instead of panorama
seam blending/repetition. Ordinary panoramas and protected native objects
retain their existing modes. Fractional menu scroll updates both the image
and its footprint together. No extra compositing pass or per-frame texture
upload is introduced. CPU, D3D12/portable effects and DXR reflection sampling
share this mode.

## Evidence

- Fresh independent unmodified-ROM captures for 19/27/28/31:
  `tmp/ex-single-planets-reference-sep20` and matching native host captures
  `tmp/ex-single-planets-host-sep20`. All four match the fixed original
  viewport with zero pixels exceeding two channel levels; wrong-Y controls
  differ. This confirms sampled native offsets, not enhanced art identity.
- Fresh enhanced 32:9 GPU/software pairs:
  `tmp/cratered-planet-{gpu,cpu}-sep20`. Both 19 and 28 are pixel-identical.
  Visually inspected: one planet, no duplicated sides and unchanged UI.
- Relative to native finals, 217 pixels change for 19 and 8910 for 28.
  Zero changed pixels lie outside the 56-radius source-aligned footprint.
- Windows/Linux application builds and image decode/footprint/clamp tests pass.
- Choice 28's 120 FPS motion capture has 42 uniform fractional scroll steps
  in `tmp/cratered-planet-motion120-sep20`; the presentation checker passes.
- Windows hardware effects parity, hardware DXR checks, and explicit Linux
  Lavapipe Vulkan effects parity pass. DXR explicitly tests mode-3 clamping
  against a nonuniform texture, not only a compile-only check.

Other blue/icy planets and physical VR rendering remain part of the active
background work. Gameplay registration is now implemented as described below.

## Gameplay registration audit

Fresh native 32:9 LEVEL3_4 tick-1000 captures were inspected in
`tmp/cratered-gameplay-native-ex34-sep20` and
`tmp/cratered-gameplay-native-original-sep20`, including their PPU snapshots.
The EX capture shows the cratered planet partially obscured by the carrier
on the left; the Original capture is a different background phase and does
not show that planet. Equal route/tick is therefore not a matched-scene
comparison between experiences.

EX reports base X=0, Y=30, but its actual horizontal offset rows are
-111 near the top and -110 below row 120. Using base X alone to register
the enhanced disk would place it incorrectly. The native wrapped center
at source X=384 is approximately -146 relative to viewport center with
the effective -110 scroll, not +256 from the base register. Gameplay
integration must follow the effective row/column transform (including
roll and interpolation), not copy the menu's base-scroll-only setup.
The Original sample reports Y=232 and row offsets -1/0; it is not evidence
for the EX planet's location.

## Gameplay implementation and moving proof

BG_3_4B/BG_3_4D now select the same enhanced body in Original/EX. Projection
mode 4 applies the fitted cartridge row/column scroll transform to both the
image and its bounded footprint. It does not substitute the camera matrix
for those tables. Fits are computed from completed source snapshots and
interpolated fractionally before presentation, including wrapped offsets.
EX's body palette (bank 5) controls both halves of its photographic fade.
Source timing, model occlusion, stars, and UI are unchanged. The immutable
artwork uses the existing effects pass, not a new upload or render pass.

In the fresh EX source snapshot, the valid column table places the vertical
offset at 46 even though base Y is 30. Both axes therefore require the
effective tables. This supersedes the base-Y estimate in the initial audit.

- Windows and Linux application builds and backdrop unit tests pass.
- Windows GPU/software effects parity, Linux Lavapipe Vulkan effects parity,
  and hardware DXR reflection tests cover the affine mode and pass. Generated
  portable shader freshness passes.
- `tmp/cratered-gameplay-final-{gpu,cpu}-sep20` contains 32:9 stills. The
  tick-1000 pair is exact; other pairs differ by at most one channel level.
  Their source scene is the same despite different preroll counts; they
  must not be treated as four independent gameplay phases.
- `tmp/cratered-gameplay-motion120-sep20` advances 240 live presentations,
  with Right held from frame 40 for 120 frames. Its software and unenhanced
  companions are `tmp/cratered-gameplay-motion120-{cpu,native}-sep20`.
  Native/enhanced final images were inspected: carrier geometry still
  occludes the single planet correctly.
- `check_celestial_scroll.py` verifies 30 moving fractional intervals with
  maximum center displacement 1.650 pixels per presentation. The final
  GPU/software images differ at four pixels by one channel level. Against
  native, 570 pixels change, all inside the transformed planet footprint;
  none change outside it.

Original's matched approach/departure scenes, EX's later departure, and
physical VR still need visual acceptance. This is not proof that every
planet background is finished.
