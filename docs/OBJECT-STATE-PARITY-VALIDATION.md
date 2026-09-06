# Object state and submitted hit-flash parity

Extending the continuous source comparison past the earlier 300-update limit
exposed two runtime differences in both games. This follow-up corrects them
and checks the source's submitted draw flags as well as its object RAM.

## Hit-flash lifetime

`MARIOSHOWVIEW` copies `AL_SFLAGS` into `DL_SFLAGS`, then immediately clears
`ASF_HITFLASH` in the object. Invisible objects skip both operations. The port
previously retained that bit in object RAM until the next update so that the
renderer could see it. That changed the RAM observed by later native routines
and cleared pending flashes on invisible objects.

The port now saves submitted flags separately, clears the object's bit at the
source boundary, and uses the saved flags in desktop and preview rendering.
The saved flags include an allocation generation so that a replacement object
cannot inherit the old occupant's flash. Repeated presentations preserve the
same submitted state without changing native object RAM.

The first strict difference before this fix was Original LEVEL2_1 update 371:
two objects had `AL_SFLAGS=10` in the host and 8 in the reference. The reference
tool now observes the actual assembled store after `DL_SFLAGS` is populated.
It checks those captured flags for objects still active at the settled transfer.
It does not suppress the object-RAM difference or claim to compare every draw
record throughout an object's lifetime.

## Pointer and counter words

After the flash correction, the next differences were Original LEVEL2_1 update
409 and EX LEVEL2_1 update 415. Both involved an `AL_PTR` word containing the
scalar 4. The host bridge treated that value as host object handle 4 and wrote
back its address: `$03e0` in Original and `$03e4` in EX.

The four object words used for attachment, immunity, collision and firing now
retain their literal cartridge values across native calls. Host PATH operations
explicitly convert between valid native pointers and active handles when they
need to follow a link. Removing an object clears references to its native
address while preserving unrelated small counters. The object layout comes
from the selected cartridge's symbols.

The bridge regression covers 320 word round trips in Original and 360 in EX,
including low counters, active pointers, misaligned/out-of-range addresses and
sentinels. Separate PATH scenarios check pairing, mutual immunity, removal and
a scalar that happens to equal an active host handle in both object layouts.

## Executable work RAM

The bridge test also exposed an address-guard error: valid code in the upper
half of WRAM banks `$7e` and `$7f` was rejected as unmapped ROM. Calls and
resumable tasks now retain the WRAM mapping throughout both banks. CPU tests
exercise both sides of `$8000`, bank-local instruction wrapping, task stop and
resume, and the expected bus clocks. The unmapped-ROM rejection remains tested.

## Reproduction and scope

```powershell
cmake --build build/current -j 4
ctest --test-dir build/current --output-on-failure -j 4
pwsh -NoProfile -File tools/reference/build-full-reference.ps1
python tools/reference/verify-gameplay.py --output tmp/object-state-audit
```

The gameplay script now defaults to 1,000 updates per case with a 10,000-video-
frame budget. It requires all four cases to finish and rejects an intentionally
incomplete run. The trace records submitted-flag comparisons and how many
contained hit-flash, so missing observation cannot silently pass.

| Game / opening map | Updates | State comparisons | Submitted flags | Flashes | Differences |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original LEVEL2_1 | 1,000 | 1,782,996 | 15,046 | 15 | 0 |
| Original LEVEL3_1 | 1,000 | 1,850,356 | 15,746 | 11 | 0 |
| EX LEVEL2_1 | 1,000 | 1,873,281 | 15,421 | 11 | 0 |
| EX LEVEL3_1 | 1,000 | 1,930,033 | 16,012 | 10 | 0 |
| Total | 4,000 | 7,436,666 | 62,225 | 47 | 0 |

Submitted flags are included in the state-comparison totals. The separate
camera observer also matches all 800 completed calls / 13,600 words. The host
minimum applies once in each Original case and never in either EX case.
The incomplete-run rejection passes. Input, executable and trace hashes are
retained in `validation/object-state-audit-summary.json`; generated raw traces
remain under `tmp/object-state-audit`.

Each case uses one initial RAM seed and neutral input, then checks consecutive
updates against the pinned full-system Ares reference. It supplies observed
native raster counts and records any application of the host's three-raster
minimum. These are object/global-state comparisons, not a full framebuffer,
input, hardware-timing or campaign-completion certification. The production
Original pace approximation and the wider audit's remaining limits stay open.

## Regression suite and local candidate

After rebuilding every runtime/test target, all **61 CTest checks passed** in
232.54 seconds. This includes both report-specific regression suites, both
ending suites, Original and EX delayed-jingle audio, MSU ending audio and the
alternate EX orchestra. The complete log is
`validation/object-state-regression-validation.txt`.

The local candidate at this validation was `dist/StarFoxEnhanced-parity-test/starfox_pc.exe`,
SHA-256 `7A3F6BF01707685CD338B5E866B86EC80B8A4F9195DA997509CDD6080BDC2D1A`.
It includes the prior fixes and MIT license/third-party notices. It is a local
test build, not a published release.
The later candidate and numbered-stage follow-up are recorded in
`STAGE-STATE-PARITY-VALIDATION.md`.
Both copied-candidate smoke launches passed using the local asset cache:
Original LEVEL2_1 and EX LEVEL3_1, 12 presentations each with dummy devices.
The saved pregame configuration remains unchanged, SHA-256
`8B6727CD87174ABFCF8455D4A78E5B33CEA189E09D6F9F42A77F57489DB0A720`.
