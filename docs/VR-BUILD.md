# VR development status

## Current native Linux PCVR regression (September 23)

Rebuilt the current shared worktree in the existing Ubuntu x86-64 OpenXR/Vulkan
Release configuration at `/home/kando/starfox-enhanced-0052-check`, including
the Linux `starfox_pcvr` player, scene checker and full desktop/VR target set.
All 17 VR-labeled CTests pass. The player starts its `--help` path and `ldd`
reports no missing linked libraries on this host.

The real EX LEVEL1_4 tick-1000 Enhanced Sky stereo scene passes the Linux
llvmpipe Vulkan readback suite with exactly two cloud/limb subjects and no
native duplicate. Its left-eye image is visually the same as the Windows GPU
capture; 829 of 65,536 pixels differ by at most one RGB value. Evidence:
`tmp/vr-linux-ex114-sep23` and `tmp/vr-ex114-enhanced-cloud-sep23`.
Software Vulkan compilation was slow; this is functional/parity evidence,
not Linux hardware FPS, Index tracking or headset acceptance.

## Windows PCVR tester handoff (September 23)

Updated tester snapshot:
`build/Starfox-Enhanced-PCVR-Windows-x64-Tester-2026-09-23-r2.zip`.
It adds the EX 1-4 live cloud/limb sky route. SHA-256:
`7A5621BE3A5716666D224C8862C882CB84C14BBBC69DC310B51311234CE24535`.
The ZIP executable hash matches staging; `--help` and the live EX 1-4
stereo/sky diagnostic pass. The original archive remains unchanged.

User-requested shareable archive:
`build/Starfox-Enhanced-PCVR-Windows-x64-Tester-2026-09-23.zip`.
Staged via the PCVR install component with symbols stripped; includes the
asset builder, setup notes, a logging launcher and drag-and-drop BIN builder.
No ROM/BIN/music, settings, saves, keys or shader caches are included.
Archive executable hash matches staging; both executables import only Windows
system/UCRT libraries. Player help, missing-BIN and invalid-option checks pass.
This is the current development snapshot through the full-surround EX room
variants, not a release tag or physical SteamVR/Index compatibility sign-off.
Tester should supply their own compatible BIN and use their active OpenXR
runtime; START-HERE.txt describes setup and remaining limitations.

## Enhanced Sky menu control (September 23)

Quest and PCVR now expose **Options → 2D Options → Enhanced Sky: Off/On**.
It defaults to Off, works with Preview, and is also available in the runtime
menu (Menu + Select). Closing the menu persists it in `vr-preferences.bin`.
Version-5 preferences retain the existing 20-byte size and migrate versions
1–4 with Enhanced Sky off. `--enhanced-sky` remains an explicit launch override;
the user can subsequently switch it off from the menu.

Changing this setting invalidates background submission even at an unchanged
paused source revision. Migrated families use the shared photographic assets;
unmigrated families retain their original surround rather than losing artwork.
See `VR-ENHANCED-BACKDROPS-STATUS.md` for coverage and limitations. No hardware
ray-tracing capability is required for this option; unsupported headsets still
hide the separate Ray Tracing row.

Current local unsigned APK SHA-256:
`9C7A10CDDAC033F186B8B90BC2DF692F08126252399D32489CA68F819619BF1A`.
This also includes the EX Mario/Luigi final-room surround variants, in addition
to the unique landscape previews and corrected snowy ground boundary; build/capture
evidence is at the top of `VR-ENHANCED-BACKDROPS-STATUS.md`.
Windows/Quest builds, preference/input tests, English/Japanese/Spanish stereo
menu captures and all-37-asset package checks pass. This is not a signed release
or physical headset acceptance. Older artifact hashes below are historical.

## Release/package build follow-up (2026-09-22)

The local Quest release variant now builds successfully (3m 11s). Its previous
failure was Ninja's 260-character Windows filename limit in Vulkan-Headers'
FetchContent stamp. Quest's Gradle staging now uses ignored `build/q` rather
than `platform/quest/.cxx`; `STARFOX_QUEST_BUILD_ROOT` can point to a still
shorter directory for longer checkouts. No SDK policy or registry change was
needed, and existing build directories were preserved.

Validated artifact: `platform/quest/build/outputs/apk/release/quest-release-unsigned.apk`,
10,509,673 bytes; SHA-256
`EE6D158AF3853C6ADF2996C04E778FFEAE3704E2AFC05E4532A74C8F85EF228A`.
Payload checker and aapt confirm arm64 VR/SDL/C++ libraries, QuestActivity,
package `com.starfox.enhanced.quest`, API 29/35, version 12 / 0.0.6.7-vr-dev,
and no ROM, BIN, signing keys, flat libmain or platform-library stubs.
This local release variant is **unsigned**, not an installable signed release.
The GitHub Quest job already signs with the permanent Android secret and
checks its certificate; that remote workflow has not been run in this pass.

The Windows PCVR player and application tests rebuild. The clean install
component at `build/pcvr-package-sep22` includes instructions and licenses,
not user game data. Its --help, missing-BIN and invalid-option paths pass;
objdump shows only Windows system/UCRT imports, not unpackaged SDL/OpenXR or
MinGW DLLs. Application lifecycle/save tests pass without starting a headset.
Executable SHA-256:
`571D2D91B7F811FC7F4A3ABD08B3A197306D880FC4E6DA19EEFDB54E29D16774`.

Native Linux also rebuilds the current PCVR player and passes its application
tests and --help smoke check. The current Linux Lavapipe background checker
passes the outlined-menu glyph/clip fixtures and complete tile parity sweep.

The standalone PCVR host keeps its own vr-data saves/preferences/shader cache
and runs until exit, not the diagnostic 120-frame limit. Physical Quest/Index
comfort, rendering and performance remain unverified here. No installation,
release publication, or claim that desktop photographic effects now exist
in the VR rendering path is made.

Build dependency improvement: dr_libs now skips its unused miniaudio test
submodule. Only its pinned standalone decoder headers are consumed.

## Full current desktop/OpenXR checks (2026-09-19)

All configured Windows VR and native Linux desktop/OpenXR targets rebuild
successfully after the grid-compute/reuse changes. The complete 16-test VR
suite passes on Windows (21.97s) and Linux (22.58s), including both cartridge
input checks, packet/cache tests and the embedded shader freshness test.
This supplements the actual Vulkan rendering comparisons below; CTest alone
does not demonstrate physical Index/Quest tracking, comfort or frame rate.
Fresh ADB enumeration lists no devices, so no installation was attempted.

## Retained grid arena build (2026-09-19)

Connected-grid camera changes now reuse their GPU output arena and drawing
descriptor. Windows dispatched update/failure-recovery and image comparisons
pass; see GPU-MIGRATION-STATUS for setup timings and scope. Quest arm64 package
verification passes (22 s), APK 17,481,136 bytes, SHA-256
`1977002EF2C0E7221014D7142C0F5F5B2E90C118BDBAD986542B43CC640CB52D`.
Not installed; physical-headset performance remains unverified.

## Connected-grid compute build (2026-09-19)

The source connected-line grid now projects and bins on Vulkan compute before
graphics. Flat/rotated Windows eye images and raw row primitives match the CPU
reference; a live EX connected-grid scene passes. See GPU-MIGRATION-STATUS for
scope and remaining performance/device checks. Quest arm64 rebuild/package
checks pass (30 s), APK 17,479,256 bytes, SHA-256
`3BD0CDAC8620A90A5F1E63946B75BE2D6F28A662E8C7CE69FCF17C4EFF68FA38`.
Native Linux software-Vulkan rotated row data and both eye images also match
the reference. No device installation or physical-headset acceptance in this
pass; fresh ADB enumeration is empty.

## Embedded shader freshness gate (2026-09-19)

Every VR configuration now validates both the main graphics shader and ray
expansion SPIR-V before building. Changes to HLSL, shared helpers, included
planet-region data or the generated headers trigger CMake reconfiguration and
the same validation during incremental builds. Previously only ray expansion
was guarded here, so the main scene shader could silently remain stale.

Both generators hash their local include graphs. Developers changing shader
source must regenerate with `tools/generate_vr_shaders.py --dxc <dxc>` and/or
`tools/generate_vr_ray_shader.py --dxc <dxc>`; normal builds need Python but no
shader compiler. The `starfox_vr_shader_freshness` CTest uses disposable projects
to prove stale-source/include/header rejection and recovery without modifying
the real checkout. Direct Windows and Linux executions pass all five cases.

## Native Linux/OpenXR validation (2026-09-19)

The existing native Linux Release build at
`/home/kando/starfox-enhanced-0052-check` now enables `STARFOX_BUILD_VR=ON`.
The full build passes after qualifying a test button name that collided with
POSIX `select()`. All 15 Linux VR CTests pass (19.49 s), including Original/EX
cartridge checks and exact Touch/Index binding validation. The native x86-64
ELF runtime has no missing linked libraries in this environment.

Linux llvmpipe Vulkan particle and scaled-text checks and their independent
CPU references all pass. Both eye BMPs match byte-for-byte for each pair in
`tmp/vr-linux-{particles,particles-reference,text,text-reference}-sep19`.
These replace syntax-only evidence, not physical SteamVR/Index acceptance or
a portable-distribution ABI/performance sign-off. No headset was used.

## Preview and reset controls (2026-09-13)

Added session-only PREVIEW: OFF/ON to the main, 2D and 3D setup pages.
Startup uses a separate silent Corneria checkpoint, with no save writes or
intro progression; runtime options preview the frozen current scene. Effects
apply live while menu text stays unfiltered. Original title models are placed
twice as far away; EX showcase/title placement is unchanged.

Hold both triggers with stick clicks released, then click both thumbsticks to
reset to startup. The chord does not repeat while held or fire on focus regain.
Saved data is retained; current unsaved gameplay is restarted.
Quest APK built and installed successfully; physical preview/reset acceptance
remains to be checked. Original/EX cartridge regressions and input tests pass.

## Cartridge-selection regression and Quest refresh (2026-09-11)

Added a cartridge-backed Titania test that deliberately switches the menu
experience choice while retaining the loaded ROM. Landscape classification
must remain based on cartridge identity. Original and EX tests both pass
(11.57 seconds total). Quest ARM64 debug APK rebuild passes in 30 seconds with
the spatial water shader and runtime height interpolation included. The APK
was not installed/launched; physical headset acceptance remains outstanding.

## EX water and cartridge identity (2026-09-11)

The standalone capture now sets/logs experience from actual cartridge metadata,
not the default menu choice. Original-only landscape classification likewise
uses cartridge identity. EX Titania water reaches background 207, Mode 1; its
BG2 atlas and BG3 native diagnostic BMP hashes exactly match Original's fixture.
Enabled the shared spatial water mapping for EX on that evidence. Inspected
`tmp/vr-ex-titania-spatial/live-scene-left.bmp`: spatial water/bridge and EX ship
HUD render together. Build and host Vulkan diagnostic pass. This is one EX
water timestamp, not all EX backgrounds or physical headset acceptance.

## Water height interpolation (2026-09-11)

Runtime water receiver transforms now interpolate camera-to-plane height across
source ticks, retaining the current packet's geometry and updating only model
transforms. Scene changes/camera cuts snap rather than blend unrelated states;
the distant backdrop is not scaled. Packet tests check all 241 interpolation
steps plus clamped endpoints and unchanged horizontal/translation components.
Build and packet tests pass. This proves geometry motion math, not animated
texture continuity: texture/scanline state still comes from the current source
snapshot and needs moving-view visual assessment.

## Spatial Titania water integration (2026-09-11)

Original water foreground now uses planar geometry in both runtime and host
capture. Per-fragment inverse projection replaces interpolated pre-projected
UVs, removing the first experiment's warped seam and reducing geometry to 12
vertices. Front/rear captures `tmp/vr-titania-surfaces-analytic` and
`tmp/vr-titania-surfaces-rear` were inspected: surrounding water, one forward
bridge, no rear duplicate or flat-panel edge. The initial
`tmp/vr-titania-surfaces-first` is failed-experiment evidence, not a delivered
result. Full vr-dev build, shader freshness, and 13/13 VR tests pass (9.26s).
Moving camera-height/tilt continuity, more water timestamps, EX and physical
headset acceptance remain unverified. No release or device install occurred.

## Water receiver geometry groundwork (2026-09-11)

Added an unintegrated `water_surface_packet` builder: planar source foreground
receivers, inverse-projected UVs in front, clamped water-only sampling behind,
and no tunnel-border fill. Packet tests verify plane height, front/back extent,
source projection and rear bridge exclusion; build/tests pass. It is not yet
used by gameplay or claimed visually correct. Rendered distortion, interpolation
across receiver seams and source camera-height alignment need checking before
integration; the existing foreground remains active.

## Titania foreground source audit (2026-09-11)

The water fixture now writes separate BG2 atlas, native scanline-composited BG2,
and native BG3 captures under `tmp/vr-titania-layer-audit`; all three inspected.
BG2 already contains authored perspective wedges with one central bridge and
water outside it, while BG3 contains stars/hills. A cylindrical/spherical wrap
of BG2 repeats the bridge and cannot satisfy the requested spatial foreground.
Its conversion must inverse-project the authored foreground onto world-space
surfaces and preserve nonrepeating bridge coverage. Diagnostic atlas images
display palette index zero as its CGRAM colour, not alpha; distinguish that
from opaque artwork when deriving coverage. Build and host capture pass.

## Titania foreground ordering (2026-09-11)

The water surround now inserts distant BG3 before the native BG2 foreground,
matching the desktop background ordering. Inspected
`tmp/vr-titania-water-order/live-scene-left.bmp`: water/bridge artwork is visible
again instead of being covered by the opaque distant terrain. Earlier panorama
captures prove surround coverage but not correct foreground visibility.
BG2 still uses a projected flat panel; its rectangular boundary is visible and
needs actual spatial conversion. No full water-environment completion claim.

## Titania distant backdrop surround (2026-09-11)

Original Mode-1 BG_2_3B now selects a distant BG3 panorama in the game and
capture renderer. Front/rear captures in `tmp/vr-titania-backdrop-surround`
and `tmp/vr-titania-backdrop-rear` were inspected: stars/horizon extend around
the viewer without the rectangular fallback-colour border. BG3 atlas/scroll
payload is unchanged; new packet tests verify that and the 64-unit radius.
Full vr-dev build and 13/13 VR tests pass (9.44 seconds). Not headset-tested.

Correction to the initial layer interpretation below: BG3 is the distant
backdrop; BG2 carries perspective water/bridge artwork. The first BG2-sphere
experiment (`tmp/vr-titania-water-surround`) produced vertical streaks and was
replaced, not accepted. BG2's foreground projection remains unchanged and
needs further review; this is not full immersive water/bridge completion.

## Titania water reproduction (2026-09-11)

`--live-stage=LEVEL2_3:water[@rear]` now reuses the desktop authored setbg
2_3b entry fixture, including native initialization and water scanline setup.
It requires Mode 1 after 200 ticks. The host Vulkan run succeeds with background
147, Mode 1; inspected `tmp/vr-titania-water-before/live-scene-left.bmp` confirms
a rectangular background panel surrounded by the fallback colour. This is
reproduction evidence, not a fix. Water/sky needs its own surrounding mapping;
the BG3 bridge must remain a distinct nonrepeating structure.

## Later-stage environment capture (2026-09-11)

The host Vulkan tool now accepts `--live-stage=LEVEL2_3:1200@rear` (0..10000
elapsed stage ticks, optional view suffix). Explicit timed audits enable god
mode to avoid mistaking a Continue screen for the requested environment; the
legacy entry fixtures and their mapping assertions remain unchanged. Output
now reports actual elapsed ticks, background ID and PPU mode.

Titania 1200/2400-tick captures build/render successfully but remain background
141, Mode 2: they do not exercise the Mode 1 water/bridge section. The first
unprotected 1200-tick capture reached Continue and is rejected as water proof.
Source BGS.ASM `bg_2_3b_1` uses separate BG2 water and BG3 bridge data; copying
the Y=232 landscape sphere mapping is not a valid fix. No water mapping was
changed on this evidence. Use the established Titania-end fixture next to
reach the intended substrate without claiming these outdoor captures cover it.

## Dedicated Sector X horizon band (2026-09-11)

The failed clamped-landscape experiment below is superseded by a dedicated
orbital band packet for Original BG_2_2. It uses distant spherical geometry
with unclamped vertical UVs; a new validated tile flag discards samples outside
the authored 224-row band. Native scroll/offset tables and the existing unique
upper-row policy are preserved. This wraps the horizon without dragging its
last texture row down the lower hemisphere. It preserves a horizon band; it
does not invent a full globe beneath the player.

Front/rear host Vulkan captures in `tmp/vr-sectorx-band-front` and
`tmp/vr-sectorx-band-rear` were inspected: the curved band surrounds the player,
with black space below and no vertical streaks. Packet tests check distant
radius, unclamped UVs, retained scroll, and combined uniqueness/coverage flags.
The explicit LEVEL2_2 capture now asserts this mapping. The full vr-dev build,
all 13 VR tests (10.34 seconds), generated-shader validation, and Quest ARM64
debug assembly (14 seconds) pass. No physical headset install/validation yet.

