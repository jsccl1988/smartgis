# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Tests for discovery file helpers."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
_PYTHON = _ROOT.parent
for path in (str(_PYTHON), str(_ROOT)):
    if path not in sys.path:
        sys.path.insert(0, path)

from smartgis.client import AgentError  # noqa: E402
from smartgis.discovery import (  # noqa: E402
    agent_url_from_discovery,
    discovery_path,
    load_discovery,
)


class DiscoveryTests(unittest.TestCase):
    def test_discovery_path_uses_temp(self) -> None:
        path = discovery_path()
        self.assertTrue(path.name == "smartgis-debug.json")

    def test_load_and_url(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "smartgis-debug.json"
            path.write_text(
                json.dumps({"host": "127.0.0.1", "port": 4242, "pid": 99}),
                encoding="utf-8",
            )
            data = load_discovery(path)
            self.assertEqual(data["port"], 4242)
            self.assertEqual(
                agent_url_from_discovery(path=path), "tcp://127.0.0.1:4242"
            )
            self.assertEqual(
                agent_url_from_discovery(data), "tcp://127.0.0.1:4242"
            )

    def test_missing_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(AgentError):
                load_discovery(Path(tmp) / "missing.json")

    def test_bad_port(self) -> None:
        with self.assertRaises(AgentError):
            agent_url_from_discovery({"host": "127.0.0.1"})
        with self.assertRaises(AgentError):
            agent_url_from_discovery({"host": "127.0.0.1", "port": -1})
        with self.assertRaises(AgentError):
            agent_url_from_discovery({"host": "127.0.0.1", "port": "x"})

    def test_malformed_json(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "bad.json"
            path.write_text("{not-json", encoding="utf-8")
            with self.assertRaises(AgentError):
                load_discovery(path)


if __name__ == "__main__":
    unittest.main()
