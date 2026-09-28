# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Smoke-test DebugAgent ui.* RPC against a running SmartGIS process.

For deeper visual forensics (analyze / record-all), see ui_visual_forensics.py.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

_PACKAGE_ROOT = Path(__file__).resolve().parents[1]
_PYTHON_ROOT = _PACKAGE_ROOT.parent
for path in (str(_PYTHON_ROOT), str(_PACKAGE_ROOT)):
    if path not in sys.path:
        sys.path.insert(0, path)

from smartgis.client import AgentError  # noqa: E402
from smartgis.discovery import connect_from_discovery  # noqa: E402


def _call_ui(client: object, method: str, params: dict | None = None) -> str:
    try:
        result = client.call(method, params or {})  # type: ignore[attr-defined]
    except AgentError as exc:
        return f"error: {exc}"
    text = result.get("text")
    if isinstance(text, str):
        return text
    return json.dumps(result, ensure_ascii=False)


def main() -> int:
    with connect_from_discovery() as client:
        client.call("ping")
        print("ping ok")
        tree = _call_ui(client, "ui.dump_tree")
        print("ui.dump_tree:", tree[:200] + ("…" if len(tree) > 200 else ""))
        overlay = _call_ui(client, "ui.overlay_stats")
        print("ui.overlay_stats:", overlay)
        if overlay == "unavailable" or tree.startswith("error:"):
            print("note: tolerated unavailable / unbound ui hooks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
