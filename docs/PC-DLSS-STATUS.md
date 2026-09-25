# PC DLSS work — September 13

September 19 menu prerequisite update deployed locally: copied the desktop
executable tested by the 120-frame high-resolution neural/control comparisons
into `build/current` while no current game was running; hashes match. Previous
executable retained at `tmp/pc-before-neural-status-sep19/starfox_pc.exe`.
An enabled startup preference now reads ENABLE DLSS when ordinary DLSS is off,
or UNAVAILABLE for unsupported renderer/stereo/capability. Restart and save-error
labels remain intact. This is prerequisite reporting, NOT live evaluation or
photorealism confirmation. No settings, add-on DLLs or VR artifacts changed.

September 19 user-session correction: archived the normal installation's latest
log and settings in `tmp/dlss5-user-session-sep19`. The 18:19 session has
NeuralUplift=1 and DLSS_MODE=1; the log confirms signed NR initialization,
feature 18 creation and successful inline evaluations at counts 1 and 60.
Its output is 720x448 with 480x299 guides. The user reports no realistic visual
transformation despite this. Therefore disabled DLSS does NOT explain this
session, and successful execution does NOT establish visual acceptance.
Do not present the experimental integration as completed photorealism.
Controlled follow-up at 4x render scale / DLSS Quality / 120 frames:
`tmp/dlss5-high-resolution-sep19/ORIGINAL-on.bmp` (neural ON) versus
`tmp/dlss5-high-resolution-control-sep19/ORIGINAL-on.bmp` (neural OFF).
Both retain ordinary DLSS; only the isolated add-on startup preference changes.
ON logs confirm feature-18 success at 1600x896 with 1067x597 guides. Both
captures were visually inspected: ON changes ground/building shading and softens
some edges, but retains the low-poly scene without a photorealistic makeover.
Thus higher resolution alone does not resolve the user's complaint. This is
evidence of visible processing, not realism acceptance. The isolated installation
was left neural OFF; normal user settings were not modified.
Additional supported-control investigation: the installed add-on's own strings
expose three presets, Natural/Cinematic style and intensity controls. Baseline
runtime logs already report intensity=1 and global_tone=1. Tested preset=2,
style=1 (third preset / Cinematic) in the isolated installation; logs confirm
those settings and successful evaluations. Visually inspected
`tmp/dlss5-cinematic-preset3-sep19/ORIGINAL-on.bmp`: altered tone and subtle
surface detail, still not the requested realistic transformation. Restored the
isolated configuration to OFF with preset/style overrides removed. No unsupported
intensity values, patched runtime, or normal-install changes were introduced.
Repeated feature creation also appears in this session; whether that is normal
scene-transition behavior or unnecessary recreation remains to be investigated.

September 19 local current-install follow-up: `build/current` now contains the
same tested ReShade proxy, RenoDX V4.7 add-on and NVIDIA neural DLL as
`tmp/dlss5-gameplay-sep19`. Previously the normal build deliberately lacked those
files, which hid its conditional DLSS5 menu row. Add-on/neural DLL hashes match
the retained author downloads; NVIDIA's neural DLL signature is valid. The
three files were added only where absent, without replacing user settings or
copying the isolated installation's gameplay configuration.

Initialized NeuralUplift OFF. Fresh installed-runtime Original checks detect
the add-on and validate regular DLSS Quality while neural processing stays OFF:
`tmp/dlss5-current-installed-off-sep19`. A subsequent ON test was not started
because a user game opened; the configurator correctly refused the running
installation. A later live launch reports enabled=ON and its config contains
NeuralUplift=1; that user state was preserved. Startup ON alone is not proof
of neural evaluation. Existing isolated positive feature-18 evidence remains
documented below. Local installation is not release redistribution clearance.

September 19 package gate: `tools/package_dlss.ps1` now invokes the read-only
`tools/verify_dlss_package.ps1` before reporting success (therefore the existing
Windows packaging workflow also checks before creating its archive). It verifies
the exact five-DLL manifest, sizes/hashes, x64 PE/DLL headers, NVIDIA Authenticode
signatures and nonempty required notices, rejecting unexpected DLLs. The current
local package passes. `tools/check_dlss_package.ps1` also passes disposable-copy
negative tests for hash corruption, duplicate manifest entries, unexpected DLLs,
and x86 machine headers with an otherwise matching hash. No DLL is loaded by
these checks. The normal local runtime folder was repackaged with updated notices.

Release review remains OPEN, not satisfied by these technical checks. The pinned
SDK's `bin/x64/nvngx_dlss.license.txt` requires protective distribution terms,
and its supplement describes NVIDIA attribution/mark placement and approval.
Official reference: https://github.com/NVIDIA/DLSS/blob/main/LICENSE.txt .
The application's GPLv3 xBRZ component also requires compatibility review; do
not assume dynamic loading alone resolves that question. No NVIDIA approval,
end-user licensing review, or RenoDX/neural-DLL redistribution permission is
established by the evidence collected here. Do not describe the package as
legally cleared or publish the experimental DLLs on that assumption.

September 19 PC menu integration: compatible already-loaded RenoDX installations
with an explicit NeuralUplift setting expose `DLSS5 (EXP.)` beneath ordinary
DLSS in 3D OPTIONS. Absent add-ons leave the normal menu/VR navigation unchanged.
The row changes the add-on's own persisted startup preference and displays
`ON - RESTART` / `OFF - RESTART` when it differs from this launch. It does not
claim live switching or automatically load unsigned code. Host preference and
capability survive experience reconstruction and save-state restoration without
becoming emulated state. Ordinary DLSS remains separately controlled; successful
neural reconstruction still requires a compatible D3D12/DLSS path.

Verified in the isolated installation: `tmp/neural-menu-on-sep19` saves ON from
an OFF launch; `tmp/neural-menu-restarted-sep19` reads ON and confirms neural
feature 18 evaluation, then saves OFF; `tmp/neural-menu-disabled-sep19` reads
OFF and confirms no feature 18 evaluation while ordinary DLSS still evaluates.
The latter run explicitly asserts the disabled state and absence of neural
evaluation. Pixel/menu availability tests and the Original simulation substrate
tests (including press-versus-hold behavior) pass. Redistribution/legal review,
broader hardware compatibility and visual/performance acceptance remain open;
this is an experimental optional integration, not a production release sign-off.
Actual button navigation also selects/toggles the new row in a 4:3 capture:
`tmp/neural-menu-visual-sep19/menu.bmp` (visually inspected: all rows fit, selected
ON - RESTART is readable). The isolated installation is left OFF afterward.

September 19 startup-control implementation: `tools/configure_dlss5.ps1`
configures NeuralUplift in an explicitly selected, already-installed experimental
installation. Defaults OFF; refuses the matching running game; checks companion
files, preserves other sections/keys, rejects duplicate sections/keys, and keeps
the original ini backup. It never loads or downloads add-ons. This is a startup
configuration utility, not the still-pending in-game menu integration or a claim
of supported hardware. `tools/check_dlss5_configuration.ps1` passes ON/OFF round
trip, default OFF, unrelated-setting preservation, backup preservation, missing
section/key insertion, idempotence and ambiguous-input rejection with inert
temporary fixtures. No normal installation or running game was changed.

September 19 live-control result: used the public ReShade global configuration
ABI documented in https://github.com/crosire/reshade/blob/main/include/reshade.hpp
through an already-loaded proxy (no additional DLL loading). Diagnostic-only
calls set NeuralUplift=1 at presentation frame 8 and 0 at frame 24; getter
readback confirms both values. V4.7's active state stays OFF and no feature-18
evaluation occurs. Ordinary DLSS evaluates all 40 frames successfully. Evidence:
`tmp/dlss5-live-control-sep19`. Therefore writing live configuration is NOT a
working neural toggle for this build. Use explicit restart-required semantics
unless a supported live interface is established. Diagnostic is opt-in through
`-TestNeuralControl`; production does not write ReShade settings. Isolated
configuration remains OFF after the test.

September 19 DLSS5 control investigation: inspected installed V4.7 exports;
only NAME and DESCRIPTION are exported, not a supported direct toggle API.
Binary configuration strings identify `[RenoDX.DLSS5] NeuralUplift`. Setting
`NeuralUplift=0` in only the isolated installation's ReShade.ini was verified
on fresh launch: add-on logs report enabled=OFF, no neural evaluation succeeds,
while regular DLSS still evaluates 24 Original frames successfully. Evidence:
`tmp/dlss5-disabled-proof-sep19`. Earlier enabled runs provide the positive case.
The isolated configuration is intentionally left OFF. This establishes a
startup configuration control, NOT a verified live-reload interface. A menu
integration must either verify live control or explicitly require restart;
do not claim a working live toggle by writing an ini value alone.

September 19 history/DLSS5 follow-up: added GPU fixtures for unchanged static
texture with advancing animation counter, changed texture, previous near-plane
clipping and previous non-billboard type. D3D12/Vulkan pass; rejected history
does not change current colour/depth. Inspected the older controlled DLSS5/plain
EX comparison: visible changes are modest, not evidence of a realism overhaul.
Updated only the authorized isolated add-on installation with the current exe
and static-CRT runtime. Original/EX each pass 32 Quality frames and the archived
add-on logs confirm neural feature 18 evaluation in `tmp/dlss5-current-proof-sep19`.
Inspected current EX output. This proves execution, not product integration:
no user-facing DLSS5 toggle, redistribution approval or broad quality/performance
acceptance yet. The normal installation still contains no unsigned add-on.

