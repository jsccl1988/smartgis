# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""DebugAgent ``ui.*`` RPC helpers (Views automation surface)."""

from __future__ import annotations

from typing import Any, Optional

from smartgis._session import require_client


def find(name: str) -> dict[str, Any]:
    return require_client().call("ui.find", {"name": name})


def click(x: int, y: int, button: int = 1) -> dict[str, Any]:
    return require_client().call("ui.click", {"x": x, "y": y, "button": button})


def type_text(text: str) -> dict[str, Any]:
    return require_client().call("ui.type", {"text": text})


def dump_tree() -> dict[str, Any]:
    return require_client().call("ui.dump_tree")


def overlay_stats() -> dict[str, Any]:
    return require_client().call("ui.overlay_stats")


def capture_shell(path: str = "") -> dict[str, Any]:
    return require_client().call("ui.capture_shell", {"path": path})


def text_of(result: dict[str, Any]) -> Optional[str]:
    """Extract the ``text`` field commonly returned by ui.* methods."""
    value = result.get("text")
    return value if isinstance(value, str) else None
