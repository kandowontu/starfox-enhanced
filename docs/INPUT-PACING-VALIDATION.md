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
90 FPS, variable 47â€“59 FPS and the same variable schedule with 100 ms stalls.
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

## Repeated presses before a pending update

InputLatch now counts each sampled press/release edge per button. A source update
consumes one of each pending edge per button, retaining additional edges for later
updates. Held state always remains the latest physical sample. This fixes repeated
presses collapsing into a single bit across multiple presentations while source
logic is pending; it introduces no additional simulation updates.

A three-tap latch regression failed with the prior implementation and passes with
the edge counts. The primary replay adds 25 ms-spaced pairs at 60/90 presentations
per second, both shoulders, routes 2/3 and both pace modes (16 additional cases per
port). The native EX multiplayer fixture now supplies both taps before either game
update, so all 24 slot/mirror cases exercise pending input as well as native rolls.
The desktop clears pending edges at flow/page/scene/pause transitions while retaining
physical held state, preventing old confirms from replaying on a new screen.

`tools/reference/verify-queued-input-baseline.py` builds these current fixtures with
the `1b4a06e` latch header in a separate include directory. Both primary games lose
one of the 25 ms-spaced presses (one delivered press, no roll), and the secondary
fixture also fails to roll. The current latch passes all three paths. This changes
only the isolated baseline test binaries; the worktree and production libraries
are not replaced.

The full regression run passes 66/66 checks in 348.68 seconds with the counted
latch. The final desktop transition guard and stronger held-reset assertion were
rebuilt afterward; all four affected input/embedded-launch checks pass in 30.98
seconds. The two runs are archived together in
`validation/queued-input-regression-validation.txt`. The packaged candidate has
not been refreshed.

## Multiple complete taps in one presentation

DigitalInputEvents records complete taps within each SDL event batch before the
latch samples held state. Per-action down depth combines overlapping bindings
within the batch into one held interval. The latch retains every completed tap
for an action that was previously up, including taps followed by a final held
press. Previously held actions keep their existing held-sampling behavior.
The desktop uses the collector for the primary and four secondary slots.

The SDL virtual-gamepad tests submit two complete shoulder taps before polling
any events. Feeding the old pressed/released masks to the latch yields only one
press; feeding the counted batch yields both, once each. Tests cover both
shoulders, overlapping bindings, a final held interval and an already-held action.
Desktop and UWP variants use the same virtual-device checks.

Primary short-tap replays now use the event collector and include the 25 ms pairs
under variable 47–59 FPS and 100 ms stalls, adding 32 closely spaced pairs per port
to the existing 64 scenarios. All 192 primary scenarios pass, alongside the 56
one-minute pace replays. All 24 native EX multiplayer cases now feed both taps in
one batch. The old-latch baseline script adapts the new event-batch overload back
to the old mask API so its historical latch remains testable.

The final rebuilt suite passes 66/66 checks in 330.93 seconds. The complete log
and repeated old-latch rejection evidence are preserved in
`validation/event-batch-regression-validation.txt`. The desktop executable is
rebuilt; the packaged candidate remains unchanged.

## Source-aware release and repress

The desktop now seeds each event batch from the previous keyboard, gamepad-button
and non-digital held masks. Digital events update only their own source, and action
edges are counted only when the combined held state changes. This retains release
and repress of an already-held action without manufacturing a release when another
binding is still down. The final held sample reconciles axes, touch and filtered
events. Primary and all secondary slots use this path. Controller reconnection
resets both the source masks and queued edges for the newly assigned devices.

SDL virtual-device tests start with a held shoulder, then deliver release, press
and release before one poll. The old mask path loses the new press; the new path
retains one press and both releases. Separate overlap tests keep a keyboard-held
action active through a gamepad tap. Both desktop and UWP input tests pass.

The primary replays add pairs with a 119 ms first hold and a 1 ms release gap,
followed by a 1 ms second tap. All four presentation schedules, both shoulders,
both paces and routes 2/3 pass: 32 more cases per port, bringing the primary total
to 256 cases across both ports. The existing 56 one-minute pacing replays also
pass. EX's native player fixture now checks both complete-tap batches and held
release/repress, for 48 passing slot/mirror cases.

The final rebuilt suite passes 66/66 checks in 377.34 seconds, including the
expanded primary and multiplayer fixtures and the desktop launch checks. The
full log and old-latch rejections are in
`validation/source-input-regression-validation.txt`. Physical controller reconnect
playthroughs are not established by these automated checks. The desktop executable
is rebuilt; the packaged candidate remains unchanged.

## Limits and next checks

The desktop source-aware path covers digital release/repress across samples.
The mask-only compatibility API still cannot reconstruct ordering already lost
by a caller. Counts do not retain timestamped ordering across different buttons
and saturate at UINT32_MAX. Analog/touch transitions remain final-state samples;
a complete analog/touch excursion between polls is not recovered by this change.
Primary and EX secondary/multitap shoulder strategies are covered. Analog
axis events, fixed remapping-menu navigation, and touch taps are not addressed
by this change. Existing held-state sampling for those paths is unchanged.

The original timing replay samples scripted input at each simulated raster and
checks source movement/roll/cadence across host FPS schedules. It remains useful
for pacing regression, but does not replace event-path or held-transition tests.
No rendering, interpolation, simulation-frequency or input-consumption gate was
changed in either correction. These checks do not prove independent player-control
cadence or complete responsiveness under all input/event combinations.
