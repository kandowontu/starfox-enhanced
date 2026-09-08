# Revival and rendering follow-up

## Source comparison and fixes

Reviewed GSTRATS.ASM's death hold/fade/life branch, TRANS.ASM's restart/background
request order, DSTRATS.ASM's RESTART_L, WINDOWS.ASM's INITBLACK/SETBLACK and
SPRITES.ASM's DO_STAGE against the host restart and presentation paths.

- Removed the cached boss-track replay: checkpoint background setup owns SPC
  and MSU selection, rather than the encounter where the player died.
- Removed the pre-death palette override: native checkpoint background/info
  setup is authoritative. An artificial stale palette regression now compares
  the restored output against those native routines rather than the old override.
- Compose native blackfade's CGADSUB mask instead of clearing the whole frame.
  Its BG1–3/backdrop subtraction leaves OBJ stage text visible. The 50-update
  stage countdown, native blink pattern, black hold and decreasing fixed-colour
  subtraction remain cartridge-owned. The display-brightness fade is separate.
- Increment scene revision on checkpoint rebuild to invalidate interpolation
  and presentation caches across reused object slots. Clear the prior encounter
  marker so it cannot affect post-boss dialogue pacing in the restarted stage.
- SETSTAGE map opcode now writes native STAGECNT, not only a diagnostic member.

Tests exercise the full death tumble/circle, life decrement, checkpoint rebuild,
control recovery, palette/music selection, black hold/reveal, announcement expiry
and the next zero-life death into Game Over. Original and EX are covered; this is
not a claim that every checkpoint on every route has been played through.
The desktop headless revival capture visibly shows STAGE-1 on black, its blink-off
frame, the partial reveal and restored normal stage colours. The first visual
fixture incorrectly reinjected death each rendered frame; correcting that test
hook to run once resolved the diagnostic mismatch without a gameplay workaround.

## Other requested rendering changes

- Tunnel HDMA scenery keeps native wrapping in the central cartridge raster,
  but added widescreen margins no longer repeat additional texture copies.
  Uncovered borders retain the solid backdrop colour; geometry/HUD stay visible.
  Tests cover wide widths and 1x/2x/4x scaling with an unchanged native centre.
- Separate 2D/3D Bloom sources and saved strengths. Old combined preferences
  migrate to both. Textured polygons remain 3D Bloom sources, not HUD or scenery.
- Verified 2D filtering on actual in-game background captures: xBRZ versus Off
  changed 10,568 output pixels at 1x and 55,485 at 4x in the tested Corneria frame,
  including the scenery region. These are visual-difference counts, not quality
  or performance scores.
- Extended filter eligibility to textured polygon coverage. Real Andross
  textures now change under xBRZ while untouched solid geometry/background pixels
  stay unchanged. This is presentation-space artwork filtering, not replacement
  high-resolution texture assets.
- Added independent 3D Smoothing for internal model colour boundaries. It mixes
  only model-covered samples, keeps HUD/world pixels outside the kernel, preserves
  alpha and offers four strengths with Off as default. Pixel tests cover strengths,
  scale and serial/threaded equality. Anti-Aliasing remains the silhouette control.
