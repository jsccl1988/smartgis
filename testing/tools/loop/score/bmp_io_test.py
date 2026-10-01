# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Unit tests for BMP pixel_diff_frac (zoom gate helper)."""

from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[2]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.score.bmp_io import pixel_diff_frac  # noqa: E402


def _write_bgr24(path: Path, w: int, h: int, pixels: list[tuple[int, int, int]]) -> None:
    row = ((w * 3 + 3) // 4) * 4
    payload = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b = pixels[y * w + x]
            payload.extend((b, g, r))
        payload.extend(b"\x00" * (row - w * 3))
    header = struct.pack(
        "<2sIHHIIiiHHIIiiII",
        b"BM",
        14 + 40 + len(payload),
        0,
        0,
        14 + 40,
        40,
        w,
        h,
        1,
        24,
        0,
        len(payload),
        0,
        0,
        0,
        0,
    )
    path.write_bytes(header + payload)


class PixelDiffFracTest(unittest.TestCase):
    def test_identical_is_zero(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            a = Path(tmp) / "a.bmp"
            b = Path(tmp) / "b.bmp"
            pix = [(10, 20, 30)] * 4
            _write_bgr24(a, 2, 2, pix)
            _write_bgr24(b, 2, 2, pix)
            self.assertEqual(pixel_diff_frac(a, b, thresh=12), 0.0)

    def test_changed_pixel_counts(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            a = Path(tmp) / "a.bmp"
            b = Path(tmp) / "b.bmp"
            _write_bgr24(a, 2, 2, [(0, 0, 0)] * 4)
            _write_bgr24(b, 2, 2, [(0, 0, 0), (0, 0, 0), (0, 0, 0), (255, 0, 0)])
            self.assertEqual(pixel_diff_frac(a, b, thresh=12), 0.25)


if __name__ == "__main__":
    unittest.main()
