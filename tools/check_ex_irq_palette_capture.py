"""Report exact authored EX cycle-table matches in captured CGRAM banks."""
import argparse
from pathlib import Path
import re
import struct
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("captures", type=Path, nargs="+")
parser.add_argument("--require-title-cycle", action="store_true")
args = parser.parse_args()
source = subprocess.check_output(
    ["git", "-C", "upstream-star-fox-ex", "show", "HEAD:SFES/RAMSTUFF.ASM"],
    text=True)
tables = {}
for name, body in re.findall(
        r"(?im)^(water[1-8]|sectork[1-8]|sun[1-8]|titl0?[1-9])\s+dw\s+([^\n]+\n\s*dw[^\n]+)", source):
    words = tuple(int(word, 16) for word in re.findall(r"\$([0-9a-fA-F]+)", body))
    if len(words) != 16:
        raise RuntimeError(f"Unexpected table size: {name}")
    tables[name.lower()] = words
if len(tables) != 42:
    raise RuntimeError(f"Expected all 42 source tables, found {len(tables)}")
for root in args.captures:
    title_phases = set()
    title_pairs = set()
    for path in sorted(root.rglob("*.cgram")):
        palette = struct.unpack("<256H", path.read_bytes())
        matches = []
        for name, words in tables.items():
            start = (0 if name.startswith("titl0") else 16 if name.startswith("titl")
                     else 48 if name.startswith("sun") else 64)
            if palette[start:start + 16] == words:
                matches.append(name)
        for phase in range(1, 10):
            if f"titl{phase}" in matches and f"titl0{phase}" in matches:
                title_phases.add(phase)
                title_pairs.add(palette[:32])
        print(f"{path}: {', '.join(matches) or 'no exact cycle table'}")
    if args.require_title_cycle and len(title_pairs) < 2:
        raise RuntimeError(f"{root}: fewer than two complete authored title palette pairs: {title_phases}")
