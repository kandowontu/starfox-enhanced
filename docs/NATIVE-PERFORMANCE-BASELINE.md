# Unenhanced native performance — September 19

## September 24 current default-path spot check

After the presentation-only destruction interpolation change, the local
Original/EX Level 1-1 benchmark ran 180 unpaced 1× frames per renderer with
all optional enhancements off and the DLSS runtime deliberately unavailable.
The first 60 samples were excluded. Frame-work median/p99, in milliseconds:

| Experience | Software | GPU |
| --- | --- | --- |
| Original | 2.716 / 4.708 | 3.343 / 5.105 |
| EX | 3.607 / 5.937 | 3.914 / 6.261 |

Logs: `tmp/native-defaults-current-sep24`. The local machine is much faster
than the reported GTX 750 Ti, the run is hidden/unpaced and no same-machine
older-release control was taken. These data do not explain or dismiss the
reported heavy-action 40 FPS dips. Presentation remains the largest measured
stage (about 2.4–2.8 ms average here); no speculative performance patch was
made from this single-device sample.

## Model upload scratch reuse — September 20

GpuModel now retains the near-plane and normal upload arrays across draws.
Every entry is reassigned for each model; upload memcpy completes before the
storage is reused, and GPU commands never retain these host pointers. Capacity
is owned by the renderer rather than allocated/freed twice per model draw.
This is allocation reduction, not a geometry/quality or shader change.

Two sequential 720-frame unpaced EX/D3D12/1x runs (60 warmup frames excluded):

| Stage | Frame-work median before/after, us | p99 before/after, us |
| --- | ---: | ---: |
| LEVEL1_1 | 2582 / 2128 | 7038 / 6661 |
| LEVEL2_3 | 2657 / 2325 | 8352 / 6334 |

Logs: `tmp/model-scratch-{before,after}-sep20`. Logic/audio also became faster
between runs, so this comparison is system-load-confounded; no causal FPS
improvement percentage is claimed. Build passes. D3D12 and Vulkan each pass
192 Original queued-model comparison images, including resource reuse/cancel
recovery. EX LEVEL1_1/LEVEL2_3 Vulkan native and final captures match the CPU
reference exactly (`tmp/model-scratch-parity-sep20`). Two generic EX model
checker selections yielded no visible pixels and correctly failed their
coverage gate; they are not counted as passing evidence.
The Vulkan destruction-face check also completes: 16 real models, 18,090
GPU images with exact coverage/palette parity against SoftwareRenderer;
normal/depth errors remain within the check's existing tolerance.

## Frame limiter improvement

Replaced PresentationPacer's `std::this_thread::sleep_until` with
`SDL_DelayPrecise`, retaining its absolute deadlines and suspend recovery.
SDL uses platform timers (high-resolution waitable timers on supported Windows)
and sleeps before a bounded final precision spin. This changes pacing only,
not source simulation, renderer selection, or image quality. It also applies
to the startup menu. Some extra CPU/power for the final spin is possible.

Windows build succeeds. Eight visible, paced, real-audio 60 FPS runs completed
without concurrent compilation before and after. Logs:
`tmp/native-defaults-interval-sep19` and `tmp/native-defaults-precise-sep19`.

| Experience / stage | Renderer | Before interval p99 ms | After interval p99 ms |
| --- | --- | ---: | ---: |
| Original 1-1 | Software | 31.696 | 20.066 |
| Original 1-1 | GPU | 31.299 | 20.152 |
| Original 2-3 | Software | 29.466 | 19.592 |
| Original 2-3 | GPU | 33.425 | 22.742 |
| EX 1-1 | Software | 28.314 | 19.317 |
| EX 1-1 | GPU | 31.359 | 20.079 |
| EX 2-3 | Software | 30.441 | 20.003 |
| EX 2-3 | GPU | 30.869 | 21.665 |

Every case improved in this sequential comparison; median intervals after the
change are 16.629–16.678 ms. Software maximum frame-work is now 5.866–8.098 ms
in these runs, but occasional GPU work/interval outliers remain (maximum interval
33.980 ms). System load can affect these numbers; this is neither a universal
no-stutter guarantee nor proof every reported regression shares this cause.
Higher FPS, battery impact and other physical platforms remain to be measured.

### 240 FPS follow-up

All eight visible, real-audio, paced runs completed at 240 FPS / 720 frames
(`tmp/native-defaults-precise-240-sep19`). Median presentation intervals span
3.784–4.163 ms; p99 spans 6.218–7.383 ms and maxima 7.624–15.707 ms.
Every run reports 720/720 interpolated presentations with zero reported cuts.
This verifies the diagnostic run, not visual interpolation of every object.
The 4.167 ms budget is exceeded by slower frames in every case, so a locked
240 FPS is explicitly **not** proven. No matching old-timer 240 FPS run was
made; do not attribute a high-FPS improvement percentage to this measurement.

