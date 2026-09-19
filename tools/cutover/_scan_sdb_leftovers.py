#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Scan for leftover sdb path/namespace/DLL references after the rename."""

from __future__ import annotations

import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SKIP = {".git", "out", "third_party", "node_modules", ".cache", ".vs"}
EXTS = {
    ".cc",
    ".cpp",
    ".cxx",
    ".c",
    ".h",
    ".hpp",
    ".hh",
    ".gn",
    ".gni",
    ".md",
    ".mdc",
    ".py",
    ".ps1",
    ".bat",
    ".json",
    ".txt",
    ".yml",
    ".yaml",
}

# Intentional leftovers: SDBD driver / sdbd_* filenames / historical archive docs.
PATTERNS = [
    (re.compile(r'#include\s+"sdb/'), "include sdb/"),
    (re.compile(r"//src/sdb\b"), "GN //src/sdb"),
    (re.compile(r"\bsrc/sdb\b"), "path src/sdb"),
    (re.compile(r"\bnamespace sdb\b"), "namespace sdb"),
    (re.compile(r"\bsdb::"), "sdb::"),
    (re.compile(r'dll_stem\s*=\s*"sdb"'), "dll_stem sdb"),
    (re.compile(r'"(sdb\.lib|sdb_d\.lib)"'), "pragma sdb.lib"),
    (re.compile(r"\bgis::scene::"), "gis::scene::"),
    (re.compile(r"\bgis::model::"), "gis::model::"),
]


def main() -> None:
    hits: dict[str, list[str]] = {label: [] for _, label in PATTERNS}
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP]
        # Skip archive docs for "path src/sdb" noise? still report but tag.
        for fn in filenames:
            p = Path(dirpath) / fn
            if p.suffix.lower() not in EXTS and p.name not in ("BUILD.gn", "BUILD"):
                continue
            try:
                text = p.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            rel = p.relative_to(ROOT).as_posix()
            for cre, label in PATTERNS:
                if cre.search(text):
                    hits[label].append(rel)
    for label, files in hits.items():
        print(f"== {label}: {len(files)}")
        for f in files[:25]:
            print(f"  {f}")
        if len(files) > 25:
            print(f"  ... +{len(files) - 25}")


if __name__ == "__main__":
    main()
