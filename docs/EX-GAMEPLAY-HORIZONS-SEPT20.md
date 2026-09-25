# EX gameplay atlas horizons: 6-2 and 6-4

Runtime captures disproved the assumption that menu and gameplay atlas
placements can be shared. Source BGS.ASM identifies BG_6_2 as fortn (Pebble
Beach) and BG_6_4 as corn (Corneria Surface). Gameplay captures use 16-pixel
tiles but place their first ground rows at 352, not the menu's row 432.
Their existing enhancement origin 240 implied a row-368 horizon.

Explicit gameplay horizon 352 and palette sampling origin 224 now preserve
the first 16 rows of ground shades. Original scenes and menu offsets are not
changed. No photographic mapping was added without matching source artwork.

Source snapshots: `tmp/ex-pebble-source-sep20` (6-2),
`tmp/ex-layout-source-sep20` (6-4/6-5). Source row inspection finds patterned
water at 352 in 6-2 and a uniform index-95 ground row at 352 in 6-4; preceding
rows still contain clouds. Do not use the first uniform water row (368) as
the ocean horizon.

Enhanced water/grass plus sky captures at tick 1000, 32:9, GodMode:
`tmp/ex-layout-fixed-gpu-sep20` and `tmp/ex-layout-fixed-cpu-sep20`.
Both GPU frames were inspected. CPU/GPU differ at eight pixels (6-2) and one
pixel (6-4). Current Windows build and focused stage-isolation/layout tests
pass. This is not all camera positions, full gameplay, photographic sky
coverage, or physical VR acceptance. The native right-side ground wedge in
the 6-4 capture is also visible in the unenhanced baseline and remains to be
investigated separately.

## Right-edge isolation

The unenhanced tick-1000 capture in `tmp/ex-corn-wedge-layers-sep20`
isolates the wedge to `EX-LEVEL6_4-1000-32_9-model-27.bmp`.
The expanded BG2 layer is continuous and does not contain it. Of all captured
model slots, only slot 27 contains non-magenta pixels in x=770..799,
y=130..223 (1739 pixels). Its isolated image was visually inspected.
This disproves a missing BG2 extension at this sampled location; it does not
yet establish whether the object's projection is correct. Do not cover the
wedge with extra background fill. Final-model pose tracing now includes the
slot number so subsequent captures can identify the exact shape and pose.

The rebuilt executable and fresh `tmp/ex-corn-wedge-identity-sep20` capture
identify slot 27 as shape 53034 ($CF2A), the EX `TREE` header in
`assets/symbols/starfox-ex.txt`. At tick 1000 its camera-space position is
approximately (100,-48,36): a tree passing very close on the right, rather
than a distant background seam. Approach captures at ticks 960 and 980 in
`tmp/ex-corn-wedge-approach-sep20` were visually inspected; the same slot has
depth 2556 and 1296 respectively before reaching 36. The tree is intentionally
left visible. This resolves the sampled wedge's provenance, not a general
certification of every near-plane model projection. Windows build passed.
