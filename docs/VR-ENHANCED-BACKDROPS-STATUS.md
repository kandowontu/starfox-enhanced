# Shared enhanced-backdrop migration — September 23

## Corneria green-hills selection — September 24

The shared landscape identity now selects resource 208's low green hills for
Original/EX `BG_1_1C` and Original Training instead of resource 200's white
alpine mountains. Native gameplay, ground height and photo projection are
unchanged. Live `LEVEL1_1:1000@enhanced-sky@front` stereo Vulkan checks pass
for both cartridges; their left-eye images in
`tmp/corneria-green-hills-2026-09-24/vr-{original,ex}-front` were inspected.
These 256-pixel checks do not replace Quest/Index headset inspection.

## Live EX 5-1/6-1/7-1 palette checkpoints

Native and Enhanced Sky stereo captures were compared before and after the
live gameplay weather/palette change for all three course variants. Actual
source tick counts (not just the checker argument) were 963/2000 for 5-1,
1017/2000 for 6-1 and 1000/2000 for 7-1. The capture checker adds a
stage-dependent entry warm-up; its logged `after N ticks` is authoritative.
Evidence: `tmp/vr-ex{51,61,71}-{593,1630,1576,1593}-{native,enhanced}-sep23`
(only the applicable pairs exist), all with corresponding passing GPU logs.
At the upper-sky sample (128,50), 7-1 changes native RGB 74,132,206 to
214,123,16 and enhanced RGB 62,126,199 to 201,118,10. 5-1 changes blue to
green in both; 6-1 increases blue brightness in both, though the enhanced
photo remains visibly less saturated than the cartridge's cobalt palette.
Ground alignment is visually retained. This establishes those two moments,
not every intermediate palette frame or headset colorimetry.

Follow-up at actual source ticks 1400/1600/1800 adds 5-1's pink→blue→green
and 6-1's gray-blue→orange→pink sequence (checked in each capture log).
Evidence: `tmp/vr-ex51-{1030,1230,1430}-enhanced-sep23` and
`tmp/vr-ex61-{976,1176,1376}-enhanced-sep23`; native comparison images at
tick 1600 are beside them. 6-1's orange photographic phase matches the
previously accepted desktop calibration capture in
`tmp/ex-palette-calibrated-gpu-sep20`. The softer cobalt endpoint is therefore
the shared realistic-art treatment, not a VR-only missing palette update.
No production color adjustment was made without stronger source evidence.

## EX 1-4 live cloud route and corridor source audit

The live EX 1-4 background is BG_1_14, not the preview's BG_1_4. Its
captured BG2 atlas has the same single blue cloud and separate green limb.
The VR enhanced-sky route now recognizes BG_1_14 and tracks both bodies
independently. A real LEVEL1_4 tick-1000 capture with `@enhanced-sky` shows
one of each against a full-surround starfield; assertions confirm two bodies,
motion interpolation, fade/cache behavior and native ground removal. Evidence:
`tmp/vr-ex114-enhanced-cloud-sep23` and its log. Windows PCVR and scene-check
builds pass. Quest release compilation and the 37-resource package check pass;
unsigned APK SHA-256 `3C246C9A6B384816518177A143717364C76BA265909FE16263812DA3AC38744B`.
The full local Windows VR build and all 18 CTest cases pass
(`tmp/vr-cloud114-{all-build,ctest}-sep23.log`).
The current Linux PCVR build and all 17 VR-labeled tests also pass. Its
llvmpipe stereo capture of this same scene (`tmp/vr-linux-ex114-sep23`)
differs from Windows by at most one RGB value in 829/65,536 left-eye pixels.
Physical-headset regression remains outstanding.

The EX LEVEL4_4 corridor's apparently striped center was compared with a
cartridge-authored BG2 reference in `tmp/vr-corridor-center-audit-sep23`.
Sampled center pixels match the source when both use the same native-width
tunnel aperture. The outer solid-color policy is deliberate; this is not a
claim of all-stage corridor acceptance.

## Abstract EX menu choices 23/24 — source classification

The static choice-map sweep leaves 23/24 outside the photographic selector.
Real EX pre-game captures at `tmp/vr-abstract-menu{23,24}-sep23` show that both
are Mode-2 abstract radial patterns (amber/magenta and green/blue), not a
missing landscape or planet. They currently fill the sphere via the native
pattern path, but visibly repeat in a grid. Do not map a landscape photograph
or duplicate a planet here; a source-faithful continuous abstract treatment
and headset visual approval are still open.

September 24: single-period sphere UV experiments passed packet checks but
produced an oversized, off-center field in actual stereo captures. The
experiment was reverted; the current player retains the prior native pattern
path. A future treatment must preserve the authored front composition as well
as avoiding repeats in the surround. No acceptance is claimed from the
rejected captures.

## Latest checkpoint: EX final-room surround variants

BG_6_6C/D/E (Mario/Luigi/course-7 abstract rooms) now use the same full-surround
native presentation as Andross's BG_3_7C. Previously these variants remained a
small front rectangle against a green border. The change only clears the
presentation tunnel mask; native gameplay state is untouched. Source palette,
ripple artwork and scroll remain native, including with Enhanced Sky selected.
Photographic final-room enhancement remains separate unfinished work.

Added a diagnostic `@continue-map=SYMBOL` route: first initialize a real level,
then continue at its authored internal final-map entry. This avoids the earlier
invalid standalone internal-fragment boot that left brightness zero. It is an
accelerated authored-script route, not a complete player-operated level clear.

Inspected before/after: `tmp/vr-final-route-{mario,luigi,andross}-sep23` and
`tmp/vr-final-surround-{mario,luigi,course7,rear}-sep23`. All three EX variants
reach native brightness 15 without `@expose`, and assert full-sphere/no tunnel
mask. The earlier Andross route now also supplies brightness-15 evidence,
superseding the need for its old exposure override as a rendering fixture.
`tmp/vr-final-surround-corridor-sep23` remains byte-identical to the previous
EX LEVEL4_4 corridor capture; this proves no regression from the room exception,
not full corridor visual correctness. Its striped appearance still warrants
source comparison. An attempted Original LEVEL4_4 reference failed because
that EX stage name does not exist in the Original symbol map; not acceptance.

Windows/Quest builds: `tmp/vr-final-surround-{build,quest}-sep23.log`.
Latest unsigned APK SHA-256:
`9C7A10CDDAC033F186B8B90BC2DF692F08126252399D32489CA68F819619BF1A`.
All 37 complete artwork payloads verified. No deployment/release; physical VR,
additional final backdrops and source-faithful corridor audit remain open.

