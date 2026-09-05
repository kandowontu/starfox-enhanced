# Cartridge rendering reference

This optional Windows/MinGW tool runs each cartridge's assembled `MSHOWOBJ3` or `MSSPRITE`
routine in an independent Snes9x Super FX implementation and compares its
224 × 192, 4-bit indexed framebuffer with the port. The reference core is a
development dependency, not part of the game or its redistribution package.

Reference source: <https://github.com/libretro/snes9x>, pinned to
`890b5d445538fe790aa3add3d5702c80f551e0ae`. Review that project's license before
using or distributing it. This repository contains only our small audit bridge,
comparison program and numeric results; no reference core or cartridge image.

## Rebuild and reproduce

From the project root, with Git, PowerShell, MinGW GCC and CMake available:

```powershell
pwsh -NoProfile -File tools/reference/build-reference.ps1
$core = 'tmp/reference-snes9x-clean/libretro/snes9x_libretro.dll'
$audit = 'build/current/starfox_reference_render.exe'
& $audit $core upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT all docs/validation/reference-render-original-census.csv
& $audit $core tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt all docs/validation/reference-render-ex-census.csv
& $audit $core upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT 'SHIP_4,ANDROSSCUBE,ANDROSS,WALL1,BOSS_0_0,BOSS_1_0,BOSS_2_0,BOSS_7_0,BOSS_8_0,BOSS_9_0,BOSS_A_1,BOSS_B_0' docs/validation/reference-render-original-scenarios.csv
& $audit $core tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt 'BOXXIE,POINTYANIM,LUIGIHSBW,LUIGIH,WOLF_1BOSS,WOLFBOSS,WOLFENBOSS,WOLFEN64BOSS,SHIP_4,ANDROSSCUBE,ANDROSS,WALL1' docs/validation/reference-render-ex-scenarios.csv
```

The build script refuses unrelated modifications to the reference checkout.
The bridge resets the GSU pipeline before each isolated call and bounds it to
10 million steps. Named scenarios return nonzero if any framebuffer differs;
`all` and `all-frames` write a census without treating a difference as a process failure.

The named mode tests every animation frame, yaw angles 0/45/90 degrees and
depths 600/1700/4500. The census tests frame 0 at yaw 45 degrees and depth 1700
for unique, decodable full headers in bank 0. Its discovery is deliberately
reported as a symbol/format scan, not as a complete source asset manifest.
`all-frames` expands that census to every animation frame and all nine angle/depth
combinations. `sprites:NAME,NAME` enters MSSPRITE with signed size adjustments
-8/-1/0/1/8, X positions -200/0/200, Y=-10 and depths 129/256/600/1700/4500.
Its separate CSV schema records those inputs directly; colour/frame are zero.

After inspecting results, `python tools/reference/select-goldens.py` selects
exact reference cases (including intentional blank output) for the normal CTest suite. It does **not** erase
failed or blank cases from `docs/validation`. The checked-in golden hashes were
produced by the independent core; they must never be regenerated from the port
alone. The regular tests need only the local cartridges and symbols, not Snes9x.
A mutation restoring the old line accumulator fails these checks on `AIR_1`.

## What the comparison establishes

Both sides share the depth-table selectors recorded in each CSV row. The port
now constructs its own model matrix and verifies it against the recorded native
matrix before checking pixels. It independently decodes the geometry,
visibility, materials and textures and projects/rasterizes them. The reference
executes the actual cartridge code for that work. Pixels are hashed in row-major
order using FNV-1a 64 (offset `14695981039346656037`, prime `1099511628211`). The
CSV also records visible-pixel counts and exact colour/mask difference counts.

The fixture uses a neutral world matrix, zero translation in X/Y, no colour
animation, no explosion and no EX rendering modifiers. SRAM is restored from
the same 600-frame boot snapshot before each case. This validates isolated
model rendering, not complete scene composition,
collision, palette RGB output, sound or cycle timing. Compact headers and
runtime LOD selection require separate tests. Effects/sprite headers can need
live scene state absent here; their census differences remain unclassified
until that state is supplied. Enhanced interpolation and resolutions have
separate tests and intentionally do not match the native pixel grid.

## Camera, model and star-field comparisons

The `matrices` and `points` modes execute MCROTWMATZXY16, MSHOWOBJ3,
MSHOWSHADOW and MWMATROTP16 with 207 camera orientations, including fractional
source angles, cardinal angles and a seeded random set. Nine object orientations
exercise both direct-copy shortcuts and general composition, with and without
shadows: **3,726 model/shadow matrices and 9,729 world points per game**.
The regular tests also launch the host's translated GSU math entry points, so
both the renderer math and the CPU adapter are checked against native words.

