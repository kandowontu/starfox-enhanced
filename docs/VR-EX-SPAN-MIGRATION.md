# Remaining EX span migration

## September 19 current-state correction

The historical connection blockers below are superseded. `SourceModels` now
prepares warp inputs and expanded primitive templates for live compute models;
`VulkanSourceModel` owns and records `VulkanWarpBindings` in its producer chain.
The current native Vulkan scene checker exercises resident warp bindings through
PRNG/material/expansion/clip/spans, including graphics, and passed again during
the September 19 particle migration verification. See GPU-MIGRATION-STATUS.md
for current evidence and limits. This is not universal EX mode/scene acceptance;
do not restart implementation of these already-connected components based on
the older September 11 notes below.

Source audit: 2026-09-11. This is an implementation handoff, not a completion claim.

## Latest palette work

The new `VulkanWarpBindings` wrapper now has GPU execution evidence. A fixture
allocates the combined arena, uploads the prepared inputs/settings, dispatches
the real projection/visibility/BSP producers and then the wrapper's three warp
stages without intermediate CPU readback. Exact seed-1234 descriptors and
expanded materials pass in `tmp/vr-bound-warp-proof`, as does the full host
scene suite. This closes the wrapper-compilation-only gap below. Connecting
its expanded outputs to resident clipping/graphics and live scene selection
remains required; this test does not establish live warp completion.

`VulkanWarpBindings` now constructs the three warp descriptor groups over a
combined source/warp arena and records PRNG/decode/expand with compute barriers.
The host library compiles. This new wrapper has not yet been exercised by a GPU
fixture or connected to live scene dispatch; that verification was interrupted
to prioritize the user's ground-attachment report. Do not treat compilation
as proof of correct descriptor wiring.

Warp input-write generation now serializes normals, shading/texture tables and
all three shader uniform blocks into the validated appended layout. It emits
no writes to generated descriptors, traversal results, decoded materials or
expanded geometry. Tests cover signed scroll serialization, empty inputs and
overlapping-layout rejection without replacing prior writes. Build and packet
tests pass. Actual Vulkan upload/binding and live dispatch remain unconnected.

Warp arena layout now supports appending its inputs and per-occurrence outputs
after the ordinary resident model arena. All ranges respect device alignment,
storage/uniform limits and the 256 MiB total budget; empty texture arrays retain
dummy descriptors. Tests cover prefix alignment, 32 corners per occurrence,
oversized counts, uniform/storage limits and transactional rejection. Host build
and packet tests pass. Allocation sizing is implemented, but actual storage
allocation/upload, descriptor binding and live dispatch are still pending.

Resident warp input preparation now exists in `prepare_source_warp_inputs`:
shared material-free topology, texture/shading packing, source-ID normals,
depth tables and projection-derived wrapping PRNG seed. Tests cover seed wrap,
face counts/normals and transactional rejection of destruction; host build and
packet tests pass. This prepares inputs only. It is not yet called by live
scene assembly, nor are its outputs allocated/bound/dispatched there. Destruction
and axis modes remain explicitly unsupported rather than rendered incorrectly.

The mixed occurrence fixture now uses a 2x2 checker texture with explicit UVs
and a one-texel horizontal scroll. Both-eye pixel assertions independently
calculate the expected quadrants, and the left capture was visually inspected
(`tmp/vr-patterned-occurrence-proof/span-depth-18-eye-0.bmp`). The full host
diagnostic passes. This replaces the constant-texture-only limitation of the
earlier fixture, but does not enable generated warp buffers in live scenes.

Mixed solid/textured occurrence graphics now has a resident fixture: slot 0
keeps green span coverage while slot 1 samples a blue indexed texture through
the ordinary polygon path. Both eyes match every pixel, including the distinct
solid-span versus textured-polygon right-edge conventions, in
`tmp/vr-textured-occurrence-proof/span-depth-18-eye-{0,1}.bmp`.
The full host scene diagnostic passes. This uses a constant one-texel texture;
nonconstant UV/scroll sampling, GPU-generated warp outputs and live dispatch
still need integration/verification. No additional headset installation made.

Warp shading input packing is now shared alongside texture packing in
`packed_faces`: source lighting, override flags, scroll, shade counts and
zero-padded diffuse tables have one implementation. The desktop GPU path uses
it; resident Vulkan integration can consume the same data. Host packet tests
cover all four padded bands and settings, and the scene diagnostic passes in
`tmp/vr-shared-warp-input-proof`. Source diffuse tables are fixed at 12 entries,
so this refactor does not establish or claim a previously reachable overflow.

The two-face resident fixture now also renders both eyes through
`VulkanSourceModel::record_graphics`, using the same GPU arena as compute.
Every output pixel matches separate green/blue occurrence materials and source
span bounds in `tmp/vr-two-face-graphics-proof/span-depth-17-eye-{0,1}.bmp`.
The left image was visually inspected; the full scene diagnostic passes.
This verifies multi-slot graphics consumption for authored solid materials,
not yet generated warp textures or live warp dispatch. No CPU command readback
is re-uploaded to produce these images.

The resident Vulkan model diagnostic now dispatches a two-face mode-2 model
through the actual arena/bindings, reads both BSP order entries and verifies
separate geometry and material spans (17 versus 231). It passes in
`tmp/vr-two-face-occurrence-proof`, along with the existing scene suite.
Unlike the standalone warp arena, this checks live-model allocation and
descriptor construction. This fixture reads commands only; multi-occurrence
graphics consumption and live warp dispatch are still not established.

Live arena allocation now preserves the complete BSP order output even in
occurrence span mode 2. Previously it inherited the span consumer's four-byte
dummy range, despite the preceding BSP producer writing every occurrence.
A two-face regression fails before the fix and passes afterward. All 14 VR
tests and the host Vulkan scene diagnostic pass (`tmp/vr-occurrence-order-arena-proof`).
This fixes a prerequisite allocation defect; live warp enablement remains pending.

The ordered warp fixture now continues through clipping and span generation
without intermediate CPU readback. Repeated occurrences produce independent
textured/solid command materials; invalid and empty traversal clears commands,
followed by successful arena reuse. Host run `tmp/vr-warp-span-chain-refresh`
passes and its 100 BMPs exactly match `tmp/vr-occurrence-graphics-proof`.
This verifies the compute chain, not repeated-material graphics consumption or
live colour-warp eligibility. Those integration steps remain required.

Physical Quest 3 / Adreno 740 refresh also passes this chain and the complete
standalone Vulkan scene diagnostic. All 100 pulled BMPs in
`tmp/quest-adreno-warp-chain-proof` exactly match the host refresh above.
This is actual device GPU dispatch/readback, not optical headset captures or
sustained in-game performance evidence.