September 19 sprite correspondence: added per-pixel motion for whole-object
camera-facing sprites, using the source's rounded/truncated screen rectangles
and normalized texture position rather than rigid polygon transforms. Texture
changes reject correspondence; unchanged texture selection is valid even when
the raw animation counter advances. Transparent texels remain invalid. The
GPU translation-plus-depth/size-change fixture passes D3D12 and Vulkan at a
fractional raster size. Original/EX Quality space runs each evaluate 32 frames
and match normal GPU queue ordering against forced serialization exactly in
`tmp/dlss-sprite-motion-sep19`. Native artwork preservation and default no-jitter
behavior are unchanged. Other sprite-face types/particles and full visual
acceptance remain; this does not establish complete DLSS5 integration.

September 19 packaged runtime verification: rebuilt the native adapter with
MSVC static CRT; `dumpbin /dependents` lists only WINTRUST.dll and KERNEL32.dll.
Repackaged the adapter plus four signature-verified NVIDIA production DLLs into
`build/current/dlss`. Original/EX installed-runtime Quality runs each evaluate
24 frames successfully without explicit adapter/binary/backend environment
paths (`tmp/dlss-static-runtime-sep19`). Packaging now writes a five-DLL SHA-256,
size and version manifest with no machine paths; all five installed hashes
verified. No release uploaded; licensing/publication review remains separate,
and the unsigned DLSS5 add-on is not included.

September 19 expanded scene checks: Original/EX each evaluate all 48 frames in
Training (Quality, 60 FPS) and asteroid space (Performance, 240 FPS), with
installed runtime discovery. Captures/logs: `tmp/dlss-training-quality-sep19`
and `tmp/dlss-space-240-sep19`; inspected the latter EX final image. These are
evaluation/lifecycle checks, not performance benchmarks or motion-vector proof.
Fixed shared one-plane GPU buffer allocation to include compute-write usage:
polygon generation can reuse the buffer previously uploaded by a sprite.
The direct sprite test now requests depth before the same model instance draws
a one-face polygon. D3D12/Vulkan targeted depth tests both pass after rebuild.

September 19 sprite-depth follow-up: Original asteroid scenes had only
whole-object sprites, which supplied no temporal depth and skipped DLSS despite
configuration. Added constant-Z GPU depth for visible sprite texels (transparent
texels remain unknown); kept lighting metadata disabled. Independent raster
remapping explicitly supports these screen-aligned planes, while general
plane restrictions remain. Corrected raster receiver gating to use flag bit 0,
not the packed plane index. Depth/colour/ownership tests pass D3D12 and Vulkan;
Original/EX Quality 32-frame space runs now both evaluate all frames in
`tmp/dlss-space-depth-verified-sep19`. Sprite object-motion vectors still need
implementation; this is not full temporal acceptance.

The first new test incorrectly read an absent buffer and exposed an SDL assert
dialog to the user. The process is gone, test readback now rejects absent
buffers before SDL, and corrected runs pass. No user game was terminated.
The preceding configured-but-unused SDK viewport cleanup crash is fixed by
tracking successful evaluation separately from options configuration. Its
Original no-evaluation regression passed before sprite depth was enabled;
EX evaluated-space regression also passed. Current regular executable rebuilt.
User reports the prior jitter issue appears fixed; preserve the current
native-artwork restoration/no-jitter behavior while completing other inputs.

## Additional queued reports — September 19

User supplied Discord screenshot `C:/Users/kando/AppData/Local/Temp/codex-clipboard-581bae5c-618b-474f-b7ea-64adc066ddf3.png`
(reports dated September 16; build/platform not established). Investigate after
the current DLSS work; these reports are not yet reproduced or fixed:

- Sector Z: tan/peach stepped artifact at the bottom-left of the gameplay view.
- Corridors and Atomic Base: review incomplete widescreen background extensions.
  Preserve the user's earlier constraint against duplicated tunnel elements;
  do not crop gameplay to the original viewport.
- Sector Z victory: last speaking pilot's portrait, static-box texture and
  dialogue remain visible until the map loads instead of clearing.
- Victory presentation: investigate the unexpected top-only letterbox band;
  compare with source behavior before deciding how to correct framing.

September 19 lifecycle follow-up: switching GPU -> software -> GPU previously
closed the SDK permanently. The host now retains its verified runtime location,
reopens after old-device destruction and before replacement-device creation,
and binds each replacement renderer. Empty environment overrides are treated
as unset. New `check_dlss_lifecycle.ps1 -RendererCycle` checks fresh SDK startup,
swapchain upgrade, initial history reset and evaluation in every GPU segment.
Original/EX installed-runtime runs each pass four renderer switches over 40
frames (24 evaluated GPU frames, 16 software frames), evidence in
`tmp/dlss-renderer-cycle-verified-sep19`. Initial harness assumed exactly one
swapchain per renderer; SDL resize legitimately creates additional swapchains,
so assertions now validate each restart segment instead of that false count.
This is lifecycle coverage, not proof of complete DLSS motion quality.

September 19 next pass: preserve native non-terrain background artwork after
DLSS, alongside the existing HUD restoration. These are screen-space tilemaps,
not surfaces with the pinhole-camera motion assumed by reconstruction. Explicit
terrain bit 27 remains eligible, as do model/textured geometry and world sprites.
The restoration remains GPU-resident and precedes normal presentation effects.
Focused D3D12/Vulkan tests verify ownership, opaque black and alias rejection.
Installed-runtime Original/EX 32-frame gameplay checks pass in
`tmp/dlss-artwork-stability-sep19`; inspected EX frame 22. The matched EX upper
region mean frame difference is now 0.183 versus 0.687 before this change and
0.211 with DLSS off. This supports improvement for the tested background, not
complete temporal acceptance. Jitter remains temporarily disabled; model-edge,
moving terrain and wider-scene acceptance are still outstanding.

September 19 follow-up: laser shape headers are explicitly excluded from
PC lighting receiver metadata and ray caster collection (Original and EX).
Emissive GPU draws also clear receiver metadata underneath opaque beam pixels,
without removing temporal depth or changing palette colour. Focused GPU depth
tests cover that ownership and pass on D3D12 and Vulkan.

DLSS wobble is NOT resolved. Default subpixel jitter is temporarily disabled
pending reliable screen-space background correspondence; diagnostic jitter
remains available. Fresh installed-runtime Original/EX 32-frame runs pass in
`tmp/dlss-beams-stability-sep19`. EX upper-region frame-change measurement is
0.687 with this mitigation versus 2.809 with centered jitter, and 0.211 with
DLSS off. These are matched-scene difference measurements, not a general
quality score. Reconstruction remains enabled but loses jitter-based sampling
benefits. Do not label DLSS motion release-ready yet.

PRIORITY REGRESSION: user reports DLSS extremely wobbly/shaky and unplayable.
Do not equate successful SDK evaluation or still images with acceptable motion.
Pause DLSS5 expansion and investigate live temporal stability, especially the
reported 1x render-upscale configuration. Added configurable test render scale
and an explicit zero-jitter diagnostic override to separate sampling from
motion/depth errors. NVIDIA's official sample applies positive pixel-offset
projection translation and sends the same offset to Streamline, matching our
model sign convention; no unsupported sign flip applied. No fix yet claimed.

Code audit found an additional concrete failure: rejected temporal frames
could present raw jitter, and transition readback could reuse jittered native
pixels. Added projection-consistency preflight before resizing/jitter and
unjittered recorded-scene replay whenever jittered reconstruction fails or
transition composition falls back. Building; not yet proven to resolve all
reported shaking. Added sequence capture and explicit no-jitter comparison
options to the targeted harness. DLSS5 work stays secondary to this regression.

1x Performance reproduction completed for Original/EX with per-frame captures
in `tmp/dlss-motion-1x-sep19` and `tmp/dlss-motion-1x-nojitter-sep19`.
All frames evaluated, so fallback alone does not explain instability.
New `tools/check_dlss_sequence.py` measures matched frame-pair background
change, not overall visual quality. EX frames 16–32: mean absolute sky change
0.211 with DLSS off, 2.746 with jittered DLSS, 0.687 without jitter (0–255
channel units). This reproduces excessive temporal variation and implicates
background sampling/correspondence; disabling jitter reduces but does not
resolve it and is not being substituted as the final fix. Input is 200x112
for 400x224 output at this setting. Consecutive jittered EX frames inspected.

Sampling audit: jittered 2D producers used pixel-edge inverse sampling and
edge-based scatter bounds, while model/depth reconstruction uses pixel
centers. Updated shared fixed-point helpers with explicit centered sampling
and enabled it for jittered backgrounds, raster/text, span producers and CPU
composition. Non-jitter legacy lookup stays unchanged. Regenerated all six
affected portable shaders successfully. CPU expectation fixtures and combined
motion validation are next; not claiming the visual regression resolved yet.

Latest integration evidence (September 19): menu-selected Quality completed
32 frames each for Original and EX, initial-only reset, matching queued and
serialized captures: `tmp/dlss-menu-batch-fixed-sep19`. Captures inspected.
The user's UNAVAILABLE report exposed a real deployment gap: Windows still
defaulted to Vulkan. Installed SDK now selects D3D12 at initial device creation
(explicit backend overrides remain honored). Added an installed-runtime test
that removes all runtime paths and backend overrides. Current build compiled.

