# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""China stack ladder: a later BMP cannot pass an earlier failure."""

from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[2]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.score.atmosphere import (  # noqa: E402
    score_china_stack,
    score_china_stack_dem,
)
from loop.score.bmp import score_bmp  # noqa: E402


def _write_bgr24(path: Path, w: int, h: int, paint) -> None:
    pixels = [paint(x, y, w, h) for y in range(h) for x in range(w)]
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
        54 + len(payload),
        0,
        0,
        54,
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


def _land(x, y, w, h):
    del x, w, h
    return (40, 140, 50)


def _ocean(x, y, w, h):
    del x, w
    if y > h // 2:
        return (40, 90, 160)
    return (40, 140, 50)


def _fog(x, y, w, h):
    del x, w
    if y < h // 3:
        return (40, 140, 50)
    if y < (2 * h // 3):
        return (140, 145, 150)
    return (40, 90, 160)


def _sky(x, y, w, h):
    del x, w
    if y < h // 8:
        return (30, 70, 190)
    if y < h // 3:
        return (90, 140, 210)
    return (40, 140, 50)


def _cloud(x, y, w, h):
    del x, w
    if y < h // 8:
        return (30, 70, 190)
    if y < h // 3:
        return (90, 140, 210)
    if y < (2 * h // 3):
        return (200, 202, 205)
    return (40, 140, 50)


def _white(x, y, w, h):
    del x, y, w, h
    return (240, 240, 240)


class ChinaStackLadderTest(unittest.TestCase):
    def test_stages_pass_in_order(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name, paint in (
                ("dem", _land),
                ("ocean", _ocean),
                ("fog", _fog),
                ("sky", _sky),
                ("cloud", _cloud),
            ):
                _write_bgr24(
                    root / f"atmosphere-showcase-china-stack-{name}.bmp",
                    24,
                    24,
                    paint,
                )
            report = score_china_stack(
                root / "atmosphere-showcase-china-stack-cloud.bmp"
            )
        self.assertTrue(report["ok"], report["stages"])
        self.assertTrue(report["witness_ignored"])

    def test_early_pixel_fail_blocks_later_stages(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            paints = {
                "dem": _white,
                "ocean": _ocean,
                "fog": _fog,
                "sky": _sky,
                "cloud": _cloud,
            }
            for name, paint in paints.items():
                _write_bgr24(
                    root / f"atmosphere-showcase-china-stack-{name}.bmp",
                    24,
                    24,
                    paint,
                )
            report = score_bmp(
                root / "atmosphere-showcase-china-stack-cloud.bmp",
                "china_stack",
            )
        self.assertFalse(report["ok"])
        self.assertEqual(report["stages"][0]["state"], "fail")
        self.assertEqual(report["stages"][1]["state"], "blocked")
        self.assertNotIn("pixels", report["stages"][1])

    def test_flag_fail_ignores_later_pixels(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name, paint in (
                ("dem", _land),
                ("ocean", _ocean),
                ("sky", _sky),
                ("cloud", _cloud),
            ):
                _write_bgr24(
                    root / f"atmosphere-showcase-china-stack-{name}.bmp",
                    24,
                    24,
                    paint,
                )
            (root / "china-stack-status.txt").write_text(
                "dem ok\nocean ok\nfog fail\nsky blocked\ncloud blocked\n",
                encoding="utf-8",
            )
            report = score_china_stack(
                root / "atmosphere-showcase-china-stack-cloud.bmp"
            )
        self.assertFalse(report["ok"])
        self.assertEqual(report["stages"][2]["state"], "fail")
        self.assertEqual(report["stages"][3]["state"], "blocked")
        self.assertNotIn("pixels", report["stages"][3])

    def test_dem_rejects_sky_dome(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "dem.bmp"
            _write_bgr24(path, 24, 24, _sky)
            self.assertFalse(score_china_stack_dem(path)["ok"])


if __name__ == "__main__":
    unittest.main()
