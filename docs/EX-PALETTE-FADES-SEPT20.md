# EX 5-1 / 6-1 / 7-1 palette response

The source game remains responsible for palette timing and colors. Enhanced
backdrops read the current CGRAM every presented frame, without their own fade
clock or extra color phases. Original tile rendering is unchanged.

## Corrections

- Replaced per-channel palette ratios with a uniform brightness multiplier and
  signed RGB offsets. A small red change in a blue source no longer produces a
  several-fold red amplification. Strong orange transitions no longer depend
  on multiplying the photograph's relatively weak red channel.
- The transform maps the source band's mean exactly to the live band's mean;
  it preserves photographic detail, identity, uniform fades and black endpoints.
  Sky and surface remain separate. This is photographic palette adaptation,
  not a claim of reproducing each original palette-index animation exactly.
- Calibrated the shared distant-snow artwork against its blue 5-1 palette.
  6-1 has a different, gray starting palette: using that as the blue artwork's
  neutral reference incorrectly made its later orange phase purple.
- Updated CPU, GPU and DXR reflection consumers together. Portable effects
  shaders were regenerated. No additional texture fetch/pass or per-pixel
  palette classification was added.

## Evidence

`tmp/ex-palette-corrected-sep20` contains source snapshots at ticks
600/1000/1200/1400/1600/1800/2000 for all three routes. Its rendered images are
an intermediate multiplicative implementation, **not** the accepted result.
Snapshots confirm multiple phases in 5-1/6-1. 7-1's sampled sky entry 8 remains
RGB5 (9,16,25) through tick 1500, then is (26,15,2) at 1600, 1800 and 2000.
There is no enhanced timer that could repeat this single authored transition.

Final 5-1/7-1 captures: `tmp/ex-palette-affine-gpu-sep20`, at ticks
1000/1200/1500/1600/2000. Their tick-1600 software counterparts are in
`tmp/ex-palette-affine-cpu-sep20` and match pixel-for-pixel. Visually inspected
5-1's orange phase and 7-1's blue/orange endpoints.

Final 6-1 captures: `tmp/ex-palette-calibrated-gpu-sep20` (600/1200/1600/2000)
and `tmp/ex-palette-calibrated-cpu-sep20` (1600). Earlier affine 6-1 images
predate the shared-artwork calibration and are rejected. Visually inspected
the corrected starting-phase sample and orange phase.
The corrected 6-1 software/GPU comparison differs at one pixel by one channel
value (rounding).

Current Windows build and focused terrain/palette tests pass, including actual
5-1/7-1 palette fixtures, 6-1 calibration, initially zero channels, black fades,
and unchanged-palette stability. GPU-effects checks pass; hardware DXR checks
include both brightness and signed-color updates without image re-upload.

This is not acceptance of all other backgrounds, Titania's weather trigger,
all gameplay phases, or VR. Those remain separately tracked.
