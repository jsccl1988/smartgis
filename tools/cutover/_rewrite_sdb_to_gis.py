#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""One-shot rewrite: sdb paths/namespaces/GN → gis (and carto → base/carto)."""

from __future__ import annotations

import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SKIP_DIRS = {".git", "out", "third_party", "node_modules", ".cache", ".vs"}
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
    ".cmake",
    ".html",
    ".js",
    ".ts",
}

# Longer / more specific first.
REPLS: list[tuple[str, str]] = [
    ("src/base/carto", "src/base/carto"),
    ("//src/base/carto", "//src/base/carto"),
    ('"base/carto/', '"base/carto/'),
    ("base/carto/", "base/carto/"),
    ("//src/gis/world", "//src/gis/world"),
    ("//src/gis/assets", "//src/gis/assets"),
    ("src/gis/world", "src/gis/world"),
    ("src/gis/assets", "src/gis/assets"),
    ('"gis/world/', '"gis/world/'),
    ('"gis/assets/', '"gis/assets/'),
    ("gis/world/", "gis/world/"),
    ("gis/assets/", "gis/assets/"),
    ("//src/gis", "//src/gis"),
    ("src/gis/", "src/gis/"),
    ('"gis/', '"gis/'),
    ("gis::", "gis::"),
    ("gis::", "gis::"),
    ("gis::style::", "gis::style::"),
    ("namespace gis", "namespace gis"),
    ("}  // namespace gis", "}  // namespace gis"),
    ("gis::", "gis::"),
    ('dll_stem = "gis"', 'dll_stem = "gis"'),
    ('smt_shared_library("gis")', 'smt_shared_library("gis")'),
    ('group("gis_all")', 'group("gis_all")'),
    ('group("gis")', 'group("gis")'),
    (":gis_all", ":gis_all"),
    ("//src/gis:gis", "//src/gis:gis"),
    ('public_deps = [ ":gis" ]', 'public_deps = [ ":gis" ]'),
    ('public_deps = [":gis"]', 'public_deps = [":gis"]'),
    ('"gis_d.lib"', '"gis_d.lib"'),
    ('"gis.lib"', '"gis.lib"'),
    ("gis_d.dll", "gis_d.dll"),
    ("gis.dll", "gis.dll"),
    ("`base/carto", "`base/carto"),
    ("`gis/", "`gis/"),
    # Source-set / group aliases that still say model/scene after path move.
    ("//src/gis/assets:assets_sources", "//src/gis/assets:assets_sources"),
    ("//src/gis/assets:assets", "//src/gis/assets:assets"),
    ("//src/gis/world:world_sources", "//src/gis/world:world_sources"),
    ("//src/gis/world:world", "//src/gis/world:world"),
]


def should_skip(path: Path) -> bool:
    parts = set(path.parts)
    if parts & SKIP_DIRS:
        return True
    # Do not rewrite this helper mid-run in a confusing way — still OK to update.
    return False


def main() -> None:
    changed: list[str] = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            p = Path(dirpath) / fn
            if should_skip(p):
                continue
            if p.suffix.lower() not in EXTS and p.name not in (
                "BUILD.gn",
                "BUILD",
                "DEPS",
                "README",
            ):
                continue
            try:
                raw = p.read_bytes()
            except OSError:
                continue
            text: str | None = None
            for enc in ("utf-8", "utf-8-sig", "gbk", "latin-1"):
                try:
                    text = raw.decode(enc)
                    break
                except UnicodeDecodeError:
                    continue
            if text is None:
                continue
            orig = text
            for a, b in REPLS:
                text = text.replace(a, b)
            text = text.replace("gis/", "gis/")
            text = text.replace("//src/gis", "//src/gis")
            text = text.replace("base/carto", "base/carto")
            if text != orig:
                # Preserve BOM-less UTF-8 for product sources.
                p.write_bytes(text.encode("utf-8"))
                changed.append(str(p.relative_to(ROOT)).replace("\\", "/"))
    print(f"changed {len(changed)} files")
    for c in changed:
        print(c)


if __name__ == "__main__":
    main()
