# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""afterShellExecution: scrub out/.build.lock.on if an agent created it."""

from __future__ import annotations

import json
import sys
from datetime import datetime
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def main() -> int:
    raw = sys.stdin.buffer.read()
    data: dict = {}
    if raw:
        for enc in ("utf-8-sig", "utf-8", "utf-16", "cp936"):
            try:
                data = json.loads(raw.decode(enc))
                if isinstance(data, dict):
                    break
            except Exception:
                data = {}

    root = _repo_root()
    sentinel = root / "out" / ".build.lock.on"
    removed = False
    if sentinel.is_file():
        try:
            sentinel.unlink()
            removed = True
        except OSError:
            removed = False

    try:
        log_dir = root / "out" / "scratch"
        log_dir.mkdir(parents=True, exist_ok=True)
        cmd = str(data.get("command") or "")[:200].replace("\n", " ")
        with (log_dir / "build_lock_gate.log").open("a", encoding="utf-8") as f:
            f.write(
                f"{datetime.now().isoformat(timespec='seconds')}\t"
                f"after_scrub removed={removed}\t{cmd}\n"
            )
    except Exception:
        pass

    print(json.dumps({}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
