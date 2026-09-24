"""Compare independently captured EX BG2 frames at matched source scroll.

Reports differences, not a blanket pass: palette cycling and cartridge stars
can differ with elapsed menu time. BG1/sprites are hidden only in the reference
diagnostic; the host input is its separate BG2 capture, never a beautified image.
The original menu's visible window is x=16..239, y=16..205. Both captures must
use the first visible source scanline (1); no best-fit alignment is applied.
Widescreen host captures use their centered native window, with no rescaling.
"""
import argparse
import csv
import re
from pathlib import Path

import numpy as np
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("reference", type=Path)
parser.add_argument("host", type=Path)
parser.add_argument("--records", type=Path, default=Path("tests/data/ex_menu_reference_offsets.csv"))
parser.add_argument("--choices", type=int, nargs="*")
args = parser.parse_args()
records = list(csv.DictReader(args.records.open(encoding="utf-8")))
for record in records:
    choice = int(record["choice"])
    if args.choices is not None and choice not in args.choices:
        continue
    log = (args.host / f"choice-{choice}.log").read_text(encoding="utf-8", errors="replace")
    # New traces also record interpolation alpha/history between mode and
    # the authoritative PPU fields. Never confuse those with displayed scroll.
    placement = re.search(r"^ex-menu-placement: choice=(\d+) mode=(\d+)\b[^\r\n]*?\bppu=\((\d+),(\d+)\)", log, re.MULTILINE)
    expected = tuple(int(record[key]) for key in ("choice", "mode", "ppu_x_at_capture", "ppu_y"))
    if not placement or tuple(map(int, placement.groups())) != expected:
        raise RuntimeError(f"Choice {choice}: capture not at reference scroll/mode")
    reference = np.array(Image.open(args.reference / f"choice-{choice}.png").convert("RGB"), dtype=np.int16)
    host = np.array(Image.open(args.host / f"choice-{choice}-layer-bg2-expanded.bmp").convert("RGB"), dtype=np.int16)
    if (reference.shape != (224, 256, 3) or host.shape[0] != 224
            or host.shape[1] < 256 or (host.shape[1] - 256) % 2):
        raise RuntimeError(f"Choice {choice}: expected 224-high native or centered widescreen captures")
    # The renderer expands symmetrically. This is the known original viewport,
    # not a data-dependent search that could conceal a placement regression.
    border = (host.shape[1] - 256) // 2
    host = host[:, border:border + 256]
    delta = np.abs(reference[16:206, 16:240] - host[16:206, 16:240])
    changed = int(np.count_nonzero(delta.max(axis=2) > 2))
    # Deliberate wrong-origin control: this must remain separate from the
    # measured result, not become an automatically selected best-fit shift.
    wrong_origin = np.abs(reference[16:206, 16:240] - host[24:214, 16:240])
    control = int(np.count_nonzero(wrong_origin.max(axis=2) > 2))
    print(f"choice={choice:02} pixels_over_2={changed}/{190*224} "
          f"max_delta={int(delta.max())} mean_delta={float(delta.mean()):.4f} "
          f"wrong_y_plus_8_pixels={control}")
