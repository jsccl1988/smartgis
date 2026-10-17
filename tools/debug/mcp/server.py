# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Minimal MCP stdio JSON-RPC server (Content-Length framing)."""

from __future__ import annotations

import json
import sys
from typing import Any, Optional, TextIO

from debug.mcp.tools import TOOL_SPECS, McpToolSession, tool_result_text


def _read_message(stdin: TextIO) -> Optional[dict[str, Any]]:
    headers: dict[str, str] = {}
    while True:
        line = stdin.readline()
        if line == "":
            return None
        line = line.rstrip("\r\n")
        if line == "":
            break
        if ":" in line:
            key, value = line.split(":", 1)
            headers[key.strip().lower()] = value.strip()
    length_s = headers.get("content-length")
    if not length_s:
        return None
    try:
        length = int(length_s)
    except ValueError:
        return None
    body = stdin.read(length)
    if not body:
        return None
    try:
        obj = json.loads(body)
    except json.JSONDecodeError:
        return None
    if not isinstance(obj, dict):
        return None
    return obj


def _write_message(stdout: TextIO, message: dict[str, Any]) -> None:
    raw = json.dumps(message, ensure_ascii=False, separators=(",", ":"))
    data = raw.encode("utf-8")
    stdout.write(f"Content-Length: {len(data)}\r\n\r\n")
    stdout.write(data.decode("utf-8"))
    stdout.flush()


def _ok(id_value: Any, result: Any) -> dict[str, Any]:
    return {"jsonrpc": "2.0", "id": id_value, "result": result}


def _err(id_value: Any, code: int, message: str) -> dict[str, Any]:
    return {
        "jsonrpc": "2.0",
        "id": id_value,
        "error": {"code": code, "message": message},
    }


def handle_request(
    msg: dict[str, Any], session: McpToolSession
) -> Optional[dict[str, Any]]:
    """Handle one JSON-RPC request; return None for notifications."""
    method = msg.get("method")
    req_id = msg.get("id")
    params = msg.get("params") or {}
    if not isinstance(params, dict):
        params = {}

    if method == "initialize":
        return _ok(
            req_id,
            {
                "protocolVersion": "2024-11-05",
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "smartgis-debug-mcp", "version": "0.1.0"},
            },
        )
    if method == "notifications/initialized" or method == "initialized":
        return None
    if method == "ping":
        return _ok(req_id, {})
    if method == "tools/list":
        return _ok(req_id, {"tools": TOOL_SPECS})
    if method == "tools/call":
        name = str(params.get("name") or "")
        arguments = params.get("arguments") or {}
        if not isinstance(arguments, dict):
            arguments = {}
        payload = session.call_tool(name, arguments)
        text = tool_result_text(payload)
        is_error = not payload.get("ok", False)
        return _ok(
            req_id,
            {
                "content": [{"type": "text", "text": text}],
                "isError": is_error,
            },
        )
    if method == "shutdown":
        session.close()
        return _ok(req_id, {})
    if req_id is None:
        return None
    return _err(req_id, -32601, f"method not found: {method}")


def run_stdio(
    stdin: Optional[TextIO] = None,
    stdout: Optional[TextIO] = None,
) -> None:
    """Serve until stdin EOF."""
    in_stream = stdin if stdin is not None else sys.stdin
    out_stream = stdout if stdout is not None else sys.stdout
    session = McpToolSession()
    try:
        while True:
            msg = _read_message(in_stream)
            if msg is None:
                break
            reply = handle_request(msg, session)
            if reply is not None:
                _write_message(out_stream, reply)
    finally:
        session.close()
