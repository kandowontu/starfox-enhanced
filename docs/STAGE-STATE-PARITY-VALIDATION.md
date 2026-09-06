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

## Gameplay bitmap DMA phases and OAM length

The ordinary bitmap service previously copied both bitmap halves, 300 OAM bytes
and swapped pages in one operation. IRQBIT3 in both assembled ports uploads
**328** OAM bytes; the 300-byte upload belongs to FOXIRQ3's separate front-end
path. A new regression through the existing bounded-call service failed at byte
300 before the correction. Ordinary gameplay completion now copies all 328 bytes;
the front-end transfer length remains 300.

Wdc65816 now exposes `advance_gameplay_bitmap_dma_phase()` for the NTSC bitmap
DMA data path. It advances 2 -> 4 after the first 10,752 bytes, 4 -> 6 after the
second half, and 6 -> 0 after OAM/page completion. The final phase respects
NOIRQBIT3. Six synthetic sequences cover source RAM and destination VRAM wrapping,
unchanged OAM/pages before completion, the held completion gate and idle behavior.

The asset-bound transition checks execute the unmodified IRQBIT1 and IRQBIT2
routines up to STARTMUS, comparing each full VRAM image and transfer/acknowledgement
bytes. They also execute IRQBIT3's unmodified OAM DMA block and compare the complete
OAM image. Both ports pass. The first version of this test incorrectly searched
ROM using IRQBIT3's WRAM address; the fixture now performs COPY_TO_0101_L before
locating and executing the copied instructions.

This API covers bitmap/OAM/page data only. It does not replace the existing
palette, controller, scroll or audio owners, model PAL/EX IRQ chaining, or supply
a raster deadline. Existing bounded CPU calls still drain these phases
synchronously, retaining their previous completion behavior while fixing OAM.
The LEVEL7_2 live transfer timing difference is therefore still unresolved.

The final rebuilt suite passes 67/67 checks in 355.20 seconds, including both
source-DMA comparisons, the six phase sequences, ending audio and pacing/input
replays. The baseline OAM-length rejection and full log are archived in
`validation/bitmap-phase-regression-validation.txt`. The desktop executable was
rebuilt; the packaged candidate remains unchanged.

## Native instruction-boundary scheduling hook

Wdc65816 now supports an optional instruction-boundary callback with cumulative
native master clocks. It runs before instructions and at call/task return or
pause boundaries; a resume may repeat the same timestamp. The caller can opt in
to owning gameplay DMA phases, preventing bounded-call service from draining
states 2/4/6 immediately. Clearing the callback restores synchronous completion.
An observer without DMA ownership leaves that completion policy unchanged.

A native program reads TRANS_FLAG four times while a callback advances the DMA
phases at supplied clock deadlines. Both a normal call and a task paused/resumed
twice observe 2,4,6,0; the completion request first encounters a closed NOIRQBIT3
gate, then completes after the native program opens it. An unscheduled control
retains 2 for all reads, while synchronous service produces 0. Passive observation
preserves native instruction clocks, return registers and stack; the final clock
boundary is delivered as well. The existing six data-path phase sequences remain
in the same regression executable.

This is an instruction-boundary hook, not a complete SNES clock or raster engine.
It does not model changes inside an instruction or supply DMA, refresh or
translated-GSU clocks. No production GameSimulation callback is installed yet.
Deriving and integrating the live transfer deadlines remains necessary to resolve
LEVEL7_2; no fixed phase value or enemy-specific override was added. The input
collection and rendering/interpolation paths are unchanged.

The final rebuilt suite passes 67/67 checks in 314.06 seconds. The full log is
preserved in `validation/native-boundary-regression-validation.txt`; it includes
the clock/read scheduling tests, both source-DMA comparisons, input/pacing replays
and normal/MSU ending audio. The desktop executable was rebuilt; the packaged
candidate remains unchanged.

## Host instruction-clock phase trace

The full-system audit now records host routine entries and both EX direct-page
chase reads in `-host-phases.csv`. This observer is installed only by the audit;
it does not own DMA or change the desktop scheduler. Its clock origin is just
before each host gameplay tick. The native reference's clock origin is its
preceding settled transfer, so the two columns measure different work and must
not be subtracted to obtain one universal scheduling delay.

At the existing LEVEL7_2 failure (update 200, GAMEFRAME 306):