Windows x64 packaging now downloads checksum-pinned official SDK 2.14.1,
builds the native adapter with static CRT, verifies NVIDIA runtime signatures,
and includes production DLLs and full notices under `dlss`. Local packaging
passed. CI itself has not run. SDK attribution/marketing and other applicable
distribution obligations still need release review; no release pushed.

Isolated third-party gameplay experiment: Original/EX SDK evaluation completed
in `tmp/dlss5-gameplay-proof-sep19`. EX's final ReShade log confirms feature 18
creation and successful inline evaluation. This is genuine game input, not
only the earlier analytical fixture. EX capture inspected, but comparison with
the normal build is confounded by different portable settings; no neural
quality/performance claim yet. Add-on logs must be preserved per process for
stronger Original evidence. Add-on redistribution and user-facing control
are not implemented. Files remain isolated in `tmp/dlss5-gameplay-sep19`.

Priority note: after DLSS/DLSS5 integration, investigate reported black-screen
startup in release 0.0.6.7. Platform/GPU/logs are not yet available. Do not
interrupt the integration to chase this report unless new evidence makes it
an integration blocker.

Additional user queue, after integration and startup investigation: fix EX god
nuke still killing the player; add a reflective complete metal/mirror model
effect; research and implement roughly ten additional 2D/3D effects. These are
queued requests, not implemented features or permission to change focus now.

Also queued: a Double Rendering Distance option. Display objects twice as
early, but keep their routines/animation static until the original spawn time.
Early visual presence must not advance gameplay routines or collision state.

Also queued: improve VR pre-game menu directional precision. Pressing Right
to adjust an option too easily also navigates Up/Down. Add deliberate axis
selection/hysteresis so horizontal adjustments do not accidentally change
rows; preserve intentional vertical navigation. Menu-only behavior, not a
change to gameplay stick precision. Investigate and verify after DLSS work.

End-of-work cleanup requested: inspect and remove obsolete builds and truly
unneeded temporary files only after integration. Preserve current/platform
toolchains, required SDKs, assets, saves, signing keys, and useful proof.

## Connected integration batch in progress (September 19)

Native resized scene sampling now maps output pixel centers directly into
the source, avoiding integer reference-canvas double rounding. Colour,
surface metadata, depth and motion share that lookup; original-size and
mosaic paths retain their established sampling. Added a native identity-grid
fixture, not yet run. Regenerated the portable compositor shader.

DLSS host fallback and preparation failure now invalidate all temporal
history consistently. Both focal axes and projection centers are validated
before evaluation; preparation/evaluation share mode parsing. Successful
shutdown clears cached device and render-plan state. The SDK swapchain
warning corresponds to the intentionally retained native swapchain reference
used to restore SDL ownership; no speculative reference-count change made.

Per user instruction, regression runs are deferred until this larger connected
batch is ready. These newest changes are not yet runtime-verified.

Capability handling now caches the actual device support result and exposes
availability/reason for subsequent menu integration. Unsupported hardware
keeps its native SDL swapchain rather than failing the optional presentation
hook, and skips DLSS preparation/evaluation. This is an integration safeguard,
not a verified explanation or fix for reported 0.0.6.7 black-screen startups.
The alignment/history batch compiled successfully; capability changes are
under compilation. No regression suite has been run for this pending batch.

Capability build passed. Added default-off persisted DLSS quality preference
(Off/Quality/Balanced/Performance/DLAA), strict read/write range validation,
and pending round-trip fixtures. PC settings snapshots preserve this value.
Menu selection and use of the saved preference by the evaluator are not yet
wired; existing diagnostic environment controls remain the active path.

Follow-up: connected the DLSS row in 3D Options, saved quality restoration,
and runtime mode changes to temporal history, native-resolution scene drawing,
jitter, terrain tagging, pre-HUD background capture and SDK evaluation.
Explicit installed adapter/runtime paths now allow capability discovery
without test switches. Missing runtime, incompatible renderer and stereo
output display UNAVAILABLE; the stored preference is preserved. Normal
installation/path discovery and complete input coverage remain unfinished.
Updated the diagnostic off-control to omit installed runtime paths.
Compilation caught a missing background callback capture; corrected it and
restarted compilation. No runtime verification of this combined batch yet.

Next connected changes: executable-local optional runtime discovery, keeping
DLSS quality when restoring save states, and a menu-selection harness mode
that removes all temporal/evaluation diagnostic enable switches. The runtime
layout is `<executable directory>/dlss/starfox_dlss_native.dll` alongside the
official SDK runtime DLLs; explicit paired `STARFOX_DLSS_ADAPTER` and
`STARFOX_DLSS_BINARIES` paths still override it. Existing signature checks
remain active for SDK binaries; no proprietary DLLs bundled or redistributed.
Missing runtime leaves normal rendering available. Full input/visual quality
and DLSS5 integration are still incomplete; this is not release acceptance.

Combined validation: Windows build, runtime settings tests, and D3D12/Vulkan
compositor checks pass, including direct native sample mapping and reduced
early/late enlargement. First menu-path gameplay run failed: clearing process
variables through .NET left an empty mode override, and SDK stderr split the
restoration log line. Corrected empty-mode handling, harness variable removal,
and single-write presentation logging. Rebuild/retry pending; no gameplay
pass claimed for this batch yet (`tmp/dlss-menu-batch-sep19`).

## Reduced layers and world-sprite ownership (September 19)

Early background and late scene layers now use the SDK render extent when
scene conversion supports it, with original-size fallback for unsupported
draws. Composition samples their actual dimensions directly, rather than
rounding through the CPU reference canvas. World billboards retain their
2D styling tag but carry a separate world-sprite marker so temporal world
composition includes them and HUD restoration does not overwrite them.
CPU gameplay HUD overlays remain on the full-resolution presentation path;
complete native HUD/reticle separation still needs acceptance verification.

Focused D3D12/Vulkan depth and composition checks passed. Live Original and
EX 32-frame Quality runs in `tmp/dlss-reduced-layers-sep19` pass native
533x299 evaluation, changing jitter phases, initial-only history reset and
identical queued/serialized captures. Both final captures visually inspected.
This proves execution and synchronization, not final temporal image quality
or a measured performance gain. SDK shutdown still emits a swap-chain
reference-count warning; investigate lifecycle ownership before release.

Remaining: full world correspondence, native HUD acceptance, capability/menu
integration, performance and visual acceptance, and DLSS5 gameplay hookup.
These remain opt-in diagnostic paths; no release pushed.

## Scene jitter batch (September 19)

Opt-in `STARFOX_TEST_DLSS_JITTER=1` with native raster evaluation now drives
a deterministic 32-phase Halton sequence through scene models, billboards,
raster commands, projected text, grid/particle/dust spans, early/late GPU
layers and CPU world composition. Actual input-pixel jitter reaches temporal
terrain reconstruction and the SDK; history poses remain unjittered. Native
and presentation draw scales are accounted for independently.

Added shared 1/256-phase integer sampling for 2D producers. Initial floating
mapping disagreed at exact boundaries; initial signed remainder also differed
on Vulkan. Both were replaced with bounded unsigned quotient/remainder
arithmetic. Zero-jitter output retains its established sampling. BG2 scatter
fills disjoint shifted intervals and clamps outer cells, without stale edges.

Windows builds. D3D12 and Vulkan projection/raster/text, background, depth/
motion and composition checks pass. Added 16 raster and 48 text fixtures plus
27 background size/phase cases, with direct/scene comparisons, and sequence/
invalid-jitter checks. Existing particle/grid tests pass; dedicated jittered
particle/grid fixtures still need strengthening. Live Original/EX 32-frame
Quality runs pass changing phases, native 533x299 inputs, initial-only history
reset and exact queued/serialized final captures in
`tmp/dlss-scene-jitter-fixed-sep19`; both final captures visually inspected.

This is still diagnostic-only. Remaining integration includes full-resolution
native HUD separation, reduced early/late layers, broader world motion/depth
coverage, capability/menu controls, visual/performance acceptance and DLSS5
gameplay hookup. No release or unrelated bug work was performed.

## Native scene/composition batch (September 19, later)

`STARFOX_TEST_DLSS_NATIVE_RASTER=1` now selects the SDK render plan before
the main ordered scene is submitted. Original and EX both evaluate native
533x299 inputs for 800x448 Quality output; this is no longer the previous
full-resolution main-scene render followed by input resampling. Preparation,
source projection dimensions, output composition dimensions and focal X/Y
are connected. CPU, early background and late overlay inputs retain their
reference coordinates. Those layers still render at their original sizes.

Added independent command-raster and whole-object billboard output, a copied
scene conversion preserving the original fallback recording, and independent
compositor output with correctly scaled motion. Legacy motion arithmetic is
preserved exactly (an initial shader reassociation changed its last bits;
the corrected shader passes exact legacy and fractional motion tests).
Wave-mode models still decline scene conversion and use the existing path.

Focused checks: D3D12/Vulkan raster/projection, compositor and depth/motion
checks pass, including fractional raster/billboard coverage, textures,
palette, offsets, mosaic, HUD writes and temporal ownership. PC builds.
Live Quality Original/EX 16-frame runs pass continuous history and identical
queued/serialized captures in `tmp/dlss-native-raster-billboards-sep19`.
Both final captures inspected. The earlier EX run intentionally failed the
new native-size assertion and exposed the billboard restriction now fixed.
No full-game regression suite run for this batch.

