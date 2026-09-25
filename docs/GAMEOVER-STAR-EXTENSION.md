# Game-over starfield extension

The previous wide-screen treatment filled the margins with black and redrew
only sparse moving dust. It did not extend the dense stars baked into BG_AND.

The game-over BG2 pass now extends star-only atlas patches from rows 0–31 and
160–223 across the margins. Stable spatial hashing avoids obvious repeated
panels and per-frame noise. Andross and the complete native canvas are left
unchanged. Both CPU and GPU use identical sampling, including CPU replay after
a GPU failure. Controls and Continue retain their own backdrop treatment.

Verified Original and EX at 32:9, 1x: normal GPU, CPU late-star pass, forced GPU
fallback, stereo fallback, and Half/Full SBS. Matching mono paths are identical.
The Original final capture contains 165 and 137 star pixels in its left and
right margins, versus the old sparse dust-only treatment. Visual inspection
confirms a full-width starfield and no repeated Andross. Direct3D12 and Vulkan
background tests pass 432 cases with over 183 million pixel/coverage comparisons.

Proof: `tmp/game-over-expanded-stars/`. Hardware-headset VR deployment remains
on hold; this validation concerns desktop widescreen and SBS.
