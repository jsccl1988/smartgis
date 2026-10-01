# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Unit tests for multi-monitor record.hwnd helpers (no HWND required)."""

from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[2]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.record.hwnd import (  # noqa: E402
    _near_black_frac,
    _rect_fully_on_primary,
    _virtual_screen,
    find_window_by_title_substr,
    hwnd_pid,
    record_mode_pref,
)


class RecordHwndHelpersTest(unittest.TestCase):
    def test_virtual_screen_positive(self) -> None:
        v = _virtual_screen()
        self.assertGreater(v["width"], 0)
        self.assertGreater(v["height"], 0)
        self.assertGreater(v["primary_w"], 0)
        self.assertGreater(v["primary_h"], 0)

    def test_rect_fully_on_primary(self) -> None:
        v = _virtual_screen()
        pw, ph = v["primary_w"], v["primary_h"]
        self.assertTrue(_rect_fully_on_primary(10, 10, min(400, pw), min(300, ph)))
        # Off primary (typical secondary monitor origin on this machine).
        self.assertFalse(_rect_fully_on_primary(pw + 100, 0, pw + 500, 400))
        self.assertFalse(_rect_fully_on_primary(-100, 0, 100, 100))

    def test_near_black_frac(self) -> None:
        black = bytes([0, 0, 0] * 100)
        self.assertGreater(_near_black_frac(black, 10, 10), 0.95)
        white = bytes([255, 255, 255] * 100)
        self.assertLess(_near_black_frac(white, 10, 10), 0.05)

    def test_record_mode_pref(self) -> None:
        self.assertEqual(record_mode_pref({}), "auto")
        self.assertEqual(record_mode_pref({"SMT_HARNESS_RECORD_MODE": "bmp"}), "bmp")
        self.assertEqual(
            record_mode_pref({"SMT_HARNESS_RECORD_MODE": "ffmpeg"}), "ffmpeg"
        )

    def test_pid_filter_skips_foreign_title_match(self) -> None:
        # Cursor workspace titles contain "smartgis"; pid filter must not bind them.
        hwnd, title = find_window_by_title_substr(
            "smartgis", timeout_sec=0.4, pid=-1
        )
        self.assertEqual(hwnd, 0)
        self.assertEqual(title, "")
        # hwnd_pid(0) is defined.
        self.assertEqual(hwnd_pid(0), 0)


if __name__ == "__main__":
    unittest.main()
