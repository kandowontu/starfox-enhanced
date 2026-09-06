# Native gameplay MAIN loop

`GameSimulation::begin_native_gameplay_update(input)` enters the cartridge's
actual `GAMELOOP2` on its first update and resumes that same CPU task on later
updates. `advance_native_transfer(master_clocks)` services its bounded chunks.
The existing transfer-only entry point remains available, but mixing the two
execution styles in one live binding is rejected.

Running MAIN itself retains its instruction timing, registers, stack,
communications, EX scored display and other cartridge-specific work. It avoids
reconstructing that work with separate host calls. Native SHOWVIEW and
BUILD_DRAWLIST boundaries still capture source submission order. The completed
view becomes visible when MAIN returns to GAMELOOP2. Intermediate execution
retains the previous published presentation.

## Pause and physical input

`sample_native_controller_held` accepts the primary and four secondary physical
held states between execution chunks. It refreshes controller ports without
consuming input edges or modifying strategy trigger/previous-input bytes.
The caller must retain those edges for the next update's begin call. Shoulder
press pulses already accepted for the current update remain present through
its IRQ polling even if a later physical sample reports the button released.
EX's one-controller multitap mirror is retained; Scope keeps ownership of its
JOY2 packet when selected.

MAIN's DOPAUSE entry and return expose the source pause through `paused()`.
Execution continues through its actual routines and waits; physical sampling
can release Start and later press/release it to resume the same MAIN iteration.
This permits responsive host polling during a source wait. The test-gated
desktop scheduler now connects those events and native audio bus commands.

Pause presentation stops on validated JSL WAITDMA_L return addresses inside
the ROM's DOPAUSE routine (three in Original, four in EX). These DMA-complete
boundaries publish the current view and immediately establish a new hold for
continued CPU execution. They increment `native_presentation_revision()` even
though the MAIN iteration remains active, so the desktop can update the pause
screen without pretending another gameplay iteration completed. Enhanced dust
and particles do not advance on those paused publications.

EX's first wait precedes a new geometry submission. That boundary refreshes
the held presentation while retaining the previous object view; later pause
boundaries import completed submissions when available. The desktop relies on
EX's native pause label rather than drawing a second host label over it.

## Scene-exit boundary

The runtime validates MAIN's actual LEVELFINISHED load and backward branch to
GAMELOOP2 in the loaded ROM. Original uses a short BEQ; EX uses a conditional
skip over an absolute JMP. A nonzero result stops on the fall-through before
game-over processing, stage advancement or a special exit. It publishes the
completed update and sets `native_gameplay_exit_pending()`.

The pause JSR return address is also validated from the ROM. Missing or
ambiguous boundaries are rejected before installing the live timing binding.
These checks support the two current cartridges, not arbitrary MAIN patches.

A pending scene exit rejects another gameplay begin. The CPU continuation
remains suspended until `finish_native_gameplay_exit()` explicitly detaches it
and calls the existing level-exit dispatcher. Before calling this method, the
scheduler must advance audio through the final native clock and detach its
APU and MSU bus bindings. An audio binding, halted CPU, pending DMA/HDMA, or
unfinished GSU work/IRQ rejects the handoff before changing the continuation.
Enabled but idle HDMA channels retain their display configuration after the
old raster is detached. RAM and accepted CPU interrupts are retained.

This boundary is implemented for host frontend ownership; it is not yet
connected to desktop scheduling. It does not flush a partial SPC packet or
make it safe to mix incremental audio with legacy whole-packet rendering.

## Validation and limits

Both cartridge tests compare nine complete MAIN updates with 4096-clock
chunks, a final two-clock quantum, and large chunks. Object bytes, submitted
order/flags, VRAM, palettes and elapsed clocks agree, and intermediate yields
retain the previous view. Instruction callbacks verify one first-channel
communications call per update, plus one second-channel and scored-display
call per update in EX. Forced game over stops at the handoff without modifying
STAGE or DOINGEND, and a subsequent gameplay begin is rejected. Both ROMs
also exercise explicit game-over and credits handoffs followed by a host
tick. An attached MSU callback rejects the handoff without changing the
pending exit, timeline identity or elapsed clock; detaching it permits retry.

Input replays reach controllable gameplay through MAIN, enter the cartridge
pause, deliver release/press/release physical Start samples during that same
update, and resume. Both left and right complete shoulder double taps survive
physical sampling between chunks, while the first tap alone does not roll
and released pulses do not remain held afterward.

The complete desktop build succeeds and all 85 regression tests pass in
234.34 seconds. `validation/native-main-regressions.txt` records the clean run
against the rebuilt implementation, including normal/MSU ending audio and
the existing multiplayer tests. EX's pause replay waits for its launch wipe
to settle, because its control flag clears before the pause gate opens.

After adding the explicit scene handoff, the complete rebuilt suite passes
91/91 tests in 221.45 seconds. The full test log is retained in
`validation/native-scene-handoff-regressions.txt`.

These compare runtime scheduling paths and inspect source routine execution;
they are not an independent full-system timing oracle or full campaign test.
The desktop driver, timestamped audio servicing during partial updates,
desktop scene/pace handoffs, interpolation integration and ACCURATE selection/default
remain unfinished. No user pace or saved configuration changes in this step.