## Latest checkpoint: snow moon, mesa and gameplay suns

The final two conservatively excluded landscape previews (4/5) now use their
enhanced snowy/desert skies. Preview 4 retains the tiny native moon/halo at
atlas (420,308), and preview 5 retains the mesa silhouette at (128,400).
Explicit ink masks remove their surrounding sky, not a rectangular cutout.
Gameplay BG_5_1/BG_6_1 retains the same moon 80 atlas rows higher; BG_5_5/BG_7_5
retains the original luminous sun over its enhanced golden storm.

Visual inspection exposed a separate eight-row native-mountain seam in the
snowy gameplay receiver. Its sky ends at row 351, verified from the decoded
source atlas; native and enhanced VR now start ground at row 352 (origin 240).
The final straight/banked captures have no retained original mountain strip.

Evidence: `tmp/vr-landmark-{menu4,menu5,ex51,ex61,ex55,ex75}-enhanced-sep23`
and corrected `tmp/vr-landmark-final-{ex51,ex61-bank,menu5-rear}-sep23`.
The earlier ex51/ex61 captures demonstrate the rejected seam; final captures
supersede them. Mesa rear has no duplicate landmark. Native atlas/ink audits:
`tmp/vr-{mesa,snow-moon}-ink-sep23`; gameplay references:
`tmp/vr-{snow-moon-ex51,gold-sun-ex55}-native-sep23`.
Scene assertions cover nonempty masks, count, native horizon boundary,
unchanged storage, display blackout and the existing GPU rendering checks.
Final builds: `tmp/vr-landmark-horizon-{build,quest}-sep23.log`.

Latest unsigned APK SHA-256:
`1751459CA20357A0CBFBB697BC56C26EE01F4B3A8B14065B778E365C1F8D32F6`.
All 37 embedded assets and catalogue tests pass. No release or device install.
This closes the identified unique landscape-preview gaps, not every remaining
space/final-room family, full palette cycle or physical-headset acceptance.

## Latest checkpoint: four unique EX menu previews

Enhanced Sky now also replaces previews 1 (day coast), 3 (storm), 10 (rocky
coast), and 12 (golden storm). Their conservative unique-atlas guard previously
kept the whole sky native. The replacement now retains one indexed coastal
island, both separately shaded rocky-coast moons, both live-palette storm moons,
and one indexed sun respectively. Menu 3's native moon rows are 96 pixels below
the gameplay atlas; menu 1's island is 80 pixels below its gameplay atlas.
These are explicit source-layout offsets, not inferred from a camera pose.

Inspected source atlases/native and enhanced images:
`tmp/vr-unique-menu{1,3,10,12}-{native,enhanced}-sep23`.
Rear captures `tmp/vr-unique-menu{3,12}-rear-sep23` confirm no repeated moon/sun.
The scene checker asserts nonempty retained landmarks, correct body counts,
unchanged geometry/image reuse, display blackout and storm palette changes.
Each run also passes the existing stereo GPU scene/effect/image checks.
Build logs: `tmp/vr-unique-previews-{build,quest}-sep23.log`.

Latest unsigned APK SHA-256:
`0556E975DE33B0EA9C72EF040370D4AEAFE17B5165A3C88FE6241CEE58DF8020`.
All 37 embedded artwork files verified; no release or device deployment.
Unique menu previews 4/5, remaining gameplay families/palette cycles and actual
headset acceptance remain open. Historical hashes/checkpoints below are retained.

## Latest checkpoint: EX Cygard storm and two moons

Gameplay BG_6_4 now uses the shared storm photograph in VR instead of falling
back to native sky. Both cratered moons retain the desktop/native atlas centers
(193,240), (226,271) and diameters 16/36; each appears once, follows the landscape
bank, and responds to its own live CGRAM highlights 74–79. No full-width bright
cloud band is retained. One immutable crater image is shared between the moons;
palette and display-brightness changes do not upload new image pixels.

Inspected native/enhanced/front/banked/rear captures:
`tmp/vr-storm-moons-{native,enhanced,bank,rear}-sep23/live-scene-left.bmp`.
The bank/rear logs also verify body count, shared storage, live palette changes,
unchanged-scene reuse and blackout. Rear view has storm/ground but no duplicate
moons. Windows build logs: `tmp/vr-storm-moons-{build,test-build}-sep23.log`;
Quest build: `tmp/vr-storm-moons-quest-sep23.log`.

Latest local unsigned APK SHA-256:
`52E9828A5C9EC8E730D215DDD2C5BE71BCE60DD30A7412B797F9141A78036311`.
All 37 complete assets verified; no deployment or physical-headset acceptance.
Native-only unique menu previews 1/3/4/5/10/12 and other unverified families
remain follow-up work; this checkpoint is not a claim of complete coverage.

## Latest checkpoint: public VR Enhanced Sky control

Enhanced Sky is now in both VR builds' 2D Options page, defaults off, persists
in version-5 preferences and works with the existing preview/runtime menu.
Version 1–4 imports keep it off; invalid records remain transactionally rejected.
The CLI switch now overrides only initial selection, not subsequent menu input.
Background submission tracks the selected option separately from source-frame
revision, so paused toggles cannot leave the old sky resident on screen.
Unsupported families continue using their native surround. This supersedes
earlier notes that a public Enhanced Sky control remains unimplemented, but
does not close the remaining family, palette-cycle or hardware acceptance work.

Input/preferences/legacy/press-only tests:
`tmp/vr-sky-menu-final-input-sep23.log`. Menu stereo captures:
`tmp/vr-sky-menu-{0,1,4}-sep23` (English/Japanese/Spanish), plus
`tmp/vr-sky-menu-final-spanish-sep23` after correcting the effect labels to
match shared localization keys. Inspected layouts show the Enhanced Sky row
and Back, without overlap. The final input suite checks translated effect labels
for all four non-English languages. Windows/Quest builds:
`tmp/vr-sky-menu-final-{build,quest}-sep23.log`; launcher help rebuild:
`tmp/vr-sky-menu-launcher-build-sep23.log`.

Latest unsigned APK hash:
`B5B684A10206DC47446E8B02B0BE7A6367BBA8F83F766A6DB40655F29105E889`.
All 37 embedded assets verified. No deployment/release; headset preview and
paused live toggling are wired but still require physical acceptance.

## Latest checkpoint: all-sky nebulae, smooth panoramas, final-room mask

