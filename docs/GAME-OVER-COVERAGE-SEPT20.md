# Game Over background coverage

## September 22 photographic starfield

Original and EX Game Over now load optional `deep-space-v1.bmp` (resource
234) when Enhanced Sky is on. This is a dedicated generated starfield, not
an unrelated nebula. Both axes use the existing overlapping panorama sampler;
the source aspect ratio controls projection, so stars are not stretched.
Sky style/motion options and display brightness remain active.

The native BG_AND atlas uses index 74 for blank sky; visible Andross ink and
authored stars share other indices, so all of those are protected. Analysis
of the captured source atlas found zero enclosed blank-ink holes in the
character. Blank sky and transparent margins receive the photographic field;
the character is not preserved by a rectangular cutout. The BG2 layer is
classified as scenery only while this enhancement is enabled; foreground
text/objects keep their separate layers. Enhanced Sky OFF is unchanged.

The check script now explicitly disables the saved FPS overlay (its changing
digits otherwise contaminate image comparisons). Enhanced Sky verification
also requires a visible on/off difference and exact preservation of every
nonblack authored pixel. Native rendering still requires exact backend
equality; photographic CPU/GPU bilinear sampling allows one RGB step, with
the count reported, rather than treating a rounding difference as geometry.

Windows/Linux builds and terrain/layer plus real-image decode tests pass.
Inspected EX tick-90 16:9 capture:
`tmp/gameover-photo-tagged-later-sep22/EX-GAMEOVER-90-16_9-final.bmp`.
The intermediate `tmp/gameover-photographic-sep22` capture predates the
scenery-tag correction and is not proof of a visible sky replacement.
Final cross-backend/SBS results are recorded below when complete.

Asset, full generation prompt and provenance:
`assets/enhanced-backdrops/deep-space-v1.md`. No VR acceptance is claimed.

Accepted final fixture: `tmp/gameover-photo-accepted-sep22`, Original and EX,
32:9 at 1x. GPU, CPU late-star path, forced fallback and stereo fallback
match exactly; full software differs at 14 pixels by one RGB step. Both
experiences change 81,191 blank pixels with Enhanced Sky enabled, while
every nonblack source character, text and star pixel remains exact. The
known 4:3 center mapping is unchanged; Half/Full SBS dimensions and starfield
coverage in both eyes pass. EX's GPU final and the separate later tick-90
capture were visually inspected. These supersede the historical statement
below that Game Over only has native extended stars.

The native BG_AND atlas contains both its starfield and Andross. The fresh
atlas in `tmp/game-over-atlas-sep20` was inspected: the central character
must not be erased by a whole-atlas photographic replacement. Its blank
lower-left atlas quadrant must not be repeated into the displayed sky.
Existing star-only expansion retains Andross once and fills the margins
with actual star pixels, not a black side border.

## Stronger checks

`check_game_over_gpu.ps1` now distinguishes the existing CPU **late-star
fallback** from a full software-renderer run. The latter explicitly selects
SOFTWARE and rejects native-GPU rendering traces. Both are compared with
the GPU, forced fallback, and stereo-fallback results.

It also captures a 4:3 reference and compares every source pixel of its
central canvas against widescreen. The comparison accounts for the known
4:3 presentation stretch (256x224 is presented as 299x224 at 1x), sampling
source-pixel centers. It does not search for an offset or best fit.

`capture_background_audit.ps1` can now inspect GAMEOVER, INTROMAP and
TITLEMAP explicitly, including their atlas/PPU captures. The default
playable-stage sweep is unchanged. GAMEOVER is recognized as the app's
synthetic entry, rather than incorrectly requiring a ROM symbol.

## Current proof

- `tmp/game-over-full-software-sep20`: Original and EX at 32:9, 1x,
  Enhanced Sky on. Full software/GPU/late-star/forced-fallback images match
  exactly; Half/Full SBS sizes and star pixels in both eyes' margins pass.
- `tmp/game-over-later-full-software-sep20`: Original and EX at 16:9, 2x,
  preroll 90. The same backend/stereo checks pass. Both final images were
  inspected: one Andross, intact GAME OVER text, and stars across the frame.
- `tmp/game-over-native-center-final-sep20`: the new exact 4:3-center
  comparison passes for Original and EX at 32:9, together with the
  full-backend and stereo checks.

These are desktop rendered captures, not physical headset evidence. They
verify the native starfield when Enhanced Sky is enabled; they do not claim
a newly generated photographic Game Over texture or completion of every
other background.
