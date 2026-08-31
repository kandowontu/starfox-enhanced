# Nintendo Switch homebrew build

The release archive contains the native homebrew application at:

`switch/StarFoxEnhanced/StarFoxEnhanced.nro`

Copy that folder to the `switch` directory on a homebrew-enabled Switch SD
card. You may rename the application folder. Before launch, create
`Starfox-Assets.BIN` on a PC with the standalone asset builder and place it
beside the NRO. For the packaged folder, that is:

`sdmc:/switch/StarFoxEnhanced/Starfox-Assets.BIN`

The application never contains a retail ROM. The optional MSU-1 companion can
also be copied beside the NRO as `Starfox-MSU1.PAK`.

## Optional NSP forwarder

`package_switch_nsp.ps1` creates an NSP *forwarder* for the NRO by invoking
[NTON](https://github.com/rlaphoenix/nton). Install NTON on the PC, put a
`prod.keys` dumped from your own console at `$HOME/.switch/prod.keys`, then run:

```powershell
.\package_switch_nsp.ps1 `
  -NroPath .\switch\StarFoxEnhanced\StarFoxEnhanced.nro
```

The forwarder expects the NRO to remain at
`sdmc:/switch/StarFoxEnhanced/StarFoxEnhanced.nro`. NTON writes the generated
NSP to `Desktop/NTON`.

No console key is included, requested, logged, or uploaded by this project.
An NSP forwarder requires a modified console and may carry console/account-ban
risk. The `.nro` through the Homebrew Menu is the supported default.

## Source build

Use devkitPro's `devkita64` environment with the `switch-dev` and
`switch-portlibs` groups installed:

```bash
DEVKITPRO=/opt/devkitpro tools/build_switch.sh
```

The build uses SDL 3.4.14 plus a pinned open libnx backend because upstream
SDL's official Switch backend is distributed only through its NDA-gated fork.
The exact backend revision and checksum are recorded in `CMakeLists.txt` and
`THIRD_PARTY_NOTICES.md`.
