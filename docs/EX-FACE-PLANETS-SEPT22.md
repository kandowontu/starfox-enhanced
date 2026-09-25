# Face-planet enhancement

EX preview 18, Original/EX BG_SPECIAL, and EX BG_6_3H now select a dedicated
high-resolution face-moon atlas. The generated stone surface is recolored by
each authored body's live CGRAM ramp. It retains the source positions and
sizes; saucers and the surrounding starfield remain native. Asset provenance
and the generation prompt are in `assets/enhanced-backdrops/face-moon-v1.md`.

Each enhanced circle follows its source center without the cartridge's
per-row/column shear. The first gameplay capture exposed remnants of the old
sheared faces beside the circles, despite the static preview matching exactly.
Consequently the replacement also removes the original face rectangles at
the tile-sampling stage, using exact source coordinates on CPU and GPU.
The new `suppress_every_copy` region flag is only selected for these faces
when Enhanced Sky is on; saucers retain the existing uniqueness rules.

## Evidence and limits

- Initial static preview: `tmp/ex-faces-enhanced-{gpu,cpu}-sep22` choice 18,
  32:9; exact pixel equality. These predate the source-removal correction.
- `tmp/ex-faces-gameplay-sep22` verifies the new atlas is selected in
  EX LEVEL_SPECIAL. LEVEL6_3 at tick 1000 still uses BG 375 (meteor field),
  so that sample does **not** establish BG_6_3H visual acceptance.
- The intermediate captures in `tmp/ex-faces-gameplay-final-sep22` and
  `tmp/original-faces-gameplay-final-sep22` still contain old distorted
  fragments. Despite their directory names, they are not accepted finals.
- Windows backdrop/image tests and the expanded CPU/GPU effects suite pass.
  The effects fixture covers projection mode 5 under roll, scroll, fades,
  styles and protected layers. The tile parity fixture additionally includes
  removal inside the native window, not just widescreen side copies.

The Linux Ninja duplicate-rule error was caused by recording the shared
region include through both canonical and `../../../` dependency paths.
CMake now normalizes each effects-source dependency before registering it;
the source fingerprint order/content is unchanged. A subsequent Linux app
build and both focused image tests passed before the exact source-removal
follow-up. Latest final capture/build results should be recorded below.

Other celestial replacements, Game Over enhanced art, all-phase palette
acceptance and physical VR validation remain outside this proof.

## Accepted source-clean captures

The final exact-source-removal build passes on Windows and Linux. Inspected
32:9 captures at tick 1000:

- `tmp/ex-faces-source-clean-gpu-sep22/EX-LEVEL_SPECIAL-1000-32_9-final.bmp`
- `tmp/original-faces-source-clean-gpu-sep22/ORIGINAL-LEVEL_SPECIAL-1000-32_9-final.bmp`
- `tmp/ex-faces-menu-source-clean-sep22/choice-18-final.bmp`

Old stretched face remnants are absent, while saucers/stars, gameplay and
menu text remain. The matching EX software capture in
`tmp/ex-faces-source-clean-cpu-sep22` differs at zero pixels (maximum RGB
delta zero). The new tile-removal flag passes `starfox_gpu_background_check`:
432 cases and 183,997,440 packed pixel/coverage samples, including the
new all-occurrence removal inside the source window.

The shared region dependency now configures cleanly under Linux. The final
incremental Linux app and tile-check targets both built successfully.
BG_6_3H remains wired but needs a capture of its actual later scene; the
tick-1000 meteor sample is not that evidence. No release or headset install
was performed.

Later-route follow-up: EX LEVEL6_3 tick 6000 reaches background 381/BG_6_3H.
The inspected 16:9 capture in `tmp/ex-face-late-route-sep22` selects projection
mode 5 and shows the detailed purple/blue faces, stars and saucer intact,
without the former distorted remnants. This supersedes the missing-phase
note above for this sampled late gameplay state, not every transition frame.
The matching software capture in `tmp/ex-face-late-route-cpu-sep22` is
pixel-identical (zero differing pixels, maximum RGB delta zero).
