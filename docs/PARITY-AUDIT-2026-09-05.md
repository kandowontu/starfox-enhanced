# Post-0.0.4 parity audit

Baseline: ae2a192. Reports: oversized EX bosses (Luigi Hydra and Crimson
King), credits Start crash, delayed ending jingle absent, incorrect Sector Y
map art, one-ship Star Wolf encounter, L+R+Select route exhaustion.

The unmodified desktop baseline passed all 35 existing CTest checks in
147.70 seconds. Those checks do not establish complete game parity: notably,
the Sector Y check compares two port frames, and boss stage boot coverage
does not prove encounter completion. Source-backed regressions are being
added for the missing behavior. This file records evidence and remaining
coverage; no claim of a perfect port follows from the baseline result.

The local source checkouts and pinned assembled data are the reference. The
user's screenshots are bug evidence, not executable instructions. The existing
changes inside upstream-ultrastarfox were preserved.

## Findings and changes

| Report / audit finding | Evidence and result |
| --- | --- |
| Timer/NMI controller groundwork | Implemented comparator, hold, acknowledgement and polling-lock behavior. One million supplied-beam events yield 332,915 matching observations against unmodified pinned Ares IRQ source; three isolated mutations fail as intended. This controller still requires production CPU bus/raster integration. See CPU-PARITY-VALIDATION.md. |
| DMA address boundaries | Corrected A1T bank carry and BBAD page carry in the CPU transfer bridge. All 101 bank/fixed-address and B-bus boundary cases pass; the bank test reproduced the previous failure. This is data-path groundwork: transfer timing and the EX in-flight read remain open. See CPU-PARITY-VALIDATION.md. |
| Hardware interrupt entry groundwork | Added level-sensitive IRQ and latched NMI delivery to existing native tasks; corrected the omitted discarded fetch and internal entry cycle. All 6,912 independent native entries match, and an old-entry mutation fails all 6,912 timing comparisons. IRQ masking, NMI priority, task return and I/O read effects pass. All 62 regression checks pass. Automatic raster events and CPU/GSU transfer scheduling remain open; these APIs are not yet driven by gameplay. See CPU-PARITY-VALIDATION.md. |
| Numbered-stage opening audit | Expanded the continuous reference to all 19 Original and 40 EX stages. Fixed missing IRQ lightning/tunnel palettes and Original RNG consumption, plus background completion that resumed cached map bytecode early. All 19 Original and 39 EX openings now pass 300 updates (29,296,709 comparisons across passing cases); the three corrected cases also pass 1,000 updates each. EX LEVEL7_2 still differs because its SCORPION4 strategy reads an in-flight transfer word. The audit remains failing, with that difference preserved. See STAGE-STATE-PARITY-VALIDATION.md. |
| Object words and submitted hit-flash | A longer continuous comparison exposed AL_SFLAGS being cleared one update late and scalar AL_PTR=4 being reinterpreted as host handle 4. Preserve submitted flags separately, retain literal native object words and resolve links explicitly in host PATH operations. A related regression corrected valid upper-WRAM code being rejected as unmapped ROM. All 61 CTest checks pass; 4,000 consecutive route-2/3 updates compare 7,436,666 state values, including 62,225 submitted flags and 47 flashes, with zero differences. See OBJECT-STATE-PARITY-VALIDATION.md. |
| Gameplay fade cadence and stores | Normal IRQBIT3 advances fades once per completed bitmap transfer. The host advanced on every raster and also omitted QFADEDOWN's second decrement. Corrected gameplay/training cadence, fade-up completion edges and all three display aliases; preserved Continue's manual fade completion. Native SETINIDISP matches 768 cases per game. A new continuous, seeded reference compares 1,200 route-2/3 opening updates and 2,125,112 state values with zero differences. It fails incomplete runs and does not certify host pace or campaigns. See FADE-PARITY-VALIDATION.md. |
| Camera word layout and crosshair | WMAT11 names a matrix word's high byte; the host wrote full words there instead of WMAT11W. The earlier view fixture shared that mistake. Correcting the reference first exposed 2,772 differences among 3,028 Original route-2 object views. The host now uses WMAT11W and executes GETVIEW_L directly, preserving the source's camera offsets, target angles, byte-angle aiming and integer crosshair projection. All 12,204 sampled object views match with the corrected layout. The new full-system Ares reference independently compares complete camera calls; see CAMERA-PARITY-VALIDATION.md. |
| Full-system timing reference | Added a separately built pinned Ares SNES core with CPU frame boundaries, actual GSU launch/configuration traces and camera comparisons. The harness fixes its own MinGW scheduler lifetime and joins video before teardown; these are reference-tool changes. The pinned Original input writes CLSR=1, while EX writes CLSR=0. Both request fast multiply. Primary MC1 measurements contradict the source comment about a fixed clock and expose different multiplier/cache behavior. Added explicit diagnostic register policies and requested/effective write traces; none is claimed as a physical MC1 implementation. See TIMING-PROFILE-VALIDATION.md. Hardware identity and CPU/GSU overlap must be resolved before replacing the production slowdown approximation. |
| Native CPU execution and timing | Corrected hidden accumulator restoration, missing decimal arithmetic, SBC's wrapped-zero flag, program-bank operand wrapping, 16-bit read-modify-write order, MVN/MVP behavior and XCE. Added missing instruction/bus clocks and FastROM accounting. An independent pinned Ares CPU agrees on 12,192 instruction cases, 524,288 decimal ALU cases and 1,048,576 complete arithmetic routines. See CPU-PARITY-VALIDATION.md for the patch, reproducibility and scope. These counts exclude translated work and concurrent hardware timing; Original pacing still needs integration. |
| Object view flags | Native SHOWVIEW_L resets flags before the invisible-object branch. Native ALIENFLAGS_L still sets AFINVIEWPL/AFLEFTPL for behind-camera objects taking `.dontkill`; only AFFRONTPL stays clear. Corrected both translated paths. The before/after Original route-2 comparison goes from 239 mismatches among 3,028 object views to zero. Sampled native CPU/GSU checks cover routes 2/3 in both games; the regular EX cockpit regression also asserts the invisible-player state. |
| Isolated GSU timing audit | A second, pinned Ares GSU implementation agrees with all 70,980 reference rows and records 73,536 isolated calls. The comparison preserves all previously classified mesh-fixture differences. It exposed the MSSPRITE fixture's reliance on an implicit pixel-cache flush; both engines now execute the cartridge's RPIX/STOP. The adapter also completes pending SRAM writes before returning CPU-visible results. These development tools do not enter game packages. Full frame cadence still needs CPU overlap, DMA and video-phase accounting. |
| Huge EX models | MOBJ's word-coordinate commands do not multiply by M_SCALE; the port applied both SH_SHIFT and EX's big-head multiplier. Preserve each expanded vertex's command encoding and apply those multipliers only to byte coordinates. LUIGIHSBW was enlarged 16 times by its shift of 4; Crimson King's BOXXIE was enlarged 8 times by its shift of 3. All 9 BOXXIE and 9 LUIGIHSBW native angle/depth cases match every pixel after the fix. Correction to the earlier audit: POINTYANIM is the stage 7-4 boss, not Crimson King. ENDSEQ's boss63demo/boss63txt2 identify BOXXIE on stage 6-3; its source points are PointsXw. |
| Credits Start crash | Reproduced the reported Original UPDATE_OBJECTS_L instruction-limit failure. CREDITSMAP jumps through RESTART into a non-returning frontend. The host now stops at the frontend entry after source initialization. Held Start can also take that jump inside MAKETOTALSCORE2; handle that path before importing the reset object pool. EX runs its source fade/cleanup before its menu handoff. |
| Sector Y art | Original SPACE4 / Sector Y and Black Hole share a packed texture address. MDSPRITE selects the nibble using sprite bit 5; both translated flat and zoom drawing had always read the upper nibble. Corrected both, checked every pixel of all flat route icons, and inspected an actual runtime map capture showing Sector Y's blue/red star field and a distinct Black Hole spiral. |
| L+R+Select route crash | Source DRAWPLANETLINES_L returns at a path terminator while preserving the last destination. Preserve that endpoint instead of throwing. Also recognize EX's additional route-choice slots. The real shortcut passes at all seven EX campaign endpoints. |
| Missing jingle, MSU off | Full source ending entry through FINALMAP_END, score tally, boss roll and CREDITSMAP produces the jingle in both games: about 740.35 seconds after staff-roll entry, through 801.85 seconds. Preceding silence is 591.25 seconds in Original and 591.4 in EX. Actual desktop mixer runs now also verify that the jingle reaches mixed output after those same silences. The reported SPC failure remains unconfirmed; no arbitrary timer was added. |
| Missing jingle, MSU on | The installed staff-roll recording is audible only through about 170.15 seconds. The runtime continued selecting its silent output after EOF. After natural completion of track 49, resume the still-running native music stem, preserving the source's delayed jingle. An explicit stop cancels this handoff. Other MSU tracks retain their existing behavior. |
| EX alternate ending music | Routes using DO_BGM_SP0RCH play the orchestra continuously through the 950-second observation. It is a different source composition, so the standard staff-roll silence/jingle assertion does not apply. EX uses its source SPC soundtrack. |
| One-ship Star Wolf | All four source actors spawn, render and sustain combat on LEVEL5_5, LEVEL6_6 and LEVEL7_5. The permanent regression now shoots them down with normal Y fire and directional steering, then verifies BOSS_PTR/BOSS_SEQ and continuation past mapwaitboss. All six encounter/pace combinations pass; enemy health is not forced. Independent GSU pixel comparisons cover all four ship models. The original one-ship state was not reproduced, so no claim is made about unidentified modifiers/save state. |
| EX compact shapes | Eight standalone compact headers used a hard-coded Original ID_0_C address. Resolve the current cartridge's ID_0_C. Parent-inherited LOD metadata remains intact. |
| EX large BSP tree | GETB/LOB uses an unsigned forward branch offset. A signed decode rejected HUMANA's large tree, used by Australia. Corrected the offset; full EX shape decoding now has no unsupported header candidates. |
| Geometry/projection precision | Independent GSU comparisons exposed byte-coordinate high-byte matrix arithmetic, reflected-point rounding and ZTAB projection differences. Follow each source encoding's arithmetic and load the cartridge's reciprocal table. Native-size frames preserve signed floor rounding and even-Z indexing. |
| Small visible faces and line pixels | MSHON_VIZIS halves screen differences before signed-byte products. MLINE starts its error accumulator at half the difference between axis lengths. Both were translated differently; corrected using source and independent raster output. |
| Clipped and textured faces | Restore source right/bottom coordinates 223/191 and each clip edge's interpolation direction. Texture span gradients use the inclusive pixel count and current right UV; edge UV deltas wrap before FMULT. These fixes remove the skewed Andross face, cube/wall texture differences and the tested close-up Original boss differences. |
| Entire model scan follow-up | Correct unsigned visibility indices, Fend continuation/return semantics, disabled MSHOWSPR commands, textured-line rejection, UV pairs beyond the nominal quad, extended shade/UV tables and bank mirroring. Near-plane projection now preserves wrapped ZTAB reads; offscreen signed visibility indexing preserves its rotated-point alias. Restore primitive boundary rejection and low-nibble transparency before dither. |
| EX reticles and scaled sprites | Gameplay uses MSSPRITE, not MSHOWOBJ3. Correct the source diameter formula, EX's deliberate bypass of SH_SHIFT, Q15 projection, centre rounding, small/large texture stepping and clipping in source texels. All 1,950 EX reticle and 150 Original fireball cases match independent cartridge pixels. Both desktop and stage preview use the corrected diameter calculation. |
| Route 2/3 speed swings (new screenshot) | Fixed a confirmed output-limiter bug: missed deadlines accumulated for up to 250 ms, allowing uncapped catch-up draws when rendering became cheaper. Rebase the next output interval on the late completion immediately; keep the independent elapsed-time simulation clock. A mutation restoring the old behavior fails the burst regression. 56 one-minute route 2/3 replays across both games/paces match every source movement/roll/cadence sample at steady 60/90 FPS, varying 47–59 FPS and 100 ms stalls, with FPS display off/on. Actual barrel rolls are asserted. Original pace still uses an object-count slowdown approximation; that separate full-parity limitation remains. |
| MIT license request | Added MIT LICENSE for project-owned code/documentation and included it in desktop, Switch and Vita packaging. Existing third-party licenses and game/music/asset ownership remain separate, as documented in README and THIRD_PARTY_NOTICES. |
| Camera/model composition | MOBJ copies the world matrix directly for zero object rotation and negates two rows for a half-turn yaw; multiplying in those paths introduced extra rounding. Restore both shortcuts and their shadow handling. Identical matrices now remain unchanged during interpolation. Native-resolution object positions and source lighting depth use per-product Q15 rounding. All 3,726 model/shadow matrices and 9,729 world-point cases per game match independent GSU words. |
| Stars, snow and pollen | MSHOWDUST feeds carry through its random generator and six coordinate shifts, then retries out-of-range/behind-camera points. The port used five shifts, a different carry stream and no retry. Restore the native point stream, ZTAB projection, vertical two-pixel stars, bottom-row SRAM alias and snow/pollen colours. Desktop and preview pass native vanishing-point and particle-colour state. All 579 point states and 576 framebuffers match across both games, including EX's 511-point option. |
| Ground dots / EX line grid | The EX ROM uses 25×25 GSU grid iterations and a 96-unit two-pixel threshold, while Original uses 15×15 and 512. Both CPU origins remain 1,920 units. The EX checkout's source constants differ from the assembled cartridge. Read the GSU operands, restore secondary PLOT placement/byte-coordinate aliasing and EX's line endpoint. All 576 native frames match; 557 are visible. Repeated presentation preserves source line history. |

