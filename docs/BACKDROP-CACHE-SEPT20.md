# Immutable backdrop upload cache

Runtime backdrops are sealed after decoding and any zenith preparation. Their
unique upload keys replace full-image comparisons in SDL GPU effects and DXR.
The cache no longer retains a duplicate CPU pixel vector for sealed assets.
Unsealed editable images keep exact content comparison, preserving existing
in-place-edit tests. Any post-seal edit must clear the key and reseal; the
zenith-preparation method automatically invalidates it. The runtime exposes
these loaded assets through const pointers and does not edit them afterward.

`tools/benchmark_backdrop_cache.cpp`, compiled with GCC C++20 -O2, compared
2,000 cache checks of a 2172x724 RGBA image (6,290,112 bytes): mutable checks
577.194 ms, sealed checks 0.0007 ms. Retained cache pixels drop from 6,290,112
bytes to zero per backend. This is a cache-only microbenchmark, not measured
in-game FPS; no broad game-performance speedup is claimed.

Current Windows executable builds. Focused backdrop tests pass, including
mutable edits, sealed identity replacement, cache-memory release and seal
invalidation. GPU-effects checks pass with added seal/reseal cases alongside
existing resize/toggle/device-reset cases. Hardware DXR checks on the RTX 5070
Ti Laptop GPU pass, including sealed image reuse/replacement, reflected output,
mutable edits and fresh-device upload. Physical non-Windows testing remains
separate. No new rendering pass, shader metadata, file asset change or default
enhancement was introduced.
