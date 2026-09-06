# Interrupt-entry status ordering

The production RetroCPU patch now sets I and clears D after pushing the old
status but before reading either vector byte. Previously it changed these
flags after vector reads. This matters when live devices and last-cycle
interrupt sampling observe CPU state during entry, even when the eventual
register values and total clocks agree.

Read-only `status_register()` inspection in the port and reference adapters
allows the hardware interrupt audit to compare P at every bus-clock callback.
Before the correction, 5,760 of 6,912 hardware interrupt cases differed in
that observation. The matrix covers IRQ and NMI, all applicable status values,
three program address regions, three stack positions and both ROM speeds.
The CPU audit also checks status at each bus step for BRK and COP.

After the correction, all 6,912 hardware cases and 192 BRK/COP cases match
the reference status sequence. The prior hardware register, memory and clock
observations remain unchanged. The full CPU audit passes 40,512 cases. The
desktop rebuild succeeds and all 70 regression tests pass in 259.72 seconds,
including both ports' input, multiplayer, transition and ending-audio checks.

The change is the sixth cumulative dependency patch,
`cmake/retro-cpu-interrupt-status.patch`. Fresh application, upgrade from the
previous five patches and repeated application produce the same source.
Unrelated checkout drift is rejected without modifying it. The user's
separate upstream source checkout is preserved.

This is an entry-order fix, not complete live interrupt delivery. Production
last-cycle hooks and pending-interrupt dummy reads are now validated separately
in LAST-CYCLE-VALIDATION.md. Live delivery, DMA arbitration and overlapping GSU
work remain necessary for ACCURATE pace. Whole-campaign parity
and the existing EX live-transfer mismatch remain unresolved. No packaged
candidate is refreshed by this change.

Evidence is archived under `docs/validation/interrupt-status-*`.