Reports have no known platform, stage, or FPS target. No general regression
was reproduced by the initial hidden checkpoint below.

Added opt-in logic/audio and frame-work distributions to the existing render
profiler. Frame-work excludes intentional pacing, event processing before the
pacer, and housekeeping after presentation. Normal play does not collect them.

`tools/benchmark_native_defaults.ps1` pins enhancements off, 1x, 4:3, native
source timing and 60 presentation FPS. It tests Original/EX, LEVEL1_1/LEVEL2_3,
Software/GPU. Each run has 360 frames and excludes the first 60 samples.
Windows tests use a hidden real SDL window, unpaced execution, and dummy audio.
These are throughput checks, not visible frame-pacing or physical-device tests.
Screenshot capture is opt-in because readback/encoding contaminates timing.

Clean logs: `tmp/native-defaults-clean-sep19/*.log`.

| Experience / stage | Renderer | Median ms | p99 ms | Max ms |
| --- | --- | ---: | ---: | ---: |
| Original 1-1 | Software | 2.777 | 8.511 | 10.353 |
| Original 2-3 | Software | 2.892 | 10.250 | 10.523 |
| EX 1-1 | Software | 3.165 | 11.250 | 11.752 |
| EX 2-3 | Software | 3.092 | 9.346 | 12.574 |
| Original 1-1 | GPU | 2.277 | 6.552 | 11.398 |
| Original 2-3 | GPU | 2.524 | 10.999 | 18.203 |
| EX 1-1 | GPU | 2.410 | 8.091 | 11.918 |
| EX 2-3 | GPU | 2.534 | 8.635 | 19.674 |

All software samples fit the 16.67 ms 60 FPS work budget here; two GPU cases
have isolated over-budget samples. That does not establish their cause or
prove other hardware, scenes, visible presentation, audio output, or higher
FPS targets are unaffected. No previous-release controlled comparison has
been performed. Next investigation should separate GPU submission/present
waits and test visible paced runs before changing render behavior.

## Visible, paced, real-audio follow-up

The eight-case run completed in `tmp/native-defaults-paced-sep19`. Software
frame-work maxima (Original 1-1, Original 2-3, EX 1-1, EX 2-3) were
20.732 / 6.425 / 8.670 / 7.695 ms; corresponding GPU maxima were
31.437 / 38.361 / 14.426 / 34.685 ms. Software p99 remained 6.079–7.843 ms.
Thus an occasional over-budget software frame *was* observed under visible
presentation, unlike the hidden run. This is not yet a diagnosed regression.
The final GPU case may overlap the start of a build and should be repeated
without background compilation before interpreting its tail latency.

The harness now supports `-Visible -Paced -RealAudio` and the profiler additionally
records consecutive presentation-completion intervals, including pacing and
inter-frame work. These intervals are not monitor scanout timestamps. Next runs
must use the rebuilt executable with all five expected summary lines.

## Clean paced follow-up — September 20

Eight cases completed with no concurrent build: 360 presentations, 60 Hz,
1x/4:3, all enhancements off, hidden window and dummy audio. FSR is now explicitly
pinned off in the harness. All five profiling summaries were present in every
log (`tmp/native-defaults-paced-sep20`). This is not visible scanout/real-audio
acceptance and is not directly comparable with the earlier unpaced run.

| Case | Frame-work p99 / max ms | Presentation-interval p99 / max ms |
| --- | --- | --- |
| Original 1-1 Software | 9.313 / 10.024 | 22.339 / 23.918 |
| Original 2-3 Software | 10.085 / 10.426 | 22.427 / 23.968 |
| EX 1-1 Software | 12.210 / 12.690 | 23.368 / 25.202 |
| EX 2-3 Software | 11.336 / 11.750 | 23.459 / 24.220 |
| Original 1-1 GPU | 7.345 / 13.612 | 20.443 / 26.206 |
| Original 2-3 GPU | 13.493 / 30.019 | 26.465 / 42.141 |
| EX 1-1 GPU | 9.767 / 17.707 | 22.933 / 32.565 |
| EX 2-3 GPU | 12.587 / 40.469 | 26.355 / 44.599 |

Software work fits the 16.67 ms budget here, but completed-frame intervals do
not stay equally spaced. The loop currently calls PresentationPacer before
logic/render work, so variable work duration contributes to completion jitter.
This is a concrete next investigation: separate deadline wake error, rendering
cost and actual present blocking, then test pacing the final submission without
altering source simulation timing. It is not yet proof that moving the wait
fixes GPU stalls, nor a claim of zero slowdowns. Three GPU cases include actual
over-budget work, which must be distinguished from mere completion jitter.

## Final-presentation pacing experiment — September 20

