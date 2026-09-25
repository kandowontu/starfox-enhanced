# EX 6-2 daylight coast

BG_6_2 now selects dedicated daylight coastal panorama 15 (resource 215),
not a night coast or an unrelated mountain scene. The source's low green
islands, blue daytime sky and white maritime clouds informed the composition.
The original water remains separate, with measured atlas horizon 352.
Original backgrounds are unaffected. The matching menu choice 1 was assigned
after separate layout verification below.

PNG, 24-bit BMP and the built-in image-generation prompt are preserved in
`assets/enhanced-backdrops/day-coast-v1.*`. Windows resources, portable embedding,
fallback loading and decode tests include the asset. The existing immutable,
lazy-upload path is reused; no new render pass or sampling cost was added.

Windows build and focused terrain/assignment and asset-decode tests pass.
Actual native-1x EX LEVEL6_2 tick-1000 GodMode captures, Enhanced Sky only:
`tmp/ex-day-coast-gpu-sep20` (16:9 and 32:9) and
`tmp/ex-day-coast-cpu-sep20` (32:9). Both GPU images were visually inspected.
The 32:9 software/GPU images differ at one pixel. Compared with the native
capture in `tmp/ex-pebble-source-sep20`, every pixel from screen row 104 down
(water and HUD) is unchanged. This verifies the sampled
layout, not every camera angle, late-stage palette event, or VR presentation.
The shared live-palette response remains in use. No release was pushed.

## Matching menu preview

Fresh source captures in `tmp/ex-menu-layout-next-sep20` establish choice 1
as the same daylight island/cloud scene, but its menu atlas starts water at
row 432 rather than gameplay's 352. Rows 428/431 contain cloud and island
colors; row 432 is the first patterned cyan water row. Choice 1 now explicitly
selects resource 215 and horizon 432, retaining origin 320.

Reference-phase 32:9 captures in `tmp/ex-menu-day-coast-{gpu,cpu}-sep20`
have both Enhanced Sky and Enhanced Ground enabled. The GPU image was viewed;
CPU/GPU differ at three pixels. All pixels from screen row 159 down match
the native source exactly, confirming the menu water is not enhanced terrain.
Windows build and layout/mapping tests pass.

Choices 10, 17 and 18 were also captured and visually inspected in the source
folder: respectively low green rocky ridges over brown ground, an asteroid
belt, and unique face planets. They are not aliases of this coastal artwork;
their enhanced coverage is still a separate task.