Resident graphics now recognizes lookup flag 4 as occurrence-indexed polygons,
clipped geometry and materials. Traversal success/count and polygon capacity
still bound every lookup; palette/sRGB flags remain independent. CPU arena
validation accepts this flag only with span mode 2 and rejects unknown bits.
The resident solid-cover fixture exercises this mode; all 100 host BMP captures
in `tmp/vr-occurrence-graphics-proof` exactly match the baseline, and all 14 VR
tests pass. This single-slot graphics fixture does not prove repeated-material
live rendering. Expanded GPU output buffers still need binding to these views,
followed by repeated-face texture tests before live colour warp is enabled.
No headset update or migration-completion claim is made by this change.

The span shader now supports ordered mode 2 for occurrence-expanded polygons
and materials: validate traversal status/count, then address the occurrence
directly instead of applying source-face order a second time. Buffer validation
requires sufficient expanded polygon slots and only a dummy source-order
binding. GPU tests deliberately use an invalid source-face index with a valid
expanded slot, then failed traversal and recovery across all six span modes.
The 12-scenario x 6-mode host chain passes (`tmp/vr-occurrence-span-proof`).
Existing direct/BSP modes remain unchanged. This is a live-warp prerequisite,
not live colour-warp enablement: expanded arena bindings and occurrence-aware
graphics lookup still need connection. The installed Quest APK is unchanged.

All three warp stages now execute in a chained Vulkan fixture with compute
barriers and no intermediate readback: descriptor PRNG, material decode and
geometry expansion. Exact repeated-face polygon offsets, independent materials,
texture UVs, hidden output and invalid/empty reuse assertions pass in
`tmp/vr-warp-expand-chain-proof`. This remains a diagnostic fixture; live model
arenas and graphics consumers still need integration. No FPS gain is claimed.

Ordered colour-warp dispatch now has native Vulkan execution evidence, beyond
pipeline creation: seed 1234 matches exact source descriptors 29351/14600/63021,
hidden occurrences do not consume words, destruction does, and failed/invalid/
empty traversals clear stale output on arena reuse. Five cases and the existing
scene suite pass (`tmp/vr-warp-sequence-proof`). Live arena integration and
material-stage dispatch/graphics consumption are still pending.

Native Vulkan wrappers now create both shared colour-warp compute stages:
ordered descriptor generation (one serial workgroup) and per-occurrence material
decode (64 lanes). Descriptor layouts match the existing portable shaders;
creation/reinitialization and existing scene regressions pass in
`tmp/vr-warp-material-pipeline-proof`. These stages are not yet connected to
resident model arenas, dispatched in live scenes, or consumed by graphics.
Next: bind occurrence outputs and validate repeated-face PRNG sequences before
opening live eligibility. Pipeline creation alone is not colour-warp completion.

Forced-colour models no longer fall back merely because their colour-warp flag
is set: native forced colour overrides descriptor selection. Resident material
regressions match the non-warp forced-colour path. Actual random colour warp
remains gated; its desktop implementation produces occurrence-based materials
after BSP traversal, whereas VR currently indexes materials by authored face.
Migration must preserve repeated BSP occurrences and random consumption order,
not substitute one random material per face.

Overlapping effect/decal GPU evidence: case 15 now repeats the waved solid's
authored surface with a textured polygon. The undeformed texture wins the
shared-depth region, while exposed wave pixels remain outside it. Both eyes
pass exact pixel assertions; left image inspected at
`tmp/vr-overlap-effect-proof/span-depth-15-eye-0.bmp`. This is one frontal wave
pose, not all effects, oblique views or headset acceptance.

Fixed a mixed-effect decal omission: exact authored texture/solid overlays now
receive the same graphics depth-bias marker as ordinary models. Regression tests
verify coincident wave-model surfaces are marked and genuinely separated ones
are not, while clipping still excludes the texture. Packet and Vulkan suites
pass (`tmp/vr-mixed-decal-regression`); that GPU suite does not yet prove an
overlapping effect/decal image, so visual acceptance remains outstanding.

Mixed primitive eligibility is now connected for textured polygons too. Case 15
contains a waved solid, native line, textured sprite and textured quad in one
model; both eyes pass exact coverage/colour checks at
`tmp/vr-mixed-effect-texture-proof`. This verifies separated primitives, not
overlapping decals or all effect/near-plane combinations. Colour warp and
destruction remain separate migration gaps.

The mixed wave fixture now also contains a textured non-square sprite. Both eyes
pass exact sprite colour/coverage checks alongside the unaffected line and waved
solid (`tmp/vr-mixed-effect-sprite-proof`). Live eligibility permits affected
solids with lines/sprites; affected solids with textured polygons remain gated.

Mixed solid-effect/line draws now select their vertex path per BSP face, preserving
source order. The wave fixture includes an unaffected line in the same model;
exact coverage checks pass in both eyes (`tmp/vr-mixed-effect-line-proof`).
Live eligibility now permits this combination. Mixed affected solids with
textures/sprites remain gated pending equivalent tests; this is not full migration.

Mixed-effect topology groundwork: graphics now reads a dedicated polygon
descriptor region with intact native corner counts, while solid clipping keeps
its zero-count exclusions for textures/lines/sprites. Both retain BSP face IDs,
materials and corner storage. Packet tests verify a line remains available to
graphics but excluded from solid clipping; Vulkan suite passes in
`tmp/vr-mixed-topology-proof`. Per-face effect/ordinary shader selection and
mixed-effect consumer wiring remain pending.

EX effect eligibility now follows native primitive selection: if a model has no
untextured solid polygons, cel/wave/wobble/wire flags cannot affect it, so it
uses the ordinary resident path. No effect flags are stripped from affected
solids; explicit bypass still rejects those. Sprite fixtures verify unchanged
topology/UVs/texture bytes for all four flags; Vulkan suite passes in
`tmp/vr-unaffected-effects-proof`. Models mixing affected solids with other
primitive types still need the combined effect/ordinary consumer.

Real decal comparison: `--resident-eyes-mario` and `--resident-eyes-luigi`
reuse the legacy front-facing eye fixture with resident geometry/materials.
Both models match legacy BMP hashes byte-for-byte in both eyes; Luigi's left
capture was inspected and eye artwork is visible. Neutral diagnostic palette,
not gameplay-colour proof. Artifacts `tmp/vr-eye-{legacy,resident}-{mario,luigi}`.
Mario's generic pitched model comparison also matches (`tmp/vr-decal-*-mario`).

