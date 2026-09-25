# Start-pause stability and grassy hills

Grass blade geometry is removed at every distance. Grass uses a continuous,
periodic world-height field with irregular broad hills and flat areas. A patch
spans 512 world units, has 25 vertices and 32 triangles, and shares one cached
mesh across distance buckets. Other terrain retains its existing patch size.
Unit checks cover nonrepetitive relief, flat areas, world-wrap continuity,
matching neighboring hill edges and absence of blade geometry.

`tmp/grassy-hills-sep20` contains software/GPU screenshots and timing logs.
The GPU screenshot was inspected. Original LEVEL1_1, native 1x, sky+ground:
median frame work 3.291 ms software / 8.207 ms GPU; previous grass-blade pass
5.004 / 11.176 ms. The new run captures its last frame, so maximum timings
include capture overhead; neither result promises weak-hardware FPS.

Start-pause now bypasses temporal reconstruction/native-raster reduction and
uses an unjittered native frame. It does not change the saved DLSS option.
Pause/resume enters a new temporal context, resetting history on resumption.
This preserves EX pause-debug controls because the scene continues rendering;
it is not a frozen screenshot that ignores input. F1 behavior is unchanged.

Normal DLSS already disabled raster jitter before this change. The initial
jitter-only guard was therefore insufficient; native paused presentation is
the substantive fix. `tmp/start-pause-native-final-sep20` records 26 native
paused frames and 39 evaluated frames across 65 presentations, with a history
reset on resume. Full-frame comparison of paused captures 20..30 is exact.
The first verifier incorrectly compared DLSS evaluation indices with render
serials and reported failure despite exact paused frames. It now accounts for
the skipped evaluations by count; do not treat that earlier failed verifier as
a passed test run. The subsequent accepted fixture is recorded separately.

This validates PC EX/DLSS pause in the tested scene, not every enhancement
combination or VR. No release or headset deployment is included.