Enhanced EX preview 29 / gameplay `BG_2_4` now use the sparse ember nebula;
preview 30 uses the separate red cloud band. Sector K and the ember nebula use
cached, seam-overlapped 2:1 sphere atlases rather than the horizon projection
that compressed their imagery into a strip. The stars/nebulae surround the
viewer. Asteroid belts deliberately retain their centered horizon projection.
Menu placement now uses the actual BG2 scroll register, not the unrelated host
camera scroll. Pattern scrolling interpolates through a draw-time matrix at
render rate, without texture rebuilds; wrapping and scene changes are guarded.

Passing inspected stereo captures and corresponding logs:

- `tmp/vr-pattern-final-29-sep23`, `tmp/vr-pattern-final-29rear-sep23`: nebula front/rear.
- `tmp/vr-pattern-final-2-sep23`: Sector K all-sky projection.
- `tmp/vr-pattern-final-24game-sep23`: natural EX LEVEL2_4, 409 source ticks.
- `tmp/vr-pattern-final-30-sep23`: red cloud band at its original menu height.
- `tmp/vr-pattern-photo-6-sep23`, `tmp/vr-pattern-photo-17-sep23`: distinct centered belts.

The Mode-2 `BG_3_7C` / `BG_1_6C` final room can retain the cartridge's tunnel
flag. VR presentation now clears that mask for this exact background identity
and supplies the native full sphere, without modifying gameplay state or
ordinary corridor classification. The native amber/magenta glows are retained:
the attempted photographic vortex was visually inferior and was removed.
Final-room photographic enhancement remains pending, not silently counted done.

Rendering evidence: `tmp/vr-vortex-room-exposed-native-sep23`. This is a targeted
`FINAL_TUNNEL:1200@from-entry@expose` diagnostic, **not** a natural full-stage
playthrough: entering the internal map directly leaves source display brightness
at zero, and the log explicitly records the diagnostic override to 15. The
unmodified 1200/1600-tick black captures failed the nonblank assertion and are
not accepted visual proof. Full natural tunnel-to-boss transition acceptance
remains open. `tmp/vr-vortex-room-corridor-verified-sep23` reaches actual BG105
from LEVEL4_4 and retains its enclosed center artwork. The earlier same-named
`corridor` capture with `@from-entry` was still BG291, not the corridor fixture.

All 34 photographic stereo GPU fixtures, live unchanged/fade storage checks,
wrapped-scroll interpolation and scene-transition guards pass. Unit checks also
cover pattern atlas dimensions/colours and interpolation endpoint/wrap behavior.
Final Windows/Quest and packet logs:
`tmp/vr-night-pattern-checkpoint-{build,quest,packets}-sep23.log`.
Catalogue identity tests, shader freshness and all-37-resource APK checks pass.
Latest unsigned APK SHA-256:
`919FB2AC4BAD314430DB4D77A3E3BAF6BCFF646393F8006AF226A1A2EB8BD5CD`.

No release, installation or physical headset acceptance. Public VR settings,
remaining native-only families (including abstract menu patterns/final-room
enhancement), full live palette-cycle coverage and hardware acceptance remain.

## Latest checkpoint: orbital surfaces and entry moon

Orbital backgrounds now use a full star surround, a horizontal atmospheric limb,
and a separately sampled stereographic lower surface. Dedicated ocean/cloud and
basalt/lava artwork replaces the stretched horizon-strip prototype. The square
surface is packed with the panorama in one immutable mipmapped atlas; the
projection has no longitude seam or repeated-tile fan at the nadir. Original
source horizon motion remains interpolated. EX's native tile-layer quarter-turn
is deliberately not applied to the already-horizontal photograph.

Entry scenes retain one banded moon using eight native CGRAM shades. Palette
and display fades change GPU parameters without rebuilding surface textures.
Default rendering does not load the new assets. Shared desktop/portable/VR
resource IDs 235/236 and catalogue tests cover the added files. Image-generation
mode, full prompts and paths: `ORBITAL-SURFACE-ARTWORK-SEPT23.md`.

Verified captures, with corresponding `.log` files and inspected left images:

- `tmp/vr-orbital-atlas-menu25-sep23`: ocean horizon, single moon, stars above.
- `tmp/vr-orbital-atlas-menu35-sep23`: downward volcanic surface, no stretched strip.
- `tmp/vr-orbital-atlas-ex51-sep23`, `ex61`, `ex71`: natural 60-tick EX scramble;
  horizontal lower planet, not the native tile layer's sideways orientation.
- `tmp/vr-orbital-atlas-original22-sep23`: natural Original 2-2 entry background.

Packet atlas validation, four catalogue/package-identity tests, all 34
photographic stereo GPU fixtures, unchanged-frame/fade storage tests, shader
freshness and Windows/Quest builds pass. Logs:
`tmp/vr-orbital-atlas-{build,quest,packets}-sep23.log`. APK verification finds all
37 complete artwork resources and no ROM/BIN/signing data. Local unsigned APK:
SHA-256 `5621ECA8C5FFC64EC39A64BDAC61A834DBEDA0798E031B83DBB7526931C0953F`.

No installation/release or physical headset acceptance. Thin boss phases, full
palette-cycle acceptance, remaining abstract/final-boss families and public VR
settings remain open. Earlier prototype cap captures are not the accepted art.

## Latest checkpoint: ocean islands and volcanic eruption

EX `BG_6_2` and `BG_6_6` now use their shared photographic coastal/cloud skies
without erasing the unique native island or eruption. Native atlas inspection
(`tmp/vr-landmark-{ocean,volcano}-sep23`) identifies island inks 58/59 and lava
inks 54..57. Small indexed tangent overlays retain only runs of those inks,
not rectangular tiles containing the old sky. The original sea/red ground
remain on the lower receiver, including the cartridge palette bands.

Landmark extraction supports 8/16-pixel tiles, both tile flips and source map
pages. Coverage depends on ink identity, not current brightness. GPU palette
changes therefore preserve geometry, and unchanged geometry/payloads retain
shared storage. Only enhanced frames perform this small cropped extraction;
default rendering does not load or classify these assets. Both native overlays
receive exactly the landscape/ground banking matrix in the application.

Passing live stereo captures (left images inspected):

- `tmp/vr-landmark-ocean-enhanced-sep23`: natural EX LEVEL6_2, 400 ticks.
- `tmp/vr-landmark-volcano-enhanced-sep23`: natural EX LEVEL6_6, 400 ticks.
- `tmp/vr-landmark-ocean-bank-sep23`: actual left input; aligned island/sky/sea.
- `tmp/vr-landmark-volcano-rear-sep23`: cloud/ground surround without a repeated
  bright eruption behind the player.

