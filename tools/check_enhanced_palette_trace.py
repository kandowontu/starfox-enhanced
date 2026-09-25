"""Verify an actual runtime backdrop-response trace, not a standalone palette fit.

This checks response delivery/cadence. It does not replace visual inspection or
independent cartridge timing comparison.
"""
import argparse
import math
import re
from pathlib import Path


def check(path, background, minimum_phases, maximum_step_gap=None, minimum_surface_phases=1):
    if minimum_phases < 1 or minimum_surface_phases < 1:
        raise ValueError('Phase thresholds must be positive')
    if maximum_step_gap is not None and maximum_step_gap < 1:
        raise ValueError('Step gap must be positive')
    pattern = r"enhanced-backdrop-palette: frame=(\d+) bg=(\d+) sky=([^\s]+) surface=([^\s]+)"
    rows = [(int(f), tuple(map(float, sky.split(','))), tuple(map(float, surface.split(','))))
            for f, bg, sky, surface in re.findall(pattern, path.read_text())
            if int(bg) == background]
    if len(rows) < 3:
        raise ValueError('Need at least three presentations of the requested background')
    for row in rows:
        if any(len(band) != 4 or not all(map(math.isfinite, band)) for band in row[1:]):
            raise ValueError('Invalid photographic palette response')
    if any(b[0] != a[0] + 1 for a, b in zip(rows, rows[1:])):
        raise ValueError('Background trace has missing presentations or switches scenes')
    phases = len({row[1] for row in rows})
    if phases < minimum_phases:
        raise ValueError(f'Only {phases} sky phases; expected at least {minimum_phases}')
    surface_phases = len({row[2] for row in rows})
    if surface_phases < minimum_surface_phases:
        raise ValueError(f'Only {surface_phases} surface phases; expected at least {minimum_surface_phases}')
    changes = [b[0] for a, b in zip(rows, rows[1:]) if a[1] != b[1]]
    surface_changes = [b[0] for a, b in zip(rows, rows[1:]) if a[2] != b[2]]
    if maximum_step_gap is not None:
        if not changes or any(b-a > maximum_step_gap for a, b in zip(changes, changes[1:])):
            raise ValueError('Sky changes do not form one source-cadence transition')
        if changes[0]-rows[0][0] < maximum_step_gap or rows[-1][0]-changes[-1] < maximum_step_gap:
            raise ValueError('Capture lacks a stable interval before/after the transition')
        if minimum_surface_phases > 1 and (surface_changes[0] < changes[0]
                                          or surface_changes[-1] > changes[-1]):
            raise ValueError('Surface changes escape the requested single sky-fade interval')
    print(f'PASS: BG {background}, {len(rows)} consecutive presentations; {phases} sky phases, '
          f'{surface_phases} surface phases; '
          f'sky changes at {changes[0] if changes else "none"}..{changes[-1] if changes else "none"}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--background', type=lambda x: int(x, 0), required=True)
    parser.add_argument('--minimum-phases', type=int, default=2)
    parser.add_argument('--minimum-surface-phases', type=int, default=1)
    parser.add_argument('--single-run-max-gap', type=int)
    args = parser.parse_args()
    check(args.log, args.background, args.minimum_phases, args.single_run_max_gap,
          args.minimum_surface_phases)
