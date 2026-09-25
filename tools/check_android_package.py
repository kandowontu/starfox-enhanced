"""Inspect the ordinary Android APK without extracting or installing it."""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import zipfile


def source_backdrops(root):
    """Read the actual portable resource list, not every obsolete BMP on disk."""
    source = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
    entries = re.findall(r'^\s*--resource "(\d+)=\$\{CMAKE_CURRENT_SOURCE_DIR\}/'
                         r'(assets/enhanced-backdrops/[^"\r\n]+\.bmp)"', source, re.M)
    if not entries:
        raise ValueError('No enhanced backdrop resource declarations found')
    identifiers = [int(identifier) for identifier, _ in entries]
    if len(identifiers) != len(set(identifiers)):
        raise ValueError('Duplicate enhanced backdrop resource IDs')
    paths = []
    for _, relative in entries:
        path = (root / relative).resolve()
        if not path.is_relative_to((root / 'assets/enhanced-backdrops').resolve()):
            raise ValueError(f'Backdrop escapes asset directory: {relative}')
        if not path.is_file():
            raise ValueError(f'Missing declared backdrop: {relative}')
        paths.append(path)
    return paths


def check(path, backdrops):
    if not backdrops:
        raise ValueError('At least one expected backdrop is required')
    with zipfile.ZipFile(path) as apk:
        entries = apk.infolist()
        names = [entry.filename for entry in entries]
        if len(names) != len(set(names)):
            raise ValueError("APK contains duplicate entry names")
        required = ("AndroidManifest.xml", "classes.dex", "lib/arm64-v8a/libmain.so",
                    "lib/arm64-v8a/libSDL3.so", "lib/arm64-v8a/libc++_shared.so")
        for name in required:
            if name not in names or apk.getinfo(name).file_size == 0:
                raise ValueError(f"Missing/empty runtime entry: {name}")
            if name.endswith('.so'):
                with apk.open(name) as library:
                    header = library.read(64)
                if (len(header) != 64 or header[:6] != b'\x7fELF\x02\x01' or
                        struct.unpack_from('<HH', header, 16) != (3, 183)):
                    raise ValueError(f'Runtime is not an arm64 ELF shared object: {name}')
        for name in names:
            lower = name.lower()
            if lower.startswith('lib/') and (not lower.startswith('lib/arm64-v8a/') or
                    lower.endswith(('/libandroid.so', '/liblog.so', '/libstarfox_quest.so'))):
                raise ValueError(f'Wrong ABI, VR entry or Android system link stub: {name}')
            if lower.endswith(('.sfc', '.smc', 'starfox-assets.bin', '.p12', '.jks', '.keystore')):
                raise ValueError(f'Private game/signing data in APK: {name}')
            if lower.startswith(("assets/docs/", "docs/")):
                raise ValueError(f"Unexpected development documentation: {name}")
        native = apk.read("lib/arm64-v8a/libmain.so")
        for backdrop_path in backdrops:
            backdrop = backdrop_path.read_bytes()
            if not backdrop.startswith(b"BM") or len(backdrop) < 54:
                raise ValueError(f"Expected a nonempty BMP backdrop: {backdrop_path}")
            if backdrop not in native:
                # Mobile builds keep each asset in bounded compiler units.
                # Every exact source chunk must still be present in the ELF;
                # the runtime assembles them into one stable byte span.
                chunks = (backdrop[offset:offset + 8192]
                          for offset in range(0, len(backdrop), 8192))
                if any(chunk not in native for chunk in chunks):
                    raise ValueError(
                        f"Native runtime lacks the complete expected backdrop: {backdrop_path}")
        if apk.testzip():
            raise ValueError('Corrupt APK entry')
    return len(backdrops)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--backdrop", type=Path, action="append", default=[],
                        help="Expected embedded BMP; repeat for every bundled backdrop")
    parser.add_argument('--source-root', type=Path,
                        help='Verify every enhanced backdrop declared in the source CMake resource list')
    args = parser.parse_args()
    backdrops = args.backdrop + (source_backdrops(args.source_root) if args.source_root else [])
    count = check(args.apk, backdrops)
    print(f'Android payload verified: arm64 runtimes, no system stubs/ROM/BIN/signing files/docs; '
          f'{count} complete backdrop resources')
    with args.apk.open("rb") as source:
        print(f"APK SHA-256: {hashlib.file_digest(source, 'sha256').hexdigest()}")


if __name__ == "__main__":
    main()
