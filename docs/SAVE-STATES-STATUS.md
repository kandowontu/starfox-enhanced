# Save-state implementation status (unreleased)

## Smooth environment clock continuation — September 20

The desktop runtime now appends an optional 64-bit environment clock position
to its RUN container. Older containers load using the cartridge counter as the
initial phase. Invalid large clock values and trailing bytes reject before
live state is committed. This does not change the GAME cartridge archive.

Clock tests cover interpolation, 16-bit counter wrap, pause, repeated ticks,
scene cuts and restored phase. Actual old-format SDL load checks pass for
Original/EX, as do new save/load/slot-selector checks. Fresh-process final-GPU
continuation with Original Drift and EX Swirl passes 30 image comparisons and
13 PCM signatures each at SaveFrame 120. Evidence:
`tmp/state-sky-motion-continuation`, `tmp/state-sky-motion-continuation-ex`.
The continuation harness now pins all six environment fields and optionally
enables a chosen SkyMotion, avoiding inherited user options in default runs.

## Environment archive and restore cache — September 20

All selectable values of the six environment fields now round-trip in actual
Original/EX game archives, including a combined nondefault configuration.
Checks discover each serialized byte by differential payload comparison,
reject checksum-valid out-of-range values and partial environment tails,
verify pre-environment archives default enhancements off, and prove rejection
does not mutate the live game. Unit/Original/EX suites pass 3/3 (50.81 seconds).

The desktop load handoff now invalidates cached atlas classification as well
as temporal rendering history. Previously a same-background-ID restore could
reuse classification from the scene it replaced. This is a cache-lifecycle
correction; no specific before/after visible corruption is claimed.

Actual SDL save/load/slot-selector event checks pass in Original and EX with
Enhanced Sky requested (`tmp/state-enhanced-sky-{original,ex}`). Those indexed
captures verify the input/state path, not final enhanced-sky visual parity.

## Long special-route continuation harness — September 20

The continuation tool now isolates and restores STARFOX/SDL diagnostic settings,
selects D3D12/Vulkan explicitly and pins optional enhancements off. GodMode is
an explicit fixture option. Indexed captures start at the save/comparison window
instead of writing every warm-up frame; presentation-sequence behavior is
unchanged. Wait observations are bounded to 60 seconds and report the same PID,
with no automatic restart. SaveFrame now allows 30,000 for later route states.

EX Comet at SaveFrame 5000, fresh-process restore, passes 60 indexed frame
comparisons and 24 mixed-PCM signatures. The inspected frame 5010 still depicts
the approach, not the boss, so this is not boss-state acceptance. Evidence:
`tmp/state-comet-boss-sep20` (the preliminary directory name is misleading).
This run uses native indexed captures rather than final GPU presentation;
no full-composition or physical audible acceptance is inferred.

The later SaveFrame 18000 run also passes all 60 indexed comparisons and 23
mixed-PCM block signatures in a fresh process. Inspected frame 18010 shows the
active ENEMY meter and Comet boss encounter, unlike the earlier approach.
Evidence: `tmp/state-comet-late-sep20/{baseline,restore}`. This establishes a
bounded active-boss continuation sample, not an entire fight/defeat transition.
Only 130 baseline BMPs were written instead of over 18,000 warm-up images.

## All selectable effect archives — September 20

Replaced the handpicked style list with every selectable entry from the shared
model/world menu order. Original and EX round-trip each layer independently,
checking exact IDs and complete archive equality. This includes all eight new
Manipulations styles on models and the six spatial styles on world layers.
Checksum-valid world archives containing model-only Trails/Long Exposure are
explicitly rejected. Existing Ice/Crosshatch migration, invalid-ID and live-state
immutability checks remain. State unit/Original/EX suites pass 3/3 (92.42 s).
This verifies serialized selection compatibility, not serialization of temporal
GPU history (which intentionally resets) or all-stage restore coverage.

## Grouped-style archive compatibility (September 19)

Original and EX real-cartridge state tests now round-trip Gold Metal, Copper
Metal, Teal/Orange and Handheld, preserving their exact serialized IDs and
unrelated state bytes. Checksum-valid legacy Ice payloads are constructed by
isolating the actual effect byte through differential serialization, not a
hardcoded archive offset. Both model and world Ice migrate to Cyanotype; the
reserialized archive matches the corresponding current Cyanotype archive.

Out-of-range IDs and styles invalid for their layer (Gold on world, Blueprint
on models) are rejected without changing live state. Existing cheat/continuation,
audio-command, checksum and atomic-file tests remain in the same run.
Windows state unit + Original + EX suites pass 3/3 in 75.36s. This closes the
new style compatibility test gap, not physical keyboard/UI or all-level restore
acceptance. No game executable changes were required for this test addition.

