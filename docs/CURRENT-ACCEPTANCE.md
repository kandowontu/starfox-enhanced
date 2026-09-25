# Current acceptance index

September 24 destruction follow-up: desktop and PCVR now interpolate retained
explosion-face progress between source frames without changing integer source
ticks. Original/EX fractional GPU-vs-Software `KICHI_0` checks pass 90 images
each; targeted desktop and VR tests pass. Newly spawned debris and full natural
death sequences still need visual/device coverage. See PRESENTATION-PARITY.md.

September 23 Intel GPU follow-up: a compact Intel-only DXIL clipping shader
avoids the prior forced D3D12 startup crash. An exact-binary64 GPU billboard
shader fixes the subsequent one-pixel sprite offset. All 59 numbered entries
and five special routes now pass strict sampled Intel D3D12 native/final
Software parity with zero CPU-image uploads; targeted Intel Vulkan and NVIDIA
D3D12 checks pass too. All 59 entries also pass forced Intel Vulkan. The
measured Intel D3D12 default is verified by Original/EX cold-start captures;
the ordinary Android arm64 Debug APK rebuilds and passes its payload check.
Physical-device performance and iPhone 4× acceptance remain open. See
GPU-MIGRATION-STATUS.md.

September 23 compositor follow-up: color and geometry metadata now share one
native sample calculation per output pixel. Vulkan/D3D12 compositor suites and
targeted 4× Intel Vulkan/1× D3D12 live parity checks pass; a single noisy
hidden-window comparison does not prove sustained FPS or resolve iPhone 4×
exits. See GPU-MIGRATION-STATUS.md.

September 23 GPU special-route follow-up: all five defined Original/EX
special-route entries also pass 60-frame strict zero-CPU-image-upload parity
checks on Vulkan and D3D12. At this checkpoint, one-thread and alternate
DXIL optimization diagnostics had not resolved forced Intel D3D12; the newer
Intel entry above supersedes that status. See GPU-MIGRATION-STATUS.md for
scope and evidence.

September 23 strict GPU stage sweep: all 59 numbered Original/EX entries pass
on both D3D12 and Vulkan at 1×/16:9, eight frames after 1,000 source ticks.
Each backend's 472 GPU frames has exact Software native/final parity, zero
CPU-image upload, and no scene readback/replay. This is sampled stage-entry
coverage, not every route frame or a physical-device FPS result; see
GPU-MIGRATION-STATUS.md.

September 23 Controls GPU migration: its high-priority foreground now starts
inside the ordered late GPU layer, removing a 465,920-byte first-frame CPU
image upload. Original/EX 1×, EX 4× Vulkan and EX 1× D3D12 sampled captures
match Software exactly, with zero upload/readback in 120 GPU frames each. See
GPU-MIGRATION-STATUS.md; device FPS is not established.

September 23 EX intro GPU migration: an early portrait/dialogue caused 14 CPU
image transfers (490,640 bytes) in 120 frames. Routing host ink through the
ordered late GPU layer removes them. Original/EX 1× and EX 4× sampled intro
captures match Software exactly; the strict audit reports zero CPU-image
uploads/readback on every GPU frame. This is not a full intro or physical-device
performance claim; see GPU-MIGRATION-STATUS.md.

September 23 GPU ending migration: the two constant inner cartridge border
strips now use compositor constants, eliminating their first-frame full CPU
image upload. Original boss-roll visible/closed captures match Software
byte-for-byte at 1×, with zero CPU-image upload/readback over 451 GPU frames;
its 4× opening frame also matches exactly with zero upload. See
GPU-MIGRATION-STATUS.md for scope and evidence. This is not all-scene or
physical-device acceptance.

September 23 resident GPU palette transfer: the compositor now skips the
staging map/copy pass when both CPU image and palette are unchanged. The 4×
pre-game menu uploaded one palette in 90 frames; a live EX 6-1 fade uploaded
two in 60 frames, with exact GPU/Software output in both fixtures. Vulkan and
D3D12 compositor suites pass. This reduces transfer work; no FPS or physical
iPhone/Android stability improvement has been established.

September 23 iOS physical startup: a sideloaded 0.0.6.7 package launched on
one iPhone after `Starfox-Assets.BIN` was supplied through iTunes File Sharing.
The published SDL file picker is unsupported on iOS; current source adds a
UIKit picker with main-thread dispatch, but it has not yet been built or tested
on the device. Two app exits occurred with 4× upscale (menu and gameplay),
while a brief 1× run did not exit. No matching crash report was available, so
the 4× fault remains open. See GPU-MIGRATION-STATUS.md for the desktop GPU
parity/performance evidence, which does not establish iPhone stability.

September 23 GPU transition audit: Original and EX Game Over and Original
BOOT match Software at 4× for 90 sampled frames each, with zero CPU-image
uploads throughout. The compositor metadata lookup now shares one source
projection; Vulkan/D3D12 compositor cases and 4× EX gameplay parity pass.
Intel integrated 4× performance is still poor and no consistent gain is
claimed. The ordinary Android arm64 Debug APK rebuilt and its package check
passed, but device-side GPU behavior is untested.