Implemented a test-only `STARFOX_TEST_PRESENT_PACING` path, exposed by the
benchmark's `-Paced -PresentPacing`. It flushes queued rendering, then waits
immediately before mono/stereo presentation. The callback is consumed once
per frame and scoped to the runtime; frozen single-stepping/unpaced tests do
not use it. Source simulation still advances from the existing elapsed-time
clock. Work/render profiling explicitly subtracts the intentional wait;
completion-interval measurements retain it. Normal launches are unchanged.

Same eight-case 360-frame/60-Hz hidden/dummy-audio batch, no parallel builds:
`tmp/native-present-paced-sep20`. Compared with the prior batch:

| Case | Prior interval p99 ms | Final-present interval p99 ms | New max ms |
| --- | ---: | ---: | ---: |
| Original 1-1 Software | 22.339 | 17.121 | 17.337 |
| Original 2-3 Software | 22.427 | 17.030 | 17.231 |
| EX 1-1 Software | 23.368 | 17.086 | 17.283 |
| EX 2-3 Software | 23.459 | 17.023 | 17.535 |
| Original 1-1 GPU | 20.443 | 18.304 | 18.608 |
| Original 2-3 GPU | 26.465 | 17.543 | 18.645 |
| EX 1-1 GPU | 22.933 | 18.016 | 20.087 |
| EX 2-3 GPU | 26.355 | 18.996 | 26.296 |

The consistent software improvement supports moving the pacing boundary as a
way to reduce completion jitter. EX 2-3 still has a 26.256 ms work stall, so
this does not fix all GPU tail latency. These sequential samples do not prove
scanout smoothness, input latency, power behavior, high-FPS/VSync behavior or
other devices. The experiment deliberately remains off for ordinary launches
until those tradeoffs and transition paths are checked; it is not a completed
performance fix or a zero-slowdown guarantee.

Transition follow-up: Original and EX real SDL F1 open/resume/reopen checks
pass with final-present pacing and the ordinary elapsed-time simulation clock.
The EX capture shows Experience LOCKED and Resume (`tmp/present-pacing-f1-ex`;
Original counterpart is `tmp/present-pacing-f1-original`). The 32:9/2x/120 Hz
Game Over six-path matrix also passes both cartridges with actual pacing:
mono/GPU/CPU/forced-failure pixels match, successful Half/Full SBS dimensions
and both-eye margin stars pass (`tmp/present-pacing-sbs-sep20`). For that pixel
comparison only, `STARFOX_TEST_FIXED_RASTER` advances deterministic source
phases while still waiting at presentation; it must not be confused with
elapsed-time simulation/latency acceptance. Normal launches remain unchanged.

## Input age and VSync isolation — September 20

Profiler now records input-sample-to-present-return time (not physical input
latency or scanout) and SDL's effective VSync setting. In EX 1-1 at 60 Hz with
VSync off, moving the wait to final presentation raises median input age:
Software 4.961 -> 16.639 ms; GPU 2.616 -> 16.634 ms. Software interval p99
improves 22.212 -> 16.971 ms; GPU 20.189 -> 18.362 ms. Therefore keep the
experiment off by default: smoother spacing is not a free latency improvement.
Evidence: `tmp/pacing-latency-{False,True}-False-sep20`.

VSync ON exposes a separate, reproducible ~186 ms GPU presentation interval,
both before/after the pacing move. Initially seen with a hidden window, it
persists with the harness's Visible path. A live Win32 query confirmed a test
window was visible and not minimized; that does not prove unobscured scanout.
Software does not exhibit this large stall. Hidden VSync is now rejected by
the benchmark to avoid treating such runs as normal presentation evidence.

Isolation results:
- Normal folder, visible D3D12, old/new pacing: ~186 ms median intervals.
  `tmp/pacing-vsync-visible-{False,True}-sep20`.
- DLSS SDK deliberately unavailable, same D3D12 folder: 183.622 ms median.
  `tmp/pacing-vsync-no-sdk-d3d12-sep20`.
- Clean isolated executable, no proxy/add-on/SDK files, visible D3D12:
  186.144 ms; visible Vulkan: 185.290 ms. Loaded-module check on the clean
  D3D12 process showed system DXGI only. `tmp/pacing-clean-{direct3d12,vulkan}-sep20`.
- Proxy-equipped Vulkan/SDK-unavailable run exited abnormally before audio
  setup; clean Vulkan launched and completed. One comparison does not yet
  prove the proxy is the crash cause; settings/location differ too.

The VSync stall is not explained by optional DLSS or the new pacing boundary.
Next isolate plain SDL presentation from game rendering and inspect display/
swapchain waits; do not disable user-selected VSync or blacklist a driver based
on this result. The normal installation/proxy files were not renamed or removed.
The benchmark supports explicit backend, renderer, stage, experience and
SDK-unavailable selections, restores its environment, and reports seven summary
lines. No performance-fix or startup-issue closure is claimed.

