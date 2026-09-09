# Settings and controls

Player reference for Star Fox Enhanced. For downloads and first launch, see
[the README](../README.md).

## Setup and graphics

The pre-game setup starts with an `EXPERIENCE` selector. `ORIGINAL` is
the default; `STARFOX EX` selects the embedded 1.11.03 source build, including
its native title/intro, three-page configuration menu, shipped `PLANETS` and
`PLANETS2` campaigns, custom stages, ships, models, palettes, music, and source
mechanics. EX's real 64 KiB cartridge SRAM is persisted byte-for-byte at
`starfox-ex.srm` beside the desktop executable; its source `SFEX` validation,
defaults, loading, START GAME commit, and L+R+DOWN+B intro reset paths all run
unchanged.

The setup also independently selects game pace, render FPS, display mode,
presentation renderer, MSU-1 music, rumble, controller remapping, and a
separate Options page. `RENDERER` defaults to GPU and can be changed to
SOFTWARE to use SDL's portable CPU presentation backend; this is an actual
backend switch and is saved with the other setup choices.
Under OPTIONS → CONTROLLER → KEYBOARD, the RESET action remaps the final key of
`Ctrl+Shift+R`. Ctrl+Shift stays fixed, the default suffix is R, and keyboard
defaults restore it. The shortcut requires both modifiers; plain Ctrl+R does
not reset. Remapping the suffix does not change normal gameplay bindings.
MSU-1 music is off by default and, when enabled for Original, replaces the
SPC music stem with the companion orchestral set while leaving sound effects
on their own channel. If `Starfox-MSU1.PAK` is not beside the executable, the
option reads `NOT FOUND` and cannot be enabled. Rumble is on by default for
Original and plays the authored
UltraStarFox sequences on compatible SDL, XInput, and Steam Input controllers.
The Options page also provides independent MUSIC and SFX volume controls.
The main page opens dedicated **2D Options** and **3D Options** submenus,
keeping full-height text and all main-page entries visible without scrolling.
2D Options contains 2D Filter, 2D Bloom, World Effects and World Effect Intensity.
3D Options contains Anti-Aliasing, VSync, Render Upscale, 3D Bloom, 3D Smoothing,
RTX Lighting, HDR Effect, Enhanced Shadows, Chromatic Aberration, Model Effects,
Model Effect Intensity, and Wireframe Thickness. B or BACK returns to
the corresponding main-page entry; live Preview stays active in either submenu.
`MODEL EFFECTS` and `WORLD EFFECTS` independently offer OFF,
INK, NEON, MONOCHROME, DITHERED, SEPIA,
THERMAL, NIGHT VISION, PASTEL, COMIC, and VAPORWAVE. The newer styles add warm
vintage tones, a false-color heat palette, green scanlines, soft colors,
halftone shading, and a purple/cyan palette respectively. CEL-DRAWN is model-only;
BLUEPRINT is world-only. World effects
cover the ground, sky, scenery and stars. Each graphics submenu provides its
effect intensity from 0–100% in 10% steps. Controller Remap remains in Options. All effect choices
are saved; older settings retain their model style with world effects OFF.
The `2D BLOOM` and `3D BLOOM` options independently offer OFF (default),
LOW, MEDIUM, and HEAVY for scenery and model light sources. They extract bright
pixels in linear light and spread tight and broad halos; HUD is excluded.
Older combined Bloom settings migrate to the same strength for both options.
`HDR EFFECT` offers OFF/LOW/MEDIUM/HIGH brightness and contrast processing; it is
not HDR display output. `CHROMATIC ABERRATION` offers three strengths of RGB
separation on models. `ENHANCED SHADOWS` replaces the original shadows with
geometry-based soft shadows. `WIREFRAME THICKNESS` ranges from 1 to 4 independently
of Render Upscale. These options are under 3D Options.