Ordinary sprite models enabled after real-art comparisons. The new
`--resident-model=NAME` diagnostic keeps the legacy camera/palette while routing
that model through resident compute/graphics. EX BLACKHOLE (64x64) and FIRE
(32x32) match legacy BMP hashes byte-for-byte in both eyes under perspective.
Artifacts: `tmp/vr-sprite-{legacy,resident}-{blackhole,fire}`. This proves these
sampled models, including the 64px sizing exception, not all sprite animation,
colour-warp or destruction states. Alternate-effect models remain gated.

Cartridge sprite inventory is available through packed_faces_tests' optional
`--list-sprites` argument. EX has 55 sprite faces; FIRE is 32x32 and BLACKHOLE /
BONFIREBALL are 64x64 targets. Model capture bounds now include sprite half-size
(including the 64px exception), avoiding zero-radius rejection for centre-only
models. EX BLACKHOLE legacy capture succeeds in `tmp/vr-sprite-legacy-blackhole`
with neutral diagnostic palette, providing a real-art baseline for resident
comparison. This is baseline evidence, not migrated sprite parity.

Coverage audit: enabling sprite eligibility temporarily did not add a sprite
in EX LEVEL2_3:400 (14 resident lines, zero sprites). Both-eye captures still
match legacy, but cannot establish sprite parity. Capture diagnostics now print
resident line/sprite counts; live sprite eligibility was restored to gated.
Artifacts `tmp/vr-sprites-live-ex` are regression evidence only, not sprite proof.

Resident sprite fixture now dispatches source projection/BSP and draws a 2x1
indexed sprite at a GPU-fetched centre. Both eyes produce exactly 128 magenta
pixels (16x8), proving square billboard sizing plus non-square artwork clipping
in this orthographic fixture. The expected Y interval accounts for its identity
projection, which maps positive eye-Y downward. Full Vulkan suite passes in
`tmp/vr-resident-sprite-proof`; capture inspected. The 64px exception, perspective
scaling and real-cartridge comparisons remain unverified; live sprite gate stays.

Resident sprite shader groundwork: unclipped preparation retains one-centre
sprite topology and reserves a four-corner fan. The shader fetches the projected
centre, builds eye-facing offsets with the native 64px-sheet size exception,
ignores polygon scroll for sprites and clips non-square art before indexed
sampling. Invalid/untextured sprite faces suppress output. Packet tests cover
topology/capacity and invalid centre count; existing Vulkan regressions pass in
`tmp/vr-sprite-shader-regression`. Sprite-specific GPU sizing/depth parity is not
yet verified, so live sprite routing remains disabled.

Ordinary mixed polygon/line models enabled in live assembly. Original asteroids
LEVEL1_2:400 moves from 10 to 16 resident models; EX Titania LEVEL2_3:400 moves
from 8 to 10. Both sampled scenes match legacy BMP hashes in both eyes, as does
Original Corneria. Artifacts: `tmp/vr-mixed-asteroids-original`,
`tmp/vr-mixed-live-ex`, `tmp/vr-mixed-live-original`. Synthetic Vulkan suite passes.
Sprites and mixed EX span-effect models still use their existing paths. These
samples do not prove all line crossings/near-plane cases or headset performance.

Resident line templates implemented for unclipped models. GPU shader selects
line versus polygon templates from ordered face metadata and fetches both line
endpoints from resident projection output. The consumer interleaves triangle
and line ranges per slot; no line resources are allocated for solid-only models.
Retained compatibility includes line presence. Packet tests retain two corners;
Vulkan case 14 adds a visible resident blue line alongside textured models and
checks all 64 line pixels in both eyes (`tmp/vr-resident-line-proof`). Captured
line rasterization occupies x=119 at the integer boundary, not the initially
assumed x=120; the failed capture exposed the incorrect expectation. Full suite
passes. Live mixed-model routing remains gated pending legacy comparisons,
particularly order, crossings, near-plane clipping and line width.

Mixed-primitive groundwork: VulkanScenePipeline now supports primitive-aligned
vertex ranges with overflow/bounds checks, retaining one buffer while allowing
ordered per-slot line/triangle submissions. The existing record API delegates
to a full range. Vulkan span case 4 submits each triangle separately and checks
identical expected pixels in both eyes; malformed ranges reject before drawing.
Synthetic suite passes (`tmp/vr-primitive-range-proof`). Resident line templates
and their shader selection are the next missing pieces; no mixed models enabled
by this change.

Shared face packing now strips texture sampling from two-point lines, matching
native draw_line and VR shape_batch. Previously descriptor-resolved art was
uploaded and tagged textured even for these colour-only primitives. Sprites
retain textures. Regression covers a line with unusable irrelevant art; both
cartridge packaging audits pass (Original 2,697 models/808 lines; EX 3,511
models/1,231 lines). Vulkan synthetic suite passes in `tmp/vr-line-material-proof`.
This corrects shared inputs; VR mixed-line resident graphics remains pending.

Solid-only preparation skips decal surface allocation/sorting; textured keys
reserve corner capacity. The 64-face packet microbenchmark measured 9.30 us
before and 4.58 us after (single local runs, CPU preparation only, not frame FPS).
Existing decal/separate-surface tests and new solid-only checks pass. EX Corneria
both-eye BMPs match legacy byte-for-byte in `tmp/vr-textured-live-ex`; this sample
does not establish cartridge texture/decal coverage.

Resident texture fixture strengthened to a 2x2 indexed texture with green,
yellow, cyan and transparent quadrants. Both UV axes vary across GPU-fetched
corners. Both eyes match exact expected pixels, including a nearer red strip
and equal-depth second model; capture inspected in `tmp/vr-resident-uv-proof`.
Quest ARM64 debug APK rebuilt successfully with the current runtime sources.
This fixture is frontal; tilted UV and real cartridge decal parity remain open.

Ordinary all-polygon models now route textured faces through resident graphics.
Preparation explicitly selects unclipped mode, retains texture corners, and
detects exact solid/texture overlays for the existing decal depth bias. Span
effects reject unclipped preparation; mixed line/sprite models still use legacy.
Packet tests cover textured topology, decal versus separate surfaces, and effect
rejection. Vulkan case 14 now tests indexed sampling through resident projection,
BSP, corner fetch, ordered two-model draws and nearer occlusion in both eyes;
its expected right edge matches hardware triangles rather than inclusive spans.
Suite passes in `tmp/vr-resident-texture-chain-proof`. Original sampled Corneria
both-eye hashes still match legacy (`tmp/vr-textured-live-original`). Nonconstant
UV and real cartridge decal comparisons remain necessary; this is not proof of
all textured models or headset acceptance.

Indexed sampling shader added: resident graphics can load corner UVs and signed
texture scroll, then sample packed byte indices with U/V masks and palette-base
wrapping. Zero indices discard; nonzero indices remain opaque as in shape_batch.
Synthetic Vulkan cases verify negative-coordinate transparency, positive wrapping,
and sRGB conversion in both eyes (`tmp/vr-indexed-texture-proof`); the complete
existing synthetic run passed. Live textured routing remains disabled pending
end-to-end corner/topology and coplanar decal comparison against legacy output.