Unit checks cover selected versus rejected ink, transparent ink zero, 8/16-pixel
horizontal flips and palette-independent coverage (`tmp/vr-landmark-packets-sep23.log`).
All 31 photographic GPU fixtures and live display-fade/storage checks pass.
Windows/Quest logs: `tmp/vr-landmark-build-sep23.log` and
`tmp/vr-landmark-quest-sep23.log` (48 seconds). Final packet-test rebuild:
`tmp/vr-landmark-packet-build-sep23.log`. Shader freshness and all 35 APK
artwork resources verified. Current local unsigned APK SHA-256:
`DD0027015BD72722842F5014C6FEC089C035C5F113601F7E2E8B011D8BEF725A`.
No release/install or physical-headset acceptance. Orbital and other remaining
families, full palette-cycle acceptance and public settings remain open.

## Latest checkpoint: city skyline and six unique moons

EX preview 9 and gameplay `BG_5_2` now use the shared moonless night-city
panorama plus all six registered source moons. The skyline closes around the
viewer; the moons do not repeat. Their native atlas centres/radii are retained
relative to the city's 248-row landscape origin, and they share the landscape
motion matrix so banking cannot separate them from the skyline/ground.

One immutable cratered image supplies every moon. GPU palette modes retain
the three gray/purple shades (CGRAM 88/87/86) and two blue shades (82/83),
including display blackout without image rebuilds. The legacy native large-moon
patch is suppressed only when the photographic replacement exists; default
unenhanced rendering retains its previous path.

Verified live captures and matching logs:

- `tmp/vr-city-native-menu9-sep23`: original reference preview.
- `tmp/vr-city-menu9-sep23`: enhanced preview, all six body packets.
- `tmp/vr-city-gameplay-sep23`: natural EX `LEVEL5_2`, 400 ticks.
- `tmp/vr-city-rear-sep23`: continuous skyline, no duplicated moons behind.
- `tmp/vr-city-bank-sep23`: actual left input; camera rotation
  `(0,64136,64000)`, aligned skyline/moons/native ground.

Front/rear/banked left-eye images inspected. Live checks enforce six moons,
shared image identity, unchanged-frame reuse and display-fade behavior. All 31
photographic stereo GPU fixtures pass, including both city palette ramps at
three brightness samples. Packet tests: `tmp/vr-city-packets-sep23.log`.
Windows/Quest builds: `tmp/vr-city-build-sep23.log` and
`tmp/vr-city-quest-sep23.log` (54 seconds). Shader freshness and all 35 APK
artwork resources verified. Current unsigned APK SHA-256:
`45BDB88F335A6CF08C0CA5F9AF27539C77EDA12033C88491B3E0746E86C86B01`.
No installation/release or physical-headset acceptance. Orbital, ocean-island,
volcanic and remaining families/public settings still need work.

## Latest checkpoint: cloud and atmospheric rim

Original `BG_1_4` and EX preview 20 now preserve two independent subjects over
the full star surround: a photographic gas cloud (amber Original, blue EX) and
the narrow, shade-encoded native planetary rim. These are not ordinary round
planet replacements. Shared desktop atlas bounds and source tile silhouettes
are retained; rigid per-body transforms follow interpolated source scrolling.

The cropped cloud has a soft irregular alpha edge. Rim alpha comes from native
tile occupancy, not brightness: opaque dark interior remains opaque, while
empty surrounding space reveals the star field. Live CGRAM bank-5 shades are
mapped in the GPU shader, so palette changes never rebuild the atlas or mips.
The larger desktop atlas is only prepared when its source tiles/artwork change;
GPU uploads use two compact 192x192 and 320x128 mip images, not the full atlas.

Evidence: `tmp/vr-cloud-menu20-sep23`, `tmp/vr-cloud-menu20-rear-sep23` and
`tmp/vr-cloud-original-sep23`, with matching passing logs and inspected front
images. All 25 photographic stereo GPU fixtures pass, including three new
15-shade rim cases. `tmp/vr-cloud-final-menu20-sep23` adds an inspected close-up
of the actual rim with the final checker. `tmp/vr-cloud-packets-final-sep23.log`
checks transparent
space versus opaque black rim, palette-independent image identity and payload
validity. EX LEVEL1_4 uses BG_1_5 (135), not this cloud scene: its attempted
enhanced capture was correctly rejected and is not claimed as completed.

Windows build: `tmp/vr-cloud-build-sep23.log`. Quest:
`tmp/vr-cloud-quest-sep23.log`, 58 seconds. Shader freshness and all-35-master
APK checks pass. Current local unsigned APK SHA-256:
`4A6663244C8330BA80CEAFB6516ACDD5BDC015734112957EB5933DBF5B519893`.
No headset install/release or physical validation. The same-named EX gameplay
cloud registration still needs a natural scene capture. Orbital/city/other
remaining families and public VR settings remain open.

## Latest checkpoint: dimension face planets and shared GPU images

EX preview 18 and Original/EX dimension gameplay now surround the viewer with
the photographic star field while preserving the 16 individually registered
subjects: 13 enhanced face planets and three native saucers. Each face uses the
same immutable cropped image, with its own live 14-shade CGRAM ramp evaluated
in the fragment shader. No coloured 2048-square atlas is rebuilt on palette
changes. Rigid, interpolated body transforms preserve placement without shear.

The Vulkan packet renderer now shares actual GPU texture storage for identical
immutable CPU image handles, including across palette-only packet updates.
Mutable connected-grid buffers are excluded. Tests require one initial image
upload for three shared photographic packets and zero on a vertex-only update.

Verified live stereo captures and logs:

- `tmp/vr-face-menu18-sep23` and `tmp/vr-face-menu18-rear-sep23`: all subjects
  visible in front; only stars behind, without duplicated face planets.
- `tmp/vr-face-original-sep23` and `tmp/vr-face-ex-special-sep23`: natural
  `LEVEL_SPECIAL` gameplay after 400 source ticks, all 16 registered bodies.
- `tmp/vr-face-gameplay-sep23`: early `LEVEL6_3` is still its debris backdrop,
  not evidence for the later `BG_6_3H` face transition, which remains uncaptured.

Packet checks, shader freshness, all 22 photographic stereo GPU fixtures and
live palette/blackout/interpolation checks pass. Windows build:
`tmp/vr-face-planets-build-sep23.log`. Quest build:
`tmp/vr-face-planets-quest-sep23.log` (53 seconds). APK payload verification
passes, including all 35 complete shared artwork resources. Current unsigned
APK SHA-256:
`C3C0D06F6E051A576D4F4FDD4CE73D67D7377B532BC7CEB51483A279BD5DCE82`.
These are local offscreen GPU checks, not physical headset acceptance or a
public enhanced-sky setting. Cloud/orbital/city and other remaining families
still need migration; no release or headset installation was performed.

