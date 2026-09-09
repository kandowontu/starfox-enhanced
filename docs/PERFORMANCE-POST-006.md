# Post-0.0.6 optimization measurements

## Retained change

Indexed-to-RGBA expansion now runs serially for buffers up to 262,144
pixels. Larger buffers retain the existing row-worker implementation. This
avoids thread creation/wake-up/barrier overhead on small presentations without
changing rendering, simulation, interpolation, or audio behavior.

## Measurements

Windows Release (`-O3`), Core Ultra 9 275HX, SDL dummy video/audio, Original
Corneria, 1,000 preroll ticks, 240 presentations at a requested 240 Hz. Three
sequential baseline/candidate pairs per configuration, alternating run order.
Both binaries used the same directory and portable settings. Effects disabled.
These are **CPU/headless presentation timings, not GPU FPS or console results**.

Median of the three per-run average presentation times:

| Aspect / upscale | Baseline µs | Candidate µs | Interpretation |
| --- | ---: | ---: | --- |
| 4:3 / 1× | 2680 | 2446 | 8.7% lower |
| 4:3 / 2× | 2875 | 2470 | 14.1% lower |
| 32:9 / 1× | 2170 | 2059 | 5.1% lower |
| 4:3 / 4× | 873 | 830 | Unchanged large-buffer path; no gain claimed |
| 32:9 / 2× | 853 | 1018 | Unchanged path; slower measured run, needs further profiling |
| 32:9 / 4× | 3852 | 3717 | Unchanged path; no gain claimed |

All 18 paired final captures were byte-identical. An initial pair with
different executable-directory settings was rejected before this collection.
Substantial scheduler/CPU-frequency variance was visible in unchanged paths;
do not extrapolate these timings into a sustained FPS promise.

A separate conversion microbenchmark (1,000 samples after 100 warm-up calls)
also showed reduced small-buffer overhead. In the final paired run, 4:3 native
conversion median/p95 went from 28.6/66.0 µs to 9.6/9.6 µs; 4:3 2× went from
56.6/120.6 µs to 37.5/43.6 µs. Widescreen native went from 38.4/78.1 µs to
29.6/29.9 µs. Large-buffer timings fluctuated substantially.

## Rejected experiment

Caching the three HDR tone curves preserved output hashes but did not provide
a consistent meaningful improvement in paired full-frame benchmarks. Reverted;
the benchmark and exact tone-curve regression coverage remain available.

## Reproduction and stutter diagnostics

Run `tools/benchmark_presentation.ps1` from the repository root with
`-Baseline` and `-Candidate` pointing to executables in the same folder.
Run it in a child PowerShell process because it configures process environment
variables. It alternates run order, records logs, and rejects capture mismatch.
Do not run tests/builds/other benchmarks concurrently with measurements.

Standalone microbenchmarks can be compiled with a C++20 compiler:

```text
c++ -O3 -std=c++20 -Iinclude tools/benchmark_palette.cpp src/render/palette.cpp src/render/row_workers.cpp -o palette-bench
c++ -O3 -std=c++20 -Iinclude tools/benchmark_hdr.cpp -o hdr-bench
```

`STARFOX_TRACE_PROFILE_DISTRIBUTION=1`, together with a bounded
`STARFOX_TEST_FRAMES` run, reports render median/p95/p99/max. It is opt-in and
does not allocate sample storage during ordinary gameplay. These render samples
exclude simulation, audio, and frame pacing; they are not whole-frame latency.

## Remaining investigation

High-resolution worker scheduling, bloom/filter composition and copies, actual
GPU upload/present cost, simulation/audio scheduling, and device-specific frame
pacing still need controlled measurements. No claims of a completed exhaustive
codebase audit or improved Switch/Android/GPU performance are made here.

## Verification

- Windows Release build and all 44 CTest checks passed.
- Linux Release build and all 38 CTest checks passed under WSL.
- Distribution-report smoke run exited successfully and emitted ordered
  median/p95/p99/max values.
- Palette regression covers 1/16/255/256-entry palettes, both aspect widths,
  and 1×/2×/4× buffers; serial and adaptive output must match exactly.
- HDR regression checks every channel value against the original rounding
  formula at all three intensities.

## Second pass: chromatic sampling and worker allocations

Changes are relative to the first-pass executable, not the original 0.0.6.

- Chromatic aberration now calculates each column's red/blue sampling taps
  once, and each row's taps once. Previously their clamping and fractional
  coordinates were calculated for each affected pixel. Arithmetic ordering,
  rounding, protected-layer sampling, green, and alpha remain unchanged.
