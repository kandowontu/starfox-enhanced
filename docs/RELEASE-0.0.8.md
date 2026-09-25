# Star Fox Enhanced 0.0.8

This release collects the work since 0.0.6.7 across the game, renderers,
platform builds, controls and packaging. It remains an alpha: supply your own
supported retail ROM and game assets. No cartridge ROM or user save is included.

## Rendering and visual options

- Expanded GPU rendering and composition across D3D12, Vulkan and Metal,
  including background layers, models, sprites, HUD composition, widescreen
  filling, frame interpolation and presentation scaling. Improved parity
  between Software and GPU paths and reduced avoidable image transfers.
- Added platform ray-tracing paths: DXR on supported Windows GPUs, Vulkan ray
  queries on supported Linux/Steam Deck hardware, and Metal hardware ray
  tracing on supported Apple devices. Ray tracing adds model shadows and
  reflections; bright laser effects, the Andross core and other emissive
  elements remain unshadowed. The stylized lighting option is named Enhanced
  Lighting. Software rendering has its own lower-cost shadow and reflection
  paths. Availability and performance depend on the device and renderer.
- Added GPU reflections for models and ground, with Low/Medium/High quality.
  Water, mirror and metallic ground can receive reflections; material controls
  are available only with the required renderer capabilities.
- Added FSR 1 for detected AMD GPUs, with Off, Ultra Quality, Quality, Balanced
  and Performance modes. NVIDIA systems retain the DLSS option. The Windows
  package includes the pinned DLSS runtime components. DLSS 5 remains an
  experimental integration for a separately installed compatible add-on; the
  third-party add-on itself is not bundled. With DLSS off, the game keeps its
  normal renderer path instead of upgrading the graphics backend just because
  the optional runtime is present.
- Added independent Enhanced Sky and Enhanced Ground options, with automatic
  or manual ground selection. Ground choices include grass, dirt, sand, snow,
  water, mirror, gold metal, red sand and bubbling lava. Water and lava have
  animated surface treatments; reflective choices use the selected renderer's
  reflection path. Enhanced landscape layers follow the scene's movement and
  palette changes.
- Expanded scene-specific enhanced backdrop artwork and coverage for gameplay,
  menus, transitions and game-over presentation, including widescreen and
  wraparound views. Added palette-responsive backdrop handling for changing
  scenes and more distinct artwork choices for EX previews.
- Reorganized visual styles into clearer groups and removed Crosshatch from
  selection. Comic no longer overlays the black dot pattern. Added Posterized,
  Film, Negative, Solarized, Amber, Emerald, Cyanotype, Copper Tone, Lavender,
  CGA, Scanlines, Teal/Orange, Handheld, Watercolour, Chalk, Emboss, Bleach
  Bypass, Stained Glass, Risograph, Hologram, Mosaic, Pencil, Oil Paint,
  Duotone, Tritone, Woodcut, X-Ray, Pop Art, Iridescent, CRT Phosphor, Noir,
  UV Glow and Topographic. Old Crosshatch saves migrate to Off; Ice migrates to
  the visually distinct Cyanotype style.
- Added a separate Manipulations group with Kaleidoscope, Prism Split, Pixel
  Sort, Trails, Long Exposure, Shatter, Melt, Ripple Warp, Barrel Warp,
  Venetian, Checker Fold, Twist, Ring Ripple and Shard Split.
- Added a separate 3D Material group with Metallic, Gold Metal, Copper Metal,
  Mirror, Prism, Glass, Obsidian, Pearl, Ruby, Jade, Porcelain, Silver, Brass,
  Rose Gold, Titanium, Amethyst, Sapphire, Opal, Marble, Graphite and Molten
  Glass. Reflective materials follow the ray tracing and Reflective Surfaces
  settings. Effects and materials exclude comms portraits and HUD elements.

## Gameplay, menus and controls

- Improved interpolation for high-refresh presentation, including explosions,
  transitions, background scrolling and other previously low-rate animations.
- Added the Planet Select Cheat with X+Y entry, A or Start launch, and fresh
  activation when changing maps. Improved the map fade and pre-game level
  selection behavior, including cumulative sound-bank loading for skipped
  stages.
- Added a visual SNES controller to the pre-game remapping screen, live
  indication of the button being assigned, and scroll indicators on long
  pre-game menus.
- Added a five-second mapped L+R hold to reset menu settings. Improved Steam
  Deck and Retroid face-button handling, including the A/B and X/Y menu swap,
  while preserving in-game bindings.
- Added a fullscreen startup option for the desktop executable. Improved
  controller and touch input handling, touch layout editing, and consistent
  widescreen menu presentation.
- Improved the control mapping and on-screen button layout editors, including
  movable and resizable touch controls, clearer controller prompts, and a
  corrected mobile display viewport.
- Updated translations and menu organization, including the expanded visual
  effect and material selectors.

## Platform builds and setup

- Added experimental Windows PCVR and Quest 3 OpenXR builds. They provide VR
  startup menus, controller input and VR scene rendering; scene-specific
  headset testing and polish remain ongoing.
- Improved iOS display fitting, runtime file selection, Metal rendering and
  touch controls. iOS render upscale is limited to 1x and 2x after higher
  settings caused crashes; 2x has been confirmed stable in user testing.
- Improved Android GPU-startup recovery and runtime asset packaging. Android
  and Quest include the backdrop assets in bounded native resource units. The
  Windows downloads include a separate asset-builder package and setup guide.
- Updated release automation and package checks for Windows x64/x86, Xbox UWP,
  Linux, macOS, iOS, Android, Quest 3, PCVR, Nintendo Switch and PS Vita.
  Packages include their required notices and exclude ROMs and signing secrets.

## Known limits

- PCVR and Quest packages are experimental development builds. Passing a build
  or package check does not establish full headset acceptance or performance
  on every scene.
- Ray tracing, reflections, FSR and DLSS depend on hardware, drivers and the
  active renderer. Some visual choices are unavailable on unsupported paths.
- DLSS 5's third-party bridge is not included, and its visual quality remains
  experimental. It should not be treated as a general realism filter.
- Performance varies by device and settings. Start with 1x rendering and
  optional effects disabled when diagnosing slowdowns.
- Enhanced backdrop and ground coverage, scene parity, high-refresh pacing and
  sustained performance still need broader route-by-route and device testing.
- The game requires a compatible retail ROM supplied by the user. Follow the
  platform setup instructions for preparing and selecting its runtime assets.
