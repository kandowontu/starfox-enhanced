"""Negative controls for the final-image shutter measurement oracle."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_scramble_sequence import visible_edges, opening_step


class ShutterOracleTests(unittest.TestCase):
    def test_straight(self):
        self.assertEqual(visible_edges([(0, 20, 1, 40)] * 96, "test"), (20, 40))

    def test_closed(self):
        self.assertIsNone(visible_edges([None] * 96, "test"))

    def test_slit(self):
        with self.assertRaisesRegex(AssertionError, "vertical slit"):
            visible_edges([(0, 20, 1, 40), None], "test")

    def test_ragged_edges(self):
        for second in [(0, 21, 1, 40), (0, 20, 1, 41)]:
            with self.assertRaisesRegex(AssertionError, "not horizontal"):
                visible_edges([(0, 20, 1, 40), second], "test")

    def test_smooth(self):
        self.assertEqual(opening_step((20, 40), (19, 42), 3, "test"), 2)

    def test_coarse_source_jump(self):
        with self.assertRaisesRegex(AssertionError, "jump"):
            opening_step((20, 40), (4, 56), 3, "test")

    def test_reverse(self):
        for current in [(21, 40), (20, 39)]:
            with self.assertRaisesRegex(AssertionError, "backwards"):
                opening_step((20, 40), current, 3, "test")


if __name__ == "__main__":
    unittest.main()
