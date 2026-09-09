# Localization sources

Language IDs persisted by the runtime are 0 English, 1 Japanese, 2 German,
3 French, and 4 Spanish. English keeps the cartridge text path.

`upstream-dialogue.json` imports 142 radio messages from UltraStarFox. Its
Japanese, German, and French text is upstream-authored. The upstream Spanish
file is an English placeholder; `spanish-original.txt` supplies a new Spanish
draft instead. Provenance and source hashes remain in the generated catalog.

`ex-dialogue.json` contains the two radio tables extracted from the user's EX
ROM, totaling 406 messages. `ex-translations.tsv` supplies new translations;
unchanged original text is reused only with an exact text-and-speaker match.
New translations are drafts, not professionally reviewed localizations.

`menu.tsv` contains host-menu translations. Language names remain recognizable
in the language selector. Catalog coverage is not proof of complete UI coverage:
dynamic prompts and cartridge-rendered text require separate runtime checks.

Regenerate from the repository root:

```powershell
python tools/import_dialogue_catalog.py upstream-ultrastarfox/SF/MSG assets/localization/upstream-dialogue.json --spanish assets/localization/spanish-original.txt
python tools/generate_dialogue_header.py assets/localization/upstream-dialogue.json include/starfox/localization/dialogue_catalog.hpp
python tools/generate_ex_dialogue_header.py
python tools/generate_menu_header.py
python tools/generate_localization_font.py assets/fonts/misaki_gothic.bdf include/starfox/localization/bitmap_font.hpp
python tests/test_dialogue_catalog.py
```

The EX generator rejects missing entries. Glyph coverage tests include both
radio catalogs and the host menus. See `assets/fonts/README.md` for font source
and licensing; composed Latin additions are defined in the font generator.

Before release, verify dialogue wrapping and timing, all menu/help/control
screens, changing language without restart, persistence, both EX message
channels, and controller colors in every non-English language. A passing
catalog test or a single menu capture does not satisfy these runtime checks.
