#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Broad call-site sync after parallel snake_case renames."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".inl"}

# Longer / more specific first.
REPLACEMENTS = [
    ("GetSingletonPtr(", "get_singleton_ptr("),
    ("DestroyInstance(", "destroy_instance("),
    ("GetDefaultLog(", "get_default_log("),
    ("GetLog(", "get_log("),
    ("CreateLog(", "create_log("),
    ("LogMessage(", "log_message("),
    ("LoadDynLib(", "load_dyn_lib("),
    ("UnLoadDynLib(", "unload_dyn_lib("),
    ("GetImageTypeByFileExt(", "get_image_type_by_file_ext("),
    ("GetDefaultStyle(", "get_default_style("),
    ("SetDefaultStyle(", "set_default_style("),
    ("GetStyleName(", "get_style_name("),
    ("GetStyleType(", "get_style_type("),
    ("SetStyleName(", "set_style_name("),
    ("SetStyleType(", "set_style_type("),
    ("GetPenDesc(", "get_pen_desc("),
    ("GetBrushDesc(", "get_brush_desc("),
    ("GetStyle(", "get_style("),
    ("AddStyle(", "add_style("),
    ("RemoveStyle(", "remove_style("),
    ("GetStyleCount(", "get_style_count("),
    ("VarToInteger(", "var_to_integer("),
    ("VarToDouble(", "var_to_double("),
    ("VarToString(", "var_to_string("),
    ("STR_Duplicate(", "str_duplicate("),
    ("STR_Count(", "str_count("),
    ("IsEqual(", "is_equal("),
    ("IsInit(", "is_init("),
    ("DivPolygenIntoTriMesh(", "divide_polygon_into_tri_mesh("),
    ("DividePolygonIntoTriMesh(", "divide_polygon_into_tri_mesh("),
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
            # Clone( -> clone( but not for Windows Clone*()
            c = new.count("->Clone(")
            if c:
                new = new.replace("->Clone(", "->clone(")
                local += c
            c = new.count(".Clone(")
            if c:
                new = new.replace(".Clone(", ".clone(")
                local += c
            if local:
                p.write_text(new, encoding="utf-8", newline="\n")
                files += 1
                n += local
    print(f"files={files} replacements={n}")


if __name__ == "__main__":
    main()
