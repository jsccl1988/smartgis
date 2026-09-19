#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Rewrite flat #include \"file.h\" to src/-relative paths."""

from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
TESTING = ROOT / "testing"
CODE_SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".inl", ".inc"}
FLAT_INCLUDE = re.compile(r'(#\s*include\s+")([^"/\\]+)(")')


def collect_headers() -> dict[str, list[str]]:
    by_base: dict[str, list[str]] = defaultdict(list)
    for p in SRC.rglob("*"):
        if p.is_file() and p.suffix.lower() in {".h", ".hpp"}:
            rel = p.relative_to(SRC).as_posix()
            by_base[p.name].append(rel)
    for base in by_base:
        by_base[base].sort()
    return by_base


def pick_path(base: str, candidates: list[str], includer_rel: str) -> str | None:
    if len(candidates) == 1:
        return candidates[0]

    # Same-module preference: longest shared path prefix with includer.
    best = None
    best_score = -1
    inc_parts = Path(includer_rel).parts
    for c in candidates:
        c_parts = Path(c).parts
        score = 0
        for a, b in zip(inc_parts, c_parts):
            if a != b:
                break
            score += 1
        # Prefer header living next to includer (same dir).
        if Path(c).parent.as_posix() == str(Path(includer_rel).parent).replace("\\", "/"):
            score += 100
        if score > best_score:
            best_score = score
            best = c

    # Special defaults when still ambiguous (score 0).
    if best_score <= 0:
        if base == "command.h":
            if includer_rel.startswith("tool/"):
                return "tool/command.h"
            return "base/core/command.h"
        if base == "scene.h":
            if includer_rel.startswith("gis/"):
                return "gis/world/scene.h"
            return "render/scene/scene.h"
        if base.startswith("gdi_"):
            if "gdi_simple" in includer_rel:
                for c in candidates:
                    if "gdi_simple" in c:
                        return c
            for c in candidates:
                if "/gdi/" in f"/{c}" and "gdi_simple" not in c:
                    return c
    return best


def rewrite_text(text: str, by_base: dict[str, list[str]], includer_rel: str) -> tuple[str, int]:
    count = 0

    def repl(m: re.Match[str]) -> str:
        nonlocal count
        base = m.group(2)
        cands = by_base.get(base)
        if not cands:
            return m.group(0)
        chosen = pick_path(base, cands, includer_rel)
        if not chosen or chosen == base:
            return m.group(0)
        # Already path-shaped somehow
        if "/" in base or "\\" in base:
            return m.group(0)
        count += 1
        return f'{m.group(1)}{chosen}{m.group(3)}'

    return FLAT_INCLUDE.sub(repl, text), count


def iter_code(roots: list[Path]):
    for root in roots:
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if p.is_file() and p.suffix.lower() in CODE_SUFFIXES:
                yield p


def main() -> int:
    by_base = collect_headers()
    total = 0
    files = 0
    for p in iter_code([SRC, TESTING]):
        if p.is_relative_to(SRC):
            includer_rel = p.relative_to(SRC).as_posix()
        else:
            # testing: no module prefix; leave ambiguous to unique-only
            includer_rel = p.relative_to(ROOT).as_posix()
        text = p.read_text(encoding="utf-8", errors="replace")
        new, n = rewrite_text(text, by_base, includer_rel)
        if n:
            p.write_text(new, encoding="utf-8", newline="\n")
            total += n
            files += 1
    print(f"rewrote_includes={total} files={files} headers_indexed={len(by_base)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
