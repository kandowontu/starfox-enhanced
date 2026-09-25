# Experimental manipulations — September 20

Independent controls update: 3D MANIPULATION and MANIPULATION INTENSITY now
stack after the model style, with automatic migration of old selected
manipulations. Added Barrel Warp, Venetian, Checker Fold, Twist, Ring Ripple
and Shard Split. A separate
3D MATERIAL row contains Metal, Gold, Copper, Mirror, Prism, Glass, Obsidian,
Pearl, Ruby, Jade and Porcelain. Materials are applied before styles; spatial manipulations follow
styles, and temporal trails retain the final styled result. New decorative
materials are stylized palette-light treatments, not physical transmission
or refractive ray tracing. Existing reflective materials still use the
independent Reflective Surfaces feature. All 3D materials are gated on the GPU
renderer with available ray tracing and enabled Reflective Surfaces; selections
are retained and labelled inactive otherwise. Long 3D menus scroll rather than
running beyond the backdrop. Saved preferences and save states retain all
three choices independently. Hidden rows have up/down triangle indicators.
Widescreen comms portraits are explicitly HUD-tagged in both immediate and
recorded rendering; model effects, materials and manipulations skip them.
VR deployment remains on hold.

Validation: Windows and native Linux builds pass, Windows pixel/settings tests
pass, and Original/EX save-state and simulation tests pass. Direct3D12, Vulkan
and Linux Lavapipe effects matrices agree with the CPU reference, including
Pearl + Cel + each spatial manipulation. Seven actual Original Corneria
software/GPU combined-effect captures are byte-identical. Four new material
captures were inspected with hardware RT and reflections enabled. Physical
non-Windows GPU performance has not been tested.

Final additional batch (Twist/Ring Ripple/Shard Split and Ruby/Jade/Porcelain):
Windows/Linux rebuilds and pixel-filter checks pass; Windows settings tests
and D3D12/Linux Vulkan effect matrices pass with all 64 stable IDs. Actual
60-frame comms captures with effects off versus Ruby + Cel + Twist contain
identical pixels throughout the 74x80 portrait rectangle (5,920 pixels).
Proof: `tmp/portrait-effects-proof/{off,on}/presentation.bmp`. Immediate and
recorded portrait-tag assertions pass in the Original simulation test.

September 24 addition: Ten more model/world effects (Duotone, Tritone,
Woodcut, X-Ray, Pop Art, Iridescent, CRT Phosphor, Noir, UV Glow,
Topographic) and ten 3D materials (Silver, Brass, Rose Gold, Titanium,
Amethyst, Sapphire, Opal, Marble, Graphite, Molten Glass) append IDs 64–83
without changing existing saves. The metallic additions select the matching
ray-reflection conductor and each new material has its own roughness; the
existing ray-tracing/reflective-surfaces gate still applies. CPU and portable
GPU shading implement the same looks. Pixel/selector tests and the full
Direct3D12 CPU/GPU effect matrix pass, including every new ID.
`tmp/effect-controls-proof/menu.bmp` shows the independent controls, inactive
material indication and both scroll arrows within the menu backdrop.

Second spatial batch: appended stable IDs 48 SHATTER, 49 MELT and 50 RIPPLE
WARP to the model/world Manipulations group. Shatter rotates 16-source-pixel
fragments, Melt pulls columns downward with deterministic varying lengths, and
Ripple Warp uses smooth integer wave offsets. None uses random frame seeds,
rapid flashing or temporal history. Existing saved IDs retain their meaning.
CPU and shared GPU shader implementations reject HUD/other-layer samples and
preserve destination opacity. OFF still avoids the effect pass. Menu labels
are translated; DXIL/SPIR-V/Metal payloads regenerated and freshness checked.

Windows application and focused test targets rebuilt. Pixel-filter tests pass;
the all-style CPU/GPU matrix passes on D3D12 and Vulkan at 0/37/100 intensity,
including the new modes. `tools/capture_manipulations.ps1` produces actual
Original/EX Corneria final-target captures with enhancements otherwise disabled.
Saved model/world intensity was verified as 100 for these runs. All eight
software/GPU image pairs (OFF plus each new style, both experiences) are
byte-identical, and each enabled style differs from OFF. All six GPU effect
images and both baselines were visually inspected; HUD bars remain readable.
Proof: `tmp/manipulations-spatial-{original,ex}-sep20`.
These are specific Windows 1x/16:9 samples, not all-stage/device acceptance.
Native Linux pixel-filter and Vulkan effects checkers also rebuild and pass
using Lavapipe. This validates the portable implementation, not physical Linux
GPU performance. No VR deployment or release was performed.

User requested a broad, opt-in collection of unusual 2D/3D effects, including
persistent frames/sprite trails, and a separate Manipulations category.