The `dust` mode executes MINITDUST and 192 consecutive MSHOWDUST updates per
point-count setting. It covers camera movement and word wrap, rotated views,
moving vanishing points, and stars/snow/pollen. Original uses 120 points; EX
tests 120 and 511. All **579 point states (including initialization) and 576
framebuffers** match. The fixture returns through the cartridge's own RPIX/STOP
to flush its final pixel-cache tile; it does not patch cartridge instructions.

```powershell
& $audit $core upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT matrices tests/data/reference-matrices-original.csv
& $audit $core upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT points tests/data/reference-points-original.csv
& $audit $core upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT dust tests/data/reference-dust-original.csv
& $audit $core tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt matrices tests/data/reference-matrices-ex.csv
& $audit $core tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt points tests/data/reference-points-ex.csv
& $audit $core tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt dust tests/data/reference-dust-ex.csv
```

These modes return nonzero on any mismatch; their entire output is retained.
Dust point hashes consume X/Y/Z signed words as little-endian bytes, in point
order. Frame hashes consume native 4-bit pixels in row-major order. Both use
the FNV-1a constants above. An initialization row has frame=-1 and frame_hash=0.
Separate mutation builds restoring general multiplication for every model
orientation or the old dust recycling fail at the first affected reference row.
These checks do not validate cycle timing, every full scene, or physical output.

## Second GSU engine and isolated cycle measurements

