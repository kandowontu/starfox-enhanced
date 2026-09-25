# EX menu rocky desert (choice 5)

Choice 5 now selects the existing desert-horizon-v1 photographic rocky-range
panorama (resource 203). Choice 8 remains the separate smooth-dune artwork.
No new image asset or additional rendering pass was introduced.

The source Mode-2 atlas has flat ground at row 432; this is now an explicit
horizon mapping. The original reference scroll phase places that boundary at
screen row 159. The source palette-response path remains active; the photograph
is an artistic replacement, not a pixel-exact reproduction of its diagonal
cloud streaks or yellow atmospheric gradient.

Fresh 32:9 captures are `tmp/ex-rocky-desert-source-sep20` (native),
`tmp/ex-rocky-desert-gpu-sep20` and `tmp/ex-rocky-desert-cpu-sep20`.
The enhanced GPU image was inspected. CPU/GPU differ at two pixels. Every
pixel at/below row 159 is identical to native output. The CPU capture also
enabled Enhanced Ground, verifying suppression for this menu sample.
Current Windows build, artwork decoding and focused mapping tests pass.

This does not establish gameplay mapping, all palette phases, all camera
angles or physical VR acceptance. Other unmapped menu scenes remain open.