## Coverage and practical limits

| Area | Exercised | Not established by these checks |
| --- | --- | --- |
| Asset decoding | Original: 2,697 header candidates; EX: 3,511; zero unsupported. Mixed point encoding retained for all expanded/mirrored animation vertices. | Header discovery is a symbol/format scan, not a complete semantic asset manifest. |
| Native CPU | 254 opcode samples across twelve status values, two direct-page alignments and two ROM speeds; exhaustive 8-bit ADC/SBC and sampled 16-bit operands; bank, transfer and I/O order regressions. | All addressing/operand combinations, emulation-mode execution, hardware interrupt/wait scheduling, complete bus ordering or concurrent hardware timing. |
| Model scaling | Source-coordinate/frame/scale comparisons include BOXXIE, Hydra, POINTYANIM, Wolf and the byte-coordinate player; multipliers 1/2/4 and raster scales 1/4. Independent GSU angle/depth fixtures cover the default scale. | Every live camera/attachment state and modifier combination. |
| Route art | 52 complete flat/zoom icon comparisons across Original and both EX map tables; existing real black-hole exit tests in both variants. | A hardware screenshot comparison of every route animation frame. |
| Gameplay | Existing EX sweep covers all 40 shipped stages for 2,000 ticks and decodes encountered model/palette pairs. Additional 8,000-tick traces across both games use invulnerability and held B (boost). Live Wolf combat uses Y fire and directional steering in all six stage/pace combinations. | Completing every boss, alternate exit, secret and modifier combination in a continuous playthrough. |
| Endings/input | Both full ending suites; early-held/late Start in both games; EX Y enters Australia; all seven EX endpoint shortcuts. | Every credits input combination and every SRAM/continue history. |
| Audio | Real SPC PCM through the full ending plus 950 seconds of staff roll, both standard scores and EX orchestra; installed MSU pack measured separately. | Physical device latency or speaker output. MSU is an optional alternate soundtrack, not identical SPC audio. |
| Timing/presentation | Existing fixed-step, original-pace, interpolation, HUD, route, transition and 244 level-clear fixtures; new Wolf tests in both pace modes. | Cycle-exact SNES slowdown: Original pace is still a workload approximation. Enhanced resolutions/interpolation intentionally change presentation. |
| Platforms | Windows build, SDL/virtual-input and UWP-configuration tests. | On-device Xbox, Switch, Vita or mobile verification. These changes have not been republished for consoles. |

