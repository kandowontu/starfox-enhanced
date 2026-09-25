# EX menu 10: rocky-green replacement

Inspected the native reference-phase choice-10 capture in
`tmp/ex-menu-layout-next-sep20`. Its low moss-green crags and small cumulus
clouds are neither alpine snow peaks nor smooth desert dunes. Added dedicated
photographic artwork, resource 217/index 17, without changing native rendering.
The source horizon stays at row 360 and palette origin at 248. Enhanced Ground
remains suppressed in this menu, including when both environment toggles are on.

Windows and portable resource manifests, fallback path and decode test include
the asset. It uses the existing lazy load/sealed upload cache, with no new
rendering pass. No gameplay mapping is guessed from menu similarity.

Windows application build, terrain/palette tests and rocky-green asset test pass.
Actual 32:9 reference-phase Enhanced Sky + Enhanced Ground captures:

- `tmp/ex-rocky-green-gpu-sep20/choice-10-final.bmp`
- `tmp/ex-rocky-green-cpu-sep20/choice-10-final.bmp`

GPU image visually inspected: low ridge at the original ground boundary,
continuous sky and readable native lettering. CPU/GPU differ at one pixel.
All pixels at y >= 160 are identical to the native reference, proving preserved
ground and lower menu text in this capture. Other menu phases, gameplay and VR
are not claimed verified by this scene.

Generated image and complete prompt: `assets/enhanced-backdrops/rocky-green-v1.md`.
