# Cooperative WAI/STP execution

With a live timeline bound, WAI and STP now execute their first sampling idle
and yield through `Wdc65816TaskResult.waiting` or `.stopped`. The task remains
active. Each resume advances one six-master-clock polling idle while halted;
these continuations are not counted as additional instructions. The decoded
halt opcode is counted once. A wake clears WAI, completes its trailing idle,
then allows accepted interrupt delivery or ordinary execution to continue.

A masked IRQ wakes WAI without entering an IRQ handler. STP ignores both IRQ
and NMI wake requests, continuing to advance the bound device timeline. Halt
states survive timeline detach. CPU reconstruction is the supported reset
path; starting another call cannot overwrite a halted CPU's state. Calls
without timeline binding retain their prior execution path.

Yielded `stop_address` is the next PC and need not be a requested breakpoint.
A halt at the synthetic return sentinel does not complete the task. Source
projection helpers and return checks cannot run ahead of a halted CPU.
Synchronous calls have a finite idle-step budget for WAI and report STP
immediately; callers needing to wait cooperatively use the task API.
Accepted interrupt entries do not consume an additional instruction-budget
slot after a WAI wake.

The independent Ares adapter executes the unmodified source WAI/STP loops
with a bounded observation window and injected wake times. **80 native
cases** cover both opcodes, both I states, slow/fast ROM, four no-wake windows
and six wake timings. Register state, bus steps, total native clocks and
sampling observations match. The oracle's observation limit ends at a bus
boundary; it does not change the source instruction bodies.

Additional production checks cover masked IRQ wake after detach, STP
persistence under NMI, preservation when another call is attempted, bounded
synchronous calls and a halt at the synthetic return PC. A mapped horizontal
timer wakes WAI and enters IRQ at native clock 112. An immediately available
IRQ enters at clock 82 with a one-instruction budget.

The eighth cumulative dependency patch is `cmake/retro-cpu-halt.patch`.
Fresh application, upgrade from seven patches and repeat application match
the production source. Unrelated drift is rejected without modification.
The user's separate upstream edits are preserved.

This completes the tested halt primitive, not the game-loop integration.
General DMA is covered separately by GENERAL-DMA-VALIDATION.md and HDMA
by HDMA-VALIDATION.md. Overlapping GSU work, the EX live-transfer mismatch and
whole-campaign parity remain unfinished. ACCURATE is not yet exposed or the
default, and the packaged candidate remains unchanged. Evidence is archived
under `docs/validation/halt-*`; reproduce the source comparison with the
`starfox_reference_halt` CTest target.

The final desktop rebuild succeeds and **71/71 regression checks pass in
221.37 seconds**, including both ports' input, multiplayer, timing,
transitions and normal/MSU ending audio. The preceding 81,024-case CPU and
6,912-case interrupt-entry CSVs remain byte-identical. These checks do not
certify the unfinished game-loop integration or whole campaigns.
