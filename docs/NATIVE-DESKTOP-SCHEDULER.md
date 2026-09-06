# Native desktop gameplay scheduler

The desktop executes native MAIN updates when ACCURATE is selected. ACCURATE
uses persisted ID 2 and is the fresh/reset default. Existing saved IDs 0
(UNLOCKED 20 HZ) and 1 (ORIGINAL) retain their meanings and host scheduler.
`STARFOX_TRACE_NATIVE_GAMEPLAY=1` reports scheduler counters without selecting
the backend. The older `STARFOX_TEST_NATIVE_GAMEPLAY=1` test override remains.
This integration does not establish full parity; the remaining audits below
still apply.

The selection integration build passed. Eleven targeted tests passed in
144.20 seconds, covering saved settings, menu choices, both simulation data
suites, desktop pace/exit matrices and smoke tests. After adding retained-mode
checks, both desktop matrices passed again in 17.30 seconds: ACCURATE produced
identical native progress at 20/60/144 presentation FPS, while ORIGINAL and
UNLOCKED produced no native updates. Logs are retained in
`validation/accurate-pace-selection-regressions.txt`.

Gameplay borrows the shared native CPU/GSU timeline and binds live SPC/MSU
callbacks. Each 60 Hz scheduling phase budgets the rational NTSC CPU oscillator
rate (236250000/11 Hz), retaining division remainder and instruction overrun
between phases. Source updates may span phases. The desktop keeps presenting
the last completed view; new button edges remain queued until the next source
update and held controller states are sampled while servicing CPU chunks.

Completed updates publish objects, camera, raster, OAM and overlay snapshots.
The presentation interpolator uses elapsed native clocks and the preceding
completed update duration, with normal scene/camera cuts. Live audio is
advanced after chunks and never replayed from the diagnostic write queues.
At a source scene exit it advances sound, unbinds callbacks, invokes the host
exit dispatcher, and retains newly generated frontend sound commands.

## Current validation

After ACCURATE default selection, boss-death audio checks and native rumble
integration (`f7575bd`), all targets rebuilt successfully and the complete
suite passed 95/95 tests in 270.69 seconds. The full log is retained in
`validation/accurate-default-full-regressions.txt`. The build emitted existing
explicit-constructor warnings in the reference GSU overlap audit.
This is regression coverage, not proof of the remaining full-campaign,
enhanced-feature or physical-input requirements listed below.

`native_desktop_timing.cmake` runs three seconds at each of 20, 60 and 144
presentation FPS. Each cartridge reaches an identical native update count,
elapsed master clock and flow across those presentation rates:

| Cartridge / direct map | Completed updates | Master clocks | Flow |
| --- | ---: | ---: | ---: |
| Original / LEVEL2_1 | 23 | 64431818 | 9 (gameplay) |
| EX / LEVEL7_2 | 19 | 64431844 | 9 (gameplay) |

The two matrix tests pass in 11.69 seconds. The three native audio replays and
three ordinary desktop smoke tests also pass after the rebuilt change (6/6,
4.40 seconds). An EX desktop frame was captured and visually inspected after
180 presentations; it shows the stage launch, landscape, Arwing and HUD.

These are short deterministic scheduling checks, not full timing parity or
campaign coverage. Training still uses the host path. Port enhancements and
overlays, rumble, long encounters, scene exits in the
desktop, real-time stalls and input latency still need integration/auditing.
The first native attachment waits for the host sound-phase boundary. The
desktop currently services CPU work at 60 Hz scheduling phases, while rendering
and event collection can run faster. These remaining requirements still need
verification before a release can claim full parity.

## Native pause presentation

The desktop now observes a separate native presentation revision during an
unfinished MAIN iteration. DMA-complete pause boundaries can publish the
interactive EX pause bitmap without completing that iteration or advancing
enhanced particle/dust effects. The first EX pause wait retains the previous
geometry because its next transfer has not submitted a new view yet.

The native input replay checks publication while the hold remains active,
changes EX's MENUSELECTED with live Down input, then resumes through Start
release/press/release. Desktop captures at presentation 500 and 650 verify a
visible EX menu and its removal on resume; a follow-up capture verifies that
only the native pause label is drawn. This validates navigation and resume,
not every EX pause option or step/model-refresh behavior.

After rebuilding all targets, the complete suite passes 93/93 tests in
276.66 seconds. `validation/native-pause-presentation-regressions.txt` retains
the full log, including both presentation-rate matrices, normal/MSU ending
audio, native pause navigation and existing multiplayer input coverage.

## Desktop exit handoffs

Native gameplay readiness now excludes an active host frontend transition.
Special exits deliberately keep the gameplay flow state while their white or
black fade runs. Checking that flow state alone reattached native MAIN during
the fade and prevented the frontend from finishing. Both the desktop scheduler
and native MAIN entry use the explicit readiness predicate.

The exit replay sets LEVELFINISHED after twelve native updates, with CPU/APU
and MSU bindings active. It tests ordinary results (1), credits (9), game over
(10), and the special white fade (16) in both ports. Original repeats these
cases with MSU on when the real pack is available, checking that a recording
was actually selected. The credits cases include Start presses after handoff.
The replay checks the exact destination flow, that only thirteen native
updates completed, and that the CPU timeline remains detached while frontend
execution continues. The special fade reaches planet travel by the end.

All twelve targeted tests pass after rebuilding the desktop and affected
native test binaries (32.49 seconds): the two exit matrices, both presentation
rate matrices, both native MAIN tests, three native audio tests and three
desktop smoke tests. `validation/native-desktop-exit-regressions.txt` retains
the log. These injected exits verify the handoff itself; naturally reaching
every exit in full campaigns and returning from every frontend remain separate
coverage requirements.

## Resume checkpoint after b50612b

Accepted gameplay button taps are retained through source controller polling;
entering pause clears those pulses and restores the physical held state.
Short taps during an already active pause menu still need coverage and handling.
The recorded targeted run passed 15/15 tests; this is not a new full-suite run.

The requested shipping state remains incomplete. Remaining work:

- Audit enhanced controls, God Mode, rumble, effects and EX pause options on
  the native path, including responsiveness during slow source updates.
- Verify pace and interpolation under real-time stalls and long boss encounters
  in both ports, with MSU enabled and disabled. Preserve enhanced input and
  presentation responsiveness rather than reproducing original input latency.
- Complete frontend return and training integration checks.
- Validate the ACCURATE default across complete campaigns and all enhanced
  features. Menu/default and persisted ID 2 are now connected; the desktop
  matrices exercise the production selector without the backend test override.
- Retain the older pace choices and verify their boss speed independently.

The delayed native credits jingle and one-ship Star Wolf reports have not been
reproduced; passing related replays does not establish that those reports are
fixed. Full campaign parity and physical controller/older-PC behavior remain
unverified. No new release has been published from this checkpoint.

## Native rumble servicing

ACCURATE now services the existing Original-port rumble adapter once per
desktop raster phase, including phases with unfinished MAIN work. It reads a
live RAM snapshot and writes the host-owned timer/index without releasing the
held renderer view. Legacy rumble behavior and EX's existing availability are
unchanged. The native replay seeds a timed command and verifies that the
cartridge leaves its timer unchanged, preventing duplicate timer advancement
by the host and source IRQ.

After rebuilding, six focused tests passed in 15.52 seconds: both transfer and
MAIN replays and both desktop pace matrices. The log is retained in
`validation/native-rumble-integration-regressions.txt`. These checks cover
scheduler integration and timer ownership; physical motor output remains
unverified.
