# Out of This Dimension white-fade investigation

Reasserting LEVELFINISHED=16 during the active special-exit transition reset
the forty-update white-fade counter on every host tick. A regression added to
the Original simulation data suite failed before the fix and passed after it.
The exit dispatcher now ignores/clears outgoing exit requests while either
special-exit fade phase owns progression, matching MAIN's accepted-exit loop.

Three targeted tests passed in 34.08 seconds: Original simulation data and
both desktop exit matrices, including Original MSU on/off. The log is in
`validation/white-fade-reentry-regressions.txt`.

This proves the repeated-request failure mode, not that the naturally reached
bird encounter is its trigger. The existing desktop fixture injects the exit
on an ordinary map; a source bird/route replay through LEVEL_SPECIAL and a
visible resumed gameplay frame remain needed before closing the user's report.
The fix is not included in the published v0.0.4.1 tag.
