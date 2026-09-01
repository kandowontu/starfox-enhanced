#!/usr/bin/env python3
"""Generate dependency-free UWP tile and splash PNGs."""

from __future__ import annotations

import argparse
import pathlib
import struct
import zlib


def chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def pixel(x: int, y: int, width: int, height: int) -> tuple[int, int, int, int]:
    # A restrained Arwing-like gold chevron on the game's deep-space blue.
    nx = (2.0 * x - width) / max(1, width)
    ny = (2.0 * y - height) / max(1, height)
    gold = abs(abs(nx) * 0.70 + ny - 0.12) < 0.085 and abs(nx) < 0.72
    core = abs(nx) < 0.08 and -0.30 < ny < 0.36
    if core:
        return (255, 238, 186, 255)
    if gold:
        return (244, 151, 36, 255)
    return (8, 21, 40, 255)


def write_png(path: pathlib.Path, width: int, height: int) -> None:
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixel(x, y, width, height))
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for name, width, height in (
        ("StoreLogo.png", 50, 50),
        ("Square44x44Logo.png", 44, 44),
        ("Square150x150Logo.png", 150, 150),
        ("Wide310x150Logo.png", 310, 150),
        ("SplashScreen.png", 620, 300),
    ):
        write_png(args.output / name, width, height)


if __name__ == "__main__":
    main()
