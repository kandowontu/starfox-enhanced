# Special EX background reference batch

Fresh independent, unmodified-ROM/controller-input BG2 references:
`tmp/ex-special-reference-batch-sep20`, choices 6/17/18/20/23/24/29/34/36.
Matching current 32:9 native host layers and PPU snapshots:
`tmp/ex-special-host-batch-sep20`. Enhanced Sky/Ground are off.

`compare_ex_menu_reference.py` verifies mode and source scroll, then compares
the fixed 224x190 interior of the native viewport without searching for an
alignment. Choices 6/17/18/20/23/24/29/34 have zero pixels differing by more
than 2 color levels. Maximum differences are 1 or 2. Deliberately shifting
the comparison eight rows changes 2901..19458 pixels, rejecting that wrong
offset. This establishes sampled native placement, not entire animated runs.

Visually inspected original references 6, 34 and 36. Cygard's repeated motif
is in the original reference, not an accidental widescreen duplication. Its
lower patterned region must not become generic flat ground. The asteroid
field's dense central band is likewise authored. The lava cave is not a sky
panorama with ordinary terrain.

## Outstanding lava-cave color difference

Choice 36 has 2521 pixels over tolerance, maximum difference 25. Inspected
the host layer. Every mismatch is confined to rows 136..166 and four colors:

| Reference RGB | Host RGB | Pixels |
| --- | --- | ---: |
| 189,24,24 | 189,49,49 | 729 |
| 222,24,24 | 222,49,49 | 631 |
| 255,24,24 | 255,49,49 | 606 |
| 172,24,24 | 173,49,49 | 555 |

The consistent color substitution indicates a palette difference rather than
misplaced geometry. It does not yet establish whether animation phase or
incorrect palette processing causes it. Do not shift/crop the background to
hide this difference or claim full acceptance. Next work: identify these
source palette entries and compare their evolution at matched update phases.

## Lava animation follow-up

Added optional `-ExtraFrames` (0..600, default 0) to the independent reference
capture helper. This advances only the unmodified ROM after the same controller
navigation; it does not alter palette/RAM or silently realign comparisons.
Fresh choice-36 references at +3/+6/+9/+12/+60/+120/+240 video callbacks are in
`tmp/ex-lava-reference-phase-<N>-sep20`.

The red shades animate (e.g. 189/222/255 become 197/205/238), but their green
and blue channels remain 24 in all sampled phases through +240 callbacks.
Thus ordinary nearby animation phase does not explain the host's 49/49 channels.
The host palette initialization/update path needs further tracing; no production
palette values were hardcoded or changed on the basis of this observation.

Host CGRAM inspection identifies the mismatched colors as entries 57..60:
RGB5 (31,6,6), (21,6,6), (23,6,6), (27,6,6). This narrows the investigation
to four uploaded colors rather than a screen-wide color transform. A direct
WRAM-byte watch at PAL0PALETTE+114 did not return those palette words in the
reference; it must not be treated as a valid CGRAM observation. The local
reference core exposes `PPU.CGDATA[256]` but its current read-only BG2 observer
only exports geometry registers. Extend that observer for a direct palette
comparison before concluding that the renderer or native update routine is wrong.

## Direct CGRAM proof

Extended the read-only local observer with `retro_debug_cgram_word`; the helper
now saves a 512-byte `.cgram` sidecar when `--observe-bg2` is used with that
observer. Older observer builds remain supported. Rebuilt the local reference
core using Strawberry GCC (explicit CC/CXX and SHELL=cmd.exe overrides).
No emulation state or ROM was modified by this observer.

Fresh `tmp/ex-lava-cgram-reference-sep20/choice-36.cgram` confirms:

| CGRAM index | Reference word | Host word |
| --- | --- | --- |
| 57 | 0c7f | 18df |
| 58 | 0c75 | 18d5 |
| 59 | 0c77 | 18d7 |
| 60 | 0c7b | 18db |

The reference green/blue components are 3, host 6. Red components match.
This rules out photographic shaders or display RGB conversion as the cause:
the mismatch precedes rendering. Palette setup/native update tracing remains.

## Menu upload restored

Source inspection via `git show HEAD:SFES/IRQ.ASM` and `RAMSTUFF.ASM` identified
FOXIRQ3's missing CHECKSUN upload in the host transfer-state-14 replacement.
Added the source SUNFADE/COLORTRIP gating, SUNFRAME two-transfer cadence and
eight authored SUN1..SUN8 RAM tables. No RGB constants or replacement artwork
are invented. Original has no matching symbols and remains unaffected.

Windows build passes. New native captures:
`tmp/ex-lava-menu-fixed-sep20` (GPU, PPU/CGRAM) and
`tmp/ex-lava-menu-fixed-cpu-sep20` (software). Finals are pixel-identical.
Entries 57..60 now all have the correct RGB5 green/blue components 3/3.
The capture reaches SUN6 whereas the reference reaches SUN1: the previous
same-scroll comparison now reports animation differences and must not be
claimed pixel-identical to the reference. Exact phase alignment remains open.

Visually inspected the fixed GPU final and a fresh full, unmodified reference
`tmp/ex-lava-full-reference-sep20/choice-36.png`. Both have black menu lettering;
the earlier white lettering was not evidence that the restored upload broke
the original menu. Source behavior is retained rather than special-casing ink.

This fix restores the menu transfer path only. Gameplay IRQBIT3 also calls
CHECKSUN (and CHECKWATER/CHECKSECTORK), so its corresponding host paths still
need an audit; do not treat this as full-stage animated palette acceptance.
