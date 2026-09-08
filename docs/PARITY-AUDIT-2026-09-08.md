# Original and EX parity audit — evidence ledger

Status: **audit complete; 1:1 parity is not established for either port.**
This is the pre-fix evidence ledger. The user subsequently authorized repairs;
see [repair status](PARITY-FIXES-2026-09-08.md) for the current implementation
and remaining validation limits. The original findings below are preserved.

## Baseline and validation limits

- Current HEAD: `8721cd4f58e302db8e42fa5c7c2b9209cd329046`.
- Worktree includes an uncommitted escape-explosion test in
  `tests/hitlist_tests.cpp` and existing changes in `upstream-ultrastarfox`.
  This is not a pristine release-tag validation.
- Fresh build succeeded; all **41/41** registered tests passed in **169.48 s**.
  Logs: `tmp/parity-audit-20260908-build.log` and
  `tmp/parity-audit-20260908-tests.log`.
- Targeted audit probes additionally reproduced both MSU failure boundaries
  described below and enumerated the EX word-coordinate/shift conflict. Those
  probes are diagnostic evidence, not additions to the registered suite.
- GitHub run `34183715151` at this HEAD had nine successful platform jobs.
  Compilation/package success does not establish on-device correctness.
- Historical reports in `dist/StarFoxEnhanced-parity-test` describe an older,
  subsequently rolled-back implementation. Their 61-test/reference-emulator
  claims cannot be applied to this worktree's 41 tests.

## Findings from current code and source

### F1 — Quick fade-down disagrees with Original source (confirmed)

`src/simulation/map_vm.cpp:297` assigns one decrement to direction -2.
`tests/transition_parity_tests.cpp:53` explicitly requires 11 to become 10.
But `upstream-ultrastarfox/SF/ASM/IRQ.ASM:1374` decrements in `.qfadedown`
and branches to `.setdown`, which decrements again at line 1356.
Thus 11 becomes 9 and reaches black after six invocations, versus eleven
in the port. The passing test enforces the discrepancy. This source file
is not among the dirty submodule files. EX source equivalence and actual
invocation cadence still need independent verification; this finding alone
does not explain the reported permanent white Out of This Dimension screen.

### F2 — Renderer scales EX word-coordinate models that native code does not (confirmed)

`src/assets/shape_decoder.cpp:343` distinguishes byte and word coordinates,
but flattens both into raw `vertices` without applying an encoding-dependent
normalization. `src/render/software_renderer.cpp:1584` passes the same
`shape.header.shift` for every vertex; `rotate` at line 585 multiplies every
coordinate by `2^shift`.

The Original source `SF/MARIO/MOBJ.MC:1319` reads word coordinates directly
into the 16-bit matrix path; byte-coordinate paths separately consume
`m_scale`. The renderer does not preserve that distinction. A word point
with nonzero shift therefore receives scaling absent from that source path.

A decoder inventory of the actual EX ROM found eight affected, word-only
headers: `BOXXIE` (shift 3), `CORNFRIEND` (2), `MARIOHFIRE` (2),
`LUIGIHFIRE` (2), `LUIGIHSBW` (4), `BLACKROSE` (1), `LUIGIH` (2), and
`MARIOH` (2). The current renderer consequently inflates them by 8x, 4x,
4x, 4x, 16x, 2x, 4x, and 4x respectively. EX `MDATA.MC` identifies BOXXIE
as **The Crimson King**, while the LUIGIH family is **Luigi Hydra**. This
directly confirms the reported oversized bosses. The registered suite runs
shape coverage only for Original; the EX stage sweep proves these shapes
decode, not that their rendered coordinates match native output. Probe logs:
`tmp/parity-shape-ex.txt` and `tmp/parity-shape-ex-summary.txt`.

### F3 — Object-pool recovery changes semantics without a reproduced cause

`src/simulation/map_vm.cpp:647` appends every object absent from both native
lists to the free list. This applies to Original and EX generally, not only
the Android escape sequence. It assumes an omitted object should be freed;
that can conceal corruption or discard an object that should remain active.
The reported Android failure was at EX `VIEWOUTOFLB1_STRAT` ($a9d43).
The local nearly-full-pool explosion test passes both before and after this
recovery change and does not force the recovery branch. Therefore neither
the root cause nor this recovery's correctness is established.

### F4 — Original pace is intentionally approximate; coverage is narrower than game scope

The user explicitly accepted approximate speed rather than cycle accuracy.
`GameSimulation::pace_decision` uses geometry estimates, bounded raster
phases, and special cases. This is an accepted design distinction, not by
itself a bug. It nevertheless cannot establish exact original timing.
`tests/geometry_pacing_tests.cpp` exercises the first three stage openings
for 60 seconds and compares 30/60/120 FPS runs against the same port at
60 FPS. That proves bounded cadence and presentation-rate independence
within those cases, not all bosses, routes, or cartridge music alignment.

