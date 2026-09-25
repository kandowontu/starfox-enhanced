"""Measure regional frame-to-frame change; not a general image-quality score.

Use matched deterministic scenes. Camera scrolling invalidates interpretation
as jitter, so always compare against the corresponding DLSS-off sequence.
"""
import argparse
import json
import struct
from pathlib import Path


def bmp(path):
    data = path.read_bytes()
    if data[:2] != b'BM':
        raise ValueError(f'Not BMP: {path}')
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bits = struct.unpack_from('<H', data, 28)[0]
    compression = struct.unpack_from('<I', data, 30)[0]
    if width <= 0 or not height or bits not in (24, 32) or compression not in (0, 3):
        raise ValueError(f'Unsupported BMP: {path}')
    stride = ((width * bits + 31) // 32) * 4
    step = bits // 8
    rows = []
    for y in range(abs(height)):
        start = offset + (y if height < 0 else abs(height) - y - 1) * stride
        rows.append([tuple(data[start + x * step:start + x * step + 3]) for x in range(width)])
    return rows


def measure(prefix, first, last, region=(.25, .125, .75, 1/3)):
    frames = [bmp(Path(f'{prefix}.frame-{i}.bmp')) for i in range(first, last + 1)]
    changes = []
    for old, new in zip(frames, frames[1:]):
        if len(old) != len(new) or len(old[0]) != len(new[0]):
            raise ValueError('Sequence dimensions changed')
        height, width = len(new), len(new[0])
        left, top, right, bottom = (int(region[0]*width), int(region[1]*height),
                                    int(region[2]*width), int(region[3]*height))
        if left >= right or top >= bottom:
            raise ValueError('Region contains no pixels')
        errors = [abs(new[y][x][c] - old[y][x][c])
                  for y in range(top, bottom)
                  for x in range(left, right) for c in range(3)]
        changes.append(sum(errors) / len(errors))
    return {'prefix': str(prefix), 'frames': len(frames),
            'region': region, 'mean_change_0_255': sum(changes) / len(changes),
            'max_change_0_255': max(changes), 'per_pair': changes}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', nargs='+')
    parser.add_argument('--first', type=int, default=16)
    parser.add_argument('--last', type=int, default=32)
    parser.add_argument('--region', type=float, nargs=4, default=(.25, .125, .75, 1/3),
                        metavar=('LEFT', 'TOP', 'RIGHT', 'BOTTOM'),
                        help='Normalized region; defaults to upper-center background')
    args = parser.parse_args()
    if args.first < 1 or args.last <= args.first:
        parser.error('Need at least two positive frame numbers')
    l, t, r, b = args.region
    if not (0 <= l < r <= 1 and 0 <= t < b <= 1):
        parser.error('Region must be nonempty and within 0..1')
    print(json.dumps([measure(p, args.first, args.last, args.region) for p in args.prefix], indent=2))
