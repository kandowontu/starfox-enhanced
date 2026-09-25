# Current goal acceptance — September 19

September 23 navigation note: `CURRENT-ACCEPTANCE.md` is the current checkpoint
index; historical scope exclusions below (including earlier VR holds) do not
describe the active overnight goal. VR implementation has continued. Public
Enhanced Sky now has a saved off-by-default menu control and local build/input/
layout evidence. Full family coverage and hardware/whole-goal acceptance remain
incomplete; no goal completion, issue closure or release is claimed.

September 20 combined terrain/backdrop/state checkpoint: the complete current
Windows build succeeds and all 61 configured CTests pass (179.48 seconds,
four workers). This includes the new terrain/image/Macbeth tests and expanded
Original/EX environment archives alongside existing endings, transitions,
input, audio and lifecycle tests. Log: `build/current/Testing/Temporary/LastTest.log`.
GPU device checkers have their separate targeted evidence; this CTest result
does not claim every GPU backend/adapter or physical platform was exercised.
`CURRENT-ACCEPTANCE.md` indexes the original requirements and remaining gaps.
No release, issue closure or device deployment was performed.

September 20 environment-state follow-up: added all-field environment archive
roundtrip/invalid/legacy coverage and invalidated atlas classification on the
desktop load handoff. All three state suites pass, plus Original/EX real SDL
save/load/slot navigation checks with Enhanced Sky requested. See
SAVE-STATES-STATUS.md; this does not establish all-scene visual restore parity.

September 20 Android backdrop refresh: current shared rendering/terrain/camera
code builds into the ordinary arm64 debug APK. Package validation proves the
complete new backdrop resource and required runtime payload are present;
see GPU-MIGRATION-STATUS.md. Physical rendering and fullscreen acceptance are
still outstanding. Fresh reads of all six requested GitHub issues add no
reporter evidence: #43/#49 closed, #47 confirmed corrected, #44/#46/#48 retain
the previously recorded limitations. No remote issue changes were made.

September 20 completed-camera follow-up: an actual Colony route capture
with interpolation alpha 1 still contains the nearby door. Fixed a separate
Q15 round-trip error in the untouched mouse camera; the new natural-route
camera regression asserts exact completed/source door depth and passes.
See BACKGROUND-AUDIT.md. Independent synchronized full-frame parity remains
unproven; this does not close the all-background requirement.

September 20 resident-backdrop follow-up: unchanged Enhanced Sky images no
longer join the per-frame SDL effects upload. Windows/Linux application builds,
D3D12/Vulkan effects cache lifecycle checks and the 108-case resident
composition checker pass; a current tilted gameplay capture was inspected.
See GPU-MIGRATION-STATUS.md for transfer savings and scope. This is additional
GPU migration progress, not completion of all-stage/device acceptance.

September 20 landscape/planet-selector follow-up: added independent Enhanced
Ground/Sky controls, ground materials/motions and sky styles/motions. Water,
Mirror and Gold Metal support hardware ray-traced ground receivers and a
separate Reflective Surfaces gate; unsupported paths use explicitly approximate
screen-space reflections. Planet Select Cheat is optional, uses X+Y on both
cartridges and A/Start to launch; actual planet coverage fixes the rectangular
map fade patch. See ENVIRONMENT-OPTIONS.md for evidence and limitations. New
DXR receiver checks, D3D12/Vulkan effects and layer protection, GPU composition,
settings and all three state suites pass. This targeted batch does not replace
or extend the earlier full-suite/physical-device acceptance. Steam/VR remains
deferred, and the Colony full-source camera/composition comparison below is
still unproven. SBS already has real GPU eye separation; stronger user-adjustable
depth and depth for the flat background are estimates, not added features.

September 20 consolidated checkpoint: full Windows build succeeds and all
58 configured desktop tests pass (199.59 seconds, four test workers), including
Original/EX endings, level clears, simulation data, states, audio, transitions,
effects, stereo, input and the unsupported-DXR contract. Logs are retained in
`build/current/Testing/Temporary/LastTest.log`. Native Linux application and
Original/EX real-asset packed-face checks also rebuilt/passed; ordinary Android
arm64 APK rebuilt in 18 seconds. This checkpoint includes the accumulated
diagnostic and regression changes; it is not a full physical-device acceptance
or a release. VR remains on hold.

Natural Colony progression now reaches the final tunnel and Andross using
controller inputs with God Mode. The reached tunnel's isolated BG2 matches
the independent SNES snapshot probe in all 57,344 RGB555 pixels. The nearby
right-hand HALF_D model, not the background, causes the observed obstruction;
source animation coordinates pass Original/EX regression checks. Source-camera
and complete source-composition comparison remain open. See BACKGROUND-AUDIT.md.

September 20 state compatibility: every selectable model/world style now
round-trips in real Original/EX archives, including all new manipulations.
Invalid world temporal modes reject without changing live state. All three
state suites pass; see SAVE-STATES-STATUS.md. Fresh #44 comments still provide
no physical Deck confirmation beyond the already recorded native/Proton split.

September 20 runtime menu coverage: the desktop input suite rebuilds and passes
(held-confirm protection, fixed menu navigation versus remapped gameplay).
The F1 capture harness now isolates inherited diagnostics and saved graphics
effects, selects Software/D3D12/Vulkan explicitly, and captures final output.
All six Original/EX Cheats launches complete; the three backend images match
exactly per experience. Both distinct images were visually inspected: all
six cheat rows and Back fit, with no clipped labels. Fresh Original/EX F1
open/resume/reopen sequences pass and retain experience. Evidence:
`tmp/runtime-cheats-backends-sep20`, `tmp/runtime-f1-isolated-sep20`.
This proves menu access/presentation and input-unit invariants, not every cheat
in every stage or physical controller acceptance. No VR changes were made.

September 20 allocation follow-up: reused GPU model upload scratch storage;
Windows rebuild, queued-model parity on D3D12/Vulkan, 18,090 Vulkan
destruction-face comparisons and two EX gameplay capture pairs pass. Before/
after timing observations are recorded with their system-load limitation in
NATIVE-PERFORMANCE-BASELINE.md; no general FPS guarantee is inferred.

September 20 desktop GPU coverage: all 59 numbered Original/EX stage entries
complete the new default-pipeline execution sweep, with exact CPU/GPU native
and final-image parity and no prohibited fallback. These are 12-frame samples
after 1,000 source ticks, not full routes or performance benchmarks. See
GPU-MIGRATION-STATUS.md and `tmp/gpu-stage-execution-sep20`.

September 20 comms coverage: all six language choices now have Original/EX
32:9/2x final captures for source message 36 with its requested teammate meter.
All 12 GPU/software pairs match exactly; all GPU images were inspected for
portrait proportions and translated-text/meter overlap. See PRESENTATION-PARITY.md.
This is one source-message fixture, not all conversation/device acceptance.

September 20 platform follow-up: ordinary Android arm64 debug now includes
Shatter/Melt/Ripple Warp and the current desktop diagnostic-only changes.
Removed unavailable Metal/DXIL effects payloads from nonmatching platforms:
Windows executable shrinks 987,795 bytes and packaged Android libmain shrinks
1,074,256 bytes. Windows/Linux applications and Android APK rebuild; Windows
D3D12/Vulkan and Linux Lavapipe effects checks pass. See GPU-MIGRATION-STATUS.md.
Fresh GitHub reads of #44/#64 add no physical-device evidence; both remain
unresolved acceptance gaps. No installation or release was performed.

September 20 replay follow-up: corrected #67's test route to Space Armada
(LEVEL1_3). After both Original ending fixtures, the normal title/level-select
handoff reaches the corridor with periodic boost, without map-state shortcuts.
120 consecutive scroll samples and three BG2 pixel comparisons at each of
256/400/800 widths match a clean launch. Original ending tests pass. This is
bounded Windows background evidence, not Android or whole-route acceptance.
Regular Android arm64 debug APK also rebuilds with the latest manipulation
effects/history integration (38 s); no device deployment or release.

September 20 follow-up: the actual ending enhancement matrix passes with all
background/UI pixels preserved (LIGHT, RAY, REFLECTION, ALL); captures in
`tmp/ending-enhancements-verified`. Regular Android arm64 debug rebuild passes
(29 s) with the ending-star extension and software tunnel high-pass fix.
No Android deployment/device acceptance, VR work or release was performed.

