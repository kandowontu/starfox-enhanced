# Additional desktop effects — September 19

## Comic cleanup and expanded 2D/3D styles

Crosshatch is removed from both selectors and is a no-op in both render paths.
Old Crosshatch preferences and model/world save-state fields migrate to Off;
IDs remain stable. Comic no longer adds the screen-aligned 4x4 dark-dot mask.
Its stronger outlines and four-band hue-preserving shading remain.

Both model and world lists now include Watercolour (edge-aware local wash),
Chalk (light contours on charcoal), Emboss (directional relief), and Bleach
Bypass (desaturated contrast). IDs 33-36 are appended. Category labels and
Japanese/German/French/Spanish translations include the new choices.

CPU pixel/layer/intensity tests and D3D12/Vulkan all-style comparisons pass.
A flat-field regression rejects Comic dot patterns at 1x/2x/4x in both layers.
An older test expected removed Crosshatch to alter pixels; that expectation
was updated and the pixel suite reran successfully. Original and EX state
tests verify removed Crosshatch migration in checksum-valid archives.
Generated DXIL/SPIR-V/Metal sources are current (physical Metal not tested).
Normal Windows executable rebuilt successfully. Five final presentation images
were inspected at `tmp/styles-no-dots-{12,33,34,35,36}-sep19/presentation.bmp`.

Deployment follow-up: after the user closed the running game, the normal
`build/current/starfox_pc.exe` successfully relinked. Alternate test executables
mentioned in the earlier checkpoints are no longer needed for these fixes.

## Grouped selectors and conductor presets

Model/world effect cycling now follows display families rather than saved ID
order: Drawn, Retro, Colour, Stylized, Reflective Materials. The desktop page
heading identifies the selected family while its effect row is selected.
Reflective materials remain model-only. Serialized IDs have not been renumbered.

Ice and Cyanotype were essentially the same blue-to-white luminance ramp.
Ice is no longer offered in the cycle; its old settings/state ID maps to
Cyanotype. Legacy Bloom remains excluded in favour of the separate Bloom
controls. Cel versus Comic (stronger outlines/four-band shading), posterization versus hue-preserving Cel,
and Night Vision versus Emerald retain distinct processing. Copper is now
labelled Copper Tone to distinguish it from the new reflective material.

New styles: Gold Metal (29), Copper Metal (30), Teal/Orange split tone (31),
and Handheld four-colour palette (32). Both metals use actual reflected scene
geometry: fixed conductor Fresnel on DXR and inexpensive fixed tint on software.
They require Reflective Surfaces; GPU additionally requires Ray Tracing.
They do not add ray samples or cost beyond the existing Metallic path.
The nonreflective styles match between CPU, Direct3D12 and Vulkan; generated
Metal shaders are updated but have not been tested on physical Apple hardware.
All new labels/family headings have Japanese, German, French and Spanish entries.

Validation: pixel/filter and software-reflection tests; Original ROM-backed menu
simulation; Direct3D12/Vulkan all-style CPU comparisons and effects chains; RTX
gold/copper reflectance fixtures plus existing reflection/shadow checks. Gold
live screenshot: `tmp/effects-gold-sep19/3.bmp`. Existing HUD exclusion and
intensity behaviour remain checked. The normal game is still running, so the
updated application is `build/current/starfox_pc-effects-check.exe`; relink
`starfox_pc.exe` after it is closed. No release or VR deployment performed.

## Physical ground without a tiled reflection material

The GPU caller previously discarded the physical receiver plane when no BG2
reflection material was available; the DXR input validator likewise rejected
such a plane. This allowed reflected geometry behind a real floor to remain
visible. The plane is now retained independently of its material. A finite
ground hit with no tiled background returns the configured opaque fallback
colour; it never interprets the zero background offset as tile metadata.

Hardware RTX 5070 Ti Laptop checks pass plane-before-object occlusion, cube-sky
versus finite-floor separation, moving the plane behind the object, and invalid
normal rejection. Existing material/alpha/roughness/environment/stereo-shadow
checks also pass. Direct3D12 resident scene/material diagnostics pass.
Ordinary authored-BG2 Mirror proof in
`tmp/reflections-ground-fallback-regression-sep19/3.bmp` remains byte-identical
to the earlier reference, SHA-256
`50A13DFF993658EB179051EECFDA867D57FE10AC7EE559352CB769EFB3D3FAF1`.
The missing-material case has hardware pixel-fixture proof, not an identified
natural-level screenshot. Behind-camera tiled-ground fidelity remains separate.

