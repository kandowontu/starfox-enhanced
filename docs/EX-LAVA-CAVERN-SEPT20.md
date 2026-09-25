# Enhanced volcanic cavern

**Superseded September 22:** the user clarified this is open space over a
sun-bright comet, not a cavern. Resource 222 now uses `comet-corona-v1.bmp`.
See `BACKDROP-CORRECTIONS-SEPT22.md`; the notes below are historical evidence
for the rejected subject, not current visual acceptance.

EX preview 36 and BG_COMET now use lava-cavern-v1 (resource 222), generated
with the built-in image tool. Master PNG, lossless 24-bit BMP, and complete
generation prompt are in `assets/enhanced-backdrops/lava-cavern-v1.*`.
The artwork replaces the distant ceiling and walls, not the animated lava.

Menu source atlas inspection places lava at row 440. Its environment origin
is now 312 instead of 328, with an explicit 440 boundary. Gameplay retains
its separate 360 boundary and actual center-column vertical scroll. Palette
indices 57..60 preserve the authored animated flame strokes over the image;
the cavern photograph follows wall bank-4 inks 71..78 instead of inheriting
the animated molten-surface colors. Enhanced Ground remains disabled in the
EX pre-game menu. No extra per-frame upload or shader pass was introduced.

## Verification

- Windows and Linux application builds pass, including embedded resource 222.
- Windows and Linux terrain mapping and new image decode tests pass.
- Inspected 32:9 enhanced menu and Comet gameplay finals in
  `tmp/lava-cavern-final-gpu-sep20` and `tmp/comet-final-gpu-sep20`.
- Matching software captures: 20 menu pixels and 7 gameplay pixels differ,
  with maximum channel difference 1 (rounding only).
- Against `tmp/comet-native-sep20`, all gameplay pixels at/below row 128
  are identical, including molten surface and HUD.

This verifies these sampled scenes, not all camera angles, full-stage
playthrough, performance on weak hardware, or physical VR acceptance.