Texture migration groundwork: source arenas now retain packed indexed texture
bytes in a dedicated region; lookup word 19 provides its word offset. Input-only
updates serialize those bytes alongside UV corners and materials. Validation
rejects unaligned payload lengths, oversized textures and material ranges beyond
storage. Packet tests cover byte preservation and transactional rejection; the
real Vulkan synthetic suite passes (`tmp/vr-texture-storage-proof`). This does
not yet enable textured models on the resident graphics path: indexed sampling,
UV/transparency parity and mixed-material routing are still pending.

Ordinary fan templates now use each model's maximum polygon corner count,
instead of 32 for every model. Triangle-only models submit one triangle per
ordered slot rather than 30; quad-only models submit two. Smaller polygons still
degenerate-pad within a model. Retained updates rebuild on changed capacity and
reject unsupported >32-corner templates. Both-eye Original/EX Corneria BMPs
remain byte-identical to legacy (`tmp/vr-sized-fans-original`,
`tmp/vr-sized-fans-ex`). Frame-time gains are unmeasured.

Broader captures: `--live-compute-stage=LEVEL...:ticks` now selects the existing
full-layer stage fixture with compute solids and adjusts placeholder indices
for its grid/dust prefixes. Original LEVEL1_2:400 (10 compute models/23 packets)
and EX LEVEL2_3:400 (8/24) matched legacy BMP hashes in both eyes. Artifacts:
`tmp/vr-stage-compute-1-2` / `tmp/vr-stage-legacy-1-2`,
`tmp/vr-stage-compute-ex-2-3` / `tmp/vr-stage-legacy-ex-2-3`.
These are sampled frames, not complete stage playthroughs or original-hardware
parity; unsupported mixed/effect paths remain outside this proof.

Compact ordinary arenas: unused clipped-polygon, span-command and coverage-mask
regions now use four-byte descriptor placeholders instead of full output
allocations. Unused span headers stay zero. Ordinary dispatch never accesses
these regions; effect arenas retain full sizes. Packet tests check the compact
layout and smaller arena. Original/EX both-eye Corneria captures remain
byte-identical to legacy in `tmp/vr-compact-solid-original` and
`tmp/vr-compact-solid-ex`. Total gameplay memory savings have not yet been
quantified across all levels.

Ordinary solids now skip continuous clipping and span generation entirely:
projection → visibility → BSP, with the final barrier exposing all earlier
compute writes to graphics. Effect models still execute five stages. Both-eye
Original/EX Corneria captures remain byte-identical to legacy after this change
(`tmp/vr-solid-three-stage-original`, `tmp/vr-solid-three-stage-ex`). This removes
two dispatches and their output writes per ordinary model; GPU frame-time savings
have not been measured. Unused clip/span arena allocation remains to remove.

Viewport regression fixed for ordinary solids: live preparation selects an
unclipped GPU-corner fan path when no wire/wobble/wave/cel mode is active.
Graphics reads resident BSP order, transformed corners, visibility and material
palette; it does not apply source screen bounds. Hardware clips the 3D geometry.
Lookup metadata grows to 20 words (materials/visibility/count added). Effects
retain expanded source span covers. Original 401-tick and EX 410-tick Corneria
models-only captures now match legacy BMP files byte-for-byte in BOTH eyes:
`tmp/vr-unclipped-original` vs `tmp/vr-gameplay-legacy-original`, and
`tmp/vr-unclipped-ex` vs `tmp/vr-gameplay-legacy-ex`. Thus the outer tower loss
reported below is resolved in those fixtures. This does not prove all-level,
near-plane, effect-mode or headset parity. Ordinary models still dispatch unused
clip/span stages and use conservative 32-corner templates; optimize after broader
coverage. Mixed-material models still use the prior path.

Actual-scene diagnostic: `--live-compute` now assembles and renders a completed
Corneria snapshot through the combined consumer/pre-render hook. Original
401-tick scene: 12 compute models/20 packets; EX 410-tick scene: 13/23. Runs
passed nonempty rendering and synthetic regressions; captures in
`tmp/vr-gameplay-compute-original` and `tmp/vr-gameplay-compute-ex`.
Visual comparison against `tmp/vr-gameplay-legacy-original/live-scene-left.bmp`
shows the outer tower faces are clipped in the compute capture. THIS IS A
KNOWN LIVE-PATH REGRESSION, not parity proof: source screen-bound clipping is
inappropriate for ordinary immersive geometry. Resolve ordinary solid coverage
outside the source viewport before release; near-plane cover work remains too.
These captures exclude background/HUD and were not taken on a headset.

Real-driver fenced path: combined scene cases 14–16 no longer pre-dispatch on
a separate diagnostic command buffer. The first eye's VulkanEyeCommands
before-render callback performs compute, and the second eye consumes the same
outputs without recomputation. Exact stereo/interleaving/palette coverage passes
in `tmp/vr-fenced-compute-proof`. A complete `build/vr-dev` rebuild followed by
all 13 VR CTests passed (9.77 s). This is real Vulkan plus injected/runtime unit
coverage, not headset gameplay or full migration acceptance.

Input staging reuse: retained models alternate two reusable input-write sets
and retain upload-view capacity. Region byte allocations survive successive
updates; invalid output/recycle aliasing rejects. Packet tests verify addresses
are reused after the alternating cycle. Both-eye suite passes in
`tmp/vr-recycled-input-proof`. Tiny retained scene setup: 1.87 us this run,
versus the previous 6.23 us input-only run (rebuild 1690.13 us this run).
This is a noisy CPU setup microbenchmark, not measured gameplay FPS.

Input-only retained uploads now serialize CPU-authored regions and batch them
through one map/flush/unmap. GPU outputs and padding are not rewritten by the
CPU. Initial allocation still uses a complete zeroed image. Packet tests compare
input bytes to full serialization and exclude command output; the moved-model
test proves previous commands survive upload, then verifies recomputed coverage.
Both-eye suite passed in `tmp/vr-input-only-proof`. Tiny fixture setup was
6.23 us retained/1456 us rebuilt: the new per-region allocations add overhead
on this small case, so no speedup versus the previous tiny benchmark is claimed.
Large-model transfer-volume and allocation optimization remain to measure.

Validation-only compatibility checks now call `validate_source_span_upload`,
sharing the upload invariants without allocating/zeroing/serializing an arena.
Misaligned word offsets are rejected as well as overlaps and incorrect sizes.
Packet tests verify rejected uploads preserve prior bytes; both-eye diagnostics
pass in `tmp/vr-validation-only-proof`. Tiny retained-scene setup measured
3.53 us this run (rebuild 1854.33 us; timing noise applies). Actual updates still
serialize and upload a complete arena once; input-only transfers remain pending.

