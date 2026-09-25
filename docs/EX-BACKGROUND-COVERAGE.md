# EX background work — September 20

## September 23 city celestial completion

Preview 9's six moons are now detailed, individually positioned and colored
from the live source palette. Stars sharing blue ink are preserved spatially.
Exact GPU/software and native/off comparisons, disk-only change bounds,
120 FPS scroll and Windows/Linux/Android build checks pass. Current evidence
is at the top of `EX-CITY-BACKDROPS-SEPT20.md`; historical statements that these
moons remain native are superseded. This does not establish physical VR parity.

## September 22 orbital banded planets

Preview 25 / BG_5_1I and asteroid BG_2_2 no longer retain low-resolution
orange disks when Enhanced Sky is on. Detailed cached planets follow the
source's eight-color palette and preserve their circular shape. Orbital
lower-surface coverage and GPU/CPU/DXR parity are verified; see
`EX-ORBITAL-MOONS-SEPT22.md`. Earlier statements about these native planets
being preserved rather than enhanced are superseded.

## September 22 preview corrections and comet subject correction

See `BACKDROP-CORRECTIONS-SEPT22.md` for the current Fortuna exposure,
preview 19 placement, preview 26 readability, open-space comet replacement,
enhanced-menu text contrast and phased EX 5-4 planet work. Historical sections
calling BG_COMET a cavern or preview 19's enhanced offscreen position accepted
are superseded by the user's latest visual corrections.

## Fortuna moon tonal follow-up

Per user feedback, the partial moon now has a lifted upper surface and a
smooth darker lower region before it disappears into the actual sky color.
This tonal treatment is gated to the partial-moon descriptor: full Cygard
moons do not inherit it. CPU, portable GPU and reflected DXR sky share the
same response, retaining the v2 naturally clear backdrop without masks.
Windows/Linux builds, backdrop tests, GPU effects parity and hardware DXR
checks pass. Inspected current screenshot:
`tmp/fortuna-tone-gpu-sep22/ORIGINAL-LEVEL3_3-200-16_9-final.bmp`; matching
software capture in `tmp/fortuna-tone-cpu-sep22` agrees within one RGB value.

## Fortuna revision 2 — rejected masking approach removed

The user rejected the circular blue clearing. Both that implementation and
the subsequent upward-open mask are removed from CPU/GPU/DXR rendering.
Resource 213 now selects `ocean-clouds-v2`, a generated panorama with naturally
clear upper sky and low distant cloud banks. The source-positioned moon fades
directly into that artwork; no local cloud-erasure patch remains. The earlier
`fortuna-clear-*` images are rejected visual evidence, not accepted finals.
Asset and full built-in generation prompt: `assets/enhanced-backdrops/ocean-clouds-v2.md`.
Windows/Linux builds, asset decode, GPU effects parity and hardware DXR checks
pass. Accepted capture: `tmp/fortuna-v2-gpu-sep22/ORIGINAL-LEVEL3_3-200-16_9-final.bmp`.
It shows the moon fading into naturally clear sky, with low cloud banks below
and no circular clearing. 32:9 gameplay and the later EX menu-scroll image in
`tmp/fortuna-v2-menu-sep22` were captured as well. The matching software gameplay
capture differs by at most one RGB value; ground/HUD from row 112 downward
remain identical to the previous accepted ground composition. Physical VR
and other remaining celestial replacements are not established by this check.

## September 22 Fortuna clear-sky correction (verification in progress)

User clarification supersedes the earlier cloud-fade treatment below: the
moon must dissolve into clear sky, with no clouds in its immediate area.
The partial-moon path now blends away nearby clouds using a soft spatial
mask around the unique body. Its clear color comes from the panorama's
prepared zenith and follows the same live sky palette response. Both the
moon fade and the surrounding clearing use that color; distant clouds,
ground and Cygard's ordinary full moons keep their existing rendering.
This affects CPU, GPU and reflected DXR sky without another full-screen pass.
Final capture/build evidence will replace this in-progress note.

## September 22 Fortuna masked moon replacement

The shared Fortuna landscape (Original/EX gameplay and EX menu 32) now uses
the cratered master at source center (116,293), radius 28. Its lower hemisphere
fades into the photographic panorama by source row 296 instead of becoming a
full disk or a squashed ellipse. Gameplay follows the stabilized source affine
center; menu scrolling keeps its subpixel phase. The fade descriptor is not
mistaken for a second moon when applying menu scroll offsets.

The packed moon/landscape texture remains cached. Live moon tint and fade are
parameters, and only the small faded moon region needs a second sky sample.
No extra full-screen pass or per-frame image upload was added. CPU, portable
GPU and reflected DXR sky use the same fade and independent cloud color response.
Windows/Linux builds, focused backdrop tests, GPU effects parity across bank/
scroll/style/motion/brightness, and the hardware DXR faded-moon check pass.

Inspected Original LEVEL3_3 tick 200 gameplay:
`tmp/fortuna-enhanced-gpu-sep22/ORIGINAL-LEVEL3_3-200-16_9-final.bmp`.
16:9/32:9 gameplay and 32:9 EX menu images match software within one RGB value
in three pairs (`tmp/fortuna-enhanced-cpu-sep22` and matching menu directory).
The initial menu text obscures the moon; this is not standalone visible-moon
proof for that scroll phase. Other native moons and physical VR remain open.

## September 22 Fortuna moon source profile

Captured EX menu 32 and Original LEVEL3_3 tick 200 with source PPU snapshots:
`tmp/fortuna-moon-source-sep22` and `tmp/fortuna-moon-gameplay-source-sep22`.
`tools/inspect_celestial_ink.py` decodes their BG2 atlases and finds the same
single connected component for inks 97..109: 1,374 pixels in exclusive bounds
(88,265)–(144,296). The gameplay final image shows a rounded upper hemisphere
whose lower part fades into the sky. It must not be replaced with a complete
disk or a vertically squashed circle. Menu text overlaps it in the sampled
preview, so the gameplay capture is the useful visual reference. Replacement
still pending: preserve the source's lower occlusion/fade while upgrading
the visible surface.

Cygard follow-up: the 120 Hz steered 32:9 sequence in
`tmp/cygard-moons-banked-gpu-sep22` was inspected at frame 91. Both textured
moons remain unique over the banked storm/ground boundary. This does not
establish physical VR behavior.

## September 22 enhanced Cygard moons

Cygard now replaces both native flat moon disks with the registered cratered
master already used by the unique rocky planet. The landscape occupies the
upper half of one cached atlas; two nonrepeating moon slots occupy the lower
half. Original centers/radii remain 193,240/8 and 226,271/18 in source space.
The moon palette uses its own live CGRAM highlights rather than the cloud
fade. Palette/camera changes update parameters, not the atlas; there is no
additional full-screen rendering pass. Sky motion remains available.

Windows/Linux builds and focused atlas/geometry tests pass. Portable GPU/CPU
effects parity includes the new packed layout under roll, scroll, styles,
motions and brightness. The hardware DXR fixture verifies the moon color and
its next palette phase with zero upload bytes for that phase.

