# Startup reports — September 19

Read current GitHub issue #58 and both attached screenshots. The report shows a
black Windows window in 0.0.6.7 versus a working 0.0.6.5 EX pregame menu using
GPU rendering. It contains no GPU/driver identity, log, or comments. The images
do not prove a shader, DLSS, asset-loader or audio cause.

Issue #64 is distinct: Android freezes when enabling preview or entering the
game. The reporter's follow-up attributes it to GPU rendering and says Software
works. Do not silently treat this as the same Windows startup failure.

Desktop startup now appends flushed, elapsed-time milestones to `startup.log`
beside the executable: optional DLSS initialization, SDL, settings, renderer,
assets/simulation, audio initialization and first presented frame. Exceptions
are recorded when reached. Logging failures are nonfatal on read-only paths.
No per-frame writes after the first frame, no automatic settings reset and no
speculative driver timeout/termination were added.

Next evidence needed from an affected launch: last startup stage, existing
standard-error log if available, GPU/driver and whether software mode works.
Neither issue is claimed fixed by diagnostic instrumentation.

## September 20 startup/backend matrix

Re-read both issues: no new hardware or startup-log evidence. Code inspection
does not establish a common cause. Added `tools/check_startup_backends.ps1`:
six fresh processes cover Original/EX with Software, D3D12 and Vulkan, actual
menu preview rendering, nonblack final captures, first-frame journal milestones,
and four GPU/software switches in each GPU case. All six pass on this Windows
host (`tmp/startup-backends-sep20`). Original Software and EX Vulkan captures
were visually inspected and contain readable menus over gameplay previews.

The test renderer override now applies before constructing the initial window.
Previously a requested Software diagnostic could first initialize the saved
GPU driver, invalidating its value as an isolated startup comparison. This is
a diagnostic correction, not a claimed production fix. Startup journals also
record SDL's selected renderer name rather than just the requested mode.
The harness restores environment variables, leaves player settings untouched,
and reports a live process ID on timeout without killing or restarting it.

No black screen or freeze reproduced in this matrix. This does not establish
Android GPU acceptance or resolve Windows #58; affected-adapter evidence is
still needed. No arbitrary driver blacklist, settings wipe, or timeout-based
process termination has been introduced.
