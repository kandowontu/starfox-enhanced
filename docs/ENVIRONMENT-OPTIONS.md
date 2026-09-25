# Landscape enhancements

## Corneria green-hills correction — September 24

Enhanced Sky for Original/EX Corneria (`BG_1_1C`) and Original Training now
selects the existing cloud-plains photograph (resource 208), with low green
hills rather than the snow-covered alpine panorama (resource 200). EX's snowy
menu previews and snow stages retain their separate alpine assignment. The
shared mapping feeds desktop Software/GPU and PCVR/Quest; no new bitmap or
resource ID was added. Original LEVEL1_1 at tick 1000 was captured at 16:9
with GPU and Software, straight and while steering right; EX LEVEL1_1 was
also captured with GPU. The images at
`tmp/corneria-green-hills-2026-09-24` were inspected for the hill horizon and
banked sky/ground join; the terrain mapping test passes. These are desktop
visual checks, not a headset or all-route sign-off.

The 2D Options page has independent Enhanced Ground and Enhanced Sky toggles,
both defaulting to Off. These stack with world effects and retain original
rendering when disabled.

- Ground Material: Auto, Grass, Dirt, Sand, Snow, Water, Mirror, Gold Metal.
- Ground Motion: Off, Wind, Ripples, Pulse.
- Sky Style: Clouds, Cirrus, Aurora, Sunset.
- Sky Motion: Off, Drift, Swirl.

Registered landscape atlases are classified by their predominantly flat source
rows, not by applying a colour test to the completed frame. Entries shared by
sky and ground remain protected. Source palette colours select Auto materials;
Papetoon explicitly selects sand. Original Titania's separate Mode-1 water
profile is restricted to blue background ink below its horizon. Unsupported
backgrounds, tunnels, models, HUD, portraits and menu lettering are untouched.

Enhanced Ground now supplies triangle geometry for grass blades and sand/snow
relief in both rendering paths. Sand/snow use a continuous, periodic multiscale
height field with broad flat regions, not a repeated mound per tile. Dirt uses
low relief and stochastic surface detail. Grass retains 75 nearby or 32
middle-distance two-sided blades per patch; floor edges remain identical across
foliage detail levels. Coverage extends to 3072 source units. This is visual
geometry, not a change to collisions or authored level objects.

Corneria/Original Training and Macbeth have replacement photographic-style
backdrop assets; other backgrounds do not yet have full realistic replacements.
Water has animated waves/highlights. Reflective Surfaces enables approximate
screen-space reflections (excluding HUD) for Water, Mirror and Gold Metal on
the non-ray-traced path; Off disables those reflections.
Selecting a material manually intentionally changes a landscape's appearance.

With hardware Ray Tracing enabled, these three materials use the source ground
height as an analytic ray receiver. Water has world-anchored animated normals,
sun highlights and ray-traced occlusion. Reflective Surfaces separately controls
actual scene/environment reflection rays: water uses angle-dependent Fresnel,
Mirror is sharp, and Gold Metal uses gold conductor reflectance. These are
single-bounce reflections, not full path tracing or displaced wave geometry.
Foreground models occlude the receiver. Compositing requires the classified
ground palette and background ownership, protecting HUD, portraits and sky.
Unsupported ray backends retain the explicitly approximate fallback.

CPU and portable GPU implementations share the equations; generated DXIL,
SPIR-V and Metal shader payloads are included. CPU water needs a temporary
source-image copy; GPU water samples the existing input texture. Disabled
options perform no classification, noise evaluation or reflection copy.

