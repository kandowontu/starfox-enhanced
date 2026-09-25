# Titania weather and enhanced terrain

## EX menu background 26

Choice 26 now uses the cloud-only photographic asset and its live bank-0
shade ramp. The EX affine photograph response is reset to identity for this
asset, because applying it after the live ramp would double-apply palette
changes. Its source atlas switches to flat snow at row 360 (index 30), rather
than the generic origin+128 row 368. The explicit horizon preserves row 360.

Fresh 32:9 reference-phase captures: `tmp/ex-snowcloud-source-sep20`,
`tmp/ex-snowcloud-final-gpu-sep20`, `tmp/ex-snowcloud-final-cpu-sep20`.
Final GPU image visually inspected. CPU/GPU images match exactly, including
when Enhanced Ground is deliberately enabled on the CPU run (suppressed in
the EX menu). Every pixel from screen row 160 downward matches native output.
Focused mapping tests and current Windows build pass. This menu capture does
not prove every EX gameplay weather event or physical VR presentation.

## Natural event, not a forced palette

`tools/capture_titania_weather.ps1` starts Original LEVEL2_3 at tick 2600,
holds Right for 60 presentation frames, then Down for three. It reaches
x=475/y=-109, inside the source TENKI_ON trigger at x=500/y=-100. No map,
object, palette or EBYTE3 writes are used. God mode only protects the replay.

The source MAP2_3A `.fogout` sets FADEPAL=33, disables INFOG and calls
BG_1_4B_1, selecting red depth lighting. Captured CGRAM changes 32 entries;
the native game already renders the gold/brown weather correctly. The checker
requires both exact sky and ground endpoints, not merely any changed pixels.
At 30-frame sampling the first complete gold/brown endpoint is frame 840.

## Implemented

- Dedicated cloud-only photographic panorama (resource 212), without mountains
  or ground baked into it. Original BG_2_3A and the matching EX gameplay atlas
  now share the subject. EX uses its own live palette response rather than the
  Original-specific 15-shade fog ramp; EX Auto ground is snow, not water.
- The live 15-shade cloud palette drives a continuous photographic tone ramp.
  The fog deliberately merges several original shades into one color, then
  separates them into gold/red/dark shades. A simple brightness/tint fit cannot
  represent that behavior. The ramp reads current CGRAM without its own timer.
- CPU, portable GPU and DXR reflected backdrops use the same ramp; the image
  remains resident while the tiny color table changes.
- Titania's icy-blue far ground is classified as snow, not water. The same
  indices become dirt when the live palette turns brown.
- Snow, sand and dirt now use the same 512-unit patches as grassy hills, with
  25 vertices/32 triangles and one cache entry across distance buckets. The
  continuous world height function is unchanged; relief is sampled every 128
  units rather than every 32. Fine surface detail remains shader-based.

## Checks and evidence

- Current Windows build, palette/terrain unit tests and asset decode pass.
- Tests cover authored cloud endpoints, merged fog shades, disabled ramp,
  snow-to-dirt classification, patch dimensions, seams and coordinate wrap.
- GPU-effects parity and hardware DXR checks pass, including shade-ramp updates
  without image upload. Natural stage-specific mirror capture is still pending.
- `tmp/titania-weather-fixture-sep20`: native/procedural baseline replays.
- `tmp/titania-weather-final-{gpu,cpu}-sep20/sky`: photographic sky with native
  ground. Final images differ at 5 pixels, maximum one channel value. Both
  palette endpoint checks pass. GPU image visually inspected.
- `tmp/titania-weather-before-gpu-sep20`: photographic fog plus snow before
  the event (before the patch-size optimization).
- `tmp/titania-weather-large-patches-sep20/sky-ground`: optimized final replay;
  final dirt/cloud render visually inspected and both palette endpoints pass.
  Draw patches fell from 530 to 45 (16,960 to 1,440 triangles) at its final frame.
- `tmp/titania-weather-large-patches-cpu-sep20/sky-ground`: matching software
  replay reaches both endpoints; final output differs at 7 pixels by at most
  one channel value. `tmp/titania-weather-proof-sep20` continues the same input
  replay to frame 2200 for an unobstructed post-weather screenshot, inspected.
- The 1100-frame replay took 22.70 seconds including preroll, application startup
  and 37 PPU/image snapshots. The earlier tiny-patch process took about 134
  seconds and exceeded its observation timeout; it was observed to completion,
  not restarted. These are diagnostic wall times, not controlled gameplay FPS:
  initial overlap with other captures and capture I/O limit direct comparison.

No release or VR deployment is claimed. Other backdrops and the broader goal
remain open. Asset and full generation prompt: `assets/enhanced-backdrops/titania-clouds-v1.md`.