## Cheat archive compatibility (September 12)

Added real-cartridge tests for pre-extension (no optional cheat bytes),
intermediate Infinite-Lives-only, and current two-field archives. Enabled
God Mode is retained; absent Infinite Lives defaults off; current enabled
settings round-trip. Repacked, checksum-valid invalid boolean fields and
extra trailing bytes are rejected without mutating live state. Full state
unit/Original/EX continuation suites pass 3/3 in 33.46 seconds. These tests
exercise payload compatibility, not old-release executable compatibility.

## Long death-to-restart continuation (September 12)

EX scheduled source death, fresh-process restore, resident final presentation:
900 advancing frames and304 mixed-PCM signatures match exactly in
tmp/state-ex-death-long-continuation. The baseline reaches PLAYERONPLANET2_STRAT
($179513), confirming return to normal planet flight rather than only a short
death animation. Frame3320 was visually inspected after restart. Death was
injected through PLAYERDEAD_ISTRAT, not caused by a natural collision; broader
boss/level cases and physical audible acceptance remain unverified.

The initial baseline exceeded the old60-second harness observation limit.
The same PID was allowed to finish; its final3390th presentation was retained.
ResumeAfterBaseline supports a completed fresh-process baseline using identical
arguments and requires its final capture. Wait limit is now180 seconds.
No baseline was restarted solely because observation timed out.

Corrected the audible-output predicate: PowerShell array -notmatch returned
all silent lines and wrongly rejected mixed silent/audible sequences. The new
check requires at least one silent=0 line; all PCM signatures still compare.
Silent-only, mixed and audible-only predicate checks pass. The long comparison
then passes in full, including naturally silent death-transition blocks.

## Scheduled death continuation (September 12)

Original with MSU also passes the default fresh-process scheduled-death check:
30 final GPU frames and14 audible mixed-PCM block signatures match, with active
MSU playback required by the verifier (tmp/state-original-msu-scheduled-death).
The comparison window is now configurable using CompareFrames (1..2000,
default30); baseline/restore frame budgets grow accordingly. Longer windows
must be verified separately and are not implied by the short pass.

Added test-only STARFOX_TEST_REVIVAL_FRAME. It waits through ordinary audio-
synchronized startup and rejects injection without an active player/checkpoint.
The continuation harness schedules it only in the saving run for fresh-process
checks, not again after restore, and requires the injection marker in the log.
Default injection is frame2400, save2420; frame600 was correctly rejected before
EX's checkpoint existed. No preroll or production death behavior is changed.

EX fresh-process -Revival -Presentation now passes at the default timing:
30 final GPU frames and14 audible PCM signatures match across restoration of
the injected death-state sequence (tmp/state-fresh-ex-scheduled-revival-late).
This resolves the fixture conflict below. It does not prove a complete natural
death-to-respawn cycle or late boss/ending cases; those remain separate gates.

## Fresh EX runtime recheck (September 12)

Current rebuilt Windows executable passes -Experience EX -FreshProcess
-Presentation in tools/check_state_continuation.ps1: 30 restored final GPU
frames and14 audible PCM signatures match (tmp/state-fresh-ex-current).
This supersedes the earlier quarantine-blocked EX attempt below. EX MSU is
not a supported runtime mode; the remaining MSU gate applies to Original,
not a hypothetical EX MSU combination.

The attempted additional -Revival run fails before rendering because the
initial checkpoint is unset. The harness prohibits preroll to retain startup
audio, but STARFOX_TEST_REVIVAL currently injects death before normal frames.
Evidence: tmp/state-fresh-ex-revival-current/baseline/runtime.log. This is an
unresolved fixture scheduling conflict, not a runtime revival regression or
passing death-state proof. Add a checkpoint-aware scheduled death hook before
using this harness to claim audible revival continuation.

## Bulk RAM archive path

Fixed byte spans and byte vectors now copy in bulk; other field types retain
explicit little-endian encoding. Vector length/allocation checks and fixed-span
truncation checks happen before copying. A 1 MiB bytewise oracle proves identical
encoding, and truncated fixed/vector inputs leave their destinations unchanged.
Original/EX whole-game and frontend/audio continuation tests pass on Windows and
Linux after this change. No save schema/version change is needed.

`starfox_state_tests --archive-benchmark` measures 1 MiB in-memory archive
operations, median of nine batches of 32 after two warm-up batches:

| Host | Write scalar / bulk (µs) | Read scalar / bulk (µs) |
| --- | ---: | ---: |
| Windows MinGW Release | 1419.22 / 324.909 | 224.859 / 13.2281 |
| WSL Linux Release | 1436.22 / 16.4834 | 239.658 / 13.6019 |

