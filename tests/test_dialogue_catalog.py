"""Source-catalog checks, independent of proprietary ROM inputs."""
import importlib.util
import csv
import re
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("catalog_import", ROOT / "tools/import_dialogue_catalog.py")
catalog_import = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog_import)
font_spec = importlib.util.spec_from_file_location("font_import", ROOT / "tools/generate_localization_font.py")
font_import = importlib.util.module_from_spec(font_spec)
font_spec.loader.exec_module(font_import)
dialogue_spec = importlib.util.spec_from_file_location("dialogue_header", ROOT / "tools/generate_dialogue_header.py")
dialogue_header = importlib.util.module_from_spec(dialogue_spec)
dialogue_spec.loader.exec_module(dialogue_header)


class DialogueCatalogTests(unittest.TestCase):
    def test_source_punctuation_display(self):
        self.assertEqual(dialogue_header.source_display_text("slippy slippy##"),
                         "slippy slippy..")
        self.assertEqual(dialogue_header.source_display_text("hello!"), "hello!")

    def test_font_covers_translated_catalogs(self):
        glyphs = font_import.parse_bdf((ROOT / "assets/fonts/misaki_gothic.bdf").read_text())
        catalog = json.loads((ROOT / "assets/localization/upstream-dialogue.json").read_text(encoding="utf-8"))
        required = set()
        for message in catalog["messages"]:
            for variant in message["variants"].values():
                required.update(variant["text"])
        required.update((ROOT / "assets/localization/menu.tsv").read_text(encoding="utf-8"))
        required.update((ROOT / "assets/localization/ex-translations.tsv").read_text(encoding="utf-8"))
        required -= set("\t\n")
        missing = sorted(ord(char) for char in required if ord(char) not in glyphs)
        self.assertEqual(missing, [], "Missing Unicode glyph codepoints")

    def test_ex_translation_ids_and_languages(self):
        source = json.loads((ROOT / "assets/localization/ex-dialogue.json").read_text())
        source_ids = {message["id"] for message in source["messages"]}
        with (ROOT / "assets/localization/ex-translations.tsv").open(encoding="utf-8", newline="") as stream:
            rows = list(csv.reader(stream, delimiter="\t"))
        self.assertEqual(rows[0], ["id", "ja", "de", "fr", "es"])
        seen = set()
        for row in rows[1:]:
            self.assertEqual(len(row), 5)
            self.assertTrue(all(value.strip() for value in row))
            self.assertIn(row[0], source_ids)
            self.assertNotIn(row[0], seen)
            seen.add(row[0])
        generated = (ROOT / "include/starfox/localization/ex_dialogue_catalog.hpp").read_text()
        addresses = [int(value, 16) for value in re.findall(r'\{0x([0-9a-f]+),', generated)]
        self.assertEqual(sorted(addresses), sorted(message["address"] for message in source["messages"]))

    def test_menu_translations_have_all_languages(self):
        with (ROOT / "assets/localization/menu.tsv").open(encoding="utf-8", newline="") as stream:
            rows = list(csv.reader(stream, delimiter="\t"))
        self.assertEqual(rows[0], ["en", "ja", "de", "fr", "es"])
        keys = set()
        for row in rows[1:]:
            self.assertEqual(len(row), 5)
            self.assertTrue(all(value.strip() for value in row))
            self.assertNotIn(row[0], keys)
            keys.add(row[0])
        self.assertTrue({"LANGUAGE", "2D OPTIONS", "3D OPTIONS", "START GAME",
                         "ACTION", "UP", "DOWN", "LEFT", "RIGHT", "RESET",
                         "HDR EFFECT", "ENHANCED SHADOWS", "CHROMATIC ABERRATION"} <= keys)

    def test_checked_in_catalog_is_reproducible(self):
        source = ROOT / "upstream-ultrastarfox/SF/MSG"
        if not source.exists():
            self.skipTest("upstream source submodule unavailable")
        expected = catalog_import.import_catalog(source, ROOT / "assets/localization/spanish-original.txt")
        actual = json.loads((ROOT / "assets/localization/upstream-dialogue.json").read_text(encoding="utf-8"))
        self.assertEqual(actual, expected)
        self.assertEqual(len(actual["messages"]), 142)
        self.assertEqual(actual["languages"]["es"]["status"], "authored_translation_draft")
        self.assertEqual(actual["languages"]["es"]["identical_to_english"], 0)

    def test_reject_duplicate_ids(self):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "bad.inc"
            script.write_text("message white,fox,<one>,other ; (1)\n"
                              "message white,fox,<two>,other ; (1)\n")
            with self.assertRaises(ValueError):
                catalog_import.read_script(script)

    def test_ex_catalog_has_unique_ids_and_valid_addresses(self):
        data = json.loads((ROOT / "assets/localization/ex-dialogue.json").read_text())
        records = data["messages"]
        self.assertEqual(len(records), 406)
        self.assertEqual(len({record["id"] for record in records}), len(records))
        self.assertEqual(sum(record["id"].startswith("ex.radio.1.") for record in records), 252)
        for record in records:
            self.assertGreaterEqual(record["address"] & 0xffff, 0x8000)
            self.assertTrue(record["en"])
            self.assertGreaterEqual(record["colour"], 1)
            self.assertLessEqual(record["colour"], 15)


if __name__ == "__main__":
    unittest.main()
