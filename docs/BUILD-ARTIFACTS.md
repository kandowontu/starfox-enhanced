# Local test builds — 2026-09-04, final level-clear pass

These historical local candidates include the final EX tally bitmap/portrait
fixes and were validated before preparing 0.0.4 ALPHA. The hashes below refer
only to these candidates, not the subsequently versioned release packages.
For published downloads and checksums, use the v0.0.4 GitHub release.

| Target | Local handoff |
| --- | --- |
| Windows x64 | `dist/StarFoxEnhanced/starfox_pc.exe` |
| Xbox UWP x64, package version 0.0.3.4 | `dist/StarFoxEnhanced-0.0.3.4-xbox-uwp-x64-test.zip` |
| Switch native NRO | `dist/StarFoxEnhanced-0.0.3-switch-test.zip` |
| PS Vita native VPK | `dist/StarFoxEnhanced-0.0.3-vita.zip` |

SHA-256 of each principal payload (not the enclosing ZIP):

```
Windows starfox_pc.exe
33DAF1F0DB9CAFB10705715102588104F5C8E2875FFDF4DBC97B3FFC5923AAE6

Xbox StarFoxEnhanced-0.0.3-xbox-uwp-x64.appx
A9E1FC380420DF6D525200E9B8761ECF333D6A620151AAB1678AB149678F65B4

Switch StarFoxEnhanced.nro
CB0E0677429304AB2554397290A15B355233C4519F38ECE5BF52A09534E5EDC6

Vita StarFoxEnhanced.vpk
0515E7A6E441299DA9FDC4FC043CA0A2A151248A9902DCFFB0AFB59A9AD23D47
```

The Xbox archive contains the APPX, public certificate, x64 VCLibs dependency
and instructions. Import/framework symbol, signature and unpacked-payload
checks pass. No private certificate key is included. The tester has confirmed
an earlier build boots on Series S, but **WGI controller input still needs
on-console confirmation**.

Switch and Vita executables were rebuilt using native AArch64/devkitPro and
32-bit ARM/VitaSDK toolchains, respectively. The Vita VPK contains eboot.bin
and sce_sys/param.sfo. Their ZIPs include asset preparation instructions and
credits/notices, not game assets. **Vita boot/input/performance and Switch's
immediate audio-delay report remain hardware tests.**

Full CTest: 35/35 passed in 375.64 seconds. After the final renderer-only
changes, focused runtime smoke/exit tests passed 4/4; clear/tally/warp captures
were rechecked for both games. See LEVEL-CLEAR-VALIDATION.md,
ENDING-VALIDATION.md and ACTIVE-HITLIST.md for the test boundaries.