September 20: implemented Original ending star-only margin extension (#72),
with native-center preservation and matching CPU/GPU outer pixels in actual
captures. Fixed a software tunnel high-pass duplication found by the BG2
parity checker. D3D12/Vulkan checks pass; see BACKGROUND-AUDIT.md. Direct
pixel comparisons disprove the suspected missing GPU nebula: enhancements
preserve the background and alter only the ending's 3D lettering. The ending
enhancement capture script now asserts that preservation numerically.

September 20: fixed #69's sky strip during the closed boss-roll wipe by covering
the full vertical dossier raster. New actual-final-target checks pass in both
software and GPU (visible card, then zero leaked pixels). See ENDING-VALIDATION.md.

Fixed #70's blinking route-selection mark after cycling courses: the unused
route-sprite tail is hidden without clipping valid path lines. The matched
before/after capture changes exactly the eight artifact pixels. Original/EX
simulation suites pass (2/2, 126.76 s); regular Android rebuilt with #68/#70 (16 s).
See BACKGROUND-AUDIT.md for reproduction and capture paths.

Fixed #68's ending-to-title corruption: retire the credits fixed-scroll flag
at the Original restart handoff. The new regression failed with scroll 0 vs
257 before the fix; Original/EX ending suites now pass, and the actual
post-ending software title capture was inspected. See ENDING-VALIDATION.md.

Regular Android arm64 debug APK rebuilt successfully after the FSR1/effects
batch and Armada trace additions (33 s, `:app:assembleDebug`). Payload checks
confirm nonempty libmain.so, libSDL3.so, libc++_shared.so, dex and manifest;
no libandroid.so system stub is bundled. This closes the stale-Android-build
gap for these shared changes, not physical Android GPU-freeze or navigation-bar
acceptance. No device deployment or release was performed. One existing
generated Metal string-length warning remains nonfatal under Android Clang.

Latest: FSR1 now has active-adapter detection, menu/settings integration and
actual gameplay captures; see FSR1-STATUS.md. This supersedes the historical
foundation-only notes below. Physical AMD acceptance is still missing. Six new
styles (37–42) and stronger Bleach Bypass are implemented and captured; Windows
D3D12/Vulkan and native Linux Vulkan comparisons pass. VR remains on hold by
user instruction. No release or full-goal completion is claimed.

## Queued: AMD FSR1 upscaling

User requested adding this to the queue on September 19. Not implemented.
Implementation started with independent Fsr1Mode values and input-resolution
calculation in fsr1_settings.hpp. Compile-time tests cover all quality ratios,
odd/ultrawide extents, minimized/tiny frames, invalid modes and overflow safety.
This foundation is not connected to rendering or menus yet; EASU/RCAS GPU
passes, active-adapter selection, persistence and visual validation remain.
AMD's pinned MIT headers and a two-stage GPU shader wrapper are now present;
DXIL, SPIR-V and Metal compilation succeeds using HLSL 2018 compatibility.
Runtime resource/dispatch integration and photographic proof are still missing.
The GpuFsr1 wrapper now builds in starfox_core and records separate EASU/RCAS
passes on a borrowed SDL GPU command buffer, with reusable textures, explicit
device release, input validation and no CPU readback/wait. Offline generation
packages DXIL/SPIR-V/Metal, and CMake's source digest includes AMD headers.
Generation freshness check passes. No runtime dispatch/image test or menu
connection yet; this must not be reported as usable in-game FSR1.
Actual D3D12 and Vulkan FSR1 dispatch checks now pass in
starfox_gpu_fsr1_check: alternating flat red/green, odd dimensions, texture
reuse and resize across four dispatches. Readback occurs only in the checker.
Upstream approximate reciprocals produce 0x3bfc rather than 0x3c00 for flat
one; the test allows at most one 8-bit step of darkening, with zero channels
and alpha exact. This is basic dispatch/lifecycle validation, not patterned
reference-image equivalence or gameplay quality/AMD hardware acceptance.
Independent FSR1_MODE persistence is now wired through desktop settings and
the simulation's host preferences. All five modes round trip independently
of DLSS; invalid save/load values are rejected without partial live changes.
Original/EX ROM-backed state tests pass (2/2, 32.40s), preserving the current
FSR1/DLSS modes across state loads without changing cartridge archive bytes.
The Windows application rebuild passes. No menu or presentation connection yet.
FSR1 now has a resident-composite adapter: upscale the HUD-free scene and restore
full-resolution HUD pixels, while permitting background artwork to remain
spatially reconstructed. DLSS keeps its original artwork-protection default.
The expanded D3D12 HUD test passes both preservation modes pixel-exactly, along
with existing depth/motion/resampling checks. This is not yet called by the game.
Detect the active rendering adapter (not merely an installed card): show FSR1
in place of DLSS on AMD, retaining DLSS on supported NVIDIA hardware. Integrate
actual FSR1 EASU upscaling and RCAS sharpening with OFF / ULTRA QUALITY / QUALITY /
BALANCED / PERFORMANCE choices. Keep preferences and unsupported-renderer
behavior coherent; verify adapter selection, runtime switching, image quality,
and performance. Do not relabel another filter as FSR1. Retain upstream license
notices for any bundled AMD implementation. No VR expansion implied.

Additional user request: include AMD hardware ray tracing and GPU reflective
surfaces in this work item. Detect capabilities on the active adapter rather
than restricting by vendor; enable both on supported AMD GPU/backend/driver
combinations, default ray tracing off, and retain clear unavailable behavior
on unsupported cards. FSR1 is independent and must not be a prerequisite for
ray tracing/reflections. Verify actual acceleration-structure/ray-query support,
shadow replacement without duplicate native shadows, reflection output and
FSR1 combinations. Do not imply every AMD card supports hardware ray tracing.

Code audit: Windows DxrShadows already selects by DXR tier 1.1 / shader model
6.5 and adapter LUID, with no NVIDIA vendor gate. SdlDxrShadows matches the SDL
D3D12/Vulkan rendering adapter to that producer. Do not add an artificial FSR1
dependency or claim that AMD support was newly enabled just by changing a
label. Actual AMD-driver/interoperability and reflection testing remains; this
Windows DXR implementation is not evidence of native Linux hardware RT support.

Latest save-state verification: Original/EX archives preserve all four new
effect IDs, migrate legacy model/world Ice to Cyanotype, and reject invalid
styles without changing live state. Unit/Original/EX suites pass 3/3 (75.36s);
see SAVE-STATES-STATUS. This is additional compatibility evidence, not completion
of the full migration/device goal. The separate obsolete-build cleanup attempt
was blocked by the environment and made no deletions.

The user has now closed the game and the normal Windows executable successfully
relinked with the grouped effects and physical-ground reflection fix. It no
longer requires using the alternate test executable mentioned below. Native
Linux build, three focused CTests, Vulkan effect comparisons and six Gold
software reflection captures also pass. See the newest GPU-MIGRATION-STATUS
checkpoint. The broader all-scene/device acceptance goal remains incomplete.

Latest desktop styles are grouped and deduplicated, with Gold/Copper reflective
materials plus two distinct colour styles. Windows CPU, D3D12, Vulkan and DXR
checks pass; see EXTRA-EFFECTS-STATUS. The updated Windows test binary is now
`build/current/starfox_pc-effects-check.exe` (supersedes ground-check below).
The user's regular game remains running and prevents relinking its executable.

At the user's disk-space request, 32,121 generated frames were removed from an
explicit list of old diagnostic capture sequences, reclaiming 24.662 GiB.
First/last samples, every 60th frame, images explicitly referenced by audit
notes, all logs and all saves were retained. Toolchains/assets/builds were not
removed. Historical full-sequence comparisons retain logs, but deleted frames
must be regenerated for a new full-sequence review; do not treat their old
directory names as proof that every image remains present. Exact deletion list:
`tmp/capture-cleanup-20260919-214007.csv`. No gameplay files were deleted.

Latest reflection coverage fix retains physical ground even when a tiled BG2
reflection material is missing. Hardware occlusion/fallback tests pass, and
the authored-background regression remains byte-exact. This is verified in
`build/current/starfox_pc-ground-check.exe`: the normal executable was running
and could not be replaced, so it remains pending relink after the user closes it.
See EXTRA-EFFECTS-STATUS. The complete migration/device goal is still open.

Latest user revision restores Enhanced Shadows **only for Software**, plus a
lower-cost software Reflective Surfaces implementation. GPU keeps Ray Tracing
and detailed GPU reflections; this supersedes the earlier global removal of
Enhanced Shadows, not the GPU-only removal. Both shadow enhancements default
off and replace native silhouettes rather than layering over them. Software
reflections do not require either shadow switch. Implementation, limitations,
capture evidence and a bounded benchmark are in EXTRA-EFFECTS-STATUS.

Current Linux desktop refresh also builds successfully: five targeted CTests
and Vulkan/Lavapipe warp/material/geometry diagnostics pass. Original/EX live
software OFF/LOW/HIGH captures pass and both HIGH images were inspected.
See GPU-MIGRATION-STATUS and `tmp/reflections-linux-software-sep19`.
This strengthens non-Windows execution evidence, not physical Deck/VR acceptance.

September 19 follow-up: user chose widened tunnel ceiling/floor for PC #48.
Implemented and captured without repeated sections or expanded foreground
occlusion; see ISSUES-43-49-VERIFICATION. The former tunnel-choice blocker is
resolved. Physical platform acceptance remains separate.

User reiterated the extra 2D/3D effects request, specifically metallic and
mirror with real reflective surfaces. DXR now outputs actual first-bounce
reflected radiance, including model materials, authored BG2 and finite ground
intersections. Reflective Surfaces is independently selectable and requires
Ray Tracing on GPU (software has the simplified independent path above).
Ten additional colour styles plus Metallic/Mirror are implemented;
hardware fixtures and live mono/stereo captures are recorded in
EXTRA-EFFECTS-STATUS. This is not full acceptance: multi-layer environments,
behind-camera ground material fidelity and all-level/platform proof remain open.
VR work remains excluded by the latest scope instruction.

September 19 Android resume follow-up: the regular SDL Activity now obtains
insets control from its decor and retries immersive mode on the UI queue after
resume, matching the existing Quest attachment safeguard. This covers a null
controller before attachment without asserting that physical gesture bars were
tested. The current arm64 debug APK builds in 20s, with manifest, dex, main,
SDL3 and C++ libraries verified; 11,624,774 bytes, SHA-256
`B35F58FCBF4D658B242A70F098A8A8B0DEA3E9BF64C5CE22EB14BB02F2C32F95`.
Artifact: `platform/android/app/build/outputs/apk/debug/app-debug.apk`.
No device installation or release.

September 19 current-build acceptance: rebuilt all configured Windows VR and
native Linux desktop/OpenXR targets after the connected-grid changes and Deck
device-name correction. All 16 VR CTests pass on both hosts (21.97s Windows,
22.58s Linux), including Original/EX cartridge input and shader freshness.
Desktop hotplug events already reopen the selected controllers on add/remove.
Fresh ADB enumeration is empty; these results do not replace physical headset
or Steam Deck Gaming Mode acceptance. No install or release performed.

September 19 grid allocation follow-up: changed VR grid inputs now reuse the
537 KB output arena and graphics descriptor. Eight dispatched camera/wrap
updates match CPU projection with no output/vertex reallocation; failed updates
preserve the preceding output. Windows final images remain exact and live EX
passes. Native Linux software Vulkan also passes, with exact reference images.
Quest rebuilds with the optimization. Setup benchmarks improve, but they are
not headset FPS measurements. See GPU-MIGRATION-STATUS.

September 19 connected-grid conversion: VR visual point projection and row
construction now run in two compute passes; canonical simulation history stays
on the CPU. Flat/rotated GPU row data and final images match CPU references in
both eyes, live EX submits successfully, targeted input tests pass, and Quest
arm64 rebuilds. Native Linux software-Vulkan rotated output also matches the
reference in both eyes. No headset/FPS claim. Broader edge-case and allocation-
performance acceptance remain; see GPU-MIGRATION-STATUS for proof.

September 19 migration inventory correction: visual dust/grid/indicator packets
are now counted before ray policy excludes nonphysical casters; line-only
particles are included. Dedicated producer-route tests pass, and live Original/
EX inventories identify migrated particles/text/grid dots. This exposed CPU
connected-grid projection/binning in the actual VR path, subsequently converted
in the follow-ups above. See GPU-MIGRATION-STATUS. Full goal acceptance remains open.

September 19 ray coverage: Titania/Fortuna final images show angled ground
shadows; Space Armada traces model shadows without inventing a ground plane.
All three current resident GPU caster captures match CPU-geometry and
CPU-composition references byte-for-byte, and off/unavailable/removed-shadow
override behavior matches. The checker now fails those comparison mismatches.
The sprite-only asteroid sample failed the caster requirement and is explicitly
not counted. See RAY-TRACING-VISUAL-AUDIT for photographic proof and remaining
precision/all-scene acceptance limitations. The overall goal remains open.

September 19 build protection: main VR graphics SPIR-V is now checked alongside
ray expansion on Windows/Linux/Quest configurations, including incremental
source/include changes. Disposable stale-build rejection/recovery tests pass on
Windows/Linux; all three real configurations build. This closes a route by which
old graphics could ship despite source fixes, not remaining visual/device gates.

September 19 EX background boundaries: final gameplay captures now straddle both
LEVEL4_4 transitions at 16:9/32:9, including the actual pre-Colony frame identified
from render-state logs. All 14 full-image GPU/software pairs match exactly;
ultrawide images were inspected and tunnel metadata changes with the background.
This closes the earlier isolated-layer-only gap for these boundaries, not the
remaining all-background/natural-route/device requirements. See BACKGROUND-AUDIT.

September 19 native Linux VR: full OpenXR build and 15/15 VR CTests pass after
fixing a test's POSIX name collision. All Touch/Index bindings are now checked
exactly. Linux software-Vulkan particle/text outputs match CPU references in
both eyes, supplementing Windows GPU evidence. Physical Index/Quest/Deck
acceptance remains open; #44's latest report remains native Deck Game Mode.

September 19 VR scaled text: sizing, placement and depth/size rejection now run
on the GPU. Original normal/near-camera and EX normal text match CPU-reference
captures in both eyes; GPU near/nonpositive rejection, 15 VR tests and Quest
arm64 package build pass. See GPU-MIGRATION-STATUS for artifact hash and limits.

September 19 Quest follow-through: arm64 APK rebuilt with the GPU particle
migration and passed package structure checks. Fresh ADB enumeration is empty,
so hardware installation/performance remains unverified. Source text/shadow and
dust/grid Vulkan fixtures now explicitly initialize display brightness and pass
visible-output tests. See GPU-MIGRATION-STATUS for the exact artifact and scope.

September 19 VR particles: signed-coordinate interpolation, near-depth rejection
and dot sizing now execute in the Vulkan vertex shader. Independent previous-CPU
reference captures match both eyes byte-for-byte; all 15 VR tests and a 1,200-tick
Fortuna world preflight pass. See GPU-MIGRATION-STATUS for remaining CPU packet
preparation and verification limits. This does not close hardware acceptance.

September 19 Linux refresh: full native rebuild and 55/55 CTest pass; Vulkan
background/indexed-source and font/portrait/raster checks pass. Original/EX
each pass four live renderer switches with final captures. Game Over now also
has successful Half/Full SBS checks with visible stars in each eye, in addition
to earlier failure-recovery parity. See GPU-MIGRATION-STATUS and issue notes.
Physical Deck/Quest/Index acceptance and the requested corridor design choice
remain open; none of these local results is a substitute for those gates.

September 19 first-corridor follow-up: Original 16:9/32:9 and EX 16:9 Titania
BG_2_3C captures match GPU/software byte-for-byte, with solid matching outer
walls and no duplicate corridor artwork. The capture's old Mode 2 assertion
was invalid: source VOFSOFFPLEASE intentionally switches this corridor to
Mode 1. Separate INATUNNEL checks now distinguish it from water. See the top
of ISSUES-43-49-VERIFICATION; the reporter's wider ceiling/floor request is
not proved resolved by parity or by the requested solid-wall implementation.

September 19 broad refresh: complete desktop and VR host rebuilds succeed.
Desktop CTest passes 56/56 (224.21 s); VR CTest passes 15/15 (30.55 s).
Quest and ordinary arm64 Android debug APKs both rebuild successfully, with
required nonempty native libraries and manifests verified. A fresh ADB listing
has no attached devices, so neither APK was installed or physically verified.
GitHub #43 is now closed; #47 has reporter confirmation; #48 still reports the
first corridor and #44 has native-Linux/Deck-specific reports. See the live
September 19 issue notes rather than older open/closed snapshots below.

September 19 comms/tally connection: separate host UI layers now record raw
glyph/portrait commands and compose resident GPU indices with correct transparent
clears and mosaic. Original/EX ultrawide full-SBS comms and Original Sector Z
tally captures match the CPU-source paths; both backends pass independent layer
fixtures. See GPU-MIGRATION-STATUS for limits. The decoder-only note below is
superseded, but broad migration/platform acceptance is still open.

September 19 localization follow-up: bitmap-font recording is connected and
Japanese Original/EX briefing five-path comparisons pass. Raw portrait decoding
is implemented for recorded targets, but comms/tally source layers still require
GPU recording/composition integration. Do not count decoder support alone as
finished portrait migration. See GPU-MIGRATION-STATUS.

September 19 font migration: cartridge 12-row glyphs now use packed GPU raster
commands for briefing/ASCII/compact/Latin menu rendering. Independent 96-case
font checks pass on each of D3D12 and Vulkan; Original/EX live briefing paths
match and EX matches its previous full-frame baseline. Misaki glyphs and comms
portrait decoding remain CPU work. See GPU-MIGRATION-STATUS for exact scope.

September 19 portrait audit correction: the ScaleFX missing-portrait report
below was a misleading image-preview observation, not missing saved pixels.
Direct decoding finds 7,008 colored pixels in Pepper's portrait in the original
suspect capture as well as the latest capture. Both portrait rectangles match
the lighting-disabled reference exactly. See GPU-MIGRATION-STATUS. Do not keep
this false visual blocker on the remaining-work list. Full migration and broad
platform acceptance still remain open.

September 19 runtime isolated overlays: BG2 and briefing text recordings now
feed GPU-held logical layers to effects. Original/EX live briefing comparisons
pass 16:9/2x including isolated-GPU failure replay, and 32:9/4x full SBS with
effects. Capture evidence and exact remaining CPU glyph work are documented in
GPU-MIGRATION-STATUS. This supersedes the disconnected-runtime note below, not
the outstanding broad migration/platform acceptance requirements.
Visual follow-up: ScaleFX EX frame 360 lacks portraits on both CPU/GPU paths;
shared overlay filtering remains an open defect despite parity passing.

September 19 resident overlay support: effects can consume packed GPU planet/
briefing layers without CPU index upload. D3D12/Vulkan comparison suites pass
resident, uploaded and mixed inputs across six filters and three fade levels.
Runtime isolated-layer recording is NOT connected yet; this is not full GPU
migration acceptance. See GPU-MIGRATION-STATUS for scope and remaining work.

September 19 HUD mismatch resolved: CPU foreground no longer inherits model
normals from the GPU compositor when palette indices match. Explicit collision
fixtures pass with the compositor suite, and the previously failing EX paused
ultrawide full-SBS effects capture now matches CPU-background output exactly
(`tmp/hud-gpu-stereo-fixed-sep19`). This supersedes the pending mismatch below;
isolated/source-art migration and broad platform acceptance remain open.

September 19 HUD composition work is IN PROGRESS: added indexed layer GPU
commands (288 D3D12/Vulkan fixtures pass) after finding the old fast copy bypassed
recording. Basic Original/EX gameplay comparisons pass, but EX paused ultrawide
full-SBS reveals lighting ownership differences over the pause text. Details
and captures are at the top of GPU-MIGRATION-STATUS. Do not accept/release this
checkpoint until that discrepancy is resolved.

September 19 EX mosaic: removed the bitmap's CPU exception with explicit
staging-inset rounding. GPU and full scene replay match the old two-pass path
for all 16 mosaic sizes at three scales in native/wide layouts (192 fixtures),
on D3D12 and Vulkan. Isolated targets and non-EX HUD remain; no physical-device
or release acceptance is implied.

September 19 EX bitmap conversion: non-mosaic native pause/tally bitmap and
following host HUD now use ordered GPU foreground recording. Paused Original/EX
16:9/2x captures match CPU and failure paths. EX's existing pale left-edge strip
is visible in the inspected capture on all paths; do not count that as fixed.
Mosaic/dossier and isolated targets remain on their previous paths.

September 19 follow-up: final main-frame cartridge overlays are now recorded
beyond titles. Original/EX planet-map full SBS at 32:9/4x with effects matches
CPU backgrounds and asserts GPU late-layer use. Empty passes are skipped.
Isolated overlays, EX native bitmap and earlier host-HUD targets still remain;
see GPU-MIGRATION-STATUS for exact scope and evidence.

September 19 GPU migration: recorded OBJ sprites now upload raw VRAM snapshots
and compact rectangles; the GPU decodes the sprite bitplanes. Independent CPU,
recording replay and GPU comparisons pass 222 cases on each of D3D12 and Vulkan.
Original/EX title, planet-map and Controls final captures match CPU/failure paths
(`tmp/sprite-gpu-frontends-sep19`). See GPU-MIGRATION-STATUS for scope: isolated
and late unrecorded targets remain, so this is not full migration completion.

September 19 tally occlusion follow-up: the black shapes in frame 1200 are
the authored CL_SHIP3_4 closing window wipe, not model corruption. The source
SF/MAPS/CL_SHIP.ASM explicitly requests roundwipefill. Fresh neutral-settings
capture `tmp/sector-z-clear-mask-sep19/ORIGINAL/001080.bmp` shows unobstructed
tally/portraits/names; frame 1200 logs wipe=1/170 while preceding samples log
wipe=0/85. Circle effects are inactive. The capture harness now explicitly
disables inherited model/world styles, filtering, bloom, HDR, stereo and ray
tracing (with an explicit -RayTracing comparison option). Both shadow modes
show the same closing shape. Preserve this source transition; the separate
top-letterbox report remains open.

September 19 null-source GPU readback assertion report: added explicit resource
checks before raster/compositor/shadow/diagnostic downloads, and device/extent
validation for scene readback. Missing sources return through existing error
handling instead of reaching SDL's interactive assertion. Added uninitialized
and released-resource readback rejection coverage to the composition checker.
Windows executable rebuilt; Direct3D 12 composition checker passes (108 core
cases plus effects/overlay coverage). The screenshot's exact caller and trigger
are NOT reproduced; these are defensive checks, not a confirmed root-cause fix.

September 19 Sector Z audit: discovered background parity tests inherited the
saved DLSS selection. The first capture set (`tmp/sector-z-current-sep19`) used
Quality reconstruction and is NOT evidence of a background regression. The
harness now explicitly disables DLSS and rejects unexpected evaluation logs.
Corrected Original/EX LEVEL3_4 captures at tick 1000 are byte-identical across
GPU, CPU-background, injected-failure and CPU-model paths, both at 16:9
(`tmp/sector-z-native-sep19`) and with held roll at 32:9
(`tmp/sector-z-roll-sep19`). Inspected native and rolled samples do not show the
reported peach lower-edge artifact; the report remains unreproduced, not fixed.
The level-clear capture harness now accepts a level and authored clear routine,
retains process handles and bounds waiting. This permits CL_SHIP3_4-specific
victory investigation rather than relying on the default asteroid CL_WARP.

That authored Sector Z sequence reproduces stale communications over the retail
tally. MAIN.ASM END_LEVEL_SEQ does not call FRIENDS_MESSAGES_L, but the host
reconstructed the last portrait/text from latched counters anyway. dialogue_state
now suppresses this retail tally overlay without modifying native counters or
shortening the preceding gameplay conversation. Before/after inspected evidence:
`tmp/sector-z-clear-full-sep19/ORIGINAL/001200.bmp` and
`tmp/sector-z-clear-fixed-sep19/ORIGINAL/001200.bmp`. The stale bottom message is
gone. Added a tally-time communication assertion and optional named-case filter
to level-clear tests; CL_SHIP3_4 passes 20/30/60/90/120/240/360/480 FPS plus
unlocked 20/120 FPS with identical timelines. The top-letterbox report remains
unresolved. A fresh tally capture subsequently identifies the apparent black
occlusion at frame 1200 as coincident with the source's closing window wipe;
frame 1140 has all portraits/names unobscured. That image is not evidence for a
model-order fix. See PRESENTATION-PARITY for evidence and limits.

September 19 VR menu direction fix: shared MenuStick filtering chooses a single
dominant axis above 0.5, rejects ambiguous diagonals and keeps that cardinal
gesture until both axes return within 0.25. The VR frame driver enables it only
for host setup/options and EX's native pre-game menu. Flight keeps its original
independent 0.35-axis thresholds and diagonal inputs. The startup menu also uses
the same axis decision to prevent horizontal gestures moving between rows.
New tests verify cardinal selection, held-axis stability, neutral release,
ambiguous diagonals, press-only edges, reset and unchanged flight diagonals.
Windows VR test targets rebuild. Injected OpenXR input/startup-menu tests and
both Original/EX VR game-input suites pass, including 40-tick native-pad state equivalence.
No APK was installed and physical thumbstick acceptance is still pending.

September 19 EX God Nuke reproduction/fix: a new real firing-to-explosion
regression failed 10 ticks after firing, whereas the old synthetic test forced
the bomb directly into an explosion and only checked the visible player.
Tracing identified the host pointer conversion, not a need to rewrite native
damage: GameSimulation still used retail ALBLKS=$338 / AL_SIZE=56, while EX
uses $339 / 57. Player collision-part pointers therefore decoded to zero and
the host God Nuke killed them; PBODY subsequently killed the visible ship.
Both conversion directions now reuse MapVm's cartridge-aware mapping. This
also removes the stale layout assumption from player rebinding and circle
object lookup; visual acceptance of those secondary effects remains separate.
No health-restoration or strategy-dispatch workaround was retained.

Current Windows executable rebuild succeeds. Original and EX simulation suites
both pass, including actual bomb spawn/detonation over 120 subsequent ticks and
unchanged health for the player plus body/left/right collision parts. Existing
enemy-kill and native God Mode synchronization tests also pass. No release or
headset deployment occurred. Reported 0.0.6.7 black-screen startup remains
unreproduced; affected platform/GPU/startup logs have been requested.

September 18 regression refresh: rebuilt simulation/state executables and ran
runtime input plus Original/EX simulation-data and state-data suites. All 5 pass
(207.31 seconds). Coverage includes setup navigation, cheats, communication
meter timing and state persistence; this is not physical controller/headset
acceptance or proof that isolated overlays/sprite decoding have migrated.

September 14 live review: #43 and #49 are closed; #44, #46, #47 and #48
remain open. New #55 reports a live renderer-switch crash, confirmed against
the current source: recreation omitted late_scene_ and background_scene_
release while shutdown included both. Both paths now share one resource-release
routine. The Windows executable rebuilds successfully. Windows Vulkan runtime
checks pass four transitions per run across Original/EX and GPU, CPU-background,
injected-background-failure and CPU-model paths (32 transitions total). Final
captures match exactly between paths in tmp/renderer-cycle-vulkan-sep14.
The reusable check_background_gpu.ps1 -RendererCycle -Frames 40 asserts each
transition and return to GPU. The reporter's Linux system remains untested.
No issue closed.

Linux follow-up: current starfox_pc rebuild succeeds in WSL. Added
tools/check_renderer_cycle_linux.sh; its first run correctly fails acceptance
because SDL reports Vulkan unsupported and falls back to software on all GPU
transitions. Evidence: tmp/renderer-cycle-linux-sep14/ORIGINAL.log. This is not
a GPU crash reproduction or a passing Linux GPU regression. WSL graphics
initialization needs investigation; the Windows Vulkan evidence remains valid.

Resolved the WSL initialization question: the application requires hardware
Vulkan by default, so Lavapipe requires STARFOX_TEST_SOFTWARE_GPU=1. With that
test-only override, Original and EX each completed four live renderer switches
and produced inspected final screenshots in tmp/renderer-cycle-linux-lavapipe-sep14.
Both logs confirm GPU resident backgrounds and no fallback. The harness was
edited while executing, causing an EOF error after both completed runs; its
current syntax and all recorded transition/fallback assertions were separately
rechecked successfully. This verifies Linux Lavapipe resource handling, not the
reporter's NVIDIA PRIME configuration. Production hardware policy is unchanged.

The full goal is **not complete**. This is the current index; older delivery
notes are chronological history and often describe limitations since removed.
No publication or issue closure is implied.

Newest: EX introductory-logo margin repair now executes on GPU, with real
EX-logo trace assertions and CPU/failure/effects/stereo parity captures.
108 new repair fixtures pass on D3D12 and Windows/Linux Vulkan. Isolated
briefing and late restored layers plus sprite decoding remain.

Newest runtime connection: native-edge histogram reduction and solid margin
fills execute on GPU. Wide Controls/Continue/Game Over now use resident
backgrounds; CPU/GPU, late-star, effects and stereo/failure comparisons pass.
EX logo repair, isolated/late layers and sprite decoding remain. See the
latest GPU-MIGRATION-STATUS section for exact scope and capture locations.

Newest runtime connection: ordered early BG1/BG2/BG3 and sprite raster chunks
now feed the GPU compositor, including title/planet selection and 4:3 Controls.
Original/EX CPU/failure parity, wide effects and stereo checks pass; Windows,
Linux and Android rebuild. Remaining dependencies are enumerated in the top
GPU-MIGRATION-STATUS entry (CPU edge reductions, isolated/late layers and
sprite decoding). No full-migration or final-hardware acceptance claim.

Latest runtime connection: Mode 2 gameplay BG2 now executes on GPU, with
pre-model CPU coverage, mono/stereo composition and full CPU recovery.
Original/EX Corneria and asteroid final captures match CPU at 32:9/4x and
with effects enabled; both SBS layouts and DXR composition checks pass.
Windows/Linux PC and Android builds pass. Other background modes and special
presentation-layer routing remain; see the newest GPU-MIGRATION-STATUS entry.

Latest foundation: raw BG1/BG2/BG3 tile decoding and priority rasterization now
execute on GPU in ordered scene batches with immutable snapshots and replay.
BG2 includes HDMA, software-double horizon fitting, ground continuation and
unique-art/water/tunnel rules. The checker now covers 432 cases / 184 million
samples. Mode 2 gameplay is integrated; remaining ordered presentation
integration is listed above. See GPU-MIGRATION-STATUS.md.

The compositor now accepts resident backgrounds with independent pre-model
CPU coverage. 108 ordering/recovery fixtures pass across D3D12 and Windows/
Linux Vulkan; Game Over normal/forced-fallback captures remain byte-identical.
The application loop now supplies these for Mode 2 gameplay.

Latest implementation: Game Over margin stars now use a late GPU world layer,
including per-eye stereo projection and complete CPU fallback. D3D12/Windows
Vulkan/Linux lavapipe composition checks pass; photographic comparisons and
limitations are recorded in GPU-MIGRATION-STATUS.md. The previously handed-off
ZIP predates this new late-layer implementation; it has not been replaced.

Latest consolidated refresh: all Windows/Linux targets, Windows VR, Quest APK
and regular Android APK rebuild successfully. The rebuilt VR suite passes
15/15 (46.96 s with concurrent desktop tests). Fresh Windows CTest passes
55/55 (263.44 s, also under concurrent build/test load); fresh Linux CTest passes
54/54 (277.00 s). The current development ZIP is
verified below; physical headset/Deck/Android acceptance remains outstanding.

Current engineering progress: live GpuScene aggregation and shared DXR geometry
routing are connected on D3D12/Windows Vulkan, with a GPU-to-GPU completion
dependency. Supported mono and stereo scenes avoid CPU caster transforms.
Effect/axis modes (including exploding axes) export GPU source-mesh casters;
SBS no longer prepares an unused mono shadow pass. Training,
Corneria pillars, EX Corneria and Original title captures verify the selected
path and reference parity. Original full/EX half SBS match reference captures,
and injected stereo failure rebuilds correct mono GPU shadows. Controls player
models now join the GPU batch; 18 CPU/GPU captures match across Original/EX,
three aspect/scale combinations, and D3D12/Vulkan. Controls GPU caster routing
also passes reference parity. Remaining non-model layers and broader hardware
acceptance still need review; the small-scene
mono benchmark has a 4–5% median cost despite
comparable p95 timings. See GPU-MIGRATION-STATUS.md for evidence and limitations.

Latest continuation: current Linux build succeeds; full WSL/Linux CTest passes
54/54 (286.33 s), including both cartridge simulation/state/ending suites and
runtime input. Separate Linux Vulkan depth check passes 366,183 analytical
samples. This is not physical Steam Deck or Metal acceptance.
EX Continue/model-viewer resident shadow capture and F1 events also pass.
The refreshed `tmp/StarFox-Enhanced-PC-Quest-GPU-current-20260913.zip` contains
the current Windows binaries and Quest APK, with clean contents and binary
hashes verified. A reusable packaging script now creates fresh staging areas
and rejects existing output archives. See ISSUES-43-49-VERIFICATION.md for the
package check/hash. No release or device installation is implied.

Current user direction: future Quest installs are authorized, including stopping
the app for the update. Preserve saves/data. Batch related implementation work
and consolidate regressions; the user has approximately 15% of their plan left
and wants to wrap up within it. This does not change the completion criteria.
September 13 latest deployment supersedes the older controller-dialog notes:
the preview/reset/title-distance APK built (27 s), `adb install -r` succeeded,
and QuestActivity launched successfully. Current logs confirm OpenXR and Adreno
swapchain initialization. This is not physical preview/reset acceptance.
`deploy_quest.ps1 -StopRunning` preserves saves while stopping the authorized app.

Latest PC shadow report was Training: the gameplay-only dispatch gate excluded
that flow. Removed the flow gate, and included native viewer/continue models
before tracing. Current Windows executable rebuilt; Original/EX Corneria,
Training geometry and title captures pass resident/readback equality. Title
self-shadow capture has no invented ground. See RAY-TRACING-VISUAL-AUDIT.md.

Latest PC follow-up: opt-in planar-model geometry depth now runs entirely on
GPU and survives both mixed-scene composition paths. A new D3D12/Vulkan checker
passes 366,183 analytical samples plus invalidation/ownership checks; existing
real-model and mixed-binning regressions also pass. Details and exclusions are
in PC-DLSS-STATUS.md. This is not DLSS evaluation or complete scene depth, and
normal game records do not enable it yet. No Quest installation occurred.

September 13 PC follow-up: stable native object identity now reaches GPU model
draw records and survives stereo eye copies. Windows runtime and stereo tests
build; the stereo regression passes. This is a DLSS prerequisite, not motion
vectors or DLSS evaluation. The approved SDK probe initializes DLSS on a real
D3D12 device and queries Quality/Balanced/Performance/DLAA successfully.

Fresh runtime checks pass F1 open/resume/reopen for Original and EX. The verifier
now requires the requested experience, not merely a constant experience value.
Inspected EX Cheats capture in `tmp/dlss-identity-ex-menu-regression`, EX Half
SBS in `tmp/dlss-identity-sbs-half`, and Original Full SBS in
`tmp/dlss-identity-sbs-full`. These are limited fresh presentation fixtures;
they do not prove all scene/device acceptance. Full existing Windows CTest
suite completed 55/55 (193.47 s). Afterwards all Windows targets were rebuilt
against the current shared core. The new previous-pose history tests pass;
a fresh full suite against those relinked binaries passed 55/55 (187.31 s). DLSS motion
vectors and evaluation remain unimplemented; the history helper is not enabled
in normal rendering.

Latest deployment: user authorized the update. Added VR Posterize, Cyanotype
and Warm Film as independent model/world styles with translated labels and
existing intensity controls. New IDs round-trip preferences and retain old
desktop IDs; bitmap glyph coverage passes. Three-style GPU checks pass exact
OFF at zero and midpoint at 50%; full captures inspected in
`tmp/vr-extra-styles-verified`. All 15 VR tests pass (23.09 s); APK builds.
ADB force-stop then install -r succeeded, preserving app data. Launch was
intercepted by Horizon OS's controllers-required dialog; no new game PID
was observed. User must wake controllers; do not claim runtime acceptance.
PC DLSS requests are tracked in PC-DLSS-STATUS.md, not implemented yet.

Orbital scope follow-up: Original Sector X also uses the entry-horizon flag;
production and capture callers now explicitly require EX experience as well as
gameplay before applying the user-requested quarter-turn. New reusable
`tools/check_vr_orbital_backgrounds.ps1` passes six real-cartridge fixtures:
Original Sector X, EX entry/boss, and menu backgrounds 21/25/35. Captures are
under `tmp/vr-orbital-scope-check`; Original and EX menu horizons remain below
the scene. The relative EX gameplay rotation is still not physical acceptance.
Quest rebuilt successfully (18 s), not installed.

Latest GPU optimization: moved invariant orbital palette probes from fragment
to vertex shading with the same sampler, not a CPU cache. Both eyes plus
synthetic depth/coverage captures match the prior build exactly (222 BMPs
across two runs). Full VR suite passes 15/15 (24.26 s); Quest APK rebuild
passes. No installation or physical FPS claim. See GPU-MIGRATION-STATUS.md.

Current verification after gameplay-only orientation gating: full VR suite
passes 15/15 (23.27 s), and fresh native Vulkan captures complete for EX 5-1
boss tick 2500 and native menu page 2. Inspected
`tmp/vr-gameplay-clockwise-current/live-scene-left.bmp`: the local fixture
now puts the surface at the left, whereas the user's headset previously put
it at the right before correction. This confirms a relative quarter-turn,
NOT the requested final physical orientation; do not claim visual acceptance.
`tmp/vr-menu-page2-unchanged-current/live-scene-left.bmp` passes the fixed UI
transform assertion but is not byte-identical to the older capture (capture
conditions have not been established identical). GitHub #44/#47 rechecked:
still open with no new comments. No headset install or app shutdown occurred.

Latest horizon follow-up: user withdrew diagnostic installation and specified
a 90-degree clockwise correction in gameplay only; EX pre-game previews are
already correctly oriented. The EX entry/thin planet layer now gates that
correction on gameplay flow, including interpolated updates. Menu and Original
orientation regression checks pass. Quest APK rebuilt successfully, not
installed; physical orientation remains unverified. Removed the unsuccessful
CPU palette-cache experiment (colour mismatch) and extra orbit diagnostics;
the hole-fill and palette-boundary antialiasing fixes remain.

Sandbox integration follow-up: fresh Original and EX cartridge tests both pass
(19.06 s wall time). The fixture enters native pause, picks the actual player
mesh through the CPU bounds path, moves its rendered model without mutating
the live object, then unpauses, commits, captures the edited world position,
and runs another native tick. This strengthens the earlier isolated-box tests;
it does not replace physical controller/comfort validation. Only test/docs
changed after the last successful APK build; no headset update occurred.

Latest local pause-sandbox/control pass: Touch/Index triggers now map to L/R;
right/left grips map to Start/Select. Native aim spaces drive pause-only
controller rays. Trigger-edge grabs use model bounds, exclude HUD/backgrounds,
prevent simultaneous ownership, and release on invalid/untracked poses. Moves
remain render offsets while paused and commit once through the inverse source
view into live object coordinates on resume, guarded by pool generation.
Input and packet checks pass, including held-on-entry, two-hand ownership,
tracking loss and recycled-slot rejection. The Quest APK builds successfully;
this is not physical sandbox acceptance, and it has not been installed.
The final full VR suite passes all 15 tests (22.16 s wall time); the final
Quest rebuild also succeeds, including tracked-pose requirements. PID 15850
remains running unchanged. No install, app shutdown, commit or push occurred.

EX page placement is reproduced using native R-shoulder navigation: pages
1/3 report model vanishing point (76,92); page 2 reports (172,172). UI now uses
a fixed centered screen plane independent of that preview camera. All three
native page captures pass the fixed-transform assertion in
`tmp/vr-menu-fixed-page1` through `page3`; the camera regression checks both
origins and unchanged non-menu placement. Source menu text/layout is retained.
The sideways scramble report is still not reproduced: the fresh 180-tick
5-1 trace reports scroll (0,232) throughout the loaded entry, not a scroll-wrap
or rotating pitch. The latest headset screencap returned a zero-byte file,
so it supplies no visual evidence. Await an actual paused problem frame;
do not claim the orientation fixed or install over the running app yet.

VR style matrix now passes: seven styles, three intensities each plus OFF
(22 fresh Vulkan captures). Zero intensity is byte-exact OFF; 50% matches
the OFF/full midpoint within rounding tolerance; every full style changes
visible pixels. All seven full captures were inspected under
`tmp/vr-colour-style-matrix`. This verifies colour application/intensity, not
independent desktop-effect equivalence or headset performance. The reusable
check is `tools/check_vr_colour_styles.ps1`. Quest APK rebuilt successfully in
19 s including native-colour exemptions; it remains uninstalled. Earlier
statements that the reticle exemption is absent from the APK are superseded.

EX reticle follow-up: DrawPacket now carries `preserve_native_colour` through
resident and fallback draws; source aiming marks and shadows bypass styles.
Exact draw comparison includes the flag so resource reuse cannot retain a stale
policy. Fresh packet and EX cartridge tests pass (23.56 s). This latest change
is local and not yet included in the previously built Quest APK. All-style
GPU pixel verification and physical acceptance remain outstanding.

VR effects follow-up: startup/runtime menus now expose separate model and world
styles with intensity and defaults OFF. Shader-side cel colour bands (models
only), monochrome, sepia, thermal, night vision, pastel and vaporwave are wired
to native Vulkan model/background passes; HUD and menu text remain unchanged.
These are lightweight colour variants, not desktop screen-space outline/bloom
passes. Affine-view packing retains the guaranteed 128-byte push-constant size.
Preferences v4 stores effects, accepts validated v1-v3 records and preserves
default-off migration. All 15 VR tests passed after the camera/shader change;
updated menu and camera tests subsequently pass. GPU OFF regression and sepia
capture pass; `tmp/vr-effect-sepia-current/live-scene-left.bmp` was inspected.
Quest APK compiled in 43 s; subsequent native-shadow style exemption compiles
locally but requires another APK build. Effects are not installed/tested on the
headset. Native EX crosshair effect exemption and all-style pixel tests remain
to be checked. The earlier pending-effects wording below is superseded only
for these implemented styles, not the remaining desktop effects.

Latest user feedback: planet detail is crisp but shimmers with head movement;
the scramble horizon is still sideways; a black ring crosses the 5-1 backdrop;
EX pre-game page 2 shifts relative to pages 1/3. User also requests GPU-backed
2D/3D effects in VR (including cel and sepia). These remain active work, not
accepted features. The local shader now antialiases palette thresholds and
allows transparent orbital atlas samples to reach the surface continuation
instead of discarding the whole fragment. Fresh native Vulkan scene capture,
packet/camera checks and Quest APK build pass; this APK is not installed over
the running PID 15850. The orientation/page-placement reports and VR effects
integration are not fixed by that shader change.

Current local follow-up: surround sampling now uses 512 rather than 256
texels/radian, with matching unique-planet dimensions and orbital pitch.
Corneria and BG 18 rear captures were inspected in `tmp/vr-corneria-smaller`
and `tmp/vr-bg18-smaller`; artwork is smaller and the rear remains populated.
These scale changes are now installed on Quest serial `2G0YC1ZF8R059K`.
The lower hemisphere now uses higher-frequency, stepped-colour detail instead
of the soft gradient. `tmp/vr-ex-horizon-detailed/live-scene-left.bmp` was
inspected: EX 5-1 at tick 60 in VR-world mode has the horizon below the scene
and sharper surface detail. This is synthesized continuation, not new original
planet artwork. Physical acceptance and the reported right-side horizon in
EX 5-1/6-1/7-1 remain open; the desktop capture does not reproduce that report.
All 15 VR tests pass (42.86 s), including the bounded fence-wait change.
The Quest debug APK rebuilt successfully and `adb install -r` returned Success;
the launch intent was sent, but a subsequent PID check found no running game.
Do not count installation as on-headset rendering or performance verification.

EX showcase placement now scales its interpolated centre by 0.375 before both
GPU generation and fallback assembly, retaining original geometry and lighting
depth. Packet-only translation was insufficient for generated models and was
removed. The EX cartridge regression passes; the settled VR-world capture in
`tmp/vr-ex-showcase-pose-settled` shows enlarged visible models. Exact apparent
size relative to the EX logo and physical comfort remain unverified.

BG 18's explicit native-menu regression and rear capture pass. The shared full
atlas path no longer clears authored scroll offsets when scanline deformation
is disabled. Original 1-2 before/after captures place its meteor belt centrally;
EX 1-2 was also rendered. The latest installed APK includes these offset/menu
changes and removal of the title setback, but not the newer scale/showcase edits.
After installation, Quest initially required controllers; subsequent PID 12686
logs prove it ran and later entered XR_SESSION_STATE_IDLE. Sampled slow sections
show about 0.5 ms command submission versus 7–8 ms submission-to-fence observation
per eye, including polling latency; this is not GPU timestamp measurement.

Latest headset feedback rejects background acceptance: artwork is approximately
twice the desired size in menu/gameplay; orbital planet surfaces remain on the
right rather than below; procedural planet continuation is unacceptably blurry;
EX pre-game BG 18 lacks surround; 1-2's meteor band appears above/below rather
than centered. These override earlier desktop visual approvals. BG 18 now has
an explicit sphere/scroll dispatch and the title model's extra two-unit setback
has been removed locally; neither change is yet installed or visually accepted.
EX showcase models still need sizing against the EX logo's apparent size.
The latest screenshot attempt returned a zero-byte file and is not evidence.

Quest deployment update (September 13, 11:42 device time): with explicit user
authorization, stopped the old app and installed the current debug APK using
`adb install -r`; package manager confirmed Success, preserving app data.
QuestActivity cold launch succeeded and new PID 10324 reached OpenXR FOCUSED.
Inspected `tmp/quest-installed-verification.png`: both eye views contain the
Original title logo, characters, ship, PUSH START and surrounding stars. This
proves current on-device rendering, not stereo comfort or all-stage acceptance.
Startup included a 611.599 ms graphics-pipeline creation; telemetry initially
settled at 72/72 FPS but later samples during flow transitions fell to 58/72
with stale frames. Smoothness is therefore not accepted yet. No fatal error
was observed in the sampled process log. These results supersede the old
asleep/PID 10205 deployment blocker below, not other physical-device gaps.

Current Linux validation: the full non-embedded GCC build passes all 49 tests
(197.09 s), plus fresh Vulkan effects and stereo checks. Reconfigured with
release-style embedded patch/symbol resources, rebuilt successfully, and both
embedded startup and multi-region BIN lifecycle tests pass (11.94 s). The
embedded smoke fixture previously waited in the asset picker without a retail
input; it now receives the configured retail ROM and is only registered when
that input exists. This fixes test setup, not a claimed runtime startup defect.
Fresh Linux install manifests contain runtime, asset builder, user docs and
required notices, not development audits or ROM/BIN files. No publication or
physical Steam Deck/Quest acceptance follows from WSL testing.

Latest package audit: Quest was inadvertently bundling the NDK's `libandroid.so`
and `liblog.so` link stubs (their sysroot origin is recorded in AGP native-build
metadata). These are now excluded so the OS supplies the runtime implementations.
`build_quest.ps1` rejects either stub in any ABI. Rebuilt APK and the packaging
guard pass; no headset installation or runtime improvement is claimed. The
current full VR rebuild and all 15 tests pass (42.38 s); generated scene shader
source stamps also pass. Both APKs were refreshed at 04:24 before this additional
Quest-only packaging correction, superseding the historical stale-package notes
below. Regional desktop full-composition checks are complete as detailed below;
physical-device presentation remains unverified.

Colony reference breakthrough: the independent core stalled in the source
custom-rumble wait, not PPU execution. A signature-checked in-memory bypass
plus first-stage selection reaches Colony and reproduces its asymmetric
cross-section. The input ROM file and core source are unchanged; observer and
unmodified-core captures match exactly. These diagnostic overrides prevent
claiming natural-route/full unmodified-ROM parity, but rule against a guessed
renderer mirroring workaround. See BACKGROUND-AUDIT.md for exact evidence.

Regional full-title follow-up: `check_regional_titles.ps1` captures all six
language choices in Original/EX at 16:9/2x, validates title flow and expected
regional composition groups. It exposed a wrapped Original BG3 logo fragment
at x758..759/y64..75. Original title BG3 now stays centered; EX's repeatable
backdrop is unchanged. Native center pixels remain byte-exact for all six
Original choices, and all six EX frames are completely byte-exact before/after.
US/Japan/German/English-Europe and EX full compositions were inspected; fresh
script regressions pass in `tmp/regional-title-regression-{original,ex}`.
This strengthens desktop logo acceptance; headset/other-device titles remain
unverified. Existing Android packages predate this shared desktop-app change.

Latest Original policy audit: all 19 entries complete a 6,000-tick God Mode
preflight; no observed gameplay state selects the flat fallback. A scripted
Game Over still occurs in 2-4. This is mapping evidence, not visual completion.
New diagnostic guard tests pass, and Original Meteor rear was freshly rendered
and inspected. The current Quest APK predates this diagnostic-only CLI option.

Latest surround pass: all 40 EX entries completed a 6,000-tick no-input
policy trace (deaths included, not complete playthroughs). Fixed EX Sector X's
missing planet hemisphere and both cartridges' Meteor star surround. Fresh
EX front/rear images inspected; shared Original/EX regression tests pass
(22.22 s). Quest APK rebuilt successfully in 11 s with these changes, not
installed. Detailed limitations and capture paths: BACKGROUND-AUDIT.md.

Latest desktop regression: full rebuild, **55/55 tests pass (209.76 s)**,
including the standalone unsupported-DXR contract and exhaustive tunnel math.
Regular Android debug build succeeds (22 s). Quest is connected but asleep
(`mWakefulness=Asleep`) with old game PID 10205 suspended; that state cannot
provide current presentation/performance evidence and was not interrupted.

Fresh Original/EX pillar shadow captures pass ground-receiver, native fallback,
removed-override and exact CPU/GPU composition checks. Both DXR images were
inspected. Evidence: `tmp/current-ray-original-pillars/8b747e0f11924581ac58c9d3d41e2e66`
and `tmp/current-ray-ex-pillars/a7c80ef655cd41209000457a93d0a223`.
Explicit test mask requests now opt into diagnostic readback; normal gameplay
remains resident. This diagnostic-only app change postdates the full suite and
was verified by these runtime checks. The regular Android APK predates it.

Follow-up: Original Black Hole's Mode-1 surround omission is fixed and its
before/after rear images inspected. Fresh Original/EX late-Black-Hole VR input
regressions pass (21.35 s). Exhaustive tunnel-gradient input coverage also
passes both simulation-data suites; Colony's source composition is still open.

Latest special-route validation: full `build/vr-dev` rebuild succeeds and all
15 VR tests pass (27.63 s), including Original/EX game input and packet tests.
Quest debug APK rebuilt successfully in 17 s with the dimension, Black Hole,
Comet and planet-surface corrections. Comet front/rear images were inspected;
see BACKGROUND-AUDIT.md. A fresh ADB check still reports game PID 10205, so this
APK has not been installed over the active session. These results supersede
the older VR build/test timings below, not the outstanding visual/device gates.

Device recheck: Quest 3 serial 2G0YC1ZF8R059K is connected/authorized, but
com.starfox.enhanced.quest is running (PID 10205 on this check), installed
September 12 at 23:34. Deployment was intentionally refused before installation.
`deploy_quest.ps1` now checks for a live app and preserves the session; the guard
was exercised against this device. User was asked to close it when convenient.
This is only a device-install gate; local remaining work can continue.

Current complete desktop suite: **54/54** (197.74 s), after a full rebuild and
clean rerun. Three old black-only margin expectations were updated to the
fixtures' actual wall ink, retaining native-center and non-repetition checks.
Current rebuilt VR suite **15/15** (17.25 s), Quest debug package
build successful (19 s). Native Vulkan
and D3D12 shadow diagnostics pass after the initializer warning cleanup.
These suites do not establish all visual/device requirements below.

Fresh targeted checks: regional Original logo selection/upload/save-state tests
pass for all six languages; EX excludes Original replacements. US/Japan/German/
English-Europe isolated-layer BMPs were inspected (`tmp/region-logo-current-original`).
EX Ctrl-F1/F2/F3 real SDL-event save/load/slot checks pass with resident GPU
geometry/shadows; slot-1 overlay inspected (`tmp/state-hotkeys-current-ex`).

Follow-up audit: all 120 EX late captures completed and all 40 ultrawide samples
were inspected. LEVEL6_3 face-planet duplication fixed locally with native-center
byte parity and fresh captures. LEVEL5_2 tunnel margin color is also corrected
with unchanged native-center captures and a colored-wall regression test.
Colony asymmetry remains open. Details are in BACKGROUND-AUDIT.md.
`tools/check_runtime_options.ps1` now automates actual F1 open/resume/reopen
events and captures the menu; Original and EX both pass in fresh processes.
EX captured menu was inspected and shows Experience LOCKED. These event checks
prove opening/resuming, not every option or physical controller interaction.

| Requirement | Evidence and remaining work |
| --- | --- |
| Full GPU migration | Windows SDL Vulkan/D3D12 now have resident models, raster, composition, effects and hardware-shadow transfer. Original/EX 240 Hz captures match. See GPU-MIGRATION-STATUS.md. Broad scenes/effects, other adapters/platforms and performance acceptance remain. |
| VR build | Quest APK builds; the current Windows OpenXR suite passes 17 VR tests, with native Linux builds checked separately. Valve Index/OpenXR and physical Quest visuals/performance still require acceptance. |
| True ray tracing, default off, no duplicate native shadows | DXR hardware diagnostics and live shadow captures pass. `ray_tracing_{}` defaults off; GPU has no separate Enhanced Shadows pass. The user's later revision restores that option for Software only; it replaces native silhouettes. Wider caster/material/scene coverage remains. |
| Issues 43/44/46/47/48/49 | September 24 GitHub recheck: 43/46/47/49 closed; 44/48 open. Reporter confirmed 47 and the water portion of 48. The user-requested first-corridor ceiling/floor widening for 48 has fresh Original 16:9 and EX 32:9 GPU/Software final-image proof; reporter/device acceptance remains. Physical native Deck input is not proven. See ISSUES-43-49-VERIFICATION.md. |
| Photographic proof | Actual runtime BMPs are indexed by the feature documents, not generated mockups. No universal all-fixes physical-device proof. |
| ScaleFX | Five-pass CPU/GPU implementation; independent upstream GLSL and 60-case GPU reference checks documented in SCALEFX-STATUS.md. DXIL added and tested. Physical Metal/console coverage remains. |
| Smooth scramble opening | Contiguous horizontal-band interpolation, phase tests and 60/240 Hz source-program captures in PRESENTATION-PARITY.md. Later transitions still part of broad coverage. |
| Comms meter and avatar aspect | Source gate and 32x40 artwork/aspect handling; meter-on/off and Original/EX GPU/software captures in PRESENTATION-PARITY.md. Full language/device coverage not established. |
| Cheats submenu | God Mode, Level Select, Single/Dual/Beam, Infinite Bombs/Boost plus later Infinite Lives present; shared simulation/input/settings tests. Current menu navigation/physical controller acceptance remains. |
| Ctrl-F1/F2/F3 states | Save/load/slot window implemented; Original/EX fresh-process video/PCM continuation, corruption and archive tests in SAVE-STATES-STATUS.md. Broader boss/state/platform coverage remains. |
| Every 16:9+ background | Entry and later sweeps documented in BACKGROUND-AUDIT.md, including 120 completed EX late captures and review of all 40 ultrawide samples. Duplicate face-planets and colored tunnel margins corrected. Colony asymmetry is independently reproduced by the unchanged reference emulator with two diagnostic ROM overrides; do not hide it with a port-only mirror/recolor. Exact synchronized comparison and natural-route coverage remain unproven. VR wrap corrections have separate captures. Do not mark this complete. |
| Upgrade wireframe synchronization | FLASHPLAYER anchored to player interpolation; phase/wrap/recycling tests and 240 Hz turning capture documented in PRESENTATION-PARITY.md. |
| Android system bars | Activity hides bars on create/resume/focus using current and legacy APIs, with a post-resume attachment retry. Regular arm64 Android package rebuilt September 19 (20 s), required libraries/manifest/dex verified. Physical navigation-mode acceptance missing. |
| Regional logos | Fresh six-language final GPU title captures pass Original grouping and EX exclusion. US/Japan/German/English-Europe final compositions inspected in `tmp/region-final-current-original`; EX final title inspected in `tmp/region-final-current-ex` (all six hashes identical). This proves this Windows 16:9/2x/tick-200 fixture, not every device. |
| Half/Full SBS | Implemented. Current Vulkan Original Full and EX Half both pass 240 Hz dimension/submission checks with two resident hardware shadow masks. Stereo visual/depth/comfort acceptance remains. |
| F1 in-game options, experience locked | Fresh real SDL F1 open/resume/reopen checks pass in Original and EX; captured EX menu shows Experience LOCKED. Shared input tests reject changing experience. Broader submenu and physical input acceptance remains. |

Next engineering priorities: remaining EX background-transition coverage,
exact synchronized Colony/reference comparison, current cross-platform migration
builds/diagnostics, and a bounded physical headset session when available.
Physical-device gaps do not block these local tasks.
