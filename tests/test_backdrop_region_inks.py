"""Exact 4bpp source-ink checks for authored celestial-region audits."""
import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    'inspect_backdrop_palette', Path(__file__).resolve().parents[1] / 'tools/inspect_backdrop_palette.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class RegionInkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.stem = Path(self.temp.name) / 'capture'
        self.path = self.stem.with_suffix('.ppu.json')
        self.metadata = {'bg2_tile16': 0, 'bg2_size': 3,
                         'bg2_map': 0x1000, 'bg2_char': 0x4000}
        self.vram = bytearray(65536)
        self.palette = [0] * 256

    def tile(self, tx, ty, word):
        pages = 2 if self.metadata['bg2_size'] & 1 else 1
        entry = (tx // 32 + ty // 32 * pages) * 1024 + ty % 32 * 32 + tx % 32
        struct.pack_into('<H', self.vram, self.metadata['bg2_map'] * 2 + entry * 2, word)

    def pixel(self, character, x, y, ink):
        for bit in range(4):
            if ink & (1 << bit):
                address = self.metadata['bg2_char'] * 2 + character * 32 + y * 2
                self.vram[(address + bit % 2 + (bit // 2) * 16) & 65535] |= 128 >> x

    def inspect(self, rectangle):
        self.path.write_text(json.dumps(self.metadata))
        self.stem.with_suffix('.vram').write_bytes(self.vram)
        self.stem.with_suffix('.cgram').write_bytes(struct.pack('<256H', *self.palette))
        return {item['index']: item for item in MODULE.region_inks(self.path, rectangle)['inks']}

    def test_all_planes_palette_and_transparency(self):
        self.tile(0, 0, 5 << 10)
        self.pixel(0, 3, 4, 13)
        self.palette[93] = 14 | 17 << 5 | 9 << 10
        inks = self.inspect([0, 0, 8, 8])
        self.assertEqual(inks[93], {'index': 93, 'rgb5': [14, 17, 9],
                                   'count': 1, 'bounds': [3, 4, 4, 5]})
        self.assertEqual(inks[0]['count'], 63)  # Zero ink is transparent, not bank 5 ink 0.

    def test_tile_flips(self):
        self.tile(0, 0, 0xc000)
        self.pixel(0, 1, 2, 7)
        self.assertEqual(self.inspect([0, 0, 8, 8])[7]['bounds'], [6, 5, 7, 6])

    def test_bottom_right_map_page(self):
        self.tile(32, 32, 2 | 3 << 10)
        self.pixel(2, 7, 7, 15)
        inks = self.inspect([256, 256, 264, 264])
        self.assertEqual(inks[63]['bounds'], [263, 263, 264, 264])

    def test_large_tile_subcharacters(self):
        self.metadata['bg2_tile16'] = 1
        self.tile(0, 0, 2 << 10)
        self.pixel(17, 2, 3, 9)
        self.assertEqual(self.inspect([0, 0, 16, 16])[41]['bounds'], [10, 11, 11, 12])

    def test_character_address_wrap(self):
        self.metadata['bg2_char'] = 0x7ff8
        self.tile(0, 0, 1)
        self.pixel(1, 2, 3, 12)
        self.assertEqual(self.inspect([0, 0, 8, 8])[12]['bounds'], [2, 3, 3, 4])

    def test_invalid_rectangle_rejected(self):
        for rectangle in ([0, 0, 0, 8], [-1, 0, 8, 8], [0, 0, 513, 8]):
            with self.assertRaises(ValueError):
                self.inspect(rectangle)


if __name__ == '__main__':
    unittest.main()
