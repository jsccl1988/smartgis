#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
from pathlib import Path

p = Path("docs/build/abi-rename-map.md")
t = p.read_text(encoding="utf-8")
old = "| `gis` | `sdb` | 1 |\n| `sde_mgr` | `sdb` | 1 |\n| `sde_gdal` | `sdb` | 1 |"
new = (
    "| `gis` (cutover short name) | `gis` (layer stem; briefly merged as `sdb`) "
    "| 1 + 2026-09-19 layer rename |\n"
    "| `sde_mgr` | `gis` | 1 |\n"
    "| `sde_gdal` | `gis` | 1 |"
)
if old not in t:
    print("cutover table block not found")
else:
    t = t.replace(old, new)
    print("replaced cutover table")
t2 = t.replace("| GIS map/feature/layer | `sdb` |", "| GIS map/feature/layer | `gis` |")
if t2 == t:
    print("namespace row unchanged or already gis")
else:
    print("replaced namespace row")
    t = t2
p.write_text(t, encoding="utf-8", newline="\n")
print("done")
