from __future__ import annotations

import unittest
from pathlib import Path

from tools.shape_audit import audit_shapes, split_arguments


class ShapeAuditTests(unittest.TestCase):
    def test_argument_split_preserves_grouped_expression(self) -> None:
        self.assertEqual(
            split_arguments("points,0,faces,(size+1)<<2,<NAME>"),
            ["points", "0", "faces", "(size+1)<<2", "<NAME>"],
        )

    def test_static_and_animated_shape_inventory(self) -> None:
        shape_dir = Path(__file__).parent / "fixtures" / "good_shapes"
        audit = audit_shapes(shape_dir)

        self.assertEqual(len(audit.headers), 1)
        self.assertEqual(audit.headers[0].name, "ship")
        self.assertEqual(audit.headers[0].bounds, ["20", "10", "30"])
        self.assertEqual(audit.point_rows, 5)
        self.assertEqual(audit.polygon_rows, 1)
        self.assertEqual(audit.animated_tables, 1)
        self.assertFalse(any(issue.severity == "error" for issue in audit.issues))

    def test_malformed_face_is_an_error(self) -> None:
        shape_dir = Path(__file__).parent / "fixtures" / "bad_shapes"
        audit = audit_shapes(shape_dir)

        self.assertTrue(any(issue.severity == "error" for issue in audit.issues))

    def test_checked_out_upstream_has_expected_scale(self) -> None:
        upstream_shapes = (
            Path(__file__).parents[1] / "upstream-ultrastarfox" / "SF" / "SHAPES"
        )
        if not upstream_shapes.is_dir():
            self.skipTest("UltraStarFox checkout is intentionally not committed")

        audit = audit_shapes(upstream_shapes)
        self.assertGreaterEqual(len(audit.headers), 450)
        self.assertGreaterEqual(audit.point_rows, 17_000)
        self.assertGreaterEqual(audit.polygon_rows, 7_000)
        self.assertFalse(any(issue.severity == "error" for issue in audit.issues))


if __name__ == "__main__":
    unittest.main()
