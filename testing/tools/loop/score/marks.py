# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

from __future__ import annotations

from pathlib import Path


def read_mark_text(path: Path | None) -> str:
    if path is None or not path.exists():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def score_marks(rc: int, mark_text: str, required: tuple[str, ...]) -> dict:
    marks = {line.strip() for line in mark_text.splitlines() if line.strip()}
    missing = [m for m in required if m not in marks]
    ok = rc == 0 and not missing
    return {
        "ok": ok,
        "showcase_rc": rc,
        "marks": sorted(marks),
        "missing": missing,
        "gates": {
            "exit==0": rc == 0,
            **{f"mark:{m}": m in marks for m in required},
        },
    }
