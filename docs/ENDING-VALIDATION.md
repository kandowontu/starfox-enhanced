# Ending regression pass — 2026-09-04

## Source defects addressed

- Native object dispatch must enter with both X and Y identifying the object.
  `PSHIPOUTOFLB3_ISTRAT` stores Y in `VIEWTOOBJ`; zero Y caused the long flight
  and world-coordinate-wrap delay instead of targeting the escaping ship.
- A suspended `END_GAME_SEQ` task already executes `GETVIEW_L`. Calling the
  host camera's bounded native helpers afterward destroys that task's CPU
  register/stack state. Only import presentation state at the yield boundary.
- Preserve the source's countdown through zero; do not prematurely force
  C_TYPE to 201. Continue transfers after the final score has been created.
- Restore `IRQ.ASM`'s ending-specific `SEQSCROLL` at raster cadence and
  `SEQTEXT` at completed bitmap-upload cadence. Preserve signed scroll values,
  exact per-axis speeds, and tile palette/priority bits.
- Boss dossiers load CGRAM directly. Do not overwrite them from the previous
  gameplay `PAL0PALETTE`. Restore BG2-high/BG3-high foreground ordering over
  the shifted BG1 model, and clip the bitmap to its source dossier canvas.
  Do not repeat dossier artwork into widescreen margins.
- Honor `SETBG2VOFS`'s requested scroll override. Keep the finished score
  screen in the active widescreen presentation path rather than exposing
  unpainted color-zero margins.

## Automated coverage

`tests/ending_tests.cpp` starts at the actual `FINALMAP_END` map continuation
after Andross. Earlier scores and defeated-boss history are fixtures; it is
not a playthrough of the preceding levels or the Andross fight itself.

Both retail and EX cover unlocked/20 Hz and Original/120 Hz settings. The
second case excludes score marker 101 from the average; EX also uses the
additional course-5 boss list.

Assertions cover escape target and bounded time, Pepper radio command/camera
states, all seven stage cards at 30-transfer intervals, total/average values
and waits, all six recorded bosses and their source countdown/wipe intervals,
staff-credit strings, and final-score animation/input. IRQ unit checks cover
all six scroll registers and one-glyph-per-upload typing. Palette checks use
the appropriate native dossier palette for the tested route.

Latest full suite: **35/35 passed** (375.64 seconds), including both ending
tests, both simulation-data tests, both transition-parity tests, desktop and
UWP-configured input, both hit-list tests, all clear-routine families and bounded audio playback.
Log: `build/current/Testing/Temporary/LastFullTest-20260904.log`.
See `LEVEL-CLEAR-VALIDATION.md` for the per-level clear/tally/warp audit.

## Visual checks and limits

Headless runtime captures were inspected for retail and EX boss models,
scroll-in descriptions, palette changes, wipes and final-score presentation.
The initially shown white/striped boss dossier was visibly wrong despite
passing progression tests; the fixes above were made after reviewing it.

Reproducible captures use `STARFOX_TEST_FRAMES` together with
`STARFOX_TEST_ENDING=1`. Optional `STARFOX_TEST_ENDING_PREROLL` advances the
same source sequence before capture; it does not inject a boss-roll state.
`STARFOX_CAPTURE_INTERVAL` avoids saving thousands of duplicate images.
These are development-only environment controls, not player-facing options.

The final-score capture was rechecked after the hit-list changes: THE END,
total and average appear and animate without an endless escape loop. Do not
infer a missing green horizon from memory: the inspected source BG_CRED
definition loads 24.CCR/PCR (nebula/star artwork). A different reference scene
needs a source/version-matched comparison before changing that background.

This is not a claim of exhaustive pixel-by-pixel SNES/video parity, a complete
normal playthrough, or an audible comparison of every ending music/voice cue.
The other active hit-list reports remain separately tracked.
