# Linux Vulkan ray-tracing status

The Linux SDL GPU path can now request Vulkan 1.2 buffer device addresses,
`VK_KHR_acceleration_structure`, `VK_KHR_ray_query`, and their supporting
extension through the pinned SDL 3.4.14 build. It falls back to an ordinary
Vulkan GPU device if that request fails. The bridge and capability probe are
implemented in `src/render/vulkan_ray_support.cpp` and
`tools/check_vulkan_ray_support.cpp`.

The native path uses SDL's pinned Vulkan command bridge to build BLAS/TLAS
from GPU-resident model geometry and dispatch ray-query shadows and reflections
to SDL GPU buffers consumed by the ordinary compositor. The menu reports `ON`
and offers reflective surfaces only when the ray-query device and bridge are
available; otherwise it retains the portable compute-shadow fallback.

`tools/check_vulkan_ray_support.cpp` probes the hardware path, dispatches
CPU-upload and GPU-resident shadow geometry, then dispatches a GPU-resident
reflection and checks its center pixel. On the Steam Deck (2026-09-25), it
reported `Vulkan ray-query device ready`, center shade `160`, reflection
RGBA `0xff302010`, and a reflected GPU-resident palette material of
`0xff11aa22`. The latter exercises the live material-buffer offset rather
than only a CPU-uploaded synthetic scene. A separate 30-frame Corneria GPU
gameplay run used both GPU-resident Vulkan ray-query shadows and reflections
without a declined ray pass. This proves the integration on that device but
is not a full-stage or 90-FPS performance guarantee.

LOW/MEDIUM/HIGH use one, two, or four ray queries per pixel. The reflection
pass samples model materials from the live palette and supports a ground
receiver. Background misses now sample an authored BG2 panorama rasterized
on the GPU on the same command stream. A Deck ray-query probe returned the
authored tile's palette colour (`0xffdd6633`) instead of the flat environment
fallback. Photographic enhanced-sky artwork is not yet mirrored in this
Linux pass; it reflects the underlying cartridge BG2.
