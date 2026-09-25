# Orbital banded planets — September 22

Replaced the remaining native orange disks in EX preview 25 / BG_5_1I
orbital entry and BG_2_2 asteroid gameplay. Native BG2 captures establish
the entry disk at (360,352), radius 24; the asteroid disk at (361,351),
radius 22. Both are banded planets, not cratered moons.

The existing detailed banded master is registered to its measured disk
bounds and packed beside the existing panorama in one immutable upload.
All eight live source colors (CGRAM 17–24) shade its surface; using only
the pale highlight was rejected after inspection. No per-frame texture
reconstruction or new image-generation asset is involved.

New orbital atlas mode 7 retains the entire lower planet surface. It clamps
panorama coordinates before encoding moon slots, so extreme banking cannot
sample moon artwork as a floor. Each disk appears once; asteroid camera
motion tracks the interpolated source center without shearing the sphere.
Native disk ink is removed underneath, while stars and other layers remain.
The CPU, portable GPU and DXR reflected environment share this behavior.
Enhanced Sky OFF does not enter this path.

## Verification

- Windows and native Linux application/backdrop test builds pass.
- Backdrop unit checks cover lower-surface ownership, extreme-bank atlas
  selection, cached crop changes, eight-color shading and existing moons.
- Portable effects parity passes on Windows and native Linux Vulkan/Lavapipe,
  including mode 7, palette changes, styles, motion, rolls and fades.
- Hardware DXR checks pass, including the new moon palette and lower-surface
  fixtures. Unchanged artwork remains resident during palette-only changes.
- Six final GPU/software image pairs are pixel-identical: preview 25 at 32:9,
  EX 2-2 tick 1000 at 16:9/32:9, and 5-1/6-1/7-1 entry tick 60 at 16:9.
- A 180-presentation, 120 FPS banking capture was inspected: the asteroid
  planet stays round while its horizon banks. This is visual proof, not an
  automated whole-sequence perceptual metric.

Final images:

- `tmp/orbital-moons-final-menu-{gpu,cpu}-sep22/choice-25-final.bmp`
- `tmp/orbital-moons-final-{gpu,cpu}-sep22/EX-LEVEL2_2-1000-*-final.bmp`
- `tmp/orbital-entry-final-{gpu,cpu}-sep22/EX-LEVEL[567]_1-60-16_9-final.bmp`
- `tmp/orbital-moon-banking-sep22/EX-LEVEL2_2-1000-16_9-final.bmp.frame-121.bmp`

Earlier `orbital-moons-menu-gpu-sep22` / `orbital-moons-gpu-sep22` captures
have the rejected pale highlight-only tint; they are not the final artwork.
Entry tick 60 proves lower-surface composition, not visible moon placement;
preview 25 supplies the visible entry-moon proof.

Logs: `tmp/orbital-moons-final-{build,effects,dxr,linux}-sep22.log` and
`tmp/orbital-moons-linux-effects-sep22.log`. No release, CI dispatch or headset
installation was performed. Physical VR and the broader goal remain open.
