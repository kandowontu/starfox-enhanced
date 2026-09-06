# Numbered-stage state audit and transfer effects

The expanded continuous reference checks all 19 Original and 40 EX numbered
stage openings. It found four failing cases. Correcting IRQ palette flashes
and background completion brings the result to **58/59 passing**: all 19
Original stages and 39 EX stages. EX LEVEL7_2 still differs and the audit
correctly exits unsuccessfully. This is not a whole-game parity certificate.

## IRQ palette flashes

Original IRQBIT3 advances IRQRAND for each enabled tunnel or lightning effect,
even when the random byte does not select a visible flash. EX reads RAND without
advancing it. The port omitted both the effect and Original's random-number
consumption. Original LEVEL1_6 and LEVEL3_7 therefore diverged at the first
compared update: RAND was 37506 in the port and 61298 in the reference.

The host now follows the source's thresholds, palette ranges and RNG behavior
once per completed ordinary transfer. Background requests are serviced first:
they can enable a flash effect during the same transfer. Tests execute the
assembled IRQ flash block and compare all four RNG bytes and all 256 CGRAM
colours for 1,024 cases per game. Both visible effects are exercised.

## Background completion

The old `MapVm::complete_background_request()` resumed cached host map bytecode
and wrote its registers back into native RAM. During gameplay, WORLD.ASM had
already taken ownership of those registers. EX LEVEL1_3 exposed this at update
274: the map cursor/countdown diverged and the port entered a later player
initialization strategy early.

Completion now releases BG_DMALIST only. The next WORLD.ASM update, or the next
standalone host map update, owns resuming WAITSETBG. The standalone regression
checks that completion preserves newer native registers and that the host
distance counter resumes only after becoming negative.

This also exposed an incorrect EX cockpit fixture: it expected a later tunnel's
right-edge mask during the outdoor SPACE section. The corrected fixture checks
PLAYERINSPACE_STRAT, SPACE_MAXX and SPACE_PMOVELIMITAND, then asserts the source
right-edge arrow. It no longer assumes the premature transition.

## Continuous stage comparison

```powershell
cmake --build build/current -j 4
ctest --test-dir build/current --output-on-failure -j 4
pwsh -NoProfile -File tools/reference/build-full-reference.ps1
python tools/reference/verify-gameplay.py --all-stages --updates 300 --output tmp/all-stage-state-fixed
```

The all-stage mode seeds once at the first settled transfer after stage entry.
Later stages inherit a nonzero GAMEFRAME, so requiring zero would leave them
uncompared. The actual seed is recorded; native RAM is not reset to force an
entry. The original four-case mode keeps its zero-frame seed. `--case` can
select individual numbered stages for deeper investigation.

| Passing cases | Updates | State comparisons | Submitted flags | Hit-flashes |
| --- | ---: | ---: | ---: | ---: |
| Original, 19/19 | 5,700 | 9,196,584 | 77,424 | 77 |
| EX, 39/40 | 11,700 | 20,100,125 | 169,468 | 252 |
| Total, 58/59 | 17,400 | 29,296,709 | 246,892 | 329 |

Submitted flags are included in the comparison totals. Those passing cases
also complete 11,600 camera calls, comparing 197,200 words without differences.
The failed case is excluded from these completion totals. It reaches update
200 and records its first difference. The incomplete-run negative check passes.

The baseline summary preserves all four failures; the updated summary preserves
the remaining failure. Both include input, executable and trace hashes under
`validation/stage-state-{baseline,updated}-summary.json`. Raw local traces are
under `tmp/all-stage-state-{audit,fixed}`. The host's three-raster minimum applies
to 68 updates in the passing cases and remains explicit in the traces. Native
raster counts are supplied to this audit; it does not validate production pace.

Separate longer observations are recorded in
`validation/deeper-stage-state-summary.json`: Original LEVEL2_1 passes 3,000
updates and EX LEVEL6_6 passes 1,500 updates. Their observations overlap other
runs and are not additional unique campaign coverage.

