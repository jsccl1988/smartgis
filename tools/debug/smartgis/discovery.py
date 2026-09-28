# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Locate the DebugAgent endpoint via the discovery file."""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any, Mapping, Optional

from smartgis.client import AgentClient, AgentError


def discovery_path() -> Path:
    """Return ``%TEMP%/smartgis-debug.json`` (or cwd fallback)."""
    temp = os.environ.get("TEMP") or os.environ.get("TMP") or os.environ.get("TMPDIR")
    if temp:
        return Path(temp) / "smartgis-debug.json"
    return Path.cwd() / "smartgis-debug.json"


def load_discovery(path: Optional[Path] = None) -> dict[str, Any]:
    """Load and validate the Agent discovery JSON object."""
    target = path if path is not None else discovery_path()
    if not target.is_file():
        raise AgentError(f"discovery file missing: {target}")
    try:
        data = json.loads(target.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise AgentError(f"cannot read discovery file: {exc}") from exc
    if not isinstance(data, dict):
        raise AgentError("discovery file must contain a JSON object")
    return data


def agent_url_from_discovery(
    data: Optional[Mapping[str, Any]] = None,
    *,
    path: Optional[Path] = None,
) -> str:
    """Build ``tcp://host:port`` from discovery data or file."""
    payload = dict(data) if data is not None else load_discovery(path)
    host = str(payload.get("host") or "127.0.0.1")
    if "port" not in payload:
        raise AgentError("discovery missing port")
    try:
        port = int(payload["port"])
    except (TypeError, ValueError) as exc:
        raise AgentError(f"discovery bad port: {payload.get('port')!r}") from exc
    if port <= 0 or port > 65535:
        raise AgentError(f"discovery port out of range: {port}")
    return f"tcp://{host}:{port}"


def connect_from_discovery(
    path: Optional[Path] = None,
    *,
    timeout: float = 10.0,
) -> AgentClient:
    """Connect using the discovery file written by DebugAgent."""
    url = agent_url_from_discovery(path=path)
    return AgentClient.connect(url, timeout=timeout)
