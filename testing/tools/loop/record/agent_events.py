# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Poll DebugAgent record.poll for semantic events during IL recording."""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path
from typing import Any

# Prefer tools/debug on path for AgentClient.
_DEBUG_ROOT = Path(__file__).resolve().parents[4] / "tools" / "debug"
if _DEBUG_ROOT.is_dir() and str(_DEBUG_ROOT) not in sys.path:
    sys.path.insert(0, str(_DEBUG_ROOT))


def try_connect_agent(timeout: float = 2.0) -> Any | None:
    try:
        from smartgis.discovery import connect_from_discovery  # type: ignore
    except ImportError:
        return None
    try:
        return connect_from_discovery(timeout=timeout)
    except Exception:
        return None


def poll_agent_events(client: Any, *, t0: float) -> list[dict[str, Any]]:
    """Call record.poll; map results to recorder event dicts with t_ms."""
    if client is None:
        return []
    try:
        result = client.call("record.poll", {})
    except Exception:
        return []
    if not isinstance(result, dict):
        return []
    raw = result.get("events")
    if not isinstance(raw, list):
        return []
    out: list[dict[str, Any]] = []
    now_ms = int((time.perf_counter() - t0) * 1000)
    for item in raw:
        if not isinstance(item, dict):
            continue
        ev = dict(item)
        ev.setdefault("src", "agent")
        # Agent may stamp t_ms relative to its own clock; prefer provided.
        if "t_ms" not in ev:
            ev["t_ms"] = now_ms
        out.append(ev)
    return out


def enable_agent_record(client: Any) -> bool:
    if client is None:
        return False
    try:
        client.call("record.enable", {"on": True})
        return True
    except Exception:
        return False


def disable_agent_record(client: Any) -> None:
    if client is None:
        return
    try:
        client.call("record.enable", {"on": False})
    except Exception:
        pass


def format_agent_event_line(ev: dict[str, Any]) -> str:
    return json.dumps(ev, ensure_ascii=False, separators=(",", ":"))
