# Original Venom clouds — partial acceptance

Original BG_1_6A, BG_3_7A and BG_1_7B now select the existing photographic
gold-storm panorama. These source scenes share the F1 cloud atlas. The escape
variant uses the same authored row-360 horizon despite a different scroll
origin. EX assignments are unchanged.

The photographic shade ramp now accepts an exposure range in its existing
header word. Titania retains 140..245; this dark storm artwork uses 0..220.
Venom reads live CGRAM bank 5 (80..95), not Titania's bank 0. CPU, portable GPU
and DXR support the same exposure/ramp interpretation. No new texture or
additional rendering pass is required. Unit tests cover exposure limits,
invalid ranges, stage mapping, horizon and live palette replacement.

Current Windows build, terrain/palette tests, GPU-effects checks and hardware
DXR checks pass. Runtime captures in `tmp/venom-enhanced-gpu-sep20` cover
LEVEL1_6 and LEVEL3_7 at tick 1000, native 1x/16:9/GodMode/unlocked. LEVEL1_6
was visually inspected. Its matching software capture in
`tmp/venom-enhanced-cpu-sep20` is pixel-identical. Comparing with the native
capture under `tmp/venom-cloud-source-sep20`, every pixel below row 113 is
unchanged. Escape-phase visual acceptance remains pending.

## Native thunder gap — transfer restored

The original IRQ.ASM IRQBIT3 uploads PAL0PALETTE, then if FLASHBG is enabled
advances IRQRAND (RNGMODE=0) and has a 2% chance of uploading THUNDERCOL to
palette bank 5. FLASHTUNNELON similarly uses REDTUNNEL with a 20% threshold.
The host gameplay transfer now applies these conditional transfers immediately
after PAL0PALETTE, preserving tunnel-before-sky order and the exact four-byte
IRQRAND borrow chain. EX and RNGMODE=2 retain read-only RAND behavior.

`tmp/venom-thunder-source-sep20` has 180 consecutive PPU/palette captures;
bank 5 is unchanged throughout. This sample alone does not prove a missing
random event, but the omitted source transfer provides the implementation
evidence. Do not mark native thunder or all Venom lighting complete merely
because a synthetic ramp-update test passes.

`tmp/venom-thunder-fixed-native-sep20` records six flash frames (125..130)
among 180 presentations, followed by restoration of the normal palette.
Focused IRQ tests cover disabled flags, source order, RNG golden values and
read-only thresholds. All four selected IRQ/state tests pass, including the
Original and EX real-ROM state checks. A flash-specific save/load replay and
live tunnel capture remain unverified.

The intermediate enhanced captures in `tmp/venom-thunder-proof-*-sep20` are
rejected: lightning-specific cyan created contours in the continuous cloud
ramp. The final `tmp/venom-lightning-clean-{gpu,cpu}-sep20` captures retain
authored lightning strokes (palette index 91), while interpolating neighboring
cloud shades for photographic luminance. The protection is enabled only for
the cyan flash palette; normal cloud shading is unchanged. GPU/CPU images
differ at one pixel. The final GPU flash was visually inspected. Focused tests
cover the unchanged normal ramp, missing palettes, lightning shade exclusion
and preservation of source pixels under photographic replacement.

Current Windows executable and portable effects shaders are rebuilt. This
does not yet establish live tunnel-flash, escape-phase or VR acceptance.

Source activation audit: Original's only FLASHTUNNELON setters are the unused
`flashtunnel` macro and resets; no macro invocation was found. EX BGS.ASM's
invocation is commented out. Therefore a natural live tunnel flash is not an
expected acceptance event for these source builds. Do not enable that feature
just to exercise the restored conditional handler. Its threshold/order remain
covered by helper tests; a forced flag would be synthetic evidence only.
