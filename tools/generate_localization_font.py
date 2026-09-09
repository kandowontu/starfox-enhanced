"""Convert the licensed Misaki BDF into portable eight-row Unicode glyphs."""
import argparse
import unicodedata
from pathlib import Path


def parse_bdf(text):
    glyphs = {}
    for block in text.split("STARTCHAR ")[1:]:
        lines = block.splitlines()
        fields = {line.split()[0]: line.split()[1:] for line in lines if line.strip()}
        code = int(fields["ENCODING"][0])
        width, height, left, bottom = map(int, fields["BBX"])
        advance = int(fields["DWIDTH"][0])
        if not 0 <= advance <= 8 or width > 8 or height > 8:
            raise ValueError(f"Unsupported glyph metrics: {code}")
        bitmap = lines.index("BITMAP") + 1
        rows = [0] * 8
        for row, encoded in enumerate(lines[bitmap:bitmap + height]):
            y = 6 - bottom - height + row
            bits = int(encoded, 16)
            for column in range(width):
                x = left + column
                if bits & (0x80 >> column):
                    if not (0 <= x < 8 and 0 <= y < 8):
                        raise ValueError(f"Glyph escapes cell: {code}")
                    rows[y] |= 0x80 >> x
        glyphs[code] = (advance, rows)
    # Misaki's Japanese distribution omits accented Latin letters. Compose
    # these from its Latin glyphs, reserving two scanlines for the accent.
    accents = {"\u0301": (0x20, 0x40), "\u0300": (0x40, 0x20),
               "\u0302": (0x40, 0xa0),
               "\u0308": (0xa0, 0x00), "\u0303": (0x50, 0xa0)}
    for code in range(0xc0, 0x100):
        decomposed = unicodedata.normalize("NFD", chr(code))
        if code in glyphs or len(decomposed) != 2 or ord(decomposed[0]) not in glyphs:
            continue
        advance, base = glyphs[ord(decomposed[0])]
        mark = decomposed[1]
        if mark in accents:
            # Keep both cap-height endpoints while fitting the six-row base.
            body = [base[index] for index in (0, 1, 2, 4, 5)]
            glyphs[code] = (max(advance, 4), [*accents[mark], *body, 0])
        elif mark == '\u0327':
            glyphs[code] = (advance, [*base[:6], 0x20, 0x40])
    for inverted, normal in ((0xa1, ord('!')), (0xbf, ord('?'))):
        advance, rows = glyphs[normal]
        glyphs[inverted] = (advance, list(reversed(rows[:6])) + [0, 0])
    # Authored five-pixel eszett; unlike accented Latin letters it does not
    # have a canonical base-plus-combining-mark decomposition.
    glyphs[0xdf] = (5, [0x60, 0x90, 0xa0, 0x90, 0x90, 0xa0, 0x80, 0])
    glyphs[0x153] = (7, [0, 0, 0x6c, 0x92, 0x9e, 0x90, 0x6e, 0])
    return glyphs


def generate(glyphs):
    lines = ["// Generated from Misaki Gothic. Copyright (C) 2002-2021 Num Kadoma.",
             "// Redistribution license and source: assets/fonts/README.md", "#pragma once",
             "#include <cstdint>", "#include <iterator>",
             "namespace starfox::localization {",
             "struct Glyph { char32_t code; unsigned char advance; unsigned char rows[8]; };",
             "inline constexpr Glyph glyphs[] = {"]
    for code, (advance, rows) in sorted(glyphs.items()):
        lines.append(f"    {{0x{code:x}, {advance}, {{" + ','.join(f"0x{row:02x}" for row in rows) + "}},")
    lines += ["};", "inline const Glyph* glyph(char32_t code) {",
              "    unsigned first = 0, last = std::size(glyphs);",
              "    while (first < last) {",
              "        const auto middle = first + (last - first) / 2;",
              "        if (glyphs[middle].code < code) first = middle + 1; else last = middle;",
              "    }",
              "    return first < std::size(glyphs) && glyphs[first].code == code ? &glyphs[first] : nullptr;",
              "}", "} // namespace starfox::localization", ""]
    return '\n'.join(lines)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    glyphs = parse_bdf(args.source.read_text())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate(glyphs), encoding="ascii")
    print(f"Generated {len(glyphs)} Unicode glyphs")
