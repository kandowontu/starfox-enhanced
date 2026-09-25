# AMD FidelityFX FSR1

Unmodified `ffx_a.h` and `ffx_fsr1.h` from:
https://github.com/GPUOpen-Effects/FidelityFX-FSR

Pinned commit: `a21ffb8f6c13233ba336352bdff293894c706575`.
Both headers retain AMD's MIT license notice. Integration shader:
`src/render/shaders/fsr1_portable.hlsl`.

Compile with DXC `-HV 2018 -T cs_6_0 -E main`; AMD's original vector ternary
helpers predate HLSL 2021 scalar short-circuit semantics. Direct3D DXIL, Vulkan
1.0 SPIR-V and SPIRV-Cross Metal 2.1 compilation have been checked. This is not
runtime GPU dispatch or visual validation. The shader uses 8x8 workgroups,
one clamp sampler, one sampled input, one storage output and a 32-byte uniform.
Stage 0 performs EASU; stage 1 performs RCAS at matching input/output dimensions.
Do not alias the input and output textures.
