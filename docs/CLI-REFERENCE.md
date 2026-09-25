# Runtime command-line reference

These are the supported command lines for the distributed desktop executables.
The flat game's menu settings are not CLI flags; use the in-game menu. Launch
a map by its uppercase label for testing, not as a substitute for a natural
playthrough (the latter supplies preceding-stage state and transitions).

## Flat PC runtime

```text
starfox_pc [--fullscreen] [MAP]
starfox_pc [--fullscreen] ROM SYMBOLS [MAP]
```

Without `MAP`, the game enters `BOOT` (the pre-game setup menu). The
two-file form loads an explicitly supplied development ROM and symbol table.
The embedded-asset build needs no external ROM or symbols. `--fullscreen`
starts the desktop game in fullscreen and may appear before or after the
positional arguments. The executable has no `--help` option.

Host flow entries: `BOOT`, `PLANETSELECT`, `TITLEMAP`, `INTROMAP`,
`CONTMAP`, `GAMEOVER`, `CONTINUE`, `CREDITSMAP`. `TRAININGMAP` is
a ROM-backed training entry.

Playable Original stage labels:

```text
LEVEL1_1 LEVEL1_2 LEVEL1_3 LEVEL1_4 LEVEL1_5 LEVEL1_6
LEVEL2_1 LEVEL2_2 LEVEL2_3 LEVEL2_4 LEVEL2_5 LEVEL2_6
LEVEL3_1 LEVEL3_2 LEVEL3_3 LEVEL3_4 LEVEL3_5 LEVEL3_6 LEVEL3_7
LEVEL_SPECIAL LEVEL_BLACKHOLE LEVEL1_END
```

Star Fox EX adds the following labels to the Original stage labels:

```text
LEVEL4_1 LEVEL4_2 LEVEL4_3 LEVEL4_4 LEVEL4_5
LEVEL5_1 LEVEL5_2 LEVEL5_3 LEVEL5_4 LEVEL5_5
LEVEL6_1 LEVEL6_2 LEVEL6_3 LEVEL6_4 LEVEL6_5 LEVEL6_6
LEVEL7_1 LEVEL7_2 LEVEL7_3 LEVEL7_4 LEVEL7_5
LEVEL_COMET
```

EX also contains `LEVEL_CREDTEST` and `LEVEL_CREDTEST2`; they are
diagnostic credits entries, not campaign stages. A map label must exist in the
selected cartridge's symbol table.

## PCVR player

```text
starfox_pcvr [--bundle PATH] [--data-dir DIRECTORY] [--msu PACK] [--enhanced-sky]
starfox_pcvr --help
```

The default bundle is `Starfox-Assets.BIN` beside the executable. Save files
and preferences default to its `vr-data` directory. An optional
`Starfox-MSU1.PAK` beside the executable is auto-detected; `--msu` overrides
it. `--enhanced-sky` enables that option at startup. The headset must have
an active OpenXR runtime, even when using a standard gamepad.

## Asset builder

```text
starfox_asset_builder RETAIL_ROM [Starfox-Assets.BIN]
```

The builder accepts a supported unmodified retail Star Fox/Starwing dump,
validates the output against the current build's asset manifest, and does not
ship a ROM. The PCVR ZIP includes `BUILD-ASSETS.bat` and
`LAUNCH-PCVR.bat` wrappers with persistent, readable errors.
