# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""sessionStart: ensure compile lock sentinel is off; remind agents."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def main() -> int:
    try:
        json.load(sys.stdin)
    except Exception:
        pass

    root = _repo_root()
    sentinel = root / "out" / ".build.lock.on"
    if sentinel.is_file():
        try:
            sentinel.unlink()
        except OSError:
            pass

    msg = (
        "Compile lock must stay OFF. Do not create out/.build.lock.on or set "
        "SMARTGIS_BUILD_LOCK=1. Use build.bat directly; partition paths across "
        "agents. Hook: .cursor/hooks/build_lock_gate.py"
    )
    print(json.dumps({"additional_context": msg}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