These compare the old bytewise operations with the new path in one executable;
allocation/runtime differences affect the hosts. They exclude disk I/O, other
state fields, whole-save/load latency and gameplay FPS.

## Background transition restore regression

Windows and Linux EX state tests now save LEVEL4_4 immediately before its
background changes (ticks 186 and 327, unlocked 20 FPS, no input, god mode,
native SPC). Each restored branch crosses the next-tick background change and
matches twelve ticks of complete game/SPC state and PCM against uninterrupted
execution. The test asserts that the first resumed tick really changes the
background, preventing a passing fixture that misses the transition. This also
covers the freshly synchronized tunnel/scanline metadata through serialization.
It is headless evidence, not runtime hotkey, disk-slot or screenshot proof.

## Integration status

In-game save/load is implemented locally (unreleased): Ctrl-F1 saves,
Ctrl-F2 loads, Ctrl-F3 opens a slot 0–9 selector. Arrow keys/D-pad select;
Enter/Escape/A/B/Start close it. Simulation and audio pause while selecting,
and closing input is suppressed until released. Remapping, frame-debug freeze,
HUD editing and exit confirmation take precedence over save-state shortcuts.

Implemented components:

- `state::Writer`/`Reader`: explicit little-endian fields, no raw C++ struct
  copies or pointers. Bounded input/allocation; strict booleans and exact input
  consumption. Enum ranges and semantic constraints belong to components.
- `state::pack`/`unpack`: magic, schema, cartridge CRC, payload length and CRC
  over header plus payload. Reject wrong cartridges/schemas, corruption,
  truncation and appended bytes before applying component state.
- `ObjectPool`: every semantic object field, including EX extensions; complete
  active/free ordering, allocation generations and generation counter. Decode
  to a temporary pool, validate lists and generations, then commit. Preserve
  raw source initializer words in object fields until the source resolves them.
- `ParticleSystem`: particle positions/history, velocities, owners, life,
  colours, flags and RNG; retain immutable cartridge/table bindings.
- `DustSystem`: all points, RNG and carry.
- `Msu1Audio`: fractional resampling cursor, cached and pending track IDs,
  normalization, volume, pause/play/repeat and staff-roll handoff. Reload the
  cached FLAC through the local loader and verify its CRC and decoded format;
  reject missing or changed assets without changing current playback. No PCM
  buffers or process-local loader callbacks are embedded in save files.
- `Spc700Audio`: both accurate SPC/DSP cores, upload protocol/ARAM/ports,
  stereo filter history and last output stems. The pinned core's state API
  omits output carry samples; a private build-time source extension preserves
  these samples and their clock/count fields, as well as the filter's semantic
  history. Original upstream sources and license headers remain intact.
  Loads construct both cores temporarily and commit only after full decoding.
- `Wdc65816`: machine RAM/SRAM/Super FX RAM, CPU registers/modes/flags/cycles,
  pending interrupts, suspended routine metadata, PPU/DMA state and latches,
  native model draw, audio-port/MSU command queues and bus open data. Loads
  construct a temporary machine, rebuild bus and instruction dispatch pointers,
  decode into fixed RAM without moving mapped buffers, then swap on success.
- `MapVm`: cursor/countdown, player and spawn references, calls/loops, messages,
  cached native writes, display flags/fade and CPU state. Preserve process-local
  condition callbacks; a whole-game transaction must pair this with ObjectPool.
  Loop mirroring uses address order, so rebuilding a hash table cannot change
  the native loop-slot ordering on the next call.
- `GameSimulation`: mutable host flow, pacing/input history, presentation and
  cheat settings, suspended frontend registers, pending music/transitions and
  all native/map/object/particle/dust components. Construct a temporary game
  against the same ROM, rebind campaign entry points, then load components.
  `swap_state` commits without allocation and rebinds the map/scheduler sibling
  pointers so existing runtime references to the game remain valid.

`starfox_state_tests` passes on Windows and Linux. Tests exercise canonical
byte order, header/payload corruption, mismatched cartridge/schema, malformed
lengths, transactional failures, both object layouts and deterministic
post-restore allocation/particle/starfield continuation. These are component
tests, not evidence of whole-game save/load completion.

Additional tests cover a suspended REP/LDA/STA/INX/INC routine, fresh-machine
and same-machine continuation, partial CGRAM/VRAM writes, WRAM port addressing,
SRAM/Super FX RAM and pending audio writes. Map tests restore a suspended
subroutine and loop and compare continuation through native calls. Cartridge
tests advance Original/EX Corneria for 600 source ticks, restore their map/CPU
and object pool into independent instances, then compare 20 map/video steps.
Whole-game tests now also compare 90 gameplay ticks and presentation phases
after restoration in Original/EX, then rewind the live object, destroy the old
storage and continue to detect stale sibling references. Intro, Continue and
planet-selection cases restore game and SPC together and compare 24 subsequent
ticks of PCM and native audio feedback.

