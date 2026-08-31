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
        "#include <array>",
        "#include <stdexcept>",
        "",
        "namespace starfox::assets {",
        "namespace {",
    ]
    for identifier, payload in resources:
        lines.append(
            f"constexpr std::array<std::uint8_t, {len(payload)}> r{identifier}{{{{")
        for offset in range(0, len(payload), 20):
            chunk = payload[offset : offset + 20]
            lines.append("    " + ",".join(f"0x{value:02x}" for value in chunk) + ",")
        lines.append("}};")
    lines.extend(["}", "", "std::span<const std::uint8_t> embedded_asset(int identifier) {"])
    lines.append("    switch (identifier) {")
    for identifier, _ in resources:
        lines.append(f"    case {identifier}: return r{identifier};")
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