This audit does **not** certify either entire game as a perfect 1:1 port.
The source-backed failures above are corrected; the coverage limits and open
reports must remain visible rather than being erased by a passing test count.

## Independent cartridge comparison added on continuation

A separately built Snes9x core now executes the actual cartridge GSU routine.
The extended scan executes every decoded animation frame at three yaws and
three depths: **15,948/15,957 Original** and **24,630/24,858 EX** cases match.
Original's nine remaining rows are the unused LEXIT_0 header: its 12 vertices
reuse EXIT_0 faces that index up to vertex 21, consuming stale SRAM in this
isolated native call. Source references are SHAPES2.ASM's LEXIT_0 and EXIT_0_F.
EX's remaining 228 mesh rows are 26 reticle headers that gameplay sends through
MSSPRITE (GSTRATS.ASM sets asf_ssprite). The separate sprite audit supplies that
entry context and matches **1,950/1,950 EX** cases; Original's FIREBALL/LFIREBALL
match **150/150**. All differing mesh rows remain in `docs/validation`.

The normal suite now includes **40,578 independent mesh framebuffer hashes**
(Original 15,948; EX 24,630, including intentional blank output) and **2,100
sprite hashes**. Mesh cases cover 36,641 visible native outputs.
Restoring the old line accumulator in a separate mutation build makes this test
fail on AIR_1. No Snes9x core or ROM is included in the game's package.