The normal Windows executable was running (PID 51236), preventing replacement.
It was not interrupted. Verified updated application:
`build/current/starfox_pc-ground-check.exe`; the regular executable still needs
relinking once closed. The capture harness accepts `-Binary` for this workflow.

## Renderer-specific shadows and inexpensive software reflections

The latest user request restores Enhanced Shadows for Software only. The same
menu row shows Ray Tracing on GPU. SOFTWARE_SHADOWS stores the CPU preference;
the obsolete ENHANCED_SHADOWS file key remains ignored. Defaults stay off.
The CPU path reuses the geometry BVH, angled light and soft-shadow mask and
suppresses the cartridge silhouettes while enabled. GPU behavior is unchanged.

Reflective Surfaces now runs independently of shadows on Software, with
LOW/MEDIUM/HIGH tracing at quarter/half/native logical resolution. Upscaling
does not multiply the primary reflection-ray budget. Small faces and changes
of material/normal/depth get separate samples; full-resolution ownership
checks prevent coarse cells from painting over HUD or unrelated pixels.
One actual secondary geometry intersection reflects off-camera models; this
is not a screen-space mirror or a DXR call. Rays share the frame's BVH and a
row-worker pool, with no temporal history. Metallic tint and Mirror strength
use the existing model styles. GPU reflections still require Ray Tracing.

Deliberate quality concessions: secondary materials use representative flat
palette colours (including simplified texture/dither sampling), no UV cutouts
or reflected texture detail, no secondary lighting/shadow rays, no recursive
bounces or roughness sampling. EX's random reflected materials use ordinary
source material colours rather than the detailed GPU occurrence resolver.
Misses sample the current background in view; out-of-view rays use a coarse
sky/ground colour probe, not a full environment reconstruction. The primary
raster is unchanged. These limitations are not GPU parity claims.

Evidence:
- `starfox_software_reflection_tests`: off-camera reflected model, material
  association through BVH sorting, all qualities at 1x/2x/4x, HUD/overwritten
  pixel exclusion, projection offsets, UI-free environment and worker parity.
- `starfox_shadow_geometry_tests`, `starfox_shape_decoder_tests`,
  `starfox_runtime_input_tests`, ROM-backed `starfox_upstream_simulation_data`
  pass. A stale world-style wrap test was corrected to exclude model-only
  Metallic/Mirror; software navigation and press-only toggling are now covered.
- Live inspected Original Mirror: `tmp/reflections-software-original-sep19/3.bmp`.
  Final LOW, 30-frame capture after UI-probe protection:
  `tmp/reflections-software-final-low-sep19/1.bmp` (also inspected).
  EX randomized model + LOW Metallic + CPU shadows:
  `tmp/reflections-software-ex-shadows-low-sep19/1.bmp`. Ground/model shadow
  receiver counts: 3701/14342. CPU shadows remain expensive (~54ms world-pass
  average in this short diagnostic); no broad software FPS promise is made.
- GPU regression `tmp/reflections-software-change-gpu-regression-sep19/3.bmp`
  is byte-identical to the prior Original Mirror GPU capture, SHA-256
  `50A13DFF993658EB179051EECFDA867D57FE10AC7EE559352CB769EFB3D3FAF1`.
  Direct3D 12 and Vulkan geometry/material/warp-chain checks pass.
- Bounded CPU-pass benchmark (`starfox_software_reflection_tests --benchmark`):
  640x448, 2x, every pixel reflective, 20 triangles, four workers, five warm-up
  and 20 measured iterations: LOW 1.82ms, MEDIUM 2.10ms, HIGH 5.15ms locally.
  These are pass-only synthetic timings, not end-to-end gameplay FPS.

VR and physical platform acceptance remain outside this desktop change.
Save-state restore now retains the current reflection preference, like DLSS
and stereo output, without extending or invalidating the archive format.

## GPU EX inverse material lookup checkpoint

Reflection-only expansion now builds a face-to-last-valid-occurrence lookup
once per expanded model. Per-triangle ray material packing uses that lookup
instead of scanning every polygon; reference scanning remains diagnostic.
Buffers and shader pipelines are lazy: reflections Off do not initialize the
extra lookup/canonical-material pipelines. D3D12/Vulkan dispatch tests compare
cached and scanned material records byte for byte, including hidden/invalid
lists and reuse. `warp_lookup_portable --check` confirms generated payloads.
The live `tmp/reflections-ex-cached-warp-live-sep19/3.bmp` is byte-identical to
the prior canonical-material capture. This is correctness evidence, not a
measured gameplay FPS improvement.

