#!/usr/bin/env python3
"""Build the optional Star Fox Enhanced MSU-1 companion archive."""

from __future__ import annotations

import argparse
import os
import struct
import tempfile
import zlib
from pathlib import Path


MAGIC = b"SFEMSU1\0"
VERSION = 1
TRACK_COUNT = 52
HEADER_SIZE = 16 + TRACK_COUNT * 24


def build_pack(output: Path, tracks: list[Path]) -> None:
    if len(tracks) != TRACK_COUNT:
        raise ValueError(f"expected {TRACK_COUNT} tracks, got {len(tracks)}")
    missing = [str(track) for track in tracks if not track.is_file()]
    if missing:
        raise FileNotFoundError("missing MSU-1 tracks: " + ", ".join(missing))

    output.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=output.name + ".", suffix=".tmp", dir=output.parent
    )
    entries: list[tuple[int, int, int, int]] = []
    try:
        with os.fdopen(descriptor, "w+b") as archive:
            archive.write(b"\0" * HEADER_SIZE)
            for track in tracks:
                offset = archive.tell()
                size = 0
                checksum = 0
                with track.open("rb") as source:
                    while chunk := source.read(1024 * 1024):
                        archive.write(chunk)
                        size += len(chunk)
                        checksum = zlib.crc32(chunk, checksum)
                if size == 0:
                    raise ValueError(f"MSU-1 track is empty: {track}")
                entries.append((offset, size, checksum & 0xFFFFFFFF, 0))

            archive.seek(0)
            archive.write(struct.pack("<8sII", MAGIC, VERSION, TRACK_COUNT))
            for entry in entries:
                archive.write(struct.pack("<QQII", *entry))
            archive.flush()
            os.fsync(archive.fileno())
        os.replace(temporary_name, output)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("tracks", nargs="+", type=Path)
    arguments = parser.parse_args()
    build_pack(arguments.output, arguments.tracks)


if __name__ == "__main__":
    main()
