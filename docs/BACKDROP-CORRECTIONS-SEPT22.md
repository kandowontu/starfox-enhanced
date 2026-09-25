# Fortuna and EX preview corrections — September 22

## Changes

- Fortuna keeps the source-positioned partial moon with its bright top,
  dark lower hemisphere and fade into the sky. Its sky exposure is now 0.94
  of the previous version, including the sky behind the fading moon. Ground,
  moon illumination and live palette changes remain independently controlled.
- Enhanced EX preview 19 moves the cratered body down 128 native pixels,
  from source Y 152 to 280, so it is visible under the menu's 200px scroll.
  Its original footprint is removed, including any partial offscreen copy.
  Preview 28 and gameplay retain their original position. This is a requested
  enhanced-preview adjustment, not a claim that the original atlas was wrong.
- Preview 26 uses the detailed cloud image directly rather than flattening
  it through the merged fog palette. Its menu-only exposure is 0.85. Original
  Titania gameplay still follows the live weather-change palette and fog ramp.
- Preview 36 and BG_COMET replace the incorrect cavern with open space and
  a brilliant comet corona. The original animated molten surface and flames
  remain. Comet is excluded from ground-shadow receivers. Generated artwork,
  exact prompt and provenance: `assets/enhanced-backdrops/comet-corona-v1.*`.
- Enhanced EX menu text gains a one-logical-pixel four-neighbor outline and
  maps dark lettering to white, keeping selection colors. This is implemented
  in both the GPU tile shader and software tile renderer, not a GPU readback
  fallback. Reserved presentation colors 254/255 follow the normal screen fade.
  Enhanced Sky OFF leaves the original text and backdrop unchanged.
- EX 5-4's two planets use cached cratered surfaces with their original
  gray-left/white-or-yellow-right phases and live source colors. Their source
  affine centers are stabilized independently. Shared rock inks are not
  globally treated as sky, and no extra image upload occurs as colors change.

## Verification

Initial batch: Windows/Linux application, backdrop and terrain builds/tests
pass. Hardware effects parity (including phased moons) and DXR checks pass.
Eight menu/gameplay GPU/software final pairs are pixel-identical in
`tmp/menu-corrections-{gpu,cpu}-sep22` and `tmp/comet-twins-{gpu,cpu}-sep22`.
These menu captures precede the final readability correction and are not its
acceptance proof. They exposed black text against the new comet space sky.

The new menu outline has an independent one-pixel glyph fixture, verifies
the exact four-neighbor border, dark-ink promotion and disabled path, and is
included in the randomized BG1 decode parity cases. D3D12 checker passes in
`tmp/menu-outline-background-d3d12-sep22.log`.

Final inspected 32:9 menu captures are in
`tmp/menu-corrections-accepted-{gpu,cpu}-sep22`: four pixel-identical pairs.
The neighbor lookup clips to the visible text canvas, preventing hidden atlas
tiles from drawing vertical outlines at its edges. A separate independent
offscreen-glyph fixture prevents that regression. Final D3D12 and Vulkan
background checks pass (432 cases / 183,997,440 packed samples, plus staged
composition and independent-size tests); logs have `-final-` in their names.

Original renderers remain unchanged: previews 19/26/36 with Enhanced Sky OFF
are pixel-identical to the before captures (`tmp/menu-corrections-off-sep22`).
Fortuna gameplay in `tmp/fortuna-darker-{gpu,cpu}-sep22` was inspected and
matches within one RGB value. Its source-positioned moon remains independent
of the slight sky exposure change. Windows and Linux applications build.
The final outlined-menu decoder also passes native Linux Lavapipe Vulkan
parity, including independent glyph/clip tests and the complete tile sweep.

No release or headset installation is implied. Physical VR, other celestial
replacements and all-angle/all-stage acceptance remain outside this batch.