See [reference tool documentation](../tools/reference/README.md) for the pinned
source revision, cartridge hashes, clean build, commands, golden selection and
fixture limitations. The mesh tests now construct the port model matrix and
compare it with the recorded native matrix. Separate matrix, shadow and point
fixtures validate the camera math and the CPU adapter's translated GSU entry
points. The source star-field checks validate persistent point state and pixels
through 192 consecutive updates per count setting. RGB palette composition,
every scene/input/route and cycle timing remain separate concerns.

## Reproduction

Build with `cmake --build build/current -j4`, then run
`ctest --test-dir build/current --output-on-failure -j2`.
The MSU ending test is registered when the optional build/install pack or an
existing `build/current/Starfox-MSU1.PAK` is available.

The longer traces use `STARFOX_GOD_MODE=1` and
`starfox_stage_trace ROM SYMBOLS LEVELn_m 8000 0x8000` for every shipped numeric
stage symbol. These are bounded traces with held B/boost, not successful complete
campaign playthroughs. Stage trace's extra raw-RAM diagnostics are Original
addresses; do not interpret those particular diagnostic fields as EX state.

## Validation and local candidate

- Fade follow-up: all 61 checks have passing results. The full run passed 59;
  the two simulation suites passed after correcting their obsolete training
  fade timeout (2/2, 136.89 seconds). Native fade coverage adds 768 cases per
  variant; four continuous opening comparisons add 1,200 updates with zero
  differences among 2,125,112 checked state values. See
  `FADE-PARITY-VALIDATION.md` for the exact scope and retained logs.