Retained scene updates: compatible device/pass, handles, legacy geometry,
compute positions/units and layouts now reuse buffers, descriptors and pipelines.
All source layouts/transforms are validated before updates. Changed layout or
membership falls back to reconstruction. The interleaved/palette cases pass
after retained updates. Tiny two-model setup benchmark on Intel Graphics:
3.75 us retained versus 1514.1 us reconstruction (10/3 iterations respectively),
`tmp/vr-retained-scene-benchmark`. This measures CPU setup, not frame FPS.
Updates still serialize/zero/upload complete arenas and compatibility validation
duplicates serialization; reducing those transfers remains important. GPU upload
failures during a retained update are fatal to that frame, not rollback-safe.

Live hookup: non-discarding LiveGame instances now request eligible compute
solids. Application assembly adjusts compute indices after extracting stars,
retains gameplay/title placeholder placement, queries physical device limits,
and initializes the combined consumer when requests exist. Its compute chain
runs before the first eye render pass; both eyes draw the same outputs. Legacy
only scenes retain their existing cache/update path. Desktop compilation, host
cancellation/save tests and the Vulkan scene regression passed
(`tmp/vr-live-consumer-regression`). These tests do NOT exercise a live headset
session. The discard-audio/cache preflight remains on the old assembly path.
Per-frame compute resource recreation is still a major performance gap; do not
describe this hookup as a performance improvement or full migration completion.

Fenced pre-render hook: VulkanEyeCommands and VulkanStereoDraw now accept an
optional callback after command begin and before render-pass begin. Compute
and graphics share submission/fence handling; callback exceptions do not submit
and fence retries do not record again. Injected-driver target tests explicitly
assert outside/inside render-pass state, failed-compute suppression and one
preparation across repeated pending polls. Tests pass. Live consumer hookup and
real-driver compute through this callback remain to verify.

Placement correction: the combined consumer now retains each compute
placeholder's model matrix and composes it with the eye camera, just as legacy
packets do. Case 14 translates both compute models and its legacy occluder;
exact shifted coverage/occlusion passes in both eyes in
`tmp/vr-compute-placement-proof`. This closes the ignored-placeholder-transform
gap affecting eventual gameplay/title distance offsets. Live hookup also needs
compute placeholder indices adjusted when extracting the star packet, and an
outside-render-pass callback in VulkanEyeCommands/VulkanStereoDraw (the current
callback runs inside the render pass).

Interleaving follow-up: case 14 now contains two equal-depth compute models,
an empty packet and a nearer red legacy strip. Compute requests are reversed
relative to placeholder order; exact pixels in both eyes confirm the first
green model wins equal depth, blue does not replace it, and the red strip is
preserved. Passed in `tmp/vr-interleaved-consumer-proof`. Scene preparation now
uses a bounded direct placeholder lookup instead of an O(packets*models) scan.
This verifies mixed submission ordering, not mixed faces within one model.

Combined consumer: `VulkanSourceScene` now owns shared compute pipelines,
per-model resident resources and legacy draw packets. It validates placeholder
indices/keys and duplicate geometry, constructs ranges ignoring empty packets,
dispatches compute outside render passes, then records legacy ranges and models
in source order. Replacement is transactional; model resources die before
borrowed pipelines. Cases 14–16 now execute through this consumer and retain
their both-eye coverage/palette/sRGB checks, including rendering after a rejected
replacement. Passed: `tmp/vr-combined-consumer-proof`. These cases have one
compute model; multi-model interleaving, live app hookup, resource reuse and
frame placement remain to verify. Current initialization rebuilds resources;
it is not yet an optimized per-frame update path.

Scene assembly now has an explicit `compute_solids` constructor capability.
Eligible all-solid models produce `SourceComputeModel` requests containing the
interpolated pose preparation, live 256-entry palette, sRGB mode, units and
ordered placeholder index/key. World assembly adjusts indices for grid/dust.
Mixed textured/line/sprite and unsupported transforms retain the existing path;
no faces are discarded to qualify. Default remains disabled until the app's
consumer is connected. Original and EX cartridge diagnostics each produced two
eligible requests from three models, with matching placeholder keys, live green
palette entry 112, sRGB flags and no duplicate CPU triangles. Full diagnostic
runs passed in `tmp/vr-assembly-original-proof` and `tmp/vr-assembly-ex-proof`.
This does not prove visual parity for assembled requests or mixed models.

Ordered submission prerequisite: `VulkanDrawPackets::record_range` records a
validated contiguous range of nonempty items, so compute-backed models can be
inserted between existing passes rather than appended after them. Whole-scene
recording delegates to this implementation. The scene diagnostic draws the
right-eye packet scene one item at a time, rejects overflowing start/count
before recording, and accepts the empty tail. Existing pixel checks passed in
`tmp/vr-ordered-range-proof`. Live assembly does not yet produce interleaved
compute models; callers must account for empty packets when constructing ranges.

Resident drawing API: `VulkanSourceModel::prepare_graphics` owns the external
arena descriptor, immutable corner templates and depth-tested scene pipeline;
`record_graphics` consumes that same compute buffer inside either eye's render
pass. Explicit close destroys graphics references before storage. Cases 14–16
now use this API (solid, wave palette, tilted wave/sRGB palette) and passed
both-eye pixel checks in `tmp/vr-owned-resident-draw-proof`. No GPU-generated
positions/commands are copied back for drawing. The application still needs
ordered scene assembly and frame-lifetime integration; the test retains its
older standalone draw setup for other cases.

`VulkanSourceModel::update_palette` now supports a 1024-byte palette-only upload
after both eyes complete, without reallocation or compute dispatch. The scene
diagnostic verifies every uploaded entry, stable buffer identity and unchanged
generated command bytes for Q15/Euler wave/tilted-wave models, restores the
palette, then checks both-eye rendered colours. Passed in
`tmp/vr-palette-update-proof`. This is a runtime API, not yet a live-game caller
or a measured end-to-end FPS improvement.

Resident source arenas now carry 256 RGBA8 palette entries and explicit enable/
sRGB flags. Graphics consumes generated row-command even/odd indices and dither
scale instead of the prototype vertex material when enabled. Upload tests cover
all entries, lookup offsets and transactional invalid-flag rejection. Shader
generation, packet tests and the existing Vulkan scene suite passed; captures
are in `tmp/vr-palette-backcompat-proof`.

