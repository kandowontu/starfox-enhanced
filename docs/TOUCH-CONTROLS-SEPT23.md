# Window-space mobile controls — September 23

The old touch overlay was rendered into the cartridge framebuffer while SDL
finger coordinates were normalized to the physical window. Upscaling and
letterboxing therefore separated the visible buttons from their hit regions;
the 4:3 content also pulled controls toward the screen centre.

`TouchOverlayLayout` now places larger buttons against the window's safe-area
edges. It is shared by normalized-finger hit testing and the SDL host overlay,
which renders after the game/effects at the actual window resolution. The
software and GPU game paths no longer paint the old framebuffer controls.

Validation: `starfox_runtime_input_tests` checks every button centre,
screen-edge placement, centre non-interception and resized-window mapping.
The Original 1-1 forced-overlay native/final captures are byte-identical on
Software and D3D12 GPU at 1×, 2× and 4× (`tmp/touch-overlay-presentation-sep23`
and `tmp/touch-overlay-presentation-upscale-sep23`); the final 1× image was
inspected for readable labels. The Android arm64 Debug APK compiles
and passes package integrity. This source change has **not** been built or
installed on the connected iPhone; its ongoing 2× test uses the older IPA.

The older IPA was terminated by iOS jetsam (`per-process-limit`) immediately
after the intro began at 4×; this is a memory-pressure kill, not a thrown game
exception. The reported 2× run did not exit before switching to 4×. A fresh
iOS build and physical retest are needed to evaluate the current GPU memory
changes.

The Apple bundle also lacked `CADisableMinimumFrameDurationOnPhone`, which
keeps ProMotion iPhones from offering their higher refresh rates to SDL's
display link. The template now opts in. This has not yet been verified on the
device; 240 FPS remains above its 120 Hz physical display maximum.