Inspected final in-game image:
`tmp/cygard-real-moons-gpu-sep22/EX-LEVEL6_4-950-16_9-final.bmp`.
Both textured moons appear over the storm without a repeated copy or renewed
horizon strip. Matching 180-frame natural-fade sequences are in
`tmp/cygard-real-moons-{gpu,cpu}-sep22`; other remaining native celestial
replacements and banked/VR acceptance are not claimed complete.

## September 22 round-body center correction

The earlier shear-removal formula used atlas coordinates rather than the
inverse-transformed screen center. It kept artwork round but displaced its
center as the camera banked. `stabilize_celestial_body` now inverts the coupled
source affine transform before removing shear. Independent forward-map tests
cover 81 combinations of bank and screen position plus a degenerate transform.
Windows/Linux builds and backdrop tests pass.

`tmp/celestial-registration-gpu-sep22` records 180 EX 3-2 presentations at
120 Hz, with 165 banked samples; the maximum forward registration error is
0.00111 source pixels. That scene's planet is outside the viewport, so it is
numerical evidence only. The visible cratered planet in the banked EX 3-4
capture `tmp/celestial-crater-gpu-sep22/EX-LEVEL3_4-200-16_9-final.bmp.frame-61.bmp`
was visually inspected: the replacement remains round at the authored edge
of the screen. This supersedes the earlier claim that the old stabilization
already preserved the interpolated center exactly. It does not complete the
remaining native moon replacements.

## September 22 Cygard horizon and banked palette ownership

The captured native atlas places Cygard ground at row 352, not 336. Corrected
the enhanced horizon by 16 source pixels, removing the differently tinted
strip above the ground. Landscape photographic sky now always uses the sky
palette response, including source-owned sky below the fitted bank line;
orbital layouts retain separate sky/surface responses. CPU, portable GPU and
DXR implementations share this rule.

Accepted 16:9 natural-fade captures: `tmp/cygard-horizon-fixed-gpu-sep22` and
`tmp/cygard-horizon-fixed-cpu-sep22`, EX LEVEL6_4 at tick 950 followed by 180
60 Hz presentations. All 13 final-frame pairs match exactly. Visual inspection
confirms the strip is gone and both original moons remain. The 60 source
snapshots contain 31 palette phases; all 192 checked channels converge
monotonically to FXFADECORN2. The moons are preserved, not yet enhanced.
Earlier captures in `tmp/cygard-enhanced-sequence-sep22` have the rejected
strip and must not be used as final visual evidence. Windows build, terrain
regressions and hardware DXR checks pass.

## September 22 Game Over sky

Game Over now has a dedicated photographic starfield on both experiences,
not only native stars copied into margins. Source Andross/text/star ink is
protected without a rectangular mask. Windows/Linux builds, CPU/GPU/fallback
checks, 4:3-center preservation and Half/Full SBS coverage pass. See
GAME-OVER-COVERAGE-SEPT20.md for on/off pixel evidence and final captures.

## September 22 face-planets — preview 18 and Dimension

Detailed face moons now replace the original bodies in preview 18 and
Original/EX BG_SPECIAL, preserving source placement, individual live palette
ramps, saucers and stars. Source-tile removal prevents warped original face
fragments from remaining behind the round replacements. Inspected 32:9 menu
and gameplay captures pass; matching EX software/GPU gameplay is identical.
Windows/Linux builds pass. See EX-FACE-PLANETS-SEPT22.md for accepted captures
and rejected intermediate images. The later BG_6_3H scene still needs its own
visual capture. Historical notes below listing all of 18/20/23/24/34 as
unimplemented are superseded by the individual September 22 entries, not by
a claim that every celestial object or palette transition is complete.

BG_6_3H follow-up: EX LEVEL6_3 tick 6000 reaches the actual face-planet phase;
the inspected 16:9 capture in `tmp/ex-face-late-route-sep22` confirms the same
enhancement with intact stars/saucer. Its initial tick-1000 meteor scene is
not used as evidence for this phase.

## September 22 abstract circles — EX menu 23/24

Source `SFES/GSTRATS2.ASM` sends both choices to `lastbg`, using BGBHOLECCR
and BGLASTPCR but different palettes (`a2fpal` versus `citypal`). Their captured
512x512 atlases show alternating circles centered at x=0/256 and y=128/384,
not mountains or the separate gameplay vortex. CGRAM banks 3/4 supply their
independent 15-shade ramps; ring steps are eight source pixels.

`radial_backdrop.hpp` reconstructs these analytic shapes into a cached
1024x512 image, interpolating their authored shades. The repeat overlap has
matching authored samples, retaining a logical 512x256 period without stretching
the pattern. Palette changes invalidate the sealed upload; unchanged frames
only compare 32 palette words. This adds no bitmap asset or generator dependency.
Enhanced Sky OFF remains on the untouched native path.

Source snapshots/atlases: `tmp/ex-radial-source-sep22`. Inspected enhanced
32:9 captures: `tmp/ex-radial-enhanced-gpu-sep22/comparison.png`; matching
software captures in `tmp/ex-radial-enhanced-cpu-sep22`. Both choices differ
between GPU/software by at most one RGB channel step. Their palette families
and source positions remain distinct. Windows and Linux app builds and focused
backdrop tests pass, covering source centers, black gaps, periodic seams,
palette updates and cache identity. This enhances the existing abstract art;
it does not claim new photographic planets or all-stage/VR acceptance.

## September 22 shared-ink menu sky correction

The 38-choice enhanced survey in `tmp/ex-menu-enhanced-complete-survey-sep22`
exposed native purple cloud fragments over the enhanced red landscape in choice
0. Gameplay palette classification was also being applied to flat menu previews:
shared ground/sky ink incorrectly excluded parts of the sky from replacement.
Enhanced menu landscapes now use their measured horizon boundary; separate
moon/star/flame protections remain. Their horizon slope is explicitly zero,
independent of the previous gameplay camera.

Post-fix inspected 32:9 captures: `tmp/ex-menu-sky-ownership-after-sep22`,
choices 0/3/9/32/34/36. Matching native captures are in
`tmp/ex-menu-sky-ownership-native-sep22`. RGB comparisons below and including
the source horizons (respectively rows 158/158/159/159/143/166) show zero
changed pixels for all six. Moons in 9/32 and source lava in 36 remain visible.
Choice 32's partially occluded native moon predates this correction; this
check does not establish that every celestial asset is enhanced.

Windows app build and `starfox_enhanced_terrain_tests`,
`starfox_backdrop_image_tests`, and `starfox_spectral-clouds-v1_tests` pass.
The Linux incremental build was interrupted without a compiler diagnostic;
it is not recorded as passing.

## September 22 spectral preview and menu bounds correction

Preview 34 now selects `spectral-clouds-v1` (resource 232). The reference is
`tmp/ex-special-reference-batch-sep20/choice-34.png`; GSTRATS2.ASM identifies
it as `venomhwymenu`, not Cygard. Its two cloud motifs repeat every 256 source
pixels above atlas row 344. The original brown rim and lower starfield stay
native. Gold cloud inks are also used by 106 lower star pixels, so spatial
ownership limits the replacement instead of classifying all gold ink as sky.

