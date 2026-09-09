"""Report Unicode coverage needed beyond the source cartridge font mappings."""
import csv
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def audit():
    available = set(chr(code) for code in range(32, 127))
    for name in ("chrmapjp.dat", "chrmapde.dat"):
        mapping = (ROOT / "upstream-ultrastarfox/SF/MSG" / name).read_text(encoding="utf-8-sig")
        for token in re.findall(r'^0x[0-9a-fA-F]+\s*=\s*"([^"]+)"', mapping, re.MULTILINE):
            if len(token) == 1:
                available.add(token)
    needed = set()
    catalog = json.loads((ROOT / "assets/localization/upstream-dialogue.json").read_text(encoding="utf-8"))
    for message in catalog["messages"]:
        for variant in message["variants"].values():
            needed.update(variant["text"])
    with (ROOT / "assets/localization/menu.tsv").open(encoding="utf-8", newline="") as stream:
        for row in list(csv.reader(stream, delimiter="\t"))[1:]:
            for value in row:
                needed.update(value)
    return {"required_character_count": len(needed),
            "source_mapping_character_count": len(available),
            "missing": sorted(needed - available)}


if __name__ == "__main__":
    print(json.dumps(audit(), ensure_ascii=True, indent=2))
