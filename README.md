# Star Fox Enhanced

A native Windows PC port of the open-source
[UltraStarFox](https://github.com/Sunlitspace542/ultrastarfox) codebase. It
presents at a selectable 20, 30, 60, 90, 120, 240, 360, or 480 frames per
second while preserving the original game's intended NTSC simulation speed
and assembled model data. The default is 60 FPS.

> You do not need to build this yourself. Prebuilt alpha downloads are
> available under GitHub Releases, and the current Windows x64 nightly is also
> kept in `dist/StarFoxEnhanced`.

This repository is an early playable fidelity pass. It does not track a game
executable, retail ROM, reconstructed ROM, or generated asset companion. The
Windows executable embeds source-built BPS deltas and symbol data. The
complete optional compressed MSU-1 music set is distributed separately as
`Starfox-MSU1.PAK` beside the executable. On its
first launch it validates the user's own unmodified Star Fox USA v1.2 (Rev 2)
ROM, reconstructs the Original and Star Fox EX runtime data locally, and writes
one version-bound `Starfox-Assets.BIN` companion beside the executable. Later
launches use that validated companion without requiring the retail ROM to stay
present. `v0.0.5.1` is the current public alpha release. See
[the 0.0.5.1 changelog](docs/RELEASE-0.0.5.1.md) and
[the full 0.0.5 changelog](docs/RELEASE-0.0.5.md).

This project utilizes Codex GPT 5.6 Sol at Ex-High thought.

## What is preserved

- Gameplay, PATH/map bytecode, strategies, collision, damage, RNG, animation,
  bosses, and stage progression update at the original deterministic 20 Hz.
- Camera and object transforms are presentation-only interpolations, producing
  smooth motion at the selected render FPS without changing game state.
- All rendered 3D geometry is decoded from the assembled UltraStarFox ROM:
  integer vertices, faces, BSP order, animation frames, LODs, shadows, texture
  coordinates, colors, and collision metadata.
- Original BG tilemaps, palettes, OBJ graphics, HUD, text, route-map sprites,
  textured planet maps, particles, dust, and SPC700 music/effects are used.
- Title, attract intro, control selection, training, route selection, all
  three routes, game over, continue, credits, pause, and stage transitions are
  connected in the native flow.

The cleaned pre-game setup starts with an `EXPERIENCE` selector. `ORIGINAL` is
the default; `STARFOX EX` selects the embedded 1.11.03 source build, including
its native title/intro, three-page configuration menu, shipped `PLANETS` and
`PLANETS2` campaigns, custom stages, ships, models, palettes, music, and source
mechanics. EX's real 64 KiB cartridge SRAM is persisted byte-for-byte at
`starfox-ex.srm` beside the desktop executable; its source `SFEX` validation,
defaults, loading, START GAME commit, and L+R+DOWN+B intro reset paths all run
unchanged.

The setup also independently selects game pace, render FPS, display mode,
presentation renderer, MSU-1 music, rumble, controller remapping, and a
separate Options page. `RENDERER` defaults to GPU and can be changed to
SOFTWARE to use SDL's portable CPU presentation backend; this is an actual
backend switch and is saved with the other setup choices.
Under OPTIONS → CONTROLLER → KEYBOARD, the RESET action remaps the final key of
`Ctrl+Shift+R`. Ctrl+Shift stays fixed, the default suffix is R, and keyboard
defaults restore it. The shortcut requires both modifiers; plain Ctrl+R does
not reset. Remapping the suffix does not change normal gameplay bindings.
MSU-1 music is off by default and, when enabled for Original, replaces the
SPC music stem with the companion orchestral set while leaving sound effects
on their own channel. If `Starfox-MSU1.PAK` is not beside the executable, the
option reads `NOT FOUND` and cannot be enabled. Rumble is on by default for
Original and plays the authored
UltraStarFox sequences on compatible SDL, XInput, and Steam Input controllers.
The Options page also provides independent MUSIC and SFX volume controls.
The main page opens dedicated **2D Options** and **3D Options** submenus,
keeping full-height text and all main-page entries visible without scrolling.
2D Options contains 2D Filter, 2D Bloom, World Effects and World Effect Intensity.
3D Options contains Anti-Aliasing, VSync, Render Upscale, 3D Bloom, 3D Smoothing,
RTX Lighting, Model Effects and Model Effect Intensity. B or BACK returns to
the corresponding main-page entry; live Preview stays active in either submenu.
`MODEL EFFECTS` and `WORLD EFFECTS` independently offer OFF,
INK, NEON, MONOCHROME, DITHERED, SEPIA,
THERMAL, NIGHT VISION, PASTEL, COMIC, and VAPORWAVE. The newer styles add warm
vintage tones, a false-color heat palette, green scanlines, soft colors,
halftone shading, and a purple/cyan palette respectively. CEL-DRAWN is model-only;
BLUEPRINT is world-only. World effects
cover the ground, sky, scenery and stars. Each graphics submenu provides its
effect intensity from 0–100% in 10% steps. Controller Remap remains in Options. All effect choices
are saved; older settings retain their model style with world effects OFF.
The `2D BLOOM` and `3D BLOOM` options independently offer OFF (default),
LOW, MEDIUM, and HEAVY for scenery and model light sources. They extract bright
pixels in linear light and spread tight and broad halos; HUD is excluded.
Older combined Bloom settings migrate to the same strength for both options.
`3D SMOOTHING` separately offers OFF/LOW/MEDIUM/HEAVY for colour transitions
within model surfaces, including untextured faces. It does not replace silhouette
Anti-Aliasing or smooth backgrounds/HUD.
The old Bloom style migrates to MEDIUM bloom. Disallowed old style selections
fall back to OFF without changing other preferences.
HUD, dialogue and menu text remain unaffected. Main-page `PREVIEW` defaults OFF each launch; ON freezes the same
Corneria reference scene used by Customize Screen and previews graphics
changes live without advancing gameplay.

For MUSIC and SFX volume, left/right changes each in 10% steps; the mouse can drag either bar to any
whole percentage from 0 through 100. The first option is the
Star Fox EX-style God Mode: player collision is disabled,
regular Nova Bombs remain infinite, and holding R while pressing A fires a
God Nuke. Press Ctrl+Alt+F12 to toggle God Mode on or off, with an on-screen
confirmation. Holding the shortcut does not repeatedly toggle it.
The Options page can also enable a live on-screen FPS counter which
reports completed presentations in 250 ms samples so lag spots remain visible,
and select green (the default), white, blue, red, yellow, cyan, magenta, or
orange crosshair art. The selected hue applies to both the original four-piece
OBJ reticle and its Super FX cockpit triangles while damaged-wing indicators
remain red.

`RENDER UPSCALE` rasterizes the Super FX world layer at 1x, 2x, 3x or 4x.
Face visibility and BSP order stay tied to the original grid while projection
retains fractional endpoints for smooth interpolation.

`2D FILTER` replaces `ENHANCED TEXTURES` and offers OFF, EDGE, XBRZ,
SHARP BILINEAR and CRT. EDGE and xBRZ smooth enlarged artwork; Sharp Bilinear
softens pixel boundaries; CRT adds scanlines and a subtle bright phosphor glow.
All modes work at native Render Upscale: the filter reconstructs 2D art in a
separate 2x buffer and resolves it back to the native raster. Higher render
scales retain more filter detail. Geometry and game timing are unchanged.
Texture artwork on 3D polygons is included too; solid-coloured faces remain
under the separate 3D Smoothing option. The polygon coverage mask is preserved.
Previously enabled Enhanced Textures settings migrate to EDGE.

xBRZ is enabled by default. Minimal builds may set `-DSTARFOX_ENABLE_XBRZ=OFF`;
the menu then skips xBRZ. See `THIRD_PARTY_NOTICES.md` for attribution.

Filtering, palette expansion, anti-aliasing and lighting share persistent CPU
workers. Higher render scales require more processing time and memory.
RTX Lighting and Anti-Aliasing remain independent options. For diagnostics,
`STARFOX_2D_FILTER_DEBUG=1` highlights filtered framebuffer pixels in magenta.

`CUSTOMIZE SCREEN` opens a mouse-driven captured native-gameplay HUD preview
using the game's actual HUD artwork. Lives, Shield, Bombs/Boost, Comms, and the
Boss Health bar can each be dragged independently; `RESET` (or Y) restores the
current display mode's defaults. Layouts are independent for 4:3,
16:9, 16:10, 21:9, and 32:9, with separate Original and Star Fox EX layouts
for every size. They save automatically to
`hud-layout.cfg` beside the desktop executable.
Game pace, render FPS, display mode, renderer, graphics choices, MSU-1 music,
rumble, music/SFX volumes, God Mode, the FPS counter, and crosshair colour
also persist in
`pregame.cfg` beside the desktop executable. Keyboard and
controller remaps are saved as `input-bindings.cfg` in the same folder.

Desktop builds are portable by default: keep these files with the executable
when moving or upgrading the game. Use an extracted, writable folder (not a
read-only installation directory). The first normal launch copies any missing
files from the former Documents/preference locations; existing portable files
always win, and originals are not deleted. The working directory does not affect
save locations. Mobile and console packages retain their writable platform
storage because their executable/package directories may be read-only.
Standard display uses the complete 256x224 raster; Widescreen 16:9,
Widescreen 16:10, Ultrawide 21:9, and Super Ultrawide 32:9 expand the intro
and gameplay scene to 400x224, 360x224, 520x224, and 800x224 respectively
while keeping cartridge-authored HUD, dialogue, title, map, and control-screen
artwork centred in their original safe area. All modes use nearest-neighbor scaling
in a resizable window. It is a hybrid source port: a pinned 65C816 core
executes bounded original routines while timing, asset decoding, simulation
orchestration, rendering, audio output, and presentation are native C++.

## Quick start (Windows)

Release builds use the checked-in source-built BPS deltas and pinned symbol
maps, so a clean checkout does not need either reconstructed ROM. Developers
regenerating those inputs can build the pinned UltraStarFox and Star Fox EX
sources; their local ROM and symbol outputs remain ignored and untracked:

```text
upstream-ultrastarfox/SF.SFC
upstream-ultrastarfox/SYMBOLS.TXT
upstream-star-fox-ex/SFES/SFES.SFC
upstream-star-fox-ex/SYMBOLS.TXT
```

Then run:

```powershell
.\play-starfox.ps1
```

The launcher configures an optimized build on first use and starts at the
pre-game setup. On the executable's first run, supply any supported unmodified
1 MiB retail Star Fox/Starwing ROM:

```text
Star Fox (Japan), revisions 1.0 or 1.1
Star Fox (USA), revisions 1.0, 1.1, or 1.2
Starwing (Europe), revisions 1.0 or 1.1
Starwing (Germany), revision 1.0
the ROM beside starfox_pc.exe or in C:\NTSC-US Super Nintendo System Roms
the path named by STARFOX_RETAIL_ROM
```

A 512-byte copier header is accepted and removed before validation. Each known
regional revision is checksum-verified and losslessly canonicalized to USA
v1.2 before the source-build patches are applied. Competition cartridges,
betas, hacks, Star Fox 2, modified dumps, and unknown revisions are rejected.
After `Starfox-Assets.BIN` is created, the retail file is no longer read unless
the executable's embedded patch or symbol manifest changes and the companion
must be rebuilt.

A development map can be selected explicitly:

```powershell
.\play-starfox.ps1 LEVEL1_1
```

Explicit external ROM/symbol pairs can still be passed to a development build
for source-to-port comparisons.

## UltraStarFox source setup

The required Original revision is pinned in `config/upstream.json` and tracked
as a submodule, so `git clone --recurse-submodules` fetches the source with the
repository and the pin keeps the embedded symbol tables bound to the checked-in
BPS deltas:

```powershell
git submodule update --init upstream-ultrastarfox
powershell -ExecutionPolicy Bypass -File tools/build_upstream.ps1
```

The UltraStarFox DOSBox assembler toolchain must be present in that checkout,
as described by its own build instructions. The helper idempotently applies
`config/ultrastarfox-native-runtime.patch`, which enables the authored MSU-1
and rumble events while retaining stock SPC music for the runtime ON/OFF
switch and routing the physical rumble transmitter through SDL.

The Star Fox EX 1.11.03 source revision is here: https://github.com/kandowontu/star-fox-ex

```powershell
git clone https://github.com/sunlitspace542/star-fox-ex.git upstream-star-fox-ex
git -C upstream-star-fox-ex checkout b5e2d837a15a72a532cd019bfe332b7a4b660924
powershell -ExecutionPolicy Bypass -File tools/build_starfox_ex.ps1
```

That checkout also supplies its DOSBox assembler toolchain. Both build helpers
reject a different source revision so the embedded symbol tables remain bound
to the checked-in BPS deltas.

## Build and test

```powershell
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j 8
ctest --test-dir build/release --output-on-failure
```

Create a portable executable folder (ROM data remains user-supplied):

```powershell
cmake --install build/release --prefix dist/StarFoxEnhanced
```

The default build downloads the pinned music sources and installs the optional
`Starfox-MSU1.PAK` beside `starfox_pc.exe`. Configure with
`-DSTARFOX_PACKAGE_MSU1_MUSIC=OFF` to build and install the game without that
companion; the executable remains fully playable with the original SPC music.

You can launch the binary directly:

```powershell
build/release/starfox_pc.exe
build/release/starfox_pc.exe LEVEL2_3
build/release/starfox_pc.exe path/to/SF.SFC path/to/SYMBOLS.TXT TITLEMAP
```

## Controls

| SNES | Keyboard | Gamepad |
|---|---|---|
| D-pad | Arrow keys | D-pad |
| B | Z | South button |
| Y | A | West button |
| A | X | East button |
| X | S | North button |
| L / R | Q / W | Shoulder buttons |
| Select | Apostrophe (`'`) | Back/View |
| Start | Enter | Start/Menu |

Escape opens an exit-confirmation dialog. Select+Start remains available to
the game and never exits the PC runtime.

Rewind history is disabled at launch, so it costs no background CPU time or
memory during normal play. Press F12 on the first setup page to opt in for the
current run; a two-second title-bar message confirms the change without adding
another permanent menu item. F12 there can disable it again.

F5 toggles the presentation debugger. While frozen, F6 advances exactly one
render frame. With history enabled, F7 walks backward through the retained
final-frame history; with history disabled it has no stored frame to visit.
The debugger uses the native 60 Hz raster as its minimum cadence, so a 20 or
30 FPS output choice never skips cartridge frames. At 90 FPS and above it also
retains every interpolated output frame. Hold F6 or F7 for 450 ms to begin a
slow eight-frame-per-second repeat.

After stepping backward, F6 first walks forward through those exact captured
presentations; at the live edge it advances simulation again. F5 resumes from
the newest live state. When explicitly enabled, the lossless compressed
history holds up to 3,600
presentations within a 128 MiB budget. Audio is paused while frozen and stepped
audio is discarded rather than playing as a backlog afterward.

## Native platform targets

The Windows x64 executable remains the primary release. The same portable SDL3
runtime also has build targets for Windows x86, Linux x64, unsigned universal
macOS, unsigned iOS arm64, Android arm64, Nintendo Switch, PS Vita, and an x64
Xbox UWP package for Developer Mode. Only Android and iOS expose translucent on-screen
controls; physical
SDL-compatible controllers continue to work normally. The Apple and Android
packages show a native file picker on first launch, accept any supported retail
revision, and store the reconstructed asset companion in writable application
storage rather than attempting to modify an application bundle.

```powershell
.\tools\build_windows_x86.ps1 -LlvmMingwRoot C:\path\to\llvm-mingw
.\tools\build_xbox_uwp.ps1
```

```bash
tools/build_linux.sh
tools/build_apple.sh . macos
tools/build_apple.sh . ios
tools/build_android.sh . debug
DEVKITPRO=/opt/devkitpro tools/build_switch.sh
VITASDK=/usr/local/vitasdk tools/build_vita.sh
```

Apple targets require Xcode on macOS, Android requires its SDK/NDK, Switch
requires the devkitPro Switch toolchain, and Vita requires VitaSDK. These
platform SDKs are not vendored.
The `Portable platform builds` GitHub Actions workflow produces a universal
unsigned macOS `.app`, an unsigned iOS arm64 device bundle, an installable
debug-signed Android arm64 APK, a Nintendo Switch homebrew `.nro`, a PS Vita
homebrew `.vpk`, and a signed x64 UWP package for Xbox Developer Mode. The Xbox
build uses UWP LocalState as its internal writable storage for settings, HUD layouts, `Starfox-Assets.BIN`,
and the optional `Starfox-MSU1.PAK`; both companion names are matched without
regard to case. Packaged application files remain read-only. The iOS
bundle must be signed with the user's own Apple identity or sideloading tool
before installation. It also packages a
standalone Windows x64 `starfox_asset_builder.exe`: run it against a supported
retail ROM on the PC, transfer the resulting `Starfox-Assets.BIN`, and select
that BIN from the mobile app. Mobile can still select the retail ROM directly;
the prebuilt BIN path is an optional convenience.

The Switch runtime finds that same BIN beside the launched NRO, even when its
application folder has been renamed. The packaged default is
`sdmc:/switch/StarFoxEnhanced/Starfox-Assets.BIN`. It includes a local PC script
for generating an optional NSP forwarder with NTON and keys dumped from the
user's own console; console keys are never stored in this repository or in the
public build workflow. See `platform/switch/README.md` for the exact layout and
the forwarder warning.

Linux builds SDL from the pinned source archive. Install the distribution's
SDL build dependencies first; the authoritative Ubuntu/Fedora package lists
are maintained in SDL's
[Linux build documentation](https://github.com/libsdl-org/SDL/blob/main/docs/README-linux.md).
`tools/build_linux.sh` performs an optimized clean build, runs the complete
test set, and installs both the runtime and standalone asset builder.

Xbox/XInput controllers, Steam Input virtual controllers, and the Steam Deck's
built-in controls are detected automatically. Both the D-pad and left stick
move by default; Deck back paddles and all other exposed controls can be
assigned from CONTROLLER REMAP.

Star Fox EX can consume up to five connected gamepads for its native two-player
and multitap modes. Devices with an SDL/Steam player index are assigned first
in player-number order, followed by the remaining detected controllers;
keyboard input belongs to player one only. EX's own `MULTITAP SUPPORT` and
`NUMBER OF PLAYERS` settings remain authoritative, including its one-player
mode that deliberately mirrors player-one input to all five ships.

EX's `SUPER SCOPE MODE` uses the PC mouse as the native light gun: move to aim,
left-click for Fire, right-click for Cursor/calibration, middle-click for Pause,
and use either side button for Turbo. Scope mode owns the mouse only while that
EX option is enabled; otherwise right-drag remains the free presentation camera.
`NTT DATA PAD SUPPORT` maps 0-9 to the matching main-row or keypad digits,
asterisk to Shift+8 or keypad Multiply, hash to Shift+3 or keypad Divide,
period to either Period key, C to C, and Hang Up to H.

Hold Tab at any time to fast-forward the complete cartridge clock at 2x speed.
Hold Ctrl+Tab for 3x total speed (200% faster), or Shift+Ctrl+Tab for 5x total
speed (400% faster). For transition testing, Ctrl+Shift+` runs at 20x total
speed (2000%). Gameplay, frontend transitions, music, and sound effects all
accelerate together. Releasing the shortcut immediately restores the selected
game pace; render FPS is unchanged.

During gameplay, hold the right mouse button and drag to freely adjust camera
yaw and pitch. While still holding the right mouse button, use the mouse wheel
to zoom in or out. The camera adjustment is presentation-only and does not
change the deterministic game pace.

## Fidelity boundary

Unlocked 20 FPS uses one logic/strategy update for every three fixed 60 Hz
cartridge raster phases. Original Speed additionally retains source frames
according to the measured 10.7 MHz workload schedule, reproducing the
characteristic cartridge slowdown. The independently selected render FPS
changes only how often frames are presented: an exact rational scheduler
services the same raster phases, logic ticks, frontend timing, and audio pace
at 20, 30, 60, 90, 120, 240, 360, and 480 FPS. Object and camera
rotations use normalized matrix interpolation between source updates, while
gameplay state remains fixed-point and unchanged.

The port consumes the exact assembled models and fixed-point state, but it is
not a cycle-accurate SNES emulator. Its software renderer reproduces the
source projection, clipping, face order, scan conversion, sprite priority,
and indexed palette behavior while the extra frames are newly interpolated
presentations. See `docs/ARCHITECTURE.md` for the subsystem boundary and test
strategy.

Useful diagnostics include `starfox_stage_trace`, `starfox_stage_preview`,
`starfox_shape_coverage`, and `starfox_planet_probe`. Third-party revisions
and licenses are recorded in `THIRD_PARTY_NOTICES.md`. The original Nintendo/
Argonaut staff, Star Fox EX team, UltraStarFox contributors, native-port
credit, and MSU-set attribution are recorded separately in `CREDITS.md`.

The EX regression runs every one of the 40 stage labels shipped through
`PLANETS` and `PLANETS2` for 2,000 deterministic logic ticks. The source-only
`PLANETS3` test campaign is intentionally outside the shipped experience.
