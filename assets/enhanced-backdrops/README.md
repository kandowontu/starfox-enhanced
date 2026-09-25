# Optional realistic backdrops — work in progress

September 22 additions: `face-moon-v1` (resource 233) upgrades the individual
Dimension face-planets, and `deep-space-v1` (resource 234) supplies Game Over's
photographic starfield. Their sibling Markdown files record built-in image
generation and complete prompts. The original Andross/text/star ink remains
separate from the new starfield; see `docs/GAME-OVER-COVERAGE-SEPT20.md` and
`docs/EX-FACE-PLANETS-SEPT22.md` for actual runtime evidence and limits.

Current alpine asset: `alpine-day-v2.png` / `.bmp`, embedded as resource 200.
It removes v1's edge sun/glare and uneven horizontal exposure. The built-in
edit tool and complete prompt are documented in `alpine-day-v2.md`. The v1
notes below are historical. Generated art is not runtime proof.

`macbeth-dusk-v1.png` and its lossless BMP (resource 201) add stage-specific
rust mountains/red sky for BG_3_5, without modifying ground. Built-in generation
provenance and the full final prompt are in `macbeth-dusk-v1.md`. Actual gameplay
proof is `tmp/backdrop-proof/macbeth-gpu/presentation.bmp`; the paired software
capture is byte-identical and pixels below row 256 remain identical to Sky Off
in this 800x448 fixture. Windows/Linux applications build. Other stage/planet
backdrops and VR integration remain unfinished.

`alpine-day-v1.png` is an AI-generated mountain/sky asset created with the built-in image-generation tool on September 20, 2026. It contains no gameplay ground. The lossless BMP copy is embedded as resource 200 in desktop runtime builds and decoded only when needed. Enhanced Sky uses it on registered Corneria/Original Training backgrounds. The generated asset itself is not photographic evidence of an in-game fix; runtime captures are under `tmp/backdrop-proof`.

The sampler blends a 1/32-width overlap across the horizontal repeat seam and clamps vertically. GPU/software scroll, roll, brightness and layer-protection tests pass, and DXR tests verify replacement-sky reflection and fading. `alpine-aligned/presentation.bmp` is an inspected tilted gameplay capture. Other landscape/planet assets, full VR integration, per-style/per-motion variants and sustained performance acceptance are unfinished. The panorama's left-edge sunlight still warrants visual review across a full wrap.

Generation prompt:

Use case: photorealistic-natural. Asset type: horizontally seamless panoramic backdrop texture for an optional realistic enhancement in a space-flight game. Generate a 3:1 wide landscape image, crisp high-resolution photographic quality. A distant alpine mountain chain with irregular snow-capped peaks and finely detailed exposed rock, atmospheric cyan haze softening the lower foothills. Mountain skyline confined to the lower quarter of the image: tallest peaks at roughly 75 percent image height, mountain bases fade into pale cyan atmospheric haze at the bottom edge. Upper three quarters blue daytime sky, darker blue toward zenith smoothly transitioning to pale cyan near horizon, scattered realistic wispy and small cumulus clouds in a loose band above mountains. Natural directional sunlight from upper left. Mountains must feel distant, not towering nearby. Left and right edges must form a visually continuous panorama with matching sky brightness and horizon height. No foreground, no ground plane, no grass, no water, no buildings, no people, no ships, no text, no logo, no borders. This is only the sky/backdrop layer; gameplay ground will be rendered separately. Avoid stylization, pixel art, obvious repeated identical mountains, dramatic oversaturated colors, painterly rendering.
# EX menu additions (September 20)

New full-resolution built-in generated assets: `orbital-clouds-v1.png`,
`orbital-volcanic-v1.png`, and `city-night-v1.png`. Lossless BMP versions are
embedded as resources 204/205/206 on desktop. Their sibling `.md` files contain
the complete final prompts and provenance. The orbital images are mapped with
their actual limb height (~63.3%), not stretched as sky-only landscapes.

Built-in image generation produced `storm-night-v1.png` and
`desert-horizon-v1.png`; lossless BMP copies are embedded as resources 202/203.
Full prompts/provenance: [storm](storm-night-v1.md),
[desert](desert-horizon-v1.md). Runtime menu mappings and uncompleted coverage
are documented in `docs/EX-BACKGROUND-COVERAGE.md`.
# Fortuna open-sky revision

Resource 213 now uses `ocean-clouds-v2.bmp` rather than v1. The matching PNG is
the unchanged generated master; `ocean-clouds-v2.md` records the full built-in
generation prompt and provenance. Low cloud banks leave naturally open sky
around the separately rendered, atmosphere-faded moon. The rejected circular
cloud-erasure approach is not used. The previous asset is retained for reference.

## Comet corona correction

Resource 222 now uses `comet-corona-v1.bmp`, open space above a sun-bright
comet rather than the incorrectly assigned cavern. The PNG master and exact
built-in generation prompt are retained as `comet-corona-v1.png` and `.md`.
Runtime projection ends at the corona; native animated surface/flames remain.
