# Fade timing and continuous gameplay comparison

This follow-up corrects gameplay/training fade timing in Original and EX.
It does not establish complete game parity or physical cartridge timing.

## Source behavior and fixes

Normal `IRQBIT3` calls `SETINIDISP` once when a bitmap transfer completes.
The previous port advanced it on every raster, including rasters spent waiting
for that transfer. A four-raster update could therefore consume four brightness
steps and release `waitfadefin` too early. Gameplay and training now advance the
fade once in the completed transfer. Ending raster modes and the separately
implemented front-end flows retain their own presentation scheduling.

The translated primitive now matches the assembled `IRQ.ASM` stores:

- Quick fade-down decrements twice: `QFADEDOWN` branches to `SETDOWN`'s second
  decrement. Brightness 11 follows 9, 7, 5, 3, 1, forced black.
- Normal fade-up publishes 15 before clearing `FADEDIR` on the next call.
- Quick fade-up has three increments, with earlier completion branches at
  starting values 13, 14 and 15 that leave the display aliases untouched.
- Every publishing path writes `XINIDISP1`, `XINIDISP2` and `XINIDISP1A`.
- Starting a fade writes its direction only. The previous repairs of a
  supposedly stale `FADE` counter hid incorrect early completion. Native
  `INITBLACK_L` can intentionally reset that counter independently of INIDISP.
- Continue's manual `FADELOOP` accepts input immediately after publishing 15;
  it does not require the IRQ primitive's extra completion call.

The transition test initializes the copied native IRQ code and runs its actual
`SETINIDISP` against the host primitive. Per game it compares 768 cases: two
GAMEFRAME parities, eight directions, all 16 valid fade counters and three
independent display values. All five native bytes match. The ordinary Corneria
test also checks that seven presentations consume no additional fade steps
until their gameplay update completes.

## Continuous reference

The optional full-system Ares tool now accepts a required gameplay update count:

```powershell
pwsh -NoProfile -File tools/reference/build-full-reference.ps1
python tools/reference/verify-gameplay.py
```

An individual example is:

```powershell
tmp/full-reference-build/full_reference.exe upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL2_1 3600 tmp/gameplay-original source 300
```

The tool clones CPU/GSU RAM once at GAMEFRAME zero, restores the host's object,
map and display caches, then advances a continuous host simulation with neutral
input. It supplies each transfer's observed native raster count, retaining the
host's existing three-raster minimum when the reference completes in two. Both
counts are recorded explicitly. Comparison is
after `TRANSFER_L` has waited for the preceding `IRQBIT3`, immediately before
its `STZ NOIRQBIT3`. Function entry is too early: the object update may be
complete while the final display interrupt is still pending. That early
boundary produced misleading brightness differences in the initial probe.

Every compared update checks 25 globals (including fade aliases, map cursor,
RNG, camera matrix and object-list heads), all active object base bytes and
their extended bytes. The tool validates list bounds, stops on a difference,
and fails if it never starts or exhausts its video budget before the requested
count. It records per-update comparison counts and every mismatch. No mismatch
mask is applied.

| Game / opening map | Consecutive updates | State comparisons | Differences |
| --- | ---: | ---: | ---: |
| Original LEVEL2_1 | 300 | 560,580 | 0 |
| Original LEVEL3_1 | 300 | 561,460 | 0 |
| EX LEVEL2_1 | 300 | 501,536 | 0 |
| EX LEVEL3_1 | 300 | 501,536 | 0 |
| Total | 1,200 | 2,125,112 | 0 |

The script also verifies that a deliberately short run fails for insufficient
updates. Input and tool hashes are in
`validation/gameplay-audit-summary.json`; raw traces remain local under
`tmp/gameplay-audit-final-counts`. The host minimum applied on one update in
each Original case and on no EX updates. A preceding run reproduced the same
state-comparison totals before those minimum-phase counts were added.

## Regression checks and local build

All 61 CTest checks have passing results for this runtime. The full run passed
59/61 in 210.62 seconds; the two simulation suites still expected training to
exit within 12 updates. The corrected source test requires its full 15-step
fade. Rerunning those two suites passed 2/2 in 136.89 seconds. Earlier obsolete
scramble/opening fade expectations were also replaced with source timing, and
testing caught and corrected the Continue manual-fade completion side effect.
Both native ending-audio checks, the MSU ending-audio check and EX's alternate
orchestra check passed in the full run. Logs are retained together in
`validation/fade-regression-validation.txt`.

The local Windows candidate is
`dist/StarFoxEnhanced-parity-test/starfox_pc.exe`, SHA-256
`4287155279244FA6F08B8281B958232D9D4B26105A56BC194A5DD3C6EAF5EBFA`.
Its checksum file is refreshed along with the executable and validation docs.
It retains the project-owned code's MIT license and separate third-party
notices. This is a local test build, not a published release. The user's saved
pregame configuration remains unchanged.
Both copied-candidate smoke launches passed using their local asset cache:
Original LEVEL2_1 and EX LEVEL3_1, 12 presentations each with dummy devices.

## Scope still open

This is a seeded opening-section comparison, not a boot-to-ending playthrough.
The host uses its unlocked pace and retains its three-raster floor; the supplied
reference counts do not constitute an exact scheduler. Input sampling, physical
hardware timing, framebuffer output, audio and unlisted RAM are not compared
by this tool. The full camera observer and other regression suites remain
separate evidence.

An exploratory Original LEVEL2_1 run first differs at update 371 in bit 1 of
two objects' `AL_SFLAGS` bytes (host 10, native 8). The host deliberately retains
hit-flash for presentation and clears it at the next update; the source copies
it into its draw list and clears object RAM sooner. The strict tool reports this
representation difference rather than masking it. Extending comparison past
that boundary requires checking both the submitted draw state and the native
clear timing. A full-game 1:1 claim remains unsupported.
