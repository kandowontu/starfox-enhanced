# EX background 20: source registration

This records the investigation checkpoint. The subsequent implementation and
verification are in EX-CRESCENT-UPGRADE-SEPT23.md.

Captured the current native and enhanced EX menu background 20 at the same
independently recorded reference phase and 32:9. Native capture and PPU data:
`tmp/ex-crescent-reference-sep23`; enhanced counterpart:
`tmp/ex-crescent-enhanced-sep23`. The source has a blue gas cloud and a
separate thin, dim green atmospheric arc. A fully illuminated replacement
disk would materially change the original composition.

Exact BG2 atlas ownership, before scrolling:

| Object | Rectangle (exclusive right/bottom) | Palette |
| --- | --- | --- |
| Blue cloud | 80,264–128,312 | Bank 0 |
| Green arc | 160,320–240,352 | Bank 5 |

The arc rectangle contains 2,560 pixels. Only 352 have nonblack live colors:
ink 81: 38 pixels; 83: 20; 84: 114; 86: 24; 87: 66; 88: 90.
Live RGB5 shades are respectively (14,17,14), (10,13,10), (8,11,8),
(4,7,4), (2,5,2), (0,3,0). Inks 89–95 are black in this phase, although
some are authored interior surface shades and must not be treated as an
absent object across palette changes. Ink 95 supplies the black surround.
Nonblack arc pixels fit 162,320–238,349. Full counts and per-ink bounds:
`tmp/ex-crescent-inks-sep23.json`.

The existing enhancement upgrades only the blue cloud. Its single-body
projection cannot independently place and recolor the arc. A complete
replacement must retain two separate centers, the bank-5 live ramp,
source-aligned scrolling, and the existing no-duplicate widescreen policy;
it must not use the cloud's bank-0 palette response for both objects. Keep
the narrow illuminated rim and black interior rather than inserting a
second full bright moon. Avoid rebuilding/uploading a large atlas every
palette-fade frame; use immutable surface data with a live palette uniform.

`inspect_backdrop_palette.py --region LEFT TOP RIGHT BOTTOM` now reports
exact source ink counts, bounds and live colors. Six synthetic tests check
all bitplanes, palette selection, transparency, tile flips, map pages,
16×16 subcharacters, VRAM address wrapping and invalid regions. All pass;
the test is registered as `starfox_backdrop_region_inks` in CTest.
