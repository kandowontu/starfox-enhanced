# Original 3-5: native banked sky corner

Reproduction: Original LEVEL3_5, tick 1000, unlocked timing, GodMode,
60 presentation frames holding Left (mask 512), native 1x, 32:9,
all enhancements off. GPU final/layer/snapshot capture:
`tmp/original-red-corner-source-sep20`. Software counterpart:
`tmp/original-red-corner-cpu-sep20`.

The isolated expanded BG2 layer shows the same top-right wedge as transparent
magenta, proving this is missing BG2 ink rather than a model or lighting bug.
The normal composite exposes the cyan backdrop there.

The Original atlas is 512 rows high and its holea artwork starts at row 256;
rows 240/248/255 sampled at x=400 are transparent, while 256/257/264 are
RGB (156,8,0). BG2 register scroll is 272. The widened, banked view reaches
the unused rows above the authored sky. EX places the same artwork at row 0,
so copying the EX offset would break Original rather than fix it.

Required fix: explicit scene-scoped top-sky continuation shared by native CPU,
GPU and reflected-background consumers, sampling the authored boundary row.
Do not globally fill transparent space tiles, copy CGRAM zero, mutate source
VRAM, or rely on Enhanced Sky to hide the problem. Original BG_3_5 is the
confirmed initial scope; other scenes need their own measured atlas bounds.
## Implemented continuation

`sky_source_min` is optional background metadata (zero disables it). Original
BG_3_5 selects row 256 each frame; EX, menus and other scenes leave it zero.
Expanded Mode-2 columns outside the native view sample that boundary instead
of lower-numbered unused rows. The rule is limited to the upper 144 screen
rows so it cannot intercept the separate wrapped-ground continuation.
Invalid bounds are ignored. The native center is never clamped.

CPU drawing, deferred CPU replay, portable GPU BG2 and DXR reflected-background
sampling carry this metadata. Portable shaders were regenerated. Source VRAM
and CGRAM are untouched; no extra texture fetch is required.

Windows build passes. The GPU background checker now includes enabled,
disabled and invalid bounds among its fixtures; all 432 background cases
and 27 independent-size/phase cases pass. Existing hardware DXR checks pass
(not a dedicated visual acceptance of this corner's reflection).

A subsequent focused hardware fixture in `tools/benchmark_dxr.cpp` creates
transparent rows above source row 256 and an opaque green authored sky at
that boundary. An offscreen +X reflection returns the fallback with the
setting off, green with the boundary enabled, and fallback again after
disabling or supplying an invalid bound. It reuses the same PPU/settings
addresses, testing metadata invalidation as well as sampling. The rebuilt
DXR checker passes this fixture and the full existing hardware suite on the
RTX 5070 Ti Laptop GPU. This is direct ray-sampling evidence, not a screenshot
of a mirror in the actual level.

Fresh matched capture proof in `tmp/original-red-corner-fixed-{gpu,cpu}-sep20`
is pixel-identical. GPU final visually inspected: no cyan corner. Relative
to the source capture, 1139 pixels changed, none within native center
x=272..527 and none below screen row 143. Other scenes and physical VR are
not certified by this targeted fix.