Runtime integration:

1. Validate game, both audio cores/MSU, mixer phase and pending audio commands
   before committing the replacement. Cartridge/schema/checksum guarded.
2. Clear queued device audio; rebase clocks and input; snap interpolation and
   invalidate background/history caches. Native state does not advance to load.
3. Store files in `states` beside the platform's cartridge-save location,
   separated by ROM CRC and slot number. Use an exclusively reserved temporary
   directory and atomic rename/replace only after a successful write/close.
   This protects against failed writes, not a guarantee against power loss.
4. `tools/check_save_states.ps1` verifies same-process save/load and `-LoadOnly`
   fresh-process load in Original and EX through actual SDL hotkey events.
   Runtime logs and viewed selector captures: `tmp/save-state-original-proof`
   and `tmp/save-state-ex-proof`.

Additional runtime verification uses `tools/check_state_continuation.ps1`:
two runs save at frame 20; one restores at frame 80. Frames 90–119 must match
uninterrupted frames 30–59 byte-for-byte. Mixer instrumentation, gated behind
test-frame/audio-signature flags, also compares every post-load PCM block's
length/CRC32, selected track and MSU playing state. The fixture rejects
all-silent continuation and requires actual MSU playback when requested.

Windows checks passed 30 rendered frames and 14 PCM block signatures each:
Original gameplay (`tmp/state-pcm-no-preroll`), EX gameplay
(`tmp/state-pcm-ex-gameplay`), Original MSU (`tmp/state-pcm-msu`), Continue
(`tmp/state-pcm-continue`) and credits (`tmp/state-pcm-credits`). These prove
deterministic mixed-output continuation at the sampled points, not physical
speaker output. Earlier visual-only comparisons also passed Original/EX death
fixtures (`tmp/state-continuation-death`, `tmp/state-continuation-ex-death`).
The fast `STARFOX_TEST_PREROLL_TICKS` fixture can discard source audio writes
while advancing game state; its silent output must not be used as evidence of
audible restoration. The continuation harness defaults to zero fast preroll
and now rejects nonzero values before launching, allowing ordinary audio
initialization. Use a later `-SaveFrame` for gameplay reached with synchronized
audio. `-Presentation` compares the final GPU images, not just native raster
captures, and rejects transition readback/replay. It retains the audible PCM
requirement; a visual match alone is not an audio-restoration pass.

Fresh-process Original MSU runtime continuation now also passes
(`tmp/state-fresh-msu-original`): the baseline process saves and exits; a
separate process loads that same slot without saving over it. All 30 checked
frames and 14 audible mixed-PCM block signatures match. Run this with
`tools/check_state_continuation.ps1 -Msu -FreshProcess`. The corresponding EX
attempt was blocked at process launch by Windows antivirus, so it supplies no
EX verification evidence; no protection was disabled or bypassed.

Remaining verification: audible full-runtime death/boss and late end-sequence
cases, console builds and physical controller
testing. Do not publish
the overall delivery while its other requested features remain incomplete.

MSU restoration tests pass on Windows and Linux: fresh-device sample-exact
continuation at 44.1 kHz across short-track loops, pause, partially written
track selection, staff-roll handoff, and transactional rejection of corrupt,
truncated, missing-asset and changed-asset states.

Original/EX direct-audio tests also cover fresh/same-device SPC restore,
24 subsequent ticks with new sound commands, audible boss music, uploads
interrupted halfway, filter/carry continuity and corrupt/truncated rejection.
The audible test failed without the carry-buffer extension and passes with it.
# Save-slot controller navigation follow-up (September 19)

The slot selector now uses fixed menu gamepad navigation rather than gameplay
bindings. Rebinding steering/fire no longer changes slot movement or confirm.
Keyboard events remain separate, avoiding double-counted navigation. Opening
the selector seeds its latch from held controls; subsequent actions remain
press-only. The shared menu sampler reuses the same gamepad-only helper.

Fresh Windows and native Linux input tests pass with every gameplay action
remapped to one button, checking seven fixed controls and held-action rejection.
Both desktop executables rebuild. Actual Ctrl-F1 save / Ctrl-F2 load / Ctrl-F3
open / Right sequences pass for Original and EX; the EX screenshot was inspected
and shows SLOT 1. Evidence: `tmp/slot-navigation-original-sep19` and
`tmp/slot-navigation-ex-sep19` (runtime logs, isolated slots, selector BMPs).
These captures exercise keyboard events; controller behavior is covered by the
SDL virtual-device test, not a physical controller run.
