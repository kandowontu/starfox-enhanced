# Input response while matching game pace

The acceptance target is the original game pace with the ports' enhanced input,
rendering and interpolation. Hardware input latency is not an acceptance target.

## Complete digital taps between presentations

The desktop loop previously drained SDL events and then sampled only the final
device state. A press and release arriving before that sample left no held bit,
so neither transition reached InputLatch. The new event mapping retains complete
digital keyboard/gamepad taps and supplies them alongside the held-state sample.
Mappings, controller identity, face-button swapping and secondary player routing
are respected. Keyboard repeat and editor/remapping-owned events are excluded.
An action already held through another binding does not receive duplicate edges.
Frame-freeze toggles discard queued edges together with the ordinary latch reset.

The runtime input test generates real SDL events with a virtual gamepad: press a
shoulder, update SDL, release it, update SDL, then drain the queue and sample.
For both shoulders the old final-state-only path reports no press; the new path
preserves press and release through twelve additional polls, delivers each edge
once, and leaves the held state unchanged. Tests also cover remapped keyboard
events, auto-repeat rejection, player isolation and overlapping held actions.
The same test runs in desktop and UWP configurations.

The rebuilt full suite passed **65/65 checks in 283.94 seconds**, including
both games' pacing replays and normal/MSU ending audio. The log is preserved in
`validation/input-tap-regression-validation.txt`. The desktop executable was
rebuilt; the existing packaged test candidate has not been refreshed.

## Shoulder taps reaching the native roll strategy

PSTRATS.ASM reads current/last held shoulder bits, so a captured press edge with
held=0 still disappeared from its roll logic. During gameplay/training the
primary input bridge now presents a fresh shoulder press for one source update.
It clears that shoulder's previous/last bit on a fresh press, preserving a
physical release that occurred between simulation samples. Subsequent updates
return to the actual held state. The source strategy still owns the roll window,
roll speed and recovery; no extra simulation update or presentation delay is
introduced. Menu controls and non-shoulder button handling are unchanged.

The new replay polls inputs once per presentation. It waits until PSHIPFLAGS
enables player control, then supplies one-millisecond shoulder taps. Paired taps
are 120 milliseconds apart. For each port, 64 cases cover routes 2/3, both pace
modes, both shoulders, single/paired taps and four presentation schedules: 60 FPS,
90 FPS, variable 47–59 FPS and the same variable schedule with 100 ms stalls.
Single taps must not roll; paired taps must roll; the consumed press count must
match the physical tap count.

`tools/reference/verify-short-roll-baseline.py` compiles the previous
`a85dceb` GameSimulation implementation separately and links it with the current
replay and other core objects. It does not change the worktree or normal build.
Both games fail the paired-tap check with that implementation while player
control is enabled: two delivered presses, CONT L=0, roll delay=0, no roll.
An earlier test attempt fired during launch (PSHIPFLAGS=96); that fixture was
corrected and is not evidence of a gameplay-input defect.

After the bridge correction, the rebuilt suite passed **65/65 checks in
277.34 seconds**. That includes all 128 new short-tap scenarios across both
ports and the existing 56 one-minute pacing replays. The baseline rejection
and full-suite log are in `validation/short-roll-regression-validation.txt`.
The packaged candidate remains unchanged; these changes are in the current
source and rebuilt desktop executable.

## EX secondary and multitap shoulder taps

The same one-update shoulder press now reaches EX's secondary controller and
multitap slots. Fresh press bits clear the corresponding native last-controller
bits; physical hardware controller values remain the sampled held state.
The one-controller multitap mode mirrors the pulse to all five native players.

`starfox_ex_multiplayer_input` activates the ROM's actual secondary player
strategies and checks both shoulders and both pace modes: two-player mode,
each secondary slot in five-player mode, and the one-controller mirror mode.
All 24 short-tap cases pass. A single tap must not roll, a paired tap must roll,
and unselected players must remain unaffected. The previous implementation
failed the first secondary-player short-tap case. The fixture's separate
`--held-control` positive control passed all 24 cases before the correction.
The rebuilt full suite passes 66/66 checks in 305.23 seconds; all 24 held-control
cases also pass after the correction. The log is preserved in
`validation/multiplayer-regression-validation.txt`. The desktop executable was
rebuilt; the packaged candidate remains unchanged.
These are native strategy tests, not a physical five-gamepad playthrough.

## Limits and next checks

The event latch still does not queue multiple presses of the same button inside
one simulation tick. Two taps compressed into a single pending press bit remain
an open case. Primary and EX secondary/multitap shoulder strategies are covered. Analog
axis events, fixed remapping-menu navigation, and touch taps are not addressed
by this change. Existing held-state sampling for those paths is unchanged.

The original timing replay samples scripted input at each simulated raster and
checks source movement/roll/cadence across host FPS schedules. It remains useful
for pacing regression, but does not replace event-path or held-transition tests.
No rendering, interpolation, simulation-frequency or input-consumption gate was
changed in either correction. These checks do not prove independent player-control
cadence or complete responsiveness under all input/event combinations.