A longer check of the three newly corrected cases passes 1,000 consecutive
updates each: Original LEVEL1_6 compares 1,471,312 values, Original LEVEL3_7
compares 1,940,176, and EX LEVEL1_3 compares 1,986,825. All 5,398,313 comparisons
match. The incomplete-run guard also passes. This overlaps the opening sweep;
its summary is `validation/stage-state-deeper-fix-summary.json`.

```powershell
python tools/reference/verify-gameplay.py --case original:LEVEL1_6 --case original:LEVEL3_7 --case ex:LEVEL1_3 --updates 1000 --output tmp/transfer-effects-deeper
```

## Open transfer-timing difference

EX LEVEL7_2 first differs at compared update 200 / GAMEFRAME 306. The SCORPION4
actor at native address $0501 (decimal 1281) has world Y 45 in the host and 46
in the source.
The source macro uses `0` rather than `#0`; the assembled instruction at $15cdc8
is `LDA $00`, reading the live TRANS_FLAG word. Native execution reads 2 while
the port's atomic transfer model exposes 0. From Y=48, the source's truncated
chase arithmetic consequently moves to 46 instead of 45.

The settled boundary has TRANS_FLAG=0 in both engines, which explains why a
boundary-only comparison cannot reveal the differing input. The development
tool records the actual read and saves local WRAM snapshots on the first
failure. The compact diagnostic evidence is in
`validation/stage-state-open-transfer-difference.json` and the associated CSV.
No enemy-coordinate override or masked comparison was added. CPU/IRQ/GSU
overlap and transfer-phase scheduling remain open runtime work.

An added phase observer leaves the five existing entry/gameplay/camera traces
byte-for-byte unchanged. At the failing read, 132,186 master clocks have elapsed
since the settled transfer boundary; the first bitmap IRQ starts at 216,196.
The source is therefore still in state 2 at the read. The selected phase rows
are preserved in `validation/stage-state-transfer-phases.csv`.

A separate 5,000-video-frame native-only observation continues beyond the first
host difference. It records 46 SCORPION4 reads, including both transfer words
2 and 4. GETVIEW_L is entered with states 2, 4 and 6 across this observation.
Holding a fixed 2 through strategies would therefore also differ from the
source. The unfiltered reads and phase counts are preserved in
`validation/stage-state-native-transfer-{reads.csv,summary.json}`. These are
native observations, not additional passing host updates.

## Current-runtime revalidation and affected strategies

The reference executable was rebuilt against host revision `446691c` after the
input changes. The 300-update LEVEL7_2 check still rejects update 200 with the
same world-Y difference (45 versus 46), after 163,460 comparisons and 200
matching camera calls. This is a failing bounded audit, not 300 passing updates.
The rebuilt executable hash, input hashes, failure log and differing field are
preserved in `validation/stage-state-current-446691c.*` and the companion CSV.

A source search at EX revision `b5e2d837a15a72a532cd019bfe332b7a4b660924`
identifies a second affected expression: `SCORPION1_STRAT` in GA2STRAT.ASM
line 2491 chases world Y using address `0` with divisor shift 3;
`SCORPION4_STRAT` at line 2646 uses address `0` with shift 4. Both strategy
initializers are referenced in LEVEL7_2's two repeated groups (lines 40/42
and 246/248). No other EX `s_achase_alvar` expression with this zero-address
operand was found by that source search. This bounds the identified macro
instances; it is not proof that no other instruction reads transfer state.
SCORPION1 therefore needs coverage alongside SCORPION4 when correcting the
transfer-state exposure. Hardcoding one enemy's coordinate or one flag value
would not cover the observed dependency.

## Both live chase readers observed