Proof captures: `tmp/environment-proof/auto/presentation.bmp` and
`tmp/environment-proof/water/presentation.bmp`. Targeted tests cover all 90
ground/sky combinations, layer protection, default-Off identity and settings
round-trip (nine ground modes including Off, five sky modes including Off,
reflection Off/On). DXR receiver tests cover offscreen scene hits, deterministic
waves, reflection Off, Mirror/Gold and legacy model isolation. Synthetic GPU
composition tests verify ground-only ownership and rejection of model-ray
markers. Live Windows captures in `tmp/environment-proof/ray-{water,mirror,gold}`
show source-driven Corneria with scene reflections; `ray-water-off` covers the
independent reflection toggle. The 2D menu and EX Papetoon were also captured
and visually inspected. Windows DXR, D3D12 effects/composition and Linux Vulkan
effects pass; the three settings/pixel/state unit suites and both Original/EX
real-cartridge state suites pass. Original/EX simulation checks passed earlier
in this batch with X+Y entry, A/Start launch and B rejection. No VR
deployment or Steam/headset validation is implied.

# Planet Select cheat and map fade

Cheats > Planet Select Cheat enables the source map selector. User-requested
bindings override the original source entry/confirmation differences:

- Both cartridges: hold X+Y to enter, then A or Start to launch. B does nothing.
- Left/Right: previous/next stage, bounded to the source route.
- Up/Select: previous route; Down: next route.
- EX L/R retain switching between the two shipped campaign maps.

Route lists, planet artwork and ship positioning use the cartridge data and
native SETSHIPPOS / SETSHIPPOS2 routines. Cheat selection and activation survive
save states; older settings/states default to Off.

The selection fade now uses actual BG1 planet coverage inside the source window.
Transparent corners fade with the map instead of leaving a rectangular patch.
The Original before/after fixture changes only 772 pixels within the selected
60x60 stored-pixel region. Proof: `tmp/planet-fade-{before,after}/presentation.bmp`;
EX: `tmp/planet-fade-ex-after/presentation.bmp`.

# SBS depth assessment

The GPU SBS path already renders separate parallel eye cameras for models,
particles, dust and the grid, with per-eye shadows/reflections. Existing native
stereo tests measure the expected positive/zero/negative disparity at three
depths. Current eye separation is fixed at 6.4 source units and convergence at
512, so the result can be subtle. Source tilemap backgrounds/HUD remain flat.

Adjustable depth/convergence is a small-to-medium feature built on that path,
not a replacement renderer. Giving the flat scenery real layered depth is a
larger scene-by-scene project (especially tunnels and unique planet art).
Software fallback depth needs a separate two-view rendering path. Neither
additional feature is claimed implemented by this checkpoint.
# In-progress terrain follow-up (2026-09-20)

Environment motion now interpolates an unwrapped source-frame clock instead
of sampling the 16-bit cartridge word directly each presentation. It preserves
the existing effect speed, freezes with held source state, cuts across scene
resets and persists through desktop runtime saves. Unit wrap/pause/restore
checks and 30-frame Original Drift/EX Swirl fresh-process GPU continuation
comparisons pass, with matching audio signatures. The earlier 61-test full
suite predates this clock change; these are targeted follow-up checks.

Macbeth now has a dedicated generated realistic rust-mountain/red-sky image,
embedded as resource 201 and selected for BG_3_5 rather than recoloring the
Corneria panorama. Windows and Linux build; image decoding checks pass. The
24-frame Original LEVEL3_5 GPU/software captures match exactly (SHA-256
`50C32144556686EF932EE18FA15EA7D5FDA7AFA3C32EE539DC022915580F9C53`), and all
pixels at/below stored row 256 remain identical to Enhanced Sky Off. The GPU
capture was inspected. See `tmp/backdrop-proof/macbeth-{gpu,software,off}` and
the asset provenance/prompt in `assets/enhanced-backdrops/macbeth-dusk-v1.md`.
This adds one actual stage backdrop, not full planet/moon/all-stage acceptance.
EX LEVEL3_5 also renders the new backdrop in the inspected
`tmp/backdrop-proof/macbeth-ex/presentation.bmp`; its separate source atlas
origin places the replacement on the same intended horizon. This is one
EX fixture, not full-route or all-scroll validation.

