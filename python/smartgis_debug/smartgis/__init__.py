# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Product-facing helpers that call DebugAgent methods via AgentClient."""

from __future__ import annotations

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

_client: Optional[AgentClient] = None


def bind_client(client: AgentClient) -> None:
    """Set the process-wide AgentClient used by helper functions."""
    global _client
    _client = client


def _require_client() -> AgentClient:
    if _client is None:
        raise AgentError("no AgentClient bound; call smartgis.bind_client first")
    return _client


def ping() -> dict[str, Any]:
    return _require_client().call("ping")


def log_tail(n: int = 200) -> dict[str, Any]:
    return _require_client().call("log.tail", {"n": n})


def log_set_level(level: str) -> dict[str, Any]:
    return _require_client().call("log.set_level", {"level": level})


def cmd_exec(line: str) -> dict[str, Any]:
    return _require_client().call("cmd.exec", {"line": line})


def sdbd_capabilities() -> dict[str, Any]:
    return _require_client().call("sdbd.capabilities")


def sdbd_collections() -> dict[str, Any]:
    return _require_client().call("sdbd.collections")


def sdbd_query(body: str | Mapping[str, Any]) -> dict[str, Any]:
    if isinstance(body, Mapping):
        import json

        body = json.dumps(body)
    return _require_client().call("sdbd.query", {"body": body})