| Phase | Host native instruction clocks | Reference elapsed master clocks |
| --- | ---: | ---: |
| INIT_STRATS_L | 13,778 | 31,442 |
| UPDATE_OBJECTS_L | 45,536 | 65,412 |
| SCORPION4 transfer-word read | 105,776 | 132,186 |
| GETVIEW_L | 128,352 | 158,386 |
| DOSOUNDS_L | 159,744 | 206,184 |
| GENERATE_COLLIST_L | 196,448 | 370,384 |

The host flag is zero at all these entries. The reference remains at two
through DOSOUNDS_L and advances to four before GENERATE_COLLIST_L. The larger
gap after DOSOUNDS includes the reference's first bitmap DMA. These observations
confirm that native instruction clocks alone cannot supply the missing elapsed
timeline. Source DMA/refresh and translated work still require accounting before
integrating phase deadlines; no constant flag or enemy-specific delay was added.

The bounded audit still fails at update 200 with Y=45 versus 46, after 163,460
comparisons. Its ten pre-existing CSV traces are byte-identical to the previous
7326a61 baseline. Evidence is recorded in `validation/host-boundary-summary.json`
and `validation/host-boundary-phases.csv`. The address observer's read PCs and
clocks are checked in both ordinary and paused/resumed native execution; all
three bitmap and source-transition CTest checks pass. The most recent full-suite
result remains the preceding 67/67 run, not a new full-suite run for this observer.

## Reference elapsed-clock accounting

The pinned full-system reference now observes every existing CPU `step` and
partitions its clocks into ordinary CPU work, DMA-active work and DRAM refresh.
Generated hooks leave the pinned checkout unchanged and add no emulated cycles.
Refresh's five recursive 6+2 sequences take priority over DMA-active, avoiding
double counting. DMA-active includes arbitration/alignment and CPU cycles while
the source flag is asserted; it is not a DMA-byte counter. Ordinary CPU clocks
include wait-loop instructions and interrupt-handler instructions outside that
flag, so they are not equivalent to the host's selected native routine calls.

At GAMEFRAME 306, the elapsed reference clocks break down as follows:

| Entry | Ordinary CPU | DMA-active | Refresh | Total |
| --- | ---: | ---: | ---: | ---: |
| INIT_STRATS_L | 23,316 | 7,206 | 920 | 31,442 |
| UPDATE_OBJECTS_L | 54,258 | 9,234 | 1,920 | 65,412 |
| GETVIEW_L | 138,944 | 14,802 | 4,640 | 158,386 |
| DOSOUNDS_L | 182,458 | 17,686 | 6,040 | 206,184 |
| GENERATE_COLLIST_L | 250,062 | 109,442 | 10,880 | 370,384 |

DMA-active work is already present before IRQBIT1, while the first bitmap
transfer accounts for a much larger increase afterward. Adding only bitmap
payload clocks to the host counter would therefore omit other elapsed costs.
Even subtracting DMA-active and refresh leaves an ordinary-CPU discrepancy:
GETVIEW_L starts at 138,944 reference CPU clocks versus 128,352 host instruction
clocks. The next scheduler work must account for both peripheral stalls and
the source execution omitted by the host's selected-call sequencing.

`tools/reference/verify-clock-parts.py` checks matching phase identities, exact
partition sums, nonnegative monotonic category clocks between transfer resets,
complete 40-clock refresh sequences and coverage of every category. It passes
2,052 EX and 3,077 Original observations. Four negative controls (extra CPU
clocks, doubled refresh, missing phase and wrong phase identity) are rejected.
The EX run retains all eleven pre-existing CSV traces byte-for-byte, including
the same update-200 failure. Original LEVEL2_1 passes 300 updates / 562,428
comparisons; EX still fails after 163,460 comparisons. The incomplete-run guard
also passes. No production runtime changed in this accounting addition.

Evidence: `validation/reference-clock-parts-summary.json` and
`validation/reference-clock-parts-failure.csv`. Reproduce using:

```powershell
cmake --build tmp/full-reference-build -j4
python tools/reference/verify-gameplay.py --case ex:LEVEL7_2 --case original:LEVEL2_1 --updates 300 --video-frames 5000 --output tmp/clock-parts-audit
python tools/reference/verify-clock-parts.py tmp/clock-parts-audit/ex-LEVEL7_2 tmp/clock-parts-audit/original-LEVEL2_1 --output tmp/clock-parts-audit/accounting.json
```

The gameplay command currently returns failure for the known EX difference;
successful clock accounting does not override that result or establish parity.

## Strategy dispatcher instruction comparison

