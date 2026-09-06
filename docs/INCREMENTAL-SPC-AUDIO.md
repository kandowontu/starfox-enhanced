# Incremental SPC servicing

`Spc700Audio::advance_frame(clock, writes)` advances the existing music and
effects processors within a 51,200-SPC-clock (50 ms) output packet. The clock
and every write offset are relative to the current packet. Calls and writes
must be monotonic, with offsets no later than the requested clock. Invalid
input is rejected before either processor advances.

Output ports are live after each partial call. The renderer runs each SPC to
the requested time rather than waiting for a completed game update. This is
the audio-side interface needed for source CPU/APU handshakes during resumable
native gameplay. It does not yet bind those calls to the native CPU bus.

At clock 51,200 the method publishes exactly 1,600 stereo frames in each stem
and returns true. Until then, the previously published PCM remains available.
The next call starts a new packet. Legacy `render_logic_tick` and constructor
upload priming remain available at packet boundaries; mixing them with an
incomplete packet is rejected.

## Uploads and command routing

The existing decoded IPL protocol works across partial calls and packets.
The old driver runs until its actual restart timestamp, and its live ARAM is
preserved for the bank overlay. Audio remains silent during the upload. Its
execute packet attaches the new driver's output at that packet's sample
position. The sample placement uses the existing 32 kHz output grid.

Restart closes and filters the preceding audio segment before resetting the
driver/filter. DSP look-ahead samples retained internally by `end_frame` are
cleared from the ungenerated portion of the public packet; they must not leak
into the following silent upload interval. Loading at a packet boundary is
equivalent whether the upload is delivered at the old packet's end or at the
new packet's start.

The existing separated music/effects command routing is retained. Global
pause/unpause commands reach both processors; effects cannot replace an MSU
music stem or consume a native music voice.

## Validation and remaining integration

Each cartridge replay checks 100 frames against large and fragmented audio
advances. Ordinary running-driver frames also match the existing timestamped
renderer in both PCM stems and exposed processor state. A queued laser effect
is acknowledged before the packet ends. The tests cover invalid deadlines,
future writes, incomplete-packet legacy rejection, preserved published PCM,
stage uploads spread across packets, silence during incomplete uploads,
resumed playback, and upload ownership at a packet boundary.

The rebuilt desktop and all 87 regression tests pass in 222.29 seconds; see
`validation/incremental-spc-regressions.txt`. This includes the existing
standard/MSU ending-audio and multiplayer suites.

The ordinary legacy upload renderer deliberately decodes an upload and then
plays a complete packet; it is not used as an oracle for the new timestamped
upload interval. Upload checks compare scheduling boundaries and output/state
consistency. This is not independent hardware timing certification.

The native master-clock/APU bus bridge, timestamped MSU servicing, desktop
audio output integration, scene/pace handoffs and ACCURATE selection/default
remain unfinished. Existing desktop audio still uses the legacy renderer.