## Latest checkpoint: registered cratered, storm and banded planets

EX previews 19, 27, 28 and 31 now use the desktop `UniqueSkyObject` measurements
over a complete photographic star surround. The unique body is independent of
the repeating panorama. Master bounds are cropped before immutable alpha-aware
mip preparation; original RGB, rather than Fortuna's grayscale treatment, is
retained. Preview 19 shares the desktop's y=280 correction for its scroll-200
source layout (not the y=152 registration used by preview 28's different layout).

Gameplay `BG_3_2` is enabled in Original/EX. `BG_3_4B/D` have the same shared
cratered registration, gated by the native unique-space classification; their
natural approach/departure phases have not yet been captured. Other screens
cannot trigger the enhancement merely by retaining one of these background IDs.
EX gameplay palette response uses only the body's own source bank, not stars
or unrelated palette slots. A palette change retains its immutable image.

The body geometry is neutral and cached. Each presentation frame fits the
cartridge scroll/offset tables, interpolates that source transform, then removes
tile shear while preserving the body's centre. Only a rigid model transform is
updated: there is no head-facing sprite, repeated disk, scale change or per-frame
texture/vertex upload. Menus use the same authored signed-scroll and unique-wrap
rule as desktop. Discontinuous scenes snap rather than interpolating across maps.

Live Vulkan evidence (both eyes; matching logs pass):

- `tmp/vr-planet-menu{19,27,28,31}-sep23` — all four preview registrations.
- `tmp/vr-planet-storm-{original,ex}-sep23` — actual `LEVEL3_2`, 400 source ticks.
- `tmp/vr-planet-motion-menu19-sep23` — rear view contains only stars, not a copy
  of the planet; synthetic intermediate menu scroll passes scale/storage checks.
- `tmp/vr-planet-motion-storm-sep23` — diagnostic close-up of the actual source
  scene's storm planet, plus intermediate gameplay scroll/scale/storage checks.

The motion checks require a true intermediate position, not just a valid matrix,
and unchanged geometry/texture identity. Unit tests compare transformed neutral
quads against directly registered bodies, check coloured alpha/mips and reject
invalid master bounds. `tmp/vr-unique-planets-packet-final-sep23.log` passes.
All 19 photographic stereo GPU cases still pass. Final Windows build logs:
`tmp/vr-unique-planets-build-final-sep23.log` and
`tmp/vr-unique-planets-motion-build-final-sep23.log` (diagnostic assertions).
Quest: `tmp/vr-unique-planets-quest-final-sep23.log`, 41 seconds. Shader freshness
and all-35-master APK checks pass. Current unsigned APK SHA-256:
`112A84A521BFEDE146ACB86ED0B1DE2EB173B17BB515AA1156A4BA5C2D5593C7`.

No device update or release. Cloud-limb/face/city/orbital/volcanic/ocean/abstract
families, complete photographic panorama-scroll interpolation, public settings,
full palette-route acceptance and physical Quest/Index validation remain open.

## Earlier checkpoint: Fortuna and EX twin planets

Fortuna (`BG_3_3A` in Original/EX and EX menu 32) now uses the shared ocean/cloud
master plus one independently registered distant moon. EX `BG_5_4` uses its
rocky coast plus two moons with the native bright/dark palettes. Other unique
planet/cloud/city/orbital families remain native; this is not all-family completion.

The new bodies are single, distant world-space quads, registered in source-pixel
units before the same interpolated landscape matrix used by sky and ground.
They never repeat with panorama UVs and are not head-facing overlays or sheared
tilemaps. Their shared mipmapped texture is prepared only once per disk/fade
variant. Palette/display changes retain the pixel allocation; unchanged body
parameters retain vertex storage. Native upper sky artwork is removed when the
replacement is active, while native lower ground remains untouched.

Fortuna reuses desktop's cratered master bounds, top highlight lift and lower
atmospheric fade, with a 0.94 sky exposure. The brightest authored moon ink
provides live colour. Twin bodies use the exact desktop two-tone shading formula;
three new stereo GPU fixtures cover its dark, transition and bright portions.

The first close-up exposed a real integration defect: photographic sampler alpha
was correct, but `VulkanDrawPackets` still selected an opaque pipeline. A separate,
cached photographic alpha pipeline now composites fading edges against the actual
background. Native models/tiles retain their prior opaque pipeline. The added
translucent-red-over-blue GPU fixture verifies the final blend, not just sampled
RGB. All 19 photographic fixtures pass in both eyes; the earlier lone transparent
sample now expects its correctly blended result over black.

Final focused evidence (actual source scenes; diagnostic narrow FOV and optional
model/HUD omission only, no game-state patch):

- `tmp/vr-moons-fortuna-blend-sep23/live-scene-{left,right}.bmp` — smooth lower fade,
  not the initial hard-cut result in the superseded `fortuna-focus` folder.
- `tmp/vr-moons-twins-blend-sep23/live-scene-{left,right}.bmp` — both separate bodies.
- `tmp/vr-moons-fortuna-rear-sep23/live-scene-left.bmp` — continuous sky, no duplicate moon.
- `tmp/vr-moons-fortuna-bank-sep23/live-scene-left.bmp` — controller-driven bank,
  shared moon/sky/ground motion, no tile shear applied to the disk.
- `tmp/vr-moons-ex-fortuna-final-sep23` — full EX gameplay, both eyes.
- `tmp/vr-moons-ex-menu32-sep23` — live menu registration and retained native ground;
  captured before the photographic alpha pipeline correction above.

Matching logs pass. Cache reuse/display blackout are checked in live captures;
unit tests cover matte corners, opaque disk, atmospheric fade, circular geometry,
bad-size rejection and retained indexed-ground behavior. Build logs:
`tmp/vr-moons-blend-build-sep23.log`, `tmp/vr-moons-focus-build-sep23.log`
(diagnostic focus helper), `tmp/vr-moons-blend-packet-sep23.log` and
`tmp/vr-moons-blend-quest-sep23.log` (Quest: 1m 3s). Shader freshness passes.
APK verification confirms all 35 complete masters and no ROM/BIN/signing payload.
Current unsigned APK SHA-256:
`236BAA100DCB26969EE6478651648F7C244643BA01907BE104B76ECF48677559`.
No install or release. Physical Quest/Index stability/comfort remains unverified;
the development `--enhanced-sky` opt-in is still not a public menu setting.

## Earlier checkpoint: Sector K and game-over surrounds

The development opt-in now includes EX Sector K (`BG_5_3`, menu background 2)
and the game-over scene. This remains a partial migration, not a public VR menu
option or physical headset acceptance.

Sector K's photographic nebula uses the two live seven-colour BGR555 shade ramps
from source palette banks 5/6. The GPU warm/cool blend follows the desktop
`backdrop_ramp_colour` mapping and leaves neutral stars unchanged. Menu previews
use authored colour rather than stale gameplay palettes. Existing immutable
texture storage is retained across shade updates. Actual Vulkan evidence:
`tmp/vr-nebula-live-sep23.log`, `tmp/vr-final-nebula-menu2-sep23.log`, and their
matching capture folders. Warm/cool/neutral GPU fixtures 12–14 pass in both eyes.
A complete natural in-level palette-cycle trace is still required.

Native game-over now continues its source star-only atlas rows around the whole
viewer, without repeating Andross. BG2 also follows the source HALFFADE subtract
independently of BG1/GAME OVER lettering. The pre-fix rear scene was blank;
`tmp/vr-gameover-native-{front,rear}-sep23.log` and
`tmp/vr-final-ex-native-rear-sep23.log` now pass live scene coverage.

The enhanced scene prepares the wide star master into a 2:1 panorama by tiling
rows with a short overlap, not stretching the source stars. Longitude wraps once
and vertical UV follows latitude, avoiding the initial horizon band and elongated
star failures. The native foreground is then drawn in front. A cached flood-fill
cutout removes exterior black ink while retaining enclosed black facial details;
source colours/fading still come from the indexed GPU layer. Default/non-enhanced
rendering does not build this cutout. Unit tests explicitly cover opaque black
holes, sRGB flags and source immutability.

Final live enhanced captures:
`tmp/vr-gameover-cutout-front-sep23/live-scene-{left,right}.bmp` and
`tmp/vr-gameover-cutout-rear-sep23/live-scene-{left,right}.bmp` (matching logs pass).
Comparison against `tmp/vr-gameover-front-before-sep23` preserves every nonblack
foreground pixel: 4,190 left and 4,184 right, zero colour/position differences.
The front capture has no opaque source-window rectangle. Earlier `photo`,
`sphere`, and `layered` captures are intermediate diagnostics, not final proof.

Windows build: `tmp/vr-gameover-cutout-build-final-sep23.log`; packet tests:
`tmp/vr-gameover-cutout-packet-sep23.log`. All 15 photographic GPU cases pass in
both eyes in the final live capture. Shader freshness passes. Shader publication
now uses atomic replacement with bounded Windows sharing-error retries, avoiding
the intermittent EINVAL direct-header-write failures encountered during this pass.

Quest release build: `tmp/vr-gameover-cutout-quest-sep23.log` (53 seconds).
Package validation confirms all 35 complete shared artwork resources and no
ROM/BIN/signing files or flat-app entry. Current unsigned APK SHA-256:
`E2833F9F05DF6A9740D91D736DDACB3385A7890BA508647047FD294656E267C7`.
No device installation, signing or publication was performed. Unique celestial,
orbital/volcanic/ocean/abstract families, new photographic scroll interpolation,
the public settings path, full palette-cycle acceptance and physical Quest/Index
validation remain open.

## Earlier checkpoint: live landscapes (development opt-in)

`starfox_pcvr --enhanced-sky` now selects shared photographic artwork in
registered Original/EX landscape and asteroid scenes. The default is unchanged. The same
application path compiles into Quest, but no public menu toggle or new device
install has been made. The older checkpoints below describe incremental history.

`vr/enhanced_landscape.hpp` owns lazy resource selection, one resident mip pyramid,
palette-region caching and shared draw geometry. An unchanged scene retains both
pixel and vertex storage. Palette/display changes retain the pixel allocation;
head/camera movement only changes the draw matrix. The native upper sky triangles
are removed when replaced; native lower geometry and payload remain intact.
Both sky and ground use `landscape_camera_motion`, including bank interpolation.

EX gameplay uses the shared desktop upload-palette response and artwork-specific
reference calibration. EX native-menu previews deliberately do not: their
`VRAM3ADDR` can refer to unrelated gameplay data. Actual BG 26 captures exposed
cyan overexposure from that stale reference; matching desktop's authored preview
exposure fixes it. Shader sampling supports signed RGB shifts as well as gain,
display brightness and the existing linear/sRGB selection.

This is **not all-family support**. Titania's in-level weather now uses the
per-shade GPU ramp described below. Unique-half/city/twin/face
planets, ocean-island, volcanic and orbital scenes also remain native. Those
families must preserve single celestial objects and native horizon placement
when migrated. Full palette sequences, per-scene visual acceptance, the public
setting, and physical Quest/Index performance/comfort are still required.

Evidence (all September 23):

- Windows PCVR/scene build: `tmp/vr-live-enhanced-final-build-sep23.log`.
- Packet tests: `tmp/vr-live-enhanced-packet-sep23.log`; signed shifts, immutable
  source preservation and cached ground-only geometry pass.
- Real Vulkan Original/EX banked Corneria captures:
  `tmp/vr-live-{original-bank,ex-bank}-sep23/live-scene-left.bmp` and matching logs.
- Corrected menu previews: `tmp/vr-live-ex-{menu26-final,menu36-final}-sep23/`.
- Rear coverage: `tmp/vr-live-ex-menu13-rear-sep23/`.
- Actual EX 7-1 at 1000 ticks: `tmp/vr-live-ex-7-1-sep23/` (one phase, not a full fade).
- Live checks assert unchanged storage reuse, display blackout without image
  reload, excluded orbital scenes, matching sky/ground matrices, and identity
  preview palette. GPU colour fixture checks gain plus signed RGB offsets in
  both eyes. Existing native Vulkan checks also run in those captures.
- Quest: `tmp/vr-live-enhanced-quest-final-sep23.log` and
  `tmp/vr-live-enhanced-quest-payload-sep23.log`; all 35 complete artworks present.

That checkpoint's unsigned APK SHA-256 was
`D58E69BB3B462F9CD4D1A0F2787311380762886E8CF95F9D1B7A9EB5A260EBDB`.
No installation or release publication. The older `build/pcvr-package-sep22`
directory has not been refreshed; current PCVR binary is in `build/vr-dev`.

## Titania weather integration checkpoint

Original Titania's live BGR555 cloud shades now travel with the draw vertices,
independently of the immutable mip pyramid. The fragment shader interpolates
the same 15-shade, 140..245 exposure mapping as desktop. It supports the merged
fog shades and their separation into gold/dark red after the weather changer;
brightness and optional sRGB conversion are applied afterward. EX gameplay uses
its own palette response, not Original's ramp. EX menu 26 now also matches the
desktop 0.85 preview exposure, preserving cloud/text contrast.

The natural Original LEVEL2_3 replay uses controller input only: Right for 20
source ticks at tick 2600, then Down for one. No object, map, weather flag or
palette is overwritten. Captures assert the original endpoint inks:

- Fog (tick 2600): CGRAM 14 = (21,25,30), 25 = (21,25,31).
- Clear (tick 3700): CGRAM 1 = (29,25,15), 14 = (4,0,0), 25 = (11,8,6).

Both actual Vulkan captures pass, including native model/ground rendering and
all existing scene fixtures. Images were inspected at
`tmp/vr-weather-{fog,clear}-sep23/live-scene-left.bmp`; matching logs record the
endpoint words. These two endpoints do not substitute for frame-by-frame fade
or physical headset acceptance.

Two additional stereo GPU fixtures compare the fog/clear shade lookup against
`backdrop_ramp_colour` and prove a palette-only change uploads zero texture
buffers. Invalid shade words are rejected transactionally. Packet tests check
palette transport and retain their existing projection/cache checks.

Evidence: `tmp/vr-weather-build-sep23.log`, `tmp/vr-weather-check-build-sep23.log`,
`tmp/vr-weather-packet-sep23.log`, `tmp/vr-weather-{fog,clear}-sep23.log`,
`tmp/vr-weather-ex-menu26-sep23.log`, `tmp/vr-weather-quest-sep23.log` and
`tmp/vr-weather-quest-payload-sep23.log`. Quest compiles and contains all 35
complete masters; no installation or publication.

Weather-checkpoint unsigned APK: `platform/quest/build/outputs/apk/release/quest-release-unsigned.apk`.
SHA-256: `C6A2B009DA65790C3521CC9698AAD3272137A07406733815C5C3CF2E1F6114DC`.

## Implemented foundation

### Full-sphere asteroid/debris checkpoint

The development opt-in now also renders Original/EX `BG_1_2`, EX `BG_6_3`,
and EX menu choices 6/17 using their distinct photographic debris/asteroid
masters. Their native backdrop packet is removed entirely; models, live dust,
comms and UI remain separate. The original scroll-based belt placement is
preserved (source origins 352/384 in gameplay and 400/352 in the previews).
The mesh covers both hemispheres instead of leaving a native or clear lower half.

Panorama preparation blends both poles once and retains uniform terminal rows
through the mip chain. The shader samples the original prepared pole row when
clamped there: the final 1x1 mip cannot represent distinct top/bottom colours.
Pole flags are explicit, so this must not change ordinary minified flat textures
or flatten a landscape's horizon row. An initial GPU fixture caught that overly
broad condition; the final scoped flags pass the existing sampler fixtures.

Windows build and packet tests pass:
`tmp/vr-full-sky-build-final-sep23.log`,
`tmp/vr-full-sky-packet-final-sep23.log`.
The tests cover symmetric upper/lower vertex coverage and nadir mip rows.
Real Vulkan captures in both eyes pass for:

- `tmp/vr-full-sky-original-sep23/` — Original LEVEL1_2, tick 400.
- `tmp/vr-full-sky-ex-dense-sep23/` — EX LEVEL6_3, tick 400.
- `tmp/vr-full-sky-menu6-sep23/` — native menu dense-belt preview.
- `tmp/vr-full-sky-menu17-rear-sep23/` — rear view of the fine-debris preview.

Matching `.log` files record the checks. Front and rear images were inspected.
The diagnostic asserts the native background geometry is empty after replacement;
no unique planet, orbital horizon, tunnel, or unregistered family is routed
through this repeatable sphere. Shader freshness and whitespace checks pass.

This remains partial migration: special nebula palettes, unique celestial
subjects, orbital surfaces, game-over integration, scrolling interpolation,
the public menu setting and physical headset validation remain open.

Quest compilation and full package verification pass for this checkpoint:
`tmp/vr-full-sky-quest-final-sep23.log` (2m46s) and
`tmp/vr-full-sky-quest-payload-sep23.log` (all 35 complete masters).
Current unsigned APK: `platform/quest/build/outputs/apk/release/quest-release-unsigned.apk`.
SHA-256: `D63BE55B90AE247504C1008B0B33AE823D334A9C9CA0AF6D38A736B4552CB57D`.
PCVR help now describes the broader opt-in accurately; final host build:
`tmp/vr-full-sky-host-final-sep23.log`. No install or release publication.

The 35-artwork catalogue, resource identities, lazy decode/cache and zenith
preparation now live in `render/enhanced_backdrop_library.hpp`, not inside the
desktop game loop. The desktop application uses this shared loader. It accepts
a platform byte-provider callback, so OpenXR does not need to depend on SDL
window/application code or duplicate asset numbering.

Images are sealed only after successful decode/preparation. Failed loads remain
retryable; repeated access retains image addresses and immutable GPU upload keys.
Only the same three pre-existing masters receive zenith preparation. No image
content or default settings were intentionally changed by this extraction.

Tests cover resource/path identity, lazy reuse, stable image identity, preparation,
invalid indices and recovery after malformed bytes. A separate catalogue check
matches all 35 entries against Windows RC and portable CMake resource mappings,
including missing/duplicate assets. Windows and Linux builds and the tests pass. The EX 20
32:9 enhanced final capture is byte-identical to its pre-refactor counterpart:
`tmp/shared-backdrop-library-capture-sep23/choice-20-final.bmp`.

## Still required for VR — not claimed complete

The current Quest/PCVR application still uses cartridge background packets.
This extraction alone does **not** add enhanced skyboxes or a visible VR option.
Remaining integration must include:

- Resident immutable texture storage separate from changing source palette,
  scrolling and camera state. Do not allocate/upload photographic artwork on
  every source frame or reinitialize a graphics pipeline during head motion.
- Sphere/ground/horizon ownership and single-occurrence celestial objects,
  using the authored offsets and palette behavior established on desktop.
- A shared option/preferences path, correct menu previews, unsupported-mode
  handling, and both-eye rendering checks before hardware acceptance.
- Physical Quest and Index image stability, alignment, performance and comfort.

The VR packet uploader now supports immutable shared texel payloads alongside
the existing owned representation. Copies retain pixel-storage identity;
geometry comparisons and texture-only reuse take the pointer fast path when
the same immutable storage is retained. Validation and uploads consume the
same payload view, including source-scene placeholder validation and ground
receiver inspection. Populating both representations is rejected. Existing
native packet producers continue using their original owned payloads.

Windows `starfox_vr_packet_check` and the real Vulkan scene/readback check pass.
The new GPU fixture verifies transform-only reuse, vertex-only changes without
pixel uploads, independent replacement of immutable pixels, and rejection of
ambiguous/truncated payloads without replacing the live scene. Evidence:
`tmp/vr-shared-texture-build-sep23.log`,
`tmp/vr-shared-texture-scene-sep23.log`. These are desktop GPU checks, not a
headset test. The subsequent Quest build below includes the shared-storage changes.

The photographic sampler added below accepts brightness/tint in draw vertices
without altering the immutable image. Source palette/profile selection is still
not integrated. Large photographic data must not be appended to every mutable
per-frame source packet.

No headset install, release or claim of desktop/VR enhanced-art parity was made.

## Asset delivery checkpoint

Quest and PCVR now embed the same 35 authored BMP resources as desktop. CMake
derives IDs 200–234 directly from the shared catalogue; there is no additional
hand-maintained VR list. The new mapping test executes that CMake function and
compares all IDs/paths with desktop and portable mappings. Missing assets fail
configuration. This adds artwork, not cartridge dumps or a generated BIN.

The Windows PCVR executable builds, `--help` passes, and the new resource check
decodes/prepares/seals all 35 embedded images one at a time. A byte-content check
also verifies every complete source BMP occurs in the actual PCVR executable.
See `tmp/vr-backdrop-delivery-pc-sep23.log` and
`tmp/vr-backdrop-delivery-checks-sep23.log`. This does not update the older
`build/pcvr-package-sep22` folder or establish Linux PCVR acceptance for this change.

Quest release compilation passes. The APK content checker verifies the complete
35 BMPs in `libstarfox_quest.so`, required arm64 libraries, and absence of forbidden
ROM/BIN/key entries, desktop runtime and Android link stubs. The GitHub Quest
job now requires that artwork check. Checker tests also reject missing/truncated
artwork. Evidence: `tmp/vr-backdrop-delivery-quest-retry-sep23.log` and
`tmp/vr-backdrop-delivery-quest-payload-sep23.log`.

Local unsigned APK: `platform/quest/build/outputs/apk/release/quest-release-unsigned.apk`.
Checkpoint SHA-256 (superseded by the sampler build below):
`64BADDD7FCD0A2C3C975DBD302D516CF50845D21E583F59F287E0F3AA49344F8`.
Not installed, signed for distribution, uploaded or released. Packaging artwork
does not itself render it; projection/palette integration and the VR menu option
remain unimplemented.

## Filtered artwork sampler checkpoint

Added a distinct VR photographic texture type. Native game sprites/models keep
their original sampling path. Artwork uses perspective-correct UVs, bilinear
sampling and derivative-selected trilinear mip filtering. Horizontal wrap is
explicit; vertical edges clamp. The pyramid uses premultiplied alpha to avoid
coloured fringes, with straight colour restored before the draw's brightness/
tint and optional sRGB conversion. Odd-size images retain their terminal rows
and columns during mip construction.

Validation requires immutable storage, matching finite geometry/texture controls,
the complete contiguous mip chain and bounded dimensions. Native packets retain
their 4,000,000-word budget; photographic packets collectively have a separate
8,000,000-word limit (about 32 MB), accommodating a 2048-square pyramid without
removing upload limits. Mixed types, overflowed extents and ambiguous storage are
rejected before replacing the live scene.

Windows packet tests pass, and all 35 embedded masters decode into valid mip
pyramids. Real Vulkan readback passes seven fixtures in both eyes: interpolated
colour, negative-coordinate wrap, clamped edges, minification of a checkerboard,
half brightness, sRGB conversion and transparent-edge colour. Existing native
scene checks still pass. Evidence: `tmp/vr-photo-packet-tests-sep23.log`,
`tmp/vr-photo-resource-tests-sep23.log`, `tmp/vr-photo-sampler-scene-sep23.log`.
Generated SPIR-V freshness check passes.

This verifies the sampler, not live skybox projection, authored palette response,
or physical head-motion stability. No gameplay option is exposed yet.

Quest release compilation and the complete APK artwork/payload check also pass
for this sampler change (`tmp/vr-photo-sampler-quest-sep23.log`,
`tmp/vr-photo-sampler-quest-payload-sep23.log`). Current unsigned APK SHA-256:
`AAA9C4D7DC4DEFBD7393805A138137C8900D8AF02F5FFFF07B5F6C78EC568B5A`.
No device install or publication. The older PCVR executable/package must be
relinked when integrating the scene path; these Windows checks used the new
diagnostic executables.

## Landscape projection checkpoint

Added an upper-hemisphere photographic landscape packet. It does not paint the
ground hemisphere. Its forward scale follows the existing source-pixel mapping;
the panorama closes behind the viewer with an integral repeat count and a smooth
rear-only scale adjustment. Horizon, vertical scale, horizontal offset and colour
response are explicit parameters. The application must apply the same landscape
motion matrix to this packet and the native ground. That live integration is
still pending; unique planets must use their own projection rather than this
repeatable landscape function.

Panorama preparation bakes the same short overlap used by desktop sampling and
prepares a uniform zenith. Every mip retains that same zenith radiance: early
captures exposed longitude-dependent upper-sky colour from averaging the next
rows into the coarser mip, which is now fixed. Unit tests check odd dimensions,
all mip-level zenith rows, retained interior samples, closed longitude range,
the horizon boundary, invalid projection controls and absence of ground vertices.

Real Vulkan captures of `alpine-day-v2.bmp` pass front, side and rear views in
both eyes. The diagnostic checks full upper-view coverage, no pixels below the
horizon and stable zenith colour across eyes/view directions/LOD. Final evidence:
`tmp/vr-photo-projection-packet-final-sep23.log`,
`tmp/vr-photo-projection-final-capture-sep23.log`, and six images under
`tmp/vr-photo-projection-sep23/photo-landscape-{0,1,2}-eye-{0,1}.bmp`.
The initial capture-only run incorrectly reached the unrelated triangle-disparity
assertion; the diagnostic now distinguishes the selected capture suite explicitly.

These are isolated renderer captures, not a live stage, complete palette fades,
banked-ground comparison or headset acceptance. No game menu option was enabled.
The current Quest APK remains the earlier sampler checkpoint, not this new
projection helper; rebuild it with the live integration batch.