Surface follow-up: a sand capture exposed repetitive high-contrast sinusoidal shading despite the irregular mesh. Sand now uses low-contrast, noise-warped ripples restricted to irregular patches, plus granular detail; grass/dirt/snow use stochastic grain instead of a crossed sine pattern. Updated Windows/Linux applications built, D3D12/Vulkan effects parity and Windows terrain/DXR checks passed. `tmp/terrain-geometry/sand-irregular/presentation.bmp` is the revised capture.

Latest combined checkpoint: `build/current` is updated. Sand/snow use deterministic multiscale relief with flat regions; grass retains blades farther into the scene, and dirt has surface detail. Backdrop sampling now uses an orthonormal roll rather than a vertical shear. Replacement-image styles and cloud-region motion share software/GPU/reflection implementations, with mountains excluded from motion. Windows terrain, image, effects and DXR checks pass. Inspected combined tilted grass/alpine proof: `tmp/backdrop-proof/terrain-final/presentation.bmp`. This is targeted validation, not all-stage or sustained-performance acceptance. Earlier checkpoints below are historical; their blocked-link and missing-alpine statements are superseded.

Portable follow-up: Linux application with resource 200 embedded built successfully; real backdrop decode CTest and Vulkan GPU/software backdrop/environment comparisons passed. No Android/VR deployment or release was performed.

Initial real backdrop integration: Corneria/Original Training now use the embedded alpine-day image when Enhanced Sky is on, including the software sampler, SDL GPU sampler and DXR environment rays. Hardware tests cover scroll/roll/fades/layer isolation and reflected image replacement. The first tilted capture exposed a horizon boundary 16 source pixels too high; changing the registered ground boundary from origin+112 to origin+128 removes the old mountain fragments in the inspected aligned capture (`tmp/backdrop-proof/alpine-aligned/presentation.bmp`). Windows build passes. Other stage backdrops/moons/planets, style/motion variants, full-wrap inspection and physical VR acceptance remain unfinished. This is not acceptance of all-level Enhanced Sky.

Surface-shading revision: enhanced meshes have a dedicated terrain pixel tag (5), treated as scenery rather than models or 2D art. Procedural ground detail now reaches these pixels without touching stars/dust; snow keeps highlight headroom, and material palette banks are chosen against a complete four-shade ramp. CPU/GPU shadow receivers include the new tag. Windows and Linux applications built; terrain tests and the extended D3D12/Vulkan environment/effects comparisons passed. Inspected dirt-v4 and tilted snow-v4 captures in tmp/terrain-geometry confirm surface detail instead of flat fills. The bright far-horizon transition still needs blending, and realistic sky replacement/blue sky seam are not fixed by this revision.

The expanded terrain initially exhausted the capture's 60-second budget with excessive memory use. Static terrain now avoids per-tile world-sprite composition and unneeded surface metadata; fused raster outputs and immediately consumed bin scratch reuse ordered buffers rather than cycling allocations for every tile. The revised Windows build linked successfully, a 12-frame grass capture completed (542 terrain patches), and starfox_gpu_composite_check passed. Proof: tmp/terrain-geometry/grass-v3/presentation.bmp. Sustained performance, temporal reconstruction and non-Windows checks remain pending.

Terrain revision after user feedback: replaced per-tile center mounds with a periodic continuous world-height field, broad variations and flat regions for sand/snow. Dirt is now low relief; terrain shading contrast is reduced. Nearby grass uses 75 two-sided blades, medium-distance grass retains 32, and terrain coverage extends to 3072 source units (previously 1536). Foliage LOD keeps identical floor edge vertices. Geometry tests cover deterministic seams/wrapping, both raised and flat regions, grass counts and layer ownership. Visual quality and the increased geometry budget remain to be checked in-game.

