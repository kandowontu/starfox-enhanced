# Requested issue verification

## Current first-corridor proof — September 24

The current Windows executable was rerun in the first Titania corridor at
frame 180 with saved visual enhancements disabled. Original 16:9 and EX 32:9
each produced GPU and Software final-presentation captures. Both GPU captures
were visually inspected: the authored ceiling and floor continue to the outer
edges, with the dark walls and foreground ships remaining visible. In each
pair the RGB images differ only in the live FPS counter rectangle; masking
that rectangle makes the images byte-identical. The harness confirmed
`tunnel=1 inatunnel=1` rather than the later water/exit scene.

Proof: `tmp/issue48-goal-current-original-{gpu,software}/presentation.bmp`
and `tmp/issue48-goal-current-ex-{gpu,software}/presentation.bmp`.
This is current desktop scene proof, not physical Android or natural-route
acceptance. GitHub #48 remains open. A live September 24 issue recheck found
#43/#46/#47/#49 closed and #44/#48 open.

## Remote recheck — September 23

Read all six requested issues again using the repository's GitHub API access.
43/46/47/49 are closed; 44/48 remain open. Latest comments on 44 still describe
native Steam Deck Linux failure versus working Proton 9. Latest comments on 48
still identify the first Titania corridor before the corrected exit. No newer
physical confirmation was present, and no comments or states were changed.
Existing local evidence below must not be presented as hardware acceptance.

## Fresh remote status — September 20 evening

Current #48 corridor recheck: tightened the fixture to disable saved ground/
sky options, materials, manipulations and FSR explicitly. Original 16:9 GPU
and software final captures at `tmp/issue48-current-{gpu,cpu}-sep20` are
byte-identical (SHA256 D7F1DC323910E2F46312185B4229890F199B1604699DCEC4D6676B4017F11071).
GPU presentation inspected: ceiling/floor extend to both sides, without
duplicating the tunnel, and foreground gameplay is visible. The fixture
asserts actual first-corridor tunnel state. This is fresh desktop evidence,
not physical Android confirmation or every corridor phase.

Read current issue state and comments directly from GitHub:

- 43: closed, red tint after death; latest owner comment says fixed in 0.0.6.7.
- 44: open; latest physical report still distinguishes native Linux Gaming
  Mode failure from working Windows/Proton 9. Local virtual-device tests are
  not a substitute for that physical acceptance.
- 46: now closed. Owner confirmed the Game Over star extension for upcoming
  0.0.7 at 2026-09-20T22:25:30Z:
  https://github.com/kandowontu/starfox-enhanced/issues/46#issuecomment-5753116613
  Earlier statements below that this ticket remains open are historical.
- 47: closed, with reporter confirmation in 0.0.6.7.
- 48: open; latest report specifically concerns the FIRST Titania corridor,
  before the already-corrected exit. Existing local corridor captures below
  address that scene, but no newer physical Android confirmation is present.
- 49: closed, documentation packaging report.

No issue state or comments were modified in this audit. Closed tickets do not
by themselves prove all runtime/platform requirements of the broader goal.

## Steam translated-button isolation — September 20

Extended the actual SDL virtual-device fixture beyond selection checks. For
each of three Steam virtual-device names, South and Start now travel through
the selected controller while the simultaneous raw Deck fixture holds East.
The selected stream must report exactly B/Start, then zero on release; the
raw duplicate must not leak through. Windows and native Linux runtime-input
tests pass. Existing duplicate-player suppression and reconnect checks remain.
This exercises SDL's virtual-device button delivery and application sampling,
not physical Steam Gaming Mode, metadata substitution, or kernel HID behavior.
Issue #44 still needs physical reporter/device confirmation.

## Steam virtual transport identity — September 20

Fresh #44 comments still contain no physical confirmation beyond the recorded
native Gaming Mode failure / Desktop and Proton success. Source inspection of
the pinned SDL identifies a further selection gap: its public joystick vendor/
product getters can return Steam metadata's physical-controller IDs, while the
Linux driver's GUID remains constructed from the kernel input device IDs.
Checking only the public IDs/name can therefore misclassify the translated
stream as a raw Deck controller.

