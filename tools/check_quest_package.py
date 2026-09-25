"""Validate Quest APK payload without installing it or reading signing secrets."""
import argparse
import struct
from pathlib import Path
import zipfile
import re


def source_backdrops(root):
    source = (root / 'include/starfox/render/enhanced_backdrop_library.hpp').read_text(encoding='utf-8')
    paths = re.findall(r'EnhancedBackdropAsset\{"(assets/enhanced-backdrops/[a-z0-9-]+\.bmp)"', source)
    if not paths or len(paths) != len(set(paths)):
        raise ValueError('Missing or duplicate shared backdrop catalogue')
    return [root / path for path in paths]


def check(path, backdrops=()):
    with zipfile.ZipFile(path) as apk:
        names=apk.namelist()
        if len(names)!=len(set(names)):
            raise ValueError('Duplicate APK entries')
        required=('AndroidManifest.xml','classes.dex','lib/arm64-v8a/libstarfox_quest.so',
                  'lib/arm64-v8a/libSDL3.so','lib/arm64-v8a/libc++_shared.so')
        for name in required:
            if name not in names or not apk.getinfo(name).file_size:
                raise ValueError(f'Missing/empty Quest runtime entry: {name}')
            if name.endswith('.so'):
                with apk.open(name) as library:
                    header=library.read(64)
                if (len(header)!=64 or header[:6]!=b'\x7fELF\x02\x01' or
                        struct.unpack_from('<HH',header,16)!=(3,183)):
                    raise ValueError(f'Quest library is not an arm64 ELF shared object: {name}')
        for name in names:
            lower=name.lower()
            if lower.endswith(('.sfc','.smc','starfox-assets.bin','.p12','.jks','.keystore')):
                raise ValueError(f'Private game/signing data in APK: {name}')
            if lower.startswith('lib/') and (
                not lower.startswith('lib/arm64-v8a/') or
                lower.endswith(('/libmain.so','/libandroid.so','/liblog.so'))):
                raise ValueError(f'Wrong ABI, flat runtime or platform stub: {name}')
        if backdrops:
            native=apk.read('lib/arm64-v8a/libstarfox_quest.so')
            for path in backdrops:
                data=path.read_bytes()
                if len(data)<54 or not data.startswith(b'BM'):
                    raise ValueError(f'Invalid expected BMP backdrop: {path}')
                if data not in native:
                    raise ValueError(f'Quest runtime lacks complete backdrop: {path}')
        bad=apk.testzip()
        if bad:
            raise ValueError(f'Corrupt APK entry: {bad}')
    print('Quest payload verified: arm64 VR/SDL runtimes, no flat entry, system stubs, ROM/BIN or signing files')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk',type=Path)
    parser.add_argument('--source-root',type=Path,
                        help='Verify every shared enhanced backdrop is embedded in the Quest runtime')
    args=parser.parse_args()
    backdrops=source_backdrops(args.source_root) if args.source_root else ()
    check(args.apk,backdrops)
    if backdrops:
        print(f'Quest artwork verified: {len(backdrops)} complete shared backdrop resources')
