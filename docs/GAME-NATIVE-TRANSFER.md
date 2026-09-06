# GameSimulation native transfer lifecycle

GameSimulation now owns an internal resumable transfer path:

- `begin_native_transfer(input)` installs the live CPU/GSU timeline on first
  use, holds presentation, latches the supplied input through the existing
  mapper, and prepares SETBLACK_L followed by TRANSFER_L.
- `advance_native_transfer(master_clocks)` advances a bounded chunk and
  returns no result while work is incomplete. A zero budget performs no
  source execution. At completion it publishes state and returns accumulated
  instruction accounting and queued APU writes.
- `native_transfer_active()`, `native_transfer_clock()` and the read-only
  timeline accessor expose lifecycle/progress for the eventual host driver.

The entry point requires source-initialized gameplay or training. It is
deliberately separate from pace selection. Mixing legacy ticks/video phases
or scene changes with the live binding is rejected until a scheduler handoff
is implemented. Errors leave the lifecycle failed rather than allowing a
second transfer to resume an unknown CPU continuation.

## Native draw-list publication

SHOWVIEW builds draw-list entries in active-object order, omitting invisible
objects, and the GSU sorts their links. GameSimulation captures the candidate
handles at SHOWVIEW entry and saves the sorted links and submitted flags at
the following BUILD_DRAWLIST entry. It validates pointers, counts and cycles.
Graphics subsequently reuses draw-list memory; reading that memory only when
TRANSFER_L returned failed at update 5 in both test scenes.

On transfer completion, the held presentation is released, native objects and
map state are imported, and surviving handles are published in the captured
source order. Submitted flags retain the original frame's hit-flash bits.
The host sorting/culling routine is not run again. The enhanced dust/particle
presentation advances once from the completed source camera/object state.
Deadline yields landing exactly on a phase entry also capture that entry,
because resuming a CPU task executes the stopped instruction before checking
stop addresses again.

## Validation and remaining integration

Each cartridge test runs nine transfers with 4096-clock chunks and a final
two-clock quantum, compared with large chunks. Completed object bytes, draw
order, submitted flags, VRAM, palettes and clock totals match. Intermediate
yields retain the prior object data, order, GAMEFRAME and VRAM. Tests also
cover zero budgets, duplicate starts, hold release and legacy-tick rejection.
The rebuilt desktop passes 79/79 regression tests in 207.22 seconds, including
existing input, multiplayer and normal/MSU ending audio checks; see
`validation/game-transfer-regressions.txt`.

The `game-transfer` diagnostic runs 100 Original LEVEL2_1 and 100 EX LEVEL7_2
transfers through GameSimulation. All 11 recorded non-wall-time/non-yield
fields match the prior uninterrupted native-transfer probe. See
`validation/game-transfer-comparison.json`, `original-game-transfer.csv` and
`ex-game-transfer.csv` in that directory. These compare runtime paths, not
an independent full-system oracle or entire campaigns.

This implements the transfer lifecycle, not the complete surrounding MAIN
and front-end flows. The desktop driver, continuous input/short-tap delivery
across source IRQ polling, audio-clock servicing during yields, communication
wrappers, pause/scene handoffs, pace switching and ACCURATE menu/default remain
unfinished. The current application still uses its existing pace choices.
