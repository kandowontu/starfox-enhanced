# Enhanced terrain GPU batching — September 23

## Change

Enhanced terrain previously sent each 25-vertex/32-triangle patch through
the GPU model pipeline separately. It now combines consecutive painter-order
patches into groups of at most 250 vertices (ten patches), safely below the
byte-index limit. World positions, normals, colors, triangle order and ray
caster/material flags are retained. The software path remains unchanged.

No terrain was removed, flattened or made lower resolution. Recorded wide
Corneria/Titania frames go from 51/52 submissions to six at 16:9 and 83/82
to nine at 32:9. `STARFOX_TEST_UNBATCHED_TERRAIN` is a diagnostic-only A/B
switch, also exposed by the capture and benchmark scripts.

## Matched runtime measurements

Same current executable, NVIDIA RTX 5070 Ti Laptop GPU / D3D12, 1×, 4:3,
60 Hz presentation target, original source timing, sky and automatic ground
enabled. Each run has 1,200 unpaced presentations after 1,000 source ticks;
the first 60 presentations are excluded from distributions. These are frame
work timings, not a claim of physical-device or VSync frame-rate acceptance.
The optional DLSS runtime is disabled in both cases. No captures run during
these matched measurements.

| Scene | Unbatched median / p95 | Batched median / p95 |
| --- | ---: | ---: |
| Original 1-1 | 13.263 / 14.400 ms | 5.184 / 6.149 ms |
| Original 2-3 | 13.417 / 14.953 ms | 5.586 / 7.053 ms |
| EX 1-1 | 13.445 / 14.681 ms | 5.210 / 6.415 ms |
| EX 2-3 | 13.669 / 15.189 ms | 5.553 / 6.853 ms |

This is a 58–61% median frame-work reduction with these enhancements.
Raw logs: `tmp/terrain-{unbatched,batched}-profile-sep23`.
An earlier discovery sweep is at `tmp/overnight-{base,enhanced}-sep23`;
its last enhanced run overlaps initial capture work and is not used above.

## Correctness evidence

- Four Original 1-1/2-3 final screenshots at 16:9 and 32:9 are byte-identical
  before/after (`tmp/terrain-batch-{before,after}-sep23`).
- Four steered EX 1-1/2-3 runs at 16:9/32:9, 180 presentations each at
  120 Hz, also match: all 32 captured native/final/sparse-sequence BMP pairs
  are exact (`tmp/terrain-{batch,unbatched}-motion-sep23`). The final grass
  and snow scenes were visually inspected as well.
- Terrain unit tests independently reconstruct world positions, face ordering,
  normal/color preservation, byte-index overflow rejection and palette splits.
- New `starfox_gpu_terrain_batches_check` compares separate versus merged
  GPU submissions, including flat/banked/reversed/near-plane views, four
  terrain types and 1×/2×/4× scales. All 96 cases pass on Windows D3D12 and
  Vulkan: exact colors/coverage/layers plus surface normal/depth tolerances,
  ray vertices/order and reflection materials (excluding deliberately
  renumbered local face IDs). Logs: `tmp/terrain-batch-rays*-sep23.log`.
- Native Linux application/checker builds and terrain unit tests pass; the
  same 96 A/B cases also pass under Vulkan/Lavapipe. This verifies the
  portable path, not low-end hardware performance:
  `tmp/terrain-batch-linux-sep23.log`.

## Limits / follow-up

The optional `starfox_gpu_model_check --terrain-merged` CPU-reference stress
fixture exposed a triangle-edge mean-depth discrepancy at 4×, despite equal
colors. A subsequent precision fix now makes that original oracle pass;
see TERRAIN-CLIP-PRECISION-SEPT23.md. The measurements above precede that
separate clipping fix and must not be silently presented as its benchmark.

Physical low-end GPU, Steam Deck, Android and VR acceptance remains open.
No release or headset installation was performed. This improves the measured
terrain bottleneck, not every enhancement or every possible gameplay scene.