Still not a finished user-facing DLSS feature: full-scene jitter, complete
world correspondence, full-resolution native HUD separation, reduced early/
late layers, capability/menu integration and performance measurement remain.
DLSS5 remains isolated-test-only; this batch does not integrate that add-on
with gameplay or redistribute its binaries. No release pushed.

## Render-plan/evaluation batch (September 19)

Particle, dust and grid scene records now carry optional logical viewports;
their shared span shader maps logical cells directly into independent output
dimensions, including signed edge coordinates. CPU replay rejects resized
records before clearing its target. Projection batch builds and passes D3D12
and Vulkan, including added 149x127 dust direct/scene reference comparisons.
Existing particle/grid tests pass, but dedicated fractional particle/grid
fixtures and gameplay render-plan routing still remain. No native performance
benefit is claimed until the application actually selects this path.

Preparation now exposes SDK render dimensions separately from evaluation.
Evaluation accepts native input extents independently from final/HUD extents,
rejects mismatched plans/devices, supports independent focal X/Y scaling and
passes actual raster jitter to terrain reconstruction and SDK constants.
Input-extent changes and failed evaluations invalidate history; failed texture
allocation cannot reuse a stale configured plan. Adapter explicitly declares
unjittered motion. These APIs are not yet selected by native scene rendering.

Windows application and MSVC adapter compile. Projection tests pass, including
rounded SDK ratios. One consolidated Quality lifecycle run, Original and EX
16 frames each, passes actual SDK evaluation, continuous history and exact
queued/serialized captures in tmp/dlss-plan-batch-sep19. This still uses the
full-resolution diagnostic resample path. Full-scene jitter, native reduced
scene routing and DLSS5 gameplay integration remain incomplete.

## Independent background raster dimensions

Projected text also supports independent output sizing, preserving its original
projection and glyph sampling while dispatching only the target pixel count.
Twelve added fixtures compare 299x255 direct/scene output to the original
1x/2x/4x reference. Both D3D12 and Vulkan pass these and existing projection,
stereo text, particle, dust and grid checks. This is not yet host-enabled.

All three background layers now accept a logical viewport independently from
their output texture dimensions, including through GpuScene composition.
Nine fixtures cover 267x149, 533x299 and 800x448 output from a 400x224 canvas;
packed pixels and coverage agree with logical reference sampling and direct
versus scene rendering on D3D12 and Vulkan. Existing 432 background cases pass.
CPU replay rejects these resized records before clearing the destination;
recovery must retain the original-resolution recording.

This removes a scene integration restriction, not the remaining gameplay host,
particle/raster sizing or full-scene jitter work. DLSS is still unfinished.

## Direct model raster dimensions

GpuModel and GpuScene now accept output dimensions independent of the logical
projection viewport. Span generation allocates/emits the requested row count;
depth projection and both motion projections use independent X/Y scale factors.
Raster jitter remains in output pixels. This permits direct geometry rendering
at reduced SDK sizes instead of resizing a full-resolution model image.

D3D12/Vulkan tests render 149x127, 299x255 and 533x299 from a 224x192 logical
viewport, with/without fractional jitter. Analytical translation motion passes;
scene composition preserves exact pixel/depth/motion outputs. Existing depth,
motion, resampling and HUD tests also pass. The span tests additionally verify
1.5x raster sizing against the software renderer.

This API is not yet selected by the gameplay DLSS host. Mixed background,
particle and raster chunks still need independent sizing. Whole-object
billboards and wave effects reject custom model sizing; CPU recovery must use
the original recording. Full-scene jitter and DLSS5 gameplay remain unfinished.

## Quality/Balanced/Performance gameplay connection (after 0.0.6.7)

The diagnostic PC host now requests all three SDK super-resolution modes in
addition to DLAA, uses the SDK's optimal input dimensions, and keeps final
output/HUD at the original presentation size. Mode changes release/reconfigure
the viewport and reset history. The checker accepts `-DlssMode` and verifies
the requested mode was actually evaluated.

A GPU-only preparation pass area-filters world color to the requested size,
selects the nearest depth and its paired motion, scales motion to input-pixel
units, and preserves invalid motion sentinels. Fractional/integer ratios,
constant-color preservation, depth/motion pairing, resize/reuse and alias
rejection pass on D3D12 and Vulkan. Generated DXIL/SPIR-V/MSL freshness passes.

Important: this currently reduces an already-rendered full-resolution scene.
It enables actual SDK SR evaluation but does NOT yet provide the main native
low-resolution rendering performance benefit. Full-scene jitter, direct
lower-resolution scene rendering, remaining world correspondence and normal
menu/capability integration remain. Do not label this finished DLSS.

`tmp/dlss-quality-gameplay` passes Original/EX, 16 frames each, initial reset
only and exact queued/serialized output. Quality uses 533x299 -> 800x448.
The Original screenshot was inspected with native HUD restored. These changes
are after the 0.0.6.7 release tag and have not been published.

`tmp/dlss-balanced-gameplay` and `tmp/dlss-performance-gameplay` each pass
32 frames per Original/EX, initial reset only and identical queued/serialized
final images, with clean SDK shutdown. Balanced uses 464x260 and Performance
400x224 for 800x448 output. Existing 5120 guide and 366183 model-depth sample
checks still pass alongside the new resampling fixtures on both GPU backends.

## Direct gameplay terrain-motion verification

The opt-in terrain audit now also downloads the RG32 motion texture actually
supplied to DLSS. For each classified ground pixel it independently intersects
the camera ray with the ground in double precision, transforms the point to
the previous camera, and compares previous-minus-current pixel displacement
against the GPU result (0.02 pixel tolerance). Missing motion and mismatches
fail the diagnostic. Samples include frame 1 and every subsequent 16th frame,
not merely an initially stationary frame. This adds no normal rendering stalls.

`tmp/dlss-terrain-motion-sampled` passes 64 evaluated frames per Original/EX at
60 FPS, initial history reset only. All classified pixels in each of five
sampled frames have usable depth and matching motion, including nonzero camera
movement. This verifies the terrain-plane reprojection transport for these
scenes; it does not establish all background artwork/scroll motion or finish
full-scene jitter, SR modes, or neural gameplay integration.

`tmp/dlss-terrain-motion-sampled-240` also passes 96 frames per experience at
240 FPS, queued and serialized. Seven sampled frames in each run have zero
motion mismatches, including moving/interpolated frames; serialized final
images match exactly, only the initial history reset occurs, and SDK shutdown
is clean. The Windows executable includes these default-off diagnostic checks.

## Terrain ownership survives intermediate GPU merges

Fixed scene merge and fused raster shaders dropping terrain bit 27. Ownership
now follows visible background color through these passes and is cleared by
covering models or opaque HUD pixels. Regenerated DXIL, SPIR-V and MSL assets.
New raw-metadata fixtures exercise both fused and separate scene merge paths;
D3D12 and Vulkan background/depth suites pass, including 183997440 background
samples and 5120 temporal-guide samples.

Added opt-in `-AuditTerrain` to the gameplay checker. It downloads the actual
world ownership buffer and the depth texture supplied to DLSS on the second
evaluated frame, reporting classified versus usable terrain depth pixels.
Readback is diagnostic-only; ordinary rendering gains no fence or CPU transfer.
The checker rejects missing/empty depth evidence or evaluation failure.

Correction to earlier sections: matched profile logs and color screenshots
alone did not prove terrain reached DLSS. The intermediate metadata loss above
meant those earlier gameplay runs could not establish that claim. Direct depth
auditing is required in addition to those existing checks. This remains a
diagnostic DLAA path, not completed SR modes or a release-ready DLSS option.

Direct gameplay evidence after the fix:
- `tmp/dlss-terrain-merge-audit`: 32 frames per Original/EX and queued/serialized
  mode, initial reset only, exact serialized output match. All 143669 Original
  and 163287 EX classified ground pixels have usable depth on the audited frame.
- `tmp/dlss-terrain-merge-level1-4`: 8 frames per experience; all 156258/163219
  classified terrain pixels have usable depth.
- `tmp/dlss-terrain-merge-level1-6`: 8 frames per experience; all 166819/166838
  classified terrain pixels have usable depth.

All runs evaluated successfully and shut down cleanly. These depth counts prove
transport and usable depth for the sampled frames, not full background motion
accuracy or quality across every stage. Windows executable rebuilt; generated
scene/raster shader freshness and whitespace checks passed.

## Additional authored terrain profiles

Added 1-4.SCR and F-1.SCR definitions (ground rows 360..511), using their
complete source fingerprints 80d53f12 and 33b82640 with the same uniform
tile-relocation normalization. Source BGS.ASM selects these maps for ground
scenes; their lower tile rows contain the authored ground gradients. Changed
maps remain unknown, and first-word palette/flip bits cheaply reject unrelated
profiles before hashing.

Both assets pass original/relocated and rejection fixtures on D3D12/Vulkan.
The gameplay checker now accepts a validated `-Level` argument.
`tmp/dlss-terrain-level1-4` and `tmp/dlss-terrain-level1-6` each pass all 16
Original/EX evaluations, log matched terrain rows, and shut down without SDK
errors. Original screenshots for both were visually inspected with comms/HUD
present. These runs do not prove all other ground or EX-specific backgrounds.

## Authored ST-P terrain enabled in diagnostic gameplay

Added a source definition for ST-P.SCR's rows 360..511, the ground-gradient
rows after the sky/cloud/mountain artwork. Classification requires the complete
8192-byte tilemap fingerprint (FNV-1a 536ac185), normalized for the loader's
uniform character-index relocation, while retaining palette/priority/flip bits.
Direct VRAM comparison in `tmp/dlss-terrain-vram` found every one of 4096 words
relocated by +192 in both Original and EX. Arbitrary modified maps do not match.
Tunnel/wrong-mode/wrong-layout maps remain excluded.

