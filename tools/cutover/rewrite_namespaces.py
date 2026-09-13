#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Rewrite remaining Smt_* namespaces to two-level public names."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Longest / most specific first.
NAMESPACES = [
    ("Smt_SDEDevMgr", "sdb"),
    ("Smt_StaDiagram", "ui"),
    ("Smt_GroupTool", "tool"),
    ("Smt_XCatalog", "ui"),
    ("Smt_XAMBox", "ui"),
    ("Smt_3DModel", "render"),
    ("Smt_3DMath", "render"),
    ("Smt_3DBase", "render"),
    ("Smt_3DGeo", "geo"),
    ("Smt_SDEMem", "sdb"),
    ("Smt_SDESmf", "sdb"),
    ("Smt_SDEWS", "sdb"),
    ("Smt_IATool", "tool"),
    ("Smt_XView", "ui"),
    ("Smt_3Drd", "render"),
    ("Smt_Core", "base"),
    ("Smt_Base", "base"),
    ("Smt_Math", "geo"),
    ("Smt_Sys", "sys"),
    ("Smt_GIS", "sdb"),
    ("Smt_Geo", "geo"),
    ("Smt_Prj", "geo"),
    ("Smt_DEM", "plugin"),
    ("Smt_App", "app"),
    ("Smt_Net", "net"),
    ("Smt_AM", "plugin"),
    ("Smt_Rd", "render"),
    ("Smt_UI", "ui"),
    ("Smt_Tool", "tool"),
    ("Smt_Sta", "stat"),
]

SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".inl", ".inc"}


def rewrite_ns(text: str) -> tuple[str, int]:
    n = 0
    for old, new in NAMESPACES:
        for cre, repl in (
            (re.compile(rf"\bnamespace\s+{re.escape(old)}\b"), f"namespace {new}"),
            (
                re.compile(rf"\busing\s+namespace\s+{re.escape(old)}\b"),
                f"using namespace {new}",
            ),
            (re.compile(rf"\b{re.escape(old)}\s*::"), f"{new}::"),
            # closing comments: }  // namespace Smt_Foo
            (
                re.compile(rf"(//\s*namespace\s+){re.escape(old)}\b"),
                rf"\1{new}",
            ),
        ):
            text, c = cre.subn(repl, text)
            n += c
    return text, n


def main() -> None:
    files = ns_n = 0
    for root in (ROOT / "src", ROOT / "testing"):
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if not p.is_file() or p.suffix.lower() not in SUFFIXES:
                continue
            text = p.read_text(encoding="utf-8", errors="replace")
            new, b = rewrite_ns(text)
            if b:
                p.write_text(new, encoding="utf-8", newline="\n")
                files += 1
                ns_n += b
    print(f"files={files} namespaces={ns_n}")


if __name__ == "__main__":
    main()
