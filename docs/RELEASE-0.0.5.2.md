# Star Fox Enhanced 0.0.5.2 — pending release

## Fixes

- Score completion percentages use the cartridge's numeric glyphs, rather than
  the text character mapping that produced solid blocks. Shared by every platform
  and both Original and EX; regression coverage checks every percentage from 0
  through 100 against the source font pixels.
- EX's NEW crosshair presentation matches successive sight positions by distance
  from the player, not the recycled particle identity. This prevents markers from
  sweeping between near/far positions at high presentation frame rates without
  changing the native simulation. Regression coverage includes 60, 120, 240 and
  480 FPS interpolation phases.
- EX's 3D sight markers use the main-menu crosshair color setting; other models
  retain their own palette.
- Android CI packages use a permanent signing certificate. CI verifies the
  expected public certificate fingerprint before accepting an APK. Both local
  debug and release build types can use this key through environment settings.
- Switch's SDL AUDOUT backend now pipelines its two hardware buffers instead of
  waiting for the hardware queue to empty after every chunk. It reuses the buffer
  actually returned by AUDOUT, handles empty wakeups, and reports device errors.
  A host-side test exercises 1,000 handoffs using the actual patched callbacks.

## Android upgrade notice (#36)

Earlier public APKs used temporary runner-generated debug keys. A new permanent
key cannot update an APK signed with one of those old keys. Back up your saves and
user data before a one-time uninstall/reinstall; uninstalling can delete app data.
Subsequent public builds will retain the new certificate and application ID.
Do not expect independently built debug APKs to update a public installation.

## Verification limits

The Switch pipeline defect is covered by a deterministic host regression, but
the reported Erista crackling still requires listening tests on real hardware,
with and without overclocking. A successful cross-build is not an on-device audio
verification. Platform build and full-suite results are pending for this draft.