The PC diagnostic path now assigns this profile to matching GPU BG2 draws,
connecting authored coverage through composition to the camera-plane converter.
`tmp/dlss-gameplay-relocated-terrain` logs rows=360:512 for both experiences and
passes 64 evaluations each, initial reset only, clean shutdown and exact
queued/serialized comparison. `ORIGINAL-on.png` was inspected with HUD intact.
The background checker accepts the authored ST-P.SCR as an optional fixture;
original/relocated matches and modified/tunnel/tile-size rejection pass on
D3D12/Vulkan. Other tilemaps remain unclassified: this does not complete all
backgrounds, full-scene jitter, SR modes or release acceptance.

## PC terrain-plane handoff

The diagnostic PC DLSS host now consumes packed terrain ownership with the
actual camera-space ground plane, current/previous pixel projections and
camera mapping. The plane is sourced independently of whether ray tracing is
enabled; source shadow availability and tunnel exclusion gate it. Changing
ground height, losing the plane or a history reset invalidates terrain motion.

`tmp/dlss-gameplay-terrain-plane-handoff` passes 64 evaluated frames each in
Original/EX, only the initial global reset, clean shutdown, no evaluation errors,
and exact queued/serialized output comparison. Authored terrain row ranges are
still empty by default, so this proves the handoff's regression behavior, not
that gameplay terrain pixels now have complete depth. Per-background source
classification remains necessary; unknown pixels have not been guessed.

## Source-row terrain ownership transport

GpuBackgroundSettings accepts optional authored BG2 terrain source-row ranges.
The GPU marks qualifying visible pixels with bit 27, following scroll/mosaic
sampling and continued ground rows; tunnels are excluded. GpuComposite retains
this background ownership only while visible: native models, CPU foreground,
late overlays and margin replacements clear it. The terrain converter can now
consume that packed ownership directly, avoiding a separate mask readback.

D3D12/Vulkan background checks cover scrolling at 1x/2x/4x, tunnel exclusion,
and an actual background->compositor test with model and opaque-black HUD
occlusion. Existing 183997440 background samples remain unchanged; D3D12's full
composition/effects suite passes. Temporal guide checks pass 5120 cases with
both packed and standalone masks on both backends. PC builds.

Ranges remain empty by default. Authored per-background ranges and their plane
association still need integration/validation before gameplay terrain guides
are enabled. This implements mask transport, not all-stage terrain completion.

## Cartridge coverage evidence

Added `-CaptureBackground` to the gameplay checker to preserve expanded,
unscrolled and complete tilemap images alongside evaluation logs. The run in
`tmp/dlss-terrain-source-layers` passes Original/EX evaluation and shutdown.
Inspected `ORIGINAL-on-bg2-tilemap.png`: Corneria's sky, mountains and ground
occupy one 512x512 BG2 tilemap (map 28672, characters 20480, scroll Y 232).
Terrain classification must follow source sampling coordinates, not a fixed
screen-space horizon or the shared background layer tag.

Important source clarification: WORLD.ASM's `if_ground` branch selects the
ground-dot mode (`dotsflag=1`); it does not emit per-pixel terrain coverage.
That flag alone is not sufficient to enable the planar terrain converter.
No terrain mask has been inferred from the screenshot's colours.

## Explicitly masked GPU terrain guides

Terrain sampling now accepts current-frame raster jitter, reconstructing the
plane at the displaced sample while excluding jitter from motion vectors.
D3D12/Vulkan checks pass 2560 samples including zero/fractional jitter on
sloped planes. Nonfinite jitter is rejected. This is converter support, not
yet a connected full-scene gameplay jitter sequence.

Follow-up validation rejects degenerate plane normals and non-affine camera
history. The GPU suite now passes 1280 guide samples on D3D12 and Vulkan,
including sloped planes, terrain behind either camera, exact coverage value 1
(other values are not terrain), existing model depth and reset behavior.
This strengthens the converter; terrain-mask integration is still outstanding.

GpuTemporalInputs now accepts optional visible-terrain coverage, a camera-space
plane and current/previous projection/camera data. On-device ray/plane
intersection provides depth and reprojection provides motion only for explicitly
covered pixels without valid model depth. Uncovered artwork stays unknown;
resets preserve depth but invalidate motion. This path has no CPU pixel readback.

D3D12/Vulkan checks pass 512 depth/motion cases spanning masked/unmasked pixels,
model ownership, resets, invalid source guides and exposure. Generated DXIL,
SPIR-V and Metal bindings validate (no Metal execution claimed). PC builds.
The terrain API is not yet enabled in gameplay: reliable terrain-only coverage
must still be carried from the cartridge renderer into composition. Full-world
DLSS is therefore still incomplete; a plane alone does not classify artwork.

## Camera axes and terrain ownership follow-up

SDK camera direction vectors are now normalized after inversion of the
Q15-derived view matrix; the exact quantized matrix still drives reprojection.
Unit tests cover quantized axes, up-sign conversion and degenerate/nonfinite
rejection. `tmp/dlss-gameplay-camera-unit-axes` passes 16 Original/EX evaluation
frames with only the initial reset and clean shutdown; PC build passes.

Terrain input investigation: the PC scenery pass rewrites all scenery tags to
PixelLayer::background, and GpuBackground preserves no terrain-only ownership.
The optional shadow receiver plane therefore cannot by itself identify terrain
pixels: sky, planet art and tunnel art share that tag. Full-world temporal depth
needs an explicit terrain coverage source carried through composition, not a
blanket plane intersection applied to all background pixels. No guessed terrain
depth or motion has been enabled.

## Interpolated gameplay camera history

The PC host now forwards the actual interpolated gameplay camera and Q15-derived
view matrix to DLSS. Clip history combines projection changes with current-view
to previous-view mapping, and SDK camera position/basis follow that camera.
Mapping inverts the quantized matrix rather than assuming an exact orthonormal
rotation; translations follow the game's 65536-unit coordinate wrap. Singular
or nonfinite camera transforms fail back to the complete original frame.

Math tests cover rotation/translation, wrap crossing, combined clip mapping and
invalid camera input. `tmp/dlss-gameplay-camera-history` passes 96 frames each
of Original/EX at 240 FPS with exactly one initial reset, no evaluation errors,
clean shutdown and exact queued/serialized comparison. `ORIGINAL-on.png` was
visually inspected: world and restored HUD remain present. This supersedes the
earlier missing-rigid-camera-history limitation for the diagnostic PC gameplay
path. Background/ground per-pixel guides and full-scene jitter are still missing;
camera constants alone do not implement them. Broader scene/cut coverage remains.

## Static-model history continuity

Fixed a real periodic-reset bug: the application gives static models the global
animation counter, but both ModelMotionHistory and GpuModel compared its raw
value. Each simulation tick therefore discarded unchanged geometry history.
Both now compare selected geometry frames (static shapes always match; animated
shapes compare modulo the decoded frame count). Different geometry frames still
invalidate correspondence, as do entity changes, missing frames and scene cuts.

History unit tests cover static tick changes, distinct animated frames and
wrapped frames. D3D12/Vulkan motion checks cover different static counters at
1x/2x/4x, stationary/moving and jittered/un-jittered geometry. Runtime proofs:
`tmp/dlss-gameplay-static-history` has 64 evaluated frames at 60 FPS, and
`tmp/dlss-gameplay-static-history-240` has 96 at 240 FPS, for each experience.
All have exactly one initial reset, clean shutdown, no evaluation errors, and
byte-identical queued/serialized captures. Earlier runs reset every few frames.
The test runner now asserts the evaluation count and optionally requires
continuous history for these stable-scene fixtures. This is not full DLSS
completion: world inputs, host jitter and user-facing modes remain outstanding.

## Projection history

The host now uses a validated, shared perspective/inverse calculation and carries
projection-only clip reprojection across successfully submitted evaluations.
Changed focal length or principal point no longer silently receives identity
clip transforms; reset frames still do. Invalid/nonfinite projection inputs fail
back to the complete original image. This assumes the same camera coordinates:
it does not yet supply the missing rigid camera-motion history.

`starfox_temporal_projection_tests` passes 144 samples spanning aspect ratios,
focal lengths, off-center views and depths, checking pixel projection and both
reprojection directions to 1e-5 normalized-device tolerance. Invalid inputs are
also rejected. `tmp/dlss-gameplay-projection-history` passes Original/EX real
evaluation and exact queued/serialized output comparison, with clean shutdown.
These runtime scenes are regression coverage, not proof of every camera cut.

## Queue-ordered evaluation

Removed the host's per-frame CPU fence wait. The pinned SDL D3D12 backend submits
guide conversion, native evaluation, HUD restoration, effects and presentation
on one command queue; transitions and submission order protect resident texture
reuse. Resize/reconfiguration and shutdown still wait for GPU idle. Failed
output allocation now triggers reconfiguration on the next attempt.

`tools/check_dlss_lifecycle.ps1 -Evaluate -CompareSerialized -OutputDirectory
tmp/dlss-gameplay-queue-ordered` passes Original and EX, with byte-identical final
captures between queue-ordered and explicitly GPU-idle-serialized evaluation,
distinct DLSS-off captures, no SDK evaluation errors, and clean shutdown. This
is synchronization regression evidence, not a measured FPS improvement or
completion of temporal reconstruction quality. PC build and diff checks pass.

