# Cooperative CPU task clock deadlines

`Wdc65816::set_task_clock_deadline` sets an optional absolute raster-clock
deadline for resumable tasks. MapVm forwards the API. At the next instruction
boundary at or beyond the deadline, the task returns `deadline_reached`
without discarding its CPU registers, stack, pending interrupts or live GSU.
Ordinary synchronous calls retain their existing behavior. A deadline requires
a CPU timeline; clear it before replacing that timeline.

The final instruction and any DMA it initiates can overrun the deadline.
This API does not suspend an instruction or DMA byte halfway through a bus
operation, and it is not a wall-time limit. An already-expired deadline runs
no CPU/DMA work. A task that has returned reports completion rather than a
deadline yield. WAI/STP state remains visible alongside the deadline flag.

Native regression checks compare uninterrupted execution with deadlines
spaced 2, 6, 100 and 4096 master clocks apart. The fixture overlaps CPU work,
DMA, refresh and a GSU IRQ. Final CPU registers, stack, status, interrupt
count and CPU/DMA/refresh/total clocks match. The zero-clock initial deadline
also verifies that synthetic task setup does not advance the devices.
The rebuilt desktop passes all 76 regression tests in 208.82 seconds,
including existing input, multiplayer and normal/MSU ending audio checks;
see `validation/task-clock-deadline-regressions.txt`.

## Native transfer probe

The live-game diagnostic now accepts `native-transfer` and
`native-transfer-sliced`. Both run SETBLACK_L and the cartridge's complete
TRANSFER_L instead of the host tick or appended drawing pass. They use the
source-initialized game state and establish INITSCREEN_L's H/V timer values
and $31 interrupt control after installing the timeline. The source IRQ
handler performs bitmap transfers; the probe does not drain TRANS_FLAG.

The sliced variant runs TRANSFER_L as a resumable task, yielding after
4096-clock chunks and importing objects only at completion. Its purpose is
to verify that relinquishing host control preserves native transfer execution.
CSV columns report yield count, GAMEFRAME, VIEWPOSZ, MAPPTR and TRANS_FLAG in
addition to execution clock totals. The column named `shell_clocks` contains
the native transfer call duration in these modes; no host tick is run.

100 Original LEVEL2_1 transfers and 100 EX LEVEL7_2 transfers match between
uninterrupted and sliced execution in all 11 recorded comparison fields.
Original yielded 57,898 times and EX yielded 51,658 times. Wall time and yield
count are intentionally excluded from equality. See
`validation/native-transfer-slicing-comparison.json` and the four
`validation/*-native-transfer*.csv` files. This comparison is within the
production CPU/GSU implementation, not against an independent full-system
reference. It verifies preservation across yields, not absolute accuracy.

These probes do not implement the surrounding MAIN loop, audio playback,
input collection or enhanced render snapshot publication. They do not prove
full-system pace parity. They are the next integration building block for
GameSimulation, where the existing host frame path still owns gameplay.
