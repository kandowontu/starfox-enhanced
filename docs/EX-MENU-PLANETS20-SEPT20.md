# EX menu 20: unique blue cloud and green limb in expanded margins

Fresh native 32:9 capture `tmp/ex-menu-horizons-next-sep20/choice-20-final.bmp`
exposed a duplicate blue cloud formation and partial duplicate green limb on the right.
The BG2 atlas isolates them at (80,264)-(128,312) and (160,320)-(240,352).
Their same-row regions 256 pixels to the right contain only starfield.

Menu choice 20 now applies the existing unique-region resampling path to
both rectangles. Primary occurrences and the native center remain intact;
repeated copies are replaced with actual source stars, not a solid rectangle.
The blue object was initially identified as a planet. It is a cloud in the
1-4 artwork; no synthetic planet should replace it.
No other menu or gameplay scene is changed. No new shader path was added.

Windows build passes. Reference-phase 32:9 GPU/software captures in
`tmp/ex-menu-planets20-{gpu,cpu}-sep20` are pixel-identical. The GPU image
was visually inspected: one blue cloud formation, one green limb, no right-side copies.
Compared with the earlier source capture, 1654 pixels change and none are
inside the native center x=272..527. This was the native-layout fix. The later
blue-cloud-v1 photographic replacement is now mapped to choice 20 and actual
BG_1_14 gameplay while the green limb remains native. See
`docs/EX-BACKGROUND-COVERAGE.md` and
`assets/enhanced-backdrops/blue-cloud-v1.md`. VR coverage is not proven.
