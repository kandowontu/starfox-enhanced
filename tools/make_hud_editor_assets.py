"""Build the HUD editor preview from deterministic native-runtime captures.

The clean capture supplies the gameplay background.  Each HUD asset is the
pixel-exact difference between the full and clean captures inside its native
screen rectangle, with untouched pixels changed to the BMP colour key.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image


COLOUR_KEY = np.array([1, 2, 3], dtype=np.uint8)
CROPS = {
    "lives": (12, 13, 32, 16),
    "shield": (20, 179, 48, 25),
    "bombs_boost": (732, 178, 48, 26),
    "comms": (332, 164, 136, 48),
}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("full", type=Path)
    parser.add_argument("clean", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    full = np.asarray(Image.open(args.full).convert("RGB"))
    clean = np.asarray(Image.open(args.clean).convert("RGB"))
    if full.shape != (224, 800, 3) or clean.shape != full.shape:
        raise SystemExit(
            f"expected matching 800x224 captures, got {full.shape} and {clean.shape}"
        )

    args.output.mkdir(parents=True, exist_ok=True)
    Image.fromarray(clean, "RGB").save(args.output / "background.bmp")

    for name, (x, y, width, height) in CROPS.items():
        source = full[y : y + height, x : x + width]
        base = clean[y : y + height, x : x + width]
        changed = np.any(source != base, axis=2)
        result = np.broadcast_to(COLOUR_KEY, source.shape).copy()
        result[changed] = source[changed]
        Image.fromarray(result, "RGB").save(args.output / f"{name}.bmp")


if __name__ == "__main__":
    main()
