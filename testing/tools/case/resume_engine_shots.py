#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Resume engine shots; skip engines that already have ok=true reports."""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_engine_shots as m  # noqa: E402


def main() -> int:
    results = []
    for eng_id, label, cmd, src in m.ENGINES:
        rep = m.OUT / f"engine-{eng_id}-report.json"
        if rep.exists():
            prev = json.loads(rep.read_text(encoding="utf-8"))
            if prev.get("ok"):
                print(f"SKIP already PASS {eng_id}", flush=True)
                results.append(prev)
                continue
        results.append(m.run_one(eng_id, label, cmd, src))
    m.kill_apps()
    (m.OUT / "engine-shots-index.json").write_text(
        json.dumps(results, indent=2), encoding="utf-8"
    )
    print("======== SUMMARY ========", flush=True)
    for r in results:
        status = "PASS" if r.get("ok") else "FAIL"
        print(f"  [{status:4}] {r['engine_id']:28}  {r.get('tagged_bmp')}", flush=True)
    return 0 if all(r.get("ok") for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