Implemented ten appended styles without changing existing saved effect IDs:
Negative, Solarized, Amber, Emerald, Cyanotype, Copper, Lavender, CGA,
Scanlines and Crosshatch. Available for model and world effects with existing
intensity controls; HUD/menus remain excluded. Copper is a colour treatment,
NOT the requested metallic material or a claim of reflection.

CPU and shared GPU implementations are matched. Regenerated D3D12/Vulkan/Metal
shader payloads; Windows application rebuilt. The GPU checker exercised all
27 IDs at 0/37/100 intensity with independently selected model/world styles:
D3D12 and Vulkan both pass exact style comparisons. Existing tone/bloom/filter
checks also pass their established tolerances. Vulkan output retained in
`tmp/effects-expanded-vulkan-sep19.log`. Pixel tests pass, including alternating
scanline rows, layer isolation, zero intensity, alpha and blending. Physical
Metal/console validation is not claimed. VR's explicit supported-style list
was not expanded, consistent with the current no-VR-work instruction.

## Real reflective materials — still unfinished

EX omitted-face extension: a separate GPU-only material pass supplies every
source face with a deterministic canonical random material, then appends the
untouched actual draw occurrences. Last-occurrence lookup therefore always
prefers the visible source material while hidden/back-facing faces no longer
leave material holes. This is an explicitly reflection-only extension: the
cartridge never assigns those omitted materials. It does not consume or change
the visible random stream, and its output is never sent to the main rasterizer.
Only reflection-enabled, non-axis random models allocate/use this extra pass.

D3D12/Vulkan dispatched checks verify hidden and empty lists, invalid traversal
rejection, byte-identical visible commands, stable canonical materials despite
visibility changes, and final packed-ray selection of visible versus canonical
records. Model/scene/shadow checks pass on both backends; PC builds and the new
DXIL/SPIR-V/Metal shader payload freshness check passes. Live EX Mirror proof is
`tmp/reflections-ex-hidden-warp-live-sep19`: captured colour-warp=1 and resident
authored-BG2 reflections, inspected on-image, with the off-image byte-identical
to `tmp/reflections-ex-warp-live-sep19/0.bmp`. Older omitted-face limitation notes
below are superseded for this non-axis path, not for all shapes/materials or
performance/platform acceptance. Transition appearance and larger random-model
lookup cost still need review; no FPS claim or release.

EX cartridge colour-warp photographic proof: the capture harness now has an
EX-only switch that sets the cartridge M_COLORWARP word after preroll and checks
the captured frame's logged flag, not merely the selected experience. Current
EX Corneria tick-1000/six-frame Mirror mono and Metallic Full-SBS off/on captures
pass resident-reflection/authored-BG2/active-colour-warp assertions and were
visually inspected. Randomized surfaces participate, sky/ground reflect, and
the green crosshair and HUD remain intact. Directories:
`tmp/reflections-ex-warp-live-sep19` and
`tmp/reflections-ex-warp-sbs-live-sep19` (0.bmp off; 3.bmp on).
Mono on-image SHA256:
`87F69B3336F7985A789A1260722D2BC2F2903570C506A7AFDABFD20F4E0FEAF0`.
PC rebuilt and both changed shader payload freshness checks pass. These injected
cartridge-state fixtures do not prove the natural EX menu transition, all random
materials, non-emitted faces or every level. Axis-mode live visual proof remains
separate from the dispatched mixed-scene coverage below.

Axis-line isolation: models collapsed to raster lines no longer invalidate every
other model's scene reflection materials. They explicitly retain their existing
full shadow-caster triangles and receive GPU-written invalid reflection records,
not invented filled polygon surfaces. Ordinary and randomized neighboring models
remain reflective. This exclusion is specific to axis-line poses; it does not
turn arbitrary material/descriptor failures into successful scene submission.

Windows PC rebuild and D3D12/Vulkan tests pass: ordinary/axis mixed submissions
with 1/33/2 model inputs retain exact shadow masks and caster counts; axis-only
reflection images have zero opaque pixels while mixed ordinary scenes still
reflect. Existing ordinary, randomized, texture, stereo and warp-chain fixtures
also pass. Exploding-axis source tests retain their caster positions and explicit
exclusion. Actual cartridge special-mode visual coverage remains open.

EX emitted-material connection: expanded GPU corners now retain source-face
identity. Source-face ray topology can resolve its local corner ordinals to the
last emitted occurrence (matching the overlapping draw order), including its
texture coordinates and signed scroll. Non-axis colour-warp model submission now
exports these resolved GPU buffers to the live scene/material packer. It retains
the complete original triangle set for shadows, with no CPU material readback.

