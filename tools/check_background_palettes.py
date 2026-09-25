"""Inspect PPU captures for live palette changes against upload sources.

Reports evidence, not automatic visual acceptance. Only equal-background,
equal-source comparisons establish an intra-background change.
"""
import argparse
import json
import re
import struct
from collections import defaultdict
from pathlib import Path


def colours(path, count):
    data = path.read_bytes()
    if len(data) < count * 2:
        raise ValueError(f"Truncated palette: {path}")
    return tuple(value & 0x7fff for value in struct.unpack(f"<{count}H", data[:count * 2]))


def inspect(root):
    groups = defaultdict(list)
    for path in sorted(root.rglob("*.ppu.json")):
        metadata = json.loads(path.read_text())
        stem = path.with_name(path.name.removesuffix(".ppu.json"))
        source_path = Path(str(stem) + ".palette-source")
        if not source_path.exists():
            continue
        reference = colours(source_path, 112)
        current = colours(Path(str(stem) + ".cgram"), 112)
        route_match = re.search(r"(?:ORIGINAL|EX)-LEVEL[^-]+", str(path.relative_to(root)))
        route = route_match.group(0) if route_match else str(path.parent.relative_to(root))
        groups[(route, metadata["background"], metadata["palette_source"])].append(
            (str(path.relative_to(root)), reference, current))
    report = []
    for (route, background, address), frames in sorted(groups.items()):
        references = {row[1] for row in frames}
        changes = [sum(a != b for a, b in zip(frames[0][2], row[2])) for row in frames]
        report.append({
            "route": route, "background": background, "source_address": address,
            "source_is_wram": (address >> 16) in (0x7e, 0x7f),
            "frames": len(frames), "source_content_stable": len(references) == 1,
            "source_nonblack_entries": sum(value != 0 for value in frames[0][1]),
            "live_palette_variants": len({row[2] for row in frames}),
            "maximum_entries_changed_from_first": max(changes),
            "samples": [{"file": row[0], "entries_different_from_source":
                         sum(a != b for a, b in zip(row[1], row[2]))} for row in frames],
        })
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", type=Path)
    args = parser.parse_args()
    result = inspect(args.captures)
    if not result:
        parser.error("No captures contain palette upload-source snapshots")
    print(json.dumps(result, indent=2))
