# EX pre-game scrolling (in progress)

120 Hz trace `tmp/ex-menu-scroll-before-sep20/choice-9.log` showed the
displayed scroll snapping directly to each current PPU update, held six
presentations per one-pixel move. The EX menu flips its BG1 image page;
that flip was incorrectly resetting the previous BG2 scroll snapshot.

The raster-cut detector now ignores normal BG1 page flips in EX pre-game
as it already does for Super FX gameplay. Actual BG2 map/character changes,
mode changes and scene cuts still reset history.

Windows rebuild passed. New trace `tmp/ex-menu-scroll-pagefix-sep20` shows
previousX=171/currentX=172 retained through alpha 0, 1/6, 2/6, 3/6, 4/6,
5/6, instead of previous=current. This fixes lost interpolation history.

Enhanced photographic sampling now retains fractional X/Y, including wrapped
register movement. Selection changes explicitly reset history even if choices
reuse the same tilemap address. Fractional tests cover rates through 480 Hz;
the EX hitlist test passes. Visual motion acceptance remains open.

Runtime proof: `tmp/ex-menu-fractional60-sep20` and
`tmp/ex-menu-fractional120-sep20`, choice 9 with Enhanced Sky. The new
read-only `check_ex_menu_scroll.py` verifies 33 and 42 consecutive uniform
fractional advances respectively, at the original 20 pixels/second speed.
These inspect the final background sampling position, not just logic alpha.
This evidence is specific to the enhanced choice-9 path; native indexed
layers and all other independently scrolling menu layers remain outstanding.

Native fractional compositing is now implemented in the existing environment
pass on CPU, D3D11 and generated DXIL/SPIR-V/Metal effects. It blends only
background-tagged colors; text/model neighbors are excluded, and the exact
zero-remainder path does no smoothing work. EX menus enable ownership tags
even with all enhancements off. Photographic backdrops still use their direct
fractional sampling after this native-layer preparation.

Windows build and environment tests pass. GPU comparison covers fractional
X/Y at 2x with nonuniform colors and protected text/model tags (max delta 1).
Runtime proof: `tmp/ex-native-scroll7-sep20`, `ex-native-scroll8-sep20`, and
`ex-native-scroll8-cpu-sep20`, native choice 9 at 120 FPS. The two final frames
differ in 15652 pixels; the frame-8 software/GPU pair matches exactly and was
visually inspected. This is not an all-background or physical-VR acceptance
claim; HDMA-specific scenes and runtime 60/240 FPS remain to check.
Follow-up runtime comparisons: native choices 0/14/25/34 at 60 FPS, frame 8
(`tmp/ex-scroll-mixed60-{gpu,cpu}-sep20`), and 240 FPS, frame 20
(`tmp/ex-scroll-moving240-{gpu,cpu}-sep20`). All eight software/GPU pairs
match exactly. The 240 FPS traces show alpha 2/3 and nonzero scroll
remainders (-1/3 for 0/14/34, +1/3 for 25), proving these are moving-phase
samples. Choice 34 was visually inspected: patterned bands remain and text
is crisp. Earlier `ex-scroll-mixed240-*` frame-8 captures were before the
first scrolling update and are not motion evidence. Full sequences, all
38 choices, selection-change runtime checks and physical VR remain open.

Portable follow-up: rebuilt Linux application, terrain tests and GPU effects
checker from the current worktree. Terrain tests pass; the effects checker
passes under explicit Lavapipe Vulkan, including fractional menu color sampling
with all environment modes off and protected foreground tags. Generated
portable shaders pass the freshness check. This establishes portable shader
correctness, not physical headset acceptance or weak-device performance.

Selection runtime proof: `tmp/ex-menu-scroll-switch-held-sep20`, enhanced
choice 9 at 120 FPS, Right held for 12 presentation frames starting at 18.
Choice 10's first sample has previousX=currentX=175; subsequent source ticks
resume fractional movement. `check_ex_menu_scroll.py --switch-to 10` passes.
The preceding three-frame press in `ex-menu-scroll-switch-sep20` did not
reach a source input update and is not selection evidence. The capture tool
now exposes PressFrames so high-refresh input fixtures cannot silently miss
their intended source tick. Other selection pairs remain unverified.
`capture_ex_menu_backgrounds.ps1` now accepts Frames and PresentationFps for
repeatable motion captures (existing defaults unchanged).
# Space panorama motion follow-up

Fresh enhanced menu captures for choices 6/17/29, 48 presentations at 120 FPS:
`tmp/ex-space-menu-motion120-sep20`. The generalized scroll checker verifies
42 uniform fractional steps for each. Choice 29's actual source history
decrements 156 to 155, unlike the increasing asteroid scenes; it is checked
with explicit `--direction -1`, not absolute-valued deltas that could hide
direction reversals. Choices 6/17 use direction +1.

EX 2-4 banked gameplay at 120 FPS is captured every 20 frames in
`tmp/ex-nebula24-motion120-sep20`. Frame 100 inspected: both red/violet clouds
follow the scene roll, star coverage remains present, UI stays upright.
Sampled captures do not establish perceptual continuity between every frame.
