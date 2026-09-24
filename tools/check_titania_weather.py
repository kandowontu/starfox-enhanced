"""Verify the natural weather replay reached both authored palette endpoints.

This checks simulation evidence, not visual acceptance of an enhanced image.
"""
import argparse
import struct
from pathlib import Path


def rgb(word):
    return tuple((word >> shift) & 31 for shift in (0, 5, 10))


def check(root):
    paths = sorted(root.rglob('*.cgram'))
    samples = [(path, struct.unpack('<256H', path.read_bytes()[:512])) for path in paths]
    before = [p for p, c in samples if rgb(c[14]) == (21, 25, 30) and rgb(c[25]) == (21, 25, 31)]
    after = [p for p, c in samples if rgb(c[1]) == (29, 25, 15) and rgb(c[14]) == (4, 0, 0)
             and rgb(c[25]) == (11, 8, 6)]
    if not before or not after:
        raise ValueError(f'{root}: weather replay did not reach both blue-fog and gold/brown endpoints')
    print(f'{root}: {len(samples)} palette snapshots; fog {before[0].name}, gold/brown {after[0].name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path, nargs='+')
    for capture in parser.parse_args().captures:
        check(capture)
