# Timing reference profiles and safe stage entry

The full-system reference now distinguishes the reconstructed ROM's actual
GSU settings from explicit diagnostic overrides. None of these settings turns
Ares's generic GSU into a verified physical MARIO Chip 1 implementation.
Production Original Speed still uses a workload estimate.

## Hardware assumption corrected

UltraStarFox's `SF/CONFIG/ROM.INC` says that its `fast` option has no effect on
the original MARIO chip. The hardware documentation instead reports that CLSR
exists on MC1, while its fast-multiply option is absent. It also cautions against
enabling fast multiply with the undivided clock. See the author's
[GSU register documentation](https://problemkaputt.de/fullsnes.htm)
and [direct cartridge measurements](https://forums.nesdev.org/viewtopic.php?start=45&t=5964).
The reference documentation no longer treats the source comment as established
hardware behavior. The user's modified upstream checkout is preserved.

Our reconstructed Original input requests CLSR=1 and CFGR=$a0. EX requests
CFGR=$a0 and retains CLSR=0. These observations come from the actual I/O writes,
not from source configuration names. Their ROM hashes are in the validation
summary. The local inputs are reconstructed game builds, not stock retail ROMs.

## Reference changes

- `source`, the default policy, preserves all register writes.
- `divided-standard` clears CLSR bit 0 and CFGR bit 5.
- `divided-fast` clears CLSR bit 0 and sets CFGR bit 5.

Other bits are preserved. Each run records requested/effective values and the
CPU PC during boot and gameplay. Overrides apply from boot without changing
ROM bytes. The Ares instruction, DMA, refresh and scheduling implementations
remain the pinned v148 versions.

The old direct-stage fixture jumped at an arbitrary CPU instruction boundary
after 600 video frames. Under the slower setting it could interrupt RUNMARIO
while the GSU was still writing scene memory. One Original run then completed
zero camera calls and never reached the gameplay transfer loop. The fixture
now waits for TRANSFER_L with the GSU stopped and its RAM write buffer empty,
then installs the requested route/map and enters GAMESTART. `-entry.csv`
records that boundary. The slower Original case now reaches 171 complete
camera calls in the same 1,200-frame observation. This fixes the development
harness, not a reported crash in the production game.

## Validation

Build and run:

```powershell
pwsh -NoProfile -File tools/reference/build-full-reference.ps1
python tools/reference/verify-timing-profiles.py
```

Eight cases cover all three policies on LEVEL2_1 in both games, plus the source
policy on LEVEL3_1. Each boots for 600 NTSC video frames and observes another
1,200 with neutral input and the existing PSHIPFLAGS3 damage-suppression flag.
All eight complete successfully: **1,368 camera calls / 23,256 matching words**
and **12,720 recorded GSU intervals**. Entry, register policies and completed
gameplay are checked. No camera mismatch occurs.
Repeating all eight cases produces the same 40 trace hashes. The repeated run
also checks the aligned opening-state invariants below; all checks pass.

The first 40 consecutive updates after GAMEFRAME resets to zero have these
CPU master-clock deltas, including native waits, refresh and DMA:

| Input / route 2 policy | Master clocks for updates 1–40 |
| --- | ---: |
| Original / source | 79,430,066 |
| Original / divided-standard | 94,873,294 |
| Original / divided-fast | 93,055,706 |
| EX / source | 98,254,050 |
| EX / divided-standard | 99,055,402 |
| EX / divided-fast | 98,254,050 |

Across these opening updates, each variant has identical sampled player
positions, camera Z, active/drawn object counts, map pointer and GAMEFRAME
under the three policies. The elapsed clocks differ. EX's source and
divided-fast runs also have identical complete frame, camera, GSU and register
CSV hashes: that override requests the settings EX already uses.

The durable summary is `validation/timing-profile-audit-summary.json`.
Reproducible raw traces and logs remain under `tmp/timing-profile-audit` and
`tmp/timing-profile-audit-repeat`; the latter also exercises the final opening
state checks. The repeated run log is `validation/timing-profile-audit.log`.
The prior `full-system-audit-summary.json` retains its original tool hash and
entry method; those earlier traces are not silently replaced by this fixture.

## Scope

This follow-up changes reference tooling and documentation. The Windows
candidate remains SHA-256
`470E495CEA88EFCB36FC753325893C24E29FC29DC64DFC75013CCF83F9A85291`;
the prior 61-test runtime validation still applies. No new runtime timing
formula has been installed from these bounded samples.

The clock policies can change the preceding frontend state and entry phase.
GSU traces begin after the 600-frame boot and can include the end of that
frontend update before the boundary recorded in `-entry.csv`.
The aligned opening checks limit that confound for the fields stated above;
they do not prove that every input state is identical. A physical hardware
profile, full CPU/GSU overlap, complete campaign/alternate-exit coverage and
full-scene RGB comparison remain necessary before certifying 1:1 parity.
