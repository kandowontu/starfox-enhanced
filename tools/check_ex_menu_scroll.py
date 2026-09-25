"""Verify actual enhanced-menu presentation scroll, not just source updates."""
import argparse
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("log", type=Path)
parser.add_argument("--fps", type=int)
parser.add_argument("--choice", type=int, default=9)
parser.add_argument("--direction", type=int, choices=(-1, 1), default=1)
parser.add_argument("--switch-to", type=int)
args = parser.parse_args()
text = args.log.read_text()
if args.switch_to is not None:
    samples = re.findall(
        rf"ex-menu-placement: choice={args.switch_to} mode=\d+ alpha=([\d.e+-]+) previousX=(-?\d+) currentX=(-?\d+)", text)
    assert samples, "Requested menu selection was never reached"
    assert samples[0][1] == samples[0][2], "Selection retained another background's scroll history"
    assert any(a != b for _, a, b in samples[1:]), "Scrolling never resumed after selection"
    print(f"PASS: choice {args.switch_to} resets history and resumes scrolling")
    raise SystemExit(0)
if not args.fps:
    parser.error("Specify --fps for motion or --switch-to for selection checks")
rows = [tuple(map(float, match)) for match in re.findall(
    rf"ex-menu-fractional: choice={args.choice} alpha=([\d.e+-]+) x=([\d.e+-]+)",
    text)]
if len(rows) < 12:
    parser.error("Need at least 12 enhanced menu presentations")
# Ignore the initial stationary history. The tested menus' authored scroll is one
# pixel per 20 Hz update; every later presentation should advance 20/fps.
deltas = [b[1] - a[1] for a, b in zip(rows, rows[1:])]
first = next((i for i, delta in enumerate(deltas) if abs(delta) > 0.001), None)
assert first is not None, "Background never scrolled"
for delta in deltas[first:]:
    expected = args.direction * 20 / args.fps
    assert abs(delta - expected) < 0.002, (
        f"Nonuniform scroll step {delta}; expected {expected}")
print(f"PASS: choice {args.choice}: {len(deltas)-first} uniform fractional steps at {args.fps} FPS")
