# Native desktop gameplay scheduler

The desktop can execute native MAIN updates with
`STARFOX_TEST_NATIVE_GAMEPLAY=1`. This remains a test setting, not the ACCURATE
menu selection or default. Existing pace settings and saved files are unchanged.

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
and event collection can run faster. ACCURATE must remain gated until these
remaining requirements, persistence and menu/default selection are complete.

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
coverage requirements. ACCURATE is still test-gated.
