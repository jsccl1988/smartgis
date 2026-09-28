# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Process-wide AgentClient binding for helper modules."""

from __future__ import annotations

from typing import Optional

from smartgis.client import AgentClient, AgentError

_client: Optional[AgentClient] = None


def bind_client(client: AgentClient) -> None:
    """Set the process-wide AgentClient used by helper functions."""
    global _client
    _client = client


def unbind_client() -> None:
    """Clear the process-wide AgentClient (tests / reconnect)."""
    global _client
    _client = None


def require_client() -> AgentClient:
    if _client is None:
        raise AgentError("no AgentClient bound; call smartgis.bind_client first")
    return _client


def current_client() -> Optional[AgentClient]:
    return _client
