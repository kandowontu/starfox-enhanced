"""Read EX stage/planet names from cartridge STAGEPATHS (no gameplay mutation)."""
import argparse
import json
import re
from pathlib import Path


def read_routes(rom_path, symbols_path, dialogue_path):
    rom = rom_path.read_bytes()
    symbols = {m[1]: int(m[2], 16) for line in symbols_path.read_text().splitlines()
               if (m := re.fullmatch(r"(\S+)\s+\$([\da-fA-F]+)", line))}
    def byte(address):
        if address & 0xffff < 0x8000:
            raise ValueError(f"Unmapped ROM address {address:06x}")
        return rom[((address >> 16) & 127) * 32768 + (address & 32767)]
    def word(address):
        return byte(address) | byte(address + 1) << 8
    levels = {v: k for k, v in symbols.items() if re.fullmatch(r"LEVEL[1-7]_[1-9]", k)}
    names = {row["id"]: row["en"].strip() for row in json.loads(dialogue_path.read_text(encoding="utf-8"))["messages"]}
    base = symbols["STAGEPATHS"]
    result = []
    for route in range(12):
        cursor = base + word(base + route * 2)
        seen = set()
        while cursor not in seen:
            seen.add(cursor)
            kind = byte(cursor)
            if kind in (0, 2):
                break  # Choice records require runtime ROUTES; do not guess.
            if kind == 1:
                cursor = base + word(cursor + 1)
                continue
            if kind != 3:
                raise ValueError(f"Invalid path record {kind} at {cursor:06x}")
            planet = byte(cursor + 3)
            level_address = byte(cursor + 6) << 16 | 0x8000 | word(cursor + 4) & 0x7fff
            message = byte(symbols["PLANETNAMES"] + planet)
            result.append({"route": route, "level": levels.get(level_address, f"{level_address:06x}"),
                           "planet": planet, "message": message,
                           "name": names.get(f"ex.radio.1.{message:03d}", "")})
            cursor += 9
            for _ in range(128):
                if word(cursor) == 0xffff:
                    cursor += 2
                    break
                cursor += 4
            else:
                raise ValueError("Unterminated route geometry")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=Path("tmp/runtime-inputs/starfox-ex/SFES.SFC"))
    parser.add_argument("--symbols", type=Path, default=Path("assets/symbols/starfox-ex.txt"))
    parser.add_argument("--dialogue", type=Path, default=Path("assets/localization/ex-dialogue.json"))
    args = parser.parse_args()
    print(json.dumps(read_routes(args.rom, args.symbols, args.dialogue), indent=2))
