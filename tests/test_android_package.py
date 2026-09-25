"""Negative package fixtures: success must require actual embedded artwork."""
import importlib.util
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest
import warnings
import zipfile

spec = importlib.util.spec_from_file_location('android_package',
    Path(__file__).resolve().parents[1] / 'tools/check_android_package.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class AndroidPackageTest(unittest.TestCase):
    def fixture(self, root, extra=None, omit='', machine=183, payload=None, duplicate=False):
        header = bytearray(64)
        header[:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<HH', header, 16, 3, machine)
        backdrop = root / 'expected.bmp'
        backdrop.write_bytes(b'BM' + bytes(range(2, 80)))
        entries = {'AndroidManifest.xml': b'manifest', 'classes.dex': b'dex'}
        for library in ('main', 'SDL3', 'c++_shared'):
            entries[f'lib/arm64-v8a/lib{library}.so'] = bytes(header)
        entries['lib/arm64-v8a/libmain.so'] += backdrop.read_bytes() if payload is None else payload
        if extra:
            entries[extra] = b'forbidden'
        entries.pop(omit, None)
        path = root / 'fixture.apk'
        with zipfile.ZipFile(path, 'w') as apk:
            for name, data in entries.items():
                apk.writestr(name, data)
            if duplicate:
                with warnings.catch_warnings():
                    warnings.simplefilter('ignore', UserWarning)
                    apk.writestr('classes.dex', b'other')
        return path, [backdrop]

    def test_runtime_and_private_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertEqual(module.check(*self.fixture(root)), 1)
            for entry in ('lib/arm64-v8a/libstarfox_quest.so', 'lib/arm64-v8a/libandroid.so',
                          'lib/arm64-v8a/liblog.so', 'lib/x86_64/libmain.so', 'assets/game.sfc',
                          'assets/game.smc', 'assets/Starfox-Assets.BIN', 'release.p12',
                          'release.jks', 'release.keystore', 'assets/docs/private.txt'):
                with self.subTest(entry=entry), self.assertRaises(ValueError):
                    module.check(*self.fixture(root, extra=entry))
            for kwargs in ({'omit': 'lib/arm64-v8a/libmain.so'}, {'machine': 62},
                           {'duplicate': True}, {'payload': b'BM'}, {'payload': bytes(range(80))}):
                with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                    module.check(*self.fixture(root, **kwargs))
            path, _ = self.fixture(root)
            with self.assertRaises(ValueError):
                module.check(path, [])

    def test_declared_assets_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = root / 'assets/enhanced-backdrops'
            assets.mkdir(parents=True)
            (assets / 'current.bmp').write_bytes(b'BM-current')
            (assets / 'obsolete.bmp').write_bytes(b'BM-obsolete')
            declaration = '--resource "200=${CMAKE_CURRENT_SOURCE_DIR}/assets/enhanced-backdrops/current.bmp"\n'
            source = root / 'CMakeLists.txt'
            source.write_text(declaration)
            self.assertEqual(module.source_backdrops(root), [(assets / 'current.bmp').resolve()])
            for content in ('', declaration * 2, declaration.replace('current.bmp', 'missing.bmp'),
                            declaration.replace('current.bmp', '../outside.bmp')):
                source.write_text(content)
                with self.subTest(content=content), self.assertRaises(ValueError):
                    module.source_backdrops(root)

    def test_split_embedded_backdrop(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            backdrop_bytes = b'BM' + b''.join(
                hashlib.sha256(str(index).encode()).digest()
                for index in range(600))
            parts = [backdrop_bytes[offset:offset + 8192]
                     for offset in range(0, len(backdrop_bytes), 8192)]
            path, backdrops = self.fixture(
                root, payload=b'\0'.join(parts))
            backdrops[0].write_bytes(backdrop_bytes)
            self.assertEqual(module.check(path, backdrops), 1)
            missing_path, backdrops = self.fixture(
                root, payload=b'\0'.join(parts[:-1]))
            backdrops[0].write_bytes(backdrop_bytes)
            with self.assertRaises(ValueError):
                module.check(missing_path, backdrops)


if __name__ == '__main__':
    unittest.main()
