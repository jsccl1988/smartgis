# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Tool definitions and dispatch onto smartgis AgentClient helpers."""

from __future__ import annotations

import json
from typing import Any, Callable, Optional

from smartgis import (
    AgentClient,
    AgentError,
    bind_client,
    cmd_exec,
    connect_from_discovery,
    diag_pack,
    log_set_level,
    log_tail,
    ping,
    rpc_confirm,
    rpc_methods,
    ui_capture_shell,
    ui_click,
    ui_dump_tree,
    ui_find,
    unbind_client,
)

DANGEROUS_TOOLS = frozenset(
    {
        "sg_ui_click",
        "sg_ui_capture",
        "sg_py_eval",
        "sg_sdbd_query",
    }
)

TOOL_SPECS: list[dict[str, Any]] = [
    {
        "name": "sg_ping",
        "description": "Ping SmartGIS DebugAgent (loopback).",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "sg_rpc_methods",
        "description": "Fetch runtime rpc.methods schema from DebugAgent.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "sg_cmd_exec",
        "description": "Run a Debug Console line (must start with ':').",
        "inputSchema": {
            "type": "object",
            "properties": {"line": {"type": "string"}},
            "required": ["line"],
        },
    },
    {
        "name": "sg_log_tail",
        "description": "Fetch recent LogSink entries.",
        "inputSchema": {
            "type": "object",
            "properties": {"n": {"type": "integer", "default": 100}},
        },
    },
    {
        "name": "sg_log_set_level",
        "description": "Set Agent log minimum level.",
        "inputSchema": {
            "type": "object",
            "properties": {"level": {"type": "string"}},
            "required": ["level"],
        },
    },
    {
        "name": "sg_diag_pack",
        "description": "Collect a diag.pack forensic bundle summary.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "capture": {"type": "boolean", "default": False},
                "log_n": {"type": "integer", "default": 100},
            },
        },
    },
    {
        "name": "sg_confirm",
        "description": "Unlock dangerous ops for this Agent session (rpc.confirm).",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "sg_ui_find",
        "description": "Find a Views control by name (ui.find).",
        "inputSchema": {
            "type": "object",
            "properties": {"name": {"type": "string"}},
            "required": ["name"],
        },
    },
    {
        "name": "sg_ui_tree",
        "description": "Dump Views UI tree (ui.dump_tree).",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "sg_ui_click",
        "description": "Click at client coords (dangerous; needs sg_confirm).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "x": {"type": "integer"},
                "y": {"type": "integer"},
                "button": {"type": "integer", "default": 1},
            },
            "required": ["x", "y"],
        },
    },
    {
        "name": "sg_ui_capture",
        "description": "Capture shell bitmap (dangerous; needs sg_confirm).",
        "inputSchema": {
            "type": "object",
            "properties": {"path": {"type": "string"}},
        },
    },
]


class McpToolSession:
    """Holds discovery-bound client + confirm latch for dangerous tools."""

    def __init__(self) -> None:
        self._client: Optional[AgentClient] = None
        self._confirmed = False

    def ensure_client(self) -> AgentClient:
        if self._client is not None:
            return self._client
        client = connect_from_discovery(timeout=5.0)
        bind_client(client)
        self._client = client
        return client

    def close(self) -> None:
        if self._client is not None:
            try:
                self._client.close()
            except OSError:
                pass
            unbind_client()
            self._client = None
        self._confirmed = False

    def call_tool(self, name: str, arguments: dict[str, Any] | None) -> dict[str, Any]:
        args = dict(arguments or {})
        if name in DANGEROUS_TOOLS and not self._confirmed:
            return {
                "ok": False,
                "error": (
                    f"{name} is dangerous; call sg_confirm first "
                    "(or set SG_DEBUG_ALLOW=1 on the product)."
                ),
            }
        self.ensure_client()
        handlers: dict[str, Callable[[], dict[str, Any]]] = {
            "sg_ping": lambda: ping(),
            "sg_rpc_methods": lambda: rpc_methods(),
            "sg_cmd_exec": lambda: _cmd_exec(args),
            "sg_log_tail": lambda: log_tail(int(args.get("n", 100))),
            "sg_log_set_level": lambda: log_set_level(str(args["level"])),
            "sg_diag_pack": lambda: diag_pack(
                capture=bool(args.get("capture", False)),
                log_n=int(args.get("log_n", 100)),
            ),
            "sg_confirm": self._do_confirm,
            "sg_ui_find": lambda: ui_find(str(args["name"])),
            "sg_ui_tree": lambda: ui_dump_tree(),
            "sg_ui_click": lambda: ui_click(
                int(args["x"]), int(args["y"]), int(args.get("button", 1))
            ),
            "sg_ui_capture": lambda: ui_capture_shell(str(args.get("path") or "")),
        }
        handler = handlers.get(name)
        if handler is None:
            return {"ok": False, "error": f"unknown tool: {name}"}
        try:
            result = handler()
        except AgentError as exc:
            return {"ok": False, "error": str(exc)}
        except (KeyError, TypeError, ValueError) as exc:
            return {"ok": False, "error": f"bad arguments: {exc}"}
        return {"ok": True, "result": result}

    def _do_confirm(self) -> dict[str, Any]:
        out = rpc_confirm()
        self._confirmed = True
        return out


def _cmd_exec(args: dict[str, Any]) -> dict[str, Any]:
    line = str(args.get("line") or "")
    if not line.startswith(":"):
        raise AgentError("sg_cmd_exec line must start with ':'")
    return cmd_exec(line)


def tool_result_text(payload: dict[str, Any]) -> str:
    return json.dumps(payload, ensure_ascii=False, indent=2)
