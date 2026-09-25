# EX live background palette acceptance

## September 23 independent sky/surface check

The runtime trace checker now accepts `--minimum-surface-phases` instead of
letting changing sky colors conceal a frozen surface response. With the
single-fade contract enabled, surface changes must also remain inside the
sky transition interval. Nine synthetic tests include frozen surfaces,
missing presentations, nonfinite values, separate fades and out-of-window
surface changes; all pass.

Fresh current-build native/enhanced runs of EX 7-1 start at tick 950 and cover
1,801 consecutive 60 Hz presentations with God Mode, at 16:9. The enhanced
BG 465 trace has 24 sky phases and six surface phases. Sky changes are at
1685–1751, three presentations apart; surface changes are at
1685/1688/1691/1694/1697. Both have stable intervals before/after, and the
strict check passes. This is the route's **later atmospheric section**, not
a claim of full orbital-planet or reflection-transition validation.

Native and enhanced final images at frames 1621, 1711 and 1801 were inspected
side by side: blue sky/clouds, intermediate warm haze, then orange sky. The
photographic cloud detail remains visible, and native ground/foreground/HUD
remain present. Evidence: `tmp/ex-single-fade-{native,enhanced}-sep23`.

The same stronger surface threshold was applied to the **existing September
22** traces: 5-1 has 69 sky/eight surface phases, 6-1 has 68/six, and 7-1 has
24/six. Those are reanalyses of older evidence, not new route captures.

Command: `python tools/check_enhanced_palette_trace.py
tmp/ex-single-fade-enhanced-sep23/EX-LEVEL7_1-950-16_9.log --background 465
--minimum-phases 20 --minimum-surface-phases 5 --single-run-max-gap 3`.

This strengthens the bounded palette acceptance below; full original-ROM
timing, every route/phase and reflected-background acceptance remain open.

## September 22 continuous-route evidence

`tmp/orbital-enhanced-sequence-sep22` contains 3,300 consecutive 60 Hz
presentations per route, starting at tick 950, with GodMode and Enhanced Sky.
The trace checker observes 69 sky phases on 5-1 (BG 309), 68 on 6-1 (BG 363),
and 24 on 7-1 (BG 465). The latter has exactly one contiguous change run,
frames 1685–1751 at three-presentation spacing, with stable colors before and
after. Final captures at frame 1501 for 5-1/6-1 and frame 1801 for 7-1 retain
cloud detail with their live warm palettes. These are real route-triggered
changes, not a time-based enhancement animation. This does not prove original
ROM timing, save/load or reflection behavior throughout every phase.

Cygard's previously unresolved transition is now verified at tick 950, not
the too-late passive samples described below. The corrected captures in
`tmp/cygard-horizon-fixed-gpu-sep22` and matching `-cpu-` directory show
31 native palette phases converging to FXFADECORN2 and exact agreement across
13 final-frame pairs. The source horizon is row 352. Earlier brighter-strip
captures are rejected; see EX-BACKGROUND-COVERAGE.md.

Requested stages: Sector K, Cygard, 5-1, 6-1 and 7-1. Enhanced backgrounds
must follow authored palette changes, not just display brightness. The native
backgrounds must retain the same changes. Reflections must not freeze the
enhanced sky's colors either.

## Authored route fade requirements

Cygard trigger resolved: `LEVEL6_4.ASM:239` sets `fadepaltocorn2,33`;
the later `BG_6_42_1` call switches horizontal oscillation, not the palette.
`PALFADETOCORN2_L` moves 64 colors starting at palette index 16 toward
`FXFADECORN2` (corn2.col), one RGB5 step per invocation. The port already
calls that original routine in its every-transfer group.

Fresh evidence in `tmp/ex-cygard-fade-dense-sep20`: preroll 950, 180 presented
frames at 60 Hz unlocked, captures every 3 frames, native artwork, GodMode.
60 snapshots contain 31 distinct palette phases on the same BG 0x18f.
`tools/check_cygard_fade.py` passes against the matching ROM and symbols:
all 192 RGB channels approach the authored target monotonically and the final
palette exactly reaches it. Earlier 1000+ captures were after this fade,
not evidence of a missing later trigger. Original-ROM timing comparison and
Cygard's distinct enhanced artwork remain outstanding.