The optional Ares adapter executes the same fixtures with the unmodified GSU
instruction, cache, pixel and transfer implementations from
[Ares v148](https://github.com/ares-emulator/ares/tree/0aafd85789215e84e1e43415c07d4c88461b7899),
revision `0aafd85789215e84e1e43415c07d4c88461b7899`. The build script checks the
revision and refuses a modified checkout. Only the audit targets link Ares;
the game has no Ares dependency. The adapted initialization retains its ISC
notice in `ares_gsu.cpp` and `THIRD_PARTY_NOTICES.md`.

```powershell
pwsh -NoProfile -File tools/reference/build-ares-reference.ps1
python tools/reference/verify-ares.py
```

Run `build-reference.ps1` first if the bootstrap DLL is absent. Both GSU engines
start from the same Snes9x 600-frame boot SRAM, so this is an independent GSU
comparison, not a second independent boot/CPU/PPU implementation. The verifier
compares every native result with the existing Snes9x CSV, preserving the
237 previously documented mesh-fixture differences from the port. All
**70,980 rows agree**, covering meshes, sprites, matrices, world points, dust
and grid. It also records **73,536 isolated GSU calls** in
`tmp/ares-audit/*-clocks.csv`; `summary.json` records hashes and ranges.

The Ares executable accepts the same arguments as `starfox_reference_render`
plus an optional final timing CSV path. Each timing row contains the call
sequence, entry address, R10/R11, instruction count and oscillator clocks.
CLSR and CFGR are zero, both buses are available, and each call starts with a cold
pipeline/cache. With the MARIO CHIP 1 clock source these are SNES master-clock
units; cached instructions cost two clocks at CLSR=0. Counts include completion
of an outstanding SRAM write after STOP. That completion normally occurs on
the emulator's idle coprocessor thread and is essential before reading results.
They do not automatically flush a pending pixel-cache tile.

This second implementation exposed the sprite fixture's implicit pixel flush.
MSSPRITE now returns through the cartridge's RPIX/STOP, as the dust/grid fixtures
do, giving the same golden pixels in both engines. The adapter's microprogram
checks independently count cold fetches, cache fills/hits and multiplication,
and verify pending SRAM writes, bounds and reset after an instruction timeout.

These measurements exclude CPU execution/overlap, DMA, bus contention and
video phase alignment. Live software can change CLSR and CFGR; those settings
must also be captured before applying these counts to gameplay.
**They are not a cycle-exact frame scheduler.** Summing
them or dividing by a nominal frame time is insufficient to establish the
Original pace cadence. That integration remains open.

## Live object view flags

`starfox_reference_view` samples a direct stage every ten source updates. It
copies CPU/Super FX RAM into a disposable snapshot, executes SHOWVIEW_L,
Ares MALLROTZSORT and ALIENFLAGS_L, and compares their flags with the port.
Because GETVIEW is host-translated, the fixture explicitly supplies the
current CPU WMAT11 to M_WMAT11 before the GSU transform. It does not use the
stale GSU scratch matrix left by another translated call.

```powershell
build/current/starfox_reference_view.exe upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL2_1 1800 tmp/view-original.csv
build/current/starfox_reference_view.exe tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL3_1 1800 tmp/view-ex.csv
```

The optional CTest cases sample routes 2 and 3 in both games. Before the fix,
Original LEVEL2_1 had 239 mismatches among 3,028 object views: invisible objects
retained old flags, and surviving behind-camera objects lost AFINVIEWPL/AFLEFTPL.
The source resets flags before the invisible branch, then sets the left/in-view
bits for every object taking `.dontkill`, even when AFFRONTPL is clear.
The corrected port matches these source results. A regular EX simulation
regression also checks the invisible player's flags after entering the cockpit.

The comparison uses the post-tick list: it does not establish that objects
already removed by the host should have been removed, or that the first
object's entry carry at an exact clipping boundary always matches hardware.
Nor is it a full-scene pixel comparison or a successful campaign playthrough.

The `grid` mode executes the cartridge's camera/origin transforms followed by
MSHOWGRID or EX's MSHOWGRID2, retaining the latter's line origin across updates.
It verifies the CPU's 1,920-unit origin constant directly from its assembled
SBC instruction. The GSU draws **15×15 dots in Original and 25×25 in EX**, with
two-pixel depth thresholds **512 and 96**, respectively. These values differ
from the EX checkout's source constants, so the port now reads the assembled
GSU operands. All **576 frames match (557 visible)** across camera movement,
word wrap, height, yaw and smaller pitch/roll changes. Regular tests repeat each
presentation to check that EX's line history advances only once per source frame.

```powershell
& $audit $core upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT grid tests/data/reference-grid-original.csv
& $audit $core tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt grid tests/data/reference-grid-ex.csv
```

A separate mutation restoring the 15×15 EX grid fails the first reference frame.

## Desktop audio output trace

`pwsh -NoProfile -File tools/reference/run-runtime-ending-audio.ps1 -Experience ORIGINAL -Msu 0 -OutputDirectory tmp/runtime-audio-original`
runs the actual desktop ending entry for 1,500 source seconds using dummy SDL
devices and records native/MSU music, effects and mixed-output peaks. Repeat
with `-Experience EX` for EX's standard ending and `-Msu 1` for Original's MSU
pack. The fixture populates boss/score history at FINALMAP_END; it does not
claim to have completed the preceding campaign. No prerecorded audio is saved.
The application's optional `STARFOX_TRACE_AUDIO` path enables this CSV; normal
launches perform no audio tracing. Test guards suppress saved-game writes.
Validate it with `python tools/reference/check-runtime-ending-audio.py tmp/runtime-audio-original/audio.csv original docs/validation/runtime-ending-audio-original.json`.
Use `ex` or `msu` for the other trace variants. The checker verifies the full
observation length, silence interval, audible reprise and actual mixed output.

## Inputs and measured results (2026-09-05)

SHA-256:

```text
SF.SFC       E79D7F08C9191701C43F427D8BB85D6804A4B7C05EB6E585698FD2E1B2932E32
SFES.SFC     ECA6CE47B19D14BEFE7170E613A127E4D174F24B7F9625CDF688D814DE6BD7CB
SYMBOLS.TXT  66F0124B38FBFE63913BA99E07A91D944A0479664A23DD2528CDE3D68DA62A84
starfox-ex.txt 36BA8A5CD028297FDB49C31830673433BB997167B7E489EBC983B0C416FA9FF6
```

| Set | Exact / total | Other results |
| --- | --- | --- |
| Original named scenarios | 693 / 693 | All visible |
| EX named scenarios | 198 / 198 | All visible; includes Crimson King's BOXXIE and every POINTYANIM animation frame |
| Original expanded mesh census | 15,948 / 15,957 | Nine LEXIT_0 cases |
| EX expanded mesh census | 24,630 / 24,858 | 228 reticle fixtures use the wrong source entry point |
| Original MSSPRITE | 150 / 150 | FIREBALL and LFIREBALL, 75 cases each |
| EX MSSPRITE | 1,950 / 1,950 | All 26 reticle headers differing in the mesh census |
| Mesh golden cases after deduplication | 40,578 | Original 15,948; EX 24,630; 36,641 visible |

The earlier OP_1/MYBASE_0/HUMANA/N64CON/FOXGUY differences are corrected.
LEXIT_0 is an unused legacy header with 12 vertices and shared faces that index
up to 21; the isolated native call reads stale SRAM. EX gameplay marks reticles
as asf_ssprite and dispatches MSSPRITE, verified separately above. These results
do not certify full-game 1:1 parity. All rows, including failures, are retained.
