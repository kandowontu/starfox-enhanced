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

Both sides share the cartridge's final model matrix and depth-table selectors,
which are recorded in each CSV row. The port independently decodes the geometry,
visibility, materials and textures and projects/rasterizes them. The reference
executes the actual cartridge code for that work. Pixels are hashed in row-major
order using FNV-1a 64 (offset `14695981039346656037`, prime `1099511628211`). The
CSV also records visible-pixel counts and exact colour/mask difference counts.

The fixture uses a neutral world matrix, zero translation in X/Y, no colour
animation, no explosion and no EX rendering modifiers. SRAM is restored from
the same 600-frame boot snapshot before each case. This validates isolated
model rendering, not host camera/matrix construction, scene composition,
collision, palette RGB output, sound or cycle timing. Compact headers and
runtime LOD selection require separate tests. Effects/sprite headers can need
live scene state absent here; their census differences remain unclassified
until that state is supplied. Enhanced interpolation and resolutions have
separate tests and intentionally do not match the native pixel grid.

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