D3D12 and Vulkan checks pass source-face lookup for repeated random colours,
nonzero source IDs, textured UV/scroll records, absent faces, malformed corner
ordinals, hidden lists, cancellation and reuse. Actual scene submissions with
1/33/2 model inputs produce reflected pixels and exactly unchanged shadow masks.
The separate warp-expansion Vulkan fixture passes and PC builds. The raster
consumer ignores the newly used corner identity lane.

Important remaining limitation: a source face with no emitted occurrence has no
resolved random material and is explicitly invalid for reflection; its geometry
still casts shadows. Thus offscreen/back-facing random materials, axis/destruction
specializations, broad cartridge visual proof and lookup-performance acceptance
remain open. This supersedes older notes saying all live colour-warp reflection
submission is disabled; it does not establish complete EX material coverage.

Current direct-write photographic check: Original Mirror mono and EX Metallic
Full SBS captures at six frames / tick 1000 execute resident reflections with
authored BG2 on the captured frame. Off/on final images differ and were visually
inspected: Original scenery reflects sky/ground and retains diagonal shadows;
EX retains distinct eye views and readable HUD. Evidence directories:
`tmp/reflections-direct-scene-original-sep19` and
`tmp/reflections-direct-scene-ex-sbs-sep19` (0.bmp off, 3.bmp on).
On-image SHA256 values respectively:
`50A13DFF993658EB179051EECFDA867D57FE10AC7EE559352CB769EFB3D3FAF1`,
`1A1EBF6CD8D8F2958954CA1F587DB645A732D84697ED0B43E4BEF5B385C1657C`.
This is ordinary EX geometry, not randomized/NAN-material acceptance; these
captures also do not replace physical stereo or all-level acceptance.

Direct scene material writes: the ray-material packer now accepts a bounded,
16-byte-aligned destination within the combined scene buffer. Live aggregation
uses it instead of packing a temporary buffer and copying 64 bytes per triangle
for every model. It removes one copy pass per reflecting model and the temporary
material allocation from this path. D3D12/Vulkan tests verify exact standalone
versus subrange output, untouched prefix/suffix sentinels, rejection of aliased,
misaligned, null and overflowing targets, and multi-model reflected pixels against
the CPU reference. Warp-chain checks pass on both backends and PC rebuild passes.
No overall FPS gain is claimed without measurement. EX occurrence topology
integration remains open; this optimization does not enable incomplete materials.

EX occurrence-chain verification: the GPU warp-chain checker now feeds resolved
corner/polygon/command buffers directly into the ray-material packer in the same
command buffer. Duplicate source faces with distinct random colours retain their
individual occurrence colours and IDs. Hidden, empty and invalid traversal slots
remain rejected; cancellation followed by reuse does not retain stale records.
Both D3D12 and Vulkan pass (the checker now advertises both shader formats).
This isolates the remaining EX work to connecting occurrence topology to scene
geometry without dropping offscreen/back-facing shadow casters; live EX warp
reflections are still not enabled. No VR changes.

The latest GPU-only live capture, `tmp/reflections-gpu-only-materials-sep19/3.bmp`,
is byte-identical to `tmp/reflections-resident-material-original-sep19/3.bmp`
(SHA256 `71B02964BDB4F518E9DB2D336F1F0F9D78E890F35E3565108CE73782DF663AEE`).

Duplicate CPU work removed: live model draws no longer pack CPU RayMaterial
triangle records alongside the GPU result. `ray_material_reference` explicitly
opts diagnostic draws into independent CPU packing. Scene aggregation publishes
the resident records with texel metadata only; DXR metadata uploads also omit
the formerly reserved/zero-filled 64 bytes per triangle. D3D12 and Vulkan tests
assert an empty CPU triangle array and compare the resulting reflected pixels
exactly to the independent CPU reference. Existing DXR tests pass; PC rebuilt.
This removes work and transfer bytes, not a measured overall FPS claim. Base
model/material inputs and texture metadata are still CPU-prepared; EX random
occurrence mapping remains incomplete.

Live resident-material connection: ordinary model draws now expose resolved
GPU corner/polygon/material buffers. Scene submission packs their ray materials
on GPU, rebases texture offsets, and stores them after all scene vertices in one
buffer. SDL D3D12/Vulkan transport copies that combined range; DXR binds its
material subrange directly and transitions the shared resource only once.
Shadow-only transfers omit the material tail. CPU-packed records remain as a
reference/fallback and are not yet removed from preparation. Both backend checks
require the resident subrange and match reflected output to the CPU reference;
packing tests also verify nonzero texture-base rebasing. Original mono and EX
Full SBS live captures pass and were inspected in
`tmp/reflections-resident-material-original-sep19` and
`tmp/reflections-resident-material-ex-sep19`. This closes transport/aggregation
for ordinary draws, not EX random occurrence topology: colour-warp remains
explicitly incomplete rather than reading wrong-face material records.