Final 32:9 evidence: `tmp/spectral-clouds-final-gpu-sep22/choice-34-final.bmp`
and `tmp/spectral-clouds-final-cpu-sep22/choice-34-final.bmp`. Software/GPU differ
at five pixels, maximum one RGB step. Against the matching native capture in
`tmp/spectral-clouds-native-sep22`, 19,394 sky pixels change and every pixel
from row 143 down remains exact. The first intermediate capture erased 106
gold lower stars and is superseded by these final captures. Asset and full
built-in generation prompt: `assets/enhanced-backdrops/spectral-clouds-v1.md`.

Code inspection also found the menu-only sentinel index being used to access
`environment_names` beyond its end while deriving scroll. Menu iteration now
uses an empty gameplay name and its own source scroll. This fixes undefined
behavior without inventing a new placement. Windows build and focused tests
pass; it does not establish that every historic preview failure shared this cause.

## September 22 follow-up: 1-4 cloud and angled Auto-ground seam

EX 1-4's small blue object is a cloud formation, not a planet. The proposed
photographic *planet* resource was removed. A replacement irregular blue-cloud
asset (resource 226) now maps the actual BG_1_14 gameplay route and preview 20;
the separate green limb remains native. Inspected correction captures:
`tmp/ex-cloud14-correction-sep22/EX-LEVEL1_4-1000-{16_9,32_9}-final.bmp`
and `tmp/ex-cloud20-menu-gpu-sep22/choice-20-final.bmp`. The native menu's
unique-region rule still prevents a second copy in ultrawide margins.
New photo-cloud captures: `tmp/ex-blue-cloud-menu-gpu-sep22/choice-20-final.bmp`
and `tmp/ex-blue-cloud-stage-fixed-sep22/EX-LEVEL1_4-1000-16_9-final.bmp`.
Asset prompt and conversion: `assets/enhanced-backdrops/blue-cloud-v1.md`.

Auto Ground now treats Corneria's green-to-cyan source palette ramp as grass,
not water. For landscape backdrops, source BG2 palette ownership controls
the enhanced-sky/ground boundary when banking; a fitted straight horizon is
only used for unclassified margin pixels. CPU and portable GPU effects use
the same rule. `starfox_enhanced_terrain_tests` and
`starfox_backdrop_image_tests` pass; `starfox_gpu_effects_check` passes. The
16:9 and 32:9 banked captures in `tmp/aligned-turn-auto-fix-sep22` were
visually inspected against the native banked view. A matched 16:9 software
capture differs from GPU at three bytes by at most one RGB value. This is
the Corneria angled-seam proof, not a complete all-stage survey.

## September 22 EX gameplay stage audit

A tick-1000 direct-entry GPU capture was taken for all 40 EX numbered stages
at 16:9: `tmp/ex-enhanced-coverage-sep22`. Actual background IDs, not route
names, drive the map. Previously uncovered matching scenes are now mapped:
BG_1_6A and BG_3_7A to gold storm; BG_2_3A to pale Titania clouds; BG_3_3A
to maritime clouds. Their shared-source captures are in
`tmp/ex-shared-skies-sep22`. EX2_3 Auto ground uses the snow palette, not a
grass/water split, and no longer clips all snow shading to pure white.

Additional EX gameplay mappings from the same source survey: BG_5_4 to rocky
green crags (native twin planets retained), BG_6_4 to night storm (native
moons/rim retained), BG_6_5 to a new dark ember sky (animated cartridge fire
retained), and BG_6_6 to the red cloud band. The red cloud stage uses the
actual horizon for photo coverage; a palette-only split left blocky unpainted
holes and recolored its red ground brown. New visual proof at 16:9:
`tmp/ex-missing-backdrops-sep22` and corrected BG_6_5/BG_6_6 in
`tmp/ex-missing-backdrops-final-sep22`. The new asset and prompt are in
`assets/enhanced-backdrops/ember-sky-v1.md`.

BG_2_2 has a separate jade planet-horizon panorama (resource 228); palette
17..24 preserves the distinct brown planet. Source capture and palette are in
`tmp/ex-orbital-gap-sep22`; enhanced 32:9 capture is in
`tmp/ex-wide-mapping-sep22/EX-LEVEL2_2-1000-32_9-final.bmp`. The brown planet
appears once over the newly extended horizon. Matching software capture in
`tmp/ex-mapping-cpu-sep22` differs at 20 pixels by at most one channel value.
The wide run also shows single EX1_4 cloud and BG_5_4 twin-planet subjects,
plus BG_6_4/6_5/6_6 sky coverage. For BG_5_4 the CPU/GPU difference is one
pixel by one value; for BG_6_6 they are identical. Asset prompt and conversion:
`assets/enhanced-backdrops/jade-planet-horizon-v1.md`.
Tunnel interiors, the large foreground model in BG_5_2, and other special
non-landscapes are not claimed covered by this photo map.

BG_6_3 is a full-height dense asteroid field rather than a ground panorama.
It now selects the existing centered asteroid-belt photograph (resource 219)
with its source scroll offset. The large foreground objects, comms and HUD
remain on their own layers. Inspected 16:9/32:9 captures:
`tmp/ex-dense-asteroids-sep22`; 32:9 software/GPU differs at eight pixels by
one channel value. Other tunnel and foreground-model scenes need separate
treatment, not a catch-all landscape assignment.

BG_2_5 (EX 1-5/2-5) and BG_3_6 (EX 3-6/4-3) use the same distant jade
planet surface as BG_2_2, but their native BG2 horizon is at screen row 200
with a different scroll source. Their BG3 offset words move other objects;
using those offsets for the planet pulled it toward the top. The stage-specific
BG2-scroll correction and exclusion from local Enhanced Ground geometry keep
the planet at the bottom. Final 16:9 visual captures are in
`tmp/ex-final-horizons4-sep22`.

BG_3_7C (EX 4-5) is an abstract magenta/amber field, not a landscape. It now
uses a dedicated full-sky panorama (resource 229); model, HUD and controls
remain separate. The source's magenta/amber palette banks drive color changes.
The initial vortex was visually unlike the source. A second, smooth-band
version was generated against the native 4-5 capture; final 16:9 and 32:9
captures are `tmp/ex-andross-bands-sep22`. The original is recorded in
`tmp/ex-boss-native-reference-sep22`. The earlier
`tmp/ex-dimension-enhanced-sep22` evidence and its CPU parity apply only to
the superseded image, not the revised texture. Asset and complete prompt:
`assets/enhanced-backdrops/dimension-vortex-v1.md`.
The final pass corrected the panorama's horizontal phase: amber is on the
left, magenta on the right, with the dark central opening retained
(`tmp/ex-boss-BG_3_7C-16_9-enhanced-final-sep22.bmp`).

