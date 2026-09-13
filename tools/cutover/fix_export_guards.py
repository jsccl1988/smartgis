#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Align header `defined(FOO_EXPORT)` with GN `FOO_EXPORTS`."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"

# Collect FOO_EXPORTS from BUILD.gn
gn_exports: set[str] = set()
for p in SRC.rglob("BUILD.gn"):
    text = p.read_text(encoding="utf-8", errors="replace")
    for m in re.finditer(r'"([A-Z0-9_]+_EXPORTS)"', text):
        gn_exports.add(m.group(1))

# Map FOO_EXPORT -> FOO_EXPORTS when GN has the latter
rewrite_names = {name[:-1]: name for name in gn_exports if name.endswith("EXPORTS")}

changed = 0
for p in list(SRC.rglob("*.h")) + list(SRC.rglob("*.hpp")):
    text = p.read_text(encoding="utf-8", errors="replace")
    new = text
    for old, new_name in rewrite_names.items():
        # defined(FOO_EXPORT) / !defined(FOO_EXPORT) but not FOO_EXPORTS / FOO_EXPORT_API
        new = re.sub(
            rf"\bdefined\({re.escape(old)}\)",
            f"defined({new_name})",
            new,
        )
        new = re.sub(
            rf"!defined\({re.escape(old)}\)",
            f"!defined({new_name})",
            new,
        )
    if new != text:
        p.write_text(new, encoding="utf-8", newline="\n")
        changed += 1
        print(p.relative_to(ROOT).as_posix())

print(f"files={changed} gn_exports={len(gn_exports)}")
