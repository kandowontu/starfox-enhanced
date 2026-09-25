# Fortuna photographic clouds

Original BG_3_3A now selects ocean-clouds-v1 (resource 213), a photographic
maritime cumulus panorama. Its water remains the original/enhanced-ground
renderer rather than being baked into the sky. Source bank-6 moon inks 97..109
remain single and unchanged; sky fill 110 is replaced, avoiding a rectangular
box around the moon. This preserves the existing moon, not a new realistic
moon replacement. The source moon's lower atmospheric fade remains authored.

Built-in image-generation prompt and provenance are in
`assets/enhanced-backdrops/ocean-clouds-v1.md`; PNG and 24-bit BMP are saved
beside it. Packaging includes Windows resources and portable embedded assets.
The image is loaded lazily, using the existing cached backdrop path.

Final captures: `tmp/fortuna-final-gpu-sep20`, Original LEVEL3_3 at source tick
1000, 16:9 and 32:9, GodMode, Enhanced Sky on. The 32:9 image was inspected:
clouds span the view, one moon, no rectangular sky fill. Matching 16:9 software
capture in `tmp/fortuna-final-cpu-sep20` is pixel-identical. All pixels at/below
row 112 match `tmp/fortuna-source-sep20` exactly, preserving water and UI.
Earlier `tmp/fortuna-enhanced-*` images have a rejected moon sky-fill box.

Current Windows build, asset decode and focused environment tests pass.
This is entry-scene acceptance, not all camera angles, full playthrough,
moon upgrades, reflected moon detail or physical VR acceptance.

## EX menu background 32

The menu now uses the same panorama with its own original scroll offsets and
ground palette. Source atlas inspection places the first flat ground row at
360 (palette index 79); rows 352 and 356 are still clouds. The previous generic
origin+128 calculation would place the horizon at 376. The explicit mapping
uses 360 and retains the existing single-moon protection.

`tmp/ex-ocean-source-sep20` is the fresh native 32:9 baseline at the independent
reference scroll phase. `tmp/ex-ocean-final-gpu-sep20` is the enhanced capture;
it was visually inspected. `tmp/ex-ocean-final-cpu-ground-sep20` also enables
Enhanced Ground deliberately: it matches the GPU result pixel-for-pixel,
confirming that setting remains suppressed in this menu sample. Every pixel
at/below the row-160 screen horizon matches the native baseline exactly.
Current Windows build and focused mapping tests pass. Other menu choices are
not covered by this result.
