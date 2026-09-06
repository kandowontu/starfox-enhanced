# Resumable GSU device

`GsuDevice` executes the GSU instruction, cache, pixel, buffered-memory and
I/O paths as resumable C++ operations. `run_until` uses master-clock deadlines,
not wall time. It suspends at the reference's clock boundaries and retains
nested cache fills, operands and blocked bus accesses. No OS threads or
platform coroutine library are used. Callers retain ownership of ROM and RAM.

The device preserves warm caches across launches, the CPU-stop cache flush,
STOP/IRQ acknowledgement, CFGR/CLSR behavior, outstanding memory buffers and
ROM/RAM ownership rules. Device bus observations are optional. Reentry and
callback replacement during execution are rejected; exceptions leave a failed
device that consistently rethrows instead of resuming a completed coroutine.
Destroying a suspended device releases its nested operations.

## Provenance and generation

The checked-in generated files adapt Ares v148, commit
`0aafd85789215e84e1e43415c07d4c88461b7899`. They retain the ISC notice in
`src/simulation/gsu/LICENSE-ARES.txt` and `THIRD_PARTY_NOTICES.md`. The opcode,
cache, pixel, buffer and I/O expressions retain their source order. Calls that
can advance time become coroutine awaits; source thread synchronization becomes
an integer deadline yield. Small project-owned integer/bit-field types replace
nall, so normal builds need neither an Ares checkout nor nall headers.

`tools/codegen/generate-gsu.py tmp/ares-timing` reproduces the files from an
unmodified pinned checkout. The source manifest records all input hashes.
The generator also marks the source cache-fill loop's unused index and strips
trailing whitespace; neither changes device behavior.

## Validation

- **24,576 opcode/ALT cases** compare the independently compiled, unmodified
  Ares implementation with the new device. Coverage includes all 256 opcodes,
  all ALT settings, both CLSR settings, CFGR=0/$a0, source/destination register
  selections, signed-boundary values and different caller deadline sizes.
  Instructions, STOP clocks/status, all 16 registers, complete SRAM and bus
  event timestamps/addresses/data/order match. Source idle completion is
  explicit in these fixtures so post-STOP buffers use the running device's
  six-clock idle cadence rather than the isolated adapter's shorter drain.
- Native checks cover cold/warm cache timing, CPU stop and IRQ masking,
  blocked RAM writes and resumed buffered bytes, observer reentry, failure
  propagation and destruction during a nested ROM wait. ROM ownership and
  backwards-deadline checks are also exercised by the comparison harness.
- With `STARFOX_COMPARE_GSU_DEVICE=1`, the existing cartridge audit compares
  every isolated source launch against the device, including its complete
  SRAM, 16 registers, STOP state, instruction count and STOP clocks. All
  **73,536 launches** pass across Original and EX meshes, sprites, matrices,
  projection, dust and grid routines. All **70,980 result rows** still agree
  with the separate Snes9x-based numeric reference.
- Four route-2/3 view audits additionally pass with device comparison enabled:
  720 transform/sort launches sampled over 1,800 updates per route/game. The
  adjacent render regressions use saved goldens and are not extra live GSU
  comparisons.

The cartridge audit uses cold GSU runs and seeded SRAM from the existing
bootstrap. It does not prove CPU/GSU/DMA contention during a live game.
The opcode matrix compares bus events; the larger cartridge audit compares
complete results and STOP clocks, not every intermediate bus event. Raw
cartridge outputs remain under `tmp/resumable-gsu-audit`; summaries/logs are
archived under `docs/validation/resumable-gsu-*`.

Reproduce the focused checks with `starfox_gsu_device_tests` and
`starfox_reference_gsu_device`. Reproduce the cartridge checks with:

```powershell
$env:STARFOX_COMPARE_GSU_DEVICE='1'
python tools/reference/verify-ares.py --output-directory tmp/resumable-gsu-audit
```

## Remaining integration

Wdc65816 now has an opt-in live binding through `set_gsu_timing(true)` after
installing a CPU timeline. CPU/DMA/refresh synchronize the device, cartridge
mapping enforces shared-bus reads, and GSU IRQ participates in interrupt
sampling and WAI wakeup. Original uses 64 KiB and EX uses two existing 64 KiB
RAM spans. Native tests cover CPU polling, buffered stores, bank mirrors, DMA
ownership, IRQ acknowledgement and guarded detach. First attachment starts
cold internal GSU state with preserved RAM and imported CPU-written ports;
it does not reconstruct a previously translated execution's warm cache.

The development-only `starfox_reference_gsu_overlap` test compiles unchanged
pinned Ares CPU step/scanline and GSU device bodies with libco. All 216 scripted
schedules match CPU raster/thread clocks, GSU clocks/instruction counts/IRQ,
memory and timestamped bus events. The matrix covers both CPU versions, both
GSU clock settings, six starting raster phases, 6/8/12-clock operations and
ROM/RAM ownership waits. HDMA is disabled in this overlap fixture; it is not
a full-game or combined HDMA/GSU contention certification. libco remains a
development dependency and is not linked into the production core.

**GameSimulation does not use the live binding yet.** The EX live-transfer
mismatch, ACCURATE menu/default, pace switching,
input/render/audio validation and full campaigns remain unfinished. Existing
pace modes still use their previous graphics path. The packaged candidate is
unchanged. The device currently uses dynamically allocated coroutine frames;
its cost in the integrated game loop still needs measurement.

The live-binding desktop rebuild passes 76/76 regression tests in 206.50
seconds; see `validation/live-gsu-regression-validation.txt`.
The previous 81,024-case CPU and 6,912-case interrupt audit CSV hashes are
unchanged. No live-game or physical-hardware parity claim follows from these
isolated device checks.
