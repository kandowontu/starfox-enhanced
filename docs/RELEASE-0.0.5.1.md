# Star Fox Enhanced 0.0.5.1

Alpha maintenance release. Changes since 0.0.5; the previous release remains available.

## Readable graphics submenus

- Restore full-height main-menu text, 50% taller than the compact 0.0.5 font.
  All main-page choices fit without scrolling, including Preview.
- Add **2D Options**: 2D Filter, 2D Bloom, World Effects,
  **World Effect Intensity**, and Back.
- Add **3D Options**: Anti-Aliasing, VSync, Render Upscale, 3D Bloom,
  3D Smoothing, RTX Lighting, Model Effects, **Model Effect Intensity**, and Back.
- Keep renderer/display settings, music, rumble, Options, Start Game and Preview
  on the main page. Controller remapping and audio volumes remain in Options.
- Each submenu uses full-height text with generous spacing. B or Back returns
  to its main-page entry, and navigation wraps in both directions.
- Live Preview remains active while changing either graphics submenu.
  Existing saved graphics settings retain their values.

## Switch audio jitter protection — issue #27

The [latest reporter comment](https://github.com/kandowontu/starfox-enhanced/issues/27#issuecomment-5556160405)
confirms the earlier multi-second audio delay was
fixed, but describes remaining static-like distortion. This release addresses
a reproducible underrun risk in that low-latency configuration:

- Replace Switch's 8 ms startup headroom with 64 ms, covering delayed delivery
  of the game's 50 ms audio packets.
- Use a bounded 150 ms source queue, matching the desktop policy, so normal
  headroom does not trigger an immediate queue reset. The backlog limiter stays
  active; SPC/MSU state always advances even when obsolete playback is discarded.
- A deterministic SDL regression reproduces underruns with the old 8/100 ms
  policy under alternating 66/34 ms consumption and verifies uninterrupted,
  unchanged PCM with the new 64/150 ms policy. Existing backlog/resampling and
  malformed-packet tests remain in place.

This is a tested protection against scheduling-jitter dropouts, **not hardware
confirmation that every reported Switch distortion is resolved**. Reporter
verification on Switch is still needed; issue #27 is not automatically closed.

## Installation and remaining limitations

Replace the application files with the matching 0.0.5.1 package while retaining
your saves, settings and user-supplied assets. No retail ROM or user data is
included. Android version code is 8; Xbox package identity is 0.0.5.1.

See [the full 0.0.5 changelog](RELEASE-0.0.5.md) for the preceding parity,
graphics, portable-storage and performance changes. Its known limitations,
including unconfirmed #35 and the unresolved boss-rendering reports, still apply.
