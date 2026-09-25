# EX night-coast backdrop (33)

Choice 33 now uses a dedicated photographic night-coast panorama with distant
island ridges and dusk haze. The source atlas horizon remains row 360 and
the independent menu scroll phase is unchanged. Source stars use indices
65 and 73; these are protected while the surrounding sky fill 72 is replaced.
No stars, moons or water were baked into the replacement image.

The built-in image-generation prompt and provenance are saved alongside the
PNG/BMP in `assets/enhanced-backdrops/night-coast-v1.md`. Resource 214 is
included in Windows and portable packaging. The backdrop uses the lazy,
sealed upload cache and adds no rendering pass.

Fresh 32:9 reference-phase captures: `tmp/ex-night-coast-source-sep20`,
`tmp/ex-night-coast-gpu-sep20`, `tmp/ex-night-coast-cpu-sep20`.
The final GPU image was inspected. CPU/GPU differ at one pixel; every pixel
at/below screen row 160 matches native water/UI exactly. The CPU run also
enabled Enhanced Ground, confirming menu suppression for this sample.
Current Windows build, asset decoding and focused mapping tests pass.

This does not prove a gameplay-stage assignment, all palette phases, all
camera angles or physical VR acceptance. Other missing background art remains
separately tracked.
