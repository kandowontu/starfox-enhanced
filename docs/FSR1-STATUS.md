# FSR1 integration — September 19

Implemented in the current Windows executable and native Linux desktop build.
The active SDL D3D12/Vulkan adapter exposes its numeric PCI vendor ID through
our pinned SDL patch. AMD (0x1002) selects FSR1 in the existing DLSS menu slot;
NVIDIA retains DLSS. No marketing-name parsing or installed-card enumeration.
Unknown adapters do not silently claim AMD support. Software retains the last
known GPU menu identity but cannot run FSR1; stereo currently reports unavailable.

Modes: Off, Ultra Quality, Quality, Balanced, Performance. Independent saved
preferences survive adapter changes and cartridge state restoration. FSR1 uses
the upstream MIT EASU and RCAS implementation, GPU-resident with no normal-path
readback. No temporal jitter, frame generation or AI reconstruction is implied.
Ray tracing and GPU reflections remain independently capability-gated, not
enabled by FSR1. Windows DXR has no NVIDIA-only vendor gate.

Verification:

- Current Windows and Linux applications build. DXIL, SPIR-V and Metal shader
  generation/freshness succeeds (Metal runtime not exercised).
- D3D12 and Vulkan FSR constant-colour, odd extent, resource reuse/resize checks
  pass. Vendor ID is 0x10de on this physical host; Linux Lavapipe reports 0x10005
  and also passes. This does not replace testing on physical AMD hardware.
- Original/EX simulation and archive tests pass 4/4 (130.02 seconds), including
  independent menu selection, press-only activation and host-state retention.
- All four quality levels captured in actual gameplay. A capture comparison
  exposed HUD sprite downsampling: FSR now renders the late cartridge layer at
  full resolution before restoring HUD ink. `tools/check_fsr1_hud.ps1` passes
  2,528 exact shield/bomb/boost pixels for each quality level against Off.
  Evidence: `tmp/fsr1-hud-final-sep19/{0,1,2,3,4}/presentation.bmp`.
- Combined ray/reflection and EX/Stained Glass captures are in
  `tmp/fsr1-ray-final-sep19` and `tmp/fsr1-ex-final-sep19`.

Remaining acceptance: physical AMD driver/performance/interop testing, broader
scene transitions and unsupported-backend checks. No AMD FPS uplift claim,
headset deployment or release. Upstream source/license provenance is retained
in `third_party/fsr1` and `THIRD_PARTY_NOTICES.md`.
