# PS Vita native build

`StarFoxEnhanced.vpk` is a native VitaSDK/SDL3 build for PS Vita and
PlayStation TV. Install it with VitaShell or another VPK installer.

The game data is not included. On a PC, use `starfox_asset_builder` with a
supported clean retail ROM to create `Starfox-Assets.BIN`, then copy it to:

```
ux0:data/StarFoxEnhanced/Starfox-Assets.BIN
```

The optional music companion is discovered case-insensitively at:

```
ux0:data/StarFoxEnhanced/Starfox-MSU1.PAK
```

The same directory is used for persistent settings and Star Fox EX SRAM.
This target uses the Vita's native buttons and sticks; mobile on-screen
controls are not displayed.

The VPK is cross-compiled and its package structure is verified. Boot,
controller input, audio latency and sustained performance still need a real
Vita/PlayStation TV test. Start with 1x Render Upscale on this hardware.

## Building

Install [VitaSDK](https://vitasdk.org/), including `zlib` and `libvita2d`, put
its `bin` directory on `PATH`, set `VITASDK`, then run:

```sh
tools/build_vita.sh
```

The output is written to `dist/StarFoxEnhanced-vita`.
