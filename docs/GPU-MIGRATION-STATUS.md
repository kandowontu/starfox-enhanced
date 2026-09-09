# GPU migration checkpoint (unreleased)

Windows desktop: D3D11 compute implementations exist for filters, model/world
styles, smoothing, Enhanced Lighting, HDR, chromatic aberration, bloom, AA,
and shadow-mask blending. DXR 1.1 builds shadow masks on supported hardware.
Unsupported backends retain software fallbacks. This is not a complete GPU
scene renderer: geometry rasterization and native composition remain CPU-side.

Late smoothing/styles, bloom and AA now execute in one upload/dispatch batch.
Pre/post-bloom snapshots preserve the separately scaled glow layer. Final RGBA
is not modified until required readbacks succeed, preserving software fallback.

Validation: Windows build and standalone GPU comparisons passed. Batched output
and both bloom snapshots exactly match the separate GPU passes at three tested
intensity combinations. Offset shadow-mask composition also matches exactly.
An isolated 514x258 fixture, eight alternating-order runs per mode, measured:

| Level | Separate passes | Batched passes |
| --- | ---: | ---: |
| 1 | 4.47 ms | 2.86 ms |
| 2 | 4.08 ms | 2.80 ms |
| 3 | 4.34 ms | 2.77 ms |

These are effect-path timings, not an in-game FPS guarantee.

Direct presentation now copies the final compute output into SDL's D3D11
texture without mapping it to CPU memory or re-uploading it. Bloom's final
base/glow split also runs on the GPU, with separate SDL textures preserving
linear filtering of the additive glow. Model-layer separation and silhouette
background reconstruction now also run on the GPU before SDL's existing 1440p
scaling/composition. This direct path applies when setup/touch overlays are inactive.
Screenshots and frame-history capture request the matching CPU pixels lazily.
Other paths retain the existing readback/composition flow. Standalone comparisons
passed exactly for both direct bloom textures and deferred capture at three
bloom/AA combinations; the Windows build and targeted regressions also pass.
Model/base separation matches the CPU reference exactly with positive/negative
offsets, palette ownership mismatches, and bloom disabled or enabled at all three
levels. These are synthetic comparisons, not end-to-end gameplay validation.

Final integration checks: a real hidden-window Windows D3D11 run exercised
direct bloom presentation, DXR shadows and BMP capture. A shutdown crash exposed
driver lifetime ordering; GPU COM resources now release before SDL destroys its
renderer, including renderer switches. The repeated smoke test exited cleanly
and its captured scene was visually inspected. Regression suites passed 44/44
on Windows and 38/38 on Linux (the full runs began before the final lifetime fix).
The xBRZ build switch now also gates shader inclusion.

Portable backend: SDL GPU now executes the same effect shader through Vulkan
or Metal on Linux, macOS, iOS and Android builds. SPIR-V and Metal source are
generated offline; the game needs no runtime shader translator. Vulkan parity
checks passed on Windows hardware and under Linux/WSL. Hidden-window gameplay
smoke tests on both entered direct SDL GPU bloom presentation, captured frames,
and exited successfully. The integrated Linux regression suite passed 38/38.
Metal and Android device runs have not been performed.
The normal Windows executable was in use; Windows smoke testing used
`build/current/starfox_pc_gpu_review.exe` instead.

Remaining: broader end-to-end visual testing, overlay-filter validation,
Metal/Android device validation, Switch/Vita GPU implementations, portable GPU
shadow tracing (only mask composition is portable), and release documentation reconciliation. Do not
describe this checkpoint as a completed all-platform GPU migration.
