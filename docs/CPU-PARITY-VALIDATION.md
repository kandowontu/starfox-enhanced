# Native CPU audit

This follow-up checks the native instruction engine used by both ports. It
does not establish complete-game or cycle-exact frame pacing.

The production dependency remains RetroCPU
`ea9049ab25084334f7cc1907b3a98bf1c2604a03`, with the reviewable MIT patch
`cmake/retro-cpu-parity.patch`. CMake checks and applies it, accepting an
already-applied patch. A fresh application and a second application were
checked in a disposable checkout; the resulting files match the tested
dependency. The unrelated dirty game-source submodule was preserved.

## Corrections

- Restore the whole 16-bit accumulator when entering a native routine, even
  when M selects 8-bit operations. The previous debugger-register setter
  restored only A and left its hidden high byte from the preceding call.
- Implement decimal ADC/SBC for 8- and 16-bit words. The pinned dependency's
  decimal paths were unimplemented. Decimal adjustment also preserves the
  processor's behavior for invalid BCD digits and its distinct overflow rule.
- Derive binary SBC's zero flag from the truncated result. For example,
  8-bit `0 - $ff - 1` yields zero with a borrow; the old full-width temporary
  incorrectly kept Z clear. The equivalent 16-bit boundary is covered too.
- Wrap 16-bit instruction-operand fetches within the program bank. A fixture
  at `$3f:fffe` distinguishes `$3f:0000` from `$40:0000`.
- Write the high byte first during a 16-bit read-modify-write. A port-write
  regression observes the order through the APU registers, not just the
  final value in ordinary RAM.
- Correct MVN/MVP termination, 8-bit index wrapping, and per-byte internal
  clocks. The old engine performed an extra instruction after the last byte
  and let 8-bit indices carry into their high bytes. The unused fast-block-
  move shortcut is not enabled by the game adapter or reference tests.
- Correct XCE's exchange of carry and emulation mode.
- Supply missing internal clocks for indexed reads, indirect and stack
  addressing, TSB/TRB, STZ indexed, REP/SEP, BRL, XBA, XCE, PHB, PER and
  indexed-indirect JMP; fetch the signature byte for BRK, COP and WDM.
- Measure 6-, 8- and 12-master-clock bus accesses, including FastROM changes
  through MEMSEL and the `$41ff/$4200` boundary. Memory pages are now 512
  bytes so the CPU core can distinguish that boundary. Internal cycles cost
  six master clocks. Host inspection, bootstrap and synthetic call-stack
  pushes do not add to `executed_master_clocks()`.

## Independent comparisons

`tools/reference/ares_cpu.cpp` includes unmodified CPU instruction bodies
from Ares v148, pinned at
`0aafd85789215e84e1e43415c07d4c88461b7899`. Its bus timing expression is
independent of the production timing helper. It uses a separate Wdc65816
instance for memory and peripheral callbacks; therefore these comparisons
test the instruction engines, not the accuracy of those shared callbacks.
The Ares adapter is an optional development tool, retains its ISC notice,
and is not linked into the game or copied into packages.

- **12,192 instruction cases agree in registers, WRAM and master clocks**:
  254 opcodes, twelve initial status values, aligned/unaligned direct pages,
  and slow/high-bank FastROM instruction fetches. WAI/STP waiting and
  interrupt scheduling are outside the bounded-call adapter. XCE covers
  the transition instruction, not general emulation-mode execution.
- **524,288 decimal ALU cases agree**: exhaustive 8-bit operands, carry and
  add/subtract; every 16-bit accumulator paired with a deterministic sample
  operand, both carries and both operations.
- **1,048,576 complete native ADC/SBC routines agree in registers and clocks**:
  exhaustive 8-bit operands in binary and decimal modes, plus all 16-bit
  accumulators paired with deterministic sample operands. These exercise the
  patched dependency and actual call adapter, including the hidden high byte,
  rather than testing only the decimal helper.
- Ordinary `starfox_cpu_timing_tests` checks hand-counted routines, bus
  boundaries/mirrors, live FastROM switching, call/task accounting, arithmetic
  boundary results, operand-bank wrapping, I/O write order and MVN/MVP index
  wrapping. It requires neither Ares nor game assets.

The first timing-aware bridge run found 2,064 register-state differences and
1,792 timing differences in its original 7,360 cases; some overlap. The final
corpus includes those cases and additional control/arithmetic coverage. The
initial run is not a measurement of release 0.0.4's frame pacing.

To reproduce, run `tools/reference/build-ares-reference.ps1`, then CTest's
`starfox_reference_cpu` and `starfox_cpu_timing_tests`. The first requires the
pinned optional reference checkout. CSVs and a hash manifest are retained in
`docs/validation/cpu-*`; the normal CTest output is in
`docs/validation/ctest-20260905-native-cpu.log`.

## Timing limits

The full game regression suite passed **61/61 tests in 197.41 seconds** after
these changes. Both packaged Windows route-3 smoke captures were inspected.

