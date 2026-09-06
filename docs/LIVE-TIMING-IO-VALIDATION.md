# Live timing I/O validation

`Wdc65816::set_cpu_timeline` binds native CPU bus operations to the shared
raster timeline and decodes live timing registers. `MapVm` forwards the same
binding. Unbound bounded-call execution retains its existing behavior.
An external bus observer can coexist with the timeline and survives detach.

The binding covers beam latching and counter read phases, PIO falling-edge
latching, PPU status acknowledgement, blanking flags, timer programming and
NMI/IRQ status acknowledgement. It does not deliver CPU hardware interrupts,
arbitrate DMA, schedule GSU overlap or attach the production GameSimulation.
Display/interlace configuration remains externally supplied. Automatic joypad
shifting is excluded; enhanced input collection remains independent.

Both ports execute their real WAITDMA_L routine against the live clock,
waiting for scanline 200 and then scanline 20 in the next field while preserving
registers. Synthetic native polling also reaches Vblank. Register tests cover
counter phases, latch acknowledgement, PIO, blanking, IRQ and NMI state.

The full-system reference independently drives a shadow port CPU's timing
register interface from native bus accesses. Original LEVEL2_1 observes
181,240,602 bus operations over 2,159 fields; EX LEVEL7_2 observes 156,984,654
over 1,858 fields. Both have zero raster and timing-register differences.
The observed reads cover SLHV, OPHCT, OPVCT, STAT78, RDNMI and TIMEUP;
writes cover NMITIMEN, WRIO and all four timer bytes. HVBJOY and RDIO reads
are covered by unit tests but were not observed in these reference runs.
Unknown open-bus bits outside the comparison masks are not certified.

All 12 Original and 14 EX pre-existing reference traces remain byte-identical.
Original passes 300 updates and 562,428 gameplay comparisons. EX retains its
known update-200 Y mismatch, with 163,460 comparisons. These are bounded
stage-opening checks, not whole-campaign parity certification.

The desktop rebuild succeeds and **70/70 CTest checks pass in 263.06 seconds**.
Evidence is archived under `docs/validation/timing-io-*`, including register
counts, source logs, trace hashes and the regression transcript. The packaged
candidate remains unchanged; ACCURATE is not yet exposed or the default.
