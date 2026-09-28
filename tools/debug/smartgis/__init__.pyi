# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Type stubs for the smartgis Agent RPC helpers (edit-time / Pyright only)."""

from pathlib import Path
from typing import Any, Mapping, Optional

from smartgis import agent_ui
from smartgis.client import AgentClient, AgentError

__all__ = [
    "AgentClient",
    "AgentError",
    "agent_ui",
    "agent_url_from_discovery",
    "bind_client",
    "cmd_exec",
    "connect_from_discovery",
    "current_client",
    "discovery_path",
    "load_discovery",
    "log_set_level",
    "log_subscribe",
    "log_tail",
    "ping",
    "reconnect_from_discovery",
    "sdbd_capabilities",
    "sdbd_collections",
    "sdbd_query",
    "unbind_client",
    "ui_click",
    "ui_dump_tree",
    "ui_find",
    "ui_overlay_stats",
    "ui_type",
]

def bind_client(client: AgentClient) -> None: ...
def unbind_client() -> None: ...
def current_client() -> Optional[AgentClient]: ...
def ping() -> dict[str, Any]: ...
def log_tail(n: int = ...) -> dict[str, Any]: ...
def log_set_level(level: str) -> dict[str, Any]: ...
def log_subscribe() -> dict[str, Any]: ...
def cmd_exec(line: str) -> dict[str, Any]: ...
def sdbd_capabilities() -> dict[str, Any]: ...
def sdbd_collections() -> dict[str, Any]: ...
def sdbd_query(body: str | Mapping[str, Any]) -> dict[str, Any]: ...
def ui_find(name: str) -> dict[str, Any]: ...
def ui_click(x: int, y: int, button: int = ...) -> dict[str, Any]: ...
def ui_type(text: str) -> dict[str, Any]: ...
def ui_dump_tree() -> dict[str, Any]: ...
def ui_overlay_stats() -> dict[str, Any]: ...
def discovery_path() -> Path: ...
def load_discovery(path: Optional[Path] = ...) -> dict[str, Any]: ...
def agent_url_from_discovery(
    data: Optional[Mapping[str, Any]] = ...,
    *,
    path: Optional[Path] = ...,
) -> str: ...
def connect_from_discovery(
    path: Optional[Path] = ...,
    *,
    timeout: float = ...,
) -> AgentClient: ...
def reconnect_from_discovery(
    path: Optional[Path] = ...,
    *,
    timeout: float = ...,
) -> AgentClient: ...