Resident DXR material checkpoint: ReflectionInput now accepts a producer-device
COMMON-state material buffer. DXR reads its 64-byte triangle records directly
through a separate SRV, acquiring/releasing resource state around the trace;
CPU material records may be absent (texel metadata remains supplied). The API
rejects wrong-device/undersized/aliased resources; the shader rejects invalid
records and out-of-range textures/UVs rather than reflecting garbage. Hardware
fixtures verify exact reflected pixels, repeated use, bad texture offsets,
invalid records and undersized buffers. D3D12/Vulkan SDL checks and the
unsupported-backend check pass. This completes the consumer interface, not
the live connection: SDL shared material transport, scene aggregation and EX
occurrence topology still need integration before enabling EX warp reflections.

EX material migration checkpoint: `GpuRayGeometry::enqueue_materials` and
`ray_materials_portable.hlsl` now convert resolved 96-byte raster commands and
occurrence-indexed corners/polygons into the 64-byte ray-material ABI entirely
on GPU. Output retains each occurrence's colours, texture addressing and signed
UV scroll; invalid records explicitly set reserved=1. D3D12/Vulkan checks compare
67 records exactly against independent CPU packing, including duplicate slots
and a partial workgroup, and reject a bad occurrence and input/output alias.
DXIL/SPIR-V/MSL payloads generated and PC rebuilt. This is a tested producer,
NOT live EX warp support yet: `GpuModel`, scene aggregation and the shared DXR
material transport still need connection. Warp output is occurrence-based and
visibility-dependent; do not map it to source face IDs or upload placeholders.

Broader live checkpoint: parameterized the reflection capture tool by cartridge,
stage and preroll, with separate names for stage and reflection strength (PowerShell
names are case-insensitive). Mirror OFF/HIGH captures differ and confirm authored
BG2 dispatch in EX Corneria tick 1000, Original Fortuna tick 1000, and Original
Space Armada tick 300. All HIGH images inspected; Armada remains floorless.
Proof: `tmp/reflections-ex-corneria-final-sep19`, `tmp/reflections-fortuna-sep19`,
`tmp/reflections-armada-sep19`. Earlier `ex-corneria` and `ex-corneria-valid`
directories are failed harness attempts, not accepted evidence. These samples
establish default EX material support, not every NAN/colour-warp mode. GPU
colour-warp materials remain unresolved in the ray export and can decline the
reflection pass; do not claim universal model-mode support.

Transparency checkpoint: authored tile ink zero now reveals the supplied scene
backdrop instead of an invented opaque darkest colour. Transparent-CGRAM-black
draws use the original 15-bit CGRAM, not the brightness/effect-transformed display
palette. Raw CGRAM participates in retained upload comparison. Hardware tests
cover transparent black with a nonblack display colour, in-place CGRAM updates
and zero ink; the existing reflection/shadow suite passes and PC rebuilds.
Source audit confirms native menu BG1 and Super FX bitmap content must not be
indiscriminately added as reflection layers. Multi-layer source selection remains
open; this checkpoint corrects transparency but does not claim that integration.

Physical-ground checkpoint: reflection inputs now accept the same camera-space
ground plane as the shadow pass, only when the scene enables that receiver and
an authored BG2 material exists. Secondary rays stop at the nearer of a model
or ground intersection. Forward ground hits sample the authored tiles at their
projected position, adding positional parallax rather than using a distant sky
direction. Stereo translates the plane per eye and restores the eye offset for
central-camera tile lookup. Hardware fixtures verify ground/object depth order,
invalid-plane rejection and eye-offset material addressing; existing DXR,
D3D12/Vulkan SDL ray checks and unsupported-backend checks pass. Windows rebuilt.
Live Mirror OFF/HIGH mono and Full SBS captures passed and were inspected in
`tmp/reflections-ground-mono-sep19` and `tmp/reflections-ground-sbs-sep19`.
This does not make the ground itself a reflective material. Behind-camera ground
still uses angular continuation, and multilayer material/environment coverage
and all-level visual acceptance remain incomplete.

Roll-continuation checkpoint: reflected BG2 no longer clamps offset-per-tile
roll to column 1/32 outside the source view. Metadata fits the valid unwrapped
32-entry roll table, and the ray shader continues that slope at pixel precision
with round-away-from-zero. Tunnels retain their native table path. Hardware
fixtures cover continuation past column 32 and a table crossing the 8192 scroll
boundary. This improves the angular surround; it does not close the remaining
finite-ground or multilayer environment work.

