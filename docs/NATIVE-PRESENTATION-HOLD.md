# Completed presentation during native task yields

The accurate scheduler must not expose partially updated source RAM or a
half-transferred bitmap between cooperative CPU yields. MapVm now offers
`hold_native_presentation()` and `release_native_presentation()` for that
boundary. The hold captures WRAM, GSU/cartridge RAM, the PPU presentation
state and the direct-model draw state. Snapshot storage is reused between
frames. Nested holds are rejected.

While held, public RAM reads and PPU/model/cartridge-RAM accessors return the
captured presentation. ROM and device I/O remain live; audio queues and native
writes are not redirected. Native CPU execution continues using its own live
memory. Display-cache refresh is deferred until release. The caller must also
defer object/map imports and other host presentation mutations until the
completed update is ready to publish. This is a single-threaded presentation
boundary, not a CPU save state or general thread-synchronization mechanism.

Wdc65816 copies backing memory directly without touching its bus, advancing
time or acknowledging device registers. This matters when the GSU owns RAM:
a renderer snapshot must capture the backing bytes without receiving CPU open
bus values or changing the open-bus latch. Snapshot RAM addressing preserves
the WRAM and live GSU aliases and the actual device's 64/128 KiB layout.

## Validation

`starfox_presentation_hold_tests` executes a native task across a yield and
checks that its WRAM/GSU stores remain hidden until release. It also checks
VRAM, palette and direct-model publication, RAM aliases in both cartridge
sizes, nested-hold rejection, reuse on the next frame, and side-effect-free
capture while the GSU owns RAM. Device I/O is excluded from the RAM lookup.
The rebuilt desktop passes 77/77 regression tests in 200.12 seconds,
including existing input, multiplayer, and normal/MSU ending audio tests;
see `validation/presentation-hold-regressions.txt`.

The sliced native-transfer probe now holds presentation while TRANSFER_L
runs, releases it at completion, then imports objects. 100 Original LEVEL2_1
and 100 EX LEVEL7_2 transfers match the prior sliced runs in all 12 recorded
non-wall-time fields, including clock totals and yield counts. See
`validation/held-transfer-comparison.json`, `original-held-transfer.csv` and
`ex-held-transfer.csv` in the same directory. This is a preservation check
inside the production runtime, not an independent full-system parity test.

The main GameSimulation and desktop loop do not use this hold yet. Their
accurate-mode input, source frame scheduling, completed-state publication and
pace selection remain to be integrated. Existing modes retain their behavior.
