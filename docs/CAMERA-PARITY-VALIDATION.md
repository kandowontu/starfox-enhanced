# Native camera follow-up

This continues the post-0.0.4 audit. It does not certify complete game or
physical-console parity.

## Confirmed differences and correction

The source's `WMAT11` symbol addresses the **high byte** of its first matrix
word. `WMAT11W` addresses the complete little-endian word. The port stored
Q15 words at WMAT11 and read them back from the same offset, so its own
rendering math appeared consistent while native byte/word consumers saw a
different physical layout. The prior view-comparison fixture shared that
wrong starting address.

| Cartridge | Word address (WMAT11W) | High-byte alias (WMAT11) |
| --- | --- | --- |
| Original | $0016a0 | $0016a1 |
| EX | $001907 | $001908 |

Original `SF/INC/MACROS.INC`'s `copymat16` copies the `*W` symbols, and
`SF/ASM/OBJ.ASM`'s CROT/WMAT routines establish the same layout. The assembled
symbol maps agree in both inputs.

The translated camera also substituted matrix rotation and division for
GAME.ASM's byte-angle aiming helpers and PROJECTLOG_L. A sampled comparison
found crosshair-coordinate differences even when the main camera position
agreed. The runtime now calls GETVIEW_L directly and uses WMAT11W wherever
the host needs its word matrix. This preserves its camera offsets, target
angles, source aiming arithmetic and integer projection together, and removes
the duplicate camera implementation and unused cached addresses.

## Independent checks

Correcting the view fixture **before** changing the host produced 2,772
mismatches among 3,028 Original LEVEL2_1 object views. The fixed host has zero.
The before/after rows are retained as `validation/camera-view-{before,after}.csv`.
All four source view tests cover 12,204 observations at the corrected address,
with 370 invisible and 309 behind-camera observations and no differences.

The separate full-system Ares reference boots the actual cartridge, snapshots
entry RAM/registers at GETVIEW_L, and compares the disposable port CPU/GSU
bridge's outputs with the independently executed cartridge at DOSOUNDS_L.
It checks 17 camera-position, angle, crosshair and matrix words per call.

| Cartridge / direct stage | Complete camera calls | Compared words | Differences |
| --- | --- | --- | --- |
| Original LEVEL2_1 | 200 | 3,400 | 0 |
| Original LEVEL3_1 | 200 | 3,400 | 0 |
| EX LEVEL2_1 | 155 | 2,635 | 0 |
| EX LEVEL3_1 | 155 | 2,635 | 0 |
| Total | 710 | 12,070 | 0 |

Each run boots for 600 video frames, then observes 1,200 frames after direct
GAMESTART entry. All four final tool runs exit successfully. The harness uses
a fixed seed for Ares's power-on RAM pattern, neutral controller input, and
PSHIPFLAGS3 bit 3. Death/respawn is still possible; these are not campaign
completions or a complete set of player inputs/camera modes.

The tool also records 6,722 completed/superseded GSU intervals and CPU frame
boundaries. Input, tool and trace hashes and counts are in
`validation/full-system-audit-summary.json`. The frame CSVs are retained beside
that summary; full camera/GSU traces are reproducible with the commands in
[the reference README](../tools/reference/README.md#full-system-frame-and-camera-reference).
Ares remains pinned to `0aafd85789215e84e1e43415c07d4c88461b7899` (v148).

The standalone reference initially failed during shutdown because MinGW's
global destruction order destroyed its scheduler before CPU Thread cleanup.
The generated reference wrapper now initializes that scheduler first. It also
joins video conversion before unloading PPU settings. These are tool lifecycle
fixes; instruction execution and emulated frame timing are unchanged.

## Runtime validation and remaining timing work

- Full regression suite: **61/61 passed**, including both simulation/ending
  suites, SPC/MSU audio, all six Wolf encounter/pace tests and route timing.
  Logs are `validation/ctest-20260905-native-camera*.log`.
- After caching the camera entry and removing unused fields, the rebuilt
  binaries passed **9/9** affected view, hit-list and desktop smoke checks.
- The refreshed local Windows candidate has SHA-256
  `470E495CEA88EFCB36FC753325893C24E29FC29DC64DFC75013CCF83F9A85291`.
  Its `validation/native-camera-{original,ex}.bmp` images were inspected.
  They use direct LEVEL3_1, 300 source preroll updates, 180 presentations,
  Original pace, 90 FPS target/4x and unpaced dummy devices. Their displayed
  FPS is not an old-PC performance measurement.
- The user's saved pregame configuration is unchanged.

The Original input writes CLSR=1 and EX writes CLSR=0; both use CFGR=$a0 in
these traces. Ares's general GSU honors those registers. A follow-up checked
UltraStarFox's fixed-clock MARIO comment against primary hardware tests and
found conflicting evidence: see [timing profile validation](TIMING-PROFILE-VALIDATION.md).
These reconstructed-ROM timings must not be described as stock-hardware
cadence. GSU intervals also overlap CPU work, and pending SRAM writes can
complete after STOP. They cannot simply be added to CPU frame deltas.

Production Original pace still uses its documented workload approximation.
Exact scheduling, complete campaign/alternate-exit coverage, full-scene RGB
comparisons and physical-device checks remain open. The successful camera
and regression checks do not erase those limits.
