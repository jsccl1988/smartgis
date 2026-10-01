# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Unit tests for Interact DSL parser (no HWND)."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[2]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.interact.dsl import (  # noqa: E402
    Block,
    Call,
    Driver,
    parse_interact,
    parse_interact_file,
)

# testing/tools/harness/ui/<suite_id>/<id>.il
_UI = Path(__file__).resolve().parents[2] / "harness" / "ui"
SCRIPTS = {
    "ui.interact.smoke": _UI / "ui.interact.smoke" / "ui.interact.smoke.il",
    "ui.interact": _UI / "ui.interact" / "ui.interact.il",
    "ui.interact.combo": _UI / "ui.interact.combo" / "ui.interact.combo.il",
}


class InteractDslParseTest(unittest.TestCase):
    def test_smoke_example_parses(self) -> None:
        path = SCRIPTS["ui.interact.smoke"]
        name, stmts = parse_interact_file(path)
        self.assertEqual(name, "ui.interact.smoke")
        self.assertGreaterEqual(len(stmts), 5)
        kinds = []
        for s in stmts:
            if isinstance(s, Call):
                kinds.append(s.name)
            elif isinstance(s, Block):
                kinds.append(s.kind)
        self.assertIn("seq", kinds)
        self.assertIn("repeat", kinds)
        self.assertIn("mark", kinds)

    def test_full_ui_interact_parses(self) -> None:
        path = SCRIPTS["ui.interact"]
        name, stmts = parse_interact_file(path)
        self.assertEqual(name, "ui.interact")
        self.assertGreaterEqual(len(stmts), 8)
        # First stmt is inproc seq of map tabs.
        first = stmts[0]
        self.assertIsInstance(first, Block)
        assert isinstance(first, Block)
        self.assertEqual(first.kind, "seq")
        self.assertEqual(first.drivers, Driver.INPROC)

    def test_os_only_annotation(self) -> None:
        src = '''
        script "t" {
          click(shell_client, 1, 2) @os @inject=sendinput;
        }
        '''
        _name, stmts = parse_interact(src)
        self.assertEqual(len(stmts), 1)
        call = stmts[0]
        self.assertIsInstance(call, Call)
        assert isinstance(call, Call)
        self.assertEqual(call.drivers, Driver.OS)
        self.assertEqual(call.inject, "sendinput")

    def test_escaped_string_args(self) -> None:
        src = r'''
        script "t" {
          run_processing(id="op", args="{\"path\":\"$bnd\",\"n\":1}");
        }
        '''
        _name, stmts = parse_interact(src)
        self.assertEqual(len(stmts), 1)
        call = stmts[0]
        self.assertIsInstance(call, Call)
        assert isinstance(call, Call)
        self.assertEqual(call.name, "run_processing")
        args = {k: v for k, v in call.args if k}
        self.assertEqual(args.get("id"), "op")
        self.assertEqual(args.get("args"), '{"path":"$bnd","n":1}')

    def test_combo_example_parses(self) -> None:
        path = SCRIPTS["ui.interact.combo"]
        name, stmts = parse_interact_file(path)
        self.assertEqual(name, "ui.interact.combo")
        self.assertGreaterEqual(len(stmts), 5)
        kinds = []
        for s in stmts:
            if isinstance(s, Call):
                kinds.append(s.name)
            elif isinstance(s, Block):
                kinds.append(s.kind)
        self.assertIn("seq", kinds)
        self.assertIn("mark", kinds)
        # Nested path / chord / bursts live inside the seq block.
        seq = next(s for s in stmts if isinstance(s, Block) and s.kind == "seq")
        nested = []
        for s in seq.body:
            if isinstance(s, Call):
                nested.append(s.name)
            elif isinstance(s, Block):
                nested.append(s.kind)
        self.assertIn("path", nested)
        self.assertIn("chord", nested)
        self.assertIn("pan_burst", nested)
        self.assertIn("wheel_burst", nested)


if __name__ == "__main__":
    unittest.main()
