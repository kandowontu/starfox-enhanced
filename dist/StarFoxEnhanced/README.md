# Star Fox Enhanced

A native Windows PC port of the open-source
[UltraStarFox](https://github.com/Sunlitspace542/ultrastarfox) codebase. It
presents at 60 frames per second while preserving the original game's intended
NTSC simulation speed and assembled model data.

This repository is an early playable fidelity pass. The prebuilt Windows
executable in `dist/StarFoxEnhanced` is a single-file build: its pinned,
assembled UltraStarFox ROM and symbol data are embedded as Windows resources,
so recipients can run `starfox_pc.exe` without installing dependencies or
supplying adjacent data files. No GitHub release has been published yet.

## Known first-pass fidelity issues

- Bomb explosion circles are not rendered.
- Grounded objects can appear detached from the ground.
- Dialogue and pre-level General Pepper chatter are missing.
- The planet-selection screen has graphics corruption.
- Music is not currently audible.
- Gameplay lighting and color reproduction need correction; title and control
  selection colors diverge more substantially.
- The control-selection Arwing is not centered.
- The title-screen Arwing is composited in front of the team graphic.
- End-of-level points, score, and percentage tally screens are absent.
- Completed levels do not reliably return to planet selection.

## What is preserved

- Gameplay, PATH/map bytecode, strategies, collision, damage, RNG, animation,
  bosses, and stage progression update at the original deterministic 20 Hz.
- The camera and object transforms are interpolated only for the two added
  presentation frames, producing 60 Hz motion without changing game state.
- All rendered 3D geometry is decoded from the assembled UltraStarFox ROM:
  integer vertices, faces, BSP order, animation frames, LODs, shadows, texture
  coordinates, colors, and collision metadata.
- Original BG tilemaps, palettes, OBJ graphics, HUD, text, route-map sprites,
  textured planet maps, particles, dust, and SPC700 music/effects are used.
- Title, attract intro, control selection, training, route selection, all
  three routes, game over, continue, credits, pause, and stage transitions are
  connected in the native flow.

The executable renders the original 224x192 image with nearest-neighbor
scaling in a resizable window. It is a hybrid source port: a pinned 65C816
core executes bounded original routines while timing, asset decoding,
simulation orchestration, rendering, audio output, and presentation are native
C++.

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

## Fidelity boundary

“Same speed” means the intended game cadence represented by the source: one
logic/strategy update for every three NTSC presentations. Slow frames caused
only by an overloaded original MARIO/Super FX chip are not replayed; doing so
would make the port hardware-load-dependent again.

The port consumes the exact assembled models and fixed-point state, but it is
not a cycle-accurate SNES emulator. Its software renderer reproduces the
source projection, clipping, face order, scan conversion, sprite priority,
and indexed palette behavior while the extra frames are newly interpolated
presentations. See `docs/ARCHITECTURE.md` for the subsystem boundary and test
strategy.

Useful diagnostics include `starfox_stage_trace`, `starfox_stage_preview`,
`starfox_shape_coverage`, and `starfox_planet_probe`. Third-party revisions
and licenses are recorded in `THIRD_PARTY_NOTICES.md`.
