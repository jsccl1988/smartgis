#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Flatten gis::scene / gis::model nested namespaces into gis::."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def collect(dir_name: str) -> list[Path]:
    d = ROOT / "src/gis" / dir_name
    out: list[Path] = []
    for pat in ("*.h", "*.cc", "*.cpp"):
        out.extend(sorted(d.glob(pat)))
    return out


TARGETS = collect("world") + collect("assets")


def flatten(text: str, nested: str) -> str:
    # Remove opening "namespace nested {" that follows namespace gis.
    text = re.sub(
        rf"(namespace gis \{{)\s*\nnamespace {nested} \{{",
        r"\1",
        text,
        count=1,
    )
    # Also handle when nested opens without being immediately after gis open
    # (e.g. forward decls then nested).
    text = re.sub(
        rf"\nnamespace {nested} \{{(\r?\n)",
        r"\1",
        text,
    )
    text = text.replace(f"}}  // namespace {nested}\n", "")
    text = text.replace(f"}} // namespace {nested}\n", "")
    # Qualified names that still mention the nested segment after bulk rewrite.
    text = text.replace(f"gis::{nested}::", "gis::")
    return text


def main() -> None:
    for p in TARGETS:
        if not p.is_file():
            continue
        orig = p.read_text(encoding="utf-8")
        text = orig
        if "world" in p.parts:
            text = flatten(text, "scene")
        if "assets" in p.parts:
            text = flatten(text, "model")
        # Include guards / comments.
        text = text.replace("SDB_SCENE_", "GIS_WORLD_")
        text = text.replace("SDB_MODEL_", "GIS_ASSETS_")
        text = text.replace("sdb/model/", "gis/assets/")
        text = text.replace("sdb/scene/", "gis/world/")
        if text != orig:
            p.write_text(text, encoding="utf-8", newline="\n")
            print("updated", p.relative_to(ROOT).as_posix())


if __name__ == "__main__":
    main()
