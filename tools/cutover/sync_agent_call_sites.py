#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Sync cross-tree call sites flagged by cutover subagents."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp"}

# Free-function / API renames only (not GetPoint-style member substrings).
REPLACEMENTS = [
    ("SmtPostIAToolMsg(", "post_ia_tool_msg("),
    ("SmtInitProjection(", "init_projection("),
    ("SmtProjectPoint(", "project_point("),
    ("SmtProjectPoints(", "project_points("),
    ("CreateDelaunayTin_Div(", "create_delaunay_tin_div("),
    ("CreateDelaunayTin_Inc(", "create_delaunay_tin_inc("),
    ("DivPolygenIntoTriMesh(", "divide_polygon_into_tri_mesh("),
    ("CopyLayer(", "copy_layer("),
    ("GetQueryRs(", "get_query_rs("),
    ("WriteSmf(", "write_smf("),
    ("ReadSmf(", "read_smf("),
]


def main() -> None:
    files = n = 0
    for root in (ROOT / "src", ROOT / "testing"):
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if not p.is_file() or p.suffix.lower() not in SUFFIXES:
                continue
            text = p.read_text(encoding="utf-8", errors="replace")
            new = text
            local = 0
            for a, b in REPLACEMENTS:
                c = new.count(a)
                if c:
                    new = new.replace(a, b)
                    local += c
            if local:
                p.write_text(new, encoding="utf-8", newline="\n")
                files += 1
                n += local
                print(f"{p.relative_to(ROOT).as_posix()}: {local}")
    print(f"files={files} replacements={n}")


if __name__ == "__main__":
    main()
