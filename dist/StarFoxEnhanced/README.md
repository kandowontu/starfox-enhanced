# Star Fox Enhanced

A native Windows PC port of the open-source
[UltraStarFox](https://github.com/Sunlitspace542/ultrastarfox) codebase. It
presents at a selectable 20, 30, 60, 120, 240, or 360 frames per second while
preserving the original game's intended NTSC simulation speed and assembled
model data. The default is 60 FPS.

This repository is an early playable fidelity pass. The prebuilt Windows
executable in `dist/StarFoxEnhanced` is a single-file build: its pinned,
assembled UltraStarFox ROM and symbol data are embedded as Windows resources,
so recipients can run `starfox_pc.exe` without installing dependencies or
supplying adjacent data files. No GitHub release has been published yet.

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

The cleaned pre-game setup independently selects game pace, render FPS,
display mode, controller remapping, and a separate Options page. Its first
option is the Star Fox EX-style God Mode: player collision is disabled,
regular Nova Bombs remain infinite, and holding R while pressing A fires a
God Nuke. Standard display uses the complete 256x224 raster; Widescreen 16:9,
Widescreen 16:10, Ultrawide 21:9, and Super Ultrawide 32:9 expand the intro
and gameplay scene to 400x224, 360x224, 520x224, and 800x224 respectively
while keeping
cartridge-authored HUD, dialogue, title, map, and control-screen artwork
centred in their original safe area. All modes use nearest-neighbor scaling
in a resizable window. It is a hybrid source port: a pinned 65C816 core
executes bounded original routines while timing, asset decoding, simulation
orchestration, rendering, audio output, and presentation are native C++.

## Quick start (Windows)

The tracked prebuilt executable can be launched directly:

```powershell
.\dist\StarFoxEnhanced\starfox_pc.exe
```

To rebuild the executable from source, first build the pinned UltraStarFox
source so these local inputs exist:

```text
upstream-ultrastarfox/SF.SFC
upstream-ultrastarfox/SYMBOLS.TXT
```

Then run:

```powershell
.\play-starfox.ps1
```

The launcher configures an optimized build on first use and starts at the
title screen. A development map can be selected explicitly:

```powershell
.\play-starfox.ps1 LEVEL1_1
```

The Windows build embeds those inputs by default. Explicit external files can
still be passed to the executable for development and source-to-port
comparisons.

## UltraStarFox source setup

The required revision is pinned in `config/upstream.json`:

```powershell
git clone https://github.com/Sunlitspace542/ultrastarfox.git upstream-ultrastarfox
git -C upstream-ultrastarfox checkout 270e959a47d82240d9290a6c6630032c9ec53ff5
powershell -ExecutionPolicy Bypass -File tools/build_upstream.ps1
```

The UltraStarFox DOSBox assembler toolchain must be present in that checkout,
as described by its own build instructions.

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
| Select | Backspace | Back/View |
| Start | Enter | Start/Menu |

Select+Start exits the PC runtime.

Xbox/XInput controllers, Steam Input virtual controllers, and the Steam Deck's
built-in controls are detected automatically. Both the D-pad and left stick
move by default; Deck back paddles and all other exposed controls can be
assigned from CONTROLLER REMAP.

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
at 20, 30, 60, 90, 120, 240, and 360 FPS.

The port consumes the exact assembled models and fixed-point state, but it is
not a cycle-accurate SNES emulator. Its software renderer reproduces the
source projection, clipping, face order, scan conversion, sprite priority,
and indexed palette behavior while the extra frames are newly interpolated
presentations. See `docs/ARCHITECTURE.md` for the subsystem boundary and test
strategy.

Useful diagnostics include `starfox_stage_trace`, `starfox_stage_preview`,
`starfox_shape_coverage`, and `starfox_planet_probe`. Third-party revisions
and licenses are recorded in `THIRD_PARTY_NOTICES.md`.
