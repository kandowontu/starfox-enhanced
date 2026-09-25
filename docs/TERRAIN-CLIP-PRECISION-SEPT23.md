# GPU clipped-edge precision — September 23

The merged-terrain CPU-reference stress fixture revealed a pre-existing
continuous-clipping defect hidden by equal adjacent triangle colors. At
4× scale, a left-clipped vertex should land at Y=98.625 (stored Y=394.5).
The GPU instead emitted 98.624992370605469, selecting the preceding scanline.
That pixel could retain the neighboring triangle's mean-depth metadata:
682.666687 instead of 725.333313. Ordinary RGB comparisons missed it.

## Fix

The compensated high/low float pair does not preserve all source binary64
bits through clipping intersections. The existing software-emulated binary64
polygon intersection path now also handles screen-clipped continuous
polygons, not only raw-coordinate backface-culling cases. Interior polygons
retain the existing path. Backface rejection remains explicitly conditional;
enabling exact clipping must not silently enable culling.

The shared source was regenerated into DXIL/SPIR-V/Metal shader payloads.
This uses portable integer arithmetic, not optional hardware float64 support.

## Evidence

- Isolated original failure: `tmp/terrain-edge-isolated13-sep23.log`.
- `starfox_gpu_model_check ... 4 --terrain-merged` now passes 72 views,
  9,336,298 nonzero pixels with exact coverage/palette ownership and the
  original normal/depth tolerances. D3D12 and Windows Vulkan both pass:
  `tmp/terrain-edge-fixed2-sep23.log`, `tmp/terrain-edge-vulkan-sep23.log`.
- Broader D3D12 mixed batches: 32 real models / 384 images match software,
  including five-layer composition, black writes and independent surfaces:
  `tmp/terrain-edge-models-sep23.log`.
- All 96 merged/unmerged terrain cases still preserve ray geometry,
  reflection materials and raster metadata:
  `tmp/terrain-edge-batch-fixed-sep23.log`.
- Native Linux rebuild and the same 72-view CPU/Vulkan-Lavapipe comparison
  pass, with zero measured depth error: `tmp/terrain-edge-linux-sep23.log`.
- Ordinary Android rebuild succeeds and its APK passes the full 35-backdrop
  asset/library/exclusion check. It remains local and debug-signed; no device
  installation or physical performance acceptance is implied.

No oracle tolerance was relaxed. The diagnostic checker supports
`STARFOX_TEST_TERRAIN_FACE=13`, `STARFOX_TEST_MODEL_VIEW=0`,
`STARFOX_TEST_MODEL_SCALE=4` and `STARFOX_TEST_TRACE_GEOMETRY=1` to isolate
and print the offending triangle. These hooks do not affect normal gameplay.

## Final-build runtime A/B

Repeated the batching benchmark with this precision fix in **both** paths,
using the same settings and 1,200-frame runs described in
TERRAIN-BATCHING-SEPT23.md. No builds or captures overlapped these runs.

| Scene | Unbatched median / p95 | Batched median / p95 |
| --- | ---: | ---: |
| Original 1-1 | 14.783 / 16.070 ms | 5.563 / 6.913 ms |
| Original 2-3 | 15.215 / 16.919 ms | 5.750 / 7.374 ms |
| EX 1-1 | 15.489 / 16.645 ms | 5.693 / 7.121 ms |
| EX 2-3 | 15.588 / 17.101 ms | 5.761 / 7.213 ms |

Raw logs: `tmp/terrain-precise-{unbatched,batched}-profile-sep23`.
The batching improvement remains 62–63% in median work. Batched p99 is
7.609–8.247 ms, but maxima range from 20.202 to 95.468 ms. Those isolated
long frames are **not** declared fixed; further per-frame timing attribution
is needed. This is neither a universal 60 FPS guarantee nor weak-device
acceptance, and the exact-clipping work has a measurable cost versus the
earlier less-accurate shader.
