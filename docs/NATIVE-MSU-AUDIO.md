# Timestamped MSU audio

`MsuAudioTimeline` selects MSU or continuing SPC music on the same SPC-clock
timeline as native audio packets. The caller chooses the output sample rate
(32,040 Hz for the pinned ares clock profile). Volume, track and play/stop
commands take effect at their sample boundary rather than at the start of
an entire 50 ms nominal packet. The adapter retains partial recording samples
and selection flags until the matching native music packet is available.

The native Wdc65816/MapVm MSU bus callback exposes timed $2000-$2007 accesses.
Like the APU binding, it requires a live CPU timeline and prevents timeline
replacement underneath an installed callback. The adapter returns the MSU
identifier and revision/status bits. Audio loading remains synchronous, so
busy bits are clear; MSU data-file streaming remains outside this audio model.

## Wiring

At a native MSU access, first advance `NativeAudioClock` to the supplied master
clock, then pass its elapsed SPC clock to `MsuAudioTimeline::access`. At each
completed SPC packet, `finish_packet` combines the recording with the provided
native music. Advance both clocks at CPU chunk boundaries as well. The native
audio test demonstrates this wiring with both APU and MSU callbacks installed.
Do not replay the diagnostic register-write queues through legacy audio too.

## Credits and playback fixes

The renderer records the first output frame after a non-repeating credits
recording ends. Only that tail uses the already-running SPC music. The older
desktop mixer selected the entire final packet based on the post-render EOF
flag, discarding the recording's final samples. It now uses the tested
`select_music` method to preserve the recording prefix and switch at EOF.
Explicit stop cancels the continuing credits tail; host pause keeps it silent.

The track high-byte write now resolves the selected recording and exposes the
missing-track bit. A failed selection/play stops the previous recording rather
than leaving it playing under the new track number. Repeat/play status follows
the control register. EOF status is updated when the last sample is consumed.

A replacement recording shorter than its configured loop point now loops from
zero. The previous clamp could choose EOF itself, leaving the cursor unchanged
inside an infinite loop. Valid loop points in the supplied pack are unchanged.

## Validation

A synthetic 320-frame, 32 kHz stereo FLAC fixture checks timestamped play,
volume, stop, repeat, pause, missing selection and disabled-MSU behavior. It
checks the exact credits recording/SPC boundary, including the desktop selector,
and compares whole-packet and 97-clock advances. A native CPU program verifies
MSU write/read bus times and reads back actual playback status.

An Original MAIN replay connects the real SPC driver and timestamped MSU
playback together, using the synthetic recording for selected tracks. Small
and large CPU chunks match clocks, traffic, selected track/status, PCM checksums
and exposed source/audio state. This verifies integration, not the duration or
content of the real music pack or entire campaigns.

The complete desktop build succeeds and all 91 tests pass in 237.32 seconds;
see `validation/native-msu-regressions.txt`. That run includes the long
standard and real-pack MSU ending-audio checks, EX orchestra audio, and the
existing input/multiplayer regressions.

The fixture is generated original test audio, not game music. Its generation
command is recorded in `tests/data/msu-tone-320.md`.

The desktop uses the corrected final-packet selector now. Native desktop
packet output/sample-rate setup, scene/pace handoffs, interpolation and the
ACCURATE setting/default still require integration.

For subsequent desktop integration and ACCURATE selection, see
`NATIVE-DESKTOP-SCHEDULER.md`.

## MSU-off boss music during player death

The 0.0.4 report was not reproduced in the current host or native scheduler.
Both cartridges now have regressions that submit encounter command $66, then
invoke PLAYERDEAD_ISTRAT with MSU disabled. They require the source to submit
death command $11 on APU port 0, require the running SPC driver to acknowledge
it, and reject a subsequent $66 command during the tested death tumble.
The native replay writes the source player's strategy pointer at an idle
MAIN boundary and services live audio throughout the following updates.

Five targeted audio tests pass in 7.09 seconds, including the existing MSU
fixture. See `validation/boss-death-audio-regressions.txt`. These are injected
death/track checks, not every naturally reached boss or a listening comparison
against 0.0.4. No production audio fix was needed for these cases.
