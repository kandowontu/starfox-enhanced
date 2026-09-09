"""Extract EX radio text from a user-supplied ROM for localization review.

The two tables contain bank-local pointers to speaker, sound, colour, and
zero-terminated ASCII text. Output contains scripts only, never ROM payloads.
"""
import argparse
import json
from pathlib import Path
import re


def extract(rom: bytes, symbols: str) -> dict:
    addresses = {name: int(value, 16) for name, value in
                 re.findall(r"^(MESSAGES2?)\s+\$([0-9a-fA-F]+)$", symbols, re.MULTILINE)}

    def offset(address):
        if address & 0xffff < 0x8000:
            raise ValueError("Not a LoROM address")
        return (address >> 16) * 0x8000 + (address & 0x7fff)

    records = []
    for name in ("MESSAGES", "MESSAGES2"):
        table = addresses[name]
        bank = table & 0xff0000
        start = offset(table)
        for index in range(512):
            pointer = int.from_bytes(rom[start + index * 2:start + index * 2 + 2], "little")
            # Message data precedes its pointer table in both EX banks.
            if pointer < 0x8000 or pointer >= table & 0xffff:
                break
            position = offset(bank | pointer)
            end = rom.find(b"\0", position + 3, position + 259)
            if end < 0 or any(c < 32 or c > 126 for c in rom[position + 3:end]):
                raise ValueError(f"Invalid text in {name} entry {index + 1}")
            records.append({"id": f"ex.radio.{1 if name == 'MESSAGES' else 2}.{index + 1:03d}",
                            "address": bank | (pointer + 2),
                            "speaker": rom[position], "sound": rom[position + 1],
                            "colour": rom[position + 2],
                            "en": rom[position + 3:end].decode("ascii")})
        else:
            raise ValueError(f"Unterminated table: {name}")
    return {"schema": 1, "source": "Star Fox EX user-supplied ROM radio tables", "messages": records}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("symbols", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    catalog = extract(args.rom.read_bytes(), args.symbols.read_text())
    args.output.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Extracted {len(catalog['messages'])} EX radio messages")