## Asteroid surround and Sector X limitation (2026-09-11)

Original BG_1_2 now selects the distant stars-only sphere, verified against its
source `stars` character/screen atlas. The explicit LEVEL1_2 capture asserts
this selection. Rear capture `tmp/vr-stage-level1-2-surround/live-scene-left.bmp`
was inspected and shows background stars without the former rectangular edge.
EX classification remains unchanged. The full vr-dev build succeeds.

Sector X BG_2_2 was captured in `tmp/vr-stage-level2-2-before`. An attempted
reuse of landscape sphere mapping stretched its final terrain row into vertical
streaks (`tmp/vr-stage-level2-2-surround` / `tmp/vr-stage-level2-2-rear`). That
attempt was reverted; the artifact folders are failed-experiment evidence,
not delivered fixes. This background needs a dedicated horizon mapping with
unique upper artwork and a repeatable lower band, not terrain-edge clamping.

## Sector Y surrounding star atlas (2026-09-11)

Original BG_2_4 and BG_CRED use the same `24` star/nebula character and screen
atlas in source BGS.ASM. Both now select the existing distant full-atlas star
sphere. EX retains its prior intro-only BG_CRED selection; planet-bearing
backgrounds are not added to this repeating-atlas set. The scene property is
now named `background_star_sphere` rather than `background_intro_stars`.

Original LEVEL2_4 entry is asserted by the capture tool. Inspected host Vulkan
captures in `tmp/vr-stage-level2-4-before`, `tmp/vr-stage-level2-4-surround`, and
`tmp/vr-stage-level2-4-rear` show the former brown outer rectangles replaced by
stars/nebulae surrounding the player. Credits selection follows the shared
source atlas but has not had a full staff-roll visual review. Full vr-dev build
and all 13 VR checks pass (11.01 seconds). No headset validation is claimed.

## Titania outdoor surround and stage captures (2026-09-11)

Original BG_2_3A now uses the distant 360-degree landscape geometry and shared
tilt/grid anchoring used by Corneria/Training. The source BGS.ASM confirms its
outdoor atlas is based at Y=232. Water BG_2_3B, tunnel BG_2_3C and EX backgrounds
are deliberately not classified by this new entry. The scene flag is renamed
`background_landscape` to describe its expanded scope.

The Vulkan capture tool now accepts `--live-stage=LEVEL2_3`, with optional
`@rear`, `@left`, `@right`, `@up`, or `@down`. Original Titania's entry asserts
landscape selection. Before/after GPU captures were inspected in
`tmp/vr-stage-level2-3` and `tmp/vr-stage-level2-3-surround`: rectangular backdrop
borders are removed. Rear and zenith captures in `tmp/vr-stage-level2-3-rear`
and `tmp/vr-stage-level2-3-up` show continued terrain/sky. These are host Vulkan
captures on Intel Graphics, not headset proof or full-stage validation.

The full vr-dev build and all 13 VR tests pass (9.81 seconds). Quest ARM64 debug
assembly succeeds in 14 seconds; the refreshed APK was not installed/launched.
An additional Macbeth LEVEL3_5 capture confirms its tunnel still needs a distinct
immersive treatment; it must not be treated as an outdoor sky sphere.

## Latest build refresh

After the projection-division/interior-clipping changes, the complete vr-dev
build succeeds and all 13 registered host VR checks pass (9.65 seconds),
including Original and EX cartridge checks. Generated VR shader validation
passes. Quest ARM64 debug assembly succeeds with payload checks; the APK is
at platform/quest/build/outputs/apk/debug/quest-debug.apk. This refresh did not
install or launch the headset. Android's SDL-disabled diagnostic model path
still produces unused-parameter warnings; Gradle deprecation warnings remain.
Desktop native model parity and broader immersive/effects work are unfinished.

## VR startup localization work (unreleased)

The four startup option rows now use the shared Japanese/German/French/Spanish
menu catalog. English and English (Europe) retain the cartridge font path.
Unicode rows use the existing localized font renderer to prepare sparse 16x16
glyph-mask tiles on menu changes; GPU text decoding/compositing uses the existing
shader. Language names use their native spelling, and invalid language IDs fall
back safely. Five Unicode tile fixtures match font pixels exactly in Original
and EX, and the 13 host VR checks pass. The two footer help lines are localized
as well. The refreshed Quest ARM64 debug APK builds and passes payload checks;
it has not been installed or tested in the headset. Japanese and Spanish Vulkan
captures render nonblank in both eyes (`tmp/vr-startup-japanese` and
`tmp/vr-startup-spanish`). These two layouts were inspected at 1536x1536: all
four rows and both help lines are visible without overlap or clipping. German
Original and French EX captures also pass GPU readback. Startup capture mode
now uses 1536x1536 and only renders the two menu eyes; it does not claim to run
the synthetic triangle suite. The unchanged 256x256 synthetic suite was run
separately and passes. Physical headset readability validation remains required.

## Current-worktree build/test refresh (2026-09-11)

Quest ARM64 debug APK assembly and payload checks pass after the desktop GPU
precision changes. No new headset install or launch was performed in this
refresh. Compilation still reports unused-parameter/capture and Gradle
deprecation warnings; this is not a warning-free build claim.

Host VR checks are now registered with CTest when BUILD_TESTING is enabled.
`ctest --test-dir build/vr-dev -L vr --output-on-failure -j4` passes all 13 checks,
including Original and EX cartridge input/scene checks. Hardware runtime/scene
probes are deliberately not included in this headset-independent suite.
The cartridge assertions were updated to use `vertex_view()`, matching the
renderer’s owned-or-shared cached geometry, rather than treating an empty owned
vector as empty GPU geometry. Shadow colour, ordering and gating checks remain.
This refresh does not replace physical headset validation or the wider goal.

## Latest headset follow-ups and CPU preparation optimizations

- Mario/Luigi eye faces exactly duplicate their underlying skin polygons.
  The VR batcher now recognizes matching textured decals and applies a tiny
  GPU depth bias only to those faces. Both models' eye captures were inspected
  (`tmp/vr-mario-eyes-fixed`, `tmp/vr-luigi-eyes-fixed`). Synthetic tests verify
  the decal survives its backing surface but remains hidden by nearer geometry
  in both eyes. Ordinary textures and exploded faces are not biased.
- Controls stars now draw **after** the opaque menu background, before the
  demo model, so the viewport no longer hides its own emitter. Births remain
  inside (24..136,24..112); later travel is not clipped to the panel. The full
  Original composition was inspected in `tmp/vr-controls-after`.
- EX's intro uses BG_CRED's star/nebula atlas, not Original's DEMO planet atlas.
  Its entire atlas now wraps a distant sphere, without a close foreground tile
  panel or source screen-warp tables. Front/rear/left/right/up/down GPU captures
  contain artwork in both eyes (`tmp/vr-ex-intro-*-final` and
  `tmp/vr-ex-intro-cached`). Original's separately revealed planet is unchanged.
- Corneria/Training grid height is anchored on outdoor entry. Moving up/down
  no longer translates the grid separately from the surrounding ground. Grid
  and background use the same interpolated tilt; independent grid yaw is
  removed. The requested native width/forward range and rear-only extension
  remain unchanged. Forty height/line/interpolation combinations plus the
  shared-tilt check pass for each cartridge.
- Immutable sky meshes are now shared across frames. CPU preparation measured
  2204 -> 73 us for stars and 2321 -> 64 us for landscapes (128-iteration local
  microbenchmarks). Compared EX intro and Training captures are byte-identical.
- Ordinary model geometry now has an exact-input, bounded cache; transforms
  still update every frame. Cache keys include animation, material/light/depth,
  palette and texture-scroll state. Special effects retain their full path.
  Retained data is capped at 65,536 vertices plus 262,144 texel words; unused
  objects are pruned. Original/EX each pass 128 mutation/restoration comparisons
  against uncached assembly. CPU preparation measured 114 -> 17 us (Original)
  and 71 -> 11 us (EX) in the sampled scene. These are not headset FPS claims.

The cache-equality preflight also passes all 59 numbered stage entries (19
Original, 40 EX), 600 source ticks each and three interpolation samples per
tick. Both intro-to-title flows pass 1,000 ticks with cached/uncached equality.
This checks CPU-produced GPU data and transforms, not whole-level visual parity.
The Windows Vulkan regression suite, input/camera/session/mesh tests, shader
regeneration check and Quest ARM64 build pass.
The latest APK is installed on Quest 3 `2G0YC1ZF8R059K` using a data-preserving
update. It was not launched while the user was away. Original and EX Controls
composites were both inspected; EX evidence is in `tmp/vr-controls-ex-final`.

These changes require another user headset check. Windows Vulkan captures and
Android compilation are not a claim of physical visual validation, a completed
all-level immersive presentation, or a completed desktop GPU migration.

## Controls star emission

Controls uses a dedicated GPU emitter centered at native PPU (80,68), the
112x88 preview midpoint. Birth rays lie within +/-48 by +/-36 pixels at far
depth, inside the panel (24..136,24..112). Stars then travel outwards without
viewport clipping, as requested; full-surround procedural stars are disabled
on both Controls flows. The emitter's stable seed cache is independent of native
star recycling. `--dust-controls` checks all seed bounds/cache stability and
passes Vulkan rendering in both eyes; `tmp/vr-controls-emitter-proof` was
inspected. Full Controls artwork composition and headset emission timing remain
to be visually verified.

## Latest user corrections: experience, bank tilt, rear-only grid

The VR startup menu now has EXPERIENCE: ORIGINAL / STAR FOX EX. On Start it
constructs the selected cartridge, checks its identity, replaces the audio/game
driver, and invalidates PPU caches. Alternate files are `SFES.SFC` plus
`SFES-SYMBOLS.TXT` beside Original's inputs; the Quest importer has separate EX
buttons. Missing alternate files leave an explicit ONLY label. EX inputs were
copied into previously absent paths on the attached Quest without replacing
Original files. Experience choice is session-only; switching currently uses
native music, not an automatically selected alternate MSU pack.

Landscape presentation now includes interpolated camera bank (roll) as well as
computed vertical-scroll tilt, but still excludes yaw and native scanline
distortion. Earlier descriptions saying all roll is omitted are superseded.

Grid extension is now strictly rear-only: x=0..14 and forward z=0..14 retain
the native span; z=-24..-1 adds rear rows. Dots use 39x15 points and connected
rows have 1,092 endpoints. The former all-direction 63x63 extension was broader
than requested and is superseded. Rear EX GPU validation now checks these bounds.

`enter_title` no longer publishes INITGAME's temporary active player as a model
frame. Normal title strategy transfers repopulate the draw list. A source-game
intro-to-title regression was added; headset validation remains pending.

## EX connected-grid world pass

Live immersive EX connected grids now share the expanded 63x63 world lattice.
Adjacent points are joined within each row using GPU-transformed line segments;
row ends are not joined to the following row. The flat source-pixel compositor
remains available only for source-parity diagnostics. The rear-facing EX run
`--grid-surround-lines` passes, with 7,812 line endpoints plus grid dots; its
capture in `tmp/vr-ex-world-grid-lines` was inspected and shows continuous rows
behind the player. This is bounded GPU evidence, not headset gameplay proof or
an assertion that native screen-space line rasterization is preserved in VR.

## Corrected motion source and wider VR grid

The previous pitch/PPU-scroll wiring did not drive motion: Original intro
tracing shows PPU Y=24 and VIEWROTXW=0 throughout ticks 40–400, while BG2SCROLL
changes 24,37,77,117,157,178,198,209,219. GSTRATS CALCBGSCROLL computes this from
OUTVX; TRANS applies it through vertical-offset tables. Snapshots now capture
BG2SCROLL directly. Intro uses it only on the planet patch; the star sphere
stays still. Corneria/Training convert its displacement from 232 into smooth
pitch-only tilt, with no native per-column warping.

Live VR dot grids now cover a cached 63x63 lattice instead of 15x15, extending
roughly 32 m horizontally in each direction. The shader retains rear points
and uses radial billboard size, instead of rejecting negative forward Z.
`--grid-surround` passes and its rear-facing capture was inspected in
`tmp/vr-grid-surround-proof`. EX connected line grids remain a separate,
unfinished source-plane path; this is not a claim that those were extended.

Gameplay/training models and their shadows are shifted 0.25 m farther away
for the user's comfort request. HUD and distant background positions remain
unchanged. These latest changes still need headset feedback.

### Bomb-circle GPU readback evidence

`starfox_vr_scene_check tmp/vr-circle-blend-proof` now exercises all four
add/subtract/half blend combinations in both eyes on an RGBA8 target. Center
RGB and untouched exterior checks pass for all eight submissions. The additive
capture was visually inspected: an analytic circular disk, not a filled quad.
This validates the fixed-function blend implementation, not native per-layer
mask parity, sRGB integer color math, or headset gameplay behavior.

## Current follow-up: pitch-only landscapes and intro reveal

Corneria BG_1_1C and BG_TRAINING now use the authored STP atlas at Y=232,
without native per-scanline horizontal/vertical offsets, mosaic or tunnel
border policy. I/A/B hangar backgrounds are no longer misclassified as outdoor
sky. The sphere's only game-camera motion is shortest-path interpolated pitch,
updated each headset frame via model uniforms; yaw and roll are omitted.
Original desktop Vulkan forward captures in `tmp/vr-corneria-pitch-only` and
`tmp/vr-training-pitch-only` were inspected. These are not headset motion proof.

The Original intro planet follows interpolated native vertical scroll as a
distant angular reveal instead of staying centered from the start. Its distance
remains 64 m. Tests cover the below-to-center path and angular wrap.

Bomb circles now have a GPU analytic-disk pass with interpolated radius/color,
add/subtract/half modes and pre/post-HUD ordering. Packet tests and builds pass;
hardware visual validation is pending. Fixed-function blending is not yet exact
SNES integer color math on sRGB targets, and fine-grained native background-layer
masking remains incomplete. Do not claim full presentation parity for this pass.

## Headset feedback: intro and Corneria scale

The user confirmed the surrounding intro looks good and Corneria is immersive,
but reported oversized/near mountains, choppy tilting, and a glitchy hangar-exit
camera. The latter two remain under investigation; model camera interpolation
already exists, whereas background packets are still updated at source ticks.

Original DEMO intro now omits the scrolling foreground BG panel. A single
planet uses atlas x=88..160, y=328..400, centered 64 m away, over the existing
star sphere. `tmp/vr-intro-distant-planet/live-scene-left.bmp` was inspected:
one planet and no foreground star rectangle. This is desktop Vulkan readback,
not validation of the new change inside the headset. EX is intentionally
unchanged because its atlas has not been verified to use this layout.

Corneria now uses twice the angular texel density on its 64 m sphere, reducing
mountain angular size approximately by half while preserving blue/green poles.
Packet, eye-camera, input tests and Original intro/Corneria Vulkan checks pass.
The pre-game VR menu is present (Start, Language, God Mode); it remains a basic,
session-only menu rather than the complete desktop options UI.

## Corneria surrounding background

Directional follow-up: Original Vulkan front/rear/left/right/up/down runs pass.
Rear, overhead and downward left-eye captures were inspected in
`tmp/vr-corneria-{rear,up,down}`: the rear continues clouds/grass, overhead is
blue, below is green, and no uncovered pole is visible. These prove bounded
directional background coverage, not headset comfort, physical terrain, or all
Corneria transitions. Other levels still need their environment policies.

BG_1_1I/A/B/C are classified explicitly in Original/EX snapshots. Their live
BG2 panel is replaced by a surrounding sphere: horizontal cloud scenery wraps,
and the outer sky/ground scanlines extend toward the poles without vertical
tilemap repetition. This is a surrounding background, not physical ground
geometry. The Original forward Vulkan capture was inspected at
`tmp/vr-corneria-surround/live-scene-left.bmp`: blue/green fill the frame and the
prior pink margins are absent. Rear/up/down GPU captures, physical-headset
validation, other stage classifications and ground interaction remain pending.

## HUD origin correction

VR meters now use the inner 224x192 Super FX origin, while OAM labels/icons
retain the 256x224 PPU origin. This supplies the missing 16-pixel guard offset
on both axes. Mode-3 layers and briefing text explicitly retain the PPU matrix
instead of inheriting the last meter's matrix. Coordinate regression and live
Original Vulkan capture pass; `tmp/vr-hud-origin/live-scene-left.bmp` was inspected
and shows meters under the shield label and bomb icons. Headset verification and
customizable VR HUD placement remain pending.

## VR startup menu and Index input

Native-intro startup now opens a GPU-text VR menu with Start Game, language and
god mode. The source simulation/audio remain paused until Start; confirmation
must be released before gameplay input resumes. Navigation and confirmation
are edge-triggered, including focus loss/regain. Injected menu/input tests and
text packet tests pass. Menu labels are currently English and settings are
session-only; headset legibility/operation is not yet verified.

Valve Index has an explicit OpenXR profile: left stick steering; right A/B
fire/bomb; left A/B boost/brake; left/right trigger clicks L/R; right grip
Start/pause and left grip Select/change view. Both grips open runtime options;
both triggers plus newly pressed stick clicks reset the app. The system button
is not used. Exact Touch and Index binding paths are covered by input tests.
Index hardware
and SteamVR end-to-end testing remain outstanding.

