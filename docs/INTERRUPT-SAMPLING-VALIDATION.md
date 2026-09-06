# Interrupt sampling integration evidence

The independent Ares CPU adapter now exposes the unmodified source core's
lastCycle hook. Each observation records the instruction address, elapsed CPU
master clocks and interrupt-disable flag. Its callback can report a pending
interrupt, allowing the source core to select the corresponding final bus
operation. This does not automatically enter an interrupt handler.

The CPU audit records these observations in `ares_interrupt_samples` as
`instruction_address:master_clocks:masked|` and requires one observation per
executed instruction. The audit includes both initial I states. Repeated block
move iterations each have their own sample. WAI and STP are excluded from this
bounded-instruction audit and still need dedicated live scheduling coverage.

All 40,512 cases across 254 opcodes pass register, memory, total-clock and
bus-step comparisons, producing 40,896 sampling observations. The original
20,256 cases retain identical prior CSV fields. Every final sample precedes
completion by six or eight clocks. In 144 initially unmasked cases, BRK, COP
and RTI sample with I set: BRK/COP set it before vector reads, while RTI
restores it from the stack. The production hook must preserve this ordering.

Ten targeted cases check CLI, SEI, REP, SEP, PLP, RTI and NOP sampling. CLI,
SEI, REP, SEP and PLP sample the old I flag; native RTI samples the restored
flag before reading the bank byte. A slow-ROM NOP samples at clock 8. Without
a pending interrupt it then idles for six clocks; with one it reads the next
opcode for eight clocks. Checking instruction boundaries or unchanged total
clocks from a no-interrupt run cannot establish this behavior.

This work supplied the oracle for the production CPU integration. The hooks
and pending-interrupt bus reads are now covered by LAST-CYCLE-VALIDATION.md.
Bounded delivery connected to SnesCpuTimeline is now covered by
LIVE-INTERRUPT-VALIDATION.md, and WAI/STP by HALT-VALIDATION.md. DMA arbitration and overlapping GSU
work remain necessary before exposing ACCURATE as the default pace.

The production interrupt-entry ordering is now corrected separately: the
stack receives the old status, then I is set and D cleared before vector
reads. The original implementation delayed those changes until after both
reads. A bus-status comparison exposed 5,760 differences among 6,912 hardware
interrupt cases despite equal final state and clock totals. The hardware
audit now checks P at every bus step; the CPU audit does the same for BRK and
COP, leaving its new `interrupt_status_bus_equal` field blank for other
instructions. See INTERRUPT-STATUS-VALIDATION.md for the resulting evidence.

Evidence: `docs/validation/interrupt-sampling-summary.json` and
`docs/validation/interrupt-sampling-regression.log`, with captured test output
in `docs/validation/interrupt-sampling-cpu.log`. Both relevant CTest checks
pass in 31.95 seconds, including 6,912 native interrupt entry comparisons.
Reproduce with the
`starfox_reference_cpu` and `starfox_reference_interrupts` CTest checks after
building their targets against the pinned Ares checkout. The CSV is emitted
as `build/current/reference-cpu.csv` with the standard build directory.
