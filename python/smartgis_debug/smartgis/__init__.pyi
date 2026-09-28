# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Type stubs for the smartgis Agent RPC helpers (edit-time / Pyright only)."""

from typing import Any, Mapping, Optional

from smartgis.client import AgentClient, AgentError

__all__ = [
    "AgentClient",
    "AgentError",
    "bind_client",
    "log_tail",
    "log_set_level",
    "cmd_exec",
    "sdbd_capabilities",
    "sdbd_collections",
    "sdbd_query",
    "ping",
]

def bind_client(client: AgentClient) -> None: ...
def ping() -> dict[str, Any]: ...
def log_tail(n: int = ...) -> dict[str, Any]: ...
def log_set_level(level: str) -> dict[str, Any]: ...
def cmd_exec(line: str) -> dict[str, Any]: ...
def sdbd_capabilities() -> dict[str, Any]: ...
def sdbd_collections() -> dict[str, Any]: ...
def sdbd_query(body: str | Mapping[str, Any]) -> dict[str, Any]: ...
