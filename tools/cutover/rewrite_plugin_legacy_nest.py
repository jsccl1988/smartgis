# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Rewrite plugin/legacy_XXX → plugin/legacy/XXX includes."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

PAIRS = [
    ("plugin/legacy/orthogrid/", "plugin/legacy/orthogrid/"),
    ("plugin/legacy/model3d/", "plugin/legacy/model3d/"),
    ("plugin/legacy/print/", "plugin/legacy/print/"),
    ("plugin/legacy/proj/", "plugin/legacy/proj/"),
    ("plugin/legacy/dem/", "plugin/legacy/dem/"),
    ("//src/plugin/legacy/orthogrid", "//src/plugin/legacy/orthogrid"),
    ("//src/plugin/legacy/model3d", "//src/plugin/legacy/model3d"),
    ("//src/plugin/legacy/print", "//src/plugin/legacy/print"),
    ("//src/plugin/legacy/proj", "//src/plugin/legacy/proj"),
    ("//src/plugin/legacy/dem", "//src/plugin/legacy/dem"),
]

EXTS = {".h", ".hh", ".hpp", ".cc", ".cpp", ".cxx", ".c", ".md", ".gn", ".gni",
        ".py"}
SKIP = {".git", "out", "third_party", "node_modules"}


def main() -> int:
    n = 0
    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue
        if any(part in SKIP for part in path.parts):
            continue
        if path.suffix.lower() not in EXTS and path.name != "BUILD.gn":
            continue
        raw = path.read_bytes()
        text = raw.decode("utf-8", errors="surrogateescape")
        orig = text
        for old, new in PAIRS:
            text = text.replace(old, new)
        if text != orig:
            path.write_bytes(text.encode("utf-8", errors="surrogateescape"))
            n += 1
            print(path.relative_to(ROOT))
    print(f"rewrote {n} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
