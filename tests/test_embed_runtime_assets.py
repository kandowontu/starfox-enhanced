"""Compile and round-trip portable binary resources without byte-initializer ASTs."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', default='c++')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix='starfox-embedded-') as directory:
        work = Path(directory)
        payload = bytes(range(256)) * 5 + b'\x00\xffABCDEF0123456789\x00'
        (work / 'bytes.bin').write_bytes(payload)
        (work / 'empty.bin').write_bytes(b'')
        (work / 'check.cpp').write_text('''
#include "starfox/assets/embedded.hpp"
#include <cstdio>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
int main() {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (!starfox::assets::embedded_asset(232).empty()) return 1;
    try { (void)starfox::assets::embedded_asset(999); return 2; }
    catch (const std::runtime_error&) {}
    const auto data = starfox::assets::embedded_asset(101);
    return std::fwrite(data.data(), 1, data.size(), stdout) == data.size() ? 0 : 3;
}
''', encoding='utf-8')
        for mode, chunk_bytes, split in (
                ('normal', 0, False), ('chunked', 257, False),
                ('split', 257, True)):
            command = [sys.executable, str(root / 'tools/embed_runtime_assets.py'),
                       '--output', str(work / 'assets.cpp'),
                       '--resource', f'101={work / "bytes.bin"}',
                       '--resource', f'232={work / "empty.bin"}']
            if chunk_bytes:
                command.extend(['--chunk-bytes', str(chunk_bytes)])
            if split:
                command.extend(['--split-dir', str(work / 'split')])
            subprocess.run(command, check=True)
            binary = work / 'check'
            sources = [str(work / 'assets.cpp')]
            if split:
                sources.extend(str(path) for path in sorted((work / 'split').glob('*.cpp')))
            subprocess.run([args.compiler, '-std=c++20', '-O2', '-I', str(root / 'include'),
                            *sources, str(work / 'check.cpp'),
                            '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True)
            assert result.stdout == payload, (
                f'Embedded binary bytes or length changed for mode={mode}: '
                f'{len(result.stdout)} != {len(payload)}')
    print('Portable resources: normal/chunked/split bytes, NULs, empty and missing IDs pass')


if __name__ == '__main__':
    main()
