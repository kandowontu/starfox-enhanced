# Parity repairs and presentation options

These working-tree changes address the confirmed findings in the original audit.
They are not a certification of whole-game 1:1 parity or an on-device release test.

## Implemented

- Original QFADEDOWN now decreases brightness by two: 11, 9, 7, 5, 3, 1, 0.
- Vertex decoding retains byte/word encoding for each expanded vertex and frame.
  Word vertices bypass the byte-coordinate shift and big-head multiplier. Tests
  cover BOXXIE, CORNFRIEND, MARIOHFIRE, LUIGIHFIRE, LUIGIHSBW, BLACKROSE,
  LUIGIH and MARIOH, plus synthetic geometry with multiple scale values.
- Natural, non-looping staff-roll MSU completion releases music selection back
  to the still-running SPC engine. Explicit stop/pause and unrelated one-shot
  completion do not enable that fallback.
- Missing or invalid replacement tracks stop old playback. Out-of-range loop
  markers fall back to frame zero, ensuring shortened tracks make progress.
- Removed automatic reclamation of unlinked native slots. Incomplete native
  lists now fail validation before replacing host lists. The existing full-pool
  escape test and a deliberately corrupted-list regression pass. The original
  platform-specific failure that motivated reclamation has not been reproduced;
  its root cause must not be described as fixed.
- ENEMY uses the actual boss-meter maximum and the same wide-screen/custom-HUD
  offsets as its bar. Regression checks cover maximum health values 1–255.
- OPTIONS → EFFECTS: OFF, CEL-DRAWN, INK, NEON, MONOCHROME. The setting is saved;
  old configuration files default to OFF. Styles preserve tagged 2D HUD pixels.
- Main-page PREVIEW defaults OFF on launch. ON uses the same hidden Corneria
  preroll as Customize Screen, freezes native scene progression, and re-renders
  with live settings. A separate menu overlay preserves game palette colors.
  Starting from preview requests a fresh boot, not continuation of the fixture.

## Verification

- Full Windows desktop build succeeded.
- Full registered regression suite: 41/41 passed (266.87 seconds), including
  Original and EX simulation, endings, audio, transitions, and pool tests.
  Log: `tmp/parity-fixes-tests.log`.
- After the final preview input/freeze changes, the 13 targeted regression and
  runtime checks passed again (26.70 seconds), including preview state tests.
- Staff-roll fallback is covered with a short decoded recording fixture; a new
  full-length 170.10-second end-to-end listening comparison was not performed.
- Headless screenshot checks exercised main preview and navigation into OPTIONS,
  selecting CEL-DRAWN while the preview remains visible.
- After the angle-flip repair, the build and six targeted checks passed, including
  the new overflow regression and Original/EX runtime smoke tests (19.06 seconds).
  A separate EX preview screenshot check also passed with 2× upscale and NEON.
- No packages were published and no platform-specific runtime certification was
  performed. Existing upstream/submodule modifications were preserved.

## New 0.0.4.2 wrong-angle report

Reproduced a presentation-math failure consistent with the report, especially
for a ship closely aligned with its camera. Rounded quaternion matrices can be
slightly longer than unit length. Composing them using native Q15 wraparound
turns a positive diagonal into -32768, flipping an axis for fractional frames.

Concrete matrix: `[32744,-384,1202; 399,32763,-404; -1198,419,32743]`.
Composing it with its transpose should be nearly identity; the native helper
instead returns a first diagonal of -32768. A new presentation-only saturating
helper returns +32767. Fractional object/camera composition now uses that helper;
source endpoints, simulation math and source lighting retain native arithmetic.
A regression explicitly reproduces the old flip and checks the corrected result.

An exhaustive probe of all 256³ source byte-angle combinations found no invalid
source rotation basis (minimum determinant 0.999426); the reproduced problem is
the subsequent composition of rounded interpolated matrices, not source angles.
This repairs a concrete matching defect, but no user's recording was available
to prove that every reported wrong-angle flash has this cause.

### Intro mothership held-rotation regression

The zero-angle source matrix has diagonals 32766, while quaternion
normalization produces 32767 even when both input matrices are identical.
That made stationary mothership geometry pulse between source and fractional
frames. Identical rotation endpoints now bypass normalization; moving rotations
still interpolate, and the previous saturating-composition overflow fix remains.
Real DEBOSS_0/1/2 probes went from up to 443 changed pixels at 4x to zero for a
held pose. Regression tests compare actual rendered pixels at 2x/4x, three
depths and three interpolation fractions, plus exact held-matrix preservation.
The follow-up report identified the remaining 240 FPS/1x path: it still
alternated integer source-frame geometry and fractional geometry, with a
different face-visibility calculation from the upscaled path. High-FPS runtime
poses now request continuous geometry, clipping and visibility at every
presentation fraction, including the source boundary. Native 20 FPS/1x audits
retain their integer behavior. The regression now covers 1x/2x/4x and all twelve
240 FPS presentation samples; the held-pose depth sweep drops from up to 550
different 1x pixels to zero across all three mothership parts.

## Attack Carrier entrance and Continue credit number

