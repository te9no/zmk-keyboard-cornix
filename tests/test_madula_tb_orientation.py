"""Mounting-transform regression from the owner's up/left and right/up report."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLAGS = {"INPUT_TRANSFORM_XY_SWAP": 1, "INPUT_TRANSFORM_X_INVERT": 2,
         "INPUT_TRANSFORM_Y_INVERT": 4}


def transform(mask, x, y):
    # ZMK input_processor_transform.c swaps event codes before inversion.
    if mask & 1:
        x, y = y, x
    return (-x if mask & 2 else x, -y if mask & 4 else y)


class MadulaTrackballOrientationTests(unittest.TestCase):
    def test_both_listeners_correct_the_reported_mounting(self):
        # Raw vectors inferred by undoing the previous X-only inversion.
        cases = {"up": ((1, 0), (0, -1)), "right": ((0, -1), (1, 0)),
                 "down": ((-1, 0), (0, 1)), "left": ((0, 1), (-1, 0)),
                 "diagonal": ((1, -1), (1, -1)), "idle": ((0, 0), (0, 0))}
        for snippet in ("madula-trackball", "madula-dya-trackball"):
            text = (ROOT / f"snippets/{snippet}/{snippet}.overlay").read_text()
            expression = re.search(r"<&zip_xy_transform\s+([^>]+)>", text).group(1)
            mask = 0
            for flag in re.findall(r"INPUT_TRANSFORM_[A-Z_]+", expression):
                mask |= FLAGS[flag]
            self.assertEqual(mask, 7)
            for direction, (raw, expected) in cases.items():
                with self.subTest(snippet=snippet, direction=direction):
                    self.assertEqual(transform(mask, *raw), expected)

    def test_studio_tuning_remains_after_mounting_transform(self):
        text = (ROOT / "snippets/madula-dya-trackball/madula-dya-trackball.overlay").read_text()
        self.assertLess(text.index("&zip_xy_transform"), text.index("&mouse_runtime_input_processor"))


if __name__ == "__main__":
    unittest.main()
