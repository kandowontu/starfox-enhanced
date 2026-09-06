# Production last-cycle sampling hooks

The production CPU now marks the final cycle of native operand reads, writes,
read-modify-write instructions, branches, stack operations and register
instructions. Width-aware helpers place the hook before the final byte, not
before the whole word. Indirect pointer fetches and synthetic caller setup do
not produce instruction samples. Hardware interrupt entry also samples before
the high vector-byte read, after the status changes documented separately.

`Wdc65816::set_interrupt_sample_callback` observes the instruction address,
cumulative native master clocks and interrupt-disable flag at this point.
Returning true selects the pending-interrupt opcode read on idleIRQ
instructions. Flag-changing instructions retain the source ordering: CLI,
SEI, REP, SEP and PLP sample the old I flag; RTI samples the restored flag.
MapVm forwards this API. Bus observers, timeline binding and sampling hooks
can coexist; synthetic clocks are excluded. Tests check installation,
detachment and rejection of callback replacement during execution.

The independent pinned Ares audit compares **81,024 cases across 254 native
opcodes**, including both I states, operand widths, direct-page alignments,
indexed boundary cases, slow/fast ROM and pending/nonpending interrupts.
All 81,792 sampling observations match, as do registers, memory, total clocks
and ordered bus steps. Pending interrupts change the bus sequence in 2,976
cases across 31 opcodes, including fast ROM where total clocks alone would
not distinguish a read from an idle. Ten targeted flag-order and NOP cases
also compare the production and reference paths. All 6,912 hardware interrupt
entries match their sampling point, status sequence, state and bus timing.

The callback alone does not deliver an interrupt handler. Timeline binding
now connects it to live timer requests and latched delivery; see
LIVE-INTERRUPT-VALIDATION.md. WAI/STP handling, DMA arbitration and overlapping
GSU work remain scheduler work. The bounded audit
does not certify emulation-mode execution or whole campaigns. GameSimulation
does not yet install this timing path; ACCURATE remains unexposed and the
packaged candidate remains unchanged.

The seventh dependency patch is `cmake/retro-cpu-last-cycle.patch`. Fresh
application, upgrade from the prior six patches and repeat application are
verified against the production source. Unrelated drift is rejected without
modifying the checkout. The user's upstream source edits are preserved.

Evidence is archived under `docs/validation/last-cycle-*`. Reproduce the
expanded comparison with `starfox_reference_cpu` and
`starfox_reference_interrupts`; the CPU audit's appended columns include
port sampling observations, their equality and the injected pending state.

The desktop rebuild succeeds and **70/70 regression tests pass in 262.63
seconds**, including both ports' timing, input, multiplayer, transitions and
normal/MSU ending audio. The complete regression audit CSV matches the
standalone run's hash. This confirms the checks listed above, not the still
unfinished live machine integration or full campaign parity.
