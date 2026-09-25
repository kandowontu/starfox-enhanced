#!/usr/bin/env python3
"""Generate a portable C++ translation unit for runtime patch resources."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--resource", action="append", default=[], metavar="ID=PATH")
    args = parser.parse_args()

    resources: list[tuple[int, bytes]] = []
    for item in args.resource:
        identifier_text, separator, path_text = item.partition("=")
        if not separator:
            parser.error(f"invalid resource mapping: {item}")
        resources.append((int(identifier_text, 0), Path(path_text).read_bytes()))
    resources.sort(key=lambda item: item[0])

    lines = [
        '#include "starfox/assets/embedded.hpp"',
        "",
        "#include <stdexcept>",
        "",
        "namespace starfox::assets {",
        "namespace {",
    ]
    for identifier, payload in resources:
        # One initializer per byte makes the compiler build tens of millions
        # of AST nodes for the photographic backdrops. Adjacent string literals
        # retain every byte (including NUL) without that compilation overhead.
        # Always escape every byte: a following hexadecimal character must not
        # extend the preceding escape. The implicit terminator is not exposed.
        lines.append(f"const unsigned char r{identifier}[] =")
        for offset in range(0, len(payload), 256):
            chunk = payload[offset : offset + 256]
            lines.append('    "' + ''.join(f"\\x{value:02x}" for value in chunk) + '"')
        if not payload:
            lines.append('    ""')
        lines.append(";")
    lines.extend(["}", "", "std::span<const std::uint8_t> embedded_asset(int identifier) {"])
    lines.append("    switch (identifier) {")
    for identifier, _ in resources:
        lines.append(f"    case {identifier}: return {{r{identifier}, sizeof(r{identifier}) - 1}};")
    lines.extend([
        "    default: throw std::runtime_error{\"embedded Star Fox asset resource is missing\"};",
        "    }",
        "}",
        "",
        "} // namespace starfox::assets",
        "",
    ])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
