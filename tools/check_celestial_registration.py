"""Verify enhanced round bodies retain their source affine screen centres."""
import argparse
import math
import re
from pathlib import Path


def check(path):
    rows = re.findall(r"celestial-registration: body=\(([^)]+)\) source=\(([^)]+)\) stable=\(([^)]+)\)", path.read_text())
    if len(rows) < 24:
        raise ValueError("Need at least 24 presented registration samples")
    largest = 0.0
    banks = 0
    for body, source, stable in rows:
        cx, cy = map(float, body.split(','))
        a, b, tx, ty = map(float, source.split(','))
        ux, uy = map(float, stable.split(','))
        sx, sy = cx - ux, cy - uy
        # Forward-map the rendered centre back into the original atlas.
        error = math.hypot(sx + a * sy + tx - cx, sy + b * sx + ty - cy)
        if not math.isfinite(error) or error > .01:
            raise ValueError(f"Rendered planet detached from source by {error:.4f}px")
        largest = max(largest, error)
        banks += abs(a) + abs(b) > .02
    if banks < 12:
        raise ValueError("Capture did not exercise banked celestial transforms")
    print(f"PASS: {len(rows)} presentations, {banks} banked; maximum atlas registration error {largest:.5f}px")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    check(parser.parse_args().log)