Enabled-palette follow-up: `tmp/vr-enabled-palette-proof` cases 15/16 now use
distinct palette entries 17/231 on generated wave spans, with dither parity
checked at every covered pixel in both eyes. Case 16 combines a tilted plane,
eye displacement and sRGB decoding into a UNORM target; expected channels are
computed independently with one-byte conversion tolerance. Black coverage is
checked exactly. Case 15's captured alternating colours were visually inspected.
The complete scene diagnostic passed and the Quest development APK rebuilt
successfully. This verifies one face, not multi-face cartridge palette parity.
Gameplay integration, near-plane cover subdivision and unsupported EX stages
remain outstanding. No headset or full migration completion is claimed.

## Confirmed architectural gap

`src/render/software_renderer.cpp` implements alternate solid-polygon spans in
the edge-tracer loop. `src/vr/shape_batch.cpp` instead emits triangle fans, and
`src/vr/shaders/scene.hlsl` receives neither the complete face boundary nor the
left/right tracer history. Removing the current rejection guards would silently
draw ordinary filled polygons. Triangle barycentric outlines would also be wrong:
they expose fan diagonals and omit the source's edge-change chords.

## Source behaviour to preserve

- Wobble bit 2 takes precedence over the other span styles. Continuing right-edge
  segments emit one pixel at the previous left X; the ordinary span is absent.
- Wireframe 1 fills a chord when either edge starts a segment, otherwise plotting
  the two span endpoints.
- Wireframe 2 uses persistent continuation state: a left-only edge change enables
  endpoint-only continuation until the right edge starts another segment.
- Cel, only with wireframe zero, omits both span endpoints. It precedes wave.
- Wave, only with wireframe zero and cel false, displaces each pixel's Y by the
  source 32-entry sine table. Its phase includes signed 16-bit wrapping, X,
  wave offset and animation frame. Vertex displacement is not equivalent.
- Wobble bit 1 changes tracer stepping: steps share a row until a segment ends,
  followed by a two-row Y advance. It can combine with the above styles.
- Textured polygons, sprites and two-point lines bypass these solid span modes.

## Required next implementation

Runtime `cover_geometry` now emits immutable ordered-slot/corner templates,
leaving projection and geometry reconstruction to the GPU. The solid, wave
and tilted-wave end-to-end tests use this method and pass exact output
(`tmp/vr-runtime-cover-template-proof`); invalid units preserve the prior
geometry. Current templates accept a uniform material. Per-face palette
binding, horizon handling and application submission still remain.

Expanded GPU-derived wave planes now pass an analytical tilted-face test
with opposing eye translations (case 16,
`tmp/vr-expanded-tilted-wave-proof`). Expected source X is independently
recovered from the Q15 plane equation and eye offset, then the signed wave
phase is applied; every output pixel matches. This verifies synthetic stereo
parallax/coverage, not headset gameplay. Projection-horizon/near-plane cover
handling and application integration remain unfinished.

Expanded-plane case 15 now consumes a generated wave-only model's resident
buffer and matches the signed-offset/frame sine displacement exactly in both
eye passes, including pixels outside the original polygon bounds
(`tmp/vr-expanded-wave-proof/span-depth-15-eye-0.bmp`). The capture was visually
inspected. This is front-facing synthetic geometry; tilted wave planes,
projection-horizon subdivision and gameplay integration remain unverified.

Graphics flag 524288 builds an expanded projected cover quad on the GPU-derived
source face plane. It uses clipped bounds plus raster/wave margins, with the
span mask retaining the actual shape. Case 14 passes exact endpoint coverage
in both eye passes (`tmp/vr-expanded-plane-proof`), restoring the source's
extra right-edge pixel that original triangles clipped. Wave margins are not
yet exercised in this fixture. Cover quads crossing the projection horizon
currently reject as a whole; subdivision/near-plane handling remains required
before gameplay enablement. No gameplay parity claim is made.

Combined graphics flags 131072/262144 now fetch polygon corners through GPU
BSP order and derive world position/source numerators from transformed point
output. Native fixtures pass exact quad coverage in both eye passes and reject
an out-of-range corner (`tmp/vr-transformed-corner-lookup-proof`, cases 12/13).
These new cases use a synthetic source-to-test-view transform, not headset
parallax. Original polygon triangles alone still clip source raster endpoint
and wave coverage outside their geometric edge; expanded face coverage and
application submission remain necessary before enabling this for gameplay.

Graphics flag 131072 now resolves an ordered occurrence through resident BSP
results/order before selecting its span header. Generated-model graphics pass
exact both-eye coverage/occlusion checks; out-of-range slots 1 and UINT32_MAX
produce no face while preserving the unrelated occluder
(`tmp/vr-ordered-header-bounds-proof`). Source positions/numerators are still
authored inputs: transformed face geometry and wave extent remain pending.

Added bounded graphics lookup metadata linking resident BSP order/results,
transformed points, per-slot headers, polygon/corner arrays and projection
parameters. Packet tests verify its word offsets and count fields. The shader
consumer for this lookup is not implemented yet; existing graphics fixtures
continue to use explicit header/plane inputs.

Closed a model-input validation gap before GPU face attachment: shader
settings must agree with packed vertex/visibility/face counts, clipping
dimensions, BSP tree capacity and residual mode. Tests mutate every clipping
setting plus invalid projection/tree settings and verify rejection preserves
the previous upload. Packet checks pass; geometry attachment is still pending.

Cross-build checkpoint after the runtime model/header work: all desktop VR
targets rebuild, labelled VR tests pass 13/13 (12.08 seconds), and Quest debug
APK builds successfully (39 seconds). No headset installation/acceptance.
Next geometry attachment must resolve the GPU BSP ordered face ID into packed
polygon corners and transformed points, not index CPU-expanded face instances
by slot. `scene.hlsl` currently accepts authored positions/source numerators;
the passing resident test still supplies that plane explicitly. Wave coverage
also requires geometry beyond the original face's projected bounds.

The stereo depth consumer now binds the generated Q15 model's resident arena
and generated header, not the earlier handwritten compute fixture buffer.
Both eyes and both draw orders pass exact coverage/nearer occlusion
(`tmp/vr-generated-span-graphics-proof/span-depth-2-eye-0.bmp`, visually
inspected). This closes the synthetic model-upload-to-graphics test chain.
The consumer geometry is still an authored plane; arbitrary cartridge face
attachment and runtime application integration remain unverified.

Model arenas now include one resident graphics header per ordered face slot,
pointing at its command block and shared mask buffer with the source wave
phase. Packet tests check header offsets/phase; the GPU suite still passes
(`tmp/vr-span-graphics-headers-proof`). Mapping ordered slots onto actual
source-face geometry remains necessary before gameplay can consume them.

