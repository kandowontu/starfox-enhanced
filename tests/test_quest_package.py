import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

spec=importlib.util.spec_from_file_location('quest_package',
    Path(__file__).resolve().parents[1]/'tools/check_quest_package.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class QuestPackageTest(unittest.TestCase):
    def fixture(self,root,extra=None,omit='',machine=183,art=b''):
        header=bytearray(64)
        header[:6]=b'\x7fELF\x02\x01'
        struct.pack_into('<HH',header,16,3,machine)
        entries={'AndroidManifest.xml':b'manifest','classes.dex':b'dex'}
        for library in ('starfox_quest','SDL3','c++_shared'):
            entries[f'lib/arm64-v8a/lib{library}.so']=header+(art if library=='starfox_quest' else b'')
        if extra:
            entries[extra]=b'forbidden'
        entries.pop(omit,None)
        path=Path(root)/'fixture.apk'
        with zipfile.ZipFile(path,'w') as apk:
            for name,data in entries.items():
                apk.writestr(name,data)
        return path

    def test_payload(self):
        with tempfile.TemporaryDirectory() as root:
            module.check(self.fixture(root))
            for entry in ('lib/arm64-v8a/libmain.so','lib/arm64-v8a/libandroid.so',
                          'lib/arm64-v8a/liblog.so','lib/x86_64/libother.so',
                          'assets/game.sfc','assets/Starfox-Assets.BIN','release.p12'):
                with self.subTest(entry=entry),self.assertRaises(ValueError):
                    module.check(self.fixture(root,extra=entry))
            with self.assertRaises(ValueError):
                module.check(self.fixture(root,omit='lib/arm64-v8a/libstarfox_quest.so'))
            with self.assertRaises(ValueError):
                module.check(self.fixture(root,machine=62))

    def test_complete_backdrop_required(self):
        with tempfile.TemporaryDirectory() as root:
            backdrop=Path(root)/'image.bmp'
            data=b'BM'+bytes(range(54))
            backdrop.write_bytes(data)
            module.check(self.fixture(root,art=data),[backdrop])
            with self.assertRaises(ValueError):
                module.check(self.fixture(root),[backdrop])
            with self.assertRaises(ValueError):
                module.check(self.fixture(root,art=data[:-1]),[backdrop])
            backdrop.write_bytes(b'BM')
            with self.assertRaises(ValueError):
                module.check(self.fixture(root,art=b'BM'),[backdrop])

    def test_split_backdrop_chunks(self):
        with tempfile.TemporaryDirectory() as root:
            backdrop=Path(root)/'split.bmp'
            data=b'BM'+b'A'*(8192-2)+b'B'*8192+b'C'*1234
            backdrop.write_bytes(data)
            parts=[data[offset:offset+8192] for offset in range(0,len(data),8192)]
            module.check(self.fixture(root,art=b'\x00gap\x00'.join(parts)),[backdrop])
            with self.assertRaises(ValueError):
                module.check(self.fixture(root,art=b'\x00gap\x00'.join(parts[:1]+parts[2:])),[backdrop])

    def test_catalogue(self):
        paths=module.source_backdrops(Path(__file__).resolve().parents[1])
        # The catalogue is shared with the desktop build and may grow. The
        # package check below validates every listed asset rather than pinning
        # an obsolete count when new worlds are added.
        self.assertGreaterEqual(len(paths),37)
        self.assertIn('orbital-ocean-surface-v1.bmp',{path.name for path in paths})
        self.assertIn('orbital-lava-surface-v1.bmp',{path.name for path in paths})
        self.assertTrue(all(path.is_file() for path in paths))


if __name__=='__main__':
    unittest.main()