The expanded native observer records the two assembled `LDA $00` instructions
separately. Over 5,000 native LEVEL7_2 video frames, SCORPION1 executes 25 reads
(21 with transfer word 2, four with word 4) and SCORPION4 executes 46 reads
(38 with word 2, eight with word 4). The direct-page register is zero and the
accumulator is in word mode at every recorded read. SCORPION1's instruction is
$15ca3b and its observed object is $0573; SCORPION4 remains $15cdc8 / $0501.
This confirms that the second source expression is exercised in the assembled
cartridge, rather than merely being present in unused source.

The pre-existing native entry, frame, camera, GSU and transfer-phase CSVs are
byte-for-byte unchanged. Filtering the new read trace to SCORPION4 and removing
the appended strategy column reproduces all 46 previous rows exactly.
The observer changes neither runtime code nor the reference's execution state.

`tools/reference/verify-transfer-reads.py` validates reader presence, direct-page
word access and both observed phase values. It also rejects a missing-SCORPION1
trace and a trace flattened to constant value 2. The 71 raw reads and checked
summary are in `validation/stage-state-both-readers-*`. This remains native-only
evidence; the host mismatch is still open.

```powershell
./tmp/full-reference-build/full_reference.exe tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL7_2 5000 tmp/scorpion-both-native source
python tools/reference/verify-transfer-reads.py tmp/scorpion-both-native --output tmp/scorpion-both-native-summary.json
```

## Counterfactual isolation of the chase input

`tools/reference/diagnose-transfer-chase.py` creates a separate reference source
and executable under `tmp/zero-transfer-diagnostic`. After each of the two native
word loads, its observation hook replaces A with zero and sets N=0/Z=1. It verifies
D=0 and word accumulator mode. The load still executes with its original cycles;
no ROM bytes or production code are changed. Later execution may change as a
consequence of the substituted value. This is explicitly a counterfactual, not a
parity correction, and its summary always marks `host_parity_passed` false.

Against host revision `7326a61`, that diagnostic reaches 1,000 consecutive LEVEL7_2
updates with 1,604,355 matching state comparisons. It substitutes 25 SCORPION1
loads and 46 SCORPION4 loads; all 71 substitutions match observed load executions.
The separate normal reference was rebuilt against the same host and still rejects
update 200 with Y=45 versus 46 after 163,460 comparisons. No production fix or
comparison exemption was introduced. The input isolation exposes no additional
state difference within this 1,000-update interval; it does not establish that
these are the only mismatches in the entire stage or campaign.

The normal baseline and counterfactual summaries, logs and read/difference rows
are preserved under `validation/transfer-chase-*`. This strengthens the case for
correcting the live transfer input and preserving its varying phases. Substituting
zero or a fixed nonzero value in production is not justified by this experiment.

```powershell
python tools/reference/diagnose-transfer-chase.py --updates 1000
```

## Regression and candidate status

The rebuilt full suite passed 60/61 checks in 238.33 seconds; its sole failure
was the cockpit fixture described above. After correcting that fixture, the
EX simulation check passed in 151.02 seconds. These are two runs, not a claim
of one 61/61 run. Both logs are preserved in
`validation/stage-state-regression-validation.txt`. The runtime did not change
between them.

The Windows test executable for these runtime fixes has SHA-256
`BB39CDBE210CC3975ED7A507071E0D01D8577CE950A83B95E3D29A6B06D9CD11`.
The local candidate includes prior report-specific fixes, MIT LICENSE and
third-party notices. No release or version bump is implied.
Both copied-candidate smoke launches passed: Original LEVEL2_1 and EX LEVEL3_1,
12 presentations each with dummy devices. The three entries in SHA256SUMS.txt
match the copied executable and existing asset/music bundles. The saved pregame
configuration remains unchanged, SHA-256
`8B6727CD87174ABFCF8455D4A78E5B33CEA189E09D6F9F42A77F57489DB0A720`.

These checks cover selected native RAM, active object bytes, submitted flags
and camera state under neutral input with one initial RAM seed. Complete
campaigns, input sampling, full-scene RGB, physical console timing and the
reported older PC's actual performance are not established by these results.
