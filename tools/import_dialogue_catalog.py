"""Import upstream message scripts without silently labelling placeholders translated.

This is a source catalog, not ROM data: speaker/control metadata and message
ordering remain available for the runtime importer and translation review.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

LANGUAGES = {
    "en": "ENGLISH", "ja": "JAPANESE", "de": "GERMAN",
    "fr": "FRENCH", "es": "SPANISH",
}
MESSAGE = re.compile(
    r"^\s*message\s+([^,]+),([^,]+),<(.*)>,([^;]+?)\s*;\s*\((\d+)\)\s*$"
)


def read_script(path: Path) -> list[dict]:
    records = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        if not re.match(r"^\s*message\s", line, re.IGNORECASE):
            continue
        match = MESSAGE.fullmatch(line)
        if match is None:
            raise ValueError(f"{path}:{line_number}: unrecognized message syntax")
        colour, speaker, text, kind, number = match.groups()
        records.append({"id": int(number), "colour": colour.strip(),
                        "speaker": speaker.strip(), "text": text,
                        "kind": kind.strip(), "line": line_number})
    ids = [record["id"] for record in records]
    if not ids or ids != list(range(1, len(ids) + 1)):
        raise ValueError(f"{path}: missing, duplicate, or reordered message IDs")
    return records


def import_catalog(source: Path, spanish: Path | None = None) -> dict:
    scripts = {code: read_script(source / f"{name}.INC")
               for code, name in LANGUAGES.items()}
    reference = scripts["en"]
    for language, records in scripts.items():
        if len(records) != len(reference):
            raise ValueError(f"{language}: message count differs from English")
    catalog = {"schema": 1, "source": "UltraStarFox SF/MSG", "languages": {}, "messages": []}
    for code, name in LANGUAGES.items():
        unchanged = sum(a["text"] == b["text"] for a, b in zip(reference, scripts[code]))
        catalog["languages"][code] = {
            "file": f"{name}.INC",
            "controller_palette": "pal" if code in ("de", "fr", "es") else
                "japanese" if code == "ja" else "ntsc",
            "sha256": hashlib.sha256((source / f"{name}.INC").read_bytes()).hexdigest(),
            "identical_to_english": unchanged,
            "status": "source" if code == "en" else
                "placeholder" if unchanged == len(reference) else "upstream_translation",
        }
    for index, english in enumerate(reference):
        catalog["messages"].append({
            "id": f"original.radio.{english['id']:03d}",
            "variants": {code: records[index] for code, records in scripts.items()},
        })
    if spanish is not None:
        translations = spanish.read_text(encoding="utf-8").splitlines()
        if len(translations) != len(reference) or any(not text.strip() for text in translations):
            raise ValueError("Spanish translation must contain one nonempty line per original message")
        for entry, translation in zip(catalog["messages"], translations):
            entry["variants"]["es"]["text"] = translation
            entry["variants"]["es"]["provenance"] = "Star Fox Enhanced translation draft"
        catalog["languages"]["es"]["status"] = "authored_translation_draft"
        catalog["languages"]["es"]["translation_file"] = spanish.name
        catalog["languages"]["es"]["translation_sha256"] = hashlib.sha256(spanish.read_bytes()).hexdigest()
        catalog["languages"]["es"]["identical_to_english"] = sum(
            entry["variants"]["es"]["text"] == entry["variants"]["en"]["text"]
            for entry in catalog["messages"])
    return catalog


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--spanish", type=Path, help="Authored Spanish replacement, one message per line")
    args = parser.parse_args()
    catalog = import_catalog(args.source, args.spanish)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Imported {len(catalog['messages'])} message IDs")
    for code, info in catalog["languages"].items():
        print(f"{code}: {info['status']} ({info['identical_to_english']} identical to English)")


if __name__ == "__main__":
    main()
