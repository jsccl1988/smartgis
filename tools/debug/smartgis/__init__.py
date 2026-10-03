# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Product-facing helpers that call DebugAgent methods via AgentClient."""

from __future__ import annotations

from pathlib import Path
from typing import Any, Mapping, Optional

from smartgis import agent_ui
from smartgis._session import (
    bind_client,
    current_client,
    require_client,
    unbind_client,
)
from smartgis.client import AgentClient, AgentError
from smartgis.discovery import (
    agent_url_from_discovery,
    connect_from_discovery,
    discovery_path,
    load_discovery,
)

__all__ = [
    "AgentClient",
    "AgentError",
    "agent_ui",
    "agent_url_from_discovery",
    "bind_client",
    "cmd_exec",
    "connect_from_discovery",
    "current_client",
    "diag_pack",
    "discovery_path",
    "load_discovery",
    "log_set_level",
    "log_subscribe",
    "log_tail",
    "ping",
    "reconnect_from_discovery",
    "record_clear",
    "record_enable",
    "record_poll",
    "rpc_confirm",
    "rpc_methods",
    "script_run",
    "sdbd_capabilities",
    "sdbd_collections",
    "sdbd_query",
    "unbind_client",
    "ui_capture_shell",
    "ui_click",
    "ui_dump_tree",
    "ui_find",
    "ui_overlay_stats",
    "ui_type",
]


def ping() -> dict[str, Any]:
    return require_client().call("ping")


def log_tail(n: int = 200) -> dict[str, Any]:
    return require_client().call("log.tail", {"n": n})


def log_set_level(level: str) -> dict[str, Any]:
    return require_client().call("log.set_level", {"level": level})


def log_subscribe() -> dict[str, Any]:
    """Enable Agent push of ``{"event":"log",...}`` on this connection."""
    return require_client().call("log.subscribe")


def cmd_exec(line: str) -> dict[str, Any]:
    return require_client().call("cmd.exec", {"line": line})


def sdbd_capabilities() -> dict[str, Any]:
    return require_client().call("sdbd.capabilities")


def sdbd_collections() -> dict[str, Any]:
    return require_client().call("sdbd.collections")


def sdbd_query(body: str | Mapping[str, Any]) -> dict[str, Any]:
    if isinstance(body, Mapping):
        import json

        body = json.dumps(body)
    return require_client().call("sdbd.query", {"body": body})


def ui_find(name: str) -> dict[str, Any]:
    return agent_ui.find(name)


def ui_click(x: int, y: int, button: int = 1) -> dict[str, Any]:
    return agent_ui.click(x, y, button)


def ui_type(text: str) -> dict[str, Any]:
    return agent_ui.type_text(text)


def ui_dump_tree() -> dict[str, Any]:
    return agent_ui.dump_tree()


def ui_overlay_stats() -> dict[str, Any]:
    return agent_ui.overlay_stats()


def ui_capture_shell(path: str = "") -> dict[str, Any]:
    return agent_ui.capture_shell(path)


def script_run(path: str) -> dict[str, Any]:
    return require_client().call("script.run", {"path": path})


def record_enable(on: bool = True) -> dict[str, Any]:
    return require_client().call("record.enable", {"on": on})


def record_poll() -> dict[str, Any]:
    return require_client().call("record.poll")


def record_clear() -> dict[str, Any]:
    return require_client().call("record.clear")


def diag_pack(*, capture: bool = False, log_n: int = 100) -> dict[str, Any]:
    return require_client().call(
        "diag.pack", {"capture": capture, "log_n": log_n}
    )


def rpc_methods() -> dict[str, Any]:
    return require_client().call("rpc.methods")


def rpc_confirm() -> dict[str, Any]:
    return require_client().call("rpc.confirm")


def reconnect_from_discovery(
    path: Optional[Path] = None,
    *,
    timeout: float = 10.0,
) -> AgentClient:
    """Close the bound client (if any) and reconnect via discovery file."""
    old = current_client()
    if old is not None:
        try:
            old.close()
        except OSError:
            pass
        unbind_client()
    client = connect_from_discovery(path, timeout=timeout)
    bind_client(client)
    return client
