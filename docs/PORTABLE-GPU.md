# Portable GPU effects (unreleased)

The SDL GPU backend uses Vulkan on Linux/Android and Metal on macOS/iOS.
It shares the HLSL effect algorithms with Windows D3D11. Shader translation is
offline, not a download or dependency added to the running game.

Coverage: model/world effects and intensity, smoothing, 2D polygon/overlay
filters, Enhanced Lighting, HDR, chromatic aberration, both bloom controls,
anti-aliasing, shadow-mask blending, bloom layer splitting, and legacy 1440p
model-layer separation. Supported paths present GPU textures directly, with
capture/history readback on demand. Unsupported devices explicitly log fallback.

Not covered: native geometry rasterization, portable shadow-ray tracing, and
Switch/Vita GPU effects. SDL GPU does not provide these consoles' backend paths.
Metal and Android runtime validation requires their devices/toolchains; passing
Linux tests is not evidence of those platforms working.

## Rebuilding shaders

Validated generators: DXC v1.9.2607 with SPIR-V enabled and SPIRV-Cross commit
`be71ee8c12cd7dc5ca8fa9581f708c2e8561fe2a`. The Windows SDK DXC used for DXR
does not necessarily include SPIR-V code generation.

```
python tools/generate_portable_effects.py --dxc PATH_TO_DXC --spirv-cross PATH_TO_SPIRV_CROSS
python tools/generate_portable_effects.py --check
```

Both xBRZ-enabled and minimal shader variants are generated. Preserve the
generated header files in source distributions; GPU xBRZ carries the same GPLv3
requirements as the CPU implementation.

## Checks

Build/run `starfox_gpu_effects_check` with `SDL_GPU_DRIVER=vulkan` or `metal`.
The check compares effects with CPU references and batched versus separate GPU
passes. It requires a working SDL GPU device; do not silently skip failures.
For Windows integration testing, `STARFOX_TEST_SDL_GPU=1` selects SDL GPU instead
of D3D11. `STARFOX_TRACE_GPU=1` reports direct presentation. Normal Windows
rendering remains D3D11.

Verified locally: Vulkan effect comparisons and captured gameplay smoke runs on
Windows and Linux/WSL; the Linux regression suite passed 38/38 after integration.
Floating-point effects permit at most one channel unit
of difference in the test fixtures; styles and shadow blending match exactly.