Source audit: `upstream-star-fox-ex` HEAD, `SFES/MAPS/LEVEL5_1.ASM`,
`LEVEL6_1.ASM`, and `LEVEL7_1.ASM`. Preserve the starting palette and every
intermediate live CGRAM step; do not replace these with a fixed stage tint.
5-1 starts `fadepaltofxgreen,34` during ExitBase; 6-1 starts
`fadepaltofxdes,34`. 7-1 has one explicit normal-route background fade,
`FADEPALTOYAMAO,33` in part 2, with `gamepal red`. Commented-out fade
commands are not additional transitions. `fadepalrandom` in these maps is
COLORTRIP-specific, not the ordinary route's palette sequence.

Acceptance still requires start/mid/end visual samples of the same background
for each route, including the single 7-1 transition, in both original and
enhanced rendering. Preserve photographic detail and independent sky/surface
colors, and check reflected skies at the same palette phase. Existing sparse
captures and shared-response tests are not full visual acceptance of these fades.

## Current evidence, September 20

Native simulation calls PALGOTO/FADEPALTO plus EX's per-transfer, fourth-transfer
and eleventh-transfer routine groups in GameSimulation::tick. Existing EX
simulation tests cover cadence counters and NOBGMODE suppression. Native
renderer submissions contain current PPU/CGRAM, not a permanently cached palette.

Nine fresh natural-route samples are in `tmp/ex-palette-transition-audit-sep20`:
LEVEL5_1/6_1/7_1, preroll ticks 60/1000/2200, UNLOCKED timing, GodMode for
survival, 16:9, original artwork, PPU snapshots and final screenshots.

| Route | Reached BG IDs | Changed BG palette entries, 60→1000 / 1000→2200 |
| --- | --- | --- |
| 5-1 | 009 → 135 → 13b | 113 / 53 |
| 6-1 | 009 → 16b → 13b | 113 / 21 |
| 7-1 | 009 → 1d1 → 13b | 119 / 112 |

Counts compare CGRAM entries 0..127. These samples prove the port is receiving
different stage palettes, **not** that every intra-background fade/cycle is
correct: the sampled background IDs also changed. Denser sequences across
the same background and independent source references remain necessary.

## Original enhanced-path gap (implementation below)

CPU environment_effects.hpp, effects_compute.hlsl and shadow_dxr.hlsl sample
the photographic backdrop and apply display brightness (environment plane W).
They do not apply a live source-palette color response. Static photographic
textures therefore cannot currently reproduce all these transitions.

Pending: identify each named stage's actual transition ranges and participating
background palette entries; preserve independent sky/surface colors and fades;
apply equivalent live response in CPU/GPU/reflection sampling; verify saves,
scene resets and enabling Enhanced Sky mid-transition do not establish a wrong
reference palette. Do not substitute a wall-clock color animation or a single
arbitrary tint, and do not infer stage acceptance from palette cadence tests.

## Within-background captures

Fresh enhanced visual evidence: `tmp/ex-fade-visual-sep20`, all three routes
at ticks 1000/1400/1800, GPU, 16:9, unlocked timing, GodMode, Enhanced Sky,
final-target images and PPU snapshots. All nine captures completed successfully.
The palette inspector confirms three variants within BG 0x135 (5-1), three
within 0x16b (6-1), and two within 0x1d1 (7-1), with stable upload-source
contents in each group. Viewed 5-1's three images: blue, pink, green sky with
cloud detail retained. Viewed 6-1's 1000/1800 images: blue-grey to pink;
7-1's 1000/1800 images: blue to orange with cloud structure retained.
7-1's 1000 and 1400 palettes are identical; 1800 differs in 31 entries.
This establishes live enhanced color changes within the same scene, unlike
the older tick-2200 captures after a BG switch. It does not establish exact
transition cadence against an independent original-ROM run, or absence of
single-frame discontinuities between these sparse samples.