- RowWorkers reuses its partition vector directly, removing the temporary
  allocation/copy on each threaded dispatch. Mutexes, generation handling,
  wake-ups and completion accounting are unchanged.

The chromatic microbenchmark uses a 796×224 source at 1×/2×/4×, with mixed
model/textured-model/background/HUD/world tags, all three intensities, ten
warm-up frames and 60 measured frames. Both baseline-first and candidate-first
orders were measured. All nine output hashes matched in both orders.

For intensity 3, median effect-only times in milliseconds were:

| Scale | First baseline → candidate | Reverse-order baseline → candidate |
| --- | ---: | ---: |
| 1× | 3.107 → 2.415 | 3.032 → 2.232 |
| 2× | 12.454 → 8.133 | 10.450 → 9.170 |
| 4× | 60.048 → 43.741 | 51.307 → 37.649 |

These selected intensity-3 reductions are 12–35%; they are **not whole-game
FPS gains**. Intensities 1 and 2 also improved at 1×/2×, but 4× reverse-order
results were inconsistent (including regressions). No universal high-resolution
speedup is claimed.

Two paired Original Corneria runs at each scale, 32:9, intensity 3, 120 frames,
1,000 preroll ticks, 240 Hz unpaced and other effects off produced six identical
capture pairs. At 2×, render medians changed from 4.052/3.981 ms to
3.248/3.312 ms; p95 changed from 5.937/6.886 ms to 5.664/5.639 ms. Native and
4× whole-render results were noisy; do not extrapolate to sustained GPU FPS.
These runs isolate the chromatic change, before the worker-storage change.

The worker microbenchmark measured **1,000 → 0 allocations** over 1,000
warmed-up dispatches with four workers. Median dispatch times were 19.9→19.8
µs and, in reversed order, 21.0→18.7 µs. p95 did not improve consistently, so
the retained benefit is allocation elimination, not a stutter/FPS promise.

Reproduce with `tools/benchmark_chromatic.cpp` (header-only C++20) and
`tools/benchmark_row_workers.cpp` (link `src/render/row_workers.cpp`). The
presentation script accepts `-Chromatic 3`, `-Displays`, and `-Scales`.
Regression coverage compares chromatic output against the original per-pixel
algorithm across empty/single-row/single-column/odd-size frames and 1×/2×/4×,
and exercises repeated worker partition growth/shrinkage and pool resizing.

Final combined-build verification: Windows 44/44 and Linux 38/38 CTest checks
passed. Six additional 12-frame capture comparisons (4:3 and 32:9 at
1×/2×/4×, intensity 3) matched byte-for-byte after both changes. Those short
runs are parity smoke tests, not performance evidence.

## Third pass: large-buffer bloom extraction

Bloom extraction can partition the reduced-resolution grid into independent
rows. Each cell accumulates its own source rectangle in the original y-then-x
order, preserving floating-point addition order exactly. The cell is written
once, without atomics or a cross-thread summation. This path is enabled only
with a worker pool and at least 1,048,576 stored pixels. Smaller buffers and
serial callers retain the contiguous raster traversal: the initial experiment
showed that another worker hand-off could hurt those cases.

The four-worker effect benchmark uses 796×224 at 4×, mixed layer tags,
3D bloom 3 / 2D bloom 2, five warm-up frames and 40 measured frames per run.
Three pairs, alternating order, measured:

| Run | Baseline median / p95 µs | Candidate median / p95 µs |
| --- | ---: | ---: |
| Baseline first | 56131 / 79620 | 35774 / 57725 |
| Candidate first | 33150 / 36910 | 28736 / 35192 |
| Baseline first | 33242 / 39462 | 27380 / 32158 |

Median of run medians: **33.24 → 28.74 ms (13.6% lower)**. Median of run p95s:
39.46 → 35.19 ms. All results had the identical output hash
`16691090500900556628`. Smaller scales and serial/threaded output also matched.
The initially broader threading threshold was discarded.

Two 120-frame Original Corneria game pairs at 32:9 / 4× / 240 Hz requested,
1,000 preroll ticks, 3D bloom 3 / 2D bloom 2 and other effects off produced
byte-identical captures. Render medians were 50.740→34.710 ms and
41.833→40.837 ms. p95 was 90.036→44.968 ms in the first pair but
52.608→59.850 ms in the reversed pair. Thus this is evidence of reduced bloom
work, **not a promise of consistently improved whole-frame tails or GPU FPS**.
Measurements used Windows Release and SDL dummy video/audio, with no concurrent
builds or tests.