Authored-background checkpoint: the live PC reflection producer now receives
the recorded BG2 draw (not a framebuffer screenshot). It uploads raw VRAM,
scroll rows, tile layout and unique-art exclusions with material metadata;
DXR decodes 4bpp tiles for secondary rays that miss geometry. Both eye producers
receive the same authored environment. Model hits still take precedence. Tile
bytes changing in place and unique-art suppression have exact hardware tests;
the unsupported backend still rejects reflection dispatch. Corneria Mirror
OFF/HIGH captures in `tmp/reflections-authored-bg-sep19` differ; HIGH was visually
inspected and shows mountains/sky on buildings instead of the former tan fill.
No CPU tile rasterization or reflection-image readback was added to presentation.
Metallic and Full SBS captures (`tmp/reflections-authored-metal-sep19` and
`tmp/reflections-authored-sbs-sep19`) also pass and were visually inspected;
their final-frame logs confirm authored BG2 rather than the flat fallback.
Added exact row-scroll and column-scroll precedence hardware tests. D3D12 and
Vulkan ray-geometry/SDL composition checks and unsupported-DXR checks pass.

This supersedes the missing-BG2-connection notes below, but is NOT final material
acceptance: the surround is an angular continuation of authored 2D art, not new
3D terrain. Multi-layer/front-end environments, finite ground intersections,
expanded offset-per-tile roll fidelity and all-level visual proof remain open.
Transparent tile ink currently uses the darkest palette entry, not a second
background layer; scenes without a recorded BG2 still use the old fallback.

Environment sampling checkpoint: cube bilinear taps now cross face boundaries
instead of clamping/stretching the edge texel. An actual DXR fixture sends a
reflection exactly across the +X/+Z boundary of black/white faces and verifies
the linear-light midpoint; all existing reflection and shadow fixtures pass on
the RTX 5070 Ti Laptop GPU. This fixes the environment sampler, not the still
missing live authored-background connection described below.

World-environment orientation checkpoint: DXR and SDL reflection inputs now
carry an orthonormal camera-ray-to-cube transform. Shader transforms only miss
directions; scene intersection stays in camera space. Hardware fixture confirms
a 90-degree mapping from +X to +Z with exact face colour and rejects a collapsed
matrix. Existing reflection/shadow suite, unsupported stub, D3D12/Vulkan ray
and stereo composition checks pass. This supports stable world-aligned sky
data without rotating/resampling its pixels on CPU; actual background data
construction is still NOT connected, so the live fallback remains wrong.

Material-selection checkpoint: appended METALLIC/MIRROR IDs 27/28 to model
styles only (saved prior IDs unchanged; world selector excludes them). They
select conductor/.35 roughness and sharp mirror respectively in the live ray
producer. Reflective Surfaces strength and model effect intensity scale their
blend; RT/Reflections Off performs no imitation colour treatment. CPU fallback
explicitly leaves these styles unchanged; all 29 style CPU/GPU comparisons pass
on D3D12 and Vulkan, pixel tests and settings tests pass. Labels localized.
Live OFF/HIGH captures differ for both styles (`tmp/reflections-mirror-sep19`,
`tmp/reflections-metallic-sep19`); Metallic image inspected. Windows rebuilt.
Environment quality remains the critical unfinished item; the solid palette
zero miss colour still makes these look wrong in open sky. No final visual
acceptance or release claimed. This supersedes earlier unselectable notes.

Menu checkpoint: REFLECTIVE SURFACES is now exposed immediately after RAY
TRACING under 3D OPTIONS, independently of model styling. OFF/LOW/MEDIUM/HIGH
drives the live mono/SBS reflection intensity; requires RT, reports unavailable
on unsupported GPU configurations, and defaults Off. The largest 15-row page
uses 12px spacing to retain its bottom row. Japanese/German/French/Spanish
labels added through the source TSV and regenerated catalog. ROM-backed menu
tests pass dependency refusal, forward/reverse cycling and A hold rejection;
Windows application rebuilt. This supersedes earlier hidden-menu checkpoints.
Visual quality is still incomplete: the palette-zero environment fallback is
wrong, full scene environments must be supplied, and Metallic/Mirror style
selection remains unimplemented. No release or goal-completion claim.