The Mario/Luigi EX finale backgrounds were separately compared to their
cartridge BG_6_7A/B/C frames (`tmp/ex-boss-BG_*-native-sep22.bmp`). BG_6_7A's
angular amber rim is retained natively; a generated alternative was too
close to a mere upscale to warrant replacing it. BG_6_7B now uses a single
fire-band panorama (resource 231) above its original ground, not a repeated
planet (`tmp/ex-boss-BG_6_7B-16_9-enhanced-final3-sep22.bmp`). BG_6_7C uses
the jade planet-horizon art with the source BG2 scroll register; the final
16:9 and 32:9 captures are `tmp/ex-boss-BG_6_7C-16_9-final2-sep22.bmp` and
`tmp/ex-boss-BG_6_7C-32_9-final2-sep22.bmp`.

The EX gameplay planet replacements now discard the source Mode-2 affine
shear while retaining its interpolated centre. This keeps round planets round
during lateral movement; the focused geometry test and the steered EX 3-2
capture in `tmp/ex-celestial-stable-right-sep22` cover the change. Native
unmodified skies retain their cartridge scroll.

The Original-stage audit at `tmp/original-enhanced-coverage-sep22` found
matching landscape lists without an enhanced assignment. Original 3-1's red
mountains, 2-2's jade limb, and 3-6's jade limb now use the matching existing
assets. Original 1-5 aliases BG_2_5's ID and uses that same horizon mapping;
`tmp/original-horizon-alias-fixed-sep22` verifies the alias. Original 1-4's
distinct amber-blue cloud now has resource 230 at its native position, while
the nearby green planet stays native. Asset/prompt:
`assets/enhanced-backdrops/amber-blue-cloud-v1.md`; capture:
`tmp/original-cloud14-enhanced-sep22`. Special tunnels remain separate from
the photo-backdrop map and are not silently assigned an unrelated sky.

The EX title with hardware ray tracing had a 59.5 ms median render-work
baseline despite no shadow-casting geometry in the logo/map phase. The
runtime now checks both resident GPU geometry and CPU fallback casters and
skips a full hardware shadow dispatch when both are empty. A paced 60 Hz
240-frame retest with hardware DXR enabled measured 60 FPS, 2.4 ms median
frame work, and 3.5 ms p95 (`tmp/ex-logo-fps-guard-paced-sep22.log`).
This fixes that empty-scene performance fault; it does not assert every
boss or effects-heavy scene is now 60 FPS.
With the same 60 Hz paced fixture, Original and EX credits and the live
planet-select map also measured 60 FPS (`tmp/credits-fps-dxr-sep22.log`,
`tmp/credits-ex-fps-dxr-sep22.log`, `tmp/planet-select-fps-dxr-sep22.log`,
`tmp/planet-select-ex-fps-dxr-sep22.log`). That rules out a current hard
20-FPS presentation lock in those sampled scenes, but not intermittent
credits spikes or another route/setting.

## Distinct blue planets

Menu 27/31 now have separate realistic storm-cloud and diagonally banded
planets, retaining their native positions and sizes. Both have inspected
32:9 GPU/software captures and uniform 120 FPS scrolling proof. The banded
asset is also verified in Original's intro. BG_3_2's storm-planet gameplay
mapping is implemented but its visible phase still needs acceptance.
See EX-BLUE-PLANETS-SEPT20.md for assets, prompts, evidence and limits.

## Single cratered planet

EX menu 19/28 now have a realistic orange cratered planet using a bounded
single-object texture mode, preserving native placement and surrounding
stars. CPU/GPU finals match exactly; Windows/Linux builds, effects parity,
image tests and hardware DXR checks pass. See EX-CRATERED-PLANET-SEPT20.md.
The same asset is now wired to BG_3_4B/BG_3_4D using fractional native
row/column transforms. EX approach has inspected moving 120 FPS GPU/software
proof; Original and departure visual acceptance remain open. Other unique
planets and VR coverage are not complete.

## Lava-cave coverage

Preview 36 and Comet gameplay now have a dedicated photographic cavern,
while preserving animated native lava and flame strokes. Windows/Linux
builds and focused tests pass; inspected 32:9 CPU/GPU captures differ only
by rounding. See EX-LAVA-CAVERN-SEPT20.md for exact proof and limits.

## Celestial objects and Game Over acceptance

Enhanced Sky must preserve every authored planet/moon: identity, placement,
count, scroll and live palette/fade. A generic landscape or nebula without
that object is not an acceptable replacement. Prefer upgraded celestial
artwork; keep the original object until a suitable replacement is available.
This includes menu previews as well as gameplay. Remaining unique-object
backgrounds (18/20/23/24/34) are still outstanding artwork work,
not implicitly complete because their original rendering is retained.

Moon/star ink protection for city artwork 6 and Fortuna artwork 13 now uses
one shared helper, independent of preview versus gameplay. Unit tests render
all 256 source indices against a replacement panorama and verify exact
celestial retention without keeping the surrounding rectangular sky fill.
The orbital entry moon continues to use its separate positioned keep region.

Game Over remains part of acceptance: actual stars must extend into the
wide margins, with Andross left unique and intact, with Enhanced Sky both off
and on. `check_game_over_gpu.ps1 -EnhancedSky` now explicitly sets all six
environment settings rather than inheriting the user's saved settings.
Photographic Game Over artwork and physical VR coverage are not claimed by
the native extended-star fixture.

The follow-up in GAME-OVER-COVERAGE-SEPT20.md adds a true full-software
backend run (the earlier `cpu` case selected only CPU late stars), later
2x animation captures, and an exact aspect-corrected 4:3-center comparison.
Original and EX pass; stars occupy both widescreen/SBS margins without
changing the central artwork. GAMEOVER/INTROMAP/TITLEMAP can now be audited
with explicit source-atlas captures rather than only playable level entries.

Fresh Windows build and celestial/IRQ unit tests pass. Enhanced-Sky-on Game
Over checks pass for Original and EX: mono GPU/CPU/fallback equality, Half/Full
SBS dimensions, and real star pixels in both eyes' margins. Proof:
`tmp/game-over-enhanced-sky-sep20`; Original mono was visually inspected.
Fresh 32:9 previews 9/25/32 in `tmp/celestial-preservation-sep20` were also
visually inspected: city moons, orange orbital-entry moon and Fortuna moon
remain visible over the enhanced panoramas. These retain original celestial
artwork; realistic replacements and the remaining atlas audit are still open.

## Space panorama edge sampling

Space-only panoramas (asteroid belt, fine debris and red/violet nebula) now
repeat vertically through a short smooth overlap instead of clamping the
last image row into streaks. Projection mode 2 selects this behavior; mode 0
landscapes and mode 1 planet horizons retain their existing clamps. CPU,
effects shaders and DXR reflected environments share the behavior.

Windows/Linux app builds and backdrop seam/period/clamp unit tests pass.
Windows GPU parity covers positive/negative scrolling, rolled full-space
projection, styles and protected layers. DXR test directly samples a
nonuniform four-row image beyond its vertical boundary and expects the
wrapped color. Windows GPU and DXR tests pass. Portable effects regenerated.
Fresh inspected gameplay: `tmp/ex-nebula24-wrap-gpu-sep20` at EX 2-4 tick
1000, 32:9. The bottom now contains continued stars/nebula, not extruded
vertical streaks. Broader stage/motion and physical VR acceptance remain open.

## Red/violet nebula gameplay follow-up

