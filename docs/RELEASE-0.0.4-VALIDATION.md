# 0.0.4 release verification

Runtime source: tag `v0.0.4`, commit
`bdb046a60851eaf46bb8f8397e9ee435d7d30359`.

The final local CTest run passed **35/35 in 241.79 seconds** after the version
bump. All eleven downloaded payloads were opened/read where applicable and
their SHA-256 values matched GitHub's uploaded-asset digests. Archives contain
no retail ROM, generated `Starfox-Assets.BIN` or private signing key. The
separate MSU companion is unchanged from 0.0.3.

GitHub Actions run `33941634602` built and packaged Windows x64/x86, Linux,
macOS universal, iOS arm64, Android arm64, Switch NRO and the standalone Windows
asset builder. Its overall status is failed because of two packaging/runner
issues, not a claim that all nine hosted jobs passed:

- Xbox compiled, signed and passed APPX/import/framework checks in CI, but the
  ZIP step rejected the SDK's `Microsoft.VCLibs.x64.14.00.appx` filename. The
  released package was rebuilt locally from the same source with the existing
  MSVC UWP toolchain. APPX identity **0.0.4.0**, payload, public certificate and
  x64 VCLibs dependency were checked; only those files and instructions are
  in the ZIP. The CI dependency filename filter needs a follow-up correction.
- Vita's non-root CI container could not write GitHub runner state during
  checkout. No runner permissions were altered. The released VPK was rebuilt
  locally with the existing VitaSDK GCC 15.2 / SDL 3.4.14 toolchain. Its
  `APP_VER` is **00.04**, and `eboot.bin` plus `sce_sys/param.sfo` were verified.
  Hosted runner/container ownership configuration remains a follow-up.

Both local builds use the tagged runtime source. The release workflow's
Switch upload also published the draft early; it was returned to draft while
the remaining packages and checksums were assembled. Final publication is
performed only after all eleven payloads are present.

These build/package checks do not replace the physical Xbox controller,
Switch audio latency, Vita/mobile performance or full hardware-reference
playthrough checks listed in `ACTIVE-HITLIST.md`.
