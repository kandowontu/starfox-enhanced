# Star Fox Enhanced

A native C++/SDL3 port of [UltraStarFox](https://github.com/Sunlitspace542/ultrastarfox),
with **Original Star Fox** and **Star Fox EX** experiences, high-frame-rate
presentation, widescreen support, and optional visual enhancements.

**[Download 0.0.6](https://github.com/kandowontu/starfox-enhanced/releases/tag/v0.0.6)** ·
[Changelog](docs/RELEASE-0.0.6.md) ·
[Settings and controls](docs/SETTINGS-AND-CONTROLS.md) ·
[Build guide](docs/BUILDING.md) ·
[Report a bug](https://github.com/kandowontu/starfox-enhanced/issues)

> This is an alpha release, not a cycle-accurate SNES emulator. A supported,
> unmodified retail ROM supplied by you is required. No retail ROM is included.

## Get started

1. Download the package for your platform. Desktop users should extract it
   into a writable folder before launching; no source build is necessary.
2. Launch the game. On desktop, place your supported `.sfc`/`.smc` ROM beside
   the executable, or set `STARFOX_RETAIL_ROM` to its path. Mobile apps provide
   a first-launch file picker.
3. The game validates your ROM and creates `Starfox-Assets.BIN` locally.
   Keep that companion with the application; it may need rebuilding after
   updates to the embedded assets.
4. Choose **Original** or **Star Fox EX**, adjust your options, then select
   **Start Game** from the main menu.

For devices needing prepared assets, the release includes a standalone Windows
[asset builder](https://github.com/kandowontu/starfox-enhanced/blob/main/platform/mobile/ASSET_BUILDER.md). It accepts the same ROMs;
mobile users can also select a ROM directly.

### Supported ROMs

- Star Fox Japan: 1.0 / 1.1
- Star Fox USA: 1.0 / 1.1 / 1.2
- Starwing Europe: 1.0 / 1.1
- Starwing Germany: 1.0

A 512-byte copier header is accepted. Known revisions are checksum-verified
and canonicalized before the source-built patches are applied. Hacks, betas,
competition cartridges, Star Fox 2 and unknown revisions are not supported.

## Platforms

| Package | Notes |
|---|---|
| Windows x64 / x86 | Extract and run `starfox_pc.exe`. |
| Linux x64 | Native SDL3 runtime. |
| macOS universal | Unsigned application for Intel and Apple Silicon. |
| iOS arm64 | Unsigned device bundle; [signing/sideloading instructions](https://github.com/kandowontu/starfox-enhanced/blob/main/platform/apple/IOS_INSTALL.md). |
| Android arm64 | Installable development APK; see the upgrade notice below. |
| Nintendo Switch | Homebrew NRO; [setup and optional forwarder](https://github.com/kandowontu/starfox-enhanced/blob/main/platform/switch/README.md). |
| PS Vita | Homebrew VPK; [setup](https://github.com/kandowontu/starfox-enhanced/blob/main/platform/vita/README.md). |
| Xbox UWP x64 | Developer Mode required; [setup](https://github.com/kandowontu/starfox-enhanced/blob/main/platform/uwp/README.md). |

Build success does not guarantee identical behavior on every device.
See the [release notes](docs/RELEASE-0.0.6.md) for verification limits.

**Android upgrades:** 0.0.6 uses a permanent signing key. Older APKs used
temporary keys, so upgrading from those builds requires a one-time reinstall.
**Back up saves and settings before uninstalling.** Later public builds will
retain the permanent signing certificate.

## Features and settings

- **Original and EX:** original routes and frontend flow, plus EX's shipped
  campaigns, native options and mechanics.
- **Smooth presentation:** 20–480 FPS choices, defaulting to 60. Game pace is
  independent of render FPS; Original Speed preserves source-style slowdown.
- **Display:** 4:3, 16:10, 16:9, 21:9 and 32:9; GPU or software presentation.
- **Languages:** English, Japanese, German, French and Spanish, including menus
  and dialogue. EX translations include authored additions.
- **2D Options:** artwork filtering, 2D Bloom, World Effects and their intensity.
  The artwork filter also covers textures on 3D polygons.
- **3D Options:** anti-aliasing, VSync, 1–4× Render Upscale, 3D Bloom, 3D Smoothing,
  RTX Lighting, HDR Effect, Enhanced Shadows, Chromatic Aberration, Model Effects
  and independent Wireframe Thickness. HDR Effect is brightness/contrast
  processing, **not HDR display output**.
- **Preview:** a fixed reference scene shows graphics changes live. Hold **Tab**
  to hide the menu temporarily. Preview defaults off each launch.
- **Customization:** draggable HUD layouts, controller/keyboard remapping,
  crosshair colors, separate music/SFX volumes, rumble and optional God Mode.

Model and world effects are independent; CEL-DRAWN is model-only and BLUEPRINT
is world-only. Most visual enhancements are optional. Higher render scales and
additional effects can increase CPU, GPU and memory use.

Optional Original MSU-1 music requires `Starfox-MSU1.PAK` beside the desktop
executable (or in the platform's writable storage). It is not included in the
standard packages. Without it, native SPC music remains available.

See [Settings and controls](docs/SETTINGS-AND-CONTROLS.md) for detailed options,
EX peripherals, multiplayer and presentation debugging.

## Default controls

| SNES control | Keyboard | Gamepad position |
|---|---|---|
| D-pad | Arrow keys | D-pad / left stick |
| B | Z | South |
| Y | A | West |
| A | X | East |
| X | S | North |
| L / R | Q / W | Shoulder buttons |
| Select | Apostrophe (`'`) | Back/View |
| Start | Enter | Start/Menu |

Button names above follow the SNES layout, not the letters printed on an Xbox
controller. Remap controls under **Options → Controller**. Menu A actions fire
once per press; B or Back returns from submenus.

| Shortcut | Action |
|---|---|
| Escape | Exit confirmation |
| Tab in setup | Hide the menu while held |
| Tab / Ctrl+Tab / Ctrl+Shift+Tab outside setup | 2× / 3× / 5× fast-forward |
| Ctrl+Shift+R | Reset; remap the final key under Keyboard → Reset |
| Ctrl+Alt+F12 | Toggle God Mode live |
| F12 on the main setup page | Enable/disable presentation history for this run |
| F5 / F6 / F7 | Freeze / step forward / step backward with history enabled |

## Saves and upgrades

Desktop builds are portable: keep these files beside the executable when
moving or upgrading the game. Use a writable folder, not a read-only directory.

| File | Purpose |
|---|---|
| `Starfox-Assets.BIN` | Locally reconstructed runtime assets |
| `starfox-ex.srm` | EX cartridge SRAM |
| `pregame.cfg` | Game and graphics preferences |
| `input-bindings.cfg` | Keyboard and controller mappings |
| `hud-layout.cfg` | Per-experience, per-aspect-ratio HUD layouts |
| `Starfox-MSU1.PAK` | Optional replacement music |

The first normal launch copies missing data from former preference locations;
existing portable files win, and originals are not deleted. Mobile and console
builds use writable platform storage instead of their read-only package folders.

## Development and credits

To build, regenerate source assets or run tests, use the
[build guide](docs/BUILDING.md). The
[architecture notes](docs/ARCHITECTURE.md) explain the hybrid native/65C816
boundary: gameplay remains fixed-point while extra presentation frames are
interpolated. Visual parity remains an ongoing effort.

See [Credits](CREDITS.md) for Nintendo/Argonaut, EX, UltraStarFox and port
contributors, and [Third-party notices](THIRD_PARTY_NOTICES.md) for dependencies
and licenses. Development uses Codex.
