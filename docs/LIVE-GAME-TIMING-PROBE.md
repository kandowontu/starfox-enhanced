# Live gameplay timing integration probe

`starfox_live_game_timing` is a development-only diagnostic target. It uses
the playable port's source stage initialization, then installs the optional
CPU timeline and live GSU binding. It prints CPU/DMA/refresh clock totals,
wall time, and the instructions accounting for the most clocks per update.
An explicit per-update clock budget turns an unbounded wait into a diagnostic
failure, with CPU PC, transfer flag and GSU registers.

The optional `native-draw` argument appends SHOWVIEW_L, BUILD_DRAWLIST_L and
DO_3D_DISPLAY_L after each existing host update. This tests whether those
native routines can execute with the initialized game state and live GSU.
It **does not** put them in source transfer order or remove duplicate host
work. Its clock totals are not a certified pace measurement. The separate
shell and appended-draw columns make that limitation explicit.

## Findings

- Both ports execute the native graphics pass when the normal cartridge
  stage initializer is used. The lower-level diagnostic constructor skips
  that initializer and is unsuitable for this check: EX LEVEL7_2 reached
  an excessive polygon scanline loop at update 9 with that incomplete setup.
  The probe now always selects the same initialization option as direct
  gameplay launches in the desktop application.
- A large second update in the lower-level constructor probe was the SPC
  bank upload (SBOOT_LOOP/SBOOT_WAIT1), not a recurring graphics stall.
- The appended graphics pass accounts for material work missing from the
  existing host-only clock totals. These measurements must not be converted
  directly to ACCURATE cadence while transfer ordering and duplicate work
  remain unresolved.
- Wall times include this diagnostic's instruction-level map accounting and
  duplicate host/native work. They are neither an optimized runtime benchmark
  nor a prediction of old-PC performance.

The 300-update Original LEVEL2_1 and EX LEVEL7_2 outputs are saved as
`validation/live-game-original-route2.csv` and
`validation/live-game-ex-route3.csv`. These are bounded execution probes,
not campaign, input, visual or independent full-system parity checks.

```powershell
cmake --build build/current --target starfox_live_game_timing -j 4
build/current/starfox_live_game_timing.exe upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL2_1 300 native-draw
build/current/starfox_live_game_timing.exe tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL7_2 300 native-draw
```

The next integration step is to place graphics work and interrupt-owned
bitmap transfers in their source order, replacing the corresponding host
operations. Gameplay still uses its existing pace modes; this diagnostic
does not expose ACCURATE or change the default.
