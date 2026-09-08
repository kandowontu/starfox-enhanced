# Star Fox Enhanced 0.0.5

Alpha release, September 8, 2026. Changes since 0.0.4.2.

## Graphics and live preview

- Separate **Model Effects** and **World Effects**, with independently saved
  intensities and live previews. Styles include Ink, Neon, Monochrome, Dithered,
  Sepia, Thermal, Night Vision, Pastel, Comic and Vaporwave. Cel-Drawn is available
  for models; Blueprint is available for the world, not models.
- New **2D Bloom** and **3D Bloom** controls: OFF, LOW, MEDIUM and HEAVY.
  Actual linear-light bright extraction, soft threshold, tight/wide blur and
  additive composition produce halos. HUD text and meters stay protected.
  Background/world and model/textured-geometry light sources have independent
  strengths. Previous single-Bloom settings migrate to both controls.
- New **3D Smoothing**: OFF, LOW, MEDIUM and HEAVY. Softens internal model color
  and face boundaries, including untextured faces, without changing geometry.
  Anti-aliasing remains the control for outer silhouettes.
- The 2D filter now also processes textures drawn on 3D polygons. Untextured
  faces remain unaffected unless 3D Smoothing is enabled. Polygon coverage is
  preserved so filtering does not spread artwork outside its model.
- **Preview: ON/OFF** on the main page, default OFF. Uses the same frozen
  gameplay reference as Customize Screen and updates renderer/effect settings
  live. Starting the game boots a fresh session rather than the preview fixture.
- Increased main-menu line spacing with a smaller font and a backdrop that
  contains the final Preview row. PACE/SPEED uses a true diagonal slash.
  VSync follows Anti-Aliasing; 2D/3D Bloom
  follow Render Upscale. Controller remapping now lives under Options.
- Widescreen tunnel margins keep a solid background instead of repeating the
  tunnel graphics beyond the single scrolled tunnel cross-section.

## Rendering and HUD fixes

- Correct Original quick fade-down: brightness 11 now steps to 9, not 10.
- Correct word-coordinate scaling for eight EX models: BOXXIE, CORNFRIEND,
  MARIOHFIRE, LUIGIHFIRE, LUIGIHSBW, BLACKROSE, LUIGIH and MARIOH. This includes
  Crimson King rendering eight times too large and a Luigi Hydra component
  rendering sixteen times too large.
- Prevent rounded presentation matrices from overflowing and briefly flipping
  model angles. Held rotations no longer pulse through normalization.
- Use consistent continuous geometry at high presentation rates, including
  source-frame boundaries at 240 FPS, addressing intro mothership face flicker.
- Exclude invisible initialization poses from interpolation snapshots so the
  1-1/2-1 Attack Carrier components do not flash in front of the player.
- Correct packed-nibble selection for Sector Y and Out of This Dimension map
  icons, including scaled icons, without replacing them with Black Hole artwork.
- Continue screen displays the actual numeric credits remaining rather than a
  missing digit or the letter T.
- ENEMY follows the variable-length boss bar and custom HUD/widescreen offsets,
  sits close to its left edge, and has the corrected one-pixel vertical offset.

## Death, revival and audio

- Restore checkpoint-selected music after death instead of replaying cached
  boss music.
- Restore the native black hold, blinking STAGE label and gradual reveal on
  revival. Stage setup initializes the native label countdown; the black fade
  preserves OBJ artwork rather than blanking the entire completed framebuffer.
- Restore checkpoint-native palette state instead of stale pre-death colors.
  Reset presentation history at revival to avoid interpolation across reused
  object slots. Regression coverage includes control restoration, life
  consumption and final-life Game Over.
- Preserve the complete approximately 0.95-second “Good luck” voice on the
  briefing screen: stage audio-bank replacement waits until the cue finishes
  while retaining the original visual fade.
- Natural staff-roll MSU completion returns music selection to the native SPC
  engine, allowing the later jingle rather than continuing silent MSU output.
- Missing/corrupt replacement MSU tracks stop previous playback. Invalid loop
  markers in shortened tracks fall back to frame zero instead of hanging.

## Controls and portability

- Ctrl+Alt+F12 toggles God Mode live with ON/OFF feedback, including when it was
  enabled in the main menu. Synchronizes native protection flags and EX state;
  disabling also clears armed God Nukes without changing ordinary bomb counts.
- The R suffix of Ctrl+Shift+R can be remapped independently of gameplay
  bindings. Ctrl+Shift remain required; the selected suffix is saved.
- Desktop portable settings and saves use the executable-side location. Keep
  the application in a writable folder. Sandboxed platforms retain their
  platform-specific writable storage requirements.

## Performance and robustness

- Reduce Bloom processing work with cached color/threshold tables, reusable
  reconstruction data, combined halo sampling and persistent parallel workers.
  Representative local Bloom medians improved from 8.08 to 4.58 ms at 1x,
  25.73 to 14.89 ms at 2x, and 96.49 to 64.67 ms at 4x. These are processing
  benchmarks, not a promise of a particular in-game FPS on every device.
- Avoid unnecessary neighbor sampling for effects that do not use edges.
- Replace broad automatic orphan-object reclamation with strict list validation
  and a targeted fix for issue #33: skip the escape camera's cosmetic explosion
  burst only when its building anchor has already been freed. Valid bursts and
  camera movement remain intact; pool ownership invariants are not relaxed.
- Expand Original/EX coverage for model scaling, 240 FPS held poses, map icons,
  credits glyphs, boss HUD placement, live controls, audio tails, revival,
  filtering, Bloom, smoothing and settings migration.

## Platform build changes since 0.0.4.2

- Xbox UWP dependency packaging corrections and updated 0.0.5.0 identity.
- Vita ELF segment spacing/SCE metadata headroom and VPK verification fixes;
  Vita application version 00.05.
- Android xBRZ debug-build fix; application version 0.0.5, version code 7.
- Versioned packages for Windows x64/x86, Xbox UWP, Linux, macOS, unsigned iOS,
  Android, Switch homebrew and Vita, plus the Windows standalone asset builder.

## Known limitations

- Issue #35's reported post-credits instruction-limit crash is **not confirmed
  fixed**. Extended Original/EX ending tests did not reproduce it; the MSU EOF
  repair is a separate defect. A reproducing save/replay remains useful.
- Nucleus death background corruption and the Spinning Core boss-roll lid
  report do not yet have verified fixes.
- Issue #33 has a reproduced shared-core fix, but Android hardware/reporter
  verification remains outstanding. Switch audio distortion and device-specific
  Android performance also need hardware testing.
- Automated and headless checks are not a certification of whole-game 1:1
  parity or every platform's GPU/audio behavior. See the dated audit and
  performance documents in `docs/` for scope and evidence.

## Installation

Download the package for your platform. Packages do not include a retail ROM,
generated game assets, user saves or settings. Supply your own supported retail
ROM and follow the included README/asset-builder instructions. Back up existing
saves before moving an older installation to executable-side portable storage.
