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
campaign coverage. Training still uses the host path. Native pause display,
port enhancements and overlays, rumble, long encounters, scene exits in the
desktop, real-time stalls and input latency still need integration/auditing.
The first native attachment waits for the host sound-phase boundary. The
desktop currently services CPU work at 60 Hz scheduling phases, while rendering
and event collection can run faster. ACCURATE must remain gated until these
remaining requirements, persistence and menu/default selection are complete.