Stereo checkpoint: each SBS eye now has its own DXR reflection owner and the
matching shifted ray geometry/convergence centre. Only complete pairs are
published, never a mono/previous-eye substitute. GPU ray tests compare each
eye to its independently shifted reference on D3D12 and Vulkan. Live OFF/HIGH
captures differ in both Full SBS (`tmp/reflections-full-sbs-sep19`) and Half SBS
(`tmp/reflections-half-sbs-sep19`); Full SBS output was visually inspected.
The shared fallback environment colour is still visibly wrong; this does not
claim finished reflective visuals or headset verification. Menu remains hidden.

Live integration checkpoint: desktop mono resident scenes now request material
payloads and surface metadata when reflections are selected; an independent
DXR owner keeps reflection output alive alongside shadows. Presentation passes
forward its RGBA/intensity/offset. Diagnostic `STARFOX_TEST_REFLECTIVE_SURFACES`
exercises the runtime path without exposing a premature menu row. Added
`tools/check_reflections.ps1`: initial captures correctly failed because missing
surface metadata produced no visible effect; corrected that dependency. Final
Level1_1 16:9 2x captures in `tmp/reflections-live-surfaces-sep19/{0,3}.bmp`
differ, the enabled log confirms resident tracing, and the image was inspected.
No reflection readback is enabled in that verification. Current missed rays
still use palette colour zero (visually pink/tan in this fixture); actual level
environment construction is essential before claiming useful final visuals.
Stereo reflection integration and live menu options are still unfinished.

Revised user design: REFLECTIVE SURFACES will be an independent OFF/LOW/MEDIUM/
HIGH control, available only with ray tracing, and compatible with model styles.
State/persistence implemented: defaults Off; cannot be enabled without ray
tracing; disabling ray tracing clears it; intensity clamped to 0..3. Saved
preferences round-trip all levels and reject invalid values. Windows build,
runtime-input tests and ROM-backed upstream simulation tests pass (including
actual ray-toggle gating checks). The menu row remains deliberately unexposed
until the live scene integration works. Metallic/Mirror are not yet selectable.

Beam eligibility checkpoint: existing application shape classification already
removes surfaces/casters for emissive lasers. GpuScene now enforces the same
rule when a draw incorrectly requests both emissive and ray geometry. All-beam
scenes export no rays; a mixed 33-model fixture removes only the beam's three
vertices and keeps complete ordinary material ordering. Restoring non-emissive
draws restores materials. D3D12 and Vulkan tests pass, Windows build succeeds.
This deliberately omits beams from secondary reflections too; it does not yet
implement reflected emissive laser geometry. Live menu wiring still remains.

Environment checkpoint: resident DXR/SDL reflection inputs accept a validated
six-face RGBA cube (+X,-X,+Y,-Y,+Z,-Z), sampled by the reflected camera-space
direction with linear-light bilinear interpolation. Real secondary geometry
hits still take precedence. Complete cube bytes participate in upload cache
invalidation. Hardware fixtures verify the +X face, in-place cube updates,
object occlusion and malformed-size rejection; DXR shadow/stub checks and
D3D12/Vulkan geometry/composition checks pass. This is an input capability,
not evidence that game backgrounds have been captured: live environment
construction, material eligibility and menu/game wiring remain unfinished.

Material checkpoint: resident reflection input now carries validated roughness
and conductor mode. Sharp mirror remains the exact original single-ray path;
rough surfaces trace eight fixed cone samples and average in linear space.
Conductor mode uses primary material colour with Schlick angle-dependent
reflectance. No random frame seed or screen-space blur. Hardware tests pass
offscreen target colour, tinted conductor response, nontrivial roughness,
repeat-frame equality and invalid roughness rejection; existing shadow tests,
unsupported-backend stub and D3D12/Vulkan transport/composition tests also pass.
Windows executable rebuilt. These are renderer API capabilities, not yet live
menu options. Environment capture, emissive eligibility and game wiring remain.

Compositor checkpoint: added a resident-only reflection stage to SDL effects,
with intensity and vertical offset. It blends only visible model surface pixels,
preserves alpha, and runs before isolated briefing artwork/host overlays. The
dedicated RGBA input is temporarily bound for that dispatch, then the shadow
binding is restored. D3D11 explicitly declines the SDL buffer. Portable shader
payloads regenerated and current Windows executable rebuilt. Actual DXR output
passes exact composition checks on D3D12 and Vulkan at 0/37/100 intensity and
positive/negative offsets, including HUD/world exclusions, invalid surface
pixels and transparent primary misses. Fixtures assert nonempty changes.
Still no live menu selection: game scene submission/material eligibility,
emissive exclusions, reflective environment and metallic roughness remain.

