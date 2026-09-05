# Xbox startup investigation

## Current report

2026-09-04: tester Paulochel20 confirms Xbox Series S, installed version
0.0.3.1, no game/provisioning screen before returning to Home. No console log
received yet. Windows success must not be described as Xbox verification.

## Evidence

- The locally distributed 0.0.3.1 executable imports `vccorlib140.dll`,
  `MSVCP140.dll`, `VCRUNTIME140.dll`, and `KERNEL32.dll` (desktop variants).
  The included Microsoft.VCLibs.140.00 framework supplies the `_app.dll`
  variants instead. A desktop can resolve these extra DLLs from System32;
  that is not a valid Xbox loader test.
- Ninja's link command originally relied entirely on the invoking shell's
  `LIB` environment. Incremental linking outside `vcvarsall x64 store` could
  silently switch the runtime import libraries.
- The GitHub v0.0.3 Xbox ZIP still has internal version 0.0.3.0 (September 1).
  This does not explain the current tester's explicitly confirmed 0.0.3.1.
- The earlier Application Hang event is from the v6 build, not the final v7
  build. The existing v7 executable stayed running in a normal Windows UWP
  launch, but was resolving desktop runtime DLLs. Neither test proves Xbox
  compatibility.
- With real UWP runtime DLLs and the speculative WASAPI bypass removed, the
  audio worker deadlocks reproducibly: `WASAPI_ActivateDevice` calls
  `WASAPI_PrepDevice`, which queues back to the same management thread.
  Debugger inspection confirmed `ManagementThread->threadid == 0`.
  The fork's stdcpp creation backend never initializes this field; SDL3's
  common `SDL_RunThread` no longer initializes it on the backend's behalf.

## Revision 2 changes

- Explicit MSVC x64 Store library search path and UWP ABI definitions across
  the whole build; desktop kernel32/user32 import libraries excluded.
- Use runtime-initialized mutexes for the bundled UWP C++ framework.
- Packaging rejects desktop runtime imports and verifies every imported
  C++ runtime symbol exists in the supplied framework.
- Explicit x64 manifest identity and dependency minimum 14.0.33519.0.
- Log from WinMain through activation and first frame, with package version
  and WinRT HRESULTs. Loader failures can still occur before logging begins.
- Removed the speculative WASAPI synchronous-task bypass. Audio work must
  stay on its management thread, as SDL requires.
- Restore the original WASAPI endpoint enumeration. Populate the stdcpp
  backend's thread ID from `std::thread::get_id()` before returning creation.
  A startup self-test checks reported versus observed worker ID, preventing
  regression into a silent audio deadlock.

Keep console verification and Windows AppContainer smoke tests separate.
Do not overwrite a public release based solely on packaging or desktop tests.

## Local validation (revision 2)

- Import validator rejects the original revision 1 executable for its desktop
  `vccorlib140.dll` import. The corrected packaged PE passes and every imported
  runtime symbol is provided by the included Microsoft-signed framework.
- Rebuilt/relinked from a desktop `vcvarsall x64` shell (without `store`);
  explicit Store library paths still produce `_app.dll` imports. Guard passes.
- Normal Windows UWP launch, no debugger or lifecycle exemption: thread
  identity self-test passes, audio opens, first frame and first game/menu
  frame are logged. The app remains responsive beyond the activation period.
- Missing-BIN launch remains running. Restoring the BIN while it runs advances
  automatically through audio initialization into the menu. No microphone
  capability or custom WASAPI enumeration path was needed for this local test.
- MakeAppx unpacked payload matches the tested executable. Package certificate
  matches the included public CER. This is a self-signed developer package;
  the Windows smoke test used loose package registration, not trusted-store
  installation and not Xbox hardware.
- Final APPX SHA-256:
  `4D81E33EEF749E18193F47DF57CB6195752DC6E639EF78267ABF19C69B7B91A1`.
- Test handoff ZIP: `dist/StarFoxEnhanced-0.0.3.2-xbox-uwp-x64-test.zip`.
  Includes exactly the APPX, public CER, README and x64 VCLibs dependency.
  No assets, private keys, or debugger files are included.

## Series S follow-up and revision 3

The tester now reports that the revision 2 build boots, but the controller is
completely unresponsive even on the first selection menu. This is a separate
input failure; do not describe the successful boot report as input validation.

- The UWP SDL build enables `SDL_JOYSTICK_WGI`; XInput, RawInput and DirectInput
  are not compiled in. Its `WGI_JoystickInit` exits without enumeration unless
  `SDL_HINT_JOYSTICK_WGI` is enabled (default false).
- The application previously enabled only the XInput hint. Revision 3 enables
  WGI explicitly before SDL initialization on UWP only.
- Startup/hotplug logging includes the effective WGI hint, joystick/gamepad
  counts, opened names and the first nonzero controller input.
- Desktop SDL virtual-gamepad tests and a separately compiled UWP-configured
  input target verify initialization and mapping. These do not emulate the
  Xbox WinRT controller provider and are not physical-console input tests.
- Revision 3 remains a console test candidate pending Series S feedback.

Revision 3 packaged APPX SHA-256:
`76E651E65B2875BB88681DA0591AA6B2AA0BF103610C6AB7085CE7E6BD3C05CA`.
The import/runtime-symbol guard passes and MakeAppx unpacked payload matches
the built executable. Test ZIP: `dist/StarFoxEnhanced-0.0.3.3-xbox-uwp-x64-test.zip`.

## Revision 4 — shared hit-list pass

Rebuilt as 0.0.3.4 with WGI initialization/logging retained, scheduler and
scene-transition fixes, ending/level-clear corrections, smooth cockpit
markers and bounded audio playback. Includes the final EX tally/portrait
compositor changes verified by headless captures in both games.
Import/framework-symbol checks, package signature and unpacked-payload
verification pass again. Desktop virtual input and UWP-configured input
tests pass; no new Series S input confirmation has been received.

APPX SHA-256:
`A9E1FC380420DF6D525200E9B8761ECF333D6A620151AAB1678AB149678F65B4`.
Test ZIP: `dist/StarFoxEnhanced-0.0.3.4-xbox-uwp-x64-test.zip`.
