# EX special-menu starfield gaps

Captured native 32:9 reference-phase choices 19/27/28/31/99 in
`tmp/ex-special-menu-audit-sep20`, including original BG2 atlases. Visually
inspected final images and planet atlases. The existing uniqueness rectangles
prevent repeated planets, but their flat black replacement also removes stars.

Choices 19/28 share a right-half cratered planet atlas. Their replacement now
samples x-256 at the same row. Choices 27/31 sample x+256. Inspected each
replacement rectangle: it contains only original stars/black sky, not another
planet. Native atlas placement, scrolling and primary planet are unchanged.
No generated artwork, new pass or image upload is introduced by this change.

Windows build passes. Updated paired captures are in
`tmp/ex-special-menu-stars-gpu-sep20` and `tmp/ex-special-menu-stars-cpu-sep20`.
All four pairs are pixel-identical. Relative to the pre-change native captures:

| Choice | Changed pixels | Native-center changes | Previously colored pixels changed |
| --- | ---: | ---: | ---: |
| 19 | 0 | 0 | 0 |
| 27 | 6 | 0 | 0 |
| 28 | 5 | 0 | 0 |
| 31 | 6 | 0 | 0 |

Every changed pixel restores a star in an expanded margin. Choice 19's sampled
phase does not expose a gap; it shares choice 28's inspected atlas and rule,
but is not independently visually proven at other scroll phases. Choice 99 is
the authored blank scene and was not changed. These checks do not establish
enhanced planet artwork, gameplay phase coverage or VR acceptance.

## Orbital entry, choice 25

Inspected fresh native final and atlas captures in
`tmp/ex-entry-moon-source-sep20`. The unique moon at x336..391/y320..383
now also samples the star-only region 256 pixels left for suppressed copies.
The reference phase shows no changed final pixels, so it does not demonstrate
a visible star restoration. GPU/software finals match exactly in
`tmp/ex-entry-moon-{gpu,cpu}-sep20`; the GPU image was inspected.

An independent unmodified-ROM/controller-input reference was captured with
`capture_ex_menu_reference.ps1 -Choices 25 -ObserveBg2 -BackgroundOnly` into
`tmp/ex-entry-moon-reference-sep20`. The matching host layer capture is
`tmp/ex-entry-moon-gpu-reference-sep20`. The comparison tool verifies source
mode and scroll before comparing its fixed 224x190 interior region:
0/42560 pixels differ by more than 2 channel levels; maximum difference 2,
mean 0.1287. An intentionally incorrect +8-row origin changes 10976 pixels.
This is evidence for the native moon/horizon placement in this menu phase,
not enhanced artwork or every animation phase. Windows build passes.
