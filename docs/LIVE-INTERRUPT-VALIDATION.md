# Live sampled interrupt delivery

When a CPU timeline is bound, the production CPU now samples raster and
external IRQ/NMI requests at its validated last-cycle hook. Accepted NMI and
IRQ requests are latched separately. The following CPU step enters NMI first,
then any already accepted IRQ, without re-testing the instruction's final I
flag. A simultaneous IRQ therefore survives NMI entry. This follows the
priority and latched-request order in the pinned source CPU::main.

External NMI is acknowledged when accepted, so a new edge during entry is
preserved. External IRQ remains level-sensitive. Polling locks inhibit both
external and raster requests. Accepted delivery survives task pause and
timeline detach; detachment restores the legacy sampling path for subsequent
requests. The sampling observer remains available alongside live delivery.
Hardware interrupt entries remain excluded from native instruction counts.

Eight targeted native execution cases cover:

- Mapped horizontal timer IRQ and vblank NMI delivery.
- CLI's one-instruction delay and IRQ acceptance before SEI changes I.
- Early versus final-cycle NMI arrival, including the pending opcode read.
- A second NMI arriving during entry, with both nested RTI frames restored.
- Simultaneous NMI/IRQ priority, IRQ source release and timeline detach.

The slow-ROM single-instruction cases enter the handler after 78 CPU master
clocks; delayed cases take 92. Stack checks ensure the final status is pushed,
even though the acceptance decision used the earlier I flag. These are
targeted integration checks based on source polling/entry ordering. They do
not replace a full-system live instruction-stream comparison.

The existing independent timer, last-cycle and architectural entry audits
remain separate evidence for those components. WAI/STP scheduling is now
covered by HALT-VALIDATION.md, and general DMA by GENERAL-DMA-VALIDATION.md.
HDMA arbitration, GSU overlap and installation
in GameSimulation remain unfinished.
The EX live-transfer mismatch and whole-campaign parity remain unresolved;
ACCURATE is not yet exposed or the default, and the packaged candidate is
unchanged. Evidence is archived under `docs/validation/live-interrupt-*`.

The desktop rebuild succeeds and **70/70 regression tests pass in 218.14
seconds**, including both ports' input, multiplayer, timing, transitions and
normal/MSU ending audio. The 81,024-case CPU and 6,912-case interrupt-entry
audit CSVs remain byte-identical to their preceding validated results.