`tmp/ex-palette-source-sep20` now contains 15 natural-route samples: 5-1 at
1000/1200/1400/1600/1800/2000/2200 ticks and 6-1/7-1 at
1000/1400/1800/2200. All use GodMode, unlocked timing, native scenery and
16:9. New PPU snapshots include the background ID, VRAM3ADDR and the first
112 entries of its upload source. `tools/check_background_palettes.py`
compares only equal-route/equal-background/equal-source groups.

- 5-1, BG 0x135: six live palette variants; up to 21 entries change.
- 6-1, BG 0x16b: three variants; up to 20 entries change.
- 7-1, BG 0x1d1: two variants; 31 entries change.
- All reach BG 0x13b at tick 2200. Differences between routes there are not
  evidence of an intra-scene transition.

Crucially, VRAM3ADDR points to WRAM (0x7f1540, 0x7f1620, 0x7f1fc0), not
immutable ROM. Its contents stayed stable within the sampled groups, but that
does not prove stability across reloads, saves or menu changes. Do not use a
pointer alone as the reference-palette cache key. The enhanced response is
still pending; this evidence rules out a static-current-palette assumption.

## Shared response implemented

CPU, D3D11, SDL GPU (DXIL/SPIR-V/Metal) and DXR backdrop sampling now accept
independent sky/surface RGB responses. EX gameplay derives these from the live
upload-source palette and CGRAM using the atlas's existing sky/ground palette
classification. Sources are reread, not cached by address or captured when an
option is enabled. Pre-game previews retain their authored colors. Palette
analysis touches at most 112 colors, not every backdrop texel; no texture upload
is needed when only the response changes.

This is a regional color response, not an exact reconstruction of every
palette-index animation. Channels absent from a reference region remain
neutral (except complete black fades); shared/unknown regions remain protected.
The ROM/RAM upload source must still be verified for every named scene and
save/load transition before declaring full stage acceptance.

Validation: terrain tests cover neutral identity, independent bands, tinted
channels, black fades (including monochrome source banks), missing regions,
and serial/parallel parity. GPU effects checks pass with independently tinted
sky and surface through all tested rolls, scrolls, styles and brightnesses
(maximum allowed difference one channel value). Hardware DXR test explicitly
halves reflected image color while proving zero image-upload bytes. Windows
build and generated portable shaders compile.

Fresh enhanced captures: `tmp/ex-palette-enhanced-sep20`, EX 5-1/6-1/7-1 tick
2200, 16:9; CPU matching 5-1 capture in `tmp/ex-palette-enhanced-cpu-sep20`.
5-1 screenshot visually inspected. These captures verify composition, not all
named palette animations. Sector K/Cygard mapping, missing replacement artwork
and natural-transition visual acceptance remain open.

Route mapping is now resolved from STAGEPATHS/PLANETNAMES and the extracted
dialogue table, using `tools/list_ex_route_names.py`: Sector K is LEVEL5_3
(planet 19, message 160), Cygard is LEVEL6_4 (planet 24, message 168).
New native captures at ticks 1000/2000/3000 are in
`tmp/ex-sector-k-cygard-palettes-sep20`. Sector K retains BG 0x159 with three
palette variants (up to 59 changed entries). Cygard retains BG 0x18f with no
palette changes in this interval; its later trigger still needs investigation.

Cygard follow-up: passive ticks 4000/6000/8000 in
`tmp/ex-cygard-late-palettes-sep20`, and a 1200-presentation-frame firing
sequence sampled every 120 frames in `tmp/ex-cygard-active-palettes-sep20`,
still show one palette variant for BG 0x18f. Do not call its requested later
transition verified from these samples. Sector K enhanced gameplay is now
wired and captured; see EX-BACKGROUND-COVERAGE.md for color-response evidence.