The selector now also checks the underlying GUID for Valve's 28de:11ff virtual
transport before considering raw-Deck duplicate suppression. Existing reported
ID/name detection remains. Windows and Linux application/input targets rebuild;
Windows and native Linux input tests pass, including virtual-stream preference, duplicate removal
and reconnect behavior. This is source-supported selection hardening, not proof
of the reporter's physical Game Mode fix or a Steam-metadata integration test.
No launcher settings, system drivers or GitHub issue state were changed.

Follow-up regression: the identity predicate is shared with tests that explicitly
pair the virtual transport IDs with substituted Deck and Xbox public IDs. Both
must remain virtual; native Deck/Xbox and unknown IDs must not. Reported virtual
IDs also work without a transport GUID. Windows and native Linux input tests
pass. These synthetic identity combinations cover the selection decision, not
Steam's metadata delivery or physical Game Mode behavior.

## ScaleFX/stereo interaction coverage — September 20

Extended the Game Over harness with explicit filter/FPS controls and pinned
FSR, reflections, software shadows and AA. Original and EX now pass the full
six-path matrix at 16:9, 4x rendering, ScaleFX and 240 presentation FPS:
GPU late stars, CPU late stars, forced mono failure, forced stereo failure,
successful Half SBS and successful Full SBS. Mono/fallback captures remain
byte-identical; both stereo eyes contain widened margin stars and have the
requested packed dimensions. EX Full SBS was visually inspected.
Evidence: `tmp/game-over-scalefx-sbs-sep20` (12 final frames and logs).
This adds combined-feature coverage for #46/ScaleFX/SBS, not physical-device
acceptance or exhaustive animation coverage. No issue was closed or release made.

## #44 native controller diagnostics (September 19)

Latest reporter comments still distinguish broken native Linux Gaming Mode from
working Desktop Mode and Windows/Proton 9. No new physical-device confirmation
of the local fix. Windows runtime input tests pass after this recheck.
Native Linux application and input tests also build; input tests pass. A
two-frame Linux software smoke run exits successfully and emits the new scan
line (`joysticks=0 selected=0 steam-virtual=1 hidapi=1 deck-hidapi=1`) on this
controller-free host. No physical Deck acceptance is implied.

