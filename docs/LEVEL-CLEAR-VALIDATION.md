# Level-clear verification — 2026-09-04

## Fixes

- Restrict the Original-pace launch penalty to actual launch strategies.
  Disabling control for a clear sequence or camera change is not a launch.
  Clear strategies now use their source update clock rather than inheriting
  the 6/7-raster launch cadence. No authored map waits were shortened.
- Separate tally flow from tally visibility. Retail `CLB2=2` and EX
  `CLB2=3` stop printing before warp; `CLB2=1` exits without redisplaying the
  score/portraits during the fade. EX's different `CLB2=2` resume meaning is
  preserved. The normal warp tally holds for exactly 100 updates (5 seconds).
- The EX results screen must composite the native BG1 text/portrait bitmap.
  That presentation path previously accepted gameplay/training only, so its
  tally progressed invisibly despite passing state tests. Include visible
  results, clear the staging layer each frame, and do not let a stale gameplay
  communications flag suppress the tally.
  Its three MCOPYFACE portraits are host-composited using NAMEGFXPOS and
  the source's alive/dead frame choices; they obey the same visibility gate.
- EX `SETSHIP`/`SETSHIP2` overwrite `AL_SBYTE3` with a ship-table index. The
  Earth-clear player strategy uses that same byte for its countdown, and
  `PLAYERMOVE -> SETCURRPSHAPE -> SETSHIP` resets it every tick. This reproduced
  an endless wait in `CL_EARTH`. The CPU adapter validates the exact lookup
  instructions and removes its redundant scratch store/reload in a private
  ROM-bank copy. A already contains the index; the original table, masks,
  shape selection and registers are retained. Disk ROMs and asset packs are
  unchanged, including existing companion BINs. There is no per-tick repair
  that invents a countdown value.

## Coverage and source boundaries

`tests/level_clear_tests.cpp` runs the authored clear call and return
continuation after initializing a live level. It does not inject a completed
tally or `LEVELFINISHED` flag. Preceding boss fights are **not** played through
by this fixture. God Mode is enabled to isolate exit logic.

Ten retail and twelve EX clear entry points are exercised:

| Entry | Retail tally / end (seconds) | EX tally / end (seconds) |
| --- | ---: | ---: |
| CL_WARP | 20.933 / 30.433 | 17.617 / 28.567 |
| CL_GROUND | 14.567 / 21.417 | 14.867 / 21.667 |
| CL_SHIP1_3 | 14.017 / 21.567 | 14.950 / 21.500 |
| CL_DIVE | 17.217 / 22.567 | 12.733 / 18.033 |
| CL_EARTH | 16.317 / 23.017 | 11.933 / 18.333 |
| CL_BRIDGE | 14.633 / 21.383 | 14.783 / 21.483 |
| CL_TURN | 17.600 / 26.500 | 15.617 / 24.467 |
| CL_CHASE | 19.383 / 26.133 | 15.117 / 21.817 |
| CL_SHIP3_4 | 14.000 / 21.550 | 14.967 / 21.517 |
| CL_UNDER | 14.800 / 21.550 | 14.367 / 21.067 |
| CL_TURN2 | — | 11.900 / 20.750 |
| CL_COMET | — | 4.083 / 26.233 |

These are **fixture elapsed source times in Original pace**, measured from
clear-call entry, not target timings measured from SNES footage or from the
last shot in a complete playthrough. Level setup and surviving objects affect
the pre-clear workload estimate. The source routines themselves own the
waits, player paths, dialogue order and completion signals.

- Original pace: compare every tick's raster count, map cursor, player
  position/strategy, CLB2 and flow at 20/30/60/90/120/240/360/480 FPS.
- Unlocked pace: compare the same complete trace at 20 and 120 FPS.
- EX `FASTEND=1`: separately verify all twelve routines at 20 and 120 FPS.
- In total: 244 clear runs, plus four colony-exit runs. All timing/path traces
  match between the compared presentation rates.
- EX selector regression: 32 calls covering both selectors, eight ship
  indices and enhanced-model on/off; the selected native shape is unchanged
  and a live countdown is preserved.
- `CL_SHIP_CONT` is exercised through both ship entry points.
- `CL_COLON` is a corridor continuation, not a score screen. Retail includes
  it inline; EX calls it. Both traverse its winding corridor and reach
  `FINAL_TUNNEL` at 13.3 fixture seconds without a tally/map interruption.
- Retail `CL_WARPOUT` is the **start** of LEVEL1_3, not a post-boss clear. Its
  sole 10000-distance wait is retained and traversed by LEVEL1_3 warmup. The
  EX CL_WARPO source file contains no executable sequence.
- The final Andross escape, seven stage cards, totals/average, boss roll,
  credits and final screen have separate full-sequence tests described in
  `ENDING-VALIDATION.md`.

Source: `upstream-ultrastarfox/SF/MAPS/CL_*.ASM`, `PCSTRATS.ASM` and
`ENDSEQ.ASM`; EX equivalents at the pinned `upstream-star-fox-ex` revision,
plus `SFES/GSTRATS2.ASM`'s model selectors.

## Visual reproduction and limits

Run `tools/capture_level_clear.ps1` after building. It captures the actual
LEVEL1_2 -> CL_WARP continuation, at 60 FPS and 16:9, once per source second.
The test-only `STARFOX_TEST_CLEAR` switch requires `STARFOX_TEST_FRAMES` and
an authored call in the selected level. Neither saves nor production launch
behavior are changed.

The retail and EX captures show score/teammate portraits during tally and none in
the subsequent flying/warp frames. Native source-state tests additionally
check hide and exit signals so the overlay cannot flash back on at CLB2=1.

Original workload slowdown is still an approximation, **not cycle-exact
SNES timing**. A normal boss-to-clear playthrough and matched hardware video
remain necessary for a 1:1 audiovisual claim. Console builds have not been
played on physical hardware here.
