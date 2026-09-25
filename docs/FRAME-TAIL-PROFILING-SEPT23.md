# Frame-tail attribution — September 23

Added opt-in `-SlowFrameUs` diagnostics to `benchmark_native_defaults.ps1`.
They record logic, background, world, composition and presentation time,
plus scene retirement, command encoding, submission and draw counts.
Numeric records are buffered and printed after the measured run; the final
terrain summary is also deferred. `-GodMode` now explicitly pins the cheat
state instead of allowing persisted settings to influence a benchmark.

The initial diagnostic run exposed an observer effect: synchronous diagnostic
writes occurred inside timed world work. One enhanced run's only slow frame
was its final terrain-summary frame. A native slow frame also contained a
scene timing print. Those samples cannot cleanly separate actual renderer
cost from logging stalls. Explicit `STARFOX_TRACE_SCENE_COST` still provides
its original verbose tracing; use the buffered threshold for frame-tail work.

## Buffered repeat

Same current Windows executable, RTX 5070 Ti Laptop / D3D12, EX 1-1,
1× 4:3, original timing, 60 presentation target, 6,000 unpaced frames,
60-frame warm-up, god mode enabled. Audio uses the dummy driver; DLSS is
intentionally unavailable. No builds or captures overlapped either run.

| Settings | Median | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: |
| Enhanced sky + enhanced ground | 6.074 ms | 7.763 ms | 8.527 ms | 10.354 ms |
| No enhancements | 3.900 ms | 5.615 ms | 6.659 ms | 8.751 ms |

Neither run exceeded the 12 ms reporting threshold after warm-up. Both
recorded 4,782 interpolated presentations out of 6,000. Logs:
`tmp/frame-tail-buffered-{enhanced,native}-sep23/EX-LEVEL1_1-GPU.log`.

## Unenhanced software follow-up

The same 6,000-frame settings, with no sky/terrain upgrades, were then run
sequentially through the software renderer:

| Experience | Median | p95 | p99 | Maximum |
| --- | ---: | ---: | ---: | ---: |
| Original | 3.311 ms | 6.493 ms | 9.142 ms | 11.506 ms |
| EX | 3.996 ms | 8.288 ms | 10.974 ms | 14.039 ms |

EX produced 22 buffered events above 12 ms. Those events show costs spread
across logic, background, composition and presentation, not a GPU scene
submission (which is correctly zero for this path). Original produced none.
All measured samples stayed below 16.667 ms on this machine; this is not
evidence that the reported weaker-system drops cannot occur. Logs:
`tmp/frame-tail-buffered-software-sep23`.

A separate 120-frame GPU smoke run with threshold 1 microsecond emitted
exactly 60 post-warm-up records, frames 60–119, including nonzero scene
retire/encode/submit measurements. This verifies the reporting path even
when the longer GPU runs do not cross their threshold:
`tmp/frame-tail-buffered-hook-sep23/EX-LEVEL1_1-GPU.log`.

This does **not** prove that previously observed outliers are all logging,
nor does it establish physical weak-device, real-audio or visible/VSync frame
pacing acceptance. Earlier benchmark maxima remain valid observations with
incomplete attribution. The new instrumentation makes future recurrence
diagnosable without adding immediate log I/O to the measured slow frame.

The Windows and native Linux application builds pass. Windows' three existing D3D12 dynamic-function cast
warnings are unrelated to these changes. No release or device installation
was performed.