`3D SMOOTHING` separately offers OFF/LOW/MEDIUM/HEAVY for colour transitions
within model surfaces, including untextured faces. It does not replace silhouette
Anti-Aliasing or smooth backgrounds/HUD.
The old Bloom style migrates to MEDIUM bloom. Disallowed old style selections
fall back to OFF without changing other preferences.
Menu text is composited separately from scene effects. Main-page `PREVIEW` defaults OFF each launch; ON freezes the same
Corneria reference scene used by Customize Screen and previews graphics
changes live without advancing gameplay.

For MUSIC and SFX volume, left/right changes each in 10% steps; the mouse can drag either bar to any
whole percentage from 0 through 100. The first option is the
Star Fox EX-style God Mode: player collision is disabled,
regular Nova Bombs remain infinite, and holding R while pressing A fires a
God Nuke. Press Ctrl+Alt+F12 to toggle God Mode on or off, with an on-screen
confirmation. Holding the shortcut does not repeatedly toggle it.
The Options page can also enable a live on-screen FPS counter which
reports completed presentations in 250 ms samples so lag spots remain visible,
and select green (the default), white, blue, red, yellow, cyan, magenta, or
orange crosshair art. The selected hue applies to both the original four-piece
OBJ reticle and its Super FX cockpit triangles while damaged-wing indicators
remain red.

`RENDER UPSCALE` rasterizes the Super FX world layer at 1x, 2x, 3x or 4x.
Face visibility and BSP order stay tied to the original grid while projection
retains fractional endpoints for smooth interpolation.

`2D FILTER` replaces `ENHANCED TEXTURES` and offers OFF, EDGE, XBRZ,
SHARP BILINEAR and CRT. EDGE and xBRZ smooth enlarged artwork; Sharp Bilinear
softens pixel boundaries; CRT adds scanlines and a subtle bright phosphor glow.
All modes work at native Render Upscale: the filter reconstructs 2D art in a
separate 2x buffer and resolves it back to the native raster. Higher render
scales retain more filter detail. Geometry and game timing are unchanged.
Texture artwork on 3D polygons is included too; solid-coloured faces remain
under the separate 3D Smoothing option. The polygon coverage mask is preserved.
Previously enabled Enhanced Textures settings migrate to EDGE.

xBRZ is enabled by default. Minimal builds may set `-DSTARFOX_ENABLE_XBRZ=OFF`;
the menu then skips xBRZ. See `THIRD_PARTY_NOTICES.md` for attribution.

Filtering, palette expansion, anti-aliasing and lighting share persistent CPU
workers. Higher render scales require more processing time and memory.
RTX Lighting and Anti-Aliasing remain independent options. For diagnostics,
`STARFOX_2D_FILTER_DEBUG=1` highlights filtered framebuffer pixels in magenta.

`CUSTOMIZE SCREEN` opens a mouse-driven captured native-gameplay HUD preview
using the game's actual HUD artwork. Lives, Shield, Bombs/Boost, Comms, and the
Boss Health bar can each be dragged independently; `RESET` (or Y) restores the
current display mode's defaults. Layouts are independent for 4:3,
16:9, 16:10, 21:9, and 32:9, with separate Original and Star Fox EX layouts
for every size. They save automatically to
`hud-layout.cfg` beside the desktop executable.
Game pace, render FPS, display mode, renderer, graphics choices, MSU-1 music,
rumble, music/SFX volumes, God Mode, the FPS counter, and crosshair colour
also persist in
`pregame.cfg` beside the desktop executable. Keyboard and
controller remaps are saved as `input-bindings.cfg` in the same folder.

Desktop builds are portable by default: keep these files with the executable
when moving or upgrading the game. Use an extracted, writable folder (not a
read-only installation directory). The first normal launch copies any missing
files from the former Documents/preference locations; existing portable files
always win, and originals are not deleted. The working directory does not affect
save locations. Mobile and console packages retain their writable platform
storage because their executable/package directories may be read-only.
Standard display uses the complete 256x224 raster; Widescreen 16:9,
Widescreen 16:10, Ultrawide 21:9, and Super Ultrawide 32:9 expand the intro
and gameplay scene to 400x224, 360x224, 520x224, and 800x224 respectively
while keeping cartridge-authored HUD, dialogue, title, map, and control-screen
artwork centred in their original safe area. All modes use nearest-neighbor scaling
in a resizable window. It is a hybrid source port: a pinned 65C816 core
executes bounded original routines while timing, asset decoding, simulation
orchestration, rendering, audio output, and presentation are native C++.

