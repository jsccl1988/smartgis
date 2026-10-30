# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Live sweep: Plugins catalog via DebugAgent :plugin sweep (UI thread).

Requires SmartGIS with debug console (SG_DEBUG=1 or --debug-console).
Equivalent to opening Plugins and running each listed plugin's primary tool.
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

_PACKAGE_ROOT = Path(__file__).resolve().parents[1]
_PYTHON_ROOT = _PACKAGE_ROOT.parent
for path in (str(_PYTHON_ROOT), str(_PACKAGE_ROOT)):
    if path not in sys.path:
        sys.path.insert(0, path)

from smartgis import (  # noqa: E402
    cmd_exec,
    connect_from_discovery,
    log_set_level,
    log_tail,
    rpc_confirm,
)
from smartgis.agent_ui import text_of  # noqa: E402


def _wait_discovery(timeout_s: float = 120.0) -> None:
    import os

    path = Path(os.environ.get("TEMP", ".")) / "smartgis-debug.json"
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if path.is_file():
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
                if data.get("port"):
                    return
            except (json.JSONDecodeError, OSError):
                pass
        time.sleep(0.25)
    raise SystemExit(f"discovery missing after {timeout_s}s: {path}")


def _cmd_output(result: dict) -> str:
    text = text_of(result)
    if text is not None:
        return text
    out = result.get("output")
    if isinstance(out, str):
        return out
    return json.dumps(result, ensure_ascii=False)


def main() -> int:
    _wait_discovery()
    with connect_from_discovery() as client:
        from smartgis import bind_client

        bind_client(client)
        rpc_confirm()
        log_set_level("debug")
        print("agent connected")
        opened = _cmd_output(cmd_exec(":ui panel plugins"))
        print("panel:", opened)
        if "opened:plugins" not in opened:
            print("warn: plugins panel:", opened)
        for _ in range(90):
            tail = log_tail(60)
            blob = json.dumps(tail, ensure_ascii=False)
            if "WaitFirstMapPresent" in blob or "map2d_last_gpu_present_ok" in blob:
                break
            time.sleep(1.0)
        report = _cmd_output(cmd_exec(":plugin sweep"))
        print(report)
        if "fail=" not in report:
            return 1
        # Trailing summary line: sweep ok=N fail=M rows=R
        summary = report.strip().splitlines()[-1]
        parts = {p.split("=")[0]: p.split("=")[1] for p in summary.split() if "=" in p}
        fail = int(parts.get("fail", "1"))
        if fail > 0:
            return 1
        print("plugin_catalog_sweep: ok")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