## Whole-game coverage ledger — still to close

### F5 — Failed MSU replacement retains previous playback (reproduced conditional failure)

`src/audio/msu1_audio.cpp:38` changes `selected_track_` and `repeat_` before
loading the replacement. If loading fails, it leaves `playing_`, decoded
samples and the cursor from the previous track intact. A missing/corrupt
death track can therefore leave old boss samples playing, now with the
new repeat flag. This is not proof that the distributed pack has a missing
track or that this caused a particular report. The current synthetic test
always returns valid bytes for every requested track, so cannot detect it.

The diagnostic probe starts a looping cue, makes track 38 unavailable, and
observes `selected=38 playing=1 old_samples_audible=1`. The same render path
clamps a configured loop start to the file length.
If a looping replacement file is shorter than its hardcoded loop start,
`loop == source_frames_`; the EOF while-loop then leaves the cursor unchanged
and cannot terminate (`msu1_audio.cpp:127`). A 320-frame valid FLAC assigned
to looping track 2 did not return from `render` within three seconds and the
exact probe process was terminated. This is a conditional hang for
shortened/replaced files, not a defect in the currently bundled pack.

The current 226,198,054-byte pack has valid FLAC STREAMINFO for all 52
entries. Tracks 38 and 39 are present (5.08 s and 5.36 s), and every non-staff
hardcoded loop offset is before EOF. Track 49 is 170.10 s and shorter than its
unused loop constant, but the staff-roll request is explicitly non-repeating.
Thus the shipped pack does not presently activate either reproduced failure;
missing, corrupt, or shortened replacement content can.

### F6 — Current MSU staff roll cannot deliver the source's delayed jingle (confirmed)

The bundled track 49 is 7,501,589 frames at 44.1 kHz, or 170.10 seconds.
`enter_credits` requests it as a one-shot. Once it ends, `Msu1Audio::render`
returns silence and clears `playing_`, but `AudioOutput::queue_logic_tick`
selects the MSU buffer whenever MSU is enabled; it never switches back to
the still-advancing SPC music buffer after EOF. Therefore any later staff-roll
event generated by the native soundtrack, including its delayed jingle, is
inaudible in the current MSU mode. The earlier candidate had an explicit EOF
handoff for this case; that code and its long timeline test are absent from
the current 41-test worktree.

### Audio and ending scope clarified

- `src/app/starfox_pc.cpp:5329` advances audio once per three presentation
  phases independently of whether a gameplay tick occurred. Native SPC and
  MSU sample progression share that cadence. This supports the intended
  separation of music duration from approximate gameplay slowdown; it is
  code evidence, not a measured full-playthrough synchronization result.
- MSU is enabled only for the Original experience (`starfox_pc.cpp:5325`).
  `request_msu_music` also explicitly excludes EX. Consequently the relevant
  matrix is Original native/MSU plus EX native; EX MSU parity is not an
  existing supported mode.
- `tests/msu1_audio_tests.cpp` checks a synthetic 320-frame cue, pause, EOF,
  looping, stop, and an injected track-38 replacement. It does not drive
  a gameplay death or use the actual death/staff-roll recording.
- `enter_credits` requests track 49 without repeat. With MSU enabled the
  mixer selects only MSU music samples, including after EOF, rather than
  falling back to the SPC music stem. Any delayed jingle must therefore
  exist in that recording or arrive through a later MSU play command; native
  SPC-only continuation is insufficient. Actual recording content and the
  full timeline remain to be checked locally.
- Original `CREDITS.ASM` keeps its terminal map alive; the port also keeps
  ticking completed credits. That removes an obvious terminal-freeze cause,
  but is not proof that the delayed audio event occurs. This source file has
  a local modification, so its terminal input branch is not a pristine
  reference.

| Area, both ports unless stated | Available evidence | Remaining evidence needed |
| --- | --- | --- |
| Boot, intro, title, training, menus | Smoke and bounded intro/transition tests | Independent source timing and visual comparison across branches |
| Campaign routes and stage scripts | Map, stage, planet and route lifecycle tests; EX coverage differs | Full route/stage/branch inventory matched to exercised scenarios |
| Bosses, damage, death, respawn | Strategy/hitlist and direct audio tests | Every boss lifecycle, especially EX multi-object encounters |
| Object lifetime and escape | Synthetic explosion fixture and ending tests | Reproduce Android failure; validate free/active invariants against source |
| Black Hole / Sector Y / secret exits | Bounded fixtures noted in existing hitlist | Actual activation, return, icon persistence and bird-trigger white transition |
| Geometry, animation, rendering | Decoder, coverage and shape tests | Encoding-scale finding, all model families, backend comparison |
| Input and presentation | Runtime input/queue tests | Physical devices, renderer switching and input/interpolation behavior |
| Native music and effects | Direct-audio boss-to-death fixture in both variants | Actual gameplay event delivery and full nonlooping timeline |
| MSU-1 | Pack and audio unit tests | Same gameplay/death/ending matrix with MSU enabled |
| Credits, endings and delayed jingle | Bounded ending and Start fixtures | Full delayed jingle timeline, both audio modes and ending variants |
| Platforms | Nine CI jobs pass | On-device runtime coverage; build success alone is insufficient |