## Model raster jitter plumbing

GpuModel/GpuScene now accept an explicit output-pixel raster displacement without
mutating temporal history poses. Planar depth follows the displaced projection;
motion removes the displacement when reconstructing current camera positions.
Nonfinite offsets and jitter on non-subpixel geometry are rejected. The default
zero displacement preserves existing callers.

The geometry-depth check passes on D3D12 and Vulkan with stationary and translated
models, zero/nonzero jitter, 1x/2x/4x scales, and scene/compositor HUD ownership.
The PC target builds. This is renderer plumbing, not completed DLSS jitter:
the host sequence, SDK sign convention, and non-model world jitter remain to be
connected and validated. No menu/release readiness is implied.

## Gameplay world/HUD split (latest)

The diagnostic gameplay path now retains the CPU backdrop at native-layer
insertion, before later HUD writes. It composes a separate world-only GPU input,
excluding two_d-tagged CPU/native/late artwork while retaining subsequent non-HUD
world writes. DLSS evaluates that world input; a new GPU pass restores exact
two_d-tagged pixels from the original final compositor output before normal
effects/presentation. Host overlays already applied afterward remain afterward.
Failure falls back to the original complete frame, not the HUD-free input.

Evidence: D3D12/Vulkan HUD restoration checks cover 128 exact pixels across all
five layer tags, opaque black and output-alias rejection. The compositor suite
adds world-only native/CPU HUD exclusion at multiple scales; all 108 existing
composition fixtures and effects/overlay suites still pass on D3D12.
`tmp/dlss-gameplay-world-separated` passes Original/EX actual evaluation/display
and clean SDK shutdown without SDK errors. Inspected `ORIGINAL-world.png` and
`EX-world.png` contain no gameplay HUD; `ORIGINAL-on.png` restores the HUD over
the evaluated world. These are Corneria captures, not all-scene acceptance.

This currently duplicates composition and snapshots CPU backdrop data only in
the opt-in diagnostic path. Native HUD coverage can hide earlier native geometry
before the scene reaches composition; complete occlusion/disocclusion behavior
still needs broader auditing. World motion, jitter/camera history, SR modes,
capability-gated settings and performance/quality acceptance remain unfinished.
No release pushed; the earlier note that all HUD is still sent to DLSS is
superseded for this newly split diagnostic gameplay path.

## First actual gameplay evaluation and display

`STARFOX_TEST_DLSS_EVALUATE=1`, together with the lifecycle and temporal-input
switches, now runs diagnostic DLAA on actual PC gameplay textures. GpuComposite
color and resident guides flow through GPU guide conversion, the scoped native
SDL command bridge, the C SDK evaluator, and back into the resident effects/
presentation path. No gameplay color/depth/motion CPU readback is used. The host
checks a common model projection, supplies explicit perspective parameters,
resets on history loss/epoch/gaps, and releases the viewport before shutdown.
Renderer changes shut down this experimental session instead of reusing stale
device resources. Normal launches do not activate any of this.

Evidence: `tools/check_dlss_lifecycle.ps1 -Evaluate -OutputDirectory <new-dir>`.
`tmp/dlss-gameplay-sdk-checked` passed 16 real evaluated/displayed frames each in
Original and EX at 800x448, distinct off/on captures, clean shutdown, and no SDK
errors (SDK warning/error logging is now captured). Initial captures in
`tmp/dlss-gameplay-first/ORIGINAL-on.png` and `EX-on.png` were visually inspected.
Warnings about ignored unrequested plugins and the temporarily retained native
swapchain reference during restoration are not treated as SDK errors.

**Not release-ready:** only equal-resolution diagnostic DLAA is connected, not
the game's SR quality selectors. Game geometry is not yet jittered; non-model
world motion/depth and full camera-history matrices remain incomplete. HUD is
still in this diagnostic input and must be separated before shipping. Current
acceptance proves evaluation/display/lifetime, not reconstruction quality or
performance. No DLSS menu choice, NVIDIA-only Quest option, neural runtime
redistribution or release push was added. DLSS5-style gameplay is not proven.

## Native command and real presentation hooks

The SDL D3D12 bridge now scopes native compute work between SDL passes, passing
borrowed native textures/list with explicit read/UAV states and restoring SDL
resource states/descriptor heaps afterward. The caller must bind a new pipeline
before later drawing. Nine RGBA8/R32/RG32 native-command/round-trip checks pass,
including alias and output-index rejection. This is not yet a gameplay DLSS call.

The default-off lifecycle path now upgrades SDL's actual swapchains immediately
after creation, so real SDL presentation reaches Streamline bookkeeping. It
restores the original owned interface before swapchain destruction and before
SDK shutdown. An initial test uncovered a leaked native reference preventing
SDL swapchain recreation and causing cleanup failure: `slUpgradeInterface`
retains a native reference but does not consume the original caller reference.
Fixed that ownership in both the adapter and analytical probe.

`tmp/dlss-game-present-hooks-fixed` proves Original/EX startup, three successive
swapchain upgrade/restore cycles, gameplay, clean SDK shutdown and unchanged
off/on pixel captures. The prior `dlss-game-present-hooks` and GDB log are failed
diagnostics, not acceptance. `check_dlss_lifecycle.ps1` now requires actual
presentation upgrade/restore evidence as well as device binding/shutdown.

Remaining: invoke evaluation with complete gameplay color/temporal textures,
world motion and jitter, preserve HUD separately, expose gated settings, and
verify evaluated gameplay quality/performance. No release pushed.

## GPU temporal texture conversion

`GpuTemporalInputs` now converts resident camera-Z and float4 motion buffers to
the exact DLSS formats: projected R32 depth, RG32 pixel motion, and a 1x1 R32
exposure texture. It records a compute pass without submission/readback. Near/
far are explicit camera inputs. Unknown/nonfinite/out-of-frustum depth maps to
far depth; invalid, reset or mismatched-depth motion maps to the SDK's -FLT_MAX
sentinel, not valid zero motion. Reset accepts a missing motion buffer without
reading it. This does not synthesize missing background/ground correspondence.

The depth checker adds 256 conversion samples covering valid/invalid camera Z,
near/far bounds, nonfinite values, rejected inputs, reset without previous motion,
depth/motion ownership and exposure. D3D12 and Vulkan checks pass. DXIL/SPIR-V
compile and generated Metal bindings validate; no Apple execution claim.
Native SDL/D3D12 texture interop now checks all three required formats (RGBA8,
R32, RG32): nine byte-exact round trips at widths 37/128/259, with all previous
DXR mask/effects checks still passing. No CPU readback was added to production.

This converter is ready for the host's frame handoff, not yet called by gameplay.
Complete world inputs, jitter and evaluated-gameplay integration remain required.

## SDK lifecycle connected to PC (latest)

The native adapter now provides trusted initialization, actual-device LUID
capability checks/binding, viewport release and shutdown. Official Streamline
secondary signatures and NGX Authenticode are verified before secure absolute-
path loading. Initialization is single-instance, downloads are disabled, and
the bound device is retained through SDK shutdown. The probe uses this lifecycle
and completed another 128 evaluated frames in `tmp/dlss-native-lifecycle-evaluation`.

PC `DlssHost` uses that same C ABI behind `STARFOX_TEST_DLSS_LIFECYCLE`, requiring
explicit absolute `STARFOX_DLSS_ADAPTER` and `STARFOX_DLSS_BINARIES` paths. Default
launches load nothing. Initialization precedes SDL. The first actual-game smoke
test exposed a shutdown access violation when SDL destroyed its renderer first;
fixed by waiting for GPU idle and shutting down SDK before Window destruction.
`tools/check_dlss_lifecycle.ps1` now passes Original and EX startup, actual GPU
binding, 16 gameplay presentations, and clean shutdown. Captures with lifecycle
off/on are identical (`tmp/dlss-game-lifecycle-fixed`). Earlier failed smoke logs
are diagnostic only. This is lifecycle integration, not gameplay evaluation.

Remaining: resident guide textures, complete world motion/depth and jitter,
evaluation/presentation wiring, HUD separation, capability-gated menu settings,
and actual evaluated-gameplay quality/performance acceptance. No release pushed.

## Reusable native evaluator (latest)

`starfox_dlss_native.dll` now separates real DLSS configuration/evaluation from
the analytical fixture. `include/starfox/render/dlss_native.h` is a versioned
plain-C boundary usable by the MinGW game and MSVC SDK adapter: no SDK/STL types,
exceptions or ownership transfer cross it. The caller supplies actual D3D12
textures, states, matrices, jitter, frame identity and reset. The adapter checks
ABI size, finite camera inputs, resource formats/extents/device ownership,
aliasing and expected states before tagging/evaluating. Errors are bounded
strings and failure codes; malformed input is rejected before evaluation.

Evidence: `tmp/dlss-native-abi-evaluation` completed 128 real GPU-evaluated frames
across four modes and all 16 saved output images match the pre-refactor fixture
byte-for-byte. Aliased resource and ABI-size rejection checks run before each
mode. `tools/check_dlss_native_abi.cpp`, compiled with the game's MinGW compiler,
loads the MSVC DLL and passes cross-compiler frame-size, failure and bounded-error
checks. Configuration now also passes through the C interface; the follow-up
fixture is `tmp/dlss-native-config-evaluation`.