`MapVm::native_master_clocks()` measures instructions actually executed by
the bridge. It includes real RTS/RTL bodies but excludes the synthetic JSL
entry, translated CPU/GSU work, DMA, WRAM refresh, concurrent CPU/GSU work,
hardware waits and video-phase alignment. The peripheral callbacks still
complete some work immediately. The sampled opcode tests do not prove
all bus-access ordering, all operand combinations, emulation mode or hardware
interrupt behavior.

The production Original-pace scheduler still uses its documented workload
approximation. Substituting this partial CPU count for the complete cartridge
timeline would not establish native pacing. CPU/GSU overlap, transfer timing
and full-frame reference alignment remain the next timing work.

## Native IRQ/NMI entry follow-up

The continuous stage audit subsequently isolated a live TRANS_FLAG read in EX
LEVEL7_2. Its source value changes during strategies as bitmap IRQs advance;
see STAGE-STATE-PARITY-VALIDATION.md. Delivering those IRQs requires preserving
the executing CPU's registers, stack and return address.

The bridge now exposes a level-sensitive IRQ line and latched NMI edge. NMI is
acknowledged on entry; IRQ remains asserted until its device releases the line.
Both operate on the existing native task rather than constructing a new
subroutine call. Interrupt entry is counted separately from executed opcodes.
The dependency's hardware entry omitted the discarded opcode read and internal
cycle. The persistent patch restores both for IRQ/NMI, preserving BRK/COP's
separate instruction-fetch sequence.

An independent Ares entry audit compares **6,912 native IRQ/NMI cases** across
all applicable status bytes, three interrupted address regions, three stack
positions and both ROM speeds. Registers, low-WRAM stack memory, handler PC and
master clocks agree in every case. IRQ-masked cases are exercised separately
in the normal lifecycle test. Restoring the old entry in an isolated dependency
worktree makes all 6,912 comparisons fail on timing. The patch set applies
and reverses cleanly against the pinned RetroCPU revision. Keeping the new
interrupt correction in a separate patch also permits an existing dependency
cache with the earlier parity patch to upgrade without resetting its files;
both clean-install and upgrade paths were exercised.

The normal CPU test also verifies masking, NMI priority, one acknowledgement per
NMI edge, preservation of a pending IRQ through NMI, source RTI continuation and
the enclosing task's return frame. An I/O-address case additionally checks the
discarded fetch's WRAM-port increment and six-master-clock bus access. The optional reference test is
`starfox_reference_interrupts`; `build-ares-reference.ps1` builds and runs it.
Evidence is recorded in `validation/cpu-interrupt-entry-summary.json`.

The rebuilt full runtime suite passed **62/62 checks in 274.52 seconds**.
The subsequently added I/O-fetch assertion passed its rebuilt CPU test in
0.08 seconds; runtime behavior was unchanged. Both logs are preserved in
`validation/cpu-interrupt-regression-validation.txt`. The existing local test
candidate remains the stage-state build recorded in
STAGE-STATE-PARITY-VALIDATION.md; this scheduler groundwork is committed in the
source and has not been published as a release.

These are architectural entry and task-lifecycle checks. Automatic raster IRQ
generation, exact interrupt polling within an instruction, WAI/STP, emulation
mode, DMA/refresh timing and CPU/GSU overlap remain separate work. The production
game does not yet drive these signal APIs from its transfer scheduler. This
change alone does not fix the open EX trajectory or Original pace approximation.

## DMA address counters

The transfer bridge now retains A1B while incrementing/decrementing the 16-bit
A1T counter, and wraps BBAD plus the transfer-mode offset within the $21xx
B-bus page. Previously, a transfer crossing $ffff carried into the next bank,
and a mode offset past BBAD=$ff wrote outside the PPU page. Fixed addressing
continues to take precedence over decrement.

The independent specification used here is the pinned Ares revision listed
above: `ares/sfc/cpu/cpu.hpp` declares sourceAddress as n16 and targetAddress
as n8; `ares/sfc/cpu/dma.cpp` holds sourceBank separately in dmaRun and narrows
the mode offset before transfer. No Ares implementation code was copied.
`starfox_dma_tests` observes actual VRAM writes in 96 combinations of channel,
increment/decrement/fixed control and boundary address, plus PPU register writes
for five transfer modes crossing BBAD=$ff. The test failed against the old
bridge on bank crossing, then all 101 cases passed after the correction.

The rebuilt full suite passed **63/63 checks in 286.75 seconds**, including
normal/MSU ending audio, native interrupt entry and both games' data checks.
The log is preserved in `validation/dma-boundary-regression-validation.txt`.
The local packaged candidate has not been refreshed for this source correction.

This establishes those address-counter behaviors only. Reverse DMA, prohibited
A-bus/WRAM-to-WRAM transfers, DMA clock scheduling, HDMA and refresh interactions
remain unaudited or incomplete. It does not resolve the live EX transfer read.