Resource 221 now uses ember-nebula-v2: targeted built-in image edit restores
the violet cluster while keeping separate red clouds. EX BG_2_4 now selects
it, with center 312 minus the effective source Y scroll. Windows build and
asset decoding pass. Actual 32:9 enhanced gameplay capture inspected:
`tmp/ex-nebula24-v2-gpu-sep20/EX-LEVEL2_4-1000-32_9-final.bmp`.
Both colors appear, unlike the rejected red-only mapping. Menu capture is in
`tmp/ex-nebula29-v2-menu-sep20`. Faint vertical edge extrusion is visible near
the bottom because panorama sampling clamps beyond its vertical bounds;
edge handling remains an explicit acceptance gap. Linux build was started
but has not yet been confirmed complete. Full palette/motion/VR acceptance
is still pending. Artwork edit prompt is in ember-nebula-v2.md.

## Sparse red nebula (menu 29)

Added distinct ember-nebula-v1 (resource 221), rather than reusing Sector K's
two-color artwork. Windows app build, terrain mapping and asset decode tests
pass. Inspected actual 32:9 menu capture:
`tmp/ex-ember-nebula-menu-sep20/choice-29-final.bmp`.
Sparse red/orange filament clusters retain large black gaps and stars; menu
text is intact. Enhanced Sky off retains the native atlas. This is menu-only
coverage; gameplay mapping and portable/VR runtime validation remain open.
Asset provenance and complete prompt: assets/enhanced-backdrops/ember-nebula-v1.md.

Gameplay mapping investigation: GSTRATS2.ASM a2424 selects BG24 tiles for
menu 29; BGS.ASM BG_2_4_1 uses those same tiles with palette 2f and scroll 232.
Fresh native capture `tmp/ex-nebula24-native-sep20` at tick 1000 shows red AND
purple nebulae. A proposed red-only gameplay assignment was therefore removed
before acceptance. The stage still needs a two-color/palette-aware replacement;
the existing menu artwork must not silently eliminate the purple component.
Menu 29 artwork is now explicitly anchored at atlas center 312 minus its live
effective scroll, instead of a fixed screen-space center.

## Fine-debris replacement

Dedicated fine-debris-v1 artwork (resource 220) now covers menu 17 and
BG_1_2, distinct from menu 6's larger-rock asset. Its belt center is atlas
row 352, following the effective source scroll: lower in the menu at Y=200,
near the middle in the sampled gameplay at Y=232. Windows app build,
terrain mapping test and asset decode test passed. Inspected 32:9 captures:
`tmp/ex-fine-debris-menu-sep20/choice-17-final.bmp` and
`tmp/ex-fine-debris-game-sep20/EX-LEVEL1_2-1000-32_9-final.bmp`.
Full bank/scroll/VR acceptance and portable rebuild with resource 220 remain.
Portable follow-up: the Linux app subsequently built successfully with
resource 220 embedded; fine-debris asset and terrain mapping tests passed.
Generated source and prompt are in assets/enhanced-backdrops/fine-debris-v1.md.

Banked follow-up: matching EX LEVEL1_2 tick-1000 runs with 60 presentation
frames holding Right are in `tmp/ex-debris-bank-{gpu,cpu}-sep20`. Inspected
GPU image: the belt tilts with the scene while communications and HUD stay
upright. Decoded CPU/GPU outputs differ at one pixel, maximum RGB difference
one. This does not establish complete motion continuity or VR correctness.

Remaining special-artwork constraints: original menu 18 contains distinctive
face-planets and colored star shapes; it must not be replaced wholesale with
generic debris. Menu 20 has a blue cloud formation and separate green limb
above its lower atmosphere bands. The blue formation is not a planet in 1-4.
Native reference images for both were inspected before further artwork
assignment; their photographic replacements remain pending.

Fresh native 32:9 edge sweep: `tmp/ex-special-edges-current-sep20`, menu
18/20/23/24/27/28/29/31. All eight final captures inspected. The large
planets in 27/28/31 remain single instances with stars across both margins.
23/24 are circular abstract patterns, not ordinary planetary landscapes;
29 is the red nebula motif and still needs a distinct enhanced treatment.
18 retains its different face-planets. These observations are sampled native
coverage, not evidence that their enhanced versions are finished.

This is a partial coverage record, not acceptance of every enhanced backdrop.

## Central asteroid belt (September 20)

EX menu 6 now has dedicated asteroid-belt-v1 artwork (resource 219), with
its central band positioned from the original center scroll offset rather
than pushed to the top/bottom. The 32:9 GPU capture in
`tmp/ex-asteroid6-gpu-sep20/choice-6-final.bmp` was visually inspected:
the asteroid concentration crosses the middle and the menu remains intact.
Matching software capture is in `tmp/ex-asteroid6-cpu-sep20`.
Decoded pixels differ at six locations, by at most one RGB channel value;
file hashes alone differ and are not the parity measurement.
The asset decode test passes. This is a menu reference-phase check, not
full gameplay or VR acceptance; background 17 remains a separate case.

Portable follow-up: Linux app build completed successfully and the dedicated
asteroid asset decode test passed. Original EX 1-2 capture at tick 1000,
32:9, is `tmp/ex-asteroid-gameplay-reference-sep20/EX-LEVEL1_2-1000-32_9-final.bmp`.
Visually inspected: its fine asteroid band crosses the middle, with scroll
(0,232), Mode 2 and map background 0x39. The original BG 17 menu reference
instead concentrates the fine debris lower in the view. Do not reuse the
menu-6 placement constant for menu 17 or gameplay without source-offset mapping.
The gameplay scene has not yet been assigned the new photographic artwork.

## Smooth dune artwork (September 20)

EX menu 8 and gameplay BG_7_2 now select dune-horizon-v2 (resource 210),
instead of rocky desert ranges for the menu and procedural sky in gameplay.
The generated panorama preserves smooth orange dunes under a pale blue/lavender
sky. Original ground and UI remain separate. Rocky menu 4 retains its existing
artwork. Windows and portable resource manifests, lazy loader and decode test
include the new asset. PNG/BMP and prompts are in assets/enhanced-backdrops.

The first generated variant had uneven horizontal lighting visible as a wrap
stripe in actual gameplay. A built-in image edit corrected edge exposure; v1
is retained as rejected source, not packaged. Final v2 menu and gameplay GPU
captures were visually inspected in `tmp/ex-dunes-v2-{menu,gameplay}-gpu-sep20`;
matching software captures use the corresponding `cpu` directories. Build,
decode and mapping/terrain tests pass. Broader scroll-phase/late-stage/VR
acceptance remains open; no release was pushed.

## Matching storm preview and banked alignment check

Menu choice 12 (the authored fiery-cloud bank matching 5-5/7-5) now selects
the same golden-storm resource and row-360 horizon as those gameplay scenes.
Choice 22 remains separately supported. New 32:9 runtime capture:
`tmp/ex-menu-storm12-sep20/choice-12-final.bmp`, visually inspected.
Windows build and terrain/assignment tests pass.