- The 1-1/2-1 Attack Carrier child initializers set `invisible` before their
  first positioned update. Presentation snapshots previously retained those
  hidden placeholder poses, letting a newly visible child interpolate across
  the scene. Shared snapshot capture now omits invisible objects. Native
  placement, strategy timing and subsequent visible interpolation are unchanged.
  The regression dispatches the actual boss/child strategies and checks all
  four components at five presentation fractions on their first visible update.
- Continue setup now reproduces `FOXIRQ`'s dynamic BG2 tile write at row 24,
  column 29, using the live `CREDITS` word, digit offset `$68`, palette 5 and
  high priority. The initial repair incorrectly used `FOXY_CONTINUE_L`'s
  temporary `$8a` offset, which selects the T in CREDIT; the source interrupt
  replaces that placeholder before displaying the screen. Runtime captures
  now show numeric 0, and the regression verifies the actual pixel shape of
  numeric 2, not merely a matching tile address, for Original and EX.

## Follow-up map icons, boss label and shortcut

- Flat and scaled map sprites now select the packed texture nibble using
  sprite bit 5, matching the source. Sector Y and Out of This Dimension use
  the low nibble; Black Hole uses the high nibble. Original and EX hitlist
  tests compare all pixels of these three icons at 32 and 64 pixels.
- ENEMY detection now accounts for the source SPRADD tile-bank offset. Its
  glyphs use the boss meter's variable width and custom HUD position. The
  layout regression uses the actual source tile bank.
- Ctrl+Alt+F12 toggles God Mode with an ON/OFF status message. Repeats and
  key releases do not toggle it; the shortcut consumes the event before
  ordinary F12 handling. Both desktop and UWP input tests pass.
- Nucleus death background corruption and the Spinning Core boss-roll lid
  report remain under investigation; no verified repair is recorded yet.

### Live toggle and label alignment follow-up

- ENEMY is one native pixel lower and its tile box ends one pixel before the
  variable-width bar, following the same custom HUD offset. The editor preview
  uses the same gap and vertical adjustment.
- Disabling God Mode now clears the persistent native no-collisions bit while
  preserving other ship flags, and clears armed God Nukes. Enabling applies
  gameplay protection immediately. The setter also synchronizes EX's native
  GODMODE byte, so the shortcut and menu share the effective setting.
- Regression coverage exercises live enable/disable, collision flags, normal
  bomb counts after disabling, EX native synchronization, and label positioning
  across all 255 nonzero boss maximum-health values.

### Issue #33 escape camera object-list corruption

- Reproduced the reported VIEWOUTOFLB1_STRAT pool-coverage error by removing
  its MAPVAR1 building anchor through REMOVEDEADAL_L before an explosion burst.
  The native allocator inserts after that free slot and loses list ownership.
- The scheduler now suppresses only this cosmetic burst when the anchor is
  inactive, restores the shared sequence flag afterward, and still executes
  camera motion. It does not reclaim objects or relax pool invariants.
- Original/EX ending regressions verify normal two-explosion bursts with a
  valid anchor and unchanged active/free lists when the anchor is stale.
  This is a shared-core fix; an Android device was not available for validation.

### Additional model/world styles

- Appended Sepia, Thermal, Night Vision, Pastel, Comic, and Vaporwave to both
  selectors. Existing saved IDs are unchanged. A shared catalog now supplies
  renderer names, menu wraparound, setters and configuration validation.
- All styles support independent model/world intensity and live preview.
  Pixel tests cover layer isolation, alpha, zero strength and intensity blending;
  desktop/UWP tests round-trip every style; menu tests cover wraparound.
- Explicitly mark sprite and meter draws as 2D artwork and test their 1x tags.
  Three mixed-style preview captures were inspected; the application build,
  pixel/input tests and Original simulation regression passed.

### Dedicated scene bloom and effect availability

- Blueprint is world-only and Cel-Drawn is model-only. The old Bloom style
  is hidden from both selectors; its saved selection migrates to independent
  MEDIUM bloom. Other removed selections fall back to OFF.
- Main-page BLOOM cycles OFF/LOW/MEDIUM/HEAVY, defaults OFF, saves with the
  other graphics preferences and updates in preview. Its scene-wide pass uses
  a soft-threshold linear-light bright extraction, tight and wide separable
  blur, bilinear reconstruction and additive linear-light composition. Blur
  buffers are half native resolution, independent of render upscale.
- HUD pixels are excluded as both light sources and bloom destinations.
  Tests cover 1x/2x/4x halos, increasing strengths, alpha/HUD preservation,
  selector exclusions, preview menu cycling and settings round trips.

### Presentation cost, briefing audio and menu order

- Optimized Bloom's lookup/reconstruction work and reused persistent row workers;
  optimized styles that do not need neighbor-edge samples. Golden pixel hashes
  and threaded/serial equality pass at 1x/2x/4x, including cache resize reuse.
  See PERFORMANCE-2026-09-08.md for measurements and hardware limitations.
- The Good luck cue was cut off by the next stage's SPC bank at 0.60 seconds.
  Keep the original fade, but guard bank replacement until one second after
  actual cue dispatch. Original/EX real-SPC regressions preserve the complete
  approximately 0.95-second voice and verify subsequent gameplay entry.
- BLOOM remains named BLOOM and now follows Render Upscale. VSync follows
  Anti-Aliasing. Stable option IDs are unchanged; navigation follows visual
  order, including reverse navigation. Final menu capture was inspected.
