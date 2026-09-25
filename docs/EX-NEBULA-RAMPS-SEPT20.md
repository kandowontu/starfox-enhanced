# Enhanced nebula two-ramp color mapping

Supersedes the single affine tint for EX Sector K gameplay. Its photographic
warm/cool clouds now independently sample live bank-5/6 shade ramps (inks
8..14), with interpolation into black. Neutral photographic stars stay neutral.
This prevents an additive green offset lifting the whole space background.
The native palette animation still owns timing; no extra clock is introduced.

Uses existing 16-word backdrop ramp storage: mode 2 holds two seven-shade
ramps. CPU, portable/D3D11 effects and DXR reflection shader all implement the
same mapping. No additional texture, upload, rendering pass or allocation.
Portable DXIL/SPIR-V/Metal effects regenerated; Windows shaders/app build pass.

Focused terrain tests pass for black preservation, neutral stars, independent
warm/cool source banks and live black colors. GPU-effects checker passes with
mode 2 included across its styles, scrolls, roll, brightness and layer cases.

Actual EX LEVEL5_3 Enhanced Sky captures at ticks 1000 and 1600, 16:9:
`tmp/ex-nebula-two-ramps-gpu-sep20`. Both inspected: black space retained,
green/purple cloud families visible rather than a full-frame green wash.
Tick-1000 software counterpart in `tmp/ex-nebula-two-ramps-cpu-sep20` differs
by at most one channel level. DXR shader compiled but a reflected live-scene
capture is not claimed here. No all-phase/VR visual sign-off or release.

## Hardware reflection follow-up

Added independent DXR fixtures for mode-2 warm/cool selection, a changed live
ramp on the same image without texture re-upload, black space and neutral
stars. All exact expected-color checks pass on the RTX 5070 Ti Laptop GPU.
The remainder of the hardware DXR checker also passes (resident resources,
reflections, physical water, stereo, changing geometry and partial groups).
This proves the new reflection sampling branch, not an in-game reflected
Sector K screenshot or headset performance.

Linux follow-up: rebuilt current application, IRQ tests, terrain tests and GPU
effects checker in `/home/kando/starfox-enhanced-0052-check`. Both focused
tests pass. The effects checker passes with the Lavapipe ICD explicitly
selected, including the new two-ramp parity cases. This verifies portable
SPIR-V behavior, not physical Steam Deck/GPU speed. Shader freshness and
changed-file whitespace checks also pass.