Runtime source models now support compatible in-place updates without
reallocating GPU storage/descriptors. Native Q15/Euler tests move a face four
source pixels, reject a subsequent incompatible update, and verify the same
buffer still produces exactly the shifted eight rows
(`tmp/vr-retained-source-update-proof`). Uploads currently regenerate the full
arena image; input-only streaming optimization and application fence ownership
remain, and no gameplay FPS improvement is claimed by this fixture.

Real cartridge compute probes now pass ROBOT_0, BOSS_H_2 and MY_DEMO across
six effects for both Original and EX (36 dispatches). Checks require valid
BSP counts/status, clean clipping status for every face, nonempty output and
correct row indices (`tmp/vr-cartridge-span-original-proof` and
`tmp/vr-cartridge-span-ex-proof`). This is structural compute evidence at one
Euler pose, not pixel parity, cartridge-effect screenshots or gameplay
submission. Existing stereo captures in those folders test the separate
graphics fixtures and must not be described as these models' span output.

`VulkanSourceModel` consolidates checked input upload, storage, descriptor
ownership and the five-stage command sequence, with GPU-to-graphics barriers.
The generated-model fixture now calls this runtime class rather than recording
its own stage sequence; all twelve model cases and the existing exact/stereo
suite pass (`tmp/vr-runtime-model-owner-proof`). The class exposes resident
buffer/layout data and requires serialized arena use. Application frame-fence
integration and real source-face graphics attachment are still pending.

Generated model uploads now run through projection, visibility, BSP, clipping
and spans using dynamic device-aligned regions and shared storage/bindings.
The native fixture passes Q15 and Euler transforms across six effect modes
(`tmp/vr-generated-model-upload-proof`): successful BSP results, nonempty
coverage, valid row coordinates, and exact solid-rectangle bounds. Alternate
effect coverage here is not yet compared pixel-for-pixel; the older authored
fixtures retain their exact checks. This synthetic quad is not cartridge
gameplay or a runtime integration claim.

Model preparation now emits all shader settings and near-projection parameters,
including six-pose sequential Euler inputs versus four-pose Q15 inputs and
the renderer's residual/lossless-camera selection. `source_span_upload_image`
serializes inputs into validated regions and zeros outputs/padding; overlapping
or incorrectly sized regions reject without replacing prior data. Packet tests
pass these paths. Dynamic model dispatch through this upload image is the next
integration step; the existing GPU fixture still authors its own input bytes.

`VulkanSourceBindings` owns all fifteen descriptor sets for the five source
compute stages, borrowing the checked arena and pipeline layouts. The native
fixture now dispatches through those sets: all 60 exact cases and both-eye
depth consumers pass (`tmp/vr-owned-span-bindings-proof`). Wrong-stage
pipelines and overflowing arena ranges are rejected. This verifies shared
binding code, still at fixture offsets; dynamic model upload, runtime dispatch
and frame-fence ownership remain pending.

`layout_source_span_model` assigns nonoverlapping aligned regions for all
projection, visibility, BSP, clipping and span inputs/outputs. It accounts for
padding in the 256 MiB arena budget and checks physical-device storage/uniform
range limits. Packet tests pass alignment, output-size and transactional
rejection checks. These dynamic offsets still need binding into the runtime
descriptor sets; the GPU fixture currently uses its fixed test offsets.

`VulkanSourceStorage` now owns reusable storage/uniform memory with bounded
uploads, diagnostic readback and noncoherent flush/invalidate support. The
native chain fixture uses this owner for all 60 cases and directly binds its
resident output for both-eye graphics (`tmp/vr-owned-span-storage-proof`,
passed). Invalid alignment and overflowing upload ranges are rejected. This
run used Intel Graphics; noncoherent hardware and headset behavior remain
unverified. Per-model descriptor layout and runtime fence ownership still
need integration; the class requires completed GPU use before mutation.

`prepare_source_span_model` now packages the shared projection/BSP/material
inputs together with checked span allocation settings. Non-solid primitives
retain their face IDs but emit zero clipping corners, so removing them cannot
shift subsequent BSP indices. Tests cover combined wave/repeated rows,
duplicate BSP occurrences (three ordered slots, two unique faces), and
transactional dimension/destruction rejection. This is input preparation;
runtime GPU allocations, per-face geometry attachment and dispatch remain.

The opt-in boundary builder now accepts EX span effects and omits ordinary
solid triangle fans for those faces, preventing duplicate filled geometry
beneath sparse effects. Default triangle-only callers still reject them.
Textures, sprites, lines and ordinary non-effect solids retain their existing
paths. Mesh tests pass six effect modes on ROBOT_0, BOSS_H_2 and MY_DEMO from
both Original and EX cartridges (36 model/mode preparations), with boundary
indices checked against decoded source faces. These are CPU preparation tests,
not proof that those models are submitted through the native GPU span chain.

Native BSP dispatch now feeds ordered face indices directly into the span
stage after source visibility. Ten scenarios across six effect modes pass
exact command/mask checks, including a visible single-node BSP group and a
hidden group with an otherwise visible face (`tmp/vr-bsp-span-chain-proof`).
The final GPU buffer also passes both-eye depth-consumer checks without a CPU
re-upload. This fixture does not yet prove multi-node traversal, cartridge
model integration, or gameplay resource lifetimes; EX runtime guards remain.
The complete labelled VR suite passed 13/13 tests (10.34 seconds), and the
Quest debug APK rebuilt successfully (17 seconds) after these changes.
No headset installation or physical-device acceptance was performed.

Added native continuous source-visibility dispatch between model projection
and clipping. Reversed visibility triples now exercise both visible geometry
and entirely empty command/mask output across all six fixture effects, with
GPU-only dependencies (`tmp/vr-visible-span-chain-proof`). BSP group ordering,
arbitrary real model batches and runtime resource management remain separate
integration work; this does not enable the guarded EX gameplay path yet.

Projection-chain fixtures now use the existing `pack_projection` model packer
instead of handwritten transform records. Source Q15 transforms, all-word
coordinates with nontrivial header/object scale, and mixed byte/word vertices
produce the same exact expected spans through all six effect modes
(`tmp/vr-mixed-coordinate-span-proof`). These are synthetic shapes using the
real packer; cartridge model integration and dynamic resource reuse remain.

Corrected source-coordinate interpolation for tilted span faces: carry source
XY numerators plus source depth through headset perspective interpolation,
then divide per fragment. Ordinary interpolated source UVs would slide across
non-front-facing geometry. Added analytical tilted-plane eye-offset/wave/depth
fixtures; both eyes and draw orders pass exact pixels
(`tmp/vr-span-tilted-proof`). Gameplay must populate this homogeneous payload;
the diagnostic fixtures do not establish that integration.

