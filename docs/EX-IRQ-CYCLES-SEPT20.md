# EX animated IRQ palette uploads

Shared the missing CHECKWATER/CHECKSECTORK/CHECKSUN behavior between menu
FOXIRQ3 replacement (transfer state 14) and gameplay's post-palette IRQ upload.
Order is water, Sector K, sun; water/Sector K share bank 4, sun uses bank 3.
COLORTRIP suppresses uploads and counter advancement. Source RAM enables,
counters and the eight tables per cycle remain authoritative and saveable.
No separate presentation timer, synthetic colors or per-pixel work is added.
The selector preserves CMP/BMI byte semantics and source two/three-transfer
table holds. Original lacks these EX descriptors.

Windows build passes. IRQ tests exercise 96 transfers of all three cycles,
upload ordering/banks, table selection/wrap, disabled flags and COLORTRIP.

Live native EX captures with PPU snapshots:
`tmp/ex-irq-cycles-gameplay-sep20`, LEVEL5_3 and LEVEL6_2 at ticks 1000/1003,
16:9, unlocked, GodMode. Menu choices 2 and 36 at 32:9 reference scroll:
`tmp/ex-irq-cycles-menu-sep20`.

The new read-only `tools/check_ex_irq_palette_capture.py` compares captured
banks against all 24 tables extracted from original RAMSTUFF.ASM:

- Sector K gameplay: table 3/7 (identical bytes), then 2/5.
- Water gameplay: table 8, then 3/5.
- Menu Sector K: table 3/7; menu lava: sun6.

Thus live uploads demonstrably advance through authored data, not just unit
test inputs. Tick-1000 gameplay finals were visually inspected. This is not
exact frame-synchronized original-game timing acceptance, all palette effects,
enhanced-artwork acceptance or VR verification. CHECKTITL and remaining IRQ
behavior still need their own audit. No release/deployment performed.

Follow-up: added deterministic counter restore/replay and incomplete-descriptor
tests; they pass. Enhanced Sky captures of the same four gameplay samples are
in `tmp/ex-irq-enhanced-cycles-sep20`; tick-1000 finals were inspected.
Sector K's response incorrectly averaged unrelated star banks into the nebula
response. It now uses only the animated bank 4, excluding fixed star banks 5/6.
Fresh built result: `tmp/ex-sector-k-bank4-response-sep20`, inspected.
This removes that dilution but does not fully calibrate the red/blue photograph
to every native green/purple phase. That visual color fidelity remains open;
do not claim the enhanced nebula matches native colors based on palette upload
tests alone. Enhanced coast keeps its native animated water separate.

## Corrected nebula ownership

The bank-4 response above was an incorrect inference from CHECKSECTORK's name.
Direct palette inspection shows bank 4 contains cycling stars. Nebula ramps
are inks 8..14 of banks 5 and 6: reference red/blue becomes blue/green at tick
1000. The response now uses those 14 entries, excluding unrelated bright inks.
The earlier bank-4 result is superseded.

Fresh built captures at 1000/1003: `tmp/ex-sector-k-nebula-response-sep20`.
Tick 1000 was visually inspected and now responds green rather than remaining
red. Software counterpart `tmp/ex-sector-k-nebula-response-cpu-sep20` differs
by at most one channel level. Photographic brightness/black preservation still
needs refinement; this is not all-phase visual acceptance.
# EX title cycle follow-up

CHECKTITL now uploads both authored palette banks, using all nine table pairs,
the original byte-comparison behavior, two-call early phases, long ninth hold,
and COLORTRIP/TITLFADE gates. Source counters remain in normal saved RAM.
The host IRQ replacement and game simulation share this implementation.

Unit tests cover cadence, wrap/reset, missing tables and gating. A live title
capture at source tick 200, 128 presentations at 60 FPS, matches changing
complete source table pairs in CGRAM banks 0 and 16. The title image was
visually inspected. Proof: `tmp/ex-title-live-cycle-early-sep20`; repeat via
`tools/capture_ex_title_palette.ps1`. The checker requires distinct palette
contents, not merely multiple names for identical source tables.

The earlier tick-1000 attempt had already entered attract/demo flow and is
not title-screen acceptance. The fixture rejects that wrong flow explicitly.
