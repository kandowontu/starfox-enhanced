# Star Fox Enhanced 0.0.6

Alpha feature and fidelity release. Includes all changes since 0.0.5.1;
the development candidate previously called 0.0.5.2 is released as 0.0.6.
Existing saves and settings should be retained when replacing application files.
Android version code is 10; Xbox package identity is 0.0.6.0.
No retail ROM, user save data or private signing keys are included.

## New options and presentation updates

- HDR EFFECT provides OFF / LOW / MEDIUM / HIGH directly below RTX Lighting.
  This is an SDR-compatible brightness/contrast style, not HDR display output.
  It preserves black/white endpoints and excludes HUD and menu text.
- ENHANCED SHADOWS (default OFF) replaces the original shadow pass with
  geometry-based light occlusion and receiving surfaces. Shadows render at
  the selected upscale resolution, with deterministic area-light samples
  producing softer edges as the caster moves away from the receiver.
  Additional stage and visual validation is still pending.
- CHROMATIC ABERRATION provides OFF / LOW / MEDIUM / HIGH in 3D options.
  Subpixel red/blue separation applies to model surfaces while preserving
  HUD and menu pixels. The setting is saved and localized.
- LANGUAGE selects English, Japanese, German, French or Spanish. Original
  scripts reuse upstream translations where available; Spanish and additional
  EX dialogue are authored translations. Menu localization and regional
  controller artwork are included in the current candidate and still undergoing
  screen-by-screen review.
- WIREFRAME THICKNESS provides independent 1–4 settings under 3D options.
- Projected text renders into the upscaled raster rather than expanding an
  already-rasterized low-resolution image.
- Controls starfield projection follows its small ship viewport, including
  widescreen offsets, without changing the gameplay starfield.
- Title foreground text preserves opaque black BG2 outline pixels over the
  ship while retaining the low/high tile-priority split. Verified in an Original
  title capture and an opaque-black foreground regression.
- Original music-cut commands also stop replacement MSU playback, leaving
  native sound effects intact.

## Fixes

