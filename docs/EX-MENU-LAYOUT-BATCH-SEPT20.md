# Remaining EX menu layout audit

Source, unenhanced, reference-phase 32:9 captures and PPU snapshots:
`tmp/ex-menu-layout-next-sep20` (1/10/17/18),
`tmp/ex-menu-horizons-next-sep20` (20/34/36).
All final images were visually inspected; the choice-34 source atlas was
also inspected directly.

- Choice 1 matches the daylight coast; its separate row-432 horizon and
  replacement artwork are covered in EX-DAY-COAST-SEPT20.md.
- Choice 10 is green rocky ridges over brown ground, not a desert dune scene.
  Row 359 still contains rocks; row 360 begins the ground, including a
  transparent first row followed by brown bands. Enhancement horizon now
  explicitly uses 360, not palette origin 248 plus 128 (=376). Palette sampling
  origin remains unchanged. Photographic artwork is still needed.
- Choice 17 is an asteroid belt, not a terrain horizon. Choice 18 contains
  unique face planets and already uses their specific bounds; neither should
  inherit a landscape replacement.
- Choice 20's duplicate planets were fixed separately; see
  EX-MENU-PLANETS20-SEPT20.md.
- Choice 34 repeats its Cygard pattern four times inside the native 512-wide
  source atlas itself. Repetition is therefore not sufficient evidence of an
  expansion bug. Do not erase it as if it were a lone planet. Its lower
  animated dark pattern also must not be classified as generic flat ground.
- Choice 36 is a cave/fire scene with jagged red walls and bright lava. Do not
  assign sky artwork or infer its lava boundary solely from a uniform row.

These captures narrow the remaining work; they do not certify all scroll
positions, enhanced artwork, gameplay phases or VR.

Windows build and focused layout/terrain tests pass. Choice-10 captures with
both enhancements enabled are in `tmp/ex-menu-rocky10-layout-{gpu,cpu}-sep20`.
The GPU image was inspected and is pixel-identical to software; all pixels
from screen row 160 down are identical to the unenhanced source. The existing
procedural cloud treatment is still only a fallback, not the requested final
photographic rocky-sky replacement.
