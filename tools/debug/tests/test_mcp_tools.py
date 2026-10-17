# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Unit tests for debug.mcp tool dispatch (mock Agent, no live product)."""

from __future__ import annotations

import io
import json
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
_TOOLS = _ROOT.parent
for path in (str(_TOOLS), str(_ROOT)):
    if path not in sys.path:
        sys.path.insert(0, path)

from debug.mcp.server import handle_request, run_stdio  # noqa: E402
from debug.mcp.tools import DANGEROUS_TOOLS, McpToolSession, TOOL_SPECS  # noqa: E402
from smartgis import bind_client, unbind_client  # noqa: E402
from smartgis.client import AgentClient  # noqa: E402
from tests.mock_agent import MockAgentServer, MockStep  # noqa: E402


def _frame(obj: dict) -> str:
    raw = json.dumps(obj, separators=(",", ":")).encode("utf-8")
    return f"Content-Length: {len(raw)}\r\n\r\n{raw.decode('utf-8')}"


def test_tool_specs_cover_core_and_mark_dangerous() -> None:
    names = {t["name"] for t in TOOL_SPECS}
    assert "sg_ping" in names
    assert "sg_cmd_exec" in names
    assert "sg_confirm" in names
    assert "sg_ui_click" in DANGEROUS_TOOLS


def test_handle_initialize_and_tools_list() -> None:
    session = McpToolSession()
    init = handle_request(
        {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {},
        },
        session,
    )
    assert init is not None
    assert init["result"]["serverInfo"]["name"] == "smartgis-debug-mcp"
    listed = handle_request(
        {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}},
        session,
    )
    assert listed is not None
    assert len(listed["result"]["tools"]) == len(TOOL_SPECS)


def test_tools_call_ping_via_mock_agent() -> None:
    with MockAgentServer(
        [MockStep(expect_method="ping", response={"ok": True, "result": {"pong": True}})]
    ) as agent:
        client = AgentClient.connect(agent.url, timeout=2.0)
        bind_client(client)
        try:
            session = McpToolSession()
            session._client = client
            reply = handle_request(
                {
                    "jsonrpc": "2.0",
                    "id": 3,
                    "method": "tools/call",
                    "params": {"name": "sg_ping", "arguments": {}},
                },
                session,
            )
            assert reply is not None
            text = reply["result"]["content"][0]["text"]
            payload = json.loads(text)
            assert payload["ok"] is True
            assert payload["result"].get("pong") is True
        finally:
            unbind_client()
            client.close()


def test_dangerous_tool_blocked_without_confirm() -> None:
    session = McpToolSession()
    session._confirmed = False
    reply = handle_request(
        {
            "jsonrpc": "2.0",
            "id": 4,
            "method": "tools/call",
            "params": {
                "name": "sg_ui_click",
                "arguments": {"x": 1, "y": 2},
            },
        },
        session,
    )
    assert reply is not None
    assert reply["result"]["isError"] is True
    text = reply["result"]["content"][0]["text"]
    assert "sg_confirm" in text


def test_stdio_roundtrip_initialize() -> None:
    stdin = io.StringIO(
        _frame(
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {"protocolVersion": "2024-11-05", "capabilities": {}},
            }
        )
    )
    stdout = io.StringIO()
    run_stdio(stdin=stdin, stdout=stdout)
    out = stdout.getvalue()
    assert "Content-Length:" in out
    assert "smartgis-debug-mcp" in out
