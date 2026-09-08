# Rendering optimization pass

Bloom was the largest measured CPU presentation cost in the tested effects-heavy
scene. This pass caches 8-bit linear-light/threshold calculations and horizontal
sample coordinates, combines the two blur fields before reconstruction, and
uses the existing persistent row workers for large-frame composition. Extraction
remains serial to avoid shared-cell write races. Alternate styles now skip edge
sampling when the selected style does not use edges. Geometry and culling are
unchanged.

## CPU benchmark

Deterministic 768x224-native RGB pattern with a 16-row excluded HUD; Heavy Bloom;
45 iterations per case, first five discarded. Same compiler with O3, saved
pre-change executable followed by the new serial implementation on this host.
Times are milliseconds, not end-to-end game frame rates.

| Render scale | Before median / p95 | After median / p95 |
| --- | --- | --- |
| 1x | 8.08 / 9.03 | 4.58 / 5.12 |
| 2x | 25.73 / 27.49 | 14.89 / 16.25 |
| 4x | 96.49 / 143.69 | 64.67 / 69.68 |

Median bloom cost fell approximately 33–43%. The mixed Comic/Thermal fixture
also retained identical pixels; its median cost fell from 1.34 to 0.91 ms at 1x,
4.91 to 4.29 ms at 2x and 23.07 to 19.57 ms at 4x.

Golden bloom hashes are now permanent pixel regressions at all three scales,
with serial/threaded equality and resize/cache reuse coverage. Existing tests
also cover HUD exclusion, alpha preservation, strengths and Off bypass.

Headless 240-FPS-requested, 32:9, 1x gameplay was exercised with Heavy Bloom.
SDL dummy video is not a GPU/device performance test; timings vary with host
load and these measurements do not establish sustained 240 FPS. Real hardware
profiling is still needed, particularly for mobile and console targets.

## Related fixes

- Keep the label BLOOM and place it immediately below Render Upscale.
- Place VSync immediately below Anti-Aliasing, preserving stable option IDs.
- Preserve the full Good luck voice before replacing the stage SPC bank.
  Original and EX tests compare the complete effect PCM to an uninterrupted SPC.
