"""Measure visible shutter edges in real final-presentation BMP sequences.

Read-only image analysis; no source masks or renderer-reported bounds are used
as the visual oracle. Intended for the bright Corneria source-wipe fixture.
"""
import argparse
import math
from pathlib import Path


def visible_edges(bounds, label):
    if not bounds:
        raise AssertionError(f"{label}: no sampled columns")
    if all(bound is None for bound in bounds):
        return None
    if any(bound is None for bound in bounds):
        raise AssertionError(f"{label}: vertical slit or unilluminated test column")
    tops = [bound[1] for bound in bounds]
    bottoms = [bound[3] for bound in bounds]
    if max(tops) != min(tops) or max(bottoms) != min(bottoms):
        raise AssertionError(f"{label}: opening edges are not horizontal")
    return tops[0], bottoms[0]


def opening_step(previous, current, limit, label):
    up, down = previous[0] - current[0], current[1] - previous[1]
    if min(up, down) < 0:
        raise AssertionError(f"{label}: opening moved backwards")
    if max(up, down) > limit:
        raise AssertionError(f"{label}: {max(up, down)}px jump exceeds {limit}px")
    return max(up, down)


def check(prefix, frames, fps, scale):
    from PIL import Image
    closed = False
    started = False
    previous = None
    edges = []
    largest_step = 0
    dimensions = None
    # Original source cadence may vary between 20 and 30 Hz. Both must be
    # interpolated rather than retaining a full eight-source-line jump.
    step_limit = math.ceil(8 * scale * 30 / fps) + 1
    for number in range(1, frames + 1):
        path = Path(f"{prefix}.frame-{number}.bmp")
        with Image.open(path) as source:
            image = source.convert("RGB")
        if dimensions is None:
            dimensions = image.size
        if image.size != dimensions:
            raise AssertionError(f"{path}: changing output dimensions")
        width, height = dimensions
        # Sample the whole width, avoiding only the two native edge-mask pixels.
        bounds = [image.crop((x, 0, x + 1, height)).getbbox()
                  for x in range(4 * scale, width - 4 * scale, max(1, width // 96))]
        if all(bound is None for bound in bounds):
            if started:
                raise AssertionError(f"{path}: opening closed again")
            closed = True
            continue
        if not closed:
            continue  # Source program has not yet closed its initial frame.
        top, bottom = visible_edges(bounds, path)
        if previous:
            largest_step = max(largest_step, opening_step(previous, (top, bottom), step_limit, path))
        previous = top, bottom
        edges.append(previous)
        started = True
    if not started or len(set(edges)) < max(8, len(edges) // 4):
        raise AssertionError("Too few distinct visible edge positions; stalled/coarse opening")
    print(f"{prefix}: {len(edges)} opening frames, {len(set(edges))} distinct edge pairs, "
          f"max step {largest_step}px; full-width straight monotonic opening")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", help="Final BMP path, before .frame-N.bmp")
    parser.add_argument("--frames", type=int, required=True)
    parser.add_argument("--fps", type=int, choices=(60, 120, 144, 240), required=True)
    parser.add_argument("--scale", type=int, choices=(1, 2, 4), required=True)
    args = parser.parse_args()
    check(args.prefix, args.frames, args.fps, args.scale)