Perspective eye-offset fixtures now verify wave coverage attached to its face
and double parallax for a nearer occluder, in both draw orders. Exact expected
pixels passed (`tmp/vr-span-perspective-proof`). This remains synthetic fixed
geometry, not arbitrary tilted faces or gameplay/headset acceptance. All VR
targets rebuilt and the Quest debug APK rebuilt successfully in 31 seconds
after the shared shader/resource changes; no installation was performed.

The depth consumer now has exact-pixel fixtures for signed-offset wave
displacement and combined wave/coverage-mask sampling, with nearer occlusion.
Both eye passes passed (`tmp/vr-span-wave-depth-proof`). These cases use
authored commands and orthographic fixture cameras; they do not establish
perspective stereo movement or full gameplay acceptance.

Added caller-owned storage-buffer binding to `VulkanSceneTextures`; closing the
descriptor does not free the producer's buffer. The depth fixture now directly
samples the projection/clip/span output allocation with a compute-to-graphics
barrier, without uploading a CPU copy of the generated commands. Both draw
orders and eye passes match exact expected coverage/occlusion
(`tmp/vr-resident-span-depth-proof`), including close/rebind ownership checks.
Diagnostic readback still inspects producer results but is not used to build
the graphics input. Gameplay resource management and arbitrary face geometry
remain unfinished.

Added a span-coverage fragment path using perspective-correct source coordinates
on an ordinary depth-tested face. It interprets source edge/mask/wave commands;
coverage remains attached to geometry rather than a screen overlay. Synthetic
solid spans with a nearer occluder pass exact pixel checks in both draw orders
and both eye passes (`tmp/vr-span-depth-proof`); one capture was inspected.
These consumer fixtures currently upload authored span commands. Connecting
the GPU-produced buffer directly, arbitrary face-plane reconstruction,
wave/other consumer coverage, and actual stereo head motion remain unverified.

The wrapper now includes continuous model projection (64-lane groups). The
native fixture starts with object-space vertices and a translation/projection
pose, executes projection -> clipping -> spans with compute barriers, and
checks all six effect modes without reading intermediate buffers. This passed
on Intel Graphics (`tmp/vr-projection-clip-span-proof`). It proves this small
fixture's producer chain, not arbitrary gameplay geometry or headset depth.

The native compute wrapper also supports the existing continuous clipping
shader (six input buffers, one output and a Settings uniform). Pipeline
creation and recovery after invalid-stage rejection passed on Intel Graphics
in `tmp/vr-clip-pipeline-proof`. Native clip-to-span chaining now passes six
effect modes for both interior and left-edge-clipped rectangles, with a
compute-write/read barrier and no intermediate readback
(`tmp/vr-clip-span-edge-proof`). Generated commands and masks match the
analytical expected rows. This does not yet produce gameplay polygons or
solve VR depth consumption; near-plane and arbitrary-face coverage remain.

`VulkanSpanPipeline` now creates the shared desktop span compute shader through
native Vulkan, with its three descriptor-set layouts and bounded dispatch API.
Host Vulkan creation, close/reinitialize and invalid-handle checks passed on
Intel Graphics in `starfox_vr_scene_check` (`tmp/vr-span-pipeline-proof`). This
creation test alone does not verify dispatch. A subsequent native Vulkan test
now binds all seven buffers, dispatches solid/cel/wire rectangle fixtures,
performs a compute-to-host barrier and checks generated command rows exactly.
It passed on Intel Graphics (`tmp/vr-span-dispatch-proof`), including zeroed
outside rows. This validates the descriptor ABI and basic execution, not full
polygon/effect parity. Gameplay buffer production, projection and the VR depth
consumer remain to be connected.
The native dispatch fixture now additionally checks sparse wobble's previous-left
pixel, repeated-row coverage masks (including untouched mask rows), and preservation
of the combined wave flag. All six modes passed on Intel Graphics in
`tmp/vr-span-wobble-proof`. Wave displacement itself belongs to the consumer and
is not proven by this producer-only test.

Reuse candidate confirmed: `src/render/shaders/spans_portable.hlsl` already
implements the desktop GPU edge tracers, wire/cel/sparse-wobble selection and
repeated-row masks. Its input ABI is 129 `int4` records per clipped polygon,
plus material commands and optional BSP order buffers. Output is one command
slot per polygon per row plus a coverage mask for repeated-row effects.
`src/render/packed_faces.cpp` supplies the effect bits and wave parameters.
VR must adapt projection/clipping, descriptor sets and depth-aware consumption;
the existing shader cannot simply be bound to `SceneVertex` triangle buffers.
Combined wave/wobble audit found and fixed a desktop mismatch: wobble bit 1
must retain wave (bit 2 still takes precedence). Repeated-row masks now retain
the wave flag and are sampled at the undisplaced source row. Regenerated Vulkan
and Metal shaders. The expanded ten-mode EX ARWINGCX fixture passes 180 direct
and 180 queued-composition GPU/software comparisons with nonzero coverage,
including exact surface coverage/palettes. Unnamed 1/16-model scans produced
only blank images and were rejected, not counted as passing coverage. VR still
needs the adaptation below; these are desktop GPU diagnostic results.
The colour-warp combination path also passes 180 visible ARWINGCX comparisons.
Diagnostic-palette CPU-left/GPU-right captures are in
`tmp/gpu-ex-wave-wobble-proof`; mode 8/view 4/scale 2 was visually inspected and
shows the same displaced repeated rows on both sides. This is raster parity
evidence, not a full-game screenshot or VR acceptance.

The batch builder now has opt-in ordered `polygon_vertices` / `polygon_ranges`.
It preserves material and visibility attributes per original BSP face occurrence,
without allocating this extra payload on the ordinary rendering path. Tests cover
a quad without fan diagonals, mixed coordinate scaling, duplicate BSP occurrences
with distinct group visibility, and preservation of prior output on invalid input.
This data is not yet uploaded or consumed by a span shader; effect guards remain.

Preserve ordered original polygon vertices and face-instance membership in an
immutable GPU payload before fan triangulation. GPU projection/clipping and span
evaluation must have access to the complete boundary, source quantization and
effect state. Keep eye/world transforms separate from the source effect plane;
do not anchor the effect to headset viewport pixels by accident. Wave needs
coverage beyond the undisplaced polygon, not just a fragment discard shader.

Colour warp is a separate ordered-material dependency: source BSP-visible face
occurrences determine the sequence. Face-index hashing or unordered membership
is not a substitute.

## Acceptance evidence still needed

Use deterministic concave/convex boundary fixtures, edge changes on either side,
signed wave phases, clipping, reversed winding, and combined flags. Compare
against the existing source span renderer at matched projection before testing
two eye views and headset movement. Include textures/lines to prove they remain
unaffected. The long stage preflight reported zero affected object-ticks in its
EX Corneria fixture; that run cannot establish support for these modes.
