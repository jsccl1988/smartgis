# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Tests for smartgis helper wrappers against a mock Agent."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
_PYTHON = _ROOT.parent
for path in (str(_PYTHON), str(_ROOT)):
    if path not in sys.path:
        sys.path.insert(0, path)

import smartgis  # noqa: E402
from smartgis.client import AgentClient, AgentError  # noqa: E402
from tests.mock_agent import MockAgentServer, MockStep  # noqa: E402


class HelperTests(unittest.TestCase):
    def tearDown(self) -> None:
        smartgis.unbind_client()

    def test_require_client_without_bind(self) -> None:
        with self.assertRaisesRegex(AgentError, "no AgentClient"):
            smartgis.ping()

    def test_ping_log_cmd_sdbd_ui(self) -> None:
        steps = [
            MockStep(expect_method="ping", response={"ok": True, "result": {"pong": True}}),
            MockStep(
                expect_method="log.tail",
                response={"ok": True, "result": {"entries": []}},
            ),
            MockStep(
                expect_method="log.set_level",
                response={"ok": True, "result": {}},
            ),
            MockStep(
                expect_method="log.subscribe",
                response={"ok": True, "result": {}},
            ),
            MockStep(
                expect_method="cmd.exec",
                response={"ok": True, "result": {"output": "ok"}},
            ),
            MockStep(
                expect_method="sdbd.capabilities",
                response={"ok": True, "result": {"ok": True, "status": 200, "body": "{}"}},
            ),
            MockStep(
                expect_method="sdbd.collections",
                response={"ok": True, "result": {"ok": True}},
            ),
            MockStep(
                expect_method="sdbd.query",
                response={"ok": True, "result": {"ok": True, "body": "[]"}},
            ),
            MockStep(
                expect_method="ui.find",
                response={"ok": True, "result": {"text": "Label"}},
            ),
            MockStep(
                expect_method="ui.click",
                response={"ok": True, "result": {"text": "clicked"}},
            ),
            MockStep(
                expect_method="ui.type",
                response={"ok": True, "result": {"text": "typed"}},
            ),
            MockStep(
                expect_method="ui.dump_tree",
                response={"ok": True, "result": {"text": "root"}},
            ),
            MockStep(
                expect_method="ui.overlay_stats",
                response={"ok": True, "result": {"text": "unavailable"}},
            ),
        ]
        with MockAgentServer(steps) as server:
            client = AgentClient.connect(server.url)
            smartgis.bind_client(client)
            try:
                self.assertEqual(smartgis.ping(), {"pong": True})
                self.assertEqual(smartgis.log_tail(10), {"entries": []})
                self.assertEqual(smartgis.log_set_level("DEBUG"), {})
                self.assertEqual(smartgis.log_subscribe(), {})
                self.assertEqual(smartgis.cmd_exec(":help"), {"output": "ok"})
                self.assertIn("status", smartgis.sdbd_capabilities())
                smartgis.sdbd_collections()
                smartgis.sdbd_query({"sql": "select 1"})
                self.assertEqual(smartgis.ui_find("btn"), {"text": "Label"})
                self.assertEqual(smartgis.ui_click(1, 2), {"text": "clicked"})
                self.assertEqual(smartgis.ui_type("hi"), {"text": "typed"})
                self.assertEqual(smartgis.ui_dump_tree(), {"text": "root"})
                self.assertEqual(smartgis.ui_overlay_stats(), {"text": "unavailable"})
                self.assertEqual(
                    smartgis.agent_ui.text_of({"text": "Label"}), "Label"
                )
            finally:
                client.close()

        methods = [r["method"] for r in server.requests]
        self.assertEqual(
            methods,
            [
                "ping",
                "log.tail",
                "log.set_level",
                "log.subscribe",
                "cmd.exec",
                "sdbd.capabilities",
                "sdbd.collections",
                "sdbd.query",
                "ui.find",
                "ui.click",
                "ui.type",
                "ui.dump_tree",
                "ui.overlay_stats",
            ],
        )
        query_req = server.requests[7]
        body = query_req["params"]["body"]
        self.assertEqual(json.loads(body), {"sql": "select 1"})


if __name__ == "__main__":
    unittest.main()