An optional final `INSTRUCTION_GAMEFRAME` argument on `full_reference` records
the selected frame's interval from UPDATE_OBJECTS_L (included) to GETVIEW_L
(excluded). Native rows are instruction entries; host rows are instruction
boundaries and include synthetic return sentinels. This is opt-in diagnostic
output, with no CPU, RAM or scheduler changes.

For EX GAMEFRAME 306, the source executes 3,224 instructions. The host reports
3,097 boundaries, including eleven unexecuted return sentinels. The differing
PC counts identify the missing TRANSFER_L/DOSTRATS/STRATLP dispatch instructions
at $2294b1 and $229780..$2297b8. `NativeStrategyScheduler::tick_all` replaces this
source linked-list dispatcher with C++ calls to individual DO_STRAT_L routines.
Their source dispatch overhead therefore never enters the native clock counter.

The two additional host entries at $0c9ee5 and $0c9eea are the intentional
SETSHIP scratch/countdown-preservation patch's NOPs, not an unexpected source
branch. That patch replaces the temporary store/reload while retaining the
live clear countdown. Its timing difference must also be distinguished from
missing dispatcher execution. This trace locates the differences; it does not
yet replace the dispatcher or derive a complete transfer schedule.

All twelve pre-existing CSV traces remain byte-identical to the preceding
clock-accounting run, including the update-200 Y mismatch. Evidence is in
`validation/strategy-instruction-difference.json` and
`validation/strategy-instructions-frame306.csv`. The trace starts at the same
UPDATE_OBJECTS_L address in both engines and has monotonic clocks within each
engine. Reproduce with the existing EX inputs and:

```powershell
tmp/full-reference-build/full_reference.exe tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL7_2 5000 tmp/strategy-clock-306 source 300 first-transfer 306
```

The command still reports the known gameplay failure; this is diagnostic
evidence toward its correction, not a passing parity result.

## Source strategy dispatcher integration

The normal strategy update now executes the cartridge's LDX ALLST / STRATLP
block through its PLB/RTS. The adapter verifies the compiled entry and its
DO_STRAT_L / REMOVEDEADAL_L call sites before using them. Pauses occur at those
dispatcher call sites, so calls made inside a strategy are not mistaken for
another top-level object update. Native register and stack state continue
between pauses. The partial-routine entry supplies the saved caller DB byte
above its near return address; a native PLB/RTS test verifies restoration and
that artificial stack setup adds no native execution clocks.

The bridge retains the port's once-per-generation update guard and imports
objects at each settled strategy/removal boundary. That prevents a removed
tail's recycled cursor from repeating completed player/boss logic and preserves
presentation identity when a freed slot is reused. The existing narrowly scoped
corrupt-path recovery is shared with individual-object dispatch; after cleanup,
the source loop resumes at an unvisited live object. Its regression now covers
both individual dispatch and the whole source loop. EX's explicit no-objects
entry retains its existing protected-object implementation.

The first prototype passed 65/67 CTest checks but failed the two existing
self-removal checks. Restoring the once-per-generation guard fixes both; the
Original whole-loop corrupt-path fixture also passes. These tests are retained,
not weakened to accept repeated strategy execution.

The corrected EX GAMEFRAME-306 trace matches every instruction count in the
source dispatcher range $229780..$2297b8 after deduplicating repeated pause/resume
boundaries. Remaining count differences are the existing SETSHIP NOP patch,
unexecuted host return sentinels and a source caller instruction outside the
dispatcher. Eleven pre-existing native/gameplay CSV traces remain byte-identical.
The known Y=45 versus 46 discrepancy still occurs at update 200, after 163,460
comparisons. Source instruction execution alone does not schedule the live
bitmap DMA, refresh or GSU overlap, so this is not a resolution of LEVEL7_2.

Evidence: `validation/source-dispatch-summary.json` and
`validation/source-dispatch-instructions.csv`. The desktop executable is rebuilt;
the packaged candidate remains unchanged. The corrected build passes all 67
CTest checks in 421.42 seconds, including primary/multiplayer input and pacing,
both self-removal checks, corrupt-path recovery, level clears, and normal/MSU
ending audio. Logs are preserved in
`validation/source-dispatch-final-regression-validation.txt`; the initial
65/67 prototype result and separate recovery check are retained alongside it.

The intermediate source-loop build also completed the 59-stage opening audit:
58 passed, and only the existing EX LEVEL7_2 mismatch failed. This run includes
per-boundary object imports but predates the final generation guard and recovery
corrections, so it is not presented as final-build all-stage coverage. Its full
report is `validation/source-dispatch-intermediate-stage-summary.json`. The final
build's focused LEVEL7_2 trace and 67/67 regression result are separate evidence.

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