Ray-reflection follow-up: background reflection samples now call the shared environment material shader using the current classifications, material selection, motion and roll. The DXR fixture confirms reflected green terrain changes to Water/Gold and restores exactly when disabled, including changing modes on the same source object. The full Windows DXR check passed. This fixes enhanced-environment sampling only; arbitrary screen-space world filters are not reproduced by this change. The application translation unit compiled, but linking this newer reflection build was blocked again by a running executable; the previous terrain build remains installed.

User clarification: Enhanced Sky must replace backdrop elements, not tint just upper sky palette entries. Scope includes realistic mountains, clouds, moons and planets while retaining authored stage composition and camera alignment; ground remains separately controlled. The current procedural sky overlay is not acceptance of this requirement.

After the game was closed, Windows linking succeeded and build/current/starfox_pc.exe was replaced. The inspected capture tmp/terrain-geometry/grass/presentation.bmp confirms visible individual grass geometry in gameplay. It does not validate tilted coverage, snow or ray-reflection corrections.

Actual triangle terrain is now implemented for grass blades, dirt, sand and snow relief. The new terrain test passes deterministic geometry, shared height boundaries, coordinate wrapping, cache eviction and CPU/GPU world-layer ownership. In-game visual acceptance is still pending; the description of procedural-only terrain below describes the earlier verified build.

Ground/sky procedural coordinates and fallback reflections now accept horizon roll. Ground palette classification includes mixed-color ground rows instead of requiring an almost uniform row. Windows D3D12 effects checks passed all 270 ground/sky/reflection/roll combinations against the CPU reference (maximum channel difference 1).

Still unresolved: the user's upper blue sky wedge, remaining horizon coverage in the supplied snow screenshot, and ray-traced secondary hits sampling the original environment. Do not treat these shader parity tests as visual acceptance. The executable compiled but replacement linking was blocked by the running game; no updated current executable was delivered in this checkpoint.
# September 20: banking, even sky, Auto parity and stable mirrors

The camera matrices are Q15 integers. Converting their quotient to float
*after* division rounded horizon slopes below one to zero. The shared
environment_horizon_slope helper converts first, and tests both bank directions
against the projected ground normal and photographic panorama coordinates.
The resulting slope feeds software, portable GPU and DXR environment shading.

Alpine resource 200 is now alpine-day-v2.bmp: a built-in image-tool edit removing
the edge sun/glare and horizontal exposure gradient. Original PNG, lossless BMP,
full prompt and provenance are in assets/enhanced-backdrops/alpine-day-v2.md.
This is an artwork replacement, not a postprocessing shadow or ground change.

Auto formerly skipped the color remapping used by explicit materials. Both
now share the same response for a detected material; tests cover all five
automatic kinds at multiple positions. Full banked Corneria Auto/Grass final
captures are byte-identical (SHA256
AF839720B116A161513E36C2F9A51BFC1BDA9F0EDFF9E973ADA14EAAF00DF372):
tmp/environment-v2-{auto,grass}-sep20/presentation.bmp.

Mirror/gold screen-space fallback now reflects across the actual horizon
plane (factor 2 instead of the water-style 1.55 compression), with bilinear
subpixel taps excluding HUD pixels. A numerical ramp fixture verifies smooth
fractional movement and frozen output as time advances. DXR photographic
background rays no longer floor their coordinates onto the native 256-wide
grid; a hardware gradient fixture distinguishes seven subpixel camera steps
within less than one native pixel. Native pixel-art sampling stays point-based.
Mirror receiver normals remain flat and time-invariant; water keeps waves.

Fresh Windows D3D12 and Linux Lavapipe effects checks cover varied source colors,
both tilt signs, all materials/motions, and protected layers. Hardware DXR tests
pass; runtime ray-ground output confirmed in the banked mirror capture at
tmp/environment-v2-mirror-sep20/presentation.bmp. Windows/current and Linux
applications rebuilt. These are targeted fixtures, not all-stage visual or
sustained-performance acceptance; no release or VR deployment was performed.
