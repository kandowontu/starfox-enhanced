# Local visual proof — September 20

Actual runtime captures, not mockups. These are selected verified cases, **not a claim that the entire goal is complete**. Links point to this workspace; assets have not been uploaded or released.

Latest consolidated Windows checkpoint: full rebuild, **58/58 tests passed (199.59 s)**. Linux and ordinary Android builds pass. VR work/deployment remains on hold.

## Ray-traced pillar shadows

Hardware DXR capture: pillars and ships cast ground shadows. This does not prove every caster or AMD support.

![Ray-traced pillar shadows](<C:/Users/kando/Documents/ChatGPT/Starfox Enhanced/tmp/current-ray-original-pillars/8b747e0f11924581ac58c9d3d41e2e66/dxr.bmp>)

## Straight scramble opening

EX, 240 Hz, 2×, 16:9. The horizontal opening matches the software capture. Motion is covered separately by phase/sequence tests; a still alone cannot prove smoothness.

![Straight scramble opening](<C:/Users/kando/Documents/ChatGPT/Starfox Enhanced/tmp/parity-current-sep20/ex-wipe-gpu/presentation.bmp>)

## Upgrade wireframe

EX, 240 Hz, 2×. The overlay follows the ship in this frame; additional ownership/interpolation tests are documented in PRESENTATION-PARITY.md.

![Upgrade wireframe](<C:/Users/kando/Documents/ChatGPT/Starfox Enhanced/tmp/parity-current-sep20/ex-upgrade-gpu/presentation.bmp>)

## Comms portrait and conditional meter

Spanish EX at 32:9. This message requests a teammate meter; the portrait is not squeezed. Six-language pairs were checked, not every conversation.

![Comms portrait and conditional meter](<C:/Users/kando/Documents/ChatGPT/Starfox Enhanced/tmp/comms-languages-sep20/EX-4-GPU/presentation.bmp>)

## Cheats submenu

All six cheat rows and Back fit. Beam is the renamed fully upgraded laser choice, although this screenshot has Single selected.

![Cheats submenu](<C:/Users/kando/Documents/ChatGPT/Starfox Enhanced/tmp/runtime-cheats-backends-sep20/EX-direct3d12/presentation.bmp>)

## F1 menu locks Experience

Actual F1 open/resume/reopen test capture. Experience remains locked during the running game.

![F1 menu locks Experience](<C:/Users/kando/Documents/ChatGPT/Starfox Enhanced/tmp/runtime-f1-isolated-sep20/EX/presentation.bmp>)

## Game Over stars, ScaleFX and Full SBS

EX, 4×, ScaleFX, 240 Hz. Stars extend into each eye's widescreen margins. Half SBS and fallback dimensions are checked separately; physical stereo comfort remains unverified.

![Game Over stars, ScaleFX and Full SBS](<C:/Users/kando/Documents/ChatGPT/Starfox Enhanced/tmp/game-over-scalefx-sbs-sep20/EX-full.bmp>)

## Still open

- Physical native Steam Deck Gaming Mode (#44), Android system-bar behavior, and headset/Index acceptance.
- Full source-camera/composition comparison for Colony; its isolated background matches the SNES probe, while a transient authored door causes the observed obstruction.
- Forced Intel D3D12 cold-dispatch crash; automatic Intel Vulkan selection passes locally.
- Broader platform/hardware acceptance; physical AMD testing is unavailable here.

Details and additional captures: GOAL-ACCEPTANCE.md, BACKGROUND-AUDIT.md, GPU-MIGRATION-STATUS.md, PRESENTATION-PARITY.md, SAVE-STATES-STATUS.md, ISSUES-43-49-VERIFICATION.md. No release was made.