- Timing-profile follow-up: **8/8 full-system cases passed**, then repeated
  with all 40 trace hashes unchanged. Checks cover safe stage entry, every
  clock-register override, unchanged EX source/divided-fast traces and the
  aligned opening state. There are 1,368 camera calls / 23,256 matching words
  and 12,720 GSU intervals per run. Summary and logs are
  `docs/validation/timing-profile-audit*`; see TIMING-PROFILE-VALIDATION.md for
  the hardware-source correction. This changes development tooling and
  documentation, not the candidate's runtime or Original-pace formula.
- Native camera follow-up: **61/61 passed in 452.37 seconds**, including both
  simulation/ending suites, SPC/MSU audio, all six live Wolf encounters and
  route timing checks. Logs: `ctest-20260905-native-camera.log` and its detailed
  counterpart under `docs/validation`. After removing unused camera fields
  and caching GETVIEW_L's entry, the rebuilt runtime passed **9/9 affected
  view, hit-list and desktop smoke checks in 18.76 seconds**. The corrected
  physical matrix reference has 12,204 matching object views; the preceding
  fixture's shared high-byte alias is documented rather than counted as an
  independent check of that memory layout.
- Native CPU follow-up: **61/61 passed in 197.41 seconds**, including both
  simulation suites, all endings/audio/MSU checks, six live Wolf encounters,
  route-timing replays, view/pixel comparisons and the new independent CPU
  checks. Summary: `docs/validation/ctest-20260905-native-cpu.log`; detailed
  output is retained alongside it. The CPU patch also passed fresh-apply and
  repeat-apply checks against the pinned dependency.
- View/timing follow-up: **58/59 passed in the full 340.04-second run**,
  `docs/validation/ctest-20260905-view-flags.log`, with detailed output beside
  it. The sole failure was the synthetic grid fixture missing the newer
  SNOW_COLS/ZTAB symbols and assembled grid parameters. After updating that
  fixture, **3/3 affected checks passed in 0.15 seconds**, including both
  real-cartridge grid references (`ctest-20260905-grid-fixture.log`). The game
  code did not need a further change. All 59 checks therefore have passing
  results for the current implementation; the full run itself is retained
  with its original failure rather than rewritten as an all-green run.
- The four new view comparisons cover **12,204 object views**, including
  **370 invisible** and **309 behind-camera** observations, with no differences.
  The old implementation fails 239 of 3,028 Original route-2 observations.
  Raw before/after rows are in `docs/validation/view-flags-*.csv`; these checks
  sample 1,800 source updates per stage and do not certify entire campaigns.