`tests/direct_audio_tests.cpp:278` injects boss and Player Down commands and
checks audible death music followed by silence. This is meaningful audio
engine evidence, but does not prove that every gameplay death emits the
right command. The full delayed credits jingle is not established by the
current bounded ending tests.

## Coverage audit completed

### Route, boss and secret-transition coverage inspection

- The current EX sweep is broader than a few openings:
  `tests/simulation_tests.cpp:1681` enumerates all 40 existing numbered
  `LEVEL1_1` through `LEVEL7_9` labels. CTest sets
  `STARFOX_EX_ROUTE_AUDIT_TICKS=2000`; each runs in god mode with no controls,
  decoding encountered visible shape/colour combinations. The test asserts
  stable gameplay, a live player and no unknown GSU launches. This is useful
  100-second logical-time coverage per label, but does not prove all maps,
  optional triggers, boss components, kills or authored route transitions.
  Named special maps are not enumerated by this numbered-label sweep.
- `starfox_upstream_route_lifecycle` is specifically a 500-tick
  `LEVEL1_3` stage trace; its name should not be read as all-route coverage.
  `starfox_upstream_stage_trace` runs `LEVEL1_1` for 100 ticks.
- Level-clear tests exercise ten shared clear scripts and two EX-only
  cases (`CL_TURN2`, `CL_COMET`). They warm up a stage then jump directly
  to its authored clear call while preserving its return continuation.
  That validates clear/tally flow, not the boss battle that normally invokes it.
- Sector Y's hitlist fixture compares a 32x32 rendered BG1 cell using actual
  CGRAM colours before/after all three Black Hole exits. Original additionally
  compares against the normal selector before activation. EX omits that
  initial comparison because the direct selector initializes another campaign.
  Route changes and exit flags are injected, so this does not replace a full
  player-triggered run, but it does establish pixel/palette persistence within
  the tested campaign states.
- The Out of This Dimension test (`simulation_tests.cpp:4139`) starts at
  Corneria, injects exit 16, sees a white-effect-active flag and reaches
  `planet_travel` within 80 ticks. It does not trigger the bird, verify the
  destination identity, render its first visible frame, or check that the
  destination remains responsive. The reported permanent-white failure is
  therefore **not resolved by that passing test**.
- No explicitly Wolf-named assertion or fixture was found in current tests.
  EX symbols do contain multiple Wolf boss/strategy entries. Incidental
  execution during the numbered-stage sweep does not establish the required
  ship count, independent behavior, or complete defeat sequence. The reported
  one-ship Star Wolf encounter remains unverified.

## Final determination

| Claim | Determination | Decisive reason |
| --- | --- | --- |
| Original gameplay/transition 1:1 | **Fail** | Confirmed QFADEDOWN source mismatch; full routes and hardware timing are not independently compared |
| EX gameplay 1:1 | **Not demonstrated** | Forty numbered openings are stable, but optional triggers, complete bosses, named maps and authored transitions are not exhausted |
| EX visual 1:1 | **Fail** | Eight actual word-coordinate models are scaled contrary to the native point path, including Crimson King and Luigi Hydra |
| Original MSU audio 1:1 | **Fail** | Staff-roll EOF permanently selects silence over the later native soundtrack; two replacement-content failure paths are also reproduced |
| Native SPC audio 1:1 | **Not demonstrated** | Bounded engine/death tests pass, but complete gameplay and ending delivery are not in the registered suite |
| Timing 1:1 | **Out of scope by design** | Original pace is an accepted approximation, not cycle-accurate cartridge scheduling |
| Platform 1:1 | **Not demonstrated** | Nine CI build/package jobs pass, but no on-device behavioral matrix was run |

The 41 green tests establish a useful regression baseline, not a parity
certificate. Several compare the port with its own expected behavior; one
explicitly codifies the opposite of the Original fade source. The historical
61-test/reference-emulator reports describe code that is no longer present.
On the current evidence, a public statement that either experience has 1:1
parity would be inaccurate. The confirmed blockers are F1, F2 and F6; F3 is
an unverified semantic recovery, F5 contains real but conditional failures,
and the coverage ledger identifies the evidence still required after fixes.