Reproduce the isolated benchmark with:

```text
c++ -O3 -std=c++20 -Iinclude tools/benchmark_bloom.cpp src/render/row_workers.cpp -o bloom-bench
bloom-bench 4 1
```

The optional arguments select scale and threaded mode. The presentation script
now accepts `-Bloom 3 -Bloom2D 2`. Regression tests retain the original golden
hashes and add odd dimensions with partial reduced cells, unequal/disabled
layer strengths, repeated buffer reuse, and HUD-only clearing after bright
frames. Serial extraction remains the independent reference for these tests.

## Enhanced-shadow passes

Full receiver resolution and all eight area-light samples are retained.

First shadow pass prepares triangle edges/scale once after BVH sorting, and
prepares the eight fixed light directions' triangle terms once per mask.
Prepared queries check scene ownership, generation, and direction before use;
rebuilds cannot reuse old transforms. Traversal stacks initialize only the root
entry instead of clearing 64 entries for every ray.

The synthetic 128-triangle scene (`tools/benchmark_shadows.cpp`, four workers,
ten measured masks after two warm-ups) produced identical mask hashes at
400×224 and 800×448. Before → after median times were 18.50→10.79 ms and
55.50→49.94 ms; reversed-order runs measured 18.00→12.47 ms and 63.03→49.71 ms.
These mask-only reductions vary by scene and do not promise a target FPS.

Two 24-frame Corneria pairs, 16:9 / 2× with shadows on and other effects off,
had identical captures. Render medians changed from 47.97→36.70 ms and
52.30→37.97 ms (roughly 23–27% lower); p95 changed from 54.10→43.78 ms and
57.54→46.30 ms. These remain short CPU/headless samples, not device benchmarks.

Second shadow pass searches the likely-nearer BVH half first for receiver
rays and bounds that search by the exact ground-plane depth when available.
It does not cull casters or approximate receiver depth. Relative to the first
shadow pass, synthetic 2× mask medians were 46.51→40.41 ms and 41.92→36.65 ms,
with the same hash `4068431040336154725`. Native mask hashes also matched.
The additional in-game comparisons were inconclusive: medians 35.64→34.84 ms
and 33.01→37.44 ms. No extra in-game FPS gain is claimed for this traversal
change. Tests compare bounded nearest hits against all-triangle searches.

The presentation benchmark accepts `-Shadows`.

## Requested behavior fixes accompanying this pass

- Removed line thickness from 3D menu navigation and drawing; legacy saved
  overrides are consumed but ignored, and no longer written. Renderer-driven
  line sizing remains at its normal multiplier.
- Player-dying/dead onset sends an immediate native music-cut and MSU stop,
  including HP-zero paths which skip the native death music branch. It does
  not clear sound-effect ports or master volume. Native death cues already
  requested by the cartridge and later checkpoint music remain scheduled.
  The latch resets on scene changes and on leaving the death state.
- SPC `$F0` also stops MSU on EX, rather than restricting that bridge to Original.

Final combined verification (bloom, both shadow passes, menu removal, and death
audio): Windows 44/44 and Linux 38/38 CTest checks passed. This includes normal
and pre-set-HP0 death-onset stop assertions for both Original and EX, followed
by complete death/revive regression coverage. Changes are local and unreleased.

## Additional shadow pass: rejected experiments

Tested compact, cache-line-aligned 64-byte BVH nodes (previously 88 bytes),
then separately per-row/per-light previous-blocker hints with exact re-testing.
Neither was retained: the compact layout regressed short game-run medians;
blocker hints improved two 16:9/2x game medians (40.284 to 39.182 ms and
40.042 to 36.659 ms) but worsened the 1x isolated mask benchmark (9.45–9.75
to 11.78–12.08 ms) and did not consistently improve tail latency.
All compared mask hashes and final game captures matched. These are short
headless CPU-render measurements, not a device/GPU FPS claim.

Artifacts are under `tmp/shadow-pass3`: `game` tests the compact layout and
`hints-final-game` tests blocker hints. Disregard `hints-game`, which overlapped
the candidate build and used the earlier executable. Both experiments were
reverted; the previously verified shadow implementation and quality remain.

Final pre-0.0.6.1 experiment: reducing BVH leaves from four triangles to two
also failed to improve the mask benchmark (1x 9.93/11.66 ms baseline versus
13.65/13.38 ms candidate; 2x results mixed). Hashes matched; reverted.