This adapter records evaluation only. Trusted SDK startup before DXGI, adapter
capability checks, device binding, queue synchronization, presentation hooks,
viewport release and SDK shutdown remain responsibilities of the integrating
host. The existing probe owns those today; the game does not yet call the DLL.
No NVIDIA binaries are copied into the game or release by this change.

## Native texture handoff verified (latest)

The pinned SDL D3D12 backend now exposes an optional versioned texture bridge.
It copies matching single-mip, single-sample 2D textures in either direction
between SDL and native D3D12 resources without CPU mapping. It validates device
ownership, dimensions, format and aliasing before recording commands, restores
native COMMON/SDL default states, and tracks SDL resource lifetimes. External
resources remain caller-owned through GPU completion; separate-queue evaluation
must use the existing fence bridges. Existing older buffer bridge ABIs remain
unchanged. This does not itself initialize or evaluate DLSS.

`starfox_sdl_d3d12_interop_check` passes exact RGBA round trips at widths 37,
128 and 259 (height 23), rejects mismatched/null inputs, and still passes all
12 existing animated/resized DXR buffer/effects comparisons. Diagnostic
readback is confined to the checker. The new source compiles with the current
MinGW PC toolchain; official NVIDIA SDK code remains in the separate MSVC probe.

Still required before release: native SDK lifecycle/evaluation integration,
complete temporal world inputs/jitter, HUD separation, capability-gated UI,
and evaluated gameplay acceptance. Do not label the texture bridge as finished
DLSS or a measured performance improvement.

## Gameplay history and final composition connected (latest)

`STARFOX_TEST_TEMPORAL_INPUTS=1` now connects validated presentation history to
the PC mono GPU scene. `ModelMotionHistory::prepare` rejects duplicate current
identities as well as previous duplicates. Only a successful GPU presentation
and successful SDL present commit the new poses. Scene/camera cuts, save-state
loads, renderer recreation, context/stereo changes and failed/native-fallback
presentations invalidate history. The production menu does not expose this
test switch as DLSS; no NVIDIA evaluation is yet called by gameplay.

The first live comparison caught a depth-only request incorrectly publishing
lighting metadata on native shadows. Fixed by separating geometry-plane
preparation from effects metadata publication. Enabling temporal inputs now
leaves captured game pixels byte-identical to the disabled path.

`GpuCompositeOutput` now carries resident camera depth and float4 motion through
native viewport offsets, source/destination scaling, clipping and foreground
coverage. Opaque black/same-color HUD writes, post-late CPU writes, late GPU
overlays and solid margins invalidate underlying guides. Mosaic remains unknown
rather than publishing a false pinhole correspondence. Data stays GPU-resident.

Evidence:
- `tmp/temporal-gameplay-240-fixed`: Original/EX 240Hz, initial reset and later
  resident depth/motion, identical enabled/disabled captures.
- `tmp/temporal-gameplay-state`: D3D12 240Hz Original/EX, saved frame 4, loaded
  frame 10, asserted no previous pose on presentation 11, identical captures.
- `tmp/temporal-gameplay-60-vulkan`: Vulkan 60Hz Original/EX, same input/pixel checks.
- D3D12 and Vulkan model-depth checker: previous plane/motion tests plus final
  composite offset, differing render scales and explicit CPU coverage checks.
- Full existing D3D12 compositor checker passes, including 108 base fixtures and
  its overlay/effect coverage suites. Stereo/history unit test passes.

Remaining: complete world/non-model temporal inputs, jittered game rendering,
native NVIDIA texture and evaluation handoff, HUD exclusion from the neural
pass, capability-gated settings, and actual evaluated gameplay acceptance.
The new test switch is input-generation evidence, not a finished DLSS option.

## Actual DLSS and neural evaluation verified (evening update)

The optional second argument of `starfox_streamline_probe` now runs an actual
D3D12 evaluation fixture, not just capability queries. It supplies analytical
moving foreground/background color, perspective depth, previous-minus-current
motion, subpixel jitter, explicit exposure, and two history resets per mode.
Quality, Balanced, Performance and DLAA each completed 32 frames on the RTX
5070 Ti Laptop: 128 evaluated, fence-completed frames with changing output and
foreground-area checks. Output is 1280x720; input dimensions were respectively
853x480, 742x418, 640x360 and 1280x720.

The initial evaluation exposed a missing frame-tagging preference and then a
missing manual-integration presentation hook. Both were fixed. The clean run
upgrades its own hidden swapchain, presents once per frame, and has no SDK
error messages. Proof: `tmp/dlss-evaluation-present/evaluation.txt`, PPM images
and `tmp/dlss-evaluation-present.log`. The earlier `first` and `second` runs are
diagnostics, not clean acceptance results.

The same evaluator was copied into `tmp/renodx-dlss-evaluation`, with the
previously authorized author add-on and signed NR runtime. ReShade.log now
explicitly reports **inline feature 18 evaluation succeeded**, including count
60, rather than merely reporting a loaded DLL. Its first evaluation was skipped
because host-state tracking was incomplete; subsequent evaluations succeeded.
GPU-read-back output differs from plain DLSS, and the final Quality image was
visually inspected. This verifies the neural path on the analytical fixture;
it is NOT evidence of neural gameplay, visual quality across levels, or FPS.
No neural binaries were added to the normal game or release package.

Reproduce plain DLSS (new output directory required):

```powershell
build/streamline-probe-msvc/starfox_streamline_probe.exe tmp/streamline-sdk-2.14.1/sdk tmp/dlss-evaluation-new
```

## Per-pixel model motion integration

`GpuProjection::enqueue_motion_surface` now reconstructs a visible camera-space
point from planar depth and reprojects it through rigid-object previous pose.
It returns pixel-space XY, camera Z and explicit validity. It handles projection
changes and current jitter; reset, unknown/nonfinite depth and previous-near
clipping invalidate motion instead of fabricating valid zero vectors. All 728
analytical samples pass on D3D12 and Vulkan.

`GpuModel::enqueue` accepts an optional preceding pose and chains this pass
directly after its resident raster depth. Packed current/previous transforms
produce the mapping without CPU per-vertex projection. Changed coordinates,
singular/mismatched byte/word transforms and native word-wrapped paths reject
correspondence. Destruction, folded faces and screen-space deformation retain
unknown motion. `GpuScene` carries motion according to visible color ownership;
opaque HUD writes invalidate it, even when black. Motion-bearing draws bypass
the fused raster merge so no prior object's motion is silently discarded.
Stereo copies transform both current and previous poses into the same eye.

Model translation and opaque HUD ownership tests pass at 1x/2x/4x on D3D12 and
Vulkan alongside the existing 366,183 planar-depth samples. Normal gameplay
still does not request temporal data: history/scene resets, non-model world
inputs, jittered rendering and native texture/evaluation handoff remain to be
connected before a working DLSS option can ship. Earlier sections below record
the progression and should not be mistaken for the latest evaluation status.

Final checks for this pass: Windows executable rebuilt; six targeted runtime,
stereo, core and raster checks passed (27.55 seconds). Linux software Vulkan
also passed both the model-depth/motion and full projection checkers. DXIL,
SPIR-V and generated Metal source pass freshness/binding checks; Apple GPU
execution and Android rebuild are not included in this pass. No release was
published and no new PC/Quest handoff archive was made.

## Planar model depth integration

`GpuModel::enqueue(..., geometry_depth=true)` now produces a separate float
camera-Z buffer. GPU surface preparation computes planes from actual transformed
vertices; source face IDs survive BSP/span ordering in the existing command ABI.
Rasterization evaluates the plane at each covered pixel centre, after texture
transparency, using the model's projection. Native rounded vanishing points and
fractional/high-resolution projection are distinguished. Existing mean-depth
effects metadata is unchanged. Unknown depth is zero; folded/nonplanar faces,
lines/sprites, colour-warp, wave and wobble paths do not fabricate valid depth.

`GpuModelDraw::geometry_depth` carries this opt-in through mixed scenes and
stereo draw copies. Fused raster composition and separate scene merge preserve
depth according to visible colour ownership (not effects surface ownership).
An opaque black/HUD overlay invalidates its pixels' depth; untouched pixels
retain the background depth. No per-frame readback is used in production.

`starfox_gpu_depth_check` passes on D3D12 and Vulkan: 366,183 analytical sloped
plane samples at native/fractional 1x/2x/4x, including fractional vanishing
points; exact unchanged colour/effects output; folded-face invalidation; mixed
scene painter ownership; byte-identical fused/separate depth composition.
Additional existing checks pass: 192 Original real-model mixed-batch images
on D3D12, 12 EX Arwing and 12 EX BOXXIE images on Vulkan, and mixed raster
binning comparisons on both backends. An initial EX first-16-header sample
failed the nonempty-fixture check and is not counted as passing evidence.

This is not a complete DLSS input set: per-pixel motion, ground/background and
other nonplanar geometry depth, jitter/history integration, GPU texture handoff
and actual evaluation remain. The normal game's draw records do not yet opt
into this buffer. DXIL/SPIR-V compile and execute; Metal source is generated
and binding-validated, not Apple-compiled or hardware-tested.

After the final shader change, all Windows targets rebuilt successfully and
both backend depth checks passed again. Eight targeted CTest checks passed
(11.49 s): packed faces/projection, raster commands, core, stereo output,
Original/EX runtime smoke, and isolated effects. This is not a fresh full
55-test run or headset acceptance.

```powershell
cmake --build build/current --target starfox_gpu_depth_check
$env:SDL_GPU_DRIVER='direct3d12' # repeat with vulkan
build/current/starfox_gpu_depth_check.exe
```

