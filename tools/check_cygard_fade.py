"""Check captured Cygard palette progression against its cartridge target.

Read-only: requires EX ROM and symbols matching the captured build. This checks
the authored 64-color fade, not photographic artwork or original IRQ timing.
"""
import argparse
import re
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", type=Path)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--symbols", type=Path, required=True)
    args = parser.parse_args()
    match = re.search(r"^FXFADECORN2\s+\$([0-9a-fA-F]+)",
                      args.symbols.read_text(), re.MULTILINE)
    if not match:
        parser.error("FXFADECORN2 symbol missing")
    address = int(match[1], 16)
    offset = ((address >> 16) & 0x7f) * 0x8000 + (address & 0x7fff)
    rom = args.rom.read_bytes()
    if len(rom) % 0x8000 == 512:
        offset += 512
    # PALFADETOCORN2_L starts X/Y at byte 32 and processes 64 words.
    target = struct.unpack_from("<64H", rom, offset + 32)
    paths = sorted(args.captures.rglob("*.cgram"))
    if len(paths) < 3:
        parser.error("Need a chronological sequence with at least 3 captures")
    frames = [struct.unpack_from("<64H", path.read_bytes(), 32) for path in paths]
    def distances(frame):
        return tuple(abs(((v >> shift) & 31) - ((t >> shift) & 31))
                     for v, t in zip(frame, target) for shift in (0, 5, 10))
    errors = [distances(frame) for frame in frames]
    assert any(errors[0]), "Sequence starts after fade; not transition evidence"
    assert not any(errors[-1]), "Final palette has not reached cartridge target"
    assert len(set(frames)) > 2, "No intermediate palette captured"
    for index, (before, after) in enumerate(zip(errors, errors[1:])):
        assert all(b <= a for a, b in zip(before, after)), (
            f"Palette channel moves away from target at {paths[index+1]}")
    print(f"PASS: {len(frames)} samples, {len(set(frames))} palette phases; "
          "all 192 channels converge monotonically to FXFADECORN2")


if __name__ == "__main__":
    main()
