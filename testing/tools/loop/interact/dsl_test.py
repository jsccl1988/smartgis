# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Gesture suites are Python source. No Interact DSL parser."""

from __future__ import annotations

import ast
import unittest
from pathlib import Path

_UI = Path(__file__).resolve().parents[2] / "harness" / "ui"
SCRIPTS = {
    "ui.interact": _UI / "ui.interact" / "ui.interact.py",
    "ui.interact.combo": _UI / "ui.interact.combo" / "ui.interact.combo.py",
}


class InteractScriptSourceTest(unittest.TestCase):
    def test_gesture_scripts_are_python(self) -> None:
        self.assertEqual(len(SCRIPTS), 2)
        for name, path in SCRIPTS.items():
            self.assertTrue(path.is_file(), path)
            self.assertFalse(path.with_suffix(".il").exists(), name)
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            self.assertIsInstance(tree, ast.Module)
            self.assertGreater(len(tree.body), 0, name)


if __name__ == "__main__":
    unittest.main()