## Current surrounding-space changes

User reports that intro and title's authored BG stars still occupy a small
window. Confirmed by `--live-intro-background`: Original INTROMAP after 400
ticks, no model/dust/OAM packets, still renders planet plus tiny static stars
(`tmp/vr-intro-background-only/live-scene-left.bmp`, inspected). Thus the
procedural volume is not a replacement for extending the authored tile stars.
Remaining work: separate the planet from this tilemap and project its star-only
content around the viewer, preserving a single planet. Do not claim the current
six-direction dust test proves this requirement.

The VR star volume is now independent of native forward-cone recycling. Stable
hashed world positions wrap in a 4096-unit volume; source camera motion and star
colours still drive presentation. Six directional Vulkan fixtures (front, rear,
left, right, up, down) render nonzero stars in both eyes and confirm cache reuse
after source points recycle. Captures are in `tmp/vr-volume-*`. This supersedes
the earlier claim that retaining the original source volume was sufficient.

Live pass order is now stars (no depth writes), background/planet art, models,
HUD. Unique space backgrounds discard black sky and removed repeat borders;
the outer clear matches their darkest source border colour instead of pink
CGRAM zero. Exact black pixels within planet artwork may still need an explicit
silhouette mask; this is not a claim of complete occlusion fidelity.

Title 3D object packets are moved two metres back, retaining original overlay
order as requested. Combined headset validation of these changes is pending.

## Planet selection layers

Mode 3 now uses the source eight-pass order: BG2 low, OBJ0, BG1 low,
OBJ1, BG2 high, OBJ2, BG1 high, OBJ3. Previously VR omitted the Mode-3
BG1 planet artwork. Backgrounds and sprites are composed together without
depth testing, before the added briefing text. Packet-order/disabled-layer
tests and the Quest APK build pass. Cartridge PLANETSELECT snapshots after
120 ticks render on Intel Vulkan for Original and EX; inspected left-eye
captures show their planet maps (`tmp/vr-planets-original/live-scene-left.bmp`
and `tmp/vr-planets-ex/live-scene-left.bmp`). This is not physical-headset
verification, full briefing-fade parity, or a surrounding 360-degree world.

## Briefing text layer

VR snapshots now retain BriefingState, and the live sprite pass appends the
message/planet-name text and their source-colored shadows. Variable-width
12-row glyphs use packed source bits consumed by the GPU font decoder; CPU work
is bounded string layout and payload assembly, not pixel rasterization. Planar
glyph flags 1024/1026 are accepted alongside existing billboard glyphs.
A synthetic font/string test compares packet-decoded pixels against the desktop
text renderer for 20 reveal-count/wrap cases. Real Original/EX TEAMTXT captures
also match a software-rasterized texture oracle byte-for-byte in both Vulkan
eyes, across four reveal counts. Original capture was visually inspected;
directories are `tmp/vr-{original,ex}-briefing-text` and corresponding
`-reference` directories. These fixtures pass, but physical Pepper
text verification, planet rendering and view-dependent
background distortion remain outstanding. This is not complete localization/UI
migration; the Quest build currently follows the native source-language path.

## Live Mode-2 horizontal background coverage

The live VR Mode-2 BG2 quad now spans logical X -384..640 instead of 0..256,
using the existing GPU expanded-margin decoder and authored unique-top policy.
Source pixel scale, sprite/HUD placement and other background modes are unchanged.
Original and EX LEVEL1_1 composite checks pass on Intel Vulkan; the inspected
Original image in `tmp/vr-wide-terrain-original/live-scene-left.bmp` shows the
terrain band spanning the view. EX captures are in `tmp/vr-wide-terrain-ex`.
This is horizontal planar coverage only, not a completed surrounding VR world:
vertical extent, sky/ground placement, and headset visual verification remain.

## First physical performance sample

The corrected session remained running without StarFoxVR errors. Quest VrApi
samples at 21:30:17–21:30:28 reported 55–63 application FPS against 72 Hz, with
11–20 stale frames in sampled intervals. This is an initial live sample, not a
controlled benchmark. The development native build had no optimization flag.
Quest debug configuration now requests `-g -O2` for C and C++; it retains debug
symbols and the debuggable APK. The optimized APK built and installed; generated
native compile commands confirm `-g -O2`. First headset samples reached 71–73/72
FPS, including three intervals with zero stale frames, but also a 47/72 interval
and a 25-frame stale maximum during the startup sample. A controlled same-scene
comparison remains pending: this is encouraging live evidence, not stable
full-game performance proof.

A subsequent 30-sample window from the same optimized process (27992) reported
71–73 FPS, mean 72.33, with 24 total stale frames and a maximum of 5 per sample;
no StarFoxVR errors were present. Runtime FPS counters can exceed the 72 Hz
target slightly across sampling boundaries. The follow-up compositor capture
`tmp/quest-empty-packet-proof/quest-optimized-headset.png` shows predominantly a
blue/gray surface in both eyes, not readable game content. Whether this was the
headset looking away/down or incorrect scene placement remains unconfirmed;
do not treat the timing result as visual-correctness proof.

Current Touch bindings: left stick steers; A fires, B bombs, X boosts, Y brakes;
left/right triggers are L/R, right grip is Start/pause, left grip is
Select/change view. Both grips together open runtime options. Index uses the
same trigger/grip mapping, with left A/B for boost/brake.

While gameplay is paused, cyan controller rays select scene models using
their bounding boxes. Release the trigger after entering pause, then hold it
to grab; the ray turns yellow. Move/aim the controller to reposition the model
at the captured ray distance. Either hand can grab, but not the same model
simultaneously. Tracking loss releases a grab. Unpausing commits the positions
to the simulation, whose normal movement/attachment scripts then resume.
Backgrounds and screen-space HUD are not movable scene objects. This local
implementation has automated checks but still needs headset validation.

## Deploying to the connected Quest 3