Latest transport checkpoint: SDL reflection output now has a distinct RGBA
type, separate from the one-byte shadow mask. The Windows application builds.
Both D3D12 and Vulkan diagnostics pass actual SDL-produced 16-byte geometry
through DXR and back into an SDL resident buffer, matching reference reflected
pixels exactly across the existing 1/33/2-model scene cases. Readback is used
only for verification. Shadow consumers cannot accidentally bind this RGBA
output as a mask. This proves the transport and GPU-derived normals, not a
finished gameplay effect: live composition, material controls, roughness and
unsupported special-model handling remain before exposing Mirror/Metallic.

First prerequisite implemented: `ray_materials.hpp` packages final face
materials in explicit ray-triangle order, retaining source face IDs, palette
indices, dither pair, indexed texture bytes and scrolled UVs. It rejects invalid
face/corner/texture references atomically. Focused packed-face tests pass for
reordered triangles, negative UV scroll, transparent texel retention and failure
without corrupting previous output. This helper is not yet wired to the live
desktop ray geometry or a reflection shader at that checkpoint.

Follow-up: now wired through opt-in `GpuModelRaySource` and `GpuScene` output.
Scene concatenation rebases texture offsets and preserves triangle order;
incomplete materials yield a null material view without invalidating existing
shadow geometry. Ordinary shadow draws do not request/pack material payloads.
GPU colour-warp and axis specializations explicitly remain unsupported rather
than supplying placeholder colours. D3D12 and Vulkan ray-geometry diagnostics
pass model correspondence plus 1/33/2-model scene resize/reuse cases and the
existing 2,925-vertex checks. Current PC rebuild succeeds.

First DXR reflection dispatch implemented: double-sided primary hit, reflected
secondary ray, indexed texture sampling, transparent-texel rejection, and live
palette colour. RGBA output is separate from byte shadow-mask interpretation.
The diagnostic API currently uploads CPU scene inputs and reads RGBA back;
it is NOT the final resident gameplay path. RTX 5070 Ti hardware tests verify
an off-camera coloured triangle reflected by a tilted surface, texture holes,
palette/texture changes, moved geometry, primary misses, and partial dispatch
groups. Existing shadow diagnostic suite passes afterward, including cached
acceleration structures, resident masks and stereo receivers. Metallic roughness,
environment/background sampling, GPU-resident geometry/normals/material transfer,
composition and menu selection remain unfinished.

Resident-output follow-up: reflection normals now come directly from triangle
positions in the GPU buffer (12-byte CPU-upload or caller-provided vertex stride),
not CPU-computed normals. The shared resident API accepts explicit reflection
inputs and labels output as four-byte RGBA instead of a one-byte shadow mask.
Deferred completion, external release and diagnostic readback retain that format.
13x9 reflected resident output equals the diagnostic image byte-for-byte, and
the existing shadow suite passes after reflection reuse. Unsupported-build
reflection methods decline cleanly. Log: `tmp/dxr-reflections-resident-sep19.log`.
The external-geometry signature is wired, but this new reflection test still
uploads the fixture geometry: actual SDL-produced geometry/import/composition
is the next integration gate, not something proven by this fixture.

Metallic and mirror require actual reflected scene colour. The current DXR
path supplies triangle geometry/alpha coverage and emits one-byte shadow
visibility; it does not carry material colour or output reflected radiance.
Next work must connect triangle material/texture data, trace reflected rays,
and composite reflected colour with roughness/Fresnel for metallic versus a
sharp mirror. Validate reflected moving objects and surfaces outside the main
camera view, HUD/laser exclusions, OFF fallback and stereo. A screen-space
colour trick or palette filter is not acceptance for this request.
# September 19 — six additional drawn/stylized effects

Appended stable IDs 37–42: Stained Glass (jewel-tone quantization/lead edges),
Risograph (two-ink paper treatment), Hologram (cyan contours/scanning lines),
Mosaic (layer-safe tile averaging/grout), Pencil (light paper/graphite edges),
Oil Paint (dominant-luminance neighbourhood brush smoothing). Available for
models and world, grouped in the selector and translated into all four existing
non-English menu catalogs. HUD and alpha remain protected.

Bleach Bypass now uses luminance overlay, 75% desaturation and stronger contrast
instead of the former mild desaturation. Inspected live captures show a clear
change. All seven sample images: `tmp/effects-expanded-{36..42}-sep19/presentation.bmp`.

Windows pixel/input tests pass; all styles/intensities match CPU in D3D12 and
Vulkan. Native Linux desktop and Vulkan/Lavapipe effect checks also pass. Shader
packages regenerated for DXIL/SPIR-V/Metal. Original/EX state and menu suites
pass 4/4, including all six new serialized model IDs. No VR install or release.
