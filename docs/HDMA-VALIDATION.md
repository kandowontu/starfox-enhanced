# HDMA bus ownership and raster scheduling

The optional native CPU timeline now schedules HDMA setup once per field and
HDMA transfers on visible scanlines. Setup follows the CPU revision's divider
alignment; scanline requests occur at H=1104. Requests are raised after a bus
operation, including refresh phases, and use the same deferred ownership
latch as general DMA. Vblank does not start additional line transfers.

The bus engine supports direct and indirect tables, all eight transfer modes,
both directions, repeat/skip line counts, table termination, 16-bit address
wrapping and channel order. HDMA can interrupt general DMA at setup and byte
boundaries and disable the affected channel. The interrupted channel retains
the source's remaining-length behavior. Indirect addresses share the same
DAS registers as general DMA lengths.

Writing HDMAEN configures this state in both pace paths, but transfers only
advance with the live timeline bound. A timeline has one HDMA owner. Binding
may follow initial register setup; replacing an active timeline requires
HDMA to be disabled and pending requests to finish. A weak state reference
prevents a destroyed CPU from leaving callbacks or blocking timeline reuse.

## Evidence and scope

The development oracle compiles the unmodified pinned Ares DMA source and
extracts unchanged DMA-edge, HDMA-event and scanline scheduling bodies from
its timing source. The source and host each use a separate instance of the
previously validated raster/refresh clock and a deterministic mock bus.
The source channel's general-DMA size and indirect address share storage.

- 18,435 existing general-DMA cases remain unchanged.
- 11,520 scheduled HDMA cases compare bus timestamps/data, memory, CPU/DMA
  clock totals, source/table/indirect addresses, line counters and channel
  completion/transfer flags. They vary modes, directions, direct/indirect
  addressing, masks, bus speeds, repeat/termination counters, DMA collisions
  at frame setup and scanline transfer, and disable/re-enable between lines.
- 48 three-field schedules cover NTSC/PAL, both CPU revisions, interlace,
  225/240 visible lines and 6/8/12-clock CPU cycles. Every visible line transfers
  once and none transfer in Vblank; timestamps match the reference.
- A native CPU fixture programs a two-line repeat table through mapped DMA
  registers, observes both APU-port writes and verifies termination/register
  updates and separate CPU/DMA clocks. It also checks ownership, detach,
  destruction and reuse of a timeline.

These checks cover normal individual bus-operation steps, not arbitrarily
large aggregate clock jumps. The mock bus does not certify every mapped PPU,
APU or GSU device. The timeline receives PPU display/interlace settings from
its caller. No original controller polling latency is introduced.

GameSimulation still uses the older immediate path. GSU overlap, the EX
live-transfer mismatch, ACCURATE menu/default integration and complete
campaign verification remain unfinished. The packaged candidate is unchanged.
Reproduce with `starfox_reference_dma` and `starfox_timing_io_tests`; evidence
is archived under `docs/validation/hdma-*`.

The rebuilt desktop passes 72/72 regression tests in 201.65 seconds. The
81,024-case CPU and 6,912-case interrupt audit CSV hashes remain unchanged.