After accepting USB debugging inside the headset, run from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/deploy_quest.ps1 -Serial YOUR_DEVICE_SERIAL -SdkRoot tmp/quest-toolchain/android-sdk -Launch
```

This selects an explicit serial, checks authorization and the Quest 3 model,
then installs the debug APK with data-preserving `install -r`. It never
uninstalls, clears data, forces a downgrade, or replaces ROMs/saves. Signing
conflicts stop deployment instead of deleting existing data. Omit `-Launch`
to install without starting the Activity. Launch success does not prove stereo
rendering, audio, controller mapping, or lifecycle correctness.

Authorization rejection and successful data-preserving installation were exercised
on the connected Quest 3. The first launch exposed a pre-decor fullscreen crash;
using the decor view's nullable insets controller fixes that startup failure.
The rebuilt launcher runs and creates app storage. Prepared Original inputs were
copied into the verified-empty app directory. OpenXR creates both 1680x1760 eye
swapchains and audio opens; visual rendering and controller operation still need
headset verification. The deployment launch explicitly supplies MAIN and the VR
category so the shell does not request an ordinary uncategorized Activity launch.

## Quest input setup

Physical startup testing subsequently reached READY but aborted with an invalid
empty scene packet (zero vertices/lines/deferred entries, invalid transform).
DrawPacket now defaults to an affine identity rather than a zero matrix; the
packet regression suite checks this invariant. The ARM64 APK rebuild passes.
Native stderr now reaches the StarFoxVR logcat tag, and packet rejection reports
the failing geometry/transform fields. After renewed authorization the corrected
APK installed and reached SYNCHRONIZED, VISIBLE and FOCUSED. A physical Quest
compositor screenshot shows the Original title, ship, portraits and stars in both
eyes: `tmp/quest-empty-packet-proof/quest-headset.png` (visually inspected).
The Intel Vulkan suite also passes default empty packets in keyed and unkeyed
uploads. Its previous fixture manually supplied a transform and missed the live
Original bitmap-placeholder failure. This establishes title rendering, not stereo
comfort, gameplay controls, audio quality or completion of the migration.

Quest fullscreen now follows the flat Android host's policy: API 30+ uses
WindowInsetsController with transient bars by swipe and edge-to-edge layout;
API 29 retains immersive-sticky flags. Fullscreen is reapplied on resume,
after decor attachment, and on focus regain, including return from the document
picker. Cutout layout is enabled and FORCE_NOT_FULLSCREEN is cleared. This
does not suppress system-owned picker UI or prevent system navigation gestures.
Compilation verifies API availability, not the absence of bars on a headset;
physical resume/focus/picker-return verification remains outstanding.

The development launcher now offers **Import prepared ROM**, **Import matching
symbols**, **Start VR**, and **Stop VR**. The Android document picker copies
selected files to the existing app-local `SF.SFC`/`SYMBOLS.TXT` locations; it
does not modify the originals or patch a retail ROM. Each copy is streamed off
the UI thread, rejects empty/oversized input (16 MiB ROM, 32 MiB symbols), and
uses Android AtomicFile commit/rollback. The shared session lease prevents an
import racing a native session or another Activity's import. Existing prepared
files still trigger automatic startup; Stop permits returning to setup.

Pure-Java input-copy tests cover exact-limit copying across multiple buffers,
oversized/empty input, zero-length reads, provider/output errors, and invalid
limits. The updated ARM64 debug APK builds. These tests do not exercise Android
AtomicFile, document-provider permissions, picker availability, or actual Quest
UI/lifecycle behavior; those require on-device verification. Matching the ROM
and symbols remains the user's responsibility and is stated in the launcher.

## Quest startup/shutdown ownership

QuestActivity now owns a per-session lease through SDL context cleanup. Native
return releases that context before posting status to the UI; the UI callback
no longer clears global SDL state. A Thread.start failure after SDL setup also
cleans up. Idempotent leases prevent a stale close from unlocking a newer
session, and exclusivity lasts through cleanup even if cleanup throws.

`tests/QuestSessionGateTest.java` passes on the workspace JDK, covering exclusive
acquisition, duplicate/stale close, failed cleanup and concurrently in-flight
cleanup with bounded waits. These are pure-Java ownership checks, not physical
Android Activity lifecycle tests. The updated Quest debug APK builds; headset
launch, shutdown and relaunch remain unverified.

## GPU upper-tilemap single-occurrence policy

BG2 packets now encode `single_occurrence_top_rows` in control-word 15 bits 8+,
retaining bits 0/1 for black transparency and horizontal wrapping. Validation
rejects reserved bits and row counts above 224. The shader suppresses repeated
upper artwork only outside the native window and authored horizontal map range,
writing the darkest source palette colour opaquely; lower rows still wrap.

Live snapshots select 168 rows for BG_2_2 and 224 for BG_3_4B/BG_3_4D in Mode 2,
matching the existing desktop policy. Native-width VR background quads currently
do not expose these margins: full headset/wide world placement remains unfinished.
This is Vulkan GPU decoder/policy work, not a claim of completed desktop geometry
migration or a new widescreen visual fix. Packet tests, shader freshness, diff
checks and Quest APK rebuild pass.
The extended Intel Vulkan suite passes all 872 tile cases in both eyes, including
72 new upper-row cases varying scroll, native/margin X and the 167/168 boundary.
A contrasting underlay detects accidental transparency. Diagnostic directory:
`tmp/vr-unique-top-proof`; no physical headset or in-game wide-scene proof.

## Shared world-pass preflight

`SourceModels::assemble_world_interpolated` now supplies both the live app and
preflight with the same grid/dust/model packet sequence and resource keys.
Previously preflight omitted dust and both grid paths. Connected grids retain
their existing source-plane placement; this refactor is not their world-depth
migration. Original and EX tests compare the combined result with the previous
explicit assembly sequence, including painter order, geometry and transforms.

`tools/check_vr_world.ps1` discovers numbered stages from the cartridge symbols,
runs each through 600 source ticks by default, and fails if any stage fails.
It covers EX routes 4-7 as well as routes 1-3. The completed sweep passes all
59 starts (19 Original + 40 EX), with zero failures. These are neutral-input source
assembly checks, not full level playthroughs, GPU execution, PPU backgrounds,
headset testing or photographic parity proof. The Quest debug APK rebuild passes.

## Quest EX cartridge persistence

Quest supplies `starfox-ex.srm` in the same app-specific external-files directory
as its prepared ROM/symbol files. EX startup loads the 65,536-byte cartridge RAM
before constructing the simulation. Initialized RAM and subsequent source-tick
changes are saved through the shared atomic-replacement writer; unchanged RAM
does not trigger disk writes. Original cartridges and diagnostic preflights do
not load or write this file. There is no power-loss durability guarantee.

Existing files with invalid sizes cause an explicit startup error and are left
intact. A write error propagates visibly and does not advance the saved baseline.
Host tests cover create/reload/change/no-op, invalid inputs, corrupt-size file
preservation, replacement failure and successful retry. Quest APK builds. Actual
on-device persistence, uninstall/data-backup behavior and VR save-slot UI remain
unverified or unimplemented; this is cartridge RAM, not a save-state feature.

## Longer intro/title regression

Both Original and EX pass 10,000 neutral-input source ticks from INTROMAP,
with three model interpolation samples per step: 229,599 / 259,152 packets.
Preflight now reports observed flows rather than implying the whole run remained
in its entry map. Original sampled 3,344 intro and 6,657 title states; EX sampled
3,772 intro and 6,229 title states. The extra state is the initial frame. No
gameplay/controls navigation or headset-rendering claim follows from this run.

Collapsed-axis model rendering now honors forced colour overriding colour warp,
matching the shared source material logic. A regression compares the complete
forced-colour geometry with warp off/on; genuine non-overridden warp remains
explicitly unsupported. Packet tests, Quest APK rebuild and diff checks pass.

## Quest Select / change-view input

The Touch profile now binds Select to the left trigger, completing the native
Select path used by the control screen and view changes. Existing A/B/X/Y,
stick steering/click rolls and left-menu Start bindings are unchanged. The
OpenXR runtime performs scalar-trigger to boolean conversion as specified in
https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html . Select, like
Start, requires release before a fresh press after startup/focus loss/inactivity.
The tick latch retains short taps and does not repeat held press edges.

Injected-runtime input tests pass. Original/EX native-pad state comparisons
now include Select; the broader scene/pacing checks pass after repairing a
stale glyph test to decode the packed nine-word font payload rather than expect
256 expanded RGBA words. The oracle still compares all pixels to source ROM.
The updated Quest debug APK builds. Physical Touch bindings remain unverified.

## Quest intro entry and rendered title-layer proof

Quest now uses `--intro ROM SYMBOLS`, entering INTROMAP without the diagnostic
checkpoint preroll. The existing `--render-game` direct-level diagnostic remains.
The host options menu is not rendered in VR yet: BOOT enters that menu and must
not be mistaken for the native intro. Original and EX INTROMAP preflights each
pass 600 source ticks and three interpolation samples per tick, producing
18,984 / 19,518 model packets. These are simulation/model-assembly checks, not
headset playback or menu-navigation proof.

The Vulkan tile suite now includes 128 title-composition cases using the real
foreground packet builder over a model-coloured plane, with depth disabled as
in the live foreground pass. CPU `draw_title_foreground` supplies the expected
pixels. Layer enables, optional BG1, BG2/BG3 priority and opaque black colours
are varied. All 800 total tile cases match in both eyes on Intel Vulkan.
Diagnostic captures: `tmp/vr-title-composition-proof`; not in-game screenshots.

## Live title foreground composition

The VR application now restores source Mode-1 title layers after models and
before OAM: high-priority BG2, optional BG1, then high-priority BG3. These
packets retain opaque CGRAM-black pixels for outlines/occlusion. EX's BG_TITLEI
background is recognized from matching-bank source symbols and omits BG1,
matching the desktop title compositor. Only BG2 inherits the background scroll
override. Unit checks cover ordering, priorities, scroll isolation, brightness,
black coverage, disabled layers and unsupported modes.

Host build, packet tests, application cancellation test, shader freshness and
Quest debug APK rebuild pass. This APK also includes the no-wrap shader below.
No Quest was connected during verification; live title captures, widescreen
composition policy and headset comfort remain unverified. SDK licenses were
accepted with explicit user approval and the SDK/NDK are workspace-local.

## GPU tilemap no-wrap support

`BackgroundTileOptions::wrap_horizontal` now supports a single authored tilemap
occurrence. The shader rejects unwrapped source X outside the map before tile
lookup, including negative scroll, 8/16px tiles and one/two horizontal pages.
Control 15 bit 0 remains transparent-CGRAM-black; bit 1 disables horizontal
wrapping. Validation rejects other bits, preserving transactional upload errors.
The extended Intel Vulkan suite passes all 672 CPU-decoder comparison cases in
both eyes, including 80 new wrap/no-wrap combinations. Shader freshness and
diff whitespace checks pass. Captures: `tmp/vr-no-wrap-proof` (diagnostic fixtures).
This is a GPU decoder capability, not proof that live VR scene composition
selects the correct expansion policy everywhere; that integration remains open.

## Numbered-stage model preflight expansion

All 19 numbered stage starts (1-1..1-6, 2-1..2-6, 3-1..3-7), in Original and
EX, have now passed 600 logic ticks with three model interpolation samples per
step and neutral input. These 38 sampled runs are not full playthroughs or
GPU/background/HUD verification; deaths and other normal flow changes may occur.

The first sweep found one common failure: LEVEL1_3 step 48, HYPER4. Original
`SF/SHAPES/SHAPES2.ASM` defines two vertices but a visibility triple 0/2/4.
The software renderer explicitly suppresses a face when its visibility record
references missing vertices. VR now records `SourceNoop::invalid_visibility`
for that same condition instead of aborting the scene. Invalid group triples
likewise suppress their batch, not unrelated children; explosions still bypass
visibility. Mesh tests cover triangle/line/group suppression and subsequent
valid input. Original and EX stage 1-3 reruns both pass 600 ticks. Source-model
errors now include shape/LOD addresses to help diagnose subsequent failures.

## GPU exploding faces: initial implementation verified

The shader now performs source face-normal rotation, downward-Y forcing,
half-away-from-zero rounding and signed progress/4 displacement. Exact Q15
rotation retains per-product shifts and signed-word wrapping; continuous poses
use floating rotation. Position transformation and displacement happen before
headset eye transforms, so fragment motion is not tied to head orientation.
Exploding meshes bypass source BSP/face visibility and reuse those vertex fields
for coefficients, translation, normal and progress; no additional vertex
attributes or larger push constants are required. CPU code packs metadata only.
This currently reuploads changing explosion metadata with the vertex batch;
further per-instance upload optimization remains possible.

Original and EX LEVEL1_1 model preflight now pass 600 ticks / three interpolation
samples per step (31,560 and 35,589 packets respectively). Native mesh/packet
tests pass. Rotated Q15 and continuous triangular explosion fixtures produce
byte-identical CPU-reference/GPU images in both eyes on Intel Vulkan, with the
existing scene diagnostic regressions also passing. Captures are under
`tmp/vr-explosion-{gpu,reference}` and
`tmp/vr-explosion-continuous-{gpu,reference}`. The left Q15 capture was inspected.
These are isolated fixtures, not exhaustive explosions or headset gameplay proof.
The Quest APK rebuilt successfully after the shader change.
Current APK SHA-256:
`33f459a840388bd04dfaa29f3b41561a0d539ea98d20accaaf13c1ff5a3506c3`.

### Original preflight finding (resolved for these tested sequences)

`starfox_vr_runtime_check --preflight ROM SYMBOLS LEVEL FRAMES` advances the real
simulation/audio scheduler without an audio device or headset and assembles VR
model packets at alpha 0, 0.5 and 1 after each 50-ms step. It rejects unsupported
draws rather than silently dropping them. It does not test GPU pixels or all
background/HUD layers, and currently uses neutral controls.

The initial 600-step LEVEL1_1 checks fail at step 82 for Original (object 15) and
81 for EX (object 20): `Native exploding-face stage pending`. The original
development APK could not sustain this gameplay sequence. GPU face explosion support,
including source normal rotation, forced-downward Y, signed rounding/shift and
visibility/BSP bypass, was added above. Passing APK packaging is explicitly not
proof of playable VR. The preflight check and shared host target compile on
Windows; actual headset rendering remains unavailable.

## Quest development APK (built; headset-unverified)

The first ARM64 development APK builds with NDK r28c/JDK 17. Android libc++
exposed an incomplete `Rgba8` definition in `gpu_effects.hpp`; including its
defining palette header fixed it. Native linking, Java compilation and packaging
passed. Payload checks confirm `libstarfox_quest.so`, SDL and no flat `libmain.so`.
`aapt` confirms the Quest Activity and arm64-v8a; APK v2 debug signing verifies.
The ELF is AArch64 and exports the Quest JNI entry point.
Artifact: `platform/quest/build/outputs/apk/debug/quest-debug.apk`.
SHA-256: `560bdcaf56285edb360a5b60b9a5aa3ecdada3da2d2689b216103606d69a29a9`.
ADB lists no connected device. Installation, OpenXR startup, audio, input and
rendered correctness remain headset-unverified. Nothing has been published.

`platform/android` now includes a separate `:quest` module at `platform/quest`.
It builds only `starfox_quest` for arm64, uses the SDL 3.4.14 AAR/Prefab dependency,
and has its own application ID. The flat Android Activity is unchanged.
The new Quest Activity initializes SDL JNI/audio support, launches the shared
OpenXR loop on a Java worker, and cancels on destruction without joining on the
UI thread. A process-wide guard prevents overlapping SDL/OpenXR sessions.
Its manifest declares Quest 3, VR launch category and OpenXR loader visibility.
Manifest XML parses locally; lifecycle behavior on a headset remains unverified.
This is development packaging, not a release APK.

Build entry: `platform/android/gradlew.bat -p platform/android :quest:assembleDebug`.
The first `:quest:tasks --all` attempt downloaded Gradle 8.12, then failed during
Android Gradle plugin resolution because this host runs Java 8. It did not reach
Quest module configuration, Java compilation or native compilation.
Requires JDK 17, Android SDK 35/NDK 28.2.13676358 and the same SDL AAR used by the
flat build at `platform/android/app/libs/SDL3-3.4.14.aar`.
Workspace-local Temurin 17 and the SHA-256-verified SDL AAR are now available.
With Java 17, Gradle gets past plugin resolution and reports the missing SDK.
SDK tools 19 were downloaded and checksum-verified. With the user's explicit
license approval, SDK 35, build tools 35, NDK r28c and CMake 3.22.1 were installed
locally. ARM64 Android API 29 NDK syntax checks of the JNI entry and Android
OpenXR initialization now pass. The full Gradle APK build subsequently passed;
packaging does not establish functioning headset presentation.
Windows helper: `tools/build_quest.ps1 -JdkRoot <JDK> -SdkRoot <SDK>` restores the
caller's environment after building and does not push or publish the APK.
The initial development host reads a prepared `SF.SFC` and matching `SYMBOLS.TXT`
from its app-specific external files directory. The setup UI can now import
those prepared inputs, but does not bundle a ROM or patch retail images. Current live presentation parity gaps
still apply; successfully packaging this target will not by itself complete VR.

Manifest reference: [Meta native Android manifest settings](https://developers.meta.com/horizon/documentation/native/android/mobile-native-manifest/).

## Shared application entry point

An Android-only `starfox_quest` shared-library target now exposes
`QuestBridge.run` through JNI. The Java declaration is staged separately under
`platform/quest`, not added to the flat Android application. It accepts host
Activity/Context, ROM/symbol paths and an AtomicBoolean cancellation flag. Global
JNI references remain alive until native teardown; C++ exceptions do not cross
the JNI boundary. The Java-owned rendering thread supplies its own JNIEnv.
Cancellation is checked before startup, during cartridge preroll and in the
render loop. The Windows shared application target and pre-start cancellation
test pass. The JNI target is now NDK-compiled, and the Activity, SDL setup,
Gradle module and manifest are present. Device testing remains required.
Reference: [Android JNI lifecycle guidance](https://developer.android.com/ndk/guides/jni-tips).

The stereo/game loop now lives in `src/vr/application.cpp`, linked into
`starfox_vr_game`, rather than inside the desktop diagnostic. `ApplicationHost`
accepts Android context, rendering-thread lifecycle cancellation, and configurable
frame/time limits (zero disables a limit). The desktop tool retains its original
120-frame/30-second limits. Cancellation exits through the existing scoped GPU,
session and runtime cleanup. JNI references remain host-owned until return.
The shared target builds on Windows and its loader-only check passes. Android
Activity wiring, packaging and headset execution remain unfinished; moving the
loop into the library does not establish playable Quest support.

## Quest 3 Android initialization (in progress)

`OpenXrRuntime::initialize` now accepts an Android Java VM, application Context
and Activity bundle. Android initializes the loader through
`xrInitializeLoaderKHR` before enumerating extensions, requires
`XR_KHR_android_create_instance`, and chains the Activity into instance creation.
Missing JNI inputs reject explicitly; desktop rejects an Android bundle.
The caller must retain valid JNI global references through instance destruction.

The desktop VR runtime target rebuilds successfully. The Android branch has not
been compiled with an NDK or exercised on a Quest. The existing Android Gradle
target still launches the flat SDL application: a Quest Activity/native entry
point, lifecycle wiring, packaging and headset verification remain required.
This change alone does not produce a playable Quest APK.

`starfox_vr_runtime_tests` now checks mocked desktop loader initialization,
reinitialization, missing graphics extension, headset/system failure, non-stereo
rejection and exact instance destruction counts. It passes without a headset.
This does not execute Android loader/JNI calls. Toolchain discovery still finds
no configured Android SDK/NDK or JDK on the host.

Runtime initialization also rejects a stereo view count that changes between
the enumeration and retrieval calls, clearing the instance and cached views.
The mocked lifecycle suite covers this failure followed by a successful retry;
all seven created instances are destroyed. This is host-side lifecycle coverage,
not headset verification.

API references: [Android loader initialization](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrLoaderInitInfoAndroidKHR.html)
and [Android instance creation](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrInstanceCreateInfoAndroidKHR.html).

## GPU tile-decoder prototype

The Vulkan textured fragment path now accepts a validated tile payload instead
of pre-rasterized pixels: 16 control words, 256 RGBA palette words and packed
64KiB VRAM. It decodes planar 2/4/8-bpp pixels, tilemap pages, scroll, tile flips,
8/16-pixel tile selection, palette selection and priority on the GPU. These
features are implemented but full-scene parity verification remains open.

The `--tile-suite` diagnostic now covers 288 combinations of 2/4/8-bpp data,
8/16-pixel tiles, all horizontal/vertical flips, four map sizes and three
priority selections, with signed/wrapped scroll samples. It populates planar
VRAM and compares each GPU-decoded sample against the existing CPU BG1/BG2/BG3
decoder. All cases pass in both eyes on Intel Vulkan. Each case samples one
source coordinate across a quad; this is not exhaustive pixel coverage or
proof of exhaustive HDMA, offset-per-tile or final composition. Linux syntax
checking of the expanded diagnostic also passes.

Mosaic cell selection is now implemented before scroll in the GPU decoder.
The 288-case suite passes with disabled, 4-pixel and 16-pixel cells, including
negative logical X coordinates from an inset viewport. Cell sizes above 16
reject during upload validation. This remains targeted sampling, not full
background composition proof.

Per-scanline H/V offsets are now decoded from 224 signed rows per axis appended
to the payload. The 4-bpp cases exercise H-only, V-only and combined offsets,
including mosaic-snapped row selection. The first run caught a mismatch in
the CPU renderer's special widescreen water margins: they extend one bridge
cross-section instead of repeating it. An explicit payload control now retains
that behavior. All 288 cases pass in both eyes on Intel Vulkan
(`tmp/vr-tile-scanline-water-proof`); Linux syntax checks also pass. This is
comparison to the existing CPU renderer, not independent SNES hardware proof.

Control 12 now enables direct Mode 2 vertical-offset reads from VRAM word
0x2fa0. Valid entries replace the scanline/register vertical scroll; invalid
entries retain it. Cartridge-width guard columns use the CPU renderer's
wrapped, signed-gradient extrapolation. Ordinary columns read just their own
table entry rather than scanning the table. The expanded 384-case diagnostic
passed on Intel Vulkan (`tmp/vr-mode2-offset-proof`), followed by expanded
left/right guard and valid/invalid-anchor coverage in both eyes
(`tmp/vr-mode2-guard-proof`). Lower-ground continuation is still pending;
this does not complete Mode 2 presentation.

Control 12 value 2 now performs the widescreen least-squares horizon fit on
the GPU, unwrapping the valid 13-bit offsets before fitting and rounding the
result back to wrapped pixel offsets. The 480-case Intel Vulkan diagnostic
passes for linear source tables (`tmp/vr-mode2-wide-fit-proof`) and quantized,
wrapped tables (`tmp/vr-mode2-quantized-fit-proof`), in both eyes. Truncated
scanline payloads and invalid offset modes preserve the previously uploaded
scene; Linux syntax checks pass. Horizon fitting has now moved to a dedicated
textured vertex entry point, with flat coefficients passed to the fragment
shader. This removes per-pixel regression work without moving it onto the CPU.
The untextured entry point remains descriptor-free; textured storage is visible
to both shader stages. The 480-case suite passes after this migration
(`tmp/vr-vertex-horizon-proof` and final guarded-entry rerun
`tmp/vr-vertex-horizon-final-proof`). EX projected text also passes after the
entry-point change (`tmp/vr-vertex-text-regression`); Linux syntax checks pass.
It is not yet enabled in the full game, and
no game-FPS improvement has been measured or claimed.

The initial Intel Vulkan `--tiles` diagnostic exercises an opaque 4-bpp texel
and palette selection in both eyes; `--tiles-empty` checks transparent zero
texels. Truncated payloads and unsupported bit depths are rejected before
replacing a working GPU scene. This prototype is not yet connected to actual
background layers. Offset-per-tile and full source composition still need
implementation and parity checks. No CPU pixel upload
is being presented as completion of the GPU background migration.

## Background/HUD source-state foundation

`--source-space-bg2` now checks actual LEVEL1_2 BG2 at source tick 58, including
its captured scroll override and palette. It supplements the 224-point grid
with 128 visible-art samples. EX passes all 352 samples in both eyes, 131 of
them coloured (`tmp/vr-space-bg2-visible-parity-ex`); its CPU layer contains
1,817 coloured pixels. This supports sampled decoding parity of the central
pattern, not full-frame SNES-reference parity or wider VR background coverage.
Original also passes all 352 stereo samples with the same coloured-sample count
(`tmp/vr-space-bg2-visible-parity-original`).

Further same-state space ablations isolate the central dense pattern to the
background pass: it remains with `--live-space-without-bitmap` (native text
disappears), and disappears with `--live-space-without-background` while model,
bitmap, dust and HUD remain. Both EX stereo runs pass; left captures inspected
in `tmp/vr-space-no-bitmap-ex` and `tmp/vr-space-no-background-ex`. This identifies
layer ownership but does not establish whether that background's art/expansion
matches the cartridge reference. No gameplay layer was removed.

The diagnostic `--live-space-without-dust` retains identical source warmup and
all other layers while omitting only the generated dust packet. Pixel comparison
against `tmp/vr-space-populated-ex` finds nine changed pixels in each eye
(`tmp/vr-space-no-dust-ex`). The dense central pattern remains without dust;
it cannot be attributed to the new native dust pass. The no-dust left capture
was inspected. This isolates dust's contribution, not proof that all remaining
bitmap/background content is correct or free of duplicate artwork. No gameplay
layer was removed based on the visual suspicion.

`--live-space` now captures actual LEVEL1_2 source state, waiting up to 600
ticks for at least 30 dust quads rather than fabricating a starfield. Original
reaches 31 quads after seven additional ticks; EX reaches 32 after nine, both
with the native 120-point pool. Combined stereo GPU runs pass
(`tmp/vr-space-populated-original`, `tmp/vr-space-populated-ex`); EX's left
capture was inspected. This is real-state composition smoke evidence, not
isolated dust pixel parity or a headset interpolation/performance measurement.

Live dust now uses fractional presentation-camera position and rotation without
copying the full snapshot. Recycled points retain current source positions;
flow/dust-mode changes and camera discontinuities force the current pose.
Original/EX tests pass thirteen fractional camera samples and transition snap
equivalence, alongside existing source-state regressions. Windows runtime
compilation and Linux syntax checks pass. This is not yet headset smoothness
proof; point recycling itself remains on source ticks and grid interpolation
is separate unfinished work.

Dust packet colour selection now has an independent STAR_COLS reference test:
four remaining-point phases times sixteen depth buckets with distinct palette
colours, run against Original and EX ROMs. All 64 cases per cartridge pass,
including the near bucket's empty geometry. This strengthens CPU packet/material
validation beyond the green-only GPU smoke fixture; exact stereo raster coverage
and real starfield transition captures remain unverified.

Point-grid packets are now submitted before models in the live runtime and
combined diagnostic under separate key 0x30000. Integration exposed and fixed
an unintended 420-unit translation inherited from RenderPose's preview default;
both dust and grid now explicitly use zero camera-relative origin. Regression
checks cover all three translation components. The EX combined GPU rerun passes
(`tmp/vr-grid-origin-fixed`) and its left capture was inspected. Ground points
are present but do not solve the missing full-field ground/background coverage.
EX connected-line grids still report unsupported; no dot substitution is used.

`assemble_grid` now emits source ground-grid point billboards: 15x15 lattice,
camera phase, fixed-point transformed row/column increments, Z>256 gate,
12287 depth cap and secondary near dots below 512. Structural EX tests verify
720 fixture vertices, ground/depth constraints and disabled-mode suppression.
The `--grid` Original/EX stereo Vulkan smoke fixtures pass
(`tmp/vr-grid-gpu-original`, `tmp/vr-grid-gpu-ex`). This verifies visible
green-only output, not exact source reciprocal-projection raster rounding.
Connected EX grid lines explicitly reject rather than becoming dots. Live
grid integration, line mode, interpolation and full ground coverage remain.

The `--dust` synthetic diagnostic now runs the dust packet through Vulkan in
both eyes with Original/EX ROMs (`tmp/vr-dust-gpu-original`, `tmp/vr-dust-gpu-ex`).
Six source-pixel billboard quads (near secondary dots included) produce nonzero
green-only output with no pixel texture upload. At this 256x256 wide-FOV fixture,
individual stars can be subpixel, so this is nonblank/colour smoke coverage,
not exact raster coverage or source colour-table validation. The live runtime
and combined diagnostic now submit dust before model packets using a distinct
0x20000 pass key. Camera interpolation, controls viewport handling, actual
starfield captures and ground/grid geometry remain incomplete.

`SourceModels::assemble_dust` now emits untextured eye-facing geometry from
owned source dust points. It uses STAR_COLS, wrapped camera deltas, source view
rotation, near cutoff 256, projected-depth cap 4095, and the nearby second pixel
at (-1,+1). Planet/continue exclusions are retained. Original/EX structural
tests pass for near/far counts, cap, secondary offset and excluded scenes, with
no CPU pixel payload. GPU readback, live integration, per-frame interpolation,
controls-window offsets and ground-grid emission remain pending. Camera-space
transforms in this assembler are still CPU work; GPU billboards are not a claim
that every transformation has migrated.

Dust/grid source state is now owned by each completed VR snapshot: all 511
possible dust points, native 120/511 active count, signed DOTSFLAG and EX's
M_GRIDLINES selector. Metadata reads are RAM-only. Original/EX input/state
tests verify native getter agreement, unchanged serialized state during capture
and unchanged retained dust after later frames. GPU star/grid emission is not
implemented yet; this closes the stereo source-state prerequisite only.

The combined diagnostic now reports shadow owners and camera-relative world
translations. EX's two large near silhouettes are handles 1 and 18 at depths
0.7148 m and 0.312481 m, both at ground-plane Y -0.222643 m
(`tmp/vr-shadow-placement-audit`). CPU `clip_near_line`/`clip_near_polygon`
also clip at source Z zero, not 256; adding a one-metre cutoff would incorrectly
remove source geometry. The narrow bitmap plane reveals these legitimate
near-ground shapes outside its coverage. Ground/grid and dust geometry are
absent from the current VR scene adapter and must be migrated, alongside
background coverage, before judging final shadow presentation. No shadow
suppression or arbitrary near-cutoff workaround was added.

The live layer plane now uses the source focal length (256 pixels) and captured
M_VANISHX/Y plus the 16-pixel bitmap origin, replacing its arbitrary .00625
scale. Eighty-one centre-eye ray-equivalence cases across depths and vanishing
points pass, with invalid-input rejection. Original/EX combined stereo captures
were rerun (`tmp/vr-composite-aligned-original`, `tmp/vr-composite-aligned-ex`);
the EX left capture was inspected. This corrects the source-plane scale but
does **not** resolve coverage outside the original viewport, the out-of-frame
ship/shadow geometry, or stereo-depth alignment of background features. The
current image remains visibly incomplete; these captures are diagnostic proof,
not a playable-VR completion claim.

`--live-composite` now captures the current combined background, model, EX
bitmap, OAM and meter passes with source backdrop colour. Original/EX stereo
readback runs pass (`tmp/vr-composite-original`, `tmp/vr-composite-ex`), and the
left captures were inspected. **These expose an unresolved layout defect:**
the fixed background/HUD plane covers only a small central region while native
model geometry extends outside it; CGRAM-zero pink is visible around it and the
ship/shadow projection does not fit the plane. This is diagnostic evidence, not
visual parity. Aligning source viewport projection and background coverage with
the tracked scene is the next integration requirement. Merely stretching a
desktop screenshot would not complete native VR migration.

Background packets now accept a native guard-column inset (0..128) and an
explicit source-black transparency policy for EX bitmap composition. Inset
clipping preserves logical UVs, so it does not stretch the remaining artwork.
The GPU tests the unfaded palette before discarding black ink: ordinary coloured
ink faded to zero must still occlude lower layers. Control-word validation,
packet clipping/empty/invalid-inset tests and Linux syntax checks pass. These
controls are now enabled for the live EX bitmap in gameplay/training/results,
after models and before OAM/meters. The snapshot gate uses detected cartridge
type rather than the desktop experience-menu default, which direct-ROM runs
do not set. Native dialogue remains visible because VR has no replacement
communication layer yet; desktop-style dialogue suppression would hide it.
Original/EX snapshot immutability and input/state regressions pass.
The actual `--source-ex-bitmap` gameplay snapshot passes 448 stereo CPU/GPU
samples (`tmp/vr-source-ex-bitmap`, three nonblack grid samples). The isolated
left-eye capture was inspected. This is sparse layer coverage, not full pause,
dialogue, results or combined-scene parity. Windows runtime compilation passes.
The 632-case stereo GPU suite passes (`tmp/vr-bitmap-transparency-proof`),
including eight foreground/background overlap cases that distinguish source
black transparency from faded coloured ink. This is synthetic policy coverage,
not a full EX pause/results screenshot comparison.

Live-game clear colour now comes from CGRAM zero and source display brightness,
shared by both eyes instead of the diagnostic red/blue clears. Integer palette
fade precedes sRGB-to-linear conversion when the swapchain is sRGB. All 512
palette/brightness CPU cases and inverse sRGB checks pass; the Windows runtime
build passes. This does not implement fixed-colour math, source window masks or
prove headset output. Non-game triangle/model diagnostics retain eye markers.

The reusable background packet assembler now supplies native-coordinate quads,
screen-enable gating and direct GPU tile payloads. Its EX title BG2/BG3 capture
passes 224 CPU RGB sample comparisons in both eyes
(`tmp/vr-background-packet-ex`). Structural tests cover all three layer enables,
immutable inputs, UV coordinates, brightness/priority payloads and sRGB flags.
The live OpenXR diagnostic now submits BG2 (modes 1..3) and BG3 (mode 1) before
models using the snapshot scroll override and brightness, cached per source
revision and shared between eyes. This is initial integration, not full scene
composition: BG1 bitmap overlays, source priority interleaving, special masks,
wide expansion and physical-headset validation remain pending.

The sprite assembler now retains the native ENEMY label's one-pixel vertical
offset and accepts source meter state for variable-length bar alignment and
inactive-boss suppression. Tests cover ordinary/high-bit bar widths and hidden
meters. Live snapshots now include meters through a RAM-only getter, which is
checked against the native getter on a cloned game while confirming unchanged
serialized state. Original/EX scene-input suites and packet tests pass. The
OpenXR sprite pass consumes this snapshot. Generated meters now use shared
CPU/GPU rectangle layout and a palette-index GPU primitive, submitted after OAM
with depth disabled. Custom layout settings are not yet connected to the VR UI.

Meter coverage passes 128 CPU/packet cases including EX multiplayer/death,
half-width bosses, moved HUDs and clipping. Original's actual gameplay snapshot
passes 448 stereo GPU samples (`tmp/vr-source-meters-original`). EX direct-level
entry was missing the outer boot copies of DOBOOSTMETER and PLAYERB_HP into
Super FX meter memory; these now follow ROM $0f8fdd/$0f906e. The actual EX snapshot
passes 448 stereo samples with width 104 and boost enabled
(`tmp/vr-source-meters-ex-boot-copy`). The separate `--meter-fixture-ex` run is
explicitly synthetic multiplayer/boss coverage, also 448 stereo samples.
`--source-meters` uses actual source state. These grids are targeted samples,
not exhaustive framebuffer or physical-headset proof.

The live `--render-game` OpenXR diagnostic now submits native OAM sprites
after the model pass, without touching scene depth. It builds packets only
when the completed snapshot revision changes, retains identical uploaded
geometry, and shares the resulting layer across both eyes/fence retries.
Its initial LOCAL-space plane matches the offscreen capture transform; final
headset comfort/layout is not verified. Windows builds and Linux syntax
checks pass. A live launch reached the gameplay checkpoint, then stopped at
OpenXR discovery because no active runtime is installed. Backgrounds,
special HUD placement and full playable VR remain incomplete.

The `--live-sprites` offscreen diagnostic now executes model and sprite passes
together from one actual gameplay snapshot. Original and EX both render in
both eyes (`tmp/vr-combined-models-sprites-original` / `-ex`). Original was
compared pixel-by-pixel with model-only and sprite-only captures: all 100/96
visible sprite pixels match in left/right eyes, with zero changes outside
visible sprite ink. The combined Original left capture was visually inspected.
This verifies the tested composition, not a headset run or complete HUD.

The fragment path now has a direct OAM sprite decoder (texture flag 16), using
the shared packed-VRAM/palette format. It implements OBSEL base/name gap,
9-bit character selection, whole-object H/V flips, 8/16/32/64-pixel squares,
transparent zero and sprite palette addressing. Source brightness is applied
on the GPU. Upload validation rejects truncated data, invalid controls and
conflicting tile/sprite flags. The sprite test compares sampled results with
`SpriteRenderer`, including tile-$ff continuation and VRAM wrapping.
HUD customization and live source-snapshot sprite integration are still
pending; a pixel decoder alone does not complete HUD migration.
All 624 background/sprite cases pass in both eyes on Intel Vulkan
(`tmp/vr-oam-tile-proof`), including 128 sprite cases. Linux syntax and
generated-shader freshness checks pass.

`source_sprite_packet` now assembles raw native OAM into an ordered packet with
one shared 66,624-byte palette/VRAM payload. Per-sprite OBSEL/character and
attribute/size fields reside in vertex metadata. It handles source empty/hidden
records, signed X, wrapped Y, native viewport clipping, priority selection,
and reverse OAM draw order. CPU structural tests verify wrapping/clipping,
order, disabled screens, option rejection and immutable input; all 624 GPU
cases pass using the assembler (`tmp/vr-source-oam-assembly-proof`). Special
HUD adjustments and full scene integration are not implemented by this helper.

The `--source-oam` diagnostic now takes a real LEVEL1_1 PPU snapshot after
checkpoint setup and compares the composed sprite result on a 448-point grid.
Original passes in both eyes (`tmp/vr-gameplay-oam-original`), with 10 source
quads and 5 nonblack grid samples; the remaining samples check empty space.
This is targeted placement/overlap evidence, not exhaustive HUD coverage.
The left-eye sprite-layer capture was visually inspected. Linux syntax passes.
EX also passes all 448 samples in both eyes (`tmp/vr-gameplay-oam-ex`), with
12 source quads and 5 nonblack grid samples.

Ordered layer submissions now support disabling depth reads/writes independently
of geometry storage. Switching between model-depth and layer-order policies
retains identical GPU geometry; the stereo overlap regression passes
(`tmp/vr-ordered-layer-depth-proof`). The `--source-bg23` diagnostic composes
actual EX title BG2 then BG3, with source brightness, and passes its 224-point
CPU RGB comparison in both eyes (`tmp/vr-title-layers-ex`). The composite left
capture was visually inspected. This is two-layer composition only: title
model interleaving, BG1, OAM, and live OpenXR background integration remain.
The OpenXR diagnostic builds; Linux syntax checks pass.

`background_tile_payload` is now reusable renderer code rather than a
diagnostic-only conversion. It packs immutable PPU data for BG1/BG2/BG3,
including explicit priority, scroll override and horizontal-extension policy.
The actual-title diagnostics use this encoder. The synthetic suite compares
its controls, packed VRAM and scanline arrays against independently built
payloads before rendering. Screen enables, clipping, layer order
and final scene integration remain caller responsibilities; this is not yet
a completed background compositor.
The shared encoder passes all 480 synthetic stereo cases
(`tmp/vr-shared-background-encoder`) and EX's 224-point BG2 RGB grid
(`tmp/vr-shared-bg2-ex`). Linux syntax checking also passes.

Source brightness is now an explicit encoder option (0..15), passed as
control-word 13 attenuation. The GPU uses integer component scaling before
sRGB conversion, preserving native palette-fade rounding. All 480 synthetic
stereo cases pass with all 16 brightness levels represented
(`tmp/vr-background-fade-proof`), including full black. Title diagnostics now
use the captured display brightness and independently faded CPU palette.
EX's 224-point title BG2 grid passes through that path
(`tmp/vr-source-ex-brightness`); Linux syntax checks pass.
This verifies layer fading, not complete death/title/level transition timing.

Control 14 now preserves tunnel margins: BG2 outside logical X 0..255 returns
the darkest source-palette entry, before tile wrapping or priority filtering.
The textured vertex shader selects that entry with native 5-bit weighted luma;
the fragment shader applies the same brightness and color-space conversion as
ordinary tile pixels. This avoids assuming CGRAM zero is black during EX
transitions. The diagnostic includes both sides of the 0/255 boundary, fades,
priority passes, and a palette containing no exact black. Full scene tunnel
composition is still pending.
All 496 cases pass in both eyes on Intel Vulkan (`tmp/vr-tunnel-border-proof`);
Linux syntax checks and generated-shader freshness checks also pass.

The standalone `--source-bg3` / `--source-bg2` diagnostics now construct tile
packets from an actual immutable title PPU snapshot, including its VRAM,
registers and BGR555 palette. They render the whole isolated layer as a stereo
quad and compare a 224-point RGB grid to the CPU background decoder. Original
BG3 passes in both eyes (`tmp/vr-source-bg3-original`); its left-eye capture
was visually inspected and contains the title logo and copyright. That initial
capture was after two ticks; a subsequent fixture allowed 120 ticks for setup.
Original BG2 also passes all 224 RGB samples in both eyes after that warmup
(`tmp/vr-source-bg2-original`). Linux diagnostic syntax checks pass.
EX's direct-title fixture had no visible grid samples at 120 ticks: BG2 had
57,344 indexed pixels but zero nonblack palette results. At 600 ticks the
source has finished further setup (BG3 map base changes from 11264 to 26624,
nonzero palette entries from 102 to 201). Both BG2 and BG3 then pass all 224
RGB samples in both eyes (`tmp/vr-source-ex-warm600` and
`tmp/vr-source-ex-bg3-warm600`). The BG2 left capture was visually inspected:
EX suffix, star and start prompt are present. The current fixture waits 600
ticks; this fixes capture timing, not game behavior. No forced palette or
tilemap substitution was used. Full layer composition, brightness,
headset placement and live background integration remain unverified.

Each completed-tick VR snapshot now owns an immutable PPU copy: VRAM, OAM,
tilemap/character-base registers, palettes, scroll and scanline offsets, plus
display brightness and the native background-scroll override. Both eyes retain
the same copy; older snapshots cannot be mutated by later simulation ticks.
The copy is not performed per eye.

Capturing the old background-scroll getter initially changed emulated open-bus
state. A new RAM-only `peek_background_scroll_override` avoids that side effect
without altering the native execution getter. Original/EX diagnostics compare
full PPU snapshots with the completed tick throughout stereo pacing and verify
that retained display state remains unchanged. Background/HUD rendering itself
is still absent; this provides its source data, not a completed display pass.

## Projected text/trail objects

Strategy-flag 0x40 objects now generate GPU textured billboards directly from
MARIOMSGS tokens and MSCALECHARS ROM glyphs, without requiring a model shape.
This also covers effects represented through that source projected-text pass.
Messages remain bounded to 256 tokens; invalid glyphs retain spacing, repeated
glyphs reuse packet texels, and zero/uninitialized pointers, nonpositive size
and near-depth objects emit no geometry. Source palette selection and signed
size adjustment are preserved. Geometry is centred as a string, with per-eye
billboarding and GPU texture sampling; object rotation does not rotate the art.

Original/EX game-input diagnostics compare every uploaded MSG_NINTENDO texel
against the cartridge font and verify pointer, size and near-depth gates.
The `--text` Vulkan diagnostic now renders this packet in both eyes. Original
and EX pass the isolated glyph/colour coverage checks on Intel Vulkan; EX
produces 1,138/1,150 lit pixels in the left/right eye. Captures are in
`tmp/vr-projected-text-original/live-scene-{left,right}.bmp` and
`tmp/vr-projected-text-ex/live-scene-{left,right}.bmp`. The Original left image
was visually inspected: the ROM lettering is upright and readable, with
transparent surrounding texels. Eye-space effect masks and headset proof
remain pending. This does not implement the remaining world/HUD layers or make
the VR build complete.

## Native source-shadow traversal

Scene capture now retains PLAYERFLYMODE's shadow-enable bit and SHADOWHEIGHT.
The assembler traverses enabled shadow objects before normal models, decodes
the header's shadow pointer, and uses a separate pass key. Shadow poses flatten
the object's world-Y matrix row before composing with the camera, including
current-tick source lighting. Ordinary shadows use SHADOWHEIGHT and forced
colour 9; true-colour shadow objects retain their own position/material and
do not also draw in the normal pass. Presentation interpolation is shared with
models, while source lighting/depth remain tied to the completed source tick.

Original game-input diagnostics now produce 24 packets instead of 20, and EX
produces 27 instead of 23, with zero deferred draws in both gameplay fixtures.
Explicit fixtures verify
height, flattened matrix, forced colour, shadow-first order, enable gating and
exclusive true-colour shadow placement. The isolated `--shadows` Vulkan
diagnostic now renders the cartridge's MYSHIP_4 shadow geometry alone (no model
pixels can satisfy the check). Original and EX pass on Intel Vulkan; the
Original fixture produces 14/15 green diagnostic pixels in the left/right eye.
`--shadows-off` verifies zero coverage in both eyes when the source enable flag
is clear. Captures are `tmp/vr-source-shadow-proof/live-scene-{left,right}.bmp`,
`tmp/vr-source-shadow-ex-proof/` and `tmp/vr-source-shadow-off-proof/`; the
Original left capture was visually inspected. These are small diagnostic
projections, not photographic proof of full gameplay or headset rendering.
This is not enhanced or ray-traced shadow verification.

## Multi-pass object identity

GPU packet cache keys are now 32-bit: the low 16 bits retain source object
identity, leaving upper bits for separate draw passes. This prepares the source
shadow traversal, which precedes the model traversal and can draw the same
object twice. Exact geometry comparison remains mandatory before reuse.
The Intel Vulkan diagnostic verifies keys 17 and 0x10011 coexist and both
reuse their own geometry when reordered. Duplicate full keys still reject
transactionally. The OpenXR diagnostic builds with the widened keys. This is
a prerequisite used by the source-shadow traversal described above.

## Owner particles

Scene snapshots now own the 300-entry source particle pool. Owner-particle
objects emit Vulkan line geometry for trails and untextured eye-facing quads
for dots, instead of being reported as an unsupported pass. Particle offsets
retain signed-word interpolation, source palette colours, life/owner filtering
and the source 256-unit near-depth gate. Owner rotation is intentionally not
applied, matching the desktop particle renderer. Dot dimensions correspond to
the source two-pixel size at its 256-unit focal length; actual headset pixel
coverage depends on the eye projection. Flow/camera discontinuities snap both
owner and particle interpolation together.

Game-input diagnostics cover pool capture, owner/dead/near filtering, dot and
trail geometry, colour, interpolation and immutable snapshots. Eye-space effect
clipping remains explicitly unsupported. The `starfox_vr_scene_check` diagnostic
accepts `--particles` after its capture-directory/ROM/symbols arguments. Intel
Vulkan readback passes with both Original and EX: each eye contains four green
dot pixels, 57 red trail pixels and no blue pixels from dead/wrong-owner entries.
The deliberately small 256-pixel eye target has a different projection from the
source game; four source dots cover four pixels here, not a claimed 2x2 native
pixel match. Captures are in `tmp/vr-particles-proof/live-scene-{left,right}.bmp`
and `tmp/vr-particles-ex-proof/live-scene-{left,right}.bmp`; the left Original
capture was visually inspected. Headset proof remains pending. This does not
complete the world, HUD or source shadow passes.

## EX reticle palette parity

The native VR model assembler now applies the desktop EX reticle rule: use
palette entry 207 and disable scene colour-warp for the aiming marks. The
override is applied to a local presentation pose, leaving the retained source
snapshot unchanged. The existing intro/non-gameplay exclusion is preserved.
Original and EX Windows game-input diagnostics pass; the EX fixture checks
green material output, colour-warp bypass, snapshot preservation and intro
exclusion. This is packet-level evidence, not headset visual verification.

## Latest: GPU axis-collapse presentation

The native packet path now supports the desktop renderer's near-camera intro
laser collapse rule. It averages the scaled object-local positions belonging
to the animated source frame's minimum/maximum raw Z groups, preserving mixed
byte/word coordinate semantics. Vulkan performs model/eye transforms, clipping
and line rasterization. It uses the first face's colour without sampling its
texture and bypasses polygon/BSP visibility, matching the desktop axis path.
This is the desktop presentation rule, not a claim of newly emulated SNES code.

Packet tests cover endpoint averaging, animation, mixed coordinate scaling and
textured first faces. Real Intel Vulkan readback shows 59 green pixels in each
eye and zero behind-eye pixels; captures are in `tmp/vr-axis-collapse/axis-*`.
Windows packet checks and Linux syntax checks pass. Exploding faces, EX
scanline effects, world/HUD passes and physical headset verification remain.

## Latest: stable object upload reuse

Live scene uploads now use unique object handles to locate cached GPU geometry,
so draw-list insertion, removal and reordering no longer invalidate unchanged
neighbours. Every candidate still requires exact vertex/line/texel equality;
recycled handles with changed geometry upload fresh data. Packet order and model
transforms remain those of the new scene. Optional keys include empty packets;
duplicate or mismatched keys fail transactionally. Unkeyed callers retain the
positional path. The whole-scene transform fast path also checks handle order.

Real Intel Vulkan checks cover insertion, removal, reorder, empty packets,
changed geometry under reused keys, and invalid keys. Both-eye readback checks
exercise reordered cached objects. Windows builds, dummy PCM playback and
Vulkan loader checks pass; Linux source syntax checks pass. This removes
avoidable uploads, not a measured full-game FPS improvement. Headset runtime
verification and complete world/HUD rendering remain outstanding.

## Latest: native PCM device output

The live-game OpenXR mode now sends generated 32 kHz S16 stereo PCM to an SDL
playback stream instead of discarding it. It reuses the desktop queue policy:
64 ms startup headroom and a 150 ms bound, dropping stale queued playback after
catch-up without skipping source SPC updates. Warmup remains silent. Focus
loss pauses and clears the queue; resume primes it without retaining stale
effects. Errors propagate through the diagnostic rather than silently muting.

SDL dummy-device tests pass for 100 rapid blocks, queue bounds, odd/oversized
packet rejection, inactive discard, pause/clear/resume, reopen and repeated
close. Windows diagnostic build and Vulkan-loader checks pass; Linux syntax
checks pass. This verifies stream handling, not audible quality or headset
device routing. Playback uses the system default device. Physical OpenXR/audio
testing and MSU integration remain outstanding. Standalone VR configurations
with `STARFOX_BUILD_RUNTIME=OFF` now require an installed SDL3 >=3.2 package;
normal runtime builds continue using the project's pinned SDL dependency.

## Latest: per-object upload and pipeline reuse

GPU scene replacement now retains unchanged objects independently, using exact
GPU-data comparisons at each nonempty draw-list position. It also retains the
triangle/line pipelines while device, function dispatch and render pass match.
Model transforms remain separate from shared immutable geometry. New resources
are staged before publication; a later object's failed upload cannot mutate
the previous scene. Reuse is currently positional, so insertion/reordering can
still cause otherwise avoidable uploads. Exact comparison retains CPU geometry
copies, trading memory for reliable reuse without hash collisions.

Real Vulkan tests verify one-object changes upload exactly one packet and
reuse its neighbour, including a failure after another packet was staged.
Both translated-eye bitmap hashes remain identical. A two-object sample
measured 56.075 us full rebuild, 11.675 us one-object refresh and 0.07411 us
matrix-only update; these exclude full gameplay/frame costs, not FPS results.
Windows build and Linux syntax checks pass.

The live 90 Hz CPU fixtures identify exact per-object reuse candidates for
2157/2471 Original updates (87%) and 2168/2802 EX updates (77%). Full serialized
simulation states still match the reference loop. These counts predict upload
eligibility, not measured headset performance. Actual object upload/reuse
counters are now available in the live OpenXR diagnostic; physical runtime
execution remains unverified here.

## Latest: transform-only GPU scene reuse

The live OpenXR diagnostic compares exact GPU-consumed vertex/line/texture
data against its uploaded packets. If only model transforms changed, it now
updates those matrices after prior eye fences complete without Vulkan calls,
allocation or uploads. Changes to UVs, visibility fields, texels or topology
invalidate reuse. Invalid matrix/count updates are rejected transactionally.

Real Intel Vulkan checks pass for independently transformed solid/textured
objects, depth and invalid updates. Both translated-eye bitmap hashes match
the prior captures exactly. A two-packet microbenchmark measured 48.1 us for
rebuilding versus 0.06695 us for matrix updates; this excludes game assembly,
geometry comparison, drawing and headset overhead and is **not an FPS claim**.
Windows build/unit checks and Linux syntax checks pass.

The two-second 90 Hz gameplay fixtures retain deterministic state equality.
Exact whole-scene geometry matched on 105/179 Original frame pairs, but only
3/179 EX pairs: a changing sprite can still invalidate the entire scene.
Per-object upload reuse is therefore the next optimization; whole-scene reuse
alone does not solve EX upload cost. Diagnostic reuse counters are printed
after a successful headset run, which remains unverified without a runtime.

## Latest: paced live-game OpenXR model diagnostic

`starfox_vr_runtime_check --render-game ROM SYMBOLS` now boots LEVEL1_1 through
the native SPC handshake and connects controller actions, `GameFrameDriver`,
scene snapshots, interpolation, native packet upload and per-eye recording.
Preparation happens only on a new predicted timestamp after the previous
stereo submissions finish; eye/fence retries reuse the uploaded scene. Focus
loss freezes game time and resets interpolation history so resume does not
replay an older endpoint. The diagnostic requires focused source ticks and
120 completed stereo frames to report success.

Windows build and Linux syntax checks pass. Original and EX startup reach
their checkpoints at ticks 361/370, then exit with the expected missing-active-
OpenXR-runtime error on this machine. Conflicting render modes reject before
opening files. Both paced full-state gameplay tests pass, including a new
focus-resume history assertion. No real headset session has been verified.

This remains a model-layer diagnostic: native SPC PCM is generated for source
handshaking but discarded; audio-device/MSU playback, backgrounds, HUD and other
passes are not integrated. Unsupported model passes stop with an explicit
error. Uploads currently rebuild per frame and still need lifetime-safe reuse
and performance work. It is not a playable or release-ready VR build.

## Latest: source-scene interpolation

`interpolate_scene_poses` and `SourceModels::assemble_interpolated` interpolate
camera/object transforms while retaining current source lighting, LOD depth,
materials and animation state. They reuse the existing recycled-slot identity
checks, EX reticle station matching and camera-float cancellation, upgrade
overlay owner anchoring, discrete UP_DOOR rotations and non-interpolated trail
rules. Camera cuts/flow changes snap to the current state. EX reticles are
suppressed outside gameplay/training. Callers must use the game's logic
interpolation alpha (derived from raster fraction), not raster fraction alone.

Tests pass for half-tick motion, replacement generations, NaN rejection and
13 upgrade-overlay phases (240 FPS). The real Original and EX 90 Hz pacing
tests now assemble interpolated model packets periodically and still match the
independent native loop's complete serialized state. Linux syntax checks pass.
This does not yet prove animated GPU capture or headset frame integration;
the currently documented photographic fixtures remain completed source ticks.

## Latest: live-game model-layer Vulkan captures

`starfox_vr_scene_check OUTPUT ROM SYMBOLS --live` now boots LEVEL1_1 with
native SPC handshaking, waits for the gameplay checkpoint, advances 40 idle
ticks, captures the live game state, assembles its model packets and uploads
the entire model list to Vulkan. It refuses explicitly deferred normal-model
draws. Both eye images use the game's captured palettes and camera-relative
poses rather than the grayscale single-shape diagnostic.

Intel Vulkan results: Original at tick 401 submits 16 packets and produces
1459/1457 lit pixels; EX at tick 410 submits 19 packets and produces 1528/1561.
Both runs pass the existing depth/texture/visibility/billboard controls. Left
eye images were visually inspected. Proof images are
`tmp/vr-live-original/live-scene-left.bmp` / `live-scene-right.bmp` and
`tmp/vr-live-ex/live-scene-left.bmp` / `live-scene-right.bmp`.

This proves the live simulation -> source pose/LOD/material -> native upload
-> stereo GPU draw/readback connection for the model layer. It does not prove
full scene parity: these captures intentionally omit backgrounds, HUD, extra
shadows and other passes. They are frozen completed-tick fixtures, not animated
headset playback, high-FPS interpolation or physical headset validation.

## Latest: whole-object sprite packets

The two EX pending model entries now use native billboard packets. Source
model assembly calculates their diameter with the original signed byte/word
wrapping and header shift. Packet construction shares animated texture lookup
with software rendering, uses the source 128-unit near cutoff and 240-pixel
size cap, stretches non-square art onto the source square destination, preserves
transparent texels and palette overrides, and ignores object rotation. Stereo
eye-facing placement uses the existing GPU billboard shader; no CPU projection
is uploaded as vertex coordinates.

EX LEVEL1_1 now assembles 23 packets with zero deferred normal-model entries.
Full-state deterministic pacing tests still pass. Packet tests cover geometry,
non-square UVs, transparent texels, sRGB flags, rotation independence, near
rejection and the size cap. Windows basic simulation substrate tests pass.
These new whole-object packets have not yet been captured from a live GPU game
scene, and this does not claim exact source screen-pixel rounding in VR.

## Latest: live source poses and model assembly

Scene snapshots now own completed-tick RenderPoses: wrapped camera-relative
coordinates, object/view matrices, source lighting/depth, native frame overrides,
texture scroll, explosion state, EX model modes, vanishing point and per-object
depth tables. RAM capture remains side-effect-free. Source models are assembled
using a persistent decoded-shape cache, native depth-based LOD selection,
hit/special/override colour tables and current CGRAM/model palette words.

`SourceModels` returns ordered packets plus explicit per-object pending reasons.
It covers the normal model pass, not the complete scene: backgrounds, overlays,
extra shadows, reticle/intro presentation overrides, high-FPS interpolation and
dynamic GPU scene replacement are still integration work. Zero pending model
entries must not be interpreted as a complete scene.

The live Original LEVEL1_1 fixture produces 20 packets with no pending normal
model entries. EX produces 21 packets plus two simple-scaled-sprite entries
(objects 6 and 25) awaiting their native GPU stage.
Both retain the existing 90 Hz/40-source-tick deterministic state match;
source camera wrapping, rotations, depth, frame overrides, scroll and explosion
data are checked.
The initial capture and model assembly leave complete serialized game state
unchanged. Linux syntax checks pass. These are CPU assembly tests, not yet
photographic proof of a live game scene submitted to Vulkan.

## Latest: multi-object native GPU submission

`VulkanDrawPackets` owns immutable per-object vertex/line buffers and texture
descriptors, shared triangle/line pipelines and model transforms. It records
an ordered packet list for either eye with no uploads or allocations in the
recording path. GPU fences must finish before successful replacement/close.
Failed initialization keeps the previous uploaded scene intact. Scene-wide
vertex/texture budgets, model validity, topology and texture bounds are checked
before allocation; deferred primitives are not silently discarded.

The Vulkan diagnostic now exercises two separately transformed objects, one
solid and one textured, through this path. Their overlapping green/red depth
results remain 1636/1987 pixels for each eye. Line-only packets and the mixed
ROBOT_0 model also pass (64 triangles, 13 lines, 905/913 lit pixels). Both
ROBOT_0 images in `tmp/vr-multi-packet` match `tmp/vr-lines-robot` byte-for-byte;
the left image was visually inspected. Invalid texture/model replacement
tests confirm that the prior scene survives validation failure. Injected GPU
allocation-failure coverage is still pending.

`--render-model` in the OpenXR runtime diagnostic now uses shared draw-packet
preparation and this submission path. Windows build/loader checks and Linux
syntax checks pass. EX ANDROSS decodes successfully, then headset discovery
stops because no active OpenXR runtime is registered. This is not headset
rendering proof or playable VR: live snapshot-to-pose/LOD assembly, dynamic
scene updates, remaining primitives, backgrounds/overlays and audio integration
are still required.

## Latest: native draw packets and shared source shading

`build_draw_packet` prepares an explicitly selected source shape/LOD and full
RenderPose for native GPU submission: animated object-local geometry, source
byte/word scaling, material/texture/line/sprite batches, and a metre-space model
matrix. Both eyes consume the same packet. It is now used by the real Vulkan
offscreen cartridge diagnostic, not only a synthetic unit test.

Software rendering and packet preparation share `source_shading`: source-tick
lighting matrix, row-oriented Q15 light transform and depth-band boundaries.
Changing interpolated geometry does not change the source lighting state.
Invalid transforms/scales and still-unsupported primitive/effect paths reject
transactionally. Axis-collapse was also added to the low-level batch rejection
guard; it previously could silently render a normal solid instead.

Windows packet tests, Original cartridge simulation substrate tests and Linux
syntax checks pass. Intel Vulkan captures of
Original MYSHIP_4 and EX ANDROSS pass the depth, visibility, line, texture,
dither and billboard controls. Captures are in `tmp/vr-packet-arwing` and
`tmp/vr-packet-andross-ex`; these use diagnostic grayscale, not in-game palette
parity. Both Arwing bitmap hashes match `tmp/vr-game-pose` exactly. The EX
Andross image was visually inspected. No headset was used.

Live snapshot-to-complete-pose/LOD assembly, multi-object GPU submission,
special effects, backgrounds/overlays and physical runtime validation remain.

## Latest: stable live scene capture

`GameSceneHistory` now captures completed logic ticks from the paced driver:
native draw-list order, owned object data and slot generations, the shared
presentation transforms, camera/view matrix, camera float, frame counter,
CGRAM/model palettes, model scale and colour-table override. Snapshots are
immutable shared objects, so an eye or pending GPU submission can retain one
while the simulation advances. Both the previous and current source ticks
remain available for interpolation, including after multi-tick catch-up.

Determinism testing exposed that the existing CPU `read8/read16` methods
change open-bus state even through const interfaces. New RAM-only peek APIs
avoid bus and I/O side effects; the three model palette/scale/colour getters
now use them. Scene capture uses these peeks rather than emulated CPU reads.
RAM aliases, cross-bank words, and rejection of I/O/ROM/unmapped boundaries
are tested, with complete serialized CPU state unchanged before/after peeks.

Original and EX live-game tests pass: 90 Hz presentation still matches the
independent native loop at 120 rasters/40 logic/audio ticks over two seconds.
They verify ordered object copies, generation identity, palettes, retained
snapshot immutability, duplicate-eye identity, focus resume and catch-up history.
Linux GCC syntax checks pass for the scene/driver/integration test sources.
Windows simulation substrate tests pass both standalone and with the Original
cartridge fixture (including its recovered-path diagnostic warning).
This is a live data producer, not yet full scene-to-Vulkan draw submission;
material/LOD packet creation, interpolation consumption, backgrounds, overlays,
audio-device output and physical headset validation remain outstanding.

## Latest: paced simulation frame driver

`GameFrameDriver` advances an existing game from predicted headset timestamps
through the native 60 Hz video loop and `logic_tick_ready()` gate. It does not
force a gameplay pace. Input transitions remain latched until a logic tick;
APU and MSU event streams accumulate until the independent 20 Hz audio
callback returns the native output ports. The caller owns audio playback,
game lifetime and source face-count metadata used by Original pace.

Repeated timestamps (including the second eye or fence retry) do not advance
input, simulation or audio, and return the same interpolation fraction. Focus
loss freezes source time and clears input; resume rebases without catch-up.
Clock reversal rebases safely, and long frames use the existing 250 ms cap.
A callback/tick exception latches failure so a partially completed tick cannot
be retried as though nothing happened.

The driver is implemented and tested independently of graphics; it is not yet
connected to live headset scene production. Native audio-device output/MSU
playback is the caller's integration work, not completed by this class.

Original and EX checks pass against a separately driven native raster loop at
90 Hz headset cadence in unlocked timing: 120 video phases, 40 logic ticks and
40 audio blocks over two seconds. Serialized states are compared periodically;
duplicate-eye interpolation, pause/resume, bounded catch-up, clock reversal
and rejection of retries after an injected audio failure pass. This does not
claim physical headset testing or Original-pace face-count parity.

## Latest: native gameplay input adapter

`VrGameInput` maps controller state to the game's existing SNES button bits
and `InputLatch`, preserving pressed/released transitions across repeated
presentation polls until a simulation tick consumes them. Default type-A
mapping is Y/fire, A/bomb, X/boost, B/brake, L/R roll and Start/menu; steering
becomes directional pad input. Native control type/inversion remains in the
game, not duplicated by the adapter. Menu requires an accepted press edge,
so a button held at focus regain cannot turn into a fresh Start.

Left/right roll actions are now bound to the respective Touch stick clicks.
Injected tests cover every mapped bit, holds, quick taps between ticks,
release/reset and Start suppression. The live headset diagnostic still only
polls state; wiring a paced game simulation and its scene into that loop is
unfinished. Controller remapping/change-view controls and physical hardware
validation remain open.

The new `starfox_vr_game_input_check ROM SYMBOLS` drives two real simulations,
including their SPC handshakes, from VR-mapped versus raw pad input. It waits
for the native stage checkpoint (361 ticks Original, 370 EX), then compares
complete serialized game states after each of 40 controlled ticks with twelve
input polls per tick. Both cartridges pass; actual player X ranges are
-382..-3 and -384..-5 respectively. Earlier fixed-warmup attempts were rejected
because the player never moved. The test now requires a ready checkpoint and
player motion, not merely equal inert states. This proves the adapter's
simulation equivalence, not physical controller or headset gameplay.

## Latest: controller actions

`OpenXrInput` creates and attaches steering, fire, bomb, boost, brake and menu
actions before the session begins. Suggested bindings use the pinned OpenXR
registry's Simple Controller and Oculus Touch paths. Touch uses left stick,
right A/B, left X/Y and left menu respectively; Simple Controller exposes
select/fire and menu only. Other controls/profiles, remapping and roll actions
still need implementation. Unsupported profiles do not abort initialization.

The tracked-eye diagnostic samples once per predicted frame (not twice per
eye), records menu press edges and clears controls when unfocused. This does
not yet route controller state into gameplay. Radial deadzone, finite/clamped
axes, inactive actions, neutral state on read/sync failure, focus loss and
held-menu suppression at startup/reconnect are implemented. Partial setup
destroys the action set and child actions. Reattachment needs a fresh session,
as required by OpenXR.

Windows build and injected tests pass, including failed creation/attachment,
unsupported profiles, focus transitions, menu holds and invalid axes. No
physical controller or active headset runtime has been validated locally.
Linux GCC syntax checks pass for the action module, its tests and the updated
runtime diagnostic; this is not Linux runtime/controller validation.

## Latest: game pose conversion

`game_model_matrix` converts the game's camera-relative `RenderPose` into the
native renderer's right-handed world coordinates and metre scale. It accepts
source Q15 rotation matrices or the source pitch/yaw/roll order. Object scale
and header shifts remain in mesh decoding so word-coordinate vertices retain
their required bypass; the pose matrix never reapplies that scale.

Both OpenXR model placement and the offscreen cartridge fixture now use this
conversion. Unit tests cover coordinate axes/translation, Q15 column order,
combined Euler rotations, scale separation and invalid input. Intel readback
keeps the Arwing's 425/418 lit eye pixels; both eye captures are SHA-256
identical to the prior placement. Original/EX mesh tests and Linux GCC syntax
checks pass. This
implements continuous geometry, not Super FX per-product integer rounding.
The live game snapshot/simulation producer is not yet connected.

## Latest: cartridge models in the OpenXR loop

`starfox_vr_runtime_check --render-model ROM SYMBOLS SHAPE` now uploads a
decoded model and submits it through the actual tracked-eye callback. It uses
the native polygon/line pipelines, texture storage and sprite billboards,
with shared immutable resources and per-eye transforms. The model is centred
roughly two metres away with normalized size and a diagnostic grayscale
palette. Both sRGB and UNORM swapchain formats are handled. GPU fences finish
before eye-image release; resources outlive pending submissions.

Windows builds pass. ANDROSS decodes successfully, then live startup stops
at the confirmed missing active OpenXR runtime; no headset frames are claimed.
Loader-only, injected session/swapchain tests and malformed/conflicting mode
checks pass. A headset/runtime is required to verify the 120-frame diagnostic.
This is still a static model diagnostic, not game simulation or playable VR.
Linux GCC C++20 syntax checks also pass for the model diagnostic, native
pipeline/buffer/texture resources, shape batching and BSP membership modules.
That is compile coverage, not Linux linking or headset runtime validation.

## Latest: sprite-face billboards

Texture-backed one-point faces now become six GPU vertices sharing the
object-local centre. The vertex shader applies the model transform to that
centre and expands the art toward each eye, retaining model-to-world unit
scaling without rotating the quad. Source texture width determines its extent
(64px sheets use the source's doubled scale). Sprite UVs clip at the texture
bounds instead of wrapping; palette, transparency and visibility are shared
with textured polygons. Non-textured sprite faces remain explicitly deferred.

Physical Intel Vulkan tests pass in both eyes: 3660 pixels normally and with
the model turned edge-on, zero behind the eye, and 1830 when half the sprite
lies outside a non-square texture. Original/EX `LFDIE` conversion tests pass;
the EX live fixture renders 3336/3343 lit pixels with its cartridge texture.
The Original fixture also rendered those counts before the non-square bounds
guard; it has not been recaptured after that guard. Shader freshness, native
resource tests and the runtime diagnostic build pass.

![EX LFDIE sprite, left eye](../tmp/vr-sprite-lfdie-ex/arwing-geometry-left.bmp)

These remain native diagnostic draws, not a completed game-loop/VR port.
Exact source 8.8 depth increments, centre rounding, the source 32-unit near
threshold and nonuniform model-scale semantics still require integration
parity checks. Simple-scaled-sprite strategy objects are a separate path and
remain unimplemented here.

## Latest: native textured faces

Textured faces now carry affine UVs, source mask wrapping, scroll offsets,
palette-resolved texels and index-zero transparency into the native GPU path.
Shared textures are uploaded once per shape batch. The optional fragment
shader samples a read-only storage buffer; transparent fragments discard
before writing depth. sRGB attachments receive linearized colours. Flat
materials retain the descriptor-free path.

Both-eye Intel Vulkan readback passes opaque sampling, transparent holes
revealing later-drawn geometry, negative/positive wrapping, and mid-gray sRGB
decoding. Source-batch tests cover shared texture reuse, palette-base wrapping,
opaque black versus transparent index zero, scrolled UVs, and transactional
rejection of malformed dimensions. Original and EX `ANDROSS` both render
four triangles with the cartridge face texture (17812 lit pixels per eye,
neutral diagnostic palette). This is native shader sampling, not a CPU-raster
image placed in VR.

![Original textured Andross, left eye](../tmp/vr-texture-andross-face/arwing-geometry-left.bmp)

Full source pixel parity remains unproven: continuous affine interpolation
does not reproduce the cartridge's fixed-point scanline rounding, and native
GPU near clipping differs from the source's texture rejection at the near
plane. Sprite billboards, special effects, main-game integration and real
headset validation remain unfinished. Earlier entries below are historical.

## Latest: native model lines

Two-point model faces now generate separate object-local line batches, with
their source materials and face/BSP visibility. The Vulkan pipeline supports
line-list topology with GPU transforms, clipping, depth and per-eye projection.
The upload buffer is topology-independent; each drawing pipeline rejects
incomplete primitives. No CPU projection or line rasterization is used here.

Physical Intel Vulkan checks pass for a 61-pixel horizontal line in each eye,
and zero pixels when its BSP group is hidden. The Original and EX `ROBOT_0` fixtures
now renders all 64 triangles and 13 lines (905/913 lit pixels) instead of
rejecting its deferred lines. Injected topology/count checks and geometry
tests pass. Line width is currently one target pixel; original Bresenham
pixel parity, scaled presentation and coplanar polygon/line ordering are not
yet verified. This still belongs to the native diagnostic renderer, not the
main game loop. Sprites and textures remain deferred.

![Mixed polygon and line model, EX left eye](../tmp/vr-lines-robot-ex/arwing-geometry-left.bmp)

## Latest: BSP membership on the GPU

The native scene shader now combines each face's authored visibility plane
with its BSP batch's visibility plane, evaluated separately for each eye.
Object-local batches retain all potentially visible groups. Empty source BSP
end commands are decoded as explicit terminal leaves; malformed graphs are
rejected transactionally, including cycles and excessive shared-subgraph
expansion. Deferred lines, sprites and textures retain their group identity.

Windows tests pass for three real BSP models (`ROBOT_0`, `BOSS_H_2`, `MY_DEMO`)
from both Original and EX cartridges. Physical Intel Vulkan readback passes
visible/hidden/tangent group controls in both eyes, alongside the previous
face visibility, depth, transforms, dither and Arwing checks. Shader-generated
header verification, resource tests and simulation substrate tests pass.

The diagnostic accepts an optional shape name after `rom symbols`. Live
`BOSS_H_2` rendering passes with 34 triangles and 1375/1373 lit eye pixels for
both Original and EX; `MY_DEMO` passes with 56 triangles and 1153/1150 pixels
for Original. These use a neutral diagnostic palette, not game colour parity.
`ROBOT_0` batch conversion passes but full drawing is explicitly rejected
because it contains deferred primitives; it is not counted as rendered proof.

![BSP boss fixture, left eye](../tmp/vr-bsp-boss/arwing-geometry-left.bmp)

This implements membership, **not source BSP painter ordering**. Depth testing
handles opaque geometry; coplanar ordering and transparency still need parity
work. Native scene rendering remains a diagnostic path, not the main game
renderer or a playable headset port. Older entries below are a chronological
implementation record and may describe stages subsequently completed.

`STARFOX_BUILD_VR=ON` enables an opt-in, pinned OpenXR loader and the
`starfox_vr_runtime_check` diagnostic. Ordinary builds remain unchanged.

Configure a separate build directory with `-DSTARFOX_BUILD_VR=ON`, then build
the `starfox_vr_runtime_check` target. Run it with the headset connected and
its OpenXR runtime selected. It checks Vulkan binding support, creates an
instance, finds the headset and reports its two recommended eye sizes.

The diagnostic builds with the Windows MinGW toolchain, including checkouts
whose paths contain spaces. This development machine currently has no active
OpenXR runtime, so headset discovery and rendering cannot be validated here.

`starfox_vr_session_check` builds and tests the new session controller through
an injected OpenXR dispatch table. `OpenXrSession` accepts a caller-owned,
runtime-compatible graphics binding, creates identity LOCAL tracking space,
handles READY/STOPPING/restart/exit/loss events and follows wait/begin/end frame
ordering. It locates the two views at the runtime's predicted display time.
Invisible frames or invalid tracking submit no layers. Partial initialization
and begun-frame failures clean up in order. The caller still owns swapchains,
GPU synchronization and completed composition layers.

Windows MinGW session tests pass, including double-begin/end rejection,
predicted-time propagation, tracking/invisibility suppression, failed view
location, session restart and cleanup rollback. These are deterministic
injected tests, **not headset validation**. The live runtime check was rerun
and still reports no active runtime in the Windows registry.

`starfox_vr_swapchain_check` now validates the separate stereo swapchain
controller: renderer-preferred supported formats, per-eye recommended sizes,
timeout retries without reacquisition, release gating, copied poses/FOV,
projection availability only after both eyes complete, invalid runtime image
indices (including retries), and partial-creation rollback. Windows MinGW
build and injected tests pass. Native image import and GPU synchronization
still belong to the graphics integration; these tests do not render to a headset.

This is **not a playable VR port**. Graphics-device integration,
native eye-image rendering, actual rendered frame submission, action input and game
rendering remain. The session controller is implemented but not connected to
the desktop rendering loop yet.

`starfox_vr_camera_check` validates the per-eye camera conversion for the
planned Vulkan rendering path: inverse tracked pose, metres-to-scene-unit
translation, asymmetric left/right/up/down FOV, Vulkan Y orientation and
zero-to-one depth with finite or infinite far planes. Tests check both eye
positions produce stereo disparity, a rotated/translated eye maps correctly,
and invalid inputs are rejected. Windows MinGW camera/session/swapchain
checks pass. The camera helper is not yet connected to game geometry or a
headset renderer; matrix tests do not establish playable VR.

`VulkanEyeTargets` owns native Vulkan image views, per-image framebuffers and
a clear/store color render pass for the two runtime-owned eye swapchains.
It preserves runtime ownership of the images, uses each eye's recommended
dimensions and leaves attachments in color-attachment layout. Its caller must
complete GPU work before destruction. All framebuffers are destroyed before
views and the render pass, including partial initialization failures.
The `--graphics` diagnostic now creates these targets after image enumeration.
Windows builds and injected resource tests pass (including framebuffer
allocation failure rollback and idempotent cleanup); actual driver/headset
target creation remains unverified without an active OpenXR runtime. This
does not yet include native draw commands, depth buffers or scene rendering.

`VulkanEyeCommands` adds a native primary command buffer, clear/render-pass
recording callback, queue submission and nonblocking completion-fence polling.
It refuses another submission while an eye is in flight. Callback failure
does not submit partially recorded commands; failed submissions/fence errors
require teardown instead of silently treating the image as reusable. Cleanup
waits for the queue before freeing in-flight command resources.
Windows injected tests pass for command submission, pending-fence gating,
completion, callback failure/retry and pending-work cleanup. This command
path still needs asynchronous integration with `StereoRenderer`, graphics
pipelines/game geometry, and physical Vulkan/OpenXR validation. It is not
yet used by the game and does not establish rendered headset output.

Asynchronous integration is now implemented through `step_async` and
`VulkanStereoDraw`: a pending fence retains the same acquired image, no draw
is submitted twice, and release happens only after completion. Uncertain GPU
errors latch a teardown-required state without returning the image early.
Windows injected session and command tests pass, including pending retries,
stereo completion and fatal-error ownership retention.

`starfox_vr_runtime_check --render-clear` exercises the full native path on
a configured headset: it attempts 120 stereo frames (dark red left eye, dark
blue right eye), using per-eye swapchains and fenced GPU submissions. The
loop has a 30-second wall-clock check, although OpenXR runtime frame waits
can themselves block. This is a diagnostic, not game rendering. The local
attempt still fails at OpenXR discovery because no runtime is configured;
no physical eye rendering is claimed. Graphics pipelines, depth and game
geometry remain to be implemented.

## Native flat-color scene pipeline

`VulkanScenePipeline` now provides actual vertex/fragment shader stages,
triangle-list graphics-pipeline creation and draw-command recording. Vertex
positions/colors come from a caller-owned GPU buffer; per-eye column-major
view/projection matrices occupy 128 bytes of push constants. The vertex
shader performs both camera transforms on the GPU. Colors are flat shaded,
with dynamic viewport/scissor matching each eye target. Depth, textures,
vertex-buffer upload and the game's geometry adapter remain unfinished;
this first pass does not establish complete scene rendering.

DXC compiled both SPIR-V shaders, the source fingerprint check passes, and
Windows pipeline/resource tests pass with injected Vulkan calls. The graphics
diagnostic now creates the pipeline, but actual driver creation and rendered
triangle output still require validation. Regenerate shaders with
`python tools/generate_vr_shaders.py --dxc <path-to-dxc>`; use `--check` to
check the committed generated header against the HLSL source.

`VulkanSceneBuffer` now uploads immutable triangle batches to host-visible
GPU vertex memory, preferring coherent memory and explicitly flushing the
whole mapped allocation otherwise. It validates triangle counts and finite
vertex data and cleans up failed maps/flushes. Both eyes share the same batch;
the caller must complete GPU use before replacing or destroying it.
Windows injected tests verify exact copied bytes, coherent/noncoherent paths,
flush-failure rollback and missing compatible memory. This is not yet the
dynamic game-geometry upload path.

`--render-triangle` now connects upload, graphics pipeline, per-eye camera
transforms, native draw commands and fenced stereo submission in the headset
diagnostic. It draws a green triangle two metres forward in LOCAL space for
120 submitted stereo frames. It builds on Windows, but actual visual output
has not been verified because this machine has no active OpenXR runtime.
Depth, textures and the live game's geometry adapter remain incomplete.

## Real GPU stereo readback

`starfox_vr_scene_check [capture-directory]` creates a standalone Vulkan
device and offscreen target, uses the actual scene shaders/upload/command
classes to draw each eye, reads pixels back and checks green triangle coverage
and correctly signed stereo disparity. It does not require or emulate an XR
runtime. Windows Intel(R) Graphics passed: 1,636 green pixels per eye; X
centroids 129.932 (left) and 125.068 (right), 4.864 pixels apart.
This verifies actual driver pipeline creation, uploaded vertex data, GPU
camera transforms, rasterization and readback—not headset composition.

![Native GPU left eye](../tmp/vr-native-triangle/left.bmp)
![Native GPU right eye](../tmp/vr-native-triangle/right.bmp)

Both readback images were visually inspected: upright green triangles on a
black background, horizontally displaced in the expected stereo direction.

### Depth-tested visibility

Eye targets now accept caller-owned per-eye depth views; the render pass
clears depth to 1 and the scene pipeline optionally enables LESS testing and
depth writes. Callers must serialize reuse of each depth view and destroy
framebuffers before the views. The native offscreen check queries a supported
D32/D16 format and draws a near green triangle followed by a larger far red
triangle. Intel Graphics preserves 1,636 green + 1,987 red pixels in each eye;
stereo disparity remains 4.864 pixels. A depth-disabled control deliberately
produces zero green + 3,623 red pixels, proving the check detects painter-order
occlusion. Native runs and existing injected tests pass.

![Depth enabled](../tmp/vr-native-depth/left.bmp)
![Depth-disabled control](../tmp/vr-native-depth/no-depth-left.bmp)

These GPU readbacks were visually inspected. The headset diagnostic still
needs its own depth-resource allocation; the live game geometry/textures and
headset rendering validation remain unfinished.

The headset depth-allocation gap above is now implemented:
`VulkanDepthTargets` selects supported D32/D16 depth, checks eye dimensions,
prefers device-local memory, and owns a separate image/view/allocation per eye.
Both `--graphics` and `--render-triangle` use these targets and enable depth
testing. The standalone real-GPU test uses the same owner and checks that eye
views are distinct. Windows Intel Graphics readback passes with identical
left/right bitmap hashes to the previous verified depth test, including its
depth-disabled control. The headset diagnostic builds, but hardware XR
composition remains unverified without an active runtime. Game geometry,
textures and a playable VR loop are still unfinished.

## Cartridge geometry bridge

`decode_shape_mesh` now converts decoded cartridge shapes into object-local
float vertices without CPU projection or camera-dependent culling. It selects
the requested animation frame, applies header/object scale only to byte
coordinates, and preserves word-coordinate vertices unscaled. Face indices,
material IDs, normals, visibility IDs and sprite/line metadata are retained
for subsequent rendering stages. Invalid geometry fails transactionally.

Windows unit tests cover mixed word/byte scales, animation wraparound,
metadata retention and malformed inputs. Real Original and EX `SHIP_4` ROM
fixtures both pass with 38 vertices and 39 faces preserved. This is an asset
bridge, not yet a rendered Arwing: source material resolution, GPU model
poses, texture/sprite/line emission and live scene submission remain.

Per-object draw transforms are now available through `record_model` and
`model_eye_camera`. Only the small view/model matrices are composed on the
CPU; object-local vertex positions stay in their existing GPU buffer and are
transformed in the vertex shader. Projection is unchanged, and nonfinite or
non-affine model transforms are rejected. Matrix tests cover noncommuting
rotation, nonuniform scale and translation against independent evaluations.

The live Intel Graphics readback test additionally verifies unchanged-buffer
draws: +0.2 world-X moves the triangle 15.211/15.2173 pixels right in the two
eyes; half X/Y scale reduces green coverage from 1,636 to 394 pixels per eye.
Baseline stereo/depth controls still pass. Captures were visually inspected:

![Translated model](../tmp/vr-native-model-transforms/translated-left.bmp)
![Scaled model](../tmp/vr-native-model-transforms/scaled-left.bmp)

The cartridge geometry-to-material draw adapter and live game scene hookup
remain unfinished; this proves GPU model-transform support, not playable VR.

## Shared source material resolution

The desktop renderer's face-material resolver is now reusable through
`render/face_material.hpp`, retaining animated material words, texture
references, alternating palette nibbles, diffuse/depth tables, forced colors
and palette overrides. The existing software renderer uses this same function.
The extraction also fixed an unsafe `optional::value_or` fallback that indexed
the color table even when a descriptor override was supplied; overrides now
work with an empty/out-of-range table. Depth-band input is bounded to the four
source tables. The GPU geometry adapter still needs to consume this API.

Windows shared-material tests, simulation substrate tests and raster-command
replay parity tests pass after extraction. The material tests include the
empty-table override regression, animation wraparound, texture identity,
COLSMOOTH, palette/forced overrides and diffuse/depth shading.

The native scene vertex/fragment pipeline now carries both source palette
colors and logical dither scale. `apply_scene_material` converts a resolved
`FaceMaterial` and palette into that vertex data, preserving XOR checkerboard
selection rather than averaging colors. It handles sRGB attachment conversion
explicitly and refuses textured materials until the texture path supports
them. Upload validation also checks the alternate color is finite.

Real Intel Graphics readback passes exact per-pixel XOR checks at scales 1
and 2 in both eyes (1,636 covered pixels each, with zero wrong-pattern pixels).
Scale 2 gives 818 green and 818 blue pixels per eye. Baseline stereo, depth,
translation and scale tests remain green; material/resource unit tests pass.
The scale-2 capture was visually inspected:

![Native palette dithering](../tmp/vr-native-dither/dither2-left.bmp)

This establishes the resolved-material GPU interface. A complete cartridge
draw adapter, textured/sprite/line rendering and live game integration remain.

## Cartridge face batches and live GPU model proof

`build_shape_batch` now combines decoded object-local geometry and the shared
source material resolver into GPU triangle vertices, retaining source face
ranges and visibility IDs. Lines, sprites and textures are explicitly deferred;
special presentation modes reject conversion instead of silently losing their
effects. Tests check conversion and transactional rejection. Original/EX
`SHIP_4` fixtures produce 52 triangles across 39 material face ranges. Note:
`SHIP_4` is not the player's Arwing; earlier conversational identification of
that fixture as an Arwing was incorrect.

The live GPU diagnostic separately decodes the actual `MYSHIP_4` player model,
identified in `PSHAPES.ASM` and player strategies. With a neutral grayscale
diagnostic palette and pitched presentation pose, Original and EX each render
20 triangles, yielding 425/418 nonblack pixels in the two eyes. Both live Intel
Graphics runs and all prior stereo/depth/dither controls pass. These captures
were visually inspected:

![Original player geometry](../tmp/vr-arwing-original/arwing-geometry-left.bmp)
![EX player geometry](../tmp/vr-arwing-ex/arwing-geometry-right.bmp)

This is actual decoded geometry through native Vulkan, not game-palette or
visibility parity proof. Source visibility rules, textured/sprite/line stages,
and live scene integration remain required before the VR game is playable.

Per-face source visibility triples are now encoded in the vertex batch and
evaluated after the GPU model/view transform. The shader uses the continuous
camera-space determinant rule (including its tangent tolerance), not generic
triangle-winding culling; hidden faces discard before color/depth writes.
Malformed triple indices fail batch construction. Tests verify byte/word
coordinate scaling also applies correctly to visibility vertices.

Intel Graphics pixel readback passes visible, reversed and tangent controls
in both eyes: 1,636 / 0 / 1,636 green pixels respectively. Arwing geometry and
prior stereo/depth/dither controls still pass. This is per-face coverage only:
BSP-node conditional face batches remain, and extreme near-tangent float
precision has not yet been compared exhaustively with the CPU double path.

Android additionally needs loader initialization through its platform context;
the current diagnostic does not establish standalone Quest support.

## Runtime-selected Vulkan device

`VulkanDevice` creates its instance and device through
`XR_KHR_vulkan_enable2`, rather than assuming the desktop GPU is suitable.
It negotiates the loader/runtime API range, selects the runtime's physical
device, checks its API support and chooses a graphics/compute queue. It exposes
the resulting graphics binding for session creation. Vulkan-Headers are pinned
to v1.4.313; Vulkan entry points are supplied by the caller's loader.

`starfox_vr_device_check` passes on Windows MinGW, covering adapter selection,
version negotiation, queue selection, reinitialization, idempotent destruction,
incompatible versions, missing queues/dispatch and failed device creation.
Late initialization failure waits and destroys the device before the instance.
The session, swapchain and camera checks also pass after this addition.
These are injected tests, not physical GPU/headset validation. The device is
not yet connected to the game loop. SDL 3.4.14's
public GPU API does not expose the native device handles/creation callbacks
needed here; the VR rendering path therefore needs explicit Vulkan integration.

## Live graphics setup diagnostic

`starfox_vr_runtime_check --graphics` now loads the installed Vulkan library,
discovers OpenXR, creates the runtime-selected Vulkan device and graphics
session, creates both eye swapchains and enumerates their Vulkan image handles.
It validates non-null images and reports each eye's image count and format.
It does not acquire, draw or submit frames. Local RAII ordering destroys
swapchains, session, device, XR instance, then the Vulkan library.

`--loader-only` checks the actual system loader and Vulkan instance entry point
without needing an OpenXR runtime. Windows uses the system directory's loader;
Linux uses `libvulkan.so.1`, Android `libvulkan.so`.

Windows MinGW build and the live loader-only check pass. `--graphics` reaches
OpenXR discovery but returns the explicit no-active-runtime error on this
machine; real session/image creation is **not verified**. Linux syntax checks
pass for the loader, device and diagnostic; that is not Linux runtime proof.
## Source no-op faces

Empty/single-vertex non-sprite faces and sprite faces without a texture are
explicitly recorded as `source_noops`, not unsupported rendering work. The
software renderer skips them, so they must not cause VR to reject an otherwise
renderable model. Missing vertex references remain invalid, and unsupported
explosion/EX effects still fail rather than silently losing geometry.

Windows packet tests compare software pixels before/after adding these faces
and verify that the VR packet retains its valid triangle. Original and EX mesh
fixtures (SHIP_4, ROBOT_0, BOSS_H_2, MY_DEMO and ANDROSS) pass; Linux syntax checks
pass. This corrects packet admission, not the pending explosion renderer.

## Two-vertex line materials

VR line faces now use the source material's decoded colour, even when its
descriptor resolves to texture art. The desktop source path draws these as
coloured lines, not textured primitives. Texture validation/upload remains
required for actual sprite and polygon faces.

The Windows packet regression uses a texture descriptor with missing texels:
the software reference produces a white line, VR emits two matching untextured
vertices and no texel upload, and an actual sprite using that malformed art is
still rejected. Linux syntax checks pass. This is material/packet evidence,
not a new headset capture or completion of the pending EX scanline effects.

## Partial audio-block focus regression

The cartridge pacing test now interrupts focus after one and two raster phases
of a three-phase audio block, then resumes after a ten-second wall-clock gap.
Game/SPC state remains unchanged during loss/regain; four resulting audio blocks,
their PCM and complete final game/SPC states match uninterrupted playback.
Unfocused Fire/Menu presses are ignored. Original and EX pass on Windows, and
the expanded test compiles under Linux syntax checking. This verifies source
timing/queue preservation, not physical headset audio-device routing.

## Live diagnostic MSU audio

`--render-game ROM SYMBOLS --msu PACK` optionally loads an existing MSU pack.
Without it, the live diagnostic retains native SPC music. Both paths now use
the desktop's shared stem mixer; SPC continues ticking while MSU supplies music,
and source MSU register writes are consumed during preroll and live playback.
Missing/unavailable packs fail explicitly before starting a graphics session.

Windows and Linux fixture tests compare mixed PCM against the previous desktop
operations, including native selection, MSU loops, stop and failed replacement.
Windows dummy-device tests cover signed rounding, clipping, volumes and queue
lifecycle. The diagnostic and desktop translation unit compile on Windows;
actual headset playback and soundtrack-pack integration remain unverified.

## Aborted stereo frames

`OpenXrSwapchains::cancel_frame` suppresses the projection layer and returns
acquired eye images after the caller has completed any GPU work. Pending
waits and failed releases retain ownership for a later cancellation retry;
starting another frame remains prohibited until both images are returned.
It never reacquires an image during cleanup. Injected lifecycle tests cover
timeouts, release failures, rejected runtime indices, and cancellation with
both eyes acquired. This is lifecycle coverage, not headset rendering proof.

## Stereo frame driver

`StereoRenderer` now connects predicted-time session frames, per-eye camera
matrices, image acquisition/release and projection submission. A graphics
callback renders each eye from the same scene; it must complete GPU work
before returning, including failure. Timeout retries preserve the current
frame and do not reacquire images or rerender completed eyes. Draw failures
cancel the frame and submit no layers. Invalid tracking skips rendering.

Windows injected integration tests pass for pending image retries, both-eye
camera/pose propagation, completed stereo submission, failed-draw cleanup,
next-frame recovery and tracking loss. The native Vulkan draw callback and
game-loop hookup remain unimplemented; these tests do not claim actual pixels
rendered to a headset or playable VR.
# Surrounding star volume — partial environment work

The user identified the repeated planet as the intro. Source `MAPS/INTRO.ASM`
selects `BG_INTRO`; that symbol existed in both cartridges but was missing from
the VR unique-background classification. It now requests single-occurrence rows
for the full intro background height. On-headset duplicate-planet verification
is still pending. The expanded GPU suite passes all 880 cases in both eyes,
including the separate EX planet/cloud masks (not an intro capture).

EX BG_5_4 unique-planet regions are now forwarded from scene snapshots to the
Vulkan tile decoder. Its two authored planet regions use the desktop ink masks
and sky replacement, preserving repeatable clouds. GPU regression cases cover
authored/repeated positions, shared cloud ink, and wrapped scroll 8191. This
change is not yet headset-verified and does not establish coverage of every
planet background; the user's specific repeated-planet scene is not identified.

Game presentation now captures the first valid stereo midpoint as a position
anchor. Later head translations and the two-eye baseline remain intact; only
scene camera copies are shifted, never OpenXR compositor poses. This removes
dependence on an arbitrary initial LOCAL translation, without changing authored
model sizes or locking the world to the head. Camera tests cover initial
centering, 64mm eye separation, subsequent movement, and invalid-input rejection.
This anchor change is not yet deployed or headset-verified. LOCAL reference-space
change events now reset the anchor at their effective predicted display time,
including changes during invisible frames. Injected lifecycle tests cover future,
due, repeated and invisible-frame events. Initial yaw alignment remains pending.

Background tile UVs now use a separate perspective-correct shader varying;
native model UVs retain source affine interpolation. This addresses the
mathematical interpolation error on obliquely viewed tile geometry, not the
remaining need to replace flat environment panels. Shader compilation passes;
The user subsequently confirmed on Quest: "the distortion is gone." This verifies
their reported view-dependent distortion, not all background placement. All 872 native tile cases pass in
both Vulkan eyes. The `--tiles-tilted` and `--tiles-tilted-alternate` fixtures
render a slanted textured plane with opposite triangle diagonals: both eye BMPs
match byte-for-byte, demonstrating triangulation-independent interpolation.
Captures: `tmp/vr-tilted-a` and `tmp/vr-tilted-b`. Live-composite regression also
passes. These checks do not prove complete world placement or 360-degree coverage.

Live VR now opts into radial star visibility, retaining source stars behind the
gameplay camera instead of discarding them by forward depth. Billboard sizing
uses radial distance. Modified immutable vertices are cached between frames;
the default source-parity path remains unchanged.

`starfox_vr_scene_check --dust-surround-rear` (after output, ROM and symbol
arguments) turns a rear-only source fixture 180 degrees and checks nonzero star
coverage in both Vulkan eyes. Original and EX fixtures pass, including immutable
vertex reuse. Packet tests and shader freshness checks pass. This change has not
yet been rebuilt into or verified on the Quest APK.

This is **not a complete 360-degree environment**: planar sky/background and
terrain layers, camera-relative scale/placement, and UI separation still need
work. The user's large geometry and pink fallback-background screenshot remains
an unresolved integration report.
