# CPU bus timeline and raster clock

The accurate scheduler now has a shared `SnesCpuTimeline` component and an
opt-in CPU bus-clock hook. GameSimulation does not yet install that hook:
DMA arbitration, GSU overlap and the new ACCURATE default remain unfinished.
The current pace choices and presentation/input policy are unchanged.

## Raster, refresh and interrupt polling

`SnesRasterClock` models NTSC/PAL fields, the NTSC short scanline, the PAL
interlaced long scanline, interlace capture at line 128 and delayed beam
positions used by timer polling. The independent development comparison
includes the unmodified pinned Ares counter implementation. All 48 fields
across both regions and three interlace policies agree, including late
interlace-setting changes and 0/2/6/10-clock beam history.

`SnesCpuTimeline::step` accepts individual CPU/DMA clock operations, not whole
instruction totals. It inserts the five 6+2-clock refresh phases after a bus
operation reaches the refresh threshold, tracks the version-2 divider change
at each scanline, and advances interrupt polling throughout the stall. Unit
checks cover both CPU versions, delayed IRQ assertion during refresh, DMA
clock accounting and field boundaries. Odd clock steps are rejected.

The full-system reference optionally feeds every native non-refresh clock
operation into a separate instance. That instance supplies its own refresh
clocks; native refresh calls are never fed into it again. After each completed
operation, the audit compares elapsed/category clocks, current and delayed
beam positions, field, interlace, dot and refresh position. It is observation
only and leaves the pinned source instruction/DMA/scheduling bodies intact.

| Final reference run | Bus operations | Elapsed master clocks | Raster fields |
| --- | ---: | ---: | ---: |
| Original LEVEL2_1 | 181,240,602 | 771,880,560 | 2,159 |
| EX LEVEL7_2 | 156,984,654 | 664,313,392 | 1,858 |

Both timelines match at every checked boundary. The source supplies the bus
operations in this audit; this is not proof that the port already runs the
same complete CPU/GSU workload or uses the same runtime pace.

## Native bus hooks and instruction ordering

`Wdc65816::set_bus_clock_callback` advances a timeline before data sampling:
reads emit wait-minus-four clocks before the read and four afterward, writes
emit the complete wait before storing, and each idle emits its own clocks.
Synthetic call-stack setup is excluded. Calls, paused tasks and native
interrupt entry use the same hook. The MapVm wrapper forwards it for the
future gameplay scheduler. Unset hooks retain the existing execution path.

The extended opcode audit initially exposed **11,424 bus-order differences
in 20,256 cases**, despite identical instruction totals and final state. The
core placed indexed-direct idles before operand reads, indexed-store idles
after writes, and several stack/call/return and read-modify-write idles at the
wrong boundary. Long and indexed-indirect calls also need operand reads
interleaved with stack writes.

`retro-cpu-bus-clock.patch` provides the hooks; `retro-cpu-bus-order.patch`
corrects the shared instruction paths. All **20,256 cases now agree in bus
step order**, registers, memory and total clocks. Every pre/post register,
memory and instruction-total column is unchanged. The independent arithmetic
checks also pass: 524,288 decimal cases and 1,048,576 native ADC/SBC routines.
All 6,912 native interrupt-entry cases also match their bus-step sequence.

A separate native test changes a byte at the actual read boundary, checks
that a write has not happened before its bus wait, and advances the shared
timeline across pause/resume. It also verifies that disabling the hook stops
callbacks and that synthetic setup does not enter the observed clock.

## Reproducibility and limits

Later patches change some lines introduced by earlier ones, so independent
reverse checks no longer identify an already-applied series reliably. CMake
now reconstructs cumulative expected files from the pinned revision in a
scratch checkout, recognizes a supported prefix, and applies only its missing
suffix. Validation covers a pristine checkout, repeated configuration, upgrade
from the prior three patches and rejection of unknown drift without changing
the dependency checkout. The user's separate upstream checkout is preserved.

After the bus-order correction, 12 Original and 14 EX full-system traces remain
byte-identical to the preceding clock audit. Original still passes its 300
update comparison; EX still stops at update 200 with its known transfer-flag
Y difference. Bus timing does not replace the missing live transfer scheduler.
These runs do not certify whole campaigns, full-scene rendering or physical
console timing.

Evidence: `validation/bus-timeline-summary.json`,
`validation/bus-clock-cpu.log`, `validation/bus-order-cpu.log`,
`validation/timeline-final-original.log`, `validation/timeline-final-ex.log`
and `validation/retro-series-validation.json`.

The rebuilt desktop and full suite pass **69/69 checks in 284.67 seconds**,
including expanded CPU/interrupt timing, the raster/timeline checks, input and
multiplayer pacing, transitions and normal/MSU ending audio. See
`validation/bus-timeline-regression-validation.txt`. The packaged candidate
has not been refreshed and ACCURATE is not yet exposed in its menu.

Reproduce the raster/CPU checks with `tools/reference/build-ares-reference.ps1`.
For a full-system clock comparison, set `STARFOX_REFERENCE_TIMELINE=1` when
running `full_reference.exe`; it writes a `-timeline.csv` summary and fails
on a clock/beam mismatch. Without that environment variable, the additional
timeline comparison is disabled. See the reference README for normal inputs.
