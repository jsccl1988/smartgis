# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Unit tests for IL compaction (no HWND / hooks required)."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[2]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.record.il_compact import compact_events, emit_il, events_to_il  # noqa: E402


class IlCompactTest(unittest.TestCase):
    def test_click_and_key(self) -> None:
        events = [
            {"t_ms": 0, "src": "os", "kind": "mouse", "action": "down", "button": "left", "x": 10, "y": 20},
            {"t_ms": 30, "src": "os", "kind": "mouse", "action": "up", "button": "left", "x": 10, "y": 20},
            {"t_ms": 200, "src": "os", "kind": "key", "action": "down", "vk": 27, "name": "ESCAPE"},
            {"t_ms": 220, "src": "os", "kind": "key", "action": "up", "vk": 27, "name": "ESCAPE"},
        ]
        text = events_to_il(events, script_name="t")
        self.assertIn('script "t"', text)
        self.assertIn("click(shell, 10, 20)", text)
        self.assertIn("key(ESCAPE)", text)
        self.assertIn("seq @os", text)

    def test_path_and_pan_burst(self) -> None:
        events = []
        t = 0
        for i in range(4):
            x0, y0 = 100 + i * 5, 200
            x1, y1 = x0 + 40, y0 + 20
            events.append(
                {"t_ms": t, "src": "os", "kind": "mouse", "action": "down", "button": "left", "x": x0, "y": y0}
            )
            t += 20
            events.append(
                {"t_ms": t, "src": "os", "kind": "mouse", "action": "move", "button": "left", "x": x1, "y": y1}
            )
            t += 20
            events.append(
                {"t_ms": t, "src": "os", "kind": "mouse", "action": "up", "button": "left", "x": x1, "y": y1}
            )
            t += 50
        ops = compact_events(events)
        self.assertTrue(any(o.get("op") == "pan_burst" for o in ops), ops)
        text = emit_il(ops)
        self.assertIn("pan_burst(", text)

    def test_wheel_burst(self) -> None:
        events = [
            {"t_ms": i * 40, "src": "os", "kind": "mouse", "action": "wheel", "x": 250, "y": 210, "delta": -120}
            for i in range(5)
        ]
        text = events_to_il(events)
        self.assertIn("wheel_burst(", text)

    def test_agent_select_map_tab(self) -> None:
        events = [
            {"t_ms": 10, "src": "os", "kind": "mouse", "action": "down", "button": "left", "x": 1, "y": 1},
            {"t_ms": 20, "src": "os", "kind": "mouse", "action": "up", "button": "left", "x": 1, "y": 1},
            {"t_ms": 50, "src": "agent", "kind": "select_map_tab", "index": 2},
        ]
        text = events_to_il(events)
        self.assertIn("select_map_tab(2) @inproc", text)


if __name__ == "__main__":
    unittest.main()
