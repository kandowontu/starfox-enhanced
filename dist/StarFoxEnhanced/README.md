# Star Fox Enhanced

A native Windows PC port of the open-source
[UltraStarFox](https://github.com/Sunlitspace542/ultrastarfox) codebase. It
presents at a selectable 20, 30, 60, 90, 120, 240, 360, or 480 frames per
second while preserving the original game's intended NTSC simulation speed
and assembled model data. The default is 60 FPS.

This repository is an early playable fidelity pass. It does not track a game
executable, retail ROM, reconstructed ROM, or generated asset companion. The
Windows executable embeds source-built BPS deltas, symbol data, and the
complete optional compressed MSU-1 music set. On its
first launch it validates the user's own unmodified Star Fox USA v1.2 (Rev 2)
ROM, reconstructs the Original and Star Fox EX runtime data locally, and writes
one version-bound `Starfox-Assets.BIN` companion beside the executable. Later
launches use that validated companion without requiring the retail ROM to stay
present. No GitHub release has been published yet.

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
`Documents/Star Fox Enhanced/starfox-ex.srm`; its source `SFEX` validation,
defaults, loading, START GAME commit, and L+R+DOWN+B intro reset paths all run
unchanged.

The setup also independently selects game pace, render FPS, display mode,
MSU-1 music, rumble, controller remapping, and a separate Options page.
MSU-1 music is off by default and, when enabled for Original, replaces the
SPC music stem with the embedded orchestral set while leaving sound effects on
their own channel. Rumble is on by default for Original and plays the authored
UltraStarFox sequences on compatible SDL, XInput, and Steam Input controllers.
The Options page's first option is the
Star Fox EX-style God Mode: player collision is disabled,
regular Nova Bombs remain infinite, and holding R while pressing A fires a
God Nuke. The Options page can also enable a live on-screen FPS counter which
reports completed presentations in 250 ms samples so lag spots remain visible,
and select green (the default), white, blue, red, yellow, cyan, magenta, or
orange crosshair art. The selected hue applies to both the original four-piece
OBJ reticle and its Super FX cockpit triangles while damaged-wing indicators
remain red.

`CUSTOMIZE SCREEN` opens a mouse-driven captured native-gameplay HUD preview
using the game's actual HUD artwork. Lives, Shield, Bombs/Boost, Comms, and the
Boss Health bar can each be dragged independently; `RESET` (or Y) restores the
current display mode's defaults. Layouts are independent for 4:3,
16:9, 16:10, 21:9, and 32:9, with separate Original and Star Fox EX layouts
for every size. They save automatically to
`Documents/Star Fox Enhanced/hud-layout.cfg`.
Game pace, render FPS, display mode, MSU-1 music, rumble, God Mode, the FPS
counter, and crosshair colour also persist in
`Documents/Star Fox Enhanced/pregame.cfg`. Keyboard and
controller remaps are saved automatically when the remapping screen closes.
Standard display uses the complete 256x224 raster; Widescreen 16:9,
Widescreen 16:10, Ultrawide 21:9, and Super Ultrawide 32:9 expand the intro
and gameplay scene to 400x224, 360x224, 520x224, and 800x224 respectively
while keeping cartridge-authored HUD, dialogue, title, map, and control-screen
artwork centred in their original safe area. All modes use nearest-neighbor scaling
in a resizable window. It is a hybrid source port: a pinned 65C816 core
executes bounded original routines while timing, asset decoding, simulation
orchestration, rendering, audio output, and presentation are native C++.

## Quick start (Windows)

Build the pinned UltraStarFox and Star Fox EX sources first so their local ROM
and symbol outputs exist. These generated files stay ignored and untracked:

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

The required Original revision is pinned in `config/upstream.json`:

```powershell
git clone https://github.com/Sunlitspace542/ultrastarfox.git upstream-ultrastarfox
git -C upstream-ultrastarfox checkout 270e959a47d82240d9290a6c6630032c9ec53ff5
powershell -ExecutionPolicy Bypass -File tools/build_upstream.ps1
```

The UltraStarFox DOSBox assembler toolchain must be present in that checkout,
as described by its own build instructions. The helper idempotently applies
`config/ultrastarfox-native-runtime.patch`, which enables the authored MSU-1
and rumble events while retaining stock SPC music for the runtime ON/OFF
switch and routing the physical rumble transmitter through SDL.

The Star Fox EX 1.11.03 source revision is pinned separately in
`config/upstream-ex.json`:

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

F5 toggles the presentation debugger. While frozen, F6 advances one selected
render frame and F7 walks backward through the retained final-frame history.
After stepping backward, F6 first walks forward through those exact captured
presentations; at the live edge it advances simulation again. F5 resumes from
the newest live state. Audio is paused while frozen and stepped audio is
discarded rather than playing as a backlog afterward.

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

Hold Tab at any time to fast-forward the complete cartridge clock at 2x speed,
including gameplay, frontend transitions, music, and sound effects. Releasing
Tab immediately restores the selected game pace; render FPS is unchanged.

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
