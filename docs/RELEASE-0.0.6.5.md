# Star Fox Enhanced 0.0.6.5

GPU effects, performance improvements, and audio fixes since 0.0.6.
This is a prerelease. Packages attach automatically as platform builds finish;
publication does not mean every build or device validation has completed.

## GPU rendering and effects

- Renamed RTX Lighting to **Enhanced Lighting**, including translations. This
  shading effect is not ray-traced global illumination.
- Windows desktop GPU mode now executes model/world styles and intensity,
  smoothing, 2D filters (including polygon textures), lighting, HDR, chromatic
  aberration, bloom, anti-aliasing and shadow-mask blending in compute shaders.
- Batched late effects to reduce repeated uploads and synchronization.
- Added direct GPU presentation, separate GPU bloom/glow layers with smooth
  filtering, and GPU preparation of the legacy 1440p model layer. Screenshots
  and frame history read back on demand.
- Added shared SDL GPU Vulkan/Metal effects for Linux/Android and macOS/iOS.
  Portable shaders are generated offline; source fingerprints prevent stale
  shader assets. Separate xBRZ-enabled/minimal variants are included.
- Supported Windows x64 hardware now uses real DXR 1.1 for enhanced shadows.
  Unsupported hardware retains software shadow rendering.
- Fixed GPU resource release order on shutdown and renderer changes, preventing
  a crash when resources outlived SDL's loaded graphics driver.

## Performance and audio

- Optimized CPU shadow traversal: prepare geometry/light directions once per
  frame, avoid redundant stack clearing, visit near receivers first, and bound
  searches by the ground plane. Full resolution, eight samples and softness remain.
- Parallelized large-buffer bloom extraction while retaining accumulation order;
  smaller buffers keep the cheaper serial path.
- Reused chromatic sampling calculations and worker partitions; small indexed
  frame conversions avoid unnecessary worker wake-ups.
- Native and MSU encounter music stop on player death, including cockpit/HP-zero
  paths. Sound-effect ports remain intact and checkpoint music can resume.
- EX forwards native music-cut commands to MSU playback.
- Removed wireframe thickness; old saved overrides are ignored.
- Added repeatable GPU/CPU benchmarks and optional timing/backend diagnostics.

## Verification and limits

- Windows: 44/44 regression tests passed during development. Linux: 38/38 passed
  after portable-backend integration. The Windows full run preceded the final
  shutdown fix and portable additions; subsequent targeted GPU comparisons and
  real Windows/Linux Vulkan smoke runs passed, including capture and clean exit.
- Tested GPU styles/shadow blending matched reference pixels exactly. Tested
  floating-point effects differed by at most one channel value. Synthetic parity
  and isolated speedups do not guarantee every scene or higher in-game FPS.
- **Metal/Android device validation remains outstanding. Switch/Vita GPU effects
  and portable GPU shadow tracing are not implemented.** These retain CPU paths;
  Xbox UWP also retains its existing fallback paths.
- Scene geometry still rasterizes on CPU. Setup/touch overlays can still require
  CPU composition/readback. This is not a complete all-platform GPU renderer;
  broader gameplay and overlay-filter validation remains necessary.
- Saves/settings remain compatible. Android keeps its permanent signing key
  (version code 11); Xbox identity is 0.0.6.5; Vita package version is 00.07.
  No retail ROMs, saves or private signing keys are included.

See [portable GPU details](PORTABLE-GPU.md), [migration status](GPU-MIGRATION-STATUS.md)
and [CPU measurements](PERFORMANCE-POST-006.md).