Source-offset change follow-up: matched Original Corneria runs at native 1x,
16:9, tick 1000 plus 60 presentation frames holding Right, unlocked/GodMode:
`tmp/aligned-turn-{native,enhanced}-sep20`. Both final images inspected:
the replacement follows the banked ground boundary. Earlier `aligned-bank-*`
uses R instead of Right and rolls only the ship, so it is not proof of tilted
background behavior. These are targeted samples, not full motion/VR acceptance.

## Storm gameplay and source-offset alignment (September 20)

Per user clarification, golden storm artwork is now assigned to EX BG_5_5
and BG_7_5, not only menu 22. BG_7_1 now uses the cloud-plains artwork from
menu 7. LEVEL6_6 was sampled at ticks 0/300/1800/3000 with GodMode and unlocked
timing: all retain BG 0x1a1, the separate red volcanic background, including
the boss sample. Evidence: `tmp/ex-66-storm-mapping-sep20`. It is not replaced
with the golden storm on that evidence; other source transitions remain open.

New storm captures exposed a shared alignment problem: the artwork used BG2
scroll 232 while the source renderer consumed the valid BG3 offset row 248.
Enhanced landscapes now use the same center offset (column 15) when valid,
falling back for invalid entries or other modes. The storm atlas ground starts
at row 360, independent of its palette-classification origin 248. Added tests
for valid/invalid/Mode-1 offsets and explicit gameplay horizons.

Final captures: `tmp/ex-storm-aligned-{gpu,cpu}-sep20`, 5-5/7-5/7-1 at tick
1000, 16:9, native 1x, Enhanced Sky. CPU/GPU differ in 1/1/2 pixels respectively,
maximum one channel value. 5-5 visually inspected; its entire region below
screen row 112 is pixel-identical to the native no-enhancement capture in
`tmp/ex-storm-ground-reference-sep20`. This supersedes misaligned intermediate
captures in `tmp/ex-storm-plains-gameplay-*`. Build and terrain/mapping tests
pass. Full palette-transition acceptance is still pending; no VR/release push.

## Golden storm artwork (September 20)

EX menu choice 22 now uses the generated gold-storm-v1 panorama (resource 209)
when Enhanced Sky is enabled. Original source atlas captured in
`tmp/ex-gold-storm-before-sep20` confirms the row-360 ground boundary. The
original ground and UI remain separate; choice 34/Cygard's unique face is
explicitly excluded. PNG, lossless RGB BMP and full prompt/provenance are in
`assets/enhanced-backdrops/gold-storm-v1.*`. Windows and portable embedding
manifests include the new asset; the existing lazy/resident image path is reused.

Windows build, asset decode and mapping/horizon tests pass. Actual 32:9 captures
in `tmp/ex-gold-storm-{gpu,cpu}-sep20` differ in two pixels, maximum one channel
value. GPU capture inspected for cloud coverage, retained ground and readable UI.
Current executable updated. This is not all-background, gameplay palette, or
VR acceptance; no release was published.

## Additional gameplay landscapes (September 20)

EX BG_3_1C now selects red-dusk artwork, and BG_7_3/BG_7_4 select alpine
artwork. Assignments are explicitly keyed by background symbol and experience;
Original BG_3_1C, EX Training and unmatched unique-planet scenes are excluded.
Existing Original Corneria/Training and Macbeth assignments are preserved.
The existing live upload-palette response, camera roll and source scroll paths
also apply to these newly registered images. No per-frame texture regeneration
or new terrain geometry is introduced.

Proof: `tmp/ex-gameplay-landscapes-{gpu,cpu}-sep20`, levels 3-1/7-3/7-4,
tick 1000, unlocked timing, GodMode, native 1x, 16:9, Enhanced Sky only.
Current PPU snapshots and native atlases accompany GPU captures. All three
final GPU images inspected. CPU/GPU differences: respectively 1/4/4 pixels,
maximum one channel value. Windows build and assignment/terrain tests pass.
This verifies those samples, not all palette transitions, late-stage swaps,
or remaining landscape artwork. Current executable updated; no release/VR push.

## Additional landscape mappings (September 20)

EX choices 14 (snowy ranges) and 16 (red dusk ranges) now select the existing
alpine and Macbeth-dusk panoramas when Enhanced Sky is enabled. Original BG2
atlas captures in `tmp/ex-alpine-dusk-before-sep20` establish the source-row-360
sky/ground boundary for both; each uses its original animated X offset and
Y=200. Ground and text palettes remain untouched. Landscape asset assignments
are now an explicit tested table; orbital/space/unknown choices return no
landscape asset instead of falling through to alpine art.

Build and enhanced-terrain tests pass. Fresh reference-phase 32:9 captures in
`tmp/ex-alpine-dusk-{gpu,cpu}-sep20` were visually inspected. Choice 14 differs
in seven pixels by at most one channel value between CPU/GPU; choice 16 is
pixel-identical. Both retain their native ground colors with the replacement
meeting the original boundary. Current Windows executable updated. This does
not claim all missing artwork, gameplay mappings or VR acceptance are finished.

## Independent original-menu placement reference (latest)

`tools/capture_ex_menu_reference.ps1` now captures all 38 choices using the
unmodified EX ROM in Snes9x, navigating by controller input only. No RAM/ROM
patches or save states are used. Images and verified page/cursor/choice logs:
`tmp/ex-menu-independent-sep20`. A read-only observer repeat additionally records
PPU registers in `tmp/ex-menu-reference-registers-all-sep20`; the optional local
core export is in `tools/reference_bg2_observer.inc` (not shipped with the game).

| Original choice | Mode / tiles | Source Y | Effective placement |
| --- | --- | --- | --- |
| 0..6, 36 | 2 / 16px | 0 | BG3 vertical row $4111 (273px) |
| 7..10, 12..19, 22..24, 26, 29, 32..34, 99 | 1 / 8px | 200 | Source scroll |
| 11, 21, 27, 35 | 1 / 8px | 230 | Source scroll |
| 20, 25, 31 | 1 / 8px | 250 | Source scroll |
| 28, 30 | 1 / 8px | 0 | Source scroll |

X scroll is animated and remains source-controlled, not replaced by a static
per-choice centering value. Reference capture phases differ from host captures.
All measured records are preserved in `tests/data/ex_menu_reference_offsets.csv`;
the X column records the capture phase, not a constant to force during play.

This supersedes the earlier guessed Mode 2 landscape-origin override below:
host menu entry retained the intro's $4010 row and disabled per-column offsets.
It now installs the independently observed title-menu handoff row and enables
Mode 2 offsets, leaving Mode 1's individual X/Y values untouched. Background 2's
missing nebula and Macbeth's shifted horizon are visibly restored in
`tmp/ex-menu-native-origin-fixed-sep20`. Enhanced sky now also accounts for that
effective source offset. Complete pixel/animated comparison and remaining
artwork placement are still open; 38 captures are not 38 accepted fixes.

### Scroll-matched layer comparison

The host fixture can now advance the real source menu to each recorded X phase
(`-ReferencePhase`) and capture native 4:3 layers. The independent fixture's
`-BackgroundOnly` uses Snes9x's layer visibility options; it still does not patch
the ROM or RAM. Choice 99 explicitly requires a black BG2 frame rather than
silently accepting an unexpected blank. Complete initial paired sets:
`tmp/ex-menu-matched-all-sep20` and `tmp/ex-menu-reference-bg2-all-sep20`.