## Authorized isolated add-on test

User explicitly approved loading the unsigned author V4.7 add-on in isolation.
`tmp/renodx-isolated-d3d12` contains a copy of the Windows executable/assets,
the author add-on/NR DLL, official SDK SR DLL, and ReShade64.dll extracted from
the official ReShade 6.8.0 Addon installer and named dxgi.dll. No installer was
run and the normal game directory was not modified. No Vulkan/global layer was
installed. Both the setup-menu run and a 180-frame Original Corneria run exited
0 on D3D12. ReShade.log confirms V4.7 registration and the signed NR runtime's
reference hash match, but supplies no evidence of neural frame evaluation.
Native capture `corneria-capture.bmp` was inspected; it occurs before ReShade
and is NOT photographic proof of neural output. The game still has no native
DLSS evaluation path, so loading successfully does not make realism functional.

## Verified capability probe

User approved the official SDK license. The isolated Windows/MSVC target in
`tools/streamline_probe` now initializes the approved SDK and checks actual DXGI
adapter LUIDs. NVIDIA's secondary signatures are validated for Streamline DLLs;
NGX's different signing scheme is checked through standard Authenticode. DLL
search is restricted, OTA/downloaded plugins are disabled, and no game files
are replaced. This is not renderer integration or a working menu option.

Built and executed against official v2.14.1: `slInit` and DLSS requirements
return `eOk`; the primary RTX 5070 Ti Laptop adapter returns `eOk`. Intel and
the other exposed adapters return `eErrorAdapterNotSupported`, including a
second adapter bearing the NVIDIA name. This demonstrates why capability must
be checked per adapter, not inferred from its name. No frame was evaluated.

Device follow-up: the probe now creates a D3D12 device on the supported adapter
and resolves DLSS's optimal-settings API after `slSetD3DDevice`. A locally
generated custom-engine project GUID was required; without it NGX unloaded the
feature despite the earlier support result. With that identity the device and
all four queries pass: 1920x1080 output requests 1280x720 Quality, 1114x626
Balanced, 960x540 Performance, and 1920x1080 DLAA. Device lifetime extends past
Streamline shutdown. This is still not a rendered/evaluated DLSS frame.

Reproduce from a VS x64 developer shell (paths can be absolute):

```powershell
cmake -S tools/streamline_probe -B build/streamline-probe-msvc -G Ninja -DCMAKE_CXX_COMPILER=cl -DSTREAMLINE_SDK="<extracted official SDK>"
cmake --build build/streamline-probe-msvc
build/streamline-probe-msvc/starfox_streamline_probe.exe "<extracted official SDK>"
```

After the user explicitly requested joining, the RenoDX wiki's
`https://discord.com/invite/renodx` invite unlocked the server. Download channel:
https://discord.com/channels/1408098019194310818/1545049227321810974

Downloaded the channel's forwarded V4.7 `renodx-dlss5.addon64` and companion
`DLSS310.8.0-Streamline2.13.zip` into `tmp/renodx-author-downloads`; extracted only
`nvngx_dlssnr.dll`. Subsequently loaded only in the authorized isolated test above.
The add-on is unsigned; the neural DLL has valid NVIDIA Authenticode. This
establishes the download source and DLL signature, not add-on safety, bridge
compatibility, or redistribution rights. The archive contains DLLs but no
license files. The channel also offers a separate ShortFuse `renodx-dlss.addon64`
variant; its posted instructions say the two add-ons cannot be used together.

SHA-256 evidence:
- V4.7 add-on: `D5ADF82EB44B065F4C590AC91FE824BAB07AFEA0EB9F994BDE936710C8593952`
- Companion ZIP: `3FDB7CB25250259332550F419DBAC516A2EBA778D8592DA75CB2FE8ABFC781D8`
- Neural DLL: `E16BCF15E16E13F527491CDF7845B2FE6521A738D8F7C9C721866A8496E1FC8E`

## User-provided experimental bridge

https://github.com/NIGos/dlss5-bridge provides a possible unofficial ReShade
route; absence from Streamline's public API does not rule this out. Inspected
README and MIT license. The bridge itself does not perform neural rendering:
it requires a separate compatible neural add-on and nvngx_dlssnr.dll (README
points to RenoDX's Discord distribution), plus ReShade and the SR runtime.
Those neural components have now been obtained as recorded above; the bridge's
MIT license does not establish redistribution rights for them.

Native DLSS inputs are preferred. The optical-flow substitute requires usable
depth and approximate motion and documents soft text/smearing. Vulkan mirror
also documents synchronization stalls; stereo/multiple-view compatibility is
not established for this game. Any eventual support must be experimental,
default off, and preserve HUD outside the neural pass. No bridge or third-party
neural DLL has been added to the normal game installation. The authorized
isolated test is documented above. The official Streamline v2.14.1
SDK is extracted in tmp and used by the isolated capability probe above,
not bundled or integrated into the game.

Requested: DLSS Super Resolution and an AI-realism option. Neither is
implemented or exposed as a working menu choice yet. Both must default off
and be capability-gated; Quest must not display NVIDIA-only features.

Verified local adapter: NVIDIA GeForce RTX 5070 Ti Laptop GPU, driver 595.79.
Public NVIDIA-RTX/Streamline release: v2.14.1. Its public include directory
exposes DLSS SR, Ray Reconstruction and Frame Generation; no DLSS 5 neural
rendering header/API was found. NVIDIA's September 1 DLSS 5 research page
describes RTX 50-series appearance generation, but is not an integration SDK.
Do not substitute Ray Reconstruction or a color-grading preset and label it
DLSS 5. That portion needs an available official integration interface.

Super Resolution integration prerequisites (official ProgrammingGuideDLSS):
render-resolution color, depth, motion vectors, output-resolution color,
camera matrices/jitter, history-reset handling and feature capability checks.
GpuSceneDraw now carries optional object identity (slot, pool generation,
shape, strategy and type), populated by the PC object's actual draw path and
preserved across stereo eye copies. Unidentified helper/shadow draws remain
explicitly unidentified. The stereo regression passes, including recycled-slot
and changed-shape inequality. Previous presentation poses, scene/reset epochs,
and motion-vector surfaces are not connected to the renderer yet.

`ModelMotionHistory` now implements the previous-pose store for successful
presentation submissions, with explicit serial/epoch/dimension checks. Tests
cover recycled entities, topology animation/explosion changes, skipped frames,
duplicate identities, disappearance, projection changes and reset. It is not
enabled in normal rendering: the future caller must supply scene/load-state
epochs, reject ambiguous current identities, and commit only submitted frames.
This is CPU pose metadata, not CPU-generated motion-vector pixels.

GPU vertex motion is now implemented by `GpuProjection::enqueue_motion` and
`motion_portable.hlsl`: paired unjittered projected vertices produce
previous-minus-current render-pixel XY, linear camera Z, and validity. The
pass records into a caller-owned command with no submission/readback. GPU
checks pass on D3D12 and Vulkan for movement direction/scale, stationary valid
vertices, history reset, behind-camera points, NaN, overflow, and alias rejection.
DXIL/SPIR-V compile and generated Metal source/bindings validate; Metal execution has
not been tested. This is not yet wired into the normal model draw path, and
per-pixel interpolation, clipping correspondence, surface composition and DLSS
evaluation are still required. Do not expose a working DLSS menu choice yet.

Portability follow-up corrected the motion pipeline's Metal entry point to
`main0`, matching SPIRV-Cross output, and capped dispatches to the portable
65535-workgroup limit. Generated MSL is not evidence of compilation by Apple's
Metal compiler or of execution on a Mac.
Motion-vector generation and disocclusion/history correctness therefore need
real renderer work before a quality selector is useful. Do not fake zero
motion vectors or upscale the HUD as though it were world geometry.

Sources:
- https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.14.1
- https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.1/docs/ProgrammingGuideDLSS.md
- https://research.nvidia.com/labs/adlr/DLSS5/

## September 24 default-path performance correction

The above historical scaffold status predates the current opt-in gameplay
integration. In the current worktree, DLSS OFF no longer upgrades SDL's D3D12
swapchain merely because the optional SDK is installed. It also retains the
ordinary Vulkan GPU backend on Windows instead of forcing D3D12. Turning DLSS
ON selects D3D12 and recreates the renderer once; turning it OFF restores the
native presentation path. Quality changes while ON do not rebuild the device.
The optional RenoDX/ReShade proxy is not included in the release workflow.

An installed-runtime lifecycle check on the local NVIDIA adapter confirms
that OFF initializes the adapter but makes no `dlss-presentation: upgraded`
call, while ON upgrades, evaluates eight frames and restores. This removes a
default-path cost but is not a GTX 750 Ti performance measurement; the reported
weak-system slowdown still needs hardware validation.

The 180-frame, 1×/unenhanced, hidden/unpaced Original 1-1 check on this
NVIDIA laptop measured 3.578 ms frame-work p99 on the restored default
backend, versus 3.216 ms with D3D12 explicitly forced. These are local
renderer-work measurements, not on-screen FPS or evidence for GTX 750 Ti;
the native-default behavior is about preserving the pre-DLSS backend rather
than claiming a universal speedup.

A 24-frame diagnostic toggled OFF→DLAA→OFF without restarting the process:
eight DLAA frames evaluated, the wrapped swapchain was restored, and the
final renderer returned to the native non-D3D12 backend. The script is
`tools/check_dlss_toggle.ps1`.
