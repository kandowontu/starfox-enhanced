# Distinct blue celestial backgrounds

EX preview 27 now has a violet storm-cloud planet; preview 31 has a
blue-green diagonally banded planet. They retain their different native
diameters and atlas positions. Preview 19/28 retains its orange cratered
planet. Unrelated moons and abstract/face planets do not use these assets.

## Assets and source identity

Created with the built-in image-generation tool; PNG masters, lossless
24-bit BMP resources, and full generation/edit prompts are stored at:

- `assets/enhanced-backdrops/storm-planet-v1.{png,bmp,md}` — resource 224.
- `assets/enhanced-backdrops/banded-planet-v1.{png,bmp,md}` — resource 225.

The banded image was edited once to correct its stripe direction without
mirroring its lighting. Both selected images were visually inspected.

The original EX `GSTRATS2.ASM` maps menu 27 to `bg32` and menu 31 to
`bgdemo`. `BGS.ASM` maps those atlases to BG_3_2 and BG_INTRO respectively.
The implementation enables the same registered assets there, with each
body's own source palette bank controlling EX palette changes. Original's
intro uses BG_INTRO; EX's narrative intro uses BG_CRED instead and is not
silently replaced with this planet.

`celestial_scroll.hpp` holds measured source/image bounds. The common
single-object/affine rendering paths preserve existing planet count, source
motion and foreground occlusion. Gameplay uses the fitted fractional
row/column transform, not menu offsets. Repeated native copies in wide
BG_3_2/BG_INTRO margins are replaced with same-row starfield samples.
This adds cached artwork, not a new render pass or per-frame image upload.

## Verification

- Windows and Linux application builds pass; all three celestial assets
  pass the real-image decode, registration and scroll unit tests.
- `tmp/celestial-menus-{gpu,cpu}-sep20`: four enhanced 32:9 previews at
  matched source offsets. Inspected 27/31: one correctly sized planet each,
  stars across both margins, unchanged menu text. GPU/software differences:
  choice 27 has two pixels differing by one channel level; 19/28/31 are exact.
- Against the native matched captures, 27 changes 6900 pixels and 31 changes
  4033. Zero changes occur outside their respective planet footprints.
- Fresh native-only captures in `tmp/celestial-native-menus-sep20` match
  the independent original-ROM references at fixed source offsets: zero
  pixels exceed two channel levels for either choice. The deliberate
  wrong-Y controls differ, confirming the comparison is offset-sensitive.
- `tmp/celestial-menus-motion120-sep20`: both 27/31 pass
  `check_ex_menu_scroll.py`, with 42 uniform fractional steps at 120 FPS.
- `tmp/banded-intro-original-250-sep20` and its `-cpu-`/`-native-` companions:
  visually inspected Original intro at preroll 250. The banded planet is
  upgraded at the native location beneath the carrier. GPU/software differ
  at 10 pixels by at most one channel level; all 16165 enhancement changes
  lie in the planet's bounding rectangle (720,298)..(863,443) at 2x scale.

The sampled BG_3_2 gameplay frame currently has its planet outside the view,
so it is not visual proof of the enhanced storm planet in gameplay. Wider
gameplay phases, remaining unique bodies, Game Over photographic treatment,
and physical VR are still open. Later intro samples that reached the title
screen are not counted as planet evidence.
