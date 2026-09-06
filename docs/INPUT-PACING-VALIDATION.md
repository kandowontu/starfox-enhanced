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

## Limits and next checks

This change preserves digital edges; it does not manufacture held samples or
queue multiple presses of the same button inside one simulation tick. Analog
axis events, fixed remapping-menu navigation, and touch taps are not addressed
by this change. Existing held-state sampling for those paths is unchanged.

Original PSTRATS.ASM's barrel-roll block uses current/previous held shoulder bits.
Thus preserving a complete tap's press edge alone does not prove that tap can
trigger a roll. The source input bridge and short-tap roll behavior still need
validation against the enhanced-input requirement. Do not describe this fix as
complete barrel-roll responsiveness or independent player-control cadence.

The existing timing replay samples scripted input at each simulated raster and
checks source movement/roll/cadence across host FPS schedules. It remains useful
for pacing regression, but does not replace event-path or held-transition tests.
No rendering, interpolation, simulation-frequency or input-consumption gate was
changed in this correction.
