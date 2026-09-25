"""Check fractional gameplay celestial registration from a 120 FPS capture log."""
import argparse
import math
import re
from pathlib import Path


def check(path):
    pattern = re.compile(r"celestial-scroll: alpha=([\d.e+-]+) affine=\(([^)]+)\)")
    samples = [(float(a), tuple(map(float, t.split(','))))
               for a, t in pattern.findall(path.read_text())]
    if len(samples) < 24:
        raise ValueError('Need a moving capture, not one source-frame snapshot')
    centers = []
    moving_groups = 0
    group = []
    for alpha, transform in samples:
        a, b, u, v = transform
        determinant = 1 - a * b
        if not all(map(math.isfinite, transform)) or abs(determinant) < .1:
            raise ValueError('Invalid celestial transform')
        x = (384 - u - a * (152 - v)) / determinant
        centers.append((x, 152 - v - b * x))
        if group and alpha <= group[-1][0]:
            if len(group) >= 3:
                first, last = group[0], group[-1]
                changed = max(abs(c - p) for c, p in zip(last[1], first[1]))
                if changed > .01:
                    moving_groups += 1
                    for phase, actual in group[1:-1]:
                        fraction = (phase - first[0]) / (last[0] - first[0])
                        expected = [p + (c - p) * fraction for p, c in zip(first[1], last[1])]
                        if max(abs(c - p) for c, p in zip(actual, expected)) > .003:
                            raise ValueError('Planet movement is quantized between source frames')
            group = []
        group.append((alpha, transform))
    if moving_groups < 3:
        raise ValueError('Capture never exercised enough moving source frames')
    largest_step = max(math.dist(a, b) for a, b in zip(centers, centers[1:]))
    if largest_step > 20:
        raise ValueError(f'Planet jumped {largest_step:.3f} pixels in one presentation')
    print(f'{len(samples)} presentations; {moving_groups} fractional moving intervals; '
          f'max planet step {largest_step:.3f}px')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    check(parser.parse_args().log)
