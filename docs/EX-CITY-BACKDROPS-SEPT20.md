# EX night-city background coverage

## September 23 detailed moons completed

EX preview 9 now replaces all six registered disks with cached cratered
surfaces, retaining their native centers/radii and separate live source
palettes. Gray/purple bodies use CGRAM 88/87/86; the blue body uses 82/83.
Spatial ownership overrides shared celestial ink only inside those disks;
the same blue ink elsewhere remains an untouched star. Positions never
repeat into widescreen margins. The two-slot immutable atlas is uploaded
once; palette/scroll updates remain small metadata changes.

Mode 8 shares its catalog and sampling/color rules between CPU, portable GPU
and DXR reflection code. It retains landscape-only coverage, protected layers
and fractional EX menu scrolling. Choice 11 remains the distinct dense city.

Current proof:

- Windows/Linux application builds, backdrop and terrain tests pass.
- CPU fixtures exercise every disk, all six layer tags, shared-ink stars and
  Enhanced Sky OFF. Geometry tests exercise unique positions and palette slots.
- Portable effects parity passes on Windows and native Linux Vulkan/Lavapipe,
  including all six bodies, palettes, rolls, fades, styles and motion settings.
- Hardware DXR checks pass, including the new palette response. This is not
  a separately photographed ray-reflected city scene.
- Final 32:9 GPU/software images for choices 9 and 11 are pixel-identical:
  `tmp/city-moons-final-{gpu,cpu}-sep23/choice-{9,11}-final.bmp`.
- Comparing choice 9 against `tmp/city-moons-before-sep23`, exactly 973 pixels
  change and **zero** lie outside the native moon disks. The independent mask
  uses measured atlas centers and actual displayed scroll (171,201), not raw
  PPU Y=200. Only two disks are visible at this scroll; unit tests cover all six.
- `tmp/city-moons-off-sep23` is pixel-identical to the native pre-change capture.
- `tmp/city-moons-motion-sep23` passes 294 uniform fractional steps at 120 FPS.
  Initial and later rendered menu images were inspected.
- Current ordinary Android APK builds with -O2 and all 35 backdrop resources
  pass payload verification. It remains local/debug-signed, not installed.

Logs: `tmp/city-moons-{build,effects,dxr,linux-build,linux-tests,android,android-payload}-sep23.log`.
No release, headset deployment or overall goal completion is implied.
The native-only status in the following historical entry is superseded.

## September 23 remaining celestial evidence

Fresh native atlas/snapshot capture is in `tmp/city-moons-native-sep23`;
current enhanced preview is in `tmp/city-moons-before-sep23`. Choice 9 still
preserves native small moons, not upgraded celestial artwork. The atlas has
six distinct bodies: blue at (24,176), radius 8; purple/gray at (56,192),
(376,192), (504,112), radius 4; (424,208), radius 8; and (400,232), radius 16.
The shaded disks do not fill their rectangular tile regions, so component
bounds alone must not move their centers or inflate the small moons.
The blue moon shares indices 82..84 with stars; a blanket palette replacement
would erase those stars. Any replacement must use spatial ownership and
preserve all six unique objects without making tiled copies.

This records the next implementation requirement, not a completed upgrade.

## Correction: distinct cities

The shared artwork described below is superseded for choice 11. Comparing
the original-ROM `choice-9.png` and `choice-11.png` in
`tmp/ex-menu-reference-bg2-all-sep20` confirms different scenery: 9 is a
distant star/moon skyline, 11 a dense closer city with plain navy sky.
Choice 11 now uses `dense-city-v1` (resource 218), preserving origin 312.
Choice 9 retains resource 206 and its independent celestial layer.

Current proof: `tmp/ex-distinct-cities-sep20` (both 32:9 enhanced previews),
then `tmp/ex-city11-zenith-sep20` (11 with corrected uniform top edge).
All were visually inspected. The final edge fix changes 36730 upper pixels
and zero pixels at/below screen row 100. Windows build, mapping tests and
asset decode tests pass. Earlier CPU parity below belongs to the old shared
artwork, not the new asset; do not present it as proof for the replacement.

## Earlier implementation history

Choice 9 now uses city-night-v1 with the source atlas horizon at row 360.
The original independent scroll is retained. Source atlas inspection found
stars at palette indices 82..84, moon shades 86..88 and black tile fill 81.
Only star/moon ink is protected from photographic replacement. Existing
unique-region suppression continues to prevent wrapped duplicates.

Fresh 32:9 reference-phase captures:
- `tmp/ex-city-source-sep20`: native choice 9.
- `tmp/ex-city-final-gpu-sep20`: enhanced choices 9 and 11.
- `tmp/ex-city-final-cpu-sep20`: matching software captures with Enhanced
  Ground additionally enabled to verify menu suppression.

Both enhanced pairs match pixel-for-pixel. Choice 9 was visually inspected:
original colored stars and moons remain over the replacement skyline without
black rectangles. Current Windows build and focused mapping/asset tests pass.

Choice 11's earlier comparison exposed a photographic sampling issue: when
the view extends above the texture, its clamped top-row stars form vertical
streaks. Those earlier captures are not the final accepted image.

The city asset now prepares its upper eight rows once when lazily loaded:
the top row uses its mean sky radiance, smoothly blending back to untouched
artwork by row seven. This prevents star/noise extrusion without per-pixel
sampling work, extra passes, shader changes or repeated image uploads. Both
GPU and DXR consume the same prepared pixels as software. Other assets and
source PNG/BMP files are unchanged.

Final evidence: `tmp/ex-city-zenith-gpu-sep20` (9/11), and
`tmp/ex-city-zenith-cpu-sep20` (11). Final choice 11 was inspected: streaks are
gone, with normal stars below the extended zenith. Software/GPU match exactly;
every pixel from screen row 53 downward matches the prior city capture.
Tests cover constant out-of-image sky, untouched image interior, disabled
preparation and empty/single-pixel images. Current Windows build passes.
Reflected-scene and physical VR visual acceptance remain separate.
