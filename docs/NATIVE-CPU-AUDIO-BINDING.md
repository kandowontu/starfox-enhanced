# Native CPU/APU clock binding

`Wdc65816::set_apu_bus_callback` (also exposed by MapVm) observes $2140-$2143
at the native bus access time. Writes arrive after their complete bus wait;
reads arrive at the data sample point before the final four master clocks.
The callback receives the raster's elapsed master-clock timestamp, port, and
an optional write value. For ordinary reads it supplies the SPC output byte.

The decoded IPL upload protocol retains its boot acknowledgement and token
echoes. The callback still receives accesses during uploads so the sound
processor's timeline advances and its uploaded driver can start at the right
time. Once the source clears the upload ports, reads return to the live driver.
No separate IPL ROM is introduced.

A native CPU timeline is required before binding. Replacing the callback during
CPU execution is rejected, as is replacing/detaching its timeline while the
callback remains installed. The caller must remove the callback before
destroying the referenced audio adapter. Queued APU writes remain available
for diagnostics; a live audio consumer must not replay those writes a second
time through the legacy output path.

## Clock adapter

`NativeAudioClock` converts master-clock timestamps to SPC clocks using an
explicit positive rational ratio no greater than one. It retains fractional
remainders between calls, and splits quotient/remainder arithmetic to avoid
multiplying an absolute timestamp by the frequency. A configurable initial
master-clock origin supports a later binding. Backward timestamps, invalid
ports/ratios and elapsed-clock overflow are rejected.

The adapter's `access` method is suitable for the APU bus callback. The host
must also call `advance_to` at execution-chunk boundaries, including waits
without sound-port traffic. Audio therefore progresses independently of
whether a whole MAIN iteration has completed. Completed 51,200-SPC-clock
packets invoke a callback with their elapsed SPC clock and both PCM stems.
Packet callbacks cannot reenter clock advancement. Audio must be primed and
at a packet boundary before creating the adapter, and the caller must not
advance that processor separately while the adapter owns its time.

The adapter intentionally does not choose the oscillators or playback sample
rate. The tests use the pinned ares NTSC CPU frequency (236250000/11 Hz) and
its 32040*32 SPC clocks/second, giving a ratio of 11278080/236250000. A desktop
using that profile must present PCM at 32,040 frames/second or resample it;
the legacy renderer's nominal 32,000 Hz is not interchangeable with that
profile. MSU rendering must use the same selected output timeline.

## Validation

### Continuous sound across scene ownership changes

Keep one `NativeAudioClock` alive across CPU scene lifetimes. After advancing
the outgoing CPU to its final timestamp, `rebase_master_clock(new_origin)`
changes the CPU anchor without resetting SPC time, the oscillator remainder,
or the partially filled PCM packet. It does not itself advance either device.

While a host frontend owns execution, `advance_spc_to` and `access_spc` accept
absolute timestamps in that same continuing sound timeline. They do not move
the detached CPU anchor. A frontend tick starting at SPC time S can deliver
its writes at S plus their offsets, then advance to S + 51200. Packet callbacks
still fire at the original packet boundaries, including inside the frontend
tick. MSU events must use those same SPC timestamps. Rebase the CPU anchor
again when entering a new native scene; do not reconstruct the sound clock or
call the legacy whole-frame renderer on the partial packet.

Both cartridges now replay 100 native/frontend switches against a continuous
SPC-domain reference. They compare every emitted music/effects sample and
exposed sound-driver state after each switch. The MSU fixture variant also
restarts, loops and stops a recording across the switches. The replay checks
fractional-clock retention, callback reentry guards and backward timestamp
rejection. The rebuilt desktop and native-audio target compile successfully;
all three native audio/MSU tests pass in 2.92 seconds. This change is a sound
lifetime API and is not yet wired to desktop scene scheduling.

### Earlier CPU bus validation

- A small native CPU program observes an APU write at master clock 46 and a
  read at clock 72, and stores the callback's returned value in WRAM. It also
  checks IPL acknowledgement/echo/driver handoff and binding lifetime guards.
- A fractional 3/7 ratio produces the same SPC clocks and packet count across
  large and fragmented advances, including a nonzero origin. A near-uint64
  origin is handled without absolute-time multiplication overflow.
- Each port runs twelve complete MAIN iterations with the uploaded SPC driver
  connected. 4096-clock and large CPU chunks agree on elapsed CPU/SPC clocks,
  port access counts, PCM checksums, exposed sound-driver state, GAMEFRAME and
  MAPPTR. The replay requires a stage upload, reads, writes and output packets.

The complete desktop rebuild succeeds and all 89 tests pass in 218.18 seconds;
see `validation/native-cpu-audio-regressions.txt`, including the existing
standard/MSU ending-audio and multiplayer regressions.

This is not an independent full-system/campaign comparison. Timestamped MSU
servicing is implemented in `NATIVE-MSU-AUDIO.md`. Desktop packet output and
sample-rate configuration, scene/pace handoffs, interpolation and ACCURATE
selection/default remain unfinished.