## Controls

| SNES | Keyboard | Gamepad |
|---|---|---|
| D-pad | Arrow keys | D-pad |
| B | Z | South button |
| Y | A | West button |
| A | X | East button |
| X | S | North button |
| L / R | Q / W | Shoulder buttons |
| Select | Apostrophe (`'`) | Back/View |
| Start | Enter | Start/Menu |

Escape opens an exit-confirmation dialog. Select+Start remains available to
the game and never exits the PC runtime.

Rewind history is disabled at launch, so it costs no background CPU time or
memory during normal play. Press F12 on the first setup page to opt in for the
current run; a two-second title-bar message confirms the change without adding
another permanent menu item. F12 there can disable it again.

F5 toggles the presentation debugger. While frozen, F6 advances exactly one
render frame. With history enabled, F7 walks backward through the retained
final-frame history; with history disabled it has no stored frame to visit.
The debugger uses the native 60 Hz raster as its minimum cadence, so a 20 or
30 FPS output choice never skips cartridge frames. At 90 FPS and above it also
retains every interpolated output frame. Hold F6 or F7 for 450 ms to begin a
slow eight-frame-per-second repeat.

After stepping backward, F6 first walks forward through those exact captured
presentations; at the live edge it advances simulation again. F5 resumes from
the newest live state. When explicitly enabled, the lossless compressed
history holds up to 3,600
presentations within a 128 MiB budget. Audio is paused while frozen and stepped
audio is discarded rather than playing as a backlog afterward.

Xbox/XInput controllers, Steam Input virtual controllers, and the Steam Deck's
built-in controls are detected automatically. Both the D-pad and left stick
move by default; Deck back paddles and all other exposed controls can be
assigned from CONTROLLER REMAP.

Star Fox EX can consume up to five connected gamepads for its native two-player
and multitap modes. Devices with an SDL/Steam player index are assigned first
in player-number order, followed by the remaining detected controllers;
keyboard input belongs to player one only. EX's own `MULTITAP SUPPORT` and
`NUMBER OF PLAYERS` settings remain authoritative, including its one-player
mode that deliberately mirrors player-one input to all five ships.

EX's `SUPER SCOPE MODE` uses the PC mouse as the native light gun: move to aim,
left-click for Fire, right-click for Cursor/calibration, middle-click for Pause,
and use either side button for Turbo. Scope mode owns the mouse only while that
EX option is enabled; otherwise right-drag remains the free presentation camera.
`NTT DATA PAD SUPPORT` maps 0-9 to the matching main-row or keypad digits,
asterisk to Shift+8 or keypad Multiply, hash to Shift+3 or keypad Divide,
period to either Period key, C to C, and Hang Up to H.

In the setup menu, hold Tab to hide the menu; release it to restore it.
With Preview enabled, the scene remains visible behind it. Confirmations are
press-only: release A before activating another action. Start the game from
the main menu, not a graphics submenu.

Outside the setup menu, hold Tab to fast-forward the complete cartridge clock at 2x speed.
Hold Ctrl+Tab for 3x total speed (200% faster), or Shift+Ctrl+Tab for 5x total
speed (400% faster). For transition testing, Ctrl+Shift+` runs at 20x total
speed (2000%). Gameplay, frontend transitions, music, and sound effects all
accelerate together. Releasing the shortcut immediately restores the selected
game pace; render FPS is unchanged.

During gameplay, hold the right mouse button and drag to freely adjust camera
yaw and pitch. While still holding the right mouse button, use the mouse wheel
to zoom in or out. The camera adjustment is presentation-only and does not
change the deterministic game pace.
