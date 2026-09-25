# EX 20 / EX 1-4: independent cloud and atmospheric limb

Implemented the two-object replacement investigated in
EX-CRESCENT-SOURCE-SEPT23.md. The blue cloud retains its photographic master;
the green arc retains the source's narrow illumination and black interior,
with subpixel reconstruction and crater-surface relief. It is deliberately
not replaced by a second fully illuminated planet.

## Rendering and cost

- A sparse, immutable 2048-square atlas stores the cloud and shade-encoded
  limb. Its 1,360-byte source signature detects relevant tile/character edits.
- Palette changes use all 15 live bank-5 entries in uniforms, independently
  of the cloud's bank-0 response. They do not rebuild or upload the atlas.
- Each object's center follows the cartridge's affine scrolling; neither
  surface is sheared. Old sheared footprints are cleared only locally.
- EX pre-game scrolling applies fractional offsets to the new transform.
  One 180-frame 120 Hz run has 174 nonzero steps, each -0.166/-0.167 source
  pixels, after its initial static interval.
- CPU, D3D12/Vulkan effects, and DXR background reflections share the mapping
  and palette behavior. Original-game cloud art is not switched to this path.

## Additional gameplay bug found

The EX 1-4 gameplay route (`BG_1_14`) did not apply the menu's existing
unique-object rule. Its original blue cloud repeated in the far-right
ultrawide margin. The route now uses that rule with enhancements on **or** off.
The exact before/after changed bounds in the captured 32:9 final image are
751,0–798,37. All native central 256 columns are unchanged.

## Evidence

- Two 32:9 menu phases have byte-identical CPU/GPU final captures:
  `tmp/ex-crescent-upgraded-{gpu,cpu}-sep23` and
  `tmp/ex-crescent-visible-{gpu,cpu}-sep23`.
- Steered EX 1-4 at 120 Hz, 180 frames, 16:9 and 32:9: eight captured bitmap
  pairs match exactly, including intermediate and final frames:
  `tmp/ex-crescent-route-fixed-{gpu,cpu}-sep23`.
- Visually inspected the visible menu crescent and final ultrawide gameplay
  capture; the repeated far-right cloud is absent.
- Geometry, independent live palette endpoints, cache reuse during palette
  edits, tile-change invalidation, no repeated objects and source decoding
  tests pass. D3D12 and Windows/Linux Vulkan effects checks pass with layout 9
  added to the existing scroll/roll/brightness/style/protected-layer matrix.
- Hardware DXR explicitly checks crescent palette changes on the same texture
  with **zero** extra upload bytes; the remaining reflection/shadow checks pass.
- Windows and native Linux applications build. Ordinary Android debug build
  and full 35-backdrop payload/exclusion validation pass. Current APK SHA-256:
  `eadc23f5da586eab03d024403c71a2daf78d2100be6ae1eb62d13543fd701c5f`.

Logs: `tmp/cloud-limb-{unit,effects-d3d,effects-vulkan,dxr-check,linux-check}-sep23.log`,
`tmp/cloud-limb-route-{build,linux,android}-sep23.log`,
`tmp/cloud-limb-android-payload-sep23.log`.

No release, device installation, or VR art-parity claim is made. Natural
palette-transition coverage across every route and physical-device acceptance
remain part of the larger goal; these captures do not substitute for them.