The first comparison found a common one-scanline visible-frame convention offset.
After accounting for it, 33/38 choices matched within two channel levels throughout
the 224x190 native visible window. Choice 1 differed in animated water (1,008
pixels), choice 2 in 13 star pixels, choice 36 in animated lava (2,521 pixels).
Choices 10/12 each contained one transparent row painted magenta by the host's
diagnostic palette, not by the game. These are reported differences, not an
unqualified pixel-parity pass. Deliberately shifting Y by eight pixels produces
large mismatches, showing that the comparison detects wrong origins.

The actual menu presentation now advances both Mode 1 sampling and Mode 2's
copied offset row by one visible scanline; source PPU/VRAM remain unchanged.
`tools/compare_ex_menu_reference.py` consequently performs no best-fit alignment.
The initial capture set predates that final one-line correction and must not be
used as evidence of its post-fix result.

Orbital follow-up in progress: the software, SDL GPU and DXR reflection paths
now share configurable panorama scale/horizon placement and full-frame coverage.
Two optional screen-space ellipses preserve unique authored planets instead of
repeating them with the panorama. Existing landscapes retain their sky-only
defaults. Ocean/volcanic orbital artwork and night-city artwork are now embedded
for desktop and mapped to EX menu choices 21/35/11. Inspected actual menu captures
are in `tmp/ex-menu-orbital-{gpu,cpu}-sep20`; horizons are level, planet surfaces
fill below them, city towers remain below the sky, and menu lettering is retained.
Seven asset/projection tests pass and the D3D12 effects checker covers orbital
mapping, protected ellipses, bank, scroll and fades. Gameplay orbital integration,
remaining menus and realistic individual moons are still outstanding.

## Native menu audit

`tools/capture_ex_menu_backgrounds.ps1` drives the real cartridge menu through
Right presses to each PGBG choice; it does not substitute a stage screenshot.
The test-only entry requires STARFOX_TEST_FRAMES and does not change normal
launch behaviour. All choices 0..36 and 99 were captured at 16:9 in
`tmp/ex-menu-aligned-sep20`. Images were inspected except 2, 6 and 99.

Mode 2 landscape choices previously retained zero title scroll, exposing empty
sky rather than their landscape strip. Origins now match the verified native
atlas profiles already used by VR. Mode 1 retains its source scrolling.

Enhanced Sky now recognizes the actual menu choice, not only BG_TITLE, and
supports both tile modes, all tilemap page sizes, and 16x16 flipped characters.
Menu BG2 is scenery; BG1 lettering and sprites remain protected UI. Palette
classification caches distinguish PGBG changes under the same background ID.

Photographic replacements currently wired for menu choices:

| Choice | Replacement |
| --- | --- |
| 0 | Macbeth dusk |
| 3 | Storm night (new) |
| 4, 8 | Desert horizon (new) |
| 13, 15 | Alpine panorama |
| 11 | Night city |
| 21 | Ocean planet horizon |
| 35 | Volcanic planet horizon |

Other registered landscape menu choices have procedural sky styles, **not yet
their requested realistic replacement artwork**. Planet, city, nebula, lava,
abstract/Dimension and remaining landscape replacements are still pending.
All selections also need animated/widescreen visual review, including unique
planet placement; captures alone do not establish correctness.

## Scramble horizon

EX BG_5_1I/BG_5_1E were receiving checkerboard tunnel scanline offsets because
the carrier/boss scene sets INATUNNEL. These two orbital atlases now bypass
the alternating 24/280 page offsets, and desktop presentation uses their actual
PPU scroll rather than stale gameplay scroll scratch variables. Real tunnels
keep the original run-length offset decoding.

Inspected capture before: `tmp/ex-scramble-sep20/EX-LEVEL5_1-60-16_9-final.bmp`.
After: `tmp/ex-scramble-horizon-sep20/EX-LEVEL5_1-60-16_9-final.bmp`.
Equivalent 6-1/7-1 captures exist. The horizon is continuous, with the planet
below it rather than cut into floating strips. This does not replace the
authored planet surface with a new realistic asset.

Actual source transition traces for LEVEL5_1/6_1/7_1 through tick 2200 verify
scanlines remain disabled for both orbital IDs, including the later transition
at ticks 2169/2156/2099. The transition checker now rejects their regression.
Original/EX hitlist tests still pass all 32 genuine tunnel phases, scroll
override and scene exit cases. Terrain palette-layout tests and all four
backdrop decode/sampling tests pass (seven targeted CTests total).

Enhanced menu CPU/GPU final captures are in
`tmp/ex-menu-enhanced-{cpu,gpu}-sep20`: choices 4 and 8 are byte-identical;
choices 3/13/15 differ in only 1/1/2 pixels respectively, by one colour level
at most (floating-point image sampling). No larger differences were found.
These are targeted stills, not every effect combination or performance proof.

### Final visible-scanline correction check

The presentation-only Mode 2 scanline adjustment is applied after raster
interpolation, which otherwise overwrote it. Source VRAM remains unchanged.
The Windows build succeeded. Fresh native captures in
`tmp/ex-menu-visible-line-final-sep20` compare at identical pixel coordinates
against the independent BG2 references: choices 0 and 25 have no pixels over
two color levels of difference; choice 2 retains only the 13 previously
identified star pixels. No one-line comparison shift is used. This verifies
both tile modes on these examples, not the remaining enhanced artwork.

### Enhanced late orbital surface

BG_5_1E now uses the enhanced ocean planet panorama as well as BG_5_1I.
Its source limb is row 400, separate from entry row 420; the replacement
fills below the limb instead of returning to black after the narrow native
surface strip. The entry's single orange moon exception is not applied here.
Windows compilation succeeded. Six captures in
`tmp/ex-orbital-exit-enhanced-alive-sep20` cover LEVEL5_1/6_1/7_1 at tick 2200,
16:9 and 32:9, with enhanced sky, UNLOCKED timing and GodMode for survival.
Every log confirms actual background 0x13b, scroll (0,232), no tunnel offsets.
The 5-1 16:9 and 7-1 32:9 final images were visually inspected: a continuous
bottom planet surface, no alternating black horizon strips, preserved UI.
The earlier `...exit-enhanced-sep20` and `...exit-enhanced-unlocked-sep20`
captures had died/restarted into another background and are NOT proof of this
fix. The capture script now exposes TimingMode explicitly for reproducibility.
Animated transition/roll review and remaining replacement artwork are pending.

### Post-fix original widescreen placement comparison

All 38 choices were recaptured after the final scanline correction in
`tmp/ex-menu-native-widescreen-final-sep20` using ReferencePhase, PpuSnapshot,
16:9, original artwork and no enhancements. The comparator now accepts
symmetrically expanded native-resolution captures and extracts the known
center 256 pixels without rescaling or best-fit searching. Every choice's
source scroll/mode matches the independent reference record. The center
224x190 original visible window has 33/38 choices within two channel levels;
the same five differences documented above remain (animated water/lava,
13 star pixels, and two transparent diagnostic rows). Choice 99 is exactly
black. No one-line comparison adjustment is needed. Choice 6's final image
visibly places the dense meteor band in the middle; final images 18 and 21
were also inspected for native face-planet and bottom-horizon placement.
An initial comparison read choice 99's unfinished log; after the capture
process exited successfully it was rerun and passed the phase/blank checks.

