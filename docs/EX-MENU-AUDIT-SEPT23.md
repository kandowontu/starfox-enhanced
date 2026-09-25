# EX enhanced menu audit — September 23

Captured and visually inspected all 38 selectable backgrounds (0–36 and
99) at 32:9 with Enhanced Sky enabled. Evidence is under
`tmp/ex-menu-complete-sep23`; the four labeled contact sheets preserve the
original captures. This is a single-phase composition audit, not a claim
that every level, palette transition or physical device is accepted.

## Corrections

- Choices 13/14 exposed dark brown lettering that passed the old RGB-sum
  contrast test. The enhanced-menu text pass now also uses integer
  perceptual luminance, on both CPU and GPU. Dark red/brown/blue glyphs
  become white with the existing black outline; bright yellow/cyan/white
  remain colored. Native Enhanced Sky OFF behavior is unchanged.
- Choice 32 revealed a capture-fixture issue: its initial background was
  already selected, so no logic ticks followed the RAM page selection.
  The old page's tilemap was captured despite page-2 RAM. The fixture now
  always drives a real background change, cycling back when the initial
  choice already matches. Idle redraw alone left old labels behind. This
  is not a gameplay-page workaround.

## Verification

- Independent outline fixtures cover six palette colors, enabled/disabled
  behavior, four neighbors and clipping.
- Windows D3D12, Windows Vulkan and native Linux Vulkan pass the background
  checker: 432 cases / 183,997,440 packed pixel-and-coverage samples, plus
  independent layered and staged composition fixtures.
- Final enhanced choices 13/14 match GPU versus software exactly:
  `tmp/menu-luma-{gpu,cpu}-sep23`.
- Independent original-ROM choice-32 reference uses controller input,
  without ROM/RAM edits: `tmp/menu32-source-page-sep23/choice-32.png`.
  It shows the Mods page, confirming the expected source presentation.
- Rebuilt and visually checked the corrected fixture at
  `tmp/menu32-cycled-sep23/choice-32-final.bmp`: Mods labels are clean,
  with no stale Cheats-page text.

## Remaining boundaries

Choice 20 still has a native green crescent; do not count it as an upgraded
celestial body. Choice 99 intentionally has blank BG2, not missing artwork.
Choices 23/24 use procedural source-derived radial patterns rather than
photographic scenery. Full palette-transition, motion and gameplay coverage
remains separate from this menu sweep. Physical Deck, Android and VR
acceptance remains open. No build was published or installed on a headset.