### Standalone presentation isolation

Added `starfox_present_check`, linked only against SDL, with no game code,
assets, shaders, optional SDKs or postprocessing. Copied the executable into
`tmp/pacing-clean-runtime-sep20` so the normal installation's proxy cannot load.
Each visible-window run clears/presents 150 frames and discards the first 30.
SDL reports the window display as 60 Hz (not the integrated adapter's separately
reported 240 Hz).

| Renderer | VSync | Median ms | p99 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: |
| GPU / D3D12 | 0 | 0.119 | 0.318 | 0.649 |
| GPU / D3D12 | 1 | 184.909 | 190.388 | 192.008 |
| GPU / Vulkan | 0 | 0.215 | 0.323 | 0.432 |
| GPU / Vulkan | 1 | 184.318 | 191.165 | 191.567 |
| Direct3D11 | 1 | 185.605 | 190.676 | 192.606 |
| Software | 1 | 16.663 | 16.671 | 16.976 |

This reproduces the large stall without the game and across three GPU
presentation paths. It narrows investigation to SDL/driver/display presentation
on this machine, but does not yet distinguish those causes or explain users'
uncharacterized slowdown reports. Software VSync meets the expected 60 Hz here.
No production VSync fallback or driver blacklist was added. The diagnostic
target builds successfully and `git diff --check` passes.

## Enhanced environment performance, September 20

Grass now uses two floor triangles per patch, dense blades near the camera,
reduced blades at middle distance, and the existing shaded surface beyond
1024 world units. Distant flat grass patches are not submitted individually.
This retains near-field blades without spending model work on subpixel foliage.
Other terrain relief is unchanged. Enhanced Ground is suppressed in the EX
native pre-game menu without changing the saved setting or Enhanced Sky.

The CPU environment pass now reuses presentation row workers for surfaces of
at least 32768 pixels. Reflections read an immutable pre-pass snapshot. Serial
and parallel output is byte-identical in tests covering photographic skies,
procedural shading, water, mirror, gold and protected 2D pixels.

Controlled hidden/unpaced Original LEVEL1_1, native 1x, 120 presentation frames,
60-frame warmup; software and GPU measured sequentially on this PC. Values
are median frame work in milliseconds, not paced FPS or weak-hardware claims.

| Configuration | Software | GPU |
| --- | ---: | ---: |
| Enhancements off | 2.113 | 2.047 |
| Grass before optimization | 22.503 | 63.809 |
| Grass after geometry optimization | 5.107 | 11.365 |
| Sky before CPU parallelization | 3.970 | 2.200 |
| Sky after CPU parallelization | 2.845 | 2.425 |
| Sky + ground before CPU parallelization | 6.380 | 11.322 |
| Sky + ground after CPU parallelization | 5.004 | 11.176 |

GPU sky implementation was unchanged between the last two runs; differences
there are run variance. Final combined GPU p95/p99 was 15.083/17.062 ms;
software p95/p99 was 6.768/9.046 ms, with a 33.228 ms maximum outlier.
This is not a guarantee of uninterrupted 60 FPS. Grass still costs materially
more than plain scenery, and other levels, higher scales and weaker systems
need further profiling.

Evidence: `tmp/grass-before-sep20`, `tmp/grass-final-sep20`,
`tmp/enhancements-off-sep20`, `tmp/enhanced-sky-sep20`,
`tmp/enhanced-both-sep20`, `tmp/enhanced-sky-parallel-sep20`,
`tmp/enhanced-both-parallel-sep20`. Reproduce using
`tools/benchmark_native_defaults.ps1` with `-EnhancedSky` / `-EnhancedGround`.
EX menu choices 3 and 15 with ground enabled are pixel-identical to sky-only
captures (`tmp/ex-menu-no-enhanced-ground-sep20`); settings remain enabled for
gameplay. Windows executable and enhanced terrain tests build and tests pass.

## Optional DLSS runtime off-path spot check, September 24

The current Windows GPU renderer was profiled in Original LEVEL1_1 at native
1x with every enhancement off, 180 hidden/unpaced frames and the first 60
excluded. With the optional DLSS SDK loaded but selection OFF, median frame
work was 3.202 ms (p95 3.489). Deliberately making that SDK unavailable gave
3.165 ms (p95 5.116). The OFF path stayed on native Vulkan, not D3D12, and
the SDK logged "evaluation disabled." This run does not support DLSS/ReShade
as the cause of the reported default slowdown; the p95 reversal also shows
the noise in such a short run. It does not establish a 0.0.6.5 comparison or
weak-hardware 60 FPS parity. Logs: `tmp/perf-dlss-{enabled,disabled}-sep24`.