Desktop now supports `STARFOX_TRACE_INPUT=1` at startup and controller hotplug:
it logs enumerated joystick names/vendor/product IDs, whether SDL recognizes
them as gamepads, which are selected, and the effective Steam-virtual/HIDAPI
settings. It does not log serial numbers or button presses. Linux launch option:
`STARFOX_TRACE_INPUT=1 %command%` (capture the application's standard error).
This is diagnostic evidence gathering, not a claimed new control fix. It lets
a physical follow-up distinguish missing enumeration/mapping from wrong device
selection without guessing from A/Start behavior alone.

## #48 complementary final-presentation coverage (September 19)

The Titania harness now captures the final presented frame in addition to the
indexed game framebuffer, and explicitly disables software shadows, reflections
and AA so saved settings cannot contaminate renderer comparisons.
Current effects-check executable: Original 16:9 and EX 32:9 corridor captures
each match GPU/software byte-for-byte. Both final images were inspected; widened
scenery is present and gameplay models remain visible. Evidence directories:
`tmp/tunnel-final-ORIGINAL-16_9-{GPU,SOFTWARE}-sep19` and
`tmp/tunnel-final-EX-32_9-{GPU,SOFTWARE}-sep19`, `presentation.bmp` in each.
Hashes respectively: D2ECD3F73D63A82AC2A6E378FD4E9AF6F446D7678CB8EC3BABAB8D89D7E92562
and 35CD5F791346E03B689803992F2F74C984C1B398AFE000D68EC95EA4DBDC8521.
These complement the preceding Original 32:9/EX 16:9 samples, not every tunnel,
natural route or physical platform. No GitHub issue was closed by this check.

Fresh GitHub #44 comments still distinguish native Linux Gaming Mode failure
from working Windows-under-Proton input. Local native tests cannot establish
physical Deck acceptance. #46 remains open with no new reporter confirmation.

## #48 widescreen tunnel choice implemented (September 19)

User explicitly selected extending the ceiling/floor, superseding the earlier
solid-margin-only requirement below. Desktop BG2 now widens one authored
cross-section across the viewport, without repeating it. Native 256-wide
sampling stays unchanged. The low-priority background includes the widened
section; foreground priority retains its native sampling/coverage so expanded
walls cannot erase gameplay models. Transparent outside material uses the
source wall colour. CPU and portable GPU implementations updated together.

Original 32:9 captures in `tmp/titania-wide-unclipped-{gpu,software}-sep19`
match byte-for-byte; GPU image visually inspected with extended ceiling/floor,
player/enemy models and HUD visible. EX 16:9 capture is retained at
`tmp/titania-wide-unclipped-ex-sep19`. Four focused tests pass, including
Original/EX source tunnel phases and split-priority extension regression.
Current Windows executable rebuilt; no release or physical Android acceptance.
Earlier `titania-extended`, `titania-wide-section` and `titania-ceiling-final`
captures are intermediate experiments, NOT final evidence.

## Steam Deck overlapping device names (September 19)

The physical-device duplicate filter now excludes positively identified Steam
Input devices before examining their names. Previously a Valve virtual device
named `Steam Deck Virtual Controller` matched both categories and was erased,
potentially leaving no controller at all. Regression coverage now exercises
three virtual names, preferred selection, multiplayer duplicate removal and
fallback after disconnection. Fresh Windows and native Linux runtime input
tests pass (0.45s and 0.02s respectively).

The bundled SDL source confirms that the existing
`SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD` environment variable is the
actual virtual-device filtering control, rather than an SDL hint. This source
and simulated-device evidence does not prove that issue #44's physical Gaming
Mode/system-button problem is fully resolved; that acceptance remains open.

## Game Over successful stereo coverage (September 19)

`tools/check_game_over_gpu.ps1` now pins DLSS, language, 2D filtering and
Enhanced Lighting rather than inheriting saved preferences. In addition to
GPU/CPU late-star and mono/stereo failure comparisons, it exercises successful
Half and Full SBS presentation, verifies packed dimensions, rejects stereo
fallback, and checks nonblack margin stars separately for both eyes.

Original and EX pass at 32:9/2x in `tmp/game-over-sbs-current-sep19`.
Mono/GPU/CPU/forced fallback images are byte-identical; successful Half SBS is
1600x448 and Full SBS 3200x448, with actual extended stars in both eyes. The
EX Full SBS image was visually inspected. This improves local #46 evidence;
it does not claim physical Quest/Index or Android acceptance or close the issue.

## Titania first-corridor capture correction (September 19)

The corridor capture now enters the authored BG_2_3C command, separately from
the existing BG_2_3B water fixture. It does not advance the extra 30,000 map
distance used to reach the water exit. Frames and GPU/software renderer are
selectable, and the capture checks actual INATUNNEL and tunnel classification.

Important correction: an initial Mode 2 assertion was wrong. BGS.ASM initializes
Mode 2, but its VOFF info then invokes WORLD.ASM's VOFSOFFPLEASE, which explicitly
writes Mode 1. Trace PC $03EB83 identifies that authored write; BG remains $99
and the corridor map still has its 5,000-distance wait. The old failed assertion
did NOT demonstrate that the scene had advanced to water. Temporary direct
background calls and CPU tracing were removed after establishing this.

Current final captures (180 presentations, neutral effects) are byte-identical
between GPU and software for each pair:
- Original 16:9: `tmp/titania-corridor-final-16-sep19/titania.bmp` and
  `tmp/titania-corridor-software-16-sep19/titania.bmp`.
- Original 32:9: `tmp/titania-corridor-final-32-sep19/titania.bmp` and
  `tmp/titania-corridor-software-32-sep19/titania.bmp`.
- EX 16:9: `tmp/titania-corridor-ex-16-sep19/titania.bmp` and
  `tmp/titania-corridor-ex-software-16-sep19/titania.bmp`.

Inspected images show the central tunnel cross-section with matching solid
green outer walls, no repeated corridor artwork, and wide HUD placement.
This follows the user's solid-border/no-duplicate tunnel requirement; it does
not widen the authored ceiling/floor cross-section. Do not call the reporter's
different request for a wider corridor resolved on the basis of CPU/GPU parity.
The water fixture also still passes (`tmp/titania-water-final-sep19`). These
are Windows captures, not a new Android-device confirmation. No issue mutation.

## Live issue recheck (September 19)

Re-read all six requested issue bodies and comments through GitHub:
- #43 is now CLOSED; the owner identified 0.0.6.7 as the fix.
- #44 remains OPEN. September 18–19 reports narrow it to Steam Deck native
  Linux/Game Mode: desktop launch works, and the Windows build under Proton 9
  detects the controller. A SteamOS laptop with a PS3 controller also works.
  Existing local virtual-device enumeration fixes are not physical confirmation.
- #46 remains OPEN; no additional comments change the Game Over starfield task.
- #47 remains OPEN, but the reporter explicitly confirms correction in 0.0.6.7
  (September 17). This is external evidence for the original Android report.
- #48 remains OPEN. The reporter confirms the ending-zone improvement on Android
  software rendering, but separately reports the FIRST corridor section remains
  4:3. Audit that section, not just the already-confirmed exit area.
- #49 remains CLOSED.

No issues were closed or commented on by this audit. These facts supersede the
older all-open/no-new-comments status elsewhere in the chronological notes.

## Setup controller routing (September 18)

Related #53 symptoms match a setup/gameplay mapping inconsistency: desktop
south/A defaults to SNES B, which some main-page actions accept but graphics
pages reject and Options interprets as Back. Setup now uses fixed navigation
with south Confirm/east Back on desktop, independent of gameplay bindings and
face-button swap. Existing fixed Switch navigation is preserved. Keyboard
setup uses arrows/X/Z/Enter; gameplay mappings are unchanged.
Windows and Linux runtime-input tests verify the distinction with virtual
gamepad buttons; Windows executable rebuild succeeds. The reporter's physical
Bazzite input and complete menu flow still need acceptance, so #53 is not closed.

## Steam virtual-controller enumeration follow-up (September 14)

For #44 and the related #57 Game Mode report, inspected the bundled SDL
gamepad filter: it rejects Steam virtual VID/PID devices unless environment
variable SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD is true. Our priority
logic could not select a controller already filtered from SDL_GetGamepads.
configure_native_gamepad_support now defaults this variable to 1 before input
initialization, without overriding any explicit launcher/user value. No forced
raw-HID access or Steam system-button interception was added.

Windows and WSL/Linux runtime-input suites pass, including new default/explicit
opt-out checks and existing virtual Deck/Steam preference, mapping and remap
fixtures. Physical Steam Deck Game Mode confirmation is still outstanding;
these synthetic tests do not prove the reporter's session is fixed.

## Reproducible current GPU handoff (September 13)

`tools/package_pc_quest.ps1` installs into a fresh unique directory, adds the
tracked player instructions and Quest APK, and checks the exact ten-file list
and current binary hashes before moving the archive to its requested path.
Existing archives are refused (guard tested); no earlier handoff is overwritten.
It never runs the game in the package directory. Failed candidates remain in
their unique temporary directory for inspection rather than being published.

Current development archive: `tmp/StarFox-Enhanced-PC-Quest-GPU-current-20260913.zip`
(13,045,300 bytes), SHA256
`37D13B5F12FB6A31C11B418192D00434640307B35F20F02CE636AE51C9ED6D6A`.
The APK and both executables match the current builds; no ROM/BIN, saves,
settings, add-ons or development audits are included. This supersedes the
older archive below, which remains preserved. Nothing was released or installed.

All Windows and Linux targets rebuilt against the Controls/caster migration.
Fresh full suites pass: Windows 55/55 (263.44 s), Linux 54/54 (277.00 s),
and the separately rebuilt Windows VR suite 15/15 (46.96 s). These concurrent
test durations are not performance benchmarks.
Quest and regular Android debug builds both succeed; the Android APK contains
its nonempty arm64 main/SDL/C++ libraries and manifest. Generated Metal literal
length and Gradle deprecation warnings remain non-fatal. This does not replace
physical device acceptance. All six requested issue bodies/comments were
re-read through GitHub; no new reports change the outstanding Deck/Android gaps.

## Clean handoff package regression (September 13)

Added `tools/check_pc_quest_package.ps1`: exact ten-file allowlist, duplicate/
empty-entry rejection, full entry decompression, and optional hashes against
the current APK and both Windows executables. A settings-contaminated archive
is rejected. The earlier smoke-run staging directory had generated BIN/config/
save files, so packaging now uses a fresh install directory rather than that
directory. No files from the contaminated directory were deleted.

`tmp/StarFox-Enhanced-PC-Quest-development-current.zip` passes the checker with
`-VerifyCurrentBuild`; no ROM/BIN, saves, settings, DLL add-ons or development
audits are included. SHA256:
`1CF1ADA64CB53DE8372ABAEB5B041DF23D86FB0F1D827785AC5B42CFC9494D22`.
The packaged asset-builder binary generated and validated a retail-derived BIN
in `tmp/asset-builder-current-check`, outside the package. No release was pushed.

## Regular Android shared-background refresh (September 13)

`:app:assembleDebug` succeeds in 24 s after the selective face-planet and
source tunnel-wall changes. APK inspection confirms nonempty arm64 libmain,
SDL3, C++ runtime and manifest entries. The activity still hides system bars on
create/resume/focus through current Insets and legacy immersive APIs. No device
installation or visual acceptance is implied. Non-fatal build warnings remain:
unused parameters in unsupported DXR stubs, long generated Metal literals, and
Gradle deprecations. These did not prevent native compilation or packaging.

September 13 live recheck: #43/#44/#46/#47/#48 remain open, #49 closed. No new
comments change the evidence or outstanding physical Deck/Android checks.

## Regular Android build refresh (September 12)

Fresh workspace-local `:app:assembleDebug` succeeded in 1m29s (35 tasks),
using the installed SDK/NDK/JDK. This builds the regular SDL Android app,
not Quest, including current shared HUD/comms code and fullscreen activity.
Inspected app-debug.apk: nonempty arm64-v8a libmain.so, libSDL3.so,
libc++_shared.so and AndroidManifest.xml are present. No installation or
publication was performed. Compilation/packaging does not prove Android
results-screen visuals or gesture/three-button navigation behavior.

GitHub bodies/comments rechecked: #43/#44/#46/#47/#48 remain open with no
new comments, #49 remains closed. Physical Deck mapping and Android visual
acceptance remain outstanding; issue states were not modified.

## Current authored Titania ending verification (2026-09-12)

Re-ran the native SETBG 2_3b map-command fixture, not a guessed elapsed
stage time. Current Windows GPU captures were visually inspected:

- `tmp/issue48-ending-current-16/titania.bmp`: Original 16:9.
- `tmp/issue48-ending-current-32/titania.bmp`: Original 32:9.
- `tmp/issue48-ending-current-ex/titania.bmp`: EX 16:9.

All show sky/mountains/water filling both margins and a single central bridge
cross-section, without repeated bridge wedges at the outer edges. Runtime
trace confirms Mode 1 with water scanline mode (hofs=1). The capture script
now requires this state and retains its process handle for reliable ExitCode.
VR native Vulkan fixture also passed and its forward-eye image was inspected:
`tmp/issue48-water-vr-current/live-scene-left.bmp` (Original, background 147,
Mode 1, 200 ticks after the authored water command). This is offscreen Vulkan
evidence, not a new physical headset acceptance run or complete playthrough.


2026-09-12 latest GitHub recheck: #43/#44/#46/#47/#48 remain OPEN and #49 CLOSED;
no new comments beyond the previously recorded reports. Rebuilt
starfox_transition_parity_tests against current shared core and reran both
Original and EX cartridge fixtures successfully. This is automated transition
regression evidence, not physical Steam Deck/Android acceptance or issue closure.

2026-09-12: rebuilt and reran transition parity against Original and EX ROMs.
Corrected the revival fixture to explicitly select EX simulation behavior for
EX assets (the constructor defaults to Original). Both modes pass the Corneria
and bottom-route meteor palette-row restoration checks. This strengthens the
automated checkpoint regression; it does not verify the reporter's Falco ship
configuration or replace physical-device acceptance.

Reports and comments inspected through GitHub on 2026-09-09. No issue closure
or release publication is implied by this local checklist.

Live GitHub recheck on 2026-09-11: #43, #44, #46, #47 and #48 remain OPEN;
#49 remains CLOSED. Current report bodies still identify #43 as EX bottom-route
meteor death tint and #47 as Android tally/HUD/alignment. #44 still has only
the unanswered Steam Input question; #47 has no comments. Local Windows and
virtual-device results below do not close the physical Deck/Android gaps.

Rechecked #43 and #44 on 2026-09-10: no new comments/evidence. #44 still has
only the Steam Input clarification question, with no reporter answer; physical
Deck verification remains open rather than inferred from local input tests.

| Issue | Reported case | Status / required evidence |
| --- | --- | --- |
| [43](https://github.com/kandowontu/starfox-enhanced/issues/43) | EX original pacing, bottom-route meteor stage; red tint survives death | Fixed locally: restore model palette row seven from the selected GAMEPALBUFF after checkpoint background requests. Regression failed on EX LEVEL3_2 before the change and passes for both Original/EX, Corneria/meteor stages afterward. Actual death/respawn captures below. |
| [44](https://github.com/kandowontu/starfox-enhanced/issues/44) | Steam Deck controls mapped to desktop actions; system buttons unavailable | Removed application Deck HIDAPI override that bypassed a launcher's global HIDAPI disable. Regression tests cover inheritance and explicit device enable/disable. This fixes a verified backend-policy conflict, not yet the entire report: physical Deck/Steam Input verification remains. |
| [46](https://github.com/kandowontu/starfox-enhanced/issues/46) | Extend Game Over stars into widescreen margins | Fixed locally: draw the existing world-space stars into only the added margins, after the solid backdrop fill. The native artwork remains untouched. Captured Original 16:9 and EX 32:9; source-width exclusion tested at 1x/2x/4x. |
| [47](https://github.com/kandowontu/starfox-enhanced/issues/47) | Missing completion bar/HUD and total score alignment, Android | Fixed locally in shared rendering: restore MSHOWPERCGRAPH geometry/colours, retain native meters at tally entry, draw HUD sprites above models and right-align percentage/total to Slippy's frame. Matching Windows runtime captures below; Android-device verification remains. |
| [48](https://github.com/kandowontu/starfox-enhanced/issues/48) | Titania ending area background remains 4:3 | Fixed locally: distinguish open water (INATUNNEL=2) from enclosed tunnels (1), and extend one bridge cross-section without repeating it at ultrawide edges. Windows Original 16:9/32:9 and EX 16:9 captures; water/bridge/tunnel substrate regressions pass. |
| [49](https://github.com/kandowontu/starfox-enhanced/issues/49) | Development documentation in release package | Already closed on GitHub, but install rules still shipped audits. Removed those rules locally; font notices retained under licenses/fonts. Verified fresh installation into tmp/package-issue49-check: only two executables, README, credits, third-party notices, xBRZ licence and font notices; no docs folder. |

Visual evidence should be actual runtime captures, not generated illustrations.
Nonvisual changes require tests or package manifests rather than decorative screenshots.

## Issue 44 route diagnostic

2026-09-10 additional compatibility change: identify Steam Input by Valve's
28de:11ff virtual-gamepad ID as well as its name. Prefer that stream for a
single player and exclude the duplicate raw Deck (28de:1205/name) when Steam's
virtual controller is present. Otherwise retain native SDL Deck handling;
do not override global HIDAPI policy or rewrite the user's remappings.
Windows SDL virtual-device tests verify selection, native fallback after
disconnect, no duplicate EX player, all 12 standard buttons, and no game action
from Steam/Quick Access buttons. Physical SteamOS/system-menu behavior is not
yet verified. This cannot transform a Steam desktop keyboard-only profile
into gamepad events when no controller stream is exposed.

References: [Valve gamepad emulation](https://partner.steamgames.com/doc/features/steam_controller/steam_input_gamepad_emulation_bestpractices)
and [SDL native Deck driver policy](https://wiki.libsdl.org/SDL3/SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK).

Rechecked the live issue on 2026-09-10: it remains open; the sole comment asks
whether Steam Input is being used. Keyboard-layout emulation is a plausible
explanation, not a verified diagnosis. No physical Deck is available locally.

The developer build now has `starfox_controller_check` (not installed into
release packages). `--snapshot` reports SDL version/platform, effective HIDAPI
policy, whether Steam launch context exists, and joystick mapping/vendor/product
information. It does not dump environment values, device paths or serials.
`--seconds 15` opens a bounded diagnostic window and the same preferred gamepad
as the game; while focused it counts gamepad/axis and keyboard events. Only
Enter/Escape/Tab are named; other keys are redacted and no text is captured.
Escape exits early; the maximum duration is 30 seconds. Run from the same
Steam shortcut/layout as the game when checking that route, then compare with
a direct launch. The tool does not change Steam configuration or bindings.

Windows/Linux builds and snapshot checks pass; Windows dummy-window lifecycle
also passes with no attached controllers. This is diagnostic tooling, not resolution or physical verification
of issue 44. Its output should determine whether further work belongs in input
selection/mapping or Steam launch configuration before changing either.

## Issue 48 captured evidence

Current-build refresh (2026-09-10): rebuilt `build/current/starfox_pc.exe` and
captured Original/EX at 4:3, 16:9 and 32:9. The capture script now isolates and
restores diagnostic environment settings, forces original pacing/GPU rendering,
disables optional visual effects and records the reached render state. Both
logs report GPU-resident raster, Mode 1, BG2 scanline scrolling; background IDs
are `$93` (Original) and `$cf` (EX). The four wide captures were visually reviewed:
one central bridge cross-section, with water/sky continuing through the margins.

Against each game's fresh 4:3 reference, the central 512 stored pixels at rows
80..219 are byte-identical at both wide ratios (71,680 RGB pixels per comparison).
This is a background-band preservation check, not whole-HUD or full-level parity.
Current captures live in `tmp/titania-current-{original,ex}-{native,16,wide}`.

![Current Original 32:9](../tmp/titania-current-original-wide/titania.bmp)
![Current EX 32:9](../tmp/titania-current-ex-wide/titania.bmp)

`tools/capture_titania.ps1` enters the cartridge's authored BG_2_3B command,
including its native initializer and water scanline program. BGMACS.INC assigns
INATUNNEL=1 for enclosed tunnels and 2 for water; treating both as tunnels
incorrectly blacked out Titania's added columns. The water layer now extends
its edge material while BG3 continues the mountain/sky backdrop. A contrasting
bridge fixture verifies that the central cross-section does not repeat at
400- or 796-pixel widths; separate tests preserve solid tunnel margins.

![Original before](../tmp/issue48-before/titania.bmp)
![Original 16:9 after](../tmp/issue48-after/titania.bmp)
![Original 32:9 after](../tmp/issue48-ultrawide/titania.bmp)
![EX 16:9 after](../tmp/issue48-ex/titania.bmp)

These are Windows runtime captures, not physical console verification.

## Issue 47 captured evidence

`tools/capture_results.ps1` follows the cartridge's authored CL_WARP transition
from LEVEL1_2. Before/after captures at the same frame (1440), Original 16:9,
2x rendering:

![Before completion bar and HUD restoration](../tmp/issue47-before/results.bmp)
![After completion bar and HUD restoration](../tmp/issue47-after/results.bmp)

The source MTXTPRT.MC draws a 104x12 border at (60,24), with an inset 100x8
maximum fill. Tests cover 0/1/50/100/255 values, clamping and untouched pixels
outside the bar. MAIN.ASM keeps the live stage underneath its tally; the host
had explicitly cleared M_METERS and excluded this flow from the final OBJ pass.
The code is shared with Android, but these captures are Windows evidence only.

## Issue 43 captured evidence

`tools/capture_revival.ps1` runs the actual EX LEVEL3_2 death strategy and
captures through respawn at original pacing, 60 FPS, 2x rendering. No save or
menu preference is modified. The screenshots below are the same frame number
from before and after the palette fix, not recreated illustrations.

Before (pink model palette persists):
![Before respawn palette fix](../tmp/issue43-baseline/001740.bmp)

After (selected model palette restored):
![After respawn palette fix](../tmp/issue43-fixed/001740.bmp)

The complete capture sequences and runtime logs remain in those directories.

## Issue 46 captured evidence

`tools/capture_game_over.ps1` captures the real Game Over flow. Matching
Original 16:9 captures at 2x and frame 90:

![Before star extension](../tmp/issue46-before/gameover.bmp)
![After star extension](../tmp/issue46-after/gameover.bmp)

Pixel comparison found **zero changes in the native 256-pixel-wide canvas**
and 24 changed stored pixels in the margins. Andross and the source stars/text
remain centred and identical. The newly visible stars use the same camera,
point cloud and projection, not tiled backdrop art.

![EX 32:9 star extension](../tmp/issue46-ex/gameover.bmp)