- Arrowed tunnel gates flip orientation instantly when shot (#40), matching
  the native 180-degree state change. Their position and the camera remain
  smoothly interpolated; other models retain rotation interpolation.
- HDR, chromatic aberration, and enhanced shadows independently enable the
  pixel-layer tracking they require, even when every other filter is disabled.
  An isolated gameplay-capture regression covers all three options.

- Object-specific depth shading overrides select from the source's global
  table, rather than indexing relative to the scene's current table. This
  restores SCRAMBLE's bright hangar columns and wireframes. Verified with
  before/after Windows captures and source-table regression tests.
- Score completion percentages use the cartridge's numeric glyphs, rather than
  the text character mapping that produced solid blocks. Shared by every platform
  and both Original and EX; regression coverage checks every percentage from 0
  through 100 against the source font pixels.
- EX's NEW crosshair presentation matches successive sight positions by distance
  from the player, not the recycled particle identity. This prevents markers from
  sweeping between near/far positions at high presentation frame rates without
  changing the native simulation. Regression coverage includes 60, 120, 240 and
  480 FPS interpolation phases.
- EX's 3D sight markers use the main-menu crosshair color setting; other models
  retain their own palette. GREEN is explicitly green, and overrides account
  for the renderer's palette-row offset and the NEW reticle's textured-sprite
  rendering path. A live Windows capture confirms green markers.
- EX aiming markers no longer inherit decorative camera float, preventing
  bobbing while holding inverted Down against the upper flight boundary. World
  camera motion and native gameplay are unchanged.
- EX's native bitmap overlay excludes the outer guard columns geometrically,
  preventing them from becoming visible when a transition changes their palette
  from black to tan. Widescreen blank fills also choose the darkest palette entry
  instead of defaulting to tan index zero when no exact black exists, and palette
  changes invalidate that cached choice. Regression coverage includes a palette
  without exact black at 16:9; the reported scramble frame still needs on-device
  confirmation.
- Bloom is displayed as a separate, linearly filtered glow layer, so enlarging
  the image no longer enlarges the glow with nearest-neighbor pixel blocks.
  Base game pixels retain their selected filtering, and late host overlays do
  not receive scene glow. Both 2D and 3D bloom use this path.
- Original: pressing Start on THE END after the music finishes no longer enters
  a non-returning native reboot inside the bounded object-update call (#35).
  The host handles the restart into the intro while retaining host options.
  The full-ending regression reproduced the reported instruction-limit error
  before the fix and passes after it in both Original pacing modes.
- Android CI packages use a permanent signing certificate. CI verifies the
  expected public certificate fingerprint before accepting an APK. Both local
  debug and release build types can use this key through environment settings.
- Switch's SDL AUDOUT backend now pipelines its two hardware buffers instead of
  waiting for the hardware queue to empty after every chunk. It reuses the buffer
  actually returned by AUDOUT, handles empty wakeups, and reports device errors.
  A host-side test exercises 1,000 handoffs using the actual patched callbacks.

## Android upgrade notice (#36)

Earlier public APKs used temporary runner-generated debug keys. A new permanent
key cannot update an APK signed with one of those old keys. Back up your saves and
user data before a one-time uninstall/reinstall; uninstalling can delete app data.
Subsequent public builds will retain the new certificate and application ID.
Do not expect independently built debug APKs to update a public installation.

## Verification limits

### Follow-up on reports from 0.0.5.1

- Localized menu Latin letters now use the same full-height font as English,
  with separate accents instead of compressed letter shapes. Japanese glyphs
  have additional spacing and exact per-glyph width measurement. Widescreen
  localized menus use a wider backdrop and columns rather than shrinking text.
  Menu text is composited separately from scene effects, preventing enabled
  filters and chromatic aberration from damaging its glyphs.
- Menu confirmation remains press-only across preview/runtime rebuilds:
  holding A cannot repeatedly activate options; release and press it again.
  Submenu Back also requires release before another confirmation, and starting
  the game is restricted to the main menu.
- Hold Tab in the setup menu to hide its overlay and inspect the scene behind
  it; release Tab to restore the menu without changing the selected page.
  Preview remains live when enabled. Tab retains fast-forward during gameplay
  and remains available to the controller/keyboard binding capture screen.

- Host scene transitions now clear both the native colour-window allocation
  and its cached presentation state. Checkpoint revival also discards the
  outgoing tint snapshot, alongside the existing circle cleanup.
  This addresses lingering red/blue tints after death or warps (#41); fixed-colour
  effects cover the full framebuffer. The exact Android report still needs
  on-device confirmation.
- Restored the native Game Over music upload omitted by the host scene wrapper.
- Accepting Continue now plays Fox's “Let's go!” cue and presents the source
  32-raster thumbs-up hold before fading out. Original and EX transition tests
  cover the gesture hold and the subsequent full-bright/14-to-0 fade.
- Fixed Atomic Base/Nucleus defeat corruption: the debris routine's byte-sized
  scroll write retained the room's high byte, turning 248–251 into 504–507.
  A narrowly scoped correction preserves the shake without exposing unrelated
  tilemap rows. The corruption was reproduced and the same live defeat capture
  verified after the fix; regression coverage also protects unrelated scrolling.
- Briefing text now uses DOG.SCR's palette bank 6 instead of bank 0, with the
  same fade as the portraits rather than an additional six-step attenuation.
- Widescreen tunnel backgrounds stop at the native view and use solid dark
  margins. This also covers rooms without the animated tunnel HDMA path;
  models and HUD are still allowed to render across the wide view.
- One Switch user reports that 0.0.5.2 resolves their audio issue; this is useful
  device feedback, not verification of every hardware/overclock combination.

The tentative shield/cockpit reports still need a deterministic reproduction;
the 0.0.5.1 screenshots alone are not evidence that those defects persist here.

The Switch pipeline defect is covered by a deterministic host regression, but
the reported Erista crackling still requires listening tests on real hardware,
with and without overclocking. A successful cross-build is not an on-device audio
verification. The final 0.0.6 source passed all nine platform build jobs:
Windows x64/x86, Xbox UWP, Linux, macOS, iOS, Android, Switch and Vita.
The published packages include the tunnel-gate orientation fix (#40).
Build results: https://github.com/kandowontu/starfox-enhanced/actions/runs/34300149806

The latest September 8 Windows verification passed all 44 local tests,
including independent HDR/chromatic/shadow captures and a repeated identical
Off baseline. Follow-up
Original and EX simulation checks also pass, covering HDR menu ordering and
cycling, plus translated radio line widths (including all 406 EX entries in
all four translated languages). These checks do not establish visual dialogue
quality, translation accuracy, or final cross-platform compatibility.

The current source also builds with GCC 13 on local Ubuntu/WSL. All 38
configured Linux tests pass, including Original and EX simulation, ending,
audio and level-clear checks, plus the isolated presentation-effects test.
This non-embedded build does not verify release packaging or console hardware.

The current Xbox/UWP runtime compiles with MSVC 14.44 and passes the UWP
import audit against Microsoft.VCLibs 14.0.33519.0: no desktop CRT imports,
and all imported app-runtime symbols are supplied. Xbox launch and gameplay
still require on-device verification.

Ultrawide menu review: the 13-row 3D page fits its backdrop in Japanese,
German, French, and Spanish at 32:9. An earlier French capture lost text;
after rebuilding, five independent 240 FPS captures were byte-identical and
complete, including with the saved mixed bloom settings. Indexed and final
captures agree that the text is present in that build. No root cause has yet
been proven, so this observation is not listed as a fixed rendering defect.