This proves native-center placement at the recorded phases, not complete
32:9 uniqueness, animation parity, or enhanced artwork alignment. In
particular, the enhanced landscape horizon is still inferred from the
palette-classification origin plus 128; this needs a separate source-limb
review rather than changing the now-verified original sampling coordinates.

### Separate enhanced landscape boundary (follow-up)

The six photographic landscape choices 0/3/4/8/13/15 now have explicit
original terrain boundaries independent of palette sampling: atlas row 432
for the three Mode 2 choices and row 360 for the three Mode 1 choices.
This removes 8–16 pixels of enhanced sky spilling below their actual horizon.
Original sampling and the conservative palette-classification bands remain
unchanged. Other choices retain their prior behavior pending individual review.
The Windows build and enhanced-terrain test executable pass, including the
new boundary checks. Reference-phase final captures for all six choices are
in `tmp/ex-menu-enhanced-horizons-final-sep20`. Final images 0, 3 and 13 were
visually inspected. Comparing each complete 16:9 output against its original
counterpart finds **zero changed pixels below the boundary** (screen row 158
for Mode 2, 159 for Mode 1), proving Enhanced Sky no longer overwrites ground
or menu text there. Remaining artwork and ultrawide uniqueness remain open.

### Native menu unique planets at 32:9

Menu choices 9, 18, 19, 25, 27, 28 and 31 now supply unique-art rectangles to
both CPU and GPU BG2 passes. Stage-ID detection did not cover these because
the EX menu keeps BG_TITLE while replacing the tiles. Existing unique-region
rules preserve the native window and only remove wrapped copies outside the
primary atlas; surrounding stars/city/planet surfaces remain repeatable.
The face rectangles reuse the existing gameplay table. Other moon bounds use
the existing VR atlas profiles, with city moons checked against its tilemap.
Replacement ink is palette 15 for space, 1 for city, 14 for face planets:
palette zero is colored on these atlases and must not be used as black.

Before: `tmp/ex-menu-unique-before-sep20`, visibly duplicated moon at the left
of choice 28 and a city moon at the left of choice 9. Final GPU/software sets:
`tmp/ex-menu-unique-final-{gpu,cpu}-sep20`, all seven choices at 32:9 with
ReferencePhase. Build succeeded. All seven full-frame CPU/GPU pairs are
byte-identical; each center 256-pixel region is byte-identical to its earlier
16:9 original-art capture. Final choices 9 and 28 were visually inspected.
Intermediate `unique-fixed-*` captures used the wrong zero palette and are
not accepted evidence; that colored-edge regression is corrected in final.
Other menu backgrounds and full animated phase coverage are still pending.

### Enhanced nebula artwork

EX menu choice 2 now uses generated red/blue astronomical nebula artwork when
Enhanced Sky is enabled. Asset `nebula-red-blue-v1` preserves its original PNG,
lossless BMP and full prompt/provenance Markdown under `assets/enhanced-backdrops`.
Resource 207 is wired into Windows RC and portable embedding, with a lazy
runtime load and an asset decode/sampling CTest. It uses full-frame panorama
coverage and the menu's source horizontal scrolling, retaining BG1 lettering.
The Windows build and new asset test pass. Actual 32:9 menu captures in
`tmp/ex-menu-nebula-{gpu,cpu}-sep20` were compared: maximum difference one
channel level, no pixels exceeding two levels. The GPU final was visually
inspected for coverage, wrap and menu readability. Native artwork remains
selected when Enhanced Sky is off. This does not claim individual enhanced
moons, remaining landscapes or VR integration are complete.

No headset validation, release publication, Quest workflow addition or PCVR
package is claimed here. Those remain queued after the background work.

### Sector K gameplay

EX BG_5_3 (0x159, LEVEL5_3) now selects the enhanced nebula asset used by
menu background 2. This applies only with Enhanced Sky enabled; native art
remains unchanged otherwise. The full-frame space panorama uses one sky
response across both sides of its projection horizon, avoiding an artificial
surface-color boundary. The response comes from the current upload palette
and CGRAM, not a fixed animation clock.

Proof: `tmp/ex-sector-k-enhanced-sep20`, ticks 1000/2000/3000, 16:9,
unlocked timing and GodMode; matching software tick 2000 in
`tmp/ex-sector-k-enhanced-cpu-sep20`. CPU/GPU maximum channel difference is
one, with none above that tolerance. Ticks 1000 and 3000 were visually inspected.
The sampled reference channel sums are 1551/1452/1563; live sums are
1027/917/1047, then 1551/1452/1563, then 1035/921/1052, demonstrating the
dim/bright/dim response rather than simply trusting changed-entry counts.
This models regional color balance; arbitrary palette-index cycling is not
claimed pixel-equivalent to the original animated artwork.

### Original menu background 32: one moon

The repeated moon is removed from expanded margins by resampling its matching
moon-free atlas region 256 pixels to the right, at the same source row. Bounds
are x=88..143, y=264..303. This preserves source sky/cloud colors rather than
painting a rectangle. The native 256-wide center and primary atlas occurrence
are untouched. The optional replacement offset defaults to zero, preserving
existing flat-fill unique-region behavior for other backgrounds.

CPU, GPU and DXR reflected-background paths support the replacement sample.
GPU parity fixtures now exercise positive and negative offsets; all 432
background cases pass. The DXR test also confirms resampling wins over the
flat replacement color. Portable background shader regenerated; Windows build
passes. Actual 32:9 reference-phase captures:
`tmp/ex-menu-moon32-fixed-{gpu,cpu}-sep20/choice-32-final.bmp`.
They are pixel-identical. Compared with the previous 32:9 capture, exactly
1374 pixels change, none in the native center. The final image was visually
inspected: one moon and uninterrupted sky/clouds at the former duplicate.
# Cloud plains follow-up (September 20)

EX menu choice 7 now has a generated cloud/low-hills photographic replacement,
resource 208, enabled only by Enhanced Sky. The original horizon is source row
360 (screen row 160 at the reference Y offset 200), not the palette sampling
origin plus 128. Ground remains original in the native pre-game menu.
Artwork and complete prompt: assets/enhanced-backdrops/cloud-plains-v1.md.
Windows and portable resource manifests include the asset; decoding and horizon
tests pass. The panorama is loaded once through the existing cached image path.

Fresh reference-phase 32:9 captures:
tmp/ex-cloud-plains-gpu-sep20/choice-7-final.bmp and
tmp/ex-cloud-plains-cpu-sep20/choice-7-final.bmp. GPU capture inspected for horizon,
clouds and retained ground. Only seven pixels differ between CPU and GPU,
by at most one channel value out of 255.
This adds one menu scene; remaining artwork, gameplay mapping and all-stage
palette/scroll acceptance are still open. No release or VR deployment.