First implementation batch appends stable style IDs 43–45: Kaleidoscope,
Prism Split and Pixel Sort. The shared model/world selector groups these under
MANIPULATIONS. CPU and GPU paths preserve HUD pixels and reject source samples
from a different layer category. Pixel Sort uses bounded eight-sample stable
insertion sorting with no per-pixel allocation. Existing IDs remain unchanged.
Menu translations and portable shaders regenerated. Pixel-filter tests pass,
including patterned manipulation output, HUD/opacity preservation and zero
intensity. D3D12 and Vulkan effects checks pass, including exact CPU/GPU style
comparisons at 0/37/100 intensity. The one-colour legacy fixtures correctly
expect spatial transformations to leave a constant colour unchanged; patterned
fixtures separately require visible transformation. Gameplay captures and
normal executable relink were subsequently completed (see integration below).

Temporal implementation in progress: FramePersistence supplies a software
reference for fading trails (350 ms half-life) and long exposure (hold until
reset). Its tests cover 60/120/240 Hz decay, HUD exclusion, scene epochs, clock
rewind and freeing history while off. The portable GPU path now implements the
same modes with two resident floating-point history textures and no history
readback in normal presentation. D3D12 and Vulkan temporal comparisons pass
at 60/120/240 Hz for both modes, including moving model trails, HUD exclusion
and a mid-sequence scene-epoch reset (maximum permitted difference one byte).
The focused software suite also passes same-area resize, long-exposure hold,
clock rewind and disabled-memory release checks.

Integration completed: stable model style IDs 46/47 select TRAILS and LONG
EXPOSURE in the MANIPULATIONS group, with saved settings and translations.
These two history modes are model-only; spatial modes remain model/world.
Host presentation time is independent of DLSS and FPS. Scene revision and flow
context reset history. Resident GPU output has separate mono/left/right history;
interleaved-eye isolation tests pass on D3D12 and Vulkan. Portable multi-pass
output now also runs history on the GPU. Intermediate passes explicitly preserve
history without advancing it; history owns its dimensions independently from
temporary overlay images. The D3D11-only legacy backend and Software retain the
software reference, because that backend has no temporal compute stage.
Disabled CPU/GPU history is released. Model effect intensity controls the blend.
The normal Windows executable rebuilt successfully. Actual game captures:
`tmp/manipulations-trails-gpu/presentation.bmp` and
`tmp/manipulations-exposure-software/presentation.bmp` (60 frames each).
Both show accumulated models with readable, unaffected HUD. These unpaced
captures demonstrate integration, not real-time fade timing (covered by tests).
Time echoes/slit-scan and additional transformations remain ideas, not completed
features. No rapid full-screen flashing is implemented; strobe is interpreted
as frame persistence. Regular Android arm64 debug APK rebuilt successfully
(38 seconds), including host integration and portable history textures.
Native Linux application rebuild also succeeds; its frame-persistence and
pixel-filter tests pass. Android physical runtime and Linux presentation-device
acceptance of this batch remain pending.

VR deployment stays on hold. No release has been requested for this batch.

September 20 multi-pass follow-up: D3D12 and Vulkan comparisons now insert a
different-resolution overlay pass before every temporal frame. Both modes still
match the CPU reference at 60/120/240 Hz, with scene resets and eye isolation.
Actual 60-frame gameplay capture with native GPU geometry deliberately disabled
(`tmp/manipulations-legacy-gpu-sep20/presentation.bmp`) shows model trails and
clean HUD; its log confirms 60 successful GPU multi-pass history submissions.
CPU history is no longer applied a second time after successful GPU effects.
This closes the deliberate portable-effects CPU detour, not all migration gaps.

Native Linux desktop and effects checker rebuilt after the multi-pass change.
Vulkan/Lavapipe tests also pass the resized-overlay temporal sequence, eye
isolation, all styles, bloom combinations, shadows, batching, deferred capture
and filter scales 1–6. Lavapipe is software-adapter correctness evidence, not
physical Linux GPU/Steam Deck performance acceptance.

September 20 live-options invalidation: changing renderer, AA, smoothing,
filter, style/intensity, bloom, lighting, HDR/chromatic, shadows or reflection
settings now advances the shared history epoch and clears software history.
This prevents Long Exposure's bright old configuration from masking a newly
selected darker setting, and avoids old software trails surviving a quick
GPU/software switch. Stable settings do not reset each frame.
`check_startup_backends.ps1 -TemporalEffect 47` passes all six Original/EX
Software/D3D12/Vulkan preview launches. Each GPU run confirms exactly five
option resets (initial setup plus four live renderer switches), with nonblack
final output. Logs/captures: `tmp/persistence-renderer-cycle-sep20`.
Epoch-reset pixel correctness remains covered by CPU/GPU temporal tests;
this host check specifically proves that renderer transitions emit the reset.
