# EX menu 30: enhanced red atmosphere

Source reference-phase captures in `tmp/ex-menu-space-tunnel-audit-sep20`
show choice 30 is entirely atmosphere: a narrow cloud band at screen y=124,
red haze above and below. It is not a ground/sky landscape.

Dedicated photographic-style asset red-cloud-band-v1 (resource 216) now
replaces it only with Enhanced Sky enabled. A tested full-sky menu selector
keeps choice 30 separate from landscape treatment and retains Sector K's
existing resource 207. The band tracks source Y offset from anchor 124;
horizontal scroll uses the existing source scroll path. Enhanced Ground
remains suppressed in the native menu. Other scenes are not assigned this
artwork without matching evidence.

The built-in image tool generated the asset without cartridge image inputs;
PNG, lossless 24-bit BMP and final prompt are stored in
`assets/enhanced-backdrops/red-cloud-band-v1.*`. Windows/portable resource
manifests and fallback loading include it. The cached image renderer is reused
without an additional pass or redundant loading of the Sector K image.

Windows build and focused mapping/terrain and asset-decode tests pass.
Actual reference-phase 32:9 captures with both enhancements enabled:
`tmp/ex-red-cloud-band-{gpu,cpu}-sep20/choice-30-final.bmp`.
The GPU final was visually inspected and matches software pixel-for-pixel.
This is not acceptance of full gameplay, all scroll phases, or VR coverage.
No release was pushed.

The same native audit captured 6/23/24/29: centered asteroid belt, paired
orange/magenta circles, paired green/blue circles, and fiery nebula knots.
Their distinct compositions must not inherit this cloud-band replacement.

## Original and EX gameplay 3-5

Source GSTRATS2.ASM dispatches menu choice 30 to `dobholebg`, loading holea.
Both source BGS.ASM versions use holea for BG_3_5. Unlike the menu, gameplay
defines a ground plane. The previous shared BG_3_5 assignment to dusk
mountain artwork was incorrect and now selects resource 216 instead.

Native atlases in `tmp/{ex,original}-red-band-gameplay-source-sep20`
confirm EX cloud detail ends at row 134 (uniform from 135); Original uses
the same data 256 rows lower. Sky coverage ends at 136/392 respectively.
The gameplay projection uses only the upper panorama through v=.55 at that
boundary, keeping the central cloud band above the original ground rather
than replacing the whole frame. Menu coverage remains full-sky.

Windows build and focused mapping/horizon tests pass. Tick-1000 native-1x,
32:9, GodMode, Enhanced Sky captures are in
`tmp/{ex,original}-red-band-gameplay-{gpu,cpu}-sep20`. Both GPU images were
visually inspected; each is pixel-identical to its software counterpart.
For both experiences, every pixel from screen row 105 down is unchanged
from the native source, including ground and HUD. Later stage events and
banked/VR behavior remain separate acceptance work.

## Banked follow-up

Held Right for 60 presentation frames after tick 1000 in unlocked EX, 16:9:
`tmp/ex-red-band-bank-{native,enhanced}-sep20`. Both images inspected; the
cloud/ground boundary banks together. Opposite-bank 32:9 captures are in
`tmp/ex-red-band-bank-left-sep20` and Original counterparts in
`tmp/original-red-band-bank-{native,enhanced}-sep20`. All were inspected.

The wide bank exposed clamped-top vertical streaks in the new photograph.
Asset 16 now receives the same one-time eight-row zenith preparation as the
city sky, before sealing/upload. No per-frame work was added. Windows build,
backdrop preparation and decode tests pass. Final EX opposite-bank proof:
`tmp/ex-red-band-bank-zenith-sep20` and `tmp/ex-red-band-bank-zenith-cpu-sep20`.
The GPU final was inspected; CPU/GPU differ at two pixels. The flat prepared
zenith replaces the vertical streaks. Prior banked captures predate this fix.

The Original unenhanced opposite-bank capture has a separate cyan top-right
corner. This is not fixed merely by covering it with the enhanced sky and
remains a native-background follow-up.
