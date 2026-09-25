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
        subprocess.run([sys.executable, str(root / 'tools/embed_runtime_assets.py'),
                        '--output', str(work / 'assets.cpp'),
                        '--resource', f'101={work / "bytes.bin"}',
                        '--resource', f'232={work / "empty.bin"}'], check=True)
        (work / 'check.cpp').write_text('''
#include "starfox/assets/embedded.hpp"
#include <cstdio>
#include <stdexcept>
int main() {
    if (!starfox::assets::embedded_asset(232).empty()) return 1;
    try { (void)starfox::assets::embedded_asset(999); return 2; }
    catch (const std::runtime_error&) {}
    const auto data = starfox::assets::embedded_asset(101);
    return std::fwrite(data.data(), 1, data.size(), stdout) == data.size() ? 0 : 3;
}
''', encoding='utf-8')
        binary = work / 'check'
        subprocess.run([args.compiler, '-std=c++20', '-O2', '-I', str(root / 'include'),
                        str(work / 'assets.cpp'), str(work / 'check.cpp'),
                        '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], check=True, capture_output=True)
        assert result.stdout == payload, 'Embedded binary bytes or length changed'
    print('Portable resources: all byte values, NULs, empty and missing IDs pass')


if __name__ == '__main__':
    main()
