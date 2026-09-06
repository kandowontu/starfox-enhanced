# Accurate pace implementation target

The requested pace choices are **ACCURATE** (the new default), the existing
**ORIGINAL SPEED**, and the existing unlocked mode. Existing saved selections
must retain their meanings. Reserve persisted value 2 for ACCURATE; values 0
and 1 already identify unlocked and Original Speed respectively.

ACCURATE must use the source execution timeline, including CPU work, DMA,
refresh, interrupt phases and overlapping GSU work. Original Speed retains its
lighter workload estimate, including corrections for composite bosses whose
cost is not represented by their object count. Shared gameplay fixes apply to
all modes. Changing pace must not reset a stage or discard queued input.

Input collection continues on presentation frames. Queued shoulder presses,
multiplayer controls, render FPS and interpolation remain independent of the
pace selection. Accurate source pace does not require reproducing original
controller polling latency or low display frame rates. The existing tests for
variable 47–59 FPS, 90 FPS and stalls must cover the new choice too.

## Current implementation boundary

ACCURATE is not yet exposed or selected by default: the production timing
scheduler is not complete. The CPU and source-dispatch corrections provide
timing inputs; they are not a substitute for the complete scheduler. Do not
label the current workload estimate ACCURATE or certify full parity from the
opening-stage comparisons.

The remaining live-transfer discrepancy in EX LEVEL7_2 reads TRANS_FLAG during
SCORPION strategy execution. The host still drains bitmap DMA synchronously.
The instruction boundary callback, separate bitmap transfer phases, interrupt
latches and reference clock accounting are available as integration groundwork.
The new `SnesCpuTimeline` supplies raster history, field/interlace rules,
refresh and live timer polling. Native CPU bus-clock callbacks now advance it
at read, write and idle boundaries; synthetic call setup remains excluded.
Those callbacks are opt-in and are not yet installed in GameSimulation.
The opt-in CPU timeline now supplies live beam counters, blanking flags,
timer control and interrupt status registers. Both ports' native WAITDMA_L
routine passes scanline waits across a field boundary. This binding now
delivers sampled IRQ/NMI requests at the next CPU step, preserving accepted
requests across flag changes and timeline detach. General DMA now takes bus
ownership after its startup delay and shares that ownership with raster-triggered
HDMA. See HDMA-VALIDATION.md, GENERAL-DMA-VALIDATION.md and
LIVE-INTERRUPT-VALIDATION.md for coverage. A resumable GSU device now supplies
instruction/cache/pixel/buffer timing and bus-access waits; see
RESUMABLE-GSU-VALIDATION.md. The opt-in Wdc65816 binding now synchronizes it
with CPU/DMA/refresh time, maps shared ROM/RAM and delivers its IRQ. Native
binding checks and 216 independent source overlap schedules pass. Installing
this binding in GameSimulation and replacing its immediate transfer orchestration
remain unfinished.
The development-only live gameplay probe now exercises the native graphics
pass against source-initialized stage state; see LIVE-GAME-TIMING-PROBE.md.
It appends that pass for diagnosis and cannot supply final pace totals until
duplicate work and source transfer ordering are resolved.
The complete native TRANSFER_L also executes in the development probe with
timer-driven IRQ/DMA. CPU task clock deadlines now permit cooperative yields
without abandoning live GSU work; see TASK-CLOCK-DEADLINES.md. The surrounding
GameSimulation input, presentation and main-loop integration remains pending.
MapVm can now hold completed presentation RAM/PPU/model data during native
task yields without changing execution or bus state. The sliced transfer
probe uses that hold; see NATIVE-PRESENTATION-HOLD.md. GameSimulation must
adopt this publication boundary before it exposes partially advanced frames.
Production last-cycle hooks now match reference sample clocks and the I flag
for 254 native opcodes, including both initial I states. A callback can select
the pending-interrupt dummy read used by idleIRQ instructions. Timeline binding
now consumes live timer requests at these hooks and delivers the selected
handler. Live WAI/STP now yield through resumable tasks, retaining their
polling cadence and trailing wake idle; see HALT-VALIDATION.md. STP requires
CPU reconstruction to reset. See LAST-CYCLE-VALIDATION.md for sampling checks.
Interrupt entry now preserves the stacked status and sets I/clears D before
vector reads, matching the source's hardware interrupt, BRK and COP order.
This preserves the status order seen by the sampling hooks during entry.
See STAGE-STATE-PARITY-VALIDATION.md and TIMING-PROFILE-VALIDATION.md for measured
results and remaining limits, and BUS-TIMELINE-VALIDATION.md for the shared
clock and bus-order validation.

After integrating and validating that timeline, wire value 2 through menu
cycling, labels, configuration validation, fresh-game defaults and test
overrides. Preserve valid saved values 0/1. Validate switching, saving/reloading,
input queues, both ports' boss pacing, normal/MSU audio and rendering cadence
before refreshing the packaged candidate.