September 23 ordinary Android GPU startup mitigation: [issue #64](https://github.com/kandowontu/starfox-enhanced/issues/64)
reports a GPU-only freeze when entering preview or play; its reporter says
Software works. New/upgraded installs now select Software once, while GPU
remains an explicit option. A persistent GPU-in-progress marker recovers the
next launch to Software after a hang during renderer creation or the first
120 scene frames, re-arming on menu/scene transitions. Android `startup.log`
is now written beside `pregame.cfg` in app storage for device diagnosis.
The ordinary arm64 APK rebuild and package-content check pass. This is a
startup escape hatch, not a demonstrated Vulkan driver fix: no ordinary
Android handset is attached, and issue #64 remains open pending logcat and
physical retest. A GPU failure after the guarded transition window may still
require manually selecting Software.

September 23 native Linux regression: the current OpenXR PCVR player and full
target set rebuild, all 17 VR-labeled tests pass, and EX 1-4's enhanced cloud
scene passes llvmpipe Vulkan stereo readback. Windows/Linux left-eye captures
vary by at most one channel value across 829/65,536 pixels. This is not
physical Index/Quest or sustained FPS acceptance; see VR-BUILD.md.

September 23 palette audit: matched native/Enhanced Sky captures before and
after the live EX 5-1/6-1/7-1 sky changes show blue/green, deep/bright blue,
and blue/orange responses respectively with ground alignment retained. Further
1400/1600/1800 captures show 5-1's pink/blue/green and 6-1's gray-blue/orange/
pink intermediate phases; 6-1 matches the accepted desktop photographic hue.
The enhanced blue is less saturated than the cartridge by design. Every
intermediate frame and physical-headset color acceptance remain open. Exact
source ticks, captures and sample colors are in VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 follow-up: EX 1-4's live BG_1_14 now uses the same single
enhanced cloud and separate limb as preview 20; actual stage stereo capture,
body/fade/cache assertions, all 18 Windows VR tests and Quest package checks
pass. The EX 4-4 corridor's center stripes match its authored BG2 sample.
Desktop late-stage 32:9 parity samples cover EX 5-1 and both sides of EX 7-1's
orbital transition, including an explicitly allowed black fade frame. These
are local samples, not all-stage/source-console/headset acceptance. Evidence:
VR-ENHANCED-BACKDROPS-STATUS.md and BACKGROUND-AUDIT.md.

September 23 VR final rooms: EX BG_6_6C/D/E now surround the player instead of
remaining small rectangles with green borders. Accelerated authored continuations
from initialized levels reach all three at native brightness 15; front/rear
captures and full-sphere/mask checks pass. Andross can also be captured without
forced exposure through this route. Existing EX corridor capture is unchanged;
the subsequent authored BG2 center comparison is documented above.
Windows/Quest builds and complete APK resource checks pass; current evidence
and hash are at the top of VR-ENHANCED-BACKDROPS-STATUS.md. No release/deployment.

September 23 VR landscape landmarks: enhanced preview 4/5 now preserves the
small snow moon/desert mesa, plus gameplay 5-1/6-1 moon and 5-5/7-5 sun.
Captured an eight-row original-mountain seam and corrected the native receiver
to source row 352; final straight/banked images remove it. No rear mesa repeat.
Windows/Quest builds, landmark/horizon/fade/cache assertions, catalogue and APK
checks pass. See the latest VR-ENHANCED-BACKDROPS-STATUS.md entry for evidence
and hash. Space/final-room coverage, palette cycles and hardware remain open.

September 23 VR unique previews: menu backgrounds 1/3/10/12 now use enhanced
sky with their individual island/moons/sun retained at native atlas offsets.
Original/enhanced/rear captures inspected; live-palette, storage, fade and body
count assertions pass, along with Windows/Quest builds and the 37-asset APK
check. Remaining unique previews 4/5 and physical acceptance stay open.
Latest hash/evidence: VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 VR Cygard: enhanced storm now retains both correctly positioned
moons, independent live palette and landscape bank without rear duplicates.
Native/front/bank/rear images inspected; image reuse, palette/fade tests,
Windows/Quest builds and all 37 APK assets pass. Latest hash/evidence at the
top of VR-ENHANCED-BACKDROPS-STATUS.md. Unique menu previews and physical
headset acceptance remain open.

September 23 VR public control: Enhanced Sky is now an off-by-default saved
2D Options setting in Quest/PCVR, including Preview and runtime menu wiring.
Paused source revisions explicitly refresh on a toggle. Preference v1–4
migration, press-only input, localized effect keys, menu stereo layouts,
Windows/Quest builds and 37-asset APK checks pass. Physical live-toggle/preview
acceptance and remaining backdrop families stay open. Latest evidence/hash:
VR-ENHANCED-BACKDROPS-STATUS.md. GitHub rechecked: 43/46/47/49 closed; 44/48
still open, with no newer physical confirmation than the previously logged
Deck-native / Android first-corridor reports. No remote writes were made.

September 23 VR patterns/final room: sparse nebula and Sector K now occupy the
full sphere; red cloud preview and asteroid belts retain correct native height.
Enhanced panorama scrolling now interpolates via draw-time transforms. Exact
final-room classification removes its inherited tunnel mask while retaining
native glows; the inferior photographic vortex was rejected. Natural EX2-4,
menu front/rear captures, interpolation/storage/unit checks, 34 photographic
stereo fixtures, Windows/Quest and 37-resource APK checks pass. Final-room visual
proof is explicitly exposure-overridden because the internal-map-only fixture
starts dark; full natural transition and hardware acceptance remain open. See
VR-ENHANCED-BACKDROPS-STATUS.md for the latest APK hash and all evidence.

September 23 VR orbital: dedicated ocean/lava lower surfaces now blend with a
horizontal limb and full star surround; entry scenes retain one native-palette
moon. Natural EX 5-1/6-1/7-1 scramble and Original 2-2, menu ocean and downward
volcanic captures pass. All 34 photographic stereo fixtures, packet/catalogue
checks, Windows/Quest builds and 37-asset APK validation pass. No headset/install
or release claims. Remaining families/public settings and hardware acceptance
stay open; see VR-ENHANCED-BACKDROPS-STATUS.md for evidence and current APK hash.

September 23 VR ocean/volcanic: natural EX LEVEL6_2 and LEVEL6_6 now use enhanced
skies while retaining native island/lava ink overlays and the original sea/ground.
Banking uses the shared landscape transform; rear captures show no duplicated
eruption. Source atlas audits, 8/16-pixel flip/palette coverage tests, four live
stereo captures, 31 photographic GPU fixtures, Windows/Quest builds and all-35-
artwork APK verification pass. Orbital/other remaining families and hardware/
public settings acceptance remain open. See VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 VR city: enhanced EX preview 9 and natural LEVEL5_2 now surround
the viewer with the night skyline and six nonrepeating moons. Native palette
ramps use one shared GPU moon image; the redundant native moon patch is omitted
only on the enhanced path. Front/rear/banked captures, source-aligned ground
motion, packet tests, 31 photographic stereo fixtures, PCVR/Quest builds and APK
checks pass. No physical headset validation or release. Orbital/ocean/volcanic
families and remaining public settings stay open. Evidence and current APK hash:
VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 VR cloud/rim: Original 1-4 and EX preview 20 now use separate
photographic gas clouds and native-shaped atmospheric rims over a complete star
surround. Live rim palettes are GPU-mapped without texture regeneration. Black
interior/transparent-space tests, 25 photographic stereo fixtures, front/rear
captures, Windows/Quest builds and APK resource verification pass. EX 1-4 is a
different background and remains outside this fix. Natural EX cloud gameplay,
other remaining families and hardware/public settings acceptance remain open.
See VR-ENHANCED-BACKDROPS-STATUS.md for evidence and the current APK hash.

September 23 VR face planets: EX preview 18 and Original/EX `LEVEL_SPECIAL`
now render 13 enhanced face bodies plus three native saucers over a complete
star surround. Per-body native palette ramps run on the GPU; one immutable
photograph is shared in actual GPU storage, including across palette changes.
Front/rear and natural gameplay captures pass, as do packet checks, 22 stereo
photographic fixtures, Windows/Quest builds, shader freshness and all-35-asset
APK verification. Early LEVEL6_3 remains debris; its later face transition is
not yet captured. Hardware/public settings and other background families remain
open. Evidence and current APK hash: VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 VR registered planets: enhanced EX previews 19/27/28/31 now use
single cratered/storm/banded bodies over a full star surround. Source master
bounds and preview offsets are shared with desktop, including 19's corrected
lower placement. Original/EX storm-planet gameplay is captured and passes.
Planet scroll is fitted/interpolated before a rigid transform, retaining fixed
scale and GPU storage; menu/gameplay intermediate-frame checks pass. PCVR/Quest,
packet tests, all 19 stereo photographic fixtures, shader freshness and APK
verification pass. BG_3_4B/D are registered but their natural entry/departure
phases still need capture. Cloud/orbital/city/face families and hardware/public
settings acceptance remain open. See VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 VR Fortuna/twin moons: added unique distant photographic bodies
separate from repeatable skies. Fortuna uses the desktop's lighter-top/lower-haze
treatment and slightly darker sky; EX BG_5_4 retains two palette-shaded bodies.
Close-up live captures exposed an opaque photographic pipeline; dedicated alpha
blending now preserves the underlying sky through the fade. Native draw paths
remain opaque. Nineteen stereo photographic fixtures pass, including two-tone
shading and translucent-over-colour blending. Live Original/EX Fortuna, EX menu
32, twin planets, rear coverage and banked alignment pass; PCVR/Quest builds,
packet tests, shader freshness and APK content verification pass. Other unique
families, public VR settings and hardware acceptance remain open. Evidence and
current unsigned APK hash: VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 VR Sector K/game-over: Sector K now follows both source nebula
palette ramps while retaining neutral stars. Fifteen stereo Vulkan photographic
fixtures pass, including warm/cool/neutral live-palette cases. Game-over now has
native stars around both hemispheres; its opt-in enhanced version has a complete
round-star panorama behind the native foreground, with a cached exterior-black
cutout preserving enclosed facial details. Before/after foreground comparison
finds zero changed nonblack pixels in either eye (4,190/4,184). Windows/Quest
builds, packet tests, shader freshness and all-35-artwork APK checks pass.
No install or release; physical acceptance and remaining families still open.
See VR-ENHANCED-BACKDROPS-STATUS.md for evidence and the current APK hash.

September 23 VR asteroid/debris surrounds: both hemispheres now use the distinct
enhanced asteroid masters in Original/EX gameplay and EX menu 6/17, retaining
source belt offsets and removing duplicate native background geometry. Actual
Vulkan captures pass including the rear view; packet/mip tests and shader
freshness pass. Quest rebuild and all-35-artwork APK verification also pass.
Other special families and physical validation remain open.
See VR-ENHANCED-BACKDROPS-STATUS.md for capture paths and scope.

September 23 VR Titania weather: added a live per-shade cloud palette without
photographic texture reuploads. The natural controller-input replay reaches
the exact Original fog and gold/brown endpoints; both Vulkan captures pass
and were visually inspected. Stereo GPU shade fixtures match desktop mapping,
including zero texture uploads on a palette change. EX preview 26 also matches
desktop exposure. Windows and Quest build, packet tests and APK content checks
pass. Full-family migration and physical headset acceptance remain open.
See VR-ENHANCED-BACKDROPS-STATUS.md for evidence and the latest APK hash.

September 23 live VR landscapes: the development `--enhanced-sky` switch now
renders registered landscape photographs in actual Original/EX scenes. Native
ground is retained, with the same interpolated motion matrix as the new sky.
Vulkan captures pass Original/EX banked Corneria, EX 7-1, menu 26/36 and menu 13
rear coverage. Fixed stale gameplay palette use in EX menu previews. Display
blackout and signed palette shifts are checked; pixel/geometry caches are reused.
Quest release and complete-artwork package checks pass. This is a partial,
opt-in migration, not physical headset acceptance or complete palette-route
validation. At that checkpoint Titania gameplay and unique/orbital families
stayed native; Titania's subsequent migration is recorded above.
See VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 VR landscape projection: added an upper-only photographic sky
packet and baked seam/zenith preparation. Fixed coarse-mip zenith colour variation
found in captures. Six actual Vulkan front/side/rear views pass coverage, ground
exclusion and stable upper-sky colour assertions; packet tests pass. Live stage
projection/palette integration and menu enablement are still pending. Quest APK
is unchanged from the sampler checkpoint. See VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 VR photo sampler: added a separately validated immutable mip
pyramid with perspective-correct, trilinear sampling, wrap/clamp controls,
premultiplied-alpha edges and draw-time brightness/tint. All 35 masters build
valid pyramids; seven real Vulkan colour/sampling fixtures pass in both eyes,
alongside native scene checks. Quest compiles and APK content checks pass.
Scene projection/palette selection and the menu option remain unimplemented;
this is not physical headset acceptance. See VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 VR artwork delivery: Quest and PCVR now embed all 35 shared
authored BMP resources. Windows PCVR builds, all embedded images decode, and
Quest release compilation and APK payload checks pass. The workflow now
requires complete-artwork verification. This rebuild also includes immutable
VR texel storage. No headset installation/release; enhanced VR sky rendering
and its menu option are still not implemented. See VR-ENHANCED-BACKDROPS-STATUS.md
for exact artifact/hash and remaining projection/palette/device work.

September 23 VR resident-texture foundation: added immutable shared pixel
storage through geometry comparison, Vulkan validation/upload/reuse and
source-scene consumers. Windows packet tests and actual Vulkan readback check
pass, including zero pixel uploads for transform/vertex-only updates and
safe rejection of malformed payloads. No photo skybox is enabled yet; native
producers retain their existing representation. Quest rebuild remains pending.
See VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 VR backdrop foundation: extracted the desktop-only 35-artwork
catalogue and lazy immutable loader into shared rendering code. Desktop now
uses it; identity/cache/retry/preparation and Windows/portable resource mapping
checks pass. A current EX 20 final capture remains byte-identical. VR still
renders native backdrop packets: asset delivery, resident photo storage,
surround projection, options and device acceptance are not complete.
See VR-ENHANCED-BACKDROPS-STATUS.md.

September 23 palette follow-up: current EX 7-1 native/enhanced captures verify
the later atmospheric section before, during and after its one blue-to-orange
fade. The strengthened checker independently requires sky/surface changes
and stable ends: 24 sky/six surface phases pass, plus nine checker tests.
This is not all orbital/reflection transitions or physical acceptance.
See EX-BACKGROUND-PALETTES.md.

September 23 EX crescent follow-up: implemented independent cloud/green-limb
textures, source-aligned non-shearing centers and live bank-5 palette uniforms.
Also fixed a previously uncaught original-cloud duplicate in EX 1-4's far-right
ultrawide margin; central native columns stay exact. Two menu pairs and eight
moving gameplay capture pairs match CPU/GPU. Windows/Linux, D3D12/Vulkan,
hardware DXR palette/upload checks and ordinary Android build/payload checks
pass. This supersedes the choice-20 implementation gap below, not physical
acceptance. See EX-CRESCENT-UPGRADE-SEPT23.md.

September 23 long-frame profiling: buffered slow-frame diagnostics and moved
the final terrain log outside measured work. Two 6,000-frame EX 1-1 GPU runs
complete with enhanced/native medians 6.074/3.900 ms and maxima 10.354/8.751 ms.
No measured frame exceeds 12 ms in these samples. Earlier long-frame outliers
are not universally declared fixed; weak-device and real presentation pacing
acceptance remain open. See FRAME-TAIL-PROFILING-SEPT23.md.
Two additional 6,000-frame unenhanced software runs have Original/EX medians
3.311/3.996 ms and maxima 11.506/14.039 ms. EX has 22 frames above 12 ms;
the buffered trace records their costs without synchronous hot-path logging.

September 23 clipped-edge correctness: fixed a high/low-float intersection
rounding error that chose the wrong 4× scanline and adjacent triangle depth.
The original 72-view terrain CPU/GPU oracle now passes on D3D12/Vulkan,
along with 384 real-model images and the 96-case terrain/ray A/B checker.
See TERRAIN-CLIP-PRECISION-SEPT23.md; this supersedes the outstanding edge
discrepancy recorded in the preceding batching checkpoint.
The final corrected shader retains a 62–63% median batching improvement in
four matched runs (5.6–5.8 ms versus 14.8–15.6 ms). Isolated long frames
remain in the timing data and still need attribution. Linux checks and the
ordinary Android rebuild/payload verification pass; no deployment occurred.

September 23 enhanced terrain performance: grouped ten ordered patches per
GPU model submission without reducing geometry. Four matched 1× Original/EX
runtime cases improve median work from 13.3–13.7 ms to 5.2–5.6 ms. Four wide
Original final images are unchanged. D3D12/Vulkan A/B checks preserve pixels,
surface metadata, ray triangles and reflection materials across 96 cases.
The same checks and application build pass on Linux; all 32 captured frames
from four steered 120 Hz EX runs also match the unbatched renderer exactly.
See TERRAIN-BATCHING-SEPT23.md for measurements; this is not physical
low-end acceptance.

September 23 full EX menu sweep: captured and reviewed all 38 choices.
Corrected dark brown/blue enhanced-menu lettering with matched CPU/GPU
perceptual contrast; Windows D3D12/Vulkan and Linux Vulkan checks pass,
and final choices 13/14 match exactly. Corrected a stale-page capture
fixture for choice 32. This is single-phase menu evidence, not full-scene
acceptance. Choice 20's native crescent remains a known enhancement gap.
See EX-MENU-AUDIT-SEPT23.md.

September 23 EX city moons: all six native disks now have detailed cached
surfaces with source-positioned geometry and separate live palettes. Stars
sharing their ink remain untouched. Two 32:9 CPU/GPU menu pairs are exact;
973 changed pixels are confined to the visible disks, and native/off output
is unchanged. Windows/Linux, portable effects, DXR palette and Android build/
payload checks pass; 120 FPS menu scrolling remains uniform. See
EX-CITY-BACKDROPS-SEPT20.md for proof and limits. Physical acceptance remains open.

September 23 Android portability: fixed a Clang-rejected string-address
assertion, enabled actual -O2 native optimization in the shipped debug
variant, and restored eight missing asset build dependencies. The current
ordinary Android APK builds; all 35 enhanced backdrop payloads are verified.
GitHub packaging now enforces that check. Windows builds and Android/Quest
negative-package CTests pass. This local APK is debug-signed and was not
installed/published; physical FPS/fullscreen acceptance is still open. See
ANDROID-PORTABILITY-SEPT23.md for exact artifact hash and scope.

September 22 orbital-moon follow-up: EX preview 25/entry and asteroid BG_2_2
now have detailed banded planets using all eight live source shades. Their
cached atlas preserves the lower horizon surface and keeps moons round while
banking. Six final GPU/software pairs are exact; Windows/Linux builds,
portable effects on Windows/Linux Vulkan and hardware DXR tests pass. A
120 FPS moving capture was inspected. See EX-ORBITAL-MOONS-SEPT22.md for
evidence and limits; this does not close physical VR or the overall goal.

September 22 VR packaging follow-up: fixed the Windows long-path blocker and
built/validated the current unsigned Quest release APK. The Windows PCVR player
and install component build; CLI, dependency and application/save checks pass.
The existing GitHub jobs include permanent Quest signing and PCVR packaging,
but have not been dispatched/published in this pass. Physical headsets and
desktop-to-VR enhanced-artwork parity remain open. See VR-BUILD.md.

September 22 user-directed preview corrections: Fortuna's sky is slightly
darker; enhanced EX 19's moon is visible lower down; 26 retains detailed
clouds and readable text; 36/BG_COMET now show open space over a brilliant
comet, not a cavern. Enhanced EX menu lettering has a GPU/software outline
with clipped neighbor sampling. Four final 32:9 menu pairs are exact; three
unenhanced before/after pairs are unchanged. Fortuna differs by at most one
RGB step between renderers. Windows/Linux builds, hardware effects/DXR and
D3D12/Vulkan background parity pass. EX 5-4's phased cratered planets also
have exact 16:9/32:9 GPU/software proof. See BACKDROP-CORRECTIONS-SEPT22.md.

September 22 celestial-center correction: round-body stabilization now solves
the source affine center before removing shear. The previous formula shifted
planets with banking. Windows/Linux builds and independent forward-map tests
pass; two 180-presentation moving captures preserve source registration within
0.0012 pixel. EX 3-4's visible cratered planet was inspected while banked.
Remaining native celestial replacements are still open; see latest coverage.

September 22 Cygard correction: native atlas evidence fixes the enhanced
horizon from row 336 to 352. Landscape sky retains sky palette ownership when
banking below a fitted horizon. Thirteen final GPU/software capture pairs
match exactly; 60 source snapshots prove 31 monotonically converging palette
phases. The erroneous horizontal tint strip is removed and both native moons
remain. Windows terrain and hardware DXR tests pass. See the latest entry in
EX-BACKGROUND-COVERAGE.md; celestial replacements and overall goal remain open.

September 22 Game Over enhancement: dedicated deep-space artwork now fills
the sky without changing Andross, text or original star ink. Original/EX
32:9 on/off proof changes 81,191 blank pixels while authored ink remains
exact; GPU/late CPU/fallbacks match, full software differs at 14 pixels by
one RGB step. Native-center alignment and Half/Full SBS checks pass.
Windows/Linux builds and focused layer/image tests pass. See
GAME-OVER-COVERAGE-SEPT20.md for final captures and generated-asset provenance.
The late BG_6_3H face-planet phase is also visually confirmed at EX LEVEL6_3
tick 6000. Other celestial replacements, full transition and physical VR
acceptance remain open; the overall goal is not complete.

September 22 face-planet follow-up: enhanced EX preview 18 and Original/EX
Out of This Dimension now have detailed face moons recolored by their live
source palettes. Exact source-tile removal eliminates the old sheared face
fragments, without removing stars/saucers. Inspected 32:9 menu and gameplay
captures pass; EX gameplay GPU/software is pixel-identical. Windows/Linux
builds and focused image, effects and tile-parity checks pass. BG_6_3H's later
phase, remaining native celestial art, enhanced Game Over and physical VR
acceptance remain open. See EX-FACE-PLANETS-SEPT22.md.

September 22 special-menu follow-up: EX previews 23/24 now enhance their
actual BGLASTPCR radial pattern instead of retaining coarse rings or borrowing
an unrelated landscape. High-resolution analytic disks preserve the 512x256
source period, authored offsets and separate live CGRAM ramps. Windows/Linux
builds and backdrop tests pass; inspected 32:9 GPU/software captures differ
by at most one RGB step. This is not a new photographic planet or proof of
all remaining special scenes. See EX-BACKGROUND-COVERAGE.md.

September 22 portable follow-up: Linux now builds the current runtime and
passes all five focused checks (terrain, backdrop, spectral image, embedded
startup and new binary-resource round trip). This supersedes the interrupted
build noted below. Portable asset generation uses byte-string literals instead
of one C++ initializer per byte, avoiding the enormous initializer AST. The
asset-library rebuild measured 16.96 seconds and 2,679,488 KiB peak RSS; no
artwork compression, runtime decoding or asset-content changes were introduced.
Physical-device and outstanding background coverage remain open.

September 22 menu follow-up: enhanced EX landscape previews now use their
authored row boundaries rather than shared sky/ground palette ownership, and
ignore stale gameplay roll. This removes jagged native clouds over the replacement.
Post-fix 32:9 captures of choices 0/3/9/32/34/36 were inspected; every pixel at
and below each source horizon remains identical to its matching native capture.
Windows build and three focused terrain/image tests pass. Linux compilation
was interrupted without a compiler diagnostic; portable build validation is
not complete. See EX-BACKGROUND-COVERAGE.md for capture paths.

September 22 follow-up: EX preview 34 now has a dedicated spectral-gold cloud
image, preserving its complete brown rim and lower stars. Source GSTRATS2.ASM
calls this `venomhwymenu`; older references below calling preview 34 Cygard
are incorrect. At 32:9, software/GPU differ at five pixels by one color value;
all pixels from row 143 down are identical to the native preview. See
EX-BACKGROUND-COVERAGE.md. Fixed an independent out-of-bounds gameplay-name
lookup in the enhanced-menu sentinel iteration.

Space shadow exclusion now distinguishes Original Training's ground from EX
Training's starfield. The latter passes exact GPU off/readback/resident/
unavailable and software off/on image comparisons, with no shadow dispatch.
Windows build and focused terrain/image tests pass. Other unfinished goal
items and physical-device acceptance below remain open.

EX pre-game scroll now preserves history across ordinary image-page flips
and uses fractional presentation for native and enhanced backgrounds. Native
software/GPU captures match for 0/14/25/34 at 60 and 240 FPS, plus 9 at 120.
See EX-MENU-SCROLL-SEPT20.md for exact scope and remaining acceptance.

EX background 14 is cloud-only, no longer sharing background 13's snowy
mountains. `tmp/ex-snow-cloud-distinct-sep20` contains inspected 32:9 proofs
of both. Original ground bands/offsets remain; 14 does not inherit Titania's
weather palette ramp merely because it shares the cloud image. Mapping test
and Windows build pass.

EX menu backgrounds 9 and 11 now have distinct enhanced artwork, matching
their different original compositions. Choice 11's upper-sky bands are fixed;
32:9 proof and Windows tests pass. See EX-CITY-BACKDROPS-SEPT20.md.

Cygard's native palette fade is captured in 31 phases and exactly converges
to the cartridge target; it happens before the previously sampled late frames.
Its enhanced replacement remains pending. See EX-BACKGROUND-PALETTES.md.

Sector K enhanced clouds now use two live shade ramps, preserving black space
and neutral stars instead of a global tint. CPU/GPU tests and actual captures
pass; see EX-NEBULA-RAMPS-SEPT20.md for proof and remaining validation limits.

Restored shared EX water/Sector K/sun IRQ palette cycles in menu and gameplay.
Live CGRAM snapshots match changing authored tables; cadence/gating tests and
Windows build pass. See EX-IRQ-CYCLES-SEPT20.md for scope and capture evidence.

Independent original-ROM checks confirm native offsets for EX choices
6/17/18/20/23/24/29/34 at the sampled phase. Choice 36 has an isolated four-color
palette mismatch, not a demonstrated positioning error; investigation remains.
See EX-SPECIAL-REFERENCE-SEPT20.md for captures, measurements and scope.

EX menu planet suppression now fills margins with same-atlas stars instead of
blank rectangles for choices 19/27/28/31. Captures prove restored stars in
27/28/31 with unchanged native center; all CPU/GPU pairs match exactly.
Choice 19's sampled phase is unchanged. See EX-SPECIAL-STAR-GAPS-SEPT20.md.

EX menu 10 now has a dedicated photographic rocky-green ridge instead of the
generic procedural fallback. Native ground is unchanged; 32:9 CPU/GPU differ
at one pixel. Windows build and focused tests pass. See EX-ROCKY-GREEN-SEPT20.md.

September 20 Linux follow-up: the current Linux application and background
checker build successfully. Explicit Lavapipe Vulkan runs pass 432 background
cases (183997440 pixel/coverage samples), 54 ordered-layer cases, 192 staged
BG1 cases and 27 independent-size/phase cases. All 18 selected terrain/palette
and backdrop decoding tests pass, including the new red cloud-band asset.
This verifies portable correctness, not physical GPU performance or VR.

Fixed Original 3-5's native cyan corner with scene-scoped top-sky continuation.
CPU/GPU captures are identical; only 1139 margin pixels changed. Background
parity and existing hardware DXR checks pass. See ORIGINAL-RED-SKY-CORNER-SEPT20.md.

Original 3-5's cyan banked corner was transparent unused atlas rows above
row 256, not a model artifact; the continuation fix above addresses it.

Red-cloud gameplay was checked under opposite banks. Fixed clamped-top
vertical streaks through one-time asset preparation. The separate native cyan
corner was subsequently fixed as described above; see
EX-RED-CLOUD-BAND-SEPT20.md and ORIGINAL-RED-SKY-CORNER-SEPT20.md.

Original and EX 3-5 now use the correct enhanced red cloud artwork rather
than dusk mountains, with separate source atlas offsets and original ground
preserved. CPU/GPU proof is identical; see EX-RED-CLOUD-BAND-SEPT20.md.

EX menu 30 now has a dedicated enhanced red cloud-band panorama, covering
sky above and below without introducing ground. Windows build and tests pass;
32:9 CPU/GPU proof is identical. See EX-RED-CLOUD-BAND-SEPT20.md.

EX menu background 20 no longer duplicates its blue planet and green limb
in widescreen margins. Source-star resampling retains the native center;
32:9 CPU/GPU are identical. See EX-MENU-PLANETS20-SEPT20.md.

EX menu choice 1 now shares the daylight coast artwork at its own atlas
horizon 432 (gameplay uses 352). Enhanced Ground remains suppressed there.
Fresh source/CPU/GPU captures and tests are recorded in EX-DAY-COAST-SEPT20.md.

EX 6-2 now has a dedicated enhanced daylight coast, with its water kept
separate. New 16:9/32:9 captures were inspected; paired CPU/GPU differ at one
pixel. See EX-DAY-COAST-SEPT20.md. Other missing backdrops remain open.

EX gameplay 6-2/6-4 now use measured atlas horizon 352 and palette origin 224,
not the previous guessed horizon 368. Enhanced water/grass captures were
inspected and paired across CPU/GPU. 6-4 photographic sky coverage remains
open. Its sampled right-side wedge was isolated to a nearby TREE model, not
missing background fill; see EX-GAMEPLAY-HORIZONS-SEPT20.md.

EX menu 33 now has dedicated night-coast artwork with source stars and water
preserved separately. Fresh 32:9 CPU/GPU images differ at one pixel; water/UI
are unchanged. See EX-NIGHT-COAST-SEPT20.md for evidence and scope limits.

Immutable runtime backdrops now skip full image comparisons and duplicate CPU
cache copies in both SDL GPU effects and DXR. Mutable-image detection remains
tested. Cache-only benchmark and hardware checks are recorded in
BACKDROP-CACHE-SEPT20.md; this is not an in-game FPS claim.

EX menu 9 now has an enhanced city skyline while retaining source stars and
unique moons. Choice 11's clamped-top-row streaks are now fixed by one-time
city texture edge preparation; final CPU/GPU captures match exactly and the
image below the repaired edge is unchanged. No per-frame pass was added.
See EX-CITY-BACKDROPS-SEPT20.md.

EX menu 5 now uses the rocky-desert panorama, distinct from choice 8's dunes,
with the source row-432 horizon. Fresh 32:9 CPU/GPU captures differ at two
pixels and preserve native ground/UI exactly. See EX-ROCKY-DESERT-SEPT20.md.

EX menu 26 now uses the photographic snow-cloud backdrop with its live shade
ramp and exact row-360 horizon. Avoided double-applying EX's color transform.
Fresh 32:9 CPU/GPU captures match exactly; native ground remains unchanged
and Enhanced Ground stays suppressed. See TITANIA-WEATHER-SEPT20.md.

Original Fortuna now has photographic maritime clouds, with its single source
moon preserved independently and water/UI unchanged. 16:9 software/GPU proof
matches exactly; 32:9 was visually inspected. See
[Fortuna evidence](FORTUNA-BACKDROP-SEPT20.md). EX menu 32 now shares this art
with its verified row-360 source horizon and original scrolling. The 32:9
software/GPU capture matches exactly, with menu Enhanced Ground suppressed
and native ground pixels unchanged. Full-stage/other-EX/VR acceptance is not
implied by these samples.

Original Venom approach/escape cloud artwork is now mapped with its bank-5
shade ramp and shared row-360 horizon. LEVEL1_6 software/GPU captures match
exactly and leave the ground/UI region unchanged. A separate missing native
IRQ thunder/tunnel palette-flash transfer has now been restored and a live
thunder flash plus return to normal captured. Photographic lightning contours
are corrected by preserving the authored bolts separately from cloud shades;
the final software/GPU captures differ at one pixel. Source audit found tunnel
flashing disabled (no active activation), so no new flashes were introduced;
see [Venom backdrop and thunder gap](VENOM-BACKDROP-SEPT20.md). This is not
acceptance of complete Venom lighting or the escape scene's visual behavior.

Titania's weather changer is now exercised through normal flight input. The
native blue-fog to gold/brown transition is verified. Added a photographic
cloud backdrop using the live shade ramp, and fixed distant snow being treated
as water before the change to dirt. Snow/sand/dirt share the large terrain
patch optimization (530 to 45 patches in the replay). See
[Titania weather evidence](TITANIA-WEATHER-SEPT20.md). Stage-specific mirror/VR
visual validation is not implied by the passing shader/reflection fixtures.

EX 5-1/6-1/7-1 acceptance includes the authored starting palettes and every
subsequent color transition; 7-1 has one fade, not a repeating enhancement
animation. Enhanced palette response is derived from live CGRAM and the upload
palette without an independent timer. Removed per-channel division that made
small red changes in 5-1's blue palette over-amplify into pink/orange. A uniform
brightness multiplier plus signed RGB offsets now maps each source palette
mean to its live mean, preserving uniform fades, black endpoints, and colors
introduced into initially zero channels. CPU, GPU and DXR reflection shading
use the same transform. Captured
5-1 and 7-1 palette values are covered by focused regression tests. Full
transition visual acceptance is recorded separately from CPU/GPU parity.
See [EX palette fade evidence](EX-PALETTE-FADES-SEPT20.md), including the 6-1
shared-artwork calibration and rejected intermediate renders.

User clarification: Titania's weather changer replaces the backdrop palette
with dirt/brown colors. Required acceptance now explicitly includes the natural
weather-changer interaction, native and enhanced sky/ground before and after,
and reflections. Generic palette-response unit tests do not prove this specific
transition. Preserve the full color change, not merely brightness; do not mark
backgrounds complete until this has been exercised and visually verified.

EX menu 8 and gameplay 7-2 now use dedicated smooth-dune artwork, separated
from rocky desert scenes. The initial generated wrap-lighting stripe was
corrected in v2 and final runtime captures inspected. Current Windows build,
asset decode and mapping tests pass; full backdrop coverage remains open.

Golden storm now applies to EX 5-5/7-5 gameplay, and cloud plains to 7-1.
Fixed enhanced-landscape center scrolling to follow the source Mode-2 offset
table and corrected the storm horizon separately from palette sampling.
Fresh 5-5 ground region matches native exactly; CPU/GPU parity is within one
channel value for all three samples. 6-6 remains its distinct volcanic art;
four source samples through the boss showed no golden-storm transition.

EX menu background 22 now has golden storm-cloud artwork with the original
row-360 boundary. Windows build, asset decode and mapping tests pass; fresh
32:9 software/GPU captures differ by at most one channel value. Unique Cygard
face artwork remains excluded and pending, rather than erased by cloud art.

EX gameplay 3-1/7-3/7-4 now selects the corresponding red-dusk/alpine images.
New source-symbol/experience mapping tests pass; fresh PPU-backed 16:9 captures
show software/GPU parity within one channel value. Broader stage transitions,
palette animation and missing artwork remain open; see EX-BACKGROUND-COVERAGE.md.

EX menu choices 14/16 now use the existing alpine/red-dusk panoramic artwork,
with source-atlas-verified row-360 horizons. Fresh 32:9 software/GPU captures
match within one channel value (14) or exactly (16); Windows build and mapping
tests pass. Remaining backgrounds and gameplay mapping coverage stay open.

EX menu choice 7 now uses a cloud/low-hills photographic panorama with the
original row-360 horizon. Embedded Windows/portable asset registration and
decode/horizon tests pass; fresh 32:9 software/GPU captures differ by at most
one channel value. This does not complete the remaining backdrop coverage.
See EX-BACKGROUND-COVERAGE.md for captures and provenance.

Background acceptance now explicitly includes live palette transitions in
Sector K, Cygard and EX 5-1/6-1/7-1 (user clarification). Check the native
transition as well as the enhanced replacement and its reflections. The current
photographic path now supports separate palette-derived sky/surface responses
in CPU/GPU/DXR, with targeted parity and reflection tests. Do not declare the
enhanced backgrounds complete until all named stage transitions are verified;
see EX-BACKGROUND-PALETTES.md for remaining source/region/artwork limits.
The simulation already invokes EX per-transfer, fourth-transfer and
eleventh-transfer palette routines; source-code presence and cadence unit tests
are not a substitute for each named stage's natural transition captures.

Additional original-art 32:9 audit: `tmp/ex-menu-landscape-unique-before-sep20`
contains choices 1/3/4/5/10/12/32. Choice 32 visibly duplicates its moon at the
right edge. This is now fixed using same-row moon-free atlas sampling,
not a flat black replacement. New CPU/GPU captures are pixel-identical,
with the native center unchanged; see EX-BACKGROUND-COVERAGE.md. Wider
scroll-phase and other-scene coverage still remains open.

Queued after backgrounds: investigate missing enemy-death/explosion interpolation
at high presentation rates, covering state transitions and debris as well as the
enemy model. User report is not yet reproduced or fixed.

New queued requests: menu-only L+R held continuously for five seconds restores
default settings, with a countdown and release-to-rearm protection. Not implemented
yet. Investigate the reported GTX 750 Ti performance regression: GPU struggles and
Software drops into the 40s during heavy action; CPU, stage and exact settings are
unknown. Compare older/current builds under matching unenhanced settings and report
frame-time tails, not only averages. Do not infer this report is the local VSync
stall. The native-default benchmark now explicitly disables persisted environment,
material and manipulation settings; earlier runs may have inherited those options.

This is a navigation index, not a completion certificate. Feature documents
contain the actual runtime captures, commands, limitations and historical
checkpoints. Newer user instructions take precedence over the original goal:
VR/Steam physical validation is deferred, Software may keep Enhanced Shadows,
and the upgraded-laser label is Beam.

Current requested order (September 20): finish missing enhanced backgrounds,
especially all EX pre-game background selections and the scramble horizon
cutoff/black seams; then add the Quest VR APK to GitHub Actions; then complete
and package a PCVR version, including Valve Index compatibility. Existing
desktop OpenXR code is a starting point, not evidence of headset acceptance.
These additions are pending, not completed or published.

Latest EX background progress: all 38 menu choices now have independent Snes9x
reference captures using unmodified EX/controller navigation only, plus read-only
PPU register records. These supersede the guessed Mode 2 atlas-origin overrides:
host entry retained the wrong BG3 offset row and disabled its table. Correcting
both restores the nebula in choice 2 and Macbeth framing; Mode 1 retains its
individual source X/Y offsets. Menu BG2 is separated from UI. Ten menu choices
have photographic replacements, including city and orbital assets; EX orbital
entry scenes also have a replacement surface with the unique orange moon kept.
Full visual alignment and missing artwork remain open. Scramble atlases no longer inherit
checkerboard tunnel scanlines; source transition checks cover 5-1/6-1/7-1.
Remaining artwork and full visual acceptance are still open. See
EX-BACKGROUND-COVERAGE.md for exact evidence and limits.

September 20 latest environment batch: fixed integer division flattening the
enhanced horizon at small banks; shared CPU/GPU/DXR parameters now retain
fractional slope. Alpine v2 removes the baked edge sun/uneven sky exposure.
Auto ground uses the same material response as explicit selection; matching
banked Corneria Auto/Grass final captures are byte-identical. Mirror fallback
uses exact plane mapping with protected bilinear taps; photographic DXR
reflections retain subpixel coordinates. Windows D3D12 and Linux Lavapipe
effects checks pass; hardware DXR checks include seven distinct colour steps
within one native-pixel camera movement and time-invariant mirror receivers.
Evidence and limitations: ENVIRONMENT-OPTIONS.md. No headset/release update.

Comms submission timing now follows MCOPYFACE before source counter changes,
fixing a one-update-early meter opening and closing. Original/EX source-bitmap
meter tests and EX alternate-channel timing pass, including boundary save/load.
The new tests are registered; this is not a fresh full-suite result. See
PRESENTATION-PARITY.md.

Latest terrain follow-up: GPU terrain now supplies the lighting/reflection
normals required by active effects. Full Corneria 4x grass/dirt/sand/snow
native and presentation captures match software exactly; generated terrain
batch coverage passes on Windows D3D12 and Linux Lavapipe. Sand/snow retain
irregular relief and flat stretches; grass extends farther with medium-distance
blades. Dirt now uses muted loam coloring rather than amplified orange. Current
Windows/Linux builds and effects checks pass; this is not all-stage visual or
sustained-performance acceptance. Details: GPU-MIGRATION-STATUS.md.

September 20 follow-up: photographic Aurora now honors Sky Motion OFF in
software, portable GPU and DXR environment reflections. The effects checker
independently compares widely separated times for all four photographic and
procedural sky styles, requiring exact frozen output when OFF and changed
output with drift enabled. Fresh Windows D3D12 and Linux Vulkan effects checks
pass, as does the hardware DXR checker. This is targeted environment validation,
not proof of every backdrop or completion of the migration.

The stage-sweep harness now explicitly disables all six persisted environment
fields, model manipulation and material, rather than inheriting saved upgrades.
Fresh Original/EX LEVEL1_1 checks (1,000 source ticks, 12 frames, 60 Hz, 1x,
16:9, D3D12) pass exact native-raster and final-presentation parity in
`tmp/gpu-stage-baseline-sep20`. This is two baseline fixtures, not a repeat of
the full stage sweep or a sustained frame-rate acceptance result.

Latest combined local checkpoint (September 20): complete Windows build and
all 61 configured CTests pass, 156.73 seconds after the headless startup fix.
Dummy-driver tests no longer load the optional DLSS runtime and smoke tests
pin baseline options rather than inheriting saved enhancements. Real-window
DLSS startup/binding/shutdown still passes. Hardware GPU comparisons remain
separate targeted checks described in their feature documents. The test log
is `build/current/Testing/Temporary/LastTest.log`.

The fresh 32:9 stage-entry sweep passes all 64 samples (21 Original, 43 EX),
including special routes, with exact native/final CPU-GPU parity. Evidence is
`tmp/gpu-stage-ultrawide-sep20`; see BACKGROUND-AUDIT.md for inspected images
and explicit scope limits. This does not prove every later transition.

| Requested requirement | Current evidence location | Remaining acceptance boundary |
| --- | --- | --- |
| GPU migration | GPU-MIGRATION-STATUS.md | All-scene/effect combinations, sustained performance and physical non-Windows hardware |
| VR build / Index compatibility | VR-BUILD.md, VR-EX-SPAN-MIGRATION.md | Physical Quest/Index presentation, comfort and performance; work currently deferred |
| True ray tracing, off by default, shadow replacement | RAY-TRACING-VISUAL-AUDIT.md; `ray_tracing_{}` in GameSimulation | Wider scene/material/adapter coverage; no claim that FSR enables unsupported RT hardware |
| Issues 43/44/46/47/48/49 | ISSUES-43-49-VERIFICATION.md | Native Deck confirmation for 44, physical Android coverage; 43/49 closed and reporter confirms 47 |
| Photographic proof | Runtime capture paths in each feature document | Missing physical-device/all-route evidence; generated backdrop artwork is not proof |
| ScaleFX | SCALEFX-STATUS.md | Physical Metal/console acceptance |
| Smooth scramble opening at high FPS | PRESENTATION-PARITY.md | Broader natural transition sequences/device coverage |
| Comms meter gating and portrait aspect | PRESENTATION-PARITY.md | All natural conversations/custom layouts, beyond sampled six-language fixtures |
| Cheats submenu and requested cheats | Runtime input/simulation/state tests; GOAL-ACCEPTANCE.md | All-stage/controller interaction beyond tested routes |
| Ctrl-F1/F2/F3 states and slot UI | SAVE-STATES-STATUS.md | Broader full-composition/boss/platform continuation |
| Every widescreen background | BACKGROUND-AUDIT.md | Synchronized full-source Colony composition and unsampled transitions; do not hide the authored door |
| Upgrade wireframe synchronization | PRESENTATION-PARITY.md | Wider natural collection/device coverage |
| Android fullscreen system bars | StarFoxEnhancedActivity.java; GPU-MIGRATION-STATUS.md | Physical gesture/navigation-mode checks |
| Regional Original logos | GOAL-ACCEPTANCE.md regional capture entries | Physical-device presentation |
| Half/Full SBS | GPU-MIGRATION-STATUS.md; stereo tests/capture harnesses | Stereo comfort/depth acceptance on actual displays |
| F1 menu, locked experience | Runtime input tests; runtime F1 capture harness | Broader settings/controller combinations |

## Additional user-requested work still open

- Enhanced Sky includes Corneria/Original Training, Macbeth, Titania, Venom,
  Fortuna clouds and several EX landscapes/orbital scenes. Full coverage,
  unique moon upgrades and remaining EX menu mappings are unfinished.
- Enhanced terrain has geometry and targeted captures, not all-stage visual or
  sustained-performance acceptance. Ray secondary-hit fidelity and the horizon
  blend still need broader review.
- Both SDL effects and DXR now cache unchanged backdrop image uploads;
  sustained all-scene performance still requires wider measurement.
- Hardware-dependent acceptance must not be inferred from a successful build,
  a mock device, a package inspection or a software reference comparison.

Keep the full goal active until its actual requirements are proven. Do not
close open GitHub issues, publish a release, deploy to a headset or discard
user data merely to make this index look complete.
