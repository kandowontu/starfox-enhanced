#!/usr/bin/env python3
"""Generate a portable C++ translation unit for runtime patch resources."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--chunk-bytes", type=int, default=0,
                        help="split large literals and assemble resources lazily")
    parser.add_argument("--split-dir", type=Path,
                        help="emit one translation unit per resource")
    parser.add_argument(
        "--resource", action="append", default=[], metavar="ID=PATH")
    args = parser.parse_args()
    if args.chunk_bytes < 0 or (args.chunk_bytes and args.chunk_bytes > 8192):
        parser.error("--chunk-bytes must be between 1 and 8192")
    if args.split_dir and not args.chunk_bytes:
        parser.error("--split-dir requires --chunk-bytes")

    resources: list[tuple[int, bytes]] = []
    for item in args.resource:
        identifier_text, separator, path_text = item.partition("=")
        if not separator:
            parser.error(f"invalid resource mapping: {item}")
        resources.append((int(identifier_text, 0), Path(path_text).read_bytes()))
    resources.sort(key=lambda item: item[0])

    if args.split_dir:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.split_dir.mkdir(parents=True, exist_ok=True)
        dispatch = [
            '#include "starfox/assets/embedded.hpp"',
            '#include <stdexcept>',
            'namespace starfox::assets {',
        ]
        for identifier, _ in resources:
            dispatch.append(
                f'std::span<const std::uint8_t> embedded_resource_{identifier}();')
        dispatch.extend([
            'std::span<const std::uint8_t> embedded_asset(int identifier) {',
            '    switch (identifier) {',
        ])
        for identifier, _ in resources:
            dispatch.append(
                f'    case {identifier}: return embedded_resource_{identifier}();')
        dispatch.extend([
            '    default: throw std::runtime_error{"embedded Star Fox asset resource is missing"};',
            '    }',
            '}',
            '} // namespace starfox::assets',
            '',
        ])
        args.output.write_text('\n'.join(dispatch), encoding='utf-8', newline='\n')

        for identifier, payload in resources:
            lines = [
                '#include "starfox/assets/embedded.hpp"',
                '#include <vector>',
                'namespace starfox::assets {',
                'namespace {',
            ]
            for part, offset in enumerate(range(0, len(payload), args.chunk_bytes)):
                lines.append(f'const unsigned char part_{part}[] =')
                for block in range(offset, min(offset + args.chunk_bytes, len(payload)), 256):
                    chunk = payload[block : min(block + 256, offset + args.chunk_bytes)]
                    lines.append('    "' + ''.join(f'\\x{value:02x}' for value in chunk) + '"')
                lines.append(';')
            lines.extend([
                '}',
                f'std::span<const std::uint8_t> embedded_resource_{identifier}() {{',
                '    static const std::vector<std::uint8_t> data = [] {',
                '        std::vector<std::uint8_t> result;',
                f'        result.reserve({len(payload)});',
            ])
            for part, _ in enumerate(range(0, len(payload), args.chunk_bytes)):
                lines.extend([
                    f'        const auto* begin_{part} = reinterpret_cast<const std::uint8_t*>(part_{part});',
                    f'        result.insert(result.end(), begin_{part}, begin_{part} + sizeof(part_{part}) - 1);',
                ])
            lines.extend([
                '        return result;',
                '    }();',
                '    return {data};',
                '}',
                '} // namespace starfox::assets',
                '',
            ])
            (args.split_dir / f'embedded_resource_{identifier}.cpp').write_text(
                '\n'.join(lines), encoding='utf-8', newline='\n')
        return 0

    lines = [
        '#include "starfox/assets/embedded.hpp"',
        "",
        "#include <stdexcept>",
        *(["#include <vector>"] if args.chunk_bytes else []),
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
        if args.chunk_bytes:
            for part, offset in enumerate(range(0, len(payload), args.chunk_bytes)):
                lines.append(f"const unsigned char r{identifier}_{part}[] =")
                for block in range(offset, min(offset + args.chunk_bytes, len(payload)), 256):
                    chunk = payload[block : min(block + 256, offset + args.chunk_bytes)]
                    lines.append('    "' + ''.join(f"\\x{value:02x}" for value in chunk) + '"')
                lines.append(";")
        else:
            lines.append(f"const unsigned char r{identifier}[] =")
            for offset in range(0, len(payload), 256):
                chunk = payload[offset : offset + 256]
                lines.append('    "' + ''.join(f"\\x{value:02x}" for value in chunk) + '"')
            if not payload:
                lines.append('    ""')
            lines.append(";")
    lines.extend(["}", "", "std::span<const std::uint8_t> embedded_asset(int identifier) {"])
    lines.append("    switch (identifier) {")
    for identifier, payload in resources:
        if args.chunk_bytes:
            lines.extend([
                f"    case {identifier}: {{",
                "        static const std::vector<std::uint8_t> data = [] {",
                "            std::vector<std::uint8_t> result;",
                f"            result.reserve({len(payload)});"])
            for part, _ in enumerate(range(0, len(payload), args.chunk_bytes)):
                lines.extend([
                    f"            const auto* begin{part} = reinterpret_cast<const std::uint8_t*>(r{identifier}_{part});",
                    f"            result.insert(result.end(), begin{part}, begin{part} + sizeof(r{identifier}_{part}) - 1);"])
            lines.extend(["            return result;", "        }();", "        return {data};", "    }"])
        else:
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