- The optional Ares audit agrees with every one of **70,980 native result
  rows** across both games and records **73,536 isolated calls**. Provenance,
  checksums and cycle ranges are in `docs/validation/ares-gsu-audit-summary.json`;
  the run log is `ares-gsu-audit.log`. Cold-cache CLSR=0/CFGR=0 measurements
  exclude CPU/GSU overlap, DMA and live clock-register changes. The runtime's
  Original pace scheduler remains the documented approximation.
- Ground-grid follow-up: **13/13 affected tests passed in 9.69 seconds**,
  `docs/validation/ctest-20260905-ground-grid.log`, with detailed output beside
  it. Includes all model, matrix, point, star-field and ground-grid references,
  Original simulation-data, core and desktop input checks. A separate mutation
  restoring the old EX grid dimensions fails the first reference frame.
- Camera/star-field follow-up: **17/17 affected tests passed in 197.61 seconds**,
  `docs/validation/ctest-20260905-camera-and-dust.log`, with detailed output beside
  it. Includes both long simulation suites, live Wolf/credits regressions,
  route timing replays, hit-list suites, 42,678 mesh/sprite pixel hashes,
  26,910 model/shadow/world-point cases, and 579 star-field states. Separate
  mutation builds fail when either the model shortcut or dust-recycling fix
  is removed. Four fresh desktop captures exercise LEVEL2_1 ground/shadows
  and LEVEL2_2 stars in both games; these are smoke/visual checks, not native
  full-scene pixel comparisons or FPS-limiter measurements.
- Earlier full suite: **48/48 passed in 362.62 seconds**,
  `docs/validation/ctest-20260905-pacing-and-sprites.log`. This includes the
  40,578 mesh/2,100 sprite goldens and all 56 route timing replays. The latest
  desktop build also exposes optional STARFOX_TRACE_AUDIO mixer peaks for
  diagnosing the MSU-off report through the actual application loop.
- Near-plane and sprite follow-up: **8/8 affected tests passed in 139.22 seconds**,
  `docs/validation/ctest-20260905-expanded-render.log`. Includes both full
  simulation-data suites, live Wolf/credits regressions and all 42,678 independent
  pixel hashes. The subsequent timing tests are included in the next full run.
- Broad CTest: **44/44 passed in 267.64 seconds**. Summary is checked in at
  `docs/validation/ctest-20260905-reference.log`; detailed output is preserved
  in `build/current/Testing/Temporary/LastFullTest-20260905-reference.log`.
  The final clipping rebuild also passed all 12 affected rendering, ending,
  hit-list, credits/combat and runtime checks in 62.22 seconds. The subsequent
  BOXXIE additions also passed all four final boss/reference checks in
  59.55 seconds, including the six live Wolf encounters. Their summary is
  `docs/validation/ctest-20260905-final-boss-check.log`.
- Earlier CTest: **42/42 passed in 373.84 seconds**. Full output is preserved in
  `build/current/Testing/Temporary/LastFullTest-20260905.log`.
- The separate MSU regression observes natural EOF/handoff at 170.15 seconds,
  570.2 seconds of silence, and the same 740.35–801.85 second jingle as SPC.
  Explicit stop and pause behavior also pass.
- Actual desktop mixer: three 30,000-frame runs (20 FPS, unpaced, Original
  pace, full FINALMAP_END fixture) plus 30 audio-settle batches each. Original
  SPC jingle reaches mixed output at 904.0–965.5 seconds after 591.25 seconds
  of silence; EX at 903.1–964.6 after 591.4 seconds; Original MSU at
  904.0–965.5 after 570.2 seconds. These absolute times include the earlier
  ending scenes and initial audio settling. Raw peak CSVs and validated JSON
  summaries are in `docs/validation/runtime-ending-audio-*`. This verifies the
  desktop mixer path, not physical speaker output.
- Original: **19/19** and EX: **40/40** numeric stages completed the 8,000-tick held-B trace
  without exceptions, recovered-path warnings, unhandled Super FX launches,
  or final-frame undecoded models: **472,000 source ticks** in total. Logs are
  `build/current/Testing/Temporary/CampaignOriginal-20260905.log` and
  `build/current/Testing/Temporary/CampaignEX-20260905.log`.
