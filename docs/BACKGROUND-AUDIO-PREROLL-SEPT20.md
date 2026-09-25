# Audio-aware background prerolls

`capture_background_audit.ps1 -AudioPreroll` now advances the real SPC/MSU
instance alongside source simulation, with output discarded. It runs after
the ordinary boot-upload initialization and retains that same audio state
when presentation begins. Test-only guards keep production startup unchanged.
The option currently requires ORIGINAL timing: one 50 ms audio frame per
source tick. Historical fixtures retain their previous behavior by default.

Windows application build passes. Fresh EX LEVEL3_4 captures in
`tmp/celestial-audio-preroll-sep20` now reach different map positions at ticks
300 and 700 (`7b1f1` / `7b547`, waits `b71` / `172`), rather than silently
capturing the same stalled source sequence. The tick-700 final was inspected:
one enhanced cratered planet, stars in the wide margins, and foreground
geometry occluding the planet. This fixes an audit limitation, not a claim
that all remaining backgrounds or gameplay phases have passed acceptance.

EX LEVEL3_2 ticks 100/300/700 also complete in
`tmp/storm-audio-preroll-sep20`. The tick-300 image was inspected, but the
storm planet remains outside the visible source window at that phase; this
is not counted as visual acceptance of that planet's gameplay replacement.
