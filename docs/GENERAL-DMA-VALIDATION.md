# General DMA bus ownership

With a live timeline bound, writing MDMAEN now queues DMA. One complete CPU
cycle runs before ownership changes. The engine aligns to the eight-clock
DMA divider, performs global and per-channel setup, transfers each byte in
two four-clock phases, then resynchronizes to the interrupted CPU bus cycle.
Refresh advances the shared raster clock but is excluded from the DMA work
counter used for resynchronization. CPU instruction clocks remain separate.

The engine implements all eight general DMA modes, channel order, both
directions, increment/decrement/fixed addressing, source and target wrapping,
zero-length 65,536-byte transfers and register updates after each byte. It
also applies the source A-bus restrictions and WRAM-port conflict rules.
Reverse transfers and these data restrictions are shared with the older
immediate path; its transfer timing remains immediate.

The independent oracle compiles the unmodified pinned Ares `dma.cpp` and an
unchanged `dmaEdge` body extracted from `timing.cpp`. **18,435 cases** match
bus event timestamps, data, channel registers and timeline totals. The matrix
varies modes, direction/address flags, CPU bus speeds, divider/refresh phases,
lengths and channel masks. It includes two full zero-length transfers and a
cancel/re-request case that preserves the source's armed DMA edge.

The oracle uses a deterministic mock bus and the separately validated shared
clock. It does not certify every mapped PPU/APU/GSU register or HDMA behavior.
In particular, reverse transfers use the port's existing device read paths;
this change does not add unimplemented PPU read registers.

Native execution checks cover a CPU store to MDMAEN followed by the deferred
cycle and transfer. That fixture takes 60 CPU clocks plus 36 DMA clocks, with
data still untouched at CPU clock 54. Other checks cover reverse APU-port
reads into WRAM, forbidden WRAM-to-WRAM transfers and preservation of the
timeline while DMA is requested. An IRQ raised during DMA waits for the next
instruction's sampling point: the fixture executes four instructions and
enters its handler at 138 CPU clocks / 174 total clocks.

The DMA engine has not yet been installed in GameSimulation's pace path.
HDMA setup, scanline transfers and interruption of general DMA are now
covered by HDMA-VALIDATION.md. GSU overlap, the EX live-transfer discrepancy
and complete campaign verification remain unfinished. ACCURATE is not yet exposed or the default. The
packaged candidate is unchanged.

Evidence is archived under `docs/validation/general-dma-*`. Reproduce the
oracle with `starfox_reference_dma`; the regular timing I/O test covers the
native bridge and shared immediate-path data fixes.

The complete regression suite passed 72/72 tests in 215.86 seconds. The
81,024-case CPU and 6,912-case interrupt audit CSV hashes remain unchanged.
The desktop was rebuilt; the packaged candidate was not refreshed.