- Windows package smoke checks run 180 presentation frames after a 3,500-tick
  direct CREDITSMAP preroll, at 60 Hz, unlocked pace and 4x rendering. Source
  Start reaches Original controls and the EX special menu. Fresh captures in
  `validation/original-credits-final` and `validation/ex-credits-final` were
  visually checked. Earlier EX Y/Australia captures are retained separately.
- Candidate: `dist/StarFoxEnhanced-parity-test/starfox_pc.exe`. This is a local
  post-0.0.4 test build, not a published release or a new version number. Its
  directory includes this report, the existing local asset cache and MSU pack.
  The generated asset cache is for this user's local testing; the directory
  is ignored by Git and is not a redistribution package.
- The candidate now includes the latest pacing/sprite fixes and MIT LICENSE.
  Its existing asset bundle was verified against both pinned ROM/symbol pairs.
  The fresh `validation/ex-reticle-final.bmp` capture exercises an actual EX
  reticle at 4x/90 FPS in the unpaced runtime fixture; this is a visual check,
  not a measurement of the live FPS limiter.
- Runtime map capture: `dist/StarFoxEnhanced-parity-test/validation/sector-y.bmp`.
  Credits/menu/Australia captures are in the other `validation` subdirectories.
- The latest candidate includes the camera and star-field corrections. Fresh
  captures are `validation/camera-dust-{original,ex}.bmp` and
  `validation/stars-{original,ex}.bmp`. The 20 FPS/1x Original ground capture
  and 90 FPS/4x captures use direct stage entry and bounded prerolls.
- `validation/grid-final-ex.bmp` is a fresh actual desktop LEVEL2_1 capture
  after the grid correction (90 FPS/4x, unpaced fixture). It supplements the
  independent pixel comparisons; it is not an old-PC performance measurement.
- The candidate now also includes the object-view-flag correction.
  `validation/view-flags-original.bmp` and `validation/view-flags-ex.bmp` are
  fresh desktop captures of LEVEL2_1 and LEVEL1_3 after 300 source preroll
  updates and 180 presentations (90 FPS target, 4x rendering, unpaced,
  MSU off). They were inspected as smoke checks. The EX capture shows third
  person; cockpit behavior is established by the source simulation test,
  not inferred from that image. The user's saved pregame configuration is
  unchanged (SHA-256 `8B6727CD87174ABFCF8455D4A78E5B33CEA189E09D6F9F42A77F57489DB0A720`).
- The latest candidate includes the native CPU corrections. Fresh
  `validation/native-cpu-{original,ex}.bmp` captures exercise direct LEVEL3_1
  entry with a 300-update preroll and 180 presentations, 90 FPS target/4x,
  Original pace, MSU off and an unpaced dummy-device test loop. Both were
  inspected as smoke checks; their displayed FPS is not a live limiter or
  old-PC performance result. The saved user configuration remains unchanged.
- The current executable also includes the native camera/crosshair correction.
  `validation/native-camera-{original,ex}.bmp` repeats the same LEVEL3_1,
  300-update preroll and 180-presentation fixture. Both captures were inspected;
  these are direct-entry/respawn smoke images, not continuous campaign or
  physical-console comparisons. The source timing approximation remains open.
- The current candidate also includes the gameplay/training fade fixes and
  Continue manual-fade completion correction. Its SHA256SUMS file was refreshed
  with the executable; the previous file still listed an older candidate hash.
- The current candidate includes literal object-word preservation, submitted
  hit-flash state and executable-WRAM guard corrections. All 61 checks pass in
  one rebuilt-suite run; the longer continuous reference evidence is in
  OBJECT-STATE-PARITY-VALIDATION.md.
- The candidate also includes IRQ palette-flash/RNG and background-completion
  corrections. The new full runtime suite passes 60/61, followed by a passing
  rerun of the corrected EX cockpit fixture; runtime code is unchanged between
  those runs. The wider continuous stage audit remains 58/59. Its unfiltered
  evidence and open transfer-timing defect are in STAGE-STATE-PARITY-VALIDATION.md.

SHA-256:

```text
starfox_pc.exe
BB39CDBE210CC3975ED7A507071E0D01D8577CE950A83B95E3D29A6B06D9CD11
Starfox-Assets.BIN
2F9A261C87F032F553952588E2EEB5DB747CBAF5FF0E5FCE7AF1862C9FC6541E
Starfox-MSU1.PAK
2139EC0BAB97A768D04AB8655AA95F8E3D204F57A16028DFD12896E3F5599E7A
```
